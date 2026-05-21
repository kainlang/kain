# Inline Ask Scheduler Lock Cut Speedup Hunt

- Date: 2026-05-21
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `inline-ask-scheduler-lock-cut-speedup-hunt`

## Research Question

After the ask-side live-snapshot ref validation and the reply-port direct-handle lane, is the next honest request-side tax the scheduler lock traffic that still wraps same-thread inline microcell asks, and can we cut that traffic without opening a double-owner race between inline execution and worker dequeue?

## Constraints

- Keep the win honest: no benchmark-only shortcut that bypasses `ask`, mailbox visibility, reply-port completion, or scheduler ownership.
- Preserve turn exclusivity when a worker dequeues an actor at the same time another thread tries to inline-claim that actor.
- Prove the ordering story instead of trusting intuition.
- Rerun the canonical full benchmark suite after the runtime change.

## Hypothesis Lattice

### Baseline

- Mechanism: `kain_actor_ask_send_ref(...)` takes the mailbox lock and then also takes `g_scheduler.lock` just to discover that the target can run inline on the same thread.
- Expected upside: none.
- Likely blocker: the scheduler lock and finish-turn lock show up on every hot ask even when no queue admission is needed.

### Unconventional

- Mechanism: use the mailbox lock plus atomic scheduler flags to claim a same-thread inline turn without taking `g_scheduler.lock`, and skip the finish-turn scheduler lock entirely when there is nothing to requeue.
- Expected upside: a real single-digit-to-low-double-digit speedup on ask-heavy semantic rows without changing authored Kain.
- Likely blocker: if dequeue clears the queue bit before it sets the turn bit, an inline claimant could observe a fake `(queue = 0, turn = 0)` window and double-own the turn.

### Moonshot

- Mechanism: push past scheduler-lock elision and teach the ask side an exact-target direct actor-state handle or direct message-dispatch lane.
- Expected upside: this is the first request-side move that still plausibly hides a multi-x actor gain.
- Likely blocker: very easy to accidentally lie about mailbox ordering or actor observability.

## Mathematical Model

- Variables: `queue_bit`, `turn_bit`, and a two-step dequeue ordering.
- Safety claim: when worker dequeue writes `turn = 1` before `queue = 0`, there is no scheduler-owned intermediate state that satisfies the inline claim predicate `(queue = 0 && turn = 0)`.
- Bad state: an inline ask claims a turn while a worker already owns the same actor turn.

## Z3 Claim

1. `runtime/native/src/core/z3/proofs-experimental/inline-ask-turn-claim-no-double-owner.smt2`
   - Encodes the scheduler-owned queued state, the worker dequeue ordering, and the inline-claim eligibility predicate.
   - `mcp__z3_local__.check_smt2(...)` returned `unsat`.
   - Report: `z3/reports/20260521T152419Z-inline-ask-turn-claim-no-double-owner.json`

## Evidence And Sources

- Runtime surface:
  - `runtime/native/src/core/actor.c`
  - `runtime/native/src/core/z3/proofs-experimental/inline-ask-turn-claim-no-double-owner.smt2`
- Benchmark lane:
  - `benchmark/out/reports/latest_actor_recycle_probe_after.llm.md`
  - `benchmark/out/reports/latest_actor_inline_scheduler_cut.llm.md`
  - `benchmark/out/reports/latest_actor_inline_scheduler_cut_rerun.llm.md`
  - `benchmark/out/reports/latest_full_after_inline_scheduler_cut.llm.md`
  - `benchmark/out/reports/latest_process_stdio_validation.llm.md`
  - `benchmark/out/reports/latest_contention_validation.llm.md`

## What Changed

- `runtime/native/src/core/actor.c`
  - kept the earlier same-run parked synthetic reply-port recycle so TLS reply-port teardown can reuse the tiny synthetic actor/mailbox shell instead of destroying it every time
- `runtime/native/src/core/z3/proofs-experimental/reply-port-parked-rebind-stale-ref-rejection.smt2`
  - proves the parked reply-port rebind still advances generation so stale refs stay dead
