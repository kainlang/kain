# Sim Cfd Frontier Hunt

- Date: 2026-05-20
- Status: completed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `sim-cfd-frontier-hunt`

## Research Question

Why did `sim_cfd_pressure_projection` look like a frontier collapse in the latest benchmark chatter, and what honest speedup was still available without cheating the solver shape?

## Constraints

- Keep the declared projection-core benchmark intact: buoyancy, divergence, Jacobi solve, gradient subtract, checksum guard.
- Preserve the live deadline touch through `deadline_millis(0)` / `deadline_elapsed(...)`.
- Prefer a substrate-real speedup over a benchmark-only algebraic shortcut.
- Finish with a full `benchmark/run.py` suite rerun, not only a focused probe.

## Hypothesis Lattice

### Baseline
- Mechanism: remove the per-step `pressure -> pressure_old` copy by ping-ponging the Jacobi buffers across even/odd iterations.
- Expected upside: low-double-digit percent on focused retakes because the hot loop stops paying a full-cell copy every step.
- Likely blocker: if LLVM was already eliding the copy or if memory traffic was not the dominant cost, the gain would be small.
- Proof obligation: linearized CFD index bounds stay proved and the final `pressure` buffer identity stays unchanged through an odd-iteration fallback copy.

### Unconventional
- Mechanism: rewrite the fixed-size CFD work arrays into compile-time-sized local arrays and let the backend stack-specialize the whole stencil.
- Expected upside: potentially larger than ping-pong because the row dimensions are fully static.
- Likely blocker: this checkout rejected `[Float; N]` / repeated-literal array syntax in the benchmark lane, so the representation move was not immediately authorable.
- Proof obligation: same bounds proof plus equivalent final checksum over the fixed grid.

### Moonshot
- Mechanism: solver-derived closed form or spectral shortcut for the fixed stencil.
- Expected upside: huge.
- Likely blocker: it would stop benchmarking the declared projection-core loop and become a benchmark-domain cheat.
- Proof obligation: full equivalence over the authored 140-step fixed-grid solver, which is the wrong target for this benchmark.

## Mathematical Model

- Variables: `pressure`, `pressure_old`, `divergence`, `velocity_x`, `velocity_y`, `velocity_z`, fixed dimensions `nx=8`, `ny=6`, `nz=5`, `jacobi_iters=8`.
- Invariants: every slot/cell access stays in the proved linearized domain; the final checksum and deadline guard remain unchanged; odd Jacobi counts still end with the final field in `pressure`.
- Objective: reduce stencil memory traffic per sim step without changing the authored solver semantics.
- Bad states: out-of-bounds slot math, checksum drift, deadline regression, or benchmark-only math that no longer measures the stated pressure projection.
- Simplifying assumptions: the fixed benchmark dimensions and `jacobi_iters=8` remain constant for the measured row, but the implementation still preserves correctness if that iteration count changes later.

## Z3 Claims

1. The existing linearized CFD bounds proof still applies because the ping-pong rewrite changes source/destination buffer choice, not the index domains.
2. The final observed `pressure` state is preserved because odd Jacobi counts copy `pressure_old` back into `pressure` after the loop.

## Evidence And Sources

- Local:
  - `benchmark/out/reports/latest_frontier_reality.llm.md`
  - `benchmark/out/reports/latest_sim_cfd_ping_pong.llm.md`
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/cases/sim_cfd_pressure_projection/main.kn`
  - `benchmark/cases/sim_cfd_pressure_projection/proofs-experimental/sim-cfd-linearized-bounds.smt2`
  - `z3/reports/20260520T202738Z-sim-cfd-linearized-bounds-2026-05-20.json`
- External:
  - None. This pass was entirely repo-local.

## Dead Ends

- A fixed-array rewrite was rejected by the current parser (`[Float; VX_COUNT]` and repeated-literal array forms were not accepted in this checkout).
- `$z3-black-magic-optimizer` candidate scans on `sim_cfd_pressure_projection`, `recursive_sum`, and `option_result` did not reveal a compelling alien-constant replacement that stayed honest to the benchmark contract.

## Conclusion

The scary 5.6x CFD story was not real frontier truth.

- The focused retake that motivated the pass showed a real but modest gap: `11.931 ms` Kain vs `10.629 ms` Rust vs `11.040 ms` C++.
- The kept change was the honest ping-pong Jacobi rewrite in `benchmark/cases/sim_cfd_pressure_projection/main.kn`, which cut the focused retake to `10.649 ms` Kain vs `10.799 ms` Rust vs `10.078 ms` C++.
- The bounds proof stayed proved: `mcp__z3_local__.check_smt2(...)` over `sim-cfd-linearized-bounds.smt2` returned `unsat`.
- The full canonical suite (`benchmark/out/reports/latest.llm.md`, generated `2026-05-20T20:31:03.373438+00:00`) stayed `PASS` and now shows `sim_cfd_pressure_projection` as a Kain win at `9.299 ms` vs Rust `10.976 ms` vs C++ `10.881 ms`.

The current suite is not lacking for useful work, so there is no need to invent new benchmark rows yet. After this pass, the next honest frontiers are:

1. `process_stdio_loop`: Kain `4941.831 ms` vs Rust `4699.807 ms`
2. `http_server_concurrency`: Kain `57.601 ms` vs Rust `43.539 ms`
3. `sim_uv_velocity_grid`: Kain `15.171 ms` vs C++ `14.236 ms`
4. `unicode_string_heavy`: Kain `9.857 ms` vs C++ `8.405 ms`
5. `crypto_block_cipher`: Kain `11.730 ms` vs C++ `11.355 ms`
