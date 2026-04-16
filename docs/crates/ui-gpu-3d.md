# UI, GPU, And 3D Crates

These crates implement the semantic UI, native desktop, GPU, and 3D lanes.

## UI

- `kain-ui` owns semantic UI meaning and runtime contracts
- `kain-ui-native` hosts the native desktop surface

## 3D

- `kain-3D` owns viewport, renderer, interaction behavior, and the canonical scene catalog used by 3D tooling
- Scene catalogs should stay enumerable and alias-aware so UIs and inspectors can present real scene choices without hardcoded lists

## GPU And Web

- `gpu` and `kain-gpu-runtime` cover GPU artifact and runtime execution lanes
- `web` covers browser-facing codegen/runtime adapters
- `browser` is the browser-oriented tooling lane

## What This Family Proves

This group is where authored Kain gets turned into visible, interactive
applications rather than just source or JSON artifacts.
