# Statements

Statements are the block-level control forms that wrap expressions and nested
items.

## Statement Forms

- `let pattern [: Type] = value`
- expression statements
- `defer expr`
- `dispatch "compute.key" [x, y, z]`
- `return [value]`
- `break [value]`
- `continue`
- `for binding in iter: body`
- `while cond: body`
- `loop: body`
- nested items such as functions, structs, and modules

## How They Behave

- `let` introduces a binding and may include an explicit type annotation.
- `defer` registers an expression cleanup for the current block. Defers run in
  strict LIFO order on fallthrough, `return`, `break`, and `continue`.
- `return` and `break` evaluate their payload first, then the exiting block's
  defers run, then control leaves the block. Cleanup cannot rewrite the already
  evaluated payload.
- `dispatch` is a host-side GPU statement. Its string is the compute key and its
  `[x, y, z]` values are dynamic dispatch dimensions that override an artifact
  or metadata default for that launch.
- `return`, `break`, and `continue` can appear both as statements and as
  expression-level control forms in the AST.
- `for` accepts a pattern, not just a plain identifier.
- nested items allow local helper definitions inside blocks.

## Practical Note

Statement behavior is runtime-sensitive in Kain. The interpreter and the
typechecker both care about control-flow forms because they feed `comptime`,
patch execution, async lowering, and actor/message handling.
