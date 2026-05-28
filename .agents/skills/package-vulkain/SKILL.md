---
name: package-vulkain
description: Use when creating, extending, debugging, validating, or reviewing the Vulkain package under `blades/vulkain`, including Kain GPU authoring, blade-local C bridge/shaders/platform lock scripts, and package examples. Always co-trigger `lang-gpu` and `lang-c-abi`; do not use this skill for generic GPU backend or runtime work.
---

# Package Vulkain

Use this skill for the Vulkain package family and examples under `blades/vulkain`.

## Owns

- `blades/vulkain/src/**`, `native/**`, `scripts/**`, `config/**`, `examples/**`, `run.ps1`, and `build-vulkain.ps1`.
- The blade-local bridge in `native/vulkain_bridge.c` and `native/vulkain_bridge.h`.
- Package-local shaders and platform lock/bootstrap behavior such as `scripts/vulkan-platform.ps1` and `config/vulkain.runtime.json`.

## Co-Trigger And Boundaries

- Always co-trigger `lang-gpu` and `lang-c-abi`.
- Escalate to `runtime-gpu` only when the change leaves the package and touches generic executors, graphics runtime ABI, or shader-bundle runtime consumption.
- Escalate to `bootstrap-gpu` when the change is really SPIR-V/PTX emission or compiler target behavior.
- Do not let package convenience scripts become the source of truth for repo-wide build plumbing; that belongs in `tool-build-system`.

## Working Rules

- Keep Vulkain package-local: blade bridge, package shaders, examples, and platform locks belong here.
- Preserve the split between package authoring and generic GPU/runtime layers.
- If bridge pointer math or bounds checks change, rerun the package-local SMT proof before trusting a pretty window.

## Validation

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\vulkain\build-vulkain.ps1
powershell -ExecutionPolicy Bypass -File .\blades\vulkain\run.ps1 -NoRun
powershell -ExecutionPolicy Bypass -File .\blades\vulkain\examples\mesh-scene\run.ps1
```

- When `native/vulkain_bridge.c` bounds or indexing logic changes, recheck `blades/vulkain/native/z3/vulkain_bridge_bounds.smt2`.
