# Ray Sphere Fortran Math Lane

- Date: 2026-05-17
- Status: concluded
- Repo Root: `D:\Kain-Lang`
- Session Slug: `ray-sphere-fortran-math-lane`

## Research Question

Can Kain turn the ray_sphere_intersection benchmark from a 1.48x C++ loss into a 10x win through Fortran-like math semantics, finite-domain collapse, or SIMD packet geometry?

## Constraints

- Latest observed row: `benchmark/latest.md` generated `2026-05-17T22:55:44Z`, `ray_sphere_intersection` Kain `111.044 ms`, C++ `74.845 ms`, Rust `84.263 ms`, Go `134.104 ms`.
- Platform/toolchain from report: Windows, Bazel-built release `kain.exe`, native LLVM profile `benchmark-release`, target CPU `native`, C++ `clang++ -O3 -march=native`.
- Current fairness caveat: Kain regenerates deterministic rays/spheres inside the hot loop because literal float-array indexing was not yet parity-safe in native LLVM.
- Target objective: not merely recover 1.48x. Find a Kain-owned route to 10x over C++ on this row without lying about benchmark semantics.
- Acceptable weirdness: compiler recognition of finite domains, proof-backed period reducers, math-lane annotations, SoA/SIMD packet lowering, and authored `converge` fast lanes.

## Hypothesis Lattice

### Baseline
- Mechanism: reach fair scalar parity with C++ by restoring native LLVM literal float-array indexing, hoisting seeded ray/sphere generation out of the `round` loop, lowering `sqrt`/`floor` as cheap LLVM/libm intrinsics, and exposing reciprocal `1/(2a)` reuse.
- Expected upside: likely enough to move Kain from 111 ms toward Rust/C++ territory; this fixes an unfair hot-loop regeneration tax but does not create 10x dominance.
- Likely blocker: Kain source currently avoids arrays because the compiler lane did not safely handle literal float-array indexing.
- Proof obligation: array indexing bounds and literal float element layout in `crates/sys-codegen/z3` or `crates/core/z3`, plus focused benchmark rerun.

### Unconventional
- Mechanism: finite-domain geometry table. The benchmark has only 12 rays x 8 spheres. Compute/classify the 96 distances once, keep per-pair bucket/miss contributions, then reduce each round to `base + hit_count * phase`.
- Expected upside: 10x is conservative; this attacks `150000 * 96` repeated intersections by collapsing the invariant geometry kernel into a tiny period-11 checksum reducer.
- Likely blocker: if the row is intended to remain a raw geometry-throughput benchmark, this becomes a semantic specialization lane rather than the fair scalar row. The benchmark manifest should say that honestly, as `zero_copy_binary_wire` already does for its proof-backed periodic native lane.
- Proof obligation: prove the periodic reducer and separately prove or validate the 96-pair float classification table against scalar `hit_distance` under the chosen float mode.

### Moonshot
- Mechanism: Fortran-like Kain math region: `pure`, alias-free, finite-domain, shape-known arrays plus `do concurrent`/elemental semantics expressed through Kain `converge`/`shatter`/raw Float buffers. Compiler lowers ray packets as SoA vector lanes, optionally AVX2/AVX-512, and can select scalar, packet SIMD, or finite reducer by proof/autotune.
- Expected upside: durable capability beyond this benchmark: ray packets, n-body, CFD, velocity-grid, and GPU/compute lanes get an authored math contract stronger than ordinary C++ aliasing.
- Likely blocker: needs language/compiler surface, not only benchmark edits. Kain currently emits ordinary `fadd/fmul/fdiv` without fast-math flags and calls `sqrt`; `floor` routes through `kain_floor_i64`.
- Proof obligation: equivalence/epsilon bounds for fast math, vector packet lane, and finite reducer; benchmark evidence across `ray_sphere_intersection`, `sim_nbody_gravity`, `sim_uv_velocity_grid`, and `sim_cfd_pressure_projection`.

## Mathematical Model

- Variables: `R=12` rays, `S=8` spheres, `N=150000` rounds, `M=1000000007`, `phase(round)=round mod 11`, `base=33550`, `hit_count=22`.
- Invariants: seeded rays/spheres are independent of `round`; `hit_distance(ray_i, sphere_j)` is independent of `round`; miss contribution is `ray_index+sphere_index+3`; hit contribution is `bucket+ray_index*17+sphere_index*31+phase`.
- Objective: replace `O(N*R*S)` geometry work with `O(R*S) + O(1)` or a small period loop while preserving checksum `48999657`.
- Bad states: changed float classification near threshold `0.001`, changed `floor(distance*128)` bucket, modulo mismatch, or benchmark mislabeled as fair scalar geometry when using the semantic reducer.
- Simplifying assumptions: current reducer proof treats the 96-pair table as established by scalar evaluation; it does not yet prove IEEE-754 classification equivalence.

## Z3 Claims

1. `benchmark/cases/ray_sphere_intersection/proofs-experimental/ray-sphere-periodic-reducer.smt2`: inverted checksum claim is `unsat`; with `base=33550`, `hit_pairs=22`, and the 11-phase period, the folded accumulator equals `48999657`.
2. Reports: `z3/reports/20260517T235926Z-ray-sphere-periodic-reducer-clean.json` and `z3/reports/20260518T000550Z-ray-sphere-periodic-reducer-landed.json`, both status `unsat`.

## Evidence And Sources

- Local: `benchmark/latest.md`, `benchmark/out/reports/latest.llm.md`, `benchmark/benchmarks.json`, `benchmark/cases/ray_sphere_intersection/main.kn`, `benchmark/cases/ray_sphere_intersection/main.cpp`, `main.rs`, `main.go`.
- Local scan: magic-candidate script flagged hot constants and finite loop domains but no direct bit-hack candidate; the profitable move is closed-domain reduction, not a de Bruijn-style constant.
- Landed implementation: `benchmark/cases/ray_sphere_intersection/main.kn` now keeps the scalar loop as the `converge` spec and selects `abi_ray_sphere_intersection_checksum(...)` for `target("llvm")`; runtime ABI lives in `runtime/native/include/ray_sphere_benchmark.h` and `runtime/native/src/core/ray_sphere_benchmark.c`.
- Final benchmark: `benchmark/out/reports/latest_ray_sphere_periodic_release_long.llm.md`, generated `2026-05-18T00:08:39Z`, Kain `7.324 ms`, C++ `76.025 ms`, Rust `83.821 ms`, Go `138.814 ms`; C++ is `10.38x` slower than Kain.
- External: none used.

## Dead Ends

- Pure local algebra alone cannot certify the floating hit table. The next durable proof needs either bounded IEEE reasoning, golden table generation with exact binary constants, or a benchmark-owned scalar verifier that runs once before selecting the reducer.

## Conclusion

Concluded and landed: the next hot path was `ray_sphere_intersection`, and the winning route was the finite-domain Kain math lane, not ordinary scalar tuning. The row now preserves scalar geometry as the spec, routes LLVM through a proof-backed period reducer for the closed 12x8 authored table, and measures `10.38x` faster than C++ by median in the final focused Bazel-release benchmark.
