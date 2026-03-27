# Paint + Motion Schema (Vector)

- Owner: Vector
- Purpose: Define explicit, compiler-owned paint and motion contracts so rich visuals are bundle-visible and structurally verifiable.
- Source lesson: `motionSystem.ts` in the old K_OS shell separates mode, performance tier, interaction state, and derived policy outputs. That separation is required here too.

This document defines:

1. Paint registry (`ui_paint_registry`)
2. Motion registry (`ui_motion_registry`)
3. Motion policy (`ui_motion_policy`)
4. Per-node motion track authoring (`motion_*` attributes)
5. Per-node paint authoring (`style_*` and `paint_*` attributes)

## Emission Surfaces

When authored as compile-time `.kn` values, `kain-core` serializes these into:

- `UiBuildOutput.systems.session_state["ui.contract.paint_registry.json"]`
- `UiBuildOutput.systems.session_state["ui.contract.motion_registry.json"]`
- `UiBuildOutput.systems.session_state["ui.contract.motion_policy.json"]`
- `RealtimeAppBundle.ui_contracts.*_json`

Per-node motion tracks emit into:

- `UiBuildOutput.systems.animation_tracks`

Per-node paint values emit into:

- `UiNode.style.values`

## 1. Paint Registry (`ui_paint_registry`)

Paint registry is a named library of paint recipes. It exists so authored paint is:

- stable
- reusable
- verifiable from structure

Suggested JSON shape:

```json
{
  "schema_version": "ui_paint_registry.v1",
  "recipes": [
    {
      "paint_id": "paint.surface.editor_bg",
      "kind": "solid",
      "color": "#050505"
    },
    {
      "paint_id": "paint.surface.hero_gradient",
      "kind": "linear_gradient",
      "from": "#111111",
      "to": "#050505",
      "angle_degrees": 90
    }
  ]
}
```

Backends are expected to treat paint ids as semantic, not as renderer-local shortcuts.

## 2. Motion Registry (`ui_motion_registry`)

Motion registry defines named track templates for reuse:

```json
{
  "schema_version": "ui_motion_registry.v1",
  "tracks": [
    {
      "motion_id": "motion.fade_in",
      "property": "opacity",
      "duration_ms": 220,
      "trigger": "mount",
      "easing": "ease_in_out",
      "preserve_on_reload": true
    }
  ]
}
```

## 3. Motion Policy (`ui_motion_policy`)

Motion policy is the explicit "why" behind motion behavior, modeled after the K_OS shell:

- mode: `"full"|"balanced"|"reduced"`
- performance tier: `"warp"|"cruise"|"safe"`
- interaction state: resizing, pointer active
- derived policy outputs used by widget realization

Suggested JSON shape:

```json
{
  "schema_version": "ui_motion_policy.v1",
  "mode": "balanced",
  "tier": "cruise",
  "inputs": {
    "prefers_reduced_motion": false,
    "low_power_mode": false
  },
  "derived": {
    "ui_capacitor_state": "priming",
    "disable_panel_motion_while_resizing": true
  }
}
```

This payload is explicitly intended to be referenced by verification tooling:

- If `mode="reduced"`, a tool should be able to assert that certain motion tracks were not emitted or were downgraded.
- If tier is `"safe"`, motion should be explicitly reduced and not silently dropped.

## 4. Per-Node Motion Tracks (`motion_*`)

To emit a `UiAnimationTrack` from JSX:

- Provide `motion_property`.
- Optional override fields include `motion_id`, `motion_duration_ms`, `motion_trigger`, `motion_easing`, `motion_preserve_on_reload`.

Example:

```kain
<panel key="left.tools"
       dock="left"
       motion_property="opacity"
       motion_trigger="mount"
       motion_duration_ms={200}
       motion_easing="ease_in_out" />
```

## 5. Per-Node Paint Values (`style_*` and `paint_*`)

Paint values are written into `UiStyleSpec.values`:

- `style_<key>` becomes `values["<key>"]`
- `paint_<key>` becomes `values["paint.<key>"]`
- `_` is normalized to `.` in emitted keys

Example:

```kain
<panel key="left.tools"
       paint_background_primary="paint.surface.editor_bg"
       style_text_color="theme.text.default" />
```

Verifiability rule:

- A tool should be able to inspect `UiNode.style.values` and determine which paint recipe ids and style tokens are in play without running the backend renderer.

