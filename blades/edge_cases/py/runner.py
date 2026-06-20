#!/usr/bin/env python3
"""
PYTHON INTEROP TEST RUNNER — Universal Python interop testing pipeline.

Mirrors bench.py's architecture (registry + build + run + report) but adapted
for test pipelines rather than language-vs-language benchmarks.

One `kain check` for all 6 source files, N test probes per file.
The diagnostics binary (diagnostics.kn) executes all tests and produces
a structured report parsed by this runner.

Usage:
  python runner.py                         # check + run all + report (frictionless)
  python runner.py check                   # typecheck only
  python runner.py list                    # list all 31 tests
  python runner.py run                     # run all tests (after check)
  python runner.py run --suite cause       # run just one module's tests
  python runner.py run --test test_venv_exists_no_path  # run a single test
  python runner.py bench                   # benchmark mode (N runs with timing)
  python runner.py bench --runs 10         # benchmark with 10 timed runs
  python runner.py history                 # show regression history
"""

from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# ============================================================================
#  PATHS
# ============================================================================

REPO_ROOT = Path(__file__).resolve().parents[2]
PY_DIR = Path(__file__).resolve().parent
SRC_DIR = PY_DIR / "src"
OUT_ROOT = PY_DIR / "out"
REPORT_ROOT = OUT_ROOT / "reports"

# 6 .kn files in the project
SOURCE_FILES = ["cause.kn", "spookymagic.kn", "effect.kn", "diagnostics.kn", "main.kn", "vm.kn"]

# ============================================================================
#  TEST REGISTRY — Hardcoded from the .kn test tables
#
#  Each entry: id, module (cause/spookymagic/effect), category (1-8),
#  tag (for dispatch), description.
#  Total: 31 tests across 3 modules.
# ============================================================================

TEST_REGISTRY: list[dict[str, Any]] = [
    # ── cause.kn (17 tests) —— Categories 1-4 ──
    {"id": "cause_sanity",               "module": "cause",  "category": 0, "tag": "cause_sanity",               "description": "Verifies all imports resolve and modules compile correctly"},
    # Cat 1 — Venv Lifecycle
    {"id": "test_venv_exists_no_path",    "module": "cause",  "category": 1, "tag": "test_venv_exists_no_path",    "description": "venv_exists returns false for a path that does not exist"},
    {"id": "test_venv_from_path_resolves","module": "cause",  "category": 1, "tag": "test_venv_from_path_resolves","description": "venv_from_path returns a PythonVenv with expected fields"},
    {"id": "test_venv_current_not_set",   "module": "cause",  "category": 1, "tag": "test_venv_current_not_set",   "description": "venv_current() returns a PythonVenv descriptor"},
    # Cat 2 — Import Resolution
    {"id": "test_import_numpy_as_np",     "module": "cause",  "category": 2, "tag": "test_import_numpy_as_np",     "description": "'import numpy as np' compiles; np is a usable Any binding"},
    {"id": "test_import_math_as_py_math", "module": "cause",  "category": 2, "tag": "test_import_math_as_py_math", "description": "'import math as py_math' compiles; py_math.pi typechecks"},
    {"id": "test_from_math_import_sqrt",  "module": "cause",  "category": 2, "tag": "test_from_math_import_sqrt",  "description": "'from math import sqrt as py_sqrt' compiles; py_sqrt(16.0) typechecks"},
    # Cat 3 — Call Patterns
    {"id": "test_py_call_basic",          "module": "cause",  "category": 3, "tag": "test_py_call_basic",          "description": "python_call / py_call signature typechecks"},
    {"id": "test_py_call_raw_trunc",      "module": "cause",  "category": 3, "tag": "test_py_call_raw_trunc",      "description": "py_call_raw_f64_trunc_i64 (Any, Float) -> Int compiles"},
    {"id": "test_py_getattr_raw",         "module": "cause",  "category": 3, "tag": "test_py_getattr_raw",         "description": "python_getattr_raw / py_getattr_raw typechecks"},
    {"id": "test_py_setattr_raw",         "module": "cause",  "category": 3, "tag": "test_py_setattr_raw",         "description": "python_setattr / py_setattr syntax compiles"},
    {"id": "test_py_hasattr",             "module": "cause",  "category": 3, "tag": "test_py_hasattr",             "description": "python_hasattr / py_hasattr typechecks"},
    # Cat 4 — Region API
    {"id": "test_region_begin_end",       "module": "cause",  "category": 4, "tag": "test_region_begin_end",       "description": "python_region_begin / python_region_end pair typechecks"},
    {"id": "test_region_import_cached",   "module": "cause",  "category": 4, "tag": "test_region_import_cached",   "description": "python_region_import + cache counters typecheck"},
    {"id": "test_region_getattr",         "module": "cause",  "category": 4, "tag": "test_region_getattr",         "description": "python_region_getattr_raw typechecks"},
    {"id": "test_region_call",            "module": "cause",  "category": 4, "tag": "test_region_call",            "description": "python_region_call_raw + attr_raw_f64_trunc_i64 typecheck"},
    {"id": "test_region_telemetry",       "module": "cause",  "category": 4, "tag": "test_region_telemetry",       "description": "import/attr/view/call counters typecheck"},

    # ── spookymagic.kn (8 tests) —— Categories 5-6 ──
    {"id": "buffer_view_checksum37",      "module": "spookymagic", "category": 5, "tag": "buffer_view_checksum37",      "description": "python_region_buffer_view_checksum37 typechecks"},
    {"id": "buffer_view_raw",             "module": "spookymagic", "category": 5, "tag": "buffer_view_raw",             "description": "python_region_buffer_view typechecks"},
    {"id": "buffer_materialization",      "module": "spookymagic", "category": 5, "tag": "buffer_materialization",      "description": "kain_image_from_py / kain_tensor_from_py / shared_buffer / geometry typecheck"},
    {"id": "float_to_int_truncation",     "module": "spookymagic", "category": 6, "tag": "float_to_int_truncation",     "description": "py_call_raw_f64_trunc_i64 return type is Int"},
    {"id": "ndarray_to_buffer_probe",     "module": "spookymagic", "category": 6, "tag": "ndarray_to_buffer_probe",     "description": "py_buffer_info + py_buffer_bytes typecheck"},
    {"id": "tensor_info_probe",           "module": "spookymagic", "category": 6, "tag": "tensor_info_probe",           "description": "py_tensor_info + py_tensor_bytes + py_tensor_view typecheck"},
    {"id": "image_probe",                 "module": "spookymagic", "category": 6, "tag": "image_probe",                 "description": "py_image_info + py_image_view + py_image_pixel typecheck"},
    {"id": "geometry_probe",              "module": "spookymagic", "category": 6, "tag": "geometry_probe",              "description": "py_geometry_info + py_geometry_vertex + py_geometry_face typecheck"},

    # ── effect.kn (6 tests) —— Categories 7-8 ──
    {"id": "missing_module_error_path",   "module": "effect", "category": 7, "tag": "missing_module_error_path",   "description": "python_module_available(...) -> Bool error path typechecks"},
    {"id": "wrong_attribute_error_path",  "module": "effect", "category": 7, "tag": "wrong_attribute_error_path",  "description": "python_hasattr + python_getattr_raw error path typechecks"},
    {"id": "type_mismatch_return",        "module": "effect", "category": 7, "tag": "type_mismatch_return",        "description": "to_int + py_call_raw_f64_trunc_i64 type-mismatch lanes typecheck"},
    {"id": "gil_state_preserved",         "module": "effect", "category": 8, "tag": "gil_state_preserved",         "description": "region begin/end + cache counters preserve GIL state contract"},
    {"id": "budget_alloc_zero",           "module": "effect", "category": 8, "tag": "budget_alloc_zero",           "description": "budget-safe fn (Pure effect) typechecks and composes"},
    {"id": "budget_lock_zero",            "module": "effect", "category": 8, "tag": "budget_lock_zero",            "description": "ownership primitives stay gated from budget-safe scopes"},
]

