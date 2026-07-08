# Phase 5: Clean Flat Layout Restructure — Summary Report

**Generated:** 2026-06-28 06:05:01
**Source tree:** `X:\llvm-project`
**Target tree:** `llvm-kain/`

---

## 1. Executive Summary

| Metric | Value |
|--------|-------|
| Current tree size | 135.1 MB |
| Current directories | 487 |
| Current files | 6387 |
| Files to move | 5605 |
| Files staying in place | 46 |
| Include paths that can be auto-mapped | 37,622 |
| Include paths needing manual review | 999 |
| References to dropped libraries | 49 |
| Estimated Phase 5 duration | 1-2 weeks |
| Risk level | **MEDIUM** — bulk file move + include path rewrite |

---

## 2. Current Tree Structure

Current root: `X:\llvm-project` (135.1 MB, 487 dirs, 6387 files)

### 2.1 Per-Area Breakdown

| Area | Size (MB) | Files | Source files | Dirs |
|------|-----------|-------|-------------|------|
| Clang Include Headers | 13.3 | 628 | 616 | 24 |
| Clang Libraries | 22.8 | 585 | 552 | 26 |
| CMake modules | 0.0 | 17 | 0 | 1 |
| compiler-rt | 1.3 | 493 | 274 | 19 |
| LLD (Linker) | 3.5 | 212 | 198 | 9 |
| LLVM Core Libraries (lib/) | 40.5 | 1140 | 1101 | 19 |
| LLVM Include Headers | 20.3 | 1815 | 1796 | 68 |
| LLVM JIT (ExecutionEngine) | 3.3 | 304 | 290 | 10 |
| LLVM Support | 2.6 | 220 | 216 | 4 |
| LLVM Targets | 21.1 | 392 | 377 | 11 |
| Runtimes | 0.0 | 12 | 0 | 3 |
| Utilities | 1.8 | 187 | 5 | 176 |

### 2.2 LLVM lib/ Subdirectories

**clang-include** (20 subdirectories at depth 2-3):
  - `clang\AST/`
  - `clang\ASTMatchers/`
  - `clang\Analysis/`
  - `clang\Basic/`
  - `clang\CodeGen/`
  - `clang\Config/`
  - `clang\Edit/`
  - `clang\Frontend/`
  - `clang\FrontendTool/`
  - `clang\Lex/`
  - `clang\Options/`
  - `clang\Parse/`
  - `clang\Sema/`
  - `clang\Serialization/`
  - `clang\ASTMatchers\Dynamic/`
  - *... and 5 more*

**clang-lib** (12 subdirectories at depth 2-3):
  - `ASTMatchers\Dynamic/`
  - `Analysis\FlowSensitive/`
  - `Analysis\LifetimeSafety/`
  - `Analysis\plugins/`
  - `Basic\Targets/`
  - `CodeGen\TargetBuiltins/`
  - `CodeGen\Targets/`
  - `Frontend\Rewrite/`
  - `Analysis\FlowSensitive\Models/`
  - `Analysis\plugins\CheckerDependencyHandling/`
  - `Analysis\plugins\CheckerOptionHandling/`
  - `Analysis\plugins\SampleAnalyzer/`

**lld** (4 subdirectories at depth 2-3):
  - `ELF\Arch/`
  - `MachO\Arch/`
  - `include\lld/`
  - `include\lld\Common/`

**llvm-include** (61 subdirectories at depth 2-3):
  - `llvm-c\Transforms/`
  - `llvm\ABI/`
  - `llvm\ADT/`
  - `llvm\Analysis/`
  - `llvm\AsmParser/`
  - `llvm\BinaryFormat/`
  - `llvm\Bitcode/`
  - `llvm\Bitstream/`
  - `llvm\CodeGen/`
  - `llvm\CodeGenTypes/`
  - `llvm\Config/`
  - `llvm\DTLTO/`
  - `llvm\DWARFCFIChecker/`
  - `llvm\Demangle/`
  - `llvm\ExecutionEngine/`
  - *... and 46 more*

