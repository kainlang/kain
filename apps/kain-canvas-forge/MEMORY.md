# Kain Canvas Forge Memory

This file keeps durable implementation memory for `M:/Code/Kain/apps/kain-canvas-forge`.

## 2026-03-27 - Initial Node-First Painting And 3D Studio Scaffold

The app now exists as a real repo-local prototype for a Krita-like and Clip Studio Paint-like workstation shape using the current Node pipeline.

What changed:

- Added a new app under `apps/kain-canvas-forge`.
- Chose a manifest-driven structure so workspace modes, tools, brushes, panels, and Three.js scene presets live in JSON registries instead of being hardcoded into the client.
- Added a Node helper runtime that bundles the client with `esbuild`, emits static outputs, serves them locally, and exposes a clean method surface for Kain orchestration.
- Added a Preact + Three.js client with:
  - a layered paint canvas
  - brush and eraser interaction
  - scene viewport panel
  - docked panel shell
  - export path for flattened PNG paint output
- Added an Electron wrapper so the app has a direct desktop packaging lane instead of staying web-only.

Design decisions to preserve:

- Node is the owner of packaging and desktop glue for this app.
- Kain owns orchestration proof through `src/main.kn`, but the browser client remains the UI runtime for now.
- The app should stay data-driven first so future studio expansion mostly adds manifest entries and focused client components instead of more monolithic conditionals.

Current risks:

- Electron packaging is provided as the current practical `.exe` path, but it is not yet aligned with a broader repo-standard Node desktop lane.
- The 2D canvas is intentionally direct and fast to understand, not a finished professional paint engine.
- The Three.js viewport is a composition/reference lane, not yet a full modeling or rigging runtime.

Recommended next step:

- Add document persistence and project save/load around the layer stack, scene selection, camera state, and workspace preferences before expanding more advanced art features.