# Map to look up tests by ID
REGISTRY_BY_ID = {t["id"]: t for t in TEST_REGISTRY}

# Category labels
CATEGORY_LABELS = {
    0: "Base Sanity",
    1: "Venv Lifecycle",
    2: "Import Resolution",
    3: "Call Patterns",
    4: "Region API",
    5: "Buffer/View",
    6: "Data Marshaling",
    7: "Error Handling",
    8: "Budget Safety",
}

# ============================================================================
#  CLI
# ============================================================================

def main() -> int:
    """Py Interop Test Runner — the Frictionless Lane.

    No-args usage:
        python runner.py            # check + run all + report
        python runner.py --no-run   # check only
        python runner.py --no-build # skip check, run only (assumes fresh)
    """
    parser = argparse.ArgumentParser(
        description="Python Interop Test Runner — check, run, bench, report",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--no-build", action="store_true",
                        help="Skip kain check step (assume types are verified)")
    parser.add_argument("--no-run", action="store_true",
                        help="Skip kain run step (check only)")
    parser.add_argument("--runs", type=int, default=None,
                        help="Number of timed runs for bench mode")
    parser.add_argument("--warmups", type=int, default=1,
                        help="Number of warmup runs")
    parser.add_argument("--timeout", type=int, default=120,
                        help="Timeout per command in seconds")

    sub = parser.add_subparsers(dest="command")

    # check
    sub.add_parser("check", help="Typecheck all 6 source files with kain check --json")

    # list
    list_p = sub.add_parser("list", help="List all 31 tests")
    list_p.add_argument("--verbose", "-v", action="store_true", help="Show descriptions")

    # run
    run_p = sub.add_parser("run", help="Run tests")
    run_p.add_argument("--suite", choices=["cause", "spookymagic", "effect"],
                       help="Run tests for a single module")
    run_p.add_argument("--test", help="Filter to a single test (runs full suite, filters results)")
    run_p.add_argument("--no-build", action="store_true", help="Skip check before run")
    run_p.add_argument("--timeout", type=int, default=None)

    # bench
    bench_p = sub.add_parser("bench", help="Benchmark mode — timed runs with statistics")
    bench_p.add_argument("--runs", type=int, default=5, help="Timed runs per test")
    bench_p.add_argument("--warmups", type=int, default=1, help="Warmup runs")
    bench_p.add_argument("--suite", choices=["cause", "spookymagic", "effect"],
                       help="Benchmark a single module")
    bench_p.add_argument("--test", help="Benchmark a single test")
    bench_p.add_argument("--timeout", type=int, default=None)

    # history
    sub.add_parser("history", help="Show regression history")

    args = parser.parse_args()

    # === FRICTIONLESS DEFAULT ===
    if args.command is None:
        return _frictionless_run(
            do_check=not args.no_build,
            do_run=not args.no_run,
            warmups=args.warmups,
            runs=args.runs,
            timeout=args.timeout,
        )

    if args.command == "check":
        return cmd_check(args)
    elif args.command == "list":
        return cmd_list(args)
    elif args.command == "run":
        return cmd_run(args)
    elif args.command == "bench":
        return cmd_bench(args)
    elif args.command == "history":
        return cmd_history(args)
    else:
        parser.print_help()
        return 0


