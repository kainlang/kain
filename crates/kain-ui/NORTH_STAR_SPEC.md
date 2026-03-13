# KAIN UI North Star Spec

## Purpose

`kain-ui` is the runtime and semantic execution layer for KAIN interfaces.
It exists to make UI a first-class software system for tools, desktop
applications, editors, and web surfaces without inheriting the browser-first
tradeoffs that shaped React-era frameworks.

This crate is the beginning of that system.

## Endgame

The endgame is not "a better React."

The endgame is:

- A native-first UI runtime
- A renderer-agnostic semantic interface graph
- Fine-grained reactive invalidation instead of broad rerender passes
- Deterministic patch streams instead of virtual DOM diffing as the center
- First-class support for software tooling UIs, not only CRUD apps
- Web as an adapter target, not the canonical runtime truth

KAIN UI should feel like writing software topology, not negotiating with a
framework.

## Core Thesis

UI in KAIN should compile into:

1. Typed semantic UI IR
2. Retained interface graph
3. Reactive dependency graph
4. Scheduler
5. Minimal backend patch stream

Not:

1. Re-run component function
2. Build ephemeral tree
3. Diff tree
4. Mutate host

## Design Principles

1. Native-first

The model must be strongest on native desktop and editor surfaces. Web support
matters, but web constraints do not get to define the architecture.

2. Semantic-first

The core runtime should understand semantic widget intent like panel, inspector,
graph, timeline, viewport, overlay, table, and tree before lowering into any
host API.

3. Reactive-first

State changes should invalidate exact dependencies. The system should know what
changed, who depends on it, and what host nodes need patches.

4. Retained-tree-first

The runtime owns a persistent interface graph. UI nodes live long enough to
carry identity, focus, animation state, layout state, and tool interaction
context.

5. Patch-first rendering

Backends should consume explicit `UiPatch` streams. This allows native, web,
Slate, and future renderers to share one semantic model while producing host-
specific updates.

6. Tooling-first

KAIN UI must excel at:

- Dockable panels
- Inspectors
- Outliners
- Graph editors
- Timelines
- Property grids
- Asset browsers
- Viewport overlays
- Command palettes
- Data-heavy professional tools

7. Explainability

The UI runtime should be inspectable by default:

- What signal changed?
- Which nodes invalidated?
- Why did layout rerun?
- Which patch was emitted?
- Which renderer capability blocked a feature?

8. Data-driven capabilities

Renderer capabilities, widget metadata, event mappings, style tokens, and
layout support should be represented as typed data tables instead of scattered
string switches and ad hoc feature flags.

## What We Refuse To Inherit

KAIN UI should explicitly avoid these framework traps:

- Virtual DOM as the center of truth
- Broad subtree rerenders as the default update path
- Browser APIs leaking into the semantic model
- Hook-only state semantics
- "Everything is a div" as the authoring baseline
- A plugin soup for core concerns like async, forms, tables, graphs, and motion
- Renderer-specific semantics baked into authoring

## Runtime Model

The runtime is expected to revolve around these concepts:

- `UiNodeId`
- `UiSignalId`
- `UiNode`
- `UiTree`
- `UiPatch`
- `UiBackendCapabilities`
- `UiLayoutSpec`
- `UiStyleSpec`

Planned additions:

- `UiComputed`
- `UiResource`
- `UiTransaction`
- `UiFocusGraph`
- `UiEventRoute`
- `UiAnimationTrack`
- `UiSurface`
- `UiScheduler`
- `UiSelectionModel`
- `UiCommandBuffer`

## Authoring Model

KAIN UI should support multiple authoring modes that compose cleanly:

1. Declarative components

Best for most interface structure and composition.

2. Reactive state and derived values

Best for local interactivity, tool state, and high-signal updates.

3. Schema-driven UI

Best for inspectors, forms, property grids, menus, and metadata-driven tools.

4. Imperative transactions

Best for drag/drop, graph editing, docking, viewport tools, and command-heavy
 editor interactions.

The winning system will not be one paradigm. It will be a coherent blend.

## Renderers

The renderer split should eventually look like this:

- `kain-ui`
  - semantic graph
  - reactive model
  - scheduler
  - patch generation
- `kain-ui-native`
  - native desktop backend
  - debug host window
  - real widget/view integration
- `kain-ui-web`
  - DOM/canvas/webgpu lowering
  - accessibility tree mapping
  - browser event mapping
- `kain-ui-slate` or integration path through UE editor crates
  - editor-facing backend
  - docking and tool integration

## Integration With Existing KAIN Crates

`kain-core`

- Owns syntax, AST, parsing, typing, and lowering hooks for UI authoring.
- Should no longer own the long-term UI runtime.

`web`

- Should eventually consume semantic UI IR and patches for web targets.

`ue5-editor`

- Should eventually consume semantic UI IR and backend mappings for editor
  surfaces, not hand-roll every UI path in isolation.

`kain-selfhost`

- Should mirror the semantic UI runtime progressively after the Rust bootstrap
  path is stable enough to trust.

## First Milestone

The first serious milestone for KAIN UI should be:

1. Execute component calls against real component definitions
2. Build a retained semantic node graph
3. Add reactive signals and dependency tracking
4. Emit patch streams for tree and prop updates
5. Stand up one native debug renderer
6. Prove a small tool shell:
   - docked panel layout
   - inspector
   - tree view
   - graph canvas placeholder

If that works, KAIN UI becomes real.

## Second Milestone

1. Add web backend adapter
2. Add schema-driven inspector/property grid generation
3. Add animation and transition scheduling
4. Add command routing, focus graph, and keyboard shortcuts
5. Add introspection/devtools for signals, patches, and layout invalidation

## Long-Term Vision

KAIN UI should become the interface substrate for:

- KAIN's own tooling
- desktop applications
- DCC/editor software
- graph authoring tools
- web applications
- embedded editor panels in larger runtimes

If successful, KAIN UI will make interface code feel:

- lower-friction than React
- more direct than Flutter
- more inspectable than most modern UI stacks
- more suitable for software tooling than web-first frameworks

That is the bar.
