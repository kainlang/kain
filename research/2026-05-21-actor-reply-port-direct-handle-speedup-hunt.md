# Actor Reply Port Direct-Handle Speedup Hunt

- Date: 2026-05-21
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `actor-reply-port-direct-handle-speedup-hunt`

## Research Question

After the borrowed inline ask payload lane removed the first request heap-copy from local microcell asks, is the next real actor hot-path tax the reply-side actor-table lookup, and can we delete it without lying about reply-port semantics?

## Constraints

- Keep the win honest: no benchmark-only shortcut that bypasses `ask`, reply-port delivery, or scheduler-visible actor semantics.
- Preserve stale-reply rejection and timeout rearm behavior for TLS reply ports.
- Keep the public actor lowering contract aligned across `crates/kain-actor`, LLVM codegen, and the native runtime.
- Leave a durable proof artifact and rerun the canonical benchmark suite before claiming progress.

## Hypothesis Lattice

### Baseline

- Mechanism: keep reply delivery on `kain_actor_reply_port_send_ref(...)`, which resolves the synthetic reply port back through the global actor table on every reply.
- Expected upside: none.
- Likely blocker: the hot path still takes actor-table lookup and lock traffic even when the waiting thread already owns the live reply-port state handle.
- Proof obligation: none beyond reproducing the remaining loss.

### Unconventional

- Mechanism: teach compiler-lowered `P` values to carry both the stable synthetic `KainActorRef` and the live reply-port state handle, then let local replies complete directly against the state handle while still checking the matching ref contract.
- Expected upside: small-but-real latency reduction on every ask/reply roundtrip, especially on actor-heavy semantic rows.
- Likely blocker: stale reply-port generations must still reject late replies after timeout rearm, so the handle fast path cannot silently weaken the old ref contract.
- Proof obligation: prove that, under the bound-handle invariant, the old table-based accept predicate and the new direct-handle predicate cannot disagree.

### Moonshot

- Mechanism: extend the same idea to the ask side and let compiler-lowered local actor refs carry a direct actor-state handle for exact-target microcell asks, not just reply ports.
- Expected upside: this is the first path that plausibly yields a multi-x win instead of a single-digit percentage trim.
- Likely blocker: it is much easier to accidentally stop measuring mailbox/scheduler semantics on the request side than on the reply side.
- Proof obligation: preserve exact-target routing, message ordering, and scheduler ownership accounting for the general actor ABI.

## Mathematical Model

- Variables: `reply_ref`, `reply_handle`, `state_generation`, `expected_generation`, `bound_generation`, and `completed`.
- Invariant: the handle fast path is only legal when `reply_handle` still points at the same reply-port state that the synthetic actor ref names.
- Safety claim: `accept_via_table(reply_ref, expected_ref)` and `accept_via_handle(reply_handle, expected_ref)` are equivalent under the bound-handle invariant.
- Bad states: stale reply accepted after timeout rearm, mismatched reply-port completion, or a null/foreign handle bypassing the old ref guard.

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-handle-ref-match-equivalence.smt2`
   - Encodes the old ref-match predicate and the new direct-handle predicate under the bound-handle invariant.
   - `mcp__z3_local__.check_smt2(...)` returned `unsat`.
   - Report: `z3/reports/20260521T051617Z-actor-reply-port-direct-handle-ref-match-equivalence.json`
2. `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/runtime/native/src/core/z3", lane="actor", report_name="actor-direct-handle-fastpath-regression-check", ...)`
   - Result: `16 proved, 0 counterexamples, 0 unknown, 0 errors`
   - Report: `runtime/native/src/core/z3/reports/20260521T051617Z-actor-direct-handle-fastpath-regression-check.json`

## Evidence And Sources

- Runtime/compiler surface:
  - `runtime/native/include/actor.h`
  - `runtime/native/src/core/actor.c`
  - `runtime/conformance/actor_runtime/test_actor_abi_contract.c`
  - `crates/kain-actor/src/native.rs`
  - `crates/kain-actor/src/tests.rs`
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
- Benchmark reports:
  - pre-pass focused repro: `benchmark/out/reports/latest_frontier_probe.llm.md`
  - post-pass focused repro: `benchmark/out/reports/latest_direct_reply_probe.llm.md`
  - post-pass canonical suite: `benchmark/out/reports/latest.llm.md`
- Supporting isolate repro:
  - `benchmark/out/reports/latest_direct_reply_isolates.llm.md`

## Results

- Focused repro before the direct-handle reply path:
  - `actor_ownership_backpressure`: Kain `488.203 ms`, C++ `17.091 ms`
  - `semantic_host_bridge_fusion`: Kain `1079.810 ms`, C++ `845.035 ms`
- Focused repro after the direct-handle reply path:
  - `actor_ownership_backpressure`: Kain `466.577 ms`, C++ `15.680 ms`
  - `semantic_host_bridge_fusion`: Kain `1010.633 ms`, C++ `842.817 ms`
- Canonical full suite on 2026-05-21:
  - `actor_ownership_backpressure`: Kain `487.169 ms`, C++ `16.384 ms`
  - `semantic_host_bridge_fusion`: Kain `1173.043 ms`, C++ `861.447 ms`
  - `kain_regressions`: `0`
  - `alert_regressions`: `0`

## Current Thesis

The reply-side actor-table lookup was a real tax, and deleting it produced an honest win, but it is not the boss-fight. The canonical suite says the remaining actor gap is still dominated by request-side exact-target ask cost plus scheduler/mailbox ownership overhead. The best next branch is a proof-backed direct-handle or exact-target specialization on the ask side, not another reply-only micro-optimization.

The full suite also surfaced a second frontier that is no longer ignorable: `unicode_string_heavy` is currently Kain `78.827 ms` vs C++ `8.405 ms`, a `9.38x` implemented-lane loss. That row is likely a better medium-term "honest 2-10x" opportunity than host-bridge noise rows because it is not proxy theater and it is large enough to matter.

## Validation Notes

- `cargo test -p kain-actor --target-dir target\codex-actor-direct-reply` passed.
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target\codex-actor-direct-reply -- --nocapture` passed.
- `clang -fsyntax-only` over the touched runtime/conformance C files passed.
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose` did not complete because the existing conformance link lane is missing attrition symbols such as `kain_attrition_heap_alloc` and `kain_attrition_note_actor_stale_reject`. Treat that as an unrelated lane-health issue, not proof that the direct-handle fast path is invalid.

## Next Branch Worth Exploring

1. Attack `actor_ownership_backpressure` at the ask side with a direct actor-handle fast path that still preserves exact-target actor semantics.
2. Open `unicode_string_heavy` as the next implemented-row frontier and inspect whether Kain is paying avoidable UTF-8 materialization, substring allocation, or repeated bounds/length scans.
3. Keep `semantic_host_bridge_fusion` on the list, but only after the actor lane or the string lane, because the current gap there is much smaller and more host-noise-sensitive.
