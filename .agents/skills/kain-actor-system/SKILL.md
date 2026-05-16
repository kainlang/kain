---
name: kain-actor-system
description: Use when changing, debugging, validating, or reviewing Kain's actor pipeline, especially crates/kain-actor contracts, actor semantics in kain-core, runtime/native actor ABI integration, mailbox/scheduler policy, supervision, or native actor Z3 proofs.
---

# Kain Actor System

Use this when the task touches actor semantics or the native actor runtime.

## First places to look

- `crates/kain-actor`: shared actor model types and contracts
- `crates/kain-core`: language-level actor semantics and lowering inputs
- `runtime/native/include/kain_runtime_actor.h`: C ABI surface
- `runtime/native/src/core/kain_runtime_actor.c`: native runtime implementation
- `runtime/native/src/core/z3/proofs`: durable actor proof lane
- `runtime/native/src/core/z3/proofs-experimental`: reference SMT experiments
- `runtime/conformance/actor_runtime`: native actor conformance suite

## Current native hot-path shape

- Actor IDs come from a fixed-capacity table (`KAIN_ACTOR_TABLE_CAPACITY`).
- Slot `0` is reserved as `KAIN_ACTOR_ID_INVALID`.
- The canonical native handle shape is now `KainActorRef { actor_id, generation, execution_class, locality_class }`, not a raw slot id in LLVM actor state.
- LLVM actor structs store that ref in field `0`, and the native ask/reply path lowers through `kain_actor_ref_from_id(...)`, `kain_actor_reply_port_actor_ref(...)`, and `kain_actor_reply_port_send_ref(...)`.
- TLS reply ports are synthetic actor-table entries with execution class `SYNTHETIC_REPLY_PORT`; on the next `kain_actor_reply_port_new()` after a successful wait, the runtime unbinds the old synthetic actor and rebinds a fresh generation before reuse. If a stale late reply bug shows up, inspect this rebind seam first.
- Free-slot discovery is an occupancy-word scan, then low-bit isolation plus a
  de Bruijn decode. Do not reintroduce a linear scan unless profiles prove the
  bitset path is wrong for a new table shape.
- The pooled scheduler is a power-of-two ring buffer of actor IDs with masked
  cursors. Do not bring back per-enqueue heap nodes without a measured reason.

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

- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_actor.c`
- `powershell -NoProfile -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_generates_actor_ask_reply_roundtrip_paths -- --nocapture`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply -- --nocapture`

## Benchmarks

- For scheduler/allocation hot paths, use a small scratch harness under
  `target/` to compare old and new paths directly before claiming a win.
- The current native reference benchmark is
  `target/codex-actor-hotpath-benchmark.c`.

## Notes for future agents

- If you change actor table capacity, revisit both the occupancy-word layout and
  the ring-mask assumptions before touching performance code.
- If actor semantics change at the language level, confirm the native registry,
  mailbox, supervision, and scheduler statistics still match the conformance
  expectations.
