#!/usr/bin/env python3
"""
Dedicated Kain FFI boundary benchmark.

This benchmark answers one narrow question:
how much boundary tax do we pay when the same tiny helper lives in
pure Kain, a directly linked C object, a directly linked shared library,
the interpreter/live bridge path, or equivalent Zig ReleaseFast code.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


CASE_ROOT = Path(__file__).resolve().parent
REPO_ROOT = CASE_ROOT.parent.parent.parent
OUT_ROOT = REPO_ROOT / "benchmark" / "out"
BUILD_ROOT = OUT_ROOT / "build" / "ffi_boundary"
REPORT_ROOT = OUT_ROOT / "reports"
NATIVE_ROOT = CASE_ROOT / "native"
GENERATED_NATIVE_ROOT = BUILD_ROOT / "native"
SOURCE_ROOT = CASE_ROOT / "sources"
MANIFEST_PATH = CASE_ROOT / "KAIN.toml"
NATIVE_RUNTIME_MANIFEST = REPO_ROOT / "runtime" / "native_core_runtime.toml"


@dataclass(frozen=True)
class Variant:
    id: str
    label: str
    kind: str
    source_name: str
    iterations: int
    needs_shared_runtime: bool = False


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


VARIANTS = [
    Variant("llvm_pure", "Kain LLVM Pure", "llvm", "llvm_pure.kn", 10_000_000),
    Variant("llvm_object", "Kain LLVM C Object", "llvm", "llvm_object.kn", 10_000_000),
    Variant(
        "llvm_shared",
        "Kain LLVM C Shared",
        "llvm",
        "llvm_shared.kn",
        10_000_000,
        needs_shared_runtime=True,
    ),
    Variant("interpret_pure", "Kain Interpret Pure", "interpret", "interpret_pure.kn", 10_000),
    Variant(
        "interpret_shared",
        "Kain Interpret C Shared",
        "interpret",
        "interpret_shared.kn",
        10_000,
        needs_shared_runtime=True,
    ),
    Variant("zig_pure", "Zig Pure", "zig", "zig_pure.zig", 10_000_000),
    Variant("zig_c_object", "Zig C Object", "zig", "zig_c_object.zig", 10_000_000),
]


def executable_name(stem: str) -> str:
    return f"{stem}.exe" if os.name == "nt" else stem


def zig_executable_name(stem: str) -> str:
    return stem


def dynamic_library_name(stem: str) -> str:
    if os.name == "nt":
        return f"{stem}.dll"
    if sys.platform == "darwin":
        return f"lib{stem}.dylib"
    return f"lib{stem}.so"


def object_name(stem: str) -> str:
    return f"{stem}.obj" if os.name == "nt" else f"{stem}.o"


def shared_link_artifact_name(stem: str) -> str | None:
    if os.name == "nt":
        return f"{stem}.lib"
    return None


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
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        env=env,
    )
    elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
    return CommandResult(
        command=command,
        returncode=completed.returncode,
        stdout=completed.stdout,
        stderr=completed.stderr,
        elapsed_ms=elapsed_ms,
    )


def resolve_kain_exe(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_kain = os.environ.get("KAIN_EXE")
    if env_kain:
        candidates.append(Path(env_kain))
    candidates.extend(
        [
            REPO_ROOT / "target" / "release" / executable_name("kain"),
            REPO_ROOT / "target" / "debug" / executable_name("kain"),
        ]
    )
    path_kain = shutil.which("kain")
    if path_kain:
        candidates.append(Path(path_kain))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError("Could not resolve kain.exe; pass --kain-exe or set KAIN_EXE.")


def resolve_clang(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_clang = os.environ.get("CLANG")
    if env_clang:
        candidates.append(Path(env_clang))
    candidates.append(REPO_ROOT / "toolchain" / "llvm" / "bin" / executable_name("clang"))
    path_clang = shutil.which("clang")
    if path_clang:
        candidates.append(Path(path_clang))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError("Could not resolve clang; pass --clang or set CLANG.")


def resolve_zig(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_zig = os.environ.get("ZIG")
    if env_zig:
        candidates.append(Path(env_zig))
    path_zig = shutil.which("zig")
    if path_zig:
        candidates.append(Path(path_zig))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError("Could not resolve zig; pass --zig or set ZIG.")


def native_benchmark_env() -> dict[str, str]:
    return {
        "KAIN_NATIVE_PROFILE": "benchmark-release",
        "KAIN_NATIVE_OPT_LEVEL": "3",
        "KAIN_NATIVE_TARGET_CPU": "native",
        "KAIN_NATIVE_DEBUG_INFO": "0",
        "KAIN_RUNTIME_MANIFEST_PATH": str(NATIVE_RUNTIME_MANIFEST),
    }


def shared_runtime_env(shared_directory: Path) -> dict[str, str]:
    env: dict[str, str] = {}
    if os.name == "nt":
        env["PATH"] = str(shared_directory) + os.pathsep + os.environ.get("PATH", "")
    elif sys.platform == "darwin":
        env["DYLD_LIBRARY_PATH"] = str(shared_directory) + os.pathsep + os.environ.get(
            "DYLD_LIBRARY_PATH", ""
        )
    else:
        env["LD_LIBRARY_PATH"] = str(shared_directory) + os.pathsep + os.environ.get(
            "LD_LIBRARY_PATH", ""
        )
    return env


def write_case_manifest() -> None:
    object_artifact = path_for_manifest(GENERATED_NATIVE_ROOT / object_name("ffi_boundary_object"))
    shared_artifact = path_for_manifest(
        GENERATED_NATIVE_ROOT / dynamic_library_name("ffi_boundary_shared")
    )
    manifest = f"""[package]
