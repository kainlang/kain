#!/usr/bin/env python3
"""
map-file-movements.py — Phase 5 File Movement Mapping
=====================================================
SAFE MODE: No files will be modified.

Reads the current LLVM tree and maps every file to its proposed location
under the new flat layout. Does NOT move anything — creates a mapping table.

Output: TSV with columns:
  current_path   — where the file is now
  target_path    — where it would go
  size_bytes     — file size
  area           — llvm-core / llvm-target / clang / rt / include
  action         — move / symlink / keep-in-place / note

Usage:
    python map-file-movements.py [--output-dir DIR] [--verbose]
"""

import argparse
import csv
import os
import sys
from datetime import datetime
from pathlib import Path

# ─── Configuration ───────────────────────────────────────────────────────────

LLVM_PROJECT_ROOT = Path("X:/llvm-project")

# The target flat layout (from LLVM_REFACTOR.md §3)
# Maps source paths under current tree -> target paths under llvm-kain/
TARGET_ROOT = "llvm-kain"

PATH_MAP = {
    # ── LLVM Core Sources ─────────────────────────────────────────────────
    "llvm/lib/IR/":                    f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/IRReader/":              f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/IRPrinter/":             f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/AsmParser/":             f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/Bitcode/":               f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/Bitstream/":             f"{TARGET_ROOT}/src/core/ir/",
    "llvm/lib/Analysis/":              f"{TARGET_ROOT}/src/core/analysis/",
    "llvm/lib/Transforms/":            f"{TARGET_ROOT}/src/core/passes/",
    "llvm/lib/Passes/":                f"{TARGET_ROOT}/src/core/passes/",
    "llvm/lib/MC/":                    f"{TARGET_ROOT}/src/core/mc/",
    "llvm/lib/Object/":                f"{TARGET_ROOT}/src/core/object/",
    "llvm/lib/BinaryFormat/":          f"{TARGET_ROOT}/src/core/support/",
    "llvm/lib/Demangle/":              f"{TARGET_ROOT}/src/core/support/",
    "llvm/lib/Linker/":                f"{TARGET_ROOT}/src/core/linker/",
    "llvm/lib/Option/":                f"{TARGET_ROOT}/src/core/support/",
    "llvm/lib/ProfileData/":           f"{TARGET_ROOT}/src/core/profiledata/",
    "llvm/lib/Remarks/":               f"{TARGET_ROOT}/src/core/remarks/",
    "llvm/lib/CodeGenTypes/":          f"{TARGET_ROOT}/src/target/shared/",
    "llvm/lib/DWARFCFIChecker/":       f"{TARGET_ROOT}/src/core/debug/",
    "llvm/lib/DTLTO/":                 f"{TARGET_ROOT}/src/core/linker/",
    "llvm/lib/TableGen/":              f"{TARGET_ROOT}/tools/tablegen/",

    # ── LLVM CodeGen (shared target infrastructure) ─────────────────────
    "llvm/lib/CodeGen/":               f"{TARGET_ROOT}/src/target/shared/codegen/",
    "llvm/lib/CodeGen/AsmPrinter/":    f"{TARGET_ROOT}/src/target/shared/codegen/",
    "llvm/lib/CodeGen/GlobalISel/":    f"{TARGET_ROOT}/src/target/shared/globalisel/",
    "llvm/lib/CodeGen/SelectionDAG/":  f"{TARGET_ROOT}/src/target/shared/selectiondag/",
    "llvm/lib/CodeGen/LiveDebugValues/": f"{TARGET_ROOT}/src/target/shared/codegen/",
    "llvm/lib/CodeGen/MIRParser/":      f"{TARGET_ROOT}/src/target/shared/mir/",

    # ── Support (was llvm/lib/Support/) ──────────────────────────────────
    "llvm/lib/Support/":               f"{TARGET_ROOT}/src/support/adt/",

    # ── Target Parser ────────────────────────────────────────────────────
    "llvm/lib/TargetParser/":          f"{TARGET_ROOT}/src/support/target/",

    # ── LLVM Targets (X86 + AArch64 only) ───────────────────────────────
    "llvm/lib/Target/X86/":            f"{TARGET_ROOT}/src/target/x86/",
    "llvm/lib/Target/AArch64/":       f"{TARGET_ROOT}/src/target/aarch64/",

    # ── JIT (ExecutionEngine) ────────────────────────────────────────────
    "llvm/lib/ExecutionEngine/Orc/":           f"{TARGET_ROOT}/src/jit/orc/",
    "llvm/lib/ExecutionEngine/JITLink/":       f"{TARGET_ROOT}/src/jit/jitlink/",
    "llvm/lib/ExecutionEngine/Interpreter/":   f"{TARGET_ROOT}/src/jit/interpreter/",

    # ── LLVM Include Headers ─────────────────────────────────────────────
    "llvm/include/llvm/IR/":                f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/IRPrinter/":         f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/IRReader/":          f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/AsmParser/":         f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/Analysis/":          f"{TARGET_ROOT}/include/core/analysis/",
    "llvm/include/llvm/Transforms/":        f"{TARGET_ROOT}/include/core/passes/",
    "llvm/include/llvm/Passes/":            f"{TARGET_ROOT}/include/core/passes/",
    "llvm/include/llvm/CodeGen/":           f"{TARGET_ROOT}/include/target/shared/codegen/",
    "llvm/include/llvm/CodeGenTypes/":      f"{TARGET_ROOT}/include/target/shared/codegen/",
    "llvm/include/llvm/MC/":                f"{TARGET_ROOT}/include/core/mc/",
    "llvm/include/llvm/Object/":            f"{TARGET_ROOT}/include/core/object/",
    "llvm/include/llvm/BinaryFormat/":      f"{TARGET_ROOT}/include/core/support/",
    "llvm/include/llvm/Demangle/":          f"{TARGET_ROOT}/include/core/support/",
    "llvm/include/llvm/Bitcode/":           f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/Bitstream/":         f"{TARGET_ROOT}/include/core/ir/",
    "llvm/include/llvm/DebugInfo/":         f"{TARGET_ROOT}/include/core/debug/",
    "llvm/include/llvm/DWARFCFIChecker/":   f"{TARGET_ROOT}/include/core/debug/",
    "llvm/include/llvm/DTLTO/":             f"{TARGET_ROOT}/include/core/linker/",
    "llvm/include/llvm/ExecutionEngine/":   f"{TARGET_ROOT}/include/jit/",
    "llvm/include/llvm/Linker/":            f"{TARGET_ROOT}/include/core/linker/",
    "llvm/include/llvm/Option/":            f"{TARGET_ROOT}/include/core/support/",
    "llvm/include/llvm/ProfileData/":       f"{TARGET_ROOT}/include/core/profiledata/",
    "llvm/include/llvm/Remarks/":           f"{TARGET_ROOT}/include/core/support/",
    "llvm/include/llvm/Support/":           f"{TARGET_ROOT}/include/support/adt/",
    "llvm/include/llvm/Target/":            f"{TARGET_ROOT}/include/target/shared/",
    "llvm/include/llvm/TargetParser/":      f"{TARGET_ROOT}/include/support/target/",
    "llvm/include/llvm/TableGen/":          f"{TARGET_ROOT}/include/tools/tablegen/",
    "llvm/include/llvm/ADT/":               f"{TARGET_ROOT}/include/support/adt/",
    "llvm/include/llvm/Config/":            f"{TARGET_ROOT}/include/core/config/",
    "llvm/include/llvm/InitializePasses.h": f"{TARGET_ROOT}/include/core/InitializePasses.h",
    "llvm/include/llvm/LinkAllIR.h":        f"{TARGET_ROOT}/include/core/LinkAllIR.h",
    "llvm/include/llvm/LinkAllPasses.h":    f"{TARGET_ROOT}/include/core/LinkAllPasses.h",
    "llvm/include/llvm/Pass.h":             f"{TARGET_ROOT}/include/core/Pass.h",
    "llvm/include/llvm/PassAnalysisSupport.h": f"{TARGET_ROOT}/include/core/PassAnalysisSupport.h",
    "llvm/include/llvm/PassInfo.h":         f"{TARGET_ROOT}/include/core/PassInfo.h",
    "llvm/include/llvm/PassRegistry.h":     f"{TARGET_ROOT}/include/core/PassRegistry.h",
    "llvm/include/llvm/PassSupport.h":      f"{TARGET_ROOT}/include/core/PassSupport.h",
    "llvm/include/llvm-c/":                 f"{TARGET_ROOT}/include/c-api/",
    "llvm/include/llvm/ABI/":               f"{TARGET_ROOT}/include/support/abi/",
    "llvm/include/llvm/WindowsResource/":   f"{TARGET_ROOT}/include/core/object/",
    "llvm/include/llvm/Frontend/":          f"{TARGET_ROOT}/include/core/frontend/",

    # ── Clang Sources ────────────────────────────────────────────────────
    "clang/lib/Basic/":             f"{TARGET_ROOT}/clang/src/basic/",
    "clang/lib/Lex/":               f"{TARGET_ROOT}/clang/src/lex/",
    "clang/lib/Parse/":             f"{TARGET_ROOT}/clang/src/parse/",
    "clang/lib/AST/":               f"{TARGET_ROOT}/clang/src/ast/",
    "clang/lib/ASTMatchers/":       f"{TARGET_ROOT}/clang/src/ast/",
    "clang/lib/Sema/":              f"{TARGET_ROOT}/clang/src/sema/",
    "clang/lib/CodeGen/":           f"{TARGET_ROOT}/clang/src/codegen/",
    "clang/lib/Frontend/":          f"{TARGET_ROOT}/clang/src/frontend/",
    "clang/lib/FrontendTool/":      f"{TARGET_ROOT}/clang/src/frontend/",
    "clang/lib/Serialization/":     f"{TARGET_ROOT}/clang/src/serialization/",
    "clang/lib/Analysis/":          f"{TARGET_ROOT}/clang/src/analysis/",
    "clang/lib/Edit/":              f"{TARGET_ROOT}/clang/src/edit/",
    "clang/lib/Headers/":           f"{TARGET_ROOT}/clang/include/",
    "clang/lib/Options/":           f"{TARGET_ROOT}/clang/src/options/",

    # ── Clang Include Headers ────────────────────────────────────────────
    "clang/include/clang/Basic/":       f"{TARGET_ROOT}/clang/include/basic/",
    "clang/include/clang/Lex/":         f"{TARGET_ROOT}/clang/include/lex/",
    "clang/include/clang/Parse/":       f"{TARGET_ROOT}/clang/include/parse/",
    "clang/include/clang/AST/":         f"{TARGET_ROOT}/clang/include/ast/",
    "clang/include/clang/ASTMatchers/": f"{TARGET_ROOT}/clang/include/ast/",
    "clang/include/clang/Sema/":        f"{TARGET_ROOT}/clang/include/sema/",
    "clang/include/clang/CodeGen/":     f"{TARGET_ROOT}/clang/include/codegen/",
    "clang/include/clang/Frontend/":    f"{TARGET_ROOT}/clang/include/frontend/",
    "clang/include/clang/Serialization/": f"{TARGET_ROOT}/clang/include/serialization/",
    "clang/include/clang/Analysis/":    f"{TARGET_ROOT}/clang/include/analysis/",
    "clang/include/clang/Edit/":        f"{TARGET_ROOT}/clang/include/edit/",

    # ── compiler-rt builtins ─────────────────────────────────────────────
    "compiler-rt/lib/builtins/":    f"{TARGET_ROOT}/rt/builtins/",

    # ── LLD ──────────────────────────────────────────────────────────────
    "lld/Common/":                  f"{TARGET_ROOT}/lld/common/",
    "lld/COFF/":                    f"{TARGET_ROOT}/lld/coff/",
    "lld/ELF/":                     f"{TARGET_ROOT}/lld/elf/",
    "lld/MachO/":                   f"{TARGET_ROOT}/lld/macho/",
    "lld/include/":                 f"{TARGET_ROOT}/lld/include/",
}

