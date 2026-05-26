# Direct Reply Port Handle Assessment

- date: `2026-05-21`
- focus: `native actor reply-port direct-handle fast path plus latest canonical benchmark frontier review`
- evidence:
  - focused pre-pass repro: `benchmark/out/reports/latest_frontier_probe.llm.md`
  - focused post-pass repro: `benchmark/out/reports/latest_direct_reply_probe.llm.md`
  - canonical full suite: `benchmark/out/reports/latest.llm.md`
  - proof lane: `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-handle-ref-match-equivalence.smt2`
  - proof report: `z3/reports/20260521T051617Z-actor-reply-port-direct-handle-ref-match-equivalence.json`

## What changed

- `runtime/native/include/actor.h`
  - adds `kain_actor_reply_port_send_handle(...)` so compiler-lowered reply ports can carry a live state handle alongside the synthetic actor ref.
- `runtime/native/src/core/actor.c`
  - completes replies directly against the live reply-port state handle when present, avoiding the old actor-table lookup on the reply hot path.
- `crates/actor/src/native.rs` and `crates/actor/src/tests.rs`
  - register and test the new required native actor symbol.
- `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - grows `%KainReplyPort` from `{ %KainActorRef }` to `{ %KainActorRef, i8* }`
  - lowers reply sends through `kain_actor_reply_port_send_handle(...)`
- `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - updates the actor ask/reply regression test to the new reply-port contract.
- `runtime/conformance/actor_runtime/test_actor_abi_contract.c`
  - exercises the direct-handle send helper in the ABI contract lane.
- `benchmark/cases/actor_ownership_backpressure/main.kn`
  - already touches `deadline_millis` / `deadline_elapsed`, so this automation pass satisfied the repo's deadline benchmark convention without inventing a new row.

## Honest performance result

- Focused before/after repro:
  - `actor_ownership_backpressure`: Kain `488.203 ms` -> `466.577 ms`
  - `semantic_host_bridge_fusion`: Kain `1079.810 ms` -> `1010.633 ms`
- That is about a `1.05x` speedup on the actor row and a `1.07x` speedup on the host-bridge row in the focused probe.
- The canonical full-suite rerun stayed green and reported:
  - `kain_regressions`: `0`
  - `alert_regressions`: `0`
  - `actor_ownership_backpressure`: Kain `487.169 ms`, C++ `16.384 ms`
  - `semantic_host_bridge_fusion`: Kain `1173.043 ms`, C++ `861.447 ms`

## What did not magically flip

- This is a real win, but it is not a moonshot. `actor_ownership_backpressure` is still `29.73x` slower than C++ in the canonical suite.
- `semantic_host_bridge_fusion` is still `1.36x` slower than C++ and remains sensitive to broad host-runtime costs, not just actor reply delivery.
- The pass did not make semantic proxy rows such as `semantic_fabric_relay` or `pulse_teleport_decay_mesh` suddenly competitive.
- Because the remaining actor loss is so large, the next serious attack should be request-side ask-path ownership, not more reply-side cleanup.

## Full-suite frontier after rerun

The current remaining C++ losses are not lacking or pointless, so there was no need to mint a new benchmark row this pass. The high-value frontier is already on the board:

1. `actor_ownership_backpressure`
- Kain `487.169 ms`, C++ `16.384 ms`
- Still the loudest semantic actor/runtime gap.

2. `semantic_fabric_relay`
- Kain `111.563 ms`, C++ `11.191 ms`
- Another actor-heavy semantic row that likely shares the same request-side substrate bottleneck.

3. `unicode_string_heavy`
- Kain `78.827 ms`, C++ `8.405 ms`
- The biggest non-proxy implemented-row loss and a strong candidate for a true `2-10x` win hunt.

4. `allocator_large_object_churn`
- Kain `92.833 ms`, C++ `41.095 ms`
- Honest allocator/runtime pressure with a large enough gap to matter.

5. `pulse_teleport_decay_mesh`
- Kain `124.817 ms`, C++ `79.211 ms`
- Worth a follow-up only after the more obvious actor and string frontiers.

## Recommendation for the next agent

1. Reopen the actor lane first, but target the ask side: exact-target actor handle, scheduler ownership, and mailbox bypass conditions that still preserve full actor semantics.
2. If the goal is the next likely multi-x implemented win, open `unicode_string_heavy` before `semantic_host_bridge_fusion`.
3. Keep using the canonical full suite after each pass; the focused probes are useful for search, but they are not the claim.
