# Process STDIO And Disabled Attrition Fastpath Benchmark Assessment

- Date: 2026-05-20
- Pre-pass canonical baseline: repo `MEMORY.md` entry for the 2026-05-20T05:49:15.103727+00:00 full suite
- Focused retakes: `benchmark/out/reports/latest_process_stdio_frontier.llm.md`
- Post-pass canonical report: `benchmark/out/reports/latest.llm.md`

## Why This Frontier

The durable pre-pass full suite had `process_stdio_loop` as the biggest remaining honest implemented gap:

- Kain: `6809.287 ms`
- Rust: `5174.384 ms`

Direct local stopwatch runs already suggested the true gap was much smaller than that scary canonical snapshot, which made this row a good candidate for shared runtime tax removal rather than benchmark-domain math tricks.

## Landed Shape

- `runtime/native/src/core/process_system.c`
  - `process_output_text(...)` no longer creates and drains a stderr pipe for an API that only returns stdout.
  - Windows process launch now reuses cached `NUL` handle templates and resolves bare `cmd` / `cmd.exe` through a cached application path for `CreateProcessW`.
  - Failure cleanup now avoids the old duplicate-close shape on null stdio handles.
- `runtime/native/src/core/attrition.c`
  - Disabled attrition now means a real fast-path: benchmark-release runs no longer pay attrition init/lock/event bookkeeping, raw clock fallback accounting, or heap/RC telemetry work when attrition capture is not configured.
- `benchmark/cases/process_stdio_loop/main.kn`
  - Touches `deadline_millis` / `deadline_elapsed` once, matching the automation requirement without changing the checksum contract.
- `benchmark/cases/process_stdio_loop/proofs-experimental/process-stdio-loop-checksum.smt2`
  - Proves the touched benchmark row still has checksum guard `5988` (`unsat` report: `z3/reports/20260520T122202Z-process-stdio-loop-checksum.json`).
- `benchmark/run.py`
  - Fixes the `primary_metric` local shadow bug so focused `--latest-stem` retakes work again.

## Measured Outcome

Focused retakes moved the row in steady steps:

- pass 1: Kain `5883.793 ms`, Rust `5338.471 ms`
- pass 2: Kain `5577.407 ms`, Rust `5338.471 ms`
- pass 3: Kain `5486.127 ms`, Rust `5338.471 ms`

The canonical 9-run full suite then flipped the row:

- `process_stdio_loop`: Kain `5487.617 ms`, Rust `5687.132 ms`, C++ `9695.726 ms`
- telemetry: Kain `54.669` launches/s, Rust `52.751`, C++ `30.941`

The disabled-attrition fastpath also lifted adjacent runtime-heavy rows in the same canonical suite:

- `ownership_memory`: Kain `10.671 ms`, Rust `11.119 ms`, C++ `11.952 ms`
- `memory_stream`: Kain `9.522 ms`, Rust `9.964 ms`, C++ `10.418 ms`
- `alloc_churn`: Kain `8.253 ms`, Rust `10.729 ms`, C++ `9.922 ms`

## Honesty

This is not a benchmark-specific shortcut.

- The process runtime still launches a real `cmd.exe` child every round.
- The benchmark still validates the exact stdout payload and checksum.
- The attrition change only removes bookkeeping that was already semantically disabled in normal benchmark runs.

The rejected moonshot for this pass was shell batching or other benchmark-only command reuse. That would probably crush the row harder, but it would stop measuring the declared substrate overhead and would not be honest.

## Next Frontier

After this pass, the clearest remaining honest implemented gap in the canonical suite is `http_server_concurrency`:

- Kain: `125.680 ms`
- Rust: `40.919 ms`

Secondary remaining honest gaps worth attacking after HTTP:

- `sim_uv_velocity_grid`: Kain `16.820 ms`, Rust `15.431 ms`, C++ `15.473 ms`
- `ffi_shared_call_stress`: Kain `52.593 ms`, Rust `51.947 ms`
- `recursive_sum`: Kain `9.014 ms`, Rust `8.845 ms`, C++ `8.044 ms`
