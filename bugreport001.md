# Bug Report 001 — `share <Ident>` Regression (LocalAlloca misclassification)

**Date:** 2026-09-04
**Kain:** `0.8.0` `#1602` `f115d840` (`T:/toolchain/Kain/.kain/bin/kain.exe`)
**Severity:** High — entire CPU parallel lane (`share`/`fanout`) is dead syntax
**Status:** Reproduced, root-caused, fix guide below

---

## 1. Executive Summary

Every in-tree use of `share <ident>:` that binds a heap allocation via `let mut x = alloc_zeroed(...)` now fails typecheck with `KAIN-TYPE-0001`. This covers the canonical parallel benchmarks and smoketest:

- `benchmark/cases/contention_wall/main.kn:7` `share counter:`
- `benchmark/cases/rayon_parallel_reduce/main.kn:15` `share partials:`
- `smoketest/src/systems/share_fanout.kn:30` `share partials:`
- `benchmark/cases_v2/classic_systems.kn:101` `share counter:`
- `Plugins/Kain/KnTest/Kain/cloth_step.kn` (and any downstream solver using the same shape)

These **did pass** when authored (user benched them) — they fail on `#1602`. Root cause is a deliberate tightening of ownership inference that classifies *every* `Ident: Ptr` as `LocalAlloca` with no init-site tracking.

---

## 2. Reproduction

```bash
cd /t/toolchain/Kain
env -u PYTHONPATH PYTHONHOME="C:/scoop/apps/python312/current" ./.kain/bin/kain.exe check benchmark/cases/contention_wall/main.kn
# → error[KAIN-TYPE-0001]: share is not supported for local_alloca ownership regions

env -u PYTHONPATH PYTHONHOME="C:/scoop/apps/python312/current" ./.kain/bin/kain.exe check benchmark/cases/rayon_parallel_reduce/main.kn
# → same

env -u PYTHONPATH PYTHONHOME="C:/scoop/apps/python312/current" ./.kain/bin/kain.exe check smoketest/src/systems/share_fanout.kn
# → same
```

All three produce:

```
error[TYPE:KAIN-TYPE-0001]: share is not supported for local_alloca ownership regions:
  Stack-allocated (local) pointers cannot be shared. Share requires a heap allocation.
  Use alloc() to create a shareable pointer.
 --> <file>:<line>:5
  |     share <ident>:
  |     ^^^^^ typechecker stopped here
```

Minimal repro (`probes/probe_a.kn` in this report cycle):

```kn
use std::runtime
use std::machine
pub fn probe_a(n: Int) -> Int with Unsafe:
  let mut buf: ptr<Float> = alloc_zeroed(n, "Float")
  share buf:
    fanout w in 0..2:
      mem_store(ptr_offset(buf, w, "Float"), 1.0, "Float")
  decay buf
  return 0
# → same KAIN-TYPE-0001 on `share buf:`
```

---

## 3. Root Cause

### 3.1 Policy Table (correct)

`crates/ownership/src/lib.rs:403`

```rust
OwnershipRegionKind::LocalAlloca  → ShareMode::Unsupported  (correct — stack can't be shared)
OwnershipRegionKind::HeapAllocation → ShareMode::AtomicSeqCst (correct — heap can be shared)
OwnershipRegionKind::WorldState   → ShareMode::Unsupported  (correct — compiler-managed)
```

### 3.2 Inference (incorrect — unconditional Ident rule)

`crates/core/src/types.rs:10011` `infer_ownership_region()`

```rust
// Check 3: Direct ident — distinguish local variables from types/globals
if let Expr::Ident(name, _) = target {
    if env.lookup_type(name).is_some() {
        return OwnershipRegionKind::WorldState;
    }
    if let Some(ty) = env.lookup(name) {
        match ty {
            ResolvedType::Ptr { .. } | ResolvedType::Ref { .. } => {
                return OwnershipRegionKind::LocalAlloca; // ← BUG: unconditional
            }
            _ => {}
        }
    }
}
// Default: HeapAllocation (most permissive)
OwnershipRegionKind::HeapAllocation
```

Check 1 correctly handles `share alloc_zeroed(...):` inline. Check 3 then **unconditionally** maps *every* `Ptr` ident to `LocalAlloca` — it never asks *where that ident came from*. A `let mut buf = alloc_zeroed(...)` binding is still `LocalAlloca` by this logic, so `share buf:` is rejected despite `buf` pointing at heap.

The comment at `types.rs:9942` says the intent was tightening:

> Instead of hardcoding HeapAllocation (the most permissive policy), determine the region from the expression context

The tightening went past the corpus — the corpus was never re-checked after this change (all `share <ident>` users break).

### 3.3 Why WorldState/Imported are also blocked (by design, correct)

`WorldState` (`MyWorld.field`) and `ImportedPointer` (harness `ptr` params) are also `Unsupported` for `share` — that's intentional. The bug is specifically the `LocalAlloca` bucket being too wide: it conflates *stack* allocas with *heap-backed locals*.

---

## 4. Why This Matters

- `share`/`fanout` is the **only** CPU data-parallel construct (`OWNERSHIP.MD:48` `Idle → Shared`, `fanout` lanes with `atomic_store`).
- Markscript (`blades/markscript/src/`) uses **zero** `share` — the flagship shipped app is single-threaded, so the regression had no in-tree canary.
- Cloth solver (`OutfitStudioCloth` / `KnTest/cloth_step.kn`) is blocked from CPU parallel without this — the current workaround is serial `collapse` (1.05× vs raw C++ at 100×100, 1.05× at 200×200) with GPU `shader compute` as the bypass.

---

## 5. Fix Guide

### 5.1 Goal