**llvm-lib** (42 subdirectories at depth 2-3):
  - `Bitcode\Reader/`
  - `Bitstream\Reader/`
  - `CodeGen\AsmPrinter/`
  - `CodeGen\GlobalISel/`
  - `CodeGen\LiveDebugValues/`
  - `CodeGen\MIRParser/`
  - `CodeGen\SelectionDAG/`
  - `ExecutionEngine\IntelJITProfiling/`
  - `ExecutionEngine\Interpreter/`
  - `ExecutionEngine\JITLink/`
  - `ExecutionEngine\Orc/`
  - `MC\MCDisassembler/`
  - `MC\MCParser/`
  - `ProfileData\Coverage/`
  - `Support\Unix/`
  - *... and 27 more*

---

## 3. File Movement Plan

**Total files to move:** 5605

### 3.1 By Area

| Area | Files | Total Size |
|------|-------|------------|
| clang | 1,212 | 36.0 MB |
| include | 1,810 | 20.2 MB |
| lld | 209 | 3.5 MB |
| llvm-core | 771 | 26.4 MB |
| llvm-jit | 158 | 1.7 MB |
| llvm-support | 220 | 2.6 MB |
| llvm-target | 762 | 35.0 MB |
| rt | 492 | 1.3 MB |
| tools | 17 | 0.4 MB |

### 3.2 Movement Command Estimate

Using git-mv or robocopy, the estimated command volume:

- 5605 individual file move operations
- ~12 batch move commands (at ~500 files/cmd)
- 9 area directories to create first

### 3.3 Target Layout Summary

| Source | Target |
|--------|--------|
| `llvm/lib/IR/` | `llvm-kain/src/core/ir/` |
| `llvm/lib/Analysis/` | `llvm-kain/src/core/analysis/` |
| `llvm/lib/Transforms/` | `llvm-kain/src/core/passes/` |
| `llvm/lib/CodeGen/` | `llvm-kain/src/target/shared/codegen/` |
| `llvm/lib/MC/` | `llvm-kain/src/core/mc/` |
| `llvm/lib/Object/` | `llvm-kain/src/core/object/` |
| `llvm/lib/Support/` | `llvm-kain/src/support/adt/` |
| `llvm/lib/Target/X86/` | `llvm-kain/src/target/x86/` |
| `llvm/lib/Target/AArch64/` | `llvm-kain/src/target/aarch64/` |
| `llvm/lib/ExecutionEngine/Orc/` | `llvm-kain/src/jit/orc/` |
| `llvm/lib/ExecutionEngine/JITLink/` | `llvm-kain/src/jit/jitlink/` |
| `llvm/include/llvm/` | `llvm-kain/include/` |
| `llvm/include/llvm-c/` | `llvm-kain/include/c-api/` |
| `clang/lib/` | `llvm-kain/clang/src/` |
| `clang/include/clang/` | `llvm-kain/clang/include/` |
| `compiler-rt/lib/builtins/` | `llvm-kain/rt/builtins/` |
| `lld/` | `llvm-kain/lld/` |

---

## 4. Include Dependency Analysis

### 4.1 DROPPED Library References (CRITICAL)

**49 include references** point to headers from libraries that have been deleted in Phases 1-3.
These will cause build failures and must be removed or guarded:

| Dropped Path | Ref Count |
|-------------|----------|
| `clang/StaticAnalyzer/` | 24 |
| `clang/Rewrite/` | 19 |
| `clang/APINotes/` | 2 |
| `clang/CIR/` | 2 |
| `clang/Driver/` | 1 |
| `clang/ExtractAPI/` | 1 |

*These are from dead libraries (Driver, Format, Tooling, StaticAnalyzer, etc.) that should already be excluded. If your Phase 2/3 deletions left code referencing them, these will need patching.*

