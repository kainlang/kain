#!/usr/bin/env python3
"""
verify-dependencies.py — Phase 5 Include Path Verification
===========================================================
SAFE MODE: No files will be modified.

Scans all .cpp, .h, .c files for #include directives and checks whether
the included path would resolve under the new flat layout.

Reports includes that would break in:
  reports/broken-includes.tsv

Usage:
    python verify-dependencies.py [--output-dir DIR] [--verbose] [--max-errors N]
"""

import argparse
import csv
import os
import re
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

# ─── Configuration ───────────────────────────────────────────────────────────

LLVM_PROJECT_ROOT = Path("X:/llvm-project")
TARGET_ROOT = "llvm-kain"

# Include patterns we can handle
INCLUDE_RE = re.compile(r'#\s*include\s+["<]([^">]+)[">]')

# Source directories to scan
SCAN_DIRS = [
    "llvm/lib",
    "llvm/include",
    "clang/lib",
    "clang/include",
    "lld",
    "compiler-rt/lib/builtins",
]

# File extensions to scan
SCAN_EXTENSIONS = {".cpp", ".c", ".h", ".hpp", ".def"}

# Include path mappings: maps old include path prefixes -> new include path prefixes
# These are the transformations that #include directives would need
INCLUDE_PATH_MAP = {
    # llvm/ headers from llvm/include/llvm -> include/core/ etc.
    "llvm/IR/":              "core/ir/",
    "llvm/IRPrinter/":       "core/ir/",
    "llvm/IRReader/":        "core/ir/",
    "llvm/AsmParser/":       "core/ir/",
    "llvm/Analysis/":        "core/analysis/",
    "llvm/Transforms/":      "core/passes/",
    "llvm/Passes/":          "core/passes/",
    "llvm/CodeGen/":         "target/shared/codegen/",
    "llvm/MC/":              "core/mc/",
    "llvm/Object/":          "core/object/",
    "llvm/BinaryFormat/":    "core/support/",
    "llvm/Demangle/":        "core/support/",
    "llvm/Bitcode/":         "core/ir/",
    "llvm/Bitstream/":       "core/ir/",
    "llvm/DebugInfo/":       "core/debug/",
    "llvm/ExecutionEngine/": "jit/",
    "llvm/ExecutionEngine/Orc/": "jit/orc/",
    "llvm/ExecutionEngine/JITLink/": "jit/jitlink/",
    "llvm/Linker/":          "core/linker/",
    "llvm/Option/":          "core/support/",
    "llvm/ProfileData/":     "core/profiledata/",
    "llvm/Remarks/":         "core/support/",
    "llvm/Support/":         "support/adt/",
    "llvm/Target/":          "target/shared/",
    "llvm/TargetParser/":    "support/target/",
    "llvm/TableGen/":        "tools/tablegen/",
    "llvm/ADT/":             "support/adt/",
    "llvm/Config/":          "core/config/",
    "llvm-c/":               "c-api/",
    "llvm/ABI/":             "support/abi/",
    "llvm/WindowsResource/": "core/object/",
    "llvm/Frontend/":        "core/frontend/",

    # clang/ headers from clang/include/clang -> clang/include/
    "clang/Basic/":          "basic/",
    "clang/Lex/":            "lex/",
    "clang/Parse/":          "parse/",
    "clang/AST/":            "ast/",
    "clang/ASTMatchers/":    "ast/",
    "clang/Sema/":           "sema/",
    "clang/CodeGen/":        "codegen/",
    "clang/Frontend/":       "frontend/",
    "clang/Serialization/":  "serialization/",
    "clang/Analysis/":       "analysis/",
    "clang/Edit/":           "edit/",
    "clang/Driver/":         "",  # Driver is dropped
    "clang/Format/":         "",  # Format is dropped
    "clang/Tooling/":        "",  # Tooling is dropped
    "clang/StaticAnalyzer/": "", # StaticAnalyzer is dropped
    "clang/Rewrite/":        "",  # Rewrite is dropped
    "clang/Index/":          "",  # Index is dropped
    "clang/Sandbox/":        "",  # Sandbox is dropped
    "clang/InstallAPI/":     "",  # InstallAPI is dropped
    "clang/APINotes/":       "",  # APINotes is dropped
    "clang/CIR/":            "",  # CIR is dropped
    "clang/Testing/":        "",  # Testing is dropped
    "clang/CrossTU/":        "",  # CrossTU is dropped
    "clang/DirectoryWatcher/": "", # DirectoryWatcher is dropped
    "clang/DependencyScanning/": "", # DependencyScanning is dropped
    "clang/ExtractAPI/":     "",  # ExtractAPI is dropped
    "clang/Interpreter/":    "",  # Interpreter is dropped
}

