---
name: kain-actor-system
description: Use when changing, debugging, validating, or reviewing Kain's actor pipeline, especially crates/kain-actor contracts, actor semantics in kain-core, runtime/native actor ABI integration, mailbox/scheduler policy, supervision, or native actor Z3 proofs.
---

# Kain Actor System

Use this when the task touches actor semantics or the native actor runtime.

## First places to look

- `crates/kain-actor`: shared actor model types and contracts
- `crates/kain-core`: language-level actor semantics and lowering inputs
- `runtime/native/include/actor.h`: C ABI surface
- `runtime/native/src/core/actor.c`: native runtime implementation
- `runtime/native/src/core/z3/proofs`: durable actor proof lane
- `runtime/native/src/core/z3/proofs-experimental`: reference SMT experiments
- `runtime/conformance/actor_runtime`: native actor conformance suite

## Current native hot-path shape

- Actor IDs come from a fixed-capacity table (`KAIN_ACTOR_TABLE_CAPACITY`).
- Slot `0` is reserved as `KAIN_ACTOR_ID_INVALID`.
- The canonical native handle shape is now `KainActorRef { actor_id, generation, execution_class, locality_class }`, not a raw slot id in LLVM actor state.
- LLVM actor structs store that ref in field `0`, and the native ask/reply path lowers through `kain_actor_ref_from_id(...)`, `kain_actor_ask_send_ref(...)`, `kain_actor_reply_port_actor_ref(...)`, and `kain_actor_reply_port_send_ref(...)`.
- Actor ABI v3 has two entry styles: `KAIN_ACTOR_ENTRY_KIND_LEGACY_BOOTSTRAP` for compatibility blocking actors and `KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN` for LLVM-native actor handlers.
- Native LLVM actors should lower to `Actor_turn(actor_id, mailbox, user_data, budget)` and poll with `kain_actor_try_receive`; do not reintroduce scheduler-owned blocking `kain_actor_receive` loops for generated actors.
- Microcell turn actors are idle until a message arrives. `kain_actor_send` appends under the mailbox lock and calls `kain_scheduler_ready_actor`; `kain_actor_ask_send_ref` is the ask-only exact-ref fast path that can claim `in_scheduler_turn` inline when the target is a local microcell actor, the mailbox was empty before enqueue, and the actor is neither queued nor already in a turn. `kain_scheduler_finish_turn` remains the shared cleanup/requeue path for both pooled workers and the inline ask handoff.
- Legacy bootstrap actors spawn on direct compatibility threads by default. This is intentional: one blocking legacy actor must not monopolize a pooled microcell scheduler worker.
- TLS reply ports are synthetic actor-table entries with execution class `SYNTHETIC_REPLY_PORT`; on the next `kain_actor_reply_port_new()` after a successful wait, the runtime reuses the same synthetic actor slot and bumps generation before resetting payload state. If a stale late reply bug shows up, inspect `kain_actor_reply_port_state_rearm_synthetic_actor(...)` and the exact-ref check inside `kain_actor_reply_port_state_complete_copied(...)` first.
- `kain_actor_reply_port_wait(...)` deliberately does one completion check, a bounded 256-spin fast path for nonzero timeouts, then the normal OS wait. Do not remove the fallback or make timeout-zero waits spin.
- Each mailbox has a capped message-node freelist. This recycles queue nodes only; payload ownership still transfers to the receiver and must be freed by the generated/runtime caller. The last payload-cache experiment regressed `actor_mailbox_erlang`, so re-benchmark before reintroducing one.
- Runtime shutdown closes mailboxes, stops the microcell scheduler, joins direct legacy actor threads, and only then cleans the actor table. This order prevents freeing direct-thread actor state while the bootstrap thread still returns through exit side effects.
- Free-slot discovery is an occupancy-word scan, then low-bit isolation plus a
  de Bruijn decode. Do not reintroduce a linear scan unless profiles prove the
  bitset path is wrong for a new table shape.
- The pooled scheduler is a power-of-two ring buffer of actor IDs with masked
  cursors. Do not bring back per-enqueue heap nodes or overflow direct-thread
  spawns for microcell actors without a measured reason.

## Proof workflow

- Treat Z3 as the primary validator for bounds, masking math, queue depth, slot
  composition, and closed-world token tricks.
- Put durable invariants in `runtime/native/src/core/z3/proofs/*.yaml` when the
  property is meant to stay with the runtime.
- Put exploratory or “alien math” reference proofs in
  `runtime/native/src/core/z3/proofs-experimental/*.smt2`.
- For native actor changes, run the actor lane:
  - `uv run --project C:\\Dev\\polytools\\z3-mcp --no-sync z3-mcp-batch --pack-path D:\\Kain-Lang\\runtime\\native\\src\\core --lane actor`
- If you add tricky bit-manipulation helpers, add a direct SMT check proving the
  exact mapping or index bounds instead of relying on examples.

## Validation loop

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/actor.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_actor_ask_reply_roundtrip_paths -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_actor_spawn_and_send_message_paths -- --nocapture`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply -- --nocapture`

## Benchmarks

- For scheduler/allocation hot paths, use a small scratch harness under
  `target/` to compare old and new paths directly before claiming a win.
- The current cross-language reference benchmark is
  `py -3 benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 3 --warmups 1 --timeout 240`.
- As of `2026-05-16`, the local exact-ref ask handoff pass measured Kain `182.275 ms` vs Erlang `389.657 ms` on that row, so Kain currently wins and Erlang is `2.14x slower`. Treat future wins as real only if they improve this row, not just synthetic allocation microbenchmarks.

## Notes for future agents

- If you change actor table capacity, revisit both the occupancy-word layout and
  the ring-mask assumptions before touching performance code.
- If actor semantics change at the language level, confirm the native registry,
  mailbox, supervision, and scheduler statistics still match the conformance
  expectations.
