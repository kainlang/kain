# Patterns

Patterns are used by `match`, `let`, `for`, and actor/message destructuring.

## Pattern Forms

- wildcard: `_`
- literal: numbers, strings, booleans, and `none`
- binding: `x`, `mut x`
- struct: `Point { x, y }`
- tuple: `(a, b, c)`
- enum variant: `Some(x)`, `Result::Ok(v)`
- slice: `[first, rest @ ..]`
- or-pattern: `A | B`
- range: `1..10`, `1..=10`

## Binding Rule

Bindings can introduce names or rebind mutable values. The runtime pattern
matcher uses these forms for:

- `match` arms
- `for` bindings
- actor and message destructuring
- imported data shaped into structs, tuples, and enums

## What To Watch For

The compiler distinguishes:

- variant patterns that carry tuple or struct field payloads
- slice patterns that can capture a `rest` binding
- range patterns that can be inclusive or exclusive

Those distinctions matter for lowering and for the runtime's binding logic.