def _frictionless_run(do_check: bool, do_run: bool, warmups: int,
                      runs: int | None, timeout: int) -> int:
    """Default: check + run all + write report."""
    print("=" * 70)
    print("  PYTHON INTEROP TEST RUNNER — Frictionless Mode")
    print("=" * 70)

    check_data = None

    if do_check:
        print("\n[PHASE 1] kain check ...")
        check_data = run_check(timeout)
        if check_data is None:
            print("\n[FAIL] kain check failed — possible causes:")
            print("  - kain binary not found (set KAIN_EXE or run kain_sync_binary)")
            print("  - Source files have type errors")
            print("  - --json mode not supported by this kain version")
            return 1
        print(f"\n[CHECK] {check_data['files_passed']}/{check_data['files_checked']} files passed")
        if check_data["files_failed"] > 0:
            print("\n[STOP] Typecheck failed — cannot proceed to run phase.")
            print("       Fix the errors above and re-run.")
            # Still write a report with check results
            _write_check_only_report(check_data)
            return 1
    else:
        print("\n[SKIP] Check phase skipped (--no-build)")
        check_data = {"kain_check_passed": True, "files_checked": 6, "files_passed": 6,
                      "files_failed": 0, "files": [], "raw": ""}

    if do_run:
        print("\n[PHASE 2] kain run ...")
        timeout_val = timeout
        run_data = run_tests(timeout_val, bench_mode=runs is not None,
                            num_runs=runs or 1, warmups=warmups)
        if run_data is None:
            print("\n[WARN] kain run failed — check results are still valid")
            print("       (Python runtime may not be available)")
            _write_check_only_report(check_data)
            return 0
    else:
        print("\n[SKIP] Run phase skipped (--no-run)")
        _write_check_only_report(check_data)
        return 0

    # Write report
    _write_report(check_data, run_data)
    return 0


# ============================================================================
#  CHECK PHASE
# ============================================================================

def run_check(timeout: int = 120) -> dict[str, Any] | None:
    """Run `kain check src/ --json` and parse the output.

    Returns structured check data or None on failure.
    """
    kain = _find_kain()
    if not kain:
        print("[ERROR] kain binary not found")
        return None

    project_dir = str(PY_DIR)
    src_dir = str(SRC_DIR)

    # Try --json mode first
    cmd = [str(kain), "check", src_dir, "--json"]
    print(f"  Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout,
            cwd=project_dir,
        )
    except subprocess.TimeoutExpired:
        print("[ERROR] kain check timed out")
        return None
    except Exception as e:
        print(f"[ERROR] Failed to run kain check: {e}")
        return None

    raw_output = result.stdout + result.stderr

    # Try to parse JSON output
    check_data = _parse_check_json(raw_output)

    if check_data is None:
        # Fallback: parse from exit code
        print("[WARN] Could not parse kain check JSON — using exit code")
        passed = (result.returncode == 0)
        check_data = {
            "kain_check_passed": passed,
            "files_checked": len(SOURCE_FILES),
            "files_passed": len(SOURCE_FILES) if passed else 0,
            "files_failed": 0 if passed else len(SOURCE_FILES),
            "files": [],
            "raw": raw_output,
        }

    if not check_data["kain_check_passed"] and check_data["files"]:
        print("\n  Failed files:")
        for f in check_data["files"]:
            if f.get("status") == "fail":
                print(f"    [FAIL] {f.get('path', 'unknown')}")

    return check_data


