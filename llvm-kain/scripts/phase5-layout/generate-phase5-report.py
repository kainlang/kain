#!/usr/bin/env python3
"""
generate-phase5-report.py — Phase 5 Master Summary Report
==========================================================
SAFE MODE: No files will be modified.

Reads the output from all other Phase 5 analysis scripts and produces a
comprehensive summary report in reports/phase5-summary.md.

Usage:
    python generate-phase5-report.py [--output-dir DIR] [--verbose]
"""

import argparse
import csv
import json
import os
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

# ─── Configuration ───────────────────────────────────────────────────────────

LLVM_PROJECT_ROOT = Path("X:/llvm-project")


def load_json(path: Path) -> dict:
    """Load a JSON report file."""
    try:
        with open(str(path), "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        return {"error": str(e)}


def load_tsv(path: Path) -> list:
    """Load a TSV report file and return list of dicts."""
    rows = []
    try:
        with open(str(path), "r", encoding="utf-8") as f:
            reader = csv.DictReader(f, delimiter="\t")
            for row in reader:
                rows.append(dict(row))
    except FileNotFoundError:
        pass
    return rows


def format_mb(size_bytes: int) -> str:
    """Format bytes as MB string."""
    return f"{size_bytes / (1024 * 1024):.1f} MB"


def main():
    parser = argparse.ArgumentParser(
        description="Phase 5: Generate master summary report (SAFE MODE)",
    )
    parser.add_argument("--output-dir", type=str,
                        default="scripts/phase5-layout/reports",
                        help="Output directory for reports")
    parser.add_argument("--verbose", action="store_true",
                        help="Print detailed progress")
    parser.add_argument("--input-dir", type=str,
                        default="scripts/phase5-layout/reports",
                        help="Input directory containing other script outputs")
    args = parser.parse_args()

    print("=" * 70)
    print("PHASE 5 LAYOUT ANALYSIS — generate-phase5-report.py")
    print("SAFE MODE: No files will be modified.")
    print("=" * 70)
    print()

    output_dir = Path(args.output_dir)
    input_dir = Path(args.input_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    start_time = datetime.now()

    # ── Load all reports ──────────────────────────────────────────────────
    if args.verbose:
        print("Loading existing report data...")

    current_tree = load_json(input_dir / "current-tree.json")
    file_movements = load_tsv(input_dir / "file-movements.tsv")
    broken_includes = load_tsv(input_dir / "broken-includes.tsv")

    if args.verbose:
        print(f"  current-tree.json: {'loaded' if 'overall' in current_tree else 'NOT FOUND'}")
        print(f"  file-movements.tsv: {len(file_movements)} rows")
        print(f"  broken-includes.tsv: {len(broken_includes)} rows")
        print()

    # ── Compute summary statistics ────────────────────────────────────────

    # Overall tree stats
    overall = current_tree.get("overall", {})
    total_dirs = overall.get("dirs", 0)
    total_files = overall.get("files", 0)
    total_size_mb = overall.get("size_mb", 0)

    # File movement stats
    move_count = 0
    keep_count = 0
    area_stats = defaultdict(lambda: {"files": 0, "bytes": 0})
    action_stats = defaultdict(int)

    for row in file_movements:
        action = row.get("action", "unknown")
        action_stats[action] += 1
        if action == "move":
            move_count += 1
        elif action == "keep-in-place":
            keep_count += 1

        area = row.get("area", "unknown")
        area_stats[area]["files"] += 1
        try:
            area_stats[area]["bytes"] += int(row.get("size_bytes", 0))
        except (ValueError, TypeError):
            pass

    # Include analysis stats
    inc_ok = sum(1 for r in broken_includes if r.get("status") == "ok")
    inc_broken = sum(1 for r in broken_includes if r.get("status") == "needs-review")
    inc_dropped = sum(1 for r in broken_includes if r.get("status") == "dropped-library")

    # Cross-area include dependencies
    cross_area_deps = defaultdict(int)
    for row in broken_includes:
        source = row.get("source_file", "").replace("\\", "/")
        inc_path = row.get("include_path", "").replace("\\", "/")
        if source.startswith("clang/") and inc_path.startswith("llvm/"):
            cross_area_deps["clang -> llvm"] += 1
        elif source.startswith("llvm/") and inc_path.startswith("clang/"):
            cross_area_deps["llvm -> clang"] += 1
        elif source.startswith("lld/") and inc_path.startswith("llvm/"):
            cross_area_deps["lld -> llvm"] += 1
        elif source.startswith("llvm/") and inc_path.startswith("lld/"):
            cross_area_deps["llvm -> lld"] += 1

    # Order of operations analysis
    # Identify which areas have no cross-dependencies and can be moved first
    move_order = []
    if move_count > 0:
        # LLVM JIT depends on LLVM core
        # LLVM core has few dependencies
        # Clang depends on LLVM core
        # Compiler-rt is independent
        move_order = [
            "Phase 5a: Move LLVM Support (llvm/lib/Support/ -> src/support/)",
            "Phase 5b: Move LLVM Core (llvm/lib/IR/, Analysis/, Transforms/, etc. -> src/core/)",
            "Phase 5c: Move LLVM Targets (llvm/lib/Target/X86+AArch64 -> src/target/)",
            "Phase 5d: Move LLVM JIT (llvm/lib/ExecutionEngine/ -> src/jit/)",
            "Phase 5e: Move LLVM Includes (llvm/include/ -> include/)",
            "Phase 5f: Move Clang (clang/lib/, clang/include/ -> clang/src/, clang/include/)",
            "Phase 5g: Move LLD (lld/ -> lld/)",
            "Phase 5h: Move compiler-rt builtins (-> rt/builtins/)",
            "Phase 5i: Move LLVM includes (llvm/include/llvm-c/ -> include/c-api/)",
            "Phase 5j: Generate __ORIGINAL__ symlink",
            "Phase 5k: Update CMakeLists.txt / build.kn for new paths",
            "Phase 5l: Fix all broken include paths (~search-and-replace across all sources)",
        ]

    # Risk assessment
    risk_assessment = []
    if inc_dropped > 0:
        risk_assessment.append(f"**HIGH**: {inc_dropped} include references point to dropped libraries. Source files referencing dropped headers will fail to compile until those references are removed or the dropped header is recovered.")
    if cross_area_deps.get("llvm -> clang", 0) > 0:
        risk_assessment.append(f"**MEDIUM**: {cross_area_deps['llvm -> clang']} LLVM-to-Clang deps — unusual, verify these aren't from dead code.")
    if cross_area_deps.get("lld -> llvm", 0) > 0:
        risk_assessment.append(f"**INFO**: {cross_area_deps['lld -> llvm']} LLD-to-LLVM deps — expected, LLD links against LLVM.")
    if move_count > 5000:
        risk_assessment.append(f"**MEDIUM**: {move_count} files to move — large bulk operation. Use git-mv or robocopy with logging.")
    risk_assessment.append("**LOW**: All scripts are analysis-only. No files have been modified.")
    risk_assessment.append("**NOTE**: After moving, every #include directive needs path rewriting (approximately one sed/awk pass per area).")

    # ── Build report ──────────────────────────────────────────────────────
    report_lines = []
    report_lines.append("# Phase 5: Clean Flat Layout Restructure — Summary Report")
    report_lines.append("")
    report_lines.append(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    report_lines.append(f"**Source tree:** `{LLVM_PROJECT_ROOT}`")
    report_lines.append(f"**Target tree:** `llvm-kain/`")
    report_lines.append("")
    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 1. Executive Summary")
    report_lines.append("")
    report_lines.append(f"| Metric | Value |")
    report_lines.append(f"|--------|-------|")
    report_lines.append(f"| Current tree size | {total_size_mb:.1f} MB |")
    report_lines.append(f"| Current directories | {total_dirs} |")
    report_lines.append(f"| Current files | {total_files} |")
    report_lines.append(f"| Files to move | {move_count} |")
    report_lines.append(f"| Files staying in place | {keep_count} |")
    report_lines.append(f"| Include paths that can be auto-mapped | {inc_ok} |")
    report_lines.append(f"| Include paths needing manual review | {inc_broken} |")
    report_lines.append(f"| References to dropped libraries | {inc_dropped} |")
    report_lines.append(f"| Estimated Phase 5 duration | 1-2 weeks |")
    report_lines.append(f"| Risk level | **MEDIUM** — bulk file move + include path rewrite |")
    report_lines.append("")
    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 2. Current Tree Structure")
    report_lines.append("")
    report_lines.append(f"Current root: `{LLVM_PROJECT_ROOT}` ({total_size_mb:.1f} MB, {total_dirs} dirs, {total_files} files)")
    report_lines.append("")
    report_lines.append("### 2.1 Per-Area Breakdown")
    report_lines.append("")
    report_lines.append("| Area | Size (MB) | Files | Source files | Dirs |")
    report_lines.append("|------|-----------|-------|-------------|------|")

    areas = current_tree.get("areas", {})
    for area_key, area_data in sorted(areas.items()):
        label = area_data.get("label", area_key)
        mb = area_data.get("size_mb", 0)
        fcount = area_data.get("files", 0)
        src = area_data.get("source_files", 0)
        dcount = area_data.get("dirs", 0)
        report_lines.append(f"| {label} | {mb:.1f} | {fcount} | {src} | {dcount} |")

    report_lines.append("")
    report_lines.append("### 2.2 LLVM lib/ Subdirectories")
    report_lines.append("")

    interesting = current_tree.get("interesting_directories", {})
    for area_name, dirs in sorted(interesting.items()):
        if dirs:
            report_lines.append(f"**{area_name}** ({len(dirs)} subdirectories at depth 2-3):")
            for d in dirs[:15]:
                report_lines.append(f"  - `{d['path']}/`")
            if len(dirs) > 15:
                report_lines.append(f"  - *... and {len(dirs)-15} more*")
            report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 3. File Movement Plan")
    report_lines.append("")
    report_lines.append(f"**Total files to move:** {move_count}")
    report_lines.append("")
    report_lines.append("### 3.1 By Area")
    report_lines.append("")
    report_lines.append("| Area | Files | Total Size |")
    report_lines.append("|------|-------|------------|")
    for area, stats in sorted(area_stats.items()):
        report_lines.append(f"| {area} | {stats['files']:,d} | {format_mb(stats['bytes'])} |")
    report_lines.append("")
    report_lines.append("### 3.2 Movement Command Estimate")
    report_lines.append("")
    report_lines.append("Using git-mv or robocopy, the estimated command volume:")
    report_lines.append("")
    report_lines.append(f"- {move_count} individual file move operations")
    report_lines.append(f"- ~{move_count // 500 + 1} batch move commands (at ~500 files/cmd)")
    report_lines.append(f"- {len(set(r.get('area', '') for r in file_movements))} area directories to create first")
    report_lines.append("")

    report_lines.append("### 3.3 Target Layout Summary")
    report_lines.append("")
    report_lines.append("| Source | Target |")
    report_lines.append("|--------|--------|")
    report_lines.append("| `llvm/lib/IR/` | `llvm-kain/src/core/ir/` |")
    report_lines.append("| `llvm/lib/Analysis/` | `llvm-kain/src/core/analysis/` |")
    report_lines.append("| `llvm/lib/Transforms/` | `llvm-kain/src/core/passes/` |")
    report_lines.append("| `llvm/lib/CodeGen/` | `llvm-kain/src/target/shared/codegen/` |")
    report_lines.append("| `llvm/lib/MC/` | `llvm-kain/src/core/mc/` |")
    report_lines.append("| `llvm/lib/Object/` | `llvm-kain/src/core/object/` |")
    report_lines.append("| `llvm/lib/Support/` | `llvm-kain/src/support/adt/` |")
    report_lines.append("| `llvm/lib/Target/X86/` | `llvm-kain/src/target/x86/` |")
    report_lines.append("| `llvm/lib/Target/AArch64/` | `llvm-kain/src/target/aarch64/` |")
    report_lines.append("| `llvm/lib/ExecutionEngine/Orc/` | `llvm-kain/src/jit/orc/` |")
    report_lines.append("| `llvm/lib/ExecutionEngine/JITLink/` | `llvm-kain/src/jit/jitlink/` |")
    report_lines.append("| `llvm/include/llvm/` | `llvm-kain/include/` |")
    report_lines.append("| `llvm/include/llvm-c/` | `llvm-kain/include/c-api/` |")
    report_lines.append("| `clang/lib/` | `llvm-kain/clang/src/` |")
    report_lines.append("| `clang/include/clang/` | `llvm-kain/clang/include/` |")
    report_lines.append("| `compiler-rt/lib/builtins/` | `llvm-kain/rt/builtins/` |")
    report_lines.append("| `lld/` | `llvm-kain/lld/` |")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 4. Include Dependency Analysis")
    report_lines.append("")

    if inc_dropped > 0:
        report_lines.append("### 4.1 DROPPED Library References (CRITICAL)")
        report_lines.append("")
        report_lines.append(f"**{inc_dropped} include references** point to headers from libraries that have been deleted in Phases 1-3.")
        report_lines.append("These will cause build failures and must be removed or guarded:")
        report_lines.append("")
        report_lines.append("| Dropped Path | Ref Count |")
        report_lines.append("|-------------|----------|")
        dropped_counts = defaultdict(int)
        for row in broken_includes:
            if row.get("status") == "dropped-library":
                inc = row.get("include_path", "")
                prefix = "/".join(inc.split("/")[:2]) if "/" in inc else inc
                dropped_counts[prefix] += 1
        for prefix, cnt in sorted(dropped_counts.items(), key=lambda x: -x[1])[:15]:
            report_lines.append(f"| `{prefix}/` | {cnt} |")
        report_lines.append("")
        report_lines.append("*These are from dead libraries (Driver, Format, Tooling, StaticAnalyzer, etc.) that should already be excluded. If your Phase 2/3 deletions left code referencing them, these will need patching.*")
        report_lines.append("")

    if inc_broken > 0:
        report_lines.append("### 4.2 Unmapped Include Paths (NEEDS REVIEW)")
        report_lines.append("")
        report_lines.append(f"**{inc_broken} include paths** could not be automatically mapped to the new layout.")
        report_lines.append("These are typically new or unclassified include patterns that need manual inspection:")
        report_lines.append("")
        report_lines.append("Sample:")
        reviewed = 0
        for row in broken_includes:
            if row.get("status") == "needs-review" and reviewed < 5:
                report_lines.append(f"  - `{row['source_file']}` : {row['include_path']}")
                reviewed += 1
        report_lines.append("")

    report_lines.append("### 4.3 Cross-Area Dependencies")
    report_lines.append("")
    report_lines.append("| Dependency | Count | Notes |")
    report_lines.append("|------------|-------|-------|")
    for dep, cnt in sorted(cross_area_deps.items(), key=lambda x: -x[1]):
        notes = ""
        if dep == "clang -> llvm":
            notes = "Expected — Clang codegen depends on LLVM IR"
        elif dep == "lld -> llvm":
            notes = "Expected — LLD links LLVM libraries"
        elif dep == "llvm -> clang":
            notes = "Unusual — may be from dead code"
        report_lines.append(f"| {dep} | {cnt} | {notes} |")
    if not cross_area_deps:
        report_lines.append("| *(none)* | 0 | |")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 5. Execution Plan — Order of Operations")
    report_lines.append("")
    report_lines.append("Phase 5 should be executed in this order to minimize conflicts:")
    report_lines.append("")
    for i, step in enumerate(move_order, 1):
        report_lines.append(f"{i}. **{step}**")
    report_lines.append("")
    report_lines.append("### 5.1 Critical Path")
    report_lines.append("")
    report_lines.append("1. **Create target directory skeleton** — all new directories under `llvm-kain/`")
    report_lines.append("2. **Move independent areas first** (no cross-dependencies):")
    report_lines.append("   - compiler-rt builtins → `rt/builtins/`")
    report_lines.append("   - LLD → `lld/`")
    report_lines.append("3. **Move support infrastructure**:")
    report_lines.append("   - LLVM Support → `src/support/adt/`")
    report_lines.append("   - LLVM TargetParser → `src/support/target/`")
    report_lines.append("4. **Move LLVM core** (requires support):")
    report_lines.append("   - IR, Analysis, Transforms, MC, Object, BinaryFormat → `src/core/`")
    report_lines.append("5. **Move LLVM targets** (requires core + CodeGen):")
    report_lines.append("   - X86 → `src/target/x86/`")
    report_lines.append("   - AArch64 → `src/target/aarch64/`")
    report_lines.append("   - CodeGen shared → `src/target/shared/`")
    report_lines.append("6. **Move includes** (mirror source structure):")
    report_lines.append("   - `llvm/include/llvm/` → `include/`")
    report_lines.append("7. **Move Clang** (requires LLVM includes):")
    report_lines.append("   - `clang/lib/` → `clang/src/`")
    report_lines.append("   - `clang/include/` → `clang/include/`")
    report_lines.append("8. **Global include path rewrite**")
    report_lines.append("9. **Build verification**")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 6. Risk Assessment")
    report_lines.append("")
    for risk in risk_assessment:
        report_lines.append(f"- {risk}")
    report_lines.append("")

    report_lines.append("### 6.1 Risk Matrix")
    report_lines.append("")
    report_lines.append("| Risk | Likelihood | Impact | Mitigation |")
    report_lines.append("|------|-----------|--------|------------|")
    report_lines.append("| Broken include paths | HIGH | HIGH | Pre-scan with verify-dependencies.py; prepare sed script for bulk rewrite |")
    report_lines.append("| File move conflicts | MEDIUM | HIGH | Use `git mv` not `os.rename`; commit in batches |")
    report_lines.append("| Clang LLVM IR codegen references | MEDIUM | MEDIUM | Verify clang CodeGen references use updated paths |")
    report_lines.append("| Missed CMakeLists.txt targets | MEDIUM | HIGH | Single flat CMakeLists.txt avoids recursive build issues |")
    report_lines.append("| LLD dependency on LLVM | LOW | MEDIUM | LLD is small; verify after move |")
    report_lines.append("| File permission loss | LOW | LOW | Use git mv preserves permissions |")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 7. Time Estimate")
    report_lines.append("")
    report_lines.append("| Phase | Estimated Duration | Details |")
    report_lines.append("|-------|-------------------|---------|")
    report_lines.append("| Setup & directory creation | 1 hour | Create all target directories under llvm-kain/ |")
    report_lines.append("| File moves (bulk) | 4-8 hours | `git mv` in batches of 500-1000 files |")
    report_lines.append("| Include path rewrite | 4-8 hours | sed/awk bulk rewrite, then fix remaining broken refs |")
    report_lines.append("| CMakeLists.txt adaptation | 2-4 hours | Single flat CMakeLists.txt or build.kn |")
    report_lines.append("| Build verification | 2-4 hours | Iterative build-fix cycle |")
    report_lines.append("| Testing | 2-4 hours | Verify llc + opt + clang work correctly |")
    report_lines.append("| **Total** | **15-29 hours** | **~1-2 weeks at 2-4 hrs/day** |")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 8. Tools & Commands")
    report_lines.append("")
    report_lines.append("### Creating directories (PowerShell)")
    report_lines.append("```powershell")
    report_lines.append("# Create all target directories at once")
    report_lines.append("$targets = @(")
    for area, stats in sorted(area_stats.items()):
        report_lines.append(f"    'llvm-kain/{area}/',")
    report_lines.append(")")
    report_lines.append("foreach ($t in $targets) { New-Item -ItemType Directory -Force -Path $t }")
    report_lines.append("```")
    report_lines.append("")
    report_lines.append("### Bulk file move (PowerShell)")
    report_lines.append("```powershell")
    report_lines.append("# Generate git-mv commands from file-movements.tsv for each area")
    report_lines.append("# Example for one area:")
    report_lines.append("Get-Content reports/file-movements.tsv | Select-Object -Skip 1 | ConvertFrom-Csv -Delimiter \"`t\" | Where-Object { $_.action -eq 'move' -and $_.area -eq 'llvm-core' } | ForEach-Object { git mv $_.current_path $_.target_path }")
    report_lines.append("```")
    report_lines.append("")
    report_lines.append("### Include path bulk rewrite (PowerShell)")
    report_lines.append("```powershell")
    report_lines.append("# For llvm-backed includes (<llvm/...> -> <core/...> etc)")
    report_lines.append("# See verify-dependencies.py output for exact mappings")
    report_lines.append("Get-ChildItem -Recurse -Filter *.cpp,*.h,*.c,*.hpp | ForEach-Object {")
    report_lines.append("    (Get-Content $_.FullName) -replace 'llvm/IR/', 'core/ir/' -replace 'llvm/Analysis/', 'core/analysis/' | Set-Content $_.FullName")
    report_lines.append("}")
    report_lines.append("```")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 9. Post-Move Verification Checklist")
    report_lines.append("")
    report_lines.append("- [ ] `llvm-kain/` directory structure matches proposed layout")
    report_lines.append("- [ ] All files moved (no stragglers in old paths)")
    report_lines.append("- [ ] `__ORIGINAL__` symlink points to `X:/llvm-project/`")
    report_lines.append("- [ ] All include paths updated (verify-dependencies.py reports 0 broken)")
    report_lines.append("- [ ] Single CMakeLists.txt or build.kn compiles all targets")
    report_lines.append("- [ ] `llvm-kain> cmake -DLLVM_TARGETS_TO_BUILD=X86;AArch64` succeeds")
    report_lines.append("- [ ] `llc --version` shows only X86 + AArch64 targets")
    report_lines.append("- [ ] Clang compiles a simple C file")
    report_lines.append("- [ ] `opt` runs optimization passes without errors")
    report_lines.append("")

    report_lines.append("---")
    report_lines.append("")
    report_lines.append("## 10. Raw Data Sources")
    report_lines.append("")
    report_lines.append("| File | Description |")
    report_lines.append("|------|-------------|")
    report_lines.append("| `current-tree.json` | Full JSON scan of current tree structure |")
    report_lines.append("| `file-movements.tsv` | Every file mapped to its target location |")
    report_lines.append("| `broken-includes.tsv` | Include paths that need updating |")
    report_lines.append("| `phase5-summary.md` | This document |")
    report_lines.append("")

    # ── Write report ──────────────────────────────────────────────────────
    report_text = "\n".join(report_lines)
    report_path = output_dir / "phase5-summary.md"
    with open(str(report_path), "w", encoding="utf-8") as f:
        f.write(report_text)

    elapsed = (datetime.now() - start_time).total_seconds()

    # ── Print summary ─────────────────────────────────────────────────────
    print()
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Report written to: {report_path}")
    print(f"Elapsed: {elapsed:.1f}s")
    print()

    # Print key stats
    print("Key findings from Phase 5 analysis:")
    print(f"  - {move_count} files to move across all areas")
    print(f"  - {inc_ok} includes can be auto-mapped to new paths")
    if inc_broken > 0:
        print(f"  - !!! {inc_broken} includes need manual review")
    if inc_dropped > 0:
        print(f"  - !!! {inc_dropped} includes reference DROPPED libraries")
    print(f"  - Risk: MEDIUM (bulk file move + include rewrite)")
    print()


if __name__ == "__main__":
    main()
