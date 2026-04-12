# Actor Lifecycle

This page summarizes the actor ownership and lifetime contract defined by the
native runtime.

## Actor State Machine

`UNINITIALIZED -> INITIALIZING -> RUNNING -> SHUTTING_DOWN -> TERMINATED/FAILED`

Actors can also be suspended and resumed while waiting on mailbox or runtime
work.

## Ownership Rules

- each runtime resource has a single owner
- mailbox ownership belongs to actor runtime state
- message payload ownership transfers on successful send
- receiver frees message data after processing
- actor user data is retained by the runtime for the actor lifetime

## Key Structures

- actor id
- actor message
- actor mailbox
- actor monitor
- actor link
- actor supervisor
- actor scheduler node
- actor handle
- actor spawn config

## Behavior Rules

- bounded mailboxes block when full
- unbounded mailboxes do not block on capacity
- closed mailboxes reject new messages
- monitors receive exit notifications
- links propagate abnormal termination
- supervisors decide whether to restart children

## Why This Matters

The language-level `actor` item and the native ABI have to agree on ownership.
If they diverge, message passing and cleanup semantics become unsafe.