# Suffix patterns to identify header directories for include analysis
HEADER_DIR_PATTERNS = ["include/", "Headers/"]


def classify_area(current_path: str, target_path: str) -> str:
    """Classify the area for a file based on its target path."""
    tp = target_path.lower()

    if tp.startswith(f"{TARGET_ROOT}/src/core/"):
        return "llvm-core"
    elif tp.startswith(f"{TARGET_ROOT}/src/target/"):
        return "llvm-target"
    elif tp.startswith(f"{TARGET_ROOT}/src/jit/"):
        return "llvm-jit"
    elif tp.startswith(f"{TARGET_ROOT}/src/support/"):
        return "llvm-support"
    elif tp.startswith(f"{TARGET_ROOT}/include/"):
        return "include"
    elif tp.startswith(f"{TARGET_ROOT}/clang/"):
        return "clang"
    elif tp.startswith(f"{TARGET_ROOT}/rt/"):
        return "rt"
    elif tp.startswith(f"{TARGET_ROOT}/lld/"):
        return "lld"
    elif tp.startswith(f"{TARGET_ROOT}/tools/"):
        return "tools"
    else:
        return "other"


def find_best_target(current_path: str) -> str:
    """
    Given a relative current path, find the best target path from PATH_MAP.
    Checks parent directories hierarchically.
    """
    # Normalize to forward slashes
    normalized = current_path.replace("\\", "/")

    # Try exact match first
    for src_prefix, tgt_prefix in sorted(PATH_MAP.items(), key=lambda x: -len(x[0])):
        src_key = src_prefix.replace("\\", "/")
        if normalized.startswith(src_key) or normalized == src_key.rstrip("/"):
            # Compute the relative suffix
            suffix = normalized[len(src_key):]
            if suffix.startswith("/"):
                suffix = suffix[1:]
            if suffix:
                # The target already includes the directory, figure out where this file goes
                if tgt_prefix.endswith("/"):
                    return tgt_prefix + suffix
                else:
                    # It's a specific file mapping (e.g. InitializePasses.h)
                    return tgt_prefix
            else:
                return tgt_prefix.rstrip("/")

    return ""


