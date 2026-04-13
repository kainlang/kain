# Kain Ultimate Guide Set

Snapshot: April 12, 2026.

This tree is the canonical long-form documentation for Kain in this checkout.
It is source-driven and meant to be read alongside the code, not as a
standalone marketing layer.

## Read This First

1. `quickstart.md` for the fastest end-to-end path.
2. `reference/legacy-crosswalk.md` when you are translating from old prose or old folder names.
3. `language-overview.md` for the mental model.
4. `syntax-and-semantics/` for the language surface.
5. `runtime/` for execution, stdlib, and runtime behavior.
6. `native-c-runtime/` for the C ABI floor.
7. `cli/` for commands, flags, packaging lanes, and target/codegen rules.
8. `pipelines/omni.md` and `pipelines/fabric.md` for Omni and Fabric orchestration concepts.
9. `ue5/overview.md` for the Unreal-facing conceptual guide.
10. `crates/` for workspace structure and crate roles.
11. `examples/` for proof surfaces and repo-local workflows.
12. `reference/` for matrices, glossary terms, troubleshooting, and legacy
   crosswalks.

## Canonical Sources

These files define the truth that the guides are derived from:

- `crates/kain-core/src/ast.rs`
- `crates/kain-core/src/runtime.rs`
- `crates/kain-core/src/stdlib.rs`
- `crates/kain-core/src/language_features.rs`
- `crates/kain-core/src/low_level_memory.rs`
- `crates/kain-core/src/low_level_abi.rs`
- `crates/cli/src/main.rs`
- `crates/cli/src/packager/`
- `src/rust-import/cli/`
- `src/rust-import/cli/packager/`
- `src/rust-import/kain-omni/`
- `src/rust-import/kain-host/`
- `src/rust-import/ue5/`
- `src/rust-import/ue5-shaders/`
- `src/rust-import/ue5-config/`
- `src/rust-import/ue5-asset-utils/`
- `src/rust-import/ue5-materials/`
- `src/rust-import/ue5-blueprints/`
- `src/rust-import/ue5-graphs/`
- `src/rust-import/ue5-gas/`
- `src/rust-import/ue5-editor/`
- `runtime/native/`
- `smoketest/`
- `apps/`
- `unreal_plugins/`
- `kn_library/`

## Guide Map

| Guide | What it covers |
| --- | --- |
| `quickstart.md` | Install, doctor, run, build, and first artifacts |
| `reference/legacy-crosswalk.md` | Bridge from stale docs and legacy terms to the current guide tree |
| `language-overview.md` | Mental model, execution flow, and target model |
| `syntax-and-semantics/syntax.md` | Tokens, keywords, item families, and syntax shape |
| `syntax-and-semantics/types.md` | All type forms and layout-aware type behavior |
| `syntax-and-semantics/effects-and-capabilities.md` | Effect vocabulary, feature gating, and target support |
| `syntax-and-semantics/patterns.md` | Match patterns and binding forms |
| `syntax-and-semantics/expressions.md` | Expressions, operators, and control flow |
| `syntax-and-semantics/statements.md` | Statements, blocks, and statement-level flow |
| `syntax-and-semantics/modules-and-items.md` | Modules, visibility, and top-level item kinds |
| `syntax-and-semantics/module-resolution.md` | `mod`, `use`, stdlib lookup, and module path encoding |
| `syntax-and-semantics/functions-traits-and-impls.md` | Function signatures, traits, and impl blocks |
| `syntax-and-semantics/macros-and-comptime.md` | Macros, comptime, and code generation seams |
| `syntax-and-semantics/async-actors-and-concurrency.md` | Async, tasks, actors, and message passing |
| `syntax-and-semantics/low-level-memory.md` | Pointer provenance, raw memory forms, and ABI-aware lowering |
| `syntax-and-semantics/domain-items.md` | Components, shaders, materials, graphs, GAS, and editor items |
| `runtime/runtime-model.md` | Interpreter model, runtime state, and environment wiring |
| `runtime/stdlib-and-builtins.md` | Source stdlib loader, runtime natives, and helper functions |
| `runtime/compiler-owned-intents.md` | `patch`, `law`, `converge`, `world`, and `orchestrate` runtime contracts |
| `runtime/effects-io-async-and-patching.md` | Effects, I/O, async, and patch semantics |
| `runtime/native-runtime-overview.md` | Native runtime role in the LLVM/native lane |
| `native-c-runtime/abi-contract.md` | ABI contract, versions, startup validation, and compatibility |
| `native-c-runtime/service-table.md` | Canonical service table and provider model |
| `native-c-runtime/helper-abi.md` | Low-level helper ABI for memory and layout operations |
| `native-c-runtime/error-codes.md` | Stable diagnostic families and error ranges |
| `native-c-runtime/actor-lifecycle.md` | Actor ownership, lifecycle, mailbox, and supervision |
| `cli/cli-overview.md` | CLI shape, launcher behavior, and command families |
| `cli/build-run-init.md` | `init`, `build`, `run`, and the build/materialization lanes |
| `cli/targets-and-codegen.md` | Target alias families, KainScript, and codegen output rules |
| `cli/importers.md` | `import-asm`, `import-c`, `import-rust`, `import-crate`, `import-ts` |
| `cli/doctor-and-repair.md` | `doctor` diagnostics plus repair modes |
| `cli/selfhost-omni-fabric-lsp.md` | `selfhost`, `omni`, `fabric`, and `lsp` |
| `cli/native-ui-and-packaging.md` | Native UI, inject, UE5 packaging, and artifact staging |
| `pipelines/omni.md` | Omni manifest shape, staged imports, and target fan-out |
| `pipelines/fabric.md` | Fabric runtime manifests, adapters, and execution reports |
| `ue5/overview.md` | UE5 project layout, validation, plugin generation, and engine knowledge |
| `crates/index.md` | Workspace crate inventory and grouping |
| `crates/compiler-core.md` | Core compiler, importers, repair, and orchestration crates |
| `crates/runtime-and-host.md` | Host stack, FFI, embedding, and bridge crates |
| `crates/ui-gpu-3d.md` | UI, GPU, native desktop, and 3D runtime crates |
| `crates/ue5-and-targets.md` | UE5 and target-adapter crates |
| `examples/smoketest.md` | Runtime proof matrix and smoke lanes |
| `examples/apps.md` | First-class applications and prototypes |
| `examples/unreal-plugins.md` | UE5 plugin examples and archived lanes |
| `examples/kn-library.md` | Corpus library and curated source examples |
| `reference/feature-matrix.md` | Coverage matrix for every major feature family |
| `reference/command-matrix.md` | CLI command matrix and canonical command entrypoints |
| `reference/target-matrix.md` | Compile targets, aliases, and output families |
| `reference/troubleshooting.md` | Common failures and operator fixes |
| `reference/glossary.md` | Shared Kain terminology |

## Notes

- This tree is the canonical guide set for the current checkout.
- The older `docs/` tree is legacy support material and may lag behind the code.
- When the docs and code disagree, trust the source files listed above.
