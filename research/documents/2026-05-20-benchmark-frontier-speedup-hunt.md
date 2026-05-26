# Benchmark Frontier Speedup Hunt

- Date: 2026-05-20
- Status: complete
- Repo Root: `D:\Kain-Lang\.codex-tmp\kain-frontier-20260520`
- Session Slug: `benchmark-frontier-speedup-hunt`

## Research Question

Which latest benchmark frontier yields the highest honest Kain win next, and what proof-backed mechanism should land?

## Constraints

- Keep the benchmark row honest: no precomputed checksum shortcuts or benchmark-specific semantic cheats.
- Prefer a compiler/runtime mechanism that benefits multiple floor-heavy rows instead of rewriting a single case.
- Touch the live deadline surface in the benchmark row because the automation explicitly requested it.
- End with a clean full-suite rerun, not only a focused probe.

## Hypothesis Lattice

### Baseline
- Mechanism: remove the out-of-line `kain_floor_i64` call from hot LLVM loops by lowering stdlib `floor(Float) -> Int` directly to `llvm.floor.f64` plus `fptosi`
- Expected upside: win back `sim_uv_velocity_grid`, improve `sim_nbody_gravity`, and shave a few percent off any floor-quantized checksum lane
- Likely blocker: preserving the stdlib contract honestly and not overclaiming wins that only appear in focused retakes
- Proof obligation: show the compiler path is equivalent to the wrapper on the defined domain and verify the IR uses the intrinsic path

### Unconventional
- Mechanism: revisit `http_server_concurrency` with tighter kernel/accept batching or fixed-response cache surgery
- Expected upside: potentially larger absolute runtime win than the sim rows
- Likely blocker: the remaining gap is now dominated by scheduler and socket lifecycle behavior, not a single obvious instruction-selection mistake
- Proof obligation: need a focused systems investigation, not just algebra

### Moonshot
- Mechanism: attack `process_stdio_loop` again if the full suite still shows Kain behind, possibly through spawn/capture amortization or deeper handle lifecycle collapse
- Expected upside: hundreds or thousands of milliseconds on the canonical row
- Likely blocker: Windows process behavior is noisy and previously shifted between wins and losses across suites
- Proof obligation: prove we are not benchmarking artifact churn or harness bias

## Mathematical Model

- Variables: benchmark median time, hot-loop dynamic call count, per-call wrapper overhead, floor-quantized checksum domains
- Invariants: authored checksum contracts stay unchanged; the row must still execute the same loop geometry; full benchmark must pass afterward
- Objective: maximize honest reduction in median Kain time on the worst current sim frontier
- Bad states: benchmark-specific cheating, semantic drift in `floor`, or a focused gain that disappears in the canonical full suite
- Simplifying assumptions: `floor(Float) -> Int` is only interesting on the domain where the floored result is representable as signed 64-bit

## Z3 Claims

1. On the defined domain where the floored result fits in signed 64-bit, the runtime-wrapper result and the intrinsic-lowered result are equal.
2. The proof is algebraic rather than IEEE-complete; the actual backend contract is additionally validated by the LLVM IR regression test and full benchmark rerun.

## Evidence And Sources

- Local:
  - `benchmark/latest.md` from the pre-pass canonical suite
  - `benchmark/out/reports/latest_floor_probe.llm.md`
  - `benchmark/out/reports/latest.llm.md` after the full rerun
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `runtime/native/src/core/core.c`
- External:
  - None. This investigation stayed inside repo evidence plus solver validation.

## Dead Ends

- The older HTTP concurrency worker-lane and exact-frame experiments were not the right immediate frontier for this automation pass. The benchmark notes showed that row had already consumed the obvious queue-shape ideas.
- The earlier typed-ephemeral-float notes were useful context, but the fresh IR inspection showed `sim_uv_velocity_grid` was already using typed `[N x double]` stack allocas, so heap/helper storage was not the remaining bottleneck.

## Conclusion

- The honest highest-value move was the compiler-owned floor fast path, not another benchmark rewrite.
- It flipped `sim_uv_velocity_grid` from Kain loss to Kain win in the canonical full suite: `17.150 ms -> 15.813 ms`.
- The same mechanism helped the focused `sim_nbody_gravity` retake, but that row did not stay a canonical full-suite Kain win, so the durable claim should stay centered on `sim_uv_velocity_grid`.
- The post-pass canonical frontier is now `process_stdio_loop`, `recursive_sum`, `sim_cfd_pressure_projection`, `option_result`, and `sim_nbody_gravity`.
