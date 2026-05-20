# Typed Ephemeral Float Stack Lowering Assessment

- date: `2026-05-20`
- focus: `LLVM ephemeral helper-buffer lowering for ptr<Float> benchmarks`
- evidence:
  - targeted CFD probe: `benchmark/out/reports/latest_sim_cfd_after_typed_ephemeral.llm.md`
  - full suite snapshot: `benchmark/out/reports/latest.llm.md`
  - proof lane: `crates/kain-sys-codegen/z3` memory lane report `20260520T052151Z-2026-05-20-typed-ephemeral-float-stack-lowering.json`

## What changed

- The LLVM backend now recovers original authored pointer element types across low-level memory normalization so the ephemeral helper-buffer fast path can keep `ptr<Float>` locals as typed `double` stack storage instead of falling back to `[N x i64]` plus `i8*` bitcasts.
- Real emitted LLVM for `sim_cfd_pressure_projection` now shows `[240 x double]`-style allocas and typed `getelementptr double`.
- Targeted LLVM regression tests for decay-only and sim-style float buffers pass.
- The sys-codegen memory proof lane stayed fully proved (`11/11` `unsat`).

## Honest performance result

- `sim_cfd_pressure_projection` improved materially.
- Before the typed lowering investigation probe, the honest frontier read was about `13.771 ms` Kain vs `9.492 ms` C++ vs `12.187 ms` Rust.
- After the typed lowering landed, the targeted rerun measured `11.585 ms` Kain vs `9.411 ms` C++ vs `12.587 ms` Rust.
- That is about a `1.19x` Kain speedup against the pre-fix probe, enough to move Kain ahead of Rust on the row while still leaving a real C++ gap.
- The benchmark already exercises the requested deadline surface through `deadline_millis` and `deadline_elapsed` in `benchmark/cases/sim_cfd_pressure_projection/main.kn`.

## Full-suite frontier after rerun

The current suite is not lacking for useful work. The highest-value honest Kain gaps exposed by the latest full run are:

1. `alloc_churn`
- Kain `305.023 ms`, Rust `14.609 ms`, C++ `101.411 ms`
- This is the largest obvious metal hole in the suite.
- Attack surface: helper allocation path, ownership/runtime protocol overhead, allocator reuse, zero-fill policy, and fresh-allocation elision.

2. `http_server_concurrency`
- Kain `64.791 ms`, Rust `48.491 ms`
- This remains the clearest real runtime/network frontier after the older request-slot exhaustion failure was fixed.
- Attack surface: request batching, accept/dispatch overlap, fixed-response caching, and lower syscall/queue churn.

3. `crypto_block_cipher`
- Kain `19.802 ms`, Rust `12.292 ms`, C++ `13.638 ms`
- Strong candidate for solver-guided constant/layout work and lower-level rotate/xor/add codegen improvements.

4. `sim_cfd_pressure_projection`
- Kain `12.987 ms`, C++ `11.350 ms`, Rust `13.114 ms`
- Still worth future work, but no longer the most urgent frontier after the typed stack fix.

5. `memory_stream` and `ownership_memory`
- `memory_stream`: Kain `14.185 ms` vs Rust `12.251 ms`
- `ownership_memory`: Kain `13.634 ms` vs C++ `11.752 ms`
- These are smaller but probably share infrastructure with the `alloc_churn` hole.

## What not to do next

- Do not create new benchmark cases yet just to manufacture work. The current suite already exposes multiple large, real, non-proxy gaps.
- Do not chase the old `54.961 ms` CFD outlier from the earlier full report as if it were stable truth. Focused reruns show that number was noise-tainted rather than the real steady-state frontier.

## Full-suite caveat

- The full benchmark rerun produced the canonical `latest` report, but the suite status was `FAIL`.
- The failure was not caused by the typed float stack lowering. The failing row was `process_stdio_loop`, and the failure mode was Windows executable/link-file locking during benchmark builds.
- In one rerun the lock hit the Kain row (`LNK1104` on `process_stdio_loop.exe`); in the earlier full run the failure surfaced on the C++ row. That points to harness/build-artifact churn, not a semantic regression in the typed-ephemeral lowering.

## Recommendation for the next agent

1. Attack `alloc_churn` first. This is the biggest remaining honest benchmark embarrassment and is likely coupled to reusable allocator/runtime wins.
2. Carry the same solver-backed rigor into `crypto_block_cipher` if the allocator frontier stalls; that row is ideal for `$z3-black-magic-optimizer`.
3. After the allocator/runtime pass, retest `memory_stream`, `ownership_memory`, and `sim_cfd_pressure_projection` together because they likely benefit from the same substrate changes.
4. Keep `http_server_concurrency` as the main runtime systems frontier once allocator pressure is less pathological.
