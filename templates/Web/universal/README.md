# Universal Web Template

`universal/` is the first serious Kain web starter that assumes:

- users should not need `rustc` or `cargo`
- Kain is still the orchestration language
- Node FFI is a practical web runtime lane today
- Kain UI should still shape authoring and preview where it already has real
  semantic strength

## Entry Points

- `src/main.kn`
  - builds the full experience matrix into `outputs/sites`
- `src/native_preview.kn`
  - semantic Kain UI preview surface for the pack
- `src/actor_server.kn`
  - reports the actor-server topology for the actor-oriented experience

## Runtime Surface

- `helpers/web_runtime.mjs`
  - manifest loader
  - HTML and client-island renderer
  - static artifact writer
  - local actor-aware HTTP server
- `package.json`
  - Node scripts for build, print, and local serving

## Manifest Surface

- `manifests/app.json`
  - top-level app and registry configuration
- `manifests/themes/*.json`
  - visual systems
- `manifests/content/*.json`
  - copy, cards, metrics, case studies, chat seeds, actor roles
- `manifests/scenes/*.json`
  - 3D/immersive scene descriptors
- `manifests/experiences/*.json`
  - final archetype compositions

## Archetypes Included

- `business_launch`
- `portfolio_signal`
- `immersive_luminous`
- `chat_orbit`
- `actor_mesh_foundry`

## Typical Usage

Use the Kain entrypoint:

```powershell
kain run src/main.kn
```

Use the Node-only scripts:

```powershell
npm run build
npm run serve:business
npm run serve:actors
```

Switch to a TypeScript-aware helper runtime by changing `[node_ffi]` in
`KAIN.toml` from `node` to `npx` with `tsx`.