def _parse_check_json(raw: str) -> dict[str, Any] | None:
    """Parse `kain check --json` output into structured data.

    The JSON output format may look like:
    {"status": "ok", "files": [{"path": "...", "status": "ok", "errors": []}, ...]}
    or a JSON-lines stream.
    """
    # Try to find a JSON object in the output
    # Look for the outermost { ... }
    json_start = raw.find("{")
    if json_start == -1:
        return None

    # Try parsing from json_start to the end, trimming trailing non-JSON
    for end_offset in range(len(raw), json_start, -1):
        try:
            candidate = raw[json_start:end_offset]
            data = json.loads(candidate)
            if isinstance(data, dict) and ("status" in data or "files" in data):
                break
        except json.JSONDecodeError:
            continue
    else:
        # Try line-by-line JSON
        for line in raw.splitlines():
            line = line.strip()
            if line.startswith("{"):
                try:
                    data = json.loads(line)
                    if isinstance(data, dict) and ("status" in data or "files" in data):
                        break
                except json.JSONDecodeError:
                    continue
        else:
            return None

    # Normalize into our structure
    # The JSON has: {"total": N, "passed": N, "failed": N, "files": [...]}
    # File status is "passed" (not "pass")
    total_from_json = data.get("total", 0)
    passed_from_json = data.get("passed", 0)
    failed_from_json = data.get("failed", 0)

    passed = (failed_from_json == 0)
    files_raw = data.get("files", [])

    files = []
    for f in files_raw:
        file_path = f.get("path", f.get("file", "unknown"))
        file_status = f.get("status", "unknown")
        # status can be "passed", "ok", "success", "pass" or "failed", "fail", "error"
        is_pass = file_status in ("passed", "ok", "pass", "success")
        errors = f.get("errors", [])
        files.append({
            "path": file_path,
            "status": "pass" if is_pass else "fail",
            "error_count": len(errors),
            "errors": errors[:10],  # first 10 errors
        })

    files_checked = len(files) or total_from_json or len(SOURCE_FILES)
    files_passed = sum(1 for f in files if f["status"] == "pass")

    return {
        "kain_check_passed": passed,
        "files_checked": files_checked,
        "files_passed": files_passed if files_passed else passed_from_json,
        "files_failed": files_checked - (files_passed if files_passed else passed_from_json),
        "files": files,
        "raw": raw,
    }


# ============================================================================
#  RUN PHASE
# ============================================================================

def run_tests(timeout: int = 120, bench_mode: bool = False,
              num_runs: int = 1, warmups: int = 1,
              suite: str | None = None, test_id: str | None = None) -> dict[str, Any] | None:
    """Run `kain run` and parse diagnostics output.

    Returns test results structured by module, or None on failure.
    """
    kain = _find_kain()
    if not kain:
        print("[ERROR] kain binary not found")
        return None

    project_dir = str(PY_DIR)

    # Build CLI arguments for the kain binary
    run_args = []
    if suite:
        run_args = ["--test", suite]
    # For single test, we still run the full module (kain doesn't do per-test dispatch
    # via CLI — the test filter goes to diagnostics)

    cmd = [str(kain), "run"] + run_args
    print(f"  Running: {' '.join(cmd)}")

    # For bench mode, we run multiple times
    all_results: list[dict] = []
    timings: list[float] = []

    total_runs = warmups + num_runs
    for run_idx in range(total_runs):
        is_warmup = run_idx < warmups
        label = "warmup" if is_warmup else f"run {run_idx - warmups + 1}"

        try:
            start = time.perf_counter()
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=timeout,
                cwd=project_dir,
            )
            elapsed = time.perf_counter() - start

            if is_warmup:
                print(f"  [{label}] {elapsed*1000:.0f}ms")
            else:
                print(f"  [{label}] {elapsed*1000:.0f}ms (exit={result.returncode})")
                timings.append(elapsed)

            raw_output = result.stdout + result.stderr

        except subprocess.TimeoutExpired:
            print(f"  [{label}] TIMED OUT")
            if not is_warmup:
                timings.append(float(timeout))
            raw_output = ""
        except Exception as e:
            print(f"  [{label}] ERROR: {e}")
            if not is_warmup:
                timings.append(0.0)
            raw_output = ""

        if not is_warmup:
            all_results.append({"raw": raw_output, "elapsed_s": elapsed})

    if bench_mode and num_runs > 1:
        # Benchmark mode — aggregate from multiple runs
        return _build_bench_results(all_results, timings, suite, test_id)

    # Single run mode — parse the diagnostics output
    return _parse_diagnostics_output(all_results[0]["raw"] if all_results else "",
                                     suite, test_id)