### 4.2 Unmapped Include Paths (NEEDS REVIEW)

**451 include paths** could not be automatically mapped to the new layout.
These are typically new or unclassified include patterns that need manual inspection:

Sample:
  - `clang/include/clang/AST/Attr.h` : clang/Support/Compiler.h
  - `clang/include/clang/AST/HLSLResource.h` : clang/Support/Compiler.h
  - `clang/include/clang/ASTMatchers/ASTMatchersMacros.h` : clang/Support/Compiler.h
  - `clang/include/clang/Analysis/FlowSensitive/NoopLattice.h` : clang/Support/Compiler.h
  - `clang/include/clang/Basic/ParsedAttrInfo.h` : clang/Support/Compiler.h

### 4.3 Cross-Area Dependencies

| Dependency | Count | Notes |
|------------|-------|-------|
| lld -> llvm | 30 | Expected — LLD links LLVM libraries |
| clang -> llvm | 5 | Expected — Clang codegen depends on LLVM IR |

---

## 5. Execution Plan — Order of Operations

Phase 5 should be executed in this order to minimize conflicts:

1. **Phase 5a: Move LLVM Support (llvm/lib/Support/ -> src/support/)**
2. **Phase 5b: Move LLVM Core (llvm/lib/IR/, Analysis/, Transforms/, etc. -> src/core/)**
3. **Phase 5c: Move LLVM Targets (llvm/lib/Target/X86+AArch64 -> src/target/)**
4. **Phase 5d: Move LLVM JIT (llvm/lib/ExecutionEngine/ -> src/jit/)**
5. **Phase 5e: Move LLVM Includes (llvm/include/ -> include/)**
6. **Phase 5f: Move Clang (clang/lib/, clang/include/ -> clang/src/, clang/include/)**
7. **Phase 5g: Move LLD (lld/ -> lld/)**
8. **Phase 5h: Move compiler-rt builtins (-> rt/builtins/)**
9. **Phase 5i: Move LLVM includes (llvm/include/llvm-c/ -> include/c-api/)**
10. **Phase 5j: Generate __ORIGINAL__ symlink**
11. **Phase 5k: Update CMakeLists.txt / build.kn for new paths**
12. **Phase 5l: Fix all broken include paths (~search-and-replace across all sources)**

### 5.1 Critical Path

1. **Create target directory skeleton** — all new directories under `llvm-kain/`
2. **Move independent areas first** (no cross-dependencies):
   - compiler-rt builtins → `rt/builtins/`
   - LLD → `lld/`
3. **Move support infrastructure**:
   - LLVM Support → `src/support/adt/`
   - LLVM TargetParser → `src/support/target/`
4. **Move LLVM core** (requires support):
   - IR, Analysis, Transforms, MC, Object, BinaryFormat → `src/core/`
5. **Move LLVM targets** (requires core + CodeGen):
   - X86 → `src/target/x86/`
   - AArch64 → `src/target/aarch64/`
   - CodeGen shared → `src/target/shared/`
6. **Move includes** (mirror source structure):
   - `llvm/include/llvm/` → `include/`
7. **Move Clang** (requires LLVM includes):
   - `clang/lib/` → `clang/src/`
   - `clang/include/` → `clang/include/`
8. **Global include path rewrite**
9. **Build verification**

---

## 6. Risk Assessment

- **HIGH**: 49 include references point to dropped libraries. Source files referencing dropped headers will fail to compile until those references are removed or the dropped header is recovered.
- **INFO**: 30 LLD-to-LLVM deps — expected, LLD links against LLVM.
- **MEDIUM**: 5605 files to move — large bulk operation. Use git-mv or robocopy with logging.
- **LOW**: All scripts are analysis-only. No files have been modified.
- **NOTE**: After moving, every #include directive needs path rewriting (approximately one sed/awk pass per area).

