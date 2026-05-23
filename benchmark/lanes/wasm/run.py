#!/usr/bin/env python3
"""Dedicated Kain WASM pipeline parity and benchmark runner.

Each case is compiled twice: Kain's built-in wasm backend and Rust's
wasm32-unknown-unknown backend. The Node WebAssembly host validates,
instantiates, executes the same export, and returns a byte transcript that this
runner compares exactly.
"""

from __future__ import annotations

import argparse
import hashlib
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
from typing import Any


WASM_ROOT = Path(__file__).resolve().parent
BENCHMARK_ROOT = WASM_ROOT.parent.parent
REPO_ROOT = BENCHMARK_ROOT.parent
MANIFEST_PATH = WASM_ROOT / "wasm_cases.json"
NODE_RUNNER = WASM_ROOT / "run_wasm_module.mjs"
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build" / "wasm"
REPORT_ROOT = OUT_ROOT / "reports"
LATEST_ROOT_REPORT = OUT_ROOT / "snapshots" / "latest_wasm.md"
LATEST_JSON_REPORT = REPORT_ROOT / "wasm_latest.json"
LATEST_LLM_REPORT = REPORT_ROOT / "wasm_latest.llm.md"


@dataclass(frozen=True)
class WasmCase:
    id: str
    title: str
    description: str
    maturity: str
    kain: Path
    rust: Path
    export: str


@dataclass
class CommandResult:
    command: list[str]
    returncode: int
    stdout: str
    stderr: str
    elapsed_ms: float


def executable_name(stem: str) -> str:
    return f"{stem}.exe" if os.name == "nt" else stem


def run_command(command: list[str], *, cwd: Path, timeout: int) -> CommandResult:
    start = time.perf_counter_ns()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )
    elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000.0
    return CommandResult(command, completed.returncode, completed.stdout, completed.stderr, elapsed_ms)


def resolve_tool(explicit: str | None, env_name: str, names: list[str], fallbacks: list[Path]) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_value = os.environ.get(env_name)
    if env_value:
        candidates.append(Path(env_value))
    candidates.extend(fallbacks)
    for name in names:
        found = shutil.which(name)
        if found:
            candidates.append(Path(found))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError(f"Could not resolve {env_name}; pass an explicit flag or set {env_name}.")


def resolve_kain(explicit: str | None) -> Path:
    return resolve_tool(
        explicit,
        "KAIN_EXE",
        ["kain"],
        [
            REPO_ROOT / "target" / "debug" / executable_name("kain"),
            REPO_ROOT / "target" / "release" / executable_name("kain"),
        ],
    )


