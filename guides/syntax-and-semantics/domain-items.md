# Domain Items

This page covers the higher-level language items that are specific to Kain's
UI, graphics, UE5, and editor surfaces.

## UI And Components

- `component` definitions own props, state, methods, effects, and JSX bodies.
- `JSXNode` forms include elements, component calls, expressions, text,
  loops, conditionals, and fragments.

## Shaders And Graphics

- `shader` items can target vertex, fragment, compute, or surface stages.
- shaders own inputs, outputs, uniforms, and executable bodies.
- explicit compute metadata is extracted from `comptime` blocks.

## Material And Graph Items

- `@material_graph`
- `@material_function`
- `@graph_editor`
- `@graph_runtime`

These items feed the UE5/material and node-graph tooling lanes.

## State And Async Domain Items

- `@state_machine`
- `@async_task`
- `@editor_module`

## Gameplay System Items

- `@gameplay_tags`
- `@ability`
- `@gameplay_effect`
- `@gameplay_cue`
- `@ability_task`
- `@target_actor`

These item kinds mirror UE5 gameplay-system concepts and are documented in the
UE5-oriented guide set and plugin examples.

## Broad Rule

These domain items are not secondary syntax. They are first-class declarations
that the compiler lowers into runtime and toolchain artifacts.
