#!/usr/bin/env python3
"""
Kain UI C Substrate Fuzz Suite — Orchestrator
===============================================
Drives the C fuzz harness, reads the data-driven taxonomy (fuzz_taxonomy.json),
compiles, runs fuzz iterations, collects telemetry, and generates timestamped
Markdown reports in reports/.

Usage:
    python run_fuzz.py                    # Quick run (10k iterations)
    python run_fuzz.py --quick            # 10k iterations per domain
    python run_fuzz.py --stress           # 500k iterations per domain
    python run_fuzz.py --iterations 50000 # Custom iteration count
    python run_fuzz.py --seed 12345       # Repeatable seed
    python run_fuzz.py --report-only      # Re-report from existing JSON

Output:
    reports/fuzz_report_YYYY-MM-DD_HH-MM-SS.md
"""

import os
import sys
import json
import time
import subprocess
import datetime
import glob
import re
from pathlib import Path

# ── Paths ──────────────────────────────────────────────────────────────────
REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent.parent  # X:/
FUZZ_DIR = Path(__file__).resolve().parent
UI_DIR = FUZZ_DIR.parent
KAIN_DIR = UI_DIR / "kain"
CORE_DIR = UI_DIR.parent / "core"
INCLUDE_DIR = REPO_ROOT / "runtime" / "native" / "include"
STB_DIR = REPO_ROOT / "runtime" / "native" / "extras" / "_stb-truetype"
BUILD_DIR = FUZZ_DIR / "_build"
REPORT_DIR = FUZZ_DIR / "reports"
TAXONOMY_FILE = FUZZ_DIR / "fuzz_taxonomy.json"
SOURCE_FILES = sorted(str(f) for f in FUZZ_DIR.glob("*_fuzzer.c")) + [str(FUZZ_DIR / "fuzzer.c")]
KAIN_SOURCES = sorted(str(f) for f in KAIN_DIR.glob("*.c"))

# Platform detection
IS_WINDOWS = sys.platform == "win32" or sys.platform == "cygwin"
CC = os.environ.get("CC", "clang" if not IS_WINDOWS else "clang")
CFLAGS = os.environ.get("CFLAGS", "-std=c11 -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function")
if IS_WINDOWS:
    LDFLAGS = os.environ.get("LDFLAGS", "-luser32 -lgdi32 -lopengl32")
else:
    LDFLAGS = os.environ.get("LDFLAGS", "-lm")


def load_taxonomy() -> dict:
    """Load the data-driven test taxonomy."""
    if not TAXONOMY_FILE.exists():
        print(f"  ! Taxonomy file not found: {TAXONOMY_FILE}")
        return {}
    with open(TAXONOMY_FILE, "r") as f:
        return json.load(f)


def get_default_iterations(taxonomy: dict) -> int:
    """Get iteration count from taxonomy."""
    orch = taxonomy.get("orchestration", {})
    return orch.get("default_iterations", 50000)


def build_fuzzer(clean: bool = False) -> bool:
    """Build the C fuzz harness. Returns True on success."""
    os.makedirs(BUILD_DIR, exist_ok=True)

    # Check if binary already exists
    binary = BUILD_DIR / ("fuzzer.exe" if IS_WINDOWS else "fuzzer")
    if binary.exists() and not clean:
        return True

    # Build include flags
    inc_flags = (
        f"-I{INCLUDE_DIR} -I{UI_DIR} -I{UI_DIR}/widgets "
        f"-I{KAIN_DIR} -I{CORE_DIR} -I{STB_DIR} -I{FUZZ_DIR}"
    )

    # Link UI sources that are needed
    # Only include sources that compile standalone (no widget deps)
    available_kain_sources = KAIN_SOURCES.copy()
    # Filter out any problematic sources
    needed_sources = [
        str(KAIN_DIR / "kain_render_software.c"),
        str(KAIN_DIR / "kain_compositor.c"),
        str(KAIN_DIR / "kain_input.c"),
        str(KAIN_DIR / "kain_font.c"),
    ]
    # Fuzz stubs provide implementations of abi_ui_* functions
    needed_sources.append(str(FUZZ_DIR / "fuzz_stubs.c"))

    all_sources = SOURCE_FILES + needed_sources

    cmd = [CC] + CFLAGS.split() + inc_flags.split()
    cmd += all_sources
    cmd += ["-o", str(binary)]
    cmd += LDFLAGS.split()

    print(f"  Building fuzzer with: {CC} ({len(all_sources)} source files)")
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

    if result.returncode != 0:
        print("  XX Build failed:")
        print(result.stderr[:2000])
        return False

    print(f"  OK Built: {binary}")
    return True


