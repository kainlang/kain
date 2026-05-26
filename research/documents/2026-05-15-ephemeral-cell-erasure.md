# Ephemeral Cell Erasure Research

- Date: 2026-05-15
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `ephemeral-cell-erasure`

## Research Question

Can a fresh helper-owned non-escaping Kain cell used only by alloc -> collapse(store) -> observe(load) -> decay be lowered to stack or pure SSA while preserving the ownership semantics that matter externally?

## Constraints

- The helper ABI is element-count based: `__kain_alloc(size, stride, zeroed)` allocates `size * stride` bytes.
- The current ownership runtime split is already live: helper-owned locals lower to helper-specific begin/end/decay calls, imported pointers lower through `__kain_ownership_ensure_imported(...)`.
- Any moonshot rewrite must stay benchmark-fair: no benchmark-local semantic cheat codes, and no using `world` / `entangle` to turn `alloc_churn` into a different category.
- The first safe frontier is a fresh helper-owned cell that does not escape, is written once under `collapse`, read once under `observe`, and then `decay`ed.

## Hypothesis Lattice

### Baseline
- Mechanism: fix authoring that accidentally passes byte counts where the helper ABI expects element counts.
- Expected upside: immediate, fair benchmark movement by deleting accidental `8x` payload inflation on single-`Int` cells.
- Likely blocker: this only removes contract leakage; it does not erase runtime ownership protocol or heap/free cost.
- Proof obligation: for bounded benchmark-style domains, a single `Int` cell under the helper ABI requires `size = 1`, not `size = sizeof_type("Int")`.

### Unconventional
- Mechanism: lower proven non-escaping single-cell ownership locals to a stack or compiler-owned storage witness, while keeping `collapse` / `observe` / `decay` as semantic markers.
- Expected upside: delete heap alloc/free and helper slot-token work while preserving the ownership story externally.
- Likely blocker: if the same physical stack slot is reused across loop iterations, terminal `decay` becomes observable unless the compiler also erases or virtualizes the ownership state.
- Proof obligation: show the ownership trace remains observationally equivalent when the cell never escapes and no external alias writes can occur.

### Moonshot
- Mechanism: full ephemeral-cell erasure to scalar SSA value flow.
- Expected upside: `alloc -> collapse(store x) -> observe(load) -> decay` collapses into `x`, leaving only the checksum math in the benchmark loop.
- Likely blocker: requires a first-class proof-carrying lowering contract for fresh non-escaping ownership cells, not just a faster runtime helper.
- Proof obligation: prove the observable loaded value and terminal lifetime state match the runtime semantics under tight freshness / non-escape / no-alias preconditions.

## Mathematical Model

- Variables:
  - `count`: helper ABI element count
  - `stride = sizeof(Int) = 8` on the current 64-bit benchmark lane
  - `written_value`: the value stored during `collapse`
  - abstract ownership states `Idle`, `Collapsed`, `Observed`, `Decayed`
- Invariants:
  - helper payload bytes are `count * stride`
  - a fresh ownership cell starts `Idle`
  - `collapse` transitions `Idle -> Collapsed -> Idle`
  - `observe` transitions `Idle -> Observed -> Idle`
  - `decay` transitions `Idle -> Decayed`
- Objective:
  - separate accidental payload inflation from real runtime/semantic cost
  - prove when the entire ownership cell can evaporate into scalar value flow
- Bad states:
  - byte-style authoring that silently allocates `sizeof(T)` elements
  - any external alias write between store and observe
  - escape paths that force a real address or heap lifetime
  - reuse of a decayed physical storage witness without a compiler-owned virtual lifetime
- Simplifying assumptions:
  - single store
  - single observe load
  - no external alias
  - no runtime fault path
  - no FFI/world/actor escape

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/helper-abi-single-int-cell-requires-one-element-count.smt2`
   - status: `unsat`
   - claim: in the bounded benchmark-style domain, the only element count that yields one `Int` payload is `1`.
2. `crates/sys-codegen/z3/proofs-experimental/ownership-ephemeral-cell-store-load-decay-erases-to-ssa.smt2`
   - status: `unsat`
   - claim: under fresh, non-escaping, single-store, no-alias preconditions, the observed value and final decayed state match scalar SSA erasure.

## Evidence And Sources

- Local:
  - `benchmark/cases/alloc_churn/main.kn`
  - `benchmark/cases/ownership_memory/main.kn`
  - `benchmark/cases/contention_wall/main.kn`
  - `runtime/fixtures/llvm_heap_memory/main.kn`
  - `docs/examples/07_low_level_memory_and_layout.kn`
  - `runtime/native/include/kain_runtime_memory.h`
  - `runtime/native/src/core/kain_runtime_memory.c`
  - `runtime/native/src/core/kain_runtime_ownership.c`
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
- External:
  - None. This branch is repo-internal and proof-backed.

## Dead Ends

- A pure source-contract fix is not enough to beat Rust on `alloc_churn`. It narrows the benchmark to the honest one-cell lane, but the remaining wall is still the runtime ownership protocol plus heap churn.

## Conclusion

The immediate fair win was real: several Kain low-level-memory examples were authored in bytes against an element-count helper ABI. Fixing that contract leak is now part of the durable repo knowledge.

The larger result is that the moonshot is no longer just a vibe. We now have an explicit theorem surface for ephemeral-cell erasure: once a helper-owned cell is fresh, non-escaping, single-store, and alias-free, the interesting work is not "make heap faster" but "prove heap was the wrong abstraction."

Measured benchmark reality from this pass:

- `alloc_churn`: after correcting the case to one `Int` element, the warm rerun landed at Kain `17.459 ms` vs Rust `9.411 ms` in `benchmark/out/reports/20260515T232656Z.llm.md` on `2026-05-15T23:26:44.637659+00:00`.
- `ownership_memory`: after the same one-cell correction, Kain ran `15.070 ms` vs Rust `10.823 ms` in `benchmark/out/reports/20260515T232601Z.llm.md` on `2026-05-15T23:25:49.490928+00:00`.
- `contention_wall`: the proxy win stayed intact after the correction, with Kain `12.764 ms` vs Rust `1758.026 ms` in `benchmark/out/reports/20260515T232632Z.llm.md` on `2026-05-15T23:26:12.998011+00:00`.

Recommended next step:

- Introduce a dedicated LLVM provenance class for ephemeral ownership locals and a proof-carrying lowering rule that lets `collapse` / `observe` / `decay` erase to compiler-owned value flow when the cell never escapes.
