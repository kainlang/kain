# Syntax

This page is the surface map. It names the language forms; the sibling pages
explain how each form behaves.

## Core Surface

Kain syntax is built from these families:

- top-level items: functions, patches, laws, converges, worlds, orchestrates,
  components, shaders, actors, structs, enums, traits, impls, aliases, uses,
  modules, consts, comptime blocks, macros, tests, and domain items
- statements: `let`, expression statements, `return`, `break`, `continue`,
  `for`, `while`, `loop`, nested items
- expressions: literals, calls, binary/unary operators, field/index access,
  struct and enum construction, control flow, async, actors, JSX, and low-level
  memory operations
- patterns: wildcard, literal, binding, struct, tuple, variant, slice, or,
  range

## Keywords That Matter

The parser and LSP both treat these as language keywords or keyword-like forms:

`fn`, `let`, `mut`, `var`, `const`, `if`, `else`, `elif`, `match`, `for`,
`while`, `loop`, `break`, `continue`, `return`, `await`, `in`, `with`, `as`,
`type`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, `self`, `Self`,
`true`, `false`, `none`, `component`, `patch`, `law`, `converge`, `world`,
`orchestrate`, `shader`, `actor`, `state`, `spawn`, `send`, `receive`, `emit`,
`comptime`, `macro`, `vertex`, `fragment`, `test`

## Reading Rule

Syntax is only half the story in Kain. Several forms have runtime meaning,
compile-time meaning, or backend-specific lowering rules. Use the sibling
pages to understand those layers:

- `types.md`
- `effects-and-capabilities.md`
- `low-level-memory.md`
- `module-resolution.md`
- `functions-traits-and-impls.md`
- `patterns.md`
- `expressions.md`
- `statements.md`
- `modules-and-items.md`
- `macros-and-comptime.md`
- `async-actors-and-concurrency.md`
- `domain-items.md`
