# Kain Apps Folder Guide

This folder holds first-class Kain applications and app prototypes that exercise the full language + runtime stack.

## What Lives Here

- `kade-desktop/` is the native desktop app lane and its supporting assets.
- `kain-fabric-modeler/` is the Fabric-first native 3D modeling workbench and flagship multi-runtime app scaffold.
- `ripgrep/` is reserved for CLI/tooling experiments and external tool integration.

## Output Hygiene

- Treat `native-app-preview/` (or similar) outputs as disposable build artifacts.
- Do not keep compiled executables or build caches under `apps/` in git.

## When Adding An App

- Add a short `README.md` inside the app folder describing what it proves.
- Keep runtime build outputs out of git and reference the correct build script instead.
