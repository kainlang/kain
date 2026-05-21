# Actor Frontier Speedup Hunt

- Date: 2026-05-21
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `actor-frontier-speedup-hunt`

## Research Question

After the live-snapshot ask path and inline scheduler-lock cut, is the next honest actor win hiding in reply-port setup and completion traffic: can compiler-lowered direct asks stop binding a synthetic actor-table slot just to mint a stale-reply token, and can same-thread inline completions hand the payload back to the owner thread without paying an extra reply-port lock/wake roundtrip?

## Constraints

- Keep the win honest: no benchmark-only shortcut that bypasses `ask`, stale-reply rejection, mailbox ordering, or authored Kain semantics.
- Preserve timeout/rearm safety for reused TLS reply ports.
- Preserve reply visibility for non-owner waiters while letting same-thread inline asks take the cheaper path.
- Keep the automation deadline requirement satisfied without mutating the benchmark contract. The current frontier row already does that through `deadline_millis(...)` and `deadline_elapsed(...)` in `benchmark/cases/actor_ownership_backpressure/main.kn`.
- Rerun the canonical full benchmark suite after the runtime/compiler change.

## Hypothesis Lattice

### Baseline

- Mechanism: every compiler-lowered direct ask still allocates or rebinds a reply-port actor-table presence through `kain_actor_reply_port_new()` and then exports a ref from that synthetic actor just to carry a stale-reply token.
- Expected upside: none.
- Likely blocker: the runtime is still paying synthetic actor bookkeeping and then re-taking the reply-port lock to copy an already-completed same-thread inline reply back out.

### Unconventional

- Mechanism: teach the compiler/runtime a direct-token lane that rearms the TLS reply-port state in place, keeps `actor_id` invalid, bumps only the reply generation, and exports that generation-tagged direct ref straight to the ask payload. Then let the owner thread observe a completed inline reply through a cheap `owner_inline_ready` flag before the normal locked copy path.
- Expected upside: a real double-digit actor improvement on `actor_ownership_backpressure` and a meaningful cut on `semantic_fabric_relay` without authored Kain changes.
- Likely blocker: stale direct tokens must stay dead after rearm, and the owner-inline readback path must not keep reading an old token after the next prepare.

### Moonshot

- Mechanism: go beyond reply-port setup and attack exact-target actor-state handles or a request-side direct dispatcher that avoids more mailbox/scheduler bookkeeping after lookup.
- Expected upside: still the best path to a future multi-x actor jump.
- Likely blocker: very easy to lie about actor observability or burst ordering.

## Mathematical Model

- Variables: `previous_generation`, `next_generation(previous_generation)`, direct reply-token identity fields, and an owner-thread completion/readback flag.
- Safety claim 1: once `prepare_direct_token(...)` rearms the TLS reply port, the old generation-tagged direct token cannot equal the new live token.
- Safety claim 2: the owner-inline completion path can only use the already-completed payload until the next prepare step resets `owner_inline_ready` and bumps the direct generation.
- Bad state: a stale direct reply from an old ask still matches the live token after rearm, or an owner-thread readback observes the old completion after a new direct ask has already prepared the port.

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-token-rearm-invalidates-stale-generation.smt2`
   - Encodes the generation-bump rule used by `kain_actor_reply_port_state_prepare_direct_token(...)`.
   - `mcp__z3_local__.check_smt2(...)` returned `unsat`.
   - Report: `z3/reports/20260521T223537Z-actor-reply-port-direct-token-rearm-invalidates-stale-generation.json`

2. `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-owner-inline-stale-direct-token-rejected.smt2`
   - Encodes the owner-thread fast-path safety claim: after the next direct prepare, the stale token from the prior completed ask still cannot match the live token.
   - `mcp__z3_local__.check_smt2(...)` returned `unsat`.
   - Report: `z3/reports/20260521T223537Z-actor-reply-port-owner-inline-stale-direct-token-rejected.json`

## Evidence And Sources

- Runtime/compiler surface:
  - `runtime/native/src/core/actor.c`
  - `runtime/native/include/actor.h`
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-actor/src/native.rs`
  - `crates/kain-actor/src/tests.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `runtime/conformance/actor_runtime/test_actor_abi_contract.c`
- Benchmark lane:
  - `benchmark/out/reports/latest_actor_frontier_baseline.llm.md`
  - `benchmark/out/reports/latest_actor_owner_inline_wait_combo_9.llm.md`
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/latest.md`

