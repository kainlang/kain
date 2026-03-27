# Kain Canvas Forge Architecture

This file is the durable project overview for `M:/Code/Kain/apps/kain-canvas-forge`.

## What It Is

`kain-canvas-forge` is a Node-first art studio prototype that proves the current Kain JavaScript bridge can drive a substantial desktop-grade UI and a real Three.js viewport.

The intended product shape is:

1. A paint-first workstation
2. A 3D-assisted illustration and layout workspace
3. A desktop-ready shell with a direct `.exe` packaging path

It is inspired by Krita and Clip Studio Paint style workflows, but the code should be treated as a fresh Kain-owned app scaffold rather than an imitation layer.

## Ownership Boundaries

- `manifests/*.json` owns the app registry data for workspaces, tools, brushes, panels, and scene presets.
- `helpers/studio_runtime.mjs` owns Node-side orchestration: loading manifests, bundling the browser client, emitting HTML, serving files, and preparing desktop outputs.
- `helpers/client/*` owns the browser UI, paint surface, and Three.js viewport runtime.
- `desktop/*` owns Electron bootstrap and native window behavior only.
- `src/main.kn` owns the Kain-facing orchestration proof that the app can be driven through the JavaScript bridge.

## Main Files

- `KAIN.toml`: Node runtime config for Kain bridge execution
- `package.json`: Node, client build, and desktop packaging script surface
- `manifests/app.json`: app identity, bundle config, desktop config, and registry map
- `manifests/workspaces.json`: workspace mode registry
- `manifests/tool_catalog.json`: tool rail and command metadata
- `manifests/brushes.json`: paint preset registry
- `manifests/panel_layout.json`: panel composition registry
- `manifests/scene_library.json`: 3D reference stage presets
- `helpers/studio_runtime.mjs`: Node helper runtime
- `helpers/client/main.tsx`: browser bootstrap
- `helpers/client/studio_app.tsx`: main application shell
- `helpers/client/style.css`: visual system
- `desktop/main.mjs`: Electron main process
- `desktop/preload.mjs`: Electron preload bridge
- `src/main.kn`: Kain build orchestration entrypoint

## Primary Data Flow

`manifests/*.json -> helpers/studio_runtime.mjs -> outputs/index.html + outputs/client/canvas-forge.bundle.js`

`outputs/index.html -> Electron desktop shell or local Node HTTP server`

`Kain src/main.kn -> std::javascript::bridge -> helpers/studio_runtime.mjs`

## Common Commands

From `M:/Code/Kain/apps/kain-canvas-forge`:

```powershell
npm install
npm run build
npm run serve
npm run desktop:dev
npm run desktop:package
```

From `M:/Code/Kain`:

```powershell
cargo run -p cli --bin kain -- run apps/kain-canvas-forge/src/main.kn
```

## Architectural Guardrails

- Keep studio metadata in manifests rather than hardcoding workspace or tool truth into the client.
- Keep Node as the owner of browser packaging and desktop wiring for this app.
- Keep the Three.js lane focused on viewport/reference composition until the repo has a broader first-class web 3D app contract.
- Keep Kain as orchestration proof and future semantic owner, not as a duplicate of the browser client implementation.

## Common Errors

- `npm install` must run before the bundler or Electron scripts can work because `esbuild`, `preact`, `three`, and `electron` are Node dependencies, not repo-global guarantees.
- `outputs/` is disposable generated output and should be rebuilt rather than edited.
- If the client bundle is missing, rerun `npm run build` before launching the desktop wrapper.

