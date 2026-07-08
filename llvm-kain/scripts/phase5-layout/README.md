# Phase 5: Clean Flat Layout Restructure

## Purpose

This directory contains safe, read-only analysis scripts for planning **Phase 5** of the LLVM vendor refactor: restructuring the current LLVM tree into the clean flat layout proposed in `LLVM_REFACTOR.md` (Section 3).

## Status

| Item | Status |
|------|--------|
| Phases 1-4 complete | ✅ Dead targets, Clang C-only, dead libs, dead projects all stripped |
| Current tree size | ~351 MB, 485 directories, 6,311 files |
| Phase 5 target | Flat layout with 3-level max depth |
| **These scripts** | **READ-ONLY** — they analyze, plan, and report. Nothing modifies the tree. |

## Proposed Layout

The target is a flat `llvm-kain/` tree modeled after Kain's own clean directory convention:

```
llvm-kain/
├── src/
│   ├── core/ir/              # llvm/lib/IR/
│   ├── core/passes/          # llvm/lib/Transforms/ (kept passes)
│   ├── core/analysis/        # llvm/lib/Analysis/
│   ├── core/support/         # llvm/lib/Support/ (kept parts)
│   ├── target/x86/           # llvm/lib/Target/X86/
│   ├── target/aarch64/       # llvm/lib/Target/AArch64/
│   ├── target/shared/        # llvm/lib/CodeGen/ (shared)
│   ├── jit/orc/              # llvm/lib/ExecutionEngine/Orc/
│   ├── jit/jitlink/          # llvm/lib/ExecutionEngine/JITLink/
│   └── support/              # ADT, math, debug, target triple
├── include/                  # Mirrors src/ structure
├── clang/src/{lex,parse,sema,ast,codegen}/
├── clang/include/
├── rt/builtins/             # compiler-rt/lib/builtins/
├── tools/{llc,opt}
├── __REFACTOR__/            # planning docs (kept)
└── __ORIGINAL__/            # symlink for diffing
```

## Scripts

| Script | Output | Purpose |
|--------|--------|---------|
| `analyze-current-tree.py` | `reports/current-tree.json` | Scans current tree structure, sizes, counts |
| `map-file-movements.py` | `reports/file-movements.tsv` | Maps every file to its proposed new location |
| `verify-dependencies.py` | `reports/broken-includes.tsv` | Checks include paths that would break |
| `generate-phase5-report.py` | `reports/phase5-summary.md` | Master summary from all other scripts |

## Running

```bash
cd X:/llvm-project

# Analyze current tree (fast)
python scripts/phase5-layout/analyze-current-tree.py --verbose

# Map file movements (slow — scans every file)
python scripts/phase5-layout/map-file-movements.py --verbose

# Verify include dependencies (slow — scans every .cpp/.h/.c)
python scripts/phase5-layout/verify-dependencies.py --verbose

# Generate summary
python scripts/phase5-layout/generate-phase5-report.py --verbose
```

All scripts accept:
- `--output-dir` : output directory (default: scripts/phase5-layout/reports/)
- `--verbose` : detailed logging to stderr
- `--dry-run` : always enabled (scripts are always read-only)

## SAFETY

**CRITICAL: These scripts NEVER modify files.** They only:
1. Read file metadata (stat, size, name)
2. Read file contents (for include scanning)
3. Write reports to the output directory

No files are moved, deleted, or altered.
