# Legacy Crosswalk

Snapshot: April 12, 2026.

This page bridges older Kain documentation into the current canonical guide tree. Use it when you land in stale prose, old folder names, or historical terms from the legacy `docs/` tree.

Rule of thumb: if a topic exists in current code, trust the live source and the matching page under `guides/`. If a topic only exists in old docs, treat it as historical until code or current CLI proves otherwise.

## Current Canonical Homes

| Legacy topic family | Current guide home |
| --- | --- |
| First run, toolchain bootstrap, and basic file execution | [guides/quickstart.md](/home/ephemara/Dev/Kain/guides/quickstart.md) |
| What Kain is and how the execution model works | [guides/language-overview.md](/home/ephemara/Dev/Kain/guides/language-overview.md) |
| Syntax, items, types, patterns, expressions, statements, modules, module resolution, functions, traits, impls, macros, comptime | [guides/syntax-and-semantics/](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/) |
| Low-level memory, provenance, and ABI-aware lowering | [guides/syntax-and-semantics/low-level-memory.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/low-level-memory.md) |
| Runtime semantics, stdlib loading, builtins, async, patching, actors | [guides/runtime/](/home/ephemara/Dev/Kain/guides/runtime/) |
| Compiler-owned intents and runtime contracts | [guides/runtime/compiler-owned-intents.md](/home/ephemara/Dev/Kain/guides/runtime/compiler-owned-intents.md) |
| Native C ABI, service table, helpers, actor lifecycle | [guides/native-c-runtime/](/home/ephemara/Dev/Kain/guides/native-c-runtime/) |
| CLI commands, flags, build lanes, importers, Omni, Fabric, selfhost, native UI | [guides/cli/](/home/ephemara/Dev/Kain/guides/cli/) |
| Omni pipeline concepts and staged import fan-out | [guides/pipelines/omni.md](/home/ephemara/Dev/Kain/guides/pipelines/omni.md) |
| Fabric pipeline concepts, runtime kinds, and report output | [guides/pipelines/fabric.md](/home/ephemara/Dev/Kain/guides/pipelines/fabric.md) |
| UE5 project layout, module inference, validation, and generated outputs | [guides/ue5/overview.md](/home/ephemara/Dev/Kain/guides/ue5/overview.md) |
| Workspace crate roles and boundaries | [guides/crates/](/home/ephemara/Dev/Kain/guides/crates/) |
| Smoke lanes, workflow examples, and proof surfaces | [docs/examples/README.md](/home/ephemara/Dev/Kain/docs/examples/README.md) |
| Product-shaped local pipeline examples | [docs/examples/11_ultimate_kain_pipeline.kn](/home/ephemara/Dev/Kain/docs/examples/11_ultimate_kain_pipeline.kn) |
| UE5-authored item gallery for the current checkout | [docs/examples/09_ue5_authoring_gallery.kn](/home/ephemara/Dev/Kain/docs/examples/09_ue5_authoring_gallery.kn) |
| Curated corpus and language mining examples | [docs/kn_library/README.md](/home/ephemara/Dev/Kain/docs/kn_library/README.md) |
| Feature, command, target, and glossary lookup | [guides/reference/](/home/ephemara/Dev/Kain/guides/reference/) |

## Legacy Terms And Where They Land

| Old term or theme | Current reading path |
| --- | --- |
| `USF`, shader bundles, and shader-canvas style output | [guides/syntax-and-semantics/domain-items.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/domain-items.md), [guides/runtime/stdlib-and-builtins.md](/home/ephemara/Dev/Kain/guides/runtime/stdlib-and-builtins.md), and the current UE5 example lanes |
| Raw pointers, imported pointers, and ABI-aware memory lowering | [guides/syntax-and-semantics/low-level-memory.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/low-level-memory.md) |
| Module resolution, visibility, and stdlib lookup | [guides/syntax-and-semantics/module-resolution.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/module-resolution.md) |
| `ks`, `KainScript`, and script-lane execution | [guides/reference/target-matrix.md](/home/ephemara/Dev/Kain/guides/reference/target-matrix.md) and [guides/cli/cli-overview.md](/home/ephemara/Dev/Kain/guides/cli/cli-overview.md) |
| Omni manifests and staged import orchestration | [guides/cli/selfhost-omni-fabric-lsp.md](/home/ephemara/Dev/Kain/guides/cli/selfhost-omni-fabric-lsp.md) and [guides/pipelines/omni.md](/home/ephemara/Dev/Kain/guides/pipelines/omni.md) |
| Fabric manifests, runtime adapters, and contract validation | [guides/cli/selfhost-omni-fabric-lsp.md](/home/ephemara/Dev/Kain/guides/cli/selfhost-omni-fabric-lsp.md) and [guides/pipelines/fabric.md](/home/ephemara/Dev/Kain/guides/pipelines/fabric.md) |
| C, Rust, TypeScript, and assembly import workflows | [guides/cli/importers.md](/home/ephemara/Dev/Kain/guides/cli/importers.md) |
| UE5 packaging, plugin generation, and inject flows | [guides/cli/native-ui-and-packaging.md](/home/ephemara/Dev/Kain/guides/cli/native-ui-and-packaging.md) and [docs/examples/09_ue5_authoring_gallery.kn](/home/ephemara/Dev/Kain/docs/examples/09_ue5_authoring_gallery.kn) |
| Native UI and app bundle materialization | [guides/cli/native-ui-and-packaging.md](/home/ephemara/Dev/Kain/guides/cli/native-ui-and-packaging.md) |
| Compiler-owned intents (`law`, `patch`, `converge`, `world`, `orchestrate`) | [guides/runtime/compiler-owned-intents.md](/home/ephemara/Dev/Kain/guides/runtime/compiler-owned-intents.md), [guides/runtime/effects-io-async-and-patching.md](/home/ephemara/Dev/Kain/guides/runtime/effects-io-async-and-patching.md), and [docs/examples/08_world_patch_law_converge_and_local_orchestrate.kn](/home/ephemara/Dev/Kain/docs/examples/08_world_patch_law_converge_and_local_orchestrate.kn) |
| Function, trait, and impl semantics | [guides/syntax-and-semantics/functions-traits-and-impls.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/functions-traits-and-impls.md) |

## Historical Only Unless Code Says Otherwise

If a legacy document uses a topic that no longer appears in the live code or current CLI, treat it as historical until proven current by the source tree.

Common examples of this rule are old target names, old packaging wording, and older UE5 or USF terminology that has been superseded by the current `guides/` tree and live command surface.

## How To Use This Page

1. Find the old topic name.
2. Jump to the current canonical page listed above.
3. Verify the behavior against the live code or CLI if the old prose and the current page disagree.
4. Leave the `docs/` tree alone unless a separate cleanup pass explicitly calls for it.
