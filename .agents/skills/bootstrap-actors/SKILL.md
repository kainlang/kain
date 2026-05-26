---
name: bootstrap-actors
description: >-
  Use when changing compiler, frontend, or selfhost truth for Kain actors in
  `crates/actor`, `crates/core`, `crates/sys-codegen`, or
  adjacent proof surfaces: actor syntax, typed actor contracts, interpreter
  behavior, runtime-contract reflection, or actor lowering. Do not use for
  `runtime/native` scheduler or mailbox substrate work or for authored actor
  blades and demos.
---

# Bootstrap Actors

Use this skill when the primary work changes actor semantics or contracts that the compiler owns.

## Trigger Surface

- `crates/actor/**` for actor IDs, addresses, message shapes, definitions, mailbox policy metadata, supervision metadata, scheduler policy metadata, registry snapshots, and native ABI descriptors consumed by the frontend.
- `crates/core/src/{ast.rs,parser.rs,types.rs,runtime.rs,runtime_contract.rs}` for actor syntax, typechecking, interpreter-visible behavior, and reflected contract emission.
- `crates/sys-codegen/**` for actor spawn, send, ask, reply-port, or contract lowering that follows compiler truth.

## Boundaries

- Co-trigger `runtime-core` when `runtime/native/include/actor.h`, `runtime/native/src/core/actor.c`, mailbox accounting, scheduler ownership bits, or ask/reply ABI substrate must change.
- Co-trigger `lang-actors` when the main task is authored actor code, blade examples, or user-facing patterns.
- Co-trigger `tool-build-system` when runtime manifests, Bazel rules, or generated BUILD state must stay in sync with actor compiler work.
- If the change is really generic frontend infrastructure rather than actor-specific semantics, route it through `bootstrap-core`.

## Workflow

1. Change `crates/actor` contract types first when the actor model itself changed.
2. Propagate that truth through parser, typechecker, interpreter behavior, and runtime-contract reflection in `kain-core`.
3. Update lowering only after the contract shape is settled.
4. Keep actor contract, ask/reply typing, and actor ID behavior aligned across crate tests and proof surfaces.

## Validation Loop

```powershell
cargo check -p kain-actor -p kain-core -p kain-sys-codegen
cargo test -p kain-actor --target-dir target\codex-bootstrap-actors
cargo test -p kain-core --test actor_contract_test --target-dir target\codex-bootstrap-actors-core -- --nocapture
cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\codex-bootstrap-actors-core -- --nocapture
```

If actor lowering changed, also run:

```powershell
cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target\codex-bootstrap-actors-llvm -- --nocapture
```

## Guardrails

- Do not treat the native scheduler or mailbox implementation as the source of actor semantics.
- Do not land new actor behavior only in stdlib wrappers while leaving typed actor contracts stale.
- If validation pressure comes from runtime-native queue math, that is a `runtime-core` follow-on, not a reason to expand this skill's scope.
