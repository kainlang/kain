# Ownership-Memory Scalar Slot Lowering

- Date: 2026-05-18
- Status: landed
- Repo Root: `D:\\Kain-Lang`
- Session Slug: `ownership-memory-scalar-slot-lowering`

## Research Question

Can Kain close the remaining `ownership_memory` gap by changing the erased single-cell stack shape that LLVM sees, without touching benchmark semantics or inventing a benchmark-only reducer?

## Constraints

- Preserve authored `collapse` / `observe` / `decay` semantics.
- Do not add any `ownership_memory`-specific checksum shortcut.
- Only strengthen alignment when the stack slot itself honestly guarantees it.
- Keep the erased `i8*` observational surface working so existing helper-owned ephemeral logic does not fork into incompatible pointer worlds.

## Hypothesis Lattice

### Baseline
- Mechanism: Accept the existing runtime erasure and hope later LLVM cleanup squeezes more out of `[8 x i8]`.
- Expected upside: Minimal.
- Likely blocker: The remaining gap already looked structural, not random.
- Proof obligation: None beyond rerunning the benchmark.

### Unconventional
- Mechanism: For supported 1/2/4/8-byte ephemeral single cells, lower to a typed scalar alloca and keep the `i8*` view only as a reversible bitcast surface.
- Expected upside: Give LLVM an actual scalar stack cell with honest alignment instead of a byte array that poisons alias/alignment reasoning.
- Likely blocker: `compile_ephemeral_storage_i8_pointer(...)` and `mem_load` / `mem_store` were built around the byte-array witness shape.
- Proof obligation: The scalar-slot lane must preserve the exact byte-observable load/store result of the old `[N x i8]` path, and the new alignment must stay clamped to the actual slot guarantee.

### Moonshot
- Mechanism: Push full SSA scalar replacement through erased helper-owned ownership locals so the stack slot itself disappears from the hot loop.
- Expected upside: Could take `ownership_memory` from parity to a durable win and might help `option_result` too.
- Likely blocker: Control-flow and alias bookkeeping complexity climbs fast once the single-slot lane is no longer enough.
- Proof obligation: Every read must still observe the same value as the lowered memory trace under all dominating store patterns.

## Mathematical Model

- Variables:
  - `W`: written 64-bit payload.
  - `s`: supported storage width in bytes (`1`, `2`, `4`, `8`).
  - `a`: access width in bytes, with `a <= s`.
  - `A_slot`: stack-slot alignment.
  - `A_nat`: natural access alignment.
  - `A_emit = min(A_slot, A_nat)`.
- Invariants:
  - The scalar lane and the old byte-array lane expose the same low `a` bytes of `W`.
  - For supported scalar widths, `A_slot == s`.
  - The emitted alignment may not exceed either `A_slot` or `A_nat`.
- Objective:
  - Reduce `ownership_memory` without creating a fake win that depends on benchmark-only authored math.
- Bad states:
  - Byte-lane observation changes.
  - Claimed alignment exceeds the slot guarantee.
  - Improvement depends on a row-local semantic hack rather than general LLVM lowering.

## Z3 Claims

1. The scalar-slot lane preserves the same byte-observable payload as the old `[N x i8]` lane for supported 1/2/4/8-byte accesses.
2. The emitted alignment clamp cannot exceed either the access alignment or the slot alignment.

## Evidence And Sources

- Local:
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - `crates/sys-codegen/z3/proofs-experimental/ownership-ephemeral-single-cell-scalar-storage-preserves-byte-lane.smt2`
  - `crates/sys-codegen/z3/proofs/memory-ephemeral-single-cell-scalar-storage-preserves-byte-lane-observation.yaml`
  - `benchmark/out/reports/latest_ownership_memory_scalar_ephemeral.llm.md`
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest_scalar_ephemeral_regression_sanity.llm.md`
- External:
  - None needed; this was repo-local compiler benchmarking and solver work.

## Dead Ends

- I rejected any benchmark-owned reducer immediately. `ownership_memory` is too small and too honest a smoke case to “win” by folding its checksum.
- I did not try the full SSA moonshot yet because the typed scalar slot alone already removed the large structural wound.

## Conclusion

The unconventional lane was correct. The old backend had already erased ownership runtime overhead, but it still left LLVM looking at a byte array stack slot with `align 1`, which was enough to generate suspicious byte-lane code for a benchmark that should have been scalar integer math. The landed change replaces that with typed scalar slot lowering for supported single-cell widths and keeps the old pointer-observable behavior through reversible `i8*` bitcasts plus bounded alignment strengthening.

Measured result:

- Pre-pass full suite (`benchmark/out/reports/20260519T005630Z.json`): Kain `14.264 ms`, Rust `11.788 ms`, C++ `11.245 ms`.
- Focused rerun (`benchmark/out/reports/latest_ownership_memory_scalar_ephemeral.llm.md`): Kain `11.554 ms`, Rust `12.177 ms`, C++ `11.090 ms`.
- Post-pass full suite (`benchmark/out/reports/latest.llm.md`): Kain `10.752 ms`, Rust `12.738 ms`, C++ `11.062 ms`.
- Focused sanity (`benchmark/out/reports/latest_scalar_ephemeral_regression_sanity.llm.md`): Kain `11.668 ms`, Rust `11.671 ms`, C++ `11.664 ms`.

That is the honest machine-truth interpretation: Kain moved from a meaningful ownership-memory loss to noise-band parity with occasional full-suite wins. The next frontier is no longer “fix ownership helpers.” It is the tighter backend/runtime set: `http_server_concurrency`, `sim_uv_velocity_grid`, `string_ops`, `branch_dispatch`, `memory_stream`, `call_chain`, `option_result`, `machine_stones_shatter_loop`, and `ffi_shared_call_stress`.
