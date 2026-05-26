# Derived Typed Stack Cfd

- Date: 2026-05-19
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `derived-typed-stack-cfd`

## Research Question

Can native LLVM prove-and-erase helper-owned buffers whose element counts come from closed-form arithmetic like nx * ny * nz, and does that flip the remaining CFD benchmark loss without regressions?

## Constraints

- Keep the benchmark honest: no benchmark-only checksum shortcut and no foreign baseline manipulation.
- Preserve helper-buffer semantics when sibling buffers are read or written inside the same hot loop.
- Prove the surviving memory invariants with the sys-codegen Z3 lane before trusting the unsafe lowering.
- Finish with a fresh full benchmark so the latest frontier reflects the landed code rather than a focused retake.

## Hypothesis Lattice

### Baseline
- Mechanism: derived-count helper buffers are already nominatable for typed stack storage, but the theorem must survive nested loops that touch sibling helper buffers through helper-call surfaces.
- Expected upside: erase `__kain_alloc` plus ownership decay from real CFD and UV hot loops instead of only the simpler single-buffer lanes.
- Likely blocker: `__kain_mem_load` and `__kain_mem_store` calls for non-target siblings may poison the remaining-statement contract and force a heap fallback.
- Proof obligation: allowing safe sibling-pointer traffic must not weaken the bounds and alignment invariants already proved for the typed stack lane.

### Unconventional
- Mechanism: treat helper-call memory ops as safe when they either hit the target ephemeral buffer or a non-target pointer expression that is otherwise side-effect-safe.
- Expected upside: the optimizer can keep a whole multi-buffer simulation region inside typed stack storage even when loops mix reads from `pressure_old` and writes to `pressure`.
- Likely blocker: helper-call recognition currently reasons like a single-buffer theorem and may reject any pointer not tied directly to the active target.
- Proof obligation: helper-call acceptance must still forbid escaping pointers, alias-creating side effects, and post-decay reuse.

### Moonshot
- Mechanism: once the heap protocol disappears from these float-heavy rows, chase the next 1.1x to 1.3x with scalar-slot promotion, vectorization-friendly loop shape, or float-specific memory SSA cleanup.
- Expected upside: narrow or retake `sim_cfd_pressure_projection` and `sim_uv_velocity_grid` against optimized C++ without changing benchmark semantics.
- Likely blocker: the next frontier is likely in LLVM IR quality rather than helper lifetime protocol.
- Proof obligation: any future vector or algebraic lane must preserve stencil math exactly and be validated against the full suite, not only a focused run.

## Mathematical Model

- Variables: `element_count`, `stride_bytes`, `index`, `byte_offset`, `buffer_ptr`, `target_buffer_ptr`, `remaining_stmt`, `decay_point`.
- Invariants:
  - `0 <= index < element_count`
  - `byte_offset = index * stride_bytes`
  - helper-owned typed stack storage preserves natural element alignment
  - non-target sibling accesses are safe only when the pointer expression is itself side-effect-safe and non-escaping
  - no helper pointer is observed after its final `decay`
- Objective: prove enough safety for LLVM to lower multi-buffer simulation helpers to typed stack arrays instead of heap helper objects.
- Bad states:
  - sibling helper-buffer loads falsely force a heap fallback
  - a relaxed helper-call rule allows escaped or aliased pointers through
  - typed storage changes observed element offsets or alignment
  - benchmark wins come from source cheating instead of compiler/runtime improvement
- Simplifying assumptions:
  - element counts remain compile-time closed forms by the time the helper-layout lane runs
  - the existing memory-lane proofs for typed arrays and allocsize metadata stay valid after this call-surface relaxation

## Z3 Claims

1. `crates/sys-codegen/z3` memory lane still proves all helper-buffer stack-lowering invariants after the helper-call relaxation.
2. The existing typed-array offset/alignment cases remain sufficient because this change widens safe call-surface recognition rather than changing layout arithmetic.

## Evidence And Sources

- Local:
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - `benchmark/out/build/sim_cfd_pressure_projection/kain/sim_cfd_pressure_projection.ll`
  - `benchmark/out/build/sim_uv_velocity_grid/kain/sim_uv_velocity_grid.ll`
  - `benchmark/out/build/sim_nbody_gravity/kain/sim_nbody_gravity.ll`
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest_sim_multibuffer_postsuite_sanity.llm.md`
  - `crates/sys-codegen/z3/reports/20260519T082634Z-20260519T-kain-sys-codegen-memory-after-multibuffer-ephemeral.json`
- External:
  - None. This pass stayed inside repo code, proofs, and benchmark telemetry.

## Dead Ends

- Derived-count nomination was not the root blocker by itself. The backend was already discovering fixed helper layouts for these rows; the heap fallback persisted because sibling helper-call mem ops invalidated the remaining-statement contract.
- The broader `cargo test -p kain-sys-codegen --test llvm_codegen_test -- --nocapture` suite is not currently green on this branch due to unrelated pre-existing expectation mismatches in other tests, so it was not a trustworthy regression signal for this specific pass.

## Conclusion

The landed fix is narrower and more honest than the initial hypothesis: the missing speedup was not "invent a new derived-count theorem" so much as "stop rejecting safe sibling helper-buffer traffic inside the theorem we already have." After relaxing `__kain_mem_load` and `__kain_mem_store` helper-call handling for safe non-target siblings, the real simulation IR now uses typed stack allocas in the hot paths instead of helper heap objects.

Validated outcome:

- Targeted regression test `llvm_erases_sim_style_derived_count_float_buffers_to_typed_local_storage` passes.
- Sys-codegen Z3 memory lane reports `11 proved, 0 counterexamples, 0 unknown, 0 errors`.
- Full benchmark refresh passed at `2026-05-19T08:30:28.919652+00:00`.
- Focused post-suite sanity still shows honest remaining gaps versus C++:
  - `sim_uv_velocity_grid`: Kain `16.832 ms`, C++ `15.648 ms`
  - `sim_cfd_pressure_projection`: Kain `10.847 ms`, C++ `9.949 ms`

That means the helper-lifetime protocol is no longer the main frontier for these rows. The next attack surface is IR quality inside the float loops: scalar-slot promotion, vectorization friendliness, and further spill reduction.