## What Changed

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - direct ask lowering now calls `kain_actor_reply_port_prepare_direct(...)` and receives the generation-tagged reply ref in one step
- `runtime/native/include/actor.h`
  - exported the direct-prepare reply-port ABI
- `runtime/native/src/core/actor.c`
  - added `kain_actor_reply_port_state_prepare_direct_token(...)`
  - direct replies now use invalid-actor direct refs plus bumped generations instead of needing a live synthetic actor-table slot
  - added `owner_inline_ready` and `kain_actor_reply_port_state_try_copy_completed_owner_inline(...)`
  - same-thread inline completion now skips the useless event/condvar wake and lets the owner thread copy the result out without re-taking the reply-port lock
- validation/proof surfaces:
  - added the direct-token proof yaml and two experimental SMT files
  - updated the actor ABI contract test and LLVM ask/reply regression test

## Honest Performance Result

- Focused baseline vs retake:
  - `actor_ownership_backpressure`: Kain `456.236 ms` -> `309.923 ms`
  - `semantic_fabric_relay`: Kain `114.217 ms` -> `93.487 ms`
- Canonical full suite (`benchmark/out/reports/latest.llm.md`, generated `2026-05-21T22:22:19.046887+00:00`):
  - `actor_ownership_backpressure`: Kain `313.311 ms`, C++ `18.673 ms`
  - `semantic_fabric_relay`: Kain `91.346 ms`, C++ `11.272 ms`
  - `pulse_teleport_decay_mesh`: Kain `93.919 ms`, C++ `16.827 ms`
- Current frontier ranking from the canonical PASS suite:
  - loudest overall gap: `actor_ownership_backpressure` at `16.78x` slower than C++
  - loudest non-proxy implemented gap: `unicode_string_heavy` at `10.36x` slower than C++
  - next semantic frontier pair: `semantic_fabric_relay` at `8.10x` and `pulse_teleport_decay_mesh` at `5.58x` slower than C++

## Validation Notes

- `toolchain\\llvm\\bin\\clang.exe -fsyntax-only runtime\\native\\src\\core\\actor.c -I runtime\\native\\include` -> PASS
- `cargo test -p kain-actor --target-dir target/codex-actor-direct-token` -> PASS
- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target/codex-actor-direct-token-codegen -- --nocapture` -> PASS
- `python benchmark/run.py --timeout 900` -> PASS
- The first full rerun failed on a stale Windows linker/permission artifact for `benchmark/out/build/process_stdio_loop/kain/process_stdio_loop.exe`; deleting that generated executable and rerunning restored a clean PASS.
- Targeted Rust validation initially failed with disk exhaustion, not a code fault. Deleting agent scratch dirs `.codex-tmp` and `target/codex-*` recovered about `86 GB`, and the rerun passed.

## Dead Ends

- No new benchmark row was needed. The existing suite is still rich with honest frontiers, so creating a fresh case now would have diluted effort away from the actor runtime substrate.

## Current Thesis

The direct ask lane was still leaving measurable runtime tax on the table. Removing the synthetic-actor reply-port setup and the owner-thread reply lock/wake roundtrip bought a real actor improvement without cheating the semantic contract. It is still not the moonshot. The remaining actor gap is now even more clearly request-side ownership and dispatch after direct ask setup.

## Next Branch Worth Exploring

1. Stay on the actor semantic frontier and attack exact-target actor-state handles or direct request-side dispatch.
2. Keep `actor_ownership_backpressure`, `semantic_fabric_relay`, and `pulse_teleport_decay_mesh` grouped as one substrate frontier.
3. When we leave actors, hit `unicode_string_heavy` next because it is the biggest remaining non-proxy implemented loss in the fresh PASS suite.
