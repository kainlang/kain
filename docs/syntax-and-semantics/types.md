# Types

This is the type-form inventory from `crates/core/src/ast.rs` and the
layout-sensitive type system that sits behind it.

## Language Type Forms

Kain currently recognizes these type forms:

- named types with optional generics, such as `Vec<T>`
- tuples: `(A, B, C)`
- arrays: `[T; N]`
- slices: `[T]`
- references: `&T`, `&mut T`, with optional lifetime annotation
- raw pointers: `ptr<T>`, `ptr_mut<T>`, with provenance tracking
- function types: `fn(A, B) -> C with Effects`
- option shorthand: `T?`
- result shorthand: `T!E`
- inferred type: `_`
- never type: `!`
- unit type: `()`
- `impl Trait` forms

## Pointer Provenance

Pointer types track provenance so the compiler can distinguish where a raw
pointer came from:

- `Raw`
- `ImportedC`
- `ImportedAsm`
- `LoweredRef`

That matters for lowering, diagnostics, and native ABI work.

## Typechecker And Layout

The typechecker in `crates/core/src/types.rs` resolves concrete forms and
feeds the memory and ABI layers. The low-level memory pipeline then uses:

- `ResolvedType` and target-specific size/alignment rules
- C ABI policy selection from `crates/core/src/low_level_abi.rs`
- struct and union layout tracking from `crates/core/src/low_level_memory.rs`

## Practical Rule

If a type affects field layout, pointer lowering, or runtime helper emission,
it is not just syntax. It is part of the native contract.
