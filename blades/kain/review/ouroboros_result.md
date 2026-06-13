
--- Phase 1: Combined Source Assembly ---
  PASS: 23 files, 15001 lines, 636.7 KB combined
# Ouroboros Verification Report

Date: 2026-06-12 21:53:40
Project: kainc (Kain Self-Host Compiler)
Source files: 23
Total source lines: 15001
Combined source: X:\blades\kain\combined\kainc_bootstrap.kn / 636.7 KB


--- Phase 2: Rust Bootstrap (Stage 0 -> Stage 1) ---
  Compiler: X:\.kain\bin\kain.exe
  FAIL: kain build exited with code 1
     Build failed: command failed: project build failed (1 task(s) failed); full report at \\?\X:\blades\kain\.kain\reports\build\session-1781315620794-30612.json

  NOTE: Combined source fails because llvm_ffi.kn has an
  unresolved 'include <llvm-c/Core.h>' statement.
  Individual files pass check (21/23) when compiled via build.kn.

--- Phase 3: Self-Compile (Stage 1 -> Stage 2) ---
  SKIP: No Stage 1 binary available.

  OUROBOROS NOT READY: Cannot self-compile without Stage 1 binary.

--- Phase 4: Verification ---
  SKIP: No comparable artifacts available.

  OUROBOROS NOT READY.
  Reason: Self-host compiler has stub typechecker and codegen.

## Phase Results

| Phase | Status | Duration |
|-------|--------|----------|
| Phase 1 (Combine) | PASS | 0.1s |
| Phase 2 (Stage 0->1) | FAIL | 22.5s |
| Phase 3 (Stage 1->2) | SKIP | 0.0s |
| Phase 4 (Verify) | SKIP | 0.0s |

## Artifacts

  Combined source: X:\blades\kain\combined\kainc_bootstrap.kn / 636.7 KB / 651956 bytes

Total duration: 22.8s