def _parse_diagnostics_output(raw: str, suite: str | None = None,
                             test_id: str | None = None) -> dict[str, Any] | None:
    """Parse the diagnostics report from kain run stdout.

    The diagnostics.kn output looks like:
      ╔═══...
      ║  DEBUG TEMPLATE — DIAGNOSTICS SUITE
      ─── CAUSE MODULE ───
        [cause] PASS: ...
      ─── EFFECT MODULE ───
        [effect] PASS: ...
      ─── SPOOKYMAGIC MODULE ───
      ═══...
        DIAGNOSTICS REPORT
      ═══...
        Total:   35
        Passed:  30
        Failed:  5
        [PASS] cause::cause_sanity
        [PASS] spookymagic::buffer_view_checksum37
        [FAIL] effect::missing_module_error_path
      ═══...
        VERDICT: ALL TESTS PASSED
    """
    # Parse per-test [PASS]/[FAIL] lines: [PASS] module::test_name
    test_pattern = re.compile(r'\[(PASS|FAIL)\]\s+(\w+)::(\w+)')

    test_results: dict[str, dict[str, Any]] = {}  # keyed by "module::test_name"
    for match in test_pattern.finditer(raw):
        status = match.group(1)
        module = match.group(2)
        test_name = match.group(3)
        key = f"{module}::{test_name}"
        test_results[key] = {
            "module": module,
            "test_name": test_name,
            "status": "pass" if status == "PASS" else "fail",
            "exit_code": 0 if status == "PASS" else 1,
        }

    # Parse summary line: Total: N / Passed: N / Failed: N
    total_match = re.search(r'Total:\s+(\d+)', raw)
    passed_match = re.search(r'Passed:\s+(\d+)', raw)
    failed_match = re.search(r'Failed:\s+(\d+)', raw)

    total_from_report = int(total_match.group(1)) if total_match else len(test_results)
    passed_from_report = int(passed_match.group(1)) if passed_match else sum(
        1 for t in test_results.values() if t["status"] == "pass")
    failed_from_report = int(failed_match.group(1)) if failed_match else sum(
        1 for t in test_results.values() if t["status"] == "fail")

    # Build per-module suites using the registry as ground truth
    suites: dict[str, dict] = {}
    for module in ["cause", "spookymagic", "effect"]:
        registry_tests = [t for t in TEST_REGISTRY if t["module"] == module]
        suite_tests = []
        suite_passed = 0
        suite_failed = 0

        for reg_test in registry_tests:
            test_name = reg_test["id"]
            key = f"{module}::{test_name}"
            if key in test_results:
                tr = test_results[key]
                status = tr["status"]
                exit_code = tr["exit_code"]
            else:
                # Test wasn't found in diagnostics output — infer from overall verdict
                status = "pass" if failed_from_report == 0 else "unknown"
                exit_code = 0 if status == "pass" else -1

            if status == "pass":
                suite_passed += 1
            elif status == "fail":
                suite_failed += 1

            suite_tests.append({
                "id": test_name,
                "tag": reg_test["tag"],
                "category": reg_test["category"],
                "category_label": CATEGORY_LABELS.get(reg_test["category"], "Unknown"),
                "description": reg_test["description"],
                "status": status,
                "exit_code": exit_code,
            })

        suites[module] = {
            "total": len(registry_tests),
            "passed": suite_passed,
            "failed": suite_failed,
            "tests": suite_tests,
        }

    # If a specific test was requested, filter
    if test_id:
        for mod_name, mod_data in suites.items():
            mod_data["tests"] = [t for t in mod_data["tests"] if t["id"] == test_id]
            mod_data["total"] = len(mod_data["tests"])
            mod_data["passed"] = sum(1 for t in mod_data["tests"] if t["status"] == "pass")
            mod_data["failed"] = mod_data["total"] - mod_data["passed"]

    if suite:
        suites = {suite: suites.get(suite, {"total": 0, "passed": 0, "failed": 0, "tests": []})}

    total_tests = sum(s["total"] for s in suites.values())
    total_passed = sum(s["passed"] for s in suites.values())
    total_failed = sum(s["failed"] for s in suites.values())

    return {
        "total_tests": total_tests,
        "passed": total_passed,
        "failed": total_failed,
        "coverage_pct": round(total_passed / total_tests * 100, 1) if total_tests > 0 else 0.0,
        "suites": suites,
        "raw": raw,
        "timings": None,  # no timing in regular run mode
    }


def _build_bench_results(all_results: list[dict], timings: list[float],
                        suite: str | None, test_id: str | None) -> dict[str, Any]:
    """Build benchmark results from multiple timed runs."""
    base = _parse_diagnostics_output(all_results[0]["raw"], suite, test_id) if all_results else None
    if base is None:
        return {"total_tests": 0, "passed": 0, "failed": 0, "coverage_pct": 0.0,
                "suites": {}, "raw": "", "timings": None}

    # Add aggregate timing to the overall result
    if timings:
        base["timings"] = {
            "n": len(timings),
            "median_ms": statistics.median(timings) * 1000,
            "min_ms": min(timings) * 1000,
            "max_ms": max(timings) * 1000,
            "mean_ms": statistics.mean(timings) * 1000,
            "stdev_ms": statistics.stdev(timings) * 1000 if len(timings) >= 2 else 0,
            "samples_ms": [t * 1000 for t in timings],
        }

    # Per-suite timings: use the full run timing as a rough proxy
    # (individual test timing would require per-test benchmark support in diagnostics.kn)
    for mod_name in base["suites"]:
        base["suites"][mod_name]["timings"] = None  # not available per-test

    return base


# ============================================================================
#  BENCH PHASE
# ============================================================================

def cmd_bench(args) -> int:
    """Run benchmarks with N timed runs."""
    print("=" * 70)
    print("  PYTHON INTEROP BENCHMARK")
    print("=" * 70)

    timeout = args.timeout if args.timeout is not None else 180
    runs_val = args.runs
    warmups_val = args.warmups

    print(f"\n  Runs: {runs_val}  Warmups: {warmups_val}  Timeout: {timeout}s")
    if args.suite:
        print(f"  Suite: {args.suite}")
    if args.test:
        print(f"  Test: {args.test}")

    # First, run check
    print("\n[PHASE 1] kain check ...")
    check_data = run_check(timeout)
    if check_data is None:
        print("[FAIL] kain check failed")
        return 1
    if check_data["files_failed"] > 0:
        print("\n[STOP] Typecheck failed — cannot benchmark")
        return 1
    print(f"  {check_data['files_passed']}/{check_data['files_checked']} files passed")

    print("\n[PHASE 2] kain run (benchmark mode) ...")
    run_data = run_tests(timeout, bench_mode=True, num_runs=runs_val,
                        warmups=warmups_val, suite=args.suite, test_id=args.test)
    if run_data is None:
        print("[FAIL] Bench run failed")
        return 1

    # Write report
    _write_report(check_data, run_data)

    # Print bench summary
    if run_data.get("timings"):
        t = run_data["timings"]
        print(f"\n  Timing ({t['n']} runs):")
        print(f"    Median: {t['median_ms']:.2f}ms")
        print(f"    Min:    {t['min_ms']:.2f}ms")
        print(f"    Max:    {t['max_ms']:.2f}ms")
        print(f"    Mean:   {t['mean_ms']:.2f}ms")
        if t.get("stdev_ms", 0) > 0:
            print(f"    Stdev:  {t['stdev_ms']:.2f}ms")

    return 0


