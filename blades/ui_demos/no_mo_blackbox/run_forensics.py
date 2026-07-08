#!/usr/bin/env python3
"""
no-mo-blackbox MASTER FORENSICS RUNNER
=======================================
Takes a .kn file or .exe as input, runs ALL analyzers, and generates
a unified FORENSICS_REPORT_<timestamp>.md with verdict, evidence, and fix guidance.

PIPELINE:
  1. Build (if .kn) → .exe
  2. Vtable Call Tracer → vtable_trace_*.json
  3. Crash Forensics → crash_report_*.json
  4. Hang Detector → hang_report_*.json
  5. Blank Window Analyzer → blank_analysis_*.json
  6. Unified Report → FORENSICS_REPORT_<timestamp>.md + .json

USAGE:
  python run_forensics.py <component.kn>
  python run_forensics.py <component.exe>
  python run_forensics.py --suite smoke         # run taxonomy suite
  python run_forensics.py --suite full
  python run_forensics.py --all                 # scan for all .kn files

IMPORTABLE FROM KAIN:
  import no_mo_blackbox.run_forensics as forensics
  report = forensics.run_full_pipeline("component.exe")
"""

import sys, os, json, time, subprocess, argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Any, Tuple

# Import our tool modules — try relative first (package import), then absolute (standalone)
try:
    from .vtable_tracer import trace_exe, write_trace_output, VtableTrace
    from .crash_forensics import analyze_exe, write_crash_report, CrashReport
    from .hang_detector import detect_hang, write_hang_report, HangReport
    from .blank_analyzer import analyze_blank, write_blank_report, BlankReport
except ImportError:
    _THIS_DIR = Path(__file__).parent
    sys.path.insert(0, str(_THIS_DIR))
    from vtable_tracer import trace_exe, write_trace_output, VtableTrace
    from crash_forensics import analyze_exe, write_crash_report, CrashReport
    from hang_detector import detect_hang, write_hang_report, HangReport
    from blank_analyzer import analyze_blank, write_blank_report, BlankReport

# Try to load taxonomy
TAXONOMY_PATH = _THIS_DIR / "taxonomy.toml"
TAXONOMY: Dict[str, Any] = {}

try:
    import tomllib
    with open(TAXONOMY_PATH, 'rb') as f:
        TAXONOMY = tomllib.load(f)
except ImportError:
    try:
        import tomli
        with open(TAXONOMY_PATH, 'rb') as f:
            TAXONOMY = tomli.load(f)
    except ImportError:
        pass  # TOML parser not available, use defaults

OUTPUT_DIR = Path(os.environ.get("NO_MO_BLACKBOX_OUTPUT", _THIS_DIR / "forensics_output"))
KAIN_BINARY = os.environ.get("KAIN", "kain")
DEFAULT_TIMEOUT = 30.0
DEFAULT_WAIT_MS = 3000


@dataclass
class UnifiedReport:
    """Combined forensics report from all analyzers."""
    target: str
    exe_path: str
    timestamp: str
    verdict: str
    summary: str

    # Per-analyzer results
    build_ok: bool = False
    build_error: str = ""

    vtable: Optional[Dict[str, Any]] = None
    crash: Optional[Dict[str, Any]] = None
    hang: Optional[Dict[str, Any]] = None
    blank: Optional[Dict[str, Any]] = None

    # Evidence
    all_errors: List[str] = field(default_factory=list)
    all_recommendations: List[str] = field(default_factory=list)
    evidence_files: List[str] = field(default_factory=list)
    vtable_slots_hit: int = 0
    vtable_slots_missing: List[int] = field(default_factory=list)
    dominant_color: str = ""
    blocked_function: str = ""
    crash_location: str = ""

    # Timing
    elapsed_s: float = 0.0
    pipeline_stages: Dict[str, float] = field(default_factory=dict)


# ============================================================================
# BUILD
# ============================================================================

