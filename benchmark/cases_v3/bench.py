#!/usr/bin/env python3
"""
CASES_V3 — Unified benchmark runner for the V3 god-file pipeline.

One compile per language, N benchmarks per binary. V3 compiles 5 files
instead of 240 and dispatches benchmarks by CLI argument.

Usage:
  bench.py build                          # compile all god files
  bench.py run --case binary_trees        # run one benchmark, all languages
  bench.py run --tier 1                   # run all Tier 1 benchmarks
  bench.py suite dev                      # run the dev suite from suites.json
  bench.py list                           # list all benchmarks
  bench.py expected --language cpp        # print expected checksums
"""

from __future__ import annotations

import argparse
import json
import os
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

REPO_ROOT = Path(__file__).resolve().parents[1]
CASES_V3 = Path(__file__).resolve().parent / "cases_v3"
OUT_ROOT = CASES_V3 / "out"
BUILD_ROOT = OUT_ROOT / "build"
REPORT_ROOT = OUT_ROOT / "reports"

# ============================================================================
#  BENCHMARK REGISTRY (auto-discovered from contract)
# ============================================================================

BENCHMARKS: list[dict[str, Any]] = [
    # Tier 1: Compute
    {"id": "binary_trees",         "tier": 1, "title": "Binary Trees",              "metric": "trees/s",         "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "nbody",                "tier": 1, "title": "N-Body Simulation",          "metric": "GFLOPS",          "work_items": None, "languages": ["kain","rust","cpp","zig"]},
    {"id": "spectral_norm",        "tier": 1, "title": "Spectral Norm",             "metric": "GFLOPS",          "work_items": None, "languages": ["kain","rust","cpp","zig"]},
    {"id": "mandelbrot",           "tier": 1, "title": "Mandelbrot Set",            "metric": "pixels/s",        "work_items": None, "languages": ["kain","rust","cpp","zig","go","mks"]},
    {"id": "fasta",                "tier": 1, "title": "FASTA DNA Generation",      "metric": "nucleotides/s",   "work_items": None, "languages": ["kain","rust","cpp","zig","go","mks"]},
    {"id": "regex_redux",          "tier": 1, "title": "Regex Redux",               "metric": "MB/s",            "work_items": None, "languages": ["kain","rust","cpp","go","mks"]},
    {"id": "pidigits",             "tier": 1, "title": "Pi Digits",                 "metric": "digits/s",        "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    # Tier 2: Data Structures
    {"id": "hashmap_heavy",        "tier": 2, "title": "HashMap Heavy",             "metric": "ops/s",           "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "btree_scan",           "tier": 2, "title": "BTree Scan",                "metric": "ops/s",           "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "sort_gauntlet",        "tier": 2, "title": "Sort Gauntlet",             "metric": "elements/s",      "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "vector_growth",        "tier": 2, "title": "Vector Growth",             "metric": "elements/s",      "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "graph_bfs",            "tier": 2, "title": "Graph BFS",                 "metric": "edges/s",         "work_items": None, "languages": ["kain","rust","cpp","go"]},
    # Tier 3: Memory
    {"id": "alloc_small_churn",    "tier": 3, "title": "Alloc Small Churn",         "metric": "allocs/s",        "work_items": None, "languages": ["kain","rust","cpp","zig"]},
    {"id": "alloc_large_objects",  "tier": 3, "title": "Alloc Large Objects",       "metric": "allocs/s",        "work_items": None, "languages": ["kain","rust","cpp","zig"]},
    {"id": "arena_vs_malloc",      "tier": 3, "title": "Arena vs Malloc",           "metric": "allocs/s",        "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "cache_march",          "tier": 3, "title": "Cache March",               "metric": "GB/s",            "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "rc_vs_gc_trace",       "tier": 3, "title": "RC vs GC Trace",            "metric": "nodes/s",         "work_items": None, "languages": ["kain","rust","cpp"]},
    # Tier 4: Concurrency
    {"id": "parallel_reduce",      "tier": 4, "title": "Parallel Reduce",           "metric": "elements/s",      "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "mutex_contention",     "tier": 4, "title": "Mutex Contention",          "metric": "increments/s",    "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "spsc_queue",           "tier": 4, "title": "SPSC Queue",                "metric": "items/s",         "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "mpmc_queue",           "tier": 4, "title": "MPMC Queue",                "metric": "items/s",         "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "actor_spam",           "tier": 4, "title": "Actor Spam",                "metric": "msgs/s",          "work_items": None, "languages": ["kain","rust","cpp","go"]},
    {"id": "async_ready_pipeline", "tier": 4, "title": "Async Ready Pipeline",      "metric": "awaits/s",        "work_items": None, "languages": ["kain","rust","cpp","go"]},
    # Tier 5: IO
    {"id": "file_read_streaming",  "tier": 5, "title": "File Read Streaming",       "metric": "MB/s",            "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "file_write_streaming", "tier": 5, "title": "File Write Streaming",      "metric": "MB/s",            "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    {"id": "tcp_echo_throughput",  "tier": 5, "title": "TCP Echo Throughput",       "metric": "MB/s",            "work_items": None, "languages": ["kain","rust","cpp","go"]},
    {"id": "process_spawn_chain",  "tier": 5, "title": "Process Spawn Chain",       "metric": "spawns/s",        "work_items": None, "languages": ["kain","rust","cpp","go"]},
    # Tier 6: FFI
    {"id": "c_ffi_call_hotloop",   "tier": 6, "title": "C FFI Call Hotloop",        "metric": "calls/s",         "work_items": None, "languages": ["kain","rust","cpp","zig"]},
    {"id": "c_buffer_handoff",     "tier": 6, "title": "C Buffer Handoff",          "metric": "roundtrips/s",    "work_items": None, "languages": ["kain","rust","cpp"]},
    # Tier 7: Compiler
    {"id": "build_self_stress",    "tier": 7, "title": "Build Self Stress",          "metric": "seconds",         "work_items": None, "languages": ["kain","rust","cpp","zig","go"]},
    # MKS-only benchmarks
    {"id": "scalar_mix",           "tier": 1, "title": "Scalar Mix",                "metric": "ops/s",           "work_items": None, "languages": ["mks"]},
    {"id": "recursive_sum",        "tier": 1, "title": "Recursive Sum",             "metric": "ops/s",           "work_items": None, "languages": ["mks"]},
    {"id": "branch_dispatch",      "tier": 1, "title": "Branch Dispatch",           "metric": "ops/s",           "work_items": None, "languages": ["mks"]},
    {"id": "string_ops",           "tier": 1, "title": "String Ops",                "metric": "ops/s",           "work_items": None, "languages": ["mks"]},
    {"id": "fizzbuzz_bomb",        "tier": 1, "title": "FizzBuzz Bomb",             "metric": "iters/s",         "work_items": None, "languages": ["mks"]},
    {"id": "prime_sieve",          "tier": 1, "title": "Prime Sieve",               "metric": "primes/s",        "work_items": None, "languages": ["mks"]},
    {"id": "fibonacci_mod",        "tier": 1, "title": "Fibonacci Mod",             "metric": "iters/s",         "work_items": None, "languages": ["mks"]},
]

# ============================================================================
#  LANGUAGE CONFIG
# ============================================================================

LANGUAGE_CONFIG: dict[str, dict[str, Any]] = {
    "kain": {
        "label": "Kain LLVM",
        "source": CASES_V3 / "kain" / "bench.kn",
        "binary": BUILD_ROOT / "kain" / "bench.exe",
        "build_cmd": None,  # filled at build time
        "extension": ".kn",
    },
    "rust": {
        "label": "Rust LLVM",
        "source": CASES_V3 / "rust" / "bench.rs",
        "binary": BUILD_ROOT / "rust" / "bench.exe",
        "build_cmd": None,
        "extension": ".rs",
    },
    "cpp": {
        "label": "C++ Clang",
        "source": CASES_V3 / "cpp" / "bench.cpp",
        "binary": BUILD_ROOT / "cpp" / "bench.exe",
        "build_cmd": None,
        "extension": ".cpp",
    },
    "zig": {
        "label": "Zig ReleaseFast",
        "source": CASES_V3 / "zig" / "bench.zig",
        "binary": BUILD_ROOT / "zig" / "bench.exe",
        "build_cmd": None,
        "extension": ".zig",
    },
    "go": {
        "label": "Go gc",
        "source": CASES_V3 / "go" / "bench.go",
        "binary": BUILD_ROOT / "go" / "bench.exe",
        "build_cmd": None,
        "extension": ".go",
    },
    "mks": {
        "label": "MarkScript VM",
        "source": CASES_V3 / "markscript" / "bench.md",
        "binary": None,  # MKS is interpreted, run via mks.exe
        "build_cmd": None,
        "extension": ".md",
    },
}

LANGUAGE_ORDER = ["kain", "rust", "cpp", "zig", "go", "mks"]


# ============================================================================
#  CLI
# ============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(description="CASES_V3 Benchmark Runner")
    sub = parser.add_subparsers(dest="command")

    # build
    build_p = sub.add_parser("build", help="Compile all god files")
    build_p.add_argument("--language", help="Build specific language only")

    # run
    run_p = sub.add_parser("run", help="Run benchmarks")
    run_p.add_argument("--case", help="Benchmark ID or comma-separated IDs")
    run_p.add_argument("--tier", type=int, help="Run all benchmarks in a tier (1-7)")
    run_p.add_argument("--language", help="Language(s) to run (comma-separated)")
    run_p.add_argument("--warmups", type=int, default=1)
    run_p.add_argument("--runs", type=int, default=3)
    run_p.add_argument("--timeout", type=int, default=300)

    # suite
    suite_p = sub.add_parser("suite", help="Run a named suite")
    suite_p.add_argument("name", help="Suite name from suites.json")

    # list
    sub.add_parser("list", help="List all benchmarks")

    # expected
    exp_p = sub.add_parser("expected", help="Show expected checksums")
    exp_p.add_argument("--language", default="cpp", help="Language to query")

    args = parser.parse_args()

    if args.command == "build":
        return cmd_build(args)
    elif args.command == "run":
        return cmd_run(args)
    elif args.command == "suite":
        return cmd_suite(args)
    elif args.command == "list":
        return cmd_list()
    elif args.command == "expected":
        return cmd_expected(args)
    else:
        parser.print_help()
        return 0


# ============================================================================
#  BUILD
# ============================================================================

def cmd_build(args) -> int:
    """Compile god files for selected languages."""
    languages = [args.language] if args.language else ["kain", "rust", "cpp", "zig", "go"]
    languages = [l for l in languages if l != "mks"]  # MKS has no build step

    BUILD_ROOT.mkdir(parents=True, exist_ok=True)

    for lang in languages:
        cfg = LANGUAGE_CONFIG.get(lang)
        if not cfg:
            print(f"Unknown language: {lang}")
            continue

        lang_build = BUILD_ROOT / lang
        lang_build.mkdir(parents=True, exist_ok=True)

        source = cfg["source"]
        if not source.exists():
            print(f"[SKIP] {lang}: source not found at {source}")
            continue

        binary = cfg["binary"]
        print(f"[BUILD] {lang}: {source} → {binary}")

        try:
            if lang == "kain":
                _build_kain(source, binary)
            elif lang == "rust":
                _build_rust(source, binary)
            elif lang == "cpp":
                _build_cpp(source, binary)
            elif lang == "zig":
                _build_zig(source, binary)
            elif lang == "go":
                _build_go(source, binary)
        except Exception as e:
            print(f"[FAIL] {lang}: {e}")
            return 1

    print("[BUILD] All languages built successfully")
    return 0


def _build_kain(source: Path, binary: Path):
    kain_exe = _find_kain()
    result = subprocess.run(
        [str(kain_exe), "build", str(source), "--target", "llvm", "-o", str(binary.with_suffix(""))],
        capture_output=True, text=True, timeout=120,
    )
    if result.returncode != 0:
        raise RuntimeError(f"kain build failed:\n{result.stderr[-500:]}")
    # kain outputs .exe at the -o path; verify
    if not binary.exists():
        raise RuntimeError(f"binary not produced at {binary}")


def _build_rust(source: Path, binary: Path):
    rustc = _find_tool("rustc")
    result = subprocess.run(
        [rustc, "-C", "opt-level=3", "-C", "target-cpu=native", "-C", "debuginfo=0",
         "-C", "panic=abort", "-C", "overflow-checks=off", "-o", str(binary), str(source)],
        capture_output=True, text=True, timeout=120,
    )
    if result.returncode != 0:
        raise RuntimeError(f"rustc failed:\n{result.stderr[-500:]}")


def _build_cpp(source: Path, binary: Path):
    cxx = _find_tool("clang++", fallback_dirs=[REPO_ROOT / "toolchain" / "llvm" / "bin"])
    result = subprocess.run(
        [cxx, "-std=c++20", "-O3", "-march=native", "-DNDEBUG", "-o", str(binary), str(source)],
        capture_output=True, text=True, timeout=120,
    )
    if result.returncode != 0:
        raise RuntimeError(f"clang++ failed:\n{result.stderr[-500:]}")


def _build_zig(source: Path, binary: Path):
    zig = _find_tool("zig")
    build_dir = binary.parent
    result = subprocess.run(
        [zig, "build-exe", "-O", "ReleaseFast", "--cache-dir", str(build_dir / "zig-cache"),
         "--global-cache-dir", str(build_dir / "zig-global-cache"),
         "-femit-bin=" + str(binary), str(source)],
        capture_output=True, text=True, timeout=120, cwd=str(source.parent),
    )
    if result.returncode != 0:
        raise RuntimeError(f"zig build-exe failed:\n{result.stderr[-500:]}")


def _build_go(source: Path, binary: Path):
    go = _find_tool("go")
    result = subprocess.run(
        [go, "build", "-ldflags=-s -w", "-o", str(binary), str(source)],
        capture_output=True, text=True, timeout=120, cwd=str(source.parent),
    )
    if result.returncode != 0:
        raise RuntimeError(f"go build failed:\n{result.stderr[-500:]}")


# ============================================================================
#  RUN
# ============================================================================

def cmd_run(args) -> int:
    """Run benchmarks."""
    selected = _select_benchmarks(args.case, args.tier)
    languages = _parse_languages(args.language)

    if not selected:
        print("No benchmarks selected")
        return 1

    results: list[dict] = []
    for bench in selected:
        bench_id = bench["id"]
        for lang in languages:
            if lang not in bench["languages"]:
                continue
            cfg = LANGUAGE_CONFIG.get(lang)
            if not cfg:
                continue

            # Check binary exists
            if lang != "mks":
                binary = cfg["binary"]
                if not binary.exists():
                    print(f"[SKIP] {bench_id}/{lang}: binary not built ({binary})")
                    results.append({**bench, "language": lang, "status": "build_missing"})
                    continue

            print(f"[RUN] {bench_id}/{lang} ", end="", flush=True)

            try:
                samples = []
                # Warmup runs
                for _ in range(args.warmups):
                    _run_bench(lang, bench_id, cfg, args.timeout)

                # Timed runs
                for _ in range(args.runs):
                    elapsed = _run_bench(lang, bench_id, cfg, args.timeout)
                    if elapsed is not None:
                        samples.append(elapsed)

                if samples:
                    median = statistics.median(samples)
                    print(f"median={median*1000:.1f}ms (n={len(samples)})")
                    results.append({
                        **bench, "language": lang, "status": "ok",
                        "median_ms": median * 1000,
                        "min_ms": min(samples) * 1000,
                        "max_ms": max(samples) * 1000,
                        "samples": [s * 1000 for s in samples],
                    })
                else:
                    print("FAILED")
                    results.append({**bench, "language": lang, "status": "run_failed"})
            except Exception as e:
                print(f"ERROR: {e}")
                results.append({**bench, "language": lang, "status": "error", "error": str(e)})

    # Write report
    _write_report(results, args)
    return 0


def _run_bench(lang: str, bench_id: str, cfg: dict, timeout: int) -> float | None:
    """Run a single benchmark. Returns elapsed seconds or None on failure."""
    if lang == "mks":
        binary = _find_mks()
        cmd = [str(binary), "run", str(cfg["source"]), "--section", bench_id]
    else:
        cmd = [str(cfg["binary"]), bench_id]

    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    elapsed = time.perf_counter() - start

    if result.returncode != 0:
        return None
    return elapsed


# ============================================================================
#  REPORTING
# ============================================================================

def _write_report(results: list[dict], args):
    """Write results as JSON + Markdown."""
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)

    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())

    report = {
        "generated_at": stamp,
        "warmups": args.warmups,
        "runs": args.runs,
        "results": results,
    }

    # JSON
    json_path = REPORT_ROOT / f"{stamp}.json"
    json_path.write_text(json.dumps(report, indent=2))

    # Latest JSON
    latest_json = REPORT_ROOT / "latest.json"
    latest_json.write_text(json.dumps(report, indent=2))

    # Markdown summary
    md = _render_markdown(report)
    md_path = REPORT_ROOT / f"{stamp}.md"
    md_path.write_text(md)

    latest_md = REPORT_ROOT / "latest.md"
    latest_md.write_text(md)

    print(f"\n[REPORT] {json_path}")
    print(f"[REPORT] {md_path}")


def _render_markdown(report: dict) -> str:
    lines = [
        "# CASES_V3 Benchmark Report",
        "",
        f"- Generated: {report['generated_at']}",
        f"- Warmups: {report['warmups']}",
        f"- Runs: {report['runs']}",
        "",
        "| Benchmark | Tier | Language | Status | Median (ms) | Min (ms) | Max (ms) |",
        "|-----------|------|----------|--------|-------------|----------|----------|",
    ]
    for r in report["results"]:
        status = r["status"]
        median = f"{r['median_ms']:.2f}" if "median_ms" in r else "n/a"
        min_ms = f"{r['min_ms']:.2f}" if "min_ms" in r else "n/a"
        max_ms = f"{r['max_ms']:.2f}" if "max_ms" in r else "n/a"
        lines.append(f"| {r['id']} | {r['tier']} | {r['language']} | {status} | {median} | {min_ms} | {max_ms} |")

    return "\n".join(lines)


# ============================================================================
#  UTILITY COMMANDS
# ============================================================================

def cmd_list() -> int:
    """List all benchmarks."""
    for b in BENCHMARKS:
        langs = ",".join(b["languages"])
        print(f"  T{b['tier']}  {b['id']:<28} [{langs}]")
    print(f"\n{BENCHMARKS_BY_ID.__len__()} benchmarks total")
    return 0


def cmd_expected(args) -> int:
    """Print expected checksums by running --compute-all on a language binary."""
    lang = args.language
    cfg = LANGUAGE_CONFIG.get(lang)
    if not cfg or not cfg["binary"] or not cfg["binary"].exists():
        print(f"No binary for {lang}")
        return 1

    result = subprocess.run([str(cfg["binary"]), "--compute-all"], capture_output=True, text=True, timeout=60)
    print(result.stdout)
    return result.returncode


def cmd_suite(args) -> int:
    """Run a named suite."""
    suites_path = Path(__file__).parent / "suites.json"
    if not suites_path.exists():
        print("suites.json not found")
        return 1

    suites = json.loads(suites_path.read_text())
    suite = suites.get("suites", {}).get(args.name)
    if not suite:
        print(f"Unknown suite: {args.name}")
        return 1

    # Build arg namespace from suite config
    ns = argparse.Namespace(
        case=suite.get("case"),
        tier=suite.get("tier"),
        language=suite.get("language"),
        warmups=suite.get("warmups", 1),
        runs=suite.get("runs", 3),
        timeout=suite.get("timeout", 300),
    )
    return cmd_run(ns)


# ============================================================================
#  HELPERS
# ============================================================================

BENCHMARKS_BY_ID = {b["id"]: b for b in BENCHMARKS}


def _select_benchmarks(case_str: str | None, tier: int | None) -> list[dict]:
    """Select benchmarks by case ID or tier."""
    if case_str:
        ids = [c.strip() for c in case_str.split(",")]
        return [BENCHMARKS_BY_ID[c] for c in ids if c in BENCHMARKS_BY_ID]
    elif tier:
        return [b for b in BENCHMARKS if b["tier"] == tier]
    return list(BENCHMARKS)


def _parse_languages(raw: str | None) -> list[str]:
    """Parse comma-separated language list."""
    if not raw:
        return LANGUAGE_ORDER
    return [l.strip().lower() for l in raw.split(",")]


def _find_tool(name: str, fallback_dirs: list[Path] | None = None) -> str:
    """Find a tool in PATH or fallback directories."""
    # Check explicit path
    env_var = f"KAIN_V3_{name.upper()}"
    if env_var in os.environ:
        return os.environ[env_var]

    # Check fallback dirs
    if fallback_dirs:
        for d in fallback_dirs:
            exe = d / f"{name}.exe"
            if exe.exists():
                return str(exe)

    # Assume in PATH
    return name


def _find_kain() -> Path:
    """Find the kain compiler binary."""
    # Check env
    if "KAIN_EXE" in os.environ:
        return Path(os.environ["KAIN_EXE"])

    # Check .kain/bin
    kain_home = Path(os.environ.get("KAIN_HOME", Path.home() / ".kain"))
    kain_bin = kain_home / "bin" / "kain.exe"
    if kain_bin.exists():
        return kain_bin

    # Check bazel
    bazel_out = REPO_ROOT / ".." / ".." / "_b"
    for root, _, files in os.walk(str(bazel_out)):
        if "kain.exe" in files and "cli" in root:
            return Path(root) / "kain.exe"

    return Path("kain")  # fallback to PATH


def _find_mks() -> Path:
    """Find the mks binary."""
    mks_path = REPO_ROOT / "blades" / "markscript" / "mks.exe"
    if mks_path.exists():
        return mks_path
    return Path("mks")


if __name__ == "__main__":
    sys.exit(main())