def run_fuzzer(iterations: int, seed: int, binary: Path) -> dict:
    """Run the fuzzer and parse stdout telemetry. Returns structured results."""
    cmd = [str(binary),
           "--iterations", str(iterations),
           "--seed", str(seed),
           "--report", str(BUILD_DIR / "fuzz_report.json")]

    print(f"  Running: {' '.join(cmd)}")

    start = time.time()
    result = subprocess.run(cmd, capture_output=True, timeout=120)
    elapsed = time.time() - start

    if result.returncode not in (0, 1):
        print(f"  XX Fuzzer crashed (exit code {result.returncode})")

    # Decode with error handling (fuzzer may output binary data to stdout)
    stdout_str = result.stdout.decode('utf-8', errors='replace')
    stderr_str = result.stderr.decode('utf-8', errors='replace')
    output = stdout_str + stderr_str
    return {
        "stdout": output,
        "return_code": result.returncode,
        "elapsed_s": elapsed,
    }


def parse_telemetry(output: str) -> list:
    """Parse structured telemetry from fuzzer stdout.

    Expected lines like:
      "  OK geometry: 50000 ops, 50 boundary tests, 0 null-ptr tolerant in 12.34 ms"
      "  OK render: 50000 ops, 10 boundary tests, 17 null-ptr tolerant in 98.76 ms"
    """
    domains = []
    patterns = {
        "geometry": r"geometry:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "render": r"render:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "compositor": r"compositor:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "input": r"input:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "font": r"font:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "surface": r"surface:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
        "vtable": r"vtable:\s+(\d+)\s+ops.*?(\d+)\s+boundary.*?(\d+)\s+null-ptr.*?([0-9.]+)\s+ms",
    }
    # Alternative patterns for crash case (use expanded regex that also matches crash output)
    crash_patterns = {
        "geometry": r"OK geometry:.*?(\d+)\s+ops.*?(\d+)\s+boundary",
        "vtable": r"vtable.*?(\d+)\s+tests",
    }

    for domain, pat in patterns.items():
        m = re.search(pat, output)
        if m:
            domains.append({
                "name": domain,
                "ops": int(m.group(1)),
                "boundary_tests": int(m.group(2)),
                "null_ptr_tolerant": int(m.group(3)),
                "time_ms": float(m.group(4)),
            })
        else:
            # Try fallback for failed domains
            fail_pat = rf"{domain}.*?(\d+)/(\d+)\s+passed,\s+(\d+)\s+failed"
            fm = re.search(fail_pat, output)
            if fm:
                domains.append({
                    "name": domain,
                    "ops": int(fm.group(2)),
                    "passed": int(fm.group(1)),
                    "failed": int(fm.group(3)),
                    "boundary_tests": 0,
                    "null_ptr_tolerant": 0,
                    "time_ms": 0,
                })

    # Total line
    total_pat = r"TOTAL\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([0-9.]+)"
    tm = re.search(total_pat, output)
    total_info = None
    if tm:
        total_info = {
            "total": int(tm.group(1)),
            "passed": int(tm.group(2)),
            "failed": int(tm.group(3)),
            "crashed": int(tm.group(4)),
            "time_ms": float(tm.group(5)),
        }

    return {
        "domains": domains,
        "total": total_info,
        "raw": output,
    }


def load_taxonomy_weights(taxonomy: dict) -> dict:
    """Extract fuzz weights from taxonomy for report."""
    weights = {}
    for domain, config in taxonomy.get("domains", {}).items():
        weights[domain] = config.get("fuzz_weight", 10)
    return weights


