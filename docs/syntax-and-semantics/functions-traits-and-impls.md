# Functions, Traits, And Impls

Snapshot: April 12, 2026.

This page is the canonical home for the behavior-bearing item families in
`crates/core/src/ast.rs` and `crates/core/src/types.rs`.

## Functions

A function item owns:

- a name
- generic parameters
- an optional `where` clause
- parameters
- an optional return type
- an effect list
- a block body
- visibility
- attributes

The AST shape is important because Kain does not treat functions as anonymous
expression sugar. A function is a first-class declaration that can be typed,
effect-checked, lowered, and re-used by method-bearing item families.

Important details:

- parameters can be mutable and can carry default expressions
- effects are part of the signature contract, not decoration
- attributes such as target-specific annotations live on the function item
- method functions are still `Function` values in the AST
- inline generic bounds and `where` bounds are stored distinctly in the AST and
  normalized by later semantic passes before constraint validation

When the typechecker processes a function, it resolves the function signature
and records the resolved type in `TypedFunction`. The runtime and backend lanes
consume that typed result instead of re-deriving the contract from scratch.

## Traits

A trait declaration owns:

- a name
- generic parameters
- an optional `where` clause
- supertraits
- a list of trait methods
- visibility
- a span

Trait methods are not just names. Each `TraitMethod` can carry:

- parameters
- an optional return type
- effects
- an optional default implementation block

That means a trait can define required behavior and also provide a default
behavior when the language wants one.

Traits are typed separately from impls. The current typechecker records trait
items as `TypedTrait` and uses trait method signatures when it checks
implementations and method bodies.

## Impl Blocks

An impl block owns:

- generic parameters
- an optional `where` clause
- an optional trait name
- trait generics
- the target type
- the method list
- a span

There are two important shapes:

- inherent impls, where `trait_name` is absent
- trait impls, where `trait_name` names the implemented trait

An impl block is checked against its target type, and each method is checked with
an explicit self type. This is why impls matter to lowering and not just to
syntax: method resolution, trait conformance, and domain-item codegen all depend
on the typed impl view.

## Where Clauses

Generic-bearing items can carry a real AST `where_clause` surface:

```kn
fn fold<T>(value: T) -> Int where T: Fold + Stable:
    return value.score()
```

The parser stores each generic name and bound list. The typechecker validates
that every constrained generic exists on the item, referenced traits are known,
and duplicate bounds are reported deterministically after inline and `where`
bounds are merged into one normalized set.

## How These Pieces Fit Together

The module graph introduces names. Functions provide behavior. Traits describe
contracts. Impl blocks connect behavior to a concrete type.

That separation matters because the same method-shaped function may be consumed
by several different lowering paths:

- plain language code
- structs with methods
- components and UI items
- actors and async/task items
- UE5-facing integration items
- graph and editor items

If a reader needs the exact item inventory, start with
[guides/syntax-and-semantics/modules-and-items.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/modules-and-items.md).
If they need the contract shape of a function or method, use this page.

## Source Files To Consult

- `crates/core/src/ast.rs`
- `crates/core/src/types.rs`
- `crates/core/src/runtime.rs`

## Practical Rule

If the question is "what does this function promise?" use this page.
If the question is "what does this function do at runtime?" use the runtime
pages.
If the question is "how does this method attach to a type?" use the impl
section above.
