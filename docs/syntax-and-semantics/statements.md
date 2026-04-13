# Statements

Statements are the block-level control forms that wrap expressions and nested
items.

## Statement Forms

- `let pattern [: Type] = value`
- expression statements
- `return [value]`
- `break [value]`
- `continue`
- `for binding in iter: body`
- `while cond: body`
- `loop: body`
- nested items such as functions, structs, and modules

## How They Behave

- `let` introduces a binding and may include an explicit type annotation.
- `return`, `break`, and `continue` can appear both as statements and as
  expression-level control forms in the AST.
- `for` accepts a pattern, not just a plain identifier.
- nested items allow local helper definitions inside blocks.

## Practical Note

Statement behavior is runtime-sensitive in Kain. The interpreter and the
typechecker both care about control-flow forms because they feed `comptime`,
patch execution, async lowering, and actor/message handling.