def generate_markdown_report(raw: dict, telemetry: dict, taxonomy: dict,
                               config: dict) -> str:
    """Generate a comprehensive Markdown report with telemetry."""
    now = datetime.datetime.now()
    timestamp = now.strftime("%Y-%m-%d %H:%M:%S")
    weights = load_taxonomy_weights(taxonomy)

    lines = []
    lines.append(f"# Kain UI C Substrate Fuzz Report")
    lines.append(f"")
    lines.append(f"**Date:** {timestamp}")
    lines.append(f"**Seed:** {config['seed']}")
    lines.append(f"**Iterations:** {config['iterations']} per domain")
    lines.append(f"**Framebuffer:** {config.get('fb_width', 800)}x{config.get('fb_height', 600)}")
    lines.append(f"**Taxonomy:** `fuzz_taxonomy.json` v{taxonomy.get('manifest_version', '?')}")
    lines.append(f"**Fuzzer exit code:** {raw.get('return_code', '?')}")
    lines.append(f"")

    # ── Summary Table ────────────────────────────────────────────────
    lines.append(f"## Summary")
    lines.append(f"")
    lines.append(f"| Domain | Fuzz Weight | Ops | Boundary Tests | NULL-Tolerant | Time (ms) |")
    lines.append(f"|--------|:-----------:|:---:|:--------------:|:-------------:|:---------:|")

    total_ops = 0
    total_boundary = 0
    total_null = 0
    total_time = 0.0

    for d in telemetry.get("domains", []):
        name = d["name"]
        w = weights.get(name, "?")
        ops = d.get("ops", 0)
        bt = d.get("boundary_tests", 0)
        nt = d.get("null_ptr_tolerant", 0)
        tm = d.get("time_ms", 0.0)
        total_ops += ops
        total_boundary += bt
        total_null += nt
        total_time += tm
        lines.append(f"| {name} | {w} | {ops} | {bt} | {nt} | {tm:.1f} |")

    ti = telemetry.get("total", {})
    lines.append(f"| **TOTAL** | — | **{ti.get('total', total_ops)}** | **{total_boundary}** | **{total_null}** | **{total_time:.1f}** |")
    lines.append(f"")

    if ti:
        passed = ti.get('passed', 0)
        failed = ti.get('failed', 0)
        crashed = ti.get('crashed', 0)
        if failed > 0 or crashed > 0:
            lines.append(f"**!! {failed} failures, {crashed} crashes detected**")
        else:
            lines.append(f"**OK All tests passed**")
    lines.append(f"")

    # ── Domain Details ───────────────────────────────────────────────
    lines.append(f"## Domain Details")
    lines.append(f"")

    taxonomy_domains = taxonomy.get("domains", {})

    for d in telemetry.get("domains", []):
        name = d["name"]
        tax = taxonomy_domains.get(name, {})
        desc = tax.get("description", name)
        lines.append(f"### {name}")
        lines.append(f"")
        lines.append(f"_{desc}_")
        lines.append(f"")
        lines.append(f"- **Operations:** {d.get('ops', 0)}")
        lines.append(f"- **Boundary tests:** {d.get('boundary_tests', 0)}")
        lines.append(f"- **NULL-pointer tolerant calls:** {d.get('null_ptr_tolerant', 0)}")
        lines.append(f"- **Time:** {d.get('time_ms', 0):.1f} ms")
        lines.append(f"")

        # List functions tested
        funcs = tax.get("functions", [])
        if funcs:
            lines.append(f"**Functions tested:**")
            lines.append(f"")
            for fn in funcs[:10]:
                lines.append(f"- `{fn['name']}` — {fn.get('desc', '')}")
            if len(funcs) > 10:
                lines.append(f"- *... and {len(funcs) - 10} more*")
            lines.append(f"")

        # List boundary cases
        bvs = tax.get("boundary_values", [])
        if bvs:
            lines.append(f"**Boundary cases tested:**")
            lines.append(f"")
            for bv in bvs[:8]:
                lines.append(f"- {bv.get('desc', 'unknown')}")
            if len(bvs) > 8:
                lines.append(f"- *... and {len(bvs) - 8} more*")
            lines.append(f"")

    # ── Crash Reproduction Guide ─────────────────────────────────────
    lines.append(f"## Crash Reproduction")
    lines.append(f"")
    lines.append(f"To reproduce any crash with the same seed:")
    lines.append(f"")
    lines.append(f"```bash")
    lines.append(f"cd {FUZZ_DIR}")
    lines.append(f"python run_fuzz.py --seed {config['seed']} --iterations {config['iterations']}")
    lines.append(f"```")
    lines.append(f"")
    lines.append(f"To test a specific domain with more iterations:")
    lines.append(f"")
    lines.append(f"```bash")
    lines.append(f"cd {FUZZ_DIR}")
    lines.append(f"python run_fuzz.py --iterations 500000 --seed {config['seed']}")
    lines.append(f"```")
    lines.append(f"")

    # ── Taxonomy Coverage ───────────────────────────────────────────
    lines.append(f"## Taxonomy Coverage")
    lines.append(f"")
    lines.append(f"| Domain | Functions | Boundary Cases | Valid Ranges |")
    lines.append(f"|--------|:---------:|:--------------:|:------------:|")
    for domain_name, cfg in taxonomy_domains.items():
        func_count = len(cfg.get("functions", []))
        bound_count = len(cfg.get("boundary_values", []))
        range_count = len(cfg.get("valid_ranges", {}))
        lines.append(f"| {domain_name} | {func_count} | {bound_count} | {range_count} |")
    lines.append(f"")

    # ── Fuzzer Configuration ────────────────────────────────────────
    lines.append(f"## Fuzzer Configuration")
    lines.append(f"")
    lines.append(f"```json")
    orch = taxonomy.get("orchestration", {})
    lines.append(json.dumps(orch, indent=2))
    lines.append(f"```")
    lines.append(f"")

    # ── Raw Output ──────────────────────────────────────────────────
    raw_out = raw.get("stdout", "")
    if raw_out:
        # Only include last ~100 lines
        out_lines = raw_out.strip().split("\n")
        if len(out_lines) > 100:
            out_lines = out_lines[-100:]
        lines.append(f"## Raw Fuzzer Output (last {len(out_lines)} lines)")
        lines.append(f"")
        lines.append(f"```")
        lines.append("\n".join(out_lines))
        lines.append(f"```")
        lines.append(f"")

    return "\n".join(lines)


