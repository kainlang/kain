# Language Overview

Kain is a compiled multi-target language with an executable runtime model.
It is not just a manifest language or orchestration DSL.

## What Kain Is For

Kain is designed to cover four things at once:

- authored language meaning
- direct runtime execution
- cross-target lowering and materialization
- host bridges and orchestration lanes that consume the same compiler truth

The language core in `crates/kain-core` owns meaning. Backends, importers, and
host bridges consume compiler-owned truth instead of redefining it.

## Mental Model

The core flow is:

`source -> lexer/parser -> module/import resolution -> comptime -> effects/capabilities -> typecheck -> runtime or backend`

That order matters.

- `use` and `mod` are real name-resolution mechanics, not decorative syntax.
- `comptime` runs before normal lowering when the compiler needs information
  early.
- effects and capabilities influence both parser/runtime behavior and target
  support.
- compiler-owned declarations such as `patch`, `law`, `converge`, `world`, and
  `orchestrate` lower into runtime and bundle contracts that downstream lanes
  consume directly.
- low-level memory forms carry pointer provenance and layout-sensitive
  lowering rules instead of flattening into generic pointer math.

## Reading Order

1. `syntax-and-semantics/syntax.md` for the surface map
2. `syntax-and-semantics/modules-and-items.md` for imports and item families
3. `syntax-and-semantics/module-resolution.md` for path encoding, visibility,
   and stdlib lookup
4. `syntax-and-semantics/types.md` and
   `syntax-and-semantics/effects-and-capabilities.md` for type forms, effect
   gating, and target support
5. `syntax-and-semantics/low-level-memory.md` for pointer provenance, ABI
   lowering, and memory operations
6. `syntax-and-semantics/functions-traits-and-impls.md` for function
   signatures, trait contracts, and impl blocks
7. `syntax-and-semantics/expressions.md` and `statements.md` for executable
   syntax
8. `syntax-and-semantics/macros-and-comptime.md` for compile-time behavior
9. `runtime/runtime-model.md` for execution semantics
10. `runtime/stdlib-and-builtins.md` for source stdlib and native helpers
11. `runtime/compiler-owned-intents.md` for `patch`, `law`, `converge`,
   `world`, and `orchestrate`
12. `runtime/effects-io-async-and-patching.md` for effects and runtime-owned
    contracts
13. `native-c-runtime/abi-contract.md` and `service-table.md` for the native ABI
14. `cli/targets-and-codegen.md` and `reference/target-matrix.md` for target
    aliases, codegen lanes, and output families

## What Kain Can Express

Kain source is organized around several first-class domains:

- ordinary functions, structs, enums, traits, impls, modules, type aliases, and
  constants
- compiler-owned intent items such as `patch`, `law`, `converge`, `world`, and
  `orchestrate`
- UI components and JSX
- shaders and GPU-oriented metadata
- actors, async tasks, and message passing
- UE5-facing material, graph, gameplay, and editor items
- compile-time execution and macro expansion
- low-level memory and ABI-oriented expressions
- stage calls into Kain, Rust, Python, or Node when a workflow needs a separate
  execution lane

## Execution Paths

- `kain run` executes Kain in the interpreter/runtime lane.
- the `test` runtime lane uses the same core runtime semantics for validation.
- `kain build` lowers toward a selected target and may also stage runtime
  artifacts.
- `kain import-*` converts foreign source into Kain forms before compiling or
  emitting `.kn`.
- `kain selfhost` mirrors Rust workspace code into owned Kain source.
- `kain omni`, `kain fabric`, and `kain build native-ui` are orchestration and
  materialization lanes that consume the same compiler truth from different
  angles.
- `kain omni` and `kain fabric` each have their own conceptual pipeline pages,
  separate from the command pages.

## Target Model

The compiler currently recognizes these compile target families:

- web: `wasm`, `js`, `ts`, `hybrid`, `ks`
- native/system: `llvm`, `rust`, `cpp`
- GPU: `spirv`, `hlsl`, `usf`
- UE5: `ue5`, `ue5editor`
- runtime lanes: `interpret`, `test`

Some features are target-sensitive, and the runtime capability registry can
gate parser or execution support separately from target selection.

## Read This As A Contract

If you want to understand a feature, follow this order:

1. the AST shape in `crates/kain-core/src/ast.rs`
2. the type and layout behavior in `crates/kain-core/src/types.rs`
3. the runtime behavior in `crates/kain-core/src/runtime.rs`
4. the target and capability rules in `crates/kain-core/src/language_features.rs`
5. the emitted artifact or host bridge that consumes the result
