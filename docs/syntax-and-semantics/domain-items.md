# Domain Items

Snapshot: April 12, 2026.

This page covers the higher-level language items that are specific to Kain's
UI, graphics, UE5, and editor surfaces.

## UI And Components

Component definitions own:

- props
- state
- methods
- effects
- a JSX body

The methods are normal `Function` items. The component item is what tells the
runtime and UI renderer how those methods, props, and state fields belong
together.

`JSXNode` forms include:

- elements
- component calls
- expressions
- text
- loops
- conditionals
- fragments

## Shaders And Graphics

Shader items can target:

- vertex
- fragment
- compute
- surface

Shaders own inputs, outputs, uniforms, and executable bodies. They also cooperate
with `comptime` when the compiler needs to extract compute metadata before
lowering.

Compute shaders may declare static local geometry in the header:

```kn
shader compute ParticleKernel(id: UVec3) -> Vec4 workgroup(8, 1, 1):
    uniform particles: StorageBuffer<Vec4> @0
    let p = particles[id.x]
    return vec4(p.x, p.y, p.z, 1.0)
```

`workgroup(x, y, z)` is canonical shader-header truth for local size. It is only
legal on compute shaders, must be compile-time positive integer geometry, and is
kept static. Explicit compute metadata can still describe dispatch defaults and
sidecar plans; if it repeats workgroup geometry, it must match the header.

Host code launches compute work through a backend-agnostic dispatch statement:

```kn
dispatch "semantic.score" [1024, 1, 1]
```

Those dimensions are dynamic grid dimensions for that launch. They override the
artifact or compute-metadata `dispatch_size` default, while `workgroup(...)`
remains the static local geometry baked into shader artifacts.

## Material And Graph Items

Material and graph-related declarations include:

- `@material_graph`
- `@material_function`
- `@graph_editor`
- `@graph_runtime`

The important schemas are:

- `MaterialFunctionDef`, which owns inputs, a material statement body, and a
  single output expression
- `GraphEditorDef`, which owns node types and an optional validation schema
- `NodeTypeDef`, which owns input pins, output pins, properties, and attributes
- `GraphRuntimeDef`, which owns graph data, node definitions, instance state,
  and pin configuration
- `GraphDataDef`, `NodeDataDef`, `GraphInstanceDef`, and `PinConfigDef`, which
  carry the runtime graph classes and callback surfaces

These items feed the UE5/material and node-graph tooling lanes. The generated
artifacts are meant to be consumed by Unreal-facing tooling, not by a separate
generic graph runtime.

## State And Async Domain Items

State and async domain items include:

- `@state_machine`
- `@async_task`
- `@editor_module`

`AsyncTask` definitions carry lifecycle methods such as activate and destroy
hooks, plus custom methods. `StateMachine` items model stateful transitions and
their associated methods. `EditorModule` items wrap editor integration
surfaces.

## Editor Integration

`EditorModuleDef` owns:

- menu entries
- toolbar buttons
- toolbar widgets
- callback methods

Menu entries and toolbar buttons are function-backed. Toolbar widgets define a
widget placement and widget type rather than a callback.

## Gameplay System Items

Gameplay-system declarations include:

- `@gameplay_tags`
- `@ability`
- `@gameplay_effect`
- `@gameplay_cue`
- `@ability_task`
- `@target_actor`

These item kinds mirror UE5 gameplay-system concepts and are documented in the
UE5-oriented guide set and plugin examples. Gameplay abilities expose methods,
gameplay cues expose lifecycle callbacks, ability tasks expose activation and
destroy callbacks, and target actors expose filtering or custom behavior
methods.

## Broad Rule

These domain items are not secondary syntax. They are first-class declarations
that the compiler lowers into runtime and toolchain artifacts.

If you are documenting one of these families, cross-link to
[guides/ue5/overview.md](/home/ephemara/Dev/Kain/guides/ue5/overview.md) and
the appropriate example lane so readers can see the generated output shape.

## Source Files To Consult

- `crates/core/src/ast.rs`
- `crates/core/src/runtime_contract.rs`
- `crates/core/src/realtime_app_bundle.rs`
- `crates/core/src/ui.rs`