### 6.1 Risk Matrix

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Broken include paths | HIGH | HIGH | Pre-scan with verify-dependencies.py; prepare sed script for bulk rewrite |
| File move conflicts | MEDIUM | HIGH | Use `git mv` not `os.rename`; commit in batches |
| Clang LLVM IR codegen references | MEDIUM | MEDIUM | Verify clang CodeGen references use updated paths |
| Missed CMakeLists.txt targets | MEDIUM | HIGH | Single flat CMakeLists.txt avoids recursive build issues |
| LLD dependency on LLVM | LOW | MEDIUM | LLD is small; verify after move |
| File permission loss | LOW | LOW | Use git mv preserves permissions |

---

## 7. Time Estimate

| Phase | Estimated Duration | Details |
|-------|-------------------|---------|
| Setup & directory creation | 1 hour | Create all target directories under llvm-kain/ |
| File moves (bulk) | 4-8 hours | `git mv` in batches of 500-1000 files |
| Include path rewrite | 4-8 hours | sed/awk bulk rewrite, then fix remaining broken refs |
| CMakeLists.txt adaptation | 2-4 hours | Single flat CMakeLists.txt or build.kn |
| Build verification | 2-4 hours | Iterative build-fix cycle |
| Testing | 2-4 hours | Verify llc + opt + clang work correctly |
| **Total** | **15-29 hours** | **~1-2 weeks at 2-4 hrs/day** |

---

## 8. Tools & Commands

### Creating directories (PowerShell)
```powershell
# Create all target directories at once
$targets = @(
    'llvm-kain/clang/',
    'llvm-kain/include/',
    'llvm-kain/lld/',
    'llvm-kain/llvm-core/',
    'llvm-kain/llvm-jit/',
    'llvm-kain/llvm-support/',
    'llvm-kain/llvm-target/',
    'llvm-kain/rt/',
    'llvm-kain/tools/',
)
foreach ($t in $targets) { New-Item -ItemType Directory -Force -Path $t }
```

### Bulk file move (PowerShell)
```powershell
# Generate git-mv commands from file-movements.tsv for each area
# Example for one area:
Get-Content reports/file-movements.tsv | Select-Object -Skip 1 | ConvertFrom-Csv -Delimiter "`t" | Where-Object { $_.action -eq 'move' -and $_.area -eq 'llvm-core' } | ForEach-Object { git mv $_.current_path $_.target_path }
```

### Include path bulk rewrite (PowerShell)
```powershell
# For llvm-backed includes (<llvm/...> -> <core/...> etc)
# See verify-dependencies.py output for exact mappings
Get-ChildItem -Recurse -Filter *.cpp,*.h,*.c,*.hpp | ForEach-Object {
    (Get-Content $_.FullName) -replace 'llvm/IR/', 'core/ir/' -replace 'llvm/Analysis/', 'core/analysis/' | Set-Content $_.FullName
}
```

---

## 9. Post-Move Verification Checklist

- [ ] `llvm-kain/` directory structure matches proposed layout
- [ ] All files moved (no stragglers in old paths)
- [ ] `__ORIGINAL__` symlink points to `X:/llvm-project/`
- [ ] All include paths updated (verify-dependencies.py reports 0 broken)
- [ ] Single CMakeLists.txt or build.kn compiles all targets
- [ ] `llvm-kain> cmake -DLLVM_TARGETS_TO_BUILD=X86;AArch64` succeeds
- [ ] `llc --version` shows only X86 + AArch64 targets
- [ ] Clang compiles a simple C file
- [ ] `opt` runs optimization passes without errors

---

## 10. Raw Data Sources

| File | Description |
|------|-------------|
| `current-tree.json` | Full JSON scan of current tree structure |
| `file-movements.tsv` | Every file mapped to its target location |
| `broken-includes.tsv` | Include paths that need updating |
| `phase5-summary.md` | This document |
