# Atlas Target Architecture

- Goal: Keep compiler-owned UI semantics authoritative while making the runtime and backend boundaries strong enough for native, web, and future Slate/UE adapters.

## Non-Negotiable Rules

- `kain-core` defines authored UI meaning.
- `kain-ui` defines runtime graph behavior and patch planning.
- Backends consume semantic contracts. They do not create product meaning.
- Debug/devtools surfaces are opt-in and contractually separate from product UI.
- Native convenience projections are compatibility artifacts, not the long-term ABI.

## Target Subsystem Boundaries

| Subsystem | Target owner | Must own | Must not own |
| --- | --- | --- | --- |
| Authoring contracts | `kain-core` | UI syntax, typed semantic IR, widget schema ids, state/command/event/focus/selection/paint/motion authoring contracts, emitted bundle truth | Host widget behavior, egui/Slate specifics, runtime inspector UI, bundle-time heuristic synthesis |
| Semantic IR emission | `kain-core` | Fully typed UI bundle sections for runtime graph, widget registry references, paint primitives, motion tracks, event routes, command surfaces, workspace persistence ids, fallback requirements | Native projection DTOs as the source of truth |
| Runtime graph | `kain-ui` | Retained nodes, signal values, computed invalidation, transactions, focus graph, selection model, scheduler, hot reload transfer, workspace layout state | Parsing JSX, interpreting components, backend-specific chrome decisions |
| Patch planner | `kain-ui` | Backend-agnostic `UiPatch` generation, runtime mutation semantics, fallback negotiation entry points | Direct egui/DOM/Slate rendering |
| Widget registry schema | `kain-core` contract plus backend tables | Semantic widget families, property/event schema, capability ids, backend-neutral fallback categories | Backend-only widget meaning hidden behind generic tags |
| Paint and motion system | `kain-core` contract; `kain-ui` runtime playback state | Semantic paints, layers, gradients, masks, blur, transitions, authored animation intent, runtime playback state | Renderer-local one-off paint hacks as the only implementation path |
| Backend capability tables | Shared schema in `kain-ui`; backend-provided data in adapters | Capability ids, required/optional fallback declarations, explicit unsupported-state reporting | Silent degradation or renderer-private feature flags |
| Native adapter | `kain-ui-native` | Translation from semantic nodes/patches/surfaces into egui/wgpu/native windowing, capability declarations, opt-in devtools host | Default app chrome, runtime snapshot-driven product framing, semantic truth |
| Slate / UE adapter | future `kain-ui-slate` plus `crates/ue5` metadata | Translation from semantic bundles into Slate widget trees, docking integration, editor event mapping, UE-specific capability declarations | Authoring truth, runtime semantics invented in UE-only metadata |
| Devtools | Separate opt-in contract consumed by adapters | Semantic tree inspector, patch log viewer, runtime diagnostics, backend capability report | Default product shell framing |

## Recommended Dependency Direction

`kain-core -> emitted semantic UI bundle -> kain-ui runtime graph -> backend adapters`

Supporting lanes:

- `kain-driver` packages emitted bundles and sidecars.
- `crates/ue5` supplies extracted backend metadata for the Slate/UE adapter.
- Devtools read runtime state through explicit devtools contracts, not product-shell fields.

## Target Runtime Boundary

### `kain-core`

Should emit:

- semantic widget identities
- typed event routes
- typed command definitions
- focus and selection scope declarations
- workspace layout identities and docking intent
- paint primitives and motion tracks
- schema-driven inspector/table/menu/property-grid contracts
- backend-neutral capability requirements

Should stop owning:

- `VNode`
- backend profile tables
- runtime component instance snapshots
- stringified event placeholders like `[event:click]`

### `kain-ui`

Should consume emitted semantic truth and own:

- runtime graph state
- invalidation and scheduler behavior
- transaction logs
- workspace layout snapshot/apply rules
- patch planning
- hot reload state transfer
- fallback resolution against backend capability tables

Compatibility inference can remain temporarily, but only as a migration path behind explicit legacy markers.

### `kain-ui-native`

Should become:

- an adapter and renderer
- a capability publisher
- an opt-in devtools host when explicitly enabled

It should not:

- show a topbar by default unless authored
- emit root/component labels into product shells
- derive app identity from a runtime snapshot sidecar
- switch semantic widget behavior based on hidden host mode

### Future `kain-ui-slate`

Should consume the same runtime bundle and patch/event contracts as native.

`crates/ue5` should remain a backend data source for:

- Slate widget properties
- delegate signatures
- slot/composition rules
- editor attribute lowering metadata

It should not define what a Kain inspector, command palette, or dock transaction means.

## Devtools Split

Target split:

- Product bundle: semantic UI, runtime systems, backend capability requirements.
- Devtools bundle or mode: semantic tree view, patch log, runtime diagnostics, backend capability report, artifact watch status.

The runtime may expose both, but adapters must not merge them by default.

## Migration-Safe Compatibility Layers

These can survive temporarily:

- `UiNativeProjection` as a compatibility projection for current native host consumers.
- `ui_runtime_systems_from_tree(...)` as a legacy synthesis pass for old authored trees.
- runtime snapshot sidecars only for opt-in devtools mode.

These should not define the final architecture.

## Explicit Anti-Goals

- Do not move more UI meaning into `kain-ui-native` because it is the easiest place to ship visuals.
- Do not let `UiNativeProjection` become the canonical cross-backend ABI.
- Do not encode Slate-only widget semantics in compiler authoring.
- Do not keep stringified event placeholders as the event contract.
- Do not keep driver-generated runtime snapshot chrome as default app identity.
