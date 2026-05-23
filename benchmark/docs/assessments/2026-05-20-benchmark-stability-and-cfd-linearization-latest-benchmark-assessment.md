# 2026-05-20 - Benchmark stability hardening plus CFD linearization latest benchmark assessment

This automation pass started from a misleading benchmark picture: the canonical `latest` suite was making `sim_cfd_pressure_projection` look like a 5.6x Kain loss even though focused retakes kept landing near parity. The kept result is twofold:

- the CFD Kain row now stays in explicit row/plane arithmetic instead of repeated index-helper composition, which pulls the focused median down materially without changing the solver contract
- the benchmark runner now treats measurement and Windows build churn honestly enough that `latest` stops failing on transient output locks or advertising obvious outlier noise as the frontier

## What changed

- `benchmark/run.py`
  - default latest snapshots now come from the manifest-owned `3` warmups / `9` timed-run profile instead of `2` / `7`
  - reports emit `Stability Alerts` when a language shows outlier-heavy timing samples instead of silently letting one spike impersonate a regression
  - direct Kain/Rust/C++/Go/Zig build outputs are purged before rebuild, and Windows linker-style permission failures now retry instead of poisoning the whole suite
  - Kain case runs retry once after purging case-local `generated/native_runtime` cache when the native cache leaves behind a transient `.tmp` miss
- `benchmark/cases/sim_cfd_pressure_projection/main.kn`
  - the hot divergence/Jacobi/gradient-subtract loop is flattened into explicit row/plane index arithmetic with precomputed scales
  - the checksum contract is unchanged; the row remains the same fixed-grid pressure projection, just expressed in a shape LLVM can see more directly
- Durable proof surface already tracked on `HEAD`:
  - `benchmark/cases/sim_cfd_pressure_projection/proofs-experimental/sim-cfd-linearized-bounds.smt2`

## Proof and validation

- Z3 bounds proof:
  - `benchmark/cases/sim_cfd_pressure_projection/proofs-experimental/sim-cfd-linearized-bounds.smt2`
  - report: `z3/reports/20260520T051221Z-sim-cfd-linearized-bounds.json`
  - result: `unsat`
- Focused CFD before rewrite:
  - `benchmark/out/reports/latest_sim_cfd_probe_before.llm.md`
  - Kain `11.041 ms`, Rust `10.667 ms`, C++ `9.657 ms`
- Focused CFD after linearization:
  - `benchmark/out/reports/latest_sim_cfd_linearized.llm.md`
  - Kain `10.334 ms`, Rust `10.336 ms`, C++ `9.870 ms`
- Pipeline/regression validation:
  - `benchmark/out/reports/latest_sim_cfd_after_pipeline.llm.md`
  - `benchmark/out/reports/latest_contention_lockfix.llm.md`
  - `benchmark/out/reports/latest_process_lockfix.llm.md`
- Canonical full-suite truth:
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T05:49:15.103727+00:00`
  - suite status: `PASS`

## Final benchmark truth

The current latest suite says the scary CFD regression was not real frontier truth. After the runner hardening and the already-tracked linearized Kain row, the honest latest snapshot is:

- `sim_cfd_pressure_projection`: Kain `12.736 ms`, Rust `12.471 ms`, C++ `12.054 ms`
- `struct_method`: Kain `10.702 ms`, Rust `15.511 ms`, C++ `15.064 ms`
- `unicode_string_heavy`: Kain `8.962 ms`, Rust `11.484 ms`, C++ `9.314 ms`
- `crypto_block_cipher`: Kain `11.251 ms`, Rust `13.338 ms`, C++ `14.068 ms`
- `native_map_lookup`: Kain `17.653 ms`, Zig `18.060 ms`, Rust `34.624 ms`, C++ `90.250 ms`

## Frontier ranking

Highest-value remaining Kain losses in the latest full suite:

1. `process_stdio_loop`
   Kain is still `1.32x` behind Rust (`6809.287 ms` vs `5174.384 ms`). This is the biggest remaining honest implemented gap.
2. `http_server_concurrency`
   Kain is still `1.18x` behind Rust (`57.447 ms` vs `48.491 ms`). This remains the highest-value runtime/native HTTP gap.
3. `recursive_sum`
   Kain is `1.12x` behind Rust (`10.566 ms` vs `9.465 ms`).
4. `ownership_memory`
   Kain is `1.09x` behind C++ (`13.178 ms` vs `12.117 ms`).
5. `memory_stream`
   Kain is `1.08x` behind C++ (`11.727 ms` vs `10.862 ms`).
6. `sim_cfd_pressure_projection`
   Kain is now only `1.06x` behind C++ (`12.736 ms` vs `12.054 ms`).
7. `ffi_shared_call_stress`
   Kain is `1.04x` behind C++ (`55.757 ms` vs `53.392 ms`).

Pure C++ frontier after this pass:

1. `ownership_memory`
2. `memory_stream`
3. `sim_cfd_pressure_projection`
4. `ffi_shared_call_stress`

## Durable lesson

- The most valuable speedup was not another benchmark-local trick; it was stopping the suite from lying. Once `latest` moved to a sturdier measurement profile and started surfacing instability, fake 5x regressions stopped hijacking the frontier queue.
- The CFD row still matters, but it is no longer a compiler emergency. The remaining gap is now a small real stencil deficit, not a benchmark hallucination.
- The next serious pass should target `process_stdio_loop` or `http_server_concurrency` before polishing the small C++ deltas.
