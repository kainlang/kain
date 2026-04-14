# Kain Universal Sculpt Space Lab

`threejs_node_ffi_space_lab` is a focused browser proof under `labs/` that now answers a bigger question than the first pass: can Kain drive a local Three.js sculpting suite through the Node FFI lane while compiling a real Rust WASM helper?

The lab stays intentionally small, but it now has real subsystem boundaries:

- `src/main.kn` proves the Kain entrypoint and `std::javascript::bridge` orchestration seam.
- `helpers/space_lab_runtime.mjs` builds both the browser bundle and the Rust `wasm32-unknown-unknown` sculpt artifact, then serves them on localhost.
- `helpers/client/` owns the Three.js editor shell, the universal viewport controller, and the WASM brush bridge.
- `manifests/*.json` keep scene, sculpt, viewport, and WASM build policy data-driven.
- `wasm/sculpt_core/` is a local Rust crate that exports raw brush deformation over vertex buffers.

## Universal Viewport Modes

- `Sculpt`: left-drag brush strokes, right-drag orbit, mouse-wheel dolly
- `Orbit`: inspection-only orbit mode
- `Fly`: pointer-locked navigation with `W`, `A`, `S`, `D`, `Shift`, `Space`, and `Ctrl`

## Commands

From `labs/threejs_node_ffi_space_lab`:

```bash
npm install
npm run build:wasm
npm run build
npm run serve
```

From the repo root:

```bash
cargo run -q -p cli --bin kain -- run labs/threejs_node_ffi_space_lab/src/main.kn
```

The local server defaults to `http://127.0.0.1:4192`.

## Current Checkout Note

The lab includes a real Kain entrypoint in `src/main.kn` plus a minimal `KAIN.fabric.toml`, but this checkout is still failing to resolve the JavaScript bridge identifiers inside Kain execution (`js_import` / `js_bridge_import`) even though the Node runtime path itself works. The validated live path for this lab is therefore:

- `npm run build:wasm`
- `npm run build`
- `npm run serve`
