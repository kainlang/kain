# Modules And Top-Level Items

This page covers the declarations that can live at module scope.

## Top-Level Item Inventory

| Item | Purpose |
| --- | --- |
| `Function` | Regular executable function |
| `Patch` | Transactional mutation block with undo semantics |
| `Law` | Invariant-checking function with boolean truth contract |
| `Converge` | Spec-plus-fast-lane dispatcher declaration |
| `World` | State and surface projection declaration |
| `Orchestrate` | Multi-stage pipeline declaration |
| `Component` | Reactive UI component |
| `Shader` | GPU program or surface shader |
| `Actor` | Message-driven concurrent entity |
| `Struct` | Product type with fields and methods |
| `Enum` | Sum type with variants |
| `Trait` | Interface contract |
| `Impl` | Trait or inherent implementation block |
| `TypeAlias` | Named alias for a type |
| `Use` | Import path |
| `Mod` | Nested module |
| `Const` | Compile-time constant binding |
| `Comptime` | Compile-time executable block |
| `Macro` | Macro definition |
| `Test` | Test-only executable block |
| `MaterialGraph` | UE-style material graph |
| `MaterialFunction` | Reusable material function graph |
| `GraphEditor` | Node editor schema |
| `GraphRuntime` | Runtime graph contract |
| `StateMachine` | Explicit state machine definition |
| `AsyncTask` | Async task description |
| `EditorModule` | Editor integration module |
| `GameplayTags` | Gameplay tag namespace |
| `GameplayAbility` | Gameplay ability system item |
| `GameplayEffect` | Gameplay effect system item |
| `GameplayCue` | Gameplay cue system item |
| `AbilityTask` | Ability task item |
| `TargetActor` | Targeting system item |

## Shared Declaration Features

Most item kinds share the same metadata model:

- visibility: private, public, crate, or super
- attributes/decorators
- generics where applicable
- spans for diagnostics

## Compiler-Owned Intent Quartet

`patch`, `law`, `converge`, `world`, and `orchestrate` are not just ordinary
declarations. They lower into runtime and bundle metadata that downstream
tooling consumes directly.

## Module Rule

Modules can be inline or nested, and `use`/`mod` shape the import graph that
the loader and runtime use to resolve names.
