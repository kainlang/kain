# Compiler Core Crates

These crates define the language frontend, importer pipeline, orchestration
lanes, and repair/bootstrap tooling.

## Core Truth

- `kain-core` owns the AST, type system, runtime execution, low-level memory
  lowering, compiler-owned intent contracts, runtime contracts, and realtime
  bundles.
- `kain-driver` turns compiler-owned truth into emitted artifacts and
  cross-target materialization.
- `kain-check` and `kain-test` own reusable validation and source-suite harness
  semantics for `kain check`, `kain test`, IDEs, CI, and future agents.

## Import And Bootstrap

- `kain-import` converts foreign source into Kain forms.
- `kain-asm` handles legacy assembly import.
- `kain-build` and `kain-selfhost` support build/bootstrap workflows.
- `kain-repair` powers the `doctor --repair` lane.
- `kain-omni` owns mixed-language omni manifests and staging.

## CLI Surface

`cli` stays thin and delegates to the core and orchestration crates.

## What To Reach For

- use `kain-core` when the question is “what does Kain mean?”
- use `kain-driver` when the question is “what artifact do we emit?”
- use `kain-check` / `kain-test` when the question is “how do we prove this
  source or suite still behaves?”
- use the importer crates when the question is “how does this foreign source
  become Kain?”
- read `guides/syntax-and-semantics/low-level-memory.md` and
  `guides/runtime/compiler-owned-intents.md` when the question is about the
  language-to-runtime lowering boundary.