def resolve_rustc(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))
    env_value = os.environ.get("RUSTC")
    if env_value:
        candidates.append(Path(env_value))
    rustup = shutil.which("rustup")
    if rustup:
        completed = subprocess.run(
            [rustup, "which", "rustc"],
            cwd=str(REPO_ROOT),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if completed.returncode == 0 and completed.stdout.strip():
            candidates.append(Path(completed.stdout.strip()))
    found = shutil.which("rustc")
    if found:
        candidates.append(Path(found))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError("Could not resolve rustc; pass --rustc or set RUSTC.")


def load_cases() -> tuple[dict[str, Any], list[WasmCase]]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    cases = []
    for raw in manifest["cases"]:
        cases.append(
            WasmCase(
                id=raw["id"],
                title=raw.get("title", raw["id"]),
                description=raw.get("description", ""),
                maturity=raw.get("maturity", "wasm-parity"),
                kain=(WASM_ROOT / raw["kain"]).resolve(),
                rust=(WASM_ROOT / raw["rust"]).resolve(),
                export=raw.get("export", "main"),
            )
        )
    return manifest, cases


def select_cases(cases: list[WasmCase], selected: str | None) -> list[WasmCase]:
    if not selected:
        return cases
    wanted = [item.strip() for item in selected.split(",") if item.strip()]
    by_id = {case.id: case for case in cases}
    missing = [case_id for case_id in wanted if case_id not in by_id]
    if missing:
        raise SystemExit(f"Unknown wasm case(s): {', '.join(missing)}")
    return [by_id[case_id] for case_id in wanted]


def sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def median(values: list[float]) -> float | None:
    if not values:
        return None
    return float(statistics.median(values))


def build_kain(case: WasmCase, kain_exe: Path, out_dir: Path, timeout: int) -> tuple[Path, CommandResult]:
    out_path = out_dir / "kain.wasm"
    command = [str(kain_exe), str(case.kain), "-t", "wasm", "-o", str(out_path)]
    return out_path, run_command(command, cwd=REPO_ROOT, timeout=timeout)


def build_rust(case: WasmCase, rustc: Path, out_dir: Path, timeout: int) -> tuple[Path, CommandResult]:
    out_path = out_dir / "rust.wasm"
    command = [
        str(rustc),
        str(case.rust),
        "--edition=2021",
        "--crate-type=cdylib",
        "--target",
        "wasm32-unknown-unknown",
        "-C",
        "opt-level=3",
        "-C",
        "panic=abort",
        "-C",
        "codegen-units=1",
        "-o",
        str(out_path),
    ]
    return out_path, run_command(command, cwd=REPO_ROOT, timeout=timeout)


def run_wasm(
    wasm_path: Path,
    export: str,
    node: Path,
    runs: int,
    warmups: int,
    timeout: int,
) -> tuple[CommandResult, dict[str, Any] | None]:
    command = [
        str(node),
        str(NODE_RUNNER),
        str(wasm_path),
        export,
        str(runs),
        str(warmups),
    ]
    result = run_command(command, cwd=REPO_ROOT, timeout=timeout)
    payload = None
    if result.returncode == 0:
        payload = json.loads(result.stdout)
    return result, payload


def command_json(result: CommandResult) -> dict[str, Any]:
    return {
        "command": result.command,
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "elapsed_ms": result.elapsed_ms,
    }


def artifact_json(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.relative_to(REPO_ROOT)),
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def execute_case(
    case: WasmCase,
    *,
    kain_exe: Path,
    rustc: Path,
    node: Path,
    runs: int,
    warmups: int,
    timeout: int,
) -> dict[str, Any]:
    case_out = BUILD_ROOT / case.id
    case_out.mkdir(parents=True, exist_ok=True)

    case_result: dict[str, Any] = {
        "id": case.id,
        "title": case.title,
        "description": case.description,
        "maturity": case.maturity,
        "export": case.export,
        "status": "PASS",
    }

    kain_wasm, kain_build = build_kain(case, kain_exe, case_out, timeout)
    rust_wasm, rust_build = build_rust(case, rustc, case_out, timeout)
    case_result["build"] = {
        "kain": command_json(kain_build),
        "rust": command_json(rust_build),
    }

    if kain_build.returncode != 0 or rust_build.returncode != 0:
        case_result["status"] = "FAIL"
        case_result["failure"] = "build failed"
        return case_result

    case_result["artifacts"] = {
        "kain": artifact_json(kain_wasm),
        "rust": artifact_json(rust_wasm),
    }

    kain_run, kain_payload = run_wasm(kain_wasm, case.export, node, runs, warmups, timeout)
    rust_run, rust_payload = run_wasm(rust_wasm, case.export, node, runs, warmups, timeout)
    case_result["run"] = {
        "kain": command_json(kain_run),
        "rust": command_json(rust_run),
    }

    if kain_run.returncode != 0 or rust_run.returncode != 0 or kain_payload is None or rust_payload is None:
        case_result["status"] = "FAIL"
        case_result["failure"] = "wasm execution failed"
        return case_result

    kain_transcript = kain_payload["transcript"].encode("utf-8")
    rust_transcript = rust_payload["transcript"].encode("utf-8")
    parity_ok = kain_transcript == rust_transcript
    deterministic_ok = bool(kain_payload.get("deterministic")) and bool(rust_payload.get("deterministic"))

    case_result["execution"] = {
        "kain": {
            "result": kain_payload["result"],
            "stdout": kain_payload["stdout"],
            "durations_ms": kain_payload["durations_ms"],
            "median_ms": median(kain_payload["durations_ms"]),
            "transcript_sha256": hashlib.sha256(kain_transcript).hexdigest(),
        },
        "rust": {
            "result": rust_payload["result"],
            "stdout": rust_payload["stdout"],
            "durations_ms": rust_payload["durations_ms"],
            "median_ms": median(rust_payload["durations_ms"]),
            "transcript_sha256": hashlib.sha256(rust_transcript).hexdigest(),
        },
        "byte_for_byte_transcript_match": parity_ok,
        "deterministic": deterministic_ok,
    }

    if not parity_ok:
        case_result["status"] = "FAIL"
        case_result["failure"] = "Kain/Rust wasm transcripts differ byte-for-byte"
    elif not deterministic_ok:
        case_result["status"] = "FAIL"
        case_result["failure"] = "At least one wasm module returned nondeterministic results"

    return case_result


def render_markdown(report: dict[str, Any]) -> str:
    lines = [
        "# Kain WASM Benchmark Snapshot",
        "",
        f"- status: `{report['status']}`",
        f"- generated_at: `{report['generated_at']}`",
        f"- warmups: `{report['warmups']}`",
        f"- timed_runs: `{report['runs']}`",
        f"- parity_contract: `Kain and Rust wasm execution transcripts must match byte-for-byte`",
        f"- json_report: `{LATEST_JSON_REPORT.relative_to(REPO_ROOT)}`",
        f"- full_report: `{LATEST_LLM_REPORT.relative_to(REPO_ROOT)}`",
        "",
        "## Summary",
        "",
        "| case | status | transcript | kain result | rust result | kain ms | rust ms | kain bytes | rust bytes |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for case in report["cases"]:
        execution = case.get("execution", {})
        artifacts = case.get("artifacts", {})
        kain_exec = execution.get("kain", {})
        rust_exec = execution.get("rust", {})
        if "byte_for_byte_transcript_match" in execution:
            transcript = "match" if execution.get("byte_for_byte_transcript_match") else "mismatch"
        else:
            transcript = "n/a"
        lines.append(
            "| {id} | {status} | {transcript} | {kain_result} | {rust_result} | {kain_ms} | {rust_ms} | {kain_bytes} | {rust_bytes} |".format(
                id=case["id"],
                status=case["status"],
                transcript=transcript,
                kain_result=kain_exec.get("result", "n/a"),
                rust_result=rust_exec.get("result", "n/a"),
                kain_ms=(
                    f"{kain_exec['median_ms']:.3f}" if kain_exec.get("median_ms") is not None else "n/a"
                ),
                rust_ms=(
                    f"{rust_exec['median_ms']:.3f}" if rust_exec.get("median_ms") is not None else "n/a"
                ),
                kain_bytes=artifacts.get("kain", {}).get("size_bytes", "n/a"),
                rust_bytes=artifacts.get("rust", {}).get("size_bytes", "n/a"),
            )
        )
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append(
        "This lane validates wasm with Node's WebAssembly compiler and compares normalized `result/stdout` transcript bytes, not raw wasm binary identity."
    )
    return "\n".join(lines) + "\n"


def write_reports(report: dict[str, Any]) -> None:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    LATEST_ROOT_REPORT.parent.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
    timestamp_json = REPORT_ROOT / f"wasm_{stamp}.json"
    timestamp_llm = REPORT_ROOT / f"wasm_{stamp}.llm.md"
    json_text = json.dumps(report, indent=2)
    md_text = render_markdown(report)
    for path in [LATEST_JSON_REPORT, timestamp_json]:
        path.write_text(json_text + "\n", encoding="utf-8")
    for path in [LATEST_LLM_REPORT, timestamp_llm, LATEST_ROOT_REPORT]:
        path.write_text(md_text, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    manifest, _cases = load_cases()
    parser = argparse.ArgumentParser(description="Run Kain-vs-Rust WASM parity benchmarks.")
    parser.add_argument("--case", help="Comma-separated case ids to run.")
    parser.add_argument("--runs", type=int, default=int(manifest.get("default_runs", 3)))
    parser.add_argument("--warmups", type=int, default=int(manifest.get("default_warmups", 1)))
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--kain-exe")
    parser.add_argument("--rustc")
    parser.add_argument("--node")
    parser.add_argument("--keep-going", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest, all_cases = load_cases()
    selected_cases = select_cases(all_cases, args.case)

    kain_exe = resolve_kain(args.kain_exe)
    rustc = resolve_rustc(args.rustc)
    node = resolve_tool(args.node, "NODE", ["node"], [])

    report: dict[str, Any] = {
        "suite": manifest.get("suite", "kain-wasm-vs-rust-wasm"),
        "status": "PASS",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "runs": args.runs,
        "warmups": args.warmups,
        "tools": {
            "kain": str(kain_exe),
            "rustc": str(rustc),
            "node": str(node),
        },
        "cases": [],
    }

    for case in selected_cases:
        print(f"[wasm] {case.id}: build + execute Kain/Rust wasm", flush=True)
        try:
            result = execute_case(
                case,
                kain_exe=kain_exe,
                rustc=rustc,
                node=node,
                runs=args.runs,
                warmups=args.warmups,
                timeout=args.timeout,
            )
        except Exception as exc:
            result = {
                "id": case.id,
                "title": case.title,
                "status": "FAIL",
                "failure": f"{type(exc).__name__}: {exc}",
            }
        report["cases"].append(result)
        if result["status"] != "PASS":
            report["status"] = "FAIL"
            print(f"[wasm] {case.id}: FAIL - {result.get('failure', 'unknown failure')}", flush=True)
            if not args.keep_going:
                break
        else:
            execution = result["execution"]
            print(
                "[wasm] {case}: PASS result={result_value} kain={kain_ms:.3f}ms rust={rust_ms:.3f}ms".format(
                    case=case.id,
                    result_value=execution["kain"]["result"],
                    kain_ms=execution["kain"]["median_ms"],
                    rust_ms=execution["rust"]["median_ms"],
                ),
                flush=True,
            )

    write_reports(report)
    print(f"[wasm] report: {LATEST_ROOT_REPORT.relative_to(REPO_ROOT)}", flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
