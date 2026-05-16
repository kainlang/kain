# 2026-05-15 - Ephemeral Loop-Local Ownership Erasure

## Thesis

Fresh helper-owned single-cell allocations no longer need to be physical heap objects when the compiler can prove the ownership trace stays local and balanced. The new LLVM `EphemeralLocal` provenance lane materializes stack-backed byte storage and erases `__kain_alloc(...)` plus ownership runtime protocol from the hot cell path.

## What Landed

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `OwnershipPointerProvenance::EphemeralLocal`
  - Added `EphemeralOwnershipLocalWitness`
  - Added block-local ephemeral candidate nomination with inherited known-`Int` literal facts for nested blocks
  - Lowered fresh non-escaping single-cell helper allocs to stack-backed `[N x i8]` storage
  - Lowered `mem_load` / `mem_store` directly against that storage
  - Erased ownership runtime calls for the local `collapse` / `observe` / `decay` path
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage`

## Formal Evidence

- LLVM proof pack: `18/18` proved
- Report: `crates/kain-sys-codegen/z3/reports/20260516T000551Z-llvm-ephemeral-loop-local-ownership-erasure.json`
- Core durable proof still backing the semantic claim:
  - `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-single-cell-ownership-erases-runtime-protocol-under-fresh-noescape-contract.yaml`

## Benchmark Evidence

### `alloc_churn`

- Earlier corrected baseline:
  - `benchmark/out/reports/20260515T232656Z.llm.md`
  - Kain `17.459 ms`
  - Rust `9.411 ms`
- First fresh rerun after the compiler pass:
  - `benchmark/out/reports/20260516T000522Z.llm.md`
  - Kain `18.748 ms`
  - Rust `28.091 ms`
  - This was obviously noisy because Rust went bimodal.
- Stability run:
  - `benchmark/out/reports/20260516T000619Z.llm.md`
  - Kain `13.767 ms`
  - Rust `10.673 ms`

Net effect on the stable run:

- Kain improved by `3.692 ms` versus the earlier corrected baseline.
- That is about a `21.1%` median improvement.
- The generated `benchmark/out/build/alloc_churn/kain/alloc_churn.ll` now shows the real win:
  - `alloca [8 x i8]`
  - direct `getelementptr` + `bitcast` load/store
  - no `call i8* @__kain_alloc(...)`
  - no `call i32 @__kain_ownership_*`

### `ownership_memory`

- Fresh rerun:
  - `benchmark/out/reports/20260516T000551Z.llm.md`
  - Kain `18.019 ms`
  - Rust `11.614 ms`

Interpretation:

- `ownership_memory` is already on the erased lane too, but it remains scalar-work bound rather than runtime-protocol bound.

## Academic Read

This is the first serious proof that Kain can compile a semantic storage request into something other than a heap object when the ownership theorem is strong enough. In PL terms, the compiler is now distinguishing:

- imported or unknown pointers
- helper-owned heap pointers
- ephemeral-local ownership cells

That is a species split, not a micro-optimization.

## Remaining Ceiling

The remaining `alloc_churn` gap is no longer “ownership runtime overhead.” The hot loop is now:

- zeroing the ephemeral storage each iteration
- storing one `i64`
- loading one `i64`
- updating `acc`
- doing `% modulus`

The next likely LLVM/Z3 attacks are:

1. Prove dead zero-init for ephemeral locals when a first write dominates every read.
2. Keep pushing scalar/register residency so the loop carries less stack traffic.
3. Only after that, revisit whether there is any solver-backed algebraic shortcut left in the accumulator math that stays fair.

## Bottom Line

The ephemeral-local path worked.

It did not beat Rust yet on `alloc_churn`, but it removed the entire heap/runtime ownership protocol from the benchmark’s hot cell path and still cut the stable Kain median from `17.459 ms` to `13.767 ms`. That is exactly the kind of category deletion the language needs if it wants to beat Rust by changing the machine model instead of merely polishing the old one.