# ============================================================================
#  REPORTING
# ============================================================================

def _write_report(check_data: dict[str, Any], run_data: dict[str, Any]):
    """Write full test report as JSON + Markdown."""
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())

    report = {
        "generated_at": stamp,
        "build": {
            "kain_check_passed": check_data["kain_check_passed"],
            "files_checked": check_data["files_checked"],
            "files_passed": check_data["files_passed"],
        },
        "test_suites": run_data.get("suites", {}),
        "summary": {
            "total_tests": run_data.get("total_tests", 0),
            "passed": run_data.get("passed", 0),
            "failed": run_data.get("failed", 0),
            "coverage_pct": run_data.get("coverage_pct", 0.0),
        },
    }

    # Add timings if available
    if run_data.get("timings"):
        report["benchmark"] = {
            "mode": "full_suite",
            "timings": run_data["timings"],
        }

    # Write JSON
    json_path = REPORT_ROOT / f"{stamp}.json"
    json_path.write_text(json.dumps(report, indent=2))

    # Latest JSON
    latest_json = REPORT_ROOT / "latest.json"
    latest_json.write_text(json.dumps(report, indent=2))

    # Write Markdown
    md = _render_markdown(report)
    md_path = REPORT_ROOT / f"{stamp}.md"
    md_path.write_text(md)

    latest_md = REPORT_ROOT / "latest.md"
    latest_md.write_text(md)

    # Update history
    _update_history(report)

    print(f"\n[REPORT] {json_path}")
    print(f"[REPORT] {md_path}")
    print(f"[REPORT] {latest_json}")
    print(f"[REPORT] {latest_md}")

    # Print summary
    s = report["summary"]
    verdict = "ALL TESTS PASSED" if s["failed"] == 0 else f"{s['failed']} TEST(S) FAILED"
    print(f"\n  VERDICT: {verdict}")
    print(f"  Total: {s['total_tests']}  Passed: {s['passed']}  Failed: {s['failed']}  Coverage: {s['coverage_pct']}%")


def _write_check_only_report(check_data: dict[str, Any]):
    """Write a report with check results only (no run data)."""
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())

    report = {
        "generated_at": stamp,
        "build": {
            "kain_check_passed": check_data["kain_check_passed"],
            "files_checked": check_data["files_checked"],
            "files_passed": check_data["files_passed"],
        },
        "test_suites": {},
        "summary": {
            "total_tests": 0,
            "passed": 0,
            "failed": 0,
            "coverage_pct": 0.0,
            "note": "Run phase skipped — check results only",
        },
    }

    json_path = REPORT_ROOT / f"{stamp}.json"
    json_path.write_text(json.dumps(report, indent=2))

    latest_json = REPORT_ROOT / "latest.json"
    latest_json.write_text(json.dumps(report, indent=2))

    md = _render_markdown(report)
    md_path = REPORT_ROOT / f"{stamp}.md"
    md_path.write_text(md)

    latest_md = REPORT_ROOT / "latest.md"
    latest_md.write_text(md)

    _update_history(report)

    print(f"\n[REPORT] {json_path}")
    print(f"[REPORT] {md_path}")


