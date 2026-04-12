# Modules And Imports

This page covers the declarations that can live at module scope, plus the way
`use` and `mod` shape the import graph that the loader and runtime resolve.

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

## Module And Import Rules

- `mod` creates a nested module boundary and gives the compiler a structured
  namespace tree.
- `use` is the current name-resolution mechanism. It is how authored code
  reaches local items, stdlib entries, and imported modules.
- stdlib loading and module resolution are related but not identical. The stdlib
  loader honors `KAIN_STDLIB_PATH` and `KAIN_STDLIB_PROFILE`, while the module
  graph resolves authored `use` and `mod` paths inside the current program and
  project layout.
- imports are not just syntax sugar. They are part of the language model that
  later runtime and target lanes rely on when they resolve names.

## Compiler-Owned Intent Quartet

`patch`, `law`, `converge`, `world`, and `orchestrate` are not just ordinary
declarations. They lower into runtime and bundle metadata that downstream
tooling consumes directly.

## Runtime-And-Domain Families

The language surface also includes runtime- and toolchain-facing item families:

- `Component` and `Shader`
- `Actor`
- `MaterialGraph` and `MaterialFunction`
- `GraphEditor` and `GraphRuntime`
- `StateMachine`
- `AsyncTask`
- `EditorModule`
- `GameplayTags`, `GameplayAbility`, `GameplayEffect`, `GameplayCue`,
  `AbilityTask`, and `TargetActor`

Read `syntax-and-semantics/domain-items.md` for the lowering details of those
families.

## Module Rule

Modules can be inline or nested, and `use`/`mod` shape the import graph that
the loader and runtime use to resolve names.

## Practical Rule

If a reader asks where a name comes from, start with this page. If they ask how
that name behaves at runtime or after lowering, send them to the runtime,
effects, or domain-item chapters instead of repeating the import story here.