def build_kn(kn_path: str) -> Tuple[Optional[str], str]:
    """Build a .kn file to LLVM .exe. Returns (exe_path, error)."""
    kn_abs = os.path.abspath(kn_path)
    if not os.path.exists(kn_abs):
        return None, f"File not found: {kn_abs}"

    print(f"  [build] {kn_abs}")
    try:
        r = subprocess.run(
            [KAIN_BINARY, "build", kn_abs, "--target", "llvm"],
            capture_output=True, text=True, timeout=120,
            cwd=os.path.dirname(kn_abs) or ".",
        )
    except FileNotFoundError:
        return None, f"kain binary not found at '{KAIN_BINARY}'. Set KAIN env or install kain."

    output = r.stdout + r.stderr

    # Find exe path from output
    for line in output.split('\n'):
        if '.exe' in line:
            p = line.strip()
            if p.startswith("\\\\?\\"):
                p = p[4:]
            if os.path.exists(p):
                return os.path.abspath(p), ""

    # Fallback: search common locations
    base = os.path.splitext(os.path.basename(kn_path))[0]
    search_dirs = [
        Path(kn_abs).parent / ".kain" / "out" / "x86_64-windows" / "dev" / "ll" / base / "compile",
        Path("X:/.kain/out/x86_64-windows/dev/ll") / base / "compile",
    ]
    for d in search_dirs:
        exe = d / f"{base}.exe"
        if exe.exists():
            return str(exe.absolute()), ""

    if r.returncode != 0:
        return None, f"Build failed (exit {r.returncode}):\n{output[:500]}"

    return None, f"Could not locate .exe after build. Output:\n{output[:500]}"


# ============================================================================
# FULL PIPELINE
# ============================================================================

