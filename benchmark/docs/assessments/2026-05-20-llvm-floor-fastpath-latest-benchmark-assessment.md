# LLVM Floor Fastpath Assessment

- date: `2026-05-20`
- focus: `compiler-owned LLVM floor lowering for floor-heavy sim benchmarks`
- evidence:
  - targeted probe: `benchmark/out/reports/latest_floor_probe.llm.md`
  - canonical full suite: `benchmark/out/reports/latest.llm.md`
  - proof lane: `crates/kain-sys-codegen/z3/proofs-experimental/floor-fastpath-defined-domain.smt2`
  - proof report: `z3/reports/20260520T195910Z-20260520T1932Z-floor-fastpath-defined-domain.json`

## What changed

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - emits `declare double @llvm.floor.f64(double)` in the LLVM prelude
  - lowers direct stdlib `floor(Float) -> Int` calls to `llvm.floor.f64` plus `fptosi`
  - keeps the older runtime wrapper symbol available for the general runtime surface instead of deleting it blindly
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - adds a regression test that proves the emitted IR uses the intrinsic path and does not call `kain_floor_i64`
- `benchmark/cases/sim_uv_velocity_grid/main.kn`
  - touches `deadline_millis` / `deadline_elapsed` once so the row exercises the requested deadline surface without changing the checksum contract

## Honest performance result

- The pre-pass canonical `latest` suite had `sim_uv_velocity_grid` at Kain `17.150 ms`, Rust `15.234 ms`, C++ `14.134 ms`.
- The focused 5-run probe after the codegen change measured:
  - `sim_uv_velocity_grid`: Kain `15.588 ms`, Rust `16.721 ms`, C++ `15.811 ms`
  - `sim_nbody_gravity`: Kain `9.774 ms`, Rust `10.343 ms`, C++ `10.859 ms`
- The canonical full-suite rerun kept the important part of that win:
  - `sim_uv_velocity_grid`: Kain `15.813 ms`, Rust `17.399 ms`, C++ `16.995 ms`
- That is about a `1.08x` improvement against the prior canonical Kain number, and it flips the row from losing to both foreign baselines into an honest Kain win.

## What did not magically flip

- `sim_nbody_gravity` improved in the focused probe, but the canonical full suite still landed at Kain `10.064 ms`, Rust `10.474 ms`, C++ `9.535 ms`.
- `sim_cfd_pressure_projection` did not become a win from this change. The post-pass full suite still shows Kain `9.889 ms` vs C++ `9.210 ms`.
- So the durable claim is not “floor lowering solves the whole sim frontier.” The durable claim is “floor lowering removed a real hot-loop tax and decisively fixed `sim_uv_velocity_grid`.”

## Full-suite frontier after rerun

The latest honest post-pass full suite says the next valuable gaps are:

1. `process_stdio_loop`
- Kain `7052.660 ms`, Rust `4709.323 ms`, C++ `9450.884 ms`
- The row is still worth a serious follow-up because the gap is large and host-heavy.

2. `recursive_sum`
- Kain `14.090 ms`, Rust `10.442 ms`, C++ `11.456 ms`
- Kain lost a row it had previously owned in other runs. This needs a fresh, stability-aware retake plus IR inspection.

3. `sim_cfd_pressure_projection`
- Kain `9.889 ms`, Rust `14.040 ms`, C++ `9.210 ms`
- This is now the clearest remaining sim/compiler gap after `sim_uv_velocity_grid` flipped.

4. `option_result`
- Kain `10.857 ms`, Rust `11.204 ms`, C++ `9.978 ms`
- Small but honest value-semantic pressure gap against C++.

5. `sim_nbody_gravity`
- Kain `10.064 ms`, Rust `10.474 ms`, C++ `9.535 ms`
- The focused probe suggests there may still be a path here, but the canonical run says not to overclaim it yet.

## Recommendation for the next agent

1. Reopen `sim_cfd_pressure_projection` first if the goal is the next compiler-driven sim win.
2. Recheck `recursive_sum` with the benchmark history lane before assuming the current loss is fundamental rather than stability-noise plus a recoverable lowering issue.
3. Keep `process_stdio_loop` in the queue because the absolute loss is too large to ignore, but treat it as a systems/runtime investigation rather than a quick algebraic codegen fix.
