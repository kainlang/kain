---
name: kaintana-framework
description: Use when creating, extending, debugging, validating, or reviewing the Kaintana UI framework under blades/kaintana, its optional Vulkan adapter under blades/kaintana-vulkan, or the acceptance blades under blades/kaintana-test and blades/kaintana-vulkan-test, especially authored layout/widget helpers, action/keybinding and host-service wrappers, blade-owned desktop/Vulkan host routing, passive native UI session usage, and Kaintana proof artifacts.
---

# Kaintana Framework

Use this skill in `D:\Kain-Lang` when touching:

- `blades/kaintana/**`
- `blades/kaintana-vulkan/**`
- `blades/kaintana-test/**`
- `blades/kaintana-vulkan-test/**`
- Kaintana proof artifacts under `blades/kaintana/z3/**`

## Ownership

Kaintana is the framework lane above the passive raw UI ABI.

- `runtime/native` owns generic sessions, events, draw commands, hot reload, and low-level host substrate.
- `blades/kaintana` owns the Kain authoring vocabulary: themes, layout helpers, retained/immediate helpers, and the default desktop host helpers.
- Concrete live presentation stays blade-owned:
  - desktop compatibility host: `blades/kaintana/native/kaintana_desktop_bridge.c`
  - optional Vulkan foreign presenter adapter: `blades/kaintana-vulkan` over `blades/vulkain`

Do not push Kaintana concepts back into `runtime/native`. If a feature is app/framework policy, keep it in Kain or a blade-local native bridge.

## Current Shape

- `blades/kaintana/src/kaintana.kn`
  - tiny public prelude only: framework name/version/surface score
- `blades/kaintana/src/api/kaintana_ui.kn`
  - public builder surface. Builders are lightweight widget specs; pass the heavy `KaintanaContext` only to `*_render(ctx, builder)`.
  - current widgets: panel, label, button, text input, slider
- `blades/kaintana/src/api/widgets.kn`
  - component wrappers that reconcile nodes and record renderer-neutral-ish fill/text commands
- `blades/kaintana/src/core/types.kn`
  - window spec, theme, rect/color, context, render result, input binding types
  - imports root `std::alloc`, `std::collections`, and `std::text`
  - `KaintanaContext.nodes` is a stdlib `SlotMap`; text-facing helpers use `StringView`
- `blades/kaintana/src/core/reconciliation.kn`
  - passive UI session creation, frame begin/commit/destroy, stable key map, SlotMap handles, and arena-backed frame widget cells
  - normalizes append-only SlotMap `free_head` from `count` after inserts because nested `SlotMapInsert.map.free_head` currently comes back stale under native LLVM
- `blades/kaintana/src/core/layout.kn`
  - rect/inset/split/row/column/grid math over root `std::math`
- `blades/kaintana/src/core/theme.kn`
  - named theme packs: solar-broadcast, marine-terminal, kawaii-voltage, oxide-dcc
- `blades/kaintana/src/core/input.kn`
  - action/axis/session wrappers over root `std::input`
- `blades/kaintana/src/core/render_commands.kn`
  - command checksum/counting, passive `std::ui` draw calls, and optional desktop bridge emission
- `blades/kaintana/src/platform/desktop/desktop_adapter.kn`
  - desktop PAL wrapper over the blade-local compatibility bridge. Do not import `c::kaintana_desktop_bridge` here; keep that import in the entrypoint that owns linking.
- `blades/kaintana/examples/*.kn`
  - single-file examples that all compile into the normal `kaintana.exe` tour app through `examples/example_tour_suite.kn`
  - current examples: to-do list, tabbed pane, modal popup, data grid, keypad, resizable panel, file explorer, mega button stress
- `blades/kaintana/native/kaintana_desktop_bridge.c`
  - Win32/GDI compatibility host
  - fixed rect/text command buffer with client-size scaling and sized text fallback
  - double-buffered paint path and non-spammy frame loop to avoid flicker/jitter during long live runs
  - screenshot/report support