def run_full_pipeline(
    target: str,
    timeout_s: float = DEFAULT_TIMEOUT,
    wait_ms: int = DEFAULT_WAIT_MS,
    output_dir: Path = None,
    skip_build: bool = False,
    skip_trace: bool = False,
    skip_crash: bool = False,
    skip_hang: bool = False,
    skip_blank: bool = False,
) -> UnifiedReport:
    """
    Run ALL forensics analyzers on a target .kn or .exe.

    Args:
        target: Path to .kn file or .exe file
        timeout_s: Timeout for hang detection and crash analysis
        wait_ms: Wait time for blank window capture
        output_dir: Output directory for all reports
        skip_build: Use target directly as .exe (don't build)
        skip_*: Skip specific analyzers

    Returns:
        UnifiedReport with combined results
    """
    if output_dir is None:
        output_dir = OUTPUT_DIR

    report = UnifiedReport(
        target=target,
        exe_path="",
        timestamp=time.strftime("%Y-%m-%dT%H:%M:%S"),
        verdict="PENDING",
        summary="",
    )

    output_dir.mkdir(parents=True, exist_ok=True)
    t0 = time.time()

    # ── Stage 0: Build ──────────────────────────────────────────
    t_stage = time.time()
    if skip_build or target.endswith('.exe'):
        report.exe_path = os.path.abspath(target)
        report.build_ok = os.path.exists(report.exe_path)
        if not report.build_ok:
            report.build_error = f"Executable not found: {target}"
    else:
        exe, err = build_kn(target)
        if exe:
            report.exe_path = exe
            report.build_ok = True
        else:
            report.exe_path = ""
            report.build_ok = False
            report.build_error = err

    report.pipeline_stages['build'] = round(time.time() - t_stage, 2)

    if not report.build_ok:
        report.verdict = "BUILD_FAILED"
        report.summary = f"Build failed for {target}"
        report.all_errors.append(report.build_error)
        report.elapsed_s = round(time.time() - t0, 2)
        return report

    # ── Stage 1: Vtable Trace ───────────────────────────────────
    vtable_trace_data = {}
    if not skip_trace:
        t_stage = time.time()
        print(f"\n{'='*60}")
        print(f"STAGE 1/4: VTABLE CALL TRACER")
        print(f"{'='*60}")

        trace = trace_exe(report.exe_path, timeout_s=min(timeout_s, 30))
        json_path, md_path = write_trace_output(trace, output_dir)

        report.vtable = {
            'verdict': trace.verdict,
            'total_calls': trace.total_calls,
            'total_frames': trace.total_frames,
            'slot_counts': trace.slot_counts,
            'json_path': str(json_path),
            'md_path': str(md_path),
        }
        report.vtable_slots_hit = len(trace.slot_counts)
        report.vtable_slots_missing = [s for s in range(24) if s not in trace.slot_counts]
        report.evidence_files.append(str(json_path))
        report.evidence_files.append(str(md_path))

        if trace.errors:
            for err in trace.errors:
                report.all_errors.append(f"[vtable] {err}")

        vtable_trace_data = {
            'calls': [asdict(c) if hasattr(c, '__dataclass_fields__') else c for c in trace.calls],
            'slot_counts': trace.slot_counts,
        }

        report.pipeline_stages['vtable_trace'] = round(time.time() - t_stage, 2)

    # ── Stage 2: Crash Forensics ────────────────────────────────
    if not skip_crash:
        t_stage = time.time()
        print(f"\n{'='*60}")
        print(f"STAGE 2/4: CRASH FORENSICS")
        print(f"{'='*60}")

        crash_report = analyze_exe(report.exe_path, timeout_s=min(timeout_s, 10))
        json_path, md_path = write_crash_report(crash_report, output_dir)

        report.crash = {
            'exception_name': crash_report.crash.exception_name,
            'exception_code': hex(crash_report.crash.exception_code),
            'fault_address': hex(crash_report.crash.fault_address),
            'json_path': str(json_path),
            'md_path': str(md_path),
        }
        if crash_report.crash.crash_location:
            loc = crash_report.crash.crash_location
            report.crash_location = f"{loc.source_file}:{loc.source_line} ({loc.function_name})"
            report.crash['location'] = report.crash_location

        report.evidence_files.append(str(json_path))
        report.evidence_files.append(str(md_path))

        if crash_report.crash.exception_name != "NO_CRASH":
            report.all_errors.append(
                f"[crash] {crash_report.crash.exception_name}: "
                f"{crash_report.crash.exception_description}"
            )
        for rec in crash_report.recommendations:
            if rec not in report.all_recommendations:
                report.all_recommendations.append(rec)

        report.pipeline_stages['crash_forensics'] = round(time.time() - t_stage, 2)

    # ── Stage 3: Hang Detection ────────────────────────────────
    if not skip_hang:
        t_stage = time.time()
        print(f"\n{'='*60}")
        print(f"STAGE 3/4: HANG DETECTOR")
        print(f"{'='*60}")

        hang_report = detect_hang(report.exe_path, timeout_s=timeout_s)
        json_path, md_path = write_hang_report(hang_report, output_dir)

        report.hang = {
            'hung': hang_report.hung,
            'pattern': hang_report.pattern,
            'pattern_description': hang_report.pattern_description,
            'blocked_function': hang_report.blocked_function,
            'verdict': hang_report.verdict,
            'json_path': str(json_path),
            'md_path': str(md_path),
        }
        report.blocked_function = hang_report.blocked_function

        report.evidence_files.append(str(json_path))
        report.evidence_files.append(str(md_path))

        if hang_report.hung:
            report.all_errors.append(f"[hang] Process hung — pattern: {hang_report.pattern}")
        for rec in hang_report.recommendations:
            if rec not in report.all_recommendations:
                report.all_recommendations.append(rec)

        report.pipeline_stages['hang_detector'] = round(time.time() - t_stage, 2)

    # ── Stage 4: Blank Window Analysis ──────────────────────────
    if not skip_blank:
        t_stage = time.time()
        print(f"\n{'='*60}")
        print(f"STAGE 4/4: BLANK WINDOW ANALYZER")
        print(f"{'='*60}")

        blank_report = analyze_blank(report.exe_path, wait_ms=wait_ms, vtable_trace=vtable_trace_data)
        json_path, md_path = write_blank_report(blank_report, output_dir)

        report.blank = {
            'verdict': blank_report.verdict,
            'width': blank_report.width,
            'height': blank_report.height,
            'image_path': blank_report.image_path,
            'json_path': str(json_path),
            'md_path': str(md_path),
        }
        if blank_report.pixel_analysis:
            report.dominant_color = blank_report.pixel_analysis.dominant_color_hex
            report.blank['dominant_color'] = report.dominant_color
            report.blank['dominant_pct'] = blank_report.pixel_analysis.dominant_color_pct
            report.blank['unique_colors'] = blank_report.pixel_analysis.unique_colors

        report.evidence_files.append(str(json_path))
        report.evidence_files.append(str(md_path))

        if blank_report.verdict in ("BLANK", "PARTIAL"):
            report.all_errors.append(f"[blank] Window is {blank_report.verdict}")
        for rec in blank_report.recommendations:
            if rec not in report.all_recommendations:
                report.all_recommendations.append(rec)

        report.pipeline_stages['blank_analyzer'] = round(time.time() - t_stage, 2)

    # ── Final Verdict ──────────────────────────────────────────
    report.elapsed_s = round(time.time() - t0, 2)

    if not report.all_errors:
        report.verdict = "ALL_CLEAN"
        report.summary = "Component passed all forensics checks — render, trace, crash, hang all clean."
    else:
        report.verdict = "ISSUES_FOUND"
        report.summary = f"Found {len(report.all_errors)} issue(s) across forensics pipeline."

    return report


