# Three.js Node FFI Space Lab Architecture

## What It Is

`threejs_node_ffi_space_lab` is a minimal Kain proof app under `labs/` that uses the host-backed Node FFI lane to bundle and serve a real Three.js scene.

The product goal is narrow and explicit:

1. Kain owns orchestration
2. Node owns browser packaging and localhost serving
3. Three.js owns the live 3D viewport runtime

## Ownership Boundaries

- `src/main.kn` owns the Kain-facing orchestration proof.
- `helpers/space_lab_runtime.mjs` owns manifest loading, client bundling, HTML emission, and static file serving.
- `helpers/client/main.ts` owns the runtime viewport scene, controls, animation loop, and HUD.
- `manifests/app.json` owns app identity, output paths, and server config.
- `manifests/space_scene.json` owns camera, sphere, beacon, star-field, and lighting values.

## Primary Data Flow

`manifests/*.json -> helpers/space_lab_runtime.mjs -> outputs/index.html + outputs/client/three-space.bundle.js`

`src/main.kn -> std::javascript::bridge -> helpers/space_lab_runtime.mjs`

`outputs/index.html -> local Node HTTP server -> browser`

## Common Commands

From `labs/threejs_node_ffi_space_lab`:

```bash
npm install
npm run build
npm run serve
```

From the repo root:

```bash
cargo run -q -p cli --bin kain -- run labs/threejs_node_ffi_space_lab/src/main.kn
```

## Architectural Guardrails

- Keep scene tuning data in `manifests/space_scene.json` instead of hardcoding it in the client loop.
- Keep Node as the owner of browser packaging and local serving for this lab.
- Keep the lab focused on proving the Kain -> Node FFI -> Three.js path, not on growing a full editor shell.

## Common Errors

- `npm install` must run before `npm run build` or `npm run serve`, because `three` and `esbuild` are local package dependencies.
- `outputs/` is generated output and should be rebuilt instead of edited by hand.
- The current checkout still rejects `js_import` / `js_bridge_import` during Kain execution for this lab, so treat `npm run build` and `npm run serve` as the validated live path until the host-backed Kain bridge registration is repaired.
