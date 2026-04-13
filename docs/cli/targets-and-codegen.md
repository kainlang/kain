# Targets And Codegen

Snapshot: April 12, 2026.

This page explains the live target registry, the difference between compile
targets and orchestration commands, and where KainScript belongs in the current
surface.

## Two Different Questions

When you are choosing a lane, first decide whether you are asking:

1. what target should this program lower to?
2. what workflow command should build or package it?

`kain-core::CompileTarget` answers the first question. The CLI command surface
answers the second. The docs should keep those two ideas separate.

## Current Target Families

| Family | Typical aliases | What it means |
| --- | --- | --- |
| Web and scripting | `wasm`, `js`, `ts`, `hybrid`, `ks` | browser, script, and KainScript outputs |
| Native and system | `llvm`, `rust`, `cpp` | native lowering and source-emission lanes |
| GPU and shader | `spirv`, `hlsl`, `usf` | shader and compute output families |
| UE5 | `ue5`, `ue5editor` | plugin and editor-facing codegen |
| Runtime lanes | `interpret`, `test` | execution and validation paths |

For the exact alias matrix, use `reference/target-matrix.md`. That page is the
canonical table; this page is the conceptual explanation.

## Workflow Versus Alias

Workflow commands and compile targets are different layers.

| Workflow | Target or alias | What it does |
| --- | --- | --- |
| `kain run` | runtime execution lane | executes the program through the interpreter/runtime path |
| `kain build -t interpret` | `Interpret` | explicit runtime target alias |
| `kain build -t test` | `Test` | validation runtime lane |
| `kain build --ue5` | UE5 packaging workflow | packages a plugin from `KAIN.toml` |
| `kain build -t ue5` | `Ue5` | single-file UE5 codegen |
| `kain build -t ue5editor` | `Ue5Editor` | editor-facing UE5 codegen |
| `kain build native-ui` | native-ui workflow | materializes a desktop app bundle |

Keep the command and the target separate in prose. The same language program can
flow through either surface, but the artifact and the operator story are not the
same.

## KainScript

KainScript is the `Ks` target family. It is not an importer and not a separate
runtime. It is a compile target that shares the JavaScript stdlib profile and
uses the `ks` / `kainscript` / `kscript` aliases in the live target registry.

Use the KainScript lane when you want:

- script-oriented output
- JS stdlib sharing
- a direct target rather than a manifest or importer workflow

## Target-Sensitive Codegen

Different targets imply different output families:

- `wasm` emits WebAssembly modules
- `js` and `ts` emit script source
- `rust` emits Rust source
- `cpp` emits C++ source
- `spirv`, `hlsl`, and `usf` emit shader source or binaries
- `ue5` and `ue5editor` emit plugin-oriented outputs and generated headers
- `interpret` and `test` are runtime target aliases instead of source-emitting
  targets

That distinction matters because `build`, `run`, `gpu-artifacts`, `native-ui`,
and UE5 packaging all consume the same language truth but materialize it in
different ways.

## UE5 And Native UI Are Not The Same

- `kain build --ue5` packages a UE5 plugin from `KAIN.toml`
- `kain build <file> -t ue5` emits single-file UE5 codegen artifacts
- `kain build native-ui` materializes a native desktop app bundle
- `kain inject --ue5` updates an existing plugin without treating the lane as a
  full rebuild

Keep those workflows separate when you document or debug them.

## Source Files To Consult

- `crates/kain-core/src/lib.rs`
- `crates/kain-driver/src/lib.rs`
- `crates/cli/src/main.rs`
- `crates/cli/src/native_ui_build.rs`
- `crates/cli/src/packager/`
- `crates/web/src/codegen_ks.rs`

## Practical Rule

If a reader asks “what should I pass to `-t`?”, send them here and to
`reference/target-matrix.md`. If they ask “what command should I run?”, send
them to the command matrix or the workflow page that owns that lane.
