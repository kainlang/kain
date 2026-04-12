# UE5 Overview

Snapshot: April 12, 2026.

This page is the conceptual home for the UE5 lane. It explains how Kain turns a
`KAIN.toml` UE5 section into plugin-shaped Unreal outputs, validation results,
and generated code.

The CLI entrypoints are `kain build --ue5`, `kain build <file.kn> -t ue5`,
and `kain inject --ue5`.

## What The UE5 Lane Owns

The UE5 packager is not just a file writer. It handles:

- plugin detection and plugin layout
- module inference
- `Build.cs` generation
- `.uplugin` generation
- shader, material, graph, Blueprint, GAS, and editor artifact generation
- validation through the UE5 semantic validator, “The Oracle”
- engine knowledge and header/type remapping

## KAIN.toml

The UE5 configuration lives in the `[ue5]` section of `KAIN.toml`.

The current `Ue5Config` shape in `src/rust-import/cli/packager/config.kn`
contains:

| Field | Meaning |
| --- | --- |
| `plugin_name` | the Unreal plugin / module base name |
| `plugin_dir` | the plugin root directory, defaulting to `Plugins/` |
| `sources` | the Kain source files to package |
| `shaders` | shader source entries gathered by the packager |
| `copyright` | optional copyright string |
| `modular_output` | whether to emit per-file modular output |
| `stdlib_path` | optional explicit stdlib override |
| `engine_version` | optional Unreal serializer version hint |
| `modules` | explicit UE5 module configuration |
| `plugin_dependencies` | extra plugin references for `.uplugin` |

If no manifest is present, the packager can still attempt auto-detection from
the plugin directory or `.uplugin` file, but `KAIN.toml` is still the canonical
project config for the lane.

`engine_version` accepts the common textual forms used by the code path, such as
`5.4`, `UE5_4`, and `VER_UE5_4`, and the packager falls back to the current UE5
target default when it is omitted.

## Plugin Generation

The pipeline produces normal Unreal-shaped outputs:

- `.uplugin`
- `Build.cs`
- runtime module source files
- editor module source files
- generated headers
- shader files
- material assets
- graph assets
- Blueprint and GAS artifacts
- config and asset-side metadata

The generated files are meant to be consumed by Unreal, not by a separate Kain
runtime.

The generated `.uplugin` file is data-driven. The current descriptor serializer
emits fields such as:

- `FileVersion`
- `Version`
- `VersionName`
- `FriendlyName`
- `Description`
- `Category`
- `CreatedBy`
- `CanContainContent`
- `Modules`
- `Plugins`

The module list can be a single runtime module or a split runtime/editor pair.
When the packager needs both, it emits `<PluginName>` and `<PluginName>Editor`
module entries with `Runtime` and `Editor` module types respectively.

## Plugin Layout Rules

The packager lays out the Unreal plugin on disk instead of leaving the structure
implicit.

- the plugin root defaults under the configured `plugin_dir`
- the runtime module lives under `Source/<PluginName>/`
- the editor module, when present, lives under `Source/<PluginName>Editor/`
- each module gets `Public/` and `Private/` subdirectories
- the corresponding `Build.cs` files are emitted as `Source/<PluginName>/<PluginName>.Build.cs`
  and, when needed, `Source/<PluginName>Editor/<PluginName>Editor.Build.cs`
- `Shaders/` is emitted when shader content is present
- split mode removes stale single-module layout files instead of leaving both
  layouts behind

That layout is what makes the generated tree feel like a normal Unreal plugin to
the engine, not a Kain-specific artifact bundle.

## Build.cs And Module Inference

`Build.cs` generation is data-driven.

The packager can infer dependencies from the imported symbols and feature set,
then add module dependencies such as:

- Slate and SlateCore
- UMG
- EnhancedInput
- OnlineSubsystem
- RenderCore, Renderer, and RHI
- Projects

When a `module_graph.json` file is present, the UE5 lane can use it to map
headers, types, APIs, and transitive dependencies more accurately. If it is
missing, the packager falls back to feature-based dependency selection.

The `module_graph.json` metadata is treated as a real UE5 dependency model. The
loader expects a `modules` array and an `include_to_module` map, and the in-memory
graph tracks type-to-module, header-to-module, API-to-module, and transitive
dependency information. The completeness checker also warns when common engine
modules such as `Core`, `CoreUObject`, `Engine`, `Slate`, or `SlateCore` are
missing from the graph.

The metadata bundle around the graph is broader than just the module map. The
validator also loads:

- `engine_knowledge.json`
- `module_graph.json`
- `uht_rules.json`
- `shader_knowledge.json`
- `widget_registry.json`

Optional companions include:

- `editor_attributes.json`
- `virtual_obligations.json`
- `codegen_rules.json`
- `engine_knowledge_expanded.json`

## Oracle Validation

Before C++ generation, the UE5 lane runs the semantic validator called the
Oracle.

The validator combines:

- engine knowledge
- UHT rules
- validation rules
- name collision checks
- replication and RPC checks
- data asset checks
- circular dependency checks

That is the part of the lane that makes UE5 authoring Kain-aware instead of
just templated text emission.

The lane also has a post-generation C++ validation step. If ReSharper C++ CLI
is available, the packager runs it and writes an XML report; otherwise it falls
back to a basic source-tree validation pass.

## Engine Knowledge And Header Mapping

`engine_knowledge.kn` stores the engine metadata used by the packager:

- engine classes
- engine structs
- engine enums
- type aliases
- include mappings
- constructor formats
- property string formats
- engine serializer versions

That knowledge lets the generator remap Kain types and member access to
Unreal-native names and headers instead of hardcoding one-off conversions.

## Generated Artifacts In Practice

The packager's output is more concrete than “generated C++.”

- modular output writes per-file module outputs rather than a single monolithic
  module
- modular mode emits a shared `KainStdlib.h` for the module set
- shader lanes emit UE5-facing shader glue plus `.usf`/C++ sidecars
- Blueprint and async-blueprint lanes emit headers and source files for the
  generated node / action classes
- state-machine and async-task lanes emit their own `U*` and `F*` wrappers
- GAS lanes emit headers for tags, abilities, effects, cues, tasks, and target
  actors
- material and graph lanes emit asset-side `.uasset`/`.cpp` support and update
  the `AssetRegistry.bin`

The current implementation still has typed-program compatibility helpers inside
the packager, but the normal documentation path should describe the
monomorphized generation flow as the primary lane.

## Lane Coverage

The current UE5 surface covers these authoring lanes:

- shaders
- materials
- graphs
- Blueprint/actor conversion
- GAS items
- Slate/editor surfaces
- data assets
- runtime modules
- editor modules

## First Files To Read In The Repo

- `src/rust-import/cli/packager/ue5_pipeline.kn`
- `src/rust-import/ue5/codegen_ue5.kn`
- `src/rust-import/ue5/ue5/project.kn`
- `src/rust-import/ue5/ue5/module_graph.kn`
- `src/rust-import/ue5/ue5/validation_rules.kn`
- `src/rust-import/ue5/ue5/engine_knowledge.kn`
- `src/rust-import/ue5/ue5/oracle.kn`
- `unreal_plugins/OfficialDocs/01-Getting-Started.md`

## Practical Rule

If you are documenting UE5 behavior, separate these three layers clearly:

- Kain language meaning
- UE5 validation and codegen
- generated Unreal artifacts
