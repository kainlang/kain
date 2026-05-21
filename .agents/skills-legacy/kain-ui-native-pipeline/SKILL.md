---
name: kain-ui-native-pipeline
description: Use when adding, changing, debugging, validating, or reviewing Kain's native UI host pipeline, especially crates/kain-ui-native, Kain-authored UI runtime bundles/projections, passive native UI substrate boundaries, blade-owned presenters, Qt host generation, and cleanup of Rust-side UI catalogs or placeholder layouts.
---

# Kain UI Native Pipeline

Use this skill in `D:\Kain-Lang` when touching:

- `crates/kain-ui-native/**`
- `crates/kain-ui/**` runtime bundle/projection fields consumed by native hosts
- `runtime/native/include/kain_runtime_ui.h`
- `runtime/native/include/kain_native_ui_system.h`
- `runtime/native/src/ui/**`
- `stdlib/native/ui.kn`
- blade-owned native presenter packages such as `blades/opengl/**`
- Kaintana framework blades such as `blades/kaintana/**` and `blades/kaintana-test/**`

## Ownership

Kain source owns UI structure, layout intent, widgets, panels, text, and app-specific visual flow. Rust/native owns host launch, manifest projection, validation, event/render substrate, ABI shape, diagnostics, and low-level rendering.

Do not add Rust-side UI catalogs, sample dashboards, renderer switchboards, placeholder pane lanes, default widget layouts, or boilerplate button/panel recipes to `kain-ui-native`. If a UI needs buttons, panels, tabs, docs, graphs, shader surfaces, or viewports, author that in Kain and make the host render the authored bundle/projection.

The raw LLVM/native UI ABI is `kain_native_ui_system`, not a widget library. Keep `runtime/native/include/kain_native_ui_system.h`, `runtime/native/src/ui/kain_native_ui_system.c`, and `stdlib/native/ui.kn` generic: sessions, windows, host frame presentation metadata, arbitrary node kind strings, stable node keys, rect/text/style/state/flag mutation, accessibility metadata, font/texture/canvas/shader resource handles, generic resource byte upload, text measurement, clipboard, IME, drag/drop, menus, dialogs, focus, hit testing, dirty counts, hot reload generations, events, and draw commands are acceptable; baked component kinds, catalog defaults, product-specific shell layouts, and named button/panel recipes are not. Generic state cells may carry Kain-authored shape, hit, draw, resource, or app data, but the runtime must not interpret them as a component catalog. Put reusable UI vocabulary in Kain stdlib code above the ABI.

The runtime-owned raw presenter boundary is passive now. `software`, `memory`, and `headless` are runtime substrate labels; live windowed presenters such as Win32/WGL must live in blade-owned packages like `blades/opengl` or future backend packages such as Vulkan/D3D12 blades. Do not reintroduce `win32-gl` or other live graphics presenters into `runtime/native`; the runtime should expose host handles, events, and draw/state ABI, while blades or packages own concrete live presentation.

Compiled-bundle hot reload now has its own native runtime lane. The public surface is `runtime/native/include/kain_ui_runtime.h` plus `runtime/native/include/kain_ui_hot_reload.h`; the implementation is `runtime/native/src/ui/kain_ui_runtime.c` plus `runtime/native/src/ui/kain_ui_hot_reload.c`. Keep that surface cross-platform by API. File-watch reload from `KAIN_NATIVE_UI_BUNDLE` is the portable baseline, and any low-latency shared-memory control ring must stay behind platform backends instead of becoming the architecture.

The Kain-authored stdlib UI layer lives in `stdlib/native/ui.kn` above the raw ABI. Keep it system-shaped: frame/session helpers, stable keyed reconciliation, layout math, style metrics, inherited style resolution, render helpers, resource helpers, and authored event/state helpers are welcome. Avoid turning it into a baked catalog of buttons, panes, app shells, dashboards, or named product components. App code can define those on top.

Two ABI rules matter now:

- `draw_text` must carry an explicit font resource id. Do not hide text rendering behind host-default fonts.
- Texture/image-style uploads should go through the generic resource-byte path. In Kain source prefer the stdlib hex helper (`native_ui_resource_set_bytes_hex` / `native_ui_texture_create_from_hex`) over inventing a widget/image catalog.
- Generic state cells (`native_ui_node_set_state_*`, `native_ui_node_state_*`) are for arbitrary authored per-node payloads and hot-reloadable session data. They are mechanism, not catalog.
- Generic pointer-state flags such as `hovered` and `pressed` are acceptable raw state substrate. Do not use flags to encode component types or theme policy.

`crates/kain-ui-native/src/lib.rs` should stay a public index like `kain-actor/src/lib.rs`. The default non-egui path is intentionally small:

- `app.rs`: app config, backend plan, source-to-runtime-bundle build helpers, launch routing
- `session.rs`: `KainUiNativeSessionManifest`, authored surfaces, backend labels, native projection derivation
- `qt_host.rs`: external Qt runtime discovery, artifact writing, thin generated QML host for authored projection data only
There should be no `src/archive`, `legacy-egui`, `no_egui*` shim modules, or optional egui/wgpu/font/image dependencies in this crate. This language is private; delete dead compatibility code instead of preserving stale host implementations.

## Runtime Overlay Rule

`KainUiCompiledOverlaySpec` diagnostic fields are not fallback UI. Compiled/authored bundle nodes take precedence. If no authored bundle and no diagnostic lines are provided, the overlay should not fabricate a panel/title just to fill space.

