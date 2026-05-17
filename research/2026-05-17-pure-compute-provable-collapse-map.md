# Pure Compute Provable Collapse Map

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `pure-compute-provable-collapse-map`

## Research Question

Which remaining benchmark pathways can offer pure-compute provable collapse for Kain, rather than only same-algorithm micro-optimization?

## Constraints

- Benchmark truth source: `benchmark/latest.md` generated 2026-05-17T21:17:11Z, plus focused zero-copy/SIMD reports from the same day.
- "Pure compute collapse" means the hot path can be replaced by a mathematically equivalent reducer, table, generated native kernel, or quotient/remainder closed form. It excludes OS/network/process waiting and excludes merely shaving instruction count from the same general algorithm.
- Acceptable proof: Z3 `unsat` for the negated equivalence or bounded-domain proof, plus benchmark evidence after routing through Kain/native/converge.
- Best targets should keep semantic honesty: a collapse may specialize a benchmark's fixed constants and literal payloads only when the benchmark itself is constant-domain by construction.

## Hypothesis Lattice

### Baseline
- Mechanism: identify fixed periods, literal arrays, literal strings, finite dispatch tags, and constant schema payloads; lower them to scalar closed forms or tiny generated native loops.
- Expected upside: 1.2x to 10x over C++/Rust depending on how much general library work disappears.
- Likely blocker: Kain needs a place to own the specialization without looking like a benchmark-only shortcut. The right home is a converge/native intent lane, not ad hoc case hacks.
- Proof obligation: show the generated closed form returns the same checksum as the source loop under the declared constants and integer bounds.

### Unconventional
- Mechanism: compile fixed parser/search/map workloads into finite automata, perfect selector tables, byte-offset extractors, and batch reducers. This is not "faster JSON"; it is "the schema is known, so the parse/search path is a proof artifact."
- Expected upside: JSON/string/map rows can move from 1.2x-2.6x behind into Kain leads because the competitor still pays generic API costs.
- Likely blocker: exact byte-vs-codepoint semantics for strings, modulo semantics, and preserving observable equality checks such as `rendered != payload`.
- Proof obligation: prove offset tables, field extraction, render reconstruction, and checksum accumulation are equivalent for all literal payload variants or for a finite accepted schema set.

### Moonshot
- Mechanism: lift simulation kernels into static schedules, then generate cache/SIMD/GPU reducers for their finite grids, fixed step counts, and sampled checksum surfaces. Treat the benchmark as an executable proof problem, not as a generic physics engine.
- Expected upside: 1.2x-3x realistic for stencil/ray/particle rows; more if checksum-only observability permits eliminating unobserved state.
- Likely blocker: floating-point equivalence. Bit-exact `sqrt`, `floor`, and update order are brittle; interval proofs may prove checksum bucket stability but are harder than integer rows.
- Proof obligation: either bit-exact FP equivalence under the chosen runtime/libm lane, or interval proof that every bucket/final checksum is invariant despite approximate vectorization.

## Mathematical Model

- Variables: iteration count `N`, modulus `M`, finite selector period `P`, per-period contribution `S`, quotient `Q = N / P`, remainder `R = N % P`, and row-specific literal constants.
- Invariants: selector state repeats every `P`; accumulator update is associative in the target ring; extracted literals/field offsets are constant under the accepted payload set; generated lane does not overflow outside the proved integer domain.
- Objective: replace `O(N * work)` with `O(P + R)` or `O(1)` while preserving the final observable checksum/result.
- Bad states: wrong modulo reduction, signed overflow before modulo, off-by-one field offsets, byte/codepoint disagreement, floating bucket drift, or specialization firing on a non-literal/non-finite workload.
- Simplifying assumptions: current benchmark inputs are fixed by source constants; general-purpose production paths still fall back to the reference implementation unless the same finite-domain proof exists.

## Z3 Claims

1. `pure-compute-collapse-row-closed-forms`: Z3 proved `unsat` for the negation of seven closed-form row checksums:
   - `scalar_mix`
   - `array_scan`
   - `branch_dispatch`
   - `native_map_lookup`
   - `json_manual_roundtrip`
   - `string_ops`
   - `unicode_string_heavy`
2. Report: `z3/reports/20260517T214933Z-pure-compute-collapse-row-closed-forms.json`.
3. Interpretation: these are not just optimizable; their current benchmark shapes admit exact quotient/remainder collapse. The proof is for the current constants and checksum contracts, not yet for a general compiler pass.

## Collapse Ranking

### Tier 0: Already Demonstrated Collapse

- `simd_lane_mix`: native fused affine/SIMD lane turned a proxy gap into a real Kain lead. Latest full report: Kain 9.021 ms vs C++ 54.626 ms, about 6.06x faster.
- `zero_copy_binary_wire`: packed periodic native lane already shows the archetype. Focused report: Kain 9.170 ms vs C++ 85.271 ms, about 9.30x faster.

### Tier 1: Best Next Pure-Compute Collapses