name = "ffi_boundary"
version = "0.1.0"
description = "Dedicated Kain FFI boundary benchmark."

[c_ffi]

[[c_ffi.libraries]]
name = "ffi_boundary_object"
header = "native/ffi_boundary.h"
shared_lib = "{object_artifact}"

[[c_ffi.libraries]]
name = "ffi_boundary_shared"
header = "native/ffi_boundary.h"
shared_lib = "{shared_artifact}"
"""
    MANIFEST_PATH.write_text(manifest, encoding="utf-8")


def compile_native_artifacts(clang: Path, timeout: int) -> dict[str, Path]:
    GENERATED_NATIVE_ROOT.mkdir(parents=True, exist_ok=True)
    source_path = NATIVE_ROOT / "ffi_boundary.c"
    object_path = GENERATED_NATIVE_ROOT / object_name("ffi_boundary_object")
    shared_path = GENERATED_NATIVE_ROOT / dynamic_library_name("ffi_boundary_shared")
    link_artifact_path = shared_link_artifact_name("ffi_boundary_shared")
    shared_link_path = GENERATED_NATIVE_ROOT / link_artifact_path if link_artifact_path else None

    object_command = [str(clang), "-c", "-O3", str(source_path), "-o", str(object_path)]
    object_result = run_command(object_command, cwd=CASE_ROOT, timeout=timeout)
    if object_result.returncode != 0 or not object_path.exists():
        raise RuntimeError(
            "Failed to compile ffi boundary object.\n"
            f"stdout:\n{object_result.stdout}\n"
            f"stderr:\n{object_result.stderr}"
        )

    shared_command = [str(clang)]
    if os.name == "nt":
        shared_command.extend(["-shared", "-O3", str(source_path), "-o", str(shared_path)])
        if shared_link_path:
            shared_command.append(f"-Wl,/implib:{shared_link_path}")
    else:
        shared_command.extend(
            ["-shared", "-fPIC", "-O3", str(source_path), "-o", str(shared_path)]
        )
    shared_result = run_command(shared_command, cwd=CASE_ROOT, timeout=timeout)
    if shared_result.returncode != 0 or not shared_path.exists():
        raise RuntimeError(
            "Failed to compile ffi boundary shared library.\n"
            f"stdout:\n{shared_result.stdout}\n"
            f"stderr:\n{shared_result.stderr}"
        )

    return {
        "object": object_path,
        "shared": shared_path,
        "shared_link": shared_link_path if shared_link_path and shared_link_path.exists() else shared_path,
    }


def path_for_manifest(path: Path) -> str:
    return os.path.relpath(path, CASE_ROOT).replace("\\", "/")


def build_llvm_variant(
    variant: Variant,
    kain_exe: Path,
    timeout: int,
    env_overrides: dict[str, str],
) -> dict[str, object]:
    build_dir = BUILD_ROOT / variant.id
    build_dir.mkdir(parents=True, exist_ok=True)
    ll_path = build_dir / f"{variant.id}.ll"
    exe_path = build_dir / executable_name(variant.id)
    source_path = SOURCE_ROOT / variant.source_name
    command = [
        str(kain_exe),
        str(source_path),
        "-t",
        "llvm",
        "-o",
        str(ll_path),
    ]
    result = run_command(command, cwd=CASE_ROOT, timeout=timeout, env_overrides=env_overrides)
    produced_exe = ll_path.with_suffix(".exe" if os.name == "nt" else "")
    if produced_exe.exists() and produced_exe != exe_path:
        shutil.copyfile(produced_exe, exe_path)
    elif produced_exe.exists():
        exe_path = produced_exe
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "variant": variant.id,
        "kind": variant.kind,
        "command": command,
        "build_ms": result.elapsed_ms,
        "exe": str(exe_path),
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "LLVM build failed or did not produce executable.",
    }


def build_zig_variant(
    variant: Variant,
    zig: Path,
    timeout: int,
    native_artifacts: dict[str, Path],
) -> dict[str, object]:
    build_dir = BUILD_ROOT / variant.id
    build_dir.mkdir(parents=True, exist_ok=True)
    exe_path = build_dir / zig_executable_name(variant.id)
    source_path = SOURCE_ROOT / variant.source_name
    command = [
        str(zig),
        "build-exe",
        str(source_path),
        "-O",
        "ReleaseFast",
        "-femit-bin=" + str(exe_path),
    ]
    if variant.id == "zig_c_object":
        command.append(str(native_artifacts["object"]))
    result = run_command(command, cwd=CASE_ROOT, timeout=timeout)
    ok = result.returncode == 0 and exe_path.exists()
    return {
        "ok": ok,
        "variant": variant.id,
        "kind": variant.kind,
        "command": command,
        "build_ms": result.elapsed_ms,
        "exe": str(exe_path),
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Zig build failed or did not produce executable.",
    }


def prepare_interpret_variant(
    variant: Variant,
    kain_exe: Path,
    timeout: int,
) -> dict[str, object]:
    source_path = SOURCE_ROOT / variant.source_name
    command = [
        str(kain_exe),
        "run",
        str(source_path),
        "--target",
        "interpret",
    ]
    result = run_command(command, cwd=CASE_ROOT, timeout=timeout)
    ok = result.returncode == 0
    return {
        "ok": ok,
        "variant": variant.id,
        "kind": variant.kind,
        "command": command,
        "build_ms": 0.0,
        "prime_ms": result.elapsed_ms,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
        "error": "" if ok else "Interpret prime run failed.",
    }


def measure_command(
    command: list[str],
    *,
    cwd: Path,
    timeout: int,
    warmups: int,
    runs: int,
    env_overrides: dict[str, str] | None = None,
) -> dict[str, object]:
    warmup_ms: list[float] = []
    for _ in range(warmups):
        result = run_command(command, cwd=cwd, timeout=timeout, env_overrides=env_overrides)
        warmup_ms.append(result.elapsed_ms)
        if result.returncode != 0:
            return {
                "ok": False,
                "warmups_ms": warmup_ms,
                "samples_ms": [],
                "error": (
                    f"Warmup failed with exit code {result.returncode}.\n"
                    f"stdout:\n{result.stdout[-2000:]}\n"
                    f"stderr:\n{result.stderr[-2000:]}"
                ),
            }

    samples_ms: list[float] = []
    for _ in range(runs):
        result = run_command(command, cwd=cwd, timeout=timeout, env_overrides=env_overrides)
        samples_ms.append(result.elapsed_ms)
        if result.returncode != 0:
            return {
                "ok": False,
                "warmups_ms": warmup_ms,
                "samples_ms": samples_ms,
                "error": (
                    f"Measured run failed with exit code {result.returncode}.\n"
                    f"stdout:\n{result.stdout[-2000:]}\n"
                    f"stderr:\n{result.stderr[-2000:]}"
                ),
            }

    return {
        "ok": True,
        "warmups_ms": warmup_ms,
        "samples_ms": samples_ms,
        "median_ms": statistics.median(samples_ms),
        "mean_ms": statistics.fmean(samples_ms),
        "min_ms": min(samples_ms),
        "max_ms": max(samples_ms),
        "error": "",
    }


def render_markdown(report: dict[str, object]) -> str:
    lines = [
        "# FFI Boundary Benchmark",
        "",
        f"- timestamp: `{report['timestamp']}`",
        f"- warmups: `{report['warmups']}`",
        f"- runs: `{report['runs']}`",
        f"- clang: `{report['clang']}`",
        f"- zig: `{report['zig']}`",
        f"- kain: `{report['kain']}`",
        "",
        "## Results",
        "",
        "| Variant | Median ms | ns/call | Relative to fastest |",
        "| --- | ---: | ---: | ---: |",
    ]
    fastest = None
    for result in report["results"]:
        if result["run"]["ok"]:
            median_ms = float(result["run"]["median_ms"])
            if fastest is None or median_ms < fastest:
                fastest = median_ms
    for result in report["results"]:
        if not result["run"]["ok"]:
            lines.append(f"| {result['label']} | failed | failed | failed |")
            continue
        median_ms = float(result["run"]["median_ms"])
        ns_per_call = float(result["ns_per_call"])
        relative = median_ms / fastest if fastest and fastest > 0 else 1.0
        lines.append(
            f"| {result['label']} | {median_ms:.3f} | {ns_per_call:.2f} | {relative:.2f}x |"
        )
    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- `llvm_object` is the lean direct-link object path.",
            "- `llvm_shared` is the direct native shared-library path.",
            "- `interpret_shared` exercises the current live bridge path, including the generated Rust-side bridge layer.",
            "- `zig_pure` and `zig_c_object` use Zig ReleaseFast with the same loop count and checksum as the LLVM variants.",
            "- `ns/call` uses the median wall-clock time divided by the variant's fixed iteration count.",
            "",
            "## Build Details",
            "",
        ]
    )
    for result in report["results"]:
        lines.append(f"### {result['label']}")
        lines.append(f"- source: `{result['source']}`")
        lines.append(f"- iterations: `{result['iterations']}`")
        lines.append(f"- prepare/build ok: `{result['build']['ok']}`")
        lines.append(f"- prepare/build ms: `{result['build'].get('build_ms', 0.0):.3f}`")
        if "prime_ms" in result["build"]:
            lines.append(f"- interpret prime ms: `{result['build']['prime_ms']:.3f}`")
        if result["build"]["command"]:
            lines.append(
                f"- command: `{ ' '.join(str(part) for part in result['build']['command']) }`"
            )
        if not result["run"]["ok"]:
            lines.append(f"- error: `{result['run']['error']}`")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Dedicated Kain FFI boundary benchmark")
    parser.add_argument("--kain-exe", help="Explicit path to kain.exe")
    parser.add_argument("--clang", help="Explicit path to clang")
    parser.add_argument("--zig", help="Explicit path to zig")
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()

    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)

    kain_exe = resolve_kain_exe(args.kain_exe)
    clang = resolve_clang(args.clang)
    zig = resolve_zig(args.zig)
    write_case_manifest()
    native_artifacts = compile_native_artifacts(clang, args.timeout)
    llvm_env = native_benchmark_env()
    shared_env = shared_runtime_env(GENERATED_NATIVE_ROOT)

    results: list[dict[str, object]] = []
    for variant in VARIANTS:
        source_path = SOURCE_ROOT / variant.source_name
        if variant.kind == "llvm":
            build = build_llvm_variant(variant, kain_exe, args.timeout, llvm_env)
            run_env = shared_env if variant.needs_shared_runtime else None
            if build["ok"]:
                command = [str(build["exe"])]
                run = measure_command(
                    command,
                    cwd=Path(build["exe"]).parent,
                    timeout=args.timeout,
                    warmups=args.warmups,
                    runs=args.runs,
                    env_overrides=run_env,
                )
            else:
                run = {"ok": False, "warmups_ms": [], "samples_ms": [], "error": build["error"]}
        elif variant.kind == "zig":
            build = build_zig_variant(variant, zig, args.timeout, native_artifacts)
            if build["ok"]:
                command = [str(build["exe"])]
                run = measure_command(
                    command,
                    cwd=Path(build["exe"]).parent,
                    timeout=args.timeout,
                    warmups=args.warmups,
                    runs=args.runs,
                )
            else:
                run = {"ok": False, "warmups_ms": [], "samples_ms": [], "error": build["error"]}
        else:
            build = prepare_interpret_variant(variant, kain_exe, args.timeout)
            if build["ok"]:
                command = [str(kain_exe), "run", str(source_path), "--target", "interpret"]
                run = measure_command(
                    command,
                    cwd=CASE_ROOT,
                    timeout=args.timeout,
                    warmups=args.warmups,
                    runs=args.runs,
                )
            else:
                run = {"ok": False, "warmups_ms": [], "samples_ms": [], "error": build["error"]}

        ns_per_call = None
        if run["ok"]:
            ns_per_call = (float(run["median_ms"]) * 1_000_000.0) / float(variant.iterations)

        results.append(
            {
                "id": variant.id,
                "label": variant.label,
                "source": str(source_path.relative_to(REPO_ROOT)).replace("\\", "/"),
                "iterations": variant.iterations,
                "build": build,
                "run": run,
                "ns_per_call": ns_per_call,
            }
        )

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report = {
        "suite": "ffi-boundary",
        "timestamp": timestamp,
        "warmups": args.warmups,
        "runs": args.runs,
        "kain": str(kain_exe),
        "clang": str(clang),
        "zig": str(zig),
        "native_artifacts": {key: str(value) for key, value in native_artifacts.items()},
        "results": results,
    }

    latest_json = REPORT_ROOT / "ffi_boundary_latest.json"
    latest_md = REPORT_ROOT / "ffi_boundary_latest.llm.md"
    stamped_json = REPORT_ROOT / f"{timestamp}.ffi_boundary.json"
    stamped_md = REPORT_ROOT / f"{timestamp}.ffi_boundary.llm.md"

    report_json = json.dumps(report, indent=2)
    report_md = render_markdown(report)
    latest_json.write_text(report_json, encoding="utf-8")
    latest_md.write_text(report_md, encoding="utf-8")
    stamped_json.write_text(report_json, encoding="utf-8")
    stamped_md.write_text(report_md, encoding="utf-8")

    print(report_md)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