- `blades/kaintana-vulkan/src/kaintana_vulkan.kn`
  - optional Vulkan adapter helpers: `kaintana_vulkan_embed_available`, `kaintana_vulkan_host_run_window`, `kaintana_vulkan_host_write_report`
- `blades/kaintana-test/src/main.kn`
  - oxide DCC-style desktop showcase entrypoint
  - proves top bar, viewport, charts, sliders, keypad, action maps, host services, snapshot text, and input trace in one shell
- `blades/kaintana-vulkan-test/src/main.kn`
  - Vulkan foreign-presenter showcase entrypoint
- `blades/kaintana-test/run.ps1`
  - desktop-only acceptance runner
  - `-FrameBudget` overrides the default long-run host pressure count through `KAINTANA_TEST_FRAME_BUDGET`
- `blades/kaintana-vulkan-test/run.ps1`
  - Vulkan-only acceptance runner
  - `-FrameBudget` overrides the foreign-presenter pressure count through `KAINTANA_VULKAN_TEST_FRAME_BUDGET`

## Kain Authoring Pattern

Desktop and Vulkan showcase entrypoints are intentionally self-contained right now because of compiler and manifest gotchas.

Minimal Kaintana desktop usage pattern:

```kn
use std::alloc
use std::collections
use std::graphics
use std::input
use std::math
use std::text
use std::ui
use c::kaintana_desktop_bridge
use desktop_adapter::kaintana_desktop_probe
use reconciliation::kaintana_context_destroy
use theme::kaintana_theme_named
use types::kaintana_backend_desktop
use types::kaintana_default_window_spec
use kaintana_ui::*

fn main() -> Int:
    if kaintana_desktop_probe() != 1:
        return 20
    let spec = kaintana_default_window_spec("My Window", 1280, 720, kaintana_backend_desktop())
    var ctx = kaintana_context("my-app", spec, kaintana_theme_named("oxide-dcc"), true)
    ctx = kaintana_begin(ctx, "rev-1", 16.0)
    let b0 = kaintana_button(ui(ctx), "Ignite")
    let b1 = kaintana_button_key(b0, "action.ignite")
    let result = kaintana_button_render(ctx, b1)
    ctx = kaintana_commit(result.ctx)
    return kaintana_context_destroy(ctx)
```

For richer desktop apps, follow `blades/kaintana-test/src/main.kn`:

- keep `world` / `entangle` / `patch` / `law` / `converge` / `orchestrate` in the entrypoint
- use Kaintana helpers for authored UI layout/theme nodes
- keep backend-specific presenter acceptance in separate blades until per-entry `[c_ffi]` manifests are real

## Validation

Framework package:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana\run.ps1 -NoRun
```

Optional Vulkan adapter package:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-vulkan\run.ps1 -NoRun
```

Desktop acceptance proof:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-test\run.ps1
```

Long-run desktop pressure test:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-test\run.ps1 -FrameBudget 1000
```