# ============================================================================
# SUITE RUNNER
# ============================================================================

def run_suite(suite_name: str, output_dir: Path = None) -> List[UnifiedReport]:
    """Run a named test suite from taxonomy.toml."""
    suites = TAXONOMY.get('suites', [])
    suite = None
    for s in suites:
        if s.get('name') == suite_name:
            suite = s
            break

    if not suite:
        print(f"Suite '{suite_name}' not found in taxonomy.toml. Available: {[s['name'] for s in suites]}")
        return []

    case_ids = suite.get('cases', [])
    test_cases = TAXONOMY.get('test_cases', [])

    print(f"\n{'='*70}")
    print(f"SUITE: {suite_name} — {suite.get('description', '')}")
    print(f"{len(case_ids)} test cases")
    print(f"{'='*70}")

    results = []
    for case_id in case_ids:
        # Find the test case
        case = None
        for tc in test_cases:
            if tc.get('id') == case_id:
                case = tc
                break
        if not case:
            print(f"  [skip] Case '{case_id}' not found in taxonomy")
            continue

        source = case.get('source', '')
        if not source or not os.path.exists(source):
            print(f"  [skip] Case '{case_id}' source not found: {source}")
            continue

        timeout = case.get('timeout_ms', DEFAULT_TIMEOUT * 1000) / 1000.0
        wait_ms = case.get('frame_wait_ms', DEFAULT_WAIT_MS)

        print(f"\n  [{case_id}] {case.get('description', '')}")
        print(f"  Source: {source}")

        result = run_full_pipeline(
            source,
            timeout_s=timeout,
            wait_ms=wait_ms,
            output_dir=output_dir,
        )
        results.append(result)
        print(f"  >>> {result.verdict}: {result.summary[:80]}")

    return results


def run_all_kn_files(root_dirs: List[str], output_dir: Path = None) -> List[UnifiedReport]:
    """Find all .kn files in root directories and run forensics on each."""
    results = []

    for rd in root_dirs:
        if not os.path.isdir(rd):
            continue
        for root, dirs, files in os.walk(rd):
            for f in files:
                if f.endswith('.kn'):
                    kn_path = os.path.join(root, f)
                    print(f"\n  [{f}]")
                    result = run_full_pipeline(kn_path, output_dir=output_dir)
                    results.append(result)

    return results


# ============================================================================
# UNIFIED REPORT GENERATION
# ============================================================================

