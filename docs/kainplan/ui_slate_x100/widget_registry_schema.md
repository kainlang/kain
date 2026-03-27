# Widget Registry Schema (Vector)

- Owner: Vector
- Purpose: Define the data-driven, compiler-owned widget registry contract required for schema-driven tooling surfaces and LLM-legible widget semantics.
- Source lesson: `M:\K_OS\src-frontend\ui\shell` uses explicit registries (commands) and explicit workspace schemas to stay verifiable.

This schema is intended to be authored as a compile-time `.kn` value named `ui_widget_registry`.

`kain-core` will serialize it into:

- `UiBuildOutput.systems.session_state["ui.contract.widget_registry.json"]`
- `RealtimeAppBundle.ui_contracts.widget_registry_json`

## Schema Versioning

The registry payload must contain a version stamp so tools can safely evolve it:

- `schema_version`: string, required. Example: `"ui_widget_registry.v1"`.

## Top-Level Shape (JSON)

```json
{
  "schema_version": "ui_widget_registry.v1",
  "widgets": [
    {
      "widget_id": "kain.panel",
      "kind": "panel",
      "chrome_role": "left_panel",
      "caps": ["ui.docking", "ui.tabs"],
      "props": [
        { "name": "title", "type": "string", "required": false },
        { "name": "persistence_key", "type": "string", "required": false }
      ],
      "events": [
        { "event": "click", "phase": "bubble", "dispatch_command": "panel.activate" }
      ],
      "slots": [
        { "name": "content", "min": 0, "max": null }
      ]
    }
  ]
}
```

## Field Definitions

### `widgets[]`

Each entry describes a widget family in a backend-neutral way.

- `widget_id`: string, required. Stable id used in tooling, verification, and future adapters.
- `kind`: string, required. Suggested values:
  - `"panel"`, `"table"`, `"tree"`, `"viewport3d"`, `"overlay"`, `"element:<tag>"`, `"component:<name>"`
- `chrome_role`: string, optional. Matches `chrome_role` in authoring (`authoring_contract.md`).
- `caps`: string array, optional. Declarative capability keys required to realize this widget.
- `props`: array, optional. Each prop is:
  - `name`: string
  - `type`: string (`string|bool|int|float|token_ref|signal_ref|id_ref|...`)
  - `required`: bool
  - `default`: optional scalar
  - `doc`: optional string
- `events`: array, optional. Each event is:
  - `event`: string (`"click"`, `"pointerdown"`, custom event ids)
  - `phase`: string (`"bubble|capture|direct"`)
  - `handler_id`: optional string (stable handler id or symbolic ref)
  - `dispatch_command`: optional string (command surface id)
- `slots`: array, optional. Each slot is:
  - `name`: string
  - `min`: integer
  - `max`: integer or null

## Verification Expectations

The widget registry is not just for rendering. It is a verification surface.

Tools should be able to:

- ensure a node with `chrome_role="left_panel"` is realized using a widget whose registry entry declares that role
- validate required props exist when a widget kind is used
- validate that events declared in registry can be matched against emitted `UiEventRoute` entries
- map semantic widgets to backend capability tables without ad hoc host logic

## Command Registry Seam

Widget registry entries should reference command ids that exist in the command registry (`ui_command_registry`), mirroring the K_OS "source-scoped registry" model.