# Common include paths that are self-referencing within the same source tree
# These are relative includes (e.g., #include "MyHeader.h") that don't change
RELATIVE_INCLUDE_PATTERNS = [
    re.compile(r'^"'),     # Relative includes like "Header.h" or "../foo.h"
]


def resolve_new_include(include_path: str) -> str:
    """
    Try to resolve an include path to its new location under the target layout.
    Returns the resolved path or an empty string if unresolvable.
    """
    for old_prefix, new_prefix in sorted(INCLUDE_PATH_MAP.items(), key=lambda x: -len(x[0])):
        if include_path.startswith(old_prefix):
            if new_prefix == "":
                # This header is from a dropped library — it's dead
                return f"(DROPPED) {include_path}"
            suffix = include_path[len(old_prefix):]
            return new_prefix + suffix

    # Check for llvm/ prefix that wasn't in the map (e.g., new subdirectories)
    if include_path.startswith("llvm/"):
        remainder = include_path[5:]  # strip "llvm/"
        # Try to map what we can
        for old_prefix, new_prefix in sorted(INCLUDE_PATH_MAP.items(), key=lambda x: -len(x[0])):
            if old_prefix.startswith("llvm/") and remainder.startswith(old_prefix[5:]):
                suffix = remainder[len(old_prefix[5:]):]
                return new_prefix + suffix

    return ""


def check_relative_include(include_path: str, source_rel_path: str) -> bool:
    """Check if a relative include is self-referencing (same-directory)."""
    if include_path.startswith("../") or include_path.startswith("./") or "/" not in include_path:
        return True
    return False


def classify_include(include_path: str, source_rel_path: str) -> str:
    """
    Classify an include as one of:
    - system — angle-bracket system include (<stdio.h>, <windows.h>)
    - relative — same-directory relative include
    - absolute — project-internal absolute include (<llvm/IR/Module.h>)
    """
    if not include_path.startswith("llvm/") and not include_path.startswith("clang/") and \
       not include_path.startswith("lld/") and not include_path.startswith("compiler-rt/"):
        return "external"

    # Check if it's in the map
    resolved = resolve_new_include(include_path)
    if resolved:
        return "mapped"

    return "needs-review"


