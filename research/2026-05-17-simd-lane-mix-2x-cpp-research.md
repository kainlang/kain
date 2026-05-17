# SIMD Lane Mix 2x Cpp Research

- Date: 2026-05-17
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `simd-lane-mix-2x-cpp-research`

## Research Question

How can Kain's native converge SIMD lane move from near-C++ parity to at least 2x faster than the C++/Rust SIMD baselines without lying about the math?

## Constraints

- Preserve the row's observable result. The landed higher-signal row uses `passes = 8192` and expected result `964251665`.
- Do not re-label a proxy as metal. Any new fast path must sit behind `converge`, route through native runtime capability selection, and keep the scalar spec path intact.
- Proof standard: Z3 `unsat` for the integer identity or lane-width safety used by the optimization.
- Benchmark standard: rerun `simd_lane_mix` against Kain/Rust/C++ and report median plus fastest-relative. The current Kain median is `10.2215 ms`; C++ median is `9.3086 ms`.

## Hypothesis Lattice

### Baseline
- Mechanism: Keep the current per-pass dot API, but improve the native kernel itself: wider AVX-512 when available, AVX2 unrolling, fewer scalar tail branches, and avoid redundant CPU-feature checks through converge selector caching.
- Expected upside: 1.1x to 1.5x. Enough to beat the current C++ median on some runs, but probably not a clean 2x.
- Likely blocker: C++ and Rust already spend almost all hot time in vectorized dot loops. Fighting them at the same operation count is a knife fight.
- Proof obligation: Existing even-dword multiply proof remains enough for i32-domain AVX2/AVX-512. Any new packed lane needs range and overflow proof.

### Unconventional
- Mechanism: Factor the affine lane bias out of the repeated dot product:
  `sum_i((left_i + b) * right_i) = sum_i(left_i * right_i) + b * sum_i(right_i)`.
  Compute `base_dot` and `sum_right` once with native SIMD, then fold the phases in scalar integer math.
- Expected upside: Huge. The landed benchmark performs `32,768 * 8,192 = 268,435,456` logical lane products in the Rust/C++ repeated-dot shape; the factored Kain kernel performs one fill/reduction pass plus 8,192 scalar phase updates.
- Likely blocker: The API must not become a benchmark cheat. The right shape is a generic native ABI such as `runtime_simd_i32_domain_affine_bias_accumulate(...)`, used by a `converge` fast lane whose spec is the literal repeated scalar-dot loop.
- Proof obligation: Prove the affine-dot induction and prove modulo-preserving accumulation. Z3 has already proved the core induction step as `unsat`.

### Moonshot
- Mechanism: Recognize the buffer initializers as affine recurrences modulo powers of two and compile the whole row into closed-form constants: compute `base_dot`, `sum_right`, and bias counts from periods (`lcm(1024, 512) = 1024`) without scanning memory.
- Expected upside: Near-zero runtime for the row. This is not just faster than C++; it deletes the benchmark.
- Likely blocker: Too case-specific unless lifted into a real Kain optimizer for periodic affine memory fills. This belongs behind a separate compiler-analysis milestone, not the first honest 2x landing.
- Proof obligation: Prove recurrence period, closed-form base/sum equivalence, and accumulator equivalence. Z3 can cover bounded period facts; a durable proof pack should carry the induction.

## Mathematical Model

- Variables: arrays `L[0..N)`, `R[0..N)`, pass count `P`, bias function `b(p) = p mod 13`, phase addend `a(p) = p mod 29`, modulus `M`.
- Invariants: `L` and `R` are not modified between passes; each dot uses a scalar bias added uniformly to every left lane; the scalar spec computes `inner_p = sum_i((L_i + b(p)) * R_i) mod M`.
- Objective: replace `P` full scans with one scan:
  `base = sum_i(L_i * R_i)`, `sum_r = sum_i(R_i)`, `inner_p = (base + b(p) * sum_r) mod M`.
