# Kain Component System — Vtable ABI Contract & Wiring

**Date:** 2026-07-05 · **Status:** Living document · **Scope:** The full 24-slot `KainComponentSurface` vtable contract, codegen wiring, runtime backends, and GPU extension discovery path.

---

## Table of Contents

1. [The 24-Slot Vtable Contract](#1-the-24-slot-vtable-contract)
2. [Codegen Slot Constants](#2-codegen-slot-constants)
3. [Kaintana's Vtable Implementation](#3-kaintanas-vtable-implementation)
4. [The Old KUIF `native_ui` Surface](#4-the-old-kuif-native_ui-surface)
5. [Slot 18 — GPU Extension Discovery](#5-slot-18--gpu-extension-discovery)
6. [The `component_calls.tsv` Catalog](#6-the-component_callstsv-catalog)

---

## 1. The 24-Slot Vtable Contract

### 1.1 Source

**File:** `X:\runtime\native\include\component_surface.h`

### 1.2 Design Philosophy

The `KainComponentSurface` is an **abstract rendering trait** — the ABI contract between the Kain compiler (which emits vtable calls) and any surface backend (which implements them). Neither side knows the other's internals. The compiler resolves the surface once at frame-loop init, then calls through the vtable every frame.

```
Kain Source  →  Compiler (LLVM IR)  →  KainComponentSurface vtable  →  Backend (native_ui / kaintana / vulkan / d3d12 / webgpu)
```

### 1.3 The Struct Definition

```c
// component_surface.h:48-128
typedef struct KainComponentSurface {
    // ── Session lifecycle (slots 0-1) ───────────────────────────
    int64_t (*session_create) (const char* name, int64_t width, int64_t height);
    void    (*session_destroy)(int64_t session_id);

    // ── Element tree (slots 2-4) ─────────────────────────────────
    int64_t (*element_begin)  (int64_t session_id, int64_t parent_id,
                               const char* kind, const char* stable_key);
    void    (*element_end)    (int64_t session_id, int64_t element_id);
    void    (*element_set_text)(int64_t session_id, int64_t element_id,
                                const char* text);

    // ── Style/attribute setters (slots 5-7) ─────────────────────
    void    (*element_set_attr_i64)   (int64_t session_id, int64_t element_id,
                                       const char* key, int64_t value);
    void    (*element_set_attr_f64)   (int64_t session_id, int64_t element_id,
                                       const char* key, double value);
    void    (*element_set_attr_string)(int64_t session_id, int64_t element_id,
                                       const char* key, const char* value);

    // ── State persistence (slots 8-9) ────────────────────────────
    int64_t (*state_get_i64)(int64_t session_id, const char* key);
    void    (*state_set_i64)(int64_t session_id, const char* key, int64_t value);

    // ── Frame lifecycle (slots 10-12) ────────────────────────────
    void    (*begin_frame)(int64_t session_id, double delta_ms);
    void    (*end_frame)  (int64_t session_id);
    void    (*present)    (int64_t session_id);

    // ── Event pump (slots 13-14) ─────────────────────────────────
    int64_t (*poll_event)  (int64_t session_id, void* out_event, int64_t max_size);
    int64_t (*should_close)(int64_t session_id);

    // ── Window lifecycle (slots 15-16) ───────────────────────────
    int64_t (*window_open)(int64_t session_id, const char* title,
                           int64_t width, int64_t height);
    int64_t (*host_pump)  (int64_t session_id);

    // ── Platform handle attachment (slot 17) ─────────────────────
    void    (*session_attach_platform)(int64_t session_id, void* platform_handle);

    // ── GPU extension discovery (slot 18) ────────────────────────
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t session_id);

    // ── Expanded state persistence (slots 19-22) ─────────────────
    double      (*state_get_f64)(int64_t session_id, const char* key);
    void        (*state_set_f64)(int64_t session_id, const char* key, double value);
    const char* (*state_get_string)(int64_t session_id, const char* key);
    void        (*state_set_string)(int64_t session_id, const char* key, const char* value);

    // ── Event callback binding (slot 23) ─────────────────────────
    void    (*element_set_callback)(int64_t session_id, int64_t element_id,
                                    const char* event_name, void* callback_fn);
} KainComponentSurface;
```

**Vtable size:** 24 slots × 8 bytes = 192 bytes on x64.

### 1.4 Complete Slot Index

| Slot | Function | Purpose |
|:----:|----------|---------|
| 0 | `session_create` | Create a rendering session (name, width, height → session_id) |
| 1 | `session_destroy` | Destroy a session at shutdown |
| 2 | `element_begin` | Begin a new element (kind, stable_key → element_id) |
| 3 | `element_end` | Close an element (children complete) |
| 4 | `element_set_text` | Set text content on an element |
| 5 | `element_set_attr_i64` | Set integer attribute (disabled, checked, direction, etc.) |
| 6 | `element_set_attr_f64` | Set float attribute (padding, opacity, width, etc.) |
| 7 | `element_set_attr_string` | Set string attribute (color, title, font_family, etc.) |
| 8 | `state_get_i64` | Read persisted i64 component state |
| 9 | `state_set_i64` | Write persisted i64 component state |
| 10 | `begin_frame` | Start a new frame (receives delta_ms) |
| 11 | `end_frame` | Finish frame rendering |
| 12 | `present` | Present rendered frame to screen |
| 13 | `poll_event` | Poll for input events (opaque buffer) |
| 14 | `should_close` | Check if window should close |
| 15 | `window_open` | Flag session as open with title/dimensions |
| 16 | `host_pump` | Process OS message queue (Win32: PeekMessage/TranslateMessage/DispatchMessage) |
| 17 | `session_attach_platform` | Attach platform window handle (HWND, X11 Window, etc.) |
| 18 | `get_gpu_extension` | Return `KainGpuSurfaceExtension*` or NULL for software backends |
| 19 | `state_get_f64` | Read persisted f64 component state |
| 20 | `state_set_f64` | Write persisted f64 component state |
| 21 | `state_get_string` | Read persisted string component state |
| 22 | `state_set_string` | Write persisted string component state |
| 23 | `element_set_callback` | Register event callback on an element |

### 1.5 Registry Mechanism

**File:** `X:\runtime\native\src\core\component_surface.c`

```c
// component_surface.c:101-124
void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface) {
    if (!name || !surface) return;
    // Check for duplicates — silently overwrite
    for (int i = 0; i < g_surface_count; i++) {
        if (strcmp(g_surface_registry[i].name, name) == 0) {
            g_surface_registry[i].surface = surface;
            return;
        }
    }
    if (g_surface_count >= KAIN_MAX_SURFACES) return; // 16 max
    g_surface_registry[g_surface_count].name    = name;
    g_surface_registry[g_surface_count].surface = surface;
    g_surface_count++;
}

// component_surface.c:126-182
const KainComponentSurface* kain_component_surface_resolve(const char* name) {
    if (!name) return NULL;

    // GPU backend routing: when codegen asks for "native_ui", check
    // RENDERER_BACKEND env var for GPU override
    if (strcmp(name, "native_ui") == 0) {
        const char* backend = getenv("RENDERER_BACKEND");
        if (backend && backend[0]) {
            const KainComponentSurface* gpu_surface = resolve_gpu_backend(backend);
            if (gpu_surface) {
                kain_component_surface_register("native_ui", gpu_surface);
                return gpu_surface;
            }
        }
    }

    // "shader_canvas" requires GPU — no GDI fallback
    if (strcmp(name, "shader_canvas") == 0) {
        const char* backend = getenv("RENDERER_BACKEND");
        if (backend && backend[0]) {
            const KainComponentSurface* gpu_surface = resolve_gpu_backend(backend);
            if (gpu_surface) {
                kain_component_surface_register("shader_canvas", gpu_surface);
                return gpu_surface;
            }
        }
        return NULL; // Codegen will panic
    }

    // Normal registry lookup
    for (int i = 0; i < g_surface_count; i++)
        if (strcmp(g_surface_registry[i].name, name) == 0)
            return g_surface_registry[i].surface;
    return NULL;
}
```

### 1.6 Pre-Registered Surfaces

| Surface Name | Backend | Registration Point | GPU? |
|-------------|---------|-------------------|------|
| `"native_ui"` | Old KUIF GDI | `native_ui_surface.c` static ctor (`.CRT$XCU` on Win32 / `__attribute__((constructor))`) | No |
| `"kaintana"` | New Kaintana engine | `kt_init()` → `kain_component_surface_register(KAINTANA_SURFACE_NAME, &kaintana_vtable)` | No |
| `"vulkan"` | Vulkan ABI | `vulkan_surface_shim.c` → dlopen `libkain-vulkan-abi.so` | **Yes** |
| `"d3d12"` | D3D12 ABI | `d3d12_surface_shim.c` → dlopen `libkain-d3d12-abi.dll` | **Yes** |
| `"webgpu"` | WebGPU ABI | `webgpu_surface_shim.c` → dlopen `libkain-webgpu-abi.so` | **Yes** |
| `"shader_canvas"` | GPU-routed (no fallback) | Resolved via `component_surface.c:154-172` routing logic | **Yes** |

---

## 2. Codegen Slot Constants

### 2.1 Source

**File:** `X:\crates\sys-codegen\src\codegen_llvm\component.rs`, lines 14–45

### 2.2 Constant Definitions

```rust
// component.rs:14-45
// ── Vtable offset constants — must match KainComponentSurface field order ────
// See: runtime/native/include/component_surface.h
const OFF_SESSION_CREATE: u32 = 0;
const OFF_SESSION_DESTROY: u32 = 1;
const OFF_ELEMENT_BEGIN: u32 = 2;
const OFF_ELEMENT_END: u32 = 3;
const OFF_ELEMENT_SET_TEXT: u32 = 4;
const OFF_ELEMENT_SET_ATTR_I64: u32 = 5;
const OFF_ELEMENT_SET_ATTR_F64: u32 = 6;
const OFF_ELEMENT_SET_ATTR_STRING: u32 = 7;
const OFF_STATE_GET_I64: u32 = 8;
const OFF_STATE_SET_I64: u32 = 9;
const OFF_BEGIN_FRAME: u32 = 10;
const OFF_END_FRAME: u32 = 11;
const OFF_PRESENT: u32 = 12;
const OFF_POLL_EVENT: u32 = 13;
const OFF_SHOULD_CLOSE: u32 = 14;
const OFF_WINDOW_OPEN: u32 = 15;
const OFF_HOST_PUMP: u32 = 16;
const OFF_SESSION_ATTACH_PLATFORM: u32 = 17;
/// Slot 18: get_gpu_extension - returns KainGpuSurfaceExtension* or NULL
pub(crate) const OFF_GET_GPU_EXTENSION: u32 = 18;
/// Slot 19: state_get_f64
pub(crate) const OFF_STATE_GET_F64: u32 = 19;
/// Slot 20: state_set_f64
pub(crate) const OFF_STATE_SET_F64: u32 = 20;
/// Slot 21: state_get_string
pub(crate) const OFF_STATE_GET_STRING: u32 = 21;
/// Slot 22: state_set_string
pub(crate) const OFF_STATE_SET_STRING: u32 = 22;
/// Slot 23: element_set_callback
pub(crate) const OFF_ELEMENT_SET_CALLBACK: u32 = 23;
```

### 2.3 1:1 Mapping to C Struct

The Rust constants map exactly to the C struct field order. The compiler uses `getelementptr` to address vtable slots:

```llvm
; component_calls.tsv VTABLE_CALL entries 1-4
%gep = getelementptr inbounds %KainComponentSurface, %KainComponentSurface* %surf, i32 0, i32 %offset
%cast = bitcast i8** %gep to %fn_ptr_ptr_ty
%fn = load %fn_ptr_ty, %fn_ptr_ptr_ty* %cast
%result = call %ret_ty %fn(%args)
```

The vtable struct is declared as _24 uniform `i8*` slots_ in LLVM IR, then each slot is **bitcast** to the correct function pointer type before loading and calling:

```llvm
; component.rs:158
%KainComponentSurface = type { i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8* }
```

This avoids needing 24 distinct function pointer types in the struct declaration. The actual typed `bitcast` happens per call site.

### 2.4 Two Call Paths

The codegen provides two vtable call helpers:

| Helper | Returns | Used For |
|--------|---------|---------|
| `emit_vtable_call(surface, offset, fn_ptr_ty, args)` | `result_reg` | Slots that return a value (create, begin, state_get, should_close, etc.) |
| `emit_vtable_call_void(surface, offset, fn_ptr_ty, args)` | nothing | Fire-and-forget slots (set_text, set_attr, set_state, end_frame, present, etc.) |

---

## 3. Kaintana's Vtable Implementation

### 3.1 Source

**File:** `X:\runtime\native\src\ui_v2\tree.c`

### 3.2 Registration

```c
// tree.c:125-129
void kt_init(void) {
    version_check_abi_compatibility((unsigned int)KT_API_VERSION);
    kain_component_surface_register(KAINTANA_SURFACE_NAME, &kaintana_vtable);
    // P0-18: ui.kaintana service registration when services.h is wired
}
```

Where `KAINTANA_SURFACE_NAME` is defined as `"kaintana"` in `kaintana.h:101`:

```c
#define KAINTANA_SURFACE_NAME          "kaintana"
```

### 3.3 Type Alias

Kaintana's vtable uses a direct typedef alias — same layout, zero add-ons:

```c
// kaintana.h:277
typedef KainComponentSurface KaintanaComponentSurface;

// kaintana.h:941 — compile-time verification
KT_STATIC_ASSERT(sizeof(KaintanaComponentSurface) == sizeof(KainComponentSurface),
    "KaintanaComponentSurface must match KainComponentSurface exactly");
```

### 3.4 Vtable Singleton

```c
// tree.c:104-120
static const KaintanaComponentSurface kaintana_vtable = {
    .session_create=v_session_create, .session_destroy=v_session_destroy,
    .element_begin=v_element_begin, .element_end=v_element_end,
    .element_set_text=v_element_set_text,
    .element_set_attr_i64=v_element_set_attr_i64,
    .element_set_attr_f64=v_element_set_attr_f64,
    .element_set_attr_string=v_element_set_attr_string,
    .state_get_i64=v_state_get_i64, .state_set_i64=v_state_set_i64,
    .begin_frame=v_begin_frame, .end_frame=v_end_frame, .present=v_present,
    .poll_event=v_poll_event, .should_close=v_should_close,
    .window_open=v_window_open, .host_pump=v_host_pump,
    .session_attach_platform=v_session_attach_platform,
    .get_gpu_extension=v_get_gpu_extension,
    .state_get_f64=v_state_get_f64, .state_set_f64=v_state_set_f64,
    .state_get_string=v_state_get_string, .state_set_string=v_state_set_string,
    .element_set_callback=v_element_set_callback,
};
```

### 3.5 What Each Vtable Function Does

**Session Lifecycle (slots 0-1):**
```c
// tree.c:213-215 — stubs in the vtable layer; real session lifecycle
// is handled by kt_make() / kt_free() which call the vtable internally
static int64_t v_session_create(...) { return 1; }
static void v_session_destroy(int64_t id) { (void)id; }
```
The real session creation happens in `kt_make()` which allocates the `kt_Session_t` struct (arena, nodes, layouts, hashes, stacks, draw batches, input state) and then internally calls `sess->vtable->session_create(name, w, h)` to wire the vtable's session ID.

**Element Tree (slots 2-4):**
```c
// tree.c:437-440 — all stubs; element tree is managed internally
// by kt_row() / kt_end_row() / kt_text()
static int64_t v_element_begin(...) { return 0; }
static void v_element_end(...) { }
static void v_element_set_text(...) { }
```
Kaintana's element tree is managed _through its own header-included API_ (`kt_row`, `kt_end_row`, `kt_text`), not through the vtable. The vtable stubs exist because the `KainComponentSurface` trait requires them. This means **the component codegen path does NOT actually call through the Kaintana vtable for element creation** — it calls `kt_row()` directly instead.

**Style/Attribute Setters (slots 5-7):**
```c
// tree.c:471-608
static void v_element_set_attr_i64(int64_t sid, int64_t e, const char* k, int64_t v) {
    // Resolves session by sid, looks up attr in the attribute table,
    // writes directly to KaintanaLayout fields (direction, justify, align)
    // or node flags (visibility, interactive).
}
static void v_element_set_attr_f64(int64_t sid, int64_t e, const char* k, double v) {
    // Maps layout.* keys to KaintanaLayout fields:
    // layout.flex_grow, flex_shrink, flex_basis, pad, pad_x, pad_y,
    // margin, margin_*, min_width, max_width, min_height, max_height,
    // width, height, opacity, radius, stroke_width
}
static void v_element_set_attr_string(int64_t sid, int64_t e, const char* k, const char* v) {
    // Parses fill/stroke color strings via kt_color_parse_hex()
    // and stores the uint32_t in KaintanaLayout fields.
}
```

**State Persistence (slots 8-9, 19-22):**
```c
// tree.c:656-679
static int64_t v_state_get_i64(int64_t sid, const char* k) {
    kt_Session* s = session_by_sid(sid);
    return s ? kt_get(s, k, 0) : 0;
}
static void v_state_set_i64(int64_t sid, const char* k, int64_t v) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_put(s, k, v);
}
// ... similar for f64 and string variants
```
State is stored in a flat array of `KaintanaStateEntry` records (max `KAINTANA_STATE_ENTRIES`) keyed by FNV-1a hash. Types: `i64` (type=0), `f64` (type=1), `string` (type=2). Each entry stores a key (char[64]) and a union of { i64_val, f64_val, str_val[256] }.

**Frame Lifecycle (slots 10-12):**
```c
// tree.c:316-319 — all stubs; real begin/end/present handled by
// kt_begin() / kt_end() / kt_present() which call the vtable internally
static void v_begin_frame(int64_t sid, double d) { (void)sid;(void)d; }
static void v_end_frame(int64_t sid) { (void)sid; }
static void v_present(int64_t sid) { (void)sid; }
```
The real frame pipeline in `kt_begin()`: reset node tree links → advance frame counter → clear text input → process DPI scale changes → reset nesting tracker → clear damage → mark arena → call vtable.begin_frame. Then `kt_end()`: process damage → layout pass 1 → layout pass 2 → hit test → draw generate → draw merge → call vtable.end_frame → release arena. `kt_present()` calls vtable.present then the backend's `render()` callback.

**Other Slots (13-17, 23):**
```c
// tree.c:93-101 — all stubs
static int64_t v_window_open(...)  { return 0; }
static int64_t v_host_pump(...)    { return 0; }
static void v_session_attach_platform(...) { }
static int64_t v_poll_event(...)   { return 0; }
static int64_t v_should_close(...) { return 0; }
static void v_element_set_callback(...) { }
```
**Slot 18 — GPU extension:**
```c
// tree.c:98-99
static const KainGpuSurfaceExtension* v_get_gpu_extension(int64_t sid) {
    (void)sid; return NULL; // Kaintana is software-only
}
```

### 3.6 Architecture Note: Internal vs Vtable API

Kaintana exposes _two_ API surfaces:
1. **Header-included C API** (`kt_row`, `kt_begin`, `kt_end`, `kt_present`, etc.) — called by the host application or platform layer
2. **Vtable stubs** — satisfy the `KainComponentSurface` trait, but mostly delegate back to the internal state

The vtable stubs don't "do the real work" — they act as a bridge. The real pipeline (layout, draw, hit-test) lives in `kt_begin()`/`kt_end()`/`kt_present()` which are called _through the header-included API_, not through the vtable. The vtable exists so that Kaintana _can be swapped in_ as a `KainComponentSurface` backend when codegen resolves `"kaintana"`.

---

## 4. The Old KUIF `native_ui` Surface

### 4.1 Source

**File:** `X:\runtime\native\src\ui\native_ui_surface.c`

### 4.2 Architecture

The `native_ui_surface` wraps every `abi_ui_*` function through the `KainComponentSurface` trait vtable. The compiler never calls `abi_ui_*` directly — it calls through `KainComponentSurface`, making the UI backend swappable.

```c
// native_ui_surface.c:1-21
// Implements the KainComponentSurface trait using the existing ui_system.h
// retained-mode UI runtime.
//
// Key design decisions:
//   - element_begin: find-or-create via stable key for reconciliation
//   - element_end:   no-op (ui_system retains nodes across frames)
//   - State:         persisted on a hidden "__kain_state_root" node
//   - Style keys:    mapped from abstract keys to ui_system operations
```

### 4.3 Vtable Registration

```c
// native_ui_surface.c:389-414
const KainComponentSurface native_ui_surface = {
    .session_create          = native_ui_session_create,
    .session_destroy         = wrap_session_destroy,
    .element_begin           = native_ui_element_begin,
    .element_end             = native_ui_element_end,
    .element_set_text        = wrap_element_set_text,
    .element_set_attr_i64    = wrap_set_attr_i64,
    .element_set_attr_f64    = wrap_set_attr_f64,
    .element_set_attr_string = native_ui_set_attr_string,
    .state_get_i64           = native_ui_state_get_i64,
    .state_set_i64           = native_ui_state_set_i64,
    .begin_frame             = wrap_begin_frame,
    .end_frame               = wrap_end_frame,
    .present                 = native_ui_present,
    .poll_event              = wrap_poll_event,
    .should_close            = abi_ui_host_should_close,
    .window_open             = abi_ui_window_open,
    .host_pump               = abi_ui_host_pump,
    .session_attach_platform = native_ui_session_attach_platform,
    .get_gpu_extension       = native_ui_get_gpu_extension, // returns NULL
    .state_get_f64           = native_ui_state_get_f64,
    .state_set_f64           = native_ui_state_set_f64,
    .state_get_string        = native_ui_state_get_string,
    .state_set_string        = native_ui_state_set_string,
    .element_set_callback    = native_ui_element_set_callback,
};
```

Auto-registration via static initializer (runs before `main()`):

```c
// native_ui_surface.c:422-438
#if defined(_WIN32)
#pragma section(".CRT$XCU", read)
static void native_ui_surface_register_ctor(void) {
    kain_component_surface_register("native_ui", &native_ui_surface);
}
__declspec(allocate(".CRT$XCU"))
    void (*native_ui_surface_register_ptr)(void) = native_ui_surface_register_ctor;
#else
__attribute__((constructor))
static void native_ui_surface_register_ctor(void) {
    kain_component_surface_register("native_ui", &native_ui_surface);
}
#endif
```

### 4.4 Key Behavioral Differences vs Kaintana

| Aspect | Old KUIF (`native_ui`) | Kaintana |
|--------|----------------------|----------|
| **Element reconciliation** | `native_ui_element_begin` does find-or-create via `abi_ui_node_find_by_stable_key()` then `abi_ui_node_set_parent()` | `kt_row()` does hash-lookup via FNV-1a on stable key, then node reuse or allocation |
| **`element_end`** | True no-op — nodes persist in retained tree until `ui_system` destroys them | Also a no-op at vtable level; nesting reset handled in `kt_begin()` |
| **State storage** | Hidden `"__kain_state_root"` node with `abi_ui_node_set_state_i64/f64/string` storing per-field values | Flat array of `KaintanaStateEntry` records with FNV-1a key hashing |
| **Style attr dispatch** | Known string keys (fill_color, border_color, ink_color, title) routed via `abi_ui_node_set_style_string`; unknowns silently ignored | Full attribute table with `kaintana__attr_lookup()`; writes directly to `KaintanaLayout` fields or node flags |
| **`present`** | `abi_ui_present()` (counter) + `abi_ui_host_present()` (InvalidateRect → WM_PAINT → BitBlt GDI blit) | vtable.present (stub) + backend `render()` callback (walks internal draw command list) |
| **GPU extension** | Always returns NULL | Always returns NULL |
| **Callback binding** | `abi_ui_node_set_callback()` for `event_name` + `callback_fn` | Stub — not implemented |
| **`session_attach_platform`** | No-op (host adapter already owns the HWND) | No-op |
| **Font loading** | Loads platform TTF at session_create (segoeui.ttf on Win32, DejaVuSans.ttf on Linux, Helvetica.ttc on macOS) via `abi_ui_font_load_ttf()` | Not applicable — Kaintana has its own text rendering via `text.cpp` |

### 4.5 BUG-026 Root Cause: Why `native_ui` Produces Blank Windows

The root cause is a **render path mismatch**. When codegen emits the `native_ui` frame loop:

1. The codegen emits element_create/set_attr/set_text calls through the `KainComponentSurface` vtable
2. The `native_ui_surface` wraps these to `abi_ui_node_create`, `abi_ui_node_set_style_*`, etc.
3. **But** the `ui_system.h` retained-mode renderer (`renderer_backend.c` → GDI path) has its own render loop that walks the node tree _independently of the vtable calls_
4. The GDI renderer renders what it sees in the `ui_system` tree — but the tree is populated _through the vtable_, and the renderer may not see those nodes in time or may be using a different pass ordering

The `native_ui_present()` function does `abi_ui_present()` + `abi_ui_host_present()` — the `abi_ui_host_present` calls `InvalidateRect` → `WM_PAINT` → `BitBlt`. The `WM_PAINT` handler renders the GDI framebuffer. **If the tree walk hasn't been triggered** or the renderer's dirty tracking doesn't align with the vtable's element lifecycle, the framebuffer stays blank.

In contrast, Kaintana's `kt_present()` directly walks the internal draw command list (`draw_batch`) and calls `backend->render(&draw_data)` — no GDI paint-cycle dependency.

---

## 5. Slot 18 — GPU Extension Discovery

### 5.1 The Extension Struct

**File:** `X:\runtime\native\include\gpu_surface_extension.h`

```c
// gpu_surface_extension.h:6-21
typedef struct KainGpuSurfaceExtension {
    /// Load a fragment shader from hex-encoded SPIR-V.
    /// Creates render pass, descriptor set layout, pipeline layout,
    /// graphics pipeline (with embedded fullscreen-triangle VS),
    /// descriptor pool, uniform buffers, and descriptor writes.
    /// Returns 0 on success, negative on error.
    int64_t (*load_shader)(int64_t session_id, const char* spirv_hex);

    /// Update a uniform buffer binding before the next frame.
    /// binding: 0=time (Float, 4 bytes), 1=resolution (Vec2, 8 bytes), 2=mouse (Vec2, 8 bytes)
    /// data: pointer to the raw bytes
    /// size: byte count
    /// Returns 0 on success, negative on error.
    int64_t (*set_uniform)(int64_t session_id, uint32_t binding,
                            const void* data, uint64_t size);
} KainGpuSurfaceExtension;
```

### 5.2 How the Codegen Probes Slot 18

**File:** `X:\crates\sys-codegen\src\codegen_llvm\component.rs`, lines 526–543

```rust
// component.rs:526-543
// ── Probe vtable slot 18: get_gpu_extension ──────────────
let ext_reg = self.emit_vtable_call(
    &surface_reg,
    OFF_GET_GPU_EXTENSION,      // slot 18
    "i8* (i64)*",
    &[(&session_id, "i64")],
);

let has_gpu = self.next_reg();
self.emit(&format!(
    "  {} = icmp ne i8* {}, null",
    has_gpu, ext_reg
));
let gpu_init_block = self.next_label();
let component_init_block = self.next_label();
self.emit(&format!(
    "  br i1 {}, label %{}, label %{}",
    has_gpu, gpu_init_block, component_init_block
));
```

This generates LLVM IR like:

```llvm
%ext_ptr = call i8* %get_gpu_extension_fn(i64 %session_id)
%has_gpu = icmp ne i8* %ext_ptr, null
br i1 %has_gpu, label %gpu_init_block, label %component_init_block
```

The LLVM LTO pass **constant-folds** this branch at `-O2` because the vtable is a link-time constant global. If the backend's `get_gpu_extension` is known to always return NULL, the entire GPU path is dead-code-eliminated. Zero runtime cost.

### 5.3 GPU vs Component Render Path Split

When `get_gpu_extension` returns non-NULL:

```
GPU Path (gpu_init_block):
  bitcast extension → KainGpuSurfaceExtension*
  load_shader(spirv_hex)         // compile SPIR-V at session init
  ┌─ GPU frame loop ────────────┐
  │ host_pump()                 │
  │ begin_frame(delta_ms)       │
  │ set_uniform(0, time_addr)   │
  │ set_uniform(1, res_addr)    │
  │ set_uniform(2, mouse_addr)  │
  │ end_frame()                 │
  │ present()                   │
  │ time += delta_ms / 1000.0   │
  │ should_close? → loop/exit   │
  └─────────────────────────────┘
```

When `get_gpu_extension` returns NULL:

```
Component Path (component_init_block):
  Register pulse/resonate handlers
  ┌─ Component frame loop ──────┐
  │ host_pump()                 │
  │ begin_frame(delta_ms)       │
  │ ComponentName_render()      │ ← recurses into JSX tree
  │ end_frame()                 │
  │ present()                   │
  │ should_close? → loop/exit   │
  └─────────────────────────────┘
```

### 5.4 Backend Behavior by Surface

| Surface | `get_gpu_extension` | Path Taken |
|---------|-------------------|------------|
| `native_ui` | Returns NULL | Component render path |
| `kaintana` | Returns NULL | Component render path |
| `vulkan` | Returns `KainGpuSurfaceExtension*` | GPU shader path |
| `d3d12` | Returns `KainGpuSurfaceExtension*` | GPU shader path |
| `webgpu` | Returns `KainGpuSurfaceExtension*` | GPU shader path |
| `shader_canvas` | Routed via `RENDERER_BACKEND` env var, returns GPU* | GPU shader path (no fallback; NULL → panic) |

### 5.5 Compile-Time SPIR-V Gate

The codegen provides `surface_needs_shader_compilation()` for compile-time shader compilation decisions:

```rust
// component.rs:132-136
pub(crate) fn surface_needs_shader_compilation(surface_kind: &str) -> bool {
    // Known shader-capable surface kinds.
    matches!(surface_kind, "shader_canvas")
}
```

This is a compile-time check — slot 18 (`get_gpu_extension`) doesn't exist at compile time. Extend this list when new GPU-capable surface kinds are registered.

---

## 6. The `component_calls.tsv` Catalog

### 6.1 Source

**File:** `X:\crates\sys-codegen\src\codegen_llvm\chunk-10-component_calls.tsv` — 231 lines

### 6.2 Structure

The TSV has eight columns: `section`, `id`, `function_name`, `llvm_call`, `kaintana_target`, `description`, `hooks`, `notes`.

### 6.3 Section Breakdown (231 entries, ~228 documented calls)

| Section | Count | What It Covers |
|---------|:-----:|---------------|
| `TYPE_DECLARATION` | 6 | LLVM type declarations: `%KainComponentSurface` (24 i8*), `%KainGpuSurfaceExtension` (2 i8*), `kain_component_surface_resolve`, `kain_runtime_panic`, `__kain_frame_delta_ms`, `%KainComponentCallback` |
| `VTABLE_CONSTANT` | 24 | The 24 `OFF_*` constants documenting every vtable slot |
| `ATTRIBUTE_SET` | 46 | JSX attribute → vtable slot mappings (f64 attrs 1-13, string attrs 14-27, i64 attrs 28-33, value/text attrs 34-35, plus explicit value paths 36-46) |
| `STATE_ACCESS` | 27 | State initialization: sentinel detection (NaN/null/-1), alloca/load/store, PHI merge, type coercion (sitofp), write-back (load + set) |
| `ELEMENT_TREE` | 13 | `element_begin`, `element_end`, `compile_jsx_text` (kind="text"), `compile_jsx_element`, `compile_jsx_for`, `compile_jsx_expression`, `compile_component_render` |
| `FRAME_LIFECYCLE` | 27 | `compile_surface_frame_loop`: surface resolve → null check → session_create → error handling → session_attach_platform → window_open → host_pump → begin_frame → component render → end_frame → present → should_close → loop → shutdown → session_destroy + pulse/resonate registration |
| `SHADER_SURFACE` | 6 | GPU path: surface resolve, session_create, `emit_gpu_set_uniform` (GEP/bitcast/load/call into `KainGpuSurfaceExtension`) |
| `LLVM_INTRINSIC` | 5 | `llvm.memset` (platform handle), alloca hoisting, store, ret void |
| `CALLBACK_BIND` | 6 | `compile_jsx_callback`: static string → compile expr → bitcast to `%KainComponentCallback` → bitcast to i8* → `element_set_callback` (slot 23) |
| `COMPONENT_CALL` | 7 | Component render function definition (surface/session/parent/props params), prop storage, cross-module declarations, component invocation, zero-value defaults, children rendering |
| `EXPR_EVAL` | 9 | Expression compilation in JSX context, stringification, `try_inline_component_method` (method call detection, arg validation, scope push/pop, block compilation) |
| `FLOW_CONTROL` | 21 | `compile_jsx_if` (condition, br, then/else), `compile_jsx_for` (iter, array_len, index alloca, loop header/body/done, array_get, child_parent = parent+idx) |
| `STABLE_KEY` | 8 | `emit_stable_key`: path_prefix + ":" + parent_to_string + ":sibling_index" → runtime `str_concat` chains |
| `VTABLE_CALL` | 6 | `emit_vtable_call`: GEP → bitcast → load fn ptr → call (two paths: void vs typed return) |
| `PULSE_RESONATE` | 10 | Component-inline pulse/resonate: stub handlers, one-time registration guard (state_get_i64 sentinel check), `kain_machine_pulse_start`, `abi_resonate_register` |
| `SETUP` | 7 | One-time type preamble, LLVM generator reset, entry block creation, component context tracking, parameter binding |

### 6.4 Key JSX Attribute → Vtable Mappings

**f64 attributes (slot 6):**
`padding`, `pad`→"padding", `spacing`, `gap`→"spacing", `corner_radius`, `radius`, `font_size`, `opacity`, `border`/`border_width`→"border_width", `stroke_width`→"border_width", `width`, `height`, `min`, `max`, `step`

**String attributes (slot 7):**
`background`→"fill_color", `fill`→"fill_color", `border_color`, `stroke`→"border_color", `color`/`ink_color`→"ink_color", `title`, `variant`, `role`, `align`, `font_family`, `distribution`→"layout.distribution", `axis`, `placeholder`, `tooltip`

**i64 attributes (slot 5):**
`direction`→"layout.direction" (string→int: "vertical"/"column"→1, "horizontal"/"row"→0), `disabled`, `checked`, `selected`, `tab_index`, `weight`

**Special:** `value` attribute bypasses key-based setting and goes directly to `element_set_text` (slot 4).

**Fallback:** Unknown attributes route through slot 7 (`element_set_attr_string`) with the raw attribute name as the style key. This enables forward compatibility — new backends can interpret any key without codegen changes.

### 6.5 State Persistence Pattern

For every component with `state count: i64 = 0`:

1. **First frame detection** via sentinels: `-1` for i64, `NaN` for f64, `null` for string
2. **Branch**: if sentinel → init block (store default + `state_set_*`), else → load block (read stored value)
3. **PHI merge**: `phi i64 [%init_val, %init_block], [%stored_val, %load_block]` merges both paths
4. **Write-back** at end of render: `load` from stack slot → `state_set_*` to persist mutations

The sentinel for i64 was **changed from `0` to `-1`** (component_calls.tsv row 73) because valid state value `0` was incorrectly triggering first-frame reinitialization.

### 6.6 Component Call Signature

Every component render function has the canonical signature:

```llvm
define void @ComponentName_render(
    %KainComponentSurface* %surface,   ; arg0 — vtable pointer for indirect calls
    i64 %session_id,                    ; arg1 — opaque session handle
    i64 %parent_id,                     ; arg2 — parent element (0 for root)
    ...props...                         ; arg3+ — typed props in declaration order
)
```

Child component calls thread the same surface/session through:

```llvm
call void @ChildName_render(
    %KainComponentSurface* %surface,
    i64 %session_id,
    i64 %child_parent,
    ...compiled props...
)
```

Missing props at the call site get zero/empty defaults generated by `zero_value_for_ty()`.

### 6.7 Stable Key Format

Elements get unique, deterministically-reconstructable stable keys:

```
"ComponentName:tag:parent_id:sibling_index"
```

Built via runtime `str_concat` chains:

```llvm
%step1 = call i8* @str_concat(i8* %prefix, i8* %colon)    ; "ComponentName:tag:"
%step2 = call i8* @str_concat(i8* %step1, i8* %parent_str) ; "ComponentName:tag:42"
%final = call i8* @str_concat(i8* %step2, i8* %si_str)     ; "ComponentName:tag:42:0"
```

For components: `"%s:root"` base (no tag). For `for` loops: `child_parent = parent_reg + loop_index`.

### 6.8 Flow Control in JSX

**`if`/`elif`/`else`:**
```llvm
%cond = icmp ne i64 %val, 0
br i1 %cond, label %then_block, label %else_block
; ... JSX body compilation in each branch ...
br label %done_block
```

**`for` loops:**
```llvm
; Pre-loop: evaluate iter, get array_len
%len = call i64 @runtime_array_len(i8* %iter)
store i64 0, i64* %idx_ptr
br label %loop_header

loop_header:
  %idx = load i64, i64* %idx_ptr
  %done = icmp sge i64 %idx, %len
  br i1 %done, label %loop_done, label %loop_body

loop_body:
  %item = call i8* @runtime_array_get(i8* %iter, i64 %idx)
  store i8* %item, i8** %item_addr
  %child_parent = add i64 %parent_reg, %idx
  ; compile JSX body under child_parent
  %next = add i64 %idx, 1
  store i64 %next, i64* %idx_ptr
  br label %loop_header
```

---

## Appendix: Cross-Reference Table

| C Struct (`component_surface.h`) | Rust Constant (`component.rs`) | C Slot | Purpose |
|----------------------------------|-------------------------------|:------:|---------|
| `session_create` | `OFF_SESSION_CREATE` | 0 | Create session |
| `session_destroy` | `OFF_SESSION_DESTROY` | 1 | Destroy session |
| `element_begin` | `OFF_ELEMENT_BEGIN` | 2 | Create element |
| `element_end` | `OFF_ELEMENT_END` | 3 | Close element |
| `element_set_text` | `OFF_ELEMENT_SET_TEXT` | 4 | Set text |
| `element_set_attr_i64` | `OFF_ELEMENT_SET_ATTR_I64` | 5 | Int attr |
| `element_set_attr_f64` | `OFF_ELEMENT_SET_ATTR_F64` | 6 | Float attr |
| `element_set_attr_string` | `OFF_ELEMENT_SET_ATTR_STRING` | 7 | String attr |
| `state_get_i64` | `OFF_STATE_GET_I64` | 8 | Read i64 state |
| `state_set_i64` | `OFF_STATE_SET_I64` | 9 | Write i64 state |
| `begin_frame` | `OFF_BEGIN_FRAME` | 10 | Start frame |
| `end_frame` | `OFF_END_FRAME` | 11 | End frame |
| `present` | `OFF_PRESENT` | 12 | Present frame |
| `poll_event` | `OFF_POLL_EVENT` | 13 | Poll events |
| `should_close` | `OFF_SHOULD_CLOSE` | 14 | Check close |
| `window_open` | `OFF_WINDOW_OPEN` | 15 | Open window |
| `host_pump` | `OFF_HOST_PUMP` | 16 | Pump OS queue |
| `session_attach_platform` | `OFF_SESSION_ATTACH_PLATFORM` | 17 | Attach HWND |
| `get_gpu_extension` | `OFF_GET_GPU_EXTENSION` | 18 | GPU probe |
| `state_get_f64` | `OFF_STATE_GET_F64` | 19 | Read f64 state |
| `state_set_f64` | `OFF_STATE_SET_F64` | 20 | Write f64 state |
| `state_get_string` | `OFF_STATE_GET_STRING` | 21 | Read string state |
| `state_set_string` | `OFF_STATE_SET_STRING` | 22 | Write string state |
| `element_set_callback` | `OFF_ELEMENT_SET_CALLBACK` | 23 | Bind callback |

---

## References

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/component_surface.h` | 150 | `KainComponentSurface` struct, registration/resolution API, `native_ui_surface` extern |
| `runtime/native/include/gpu_surface_extension.h` | 23 | `KainGpuSurfaceExtension` struct (load_shader, set_uniform) |
| `runtime/native/src/core/component_surface.c` | 202 | Registry implementation, GPU backend routing, `kain_runtime_panic`, `__kain_frame_delta_ms` |
| `runtime/native/src/ui/native_ui_surface.c` | 438 | Old KUIF GDI vtable implementation, auto-registration |
| `runtime/native/src/ui_v2/tree.c` | 883 | Kaintana vtable implementation, session lifecycle, element tree, layout, state, draw, input, hit-test, backend registry |
| `runtime/native/src/ui_v2/kaintana.h` | 1048 | `KaintanaComponentSurface` typedef, `KAINTANA_SURFACE_NAME` define, renderer types |
| `crates/sys-codegen/src/codegen_llvm/component.rs` | 2004 | `OFF_*` constants, `declare_surface_trait_types`, `compile_component_render`, `compile_surface_frame_loop`, `compile_jsx_*`, `emit_vtable_call`, `emit_gpu_set_uniform`, attribute mapping, state persistence |
| `crates/sys-codegen/src/codegen_llvm/chunk-10-component_calls.tsv` | 231 | Authoritative TSV catalog: type declarations, vtable constants, attribute sets, state access patterns, element tree ops, frame lifecycle, GPU surface path, callbacks, component calls, flow control, stable keys, vtable call mechanics, pulse/resonate registration |