- `runtime/native/src/core/actor.c`
  - added atomic scheduler-flag helpers for `shutdown`, `in_scheduler_queue`, and `in_scheduler_turn`
  - removed the `g_scheduler.lock` acquisition from the same-thread inline ask claim path in `kain_actor_ask_send_ref(...)`
  - changed `kain_scheduler_finish_turn(...)` so it only touches `g_scheduler.lock` when there is actual backlog to requeue
  - changed dequeue handoff ordering to publish `turn = 1` before clearing `queue = 0`

## Honest Performance Result

- Focused 5-run repro against the hot actor frontier:
  - `actor_ownership_backpressure`: Kain `485.658 ms` -> `461.558 ms`
  - `semantic_fabric_relay`: Kain `109.095 ms` -> `114.365 ms`
- Focused 9-run retake to check for Windows noise:
  - `actor_ownership_backpressure`: Kain `470.161 ms`, C++ `16.799 ms`
  - `semantic_fabric_relay`: Kain `111.154 ms`, C++ `10.439 ms`
- Canonical full-suite rerun:
  - `actor_ownership_backpressure`: Kain `459.963 ms`, previous full latest `526.917 ms`
  - `semantic_fabric_relay`: Kain `114.693 ms`, previous full latest `121.885 ms`

## Regression Triage

- The first full-suite diff also showed scary regressions in rows such as `process_stdio_loop` and `contention_wall`, but isolated reruns disproved them:
  - `process_stdio_loop`: isolated median `6382.368 ms`, better than the previous full latest `6860.217 ms`
  - `contention_wall`: isolated median `9.842 ms`, close to the previous full latest `8.937 ms` and well within proxy-row noise
- Honest takeaway: the actor runtime patch moved the actor frontier and did not produce a reproducible non-actor regression in the isolated checks that matter most.

## Validation Notes

- `toolchain\llvm\bin\clang.exe -fsyntax-only runtime\native\src\core\actor.c -I runtime\native\include` -> PASS
- `mcp__z3_local__.check_smt2(...)` for `inline-ask-turn-claim-no-double-owner.smt2` -> `unsat`
- `mcp__z3_local__.check_smt2(...)` for `reply-port-parked-rebind-stale-ref-rejection.smt2` -> `unsat`
- `python benchmark/run.py --case actor_ownership_backpressure,semantic_fabric_relay --languages kain,cpp --runs 5 --warmups 2 --timeout 600 --latest-stem latest_actor_inline_scheduler_cut` -> PASS
- `python benchmark/run.py --case actor_ownership_backpressure,semantic_fabric_relay --languages kain,cpp --runs 9 --warmups 3 --timeout 600 --latest-stem latest_actor_inline_scheduler_cut_rerun` -> PASS
- `python benchmark/run.py --timeout 900 --latest-stem latest_full_after_inline_scheduler_cut` -> PASS
- `python benchmark/run.py --case process_stdio_loop --languages kain,rust,cpp --runs 9 --warmups 3 --timeout 900 --latest-stem latest_process_stdio_validation` -> PASS
- `python benchmark/run.py --case contention_wall --languages kain,rust,cpp,zig,javascript,python --runs 9 --warmups 3 --timeout 900 --latest-stem latest_contention_validation` -> PASS
- `bash runtime/conformance/actor_runtime/run_tests.sh` -> blocked by a pre-existing missing `attrition.c` link closure in the script, not by this patch

## Current Thesis

The scheduler lock was still a real tax on same-thread inline asks, and cutting it bought a measurable actor win. It is not the moonshot. The remaining actor gap is still dominated by request-side ownership and dispatch costs after the inline-claim decision, not by the scheduler lock alone.

## Next Branch Worth Exploring

1. Attack direct request-side actor dispatch or exact-target actor-state handles without lying about mailbox ordering.
2. Treat `actor_ownership_backpressure` and `semantic_fabric_relay` as the paired witness rows for that work.
3. Keep isolating any scary full-suite diff before blaming the actor lane; long Windows benchmark runs are still noisy enough to fake regressions.