def should_process_file(rel_path: str) -> bool:
    """Check if a file should be mapped to the new layout."""
    parts = rel_path.replace("\\", "/").split("/")

    # Only process files in areas that will be kept/moved
    keep_prefixes = [
        "llvm/lib/", "llvm/include/",
        "clang/lib/", "clang/include/",
        "compiler-rt/lib/builtins",
        "lld/",
    ]
    for prefix in keep_prefixes:
        if rel_path.startswith(prefix):
            return True
    return False


def get_area_from_source(rel_path: str) -> str:
    """Get area label from current source path."""
    rel = rel_path.replace("\\", "/")
    if rel.startswith("clang/"):
        return "clang"
    elif rel.startswith("compiler-rt/"):
        return "rt"
    elif rel.startswith("lld/"):
        return "lld"
    elif rel.startswith("llvm/lib/Target/X86") or rel.startswith("llvm/lib/Target/AArch64"):
        return "llvm-target"
    elif rel.startswith("llvm/lib/ExecutionEngine"):
        return "llvm-jit"
    elif rel.startswith("llvm/lib/Support"):
        return "llvm-support"
    elif rel.startswith("llvm/"):
        return "llvm-core"
    return "other"


def determine_action(current_path: str, target_path: str) -> str:
    """Determine what action would be needed for this file."""
    if not target_path:
        return "keep-in-place"
    if current_path == target_path:
        return "keep-in-place"
    # Files at root level like cmake/ utils/ runtimes/ stay in place
    area = get_area_from_source(current_path)
    if area == "other":
        return "keep-in-place"
    return "move"


