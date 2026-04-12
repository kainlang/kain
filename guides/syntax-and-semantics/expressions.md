# Expressions

Expressions are where most executable meaning lives.

## Core Expression Families

- literals: integers, floats, strings, f-strings, booleans, and `none`
- identifiers
- macro calls
- binary and unary operators
- function calls and method calls
- field access and index access
- assignments
- struct literals and aggregate initialization
- enum variant construction
- arrays, tuples, and ranges
- `if` and `match`
- lambdas
- references and dereferences
- low-level pointer helpers
- allocation and layout helpers
- async and actor operations
- comptime expressions
- blocks, parens, JSX, and control-flow expressions

## Calls And Stage Calls

Kain supports ordinary calls plus target/runtime-specific stage calls:

- `kain fn(...)`
- `rust fn(...)`
- `python fn(...)`
- `node fn(...)`

Those stage calls are how authored Kain can explicitly delegate work into a
different execution lane.

## Low-Level Memory Expressions

The AST includes first-class forms for:

- `addr_of`
- `ptr_offset`
- `mem_load`
- `mem_store`
- `sizeof_type`
- `alignof_type`
- `alloca`
- `uninit`
- `alloc`
- `realloc`

These are not just library calls. They are part of the language surface and
feed the ABI and lowering layers.

## Async, Actors, And Control Flow

The expression layer also includes:

- `try`/`?`
- `await`
- `async` blocks
- `spawn` for actors
- `send` for actor messages
- `return`, `break`, and `continue` as expressions

## JSX And UI

JSX is an expression form, not a separate language. That is why component and
UI behavior stays inside the same semantic pipeline as the rest of the AST.

## Runtime Sensitivity

Several expression families carry extra meaning during runtime evaluation:

- field and index mutation
- closures
- `match`
- async state machines
- actor message passing
- patch history and replay
- comptime evaluation
