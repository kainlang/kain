# Typed Pointer Memory Lowering

- Date: 2026-05-18
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `typed-pointer-memory-lowering`

## Research Question

Can Kain turn `ptr_offset`-based memory loops into typed, aligned LLVM memory walks that close the latest `memory_stream` gap without benchmark-specific cheating?

## Constraints

- Preserve the authored `collapse` / `observe` / `decay` semantics.
- Do not add benchmark-only closed forms or case-specific runtime helpers.
- Only claim natural alignment where helper-owned heap provenance makes it honest.
- Keep the general LLVM path valid for other pointer-heavy rows before celebrating the win.

## Hypothesis Lattice

### Baseline
- Mechanism: Strength-reduce power-of-two `ptr_offset` byte scaling from `mul` to `shl`.
- Expected upside: Small but free win on hot raw-pointer loops.
- Likely blocker: Multiplies were probably not the whole tax.
- Proof obligation: `offset * 8 == offset << 3` on the bounded non-negative domain we actually use.

### Unconventional
- Mechanism: Lower `mem_load` / `mem_store` over helper-owned `ptr_offset(..., "Int")` into typed `getelementptr i64` plus `align 8` loads/stores instead of opaque integer-address byte walks.
- Expected upside: Restore enough alias/alignment signal for LLVM to treat the loop like a real heap pointer walk.
- Likely blocker: Kain pointer values still travel through raw integer storage in much of the pipeline.
- Proof obligation: The stride/type match must preserve byte address equivalence and must not overstate alignment for imported or ephemeral pointers.

### Moonshot
- Mechanism: Carry a parallel typed/raw helper-pointer sidecar through local bindings so `__kain_alloc` provenance survives even more of the lowering pipeline.
- Expected upside: Could unlock another tier of alias-analysis wins on memory- and ownership-heavy rows.
- Likely blocker: SSA/control-flow bookkeeping risk is much higher than the first pass.
- Proof obligation: Sidecar state must stay coherent across assignment and scope boundaries.

## Mathematical Model

- Variables:
  - `B`: base heap pointer
  - `i`: signed element offset
  - `w`: element width in bytes
  - `A`: claimed alignment
- Invariants:
  - Helper-owned heap allocations come from the native allocator and can safely claim natural alignment for primitive `Int` / `Float` accesses.
  - For typed GEP promotion we only rewrite when `w(access_ty) == w(ptr_offset element_ty)` or the canonical literal stride matches the access width.
  - For the shift reduction proof we require `0 <= i < 2^60`.
- Objective:
  - Minimize full-suite `memory_stream` median while improving, or at least not harming, other pointer-heavy LLVM rows.
- Bad states:
  - Wrong byte address.
  - Overstated alignment on imported/ephemeral pointers.
  - Benchmark win that depends on case-local semantic cheating.
- Simplifying assumptions:
  - The first pass targets primitive-width helper-owned accesses only.

## Z3 Claims

1. `offset * 8 == offset << 3` for non-negative 64-bit offsets below `2^60`.
2. No benchmark-specific checksum collapse is needed if the backend can expose the real pointer walk honestly.

## Evidence And Sources

- Local:
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `crates/kain-sys-codegen/z3/proofs-experimental/power-of-two-ptr-offset-shift-equivalence.smt2`
  - `benchmark/latest_typed_pointer_memory_probe.md`
  - `benchmark/latest.md`
- External:
  - None needed; this pass was repo-local compiler/runtime work.

## Dead Ends

- Rejected the benchmark-only path immediately: `memory_stream` admits cheap closed-domain tricks, but they would have been exactly the kind of fairness rot this pass was supposed to avoid.
- Did not land the typed helper-pointer sidecar moonshot because the first typed-GEP/alignment pass already moved the row from a catastrophic loss to a Kain full-suite win.

## Conclusion

The unconventional lane won cleanly. The landed LLVM change teaches `mem_load` / `mem_store` to keep helper-owned `ptr_offset` accesses as typed GEPs with honest natural alignment, while imported and ephemeral pointers stay conservative. The supporting Z3 artifact proves the power-of-two shift reduction used by the generic `PtrOffset` path.

Measured result:

- Pre-pass latest full suite (`2026-05-18T23:37:06.421184+00:00`): `memory_stream` was Kain `37.481 ms`, Rust `10.447 ms`, C++ `8.811 ms`.
- Focused probe (`benchmark/latest_typed_pointer_memory_probe.md`): Kain `9.749 ms`, Rust `10.169 ms`, C++ `9.222 ms`.
- Post-pass full suite (`2026-05-19T00:14:32.341687+00:00`): Kain `8.446 ms`, Rust `9.652 ms`, C++ `9.835 ms`.

That is the right kind of win: one backend change, no row-specific special casing, and the broader suite stayed green. The next honest branches are `ownership_memory`, `string_ops`, `dynamic_vtable_thrashing`, `sim_uv_velocity_grid`, and the runtime-heavy `http_server_concurrency` / `process_stdio_loop` lanes.
