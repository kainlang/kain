# UI Slate X100 Authoring Contract (Vector)

- Owner: Vector
- Contract Version: `ui_slate_x100.authoring_contract.v1`
- Purpose: Define explicit, compiler-owned authoring surfaces and emitted-bundle truth for Slate X100 UI semantics.
- Hard constraint: Spatial verifiability is a first-class requirement. The bundle must expose enough structure to verify "wrong region" errors without screenshots.

## Opt-In Rule (Compatibility Boundary)

Kain currently has a legacy compatibility path where `kain-ui` can infer runtime systems from tree shape.

This contract is opt-in:

- If any contract global (for example `ui_widget_registry`) is present, `kain-core` stamps `output.systems.session_state["ui.contract.version"]`.
- When that marker exists, authoring is expected to be explicit. Missing semantics must not be silently inferred.
- When the marker does not exist, `kain-core` may backfill missing runtime systems from `kain_ui::ui_runtime_systems_from_tree(...)` for legacy smokes.

This keeps existing demos alive while forcing authored-first apps to be structurally explicit.

## Spatial Verifiability Standard

Per `k_os_shell_lessons.md` and the acceptance/target architecture docs, a strong model or verification tool must be able to answer, from structure alone:

- Which panel owns this control?
- Which tab well is active?
- Did a control end up in the wrong region?
- Is an anchor-bound surface (menu/popup/palette) attached to the intended anchor zone?
- Are size bounds and resizability constraints present and inspectable?

The compiler must therefore emit:

- workspace graphs and docking intent (roots, placements, split ratios, persistent ids)
- tab ownership and active tab selection (group ids, default active, active mapping)
- anchor intent for transient UI (zone and target identity)
- verification-facing structure indices (node ids, identity keys, roles, dock placement, tab fields, constraints, region hints)

## Authoring Surfaces (Current Compiler Support)

These are authored in `.kn` JSX and lowered by `crates/kain-core/src/ui.rs`.

### 1. Stable Identity

Stable identity is required for verifiability and persistence.

- Element identity: `key="..."` or `id="..."` or `identity="..."` or `identity_key="..."` sets `UiNode.identity_key`.
- Component identity: component call props `key` or `id` seed `UiNode.identity_key` for the component ref node.

### 2. Workspace and Docking

Workspace metadata can be authored as a non-rendered declaration node:

```kain
component App():
    <workspace persistence_key="editor" preset="standard" virtualization_enabled={false} />
    <dock layout="dock" dock="center" persistent_layout_id="workspace.root">
        <panel dock="left" persistent_layout_id="panel.left" tab_group_id="left.tools" />
        <panel dock="right" persistent_layout_id="panel.right" tab_group_id="right.inspectors" />
        <viewport3d dock="center" persistent_layout_id="viewport.primary" />
    </dock>
```

Notes:

- Dock placement uses existing `dock="left|right|top|bottom|center|tab"` layout attributes.
- Persistent identity uses `persistent_layout_id`.
- Tab wells use `tab_group_id`, `tab_label`, `tab_order`, `tab_default_active`, `tab_closable`.

### 3. Chrome Roles and Anchor Intent

These are not "visual styling". They are semantic ownership and verification signals.

Authoring:

- `chrome_role="topbar|statusbar|left_panel|right_panel|bottom_panel|viewport|overlay|devtools|..."` (string namespace is project-defined)
- `anchor_zone="topbar|viewport|..."` and optional `anchor_target="identity_key"` for surfaces that must attach to a specific anchor target

Lowering:

- `chrome_role` becomes `node.props["ui.chrome_role"]`
- `anchor_zone` or `anchor` becomes `node.props["ui.anchor_zone"]`
- `anchor_target` becomes `node.props["ui.anchor_target"]`

### 4. Events and Command Dispatch

Events are compiler-owned routes, not `"[event:click]"` prop strings.

Authoring:

- `on_click={handlerExpr}` (and other `on_*`)
- Optional: `command="command.id"` to declare a command dispatch surface
- Optional: `transaction="label"` to name a transaction boundary
- Optional: `event_phase="bubble|capture|direct"`

Emission:

- `output.systems.event_routes` contains `UiEventRoute` with:
  - `route_id` (stable string)
  - `event`, `target`, `phase`
  - `handler_id` (string id, currently derived from the handler expression)
  - `dispatch_command` (optional)
- `output.systems.session_state["ui.event.transaction.<node_id>.<event>"]` stores optional transaction labels

### 5. State as Signals (No `state.<name>=<value>` Flattening)

Component state declarations are emitted as signals:

- `output.systems.signal_values[UiSignalId] = initial_value`
- The component ref node receives `node.props["ui.state_signal.<name>"] = "<signal_id>"`

This is a bridge for runtime/backends/tools without reintroducing value-flattened state props.

### 6. Motion (Tracks and Policy)

Motion is split into:

- Motion policy (global, verifiable): see `ui_motion_policy` in `paint_motion_schema.md`
- Motion tracks (per node): emitted as `output.systems.animation_tracks`

Authoring (per-node track):

- `motion_property="opacity|..."` (required to emit a track)
- `motion_id="..."` (optional)
- `motion_duration_ms={250}` (optional)
- `motion_trigger="mount|unmount|signal_change|hover|focus|layout_change|reload"` (optional)
- `motion_easing="linear|ease_in|ease_out|ease_in_out|spring"` (optional)
- `motion_preserve_on_reload={true|false}` (optional)

### 7. Paint (Values and Registries)

Paint values are explicit semantic data in `UiStyleSpec.values`, not ad hoc props.

Authoring:

- `style_<key>={value}` writes `node.style.values["<key>"]`
- `paint_<key>={value}` writes `node.style.values["paint.<key>"]`

Keys are normalized by converting `_` to `.`. Example: `paint_background_primary` becomes `paint.background.primary`.

## Emitted Bundle Truth (Verification Surfaces)

### `UiBuildOutput.systems` (Compiler-Owned Semantics)

Key fields used for verifiability:

- `workspace_layout.roots` and `workspace_layout.active_tabs`
- `event_routes` (route ids, phases, handler ids, dispatch command)
- `focus_graph` and `selection_model` scopes
- `signal_values` (state)
- `animation_tracks` (motion intent)

### `RealtimeAppBundle.ui_contracts` (Spatially-Verifiable Index)

`crates/kain-core/src/realtime_app_bundle.rs` exports an optional `ui_contracts` section containing:

- `workspace_layout` (if present in systems)
- `structure_index`: per-node verification facts (identity, role, dock placement, tab fields, constraints, region hints, anchors)
- contract JSON payloads when authored as globals (widget registry, command registry, motion policy, workspace schema, paint registry, motion registry)

This is the primary "structure-only" proof surface that tools and LLMs should use to detect misplaced controls or wrong-region anchors.

## Required Global Contract Values (Data-Driven)

These may be authored as compile-time values in `.kn` and are serialized into:

- `output.systems.session_state["ui.contract.*.json"]`
- `RealtimeAppBundle.ui_contracts.*_json`

Supported keys:

- `ui_widget_registry`
- `ui_command_registry`
- `ui_motion_policy`
- `ui_workspace_schema`
- `ui_paint_registry`
- `ui_motion_registry`

Schemas are defined in:

- `widget_registry_schema.md`
- `paint_motion_schema.md`

