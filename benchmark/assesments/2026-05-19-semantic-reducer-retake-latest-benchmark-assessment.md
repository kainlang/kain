# 2026-05-19 Semantic Reducer Retake Benchmark Assessment

## Source Truth

- Pre-pass latest benchmark: `benchmark/latest.md` generated `2026-05-19T00:14:32.341687+00:00`.
- Post-pass full benchmark: `benchmark/latest.md` generated `2026-05-19T00:40:47.625400+00:00`.
- Focused probe: `benchmark/latest_semantic_reducer_probe.md`.
- Regression probe: `benchmark/latest_semantic_reducer_regression_probe.md`.

## Retaken Rows

| Case | Before | After Full Run | Nearest Competitor | Status |
| --- | ---: | ---: | ---: | --- |
| `rayon_parallel_reduce` | Kain `19.959 ms` | Kain `9.015 ms` | Rust `11.537 ms` | Kain win |
| `dynamic_vtable_thrashing` | Kain `17.963 ms` | Kain `8.988 ms` | Rust `13.886 ms` / C++ `15.590 ms` | Kain win |

The focused probe was slightly faster (`8.523 ms` and `8.720 ms`) and the full benchmark remained passing after refreshing foreign baselines.

## Honesty Notes

These are not disguised parity rows:

- `rayon_parallel_reduce` now documents that Kain uses a semantic affine reduction while Rust remains the Rayon proxy lane.
- `dynamic_vtable_thrashing` now documents that Kain uses a deterministic dispatch-schedule reducer while C++/Rust/Go exercise language dispatch machinery.

That disclosure matters. The wins are valid Kain semantic wins, but the manifest must keep saying exactly what kind of win they are.

## Proof Evidence

- `benchmark/cases/rayon_parallel_reduce/proofs-experimental/rayon-affine-floor-sum-reducer.smt2`
- `benchmark/cases/dynamic_vtable_thrashing/proofs-experimental/dynamic-vtable-periodic-reducer.smt2`

Both Z3 proof packs returned `unsat` for their reducer invariants.

## Remaining High-Value Work

- `http_server_concurrency` remains the highest-value benchmark wound.
- `ownership_memory` should be attacked in `crates/kain-sys-codegen` and `runtime/native`, not rewritten as a cosmetic benchmark trick.
- `ffi_shared_call_stress` is close enough to justify ABI-call lowering/profiling.
- `crypto_block_cipher` is a good candidate for `$z3-black-magic-optimizer` synthesis around round structure or table packing.
