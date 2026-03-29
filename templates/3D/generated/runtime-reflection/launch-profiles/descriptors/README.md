# Launch Profile Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the launch
profile catalog.

The files here are generated from the same manifest-driven launch surface that
powers `generated/runtime-reflection/launch-profiles/catalog.json`. They keep
the launch, receipt, and runtime-app bindings available through a single
descriptor document.

Contents:

- `launch_profile_catalog.json`: launch-profile metadata and descriptor-rooted
  catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
