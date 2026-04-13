# Macros And Comptime

Macros and compile-time execution are separate but adjacent features.

## Macros

Macro definitions use the AST form:

- `macro name!(params) { expansion }`

Macro parameters can accept:

- expressions
- types
- identifiers
- blocks
- tokens
- repeated groups

Macro bodies can be token-based or block-based.

## Comptime

`comptime` is executable compile-time code. It is used for:

- AST shaping
- metadata extraction
- shader and compute-plan discovery
- code generation decisions that need to run before typechecking or lowering

## Compute Metadata

The shader pipeline recognizes `compute` or `compute_plan` bindings inside a
`comptime` block. The extracted metadata can include:

- dispatch and workgroup sizes
- tensor plans
- stream plans
- neural node plans

The default compute tensor and stream contract strings live in
`crates/kain-core/src/ast.rs`.

## Practical Rule

If the compiler needs the result before it can typecheck or lower the rest of
the program, it belongs in `comptime`, not in a runtime helper.