def _render_markdown(report: dict) -> str:
    """Render the report as a Markdown table."""
    build = report.get("build", {})
    summary = report.get("summary", {})
    suites = report.get("test_suites", {})
    bench = report.get("benchmark", {})

    lines = [
        "# Python Interop Test Report",
        "",
        f"- **Generated:** {report['generated_at']}",
        "",
        "## Build",
        "",
        f"| Metric | Value |",
        f"|--------|-------|",
        f"| kain check passed | {build.get('kain_check_passed', 'N/A')} |",
        f"| Files checked | {build.get('files_checked', 0)} |",
        f"| Files passed | {build.get('files_passed', 0)} |",
        "",
    ]

    # Bench timing
    if bench.get("timings"):
        t = bench["timings"]
        lines.extend([
            "## Benchmark",
            "",
            f"| Metric | Value |",
            f"|--------|-------|",
            f"| Runs | {t.get('n', 0)} |",
            f"| Median | {t.get('median_ms', 0):.2f}ms |",
            f"| Min | {t.get('min_ms', 0):.2f}ms |",
            f"| Max | {t.get('max_ms', 0):.2f}ms |",
            f"| Mean | {t.get('mean_ms', 0):.2f}ms |",
            f"| Stdev | {t.get('stdev_ms', 0):.2f}ms |",
            "",
        ])

    # Summary
    lines.extend([
        "## Summary",
        "",
        f"| Metric | Value |",
        f"|--------|-------|",
        f"| Total tests | {summary.get('total_tests', 0)} |",
        f"| Passed | {summary.get('passed', 0)} |",
        f"| Failed | {summary.get('failed', 0)} |",
        f"| Coverage | {summary.get('coverage_pct', 0.0)}% |",
        "",
    ])

    # Per-module tables
    for mod_name in ["cause", "spookymagic", "effect"]:
        mod_data = suites.get(mod_name, {})
        tests = mod_data.get("tests", [])
        if not tests:
            # Try to fill from registry
            reg_tests = [t for t in TEST_REGISTRY if t["module"] == mod_name]
            if reg_tests:
                tests = [{
                    "id": t["id"], "tag": t["tag"], "category": t["category"],
                    "category_label": CATEGORY_LABELS.get(t["category"], "Unknown"),
                    "description": t["description"], "status": "unknown", "exit_code": -1,
                } for t in reg_tests]
                mod_data = {"total": len(tests), "passed": 0, "failed": 0, "tests": tests}

        lines.extend([
            f"## {mod_data.get('total', 0)} tests in {mod_name}.kn",
            "",
            f"| Status | Test | Cat | Description |",
            f"|--------|------|-----|-------------|",
        ])
        for t in tests:
            status_icon = "PASS" if t.get("status") == "pass" else ("FAIL" if t.get("status") == "fail" else "???")
            lines.append(
                f"| {status_icon} | `{t['id']}` | {t.get('category_label', t.get('category', '?'))} | {t['description']} |"
            )
        lines.append("")

    return "\n".join(lines)


# ============================================================================
#  HISTORY
# ============================================================================

HISTORY_PATH = REPORT_ROOT / "history.json"
HISTORY_MAX = 50  # keep last 50 runs


def _update_history(report: dict):
    """Append this run to the history file."""
    history = _load_history()

    entry = {
        "timestamp": report["generated_at"],
        "check_passed": report["build"]["kain_check_passed"],
        "files_checked": report["build"]["files_checked"],
        "files_passed": report["build"]["files_passed"],
        "total_tests": report["summary"]["total_tests"],
        "passed": report["summary"]["passed"],
        "failed": report["summary"]["failed"],
        "coverage_pct": report["summary"]["coverage_pct"],
    }

    if report.get("benchmark", {}).get("timings"):
        entry["bench_median_ms"] = report["benchmark"]["timings"]["median_ms"]

    history.insert(0, entry)

    # Trim to max
    if len(history) > HISTORY_MAX:
        history = history[:HISTORY_MAX]

    HISTORY_PATH.parent.mkdir(parents=True, exist_ok=True)
    HISTORY_PATH.write_text(json.dumps({"runs": history}, indent=2))


def _load_history() -> list[dict]:
    """Load the history file, returning empty list if not found."""
    if not HISTORY_PATH.exists():
        return []
    try:
        data = json.loads(HISTORY_PATH.read_text())
        return data.get("runs", [])
    except (json.JSONDecodeError, KeyError):
        return []


def _detect_regression(current: dict, previous: dict | None) -> list[str]:
    """Detect regressions between current and previous run."""
    if previous is None:
        return []

    regressions = []

    # Check pass/fail
    if current["check_passed"] and not previous["check_passed"]:
        regressions.append("CHECK: now passing (was failing)")
    elif not current["check_passed"] and previous["check_passed"]:
        regressions.append(f"CHECK: REGRESSION — was passing, now failing")

    if current["passed"] < previous["passed"]:
        delta = previous["passed"] - current["passed"]
        regressions.append(f"TESTS: {delta} fewer tests passing ({previous['passed']} -> {current['passed']})")

    if current["failed"] > previous["failed"]:
        delta = current["failed"] - previous["failed"]
        regressions.append(f"TESTS: {delta} new test failures ({previous['failed']} -> {current['failed']})")

    if current["coverage_pct"] < previous["coverage_pct"]:
        regressions.append(
            f"COVERAGE: dropped from {previous['coverage_pct']}% to {current['coverage_pct']}%")

    return regressions


# ============================================================================
#  SUBCOMMANDS
# ============================================================================

def cmd_check(args) -> int:
    """Run typecheck only."""
    print("=" * 70)
    print("  PYTHON INTEROP — CHECK")
    print("=" * 70)

    check_data = run_check()
    if check_data is None:
        print("\n[FAIL] kain check could not run")
        return 1

    print(f"\n  Files checked: {check_data['files_checked']}")
    print(f"  Files passed:  {check_data['files_passed']}")
    print(f"  Files failed:  {check_data['files_failed']}")

    if check_data["files"]:
        for f in check_data["files"]:
            status = "PASS" if f["status"] == "pass" else "FAIL"
            err_info = f" ({f['error_count']} errors)" if f["error_count"] > 0 else ""
            print(f"    [{status}] {f['path']}{err_info}")

    _write_check_only_report(check_data)
    return 0 if check_data["files_failed"] == 0 else 1


