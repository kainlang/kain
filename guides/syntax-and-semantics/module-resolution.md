# Module Resolution

Snapshot: April 12, 2026.

This page explains how Kain turns `mod`, `use`, inline module trees, and stdlib
profiles into concrete names that the runtime and typechecker can resolve.

## What This Page Owns

Use this page when you need to understand:

- where a name comes from
- how nested modules encode their path
- how stdlib lookup differs from authored module lookup
- how runtime aliases and type names are encoded

## `mod` And `use`

`mod` creates nested module structure. `use` creates a resolution edge into that
structure.

The important practical rule is that `use` is not just decorative syntax. It
participates in the program's import graph, and the compiler uses that graph when
it resolves authored names, module-local names, and imported names.

`mod` and `use` are therefore part of the semantic model, not just parser sugar.

## Inline Module Trees

The live code registers inline module children recursively.

- runtime aliases are encoded with `module__name`
- type and reflection aliases are encoded with `module::name`

That means the same authored module tree can appear in two different forms:

- as runtime values that the interpreter can look up directly
- as type names that the typechecker and reflection payload can carry forward

For example, a nested item inside a module path may appear as:

- `app__ui__Button` at runtime
- `app::ui::Button` in type and reflection data

## Visibility

Visibility still matters even when the name is available in the module graph.
Current authored items can be private, public, crate-visible, or super-visible,
and that affects whether a declaration is intended to escape its module boundary.

If you are writing docs, do not flatten visibility into a generic "public or
private" story. Kain has more than two levels here, and the distinction is part
of the language model.

## Stdlib Versus Project Lookup

The stdlib loader and the authored module graph are separate mechanisms.

The stdlib loader searches in this order:

1. `KAIN_STDLIB_PATH`
2. a sibling `stdlib/` beside the compiler binary
3. a workspace `stdlib/`

It also respects `KAIN_STDLIB_PROFILE`.

The authored module graph, by contrast, resolves the program's own nested
modules, imported items, and inline modules. That is why a missing stdlib entry
and a missing authored module are different failures.

## Practical Reading Order

1. `syntax-and-semantics/syntax.md`
2. `syntax-and-semantics/modules-and-items.md`
3. `syntax-and-semantics/module-resolution.md`
4. `runtime/stdlib-and-builtins.md`
5. `language-overview.md`

## Source Files To Consult

- `crates/kain-core/src/runtime.rs`
- `crates/kain-core/src/types.rs`
- `crates/kain-core/src/stdlib.rs`
- `crates/kain-core/src/runtime_contract.rs`

## Practical Rule

If the question is "why did this name resolve?" start with the module tree and
then check the stdlib loader.
If the question is "why did the runtime/typed name look different?" inspect the
module path encoding rules above.
