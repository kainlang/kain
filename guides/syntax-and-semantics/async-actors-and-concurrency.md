# Async, Actors, And Concurrency

Kain has real async and actor forms in the AST plus runtime support in
`crates/kain-core/src/runtime.rs` and the native runtime headers.

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

## Message Passing

Actors communicate by transferring message ownership through the mailbox.
That ownership model is mirrored in the native actor ABI, so the language
surface and the C runtime contract line up.

## Why This Matters

Async and actor semantics are not just library conveniences in Kain. They are
part of the language's execution model and its native ABI story.
