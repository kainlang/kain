# CFD Jacobi Ping-Pong Latest Benchmark Assessment

- Date: `2026-05-20`
- Focus: `sim_cfd_pressure_projection`
- Evidence:
  - focused frontier snapshot: `benchmark/out/reports/latest_frontier_reality.llm.md`
  - focused post-change retake: `benchmark/out/reports/latest_sim_cfd_ping_pong.llm.md`
  - canonical full suite: `benchmark/out/reports/latest.llm.md`
  - proof report: `z3/reports/20260520T202738Z-sim-cfd-linearized-bounds-2026-05-20.json`

## Why This Frontier

The most recent focused benchmark truth before the edit showed CFD as the cleanest honest sim deficit still worth attacking locally:

- Kain: `11.931 ms`
- Rust: `10.629 ms`
- C++: `11.040 ms`

That gap was not large enough to justify a fake benchmark-domain shortcut, but it was large enough to justify deleting wasted work inside the authored solver lane.

## Landed Shape

- `benchmark/cases/sim_cfd_pressure_projection/main.kn`
  - keeps the same fixed-grid divergence, Jacobi, and gradient-subtract solver
  - keeps the same checksum and deadline guard
  - removes the old per-step pressure copy by alternating the Jacobi source/destination buffers across even and odd iterations
  - preserves correctness for odd iteration counts with a post-loop `pressure_old -> pressure` copy fallback
- `benchmark/benchmarks.json`
  - updates the Kain language note so future reports describe the shipped lane accurately: raw `Float` buffers, flattened row/plane arithmetic, and ping-pong Jacobi pressure buffers

## Proof And Validation

- Bounds proof:
  - `benchmark/cases/sim_cfd_pressure_projection/proofs-experimental/sim-cfd-linearized-bounds.smt2`
  - `mcp__z3_local__.check_smt2(...)` result: `unsat`
- Focused post-change retake:
  - `benchmark/out/reports/latest_sim_cfd_ping_pong.llm.md`
  - Kain `10.649 ms`, Rust `10.799 ms`, C++ `10.078 ms`
- Canonical full-suite rerun:
  - `python benchmark/run.py --timeout 900 --baseline-mode refresh-foreign`
  - `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T20:31:03.373438+00:00`
  - suite status: `PASS`

## Honest Outcome

This pass is a real improvement, but the honest claim is narrower than a frontier flip headline:

- focused retake improvement: `11.931 -> 10.649 ms` for Kain, about a `10.7%` speedup
- canonical full-suite improvement versus the previous comparable run: `9.384 -> 9.299 ms` for Kain, about a `0.9%` speedup
- canonical latest winner: Kain now leads the row at `9.299 ms` vs Rust `10.976 ms` vs C++ `10.881 ms`

The reason the focused gain is larger than the canonical delta is simple: the scary CFD narrative was already partly a measurement/frontier-selection problem. The ping-pong rewrite still helped, but it landed on top of a row that was closer to healthy than the earlier chatter implied.

## Honesty

This is not a benchmark cheat.

- The row still runs the declared pressure-projection solver.
- No step count, checksum contract, or deadline guard changed.
- The rejected moonshot for this pass was a closed-form or benchmark-domain shortcut for the stencil itself. That might have produced a louder number, but it would stop measuring the advertised workload.

## What Matters Next

The current benchmark suite is still rich enough that new rows are not needed yet. After this pass, the highest-value remaining honest frontiers are:

1. `process_stdio_loop`: Kain `4941.831 ms`, Rust `4699.807 ms`
2. `http_server_concurrency`: Kain `57.601 ms`, Rust `43.539 ms`
3. `sim_uv_velocity_grid`: Kain `15.171 ms`, C++ `14.236 ms`
4. `unicode_string_heavy`: Kain `9.857 ms`, C++ `8.405 ms`
5. `crypto_block_cipher`: Kain `11.730 ms`, C++ `11.355 ms`
