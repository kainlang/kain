# Actor Ask Live Snapshot Speedup Hunt

- Date: 2026-05-21
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `actor-ask-live-snapshot-speedup-hunt`

## Research Question

After the borrowed inline ask payload lane and the reply-port direct-handle lane, is the next honest actor hot-path tax the global actor-table lock that still guards `kain_actor_ask_send_ref(...)`, and can we delete it without weakening stale-ref rejection?

## Constraints

- Keep the win honest: no benchmark-only reduction that bypasses `ask`, mailbox visibility, reply-port completion, or scheduler ownership.
- Preserve the same generation-tagged stale-ref contract that `kain_actor_table_ref_matches_locked(...)` enforced before the change.
- Leave the benchmark lane healthier than we found it; the current checkout was missing `benchmark/cases/actor_ownership_backpressure/main.kn`.
- Rerun the canonical full suite after the runtime change instead of claiming progress from a focused probe alone.

## Hypothesis Lattice

### Baseline

- Mechanism: keep `kain_actor_ask_send_ref(...)` taking `g_actor_table.lock` before every ask.
- Expected upside: none.
- Likely blocker: every actor ask still serializes through a global table lock before it even reaches the mailbox lock.

### Unconventional

- Mechanism: mirror the existing `kain_actor_send(...)` lookup shape on the ask path, load the target actor pointer once with `kain_actor_table_get(...)`, and validate the same generation/execution/locality tuple against that live snapshot before touching the mailbox.
- Expected upside: small but real speedup on ask-heavy semantic rows without changing the authored Kain benchmark shape.
- Likely blocker: the proof burden is not "does it compile?" but "did we preserve the old stale-ref predicate under the same live-slot invariant?"

### Moonshot

- Mechanism: move beyond snapshot validation and let compiler-lowered local actor refs carry an exact-target direct handle that can bypass the global table entirely on the request side.
- Expected upside: this is the first ask-side path that plausibly yields a multi-x actor win.
- Likely blocker: easy to cheat the actor model if mailbox ordering and scheduler ownership become optional.

## Mathematical Model

- Variables: `ref_actor_id`, `ref_generation`, `table_generation`, `actor_slot_id`, `actor_generation`, `actor_execution`, and `actor_locality`.
- Stable live-slot invariant: the loaded actor is still the live table occupant for the ref's slot and its `actor_id` still matches the slot/ref identity.
- Safety claim: the old locked ref predicate and the new live snapshot predicate cannot disagree under that invariant.
- Bad states: stale actor ref accepted, mismatched execution/locality accepted, or a benchmark-only path that skips mailbox semantics.

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/actor-ask-live-snapshot-ref-match-equivalence.smt2`
   - Encodes the old locked ref predicate and the new live-snapshot predicate under the stable live-slot invariant.
   - `mcp__z3_local__.check_smt2(...)` returned `unsat`.
   - Clean report: `runtime/native/src/core/z3/reports/20260521T101723Z-actor-ask-live-snapshot-ref-match-equivalence-clean.json`
2. `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/runtime/native/src/core/z3", lane="actor", report_name="actor-ask-live-snapshot-regression-check", ...)`
   - Result: `16 proved, 0 counterexamples, 0 unknown, 0 errors`
   - Report: `runtime/native/src/core/z3/reports/20260521T103827Z-actor-ask-live-snapshot-regression-check.json`

## Evidence And Sources

- Runtime surface:
  - `runtime/native/src/core/actor.c`
  - `runtime/native/src/core/z3/proofs-experimental/actor-ask-live-snapshot-ref-match-equivalence.smt2`
- Benchmark lane:
  - `benchmark/cases/actor_ownership_backpressure/main.kn`
  - `benchmark/out/reports/latest_actor_probe.llm.md`
  - `benchmark/out/reports/latest.llm.md`
- Build plumbing:
  - `tools/bazel/sync_rust_builds.py`
  - `crates/kain-build/BUILD.bazel`
  - `crates/kain-core/BUILD.bazel`

## Results

- Restored the missing `benchmark/cases/actor_ownership_backpressure/main.kn` source so the row could run again.
- Focused probe against the previous latest benchmark report:
  - `actor_ownership_backpressure`: Kain `506.508 ms` -> `482.118 ms`
  - semantic rounds/s: `355,374.446` -> `373,352.349`
  - ask roundtrips/s: `710,748.892` -> `746,704.699`
- Canonical full suite on 2026-05-21:
  - `actor_ownership_backpressure`: Kain `472.025 ms`, C++ `16.683 ms`
  - `semantic_fabric_relay`: Kain `134.765 ms`, C++ `11.191 ms`
  - `pulse_teleport_decay_mesh`: Kain `125.148 ms`, C++ `79.211 ms`
  - `semantic_host_bridge_fusion`: Kain `1264.507 ms`, C++ `861.447 ms`

## Current Thesis

The ask-path global table lock was a real tax, and deleting it produced an honest win, but it is still not the boss fight. The canonical suite says the remaining semantic actor gap is dominated by request-side actor ownership cost after lookup, not by ref validation alone.

`semantic_fabric_relay` and `actor_ownership_backpressure` now look like the same frontier wearing different clothes: both are still dominated by actor ask/reply substrate overhead rather than authored benchmark math. The best next branch is an ask-side exact-target or direct-handle specialization that still preserves mailbox ordering and scheduler ownership.

## Validation Notes

- `clang -fsyntax-only runtime/native/src/core/actor.c -I runtime/native/include` -> PASS
- `bazel build //:kain --config=dev` -> PASS after regenerating stale Rust BUILD files and killing a stale `kain.exe` file lock
- `kain check benchmark/cases/actor_ownership_backpressure/main.kn --target llvm` -> PASS
- `cargo test -p kain-actor --target-dir target/codex-actor-ask-live-snapshot` -> PASS
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target/codex-actor-direct-reply -- --nocapture` -> PASS
- `python benchmark/run.py --case actor_ownership_backpressure --languages kain,cpp,rust --runs 3 --warmups 1 --timeout 240 --latest-stem latest_actor_probe` -> PASS
- `python benchmark/run.py --timeout 900 --baseline-mode auto` -> PASS

## Next Branch Worth Exploring

1. Attack ask-side exact-target ownership after lookup: scheduler queue admission, mailbox append rules, and actor state visibility are still too expensive.
2. Reopen `semantic_fabric_relay` as the second witness row for the same request-side actor substrate.
3. Keep an eye on `process_stdio_loop` and other full-suite regressions, but treat the current history delta carefully because the canonical comparison is against commit `5229b9f8d978999ddea120a5c9403e9505548e42`, not against a pre-change full suite on this checkout.