`share <ident>:` should succeed iff that ident is **heap-backed** (`alloc`/`alloc_zeroed`/`realloc_mem` init-site, or provably heap-propagated). Stack allocas stay rejected.

### 5.2 Recommended Fix — Init-Site Tracking

Track heap-alloc idents at `let` binding time, consult in `infer_ownership_region`.

**Option A — Minimal (single-file, types.rs only):**

1. Introduce a `HashSet<String>` (or `HashMap<String, OwnershipRegionKind>`) on `TypeEnv` / `TypeCheckCtx` — e.g. `heap_idents: HashSet<String>`.
2. In the `let`/`let mut` handling (where `is_alloc_call` is already available for the initializer), if `init` is `alloc`/`alloc_zeroed`/`realloc_mem`, insert the bound name into `heap_idents`.
3. In `infer_ownership_region` Check 3, before returning `LocalAlloca`, consult:
   ```rust
   if env.is_heap_ident(name) {
       return OwnershipRegionKind::HeapAllocation;
   }
   ```
4. Propagate through trivial aliases if desired (`let b = a` where `a` is heap → `b` is heap). Single-level is enough for the corpus.

**Option B — Dataflow (stronger):**

Walk the HIR to mark any `Ident` whose reaching definition is a heap alloc (handles reassigns, branches). Heavier, but precise for advanced patterns.

**Option C — Revert (quickest, permissive):**

Change Check 3's `LocalAlloca` arm to fall through to the `HeapAllocation` default (i.e. make `Ident: Ptr` permissive again). Restores the corpus in one line but re-opens the original tightening intent — use as a hotfix if Option A needs more time.

Recommended: **Option A** — small blast radius, no new passes, matches the existing `is_alloc_call` helper.

### 5.3 Code Pointers

- Inference: `crates/core/src/types.rs:10011` `infer_ownership_region`, `10057` `is_alloc_call`
- Policy: `crates/ownership/src/lib.rs:403` `OWNERSHIP_POLICY_TABLE`, `27` `OwnershipRegionKind`
- Let handling: `crates/core/src/types.rs` `typecheck_let` / `infer_let` (where `env.lookup`/`env.insert` happens — add `heap_idents` insert there)
- Tests to update: add a positive `share <heap-ident>` test and a negative `share <stack-ident>` test in `crates/core/tests/` or `crates/ownership/src/lib.rs:620` `lowering_hints_match_region_policy` family

### 5.4 Verification

After fix, all of these must flip to `Check passed: 1/1`:

```bash
./.kain/bin/kain.exe check benchmark/cases/contention_wall/main.kn
./.kain/bin/kain.exe check benchmark/cases/rayon_parallel_reduce/main.kn
./.kain/bin/kain.exe check smoketest/src/systems/share_fanout.kn
./.kain/bin/kain.exe check benchmark/cases_v2/classic_systems.kn   # if checked file-wise
./.kain/bin/kain.exe check Plugins/Kain/KnTest/Kain/cloth_step.kn  # after re-enabling share in integrate
```

Negative check (should still fail):

```kn
pub fn neg(n: Int) -> Int with Unsafe:
  var x: Int = 42
  let p: ptr<Int> = ptr_offset(addr_of(x), 0, "Int") // stack-derived, not heap
  share p:
    fanout w in 0..2: atomic_add(p, 1)
  return 0
# → must remain KAIN-TYPE-0001
```

Benchmark sanity (existing `KnTest` harness):

```
GW=100  kain ~35ms  raw ~37ms  maxDiff 0.0
GW=200  kain ~139ms raw ~145ms maxDiff 0.0
# With share re-enabled, expect GW=200 to drop noticeably (parallel integrate)
```

### 5.5 Secondary Findings (not blocking, noted)

- **Native link lane** — fixed in Sep 4 Bazel build (`target-triple` system-lib tables now emit `Ws2_32`, `Winhttp`, `Shell32`, `User32`, `Advapi32`, `Ole32`, `Uuid`, `Winmm`, `Dbghelp`). Driver lane `--emit shared-lib`/`exe` is now primary; manual `clang -shared ... -lws2_32 ...` is fallback.
- **Embedded Python path** — `kain.exe` embeds `F:\Scoop\...` python paths from the build host; on machines where Python lives at `C:\scoop\...` or `T:\Quin\...` env, `PYTHONPATH` poisoning breaks `encodings` import. Fixed by `env -u PYTHONPATH PYTHONHOME="C:/scoop/apps/python312/current"` for now; long-term `kain_toolchain.py` / `PYTHONHOME` guidance in README covers it.
- **Install sync** — fresh `T:/toolchain/Kain/.kain/bin/` shipped without companion DLLs (`python312.dll`, `libclang.dll`, VCRTs); staged by hand from `C:/Users/zenta/.kain/bin/` to make `kain.exe` launch. Worth folding into the install step.

---

## 6. References

- Policy table: `crates/ownership/src/lib.rs:16` `OwnershipRegionKind` enum, `403` table
- Inference: `crates/core/src/types.rs:9938` ownership gate, `10011` region inference
- Docs: `docs/OWNERSHIP.MD:48` share/fanout lifecycle, `CATALOG.md:110` keyword surface
- Corpus hits: `benchmark/cases/contention_wall/main.kn`, `benchmark/cases/rayon_parallel_reduce/main.kn`, `smoketest/src/systems/share_fanout.kn`, `benchmark/cases_v2/classic_systems.kn:101`
- Build: `T:/toolchain/Kain/.kain/bin/kain.exe` `#1602`, `crates/build/src/native_link.rs:311` link libs, `kain_toolchain.py`

---

*End of report — specialized agents: Option A is ~20 lines in `types.rs` + env set. Probe with `probe_a.kn` above for the tightest loop.*
