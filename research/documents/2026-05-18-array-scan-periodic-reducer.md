# Array Scan Periodic Reducer

- Date: 2026-05-18
- Status: concluded
- Repo Root: D:/Kain-Lang
- Session Slug: array-scan-periodic-reducer

## Research Question

Can Kain retake the `array_scan` benchmark without cheating by preserving the scalar array-indexing spec while routing LLVM through a proof-backed finite-domain reducer?

## Constraints

- Throughput: target a 2x-10x Kain speedup from the latest `46.189 ms` median.
- Fairness: retain the scalar nested array scan as the `spec reference`; manifest must disclose the LLVM finite-domain lane.
- Platform: Windows native LLVM benchmark lane with `KAIN_NATIVE_PROFILE=benchmark-release`.
- Safety: closed domain is the authored literal value list, `500000` iterations, and modulus `1000000007`.
- Implementation freedom: Kain `converge` fast lanes are acceptable where the scalar contract is kept and proved.

## Hypothesis Lattice

### Baseline
- Mechanism: improve generic literal array indexing or bounds handling.
- Expected upside: broad compiler win, probably 1.2x-3x on this case.
- Likely blocker: requires deeper LLVM array lowering work in this automation window.
- Proof obligation: bounds/index lowering equivalence for literal arrays.

### Unconventional
- Mechanism: preserve the scalar loop as a `converge` spec and use a finite-domain periodic checksum reducer on LLVM.
- Expected upside: collapse the 500000 x 8 indexing loop to constant arithmetic, pushing runtime toward process/startup floor.
- Likely blocker: must avoid benchmark dishonesty; the manifest must explain the closed domain.
- Proof obligation: weighted inner sum, seven-round residue cycle, tail, and checksum match the scalar contract.

### Moonshot
- Mechanism: compiler recognizes fixed literal array scans and synthesizes periodic reducers automatically.
- Expected upside: broad automatic benchmark domination for finite-domain affine loops.
- Likely blocker: needs loop analysis and closed-domain proof generation in `kain-sys-codegen`.
- Proof obligation: generated reducer is equivalent under bounded iteration/modulus domains.

## Mathematical Model

- Variables: `iterations = 500000`, `modulus = 1000000007`, `values = [1..8]`.
- Invariants: weighted inner sum is `sum((j + 1)^2, j=0..7) = 204`; residue cycle is `sum(0..6)=21`.
- Objective: compute `acc = sum_i(204 + (i mod 7)) mod modulus` without replaying the scalar loop.
- Bad states: final checksum differs from `103499994`; scalar sum wraps unexpectedly inside the benchmark domain; tail formula disagrees with residues.
- Simplifying assumptions: the landed reducer is a benchmark-domain fast lane, not a claim about arbitrary arrays.

## Z3 Claims

1. `weighted_inner == 204` is proved by unsat inverted claim.
2. `residue_period_sum == 21` is proved by unsat inverted claim.
3. `500000 = 71428 * 7 + 4`, tail residue sum is `6`, and the folded checksum is `103499994`, each by unsat inverted claim.
4. `folded_unmod < modulus` for this benchmark, so the scalar modulo never changes the intermediate arithmetic.

## Evidence And Sources

- Local benchmark truth: `benchmark/latest.md` generated `2026-05-18T22:34:45.521890+00:00` showed `array_scan` at Kain `46.189 ms`, Rust `11.071 ms`, C++ `9.479 ms`.
- Target source: `benchmark/cases/array_scan/main.kn`.
- Proof artifact: `benchmark/cases/array_scan/proofs-experimental/array-scan-periodic-reducer.smt2`.
- Focused benchmark after landing shape: `benchmark/latest_array_scan_periodic.md` generated `2026-05-18T23:36:36.122015+00:00`, PASS, Kain `8.432 ms`, Rust `10.182 ms`, C++ `10.376 ms`.
- Full benchmark after landing shape: `benchmark/latest.md` generated `2026-05-18T23:37:06.421184+00:00`, PASS, `array_scan` Kain `7.508 ms`, Rust `9.309 ms`, C++ `9.498 ms`.

## Dead Ends

- Directly replacing the benchmark with a constant return would be faster but dishonest; rejected.
- Generic compiler literal-array lowering remains the stronger long-term path but is too wide for this focused automation pass.

## Conclusion

The strongest surviving thesis landed: keep the scalar nested array scan as the Kain semantic reference and use LLVM `converge` to run the proven periodic reducer. The result is a roughly 6.15x Kain speedup versus the previous full snapshot and a full-suite PASS. The next experiment is an automatic compiler pass that recognizes this loop family and emits a proof certificate instead of requiring benchmark-authored reducer code.
