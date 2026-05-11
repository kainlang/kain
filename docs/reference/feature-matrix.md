# Feature Matrix

Snapshot: April 12, 2026.

This page is the coverage index for the live Kain language and runtime surface.
It maps each feature family to the source file that defines it and the guide that
explains it.

For the flagship DCC parity inventory, use
`reference/dcc-parity-matrix.md` plus
`apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json`.

## Truth Sources

- `crates/kain-core/src/ast.rs`
- `crates/kain-core/src/types.rs`
- `crates/kain-core/src/effects.rs`
- `crates/kain-core/src/runtime.rs`
- `crates/kain-core/src/stdlib.rs`
- `crates/kain-core/src/language_features.rs`
- `crates/kain-core/src/runtime_contract.rs`
- `crates/kain-core/src/realtime_app_bundle.rs`
- `crates/kain-entangle/src/lib.rs`
- `runtime/native/include/*.h`
- `src/.rustimport/reference/kain-omni/lib.kn`
- `src/.rustimport/reference/kain-omni/fabric.kn`
- `src/.rustimport/reference/kain-host/fabric.kn`
- `src/.rustimport/reference/cli/packager/ue5_pipeline.kn`
- `src/.rustimport/reference/ue5/*`
- `src/.rustimport/reference/ue5-shaders/*`
- `src/.rustimport/reference/ue5-config/*`
- `src/.rustimport/reference/ue5-asset-utils/*`
- `src/.rustimport/reference/ue5-materials/*`
- `src/.rustimport/reference/ue5-blueprints/*`
- `src/.rustimport/reference/ue5-graphs/*`
- `src/.rustimport/reference/ue5-gas/*`
- `src/.rustimport/reference/ue5-editor/*`
- `crates/cli/src/main.rs`

## Capability Flags

The capability registry in `crates/kain-core/src/language_features.rs` is the
current feature gate for parser and runtime behavior.

| Capability | Default | What it gates |
| --- | --- | --- |
| `ParserStructLiterals` | unsettled | `Type { field: value }` struct literal syntax |
| `ParserBitwiseAnd` | yes | parser support for `&` |
| `ParserBitwiseOr` | yes | parser support for `|` |
| `ParserBitwiseXor` | yes | parser support for `^` |
| `ParserShiftLeft` | yes | parser support for `<<` |
| `ParserShiftRight` | yes | parser support for `>>` |
| `RuntimeBitwiseAnd` | yes | runtime execution of `&` on integers |
| `RuntimeBitwiseOr` | yes | runtime execution of `|` on integers |
| `RuntimeBitwiseXor` | yes | runtime execution of `^` on integers |
| `RuntimeShiftLeft` | yes | runtime execution of `<<` on integers |
| `RuntimeShiftRight` | yes | runtime execution of `>>` on integers |

Note: `ParserStructLiterals` currently has a code/test disagreement. The
registry lists it in the default set, but the unit test still asserts it is
disabled. Treat the capability as unresolved until the implementation and tests
agree.

## AST Item Families

Every top-level item kind defined in `crates/kain-core/src/ast.rs` is covered
here.

| Family | Variants | Primary guide |
| --- | --- | --- |
| Core declarations | `Function`, `Struct`, `Enum`, `Trait`, `Impl`, `TypeAlias`, `Use`, `Mod`, `Const`, `Comptime`, `Macro`, `Test` | `syntax-and-semantics/modules-and-items.md`, `syntax-and-semantics/module-resolution.md`, `syntax-and-semantics/functions-traits-and-impls.md`, `syntax-and-semantics/macros-and-comptime.md` |
| Compiler-owned program contracts | `Patch`, `Law`, `Converge`, `World`, `Entangle`, `Orchestrate` | `runtime/compiler-owned-intents.md`, `runtime/effects-io-async-and-patching.md` |
| Runtime and domain items | `Component`, `Shader`, `Actor`, `MaterialGraph`, `MaterialFunction`, `GraphEditor`, `GraphRuntime`, `StateMachine`, `AsyncTask`, `EditorModule`, `GameplayTags`, `GameplayAbility`, `GameplayEffect`, `GameplayCue`, `AbilityTask`, `TargetActor` | `syntax-and-semantics/domain-items.md`, `syntax-and-semantics/async-actors-and-concurrency.md` |

Domain enums that matter to authored code are also part of the surface:

- `WorldSurfaceKind` = `NativeUi`, `Viewport3d`, `Web`, `Ue5`
- `OrchestrateStageRuntime` = `Kain`, `Rust`, `Python`, `Node`
- `ShaderStage` = `Vertex`, `Fragment`, `Compute`, `Surface`
- `CueType` = `Static`, `Actor`
- `TraceType` = `Line`, `Sphere`, `Cone`, `Box`, `Cylinder`

## Type Forms

The type system currently exposes these forms:

- `Named`
- `Tuple`
- `Array`
- `Slice`
- `Ref`
- `Ptr`
- `Function`
- `Option`
- `Result`
- `Infer`
- `Never`
- `Unit`
- `Impl`

Pointer provenance is tracked separately:

- `Raw`
- `ImportedC`
- `ImportedAsm`
- `LoweredRef`

