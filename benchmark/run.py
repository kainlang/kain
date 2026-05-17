#!/usr/bin/env python3
"""
Kain multi-language benchmark runner.

The benchmark cases stay dependency-free. This runner uses only the Python
standard library for orchestration, timing, JSON, and LLM-readable report output.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


BENCHMARK_ROOT = Path(__file__).resolve().parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build"
REPORT_ROOT = OUT_ROOT / "reports"
BASELINE_ROOT = OUT_ROOT / "baselines"
NATIVE_CORE_RUNTIME_MANIFEST = REPO_ROOT / "runtime" / "native_core_runtime.toml"
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
FFI_SHARED_CASE_ID = "ffi_shared_call_stress"
DEFAULT_MINIMAL_REPORT_NAME = "latest.md"
DEFAULT_LATEST_REPORT_STEM = "latest"
BASELINE_CACHE_SCHEMA_VERSION = 1
CASE_WORKLOAD_FINGERPRINT_IGNORED_KEYS = {"default_enabled"}

LANGUAGE_ORDER = ["kain", "rust", "cpp", "zig", "go", "erlang", "javascript", "python"]
LANGUAGE_LABELS = {
    "kain": "Kain LLVM",
    "rust": "Rust LLVM",
    "cpp": "C++ Clang",
    "zig": "Zig ReleaseFast",
    "go": "Go gc",
    "erlang": "Erlang OTP",
    "javascript": "JavaScript Node",
    "python": "Python CPython",
}
LANGUAGE_SOURCE_KEYS = {
    "kain": "kain",
    "rust": "rust",
    "cpp": "cpp",
    "zig": "zig",
    "go": "go",
    "erlang": "erlang",
    "javascript": "javascript",
    "python": "python",
}


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


@dataclass
class ResolvedExecutable:
    path: Path
    source: str
    build_command: list[str] | None = None


RUST_RELEASE_FLAGS = [
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

CPP_RELEASE_FLAGS = [
    "-std=c++20",
    "-O3",
    "-march=native",
    "-DNDEBUG",
]

GO_RELEASE_FLAGS = [
    "-trimpath",
    "-ldflags=-s -w",
]

ZIG_RELEASE_FLAGS = [
    "build-exe",
    "-O",
    "ReleaseFast",
]

KAIN_NATIVE_PROFILE_DEFAULTS: dict[str, dict[str, str]] = {
    "debug": {
        "KAIN_NATIVE_PROFILE": "debug",
        "KAIN_NATIVE_OPT_LEVEL": "0",
        "KAIN_NATIVE_TARGET_CPU": "",
        "KAIN_NATIVE_DEBUG_INFO": "1",
    },
    "release": {
        "KAIN_NATIVE_PROFILE": "release",
        "KAIN_NATIVE_OPT_LEVEL": "2",
        "KAIN_NATIVE_TARGET_CPU": "",
        "KAIN_NATIVE_DEBUG_INFO": "0",
    },
    "benchmark-release": {
        "KAIN_NATIVE_PROFILE": "benchmark-release",
        "KAIN_NATIVE_OPT_LEVEL": "3",
        "KAIN_NATIVE_TARGET_CPU": "native",
        "KAIN_NATIVE_DEBUG_INFO": "0",
    },
}


def strip_ansi(value: str) -> str:
    return ANSI_RE.sub("", value)


def repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def display_command(command: list[str] | None) -> str:
    if not command:
        return "n/a"
    return " ".join(command)


def run_command(
    command: list[str],
    timeout: int,
    cwd: Path = REPO_ROOT,
    env_overrides: dict[str, str] | None = None,
) -> CommandResult:
    start = time.perf_counter_ns()
    env = None
    if env_overrides:
        env = os.environ.copy()
        env.update(env_overrides)
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


def find_line_that_looks_like_path(output: str) -> str | None:
    for raw_line in reversed(output.splitlines()):
        line = strip_ansi(raw_line).strip()
        if not line:
            continue
        if ":" in line or line.startswith("/") or line.startswith("\\"):
            return line
    return None


def executable_name(stem: str) -> str:
    if os.name == "nt":
        return f"{stem}.exe"
    return stem


def dynamic_library_name(stem: str) -> str:
    if os.name == "nt":
        return f"{stem}.dll"
    if sys.platform == "darwin":
        return f"lib{stem}.dylib"
    return f"lib{stem}.so"


def shared_link_artifact_name(stem: str) -> str | None:
    if os.name == "nt":
        return f"{stem}.lib"
    return None


def resolve_kain_exe(explicit: str | None, timeout: int) -> ResolvedExecutable:
    candidates: list[ResolvedExecutable] = []

    if explicit:
        candidates.append(ResolvedExecutable(Path(explicit), "explicit --kain-exe"))

    env_kain = os.environ.get("KAIN_EXE")
    if env_kain:
        candidates.append(ResolvedExecutable(Path(env_kain), "KAIN_EXE"))

    bazel = shutil.which("bazel")
    compiler_timeout = max(timeout, 1200)
    if bazel:
        build_command = [bazel, "build", "//:kain", "--config=release"]
        build = run_command(build_command, timeout=compiler_timeout)
        info = run_command(
            [bazel, "info", "bazel-bin", "--config=release"],
            timeout=compiler_timeout,
        )
        info_line = find_line_that_looks_like_path(info.stdout)
        if info_line:
            candidates.append(
                ResolvedExecutable(
                    Path(info_line) / "crates" / "cli" / executable_name("kain"),
                    "bazel --config=release",
                    build_command,
                )
            )
        if build.returncode != 0 and not any(candidate.path.exists() for candidate in candidates):
            combined = (build.stdout + "\n" + build.stderr).strip()
            raise RuntimeError(f"Unable to build //:kain with Bazel.\n{combined}")

    for candidate in candidates:
        if candidate.path.exists():
            candidate.path = candidate.path.resolve()
            return candidate

    cargo_release = shutil.which("cargo")
    if cargo_release:
        build_command = [cargo_release, "build", "--release", "-p", "cli"]
        build = run_command(build_command, timeout=compiler_timeout)
        release_candidate = REPO_ROOT / "target" / "release" / executable_name("kain")
        if release_candidate.exists() and build.returncode == 0:
            return ResolvedExecutable(
                release_candidate.resolve(),
                "cargo --release -p cli",
                build_command,
            )

    candidates.extend(
        [
            ResolvedExecutable(REPO_ROOT / "target" / "release" / executable_name("kain"), "target/release"),
            ResolvedExecutable(REPO_ROOT / "target" / "debug" / executable_name("kain"), "target/debug"),
        ]
    )

    path_kain = shutil.which("kain")
    if path_kain:
        candidates.append(ResolvedExecutable(Path(path_kain), "PATH kain"))

    for candidate in candidates:
        if candidate.path.exists():
            candidate.path = candidate.path.resolve()
            return candidate

    raise RuntimeError("Could not find kain compiler. Set KAIN_EXE or pass --kain-exe.")


def resolve_tool(explicit: str | None, env_key: str, default_name: str) -> str:
    requested = explicit or os.environ.get(env_key) or default_name
    return shutil.which(requested) or requested


def resolve_cpp_compiler(explicit: str | None) -> str:
    if explicit or os.environ.get("CXX"):
        return resolve_tool(explicit, "CXX", "clang++")

    bundled = REPO_ROOT / "toolchain" / "llvm" / "bin" / executable_name("clang++")
    if bundled.exists():
        return str(bundled.resolve())

    for candidate in ("clang++", "g++", "c++"):
        found = shutil.which(candidate)
        if found:
            return found

    return "clang++"


def resolve_clang(explicit: str | None) -> str:
    if explicit or os.environ.get("CLANG"):
        return resolve_tool(explicit, "CLANG", "clang")

    bundled = REPO_ROOT / "toolchain" / "llvm" / "bin" / executable_name("clang")
    if bundled.exists():
        return str(bundled.resolve())

    found = shutil.which("clang")
    if found:
        return found
    return "clang"


def resolve_erlang_tool(explicit: str | None, env_key: str, default_name: str) -> str:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_path = os.environ.get(env_key)
    if env_path:
        candidates.append(Path(env_path))
    if os.name == "nt":
        candidates.append(Path("C:/Program Files/Erlang OTP/bin") / executable_name(default_name))
        candidates.append(Path("C:/Program Files/Erlang OTP/erts-17.0/bin") / executable_name(default_name))
    path_candidate = shutil.which(default_name)
    if path_candidate:
        candidates.append(Path(path_candidate))
    for candidate in candidates:
        if candidate.exists():
            return str(candidate.resolve())
    return default_name


def resolved_kain_native_tuning(args: argparse.Namespace) -> dict[str, str]:
    profile = args.kain_native_profile
    defaults = KAIN_NATIVE_PROFILE_DEFAULTS[profile]
    opt_level = args.kain_native_opt_level.strip() if args.kain_native_opt_level else ""
    target_cpu = args.kain_native_target_cpu.strip() if args.kain_native_target_cpu else ""
    debug_info = args.kain_native_debug_info.strip() if args.kain_native_debug_info else ""
    return {
        "profile": defaults["KAIN_NATIVE_PROFILE"],
        "opt_level": opt_level or defaults["KAIN_NATIVE_OPT_LEVEL"],
        "target_cpu": target_cpu or defaults["KAIN_NATIVE_TARGET_CPU"],
        "debug_info": debug_info or defaults["KAIN_NATIVE_DEBUG_INFO"],
    }


def resolved_kain_runtime_manifest(case: dict[str, Any]) -> Path:
    runtime_manifest = case.get("kain_runtime_manifest")
    if runtime_manifest is None:
        return NATIVE_CORE_RUNTIME_MANIFEST
    return (REPO_ROOT / str(runtime_manifest)).resolve()


def kain_native_env_from_tuning(
    tuning: dict[str, str], runtime_manifest: Path = NATIVE_CORE_RUNTIME_MANIFEST
) -> dict[str, str]:
    env = {
        "KAIN_NATIVE_PROFILE": tuning["profile"],
        "KAIN_NATIVE_OPT_LEVEL": tuning["opt_level"],
        "KAIN_NATIVE_DEBUG_INFO": tuning["debug_info"],
        "KAIN_RUNTIME_MANIFEST_PATH": str(runtime_manifest),
    }
    if tuning["target_cpu"]:
        env["KAIN_NATIVE_TARGET_CPU"] = tuning["target_cpu"]
    return env


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    include_manifest = manifest.get("include_manifest")
    if include_manifest:
        include_path = (BENCHMARK_ROOT / str(include_manifest)).resolve()
        included_manifest = load_manifest(include_path)
        included_cases = included_manifest.get("cases", [])
        included_by_id = {
            case["id"]: case
            for case in included_cases
            if isinstance(case, dict) and "id" in case
        }
        requested_case_ids = manifest.get("case_ids")
        override_cases = {
            case["id"]: case
            for case in manifest.get("cases", [])
            if isinstance(case, dict) and "id" in case
        }
        if requested_case_ids is None:
            selected_case_ids = [case["id"] for case in included_cases if isinstance(case, dict) and "id" in case]
        else:
            selected_case_ids = [str(case_id) for case_id in requested_case_ids]
        selected_cases: list[dict[str, Any]] = []
        missing_case_ids = [case_id for case_id in selected_case_ids if case_id not in included_by_id]
        if missing_case_ids:
            raise ValueError(
                f"unknown included case(s) in {path.name}: {', '.join(missing_case_ids)}"
            )
        for case_id in selected_case_ids:
            merged_case = dict(included_by_id[case_id])
            override_case = override_cases.get(case_id)
            if override_case:
                for key, value in override_case.items():
                    if key in {"languages", "language_notes"} and isinstance(merged_case.get(key), dict) and isinstance(value, dict):
                        merged_case[key] = {**merged_case[key], **value}
                    else:
                        merged_case[key] = value
            selected_cases.append(merged_case)
        manifest["cases"] = selected_cases
    if "cases" not in manifest or not isinstance(manifest["cases"], list):
        raise ValueError("manifest must contain a cases array")
    return manifest


def stable_json_text(payload: Any) -> str:
    return json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def machine_fingerprint() -> dict[str, Any]:
    uname = platform.uname()
    return {
        "platform": sys.platform,
        "system": uname.system,
        "release": uname.release,
        "version": uname.version,
        "machine": uname.machine,
        "processor": uname.processor,
        "python": sys.version,
        "cpu_count": os.cpu_count(),
        "computer_name": os.environ.get("COMPUTERNAME", ""),
    }


def path_stat_descriptor(path: Path) -> dict[str, Any]:
    descriptor = {"path": str(path)}
    if path.exists():
        stat = path.stat()
        descriptor.update(
            {
                "resolved": str(path.resolve()),
                "size": stat.st_size,
                "mtime_ns": stat.st_mtime_ns,
            }
        )
    return descriptor


def tool_descriptor(tool: Any) -> dict[str, Any]:
    if isinstance(tool, ResolvedExecutable):
        descriptor = path_stat_descriptor(tool.path)
        descriptor["source"] = tool.source
        if tool.build_command:
            descriptor["build_command"] = tool.build_command
        return descriptor
    if isinstance(tool, Path):
        return path_stat_descriptor(tool)
    if isinstance(tool, str):
        return path_stat_descriptor(Path(tool))
    return {"value": str(tool)}


def fingerprint_tree(path: Path) -> dict[str, Any]:
    path = path.resolve()
    if path.is_file():
        return {
            "type": "file",
            "path": repo_relative(path),
            "sha256": sha256_file(path),
            "size": path.stat().st_size,
        }
    if path.is_dir():
        entries: list[dict[str, Any]] = []
        for file_path in sorted(item for item in path.rglob("*") if item.is_file()):
            relative_parts = file_path.relative_to(path).parts
            if any(part in {"target", "__pycache__", ".kain", ".git"} for part in relative_parts):
                continue
            entries.append(
                {
                    "path": str(Path(*relative_parts)).replace("\\", "/"),
                    "sha256": sha256_file(file_path),
                    "size": file_path.stat().st_size,
                }
            )
        return {
            "type": "directory",
            "path": repo_relative(path),
            "entries": entries,
        }
    return {
        "type": "missing",
        "path": repo_relative(path),
    }


def case_workload_fingerprint(case: dict[str, Any], language: str) -> dict[str, Any]:
    source_path = case_source_path(case, language)
    rust_manifest = case.get("rust_manifest") if language == "rust" else None
    go_manifest = case.get("go_manifest") if language == "go" else None
    primary_path = source_path
    if rust_manifest:
        primary_path = (BENCHMARK_ROOT / str(rust_manifest)).resolve().parent
    elif go_manifest:
        primary_path = (BENCHMARK_ROOT / str(go_manifest)).resolve().parent
    fingerprint: dict[str, Any] = {
        "case": {
            key: value
            for key, value in case.items()
            if key not in CASE_WORKLOAD_FINGERPRINT_IGNORED_KEYS
        },
        "language": language,
        "primary": fingerprint_tree(primary_path),
    }
    if case["id"] == FFI_SHARED_CASE_ID:
        fingerprint["ffi_shared_support"] = fingerprint_tree(
            BENCHMARK_ROOT / "ffi_boundary" / "native" / "ffi_boundary.c"
        )
    return fingerprint


def build_flags_descriptor(case: dict[str, Any], language: str) -> dict[str, Any]:
    if language == "rust":
        flags: dict[str, Any] = {"flags": RUST_RELEASE_FLAGS}
        if case.get("rust_manifest"):
            flags["cargo_mode"] = True
            flags["rust_manifest"] = str(case.get("rust_manifest"))
            flags["rust_package"] = str(case.get("rust_package", case["id"].replace("_", "-")))
            flags["rust_binary"] = str(case.get("rust_binary", flags["rust_package"]))
        return flags
    if language == "cpp":
        return {
            "flags": CPP_RELEASE_FLAGS,
            "extra_flags": cpp_extra_flags_for_case(case["id"]),
            "link_flags": cpp_link_flags_for_case(case["id"]),
        }
    if language == "zig":
        return {
            "flags": ZIG_RELEASE_FLAGS,
            "link_flags": zig_link_flags_for_case(case["id"]),
        }
    if language == "go":
        flags = {"flags": GO_RELEASE_FLAGS}
        if case.get("go_manifest"):
            flags["go_manifest"] = str(case.get("go_manifest"))
            flags["go_package"] = str(case.get("go_package", "."))
            flags["go_binary"] = str(case.get("go_binary", case["id"]))
        return flags
    if language == "erlang":
        return {"erlang_module": str(case.get("erlang_module", case_source_path(case, "erlang").stem))}
    return {}


def baseline_cache_path(case_id: str, language: str) -> Path:
    return BASELINE_ROOT / case_id / f"{language}.json"


def baseline_mode_uses_cache(mode: str, selected_languages: list[str], language: str) -> bool:
    if language == "kain":
        return False
    if mode == "off":
        return False
    if mode in {"reuse-foreign", "refresh-foreign"}:
        return True
    if mode == "auto":
        return "kain" in selected_languages
    raise ValueError(f"unknown baseline mode: {mode}")


def baseline_cache_key(
    case: dict[str, Any],
    language: str,
    tools: dict[str, Any],
    warmups: int,
    runs: int,
) -> str:
    payload = {
        "schema_version": BASELINE_CACHE_SCHEMA_VERSION,
        "machine": machine_fingerprint(),
        "tool": tool_descriptor(tools.get("kain") if language == "kain" else tools.get(language) or tools.get(f"{language}c")),
        "workload": case_workload_fingerprint(case, language),
        "build_flags": build_flags_descriptor(case, language),
        "measurement": {"warmups": warmups, "runs": runs},
    }
    if language == "rust":
        payload["tool"] = tool_descriptor(tools.get("rustc"))
    elif language == "cpp":
        payload["tool"] = tool_descriptor(tools.get("cxx"))
    elif language == "erlang":
        payload["tool"] = {
            "erl": tool_descriptor(tools.get("erl")),
            "erlc": tool_descriptor(tools.get("erlc")),
        }
    return sha256_text(stable_json_text(payload))


def load_cached_baseline(
    case: dict[str, Any],
    language: str,
    cache_key: str,
) -> tuple[dict[str, Any], dict[str, Any]] | None:
    path = baseline_cache_path(case["id"], language)
    if not path.exists():
        return None
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None
    if payload.get("schema_version") != BASELINE_CACHE_SCHEMA_VERSION:
        return None
    if payload.get("cache_key") != cache_key:
        return None
    build = payload.get("build")
    run = payload.get("run")
    if not isinstance(build, dict) or not isinstance(run, dict):
        return None
    return copy.deepcopy(build), copy.deepcopy(run)


def save_cached_baseline(
    case: dict[str, Any],
    language: str,
    cache_key: str,
    build: dict[str, Any],
    run: dict[str, Any],
) -> None:
    path = baseline_cache_path(case["id"], language)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": BASELINE_CACHE_SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "case_id": case["id"],
        "language": language,
        "cache_key": cache_key,
        "build": build,
        "run": run,
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def annotate_cache_usage(
    build: dict[str, Any],
    run: dict[str, Any],
    *,
    mode: str,
    status: str,
    cache_path: Path | None,
    reason: str,
    cache_key: str | None,
) -> None:
    cache_info = {
        "mode": mode,
        "status": status,
        "path": str(cache_path) if cache_path else "",
        "reason": reason,
        "cache_key": cache_key or "",
    }
    build["baseline_cache"] = cache_info
    run["baseline_cache"] = cache_info


def case_enabled_for_default_suite(case: dict[str, Any]) -> bool:
    return bool(case.get("default_enabled", True))


def selected_cases(manifest: dict[str, Any], only_case: str | None) -> list[dict[str, Any]]:
    cases = manifest["cases"]
    if only_case is None:
        return [case for case in cases if case_enabled_for_default_suite(case)]
    requested = [case_id.strip() for case_id in only_case.split(",") if case_id.strip()]
    by_id = {case["id"]: case for case in cases}
    missing = [case_id for case_id in requested if case_id not in by_id]
    if missing:
        raise ValueError(f"unknown case: {', '.join(missing)}")
    return [by_id[case_id] for case_id in requested]


def parse_languages(raw_languages: str | None) -> list[str]:
    if not raw_languages:
        return LANGUAGE_ORDER.copy()
    requested = [item.strip().lower() for item in raw_languages.split(",") if item.strip()]
    aliases = {
        "js": "javascript",
        "node": "javascript",
        "py": "python",
        "cpython": "python",
        "rs": "rust",
        "c++": "cpp",
        "cxx": "cpp",
        "cplusplus": "cpp",
        "ziglang": "zig",
        "golang": "go",
        "erl": "erlang",
        "beam": "erlang",
        "otp": "erlang",
        "kn": "kain",
    }
    normalized: list[str] = []
    for language in requested:
        language = aliases.get(language, language)
        if language not in LANGUAGE_ORDER:
            valid = ", ".join(LANGUAGE_ORDER)
            raise ValueError(f"unknown language '{language}'. Valid languages: {valid}")
        if language not in normalized:
            normalized.append(language)
    return normalized


def case_source_relative(case: dict[str, Any], language: str) -> str:
    key = LANGUAGE_SOURCE_KEYS[language]
    language_sources = case.get("languages", {})
    if isinstance(language_sources, dict) and language in language_sources:
        return str(language_sources[language])
    if key in case:
        return str(case[key])
    raise KeyError(f"case {case['id']} has no source for {language}")


def declared_case_languages(case: dict[str, Any]) -> set[str]:
    language_sources = case.get("languages")
    if isinstance(language_sources, dict) and language_sources:
        return {language for language in language_sources if language in LANGUAGE_ORDER}
    return {language for language in LANGUAGE_ORDER if LANGUAGE_SOURCE_KEYS[language] in case}


def selected_case_languages(case: dict[str, Any], requested_languages: list[str]) -> list[str]:
    declared = declared_case_languages(case)
    selected = [language for language in requested_languages if language in declared]
    if not selected:
        available = ", ".join(language for language in LANGUAGE_ORDER if language in declared) or "none"
        requested = ", ".join(requested_languages)
        raise ValueError(
            f"case {case['id']} has no selected languages. requested={requested}; available={available}"
        )
    return selected


def case_source_path(case: dict[str, Any], language: str) -> Path:
    return BENCHMARK_ROOT / case_source_relative(case, language)


def validate_case_files(case: dict[str, Any], languages: list[str]) -> None:
    for language in languages:
        path = case_source_path(case, language)
        if not path.exists():
            raise FileNotFoundError(f"missing {language} benchmark: {path}")


def ffi_shared_support_paths(case_id: str) -> dict[str, Path]:
    native_dir = BUILD_ROOT / case_id / "native"
    shared_path = native_dir / dynamic_library_name("ffi_boundary_shared")
    link_name = shared_link_artifact_name("ffi_boundary_shared")
    link_path = native_dir / link_name if link_name else shared_path
    return {
        "source": BENCHMARK_ROOT / "ffi_boundary" / "native" / "ffi_boundary.c",
        "shared": shared_path,
        "link": link_path,
        "native_dir": native_dir,
    }


def prepare_case_support(
    case: dict[str, Any],
    tools: dict[str, Any],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    if case["id"] != FFI_SHARED_CASE_ID:
        return None

    paths = ffi_shared_support_paths(case["id"])
    paths["native_dir"].mkdir(parents=True, exist_ok=True)
    if no_build:
        return paths

    shared_exists = paths["shared"].exists()
    link_exists = paths["link"].exists()
    if shared_exists and link_exists:
        return paths

    clang = str(tools["clang"])
    if not shutil.which(clang) and not Path(clang).exists():
        raise RuntimeError(f"Clang executable not found for {FFI_SHARED_CASE_ID}: {clang}")

    command = [clang]
    if os.name == "nt":
        command.extend(
            [
                "-shared",
                "-O3",
                str(paths["source"]),
                "-o",
                str(paths["shared"]),
                f"-Wl,/implib:{paths['link']}",
            ]
        )
    else:
        command.extend(
            [
                "-shared",
                "-fPIC",
                "-O3",
                str(paths["source"]),
                "-o",
                str(paths["shared"]),
            ]
        )

    result = run_command(command, timeout=timeout)
    if result.returncode != 0 or not paths["shared"].exists() or not paths["link"].exists():
        raise RuntimeError(
            "Failed to build ffi shared support.\n"
            f"command: {display_command(command)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return paths


def copy_runtime_sidecar(sidecar: Path, exe_path: Path) -> None:
    if not sidecar.exists() or not exe_path.exists():
        return
    target = exe_path.parent / sidecar.name
    if target == sidecar:
        return
    shutil.copyfile(sidecar, target)


def missing_tool_build(language: str, command: list[str], error: str) -> dict[str, Any]:
    return {
        "ok": False,
        "language": language,
        "exe": "",
        "run_command": command,
        "command": command,
        "build_ms": 0.0,
        "stdout": "",
        "stderr": "",
        "error": error,
    }


def build_kain_case(
    case: dict[str, Any],
    kain_exe: ResolvedExecutable,
    timeout: int,
    no_build: bool,
    env_overrides: dict[str, str],
    support_artifacts: dict[str, Any] | None,
) -> dict[str, Any]:
    case_id = case["id"]
    source_path = case_source_path(case, "kain")
    build_dir = BUILD_ROOT / case_id / "kain"
    build_dir.mkdir(parents=True, exist_ok=True)
    ll_path = build_dir / f"{case_id}.ll"
    exe_path = build_dir / executable_name(case_id)

    command = [
        str(kain_exe.path),
        str(source_path.resolve()),
        "-t",
        "llvm",
        "-o",
        str(ll_path.resolve()),
    ]

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "kain",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "env": env_overrides,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    runtime_manifest = Path(env_overrides["KAIN_RUNTIME_MANIFEST_PATH"])
    if not runtime_manifest.exists():
        return {
            "ok": False,
            "language": "kain",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "env": env_overrides,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": f"missing Kain runtime manifest {runtime_manifest}",
        }

    result = run_command(command, timeout=timeout, cwd=source_path.parent, env_overrides=env_overrides)
    produced_exe = ll_path.with_suffix(".exe" if os.name == "nt" else "")
    if produced_exe.exists() and produced_exe != exe_path:
        shutil.copyfile(produced_exe, exe_path)
    elif produced_exe.exists():
        exe_path = produced_exe
    if support_artifacts:
        copy_runtime_sidecar(Path(support_artifacts["shared"]), exe_path)

    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "kain",
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "env": env_overrides,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Kain build failed or did not produce executable.",
    }


def build_rust_case(
    case: dict[str, Any],
    rustc: str,
    timeout: int,
    no_build: bool,
    support_artifacts: dict[str, Any] | None,
) -> dict[str, Any]:
    case_id = case["id"]
    build_dir = BUILD_ROOT / case_id / "rust"
    build_dir.mkdir(parents=True, exist_ok=True)
    rust_manifest = case.get("rust_manifest")
    if rust_manifest:
        manifest_path = BENCHMARK_ROOT / str(rust_manifest)
        cargo = shutil.which("cargo") or "cargo"
        package_name = str(case.get("rust_package", case_id.replace("_", "-")))
        exe_stem = str(case.get("rust_binary", package_name))
        exe_path = build_dir / "target" / "release" / executable_name(exe_stem)
        command = [
            cargo,
            "build",
            "--release",
            "--manifest-path",
            repo_relative(manifest_path),
            "--target-dir",
            repo_relative(build_dir / "target"),
        ]
        if not shutil.which(cargo) and not Path(cargo).exists():
            return missing_tool_build("rust", command, f"Cargo executable not found: {cargo}")
        if no_build:
            return {
                "ok": exe_path.exists(),
                "language": "rust",
                "exe": str(exe_path),
                "run_command": [str(exe_path)],
                "command": command,
                "flags": ["cargo", "release"],
                "build_ms": 0.0,
                "stdout": "",
                "stderr": "",
                "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
            }
        result = run_command(command, timeout=timeout)
        ok = result.returncode == 0 and exe_path.exists()
        return {
            "ok": ok,
            "language": "rust",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "flags": ["cargo", "release"],
            "build_ms": result.elapsed_ms,
            "stdout": result.stdout[-4000:],
            "stderr": result.stderr[-4000:],
            "error": "" if ok else "Cargo Rust build failed or did not produce executable.",
        }

    exe_path = build_dir / executable_name(case_id)
    extra_flags: list[str] = []
    if support_artifacts:
        extra_flags = [
            "-L",
            f"native={repo_relative(Path(support_artifacts['native_dir']))}",
            "-l",
            "dylib=ffi_boundary_shared",
        ]
    command = [
        rustc,
        repo_relative(case_source_path(case, "rust")),
        *RUST_RELEASE_FLAGS,
        *extra_flags,
        "-o",
        repo_relative(exe_path),
    ]

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "rust",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "flags": RUST_RELEASE_FLAGS,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout)
    if support_artifacts:
        copy_runtime_sidecar(Path(support_artifacts["shared"]), exe_path)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "rust",
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "flags": RUST_RELEASE_FLAGS,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Rust build failed or did not produce executable.",
    }


def cpp_link_flags_for_case(case_id: str) -> list[str]:
    if os.name == "nt" and case_id == "ghost_mirror":
        return ["-lws2_32"]
    return []


def cpp_extra_flags_for_case(case_id: str) -> list[str]:
    return []


def zig_link_flags_for_case(case_id: str) -> list[str]:
    if os.name == "nt" and case_id == "ghost_mirror":
        return ["-lws2_32"]
    return []


def build_cpp_case(
    case: dict[str, Any],
    cxx: str,
    timeout: int,
    no_build: bool,
    support_artifacts: dict[str, Any] | None,
) -> dict[str, Any]:
    case_id = case["id"]
    source = case_source_path(case, "cpp")
    build_dir = BUILD_ROOT / case_id / "cpp"
    build_dir.mkdir(parents=True, exist_ok=True)
    exe_path = build_dir / executable_name(case_id)
    extra_flags = cpp_extra_flags_for_case(case_id)
    link_flags = cpp_link_flags_for_case(case_id)
    if support_artifacts:
        link_flags = link_flags + [repo_relative(Path(support_artifacts["link"]))]
    command = [
        cxx,
        repo_relative(source),
        *CPP_RELEASE_FLAGS,
        *extra_flags,
        "-o",
        repo_relative(exe_path),
        *link_flags,
    ]

    if not shutil.which(cxx) and not Path(cxx).exists():
        return missing_tool_build("cpp", command, f"C++ compiler not found: {cxx}")

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "cpp",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "flags": CPP_RELEASE_FLAGS + extra_flags,
            "link_flags": link_flags,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout)
    if support_artifacts:
        copy_runtime_sidecar(Path(support_artifacts["shared"]), exe_path)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "cpp",
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "flags": CPP_RELEASE_FLAGS + extra_flags,
        "link_flags": link_flags,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "C++ build failed or did not produce executable.",
    }


def build_go_case(
    case: dict[str, Any],
    go_exe: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    case_id = case["id"]
    source = case_source_path(case, "go")
    build_dir = BUILD_ROOT / case_id / "go"
    build_dir.mkdir(parents=True, exist_ok=True)
    go_manifest = case.get("go_manifest")
    exe_stem = str(case.get("go_binary", case_id))
    exe_path = build_dir / executable_name(exe_stem)

    if go_manifest:
        manifest_path = BENCHMARK_ROOT / str(go_manifest)
        package_name = str(case.get("go_package", "."))
        command = [
            go_exe,
            "build",
            *GO_RELEASE_FLAGS,
            "-o",
            str(exe_path.resolve()),
            package_name,
        ]
        command_cwd = manifest_path.parent
    else:
        command = [
            go_exe,
            "build",
            *GO_RELEASE_FLAGS,
            "-o",
            str(exe_path.resolve()),
            source.name,
        ]
        command_cwd = source.parent

    if not shutil.which(go_exe) and not Path(go_exe).exists():
        return missing_tool_build("go", command, f"Go executable not found: {go_exe}")

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "go",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "flags": GO_RELEASE_FLAGS,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout, cwd=command_cwd)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "go",
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "flags": GO_RELEASE_FLAGS,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Go build failed or did not produce executable.",
    }


def build_zig_case(
    case: dict[str, Any],
    zig_exe: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    case_id = case["id"]
    source = case_source_path(case, "zig")
    build_dir = BUILD_ROOT / case_id / "zig"
    build_dir.mkdir(parents=True, exist_ok=True)
    exe_path = build_dir / executable_name(case_id)
    link_flags = zig_link_flags_for_case(case_id)
    command = [
        zig_exe,
        *ZIG_RELEASE_FLAGS,
        source.name,
        *link_flags,
        "-femit-bin=" + str(exe_path.resolve()),
    ]

    if not shutil.which(zig_exe) and not Path(zig_exe).exists():
        return missing_tool_build("zig", command, f"Zig executable not found: {zig_exe}")

    if no_build:
        return {
            "ok": exe_path.exists(),
            "language": "zig",
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "flags": ZIG_RELEASE_FLAGS,
            "link_flags": link_flags,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }

    result = run_command(command, timeout=timeout, cwd=source.parent)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "language": "zig",
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "flags": ZIG_RELEASE_FLAGS,
        "link_flags": link_flags,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Zig build failed or did not produce executable.",
    }


def build_javascript_case(
    case: dict[str, Any],
    node: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    source = case_source_path(case, "javascript")
    run_command_text = [node, repo_relative(source)]
    command = [node, "--check", repo_relative(source)]

    if not shutil.which(node) and not Path(node).exists():
        return missing_tool_build("javascript", command, f"Node executable not found: {node}")

    if no_build:
        return {
            "ok": source.exists(),
            "language": "javascript",
            "exe": "",
            "run_command": run_command_text,
            "command": command,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if source.exists() else f"missing JavaScript source {source}",
        }

    result = run_command(command, timeout=timeout)
    ok = result.returncode == 0
    return {
        "ok": ok,
        "language": "javascript",
        "exe": "",
        "run_command": run_command_text,
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "JavaScript syntax check failed.",
    }


def build_erlang_case(
    case: dict[str, Any],
    erl: str,
    erlc: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    case_id = case["id"]
    source = case_source_path(case, "erlang")
    module_name = str(case.get("erlang_module", source.stem))
    build_dir = BUILD_ROOT / case_id / "erlang"
    build_dir.mkdir(parents=True, exist_ok=True)
    beam_path = build_dir / f"{module_name}.beam"
    command = [
        erlc,
        "-o",
        repo_relative(build_dir),
        repo_relative(source),
    ]
    run_command_text = [
        erl,
        "-noshell",
        "-pa",
        str(build_dir.resolve()),
        "-s",
        module_name,
        "main",
    ]

    if not shutil.which(erlc) and not Path(erlc).exists():
        return missing_tool_build("erlang", command, f"Erlang compiler not found: {erlc}")
    if not shutil.which(erl) and not Path(erl).exists():
        return missing_tool_build("erlang", run_command_text, f"Erlang runtime not found: {erl}")

    if no_build:
        return {
            "ok": beam_path.exists(),
            "language": "erlang",
            "exe": str(beam_path),
            "run_command": run_command_text,
            "command": command,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if beam_path.exists() else f"missing compiled beam {beam_path}",
        }

    result = run_command(command, timeout=timeout)
    ok = result.returncode == 0 and beam_path.exists()
    return {
        "ok": ok,
        "language": "erlang",
        "exe": str(beam_path),
        "run_command": run_command_text,
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Erlang build failed or did not produce beam output.",
    }


def build_python_case(
    case: dict[str, Any],
    python_exe: str,
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    source = case_source_path(case, "python")
    run_command_text = [python_exe, repo_relative(source)]
    command = [python_exe, "-m", "py_compile", repo_relative(source)]

    if not shutil.which(python_exe) and not Path(python_exe).exists():
        return missing_tool_build("python", command, f"Python executable not found: {python_exe}")

    if no_build:
        return {
            "ok": source.exists(),
            "language": "python",
            "exe": "",
            "run_command": run_command_text,
            "command": command,
            "build_ms": 0.0,
            "stdout": "",
            "stderr": "",
            "error": "" if source.exists() else f"missing Python source {source}",
        }

    result = run_command(command, timeout=timeout)
    ok = result.returncode == 0
    return {
        "ok": ok,
        "language": "python",
        "exe": "",
        "run_command": run_command_text,
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Python py_compile failed.",
    }


def build_language_case(
    case: dict[str, Any],
    language: str,
    tools: dict[str, Any],
    timeout: int,
    no_build: bool,
    kain_native_env: dict[str, str],
) -> dict[str, Any]:
    support_artifacts = prepare_case_support(case, tools, timeout, no_build)
    if language == "kain":
        return build_kain_case(case, tools["kain"], timeout, no_build, kain_native_env, support_artifacts)
    if language == "rust":
        return build_rust_case(case, tools["rustc"], timeout, no_build, support_artifacts)
    if language == "cpp":
        return build_cpp_case(case, tools["cxx"], timeout, no_build, support_artifacts)
    if language == "zig":
        return build_zig_case(case, tools["zig"], timeout, no_build)
    if language == "go":
        return build_go_case(case, tools["go"], timeout, no_build)
    if language == "erlang":
        return build_erlang_case(case, tools["erl"], tools["erlc"], timeout, no_build)
    if language == "javascript":
        return build_javascript_case(case, tools["node"], timeout, no_build)
    if language == "python":
        return build_python_case(case, tools["python"], timeout, no_build)
    raise ValueError(f"unsupported language: {language}")


def run_program(command: list[str], timeout: int) -> CommandResult:
    return run_command(command, timeout=timeout)


def measure_program(
    build: dict[str, Any],
    warmups: int,
    runs: int,
    timeout: int,
) -> dict[str, Any]:
    if not build["ok"]:
        return {
            "ok": False,
            "samples_ms": [],
            "warmups": [],
            "min_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "error": build["error"],
        }

    command = build["run_command"]
    warmup_results = []
    for _ in range(warmups):
        result = run_program(command, timeout=timeout)
        warmup_results.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_run_result(result, warmup_results)

    samples = []
    for _ in range(runs):
        result = run_program(command, timeout=timeout)
        samples.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_run_result(result, warmup_results, samples)

    return {
        "ok": True,
        "samples_ms": samples,
        "warmups": warmup_results,
        "min_ms": min(samples),
        "median_ms": statistics.median(samples),
        "mean_ms": statistics.fmean(samples),
        "error": "",
    }


def failed_run_result(
    result: CommandResult,
    warmups: list[float],
    samples: list[float] | None = None,
) -> dict[str, Any]:
    return {
        "ok": False,
        "samples_ms": samples or [],
        "warmups": warmups,
        "min_ms": None,
        "median_ms": None,
        "mean_ms": None,
        "error": (
            f"Executable failed with exit code {result.returncode}.\n"
            f"command:\n{display_command(result.command)}\n"
            f"stdout:\n{result.stdout[-2000:]}\n"
            f"stderr:\n{result.stderr[-2000:]}"
        ),
    }


def compute_winner(run_results: dict[str, dict[str, Any]], languages: list[str]) -> tuple[str, float | None]:
    winners: list[tuple[str, float]] = []
    for language in languages:
        run = run_results[language]
        if run["ok"] and run["median_ms"] is not None:
            winners.append((language, float(run["median_ms"])))
    if not winners:
        return "n/a", None
    winners.sort(key=lambda item: item[1])
    return winners[0]


def compute_relative_to_fastest(
    run_results: dict[str, dict[str, Any]],
    fastest_ms: float | None,
    languages: list[str],
) -> dict[str, float | None]:
    ratios: dict[str, float | None] = {}
    for language in languages:
        median = run_results[language]["median_ms"]
        if fastest_ms is None or median is None or fastest_ms <= 0:
            ratios[language] = None
        else:
            ratios[language] = float(median) / fastest_ms
    return ratios


def benchmark_case(
    case: dict[str, Any],
    languages: list[str],
    tools: dict[str, Any],
    warmups: int,
    runs: int,
    timeout: int,
    no_build: bool,
    kain_native_env: dict[str, str],
    baseline_mode: str,
) -> dict[str, Any]:
    case_languages = selected_case_languages(case, languages)
    validate_case_files(case, case_languages)
    build_results: dict[str, dict[str, Any]] = {}
    run_results: dict[str, dict[str, Any]] = {}
    case_kain_native_env = dict(kain_native_env)
    case_kain_native_env["KAIN_RUNTIME_MANIFEST_PATH"] = str(resolved_kain_runtime_manifest(case))

    for language in case_languages:
        cache_enabled = baseline_mode_uses_cache(baseline_mode, languages, language)
        cache_key = baseline_cache_key(case, language, tools, warmups, runs) if cache_enabled else None
        cache_path = baseline_cache_path(case["id"], language) if cache_enabled else None
        used_cached_baseline = False
        if cache_enabled and baseline_mode != "refresh-foreign" and cache_key:
            cached = load_cached_baseline(case, language, cache_key)
            if cached:
                build_results[language], run_results[language] = cached
                annotate_cache_usage(
                    build_results[language],
                    run_results[language],
                    mode=baseline_mode,
                    status="hit",
                    cache_path=cache_path,
                    reason="foreign baseline cache key matched",
                    cache_key=cache_key,
                )
                used_cached_baseline = True

        if not used_cached_baseline:
            build_results[language] = build_language_case(
                case,
                language,
                tools,
                timeout,
                no_build,
                case_kain_native_env,
            )
            run_results[language] = measure_program(build_results[language], warmups, runs, timeout)
            if cache_enabled and cache_key:
                if build_results[language]["ok"] and run_results[language]["ok"]:
                    save_cached_baseline(case, language, cache_key, build_results[language], run_results[language])
                    annotate_cache_usage(
                        build_results[language],
                        run_results[language],
                        mode=baseline_mode,
                        status="refreshed",
                        cache_path=cache_path,
                        reason="saved fresh foreign baseline",
                        cache_key=cache_key,
                    )
                else:
                    annotate_cache_usage(
                        build_results[language],
                        run_results[language],
                        mode=baseline_mode,
                        status="miss",
                        cache_path=cache_path,
                        reason="cache not updated because build or run failed",
                        cache_key=cache_key,
                    )
            elif language != "kain":
                annotate_cache_usage(
                    build_results[language],
                    run_results[language],
                    mode=baseline_mode,
                    status="disabled",
                    cache_path=None,
                    reason="foreign baseline cache disabled for this run",
                    cache_key=None,
                )

    winner, fastest_ms = compute_winner(run_results, case_languages)
    telemetry = compute_case_telemetry(case, run_results, case_languages)
    return {
        "id": case["id"],
        "title": case.get("title", case["id"]),
        "description": case.get("description", ""),
        "maturity": case.get("maturity", "implemented"),
        "fairness_note": case.get("fairness_note", ""),
        "language_notes": case.get("language_notes", {}),
        "languages": case_languages,
        "source": {
            language: case_source_relative(case, language)
            for language in case_languages
        },
        "build": build_results,
        "run": run_results,
        "winner": winner,
        "fastest_median_ms": fastest_ms,
        "relative_to_fastest": compute_relative_to_fastest(run_results, fastest_ms, case_languages),
        "telemetry": telemetry,
    }


def compute_case_telemetry(
    case: dict[str, Any],
    run_results: dict[str, dict[str, Any]],
    languages: list[str],
) -> dict[str, Any] | None:
    telemetry = case.get("telemetry")
    if not isinstance(telemetry, dict):
        return None
    metric_configs = telemetry.get("metrics")
    if not isinstance(metric_configs, list):
        return None
    computed_metrics: list[dict[str, Any]] = []
    for metric in metric_configs:
        if not isinstance(metric, dict):
            continue
        metric_id = str(metric.get("id", "")).strip()
        if not metric_id:
            continue
        work_items_raw = metric.get("work_items")
        try:
            work_items = float(work_items_raw)
        except (TypeError, ValueError):
            continue
        label = str(metric.get("label", metric_id))
        unit = str(metric.get("unit", "items/s"))
        values: dict[str, float | None] = {}
        for language in languages:
            run = run_results.get(language)
            rate: float | None = None
            if run and run.get("ok") and run.get("median_ms") not in (None, 0):
                median_ms = float(run["median_ms"])
                if median_ms > 0.0:
                    rate = (work_items * 1000.0) / median_ms
            values[language] = rate
        computed_metrics.append(
            {
                "id": metric_id,
                "label": label,
                "unit": unit,
                "work_items": work_items_raw,
                "values": values,
            }
        )
    if not computed_metrics:
        return None
    primary_metric_id = str(telemetry.get("primary_metric_id", computed_metrics[0]["id"]))
    return {
        "primary_metric_id": primary_metric_id,
        "metrics": computed_metrics,
    }


def fmt_ms(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):.3f}"


def fmt_rate(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):,.3f}"


def fmt_work_items(value: Any) -> str:
    if value is None:
        return "n/a"
    numeric = float(value)
    if numeric.is_integer():
        return f"{int(numeric):,}"
    return f"{numeric:,.3f}"


def fmt_ratio(value: Any) -> str:
    if value is None:
        return "n/a"
    ratio = float(value)
    if ratio <= 1.001:
        return "fastest"
    return f"{ratio:.2f}x slower"


def render_samples(samples: list[float]) -> str:
    if not samples:
        return "[]"
    return "[" + ", ".join(f"{sample:.3f}" for sample in samples) + "]"


def status_text(ok: bool) -> str:
    return "PASS" if ok else "FAIL"


def markdown_table_row(cells: list[str]) -> str:
    escaped = [cell.replace("|", "\\|").replace("\n", " ") for cell in cells]
    return "| " + " | ".join(escaped) + " |"


def root_snapshot_path(name: str) -> Path:
    candidate = Path(name)
    if candidate.is_absolute() or len(candidate.parts) != 1 or candidate.name != name:
        raise ValueError(f"minimal report name must stay in benchmark root: {name}")
    return BENCHMARK_ROOT / candidate.name


def latest_report_stem(stem: str) -> str:
    candidate = Path(stem)
    if candidate.is_absolute() or len(candidate.parts) != 1 or candidate.name != stem:
        raise ValueError(f"latest report stem must stay in benchmark report root: {stem}")
    if "." in stem:
        raise ValueError(f"latest report stem cannot include dots: {stem}")
    return candidate.name


def metric_by_id(case: dict[str, Any], metric_id: str) -> dict[str, Any] | None:
    telemetry = case.get("telemetry")
    if not isinstance(telemetry, dict):
        return None
    for metric in telemetry.get("metrics", []):
        if isinstance(metric, dict) and metric.get("id") == metric_id:
            return metric
    return None


def primary_metric(case: dict[str, Any]) -> dict[str, Any] | None:
    telemetry = case.get("telemetry")
    if not isinstance(telemetry, dict):
        return None
    primary_metric_id = str(telemetry.get("primary_metric_id", ""))
    if not primary_metric_id:
        return None
    return metric_by_id(case, primary_metric_id)


def baseline_cache_summary(report: dict[str, Any]) -> dict[str, Any]:
    summary = {
        "mode": report.get("baseline_mode", "off"),
        "root": str(BASELINE_ROOT),
        "hits": 0,
        "refreshed": 0,
        "misses": 0,
        "disabled": 0,
        "eligible_languages": 0,
    }
    for case in report.get("cases", []):
        for language in case.get("languages", []):
            build = case.get("build", {}).get(language, {})
            cache_info = build.get("baseline_cache")
            if not isinstance(cache_info, dict):
                continue
            summary["eligible_languages"] += 1
            status = str(cache_info.get("status", ""))
            if status == "hit":
                summary["hits"] += 1
            elif status == "refreshed":
                summary["refreshed"] += 1
            elif status == "miss":
                summary["misses"] += 1
            elif status == "disabled":
                summary["disabled"] += 1
    return summary


def render_summary_table(report: dict[str, Any]) -> str:
    languages = report["languages"]
    header = ["case", "maturity", "winner"] + [f"{language} median ms" for language in languages]
    divider = ["---"] * len(header)
    rows = [markdown_table_row(header), markdown_table_row(divider)]
    for case in report["cases"]:
        cells = [case["id"], case["maturity"], case["winner"]]
        for language in languages:
            run = case["run"].get(language)
            cells.append(fmt_ms(run["median_ms"]) if run else "n/a")
        rows.append(markdown_table_row(cells))
    return "\n".join(rows)


def render_telemetry_table(report: dict[str, Any]) -> str:
    languages = report.get("languages", [])
    telemetry_cases = [case for case in report.get("cases", []) if primary_metric(case)]
    if not telemetry_cases:
        return ""
    header = ["case", "primary metric", "winner"] + [f"{language} value" for language in languages]
    divider = ["---"] * len(header)
    rows = [markdown_table_row(header), markdown_table_row(divider)]
    for case in telemetry_cases:
        metric = primary_metric(case)
        if not metric:
            continue
        cells = [case["id"], metric["label"], case.get("winner", "n/a")]
        values = metric.get("values", {})
        for language in languages:
            rate = values.get(language) if isinstance(values, dict) else None
            cells.append(fmt_rate(rate))
        rows.append(markdown_table_row(cells))
    return "\n".join(rows)


def render_toolchain(report: dict[str, Any]) -> str:
    toolchain = report.get("toolchain", {})
    kain_native_env = toolchain.get("kain_native_env", {})
    cache_summary = report.get("baseline_cache", {})
    lines = [
        "## Toolchain",
        "",
        f"- kain_exe: `{toolchain.get('kain_exe', 'n/a')}`",
        f"- kain_exe_source: `{toolchain.get('kain_exe_source', 'n/a')}`",
        f"- kain_exe_build_command: `{display_command(toolchain.get('kain_exe_build_command'))}`",
        f"- kain_native_env_default: `{json.dumps(kain_native_env, sort_keys=True)}`",
        f"- rustc: `{toolchain.get('rustc', 'n/a')}`",
        f"- rust_flags: `{display_command(toolchain.get('rust_flags', []))}`",
        f"- cxx: `{toolchain.get('cxx', 'n/a')}`",
        f"- clang: `{toolchain.get('clang', 'n/a')}`",
        f"- cpp_flags: `{display_command(toolchain.get('cpp_flags', []))}`",
        f"- zig: `{toolchain.get('zig', 'n/a')}`",
        f"- zig_flags: `{display_command(toolchain.get('zig_flags', []))}`",
        f"- go: `{toolchain.get('go', 'n/a')}`",
        f"- go_flags: `{display_command(toolchain.get('go_flags', []))}`",
        f"- erl: `{toolchain.get('erl', 'n/a')}`",
        f"- erlc: `{toolchain.get('erlc', 'n/a')}`",
        f"- node: `{toolchain.get('node', 'n/a')}`",
        f"- python: `{toolchain.get('python', 'n/a')}`",
        f"- baseline_mode: `{report.get('baseline_mode', 'off')}`",
        f"- baseline_cache_root: `{cache_summary.get('root', str(BASELINE_ROOT))}`",
        f"- baseline_cache_hits: `{cache_summary.get('hits', 0)}`",
        f"- baseline_cache_refreshed: `{cache_summary.get('refreshed', 0)}`",
        f"- baseline_cache_misses: `{cache_summary.get('misses', 0)}`",
    ]
    return "\n".join(lines)


def render_case_detail(case: dict[str, Any], languages: list[str]) -> str:
    case_languages = case.get("languages", languages)
    lines = [
        f"### {case['id']} - {case['title']}",
        "",
        f"- maturity: `{case.get('maturity', 'implemented')}`",
        f"- winner: `{case.get('winner', 'n/a')}`",
        f"- fastest_median_ms: `{fmt_ms(case.get('fastest_median_ms'))}`",
        f"- description: {case.get('description', '')}",
        f"- fairness_note: {case.get('fairness_note', '')}",
    ]

    language_notes = case.get("language_notes", {})
    if language_notes:
        lines.append("- language_notes:")
        for language in case_languages:
            note = language_notes.get(language)
            if note:
                lines.append(f"  - {language}: {note}")

    telemetry = case.get("telemetry")
    if isinstance(telemetry, dict) and telemetry.get("metrics"):
        lines.extend(["", "Telemetry:"])
        metric = primary_metric(case)
        if metric:
            lines.append(f"- primary_metric: `{metric.get('label', 'n/a')}`")
        for entry in telemetry.get("metrics", []):
            if not isinstance(entry, dict):
                continue
            values = entry.get("values", {})
            value_text = ", ".join(
                f"{language} `{fmt_rate(values.get(language) if isinstance(values, dict) else None)}`"
                for language in case_languages
            )
            lines.append(
                f"- {entry.get('label', 'metric')} (`{fmt_work_items(entry.get('work_items'))}` work/run, `{entry.get('unit', 'items/s')}`): {value_text}"
            )

    lines.extend(["", "Sources:"])
    for language in case_languages:
        lines.append(f"- {language}: `{case['source'][language]}`")

    lines.extend(["", "Measurements:"])
    for language in case_languages:
        build = case["build"][language]
        run = case["run"][language]
        lines.extend(
            [
                f"- {language}:",
                f"  - build_ok: `{status_text(build['ok'])}`",
                f"  - run_ok: `{status_text(run['ok'])}`",
                f"  - build_ms: `{fmt_ms(build['build_ms'])}`",
                f"  - min_ms: `{fmt_ms(run['min_ms'])}`",
                f"  - median_ms: `{fmt_ms(run['median_ms'])}`",
                f"  - mean_ms: `{fmt_ms(run['mean_ms'])}`",
                f"  - relative_to_fastest: `{fmt_ratio(case['relative_to_fastest'][language])}`",
                f"  - samples_ms: `{render_samples(run['samples_ms'])}`",
                f"  - build_command: `{display_command(build.get('command'))}`",
                f"  - run_command: `{display_command(build.get('run_command'))}`",
            ]
        )
        cache_info = build.get("baseline_cache")
        if isinstance(cache_info, dict):
            reason = str(cache_info.get("reason", "")).strip()
            reason_suffix = f" - {reason}" if reason else ""
            lines.append(f"  - baseline_cache: `{cache_info.get('status', 'n/a')}`{reason_suffix}")
        if build.get("env"):
            lines.append(f"  - build_env: `{json.dumps(build['env'], sort_keys=True)}`")
        if build.get("error") or run.get("error"):
            error_text = (build.get("error", "") + "\n" + run.get("error", "")).strip()
            lines.append("  - error:")
            for line in error_text.splitlines():
                lines.append(f"    {line}")

    return "\n".join(lines)


def render_llm_report(report: dict[str, Any]) -> str:
    languages = report.get("languages", [])
    latest_stem = report.get("latest_stem", DEFAULT_LATEST_REPORT_STEM)
    lines = [
        "# Kain Benchmark Report",
        "",
        f"- status: `{status_text(report.get('ok', False))}`",
        f"- generated_at: `{report.get('generated_at', 'n/a')}`",
        f"- suite: `{report.get('suite', 'n/a')}`",
        f"- platform: `{report.get('platform', 'n/a')}`",
        f"- warmups: `{report.get('warmups', 'n/a')}`",
        f"- timed_runs: `{report.get('runs', 'n/a')}`",
        f"- languages: `{', '.join(languages)}`",
        f"- baseline_mode: `{report.get('baseline_mode', 'off')}`",
        f"- json_report: `benchmark/out/reports/{latest_stem}.json`",
        "",
    ]
    if report.get("fatal_error"):
        lines.extend(["## Fatal Error", "", report["fatal_error"], ""])
    lines.extend([render_toolchain(report), "", "## Summary", "", render_summary_table(report), ""])
    telemetry_table = render_telemetry_table(report)
    if telemetry_table:
        lines.extend(["## Telemetry", "", telemetry_table, ""])
    lines.extend(["## Case Details", ""])
    for case in report.get("cases", []):
        lines.extend([render_case_detail(case, languages), ""])
    return "\n".join(lines).rstrip() + "\n"


def render_minimal_report(report: dict[str, Any], minimal_name: str, latest_stem: str) -> str:
    languages = report.get("languages", [])
    lines = [
        "# Kain Benchmark Snapshot",
        "",
        f"- status: `{status_text(report.get('ok', False))}`",
        f"- generated_at: `{report.get('generated_at', 'n/a')}`",
        f"- warmups: `{report.get('warmups', 'n/a')}`",
        f"- timed_runs: `{report.get('runs', 'n/a')}`",
        f"- languages: `{', '.join(languages)}`",
        f"- baseline_mode: `{report.get('baseline_mode', 'off')}`",
        f"- root_snapshot: `benchmark/{minimal_name}`",
        f"- full_report: `benchmark/out/reports/{latest_stem}.llm.md`",
        f"- json_report: `benchmark/out/reports/{latest_stem}.json`",
        "",
    ]
    cache_summary = report.get("baseline_cache", {})
    if isinstance(cache_summary, dict):
        lines.extend(
            [
                f"- baseline_cache_hits: `{cache_summary.get('hits', 0)}`",
                f"- baseline_cache_refreshed: `{cache_summary.get('refreshed', 0)}`",
                f"- baseline_cache_misses: `{cache_summary.get('misses', 0)}`",
                "",
            ]
        )
    if report.get("fatal_error"):
        lines.extend(["## Fatal Error", "", report["fatal_error"], ""])
    lines.extend(["## Summary", "", render_summary_table(report)])
    telemetry_table = render_telemetry_table(report)
    if telemetry_table:
        lines.extend(["", "## Telemetry", "", telemetry_table])
    return "\n".join(lines).rstrip() + "\n"


def write_reports(
    report: dict[str, Any],
    minimal_name: str = DEFAULT_MINIMAL_REPORT_NAME,
    latest_stem: str = DEFAULT_LATEST_REPORT_STEM,
) -> dict[str, Path]:
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

    stale_latest_html = REPORT_ROOT / "latest.html"
    if stale_latest_html.exists():
        stale_latest_html.unlink()

    return {
        "timestamped_json": json_path,
        "latest_json": latest_json,
        "timestamped_llm": llm_path,
        "latest_llm": latest_llm,
        "latest_minimal": latest_minimal,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=str(BENCHMARK_ROOT / "benchmarks.json"))
    parser.add_argument("--case", dest="only_case", help="Single case id or comma-separated case ids")
    parser.add_argument("--languages", help="Comma-separated subset: kain,rust,cpp,zig,go,erlang,javascript,python")
    parser.add_argument("--runs", type=int)
    parser.add_argument("--warmups", type=int)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--kain-exe")
    parser.add_argument("--rustc", default=os.environ.get("RUSTC", "rustc"))
    parser.add_argument("--cxx")
    parser.add_argument("--clang")
    parser.add_argument("--zig", default=os.environ.get("ZIG", "zig"))
    parser.add_argument("--go", default=os.environ.get("GO", "go"))
    parser.add_argument("--erl", default=os.environ.get("ERL"))
    parser.add_argument("--erlc", default=os.environ.get("ERLC"))
    parser.add_argument("--node", default=os.environ.get("NODE", "node"))
    parser.add_argument("--python", default=os.environ.get("PYTHON", sys.executable))
    parser.add_argument(
        "--kain-native-profile",
        choices=sorted(KAIN_NATIVE_PROFILE_DEFAULTS.keys()),
        default="benchmark-release",
    )
    parser.add_argument("--kain-native-opt-level")
    parser.add_argument("--kain-native-target-cpu")
    parser.add_argument("--kain-native-debug-info")
    parser.add_argument("--minimal-name")
    parser.add_argument("--latest-stem")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument(
        "--baseline-mode",
        choices=["auto", "reuse-foreign", "refresh-foreign", "off"],
        default="auto",
        help="auto = reuse foreign baselines only when Kain is also selected; reuse-foreign = reuse foreign baselines whenever possible; refresh-foreign = force rerun and refresh foreign baselines; off = disable baseline caching",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = load_manifest(Path(args.manifest))
    default_languages = args.languages
    if default_languages is None:
        manifest_default_languages = manifest.get("default_languages")
        if isinstance(manifest_default_languages, list):
            default_languages = ",".join(str(language) for language in manifest_default_languages)
        elif isinstance(manifest_default_languages, str):
            default_languages = manifest_default_languages
    languages = parse_languages(default_languages)
    warmups = args.warmups if args.warmups is not None else int(manifest.get("default_warmups", 2))
    runs = args.runs if args.runs is not None else int(manifest.get("default_runs", 7))
    minimal_name = args.minimal_name or str(manifest.get("minimal_name", DEFAULT_MINIMAL_REPORT_NAME))
    latest_stem = args.latest_stem or str(manifest.get("latest_stem", DEFAULT_LATEST_REPORT_STEM))
    kain_native_tuning = resolved_kain_native_tuning(args)
    kain_native_env = kain_native_env_from_tuning(kain_native_tuning)

    report: dict[str, Any] = {
        "suite": manifest.get("suite", "kain-multi-language-benchmarks"),
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "platform": sys.platform,
        "warmups": warmups,
        "runs": runs,
        "languages": languages,
        "latest_stem": latest_stem,
        "baseline_mode": args.baseline_mode,
        "language_labels": {language: LANGUAGE_LABELS[language] for language in languages},
        "cases": [],
        "ok": False,
        "toolchain": {},
    }

    try:
        kain_exe = resolve_kain_exe(args.kain_exe, args.timeout) if "kain" in languages else None
        rustc_path = resolve_tool(args.rustc, "RUSTC", "rustc")
        cxx_path = resolve_cpp_compiler(args.cxx)
        clang_path = resolve_clang(args.clang)
        zig_path = resolve_tool(args.zig, "ZIG", "zig")
        go_path = resolve_tool(args.go, "GO", "go")
        erl_path = resolve_erlang_tool(args.erl, "ERL", "erl")
        erlc_path = resolve_erlang_tool(args.erlc, "ERLC", "erlc")
        node_path = resolve_tool(args.node, "NODE", "node")
        python_path = resolve_tool(args.python, "PYTHON", sys.executable)
        tools: dict[str, Any] = {
            "kain": kain_exe,
            "rustc": rustc_path,
            "cxx": cxx_path,
            "clang": clang_path,
            "zig": zig_path,
            "go": go_path,
            "erl": erl_path,
            "erlc": erlc_path,
            "node": node_path,
            "python": python_path,
        }
        report["toolchain"] = {
            "kain_exe": str(kain_exe.path) if kain_exe else "not selected",
            "kain_exe_source": kain_exe.source if kain_exe else "not selected",
            "kain_exe_build_command": kain_exe.build_command if kain_exe else None,
            "kain_native_tuning": kain_native_tuning,
            "kain_native_env": kain_native_env,
            "rustc": rustc_path,
            "rust_flags": RUST_RELEASE_FLAGS,
            "cxx": cxx_path,
            "clang": clang_path,
            "cpp_flags": CPP_RELEASE_FLAGS,
            "zig": zig_path,
            "zig_flags": ZIG_RELEASE_FLAGS,
            "go": go_path,
            "go_flags": GO_RELEASE_FLAGS,
            "erl": erl_path,
            "erlc": erlc_path,
            "node": node_path,
            "python": python_path,
        }
        for case in selected_cases(manifest, args.only_case):
            case_languages = selected_case_languages(case, languages)
            print(f"[bench] {case['id']} ({', '.join(case_languages)})")
            result = benchmark_case(
                case=case,
                languages=languages,
                tools=tools,
                warmups=warmups,
                runs=runs,
                timeout=args.timeout,
                no_build=args.no_build,
                kain_native_env=kain_native_env,
                baseline_mode=args.baseline_mode,
            )
            report["cases"].append(result)
        report["ok"] = all(
            case["run"][language]["ok"]
            for case in report["cases"]
            for language in case.get("languages", languages)
        )
        report["baseline_cache"] = baseline_cache_summary(report)
    except Exception as exc:
        report["fatal_error"] = str(exc)
        report["ok"] = False
        print(f"[bench] fatal: {exc}", file=sys.stderr)
    finally:
        if "baseline_cache" not in report:
            report["baseline_cache"] = baseline_cache_summary(report)
        outputs = write_reports(report, minimal_name=minimal_name, latest_stem=latest_stem)
        print(f"[bench] report: {outputs['latest_llm']}")
        print(f"[bench] snapshot: {outputs['latest_minimal']}")

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
