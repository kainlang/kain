# World Surfaces — Complete Pipeline Reference

**Date:** 2026-06-24
**Based on:** Compiler frontend (ast.rs, parser.rs, types.rs, runtime_contract.rs, realtime_app_bundle.rs), codegen (component.rs), runtime (component_surface.h/c, native_ui_surface.c, vulkan_surface_shim.c, d3d12_surface_shim.c, webgpu_surface_shim.c, renderer_session.c, gpu_surface_extension.h), newsletters (#1-#5).

A **world surface** is the compile-time binding that maps a world to a rendering target. The surface kind determines which codegen path and which runtime backend services the world's frame loop.

---

## 1. Architecture: Source to Pixels

```
Kain source (.kn)
  │  surface <kind> => <expr>
  ▼
parser.rs: parse_world_surface_projection()
  │  WorldSurfaceProjection { kind, expr, span }
  ▼
types.rs: check_world_surface_projection()
  │  Validates expr shape per kind:
  │    NativeUi, Web       → component ident or call
  │    ShaderCanvas        → shader fragment ident
  │    Viewport3d, Ue5     → ident or string literal
  ▼
codegen_llvm/component.rs: compile_surface_frame_loop()
  │  if surface_kind == "shader_canvas"
  │    → compile_shader_surface_loop()     (19-slot vtable + GPU extension)
  │  else
  │    → component render loop             (18-slot vtable)
  ▼
LLVM IR
  │  call %KainComponentSurface* @kain_component_surface_resolve(i8* "native_ui")
  │  call vtable -> session_create, begin_frame, component_render, end_frame, present
  │  call vtable -> host_pump, poll_event, should_close
  ▼
runtime/native: component_surface.c
  │  kain_component_surface_resolve("name")
  │  → registry lookup
  │  → RENDERER_BACKEND env var routing
  ▼
Backend (one of)
  ├─ native_ui_surface.c       (GDI software)
  ├─ vulkan_surface_shim.c     → dlopen libkain-vulkan-abi.so
  ├─ d3d12_surface_shim.c      → dlopen libkain-d3d12-abi.dll
  └─ webgpu_surface_shim.c     → dlopen libkain-webgpu-abi.so
```

---

## 2. Surface Kinds — Full Reference

### 2.1. `native_ui` ✅ Working

| Property | Value |
|----------|-------|
| **AST variant** | `WorldSurfaceKind::NativeUi` |
| **Parser token** | ident `"native_ui"` |
| **Expr shape** | Component identifier (`MyPanel`) or component call (`MyPanel {}`) |
| **Codegen path** | `compile_surface_frame_loop()` → component render loop |
| **Vtable width** | 18 slots |
| **Registry name** | `"native_ui"` |
| **Default backend** | `native_ui_surface.c` (GDI software renderer, retained-mode UI) |
| **GPU routing** | `RENDERER_BACKEND=vulkan|d3d12|webgpu` → GPU backend vtable |
| **Capability key** | `ui.native` |
| **Realtime bundle** | `world.native-ui` requirement |
| **Oracle verified** | Yes (Newsletter #3, 17-line window proof at `blades/window_proof/src/main.kn`) |

**What it does:** Creates a native OS window (Win32 `CreateWindowExA` via winit host adapter) and runs a retained-mode UI component tree through the vtable. The component's `render` JSX is lowered to vtable calls (`element_begin`, `element_set_text`, `element_set_attr_*`, etc.) every frame, then `present` flips the swapchain or blits the GDI surface.

**GPU override:** Set `RENDERER_BACKEND=vulkan` and `native_ui` resolves to the Vulkan ABI backend instead of GDI. The compiler never knows which backend it gets — it always calls through the same `KainComponentSurface` vtable. The `component_surface.c` registry performs the routing transparently.

**Backend list:**
| Backend | Implementation | Build gate |
|---------|---------------|------------|
| GDI (default) | `native_ui_surface.c` | Always compiled |
| Vulkan | `vulkan_surface_shim.c` → `libkain-vulkan-abi.dll` | `KAIN_RUNTIME_HAS_VULKAN_LOADER` |
| D3D12 | `d3d12_surface_shim.c` → `libkain-d3d12-abi.dll` | `KAIN_RUNTIME_HAS_D3D12` |
| WebGPU | `webgpu_surface_shim.c` → `libkain-webgpu-abi.so` | `KAIN_RUNTIME_HAS_WEBGPU` |

---

### 2.2. `shader_canvas` 🟡 Codegen Complete, Blocked on SPIR-V Embedding

| Property | Value |
|----------|-------|
| **AST variant** | `WorldSurfaceKind::ShaderCanvas` |
| **Parser token** | ident `"shader_canvas"` |
| **Expr shape** | Shader fragment identifier (e.g. `OceanFragment`) |
| **Codegen path** | `compile_shader_surface_loop()` → 19-slot vtable + GPU extension |
| **Vtable width** | 19 slots (slot 18 = `get_gpu_extension`) |
| **Registry name** | `"shader_canvas"` |
| **Backend** | GPU only — no GDI/software fallback |
| **Capability key** | `ui.shader-canvas` |
| **Realtime bundle** | `ui.shader-canvas` + `ui.shader-canvas.presented-or-fallback` |
| **Oracle verified** | Not yet (blocked on SPIR-V hex embedding) |

**What it does:** Fullscreen GPU shader rendering — no component tree, no element reconciliation. The codegen emits:

```
init:
  resolve("shader_canvas") → vtable ptr
  session_create → session_attach_platform → window_open
  ext = vtable->get_gpu_extension(sid)         ← slot 18
  ext->load_shader(sid, spirv_hex)             ← embedded at build time

frame loop:
  ext->set_uniform(sid, 0, &time, 4)           ← Float time
  ext->set_uniform(sid, 1, &resolution, 8)      ← Vec2 resolution
  ext->set_uniform(sid, 2, &mouse, 8)           ← Vec2 mouse
  begin_frame → end_frame → present
  host_pump → should_close
```

**The GPU extension struct** (`runtime/native/include/gpu_surface_extension.h`):
```c
typedef struct KainGpuSurfaceExtension {
    int64_t (*load_shader)(int64_t session_id, const char* spirv_hex);
    int64_t (*set_uniform)(int64_t session_id, uint32_t binding,
                            const void* data, uint64_t size);
} KainGpuSurfaceExtension;
```

Non-GPU backends (GDI, software) set `get_gpu_extension` to NULL — the codegen checks for NULL and panics with a clear error. Zero no-op implementations to maintain.

**SPIR-V hex embedding blocker:** The codegen scaffolding has a merge point at `component.rs:386` for reading the SPIR-V hex from `self.shader_spirv_hexes`. The `compile_shader_surface_loop()` function tries to read from this map but it's never populated because the GPU artifact pipeline (`crates/gpu` + `crates/driver`) needs to be wired into the codegen phase. Everything else — parser, typechecker, codegen structure, vtable slot 18, runtime extension, Vulkan ABI `load_shader`/`set_uniform` — is implemented and verified.

**The Vulkan ABI library** (`libkain-vulkan-abi.dll`) already supports:
- `kain_vulkan_abi_load_shader(sid, spirv_hex)` → creates render pass, descriptor set layout, pipeline layout, graphics pipeline with embedded fullscreen-triangle vertex shader, descriptor pool, uniform buffers
- `kain_vulkan_abi_set_uniform(sid, binding, data, size)` → updates uniform buffer before next frame
- Full descriptor set/pipeline infrastructure, swapchain lifecycle, frame submission

**Source example (what you write):**
```kn
world OceanWorld:
    surface shader_canvas => OceanFragment

shader fragment OceanFragment(uv: Vec2) -> Vec4:
    uniform time: Float @0
    uniform resolution: Vec2 @1
    uniform mouse: Vec2 @2
    // ... raymarched ocean shader body ...
```

**Implementation history:** 20-round pair programming session (Newsletter #5), 11 tasks across 11 files, ~260 lines of change. Build-verified with `cargo check` + `bazel build //runtime:native_core_runtime`.

---

### 2.3. `viewport3d` ❌ Parsed, No Codegen

| Property | Value |
|----------|-------|
| **AST variant** | `WorldSurfaceKind::Viewport3d` |
| **Parser token** | ident `"viewport3d"` |
| **Expr shape** | Identifier or string literal |
| **Codegen path** | Falls through to component render path → tries to resolve `"viewport3d"` → nothing registered → PANIC |
| **Registry name** | `"viewport3d"` (not registered) |
| **Capability key** | `gpu.viewport` |
| **Realtime bundle** | `world.viewport3d` requirement |
| **Runtime contract** | Tracked as `world_viewport3d` count |
| **Status** | Parse + typecheck only. No surface registered in `component_surface.c`. |

**Alias candidate:** Conceptually, a 3D viewport IS a shader canvas with camera and scene-graph metadata. Routing `viewport3d` through the `shader_canvas` codegen path (with a different default uniform set) would unblock it with zero new codegen.

---

### 2.4. `web` ❌ Parsed, No Codegen

| Property | Value |
|----------|-------|
| **AST variant** | `WorldSurfaceKind::Web` |
| **Parser token** | ident `"web"` |
| **Expr shape** | Component identifier or call |
| **Codegen path** | Falls through to component render path → tries to resolve `"web"` → nothing registered → PANIC |
| **Registry name** | `"web"` (not registered) |
| **Capability key** | `ui.web` |
| **Realtime bundle** | `world.web` requirement |
| **Status** | Parse + typecheck only. Future: WASM backend. |

---

### 2.5. `ue5` ❌ Parsed, No Codegen

| Property | Value |
|----------|-------|
| **AST variant** | `WorldSurfaceKind::Ue5` |
| **Parser token** | ident `"ue5"` |
| **Expr shape** | Identifier or string literal |
| **Codegen path** | Falls through to component render path → tries to resolve `"ue5"` → nothing registered → PANIC |
| **Registry name** | `"ue5"` (not registered) |
| **Capability key** | `editor.ue5` |
| **Realtime bundle** | `world.ue5` requirement |
| **Status** | Parse + typecheck only. Future: Unreal Engine bridge. |

---

### 2.6. `headless` — Not a WorldSurfaceKind

`headless` is a **renderer backend ID** in `ui_host_adapter.c:332`, not a `WorldSurfaceKind` variant. It routes `abi_ui_host_adapter_attach()` into passive mode (no OS window). Used for CI, benchmarks, server-mode where a world exists but no visual surface is needed.

```c
// ui_host_adapter.c:332
if (strcmp(backend_id, "headless") == 0 ||
    strcmp(backend_id, "memory") == 0 ||
    strcmp(backend_id, "software") == 0) {
    return abi_ui_host_adapter_attach_passive(session, backend_id);
}
```

---

## 3. Vtable Evolution

The `KainComponentSurface` trait in `runtime/native/include/component_surface.h` is the ABI contract between the compiler and every surface backend.

| Version | Slots | Change | Source |
|---------|:-----:|--------|--------|
| **15-slot** | 0-14 | session_create, element lifecycle, state persistence, begin/end frame, present, poll_event, should_close | Newsletter #2 |
| **18-slot** | 0-17 | + `window_open` (15), `host_pump` (16), `session_attach_platform` (17) | Newsletter #3 |
| **19-slot** | 0-18 | + `get_gpu_extension` (18) — returns `KainGpuSurfaceExtension*` or NULL | Newsletter #5 |

### Current 19-Slot Vtable Layout

```
Offset 0:  session_create(name, width, height) → session_id
Offset 1:  session_destroy(session_id)
Offset 2:  element_begin(session_id, parent_id, kind, stable_key) → element_id
Offset 3:  element_end(session_id, element_id)
Offset 4:  element_set_text(session_id, element_id, text)
Offset 5:  element_set_attr_i64(session_id, element_id, key, value)
Offset 6:  element_set_attr_f64(session_id, element_id, key, value)
Offset 7:  element_set_attr_string(session_id, element_id, key, value)
Offset 8:  state_get_i64(session_id, key) → value
Offset 9:  state_set_i64(session_id, key, value)
Offset 10: begin_frame(session_id, delta_ms)
Offset 11: end_frame(session_id)
Offset 12: present(session_id)
Offset 13: poll_event(session_id, out_event, max_size) → event_count
Offset 14: should_close(session_id) → 0|1
Offset 15: window_open(session_id, title, width, height) → status
Offset 16: host_pump(session_id) → status
Offset 17: session_attach_platform(session_id, platform_handle)
Offset 18: get_gpu_extension(session_id) → KainGpuSurfaceExtension* | NULL
```

The codegen constants in `component.rs`:
```rust
const OFF_SESSION_CREATE:          u32 = 0;
const OFF_SESSION_DESTROY:         u32 = 1;
const OFF_ELEMENT_BEGIN:           u32 = 2;
const OFF_ELEMENT_END:             u32 = 3;
const OFF_ELEMENT_SET_TEXT:        u32 = 4;
const OFF_ELEMENT_SET_ATTR_I64:    u32 = 5;
const OFF_ELEMENT_SET_ATTR_F64:    u32 = 6;
const OFF_ELEMENT_SET_ATTR_STRING: u32 = 7;
const OFF_STATE_GET_I64:           u32 = 8;
const OFF_STATE_SET_I64:           u32 = 9;
const OFF_BEGIN_FRAME:             u32 = 10;
const OFF_END_FRAME:               u32 = 11;
const OFF_PRESENT:                 u32 = 12;
const OFF_POLL_EVENT:              u32 = 13;
const OFF_SHOULD_CLOSE:            u32 = 14;
const OFF_WINDOW_OPEN:             u32 = 15;
const OFF_HOST_PUMP:               u32 = 16;
const OFF_SESSION_ATTACH_PLATFORM: u32 = 17;
const OFF_GET_GPU_EXTENSION:       u32 = 18;
```

---

## 4. GPU Backend Routing

The surface registry (`component_surface.c`) uses the `RENDERER_BACKEND` environment variable to transparently swap the vtable behind `"native_ui"`:

```
kain_component_surface_resolve("native_ui")
  │
  ├─ getenv("RENDERER_BACKEND") == NULL
  │   → g_surface_registry["native_ui"]  (GDI native_ui_surface)
  │
  ├─ getenv("RENDERER_BACKEND") == "vulkan"
  │   → resolve_gpu_backend("vulkan")
  │     → vulkan_surface_shim_resolve(&vsurf)
  │       → dlopen("libkain-vulkan-abi.dll")
  │       → dlsym("kain_vulkan_abi_get_vtable")
  │       → vtable filled with 18 function pointers
  │   → overwrites g_surface_registry["native_ui"] with GPU vtable
  │
  ├─ ... same pattern for "d3d12", "webgpu" ...
```

For `"shader_canvas"`, there is NO GDI fallback — the registry returns NULL if no GPU backend is available, and the codegen panics.

---

## 5. The SPIR-V Hex Embedding Blocker

Everything about `shader_canvas` works except one step: embedding the compiled SPIR-V hex string into the LLVM IR global at build time.

**What works:**
- Parser recognizes `surface shader_canvas => FragmentName` ✅
- Typechecker validates the fragment shader exists and has correct signature ✅
- Codegen `compile_shader_surface_loop()` emits the full 19-slot vtable loop including `get_gpu_extension` → `load_shader` → `set_uniform` calls ✅
- Codegen declares the SPIR-V global (`@__kain_spirv_FragmentName`) ✅
- Runtime `KainGpuSurfaceExtension` struct defines `load_shader`/`set_uniform` ✅
- Vulkan ABI library implements `kain_vulkan_abi_load_shader`/`set_uniform` with full pipeline creation ✅
- `component_surface.c` resolves `"shader_canvas"` through GPU backend routing ✅

**What's blocked:**
- `self.shader_spirv_hexes` map in the LLVM codegen is never populated
- The merge point at `component.rs:386` reads from an empty map
- Requires wiring `crates/gpu` (SPIR-V compilation) + `crates/driver` (artifact pipeline) into `crates/sys-codegen` so the compiler can compile the referenced shader fragment to SPIR-V hex during codegen

**What the codegen does right now at component.rs:386:**
```rust
// Embed SPIR-V hex from the global emitted in the codegen preamble
let spirv_ptr = if let Some(hex) = self.shader_spirv_hexes.get(shader_fragment_name) {
    // ... would emit load_shader call with embedded hex ...
} else {
    // TODO: compile shader to SPIR-V hex at build time
    // For now: emit a placeholder
};
```

**What's needed:**
1. Wire `kain-gpu` crate's SPIR-V compiler into `kain-sys-codegen`
2. Before codegen, compile the referenced shader fragment to SPIR-V hex
3. Store the hex in `shader_spirv_hexes` keyed by fragment name
4. Emit the hex as an LLVM global `@__kain_spirv_FragmentName`
5. Pass the global pointer to `load_shader` at frame-loop init

---

## 6. Typechecker Validation Per Kind

From `types.rs:check_world_surface_projection()`:

| Surface Kind | Accepted Expression | Rejected Expression |
|-------------|-------------------|-------------------|
| `NativeUi` | `Expr::Ident` (component name), `Expr::Call` (component call) | Anything else |
| `Web` | `Expr::Ident`, `Expr::Call` | Anything else |
| `ShaderCanvas` | `Expr::Ident` (shader fragment name) | Anything else |
| `Viewport3d` | `Expr::Ident`, `Expr::String` | Anything else |
| `Ue5` | `Expr::Ident`, `Expr::String` | Anything else |

Duplicate surface kinds per world are rejected via `HashSet<WorldSurfaceKind>`.

---

## 7. Runtime Contract & Realtime Bundle

### Runtime contract (`runtime_contract.rs`)

```rust
pub struct RuntimeWorldSurfaceContract {
    pub kind: String,           // "native_ui", "shader_canvas", etc.
    pub authored_expr: String,  // component/shader name or target string
}

pub struct RuntimeWorldContract {
    pub name: String,
    pub state_slots: Vec<RuntimeWorldStateContract>,
    pub surfaces: Vec<RuntimeWorldSurfaceContract>,
}
```

Collected by `collect_world_contracts()` — walks all `TypedItem::World` entries, including module-nested worlds.

### Realtime app bundle (`realtime_app_bundle.rs`)

Each surface kind generates requirements:

| Surface | Requirements |
|---------|-------------|
| `native_ui` | `world.native-ui` |
| `shader_canvas` | `ui.shader-canvas` + `ui.shader-canvas.presented-or-fallback` |
| `viewport3d` | `world.viewport3d` |
| `web` | `world.web` |
| `ue5` | `world.ue5` |

The `bundle_has_world_surface()` helper checks if any world in the program declares a given surface kind. Each world also gets a `world.{name}.{surface_kind}` key in the bundle for per-world surface tracking.

---

## 8. Source Anchors

### Frontend

| File | Lines | What |
|------|-------|------|
| `crates/core/src/ast.rs` | 430-452 | `WorldSurfaceKind` enum (5 variants), `as_str()`, `WorldSurfaceProjection` |
| `crates/core/src/parser.rs` | 2393-2440 | `parse_world_surface_projection()` — matches 5 kind identifiers |
| `crates/core/src/types.rs` | 6677-6735 | `check_world()` — surface collection, duplicate detection, validation |
| `crates/core/src/types.rs` | 6985-7023 | `check_world_surface_projection()` — per-kind expression validation |
| `crates/core/src/runtime_contract.rs` | 220-230 | `RuntimeWorldSurfaceContract`, `RuntimeWorldContract` |
| `crates/core/src/runtime_contract.rs` | 1806 | `world_viewport3d` count tracking |
| `crates/core/src/runtime_contract.rs` | 2642 | Surface capability emission |
| `crates/core/src/realtime_app_bundle.rs` | 2514-2517 | `bundle_has_world_surface()` checks per kind |
| `crates/core/src/realtime_app_bundle.rs` | 2545-2547 | `ui.shader-canvas` requirements |

### Codegen

| File | Lines | What |
|------|-------|------|
| `crates/sys-codegen/src/codegen_llvm/component.rs` | 16-35 | Vtable offset constants (OFF_SESSION_CREATE ... OFF_GET_GPU_EXTENSION) |
| `crates/sys-codegen/src/codegen_llvm/component.rs` | 237-390 | `compile_shader_surface_loop()` — 19-slot GPU shader loop |
| `crates/sys-codegen/src/codegen_llvm/component.rs` | 584-700 | `compile_surface_frame_loop()` — 18-slot component render loop |

### Runtime

| File | What |
|------|------|
| `runtime/native/include/component_surface.h` | `KainComponentSurface` vtable (19 slots), `KainPlatformSurfaceHandle` |
| `runtime/native/include/gpu_surface_extension.h` | `KainGpuSurfaceExtension` struct (load_shader, set_uniform) |
| `runtime/native/src/core/component_surface.c` | Surface registry, GPU backend routing via `RENDERER_BACKEND` |
| `runtime/native/src/ui/native_ui_surface.c` | GDI software backend registered as `"native_ui"` |
| `runtime/native/src/core/vulkan_surface_shim.c` | Vulkan ABI shim — dlopen, capability probe, vtable fill |
| `runtime/native/src/core/d3d12_surface_shim.c` | D3D12 shim — NULL `get_gpu_extension` stub |
| `runtime/native/src/core/webgpu_surface_shim.c` | WebGPU shim — NULL `get_gpu_extension` stub |
| `runtime/native/src/core/renderer_session.c` | GPU backend routing dispatch logic |
| `runtime/native/src/ui/ui_host_adapter.c` | `headless` backend_id at line 332 |

### Newsletters

| File | Topic |
|------|-------|
| `blades/NEWSLETTER-2.md` | 15-slot KainComponentSurface trait, component surface design |
| `blades/NEWSLETTER-3.md` | Multi-backend GPU presenter, 18-slot vtable, Oracle-verified window |
| `blades/NEWSLETTER-4.md` | Vulkan rendering pipeline, surface shader proposal |
| `blades/NEWSLETTER-5.md` | `shader_canvas` implemented, 19-slot vtable, SPIR-V hex embedding deferred |

---

## 9. Quick Reference

| Question | Answer |
|----------|--------|
| How many surface kinds exist? | 5: `native_ui`, `shader_canvas`, `viewport3d`, `web`, `ue5` |
| Which are working? | `native_ui` (GDI + GPU backends). `shader_canvas` is codegen-complete but blocked on SPIR-V embedding. |
| What happens if no surface is declared? | World is a pure state authority — no frame loop emitted, no window created. Valid. |
| How does GPU routing work? | `RENDERER_BACKEND` env var → `component_surface.c` transparently swaps vtables |
| How many vtable slots? | 19 (offset 0-18). Slot 18 is `get_gpu_extension` for shader surfaces. |
| What blocks `shader_canvas`? | SPIR-V hex embedding at build time — codegen map at `component.rs:386` needs to be wired to `crates/gpu`. |
| Is `headless` a surface kind? | No. It's a backend ID in `ui_host_adapter.c`, not a `WorldSurfaceKind`. |
| Can worlds have duplicate surfaces? | No — typechecker rejects duplicate surface kinds per world. |
