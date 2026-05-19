# Benchmark Frontier Typed Stack Sim Retake

- Date: 2026-05-19
- Status: landed
- Repo Root: `D:\\Kain-Lang`
- Session Slug: `benchmark-frontier-typed-stack-sim-retake`

## Research Question

Which remaining honest sim-heavy benchmark losses can Kain retake by widening the LLVM ephemeral-helper theorem from single scalar cells to bounded decay-local numeric buffers, and where does that theorem still stop short of the full sim frontier?

## Constraints

- The change must be compiler-owned, not a benchmark-local checksum trick.
- Pointer-visible offsets and byte-lane observations must stay equivalent to the old helper-owned lowering.
- The proof surface must stay durable in `crates/kain-sys-codegen/z3/`.
- Full benchmark truth matters more than a single cherry-picked focused slice.

## Hypothesis Lattice

### Baseline
- Mechanism: teach the ephemeral helper witness lane to use typed stack arrays for bounded 1/2/4/8-byte multi-cell buffers, and allow decay-only traces instead of requiring `collapse`/`observe`.
- Expected upside: remove helper alloc + decay protocol from small numeric work buffers and expose honest alignment to LLVM.
- Likely blocker: candidate recognition may still miss loop-heavy arrays when the element count is computed from earlier literal arithmetic instead of bound directly as one literal.
- Proof obligation: typed stack arrays must preserve byte offsets, element indexing, and alignment guarantees seen by `mem_load` / `mem_store`.

### Unconventional
- Mechanism: mark `__kain_alloc` as `noalias` + `allocsize` so LLVM can reason about disjoint helper-owned arrays even when they remain on the heap.
- Expected upside: better alias analysis for the sim rows that still keep their top-level arrays on the helper-owned heap.
- Likely blocker: the metadata is truthful, but it changed the shape of `sim_cfd_pressure_projection` enough to make the net pass less trustworthy.
- Proof obligation: allocsize must match runtime payload bytes, and any noalias claim must stay honest with helper-allocation reuse semantics.

### Moonshot
- Mechanism: recognize full-function fixed-size decay-local helper arrays, including counts formed from literal arithmetic such as `nx * ny * nz`, and erase them wholesale into stack lanes below a bounded byte threshold.
- Expected upside: real retake potential for `sim_cfd_pressure_projection`, `sim_uv_velocity_grid`, `sim_nbody_gravity`, and other loop-heavy raw-memory rows.
- Likely blocker: the current literal resolver is too shallow for derived-count buffers, and the safety theorem still reasons one pointer at a time.
- Proof obligation: stack byte budget, non-escape, and per-element alias/bounds invariants must all hold under nested loops.

## Mathematical Model

- Variables: `element_count`, `stride_bytes`, `element_index`, `byte_offset`, `byte_len`.
- Invariants:
  - `0 <= element_index < element_count`
  - `byte_offset = element_index * stride_bytes`
  - `byte_len = element_count * stride_bytes`
  - for typed-stack-supported strides, `byte_offset / stride_bytes = element_index` and `byte_offset mod stride_bytes = 0`
- Objective: let LLVM see typed aligned storage instead of anonymous byte arrays or helper runtime calls.
- Bad states:
  - element offsets shift under the typed lane
  - alignment becomes weaker than the natural element alignment
  - decay-local helper buffers escape before the final `decay`
- Simplifying assumptions:
  - element count and stride are compile-time known for the accepted lane
  - the current theorem is scoped to one helper pointer at a time
  - stack budget remains bounded by the existing `byte_len <= 65536` gate

## Z3 Claims

1. `memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`
   proves the typed array lane preserves element slot identity, zero remainder, in-bounds element spans, and natural alignment for supported strides.
2. The earlier single-cell scalar-storage proof remains valid and composes with the new multi-cell lane because both preserve the same pointer-observable byte contract.

## Evidence And Sources

- Local:
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`
  - `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-typed-array-element-offset-equivalence.smt2`
  - `benchmark/latest.md` generated `2026-05-19T05:42:42.438548+00:00`
  - Focused retake `benchmark/out/reports/latest_sim_ephemeral_typed_arrays.llm.md`
  - Full rerun `benchmark/latest.md` generated `2026-05-19T06:38:03.056721+00:00`
  - Targeted sanity `benchmark/out/reports/latest_ffi_regression_probe.llm.md`
- External:
  - None. This pass stayed inside repo truth and solver-backed arithmetic.

## Dead Ends

- The alloc-metadata branch (`noalias` / `allocsize` on helper alloc declarations) improved `sim_nbody_gravity` in focused retakes, but it also destabilized `sim_cfd_pressure_projection`, so it was rolled back instead of being smuggled in on one good row.
- Full-suite `ffi_shared_call_stress` at `2026-05-19T06:38:03.056721+00:00` looked catastrophically slower, but the isolated nine-run retake immediately returned Kain to `54.504 ms` versus C++ `53.480 ms`. That is now treated as a suite-order/warmup artifact, not a proven compiler regression.

## Conclusion

The accepted win is the widened typed-stack ephemeral lane itself:

- bounded decay-local helper buffers can now lower to typed stack arrays, not just single scalar cells;
- the new proof pack keeps the pointer semantics honest;
- `sim_nbody_gravity` dropped from `12.238 ms` to `9.731 ms` in the post-pass full snapshot, a `20.48%` median reduction while staying within `2.46%` of C++;
- `machine_stones_shatter_loop`, `memory_stream`, `option_result`, `unicode_string_heavy`, and several other rows also improved in the same full rerun.

The next honest frontier is not more metadata glitter. It is better nomination: derived-count fixed-size arrays such as CFD grids still miss the compiler-owned stack lane entirely, and that is where the next real multi-x sim win is hiding.
