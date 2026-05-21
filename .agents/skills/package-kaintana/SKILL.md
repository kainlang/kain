---
name: package-kaintana
description: Use when creating, extending, debugging, validating, or reviewing the Kaintana framework family under `blades/kaintana*`, including authored widgets/themes/layout helpers, package-local desktop/Vulkan bridges, and acceptance blades. Co-trigger `lang-ui` for authored UI behavior and escalate to runtime skills only when the work escapes the package.
---

# Package Kaintana

Use this skill for the package family rooted at `blades/kaintana`, not for generic runtime UI or compiler work.

## Owns

- `blades/kaintana/**`, `blades/kaintana-vulkan/**`, `blades/kaintana-test/**`, and `blades/kaintana-vulkan-test/**`.
- Kaintana's authored UI vocabulary, themes, layout helpers, reconciliation, widget events, examples, and package-local bridges such as `native/kaintana_desktop_bridge.c`.
- Package-local proof artifacts under `blades/kaintana/z3/**`.

## Co-Trigger And Boundaries

- Co-trigger `lang-ui` for authored widgets, themes, layout semantics, and app-facing UI behavior.
- Co-trigger `package-vulkain` when the Vulkan adapter or foreign presenter contract crosses into `blades/vulkain`.
- Escalate to `runtime-stdlib` or `runtime-core` only when the task leaves the package and changes generic `runtime/native` UI substrate or service behavior.
- Do not push Kaintana concepts down into raw runtime ABI files.

## Working Rules

- Keep framework policy in Kain/package code and keep runtime ABI generic.
- Keep live presenter work blade-owned; the passive UI substrate belongs below this skill.
- Prefer self-contained acceptance blades when the package needs native bridges or backend-specific manifests.

## Validation

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun
powershell -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -FrameBudget 3
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-test\run.ps1
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-vulkan-test\run.ps1
```
