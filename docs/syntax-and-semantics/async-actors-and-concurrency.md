# Async, Actors, And Concurrency

Kain has real async and actor forms in the AST plus runtime support in
`crates/core/src/runtime.rs` and the native runtime headers.

For lifecycle, mailbox ownership, and shutdown semantics, also read
[guides/native-c-runtime/actor-lifecycle.md](/home/ephemara/Dev/Kain/guides/native-c-runtime/actor-lifecycle.md).

## Async Surface

- `await` expressions
- `async` blocks
- task polling and completion helpers in the runtime
- target-side async lowering where applicable

The runtime-native builtins include:

- `block_on`
- `spawn_task`
- `poll_once`
- `is_ready`
- `is_pending`
- `unwrap_ready`

## Actor Surface

Actors are first-class AST items. The runtime model includes:

- actor definition and methods
- message handlers
- `spawn` and `send`
- actor references
- mailbox ownership and backpressure
- supervision, monitors, and links

The important rule is that `spawn` creates a runtime handle and mailbox, while
`send` moves message ownership through that mailbox. Lifecycle shutdown and
supervision are owned by the runtime and native ABI, not by ordinary library
code.

## Message Passing

Actors communicate by transferring message ownership through the mailbox.
That ownership model is mirrored in the native actor ABI, so the language
surface and the C runtime contract line up.

## Why This Matters

Async and actor semantics are not just library conveniences in Kain. They are
part of the language's execution model and its native ABI story.
