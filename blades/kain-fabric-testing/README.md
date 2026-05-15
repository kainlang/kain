# Kain Apps Folder Guide

This folder holds first-class Kain applications and app prototypes that exercise the full language + runtime stack.

Canonical app guidance lives in [`../guides/examples/apps.md`](../guides/examples/apps.md)
and the root guide tree at [`../guides/README.md`](../guides/README.md).

## What Lives Here

- `kade-desktop/` is the native desktop app lane and its supporting assets.
- `kain-canvas-forge/` is a Node-first painting and Three.js composition studio prototype with a direct Electron desktop path.
- `kain-fabric-modeler/` is the Fabric-first native 3D modeling workbench and flagship multi-runtime app scaffold.
- `kain-fabric-dcc-suite/` is the broader flagship Fabric-first DCC suite scaffold with scene, ingest, sculpt, material, rig, sim, render, compositor, publish, automation, and tensor planning lanes.
- `ripgrep/` is reserved for CLI/tooling experiments and external tool integration.

## Output Hygiene

- Treat `native-app-preview/` (or similar) outputs as disposable build artifacts.
- Do not keep compiled executables or build caches under `apps/` in git.

## When Adding An App

- Add a short `README.md` inside the app folder describing what it proves.
- Keep runtime build outputs out of git and reference the correct build script instead.
