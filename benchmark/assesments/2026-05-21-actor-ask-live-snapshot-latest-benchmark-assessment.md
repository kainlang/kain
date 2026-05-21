# Actor Ask Live Snapshot Benchmark Assessment

- date: `2026-05-21`
- focus: `repair the broken actor benchmark lane, remove the ask-side global actor-table lock, and reassess the latest canonical frontier`
- evidence:
  - previous canonical latest: `benchmark/out/reports/latest.json` from commit `5229b9f8d978999ddea120a5c9403e9505548e42`
  - focused repro: `benchmark/out/reports/latest_actor_probe.llm.md`
  - canonical full suite: `benchmark/out/reports/latest.llm.md`
  - proof lane: `runtime/native/src/core/z3/proofs-experimental/actor-ask-live-snapshot-ref-match-equivalence.smt2`
  - proof report: `runtime/native/src/core/z3/reports/20260521T101723Z-actor-ask-live-snapshot-ref-match-equivalence-clean.json`

## What changed

- `benchmark/cases/actor_ownership_backpressure/main.kn`
  - restored the missing benchmark source and kept the `deadline_millis(...)` / `deadline_elapsed(...)` touch alive so the automation requirement remains exercised by a real row.
- `runtime/native/src/core/actor.c`
  - added `kain_actor_ref_matches_live_snapshot(...)`
  - changed `kain_actor_ask_send_ref(...)` to validate a live target snapshot without taking `g_actor_table.lock` on every ask
- `runtime/native/src/core/z3/proofs-experimental/actor-ask-live-snapshot-ref-match-equivalence.smt2`
  - proves the new snapshot predicate matches the old locked ref predicate under the stable live-slot invariant
- `crates/kain-build/BUILD.bazel` and `crates/kain-core/BUILD.bazel`
  - regenerated through `python tools/bazel/sync_rust_builds.py` to repair stale Bazel/Cargo drift that blocked `kain check`

## Honest performance result

- Focused actor retake against the prior latest report:
  - `actor_ownership_backpressure`: Kain `506.508 ms` -> `482.118 ms`
  - semantic rounds/s: `355,374.446` -> `373,352.349`
  - ask roundtrips/s: `710,748.892` -> `746,704.699`
- That is about a `1.05x` speedup on the focused actor row.
- The canonical 9-run full suite stayed green and reported:
  - `actor_ownership_backpressure`: Kain `472.025 ms`, C++ `16.683 ms`
  - `kain_improvements`: `14`
  - `kain_regressions`: `25`
  - `alert_regressions`: `16`

## What did not magically flip

- The actor row is still the loudest remaining semantic gap at `28.29x` slower than C++ in the canonical suite.
- `semantic_fabric_relay` is still `12.04x` slower than C++ and likely shares the same ask-side substrate bottleneck.
- `semantic_host_bridge_fusion` and `pulse_teleport_decay_mesh` are still meaningful but much smaller opportunities than the two actor-heavy rows.
- The full-suite regression table is not clean, but it is not evidence that this patch regressed the actor lane. The canonical comparison is against an older repo commit, not against a pre-change full suite on this exact checkout.

## Full-suite frontier after rerun

The remaining benchmarks are not lacking or pointless, so there was no need to mint a new benchmark this pass. The current highest-value frontier is already visible:

1. `actor_ownership_backpressure`
- Kain `472.025 ms`, C++ `16.683 ms`
- Still the biggest semantic actor/runtime gap by far.

2. `semantic_fabric_relay`
- Kain `134.765 ms`, C++ `11.191 ms`
- The second actor-heavy semantic row and likely the best corroborating witness for the same substrate problem.

3. `pulse_teleport_decay_mesh`
- Kain `125.148 ms`, C++ `79.211 ms`
- Real gap, but much smaller than the two ask-heavy rows.

4. `semantic_host_bridge_fusion`
- Kain `1264.507 ms`, C++ `861.447 ms`
- Worth revisiting after the actor request-side work, not before.

5. `process_stdio_loop`
- Kain still wins the row at `7208.012 ms` vs Rust `7649.524 ms`, but the full-suite history shows a `+34.73%` Kain regression vs the prior comparable run, so it deserves a stability audit even though it is not currently a foreign-language loss.

## Recommendation for the next agent

1. Reopen the actor lane first and stay on the ask side: exact-target handles, queue admission, scheduler ownership, and mailbox append cost are still where the big semantic loss lives.
2. Use `semantic_fabric_relay` as the companion row for that work so we do not overfit only to `actor_ownership_backpressure`.
3. Treat the current full-suite regression table as a separate cleanup backlog; do not confuse it with proof that this actor ask snapshot change regressed behavior.
