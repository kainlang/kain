---
name: kaintana-framework
description: Use when creating, extending, debugging, validating, or reviewing the Kaintana UI framework under blades/kaintana or its acceptance blade under blades/kaintana-test, especially dual-register Kain UI helpers, blade-owned desktop/Vulkan host routing, passive native UI session usage, and Kaintana proof artifacts.
---

# Kaintana Framework

Use this skill in `D:\Kain-Lang` when touching:

- `blades/kaintana/**`
- `blades/kaintana-test/**`
- Kaintana proof artifacts under `blades/kaintana/z3/**`

## Ownership

Kaintana is the framework lane above the passive raw UI ABI.

- `runtime/native` owns generic sessions, events, draw commands, hot reload, and low-level host substrate.
- `blades/kaintana` owns the Kain authoring vocabulary: themes, layout helpers, retained/immediate helpers, and backend-neutral host helpers.
- Concrete live presentation stays blade-owned:
  - desktop compatibility host: `blades/kaintana/native/kaintana_desktop_bridge.c`
  - Vulkan foreign presenter lane: `blades/vulkain`

Do not push Kaintana concepts back into `runtime/native`. If a feature is app/framework policy, keep it in Kain or a blade-local native bridge.

## Current Shape

- `blades/kaintana/src/kaintana.kn`
  - public framework API
  - `KaintanaWindowSpec`, `KaintanaTheme`, rect/split/row/column helpers
  - retained helpers: `kaintana_retained_region`, `kaintana_retained_surface`, `kaintana_retained_label`
  - immediate helpers: `kaintana_immediate_panel`, `kaintana_immediate_badge`, `kaintana_immediate_button`, `kaintana_immediate_metric`
  - host wrappers: `kaintana_session_create`, `kaintana_begin_frame`, `kaintana_commit_frame`, `kaintana_host_run_window`, `kaintana_host_write_report`
- `blades/kaintana/native/kaintana_desktop_bridge.c`
  - Win32/GDI compatibility host
  - fixed rect/text command buffer
  - screenshot/report support
- `blades/kaintana-test/src/main.kn`
  - full desktop showcase entrypoint
- `blades/kaintana-test/entrypoints/vulkan.kn`
  - Vulkan foreign-presenter showcase entrypoint
- `blades/kaintana-test/run.ps1`
  - chooses `desktop`, `vulkan`, or `all`

## Kain Authoring Pattern

Desktop entrypoints are intentionally self-contained right now because of a compiler gotcha.

Minimal Kaintana usage pattern:

```kn
use kaintana::kaintana_backend_desktop
use kaintana::kaintana_default_window_spec
use kaintana::kaintana_session_create

fn main() -> Int:
    let spec = kaintana_default_window_spec("My Window", 1280, 720, kaintana_backend_desktop())
    let session = kaintana_session_create("my-app", spec)
    let _frame = kaintana_begin_frame(session, "rev-1", 16.0)
    let _commit = kaintana_commit_frame(session)
    return kaintana_host_run_window(spec)
```

For richer apps, follow `blades/kaintana-test/src/main.kn`:

- keep `world` / `entangle` / `patch` / `law` / `converge` / `orchestrate` in the entrypoint
- use Kaintana helpers for authored UI layout/theme nodes
- let the same app contract target `desktop` or `vulkan` by changing settings, not runtime architecture

## Validation

Framework package:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun
```

Desktop acceptance proof:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-test\run.ps1 -Backend desktop
```

Vulkan acceptance proof:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-test\run.ps1 -Backend vulkan
```

Proof artifacts:

- desktop frame report: `blades/kaintana-test/.kain/run/kaintana_test_desktop_frame.txt`
- desktop host report: `blades/kaintana-test/.kain/run/kaintana_test_desktop_host.txt`
- desktop screenshot: `blades/kaintana-test/.kain/run/kaintana_test_desktop.bmp`
- Vulkan frame report: `blades/kaintana-test/.kain/run/kaintana_test_vulkan_frame.txt`
- Vulkan host report: `blades/kaintana-test/.kain/run/kaintana_test_vulkan_host.txt`

Current Z3 checks:

- `z3/reports/20260516T015051Z-kaintana-desktop-command-capacity.json`
- `z3/reports/20260516T015051Z-kaintana-layout-split-partition.json`

## Gotchas

- Imported local Kain modules that contain `world` / `entangle` currently double-stage those bindings during native LLVM artifact staging. Symptom: `entangle endpoint '...' participates in more than one binding`. Until fixed, keep showcase entrypoints self-contained and share only plain helper modules.
- `[c_ffi]` bindings are not transitive through a library blade yet. If `blades/kaintana` wraps a native bridge, consumer blades such as `blades/kaintana-test` still need their own matching `[[c_ffi.libraries]]` entries for `use c::...`.
- `ui_host_session_create(..., "software")` is still passive. It records authored session/draw state; it does not open a live OS window by itself.
- The current Vulkan proof proves host routing into a foreign presenter lane. It does not yet rasterize the full Kaintana scene graph through Vulkan.
