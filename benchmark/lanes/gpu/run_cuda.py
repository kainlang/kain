#!/usr/bin/env python3
"""CUDA/PTX gauntlet runner for Kain-authored GPU kernels."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


GPU_ROOT = Path(__file__).resolve().parent
BENCHMARK_ROOT = GPU_ROOT.parent.parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build" / "gpu-cuda"
REPORT_ROOT = OUT_ROOT / "reports"
SNAPSHOT_ROOT = OUT_ROOT / "snapshots"
DEFAULT_MANIFEST = GPU_ROOT / "cuda_cases.json"


def repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def load_manifest(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def select_cases(manifest: dict[str, Any], requested: list[str]) -> list[dict[str, Any]]:
    cases = manifest.get("cases", [])
    if not requested:
        return cases
    wanted = set(requested)
    selected = [case for case in cases if case.get("id") in wanted]
    missing = sorted(wanted.difference(case.get("id") for case in selected))
    if missing:
        raise SystemExit(f"unknown CUDA gauntlet case(s): {', '.join(missing)}")
    return selected


def resolve_kain_bin(args: argparse.Namespace) -> str:
    if args.kain_bin:
        return args.kain_bin
    if os.environ.get("KAIN_BIN"):
        return os.environ["KAIN_BIN"]
    repo_launcher = REPO_ROOT / ".kain" / "bin" / ("kain.exe" if os.name == "nt" else "kain")
    if repo_launcher.exists():
        return str(repo_launcher)
    return "kain"


def run_command(command: list[str], env: dict[str, str], cwd: Path) -> dict[str, Any]:
    start = time.perf_counter()
    proc = subprocess.run(
        command,
        cwd=str(cwd),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    return {
        "command": command,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "elapsed_ms": elapsed_ms,
    }


def host_path(path_text: str) -> Path:
    if path_text.startswith("\\\\?\\"):
        path_text = path_text[4:]
    return Path(path_text)


def report_path_from_stdout(stdout: str) -> Path | None:
    for line in stdout.splitlines():
        match = re.search(r"Report:\s+(.+)$", line.strip())
        if match:
            return host_path(match.group(1).strip())
    return None


def extract_ptx_from_build_report(command_result: dict[str, Any], case_build_root: Path) -> list[str]:
    report_path = report_path_from_stdout(command_result.get("stdout", ""))
    if not report_path or not report_path.exists():
        return []
    report = json.loads(report_path.read_text(encoding="utf-8"))
    ptx_modules: list[str] = []
    for task in report.get("tasks", []):
        for output in task.get("outputs", []):
            output_path = host_path(output)
            if output_path.suffix.lower() != ".ptx" or not output_path.exists():
                continue
            text = output_path.read_text(encoding="utf-8")
            ptx_modules.append(text)
            stable_path = case_build_root / output_path.name
            if output_path.resolve() != stable_path.resolve():
                shutil.copyfile(output_path, stable_path)
    return ptx_modules


def extract_ptx_from_bundle(bundle_path: Path) -> list[str]:
    if not bundle_path.exists():
        return []
    bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
    ptx_modules: list[str] = []
    for artifact in bundle.get("derived_outputs", []):
        if str(artifact.get("format", "")).lower() == "ptx":
            contents = artifact.get("contents", "")
            if contents:
                ptx_modules.append(contents)
    return ptx_modules


def summarize_ptx(ptx_modules: list[str], tracked: list[str]) -> dict[str, Any]:
    combined = "\n".join(ptx_modules)
    lines = combined.splitlines()
    return {
        "module_count": len(ptx_modules),
        "bytes": len(combined.encode("utf-8")),
        "lines": len(lines),
        "tracked": {token: (token in combined) for token in tracked},
        "target_directives": [line.strip() for line in lines if line.strip().startswith(".target")],
        "entry_count": combined.count(".visible .entry"),
    }


def run_case(
    case: dict[str, Any],
    args: argparse.Namespace,
    kain_bin: str,
    manifest: dict[str, Any],
) -> dict[str, Any]:
    language = "kain"
    spec = case.get("languages", {}).get(language)
    if not spec:
        return {
            "case": case.get("id"),
            "language": language,
            "status": "skipped",
            "reason": "no Kain spec",
        }

    shader_path = GPU_ROOT / spec["shader"]
    case_build_root = BUILD_ROOT / case["id"] / language
    case_build_root.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["KAIN_CUDA_ARCH"] = str(case.get("target_arch") or manifest.get("default_target_arch") or "sm_75")
    command = [
        kain_bin,
        "build",
        str(shader_path),
        "--target",
        "cuda",
    ]

    samples = []
    for _ in range(args.warmups):
        run_command(command, env, REPO_ROOT)
    for _ in range(args.runs):
        samples.append(run_command(command, env, REPO_ROOT))

    ok = all(sample["returncode"] == 0 for sample in samples)
    bundle_path = case_build_root / "kain_shader_bundle.json"
    residency_path = case_build_root / "kain_compute_residency.json"
    ptx_modules = []
    for sample in samples:
        if sample["returncode"] == 0:
            ptx_modules = extract_ptx_from_build_report(sample, case_build_root)
            if ptx_modules:
                break
    if not ptx_modules:
        ptx_modules = extract_ptx_from_bundle(bundle_path)
    ptx_summary = summarize_ptx(ptx_modules, case.get("tracked_ptx", []))
    tracked_ok = all(ptx_summary["tracked"].values()) if ptx_summary["tracked"] else True
    elapsed = [sample["elapsed_ms"] for sample in samples]

    return {
        "case": case["id"],
        "title": case.get("title", case["id"]),
        "language": language,
        "status": "ok" if ok and tracked_ok else "failed",
        "shader": repo_relative(shader_path),
        "build_dir": repo_relative(case_build_root),
        "ptx_files": [
            repo_relative(path) for path in sorted(case_build_root.glob("*.ptx"))
        ],
        "bundle": repo_relative(bundle_path) if bundle_path.exists() else "",
        "residency": repo_relative(residency_path) if residency_path.exists() else "",
        "target_arch": env["KAIN_CUDA_ARCH"],
        "runs": args.runs,
        "warmups": args.warmups,
        "elapsed_ms_median": statistics.median(elapsed) if elapsed else 0.0,
        "elapsed_ms_min": min(elapsed) if elapsed else 0.0,
        "ptx": ptx_summary,
        "commands": samples,
    }


def write_reports(results: list[dict[str, Any]], manifest: dict[str, Any]) -> None:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    SNAPSHOT_ROOT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report = {
        "suite": manifest.get("suite", "gpu-cuda"),
        "generated_at": stamp,
        "case_count": len(results),
        "ok_count": sum(1 for result in results if result.get("status") == "ok"),
        "results": results,
    }
    latest_json = REPORT_ROOT / "latest_cuda_gpu.json"
    stamped_json = REPORT_ROOT / f"{stamp}.cuda_gpu.json"
    latest_md = REPORT_ROOT / "latest_cuda_gpu.llm.md"
    snapshot_md = SNAPSHOT_ROOT / "latest_cuda_gpu.md"
    latest_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    stamped_json.write_text(json.dumps(report, indent=2), encoding="utf-8")

    lines = [
        "# CUDA GPU Gauntlet",
        "",
        f"- generated_at: {stamp}",
        f"- cases: {len(results)}",
        f"- ok: {report['ok_count']}",
        "",
    ]
    for result in results:
        lines.extend(
            [
                f"## {result.get('case')}",
                "",
                f"- status: {result.get('status')}",
                f"- target_arch: {result.get('target_arch')}",
                f"- median_ms: {result.get('elapsed_ms_median', 0.0):.3f}",
                f"- ptx_bytes: {result.get('ptx', {}).get('bytes', 0)}",
                f"- ptx_entries: {result.get('ptx', {}).get('entry_count', 0)}",
                f"- ptx_files: {', '.join(result.get('ptx_files', []))}",
                "",
            ]
        )
    text = "\n".join(lines)
    latest_md.write_text(text, encoding="utf-8")
    snapshot_md.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Kain CUDA/PTX GPU gauntlet")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--warmups", type=int, default=0)
    parser.add_argument("--kain-bin", default="")
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    cases = select_cases(manifest, args.case)
    if args.list:
        for case in cases:
            print(f"{case['id']}: {case.get('title', case['id'])}")
        return 0

    kain_bin = resolve_kain_bin(args)
    results = [run_case(case, args, kain_bin, manifest) for case in cases]
    write_reports(results, manifest)
    failed = [result for result in results if result.get("status") != "ok"]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