## Pattern Forms

Patterns currently include:

- `Wildcard`
- `Literal`
- `Binding`
- `Struct`
- `Tuple`
- `Variant`
- `Slice`
- `Or`
- `Range`

## Expression Families

Expressions are grouped by what they do rather than by parser branch names.

- Construction and access: `Ident`, `Struct`, `AggregateInit`, `EnumVariant`,
  `Array`, `Tuple`, `JSX`, `Paren`, `Field`, `Index`
- Calls and dispatch: `MacroCall`, `Call`, `StageCall`, `MethodCall`,
  `Lambda`
- Operators: `Binary`, `Unary`, `Assign`, `Cast`, `Range`
- Control flow: `If`, `Match`, `Block`, `Return`, `Break`, `Continue`, `Try`
- Async and concurrency: `Await`, `AsyncBlock`, `Spawn`, `SendMsg`
- Memory and low-level: `Ref`, `AddrOf`, `Deref`, `PtrOffset`, `MemLoad`,
  `MemStore`, `SizeOfType`, `AlignOfType`, `Alloca`, `Uninit`, `Alloc`,
  `Realloc`
- Compile-time: `Comptime`

Binary operators currently covered by the parser/runtime capability registry:

- arithmetic operators
- comparison operators
- logical operators
- bitwise operators
- assignment operators
- range operators

Unary operators currently covered:

- `Neg`
- `Not`
- `BitNot`
- `Ref`
- `RefMut`
- `Deref`

## Statements

Statement forms are:

- `Let`
- `Expr`
- `Return`
- `Break`
- `Continue`
- `For`
- `While`
- `Loop`
- `Item`

## Runtime and Bridge Features

| Family | Source of truth | Guide |
| --- | --- | --- |
| Interpreter and runtime semantics | `crates/kain-core/src/runtime.rs` | `runtime/runtime-model.md` |
| Builtins and stdlib loading | `crates/kain-core/src/runtime.rs`, `crates/kain-core/src/stdlib.rs` | `runtime/stdlib-and-builtins.md` |
| Effects | `crates/kain-core/src/effects.rs` | `runtime/effects-io-async-and-patching.md` |
| Low-level memory and provenance | `crates/kain-core/src/ast.rs`, `crates/kain-core/src/low_level_memory.rs`, `crates/kain-core/src/low_level_abi.rs` | `syntax-and-semantics/low-level-memory.md`, `native-c-runtime/helper-abi.md` |
| Compiler-owned intents | `crates/kain-core/src/ast.rs`, `crates/kain-core/src/runtime_contract.rs`, `crates/kain-core/src/realtime_app_bundle.rs` | `runtime/compiler-owned-intents.md` |
| Runtime contracts and realtime bundles | `crates/kain-core/src/runtime_contract.rs`, `crates/kain-core/src/realtime_app_bundle.rs` | `runtime/runtime-model.md`, `native-c-runtime/abi-contract.md` |
| Native C ABI | `runtime/native/include/*.h` | `native-c-runtime/*.md` |
| Omni orchestration | `src/.rustimport/reference/kain-omni/lib.kn` | `pipelines/omni.md`, `cli/selfhost-omni-fabric-lsp.md` |
| Fabric orchestration | `src/.rustimport/reference/kain-omni/fabric.kn`, `src/.rustimport/reference/kain-host/fabric.kn` | `pipelines/fabric.md`, `cli/selfhost-omni-fabric-lsp.md` |
| UE5 plugin pipeline | `src/.rustimport/reference/cli/packager/ue5_pipeline.kn`, `src/.rustimport/reference/ue5/*` | `ue5/overview.md`, `cli/native-ui-and-packaging.md` |
| Compile targets and output families | `crates/kain-core/src/lib.rs`, `crates/kain-driver/src/lib.rs` | `reference/target-matrix.md` |
| CLI command surface | `crates/cli/src/main.rs` | `reference/command-matrix.md` |

## Exhaustive Coverage Checklist

The following variant families are intentionally accounted for in the guide set:

- `Function`, `Patch`, `Law`, `Converge`, `World`, `Orchestrate`
- `PointerProvenance` and low-level memory expressions
- `Component`, `Shader`, `Actor`, `Struct`, `Enum`, `Trait`, `Impl`, `TypeAlias`
- `Use`, `Mod`, `Const`, `Comptime`, `Macro`, `Test`
- `MaterialGraph`, `MaterialFunction`, `GraphEditor`, `GraphRuntime`
- `StateMachine`, `AsyncTask`, `EditorModule`
- `GameplayTags`, `GameplayAbility`, `GameplayEffect`, `GameplayCue`
- `AbilityTask`, `TargetActor`

The guide coverage for those families is split across:

- `syntax-and-semantics/modules-and-items.md`
- `syntax-and-semantics/functions-traits-and-impls.md`
- `syntax-and-semantics/domain-items.md`
- `syntax-and-semantics/async-actors-and-concurrency.md`
- `syntax-and-semantics/macros-and-comptime.md`
- `runtime/runtime-model.md`
- `runtime/effects-io-async-and-patching.md`
- `native-c-runtime/actor-lifecycle.md`
