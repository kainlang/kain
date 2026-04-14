# Kain Three.js Node FFI Space Lab

`threejs_node_ffi_space_lab` is a focused proof under `labs/` that answers one question: can Kain drive a local Three.js browser app through the Node FFI lane?

This lab keeps the stack deliberately small:

- `src/main.kn` proves Kain can orchestrate the build through `std::javascript::bridge`
- `helpers/space_lab_runtime.mjs` owns Node-side bundling, HTML emission, and localhost serving
- `helpers/client/main.ts` owns the Three.js free-fly viewport
- `manifests/space_scene.json` keeps the scene shape data-driven

## Controls

- `W`, `A`, `S`, `D`: move
- mouse: look around after pointer lock
- `Shift`: sprint
- `Space`: rise
- `Ctrl`: descend
- `Esc`: release pointer lock

## Commands

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

The local server defaults to `http://127.0.0.1:4192`.

## Current Checkout Note

The lab includes a real Kain entrypoint in `src/main.kn` plus a minimal
`KAIN.fabric.toml`, but this checkout is currently failing to resolve the
JavaScript bridge identifiers inside Kain execution (`js_import` /
`js_bridge_import`) even though the Node runtime path itself works. The live
Three.js proof is therefore validated through the Node helper commands above.
