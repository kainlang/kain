#!/usr/bin/env python3
"""
Dedicated GPU/SPIR-V benchmark runner.

This runner is separate from benchmark/run.py on purpose: shader artifact
density, Vulkan validation, and pipeline executable telemetry are a different
truth lane than general language benchmark cases.
"""

from __future__ import annotations

import argparse
import json
import os
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


GPU_ROOT = Path(__file__).resolve().parent
BENCHMARK_ROOT = GPU_ROOT.parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build" / "gpu"
REPORT_ROOT = OUT_ROOT / "reports"
DEFAULT_MANIFEST = GPU_ROOT / "gpu_cases.json"
DEFAULT_LATEST_STEM = "latest_gpu"
DEFAULT_MINIMAL_NAME = "latest_gpu.md"
LANGUAGE_ORDER = ["kain", "cpp", "rust"]

SPIRV_OPCODE_NAMES = {
    0: "OpNop",
    1: "OpUndef",
    3: "OpSource",
    5: "OpName",
    15: "OpEntryPoint",
    16: "OpExecutionMode",
    17: "OpCapability",
    19: "OpTypeVoid",
    20: "OpTypeBool",
    21: "OpTypeInt",
    22: "OpTypeFloat",
    23: "OpTypeVector",
    30: "OpTypeStruct",
    32: "OpTypePointer",
    43: "OpConstant",
    54: "OpFunction",
    56: "OpFunctionEnd",
    59: "OpVariable",
    61: "OpLoad",
    62: "OpStore",
    65: "OpAccessChain",
    71: "OpDecorate",
    72: "OpMemberDecorate",
    247: "OpSelectionMerge",
    248: "OpLabel",
    249: "OpBranch",
    250: "OpBranchConditional",
    253: "OpReturn",
    254: "OpReturnValue",
}

TRACKED_OPCODES = [
    "OpLoad",
    "OpStore",
    "OpAccessChain",
    "OpBranch",
    "OpBranchConditional",
    "OpSelectionMerge",
    "OpFunction",
]


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


def executable_name(stem: str) -> str:
    return f"{stem}.exe" if os.name == "nt" else stem


def repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def display_command(command: list[str] | None) -> str:
    if not command:
        return "n/a"
    return " ".join(str(part) for part in command)