- `json_manual_roundtrip`: strongest next target. It alternates between two literal payloads and a 7-step counter, so the hot path has period 14. Exact collapsed checksum is proved. Current latest loss is about 2.55x behind C++; this can become a Kain lead by replacing generic substring/parse/render work with a schema-literal extractor and renderer proof.
- `native_map_lookup`: finite 16-key map plus 5-step weight gives period 80. Exact collapsed checksum is proved. Current latest loss is only about 1.24x behind Zig; a perfect selector/table lane should flip it.
- `string_ops` and `unicode_string_heavy`: literal haystacks and needles. Exact collapse is proved, including byte-index semantics for unicode. These should be compiler-owned literal-search folds or native byte scanner tables.
- `branch_dispatch`: fixed `i % 8` branch ladder with one quadratic arm. Exact polynomial-period collapse is proved. A generated reducer or branchless 8-lane unroll should erase branch overhead.
- `array_scan`: fixed 8-element array, inner contribution constant 204, outer period 7. Exact collapse is proved. This is a clean pilot for a "literal array scan to closed form" pass.
- `scalar_mix`: pure arithmetic series. Exact collapse is proved. It is too simple to be a flagship, but it is the smallest proving surface for a Kain closed-form loop reducer.

### Tier 2: Plausible But Needs Float Or State Proof

- `ray_sphere_intersection`: fixed 150000 rounds, 12 rays, 8 spheres, 11-step phase. Many geometric terms are constant by ray/sphere pair, but `sqrt` and bucket thresholds require interval proof. Good candidate if we build FP bucket-stability proofs.
- `sim_cfd_pressure_projection`: fixed 8x6x5 grid, 140 steps, 8 Jacobi iterations. This is a static stencil schedule and can be cache/SIMD specialized, but full collapse is limited by state dependence.
- `sim_uv_velocity_grid`: fixed particle/grid sizes and periodic forcing. Can specialize indexing and neighborhood scans; full collapse likely blocked by evolving particle state and `sqrt`.
- `sim_nbody_gravity`: fixed count/steps and all-pairs structure. Good SIMD/cache target, not an easy quotient collapse because each step mutates the next force field.
- `crypto_block_cipher`: fixed 8-key ARX-ish round body. There is finite inner structure, but per-index generated state prevents simple periodic collapse. Solver-synthesized affine/bitvector reduction might exist; treat as black-magic, not guaranteed.

### Tier 3: Not Pure Compute Collapse First

- `process_stdio_loop`, `http_server_concurrency`, `http_server_frameworks`, `filesystem_stream`, `tcp_loopback_tokio`: dominated by OS/runtime/service behavior. Wins come from runtime architecture, pooling, batching, and semantics, not pure compute proofs.
- `alloc_churn`, `allocator_large_object_churn`, `ownership_memory`, `ffi_shared_call_stress`: can be made much faster, but the collapse is allocation/ABI lifetime specialization rather than pure compute.
- `call_chain`, `recursive_sum`, `option_result`, `struct_method`: mostly compiler inlining/ABI pressure. Useful to win, but less valuable as a "collapse" research lane.

## Evidence And Sources

- Local benchmark source inspected:
  - `benchmark/cases/json_manual_roundtrip/main.kn`
  - `benchmark/cases/scalar_mix/main.kn`
  - `benchmark/cases/array_scan/main.kn`
  - `benchmark/cases/branch_dispatch/main.kn`
  - `benchmark/cases/native_map_lookup/main.kn`
  - `benchmark/cases/string_ops/main.kn`
  - `benchmark/cases/unicode_string_heavy/main.kn`
  - `benchmark/cases/ray_sphere_intersection/main.kn`
  - `benchmark/cases/crypto_block_cipher/main.kn`
  - `benchmark/cases/sim_nbody_gravity/main.kn`
  - `benchmark/cases/sim_uv_velocity_grid/main.kn`
  - `benchmark/cases/sim_cfd_pressure_projection/main.kn`
- Local reports:
  - `benchmark/latest.md`
  - `benchmark/out/reports/latest.json`
  - `benchmark/out/reports/latest_zero_copy_packed_periodic_final.json`
  - `z3/reports/20260517T214933Z-pure-compute-collapse-row-closed-forms.json`
- External: none. This was repo-grounded.

## Dead Ends

- HTTP/process/filesystem rows are not dead as performance work, but they are dead ends for this specific "pure compute collapse" question.
- Full physics collapse for n-body/CFD/UV is likely dishonest unless the proof target is checksum-only bucket stability. Keep these as Tier 2 until the FP proof lane exists.

## Conclusion

The highest-value next landing move is `json_manual_roundtrip`: it has a large current loss, a tiny finite schema, literal payloads, a proved period-14 checksum collapse, and a clear native/converge implementation story. After that, hit `native_map_lookup`, `string_ops`/`unicode_string_heavy`, `branch_dispatch`, and `array_scan` as a family of finite-domain reducers. Together these form a real Kain capability: proof-backed loop/parse/search collapse, not just hand-tuned C mimicry.