def main():
    parser = argparse.ArgumentParser(
        description="Phase 5: Map file movements to new flat layout (SAFE MODE)",
    )
    parser.add_argument("--output-dir", type=str,
                        default="scripts/phase5-layout/reports",
                        help="Output directory for reports")
    parser.add_argument("--verbose", action="store_true",
                        help="Print detailed progress")
    args = parser.parse_args()

    print("=" * 70)
    print("PHASE 5 LAYOUT ANALYSIS — map-file-movements.py")
    print("SAFE MODE: No files will be modified.")
    print("=" * 70)
    print()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    start_time = datetime.now()

    # ── Collect all files ─────────────────────────────────────────────────
    if args.verbose:
        print(f"Scanning {LLVM_PROJECT_ROOT} for files to map...")
        print()

    rows = []
    skipped_headers = []
    file_count = 0
    mapped_count = 0
    total_size = 0

    for root, dirs, files in os.walk(str(LLVM_PROJECT_ROOT)):
        # Skip hidden dirs and __* dirs
        dirs[:] = [d for d in dirs if not d.startswith(".") and not d.startswith("__")]
        rel_root = os.path.relpath(root, str(LLVM_PROJECT_ROOT))
        if rel_root == ".":
            continue

        for fname in files:
            # Skip hidden files
            if fname.startswith("."):
                continue

            current_rel = os.path.join(rel_root, fname).replace("\\", "/")

            # Only process files in keep areas
            if not should_process_file(current_rel):
                continue

            file_count += 1
            fpath = os.path.join(root, fname)

            try:
                size = os.path.getsize(fpath)
            except (OSError, FileNotFoundError):
                size = 0

            target = find_best_target(current_rel)
            area = classify_area(current_rel, target) if target else get_area_from_source(current_rel)
            action = determine_action(current_rel, target)

            rows.append({
                "current_path": current_rel,
                "target_path": target if target else "(keep-in-place)",
                "size_bytes": size,
                "area": area,
                "action": action,
            })

            if target:
                mapped_count += 1
                total_size += size

            if args.verbose and file_count % 5000 == 0:
                print(f"  Processed {file_count} files...")

    # ── Write TSV report ──────────────────────────────────────────────────
    report_path = output_dir / "file-movements.tsv"
    with open(str(report_path), "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, delimiter="\t")
        writer.writerow(["current_path", "target_path", "size_bytes", "area", "action"])
        for row in rows:
            writer.writerow([
                row["current_path"],
                row["target_path"],
                row["size_bytes"],
                row["area"],
                row["action"],
            ])

    elapsed = (datetime.now() - start_time).total_seconds()

    # ── Summary ───────────────────────────────────────────────────────────
    print()
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Total files scanned:    {file_count}")
    print(f"Files with target map:  {mapped_count}")
    print(f"Total data to move:     {total_size / (1024*1024):.1f} MB")
    print()

    # Count by action type
    action_counts = {}
    area_counts = {}
    for r in rows:
        action_counts[r["action"]] = action_counts.get(r["action"], 0) + 1
        area_counts[r["area"]] = area_counts.get(r["area"], 0) + 1

    print("Files by action type:")
    for action, cnt in sorted(action_counts.items()):
        pct = 100.0 * cnt / len(rows) if rows else 0
        print(f"  {action:20s} {cnt:>6d} ({pct:5.1f}%)")
    print()

    print("Files by area:")
    for area, cnt in sorted(area_counts.items()):
        area_size = sum(r["size_bytes"] for r in rows if r["area"] == area)
        print(f"  {area:20s} {cnt:>6d} files, {area_size / (1024*1024):.1f} MB")
    print()

    print(f"Report written to: {report_path}")
    print(f"Elapsed: {elapsed:.1f}s")
    print()


if __name__ == "__main__":
    main()
