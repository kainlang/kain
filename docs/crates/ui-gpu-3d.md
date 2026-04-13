# UI, GPU, And 3D Crates

These crates implement the semantic UI, native desktop, GPU, and 3D lanes.

## UI

- `kain-ui` owns semantic UI meaning and runtime contracts
- `kain-ui-native` hosts the native desktop surface

## 3D

- `kain-3D` owns viewport, renderer, and interaction behavior

## GPU And Web

- `gpu` and `kain-gpu-runtime` cover GPU artifact and runtime execution lanes
- `web` covers browser-facing codegen/runtime adapters
- `browser` is the browser-oriented tooling lane

## What This Family Proves

This group is where authored Kain gets turned into visible, interactive
applications rather than just source or JSON artifacts.
