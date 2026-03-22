# Kain Labs Folder Guide

`labs/` holds focused runtime validation and experiment harnesses.

Current lanes:

- `raw_native_world_lab/` for raw-native world and UI bundle validation
- `raw_native_magma_forge_lab/` for magma forge runtime proofing
- `native_ui_viewport_smoke/` for native UI + viewport smoke validation

Keep sources, configs, and summary reports. Treat compiled outputs (`.exe`, `.pdb`, `.ilk`, `.obj`, `.o`) plus build caches (`target/`, `.kain-runtime`) as disposable.
