# Kain Canvas Forge

`kain-canvas-forge` is a Node-first desktop-ready painting and 3D studio prototype under `M:/Code/Kain/apps`.

It is intentionally aimed at the "Krita + Clip Studio Paint style workspace" shape the current Node pipeline can prove well:

- a dense artist-facing docked UI
- a real 2D paint surface with layers and brush presets
- a live Three.js scene viewport for pose, lighting, and composition reference
- a Node-owned browser and desktop packaging lane

This is a robust product scaffold, not a claim that the repo already ships full parity with Krita or Clip Studio Paint.
The goal is to prove that the current Node + TypeScript + Three lane can already own a serious app shell in `apps/`.

## Main Pieces

- `manifests/*.json`: source of truth for workspaces, tools, brushes, panels, and 3D scene presets
- `helpers/studio_runtime.mjs`: Node runtime for manifest loading, client bundling, HTML/dashboard emission, and local serving
- `helpers/client/*`: Preact + Three.js client bundle
- `desktop/*`: Electron wrapper for desktop preview and packaging
- `src/main.kn`: Kain orchestration entrypoint that drives the Node helper runtime

## Suggested Commands

From `M:/Code/Kain/apps/kain-canvas-forge`:

```powershell
npm install
npm run print
npm run build
npm run serve
npm run desktop:dev
npm run desktop:package
```

From `M:/Code/Kain`:

```powershell
cargo run -p cli --bin kain -- run apps/kain-canvas-forge/src/main.kn
```

## Current Limits

- The 2D surface is a real layered paint prototype, but it is not yet a full non-destructive engine with masks, vector layers, or complex blend modes.
- The 3D lane is Three.js viewport composition today, not a full sculpt, rig, or asset pipeline.
- Desktop packaging uses Electron because the repo does not yet have a first-class Node-desktop packager for this app lane.

