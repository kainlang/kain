#!/usr/bin/env python3
"""
Dedicated Kain-vs-Rust compiler benchmark lane.

This runner measures compile throughput rather than program runtime throughput.
Each case generates deterministic Kain and Rust sources, times clean compiles and
warm rebuilds, validates the produced artifact with a one-shot checksum run, and
emits telemetry plus a dedicated history database.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import sqlite3
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


CASE_ROOT = Path(__file__).resolve().parent
BENCHMARK_ROOT = CASE_ROOT.parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
COMPILER_BUILD_ROOT = OUT_ROOT / "build" / "compiler"
WORKLOAD_ROOT = COMPILER_BUILD_ROOT / "workloads"
WORKLOAD_SESSION_TOKEN = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
WORKLOAD_SESSION_ROOT = WORKLOAD_ROOT / WORKLOAD_SESSION_TOKEN
ARTIFACT_ROOT = COMPILER_BUILD_ROOT / "artifacts"
RUNTIME_CACHE_ROOT = COMPILER_BUILD_ROOT / "runtime_cache"
REPORT_ROOT = OUT_ROOT / "reports"
HISTORY_ROOT = OUT_ROOT / "history"
DEFAULT_MANIFEST = CASE_ROOT / "cases.json"
DEFAULT_MINIMAL_REPORT_NAME = "out/snapshots/latest_compiler.md"
DEFAULT_LATEST_REPORT_STEM = "latest_compiler"
DEFAULT_HISTORY_DB_PATH = HISTORY_ROOT / "compiler_history.sqlite3"
PHASE_ORDER = ["clean", "rebuild"]
LANGUAGE_ORDER = ["kain", "rust"]
LANGUAGE_LABELS = {
    "kain": "Kain LLVM",
    "rust": "Rust LLVM",
}
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
NATIVE_RUNTIME_CACHE_RE = re.compile(
    r"Native runtime cache:\s+"
    r"(?P<objects_reused>\d+)\s+reused,\s+"
    r"(?P<objects_compiled>\d+)\s+compiled,\s+"
    r"(?P<archives_reused>\d+)\s+archives reused,\s+"
    r"(?P<archives_rebuilt>\d+)\s+archives rebuilt"
)
COMPILER_HISTORY_SCHEMA_VERSION = 1
MEASUREMENT_INSTABILITY_MAX_TO_MEDIAN = 1.75
MEASUREMENT_INSTABILITY_COEFF_VAR = 0.35
CASE_MODULUS = 1_000_000_007
CASE_SEED_BASE = 17
KAIN_NATIVE_ENV = {
    "KAIN_NATIVE_PROFILE": "benchmark-release",
    "KAIN_NATIVE_OPT_LEVEL": "3",
    "KAIN_NATIVE_TARGET_CPU": "native",
    "KAIN_NATIVE_DEBUG_INFO": "0",
    "KAIN_RUNTIME_MANIFEST_PATH": str(REPO_ROOT / "runtime" / "native_core_runtime.toml"),
}
RUST_RELEASE_FLAGS = [
    "--edition=2021",
    "-C",
    "opt-level=3",
    "-C",
    "target-cpu=native",
    "-C",
    "debuginfo=0",
    "-C",
    "panic=abort",
    "-C",
    "overflow-checks=off",
]


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


@dataclass
class ResolvedCompiler:
    path: Path
    source: str
    build_command: list[str] | None = None
    build_ms: float | None = None


@dataclass
class GeneratedWorkload:
    case_id: str
    generator: str
    entrypoints: dict[str, Path]
    source_roots: dict[str, Path]
    source_metrics: dict[str, dict[str, int]]
    abstract_metrics: dict[str, int]


def strip_ansi(value: str) -> str:
    return ANSI_RE.sub("", value)


def display_command(command: list[str] | None) -> str:
    if not command:
        return "n/a"
    return " ".join(command)


def executable_name(stem: str) -> str:
    return f"{stem}.exe" if os.name == "nt" else stem


def stable_json_text(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def parse_csv(raw: str | None) -> list[str]:
    if raw is None:
        return []
    return [part.strip() for part in raw.split(",") if part.strip()]


def parse_languages(raw: str | None) -> list[str]:
    if raw is None:
        return []
    requested = parse_csv(raw)
    seen: set[str] = set()
    languages: list[str] = []
    for language in requested:
        if language not in LANGUAGE_ORDER:
            raise ValueError(f"unsupported language '{language}'. Supported: {', '.join(LANGUAGE_ORDER)}")
        if language in seen:
            continue
        seen.add(language)
        languages.append(language)
    return languages


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if not isinstance(manifest, dict):
        raise ValueError(f"compiler manifest must be a JSON object: {path}")
    cases = manifest.get("cases")
    if not isinstance(cases, list):
        raise ValueError(f"compiler manifest must contain a cases array: {path}")
    return manifest


def selected_cases(manifest: dict[str, Any], raw_case: str | None) -> list[dict[str, Any]]:
    cases = manifest.get("cases", [])
    if raw_case is None:
        return [case for case in cases if isinstance(case, dict)]
    requested = parse_csv(raw_case)
    indexed = {
        str(case["id"]): case
        for case in cases
        if isinstance(case, dict) and "id" in case
    }
    missing = [case_id for case_id in requested if case_id not in indexed]
    if missing:
        raise ValueError(f"unknown compiler case(s): {', '.join(missing)}")
    return [indexed[case_id] for case_id in requested]


def run_command(
    command: list[str],
    *,
    cwd: Path,
    timeout: int,
    env_overrides: dict[str, str] | None = None,
) -> CommandResult:
    env = None
    if env_overrides:
        env = os.environ.copy()
        env.update(env_overrides)
    start = time.perf_counter_ns()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        env=env,
    )
    elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
    return CommandResult(
        command=command,
        returncode=completed.returncode,
        stdout=strip_ansi(completed.stdout),
        stderr=strip_ansi(completed.stderr),
        elapsed_ms=elapsed_ms,
    )


def optional_command_output(command: list[str], *, cwd: Path = REPO_ROOT, timeout: int = 15) -> str:
    try:
        result = run_command(command, cwd=cwd, timeout=timeout)
    except (FileNotFoundError, subprocess.SubprocessError, OSError):
        return ""
    return result.stdout.strip()


def git_metadata() -> dict[str, Any]:
    git = shutil.which("git")
    if not git:
        return {
            "available": False,
            "branch": "",
            "commit": "",
            "dirty": False,
            "dirty_entries": 0,
        }
    branch = optional_command_output([git, "rev-parse", "--abbrev-ref", "HEAD"])
    commit = optional_command_output([git, "rev-parse", "HEAD"])
    status = optional_command_output([git, "status", "--porcelain=v1"])
    dirty_lines = [line for line in status.splitlines() if line.strip()]
    return {
        "available": bool(commit),
        "branch": branch,
        "commit": commit,
        "dirty": bool(dirty_lines),
        "dirty_entries": len(dirty_lines),
    }


def machine_fingerprint() -> dict[str, Any]:
    return {
        "system": platform.system(),
        "release": platform.release(),
        "version": platform.version(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python": platform.python_version(),
    }


def history_machine_key() -> str:
    return sha256_text(stable_json_text(machine_fingerprint()))


def resolve_history_db_path(raw: str | None) -> Path | None:
    if raw is None:
        return DEFAULT_HISTORY_DB_PATH.resolve()
    text = str(raw).strip()
    if not text:
        return DEFAULT_HISTORY_DB_PATH.resolve()
    if text.lower() in {"off", "none", "disable", "disabled"}:
        return None
    path = Path(text)
    if not path.is_absolute():
        path = (REPO_ROOT / path).resolve()
    return path


def latest_report_stem(stem: str) -> str:
    candidate = Path(stem)
    if candidate.is_absolute() or len(candidate.parts) != 1 or candidate.name != stem:
        raise ValueError(f"latest report stem must stay inside benchmark report root: {stem}")
    if "." in stem:
        raise ValueError(f"latest report stem cannot include dots: {stem}")
    return stem


def root_snapshot_path(name: str) -> Path:
    candidate = Path(name)
    if candidate.is_absolute():
        raise ValueError(f"minimal report name must be relative to benchmark root: {name}")
    resolved = (BENCHMARK_ROOT / candidate).resolve()
    benchmark_root = BENCHMARK_ROOT.resolve()
    out_root = OUT_ROOT.resolve()
    try:
        resolved.relative_to(benchmark_root)
    except ValueError as exc:
        raise ValueError(f"minimal report path escapes benchmark root: {name}") from exc
    if len(candidate.parts) == 1:
        return BENCHMARK_ROOT / candidate.name
    try:
        resolved.relative_to(out_root)
    except ValueError as exc:
        raise ValueError(f"nested minimal report paths must stay under benchmark/out: {name}") from exc
    resolved.parent.mkdir(parents=True, exist_ok=True)
    return resolved


def resolve_tool(explicit: str | None, env_key: str, default_name: str) -> str:
    requested = explicit or os.environ.get(env_key) or default_name
    return shutil.which(requested) or requested


def find_line_that_looks_like_path(output: str) -> str | None:
    for raw_line in reversed(output.splitlines()):
        line = strip_ansi(raw_line).strip()
        if not line:
            continue
        if ":" in line or line.startswith("/") or line.startswith("\\"):
            return line
    return None


def resolve_kain_exe(explicit: str | None, timeout: int) -> ResolvedCompiler:
    if explicit:
        path = Path(explicit)
        if path.exists():
            return ResolvedCompiler(path.resolve(), "explicit --kain-exe")
        raise FileNotFoundError(f"explicit --kain-exe does not exist: {explicit}")

    env_kain = os.environ.get("KAIN_EXE")
    if env_kain:
        path = Path(env_kain)
        if path.exists():
            return ResolvedCompiler(path.resolve(), "KAIN_EXE")
        raise FileNotFoundError(f"KAIN_EXE does not exist: {env_kain}")

    compiler_timeout = max(timeout, 1200)
    bazel = shutil.which("bazel")
    if bazel:
        build_command = [bazel, "build", "//:kain", "--config=release"]
        build = run_command(build_command, cwd=REPO_ROOT, timeout=compiler_timeout)
        info = run_command([bazel, "info", "bazel-bin", "--config=release"], cwd=REPO_ROOT, timeout=compiler_timeout)
        info_line = find_line_that_looks_like_path(info.stdout)
        candidate = Path(info_line) / "crates" / "cli" / executable_name("kain") if info_line else None
        if build.returncode == 0 and candidate and candidate.exists():
            return ResolvedCompiler(candidate.resolve(), "bazel --config=release", build_command, build.elapsed_ms)
        if build.returncode != 0 and candidate and candidate.exists():
            return ResolvedCompiler(candidate.resolve(), "bazel release fallback after failed refresh", build_command, build.elapsed_ms)
        if build.returncode != 0:
            combined = (build.stdout + "\n" + build.stderr).strip()
            raise RuntimeError(f"Unable to build //:kain with Bazel.\n{combined}")

    cargo = shutil.which("cargo")
    if cargo:
        build_command = [cargo, "build", "--release", "-p", "cli"]
        build = run_command(build_command, cwd=REPO_ROOT, timeout=compiler_timeout)
        candidate = REPO_ROOT / "target" / "release" / executable_name("kain")
        if build.returncode == 0 and candidate.exists():
            return ResolvedCompiler(candidate.resolve(), "cargo --release -p cli", build_command, build.elapsed_ms)

    fallback_candidates = [
        REPO_ROOT / "target" / "release" / executable_name("kain"),
        REPO_ROOT / "target" / "debug" / executable_name("kain"),
    ]
    path_kain = shutil.which("kain")
    if path_kain:
        fallback_candidates.append(Path(path_kain))
    for candidate in fallback_candidates:
        if candidate.exists():
            return ResolvedCompiler(candidate.resolve(), "fallback existing kain binary")
    raise RuntimeError("Could not resolve a usable kain compiler binary.")


def count_text_metrics(files: dict[str, str]) -> dict[str, int]:
    file_count = len(files)
    line_count = 0
    byte_count = 0
    for text in files.values():
        line_count += len(text.splitlines())
        byte_count += len(text.encode("utf-8"))
    return {
        "file_count": file_count,
        "line_count": line_count,
        "byte_count": byte_count,
    }


def parse_native_runtime_cache(stderr: str) -> dict[str, int] | None:
    match = NATIVE_RUNTIME_CACHE_RE.search(stderr)
    if not match:
        return None
    return {key: int(value) for key, value in match.groupdict().items()}


def group_parameters(group_index: int, helper_index: int) -> dict[str, int]:
    return {
        "left_bias": 17 + (group_index * 11) + (helper_index * 7),
        "right_mul": 5 + ((group_index + helper_index) % 9),
        "right_bias": 29 + (group_index * 13) + (helper_index * 5),
        "mix_mul": 7 + ((group_index * 3 + helper_index * 5) % 11),
        "score_mul": 3 + ((group_index + helper_index) % 5),
        "add_bias": 41 + (group_index * 19) + (helper_index * 23),
    }


def stage_python(seed: int, group_index: int, helper_count: int, dispatch_arms: int) -> int:
    value = seed % CASE_MODULUS
    base_pair = group_parameters(group_index, 0)
    for helper_index in range(helper_count):
        params = group_parameters(group_index, helper_index)
        pair_seed = value + params["left_bias"]
        pair_left = (pair_seed + base_pair["left_bias"]) % 97
        pair_right = ((pair_seed * base_pair["right_mul"]) + base_pair["right_bias"]) % 101
        value = (
            (value * params["mix_mul"])
            + pair_left
            + (pair_right * params["score_mul"])
            + params["add_bias"]
        ) % CASE_MODULUS
    selector = (value + (group_index * 13) + helper_count) % dispatch_arms
    if selector == 0:
        return (value + (group_index * 7) + 5) % CASE_MODULUS
    if selector == 1:
        return ((value * 2) + (group_index * 11) + 9) % CASE_MODULUS
    if selector == 2:
        return ((value * 3) + (group_index * 5) + 13) % CASE_MODULUS
    if selector == 3:
        return (value + selector + (group_index * 17) + 19) % CASE_MODULUS
    if selector == 4:
        return ((value * 4) + (group_index * 3) + 23) % CASE_MODULUS
    if selector == 5:
        return ((value * 5) + (group_index * 2) + 29) % CASE_MODULUS
    return (value + (selector * 13) + (group_index * 3) + 31) % CASE_MODULUS


def compute_case_expected(groups: int, helper_count: int, dispatch_arms: int) -> int:
    acc = 0
    for group_index in range(groups):
        seed = CASE_SEED_BASE + acc + (group_index * 17)
        acc = (acc + stage_python(seed, group_index, helper_count, dispatch_arms)) % CASE_MODULUS
    return acc


def kain_group_block(group_index: int, helper_count: int, dispatch_arms: int) -> str:
    group_tag = f"{group_index:03d}"
    struct_name = f"CompilerGroup{group_tag}Pair"
    lines = [
        f"struct {struct_name}:",
        "    left: Int",
        "    right: Int",
        "",
        f"fn compiler_group_{group_tag}_pair(seed: Int) -> {struct_name}:",
    ]
    params0 = group_parameters(group_index, 0)
    lines.append(
        "    return "
        f"{struct_name} {{ left: compiler_mod_fold(seed + {params0['left_bias']}, 97), "
        f"right: compiler_mod_fold((seed * {params0['right_mul']}) + {params0['right_bias']}, 101) }}"
    )
    lines.append("")
    for helper_index in range(helper_count):
        params = group_parameters(group_index, helper_index)
        helper_tag = f"{helper_index:02d}"
        lines.extend(
            [
                f"fn compiler_group_{group_tag}_mix_{helper_tag}(value: Int) -> Int:",
                f"    let pair: {struct_name} = compiler_group_{group_tag}_pair(value + {params['left_bias']})",
                f"    return compiler_mod_fold((value * {params['mix_mul']}) + pair.left + (pair.right * {params['score_mul']}) + {params['add_bias']}, COMPILER_MODULUS)",
                "",
            ]
        )
    lines.extend(
        [
            f"fn compiler_group_{group_tag}_dispatch(value: Int) -> Int:",
            f"    let selector: Int = compiler_mod_fold(value + {(group_index * 13) + helper_count}, {dispatch_arms})",
        ]
    )
    for selector in range(dispatch_arms):
        branch_value = stage_python(0, group_index, helper_count, dispatch_arms)
        lines.append(f"    if selector == {selector}:")
        if selector == 0:
            lines.append(f"        return compiler_mod_fold(value + {(group_index * 7) + 5}, COMPILER_MODULUS)")
        elif selector == 1:
            lines.append(f"        return compiler_mod_fold((value * 2) + {(group_index * 11) + 9}, COMPILER_MODULUS)")
        elif selector == 2:
            lines.append(f"        return compiler_mod_fold((value * 3) + {(group_index * 5) + 13}, COMPILER_MODULUS)")
        elif selector == 3:
            lines.append(f"        return compiler_mod_fold(value + {selector + (group_index * 17) + 19}, COMPILER_MODULUS)")
        elif selector == 4:
            lines.append(f"        return compiler_mod_fold((value * 4) + {(group_index * 3) + 23}, COMPILER_MODULUS)")
        elif selector == 5:
            lines.append(f"        return compiler_mod_fold((value * 5) + {(group_index * 2) + 29}, COMPILER_MODULUS)")
        else:
            lines.append(f"        return compiler_mod_fold(value + {(selector * 13) + (group_index * 3) + 31}, COMPILER_MODULUS)")
    lines.append(f"    return compiler_mod_fold(value + {branch_value + group_index + 37}, COMPILER_MODULUS)")
    lines.append("")
    lines.append(f"fn compiler_group_{group_tag}_stage(seed: Int) -> Int:")
    previous = "seed"
    for helper_index in range(helper_count):
        helper_tag = f"{helper_index:02d}"
        current = f"mixed_{helper_tag}"
        lines.append(f"    let {current}: Int = compiler_group_{group_tag}_mix_{helper_tag}({previous})")
        previous = current
    lines.append(f"    return compiler_group_{group_tag}_dispatch({previous})")
    return "\n".join(lines)


def rust_group_block(group_index: int, helper_count: int, dispatch_arms: int) -> str:
    group_tag = f"{group_index:03d}"
    struct_name = f"CompilerGroup{group_tag}Pair"
    lines = [
        f"struct {struct_name} {{",
        "    left: i64,",
        "    right: i64,",
        "}",
        "",
        f"fn compiler_group_{group_tag}_pair(seed: i64) -> {struct_name} {{",
    ]
    params0 = group_parameters(group_index, 0)
    lines.append(
        "    "
        f"{struct_name} {{ left: compiler_mod_fold(seed + {params0['left_bias']}, 97), "
        f"right: compiler_mod_fold((seed * {params0['right_mul']}) + {params0['right_bias']}, 101) }}"
    )
    lines.append("}")
    lines.append("")
    for helper_index in range(helper_count):
        params = group_parameters(group_index, helper_index)
        helper_tag = f"{helper_index:02d}"
        lines.extend(
            [
                f"fn compiler_group_{group_tag}_mix_{helper_tag}(value: i64) -> i64 {{",
                f"    let pair = compiler_group_{group_tag}_pair(value + {params['left_bias']});",
                f"    compiler_mod_fold((value * {params['mix_mul']}) + pair.left + (pair.right * {params['score_mul']}) + {params['add_bias']}, COMPILER_MODULUS)",
                "}",
                "",
            ]
        )
    lines.extend(
        [
            f"fn compiler_group_{group_tag}_dispatch(value: i64) -> i64 {{",
            f"    let selector = compiler_mod_fold(value + {(group_index * 13) + helper_count}, {dispatch_arms});",
        ]
    )
    for selector in range(dispatch_arms):
        branch_value = stage_python(0, group_index, helper_count, dispatch_arms)
        if selector == 0:
            body = f"compiler_mod_fold(value + {(group_index * 7) + 5}, COMPILER_MODULUS)"
        elif selector == 1:
            body = f"compiler_mod_fold((value * 2) + {(group_index * 11) + 9}, COMPILER_MODULUS)"
        elif selector == 2:
            body = f"compiler_mod_fold((value * 3) + {(group_index * 5) + 13}, COMPILER_MODULUS)"
        elif selector == 3:
            body = f"compiler_mod_fold(value + {selector + (group_index * 17) + 19}, COMPILER_MODULUS)"
        elif selector == 4:
            body = f"compiler_mod_fold((value * 4) + {(group_index * 3) + 23}, COMPILER_MODULUS)"
        elif selector == 5:
            body = f"compiler_mod_fold((value * 5) + {(group_index * 2) + 29}, COMPILER_MODULUS)"
        else:
            body = f"compiler_mod_fold(value + {(selector * 13) + (group_index * 3) + 31}, COMPILER_MODULUS)"
        lines.append(f"    if selector == {selector} {{")
        lines.append(f"        return {body};")
        lines.append("    }")
    lines.append(f"    compiler_mod_fold(value + {branch_value + group_index + 37}, COMPILER_MODULUS)")
    lines.append("}")
    lines.append("")
    lines.append(f"fn compiler_group_{group_tag}_stage(seed: i64) -> i64 {{")
    previous = "seed"
    for helper_index in range(helper_count):
        helper_tag = f"{helper_index:02d}"
        current = f"mixed_{helper_tag}"
        lines.append(f"    let {current} = compiler_group_{group_tag}_mix_{helper_tag}({previous});")
        previous = current
    lines.append(f"    compiler_group_{group_tag}_dispatch({previous})")
    lines.append("}")
    return "\n".join(lines)


def render_kain_single_file(case_id: str, groups: int, helper_count: int, dispatch_arms: int, expected: int) -> str:
    lines = [
        "const COMPILER_MODULUS: Int = 1000000007",
        f"const COMPILER_EXPECTED: Int = {expected}",
        f"const COMPILER_SEED_BASE: Int = {CASE_SEED_BASE}",
        "",
        "fn compiler_mod_fold(value: Int, modulus: Int) -> Int:",
        "    return value % modulus",
        "",
    ]
    for group_index in range(groups):
        lines.append(kain_group_block(group_index, helper_count, dispatch_arms))
        lines.append("")
    lines.extend(
        [
            "fn main() -> Int:",
            "    var acc: Int = 0",
        ]
    )
    for group_index in range(groups):
        group_tag = f"{group_index:03d}"
        lines.append(
            f"    acc = compiler_mod_fold(acc + compiler_group_{group_tag}_stage(COMPILER_SEED_BASE + acc + {group_index * 17}), COMPILER_MODULUS)"
        )
    lines.extend(
        [
            "    if acc != COMPILER_EXPECTED:",
            "        return 1",
            "    return 0",
        ]
    )
    return "\n".join(lines) + "\n"


def render_rust_single_file(case_id: str, groups: int, helper_count: int, dispatch_arms: int, expected: int) -> str:
    lines = [
        "const COMPILER_MODULUS: i64 = 1_000_000_007;",
        f"const COMPILER_EXPECTED: i64 = {expected};",
        f"const COMPILER_SEED_BASE: i64 = {CASE_SEED_BASE};",
        "",
        "#[inline]",
        "fn compiler_mod_fold(value: i64, modulus: i64) -> i64 {",
        "    value % modulus",
        "}",
        "",
    ]
    for group_index in range(groups):
        lines.append(rust_group_block(group_index, helper_count, dispatch_arms))
        lines.append("")
    lines.extend(
        [
            "fn main() {",
            "    let mut acc: i64 = 0;",
        ]
    )
    for group_index in range(groups):
        group_tag = f"{group_index:03d}"
        lines.append(
            f"    acc = compiler_mod_fold(acc + compiler_group_{group_tag}_stage(COMPILER_SEED_BASE + acc + {group_index * 17}), COMPILER_MODULUS);"
        )
    lines.extend(
        [
            "    if acc != COMPILER_EXPECTED {",
            "        std::process::exit(1);",
            "    }",
            "}",
        ]
    )
    return "\n".join(lines) + "\n"


def render_kain_module_file(group_index: int, helper_count: int, dispatch_arms: int) -> str:
    group_tag = f"{group_index:03d}"
    block = kain_group_block(group_index, helper_count, dispatch_arms).replace(
        f"fn compiler_group_{group_tag}_stage",
        f"pub fn compiler_group_{group_tag}_stage",
        1,
    )
    block = block.replace("COMPILER_MODULUS", "1000000007")
    return "use shared::compiler_mod_fold\n\n" + block + "\n"


def render_rust_module_file(group_index: int, helper_count: int, dispatch_arms: int) -> str:
    group_tag = f"{group_index:03d}"
    block = rust_group_block(group_index, helper_count, dispatch_arms).replace(
        f"fn compiler_group_{group_tag}_stage",
        f"pub fn compiler_group_{group_tag}_stage",
        1,
    )
    return "use crate::shared::{compiler_mod_fold, COMPILER_MODULUS};\n\n" + block + "\n"


def render_kain_module_main(groups: int, expected: int) -> str:
    imports = ["use shared::compiler_mod_fold"] + [
        f"use group_{group_index:03d}::compiler_group_{group_index:03d}_stage"
        for group_index in range(groups)
    ]
    lines = imports + [
        "",
        "const COMPILER_MODULUS: Int = 1000000007",
        f"const COMPILER_EXPECTED: Int = {expected}",
        f"const COMPILER_SEED_BASE: Int = {CASE_SEED_BASE}",
        "",
        "fn main() -> Int:",
        "    var acc: Int = 0",
    ]
    for group_index in range(groups):
        group_tag = f"{group_index:03d}"
        lines.append(
            f"    acc = compiler_mod_fold(acc + compiler_group_{group_tag}_stage(COMPILER_SEED_BASE + acc + {group_index * 17}), COMPILER_MODULUS)"
        )
    lines.extend(
        [
            "    if acc != COMPILER_EXPECTED:",
            "        return 1",
            "    return 0",
        ]
    )
    return "\n".join(lines) + "\n"


def render_rust_module_main(groups: int, expected: int) -> str:
    lines = ["mod shared;"] + [f"mod group_{group_index:03d};" for group_index in range(groups)]
    lines.append("")
    lines.append("use shared::{compiler_mod_fold, COMPILER_MODULUS};")
    for group_index in range(groups):
        lines.append(f"use group_{group_index:03d}::compiler_group_{group_index:03d}_stage;")
    lines.extend(
        [
            "",
            f"const COMPILER_EXPECTED: i64 = {expected};",
            f"const COMPILER_SEED_BASE: i64 = {CASE_SEED_BASE};",
            "",
            "fn main() {",
            "    let mut acc: i64 = 0;",
        ]
    )
    for group_index in range(groups):
        group_tag = f"{group_index:03d}"
        lines.append(
            f"    acc = compiler_mod_fold(acc + compiler_group_{group_tag}_stage(COMPILER_SEED_BASE + acc + {group_index * 17}), COMPILER_MODULUS);"
        )
    lines.extend(
        [
            "    if acc != COMPILER_EXPECTED {",
            "        std::process::exit(1);",
            "    }",
            "}",
        ]
    )
    return "\n".join(lines) + "\n"


def generate_case_files(case: dict[str, Any], languages: list[str]) -> tuple[dict[str, dict[str, str]], dict[str, int]]:
    generator = str(case.get("generator", "")).strip()
    params = case.get("parameters", {})
    if not isinstance(params, dict):
        raise ValueError(f"case parameters must be an object for {case.get('id', '<unknown>')}")
    groups = int(params.get("groups", 0))
    helper_count = int(params.get("helpers_per_group", 0))
    dispatch_arms = int(params.get("dispatch_arms", 0))
    if groups <= 0 or helper_count <= 0 or dispatch_arms <= 0:
        raise ValueError(f"invalid generator parameters for {case['id']}")
    expected = compute_case_expected(groups, helper_count, dispatch_arms)

    files_by_language: dict[str, dict[str, str]] = {}
    if "kain" in languages:
        if generator == "single_file_mesh":
            files_by_language["kain"] = {
                "main.kn": render_kain_single_file(case["id"], groups, helper_count, dispatch_arms, expected)
            }
        elif generator == "module_fanout":
            files = {
                "shared.kn": "pub fn compiler_mod_fold(value: Int, modulus: Int) -> Int:\n    return value % modulus\n",
                "main.kn": render_kain_module_main(groups, expected),
            }
            for group_index in range(groups):
                files[f"group_{group_index:03d}.kn"] = render_kain_module_file(group_index, helper_count, dispatch_arms)
            files_by_language["kain"] = files
        else:
            raise ValueError(f"unknown generator '{generator}' for {case['id']}")
    if "rust" in languages:
        if generator == "single_file_mesh":
            files_by_language["rust"] = {
                "main.rs": render_rust_single_file(case["id"], groups, helper_count, dispatch_arms, expected)
            }
        elif generator == "module_fanout":
            files = {
                "shared.rs": "pub const COMPILER_MODULUS: i64 = 1_000_000_007;\n\n#[inline]\npub fn compiler_mod_fold(value: i64, modulus: i64) -> i64 {\n    value % modulus\n}\n",
                "main.rs": render_rust_module_main(groups, expected),
            }
            for group_index in range(groups):
                files[f"group_{group_index:03d}.rs"] = render_rust_module_file(group_index, helper_count, dispatch_arms)
            files_by_language["rust"] = files
        else:
            raise ValueError(f"unknown generator '{generator}' for {case['id']}")

    abstract_metrics = {
        "group_count": groups,
        "helper_count_per_group": helper_count,
        "dispatch_arm_count": dispatch_arms,
        "function_count": (groups * (helper_count + 3)) + 1 + (1 if generator == "module_fanout" else 0),
        "struct_count": groups,
        "module_count": 1 if generator == "single_file_mesh" else groups + 2,
    }
    abstract_metrics["declaration_count"] = (
        abstract_metrics["function_count"]
        + abstract_metrics["struct_count"]
        + abstract_metrics["module_count"]
    )
    return files_by_language, abstract_metrics


def remove_path(path: Path) -> None:
    retries = 6 if os.name == "nt" else 1
    for attempt in range(retries):
        if not path.exists():
            return
        try:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
            return
        except FileNotFoundError:
            return
        except OSError:
            if attempt + 1 >= retries:
                raise
            time.sleep(0.25)


def write_workload_files(root: Path, files: dict[str, str]) -> None:
    remove_path(root)
    root.mkdir(parents=True, exist_ok=True)
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


def generate_case_workload(case: dict[str, Any], languages: list[str]) -> GeneratedWorkload:
    case_id = str(case["id"])
    generator = str(case.get("generator", ""))
    files_by_language, abstract_metrics = generate_case_files(case, languages)
    entrypoints: dict[str, Path] = {}
    source_roots: dict[str, Path] = {}
    source_metrics: dict[str, dict[str, int]] = {}
    for language, files in files_by_language.items():
        source_root = WORKLOAD_SESSION_ROOT / case_id / language
        write_workload_files(source_root, files)
        source_roots[language] = source_root
        entrypoints[language] = source_root / ("main.kn" if language == "kain" else "main.rs")
        source_metrics[language] = count_text_metrics(files)
    return GeneratedWorkload(
        case_id=case_id,
        generator=generator,
        entrypoints=entrypoints,
        source_roots=source_roots,
        source_metrics=source_metrics,
        abstract_metrics=abstract_metrics,
    )


def sidecar_paths_for_executable(exe_path: Path) -> list[Path]:
    paths = [exe_path]
    if os.name == "nt":
        paths.extend(
            [
                exe_path.with_suffix(".pdb"),
                exe_path.with_suffix(".ilk"),
                exe_path.with_suffix(".lib"),
                exe_path.with_suffix(".exp"),
            ]
        )
    return paths


def wait_for_build_output(path: Path, *, attempts: int | None = None, delay_secs: float = 0.1) -> bool:
    if path.exists():
        return True
    settle_attempts = attempts if attempts is not None else (12 if os.name == "nt" else 1)
    for _ in range(settle_attempts):
        time.sleep(delay_secs)
        if path.exists():
            return True
    return False


def should_retry_output_lock(stderr: str) -> bool:
    text = stderr.lower()
    return (
        "permission denied" in text
        or "access is denied" in text
        or "fatal error lnk1104" in text
        or "cannot open file" in text
        or "unable to remove file" in text
        or "unable to remove stale runtime cache artifact" in text
    )


def purge_build_outputs(paths: list[Path]) -> None:
    retries = 6 if os.name == "nt" else 1
    for path in paths:
        for attempt in range(retries):
            if not path.exists():
                break
            try:
                if path.is_dir():
                    shutil.rmtree(path)
                else:
                    path.unlink()
                break
            except FileNotFoundError:
                break
            except PermissionError:
                if attempt + 1 >= retries:
                    break
                time.sleep(0.25)


def run_build_command_with_retries(
    command: list[str],
    *,
    timeout: int,
    cwd: Path,
    env_overrides: dict[str, str] | None = None,
    output_paths: list[Path] | None = None,
) -> CommandResult:
    attempts = 4 if os.name == "nt" and output_paths else 1
    total_elapsed_ms = 0.0
    last_result: CommandResult | None = None
    for attempt in range(attempts):
        if output_paths:
            purge_build_outputs(output_paths)
        result = run_command(command, cwd=cwd, timeout=timeout, env_overrides=env_overrides)
        total_elapsed_ms += result.elapsed_ms
        last_result = result
        if result.returncode == 0:
            return CommandResult(result.command, result.returncode, result.stdout, result.stderr, total_elapsed_ms)
        if attempt + 1 >= attempts or not should_retry_output_lock(result.stderr):
            break
    if last_result is None:
        raise RuntimeError("build command did not execute")
    return CommandResult(
        last_result.command,
        last_result.returncode,
        last_result.stdout,
        last_result.stderr,
        total_elapsed_ms,
    )


def sample_runtime_cache_base(case_id: str, language: str, phase: str, sample_index: int) -> Path:
    return RUNTIME_CACHE_ROOT / case_id / language / phase / f"sample_{sample_index:02d}"


def reset_measurement_state(
    language: str,
    source_root: Path,
    build_root: Path,
    *,
    runtime_cache_base: Path | None = None,
) -> None:
    remove_path(build_root)
    build_root.mkdir(parents=True, exist_ok=True)
    if language == "kain":
        for cache_name in ("generated", ".kain"):
            cache_root = source_root / cache_name
            if cache_root.exists():
                remove_path(cache_root)
        if runtime_cache_base and runtime_cache_base.exists():
            remove_path(runtime_cache_base)


def build_kain_artifact(
    case_id: str,
    entry_path: Path,
    build_root: Path,
    kain_exe: ResolvedCompiler,
    timeout: int,
    env_overrides: dict[str, str] | None = None,
) -> dict[str, Any]:
    build_root.mkdir(parents=True, exist_ok=True)
    ll_path = build_root / f"{case_id}.ll"
    exe_path = ll_path.with_suffix(".exe" if os.name == "nt" else "")
    build_env = dict(KAIN_NATIVE_ENV)
    if env_overrides:
        build_env.update(env_overrides)
    command = [
        str(kain_exe.path),
        str(entry_path.resolve()),
        "-t",
        "llvm",
        "-o",
        str(ll_path.resolve()),
    ]
    result = run_build_command_with_retries(
        command,
        timeout=timeout,
        cwd=entry_path.parent,
        env_overrides=build_env,
        output_paths=[ll_path, *sidecar_paths_for_executable(exe_path)],
    )
    wait_for_build_output(ll_path)
    wait_for_build_output(exe_path)
    ok = result.returncode == 0 and exe_path.exists()
    native_runtime_cache = parse_native_runtime_cache(result.stderr)
    return {
        "ok": ok,
        "command": command,
        "cwd": str(entry_path.parent),
        "env": build_env,
        "elapsed_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "artifact_path": str(exe_path),
        "artifact_bytes": exe_path.stat().st_size if exe_path.exists() else None,
        "native_runtime_cache": native_runtime_cache,
        "error": "" if ok else "Kain compile failed or did not produce an executable.",
    }


def build_rust_artifact(case_id: str, entry_path: Path, build_root: Path, rustc: str, timeout: int) -> dict[str, Any]:
    build_root.mkdir(parents=True, exist_ok=True)
    exe_path = build_root / executable_name(case_id)
    command = [
        rustc,
        str(entry_path.name),
        *RUST_RELEASE_FLAGS,
        "-o",
        str(exe_path.resolve()),
    ]
    result = run_build_command_with_retries(
        command,
        timeout=timeout,
        cwd=entry_path.parent,
        output_paths=sidecar_paths_for_executable(exe_path),
    )
    wait_for_build_output(exe_path)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "command": command,
        "cwd": str(entry_path.parent),
        "env": {},
        "elapsed_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "artifact_path": str(exe_path),
        "artifact_bytes": exe_path.stat().st_size if exe_path.exists() else None,
        "native_runtime_cache": None,
        "error": "" if ok else "Rust compile failed or did not produce an executable.",
    }


def build_language_artifact(
    language: str,
    case_id: str,
    entry_path: Path,
    build_root: Path,
    *,
    kain_exe: ResolvedCompiler | None,
    rustc: str,
    timeout: int,
    env_overrides: dict[str, str] | None = None,
) -> dict[str, Any]:
    if language == "kain":
        if kain_exe is None:
            raise RuntimeError("kain compiler not available for selected language set")
        return build_kain_artifact(case_id, entry_path, build_root, kain_exe, timeout, env_overrides=env_overrides)
    if language == "rust":
        return build_rust_artifact(case_id, entry_path, build_root, rustc, timeout)
    raise ValueError(f"unsupported language '{language}'")


def verify_artifact(exe_path: Path, *, cwd: Path, timeout: int) -> dict[str, Any]:
    if not exe_path.exists():
        return {
            "ok": False,
            "returncode": -1,
            "stdout": "",
            "stderr": f"missing executable {exe_path}",
            "elapsed_ms": 0.0,
        }
    result = run_command([str(exe_path)], cwd=cwd, timeout=timeout)
    return {
        "ok": result.returncode == 0,
        "returncode": result.returncode,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "elapsed_ms": result.elapsed_ms,
    }


def summarize_samples(samples_ms: list[float]) -> dict[str, Any]:
    if not samples_ms:
        return {
            "min_ms": None,
            "max_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "stdev_ms": None,
            "coefficient_of_variation": None,
            "max_to_median_ratio": None,
            "unstable": False,
            "stability_note": "",
        }
    sample_min = min(samples_ms)
    sample_max = max(samples_ms)
    sample_median = statistics.median(samples_ms)
    sample_mean = statistics.fmean(samples_ms)
    sample_stdev = statistics.stdev(samples_ms) if len(samples_ms) > 1 else 0.0
    coeff_var = (sample_stdev / sample_mean) if sample_mean else 0.0
    max_to_median = (sample_max / sample_median) if sample_median else None
    unstable = bool(
        max_to_median is not None
        and max_to_median >= MEASUREMENT_INSTABILITY_MAX_TO_MEDIAN
        or coeff_var >= MEASUREMENT_INSTABILITY_COEFF_VAR
    )
    notes: list[str] = []
    if max_to_median is not None and max_to_median >= MEASUREMENT_INSTABILITY_MAX_TO_MEDIAN:
        notes.append(f"max/median {max_to_median:.2f} >= {MEASUREMENT_INSTABILITY_MAX_TO_MEDIAN:.2f}")
    if coeff_var >= MEASUREMENT_INSTABILITY_COEFF_VAR:
        notes.append(f"cv {coeff_var:.2f} >= {MEASUREMENT_INSTABILITY_COEFF_VAR:.2f}")
    return {
        "min_ms": sample_min,
        "max_ms": sample_max,
        "median_ms": sample_median,
        "mean_ms": sample_mean,
        "stdev_ms": sample_stdev,
        "coefficient_of_variation": coeff_var,
        "max_to_median_ratio": max_to_median,
        "unstable": unstable,
        "stability_note": "; ".join(notes),
    }


def measure_phase(
    *,
    case_id: str,
    language: str,
    phase: str,
    workload: GeneratedWorkload,
    warmups: int,
    runs: int,
    timeout: int,
    kain_exe: ResolvedCompiler | None,
    rustc: str,
) -> dict[str, Any]:
    if phase not in PHASE_ORDER:
        raise ValueError(f"unsupported phase '{phase}'")
    source_root = workload.source_roots[language]
    entry_path = workload.entrypoints[language]
    build_root = ARTIFACT_ROOT / case_id / language / phase
    timed_samples: list[float] = []
    warmup_samples: list[float] = []
    last_build: dict[str, Any] | None = None
    last_verify: dict[str, Any] | None = None
    total_iterations = warmups + runs
    for sample_index in range(total_iterations):
        runtime_cache_base = (
            sample_runtime_cache_base(case_id, language, phase, sample_index) if language == "kain" else None
        )
        build_env = (
            {"KAIN_RUNTIME_CACHE_DIR": str(runtime_cache_base.resolve())}
            if runtime_cache_base is not None
            else None
        )
        reset_measurement_state(language, source_root, build_root, runtime_cache_base=runtime_cache_base)
        if phase == "rebuild":
            prime_build = build_language_artifact(
                language,
                case_id,
                entry_path,
                build_root,
                kain_exe=kain_exe,
                rustc=rustc,
                timeout=timeout,
                env_overrides=build_env,
            )
            if not prime_build.get("ok"):
                return {
                    "ok": False,
                    "phase": phase,
                    "language": language,
                    "samples_ms": timed_samples,
                    "warmup_samples_ms": warmup_samples,
                    "artifact_path": prime_build.get("artifact_path", ""),
                    "artifact_bytes": prime_build.get("artifact_bytes"),
                    "command": prime_build.get("command", []),
                    "cwd": prime_build.get("cwd", ""),
                    "env": prime_build.get("env", {}),
                    "stdout": prime_build.get("stdout", ""),
                    "stderr": prime_build.get("stderr", ""),
                    "native_runtime_cache": prime_build.get("native_runtime_cache"),
                    "verify_ok": False,
                    "verify_exit_code": None,
                    "verify_stdout": "",
                    "verify_stderr": "",
                    "error": f"warm prime build failed: {prime_build.get('error', '')}".strip(),
                    **summarize_samples(timed_samples),
                }
        build = build_language_artifact(
            language,
            case_id,
            entry_path,
            build_root,
            kain_exe=kain_exe,
            rustc=rustc,
            timeout=timeout,
            env_overrides=build_env,
        )
        if not build.get("ok"):
            return {
                "ok": False,
                "phase": phase,
                "language": language,
                "samples_ms": timed_samples,
                "warmup_samples_ms": warmup_samples,
                "artifact_path": build.get("artifact_path", ""),
                "artifact_bytes": build.get("artifact_bytes"),
                "command": build.get("command", []),
                "cwd": build.get("cwd", ""),
                "env": build.get("env", {}),
                "stdout": build.get("stdout", ""),
                "stderr": build.get("stderr", ""),
                "native_runtime_cache": build.get("native_runtime_cache"),
                "verify_ok": False,
                "verify_exit_code": None,
                "verify_stdout": "",
                "verify_stderr": "",
                "error": build.get("error", "compile failed"),
                **summarize_samples(timed_samples),
            }
        verify = verify_artifact(Path(str(build["artifact_path"])), cwd=entry_path.parent, timeout=min(timeout, 60))
        if not verify.get("ok"):
            return {
                "ok": False,
                "phase": phase,
                "language": language,
                "samples_ms": timed_samples,
                "warmup_samples_ms": warmup_samples,
                "artifact_path": build.get("artifact_path", ""),
                "artifact_bytes": build.get("artifact_bytes"),
                "command": build.get("command", []),
                "cwd": build.get("cwd", ""),
                "env": build.get("env", {}),
                "stdout": build.get("stdout", ""),
                "stderr": build.get("stderr", ""),
                "native_runtime_cache": build.get("native_runtime_cache"),
                "verify_ok": False,
                "verify_exit_code": verify.get("returncode"),
                "verify_stdout": verify.get("stdout", ""),
                "verify_stderr": verify.get("stderr", ""),
                "error": f"artifact verification failed with exit code {verify.get('returncode')}",
                **summarize_samples(timed_samples),
            }
        if sample_index < warmups:
            warmup_samples.append(float(build["elapsed_ms"]))
        else:
            timed_samples.append(float(build["elapsed_ms"]))
        last_build = build
        last_verify = verify
    summary = summarize_samples(timed_samples)
    return {
        "ok": True,
        "phase": phase,
        "language": language,
        "samples_ms": timed_samples,
        "warmup_samples_ms": warmup_samples,
        "artifact_path": last_build.get("artifact_path", "") if last_build else "",
        "artifact_bytes": last_build.get("artifact_bytes") if last_build else None,
        "command": last_build.get("command", []) if last_build else [],
        "cwd": last_build.get("cwd", "") if last_build else "",
        "env": last_build.get("env", {}) if last_build else {},
        "stdout": last_build.get("stdout", "") if last_build else "",
        "stderr": last_build.get("stderr", "") if last_build else "",
        "native_runtime_cache": last_build.get("native_runtime_cache") if last_build else None,
        "verify_ok": bool(last_verify and last_verify.get("ok")),
        "verify_exit_code": last_verify.get("returncode") if last_verify else None,
        "verify_stdout": last_verify.get("stdout", "") if last_verify else "",
        "verify_stderr": last_verify.get("stderr", "") if last_verify else "",
        "error": "",
        **summary,
    }


def compute_phase_summary(
    results: dict[str, dict[str, Any]],
    phase: str,
    languages: list[str],
) -> dict[str, Any]:
    phase_results = results.get(phase, {})
    medians: dict[str, float] = {}
    for language in languages:
        result = phase_results.get(language)
        if not isinstance(result, dict) or not result.get("ok") or result.get("median_ms") is None:
            continue
        medians[language] = float(result["median_ms"])
    if not medians:
        return {
            "winner": "n/a",
            "fastest_median_ms": None,
            "relative_to_fastest": {language: None for language in languages},
        }
    fastest_language = min(medians, key=medians.get)
    fastest_ms = medians[fastest_language]
    relative = {
        language: (medians[language] / fastest_ms) if language in medians and fastest_ms > 0 else None
        for language in languages
    }
    return {
        "winner": fastest_language,
        "fastest_median_ms": fastest_ms,
        "relative_to_fastest": relative,
    }


def compute_case_telemetry(case_result: dict[str, Any]) -> dict[str, Any]:
    languages = case_result.get("languages", [])
    source_metrics = case_result.get("source_metrics", {})
    abstract_metrics = case_result.get("abstract_metrics", {})
    declaration_count = float(abstract_metrics.get("declaration_count", 0))
    function_count = float(abstract_metrics.get("function_count", 0))
    metrics: list[dict[str, Any]] = []
    for phase in PHASE_ORDER:
        phase_results = case_result.get("results", {}).get(phase, {})
        decl_values: dict[str, float | None] = {}
        function_values: dict[str, float | None] = {}
        line_values: dict[str, float | None] = {}
        byte_values: dict[str, float | None] = {}
        native_runtime_objects_reused: dict[str, float | None] = {}
        native_runtime_objects_compiled: dict[str, float | None] = {}
        native_runtime_archives_reused: dict[str, float | None] = {}
        native_runtime_archives_rebuilt: dict[str, float | None] = {}
        line_work_items: dict[str, int | None] = {}
        byte_work_items: dict[str, int | None] = {}
        for language in languages:
            result = phase_results.get(language, {})
            median_ms = result.get("median_ms")
            source = source_metrics.get(language, {})
            native_runtime_cache = result.get("native_runtime_cache")
            line_work_items[language] = source.get("line_count")
            byte_work_items[language] = source.get("byte_count")
            if result.get("ok") and median_ms not in (None, 0):
                median = float(median_ms)
                decl_values[language] = (declaration_count * 1000.0) / median if declaration_count else None
                function_values[language] = (function_count * 1000.0) / median if function_count else None
                line_count = float(source.get("line_count", 0))
                byte_count = float(source.get("byte_count", 0))
                line_values[language] = (line_count * 1000.0) / median if line_count else None
                byte_values[language] = (byte_count * 1000.0) / median if byte_count else None
            else:
                decl_values[language] = None
                function_values[language] = None
                line_values[language] = None
                byte_values[language] = None
            if isinstance(native_runtime_cache, dict):
                native_runtime_objects_reused[language] = coerce_float(native_runtime_cache.get("objects_reused"))
                native_runtime_objects_compiled[language] = coerce_float(native_runtime_cache.get("objects_compiled"))
                native_runtime_archives_reused[language] = coerce_float(native_runtime_cache.get("archives_reused"))
                native_runtime_archives_rebuilt[language] = coerce_float(native_runtime_cache.get("archives_rebuilt"))
            else:
                native_runtime_objects_reused[language] = None
                native_runtime_objects_compiled[language] = None
                native_runtime_archives_reused[language] = None
                native_runtime_archives_rebuilt[language] = None
        metrics.extend(
            [
                {
                    "id": f"{phase}_declarations_per_second",
                    "label": f"{phase.title()} declarations/s",
                    "unit": "decl/s",
                    "phase": phase,
                    "work_items": {language: declaration_count for language in languages},
                    "values": decl_values,
                },
                {
                    "id": f"{phase}_functions_per_second",
                    "label": f"{phase.title()} functions/s",
                    "unit": "fn/s",
                    "phase": phase,
                    "work_items": {language: function_count for language in languages},
                    "values": function_values,
                },
                {
                    "id": f"{phase}_source_lines_per_second",
                    "label": f"{phase.title()} source lines/s",
                    "unit": "lines/s",
                    "phase": phase,
                    "work_items": line_work_items,
                    "values": line_values,
                },
                {
                    "id": f"{phase}_source_bytes_per_second",
                    "label": f"{phase.title()} source bytes/s",
                    "unit": "bytes/s",
                    "phase": phase,
                    "work_items": byte_work_items,
                    "values": byte_values,
                },
            ]
        )
        if any(value is not None for value in native_runtime_objects_reused.values()):
            metrics.extend(
                [
                    {
                        "id": f"{phase}_native_runtime_objects_reused",
                        "label": f"{phase.title()} native runtime objects reused",
                        "unit": "count",
                        "phase": phase,
                        "work_items": {language: None for language in languages},
                        "values": native_runtime_objects_reused,
                    },
                    {
                        "id": f"{phase}_native_runtime_objects_compiled",
                        "label": f"{phase.title()} native runtime objects compiled",
                        "unit": "count",
                        "phase": phase,
                        "work_items": {language: None for language in languages},
                        "values": native_runtime_objects_compiled,
                    },
                    {
                        "id": f"{phase}_native_runtime_archives_reused",
                        "label": f"{phase.title()} native runtime archives reused",
                        "unit": "count",
                        "phase": phase,
                        "work_items": {language: None for language in languages},
                        "values": native_runtime_archives_reused,
                    },
                    {
                        "id": f"{phase}_native_runtime_archives_rebuilt",
                        "label": f"{phase.title()} native runtime archives rebuilt",
                        "unit": "count",
                        "phase": phase,
                        "work_items": {language: None for language in languages},
                        "values": native_runtime_archives_rebuilt,
                    },
                ]
            )
    return {
        "primary_metric_id": "clean_declarations_per_second",
        "metrics": metrics,
    }


def benchmark_case(
    case: dict[str, Any],
    workload: GeneratedWorkload,
    languages: list[str],
    *,
    warmups: int,
    runs: int,
    timeout: int,
    kain_exe: ResolvedCompiler | None,
    rustc: str,
) -> dict[str, Any]:
    supported_languages = case.get("languages")
    if isinstance(supported_languages, list):
        case_languages = [language for language in languages if language in supported_languages]
    else:
        case_languages = list(languages)
    results: dict[str, dict[str, Any]] = {}
    for phase in PHASE_ORDER:
        phase_results: dict[str, Any] = {}
        for language in case_languages:
            phase_results[language] = measure_phase(
                case_id=str(case["id"]),
                language=language,
                phase=phase,
                workload=workload,
                warmups=warmups,
                runs=runs,
                timeout=timeout,
                kain_exe=kain_exe,
                rustc=rustc,
            )
        results[phase] = phase_results
    phase_summary = {
        phase: compute_phase_summary(results, phase, case_languages)
        for phase in PHASE_ORDER
    }
    case_result = {
        "id": str(case["id"]),
        "title": str(case.get("title", case["id"])),
        "description": str(case.get("description", "")),
        "generator": workload.generator,
        "languages": case_languages,
        "source_paths": {language: str(workload.entrypoints[language]) for language in case_languages},
        "source_metrics": {language: workload.source_metrics[language] for language in case_languages},
        "abstract_metrics": dict(workload.abstract_metrics),
        "results": results,
        "phase_summary": phase_summary,
    }
    case_result["telemetry"] = compute_case_telemetry(case_result)
    return case_result


def coerce_float(value: Any) -> float | None:
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def metric_by_id(case_result: dict[str, Any], metric_id: str) -> dict[str, Any] | None:
    telemetry = case_result.get("telemetry")
    if not isinstance(telemetry, dict):
        return None
    metrics = telemetry.get("metrics", [])
    if not isinstance(metrics, list):
        return None
    for metric in metrics:
        if isinstance(metric, dict) and metric.get("id") == metric_id:
            return metric
    return None


def fmt_ms(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):.3f}"


def fmt_rate(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):,.3f}"


def fmt_ratio(value: Any) -> str:
    if value is None:
        return "n/a"
    ratio = float(value)
    if ratio <= 1.001:
        return "fastest"
    return f"{ratio:.2f}x slower"


def markdown_table_row(cells: list[str]) -> str:
    escaped = [cell.replace("|", "\\|").replace("\n", " ") for cell in cells]
    return "| " + " | ".join(escaped) + " |"


def render_phase_summary_table(report: dict[str, Any], phase: str) -> str:
    languages = report.get("languages", [])
    header = ["case", "winner"] + [f"{language} median ms" for language in languages] + [f"{language} vs fastest" for language in languages]
    divider = ["---"] * len(header)
    rows = [markdown_table_row(header), markdown_table_row(divider)]
    for case in report.get("cases", []):
        summary = case.get("phase_summary", {}).get(phase, {})
        cells = [case.get("id", ""), str(summary.get("winner", "n/a"))]
        phase_results = case.get("results", {}).get(phase, {})
        for language in languages:
            result = phase_results.get(language, {})
            cells.append(fmt_ms(result.get("median_ms")))
        relative = summary.get("relative_to_fastest", {})
        for language in languages:
            cells.append(fmt_ratio(relative.get(language) if isinstance(relative, dict) else None))
        rows.append(markdown_table_row(cells))
    return "\n".join(rows)


def render_telemetry_table(report: dict[str, Any], metric_id: str) -> str:
    languages = report.get("languages", [])
    metric_label = metric_id
    header = ["case", "metric"] + [f"{language} value" for language in languages]
    divider = ["---"] * len(header)
    rows = [markdown_table_row(header), markdown_table_row(divider)]
    added = False
    for case in report.get("cases", []):
        metric = metric_by_id(case, metric_id)
        if not isinstance(metric, dict):
            continue
        metric_label = str(metric.get("label", metric_id))
        values = metric.get("values", {})
        if not isinstance(values, dict):
            continue
        cells = [case.get("id", ""), metric_label]
        for language in languages:
            cells.append(fmt_rate(values.get(language)))
        rows.append(markdown_table_row(cells))
        added = True
    return "\n".join(rows) if added else ""


def render_toolchain(report: dict[str, Any]) -> str:
    toolchain = report.get("toolchain", {})
    if not isinstance(toolchain, dict):
        return ""
    lines = [
        "## Toolchain",
        "",
        f"- kain_exe: `{toolchain.get('kain_exe', 'n/a')}`",
        f"- kain_source: `{toolchain.get('kain_exe_source', 'n/a')}`",
        f"- kain_version: `{toolchain.get('kain_version', 'n/a')}`",
        f"- kain_refresh_ms: `{fmt_ms(toolchain.get('kain_exe_build_ms'))}`",
        f"- rustc: `{toolchain.get('rustc', 'n/a')}`",
        f"- rustc_version: `{toolchain.get('rustc_version', 'n/a')}`",
        f"- rust_flags: `{json.dumps(toolchain.get('rust_flags', []))}`",
        f"- kain_native_env: `{json.dumps(toolchain.get('kain_native_env', {}), sort_keys=True)}`",
    ]
    return "\n".join(lines)


def render_history_overview(report: dict[str, Any]) -> str:
    history = report.get("history", {})
    if not isinstance(history, dict) or not history.get("enabled"):
        return ""
    database = history.get("database", {})
    current_run = history.get("current_run", {})
    lines = [
        "## History",
        "",
        f"- history_db: `{history.get('db_path', 'n/a')}`",
        f"- comparison_key: `{history.get('comparison_key', '')}`",
        f"- current_run_id: `{current_run.get('run_id', 'pending') if isinstance(current_run, dict) else 'pending'}`",
        f"- total_runs: `{database.get('total_runs', 0) if isinstance(database, dict) else 0}`",
        f"- total_phase_results: `{database.get('total_phase_results', 0) if isinstance(database, dict) else 0}`",
    ]
    return "\n".join(lines)


def render_case_detail(case: dict[str, Any], languages: list[str]) -> str:
    lines = [
        f"### {case.get('id', '')}",
        "",
        f"- title: `{case.get('title', '')}`",
        f"- generator: `{case.get('generator', '')}`",
        f"- description: {case.get('description', '')}",
        f"- abstract_metrics: `{json.dumps(case.get('abstract_metrics', {}), sort_keys=True)}`",
    ]
    source_paths = case.get("source_paths", {})
    source_metrics = case.get("source_metrics", {})
    for language in languages:
        if language not in case.get("languages", []):
            continue
        lines.append(f"- {language}_source: `{source_paths.get(language, '')}`")
        lines.append(
            f"- {language}_source_metrics: `{json.dumps(source_metrics.get(language, {}), sort_keys=True)}`"
        )
    for phase in PHASE_ORDER:
        summary = case.get("phase_summary", {}).get(phase, {})
        lines.append(f"- {phase}_winner: `{summary.get('winner', 'n/a')}`")
        for language in languages:
            result = case.get("results", {}).get(phase, {}).get(language)
            if not isinstance(result, dict):
                continue
            lines.append(
                f"- {phase}_{language}: `median={fmt_ms(result.get('median_ms'))} ms, artifact_bytes={result.get('artifact_bytes', 'n/a')}, verify_ok={result.get('verify_ok', False)}`"
            )
            lines.append(f"- {phase}_{language}_command: `{display_command(result.get('command'))}`")
            if result.get("env"):
                lines.append(f"- {phase}_{language}_env: `{json.dumps(result.get('env', {}), sort_keys=True)}`")
            if result.get("native_runtime_cache"):
                lines.append(
                    f"- {phase}_{language}_native_runtime_cache: `{json.dumps(result.get('native_runtime_cache', {}), sort_keys=True)}`"
                )
            if result.get("stability_note"):
                lines.append(f"- {phase}_{language}_stability: `{result.get('stability_note', '')}`")
            if result.get("error"):
                lines.append(f"- {phase}_{language}_error: `{result.get('error', '')}`")
    return "\n".join(lines)


def render_llm_report(report: dict[str, Any]) -> str:
    languages = report.get("languages", [])
    latest_stem = report.get("latest_stem", DEFAULT_LATEST_REPORT_STEM)
    lines = [
        "# Kain Compiler Benchmark Report",
        "",
        f"- status: `{'PASS' if report.get('ok') else 'FAIL'}`",
        f"- generated_at: `{report.get('generated_at', 'n/a')}`",
        f"- suite: `{report.get('suite', 'n/a')}`",
        f"- platform: `{report.get('platform', 'n/a')}`",
        f"- warmups: `{report.get('warmups', 'n/a')}`",
        f"- timed_runs: `{report.get('runs', 'n/a')}`",
        f"- languages: `{', '.join(languages)}`",
        f"- json_report: `benchmark/out/reports/{latest_stem}.json`",
        "",
    ]
    if report.get("fatal_error"):
        lines.extend(["## Fatal Error", "", str(report["fatal_error"]), ""])
    lines.extend([render_toolchain(report), ""])
    history_overview = render_history_overview(report)
    if history_overview:
        lines.extend([history_overview, ""])
    lines.extend(["## Clean Compile Summary", "", render_phase_summary_table(report, "clean"), ""])
    lines.extend(["## Rebuild Summary", "", render_phase_summary_table(report, "rebuild"), ""])
    for metric_id in (
        "clean_declarations_per_second",
        "rebuild_declarations_per_second",
        "clean_source_lines_per_second",
        "rebuild_source_lines_per_second",
        "clean_native_runtime_objects_compiled",
        "rebuild_native_runtime_objects_reused",
    ):
        table = render_telemetry_table(report, metric_id)
        if table:
            metric = metric_by_id(report.get("cases", [{}])[0], metric_id) if report.get("cases") else None
            label = metric.get("label", metric_id) if isinstance(metric, dict) else metric_id
            lines.extend([f"## Telemetry: {label}", "", table, ""])
    lines.extend(["## Case Details", ""])
    for case in report.get("cases", []):
        lines.extend([render_case_detail(case, languages), ""])
    return "\n".join(lines).rstrip() + "\n"


def render_minimal_report(report: dict[str, Any], minimal_name: str, latest_stem: str) -> str:
    languages = report.get("languages", [])
    lines = [
        "# Kain Compiler Benchmark Snapshot",
        "",
        f"- status: `{'PASS' if report.get('ok') else 'FAIL'}`",
        f"- generated_at: `{report.get('generated_at', 'n/a')}`",
        f"- warmups: `{report.get('warmups', 'n/a')}`",
        f"- timed_runs: `{report.get('runs', 'n/a')}`",
        f"- languages: `{', '.join(languages)}`",
        f"- root_snapshot: `benchmark/{minimal_name}`",
        f"- full_report: `benchmark/out/reports/{latest_stem}.llm.md`",
        f"- json_report: `benchmark/out/reports/{latest_stem}.json`",
        "",
        "## Clean Compile Summary",
        "",
        render_phase_summary_table(report, "clean"),
        "",
        "## Rebuild Summary",
        "",
        render_phase_summary_table(report, "rebuild"),
    ]
    history_overview = render_history_overview(report)
    if history_overview:
        lines.extend(["", history_overview])
    return "\n".join(lines).rstrip() + "\n"


def write_reports(report: dict[str, Any], *, minimal_name: str, latest_stem: str) -> dict[str, Path]:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    latest_stem = latest_report_stem(latest_stem)
    json_text = json.dumps(report, indent=2)
    llm_text = render_llm_report(report)
    minimal_text = render_minimal_report(report, minimal_name, latest_stem)
    stamp_prefix = stamp if latest_stem == DEFAULT_LATEST_REPORT_STEM else f"{stamp}.{latest_stem}"
    json_path = REPORT_ROOT / f"{stamp_prefix}.json"
    llm_path = REPORT_ROOT / f"{stamp_prefix}.llm.md"
    latest_json = REPORT_ROOT / f"{latest_stem}.json"
    latest_llm = REPORT_ROOT / f"{latest_stem}.llm.md"
    latest_minimal = root_snapshot_path(minimal_name)
    json_path.write_text(json_text, encoding="utf-8")
    latest_json.write_text(json_text, encoding="utf-8")
    llm_path.write_text(llm_text, encoding="utf-8")
    latest_llm.write_text(llm_text, encoding="utf-8")
    latest_minimal.write_text(minimal_text, encoding="utf-8")
    return {
        "timestamped_json": json_path,
        "latest_json": latest_json,
        "timestamped_llm": llm_path,
        "latest_llm": latest_llm,
        "latest_minimal": latest_minimal,
    }


def history_comparison_payload(report: dict[str, Any]) -> dict[str, Any]:
    selected = [
        {
            "id": case.get("id", ""),
            "languages": list(case.get("languages", [])),
            "generator": case.get("generator", ""),
            "abstract_metrics": case.get("abstract_metrics", {}),
        }
        for case in report.get("cases", [])
    ]
    selected.sort(key=lambda item: str(item["id"]))
    return {
        "suite": report.get("suite", ""),
        "latest_stem": report.get("latest_stem", DEFAULT_LATEST_REPORT_STEM),
        "platform": report.get("platform", ""),
        "machine_key": history_machine_key(),
        "warmups": report.get("warmups"),
        "runs": report.get("runs"),
        "languages": list(report.get("languages", [])),
        "selected_cases": selected,
    }


def history_comparison_key(report: dict[str, Any]) -> str:
    return sha256_text(stable_json_text(history_comparison_payload(report)))


def open_history_database(path: Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(path)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA journal_mode = WAL")
    connection.execute("PRAGMA synchronous = NORMAL")
    ensure_history_schema(connection)
    return connection


def ensure_history_schema(connection: sqlite3.Connection) -> None:
    version = int(connection.execute("PRAGMA user_version").fetchone()[0])
    if version not in {0, COMPILER_HISTORY_SCHEMA_VERSION}:
        raise RuntimeError(
            f"compiler history schema version mismatch: found {version}, expected {COMPILER_HISTORY_SCHEMA_VERSION}"
        )
    connection.executescript(
        """
        CREATE TABLE IF NOT EXISTS compiler_runs (
            run_id INTEGER PRIMARY KEY AUTOINCREMENT,
            suite TEXT NOT NULL,
            generated_at TEXT NOT NULL,
            platform TEXT NOT NULL,
            latest_stem TEXT NOT NULL,
            minimal_name TEXT NOT NULL,
            manifest_path TEXT NOT NULL,
            case_filter TEXT NOT NULL,
            warmups INTEGER NOT NULL,
            timed_runs INTEGER NOT NULL,
            ok INTEGER NOT NULL,
            comparison_key TEXT NOT NULL,
            machine_key TEXT NOT NULL,
            machine_json TEXT NOT NULL,
            git_json TEXT NOT NULL,
            toolchain_json TEXT NOT NULL,
            languages_json TEXT NOT NULL,
            report_latest_json TEXT NOT NULL,
            report_latest_llm TEXT NOT NULL,
            report_timestamped_json TEXT NOT NULL,
            report_timestamped_llm TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_compiler_runs_comparison
            ON compiler_runs (comparison_key, generated_at DESC, run_id DESC);

        CREATE TABLE IF NOT EXISTS compiler_case_results (
            run_id INTEGER NOT NULL,
            case_id TEXT NOT NULL,
            title TEXT NOT NULL,
            description TEXT NOT NULL,
            generator TEXT NOT NULL,
            languages_json TEXT NOT NULL,
            abstract_metrics_json TEXT NOT NULL,
            source_metrics_json TEXT NOT NULL,
            telemetry_json TEXT NOT NULL,
            PRIMARY KEY (run_id, case_id),
            FOREIGN KEY (run_id) REFERENCES compiler_runs(run_id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS compiler_phase_results (
            run_id INTEGER NOT NULL,
            case_id TEXT NOT NULL,
            language TEXT NOT NULL,
            phase TEXT NOT NULL,
            ok INTEGER NOT NULL,
            verify_ok INTEGER NOT NULL,
            median_ms REAL,
            mean_ms REAL,
            min_ms REAL,
            max_ms REAL,
            stdev_ms REAL,
            coefficient_of_variation REAL,
            max_to_median_ratio REAL,
            unstable INTEGER NOT NULL,
            artifact_bytes INTEGER,
            command_json TEXT NOT NULL,
            env_json TEXT NOT NULL,
            samples_json TEXT NOT NULL,
            warmups_json TEXT NOT NULL,
            error_text TEXT NOT NULL,
            primary_metric_id TEXT NOT NULL,
            primary_metric_label TEXT NOT NULL,
            primary_metric_unit TEXT NOT NULL,
            primary_metric_value REAL,
            PRIMARY KEY (run_id, case_id, language, phase),
            FOREIGN KEY (run_id, case_id) REFERENCES compiler_case_results(run_id, case_id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_compiler_phase_results_lookup
            ON compiler_phase_results (case_id, language, phase, run_id DESC);
        """
    )
    if version == 0:
        connection.execute(f"PRAGMA user_version = {COMPILER_HISTORY_SCHEMA_VERSION}")


def history_database_summary(connection: sqlite3.Connection) -> dict[str, Any]:
    total_runs = int(connection.execute("SELECT COUNT(*) FROM compiler_runs").fetchone()[0])
    total_cases = int(connection.execute("SELECT COUNT(*) FROM compiler_case_results").fetchone()[0])
    total_phase_results = int(connection.execute("SELECT COUNT(*) FROM compiler_phase_results").fetchone()[0])
    return {
        "schema_version": COMPILER_HISTORY_SCHEMA_VERSION,
        "total_runs": total_runs,
        "total_case_results": total_cases,
        "total_phase_results": total_phase_results,
    }


def case_primary_metric_value(case: dict[str, Any], language: str) -> tuple[str, str, str, float | None]:
    telemetry = case.get("telemetry", {})
    if not isinstance(telemetry, dict):
        return "", "", "", None
    metric_id = str(telemetry.get("primary_metric_id", ""))
    metric = metric_by_id(case, metric_id)
    if not isinstance(metric, dict):
        return metric_id, "", "", None
    values = metric.get("values", {})
    value = values.get(language) if isinstance(values, dict) else None
    return (
        str(metric.get("id", metric_id)),
        str(metric.get("label", "")),
        str(metric.get("unit", "")),
        coerce_float(value),
    )


def persist_report_history(
    report: dict[str, Any],
    history_db_path: Path,
    outputs: dict[str, Path],
    *,
    manifest_path: str,
    raw_case_filter: str,
    minimal_name: str,
) -> dict[str, Any]:
    comparison_key = history_comparison_key(report)
    machine = machine_fingerprint()
    git = report.get("git", {})
    if not isinstance(git, dict):
        git = git_metadata()
    with open_history_database(history_db_path) as connection:
        cursor = connection.execute(
            """
            INSERT INTO compiler_runs (
                suite, generated_at, platform, latest_stem, minimal_name, manifest_path, case_filter,
                warmups, timed_runs, ok, comparison_key, machine_key, machine_json, git_json,
                toolchain_json, languages_json, report_latest_json, report_latest_llm,
                report_timestamped_json, report_timestamped_llm
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                str(report.get("suite", "")),
                str(report.get("generated_at", "")),
                str(report.get("platform", "")),
                str(report.get("latest_stem", DEFAULT_LATEST_REPORT_STEM)),
                minimal_name,
                manifest_path,
                raw_case_filter,
                int(report.get("warmups", 0) or 0),
                int(report.get("runs", 0) or 0),
                1 if report.get("ok") else 0,
                comparison_key,
                history_machine_key(),
                stable_json_text(machine),
                stable_json_text(git),
                stable_json_text(report.get("toolchain", {})),
                stable_json_text(report.get("languages", [])),
                str(outputs["latest_json"].resolve()),
                str(outputs["latest_llm"].resolve()),
                str(outputs["timestamped_json"].resolve()),
                str(outputs["timestamped_llm"].resolve()),
            ),
        )
        run_id = int(cursor.lastrowid)
        for case in report.get("cases", []):
            connection.execute(
                """
                INSERT INTO compiler_case_results (
                    run_id, case_id, title, description, generator, languages_json,
                    abstract_metrics_json, source_metrics_json, telemetry_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    run_id,
                    str(case.get("id", "")),
                    str(case.get("title", "")),
                    str(case.get("description", "")),
                    str(case.get("generator", "")),
                    stable_json_text(case.get("languages", [])),
                    stable_json_text(case.get("abstract_metrics", {})),
                    stable_json_text(case.get("source_metrics", {})),
                    stable_json_text(case.get("telemetry", {})),
                ),
            )
            for language in case.get("languages", []):
                primary_metric_id, primary_metric_label, primary_metric_unit, primary_metric_value = case_primary_metric_value(case, language)
                for phase in PHASE_ORDER:
                    result = case.get("results", {}).get(phase, {}).get(language, {})
                    connection.execute(
                        """
                        INSERT INTO compiler_phase_results (
                            run_id, case_id, language, phase, ok, verify_ok, median_ms, mean_ms, min_ms,
                            max_ms, stdev_ms, coefficient_of_variation, max_to_median_ratio, unstable,
                            artifact_bytes, command_json, env_json, samples_json, warmups_json, error_text,
                            primary_metric_id, primary_metric_label, primary_metric_unit, primary_metric_value
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            run_id,
                            str(case.get("id", "")),
                            str(language),
                            phase,
                            1 if result.get("ok") else 0,
                            1 if result.get("verify_ok") else 0,
                            coerce_float(result.get("median_ms")),
                            coerce_float(result.get("mean_ms")),
                            coerce_float(result.get("min_ms")),
                            coerce_float(result.get("max_ms")),
                            coerce_float(result.get("stdev_ms")),
                            coerce_float(result.get("coefficient_of_variation")),
                            coerce_float(result.get("max_to_median_ratio")),
                            1 if result.get("unstable") else 0,
                            int(result.get("artifact_bytes")) if result.get("artifact_bytes") is not None else None,
                            stable_json_text(result.get("command", [])),
                            stable_json_text(result.get("env", {})),
                            stable_json_text(result.get("samples_ms", [])),
                            stable_json_text(result.get("warmup_samples_ms", [])),
                            str(result.get("error", "")),
                            primary_metric_id,
                            primary_metric_label,
                            primary_metric_unit,
                            primary_metric_value,
                        ),
                    )
        connection.commit()
        database = history_database_summary(connection)
        return {
            "enabled": True,
            "db_path": str(history_db_path),
            "comparison_key": comparison_key,
            "database": database,
            "current_run": {
                "run_id": run_id,
                "generated_at": str(report.get("generated_at", "")),
                "git_commit": str(git.get("commit", "")),
                "git_branch": str(git.get("branch", "")),
            },
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--case", dest="only_case", help="Single case id or comma-separated case ids")
    parser.add_argument("--languages", help="Comma-separated subset: kain,rust")
    parser.add_argument("--runs", type=int)
    parser.add_argument("--warmups", type=int)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--kain-exe")
    parser.add_argument("--rustc", default=os.environ.get("RUSTC", "rustc"))
    parser.add_argument("--minimal-name")
    parser.add_argument("--latest-stem")
    parser.add_argument(
        "--history-db",
        default=str(DEFAULT_HISTORY_DB_PATH),
        help="SQLite history database path. Pass off to disable persistent compiler benchmark history.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = load_manifest(Path(args.manifest))
    default_languages = args.languages
    if default_languages is None:
        manifest_languages = manifest.get("default_languages")
        if isinstance(manifest_languages, list):
            default_languages = ",".join(str(language) for language in manifest_languages)
        elif isinstance(manifest_languages, str):
            default_languages = manifest_languages
    languages = parse_languages(default_languages)
    warmups = args.warmups if args.warmups is not None else int(manifest.get("default_warmups", 0))
    runs = args.runs if args.runs is not None else int(manifest.get("default_runs", 3))
    minimal_name = args.minimal_name or str(manifest.get("minimal_name", DEFAULT_MINIMAL_REPORT_NAME))
    latest_stem = args.latest_stem or str(manifest.get("latest_stem", DEFAULT_LATEST_REPORT_STEM))
    history_db_path = resolve_history_db_path(args.history_db)
    git_info = git_metadata()
    report: dict[str, Any] = {
        "suite": str(manifest.get("suite", "kain-compiler-benchmarks")),
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "platform": sys.platform,
        "warmups": warmups,
        "runs": runs,
        "languages": languages,
        "latest_stem": latest_stem,
        "cases": [],
        "ok": False,
        "toolchain": {},
        "git": git_info,
        "history": {
            "enabled": bool(history_db_path),
            "db_path": str(history_db_path) if history_db_path else "",
        },
    }
    outputs: dict[str, Path] | None = None
    try:
        kain_exe = resolve_kain_exe(args.kain_exe, args.timeout) if "kain" in languages else None
        rustc = resolve_tool(args.rustc, "RUSTC", "rustc")
        report["toolchain"] = {
            "kain_exe": str(kain_exe.path) if kain_exe else "not selected",
            "kain_exe_source": kain_exe.source if kain_exe else "not selected",
            "kain_exe_build_command": kain_exe.build_command if kain_exe else None,
            "kain_exe_build_ms": kain_exe.build_ms if kain_exe else None,
            "kain_version": optional_command_output([str(kain_exe.path), "--version"], timeout=30) if kain_exe else "",
            "kain_native_env": dict(KAIN_NATIVE_ENV),
            "rustc": rustc,
            "rustc_version": optional_command_output([rustc, "--version"], timeout=30),
            "rust_flags": list(RUST_RELEASE_FLAGS),
        }
        for case in selected_cases(manifest, args.only_case):
            case_languages = case.get("languages")
            selected_case_languages = languages
            if isinstance(case_languages, list):
                selected_case_languages = [language for language in languages if language in case_languages]
            print(f"[compiler-bench] {case['id']} ({', '.join(selected_case_languages)})")
            workload = generate_case_workload(case, selected_case_languages)
            report["cases"].append(
                benchmark_case(
                    case,
                    workload,
                    selected_case_languages,
                    warmups=warmups,
                    runs=runs,
                    timeout=args.timeout,
                    kain_exe=kain_exe,
                    rustc=rustc,
                )
            )
        report["ok"] = all(
            result.get("ok")
            for case in report.get("cases", [])
            for phase in PHASE_ORDER
            for result in case.get("results", {}).get(phase, {}).values()
        )
    except Exception as exc:
        report["fatal_error"] = str(exc)
        report["ok"] = False
        print(f"[compiler-bench] fatal: {exc}", file=sys.stderr)
    finally:
        outputs = write_reports(report, minimal_name=minimal_name, latest_stem=latest_stem)
        if history_db_path:
            try:
                persisted = persist_report_history(
                    report,
                    history_db_path,
                    outputs,
                    manifest_path=str(Path(args.manifest).resolve()),
                    raw_case_filter=args.only_case or "",
                    minimal_name=minimal_name,
                )
                history = report.get("history", {})
                if not isinstance(history, dict):
                    history = {}
                history.update(persisted)
                report["history"] = history
                outputs = write_reports(report, minimal_name=minimal_name, latest_stem=latest_stem)
            except Exception as exc:
                history = report.get("history", {})
                if not isinstance(history, dict):
                    history = {}
                history["enabled"] = True
                history["db_path"] = str(history_db_path)
                history["error"] = str(exc)
                report["history"] = history
                outputs = write_reports(report, minimal_name=minimal_name, latest_stem=latest_stem)
        print(f"[compiler-bench] report: {outputs['latest_llm']}")
        print(f"[compiler-bench] snapshot: {outputs['latest_minimal']}")
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
