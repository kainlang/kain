# Atlas Current State Map

- Lane: `Atlas`
- Date: `2026-03-27`
- Scope: Current ownership, leak points, and migration pressure for the UI Slate X100 overhaul.

## Inspected Repo Evidence

- `crates/kain-core/src/ui.rs`
- `crates/kain-ui/src/lib.rs`
- `crates/kain-ui/NORTH_STAR_SPEC.md`
- `crates/kain-ui-native/src/lib.rs`
- `crates/kain-driver/src/native_app.rs`
- `crates/ue5/src/ue5/context.rs`
- `crates/ue5/src/ue5/widget_registry.rs`

## Current Ownership Snapshot

| Concern | Current owner | Evidence | Current issue |
| --- | --- | --- | --- |
| UI authoring parse + interpreter execution | `kain-core` | `build_ui_output_from_source`, `eval_jsx`, `render_component_definition` in `crates/kain-core/src/ui.rs` | Compiler crate still owns runtime execution mechanics, component instance state, and backend profile tables. |
| Semantic node graph and patch ABI | `kain-ui` | `UiNode`, `UiTree`, `UiPatch`, `UiBuildOutput` in `crates/kain-ui/src/lib.rs` | Good ownership direction, but many runtime systems are still inferred heuristically from the tree instead of emitted as first-class truth. |
| Runtime systems synthesis | `kain-ui` | `ui_runtime_systems_from_tree` in `crates/kain-ui/src/lib.rs` | Computed nodes, scheduler entries, workspace layout, surfaces, and theme scaffolding are auto-derived from tree shape, not authored semantic contracts. |
| Compiler/runtime UI bundle boundary | `kain-ui` plus `kain-driver` packaging | `UiRuntimeBundle`, `ui_runtime_bundle_from_output` in `crates/kain-ui/src/lib.rs`; `compile_native_app_bundle` in `crates/kain-driver/src/native_app.rs` | Bundle exists, but it still embeds a native-oriented projection and tolerates missing runtime systems by synthesizing them. |
| Native app materialization | `kain-driver` | `compile_native_app_bundle`, `materialize_native_app_bundle` in `crates/kain-driver/src/native_app.rs` | Packaging is correct ownership, but it also emits host-facing runtime snapshot content that bleeds product chrome and devtools assumptions into normal apps. |
| Native backend realization | `kain-ui-native` | `run_output`, `KainUiNativeApp`, `render_node` in `crates/kain-ui-native/src/lib.rs` | Backend realizes the semantic tree, but it also injects topbar, inspector, agent-desktop framing, and fallback widget behavior that changes app posture. |
| UE editor / Slate metadata | `crates/ue5` | `Ue5Context`, `WidgetRegistry` in `crates/ue5/src/ue5/context.rs` and `widget_registry.rs` | Data-driven UE metadata exists, but it is not consuming the Kain semantic UI bundle. It is a separate metadata lane, not yet an adapter. |

## Actual Data Flow Today

`Kain source -> kain-core JSX/interpreter lowering -> kain-ui retained tree + inferred runtime systems -> kain-driver native bundle/materialization -> kain-ui-native egui/wgpu host`

Parallel to that:

`Kain source -> crates/ue5 metadata/codegen lane -> UE-specific widget/editor knowledge`

Those lanes are adjacent, not unified.

## Current Leak Map

### 1. Compiler crate still owns runtime-shaped UI behavior

- `crates/kain-core/src/ui.rs` defines `UIBackendKind`, `UIBackendProfile`, `UIEvent`, `ComponentInstance`, and `VNode`.
- `kain-core` executes component calls and state declarations before lowering into `kain-ui`.
- Event handlers are currently collapsed into string props like `[event:click]` instead of first-class event routes.
- Component state is flattened into node props like `state.foo` instead of a dedicated runtime state contract.

This means the compiler boundary is not yet “typed semantic IR only.” It still carries runtime-host baggage.

### 2. `kain-ui` owns the right runtime concepts, but many are scaffolded by inference

- `UiRuntimeSystems` already contains the correct categories: computed values, transactions, focus graph, event routes, animation tracks, surfaces, scheduler, selection model, command buffer, theme registry, workspace layout, and hot reload state.
- When `output.systems` is empty, `ui_runtime_bundle_from_output` calls `ui_runtime_systems_from_tree`.
- That synthesis pass infers docking roots, pending scheduler work, animation tracks, theme scopes, and surface descriptors from existing nodes.

This is useful as a compatibility bridge, but it is not a trustworthy long-term semantic contract for Slate-class tooling.

### 3. The runtime bundle still carries a native-host convenience ABI

- `UiRuntimeBundle` contains both `output` and `native_projection`.
- `ui_native_projection_from_output` converts the retained tree into a flattened host-friendly projection with titles, tab groups, and viewport summaries.

That projection is a compatibility layer for the current native host. It should not become the durable multi-backend semantic ABI.

### 4. Native host still injects product chrome and devtools posture

- `show_runtime_topbar(...)` defaults to `true`.
- `show_runtime_inspector(...)` reads host visibility flags and can open a full runtime inspector inside the app shell.
- The host renders hardcoded strings such as `native agent desktop`.
- In non-product mode it adds host-originated labels such as `root <component>` and `component <name>`.
- `Inspector` and `Tree` widgets switch behavior based on `product_shell`, meaning the backend changes interaction posture instead of only realizing semantic intent.

This is the clearest current contamination path.

### 5. Driver emits non-semantic runtime snapshot content

- `build_native_app_runtime_snapshot` hardcodes:
- a `runtime_surface` panel
- a `Reload Runtime` command
- a `Native Runtime` provider
- synthetic sessions/workspaces metadata

`kain-ui-native` then uses that snapshot to drive topbar and inspector content. That is host/devtools data masquerading as product-shell context.

### 6. UE/Slate readiness exists only as metadata, not as a semantic adapter

- `crates/ue5` already has data-driven registries for Slate widgets, delegates, composition rules, and editor attributes.
- `Ue5Context` loads those registries from extracted JSON metadata.

What is missing is the semantic bridge from `UiRuntimeBundle` and `UiPatch` into a true `kain-ui-slate` or UE editor adapter.

## Atlas Conclusions

- `kain-ui` is the correct long-term home for runtime graph ownership.
- `kain-core` still needs to shed runtime-specific UI execution concerns and emit richer first-class semantic contracts.
- `kain-ui-native` must stop defining default chrome, debug posture, and host-only widget behavior.
- `kain-driver` should package semantic bundles and optional devtools sidecars, not invent default UI shell metadata.
- `crates/ue5` should remain a data-driven backend metadata source until a real adapter consumes Kain semantic UI bundles directly.