Vulkan acceptance proof:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\kaintana-vulkan-test\run.ps1
```

Proof artifacts:

- framework package host report: `blades/kaintana/.kain/run/kaintana_host_report.txt`
- framework package screenshot: `blades/kaintana/.kain/run/kaintana_host.bmp`
- framework package examples tour currently renders `commands=169` in the host report
- desktop frame report: `blades/kaintana-test/.kain/run/kaintana_test_desktop_frame.txt`
- desktop host report: `blades/kaintana-test/.kain/run/kaintana_test_desktop_host.txt`
- desktop screenshot: `blades/kaintana-test/.kain/run/kaintana_test_desktop.bmp`
- desktop snapshot text: `blades/kaintana-test/.kain/run/kaintana_test_desktop_snapshot.txt`
- desktop input trace: `blades/kaintana-test/.kain/run/kaintana_test_desktop_input_trace.txt`
- Vulkan frame report: `blades/kaintana-vulkan-test/.kain/run/kaintana_vulkan_test_frame.txt`
- Vulkan host report: `blades/kaintana-vulkan-test/.kain/run/kaintana_vulkan_test_host.txt`

Current Z3 checks:

- `z3/reports/20260516T015051Z-kaintana-desktop-command-capacity.json`
- `z3/reports/20260516T015051Z-kaintana-layout-split-partition.json`
- latest rerun observed: `z3/reports/20260520T053638Z-kaintana-desktop-command-capacity.json` and `z3/reports/20260520T053638Z-kaintana-layout-split-partition.json`

## Gotchas

- Imported local Kain modules that contain `world` / `entangle` are supported again after the `crates/kain-driver` staging fix. If `entangle endpoint '...' participates in more than one binding` ever returns, re-check realtime/native UI staging before assuming the app structure is wrong.
- `[c_ffi]` bindings are not transitive through a library blade yet. If `blades/kaintana` or `blades/kaintana-vulkan` wraps a native bridge, consumer blades such as `blades/kaintana-test` or `blades/kaintana-vulkan-test` still need their own matching `[[c_ffi.libraries]]` entries for `use c::...`.
- Inside `blades/kaintana`, keep `use c::kaintana_desktop_bridge` in `src/main.kn` or another entrypoint that owns linking. Importing the C bridge from `platform/desktop/desktop_adapter.kn` can duplicate generated LLVM definitions; omitting it from the entrypoint can leave unresolved externals.
- `api/ui.kn` collides too easily with root `std::ui`; the public builder module is `api/kaintana_ui.kn`.
- Native LLVM currently mislowers imported `impl Self_` builder methods when they touch struct fields (`Field address for .ctx requires a struct or struct pointer`). Keep builders as explicit stage functions until that compiler path is fixed.
- Keep builder structs lightweight. Do not store the full `KaintanaContext` inside every builder stage; pass context to `*_render(ctx, builder)` so SlotMap, arena, and native handles do not get copied through every shim.
- `SlotMapInsert.map.free_head` currently returns stale in this nested native LLVM path. Kaintana normalizes append-only node maps from `count`; remove that shim only after a focused compiler/stdlib proof says nested SlotMap returns preserve `free_head`.
- For desktop reports/screenshots, prefer direct path wrappers such as `kaintana_desktop_host_write_report_path(".kain/run/...")` and create `.kain/run` first. Passing large window spec structs back into host-write wrappers after many UI calls can surface stale String fields.
- Do not put desktop and Vulkan acceptance entrypoints back into one consuming blade unless per-entry manifests become real. A shared manifest/output path can silently reintroduce `vulkain_bridge.dll` into the desktop executable or overwrite the user-facing root exe with the Vulkan proof build.
- `kaintana_split_right(rect, fraction, gap)` uses `fraction` as the left-hand share. If you want a 25% right inspector rail, pass about `0.75`, not `0.25`, or the right panel will silently eat most of the shell.
- `ui_host_session_create(..., "software")` is still passive. It records authored session/draw state; it does not open a live OS window by itself.
- The current Vulkan proof proves host routing into a foreign presenter lane. It does not yet rasterize the full Kaintana scene graph through Vulkan.
- The current Kaintana helpers still emit desktop fill/text bridge calls during composition, so non-desktop consumers must keep `kaintana_desktop_bridge` in scope until those side effects are pushed behind a more renderer-neutral seam.
- Visual popover composition is optional. The host-service proof lives in native UI state plus snapshot/input-trace artifacts; the acceptance shell can keep the popover state open for validation without drawing the overlay into the visible BMP if that overlay hurts readability.
- If the desktop acceptance app appears to crash after many frames, do not guess from the host code first. Re-run with a high `-FrameBudget`, collect `%LOCALAPPDATA%\CrashDumps\kaintana-test.exe.*.dmp` if Windows emits one, and then feed the dump plus `.kain/out/kaintana-test/kaintana-test.ll`, `.kain/run/kaintana_test_*_frame.txt`, and `.kain/run/kaintana_test_*_host.txt` into `tools/crash-forensics/analyze_native_crash.ps1`.
