#!/usr/bin/env python3
"""
analyze-current-tree.py — Phase 5 Layout Analysis
==================================================
SAFE MODE: No files will be modified.

Scans the current X:/llvm-project/ tree and produces a structured report:
- Total directory count and file count
- Size breakdown by area (llvm, clang, lld, compiler-rt)
- Lists all subdirectories at depth 2-3
- Identifies files that would need to move under the new layout
- Outputs a JSON report

Usage:
    python analyze-current-tree.py [--output-dir DIR] [--verbose]
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path

# ─── Configuration ───────────────────────────────────────────────────────────

LLVM_PROJECT_ROOT = Path("X:/llvm-project")

# Areas with human-readable labels and subdirectory patterns
AREA_CONFIG = {
    "llvm-core-lib": {
        "label": "LLVM Core Libraries (lib/)",
        "paths": ["llvm/lib/IR", "llvm/lib/IRReader", "llvm/lib/IRPrinter",
                  "llvm/lib/AsmParser", "llvm/lib/Bitcode", "llvm/lib/Bitstream",
                  "llvm/lib/Analysis", "llvm/lib/Transforms", "llvm/lib/CodeGen",
                  "llvm/lib/MC", "llvm/lib/Object", "llvm/lib/BinaryFormat",
                  "llvm/lib/Passes", "llvm/lib/Linker", "llvm/lib/Option",
                  "llvm/lib/ProfileData", "llvm/lib/Remarks",
                  "llvm/lib/Demangle", "llvm/lib/CodeGenTypes",
                  "llvm/lib/DWARFCFIChecker", "llvm/lib/DTLTO",
                  "llvm/lib/TableGen"],
    },
    "llvm-support": {
        "label": "LLVM Support",
        "paths": ["llvm/lib/Support", "llvm/lib/TargetParser"],
    },
    "llvm-targets": {
        "label": "LLVM Targets",
        "paths": ["llvm/lib/Target/X86", "llvm/lib/Target/AArch64"],
    },
    "llvm-jit": {
        "label": "LLVM JIT (ExecutionEngine)",
        "paths": ["llvm/lib/ExecutionEngine",
                  "llvm/lib/ExecutionEngine/Orc",
                  "llvm/lib/ExecutionEngine/JITLink",
                  "llvm/lib/ExecutionEngine/Interpreter"],
    },
    "llvm-include": {
        "label": "LLVM Include Headers",
        "paths": ["llvm/include/llvm", "llvm/include/llvm-c"],
    },
    "clang-lib": {
        "label": "Clang Libraries",
        "paths": ["clang/lib"],
    },
    "clang-include": {
        "label": "Clang Include Headers",
        "paths": ["clang/include"],
    },
    "lld": {
        "label": "LLD (Linker)",
        "paths": ["lld"],
    },
    "compiler-rt": {
        "label": "compiler-rt",
        "paths": ["compiler-rt/lib"],
    },
    "cmake": {
        "label": "CMake modules",
        "paths": ["cmake"],
    },
    "utils": {
        "label": "Utilities",
        "paths": ["utils"],
    },
    "runtimes": {
        "label": "Runtimes",
        "paths": ["runtimes"],
    },
}

# File extensions that count as "source" for KEEP areas
SOURCE_EXTENSIONS = {".cpp", ".c", ".h", ".hpp", ".def", ".td", ".inc", ".ll"}


def scan_directory(base_path: Path, max_depth: int = 3) -> dict:
    """
    Recursively scan a directory and return statistics.
    Returns: {
        'dirs': int,
        'files': int,
        'size_bytes': int,
        'size_mb': float,
        'source_files': int,
        'directories': [str],  # subdir names at current level
    }
    """
    if not base_path.exists():
        return {"dirs": 0, "files": 0, "size_bytes": 0, "size_mb": 0,
                "source_files": 0, "directories": []}

    total_dirs = 0
    total_files = 0
    total_size = 0
    source_files = 0
    subdirs = set()
    dirs_at_depth = {}

    for root, dirs, files in os.walk(str(base_path)):
        # Skip hidden directories
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        rel_root = os.path.relpath(root, str(base_path))
        depth = 0 if rel_root == "." else len(Path(rel_root).parts)

        # Track directories at each depth for reporting
        if depth <= max_depth:
            subdir_names = [os.path.join(rel_root, d) if rel_root != "." else d
                           for d in dirs]
            subdirs.update(subdir_names)

        total_dirs += len(dirs)
        total_files += len(files)
        for f in files:
            try:
                fp = os.path.join(root, f)
                total_size += os.path.getsize(fp)
                ext = os.path.splitext(f)[1].lower()
                if ext in SOURCE_EXTENSIONS:
                    source_files += 1
            except (OSError, FileNotFoundError):
                pass

    return {
        "dirs": total_dirs,
        "files": total_files,
        "size_bytes": total_size,
        "size_mb": round(total_size / (1024 * 1024), 2),
        "source_files": source_files,
        "directory_count": total_dirs,
    }


def scan_area(rel_path: str) -> dict:
    """Scan a single area path and return stats."""
    full_path = LLVM_PROJECT_ROOT / rel_path
    return {
        "path": rel_path,
        "full_path": str(full_path.resolve()),
        "exists": full_path.exists(),
        **scan_directory(full_path, max_depth=4),
    }


def list_interesting_dirs(base_path: Path, max_depth: int = 3) -> list:
    """List all subdirectories at depth 2-3 under a base path."""
    results = []
    if not base_path.exists():
        return results
    for root, dirs, _ in os.walk(str(base_path)):
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        rel = os.path.relpath(root, str(base_path))
        if rel == ".":
            depth = 0
        else:
            depth = len(Path(rel).parts)
        if 2 <= depth <= max_depth:
            results.append({
                "path": rel,
                "depth": depth,
            })
    # Sort by depth then path
    results.sort(key=lambda x: (x["depth"], x["path"]))
    return results


def main():
    parser = argparse.ArgumentParser(
        description="Phase 5: Analyze current LLVM tree structure (SAFE MODE)",
    )
    parser.add_argument("--output-dir", type=str,
                        default="scripts/phase5-layout/reports",
                        help="Output directory for reports")
    parser.add_argument("--verbose", action="store_true",
                        help="Print detailed progress")
    args = parser.parse_args()

    print("=" * 70)
    print("PHASE 5 LAYOUT ANALYSIS — analyze-current-tree.py")
    print("SAFE MODE: No files will be modified.")
    print("=" * 70)
    print()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    start_time = datetime.now()
    if args.verbose:
        print(f"Starting scan at {start_time}")
        print(f"Root: {LLVM_PROJECT_ROOT}")
        print()

    # ── 1. Overall tree stats ───────────────────────────────────────────────
    if args.verbose:
        print("Scanning overall tree...")
    overall = scan_directory(LLVM_PROJECT_ROOT, max_depth=5)

    # ── 2. Per-area stats ───────────────────────────────────────────────────
    if args.verbose:
        print("Scanning per-area breakdown...")
    areas = {}
    for area_key, area_cfg in AREA_CONFIG.items():
        if args.verbose:
            print(f"  {area_cfg['label']}...")
        area_stats = {"label": area_cfg["label"], "sub_areas": []}
        for sub_path in area_cfg["paths"]:
            sub_stats = scan_area(sub_path)
            area_stats["sub_areas"].append(sub_stats)
        # Aggregate
        total_dirs = sum(s["dirs"] for s in area_stats["sub_areas"])
        total_files = sum(s["files"] for s in area_stats["sub_areas"])
        total_size = sum(s["size_bytes"] for s in area_stats["sub_areas"])
        total_source = sum(s["source_files"] for s in area_stats["sub_areas"])
        area_stats["dirs"] = total_dirs
        area_stats["files"] = total_files
        area_stats["size_bytes"] = total_size
        area_stats["size_mb"] = round(total_size / (1024 * 1024), 2)
        area_stats["source_files"] = total_source
        areas[area_key] = area_stats

    # ── 3. Interesting directories (depth 2-3 in key areas) ─────────────────
    if args.verbose:
        print("Listing interesting subdirectories...")
    interesting = {
        "llvm-lib": list_interesting_dirs(LLVM_PROJECT_ROOT / "llvm/lib", max_depth=3),
        "llvm-include": list_interesting_dirs(LLVM_PROJECT_ROOT / "llvm/include", max_depth=3),
        "clang-lib": list_interesting_dirs(LLVM_PROJECT_ROOT / "clang/lib", max_depth=3),
        "clang-include": list_interesting_dirs(LLVM_PROJECT_ROOT / "clang/include", max_depth=3),
        "lld": list_interesting_dirs(LLVM_PROJECT_ROOT / "lld", max_depth=3),
    }

    # ── 4. File counts by language ──────────────────────────────────────────
    if args.verbose:
        print("Counting source files by extension...")
    ext_counts = {}
    for root, _, files in os.walk(str(LLVM_PROJECT_ROOT)):
        # Skip hidden dirs
        parts = root.replace("\\", "/").split("/")
        if any(p.startswith(".") for p in parts):
            continue
        for f in files:
            ext = os.path.splitext(f)[1].lower()
            if ext:
                ext_counts[ext] = ext_counts.get(ext, 0) + 1

    # Sort by count descending
    ext_sorted = sorted(ext_counts.items(), key=lambda x: -x[1])

    # ── 5. Estimate move requirements ───────────────────────────────────────
    # Files that would need to move under new layout (heuristic: all kept files)
    # Files in llvm/lib/, llvm/include/, clang/lib/, clang/include/,
    # compiler-rt/lib/builtins, lld/ -> all will move
    if args.verbose:
        print("Estimating files to move...")
    move_areas = [
        ("llvm-lib", LLVM_PROJECT_ROOT / "llvm/lib"),
        ("llvm-include", LLVM_PROJECT_ROOT / "llvm/include"),
        ("clang-lib", LLVM_PROJECT_ROOT / "clang/lib"),
        ("clang-include", LLVM_PROJECT_ROOT / "clang/include"),
        ("compiler-rt-builtins", LLVM_PROJECT_ROOT / "compiler-rt/lib/builtins"),
        ("lld", LLVM_PROJECT_ROOT / "lld"),
    ]
    move_estimate = {}
    for area_name, area_path in move_areas:
        stats = scan_directory(area_path, max_depth=5)
        move_estimate[area_name] = {
            "path": str(area_path),
            "files": stats["files"],
            "dirs": stats["dirs"],
            "size_mb": stats["size_mb"],
        }

    # ── Build report ────────────────────────────────────────────────────────
    report = {
        "meta": {
            "tool": "analyze-current-tree.py",
            "timestamp": datetime.now().isoformat(),
            "root": str(LLVM_PROJECT_ROOT.resolve()),
        },
        "overall": {
            "dirs": overall["dirs"],
            "files": overall["files"],
            "size_bytes": overall["size_bytes"],
            "size_mb": overall["size_mb"],
            "source_files": overall["source_files"],
        },
        "areas": areas,
        "interesting_directories": interesting,
        "file_extensions": ext_sorted,
        "move_estimate": move_estimate,
    }

    # ── Write report ────────────────────────────────────────────────────────
    report_path = output_dir / "current-tree.json"
    with open(str(report_path), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, default=str)

    elapsed = (datetime.now() - start_time).total_seconds()

    # ── Print summary ───────────────────────────────────────────────────────
    print()
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"Total dirs:    {overall['dirs']}")
    print(f"Total files:   {overall['files']}")
    print(f"Total size:    {overall['size_mb']:.1f} MB ({overall['size_bytes']:,} bytes)")
    print(f"Source files:  {overall['source_files']}")
    print()
    print("Per-area breakdown:")
    for area_key, area_stats in sorted(areas.items()):
        label = area_stats["label"]
        mb = area_stats["size_mb"]
        files = area_stats["files"]
        src = area_stats["source_files"]
        dirs = area_stats["dirs"]
        print(f"  {label:40s} {mb:>8.1f} MB  {files:>6d} files  {src:>5d} src  {dirs:>4d} dirs")
    print()
    print("Files requiring movement (new layout):")
    for area_name, est in sorted(move_estimate.items()):
        print(f"  {area_name:30s} {est['files']:>6d} files, {est['dirs']:>4d} dirs, {est['size_mb']:>6.1f} MB")
    print()
    print(f"Top 10 file extensions:")
    for ext, count in ext_sorted[:10]:
        print(f"  {ext:10s} {count:>6d}")
    print()
    print(f"Report written to: {report_path}")
    print(f"Elapsed: {elapsed:.1f}s")
    print()


if __name__ == "__main__":
    main()