Use diagnostic fields only for explicit native runtime diagnostics such as profiling, validation warnings, and controls hints. Do not use them to define app UI.

## Validation

For default UI-native work:

```powershell
cargo fmt -p kain-ui-native
cargo test -p kain-ui-native --target-dir target\codex-kain-ui-native
cargo check -p kain-ui-native --target-dir target\codex-kain-ui-native-check
```

For raw C ABI UI work:

```powershell
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture
bash runtime/conformance/ui_runtime/run_tests.sh --verbose
runtime/conformance/ui_runtime/bin/test_ui_runtime_reload.exe
cargo build -p cli --target-dir target\codex-native-ui-win32
target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_single_file\main.kn --target llvm
target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_runtime_systems\main.kn --target llvm
target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm
target\codex-native-ui-win32\debug\kain.exe build runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm --output target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe
target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe
.\smoketest\native-ui\pilot\run.ps1
```

When the higher-level `kain build` lane is temporarily broken but you still need to validate `runtime/native/src/ui/**`, compile the direct C harness instead of stopping at theory:

```powershell
clang -std=c11 -D_CRT_SECURE_NO_WARNINGS -I runtime\native\include -I runtime\native\src\ui `
  runtime\native\src\ui\z3\fixtures\native_ui_runtime_index_smoke.c `
  runtime\native\src\ui\kain_native_ui_system.c `
  runtime\native\src\ui\kain_native_ui_host_adapter.c `
  -o target\codex-native-ui-runtime-smoke\native_ui_runtime_index_smoke.exe

target\codex-native-ui-runtime-smoke\native_ui_runtime_index_smoke.exe
```

For a live compatibility presenter proof, validate the blade-owned OpenGL lane instead of reattaching GL to the runtime:

```powershell
powershell -ExecutionPolicy Bypass -File .\blades\opengl\run.ps1 -NoRun
$env:OPENGL_BLADE_SCREENSHOT_PATH = "D:\Kain-Lang\blades\opengl\.kain\run\opengl.bmp"
powershell -ExecutionPolicy Bypass -File .\blades\opengl\run.ps1
type .\blades\opengl\.kain\run\opengl_report.txt
```

Before finishing, scan the crate for reintroduced catalog or archive language:

```powershell
rg 'archive|legacy-egui|legacy_egui|no_egui|atrium|PaneCard|document_panes|viewport_panes|fallback_panes|run_demo|run_generic_scene_smoke|KainUiNativeDemoConfig|No authored text|fallback_title|fallback_subtitle|fallback_hint' crates\kain-ui-native runtime\native\include\kain_runtime_ui.h runtime\native\src\ui -n
```

The scan should only hit tests that assert those strings are absent, enum labels shared with `kain-ui`, or nothing at all.

## Common Gotchas

- This repo often has unrelated dirty changes. Stage `kain-ui-native` and UI overlay hunks carefully; do not commit unrelated 3D, actor, LLVM, smoke, or lab changes while doing UI-native work.
- Default backend plans should not imply a document/devtools catalog. Prefer `Auto` until an authored bundle or explicit adapter needs a concrete backend.
- A blank bundle should produce a blank host surface, not synthesized placeholder panes.
- Qt generated QML may provide a thin recursive renderer for authored projection nodes, but it must not invent app copy, renderer modes, lane cards, or sample data.
- The raw UI lane now splits into two proofs: `smoketest/native-ui/pilot` for the passive runtime-owned `software` substrate, and `blades/opengl` for the live Win32/WGL compatibility presenter. Do not collapse those back together by sneaking a live GL path into the runtime.
- `runtime/fixtures/native_ui_stdlib_layer` is the fast headless proof for stdlib helpers. Use it when changing `stdlib/native/ui.kn` so failures are about authored reconciliation/layout/style/event helpers rather than live Win32 presentation.
- `smoketest/native-ui/episode-two` is still the aggressive mixed proof, but it now proves authored UI semantics over the passive runtime substrate. Use blade-owned presenters for live window proofs.
- The old runtime-owned Win32/GL helper surface is now salvage-only content under `blades/opengl/reference/runtime_legacy/`. Treat it as donor code for blade-owned presenters, not as active runtime architecture.
- `blades/kaintana` is the reference framework layer above the passive raw UI ABI, and `blades/kaintana-test` is the acceptance blade that proves desktop plus Vulkan host routing. Use those blades when future work needs a real Kain-authored framework surface rather than another one-off native UI demo.
- Imported local Kain modules that contain `world` / `entangle` currently double-stage those bindings during native LLVM artifact staging. For now, keep Kaintana-style showcase entrypoints self-contained and reserve imported modules for plain helper code.
- `runtime/native/src/ui/kain_native_ui_system.c` now uses power-of-two hash sidecars and occupancy bitsets for nodes, stable keys, styles, state cells, resources, menus, and dialogs. If a change touches create/destroy/lookup semantics, re-run `runtime/native/src/ui/z3/fixtures/native_ui_runtime_index_smoke.c`; it specifically catches style/state cleanup leaks, stable-key index drift, cycle regressions, and legacy host-attach contract regressions.
- `runtime/native/src/ui/kain_ui_hot_reload.c` is not a Windows-only architecture contract. The shared-memory control path can use different platform backends, but the operator-facing rules stay the same: `KAIN_NATIVE_UI_BUNDLE` is the canonical watched bundle path, `KAIN_NATIVE_UI_HOT_RELOAD_CHANNEL` is the optional low-latency control plane name, and `runtime/conformance/ui_runtime/test_ui_runtime_reload.c` is the fastest proof that state-preserving reload still works.
