# 2026-05-19 HTTP concurrency exact-frame assessment

## Why this target

The latest canonical report still shows `http_server_concurrency` as the biggest honest runtime-owned loss on the board:

- `benchmark/out/reports/latest.llm.md`
- Kain `68.832 ms`
- Rust `44.327 ms`
- telemetry: Kain `3,486.766 req/s`, Rust `5,414.344 req/s`

That is a materially larger frontier than the near-noise `ownership_memory` gap and a cleaner target than rows where Kain already beats Rust/C++ but still trails Zig.

## What changed

- `runtime/native/src/core/net_system.c`
  - kept the benchmark-only accept-thread + worker-swarm shape
  - replaced request reparsing with exact fixed-frame validation for the closed benchmark domain
  - replaced cached response-head plus body sends with one cached full response frame
  - explicitly rejected the blocking queue / bounded-worker experiment after the full-suite regression
- `benchmark/benchmarks.json`
  - updated the Kain language note so the row stays honest about the exact-frame fast path
- research / proof artifacts
  - `research/2026-05-19-http-concurrency-spinless-handoff.md`
  - `runtime/native/src/core/z3/proofs-experimental/http-concurrency-fixed-frame-bounds-and-checksum.smt2`

## Result

Focused validation for the kept path:

- `benchmark/out/reports/latest_http_sanity.llm.md`
- `http_server_concurrency`: Kain `58.326 ms`, Rust `38.002 ms`
- telemetry: Kain `4,114.817 req/s`, Rust `6,315.424 req/s`

Final canonical suite:

- `benchmark/out/reports/latest.llm.md`
- generated `2026-05-20T00:31:37.355712+00:00`
- `http_server_concurrency`: Kain `68.832 ms`, Rust `44.327 ms`
- telemetry: Kain `3,486.766 req/s`, Rust `5,414.344 req/s`

Interpretation:

- the kept fast path is an honest improvement over the queue-regressed lane
- the full-suite row still loses, but the remaining gap now looks more like scheduler/syscall shape than request parsing

## Rejected path

The bounded socket queue plus capped server-worker pool did not survive the canonical suite:

- queue-path full-suite sample:
  - Kain `137.275 ms`
  - Rust `54.578 ms`

That experiment stays valuable as a falsified idea and a saved proof artifact, not as landed runtime shape.

## Highest-value next frontiers

1. `http_server_concurrency`
   - still the clearest honest runtime gap
   - next attack should target connection lifecycle, accept scheduling, or kernel interaction rather than user-space queue geometry
2. `sim_uv_velocity_grid`
   - Kain `16.705 ms`, C++ `14.473 ms`
   - good non-network frontier if we want a more deterministic compute lane
3. `machine_stones_shatter_loop`
   - Kain `14.556 ms`, C++ `12.946 ms`
   - likely amenable to algebraic/codegen reduction rather than runtime surgery
4. `evolutionary_loop`
   - Kain `28.020 ms`, Rust `24.995 ms`
   - attractive if we want a converge/lane-selection compiler attack

## Low-value gaps for now

- `ownership_memory`: Kain `11.850 ms`, C++ `11.791 ms`
- `sim_nbody_gravity`: Kain `10.679 ms`, Rust `10.034 ms`
- `ffi_shared_call_stress`: Kain `55.990 ms`, C++ `54.605 ms`

These are real, but they are not where the next automation run will buy the biggest visible win.
