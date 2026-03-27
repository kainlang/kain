# Atlas Target Architecture

- Goal: keep compiler-owned UI semantics authoritative while making runtime and backend boundaries strong enough for native, web, and future Slate/UE adapters.

This doc is the boundary contract that Forge, Vector, Delta, and future Slate/UE work must follow.

## Non-Negotiable Rules

- `kain-core` defines authored UI meaning and emits backend-neutral semantic truth.
- `kain-ui` defines runtime graph behavior, invalidation, scheduling, and patch planning.
- Backends consume semantic contracts. They do not create product meaning, posture, or chrome.
- Debug/devtools surfaces are opt-in and contractually separate from product UI.
- Native convenience projections are compatibility artifacts, not the long-term ABI.

## Choke Point Fixes (What Must Change Architecturally)

These are the minimum architectural corrections required to meet the acceptance bar.

1. Stop lowering events to placeholder strings.
   - Replace `"[event:click]"` props with typed event routes that include handler identity and (when applicable) transaction/command linkage.
2. Stop flattening component state into props.
   - Replace `state.<name>` props with compiler-emitted signal/state declarations plus runtime-owned state storage.
3. Stop letting `kain-ui` infer primary runtime semantics from tree shape.
   - `ui_runtime_systems_from_tree` becomes compatibility-only; new authored UI must emit runtime systems explicitly.
4. Isolate `UiNativeProjection` so it cannot become the cross-backend ABI.
   - Keep it for current native/C consumers, but treat it as a native adapter sidecar or explicit compatibility section, not the semantic contract.
5. Keep `RealtimeAppBundle` focused on realtime render surfaces, but make it consume compiler-emitted surface truth.
   - Surface identity and capabilities must originate from compiler/runtime contracts, not prop scanning and heuristic inference.

## Target Subsystem Boundaries

| Subsystem | Target owner | Must own | Must not own |
| --- | --- | --- | --- |
| Authoring syntax + typing | `kain-core` | UI syntax, parsing/typing, schema ids, typed semantic IR | Any host widget behavior, egui/Slate specifics |
| UI semantic IR emission | `kain-core` | Emitted UI bundle truth: widget identities, typed props, event routes, command surfaces/defs, focus/selection scopes, workspace layout intent, paint/motion intent, schema-driven widget contracts, capability requirements | Native-only DTO projections used as "the truth" |
| Runtime graph | `kain-ui` | Retained nodes, stable node identity, state/signal storage, computed invalidation, transaction log, focus graph, selection model, scheduler, workspace layout state, hot reload transfer, fallback resolution | JSX execution, component interpretation, backend-specific chrome decisions |
| Patch planner | `kain-ui` | Backend-agnostic `UiPatch` generation from runtime mutations and scheduler phases | Direct egui/DOM/Slate rendering |
| Backend capability model | Shared schema in `kain-ui`; backend-provided data in adapters | Capability ids, fallback categories/policies, explicit unsupported-state reporting | Silent degradation, backend-private feature flags with no declared contract |
| Paint + motion | `kain-core` contract; `kain-ui` runtime playback | Semantic paint primitives and motion intent; runtime playback state and scheduling | Backend-local rendering hacks as the only way to express authored visuals |
| Realtime surfaces | `kain-ui` runtime (surface registry) + `kain-core` contract (surface authoring) | Surface ids, kinds, shader bindings, composition intent, GPU backing requirements | Treating viewports as "just a tag" with semantics invented in the host |
| Native adapter | `kain-ui-native` | Translation from retained nodes/patches/surfaces into egui/wgpu/native windowing; capability publication; opt-in devtools host | Default topbar/inspector injection, product posture decisions, runtime snapshot driven app identity, semantics invented behind `product_shell` switches |
| Slate/UE adapter | future `kain-ui-slate` plus `crates/ue5` metadata | Translation from semantic bundles into Slate widget trees; docking integration; editor event mapping; UE-specific capability declarations and mapping tables | Authoring truth, runtime semantics invented by UE metadata |
| Packaging | `kain-driver` | Packaging emitted bundles/sidecars and manifest wiring | Driver-generated product chrome or default commands presented as authored UI |
| Devtools | Separate opt-in contract consumed by adapters | Semantic tree inspector, patch log, runtime diagnostics, capability reports | Default product shell framing |

## Recommended Dependency Direction

`kain-core (authoring + emission) -> kain-ui (runtime graph + patches) -> backend adapters (native/web/slate)`

Supporting lanes:

- `kain-driver` packages emitted bundles and declared sidecars.
- `crates/ue5` supplies extracted backend metadata (properties/events/slots) for Slate mapping.
- Devtools read runtime state via explicit devtools contracts, not by smuggling product-shell fields into runtime sidecars.

## Target Bundle Boundaries (Do Not Mix These)

### `UiRuntimeBundle` (backend-neutral)

Owned by: `kain-ui` (schema), emitted by `kain-core`, executed by `kain-ui`, consumed by adapters.

Must contain:

- retained semantic tree (or a stable runtime graph representation)
- compiler-emitted runtime systems (events, commands, focus/selection scopes, workspace layout intent, paint/motion intent, schema widgets)
- backend capability requirements and explicit fallback expectations

Must not contain:

- native-only projections that other backends will be forced to follow
- driver-generated product chrome metadata

### `UiNativeProjection` (compatibility-only)

Owned by: native adapter compatibility layer.

Facts on the ground:

- It is already treated as stable for non-Rust consumers (parity fixture guards its serialized tags).

Target:

- keep it available for current native/C loaders
- isolate it so it cannot become the semantic contract for Slate/UE or web

### `RealtimeAppBundle` (realtime render surface bundle)

Owned by: `kain-core` schema today.

Target:

- remain focused on scenes/materials/shader-canvas bindings/assets/requirements
- consume compiler/runtime-emitted surface truth (stable `surface_id`, surface kind, shader binding) rather than heuristic or prop-based discovery as the primary source of identity

### Native runtime snapshot sidecar (devtools only)

Target:

- if it exists, it is opt-in devtools metadata only
- it must not define product mode posture, default topbar commands, or app identity

## Adapter Boundary For Future `kain-ui-slate`

`kain-ui-slate` (future crate) must:

- consume the same `UiRuntimeBundle` + `UiPatch` streams as native
- use `crates/ue5` registries for mapping Slate widget classes/properties/events/slots
- publish backend capabilities through the shared capability table schema

`kain-ui-slate` must not:

- consume `UiNativeProjection` as if it were the semantic IR
- invent a parallel semantic model in UE land

## Migration-Safe Compatibility Layers

These can exist temporarily, but must be visibly marked as compatibility-only in code and docs:

- `UiNativeProjection`
- `ui_runtime_systems_from_tree(...)`
- driver-emitted runtime snapshot chrome (only when devtools mode is enabled)

## Explicit Anti-Goals

- Do not move more UI meaning into `kain-ui-native` because it is the easiest place to ship visuals.
- Do not let `UiNativeProjection` become the canonical cross-backend ABI.
- Do not keep stringified event placeholders as the event contract.
- Do not keep driver-generated runtime snapshot chrome as default app identity or product posture.
- Do not begin Slate/UE work by reverse engineering native host behavior.