def main():
    parser = argparse.ArgumentParser(
        description="Phase 5: Verify includes under new layout (SAFE MODE)",
    )
    parser.add_argument("--output-dir", type=str,
                        default="scripts/phase5-layout/reports",
                        help="Output directory for reports")
    parser.add_argument("--verbose", action="store_true",
                        help="Print detailed progress")
    parser.add_argument("--max-errors", type=int, default=200,
                        help="Max broken includes to report")
    args = parser.parse_args()

    print("=" * 70)
    print("PHASE 5 LAYOUT ANALYSIS — verify-dependencies.py")
    print("SAFE MODE: No files will be modified.")
    print("=" * 70)
    print()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    start_time = datetime.now()

    # ── Collect all source files ──────────────────────────────────────────
    if args.verbose:
        print(f"Scanning source files in {LLVM_PROJECT_ROOT}...")
        print()

    source_files = []
    for scan_dir in SCAN_DIRS:
        scan_path = LLVM_PROJECT_ROOT / scan_dir
        if not scan_path.exists():
            if args.verbose:
                print(f"  SKIP (not found): {scan_path}")
            continue
        for root, dirs, files in os.walk(str(scan_path)):
            dirs[:] = [d for d in dirs if not d.startswith(".") and not d.startswith("__")]
            for fname in files:
                ext = os.path.splitext(fname)[1].lower()
                if ext in SCAN_EXTENSIONS:
                    source_files.append(os.path.join(root, fname))
                    if args.verbose and len(source_files) % 2000 == 0:
                        print(f"  Found {len(source_files)} source files...")

    if args.verbose:
        print(f"Found {len(source_files)} source files total")
        print()

    # ── Scan each file for includes ───────────────────────────────────────
    broken_includes = []
    all_includes = defaultdict(list)
    total_checked = 0
    ok_count = 0
    broken_count = 0
    external_count = 0
    dropped_count = 0
    relative_count = 0

    for file_idx, filepath in enumerate(source_files):
        if args.verbose and file_idx > 0 and file_idx % 1000 == 0:
            print(f"  Scanned {file_idx}/{len(source_files)} files...")

        rel_filepath = os.path.relpath(filepath, str(LLVM_PROJECT_ROOT)).replace("\\", "/")

        try:
            with open(filepath, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
        except (OSError, PermissionError):
            continue

        total_checked += 1

        for match in INCLUDE_RE.finditer(content):
            include_path = match.group(1)
            line_offset = content[:match.start()].count("\n") + 1

            # Skip system/external includes
            inc_class = classify_include(include_path, rel_filepath)

            full_include = rel_filepath  # the file doing the including
            entry = {
                "source_file": rel_filepath,
                "line": line_offset,
                "include_path": include_path,
                "classification": inc_class,
            }

            if inc_class == "external":
                external_count += 1
                continue

            if inc_class == "mapped":
                # Check if the mapped path resolves to something
                resolved = resolve_new_include(include_path)
                if resolved.startswith("(DROPPED)"):
                    entry["new_path"] = resolved
                    entry["status"] = "dropped-library"
                    broken_includes.append(entry)
                    dropped_count += 1
                else:
                    entry["new_path"] = f"{TARGET_ROOT}/include/{resolved}"
                    entry["status"] = "ok"
                    ok_count += 1
                continue

            # needs-review or other
            entry["new_path"] = resolve_new_include(include_path) if inc_class == "needs-review" else ""
            entry["status"] = "needs-review"
            broken_includes.append(entry)
            broken_count += 1

    # ── Sort broken includes by severity ──────────────────────────────────
    severity_order = {
        "dropped-library": 0,
        "needs-review": 1,
        "ok": 2,
    }
    broken_includes.sort(key=lambda x: (
        severity_order.get(x.get("status", "needs-review"), 99),
        x["source_file"],
        x["line"],
    ))

    # ── Write TSV ─────────────────────────────────────────────────────────
    report_path = output_dir / "broken-includes.tsv"
    with open(str(report_path), "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, delimiter="\t")
        writer.writerow([
            "status", "source_file", "line", "include_path",
            "new_target_path", "classification"
        ])

        written = 0
        for entry in broken_includes:
            if written >= args.max_errors:
                break
            writer.writerow([
                entry.get("status", "?"),
                entry["source_file"],
                entry["line"],
                entry["include_path"],
                entry.get("new_path", ""),
                entry.get("classification", "?"),
            ])
            written += 1

    elapsed = (datetime.now() - start_time).total_seconds()

    # ── Summary ───────────────────────────────────────────────────────────
    print()
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Files scanned:       {len(source_files)}")
    print(f"Files with includes: {total_checked}")
    print()
    print(f"OK includes:         {ok_count}")
    print(f"Broken (needs-review): {broken_count}")
    print(f"Dropped library ref:   {dropped_count}")
    print(f"External/system:     {external_count}")
    print(f"Total broken entries: {len(broken_includes)}")
    if len(broken_includes) > args.max_errors:
        print(f"  (showing first {args.max_errors})")
    print()

    # Top categories of broken includes
    dropped_sources = defaultdict(int)
    for entry in broken_includes:
        if entry.get("status") == "dropped-library":
            inc = entry["include_path"]
            # Get the first directory component
            parts = inc.split("/")
            if parts:
                dropped_sources[parts[0]] += 1

    if dropped_sources:
        print("Dropped library references (by library):")
        for lib, cnt in sorted(dropped_sources.items(), key=lambda x: -x[1])[:10]:
            print(f"  {lib}: {cnt} includes")
        print()

    if broken_count > 0:
        print("Sample broken includes (unmapped paths):")
        for entry in broken_includes:
            if entry.get("status") == "needs-review":
                print(f"  {entry['source_file']}:{entry['line']} -> {entry['include_path']}")
                break
        print()

    print(f"Report written to: {report_path}")
    print(f"Elapsed: {elapsed:.1f}s")
    print()


if __name__ == "__main__":
    main()
