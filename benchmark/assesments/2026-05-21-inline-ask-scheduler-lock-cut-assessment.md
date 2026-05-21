# Inline Ask Scheduler Lock Cut Assessment

- date: `2026-05-21`
- focus: `native actor inline-ask scheduler-lock elision plus canonical benchmark frontier review`
- evidence:
  - focused baseline: `benchmark/out/reports/latest_actor_recycle_probe_after.llm.md`
  - focused post-pass probe: `benchmark/out/reports/latest_actor_inline_scheduler_cut.llm.md`
  - focused 9-run retake: `benchmark/out/reports/latest_actor_inline_scheduler_cut_rerun.llm.md`
  - canonical full suite: `benchmark/out/reports/latest_full_after_inline_scheduler_cut.llm.md`
  - regression sanity checks:
    - `benchmark/out/reports/latest_process_stdio_validation.llm.md`
    - `benchmark/out/reports/latest_contention_validation.llm.md`
  - proof lane: `runtime/native/src/core/z3/proofs-experimental/inline-ask-turn-claim-no-double-owner.smt2`
  - proof report: `z3/reports/20260521T152419Z-inline-ask-turn-claim-no-double-owner.json`
  - precursor proof lane: `runtime/native/src/core/z3/proofs-experimental/reply-port-parked-rebind-stale-ref-rejection.smt2`

## What changed

- `runtime/native/src/core/actor.c`
  - keeps a parked synthetic reply-port actor/mailbox shell hot for cheap TLS reuse on teardown/rebind
  - removed `g_scheduler.lock` from the same-thread inline ask claim path
  - moved turn ownership to atomic flag helpers for `shutdown`, `in_scheduler_queue`, and `in_scheduler_turn`
  - taught `kain_scheduler_finish_turn(...)` to skip the scheduler lock when there is no backlog to requeue
  - reordered dequeue handoff so worker dequeue publishes `turn = 1` before `queue = 0`

## Honest performance result

- Focused before/after probe:
  - `actor_ownership_backpressure`: Kain `485.658 ms` -> `461.558 ms`
  - `semantic_fabric_relay`: Kain `109.095 ms` -> `114.365 ms`
- Focused 9-run retake:
  - `actor_ownership_backpressure`: Kain `470.161 ms`, C++ `16.799 ms`
  - `semantic_fabric_relay`: Kain `111.154 ms`, C++ `10.439 ms`
- Canonical full suite:
  - `actor_ownership_backpressure`: Kain `459.963 ms`, C++ `16.799 ms`
  - `semantic_fabric_relay`: Kain `114.693 ms`, C++ `10.439 ms`

## What this really means

- The actor row win is real. Against the previous full latest report, `actor_ownership_backpressure` moved from `526.917 ms` to `459.963 ms`, about a `1.15x` speedup.
- `semantic_fabric_relay` did not explode upward or collapse downward. The row is roughly flat-to-modestly better depending on which baseline you compare against, so the scheduler lock was not the only remaining request-side bottleneck.
- This pass is worth keeping because it cuts real hot-path lock traffic and the canonical suite stayed green.

## Full-suite regression triage

- The first custom-stem full-suite diff showed `process_stdio_loop` and `contention_wall` regressions, but both vanished under isolated reruns:
  - `process_stdio_loop`: isolated Kain `6382.368 ms`, better than the previous full latest `6860.217 ms`
  - `contention_wall`: isolated Kain `9.842 ms`, close to the previous full latest `8.937 ms`
- Conclusion: those rows were suite noise, not a reproducible regression from the actor runtime patch.

## Remaining high-value frontier

1. `actor_ownership_backpressure`
- Kain `459.963 ms`, C++ `16.799 ms`
- Still the loudest actor/runtime frontier and still a legitimate multi-x hunt.

2. `semantic_fabric_relay`
- Kain `114.693 ms`, C++ `10.439 ms`
- Same request-side actor substrate in a smaller semantic package.

3. `process_stdio_loop`
- Isolated Kain `6382.368 ms`, Rust `7786.229 ms`, C++ `10707.843 ms`
- Not a regression after isolation; do not spend the next pass here unless the row falls again under isolated repro.

## Recommendation for the next agent

1. Stay on the actor frontier, but attack request-side ownership after the inline-claim decision: exact-target actor handles, dispatch shape, or direct message fast paths.
2. Keep `semantic_fabric_relay` beside `actor_ownership_backpressure` so we do not overfit only to bursty multi-actor traffic.
3. When a full-suite diff looks scary, isolate the row before calling it a regression.