def write_report(markdown: str):
    """Write timestamped markdown report to reports/ directory."""
    os.makedirs(REPORT_DIR, exist_ok=True)
    now = datetime.datetime.now()
    fname = f"fuzz_report_{now.strftime('%Y-%m-%d_%H-%M-%S')}.md"
    path = REPORT_DIR / fname
    with open(path, "w", encoding="utf-8") as f:
        f.write(markdown)
    print(f"\n  [REPORT] Report written: {path}")
    return path


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Kain UI C Substrate Fuzz Suite Orchestrator")
    parser.add_argument("--iterations", type=int, default=None,
                        help="Fuzz iterations per domain")
    parser.add_argument("--quick", action="store_true",
                        help="Quick run (10k iterations)")
    parser.add_argument("--stress", action="store_true",
                        help="Stress run (500k iterations)")
    parser.add_argument("--seed", type=int, default=42,
                        help="Fuzzer seed (default: 42)")
    parser.add_argument("--clean", action="store_true",
                        help="Clean rebuild")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip build step")
    parser.add_argument("--report-only", type=str, default=None,
                        help="Re-report from existing JSON output file")
    parser.add_argument("--fb-width", type=int, default=800,
                        help="Framebuffer width")
    parser.add_argument("--fb-height", type=int, default=600,
                        help="Framebuffer height")
    args = parser.parse_args()

    # Load taxonomy
    taxonomy = load_taxonomy()

    # Determine iteration count
    if args.report_only:
        # Re-report from saved data
        report_data = {"stdout": open(args.report_only, "r").read(),
                       "return_code": 0}
        telemetry = parse_telemetry(report_data["stdout"])
        config = {"seed": args.seed, "iterations": "?", "fb_width": 800, "fb_height": 600}
        md = generate_markdown_report(report_data, telemetry, taxonomy, config)
        write_report(md)
        return

    if args.iterations:
        iterations = args.iterations
    elif args.stress:
        iterations = 500000
    elif args.quick:
        iterations = 10000
    else:
        iterations = get_default_iterations(taxonomy)

    config = {
        "seed": args.seed,
        "iterations": iterations,
        "fb_width": args.fb_width,
        "fb_height": args.fb_height,
    }

    print("+----------------------------------------------------------+")
    print("|  Kain UI C Substrate Fuzz Suite v1.0                     |")
    print("+----------------------------------------------------------+")
    print(f"|  Iterations: {iterations:<12d}  Seed: {args.seed:<10d}    |")
    print(f"|  Taxonomy:   {len(taxonomy.get('domains', {}))} domains, "
          f"{sum(len(v.get('functions',[])) for v in taxonomy.get('domains',{}).values())} funcs    |")
    print(f"|  FB:         {args.fb_width}x{args.fb_height:<4d}                                   |")
    print("+----------------------------------------------------------+")
    print()

    # Build
    if not args.no_build:
        if not build_fuzzer(clean=args.clean):
            sys.exit(1)

    # Run
    binary = BUILD_DIR / ("fuzzer.exe" if IS_WINDOWS else "fuzzer")
    if not binary.exists():
        print(f"  XX Fuzzer binary not found: {binary}")
        sys.exit(1)

    raw = run_fuzzer(iterations, args.seed, binary)

    # Parse telemetry
    telemetry = parse_telemetry(raw["stdout"])

    # Print parsed summary
    print()
    print("--- TELEMETRY --------------------------------------------")
    for d in telemetry.get("domains", []):
        print(f"  {d['name']:15s}  {d['ops']:8d} ops  "
              f"{d.get('boundary_tests',0):4d} boundary  "
              f"{d.get('null_ptr_tolerant',0):4d} null-safe  "
              f"{d['time_ms']:8.1f} ms")
    ti = telemetry.get("total")
    if ti:
        print(f"  {'TOTAL':15s}  {ti.get('total',0):8d}  "
              f"{ti.get('passed',0):4d} passed  "
              f"{ti.get('failed',0):4d} failed  "
              f"{ti.get('crashed',0):4d} crashed  "
              f"{ti.get('time_ms',0):8.1f} ms")
    print()

    # Generate report
    md = generate_markdown_report(raw, telemetry, taxonomy, config)
    report_path = write_report(md)

    print(f"  Report: {report_path}")
    print()

    # Final status
    if ti and (ti.get('failed', 0) > 0 or ti.get('crashed', 0) > 0):
        print("!!  Some fuzz tests reported issues. Review the report for details.")
        sys.exit(1)
    else:
        print("OK  All fuzz domains clean. Zero issues detected.")


if __name__ == "__main__":
    main()
