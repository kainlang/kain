# Effects, I/O, Async, And Patching

This page covers the runtime rules that shape call safety and mutation
behavior.
For the compiler-owned item quartet (`patch`, `law`, `converge`, `world`, and
`orchestrate`), also read
[guides/runtime/compiler-owned-intents.md](/home/ephemara/Dev/Kain/guides/runtime/compiler-owned-intents.md).

## Effect System

The effect lattice currently tracks eight effect kinds:

- `Pure`
- `IO`
- `Async`
- `GPU`
- `Reactive`
- `Unsafe`
- `Alloc`
- `Panic`

The checker in `crates/kain-core/src/effects.rs` uses those sets to decide
whether a caller may invoke a callee.

## Call Rule

The key rule is simple:

- pure code can only call pure code
- effectful code can call compatible effectful code
- `Unsafe` can call anything

This is enforced at typecheck time, not left to the backend.

## I/O And External Effects

I/O-like builtins cover:

- file I/O
- console I/O
- environment access
- HTTP
- socket access
- native bridge configuration

These functions are marked so the compiler can explain why a call is allowed or
rejected.

## Async Semantics

Async forms lower into future/poll behavior in the runtime. The runtime exposes
task helpers that let authored code:

- spawn work
- poll readiness
- wait for completion
- handle cancellation or pending states

The concrete runtime helpers include:

- `spawn_task`
- `block_on`
- `poll_once`
- `is_ready`
- `is_pending`
- `unwrap_ready`

The AST side includes `AsyncBlock`, `Await`, `Spawn`, and `SendMsg`, so docs
should always distinguish syntax from runtime helper behavior.

## Patching

Patches are a first-class runtime concept in Kain.

- patch execution records mutation paths
- collaboration events are tracked
- undo and replay are part of the runtime state
- `PatchUndoMode` distinguishes reversible and best-effort behavior

The runtime helpers for that surface include:

- `patch_history`
- `patch_collaboration_events`
- `patch_undo_last`
- `patch_replay_last`
- `patch_replay`

That means a patch is not just a syntax form; it is a runtime transaction
record with history.

## Why This Page Exists

Effects, async, and patching all influence call safety and runtime behavior.
They belong together because they shape what the runtime can legitimately do
with a function body.