def run_command(
    command: list[str],
    *,
    cwd: Path = REPO_ROOT,
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
    return CommandResult(command, completed.returncode, completed.stdout, completed.stderr, elapsed_ms)


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if not isinstance(manifest, dict):
        raise ValueError(f"GPU manifest must be a JSON object: {path}")
    cases = manifest.get("cases")
    if not isinstance(cases, list):
        raise ValueError(f"GPU manifest needs a cases array: {path}")
    return manifest


def resolve_tool(explicit: str | None, env_key: str, names: list[str], known: list[Path] | None = None) -> str | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_value = os.environ.get(env_key)
    if env_value:
        candidates.append(Path(env_value))
    if known:
        candidates.extend(known)
    for name in names:
        found = shutil.which(name)
        if found:
            candidates.append(Path(found))
    for candidate in candidates:
        if candidate.exists():
            return str(candidate.resolve())
    return None


def resolve_kain(explicit: str | None) -> str | None:
    return resolve_tool(
        explicit,
        "KAIN_EXE",
        ["kain"],
        [
            REPO_ROOT / "target" / "release" / executable_name("kain"),
            REPO_ROOT / "target" / "debug" / executable_name("kain"),
        ],
    )


def resolve_spirv_tool(explicit: str | None, env_key: str, stem: str) -> str | None:
    known = []
    if os.name == "nt":
        known.append(Path(r"C:\VulkanSDK\1.4.341.1\Bin") / executable_name(stem))
    return resolve_tool(explicit, env_key, [stem, executable_name(stem)], known)


def resolve_case_path(raw: str) -> Path:
    path = Path(raw)
    if path.is_absolute():
        return path
    return GPU_ROOT / path


def selected_cases(manifest: dict[str, Any], requested: str | None) -> list[dict[str, Any]]:
    cases = [case for case in manifest["cases"] if isinstance(case, dict)]
    if not requested:
        return cases
    wanted = {item.strip() for item in requested.split(",") if item.strip()}
    return [case for case in cases if str(case.get("id", "")) in wanted]


def parse_languages(raw: str | None) -> list[str]:
    if not raw:
        return LANGUAGE_ORDER.copy()
    languages = [item.strip() for item in raw.split(",") if item.strip()]
    unknown = [language for language in languages if language not in LANGUAGE_ORDER]
    if unknown:
        raise ValueError(f"unsupported GPU benchmark language(s): {', '.join(unknown)}")
    return languages


def ensure_build_dir(case_id: str, language: str) -> Path:
    build_dir = BUILD_ROOT / case_id / language
    build_dir.mkdir(parents=True, exist_ok=True)
    return build_dir


def compile_kain_spirv(
    case: dict[str, Any],
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    source = resolve_case_path(str(language_cfg["shader"]))
    spv_path = build_dir / f"{case['id']}.kain.spv"
    command = [
        str(tools.get("kain") or "kain"),
        str(source.resolve()),
        "-t",
        str(language_cfg.get("target", "spirv")),
        "-o",
        str(spv_path.resolve()),
    ]
    if not tools.get("kain"):
        return {
            "ok": False,
            "path": str(spv_path),
            "command": command,
            "build_ms": 0.0,
            "error": "kain executable not found; pass --kain-exe or set KAIN_EXE",
        }
    if no_build:
        return {
            "ok": spv_path.exists(),
            "path": str(spv_path),
            "command": command,
            "build_ms": 0.0,
            "error": "" if spv_path.exists() else f"missing existing SPIR-V artifact {spv_path}",
        }
    result = run_command(command, cwd=source.parent, timeout=timeout)
    ok = result.returncode == 0 and spv_path.exists()
    return {
        "ok": ok,
        "path": str(spv_path),
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Kain SPIR-V build failed or did not produce .spv.",
    }


def compile_glsl_spirv(
    case: dict[str, Any],
    language: str,
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any]:
    source = resolve_case_path(str(language_cfg["shader"]))
    spv_path = build_dir / f"{case['id']}.{language}.spv"
    stage = str(language_cfg.get("stage", "compute"))
    command = [
        str(tools.get("glslang") or "glslangValidator"),
        "-V",
        "-S",
        stage,
        "-o",
        str(spv_path.resolve()),
        str(source.resolve()),
    ]
    if not tools.get("glslang"):
        return {
            "ok": False,
            "path": str(spv_path),
            "command": command,
            "build_ms": 0.0,
            "error": "glslangValidator not found; pass --glslang or set GLSLANG_VALIDATOR_EXE",
        }
    if no_build:
        return {
            "ok": spv_path.exists(),
            "path": str(spv_path),
            "command": command,
            "build_ms": 0.0,
            "error": "" if spv_path.exists() else f"missing existing SPIR-V artifact {spv_path}",
        }
    result = run_command(command, cwd=source.parent, timeout=timeout)
    ok = result.returncode == 0 and spv_path.exists()
    return {
        "ok": ok,
        "path": str(spv_path),
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "GLSL SPIR-V build failed or did not produce .spv.",
    }


def copy_prebuilt_spirv(language_cfg: dict[str, Any], build_dir: Path, case_id: str, language: str) -> dict[str, Any]:
    source = resolve_case_path(str(language_cfg["spirv"]))
    spv_path = build_dir / f"{case_id}.{language}.spv"
    if source.exists():
        shutil.copyfile(source, spv_path)
    ok = spv_path.exists()
    return {
        "ok": ok,
        "path": str(spv_path),
        "command": ["copy", str(source), str(spv_path)],
        "build_ms": 0.0,
        "error": "" if ok else f"missing prebuilt SPIR-V artifact {source}",
    }


def build_shader_artifact(
    case: dict[str, Any],
    language: str,
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    if "spirv" in language_cfg:
        return copy_prebuilt_spirv(language_cfg, build_dir, str(case["id"]), language)
    if "shader" not in language_cfg:
        return None
    compiler = str(language_cfg.get("shader_compiler", "kain" if language == "kain" else "glslang"))
    if compiler == "kain":
        return compile_kain_spirv(case, language_cfg, build_dir, tools, timeout, no_build)
    if compiler == "glslang":
        return compile_glsl_spirv(case, language, language_cfg, build_dir, tools, timeout, no_build)
    return {
        "ok": False,
        "path": "",
        "command": [],
        "build_ms": 0.0,
        "error": f"unsupported shader compiler '{compiler}' for {language}",
    }


def build_cpp_runner(
    case: dict[str, Any],
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    runner = language_cfg.get("runner")
    if not runner:
        return None
    source = resolve_case_path(str(runner))
    exe_path = build_dir / executable_name(str(case["id"]))
    cxx = tools.get("cxx")
    command = [
        str(cxx or "clang++"),
        str(source.resolve()),
        "-std=c++20",
        "-O3",
        "-march=native",
        "-DNDEBUG",
        *[str(arg) for arg in language_cfg.get("include_args", [])],
        "-o",
        str(exe_path.resolve()),
    ]
    command.extend(str(arg) for arg in language_cfg.get("link_args", []))
    if not cxx:
        return {
            "ok": False,
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "C++ compiler not found; pass --cxx or set CXX",
        }
    if no_build:
        return {
            "ok": exe_path.exists(),
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }
    result = run_command(command, cwd=source.parent, timeout=timeout)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "C++ GPU dispatcher build failed or did not produce executable.",
    }


def build_kain_runner(
    case: dict[str, Any],
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    runner = language_cfg.get("runner")
    if not runner:
        return None
    source = resolve_case_path(str(runner))
    ll_path = build_dir / f"{case['id']}.kain-host.ll"
    exe_path = build_dir / executable_name(f"{case['id']}_kain_host")
    command = [
        str(tools.get("kain") or "kain"),
        str(source.resolve()),
        "-t",
        "llvm",
        "-o",
        str(ll_path.resolve()),
    ]
    if not tools.get("kain"):
        return {
            "ok": False,
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "kain executable not found; pass --kain-exe or set KAIN_EXE",
        }
    if no_build:
        return {
            "ok": exe_path.exists(),
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }
    result = run_command(command, cwd=source.parent, timeout=timeout)
    produced_exe = ll_path.with_suffix(".exe" if os.name == "nt" else "")
    if produced_exe.exists() and produced_exe != exe_path:
        shutil.copyfile(produced_exe, exe_path)
    elif produced_exe.exists():
        exe_path = produced_exe
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Kain GPU dispatcher build failed or did not produce executable.",
    }


def build_rust_runner(
    case: dict[str, Any],
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    runner = language_cfg.get("runner")
    manifest = language_cfg.get("rust_manifest")
    if not runner and not manifest:
        return None
    rustc = tools.get("rustc")
    if manifest:
        cargo = tools.get("cargo")
        manifest_path = resolve_case_path(str(manifest))
        binary = str(language_cfg.get("rust_binary", str(case["id"])))
        exe_path = build_dir / "target" / "release" / executable_name(binary)
        command = [
            str(cargo or "cargo"),
            "build",
            "--release",
            "--manifest-path",
            str(manifest_path.resolve()),
            "--target-dir",
            str((build_dir / "target").resolve()),
        ]
        if not cargo:
            return {
                "ok": False,
                "exe": str(exe_path),
                "run_command": [str(exe_path)],
                "command": command,
                "build_ms": 0.0,
                "error": "Cargo not found; pass --cargo or set CARGO",
            }
        if no_build:
            return {
                "ok": exe_path.exists(),
                "exe": str(exe_path),
                "run_command": [str(exe_path)],
                "command": command,
                "build_ms": 0.0,
                "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
            }
        result = run_command(command, cwd=manifest_path.parent, timeout=timeout)
        ok = result.returncode == 0 and exe_path.exists()
        return {
            "ok": ok,
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": result.elapsed_ms,
            "stdout": result.stdout[-4000:],
            "stderr": result.stderr[-4000:],
            "error": "" if ok else "Cargo GPU dispatcher build failed or did not produce executable.",
        }

    source = resolve_case_path(str(runner))
    exe_path = build_dir / executable_name(str(case["id"]))
    command = [
        str(rustc or "rustc"),
        str(source.resolve()),
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
        "-o",
        str(exe_path.resolve()),
    ]
    if not rustc:
        return {
            "ok": False,
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "rustc not found; pass --rustc or set RUSTC",
        }
    if no_build:
        return {
            "ok": exe_path.exists(),
            "exe": str(exe_path),
            "run_command": [str(exe_path)],
            "command": command,
            "build_ms": 0.0,
            "error": "" if exe_path.exists() else f"missing existing executable {exe_path}",
        }
    result = run_command(command, cwd=source.parent, timeout=timeout)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "exe": str(exe_path),
        "run_command": [str(exe_path)],
        "command": command,
        "build_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Rust GPU dispatcher build failed or did not produce executable.",
    }


def build_runner(
    case: dict[str, Any],
    language: str,
    language_cfg: dict[str, Any],
    build_dir: Path,
    tools: dict[str, str | None],
    timeout: int,
    no_build: bool,
) -> dict[str, Any] | None:
    runner_compiler = str(language_cfg.get("runner_compiler", language))
    if runner_compiler == "cpp":
        return build_cpp_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    if runner_compiler == "rust":
        return build_rust_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    if runner_compiler == "kain":
        return build_kain_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    if language == "cpp":
        return build_cpp_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    if language == "rust":
        return build_rust_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    if language == "kain" and language_cfg.get("runner"):
        return build_kain_runner(case, language_cfg, build_dir, tools, timeout, no_build)
    return None


def parse_spirv_binary(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if len(data) < 20 or len(data) % 4 != 0:
        return {"ok": False, "error": "SPIR-V file is too small or not word-aligned"}
    words = [int.from_bytes(data[index : index + 4], "little") for index in range(0, len(data), 4)]
    if words[0] != 0x07230203:
        return {"ok": False, "error": "SPIR-V magic mismatch"}
    offset = 5
    instruction_count = 0
    opcode_counts: dict[str, int] = {name: 0 for name in TRACKED_OPCODES}
    while offset < len(words):
        first = words[offset]
        word_count = first >> 16
        opcode = first & 0xFFFF
        if word_count == 0:
            return {"ok": False, "error": f"zero word-count instruction at word {offset}"}
        instruction_count += 1
        name = SPIRV_OPCODE_NAMES.get(opcode)
        if name in opcode_counts:
            opcode_counts[name] += 1
        offset += word_count
    return {
        "ok": offset == len(words),
        "bytes": len(data),
        "words": len(words),
        "instruction_count": instruction_count,
        "opcode_counts": opcode_counts,
        "error": "" if offset == len(words) else "SPIR-V instruction stream ended off word boundary",
    }


def profile_spirv(path: str | None, spirv_dis: str | None, timeout: int) -> dict[str, Any] | None:
    if not path:
        return None
    spv_path = Path(path)
    if not spv_path.exists():
        return {"ok": False, "error": f"SPIR-V artifact not found: {spv_path}"}
    binary_stats = parse_spirv_binary(spv_path)
    stats = {
        "ok": binary_stats.get("ok", False),
        "path": str(spv_path),
        "bytes": binary_stats.get("bytes"),
        "words": binary_stats.get("words"),
        "instruction_count": binary_stats.get("instruction_count"),
        "opcode_counts": binary_stats.get("opcode_counts", {}),
        "source": "binary",
        "error": binary_stats.get("error", ""),
    }
    if not spirv_dis:
        return stats
    result = run_command([spirv_dis, str(spv_path)], cwd=spv_path.parent, timeout=timeout)
    if result.returncode != 0:
        stats["disassembly_error"] = result.stderr[-2000:] or result.stdout[-2000:]
        return stats
    lines = [
        line.strip()
        for line in result.stdout.splitlines()
        if line.strip() and not line.strip().startswith(";")
    ]
    opcode_counts = {opcode: 0 for opcode in TRACKED_OPCODES}
    for line in lines:
        match = re.search(r"\b(Op[A-Za-z0-9_]+)\b", line)
        if match and match.group(1) in opcode_counts:
            opcode_counts[match.group(1)] += 1
    stats.update(
        {
            "ok": True,
            "instruction_count": len(lines),
            "opcode_counts": opcode_counts,
            "source": "spirv-dis",
        }
    )
    return stats


def validate_spirv(path: str | None, spirv_val: str | None, timeout: int) -> dict[str, Any] | None:
    if not path:
        return None
    spv_path = Path(path)
    if not spv_path.exists():
        return {"ok": False, "error": f"SPIR-V artifact not found: {spv_path}"}
    if not spirv_val:
        return {"ok": None, "skipped": True, "error": "spirv-val not found"}
    result = run_command(
        [spirv_val, "--target-env", "vulkan1.3", str(spv_path)],
        cwd=spv_path.parent,
        timeout=timeout,
    )
    return {
        "ok": result.returncode == 0,
        "command": result.command,
        "elapsed_ms": result.elapsed_ms,
        "stdout": result.stdout[-2000:],
        "stderr": result.stderr[-2000:],
        "error": "" if result.returncode == 0 else "spirv-val rejected module",
    }


def load_sidecar(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except Exception as exc:
        return {"ok": False, "error": f"failed to load telemetry sidecar: {exc}"}
    if not isinstance(value, dict):
        return {"ok": False, "error": "telemetry sidecar must be a JSON object"}
    return value


def telemetry_value(telemetry: dict[str, Any] | None, keys: list[str]) -> Any:
    if not isinstance(telemetry, dict):
        return None
    for key in keys:
        if key in telemetry:
            return telemetry[key]
    pipeline = telemetry.get("pipeline_executable")
    if isinstance(pipeline, dict):
        for key in keys:
            if key in pipeline:
                return pipeline[key]
    return None


def normalize_runner_env(value: Any) -> dict[str, str]:
    if not isinstance(value, dict):
        return {}
    env: dict[str, str] = {}
    for key, item in value.items():
        if not isinstance(key, str):
            continue
        env[key] = str(item)
    return env


def measure_runner(
    build: dict[str, Any] | None,
    build_dir: Path,
    language: str,
    case_id: str,
    shader_path: str | None,
    entry_point: str | None,
    work_items: Any,
    width: Any,
    warmups: int,
    runs: int,
    timeout: int,
    no_run: bool,
    extra_env: dict[str, str] | None = None,
) -> dict[str, Any]:
    sidecar_path = build_dir / f"{language}.telemetry.json"
    if no_run or not build:
        return {
            "ok": True,
            "skipped": True,
            "reason": "static-only" if not build else "no-run requested",
            "samples_ms": [],
            "warmups": [],
            "min_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "telemetry_path": str(sidecar_path),
            "telemetry": load_sidecar(sidecar_path),
            "error": "",
        }
    if not build.get("ok"):
        return {
            "ok": False,
            "samples_ms": [],
            "warmups": [],
            "min_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "telemetry_path": str(sidecar_path),
            "telemetry": load_sidecar(sidecar_path),
            "error": build.get("error", "runner build failed"),
        }
    command = list(build.get("run_command", []))
    command.extend(str(arg) for arg in build.get("run_args", []))
    env = {
        "KAIN_GPU_CASE_ID": case_id,
        "KAIN_GPU_LANGUAGE": language,
        "KAIN_GPU_TELEMETRY_PATH": str(sidecar_path.resolve()),
    }
    if shader_path:
        env["KAIN_GPU_SHADER_SPV"] = str(Path(shader_path).resolve())
    if entry_point:
        env["KAIN_GPU_ENTRY_POINT"] = entry_point
    if work_items is not None:
        env["KAIN_GPU_WORK_ITEMS"] = str(work_items)
    if width is not None:
        env["KAIN_GPU_WIDTH"] = str(width)
    if extra_env:
        env.update(extra_env)

    warmup_results: list[float] = []
    for _ in range(warmups):
        result = run_command(command, cwd=build_dir, timeout=timeout, env_overrides=env)
        warmup_results.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_measure(result, warmup_results, [], sidecar_path)

    samples: list[float] = []
    for _ in range(runs):
        result = run_command(command, cwd=build_dir, timeout=timeout, env_overrides=env)
        samples.append(result.elapsed_ms)
        if result.returncode != 0:
            return failed_measure(result, warmup_results, samples, sidecar_path)

    return {
        "ok": True,
        "skipped": False,
        "samples_ms": samples,
        "warmups": warmup_results,
        "min_ms": min(samples) if samples else None,
        "median_ms": statistics.median(samples) if samples else None,
        "mean_ms": statistics.fmean(samples) if samples else None,
        "telemetry_path": str(sidecar_path),
        "telemetry": load_sidecar(sidecar_path),
        "error": "",
    }


def failed_measure(
    result: CommandResult,
    warmups: list[float],
    samples: list[float],
    sidecar_path: Path,
) -> dict[str, Any]:
    return {
        "ok": False,
        "samples_ms": samples,
        "warmups": warmups,
        "min_ms": None,
        "median_ms": None,
        "mean_ms": None,
        "telemetry_path": str(sidecar_path),
        "telemetry": load_sidecar(sidecar_path),
        "error": (
            f"run failed with exit code {result.returncode}\n"
            f"stdout:\n{result.stdout[-2000:]}\n"
            f"stderr:\n{result.stderr[-2000:]}"
        ),
    }


def build_language_result(
    case: dict[str, Any],
    language: str,
    language_cfg: dict[str, Any],
    tools: dict[str, str | None],
    args: argparse.Namespace,
) -> dict[str, Any]:
    build_dir = ensure_build_dir(str(case["id"]), language)
    shader = build_shader_artifact(case, language, language_cfg, build_dir, tools, args.timeout, args.no_build)
    spirv_stats = profile_spirv(shader.get("path") if shader else None, tools.get("spirv_dis"), args.timeout)
    spirv_validation = validate_spirv(shader.get("path") if shader else None, tools.get("spirv_val"), args.timeout)
    runner = build_runner(case, language, language_cfg, build_dir, tools, args.timeout, args.no_build)
    if runner and language_cfg.get("run_args"):
        runner["run_args"] = list(language_cfg.get("run_args", []))
    run = measure_runner(
        runner,
        build_dir,
        language,
        str(case["id"]),
        shader.get("path") if shader else None,
        str(language_cfg.get("entry_point", "")) or None,
        language_cfg.get("work_items", case.get("work_items")),
        language_cfg.get("width", case.get("width")),
        args.warmups,
        args.runs,
        args.timeout,
        args.no_run,
        normalize_runner_env(case.get("runner_env")) | normalize_runner_env(language_cfg.get("runner_env")),
    )
    return {
        "language": language,
        "configured": True,
        "build_dir": str(build_dir),
        "shader": shader,
        "runner": runner,
        "spirv_stats": spirv_stats,
        "spirv_validation": spirv_validation,
        "run": run,
    }


def missing_language_result(language: str) -> dict[str, Any]:
    return {
        "language": language,
        "configured": False,
        "build_dir": "",
        "shader": None,
        "runner": None,
        "spirv_stats": None,
        "spirv_validation": None,
        "run": {
            "ok": False,
            "skipped": True,
            "reason": "not configured",
            "samples_ms": [],
            "warmups": [],
            "min_ms": None,
            "median_ms": None,
            "mean_ms": None,
            "telemetry": None,
            "error": "language not configured for this GPU case",
        },
    }


def case_ok(case_result: dict[str, Any], languages: list[str]) -> bool:
    configured = False
    for language in languages:
        entry = case_result["languages"].get(language)
        if not entry or not entry.get("configured"):
            continue
        configured = True
        shader = entry.get("shader")
        if shader and not shader.get("ok"):
            return False
        validation = entry.get("spirv_validation")
        if validation and validation.get("ok") is False:
            return False
        run = entry.get("run")
        if run and not run.get("ok"):
            return False
    return configured


def fmt_ms(value: Any) -> str:
    if value is None:
        return "n/a"
    return f"{float(value):.3f}"


def fmt_int(value: Any) -> str:
    if value is None:
        return "n/a"
    try:
        return f"{int(value):,}"
    except (TypeError, ValueError):
        return str(value)


def fmt_bool(value: Any) -> str:
    if value is True:
        return "PASS"
    if value is False:
        return "FAIL"
    return "n/a"


def fmt_telemetry(value: Any) -> str:
    if value is None:
        return "n/a"
    try:
        number = float(value)
    except (TypeError, ValueError):
        return str(value)
    if number.is_integer():
        return f"{int(number):,}"
    return f"{number:.3f}"


def markdown_row(cells: list[str]) -> str:
    return "| " + " | ".join(cell.replace("|", "\\|").replace("\n", " ") for cell in cells) + " |"


def render_summary_table(report: dict[str, Any]) -> str:
    languages = report["languages"]
    header = ["case", "status"] + [f"{language} median ms" for language in languages]
    rows = [markdown_row(header), markdown_row(["---"] * len(header))]
    for case in report["cases"]:
        cells = [case["id"], fmt_bool(case["ok"])]
        for language in languages:
            run = case["languages"].get(language, {}).get("run", {})
            cells.append(fmt_ms(run.get("median_ms")))
        rows.append(markdown_row(cells))
    return "\n".join(rows)


def render_spirv_table(report: dict[str, Any]) -> str:
    languages = report["languages"]
    header = ["case"]
    for language in languages:
        header.extend([f"{language} inst", f"{language} bytes", f"{language} val"])
    rows = [markdown_row(header), markdown_row(["---"] * len(header))]
    for case in report["cases"]:
        cells = [case["id"]]
        for language in languages:
            entry = case["languages"].get(language, {})
            stats = entry.get("spirv_stats") or {}
            validation = entry.get("spirv_validation") or {}
            cells.extend(
                [
                    fmt_int(stats.get("instruction_count")),
                    fmt_int(stats.get("bytes")),
                    fmt_bool(validation.get("ok")),
                ]
            )
        rows.append(markdown_row(cells))
    return "\n".join(rows)


def render_hardware_table(report: dict[str, Any]) -> str:
    languages = report["languages"]
    any_sidecar = any(
        isinstance(entry.get("run", {}).get("telemetry"), dict)
        for case in report["cases"]
        for entry in case["languages"].values()
    )
    if not any_sidecar:
        return ""
    header = ["case"]
    for language in languages:
        header.extend([
            f"{language} mismatch",
            f"{language} max abs err",
            f"{language} rounds",
            f"{language} regs",
            f"{language} binary bytes",
            f"{language} VGPR",
            f"{language} SGPR",
            f"{language} spills",
            f"{language} duration ns",
        ])
    rows = [markdown_row(header), markdown_row(["---"] * len(header))]
    for case in report["cases"]:
        cells = [case["id"]]
        for language in languages:
            telemetry = case["languages"].get(language, {}).get("run", {}).get("telemetry")
            cells.extend(
                [
                    fmt_telemetry(telemetry_value(telemetry, ["mismatch_count", "mismatches"])),
                    fmt_telemetry(telemetry_value(telemetry, ["max_abs_error", "max_error", "max_abs_diff"])),
                    fmt_telemetry(telemetry_value(telemetry, ["rounds", "dispatch_rounds"])),
                    fmt_telemetry(telemetry_value(telemetry, ["register_count", "registers"])),
                    fmt_telemetry(telemetry_value(telemetry, ["binary_size", "executable_binary_size"])),
                    fmt_telemetry(telemetry_value(telemetry, ["vgpr_count", "vgprs", "VGPRs"])),
                    fmt_telemetry(telemetry_value(telemetry, ["sgpr_count", "sgprs", "SGPRs"])),
                    fmt_telemetry(telemetry_value(telemetry, ["spill_count", "spills"])),
                    fmt_telemetry(telemetry_value(telemetry, ["duration_ns", "execution_duration_ns"])),
                ]
            )
        rows.append(markdown_row(cells))
    return "\n".join(rows)


def render_case_details(report: dict[str, Any]) -> str:
    lines: list[str] = []
    for case in report["cases"]:
        lines.extend(
            [
                f"### {case['id']} - {case.get('title', case['id'])}",
                "",
                f"- status: `{fmt_bool(case['ok'])}`",
                f"- maturity: `{case.get('maturity', 'unknown')}`",
                f"- description: {case.get('description', '')}",
                f"- fairness_note: {case.get('fairness_note', '')}",
                "",
            ]
        )
        for language in report["languages"]:
            entry = case["languages"].get(language)
            if not entry:
                continue
            shader = entry.get("shader") or {}
            runner = entry.get("runner") or {}
            stats = entry.get("spirv_stats") or {}
            validation = entry.get("spirv_validation") or {}
            run = entry.get("run") or {}
            telemetry = run.get("telemetry") if isinstance(run, dict) else None
            lines.extend(
                [
                    f"- {language}:",
                    f"  - configured: `{entry.get('configured', False)}`",
                    f"  - shader_ok: `{fmt_bool(shader.get('ok'))}`",
                    f"  - spirv_path: `{shader.get('path', 'n/a')}`",
                    f"  - spirv_instructions: `{fmt_int(stats.get('instruction_count'))}`",
                    f"  - spirv_bytes: `{fmt_int(stats.get('bytes'))}`",
                    f"  - spirv_validation: `{fmt_bool(validation.get('ok'))}`",
                    f"  - runner_ok: `{fmt_bool(runner.get('ok'))}`",
                    f"  - run_ok: `{fmt_bool(run.get('ok'))}`",
                    f"  - median_ms: `{fmt_ms(run.get('median_ms'))}`",
                    f"  - shader_command: `{display_command(shader.get('command'))}`",
                    f"  - runner_command: `{display_command(runner.get('command'))}`",
                    f"  - telemetry_path: `{run.get('telemetry_path', 'n/a')}`",
                    f"  - telemetry_checksum: `{fmt_telemetry(telemetry_value(telemetry, ['checksum']))}`",
                    f"  - telemetry_mismatch_count: `{fmt_telemetry(telemetry_value(telemetry, ['mismatch_count', 'mismatches']))}`",
                    f"  - telemetry_max_abs_error: `{fmt_telemetry(telemetry_value(telemetry, ['max_abs_error', 'max_error', 'max_abs_diff']))}`",
                    f"  - telemetry_rounds: `{fmt_telemetry(telemetry_value(telemetry, ['rounds', 'dispatch_rounds']))}`",
                    f"  - telemetry_register_count: `{fmt_telemetry(telemetry_value(telemetry, ['register_count', 'registers']))}`",
                    f"  - telemetry_duration_ns: `{fmt_telemetry(telemetry_value(telemetry, ['duration_ns', 'execution_duration_ns']))}`",
                ]
            )
            errors = "\n".join(
                str(item.get("error", ""))
                for item in [shader, runner, validation, run]
                if isinstance(item, dict) and item.get("error")
            ).strip()
            if errors:
                lines.append("  - error:")
                for line in errors.splitlines():
                    lines.append(f"    {line}")
        lines.append("")
    return "\n".join(lines).rstrip()


def render_markdown(report: dict[str, Any], minimal: bool = False) -> str:
    lines = [
        "# Kain GPU Benchmark Report" if not minimal else "# Kain GPU Benchmark Snapshot",
        "",
        f"- status: `{fmt_bool(report.get('ok'))}`",
        f"- generated_at: `{report.get('generated_at')}`",
        f"- suite: `{report.get('suite')}`",
        f"- warmups: `{report.get('warmups')}`",
        f"- timed_runs: `{report.get('runs')}`",
        f"- languages: `{', '.join(report.get('languages', []))}`",
        f"- json_report: `benchmark/out/reports/{report.get('latest_stem', DEFAULT_LATEST_STEM)}.json`",
        "",
        "## Summary",
        "",
        render_summary_table(report),
        "",
        "## SPIR-V Density",
        "",
        render_spirv_table(report),
        "",
    ]
    hardware = render_hardware_table(report)
    if hardware:
        lines.extend(["## Hardware Telemetry", "", hardware, ""])
    if not minimal:
        lines.extend(["## Case Details", "", render_case_details(report), ""])
    return "\n".join(lines).rstrip() + "\n"


def write_reports(report: dict[str, Any], minimal_name: str, latest_stem: str) -> None:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    BENCHMARK_ROOT.mkdir(parents=True, exist_ok=True)
    report["latest_stem"] = latest_stem
    json_text = json.dumps(report, indent=2)
    md_text = render_markdown(report)
    minimal_text = render_markdown(report, minimal=True)
    latest_json = REPORT_ROOT / f"{latest_stem}.json"
    latest_md = REPORT_ROOT / f"{latest_stem}.llm.md"
    stamped_json = REPORT_ROOT / f"{report['stamp']}.gpu.json"
    stamped_md = REPORT_ROOT / f"{report['stamp']}.gpu.llm.md"
    latest_json.write_text(json_text, encoding="utf-8")
    latest_md.write_text(md_text, encoding="utf-8")
    stamped_json.write_text(json_text, encoding="utf-8")
    stamped_md.write_text(md_text, encoding="utf-8")
    (BENCHMARK_ROOT / minimal_name).write_text(minimal_text, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--case", help="Comma-separated case id filter")
    parser.add_argument("--languages", help="Comma-separated language filter")
    parser.add_argument("--list", action="store_true", help="List GPU benchmark cases")
    parser.add_argument("--no-build", action="store_true", help="Reuse existing build artifacts")
    parser.add_argument("--no-run", action="store_true", help="Build/profile/validate shaders without dispatcher timing")
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--kain-exe", help="Explicit path to kain executable")
    parser.add_argument("--cxx", help="Explicit path to C++ compiler")
    parser.add_argument("--rustc", help="Explicit path to rustc")
    parser.add_argument("--cargo", help="Explicit path to cargo")
    parser.add_argument("--glslang", help="Explicit path to glslangValidator")
    parser.add_argument("--spirv-dis", help="Explicit path to spirv-dis")
    parser.add_argument("--spirv-val", help="Explicit path to spirv-val")
    parser.add_argument("--latest-stem", default=DEFAULT_LATEST_STEM)
    parser.add_argument("--minimal-name", default=DEFAULT_MINIMAL_NAME)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest_path = Path(args.manifest)
    if not manifest_path.is_absolute():
        manifest_path = REPO_ROOT / manifest_path
    manifest = load_manifest(manifest_path)
    cases = selected_cases(manifest, args.case)
    if args.list:
        for case in cases:
            print(f"{case.get('id')} - {case.get('title', '')}")
        return 0
    languages = parse_languages(args.languages)
    tools = {
        "kain": resolve_kain(args.kain_exe),
        "cxx": resolve_tool(
            args.cxx,
            "CXX",
            ["clang++", "g++"],
            [REPO_ROOT / "toolchain" / "llvm" / "bin" / executable_name("clang++")],
        ),
        "rustc": resolve_tool(args.rustc, "RUSTC", ["rustc"]),
        "cargo": resolve_tool(args.cargo, "CARGO", ["cargo"]),
        "glslang": resolve_tool(args.glslang, "GLSLANG_VALIDATOR_EXE", ["glslangValidator"]),
        "spirv_dis": resolve_spirv_tool(args.spirv_dis, "SPIRV_DIS_EXE", "spirv-dis"),
        "spirv_val": resolve_spirv_tool(args.spirv_val, "SPIRV_VAL_EXE", "spirv-val"),
    }

    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    case_results: list[dict[str, Any]] = []
    for case in cases:
        language_configs = case.get("languages", {})
        if not isinstance(language_configs, dict):
            language_configs = {}
        per_language: dict[str, Any] = {}
        for language in languages:
            cfg = language_configs.get(language)
            if isinstance(cfg, dict):
                per_language[language] = build_language_result(case, language, cfg, tools, args)
            else:
                per_language[language] = missing_language_result(language)
        case_result = {
            "id": str(case["id"]),
            "title": str(case.get("title", case["id"])),
            "description": str(case.get("description", "")),
            "maturity": str(case.get("maturity", "unknown")),
            "work_items": case.get("work_items"),
            "fairness_note": str(case.get("fairness_note", "")),
            "languages": per_language,
        }
        case_result["ok"] = case_ok(case_result, languages)
        case_results.append(case_result)

    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report = {
        "suite": "gpu",
        "description": manifest.get("description", ""),
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "stamp": stamp,
        "ok": all(case["ok"] for case in case_results) if case_results else False,
        "warmups": args.warmups,
        "runs": args.runs,
        "languages": languages,
        "manifest": repo_relative(manifest_path),
        "toolchain": tools,
        "cases": case_results,
    }
    write_reports(report, args.minimal_name, args.latest_stem)
    print(render_markdown(report, minimal=True))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
