# Language Overview

Kain is a compiled multi-target language with an executable runtime model.
It is not just a manifest or orchestration DSL.

## Mental Model

The core flow is:

`source -> lexer -> parser -> comptime -> typecheck -> runtime or backend`

The language core in `crates/kain-core` owns the semantics. Backends and
host bridges consume compiler-owned truth instead of redefining it.

## What Kain Can Express

Kain source is organized around several first-class domains:

- ordinary functions, structs, enums, traits, impls, modules, and constants
- compiler-owned intent items such as `patch`, `law`, `converge`, `world`, and `orchestrate`
- UI components and JSX
- shaders and GPU-oriented metadata
- actors, async tasks, and message passing
- UE5-facing material, graph, gameplay, and editor items
- compile-time execution and macro expansion
- low-level memory and ABI-oriented expressions

## Execution Paths

- `kain run` executes Kain in the interpreter/runtime lane.
- `kain test` uses the same core runtime semantics for validation.
- `kain build` lowers toward a selected target and may also stage runtime
  artifacts.
- `kain import-*` converts foreign source into Kain forms before compiling or
  emitting `.kn`.
- `kain selfhost` mirrors Rust workspace code into owned Kain source.

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
2. the runtime behavior in `crates/kain-core/src/runtime.rs`
3. the target and capability rules in `crates/kain-core/src/language_features.rs`
4. the emitted artifact or host bridge that consumes the result
