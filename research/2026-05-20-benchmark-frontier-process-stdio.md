# Benchmark frontier process stdio speedup hunt

- Date: 2026-05-20
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `benchmark-frontier-process-stdio`

## Research Question

Which native/runtime/compiler changes can honestly flip process_stdio_loop and nearby benchmark frontiers without cheating?

## Constraints

- Keep the benchmark truthful: no benchmark-only command batching, no hardcoded stdout bypass, no shell-result forgery.
- Favor general runtime wins that also help adjacent rows when the mechanism is shared.
- Validate with the real benchmark runner and then the canonical full suite, not only ad hoc stopwatch runs.
- Touch the requested `deadline_millis` / `deadline_elapsed` surface once in the Kain benchmark row.

## Hypothesis Lattice

### Baseline
- Mechanism: delete wasted per-launch work inside the Windows process output lane by routing invisible stderr to `NUL`, caching reusable `NUL` handles, and giving `CreateProcessW` a cached `cmd.exe` application path.
- Expected upside: claw back the host-tax gap in `process_stdio_loop` without changing what the API returns.
- Likely blocker: the row is dominated by Windows shell/process overhead, so each individual micro-cut may only buy a few percent.
- Proof obligation: preserve stdout/exit-code behavior and keep the benchmark checksum guard unchanged.

### Unconventional
- Mechanism: make attrition-disabled really mean disabled in benchmark builds by short-circuiting attrition event hooks, raw clock/sleep bookkeeping, and heap telemetry paths when capture is not configured.
- Expected upside: broad runtime speedups across process-heavy, allocation-heavy, and deadline-touching rows instead of only one benchmark.
- Likely blocker: if any non-attrition surface was implicitly depending on disabled-mode attrition counters, the fast-path could hide diagnostics.
- Proof obligation: only remove work that was already semantically disabled by `config.enabled == 0`.

### Moonshot
- Mechanism: replace repeated shell launches with benchmark-specific command batching or a specialized pre-spawn worker.
- Expected upside: likely 2-10x on the single row.
- Likely blocker: it would stop measuring the declared host-substrate cost and would be benchmark cheating.
- Proof obligation: rejected on honesty grounds before implementation.

## Mathematical Model

- Variables:
  - `T_spawn`: fixed child-process launch cost
  - `T_shell_lookup`: executable resolution tax
  - `T_stdio`: pipe/null-handle setup and drain cost
  - `T_attrition_disabled`: benchmark-release bookkeeping that should collapse to zero when attrition capture is off
  - `N = 300`: launches per run
- Invariants:
  - stdout must still equal `process-bench\r\n` on every round
  - the benchmark checksum guard must stay `5988`
  - nonzero exit codes must still fail the row
- Objective: minimize `N * (T_spawn + T_shell_lookup + T_stdio + T_attrition_disabled)` without introducing benchmark-specific shortcuts.
- Bad states:
  - changing the declared benchmark shape
  - hiding stderr semantics that the API actually surfaces
  - turning disabled attrition into a semantic change instead of a bookkeeping collapse
- Simplifying assumptions:
  - `process_output_text(...)` only promises stdout and exit status, not captured stderr bytes
  - benchmark-release does not configure attrition capture unless an attrition result path is explicitly present

## Z3 Claims

1. `benchmark/cases/process_stdio_loop/proofs-experimental/process-stdio-loop-checksum.smt2`
   - Claim: the touched Kain row still has the exact checksum guard `5988`.
   - Result: `unsat` via `z3/reports/20260520T122202Z-process-stdio-loop-checksum.json`.
2. Rejected moonshot lane:
   - No solver-backed black-magic reducer was worth landing because the frontier was dominated by host/runtime tax, not a finite arithmetic classifier.

## Evidence And Sources

- Local:
  - Focused retakes:
    - `benchmark/out/reports/latest_process_stdio_frontier.llm.md`
    - pass 1: Kain `5883.793 ms`, Rust `5338.471 ms`
    - pass 2: Kain `5577.407 ms`, Rust `5338.471 ms`
    - pass 3: Kain `5486.127 ms`, Rust `5338.471 ms`
  - Canonical full suite:
    - `benchmark/out/reports/latest.llm.md`
    - `process_stdio_loop`: Kain `5487.617 ms`, Rust `5687.132 ms`, C++ `9695.726 ms`
    - `ownership_memory`: Kain `10.671 ms`, Rust `11.119 ms`, C++ `11.952 ms`
    - `memory_stream`: Kain `9.522 ms`, Rust `9.964 ms`, C++ `10.418 ms`
    - `alloc_churn`: Kain `8.253 ms`, Rust `10.729 ms`, C++ `9.922 ms`
  - Durable prior canonical baseline from `MEMORY.md`:
    - `process_stdio_loop`: Kain `6809.287 ms`, Rust `5174.384 ms`
- External:
  - None. This pass stayed entirely on repo-local runtime and benchmark evidence.

## Dead Ends

- Rejected the benchmark-specific shell batching idea. It probably wins the row spectacularly, but it would no longer measure the declared repeated child-process substrate cost.
- Rejected a direct blocking post-exit pipe-drain rewrite for now because it risks hanging on descendants that inherit the output handle.

## Conclusion

Landed the baseline lane plus the unconventional configuration-truth lane.

- In `runtime/native/src/core/process_system.c`, `process_output_text(...)` now:
  - routes invisible stderr to `NUL`
  - reuses cached `NUL` handle templates
  - resolves `cmd` / `cmd.exe` through a cached application path
- In `runtime/native/src/core/attrition.c`, disabled attrition now fast-paths out of event hooks, raw time bookkeeping, and heap/RC telemetry paths instead of paying lock-heavy no-op work in benchmark builds.
- The focused 5-run probe still leaves Kain a few percent behind Rust, which is honest and important.
- The canonical 9-run full suite flips `process_stdio_loop` all the way into a Kain win and also lifts several allocation/runtime rows for free.

Best next frontier after this pass: `http_server_concurrency`, now the clearest remaining honest runtime gap in the canonical suite at Kain `125.680 ms` vs Rust `40.919 ms`.