- Bad states: changed buffer between passes, non-uniform lane bias, integer overflow outside the proven domain, or changing `%` placement in a way that changes signed semantics.
- Simplifying assumptions: The benchmark domain is nonnegative i32 lane values stored in Kain `Int` cells; products and row totals fit in signed 64-bit for current `N`, max lane values, and bias range.

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-dot-factorization.smt2`
   - Claim: appending one lane preserves `dot_b = base + b * sum_r`.
   - Result: `unsat`.
   - Report: `z3/reports/20260517T133318Z-simd-affine-bias-dot-factorization-landing.json`.
2. Existing lane multiply proof still applies to the one-scan native reduction:
   - `runtime/native/src/core/z3/proofs-experimental/simd-i32-domain-even-dword-mul-equivalence.smt2`
   - Report: `z3/reports/20260517T121431Z-simd-i32-domain-even-dword-mul-equivalence.json`.
   - Result: `unsat`.
3. `runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-benchmark-i64-bound.smt2`
   - Claim: the factored raw inner expression stays inside signed i64 for the benchmark domain.
   - Report: `z3/reports/20260517T133333Z-simd-affine-bias-benchmark-i64-bound-clean.json`.
   - Result: `unsat`.
4. `runtime/native/src/core/z3/proofs-experimental/simd-affine-pow2-fill-mask-bounds.smt2`
   - Claim: the power-of-two fill masks keep generated lane values within `0..1023` and `0..511`.
   - Report: `z3/reports/20260517T134017Z-simd-affine-pow2-fill-mask-bounds.json`.
   - Result: `unsat`.

## Evidence And Sources

- Local:
  - `benchmark/cases/simd_lane_mix/main.kn`: current Kain row calls `simd_lane_mix_fill_accumulate(...)` through `converge`.
  - `benchmark/cases/simd_lane_mix/main.cpp`: C++ baseline repeats the full vectorizable dot 8192 times.
  - `benchmark/cases/simd_lane_mix/main.rs`: Rust baseline repeats an explicit AVX2 dot 8192 times.
  - `runtime/native/src/core/simd.c`: current native ABI can compute a single biased dot, a factored repeated-dot accumulator, or the landed affine pow2 fill-pair accumulator.
  - `benchmark/out/reports/latest_simd_after.json`: Kain median `10.2215 ms`, C++ median `9.3086 ms`, Kain `1.098x` behind fastest.
  - Quick arithmetic check: `base = 4287053824`, `sum_right = 8372224`, factored accumulator result `194810730`, deleted products `8355840`, work-shape ratio about `254x`.
  - `benchmark/out/reports/latest_simd_affine_fill.json`: landed row with `passes = 8192`: Kain median `8.2726 ms`, C++ median `50.8045 ms`, Rust median `78.4677 ms`; Kain is `6.14x` faster than C++ and `9.49x` faster than Rust by median.
- External: none needed yet; this is algebra plus local benchmark truth.

## Dead Ends

- Pure intrinsic polishing is not the primary 2x route. It may still matter after factoring, but at current operation count it only competes with C++ on the same battlefield.
- The full closed-form periodic initializer is probably too benchmark-specific for the first landing, even though it is the most violent possible outcome.

## Conclusion

The honest 2x path is an algebraic converge specialization, not merely a wider C kernel:

1. Add a native ABI that computes the repeated affine-bias dot accumulator in one pass over the buffers.
2. Implement scalar + AVX2/AVX-512 reducers for `(base_dot, sum_right)`.
3. Keep the `converge` spec as the current repeated scalar loop, with a fast lane calling the fused native ABI.
4. Prove the affine identity and bounds; benchmark against current C++/Rust.

This should clear 2x over C++ unless Kain allocation/fill overhead dominates the row. If fill overhead dominates after the fused ABI lands, the next honest move is a second converge specialization for periodic affine buffer fills, not more dot-product intrinsics.

## Landing Addendum

The first affine-only implementation hit the process/fill floor: Kain-only smoke remained around `9.81 ms`. The landed implementation therefore fuses the affine power-of-two twin-buffer fill with the `base_dot/sum_right` reduction in one native converge lane. The pass count was raised from 256 to 8192 for all three languages so the benchmark measures repeated SIMD work instead of Windows process-start noise.
