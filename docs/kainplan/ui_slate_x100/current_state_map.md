# Atlas Current State Map

- Lane: `Atlas`
- Date: `2026-03-27`
- Scope: Current ownership, leak points, and migration pressure for the UI Slate X100 overhaul.

## Inspected Repo Evidence

- `crates/kain-core/src/ui.rs` (JSX evaluation, `VNode` runtime tree, lowering into `kain-ui`)
- `crates/kain-core/src/realtime_app_bundle.rs` (`RealtimeAppBundle` schema and UI surface extraction)
- `crates/kain-core/src/runtime_contract.rs` (`RuntimeContractBundle` schema, capability signaling)
- `crates/kain-ui/src/lib.rs` (`UiRuntimeSystems`, `ui_runtime_systems_from_tree`, `UiRuntimeBundle`, `UiNativeProjection`)
- `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs` (`UiNativeProjection` is treated as a stable ABI for non-Rust consumers)
- `crates/kain-ui/NORTH_STAR_SPEC.md` (intended ownership)
- `crates/kain-driver/src/native_app.rs` (`build_native_app_runtime_snapshot` sidecar injection)
- `crates/kain-ui-native/src/lib.rs` (host chrome defaults, `product_shell` gating, devtools posture)
- `crates/ue5/src/ue5/context.rs` and `crates/ue5/src/ue5/widget_registry.rs` (data-driven Slate/UE metadata registries)

## Known Choke Points (Must Be Designed Around)

These are the specific places where the current system cannot scale to Slate-class tooling surfaces without lying about ownership.

1. `crates/kain-core/src/ui.rs` lowering gaps: compiler emits a `UiTree`, but not the richer runtime semantics. Most of `output.systems` is empty, so downstream has to synthesize behavior.
2. Event lowering collapses to placeholder strings: event attributes become `UiValue::String("[event:click]")`, not first-class event routes.
3. `kain-ui` runtime systems are still heuristic for primary behavior: `ui_runtime_systems_from_tree` backfills docking, surfaces, animations, and scopes; commands/resources/event routes are mostly absent or too generic.
4. `RealtimeAppBundle` schema narrowness: it is focused on scenes/shaders/surfaces, and it relies on `output.systems.surfaces` plus prop-based discovery, so UI evolution is bottlenecked by inferred surface truth.
5. `UiNativeProjection` flattening: a host-friendly, lossy projection is embedded in the shared bundle and is treated as stable (including tests guarding its serialized tags), risking accidental promotion to canonical ABI.

## Current Ownership Snapshot

| Concern | Current owner | Evidence | Current issue |
| --- | --- | --- | --- |
| UI authoring parse + interpreter execution | `kain-core` | `build_ui_output_from_program`, `eval_jsx`, `render_component_definition` in `crates/kain-core/src/ui.rs` | Compiler crate still owns runtime execution mechanics, component instance snapshots, VDOM reconciliation, and backend profile tables. |
| Compiler lowering into `kain-ui` | `kain-core` | `lower_vnode_to_ui_tree`, `lower_vnode_into_tree` in `crates/kain-core/src/ui.rs` | Lowering emits mostly a tree and props. Events and state are flattened into strings and prop keys, not typed runtime contracts. |
| Semantic node graph and patch ABI | `kain-ui` | `UiNode`, `UiTree`, `UiPatch`, `UiBuildOutput` in `crates/kain-ui/src/lib.rs` | Correct long-term owner, but the compiler is not yet emitting enough truth for the runtime to be authoritative without inference. |
| Runtime systems synthesis | `kain-ui` | `ui_runtime_systems_from_tree` in `crates/kain-ui/src/lib.rs` | Many semantics are guessed from tree shape. Some systems exist only as shallow structs (`UiEventRoute`, `UiCommandBuffer`) with no meaningful contract. |
| Shared UI runtime bundle ABI | `kain-ui` plus `kain-driver` packaging | `UiRuntimeBundle`, `ui_runtime_bundle_from_output` in `crates/kain-ui/src/lib.rs`; native app bundling in `crates/kain-driver/src/native_app.rs` | Bundle exists, but defaults to legacy synthesis when compiler did not emit systems. It also embeds `UiNativeProjection` which is a native convenience ABI. |
| Realtime app bundle ABI | `kain-core` | `RealtimeAppBundle` in `crates/kain-core/src/realtime_app_bundle.rs` | UI-facing viewports and shader-canvas bindings are derived from `output.systems.surfaces` and node props, so schema evolution is tightly coupled to inference. |
| Native app materialization sidecars | `kain-driver` | `build_native_app_runtime_snapshot` in `crates/kain-driver/src/native_app.rs` | Driver emits a runtime snapshot that looks like product-shell truth (panels, commands, providers, sessions), which backends then treat as meaning. |
| Native backend realization | `kain-ui-native` | `show_runtime_topbar`, `show_runtime_inspector`, `render_node` in `crates/kain-ui-native/src/lib.rs` | Backend injects default chrome and devtools posture; widget behavior branches on `product_shell`, meaning the backend changes semantics instead of only realizing them. |
| UE editor / Slate metadata | `crates/ue5` | `WidgetRegistry`, `Ue5Context` in `crates/ue5/src/ue5/*` | Solid data-driven metadata, but it is not consuming Kain UI bundles/patches yet. Today it is parallel knowledge, not an adapter. |

## Actual Data Flow Today

Primary path:

`Kain source -> kain-core JSX/interpreter -> lower into kain-ui UiTree -> (if systems empty) kain-ui infers UiRuntimeSystems -> kain-driver packages native bundles/sidecars -> kain-ui-native egui/wgpu host`

Parallel UE metadata path:

`Kain source -> crates/ue5 extracted registries + codegen -> UE-specific widget/editor knowledge`

Those lanes are adjacent, not unified.

## Current Leak Map (What Must Be Cut)

### 1. Compiler crate still owns runtime-shaped UI behavior

In `crates/kain-core/src/ui.rs`:

- `UIBackendKind` and `UIBackendProfile` live in the compiler crate and include a `Slate` profile. This couples compiler lowering to backend expectations.
- `VNode` and `ComponentInstance` exist as a runtime execution tree with reconciliation logic, which is not a durable semantic IR boundary.
- Component state is flattened into node props via keys like `state.<name>` when lowering `VNode::Component`.

### 2. Event lowering collapses to placeholder strings (not event routes)

In `crates/kain-core/src/ui.rs`:

- `apply_attrs_to_ui_props` lowers `UIAttr::Event` to `UiValue::String(format!("[event:{}]", ...))`.
- `render_attr_to_string` produces `name="[event:click]"` strings.

This is not an event contract. It cannot support typed payloads, bubbling/capture, command dispatch, or transactions without backend inference.

### 3. `kain-ui` infers runtime truth when the compiler emitted none

In `crates/kain-ui/src/lib.rs`:

- `UiRuntimeBundle` calls `ui_runtime_systems_from_tree` when `output.systems.is_empty()`.
- `ui_runtime_systems_from_tree` populates docking roots, scheduler entries, focus/selection scope sets, surface descriptors, and even animation tracks by scanning nodes.
- `UiEventRoute` and `UiCommandBuffer` exist, but `ui_runtime_systems_from_tree` does not construct meaningful event route or command surface truth (it cannot, because the compiler never emitted it).

This compatibility path is useful, but it is the exact place where Slate-class semantics will silently become "whatever the host guessed."

### 4. `UiNativeProjection` is a lossy convenience ABI that is treated as stable

In `crates/kain-ui/src/lib.rs` and `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs`:

- `UiNativeProjection` is explicitly described as a "Flat raw-native projection" for runtimes not consuming the retained tree.
- The parity fixture test guards its serialized enum tags because "the native C loader depends on" them.

That means this is already a cross-language ABI. If we keep embedding it in the shared runtime bundle, it will accidentally become the canonical contract for Slate/UE and web, which is the wrong architecture.

### 5. `RealtimeAppBundle` depends on inferred surfaces and prop discovery

In `crates/kain-core/src/realtime_app_bundle.rs`:

- `emit_realtime_app_bundle` calls `collect_shader_canvas_bindings` and `collect_scene_bindings`.
- Those collectors iterate `output.systems.surfaces` and then inspect node props like `scene`, `material`, and shader binding keys.

This creates a bottleneck: richer UI semantics (commands, transactions, schemas) are not represented here at all, and even viewport truth depends on whether surfaces were inferred correctly upstream.

### 6. Native host posture is still debug-chrome by default

In `crates/kain-ui-native/src/lib.rs`:

- `show_runtime_topbar` defaults to visible (`unwrap_or(true)`).
- `show_runtime_inspector` can show a runtime inspector inside the app shell.
- The host emits host-originated strings like `native agent desktop`.
- Widget rendering behavior branches on `product_shell` (example: `Inspector` and `Tree` use `CollapsingHeader` vs normal panel behavior).

This is backend-owned posture and meaning, not adapter-only realization.

### 7. Driver emits runtime snapshot chrome as if it were product-shell truth

In `crates/kain-driver/src/native_app.rs`:

- `build_native_app_runtime_snapshot` hardcodes a `runtime_surface` panel, a `Reload Runtime` command, a `native_runtime` provider, and synthetic session/workspace metadata.

`kain-ui-native` then reads this snapshot to drive topbar/chrome behavior, which makes "product mode owns the screen" impossible without first cutting this leak.

### 8. UE/Slate readiness exists only as metadata, not as a semantic adapter

In `crates/ue5/src/ue5/widget_registry.rs` and `crates/ue5/src/ue5/context.rs`:

- The UE registries are already data-driven, including Slate widget classes, properties, events (delegate types), and composition rules.

What is missing is an adapter layer that consumes `UiRuntimeBundle` and `UiPatch` and maps them to Slate/UE. Today the semantic model and the UE metadata are not connected.

## Atlas Conclusions

- `kain-ui` is the correct long-term home for runtime graph ownership and patch planning, but it can only be authoritative once the compiler emits real runtime semantics (events, commands, transactions, schema widgets).
- `kain-core` must shed backend profiles, placeholder event lowering, and prop-flattened state, and move toward typed semantic IR emission.
- `UiNativeProjection` must be treated as compatibility-only and progressively isolated so it cannot become the cross-backend ABI.
- `RealtimeAppBundle` should remain focused on realtime scenes/surfaces, but it must consume compiler-emitted surface truth rather than inference and prop scanning as the primary source of reality.
- `kain-ui-native` must stop injecting default chrome/devtools posture; `kain-driver` must stop emitting default runtime snapshot chrome that backends treat as product meaning.
- `crates/ue5` should remain a data-driven backend metadata source until a real `kain-ui-slate` adapter consumes Kain semantic UI bundles directly.
