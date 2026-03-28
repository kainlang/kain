# Gamma Operator Packaging Proof

- Owner: Gamma
- Scope: native packaging, devtools separation, hot-reload operator loop, and proof surfaces for the UI Slate X100 work
- Status: active

## What We Treat As Stable

- `kain-driver` is the packaging boundary. It emits the materialized app tree, packaged runtime snapshot, and launcher-side sidecars that the native host consumes.
- The runtime snapshot is part of the operator surface. It should describe the app in data, not hide reload or capability state in launcher-only logic.
- Devtools stay opt-in. Product mode should launch cleanly without inspector furniture unless a debug path is explicitly enabled.

## Proof Surfaces

- `smoketest/UI/kinetic_ui_atlas` proves semantic tabs, docked shells, shader canvases, and a real viewport-backed workspace in one authored executable.
- `smoketest/UI/website_clone_signalcraft` proves a product-facing shell with stronger visual separation from tool chrome.
- `smoketest/UI/spv_ui_surface_probe` proves the shader-canvas packaging lane and the packaged asset path.

## Operator Loop

1. Materialize the native app through the driver.
2. Launch the packaged artifact, not a debug scaffold.
3. Use the snapshot and manifest sidecars as the inspectable state boundary when behavior changes.
4. Rebuild or relaunch when the authored truth changes, but preserve valid state only when the snapshot contract still matches.

## Notes

- This doc is intentionally narrow. It records the packaging/operator shape we can already infer from the repo so future work can stay grounded.
- If the launcher or snapshot contract changes, update this note and the repo memory together.
