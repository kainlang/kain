# Three.js Node FFI Sculpt Lab Architecture

## What It Is

`threejs_node_ffi_space_lab` is a browser-side Kain proof under `labs/` that now validates three things together:

1. Kain can still own the orchestration surface.
2. Node can bundle and serve a localhost Three.js application.
3. A local Rust crate can compile to `wasm32-unknown-unknown` and drive sculpt deformation inside that viewport.

The live product shape is a compact sculpting suite with one universal viewport that can switch between sculpt, orbit, and fly behavior over the same floating orb scene.

## Ownership Boundaries

- `src/main.kn` owns the Kain-facing orchestration proof through `std::javascript::bridge`.
- `helpers/space_lab_runtime.mjs` owns manifest loading, Rust WASM compilation, browser bundling, HTML emission, and static localhost serving.
- `helpers/client/model.ts` owns browser-side manifest validation and typed runtime model loading.
- `helpers/client/universal-viewport.ts` owns camera-mode policy for the universal viewport.
- `helpers/client/wasm-sculpt-core.ts` owns WASM loading plus JS-to-linear-memory brush calls.
- `helpers/client/main.ts` owns scene assembly, sculpt interaction, HUD wiring, and the animation loop.
- `manifests/space_scene.json` owns camera, orb, environment, and lighting values.
- `manifests/sculpt_suite.json` owns brush registry and default sculpt tuning.
- `manifests/viewport_profiles.json` owns viewport-mode policy.
- `manifests/wasm_pipeline.json` owns Rust crate build and public artifact paths.
- `wasm/sculpt_core/` owns the Rust brush kernel that compiles to `outputs/wasm/sculpt_core.wasm`.

## Primary Data Flow

`manifests/*.json -> helpers/space_lab_runtime.mjs -> cargo build --target wasm32-unknown-unknown + esbuild bundle -> outputs/index.html + outputs/client/* + outputs/wasm/sculpt_core.wasm`

`outputs/index.html -> browser -> helpers/client/*.ts -> Three.js scene + universal viewport + WASM sculpt brush`

`src/main.kn -> std::javascript::bridge -> helpers/space_lab_runtime.mjs`

## Common Commands

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

## Architectural Guardrails

- Keep scene, brush, viewport, and WASM build tuning in the manifest registry files instead of hardcoding them into the client loop.
- Keep Node as the owner of browser packaging and localhost serving for this lab.
- Keep the sculpt core narrow and data-oriented. It mutates vertex positions only; camera policy, raycasts, UI, and geometry normal rebuilds stay in the browser lane.
- Treat this lab as a browser DCC proof, not as a substitute for the native `viewport3d` stack elsewhere in the repo.

## Common Errors

- `npm install` must run before `npm run build` or `npm run serve`, because `three` and `esbuild` are local package dependencies.
- `rustup target add wasm32-unknown-unknown` must exist before the Rust sculpt crate can compile.
- `outputs/` is generated output and should be rebuilt instead of edited by hand.
- The current checkout still rejects `js_import` / `js_bridge_import` during Kain execution for this lab, so treat `npm run build`, `npm run build:wasm`, and `npm run serve` as the validated live path until the host-backed Kain bridge registration is repaired.