def write_unified_report(report: UnifiedReport, output_dir: Path = None):
    """Write the grand unified forensics report."""
    if output_dir is None:
        output_dir = OUTPUT_DIR
    output_dir.mkdir(parents=True, exist_ok=True)

    ts = time.strftime("%Y%m%d_%H%M%S")
    base = Path(report.target).stem

    # ── Markdown ──
    md_path = output_dir / f"FORENSICS_REPORT_{base}_{ts}.md"
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(f"# 🔍 Kain Component Forensics Report\n\n")
        f.write(f"**Target:** `{report.target}`  \n")
        f.write(f"**Executable:** `{report.exe_path}`  \n")
        f.write(f"**Timestamp:** {report.timestamp}  \n")
        f.write(f"**Pipeline elapsed:** {report.elapsed_s:.1f}s  \n")
        f.write(f"**Build:** {'✅' if report.build_ok else '❌ Failed'}  \n\n")

        # Verdict banner
        verdict_emoji = {
            "ALL_CLEAN": "✅",
            "ISSUES_FOUND": "⚠️",
            "BUILD_FAILED": "❌",
            "PENDING": "⏳",
        }
        emoji = verdict_emoji.get(report.verdict, "❓")
        f.write(f"## Verdict: {emoji} **{report.verdict}**\n\n")
        f.write(f"{report.summary}\n\n")

        # Stage timings
        if report.pipeline_stages:
            f.write("### Pipeline Stages\n\n")
            f.write("| Stage | Time |\n|-------|------|\n")
            for stage, t in report.pipeline_stages.items():
                f.write(f"| {stage} | {t:.1f}s |\n")
            f.write(f"| **Total** | **{report.elapsed_s:.1f}s** |\n\n")

        # Vtable summary
        if report.vtable:
            f.write("### 🔌 Vtable Call Trace\n\n")
            f.write(f"- **Verdict:** {report.vtable.get('verdict', 'N/A')}\n")
            f.write(f"- **Total calls:** {report.vtable.get('total_calls', 0)}\n")
            f.write(f"- **Slots hit:** {report.vtable_slots_hit}/24\n")
            if report.vtable_slots_missing:
                slot_names = {
                    0: "session_create", 1: "session_destroy", 2: "element_begin",
                    3: "element_end", 4: "element_set_text", 5: "element_set_attr_i64",
                    6: "element_set_attr_f64", 7: "element_set_attr_string",
                    8: "state_get_i64", 9: "state_set_i64", 10: "begin_frame",
                    11: "end_frame", 12: "present", 13: "poll_event", 14: "should_close",
                    15: "window_open", 16: "host_pump", 17: "session_attach_platform",
                    18: "get_gpu_extension", 19: "state_get_f64", 20: "state_set_f64",
                    21: "state_get_string", 22: "state_set_string", 23: "element_set_callback",
                }
                f.write(f"- **Missing slots:** {len(report.vtable_slots_missing)}\n")
                for slot in report.vtable_slots_missing:
                    name = slot_names.get(slot, f"slot_{slot}")
                    f.write(f"  - Slot {slot}: `{name}` — never called\n")
            f.write("\n")

        # Crash summary
        if report.crash:
            f.write("### 💥 Crash Analysis\n\n")
            f.write(f"- **Result:** `{report.crash.get('exception_name', 'N/A')}`\n")
            if report.crash_location:
                f.write(f"- **Location:** {report.crash_location}\n")
            f.write("\n")

        # Hang summary
        if report.hang:
            f.write("### ⏸️ Hang Detection\n\n")
            f.write(f"- **Hung:** {'YES' if report.hang.get('hung') else 'NO'}\n")
            f.write(f"- **Pattern:** `{report.hang.get('pattern', 'none')}`\n")
            if report.blocked_function:
                f.write(f"- **Blocked at:** `{report.blocked_function}`\n")
            f.write("\n")

        # Blank analysis summary
        if report.blank:
            f.write("### 🖼️ Visual Analysis\n\n")
            f.write(f"- **Verdict:** **{report.blank.get('verdict', 'N/A')}**\n")
            f.write(f"- **Resolution:** {report.blank.get('width', 0)}x{report.blank.get('height', 0)}\n")
            if report.dominant_color:
                f.write(f"- **Dominant color:** `{report.dominant_color}`\n")
            if report.blank.get('image_path'):
                f.write(f"- **Screenshot:** `{report.blank['image_path']}`\n")
            f.write("\n")

        # All errors
        if report.all_errors:
            f.write("### 🚨 All Errors\n\n")
            for err in report.all_errors:
                f.write(f"- {err}\n")
            f.write("\n")

        # All recommendations
        if report.all_recommendations:
            f.write("### 💡 Recommendations\n\n")
            for rec in report.all_recommendations:
                f.write(f"- {rec}\n")
            f.write("\n")

        # Evidence files
        if report.evidence_files:
            f.write("### 📁 Evidence Files\n\n")
            for ev in report.evidence_files:
                f.write(f"- `{ev}`\n")
            f.write("\n")

        f.write(f"---\n*Generated by no-mo-blackbox — Kain UI Component Forensics Kit*\n")

    print(f"\n  [unified] MD → {md_path}")

    # ── JSON ──
    json_path = output_dir / f"FORENSICS_REPORT_{base}_{ts}.json"
    report_dict = {
        'target': report.target,
        'exe_path': report.exe_path,
        'timestamp': report.timestamp,
        'verdict': report.verdict,
        'summary': report.summary,
        'build_ok': report.build_ok,
        'build_error': report.build_error,
        'vtable': report.vtable,
        'crash': report.crash,
        'hang': report.hang,
        'blank': report.blank,
        'all_errors': report.all_errors,
        'all_recommendations': report.all_recommendations,
        'vtable_slots_hit': report.vtable_slots_hit,
        'vtable_slots_missing': report.vtable_slots_missing,
        'elapsed_s': report.elapsed_s,
        'pipeline_stages': report.pipeline_stages,
        'evidence_files': report.evidence_files,
    }
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report_dict, f, indent=2, ensure_ascii=False, default=str)
    print(f"  [unified] JSON → {json_path}")

    return md_path, json_path


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="no-mo-blackbox Master Forensics Runner — comprehensive Kain component debugging",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
EXAMPLES:
  python run_forensics.py my_component.kn
  python run_forensics.py my_component.exe --skip-build
  python run_forensics.py --suite smoke
  python run_forensics.py --suite full
  python run_forensics.py --all
  python run_forensics.py my_component.kn --timeout 10 --wait 2000
        """
    )
    parser.add_argument("target", nargs='?', help="Path to .kn or .exe file")
    parser.add_argument("--suite", choices=["smoke", "full", "forensics", "vtable"],
                        help="Run a named test suite from taxonomy.toml")
    parser.add_argument("--all", action="store_true", help="Run on all .kn files in taxonomy component roots")
    parser.add_argument("--skip-build", action="store_true", help="Target is already a .exe")
    parser.add_argument("--skip-trace", action="store_true", help="Skip vtable tracing")
    parser.add_argument("--skip-crash", action="store_true", help="Skip crash forensics")
    parser.add_argument("--skip-hang", action="store_true", help="Skip hang detection")
    parser.add_argument("--skip-blank", action="store_true", help="Skip blank window analysis")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Timeout in seconds")
    parser.add_argument("--wait", type=int, default=DEFAULT_WAIT_MS, help="Wait ms for blank capture")
    parser.add_argument("--output", help="Output directory")
    parser.add_argument("--json-only", action="store_true", help="Output JSON only (no markdown)")

    args = parser.parse_args()

    output_dir = Path(args.output) if args.output else None

    # Suite mode
    if args.suite:
        results = run_suite(args.suite, output_dir)
        # Summary
        print(f"\n{'='*70}")
        print(f"SUITE '{args.suite}' RESULTS")
        print(f"{'='*70}")
        counts = {}
        for r in results:
            v = r.verdict
            counts[v] = counts.get(v, 0) + 1
        for v, c in counts.items():
            print(f"  {v}: {c}")
        return 0 if all(r.verdict == "ALL_CLEAN" for r in results) else 1

    # All mode
    if args.all:
        roots = TAXONOMY.get('component_roots', {}).get('paths', [])
        if not roots:
            print("No component roots in taxonomy. Use --target instead.")
            return 1
        results = run_all_kn_files(roots, output_dir)
        return 0

    # Single target mode
    if not args.target:
        parser.print_help()
        return 1

    report = run_full_pipeline(
        args.target,
        timeout_s=args.timeout,
        wait_ms=args.wait,
        output_dir=output_dir,
        skip_build=args.skip_build,
        skip_trace=args.skip_trace,
        skip_crash=args.skip_crash,
        skip_hang=args.skip_hang,
        skip_blank=args.skip_blank,
    )

    if not args.json_only:
        write_unified_report(report, output_dir)

    # Terminal summary
    print(f"\n{'='*70}")
    print(f"FORENSICS REPORT")
    print(f"{'='*70}")
    print(f"  Target:    {report.target}")
    print(f"  Verdict:   {report.verdict}")
    print(f"  Elapsed:   {report.elapsed_s:.1f}s")
    print(f"  VTable:    {report.vtable_slots_hit}/24 slots hit"
          f"{' ⚠️' if report.vtable_slots_missing else ''}")
    print(f"  Crash:     {report.crash.get('exception_name', 'N/A') if report.crash else 'N/A'}")
    print(f"  Hang:      {'HUNG' if report.hang and report.hang.get('hung') else 'OK'}")
    print(f"  Blank:     {report.blank.get('verdict', 'N/A') if report.blank else 'N/A'}")
    print(f"  Errors:    {len(report.all_errors)}")
    print(f"  Evidence:  {len(report.evidence_files)} files")
    print(f"{'='*70}")

    return 0 if report.verdict == "ALL_CLEAN" else 1


if __name__ == "__main__":
    sys.exit(main())