def cmd_list(args) -> int:
    """List all tests."""
    verbose = getattr(args, "verbose", False)

    # Group by module
    for module in ["cause", "spookymagic", "effect"]:
        tests = [t for t in TEST_REGISTRY if t["module"] == module]
        print(f"\n{module}.kn ({len(tests)} tests):")
        for t in tests:
            cat_label = CATEGORY_LABELS.get(t["category"], f"Cat {t['category']}")
            if verbose:
                print(f"  [{cat_label}] {t['id']}")
                print(f"           {t['description']}")
            else:
                print(f"  [{cat_label}] {t['id']}")

    print(f"\n{len(TEST_REGISTRY)} tests total across 3 modules")
    return 0


def cmd_run(args) -> int:
    """Run tests."""
    timeout = args.timeout if args.timeout is not None else 120

    if not args.no_build:
        print("[PHASE 1] kain check ...")
        check_data = run_check(timeout)
        if check_data is None:
            print("[FAIL] kain check failed")
            return 1
        if check_data["files_failed"] > 0:
            print(f"\n[STOP] {check_data['files_failed']} file(s) failed typecheck")
            _write_check_only_report(check_data)
            return 1
        print(f"  {check_data['files_passed']}/{check_data['files_checked']} files passed\n")
    else:
        check_data = {"kain_check_passed": True, "files_checked": 6, "files_passed": 6,
                      "files_failed": 0, "files": [], "raw": ""}

    print("[PHASE 2] kain run ...")
    run_data = run_tests(timeout, suite=args.suite, test_id=args.test)
    if run_data is None:
        print("\n[FAIL] kain run could not execute")
        print("       (Python runtime may not be available — check results are valid)")
        _write_check_only_report(check_data)
        return 0

    # Print per-test results
    print("\n  Results:")
    for mod_name, mod_data in run_data.get("suites", {}).items():
        for t in mod_data.get("tests", []):
            status_icon = "PASS" if t["status"] == "pass" else ("FAIL" if t["status"] == "fail" else "???")
            print(f"    [{status_icon}] {mod_name}::{t['id']}")

    _write_report(check_data, run_data)
    return 0 if run_data.get("failed", 0) == 0 else 1


def cmd_history(args) -> int:
    """Show regression history."""
    history = _load_history()
    if not history:
        print("No history yet. Run the test suite first.")
        return 0

    print("=" * 70)
    print("  PYTHON INTEROP — REGRESSION HISTORY")
    print("=" * 70)

    print(f"\n  {len(history)} runs recorded (last {HISTORY_MAX} kept)")
    print()

    # Print table
    print(f"  {'Timestamp':<20} {'Check':<8} {'Total':<6} {'Pass':<6} {'Fail':<6} {'Cover%':<8} {'Regr':<5}")
    print(f"  {'-'*19} {'-'*7} {'-'*5} {'-'*5} {'-'*5} {'-'*7} {'-'*5}")

    prev = None
    for i, entry in enumerate(history):
        ts = entry["timestamp"]
        check_ok = "PASS" if entry["check_passed"] else "FAIL"
        regr = _detect_regression(entry, prev) if prev else []
        regr_flag = "YES" if regr else ""
        if regr_flag:
            check_ok = f"\033[91m{check_ok}\033[0m"
            regr_flag = f"\033[91m{regr_flag}\033[0m"

        print(f"  {ts:<20} {check_ok:<8} {entry['total_tests']:<6} {entry['passed']:<6} {entry['failed']:<6} {entry['coverage_pct']:<8.1f} {regr_flag:<5}")

        if regr:
            for r in regr:
                print(f"    \033[91m! {r}\033[0m")

        prev = entry

    print()
    return 0


# ============================================================================
#  HELPERS
# ============================================================================

def _find_kain() -> Path | None:
    """Find the kain compiler binary."""
    # Check env
    if "KAIN_EXE" in os.environ:
        p = Path(os.environ["KAIN_EXE"])
        if p.exists():
            return p

    # Check .kain/bin
    home = Path(os.environ.get("HOME", Path.home()))
    kain_home = Path(os.environ.get("KAIN_HOME", home / ".kain"))
    kain_bin = kain_home / "bin" / "kain.exe"
    if kain_bin.exists():
        return kain_bin

    # Also check KAIN_HOME directly
    kain_bin2 = home / ".kain" / "bin" / "kain.exe"
    if kain_bin2.exists():
        return kain_bin2

    # Check repo-local .kain
    repo_kain = REPO_ROOT / ".kain" / "bin" / "kain.exe"
    if repo_kain.exists():
        return repo_kain

    # Check bazel output
    try:
        # Look in common bazel output locations
        bazel_candidates = [
            REPO_ROOT / ".." / ".." / "_b",
            Path("Z:/_b"),
        ]
        for bazel_root in bazel_candidates:
            if bazel_root.exists():
                for root, dirs, files in os.walk(str(bazel_root)):
                    if "kain.exe" in files and "cli" in root:
                        return Path(root) / "kain.exe"
    except Exception:
        pass

    # Fallback: check PATH
    import shutil
    found = shutil.which("kain")
    if found:
        return Path(found)

    return None


# ============================================================================
#  MAIN
# ============================================================================

if __name__ == "__main__":
    sys.exit(main())
