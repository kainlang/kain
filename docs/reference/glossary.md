# Glossary

Snapshot: April 12, 2026.

This glossary keeps the terminology in the guide set consistent.

| Term | Meaning |
| --- | --- |
| `Kain` | the compiled multi-target language and runtime system in this repo |
| `kain` | the explicit CLI entrypoint |
| `kn` | the run-first launcher that favors interpret mode |
| `KAIN.toml` | the standard project manifest for build/materialization lanes |
| `KAIN.omni.toml` | the manifest used by the omni orchestration lane |
| `KAIN.fabric.toml` | the manifest used by the Fabric lane |
| `KainScript` | the script-oriented compile target family exposed as `ks` |
| `AST` | the language tree produced by parsing Kain source |
| `typed program` | the AST after type checking and semantic validation |
| `comptime` | compile-time evaluation performed before type checking/codegen |
| `effect` | a runtime or semantic permission such as `IO`, `Async`, or `GPU` |
| `patch` | a compiler-owned item that represents mutation or collaboration flow |
| `law` | a compiler-owned item for rule-like semantic contracts |
| `converge` | a compiler-owned item for reconciliation or agreement logic |
| `world` | a runtime surface or application world definition |
| `orchestrate` | a cross-runtime stage pipeline that can run in Kain, Rust, Python, or Node |
| `component` | a UI-facing item that can become part of a runtime bundle |
| `actor` | a concurrent runtime entity with mailbox and supervision behavior |
| `shader` | a GPU-oriented item that can target SPIR-V, HLSL, USF, or UE5 lanes |
| `material graph` | a graph-oriented rendering or material authoring item |
| `runtime contract` | the JSON bundle that describes what the runtime must provide |
| `realtime app bundle` | the compiled bundle that captures app, UI, shader, and tool metadata |
| `service table` | the native runtime capability catalog exposed by `runtime/native` |
| `helper ABI` | the low-level memory and layout helper contract used by codegen |
| `pointer provenance` | the compiler-tracked origin of a pointer value, such as raw, imported C, imported ASM, or lowered reference |
| `low-level memory` | the language surface that covers raw pointers, layout queries, and ABI-aware allocation/load/store lowering |
| `runtime-owned intent` | a compiler-owned declaration such as `patch`, `law`, `converge`, `world`, or `orchestrate` that lowers into bundle metadata |
| `host bridge` | a lane that connects Kain to Rust, Python, Node, C, or other host ecosystems |
| `importer` | a source conversion lane such as `import-c`, `import-rust`, or `import-ts` |
| `materialization` | the process of turning Kain source into a project, plugin, or app bundle |
| `selfhost` | the pipeline that mirrors and repairs Kain source into Kain itself |
| `omni` | the mixed-language orchestration layer for manifests and staged imports |
| `fabric` | the local-first manifest and session orchestration lane |
| `Oracle` | the UE5 semantic validator that runs before C++ generation |
| `module graph` | the UE5 module/dependency map used to infer `Build.cs` dependencies |
| `staged import` | an Omni-imported foreign source file that has been normalized into generated `.kn` before fan-out |
| `UE5` | the Unreal Engine 5 target-adapter surface for plugin and editor generation |
| `smoke` | a focused runnable proof that a feature still works |
| `lane` | a single runtime, build, or tooling path inside the workspace |
| `target` | a compile or execution destination such as `wasm`, `llvm`, or `ue5` |
| `runtime native` | the manifest-driven C runtime substrate in `runtime/native` |
| `reflection payload` | metadata used to inspect types, items, or scenes at runtime |

## Important Cross-Cutting Terms

- `Patch`, `Law`, `Converge`, `World`, and `Orchestrate` are not just naming
  conventions. They are first-class AST item kinds.
- `NativeUi`, `Viewport3d`, `Web`, and `Ue5` are world surface kinds.
- `Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`, `Alloc`, and `Panic` are
  the current effect set.
- `Runtime`, `Editor`, `Program`, `UncookedOnly`, and `Developer` are UE5
  module types used by the packaging/configuration lanes.
