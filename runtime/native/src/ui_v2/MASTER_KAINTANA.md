# MASTER_KAINTANA.md — The Definitive Design Document

**Date:** 2026-06-27
**Status:** FINAL — authoritative synthesis of all research
**Supersedes:** `_KAINTANA.md`, `_ARCHITECTURE.md`, all 11 MASTER_* docs, 3 assessments, 2 API analyses
**Repository:** `X:/runtime/native/src/ui_v2/`
**Total sources synthesized:** 17 documents (5,300+ lines), 19,000+ lines of reference framework analysis
**Construct:** `kt_` (3-char prefix) · 34 public functions · 6 types · 24 vtable slots · 4 C files · 1 public header

---

## Table of Contents

1. [What Is Kaintana?](#1-what-is-kaintana)
2. [Why KUIF Was Wrong](#2-why-kuif-was-wrong)
3. [The 4-Layer Architecture](#3-the-4-layer-architecture)
4. [Design Tenets](#4-design-tenets)
5. [The API Contract](#5-the-api-contract)
6. [The Surface Types (World Surfaces)](#6-the-surface-types-world-surfaces)
7. [The Kain-to-C Flow (End-to-End)](#7-the-kain-to-c-flow-end-to-end)
8. [File Inventory with Rationale](#8-file-inventory-with-rationale)
9. [The Phases](#9-the-phases)
10. [Where Kaintana Deploys](#10-where-kaintana-deploys)
11. [Runtime Integration Summary](#11-runtime-integration-summary)
12. [The Core Math Contracts](#12-the-core-math-contracts)

---

## 1. What Is Kaintana?

Kaintana is a **clean-slate C rendering substrate** for Kain's UI system. It replaces KUIF — the 22-file, 25-header, 174-export, 11,000-line retained-mode monolith that was built before the Kain compiler could emit `component` keyword codegen. Kaintana is the service layer for exactly **one contract**: the 24-slot `KainComponentSurface` vtable.

The entire C substrate is **4 files** (`tree.c`, `box_math.c`, `damage.c`, `draw_pixels.c`), **1 public header** (`kaintana.h`), **1 private header** (`internal.h`), **2 support modules** (`arena.h/c`, `hash_table.h/c`), and a **`backends/` directory** of platform-specific host implementations. The total C code footprint is ~1,500 lines of substrate + ~3,000-5,000 lines of backends. Down from KUIF's 36,000+.

All widget logic — every button, label, slider, checkbox, layout composition, theme, and animation — lives in Kain source code (`std::kaintana::widgets.kn`). The C substrate does not know what a "button" is. It draws boxes, circles, text, and clips. That is the entire job.

Kaintana's API prefix is `kt_` (3 characters — same class as nuklear's `nk_`, microui's `mu_`). Types use PascalCase (`kt_Rect`, `kt_Color`, `kt_Input`, `kt_DrawCmd`). Functions use snake_case verbs (`kt_begin_frame`, `kt_element_begin`, `kt_present`). The public surface is 34 functions, 6 types, and the 24-slot vtable.

---

## 2. Why KUIF Was Wrong

KUIF (the old UI system at `src/ui/`) made **5 fatal architectural decisions**. Each is documented across multiple master documents. Here is the definitive autopsy.

### 2.1 Built Before the Compiler Could Emit Component Codegen

KUIF was created when the Kain compiler could not yet generate calls into the 24-slot `KainComponentSurface` vtable. To compensate, KUIF invented 174 `abi_ui_*` exports (`ui_system.h`), a hand-rolled node tree (`ui_system.c`, 3,162 lines), and a widget system in C (`widgets/ui_widget.c`, 1,559 lines). Every one of these was a workaround for missing compiler features that now exist.

**Master doc reference:** `MASTER_CONTRACT.md` — "KUIF was written BEFORE the core runtime had stable ABIs for arena, input, diagnostics, and profiling."

### 2.2 No Core Runtime Integration

KUIF duplicated everything:
- **Own arena** — Fixed arrays inside `KainNativeUiSession`. Not `kain_arena_alloc_lo()` from the CBMC-proven arena (833 assertions). Hand-rolled fixed-size buffers with malloc/free for oversize nodes.
- **Own input system** — `abi_ui_push_event()` / `abi_ui_poll_event()` in `ui_system.c`. A separate event ring buffer with no action/axis binding, no replay, no trace. Duplicated what `input_system.c` already did with 875 lines of Z3-proven code.
- **Own everything** — No service registry registration, no diagnostics, no profiling, no handle tables.

**Master doc reference:** `MASTER_CONTRACT.md` §5 — "KUIF's five fatal mistakes: Own arena, own input, own everything, no diagnostics, no profiling."

### 2.3 25 Headers Polluted the Runtime Include Directory

KUIF spread 25 UI headers across `include/`, interleaved with 59 core runtime headers. Twin-header copies of every `kain_*.h` file existed in both `src/ui/kain/` and `include/` — two source-of-truth copies that could diverge. **Kaintana has exactly 1 header.**

**Master doc reference:** `MASTER_GIT.md` §5 — "Generated file churn: KUIF's twin header copies mirror nuklear's amalgamation failure."

### 2.4 Widgets in C with Hardcoded Colors

`widgets/ui_widget.c` had 18 `#define UI_COLOR_*` constants and 11 `#define UI_*_SIZE` constants. Colors, spacing, and widget dimensions were irreversibly baked into C code. No hot-reload. No theme switching. No Kain-side control.

**Master doc reference:** `MASTER_GIT.md` §9 — "Widget Systems Take 12+ Months to Stabilize — And They Don't Belong in C."

### 2.5 No Testing Backend for 11,000 Lines

KUIF had zero testing infrastructure. No null backend. No headless render path. No Python ABI driver. Every test required launching a real OS window. The frameworks studied (egui: 6 years, slint: 4 years, imgui: 3-4 years) all deferred testing and all regretted it.

**Master doc reference:** `MASTER_GIT.md` §4 — "Testing Infrastructure Is Always Deferred — Don't."

### 2.6 The 3,162-Line Monolith

`ui_system.c` at 3,162 lines handled session lifecycle, node CRUD, style/state management, event routing, focus, IME, drag-drop, menus, dialogs, resource management, fonts, and callback dispatch. This single file was larger than Kaintana's entire C substrate.

**Root cause (from `_KAINTANA.md` §I):** *"The UI system should have come last, not first."* By the time the core runtime was stable and verified, KUIF was too large to refactor.

---

## 3. The 4-Layer Architecture

Kaintana strictly separates concerns across 4 distinct layers. The rule is absolute: **C handles only primitives, geometry, damage tracking, and platform abstraction. Kain handles everything else.**

```
┌──────────────────────────────────────────────────────────────────┐
│  LAYER 3: Kain Components & Widgets (std::kaintana)              │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  Author in pure .kn code. Widget library over              │  │
│  │  the 24-slot vtable. Layout composition, theme,            │  │
│  │  animation, state management. All Kain. Zero C.            │  │
│  │                                                            │  │
│  │  std::kaintana/core.kn     — 24 @extern bindings            │  │
│  │  std::kaintana/theme.kn    — colors, spacing, dark mode     │  │
│  │  std::kaintana/layout.kn   — HStack, VStack, Grid           │  │
│  │  std::kaintana/widgets.kn — Button, Label, Slider, etc.     │  │
│  │  std::kaintana/animation.kn — pulse-driven spring physics   │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  LAYER 2: The Vtable ABI Contract (kaintana.h)                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  THE public header. 24-slot KainComponentSurface vtable.   │  │
│  │  16 named theme colors. 6 geometry/input/draw types.       │  │
│  │  Backend registry. Slot 24-31 reserved for expansion.      │  │
│  │                                                            │  │
│  │  #include "kaintana.h" ← THE ONLY INCLUDE                  │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  LAYER 1: C Substrate (4 files + 2 support modules)              │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  Pure C11, zero OS headers, zero platform dependencies,     │  │
│  │  arena-only allocation, O(n) layout, O(k) damage track.    │  │
│  │                                                            │  │
│  │  tree.c         — ABI ingestion, node arena, state mgmt    │  │
│  │  box_math.c     — Two-pass flexbox constraint solver       │  │
│  │  damage.c       — Three-phase dirty pipeline (Slate-style) │  │
│  │  draw_pixels.c — 16 draw primitives, write-pointer merge   │  │
│  │  arena.h/c      — Grow-only arena allocator                │  │
│  │  hash_table.h/c — FNV-1a open-addressing stable key table  │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  LAYER 0: Platform Backends (backends/)                          │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  OS-specific code, exiled here. THE ONLY files that         │  │
│  │  include <windows.h>, <X11/Xlib.h>, <vulkan/vulkan.h>.    │  │
│  │  Every backend implements exactly 4 functions:             │  │
│  │    init, shutdown, new_frame, render                       │  │
│  │                                                            │  │
│  │  host_null.c   — 100 lines, testing backend               │  │
│  │  host_win32.c — 800 lines, GDI software fallback          │  │
│  │  render_vulkan.c — 2000 lines, GPU compute path           │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Layer Boundaries — What Each Layer Must NOT Do

| Layer | Must NOT |
|-------|----------|
| L3 (Kain .kn) | Call C directly. All interaction goes through the 24 @extern bindings in `core.kn`. |
| L2 (kaintana.h) | Include platform headers. Include anything beyond `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`. |
| L1 (4 .c files) | Include `<windows.h>`, `X11/`, or any OS-specific header. Call `malloc`/`free` directly. Use fixed-size arrays. |
| L0 (backends/) | Touch tree.c, box_math.c, damage.c, or draw_pixels.c internal state. Backends consume ONLY `KaintanaDrawData`. |

**Source:** `MASTER_PLATFORM.md` §1 — "Zero Platform Deps in Core" (ImGui, egui, Slint, Yoga, Clay, Vello all proven).

---

## 4. Design Tenets

The following 7 tenets are the load-bearing walls of Kaintana's architecture. Each is validated by multiple framework analyses from the research corpus.

### Tenet 1: One Public Header

**`kaintana.h` is THE include.** The entire public API — types, vtable, input struct, draw output, backend registry — lives in one file. Any external consumer includes exactly one header. No twin copies. No private headers exposed. No "kain_geometry.h" alongside "kaintana.h".

**Verified by:** ImGui (`imgui.h`, 12 years, single header), Yoga (`yoga/Yoga.h`), Clay (`clay.h`). Every long-lived framework converges on a single header for the public API.

**Source:** `MASTER_GIT.md` §2 — "The One Header Never Changes."

### Tenet 2: No Platform Headers in Core

**Files 1-4 in L1 must compile on a freestanding C11 compiler.** No `<windows.h>`, no `<X11/`, no `<vulkan/`, no `<GL/>`. The includes needed for tree.c: `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

**Verified by:** Yoga (pure C++ math, zero platform), Clay (single header, zero platform), ImGui core (`imgui.h` includes only `<stdint.h>`, `<stdlib.h>`, `<math.h>`, `<float.h>`).

**Source:** `MASTER_PLATFORM.md` §1 — The Universal Law.

### Tenet 3: No Hardcoding

**Zero `#define` for colors, spacing, or widget sizes.** All visual values — fill colors, border widths, corner radii, padding, opacity — are passed as data through the 24-slot vtable's `element_set_attr_*` functions. The C substrate is a dumb pipe. It does math on the values it receives. It does not invent values.

KUIF had 18 color `#define`s and 11 size `#define`s in `ui_widget.h`. Kaintana has zero.

**Source:** `_KAINTANA.md` §IV.2 — "Hardcoding is a Sin."

### Tenet 4: Arena-Only Allocation

**No malloc per node.** Every per-frame allocation comes from a grow-only arena backed by `kain_arena_alloc_lo()` from the core runtime (833 CBMC assertions). Frame lifecycle uses `kain_frame_set_marker()`/`kain_frame_release_to_last_marker()` for O(1) per-frame cleanup. No per-object free, no mark-sweep, no reference counting.

**Verified by:** ImGui (`ImVector` geometric growth, clear-but-no-free between frames), Slate (`FWidgetProxy` flat arena), Clay (single header arena, no malloc in hot path).

**Source:** `MASTER_MEMORY_AND_ARENA.md` §1, `MASTER_CONTRACT.md` §4 (Path A).

### Tenet 5: Flat Names

Every file name must be so obvious that a 10-year-old (or an LLM) immediately knows what the file does:

| This file | Instead of |
|-----------|------------|
| `box_math.c` | `ui_layout_resolver.c` |
| `draw_pixels.c` | `ui_renderer.c` |
| `tree.c` | `ui_surface_controller.c` |
| `damage.c` | `ui_invalidation_pipeline.c` |

**If you cannot name the file simply, it is doing too many things.** Break it apart.

**Source:** `_KAINTANA.md` §IV.1, `_ARCHITECTURE.md` — "The 10-Year-Old Naming Convention."

### Tenet 6: Testing Infrastructure from Day One

Kaintana's `draw_pixels.c` renders to a `uint32_t*` framebuffer in memory. **This IS a headless testing backend.** No GPU, no window system, no X server, no DISPLAY variable.

The `host_null.c` backend (~100 lines) implements the 4-function backend contract with no-ops. Every CI run exercises the full pipeline — tree.c → box_math.c → damage.c → draw_pixels.c — with zero GPU requirements. Python ABI tests (`tests/python_abi/`) drive the vtable via ctypes. Fuzz targets (`tests/fuzzer/`) bomb the ABI with random attributes.

**egui waited 6 years for a testing backend. slint waited 4 years. Kaintana has no excuse.**

**Source:** `MASTER_GIT.md` §4, `MASTER_PLATFORM.md` §11.

### Tenet 7: The Prefix Is Frozen

The `kt_` prefix and all type/enum names are locked before the first line of C is written. Framework history shows every rename costs 10-100x more than expected: nuklear's 2 renames (zahnrad → nuklear) cost ~80 files of churn. Yoga's rename (css-layout → Yoga) cost 700+ file renames over 5 months. Slint's rename (SixtyFPS → Slint) took multiple days.

**Kaintana changes its prefix exactly zero times.**

**Source:** `MASTER_GIT.md` §8, `MASTER_API.md` — "The Prefix."

---

## 5. The API Contract

### 5.1 The Prefix: `kt_`

3 characters. Distinct from all other frameworks (not `nk_`, not `Im`, not `YG`, not `egui::`). `kaintana_` (9 chars) is too long — 3x the typing for 1,000+ call sites. `kn_` could collide with nuklear. `k_` is too generic.

**Source:** `MASTER_API.md` — "The Prefix."

### 5.2 Type Naming Convention

| Category | Pattern | Examples |
|----------|---------|----------|
| Geometry | PascalCase | `kt_Rect`, `kt_Vec2`, `kt_Color`, `kt_Matrix` |
| Input | PascalCase | `kt_Input`, `kt_Event`, `kt_Key` |
| Draw | PascalCase | `kt_DrawCmd`, `kt_DrawData`, `kt_Mesh` |
| Layout | PascalCase | `kt_Layout`, `kt_SizeConstraint`, `kt_FlexConfig` |
| Context | PascalCase | `kt_Session`, `kt_Backend`, `kt_Context` |
| Functions | snake_case verbs | `kt_begin_frame`, `kt_element_begin`, `kt_present` |
| Enums | `KT_` prefix + UPPER | `KT_ALIGN_START`, `KT_DIR_ROW`, `KT_SIZING_GROW` |
| Macros | UPPER_CASE | `KT_API`, `KT_INLINE` |

**Source:** `MASTER_API.md` — "The Types."

### 5.3 The 24 Vtable Slots

This is the ABI contract. Immutable. Never reorder. Never delete. Only append (slots 24-31 reserved).

```c
typedef struct KainComponentSurface {
    // Slots 0-1: Session lifecycle
    int64_t (*session_create)(const char* name, int64_t w, int64_t h);
    void    (*session_destroy)(int64_t sid);

    // Slots 2-4: Element tree
    int64_t (*element_begin)(int64_t sid, int64_t parent, const char* kind, const char* stable_key);
    void    (*element_end)(int64_t sid, int64_t elem);
    void    (*element_set_text)(int64_t sid, int64_t elem, const char* text);

    // Slots 5-7: Attributes (i64, f64, string)
    void (*element_set_attr_i64)(int64_t sid, int64_t elem, const char* key, int64_t v);
    void (*element_set_attr_f64)(int64_t sid, int64_t elem, const char* key, double v);
    void (*element_set_attr_string)(int64_t sid, int64_t elem, const char* key, const char* v);

    // Slots 8-9: i64 state
    int64_t (*state_get_i64)(int64_t sid, const char* key);
    void    (*state_set_i64)(int64_t sid, const char* key, int64_t v);

    // Slots 10-14: Frame lifecycle + events
    void    (*begin_frame)(int64_t sid, double delta_ms);
    void    (*end_frame)(int64_t sid);
    void    (*present)(int64_t sid);
    int64_t (*poll_event)(int64_t sid, void* out, int64_t max_size);
    int64_t (*should_close)(int64_t sid);

    // Slots 15-17: Window management
    int64_t (*window_open)(int64_t sid, const char* title, int64_t w, int64_t h);
    int64_t (*host_pump)(int64_t sid);
    void    (*session_attach_platform)(int64_t sid, void* handle);

    // Slot 18: GPU extension
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t sid);

    // Slots 19-22: f64 and string state
    double      (*state_get_f64)(int64_t sid, const char* key);
    void        (*state_set_f64)(int64_t sid, const char* key, double v);
    const char* (*state_get_string)(int64_t sid, const char* key);
    void        (*state_set_string)(int64_t sid, const char* key, const char* v);

    // Slot 23: Callbacks
    void (*element_set_callback)(int64_t sid, int64_t elem, const char* event, void* fn);

    // Slots 24-31: RESERVED for future expansion. Must be NULL.
} KainComponentSurface;
```

**Source:** `_KAINTANA.md` §VII, `MASTER_API.md` — "The 24-Slot Vtable."

### 5.4 The 16 Named Colors

All visual values come from Kain as data. The C layer converts string keys to RGBA:

```c
"bg"              // Deepest background       ("#1A1A24")
"surface"         // Card/panel background    ("#232339")
"accent"          // Primary interactive      ("#21D4A1")
"accent2"         // Secondary                ("#7C3AED")
"accent3"         // Tertiary                 ("#F59E0B")
"accent4"         // Destructive              ("#EF4444")
"text"            // Primary text             ("#F1F1F6")
"text_dim"        // Muted text               ("#6B6B80")
"border"          // Borders and separators   ("#2E2E4A")
"button"          // Button normal            ("#2A2A40")
"button_hover"    // Button hovered           ("#3A3A58")
"button_press"    // Button pressed           ("#4A4A66")
"input_bg"        // Text input background    ("#141420")
"transparent"     // No fill                  ("#00000000")
```

Hex colors also work directly: `"#21D4A1"`, `"#1A1A24"`, `"#FF0000FF"`.

**Source:** `MASTER_API.md` — "The Named Colors."

### 5.5 The 34 Public Functions

| Section | Functions | Count |
|---------|-----------|-------|
| Lifecycle | `kt_init`, `kt_make`, `kt_free` | 3 |
| Frame | `kt_begin`, `kt_end`, `kt_present`, `kt_should_close` | 4 |
| Input | `kt_input_mouse_move`, `kt_input_mouse_down`, `kt_input_mouse_up`, `kt_input_scroll`, `kt_input_key_down`, `kt_input_key_up`, `kt_input_text` | 7 |
| Elements | `kt_row`, `kt_end_row`, `kt_text` | 3 |
| Layout | `kt_width`, `kt_height`, `kt_pad`, `kt_pad_xy`, `kt_gap`, `kt_direction` | 6 |
| Style | `kt_fill`, `kt_stroke`, `kt_radius`, `kt_opacity`, `kt_font` | 5 |
| State | `kt_put`, `kt_put_f`, `kt_put_s`, `kt_get`, `kt_get_f`, `kt_get_s` | 6 |

**Total: 34 public functions.** Compare: imgui has ~200, nuklear has ~200, KUIF had 174.

**Source:** `MASTER_API.md` — "The Public Functions."

### 5.6 The 10-Year-Old Frame Loop

```c
int main(void) {
    kt_init();
    kt_Session* ui = kt_make("My App", 800, 600);

    while (!kt_should_close(ui)) {
        kt_input_mouse_move(ui, mouse_x, mouse_y);
        kt_begin(ui, 16.0);

        int root = kt_row(ui, -1, "box", "root");
        kt_fill(ui, root, "bg");

        int btn = kt_row(ui, root, "box", "hello_btn");
        kt_fill(ui, btn, "accent");
        kt_width(ui, btn, 100);
        kt_height(ui, btn, 30);
        kt_radius(ui, btn, 4);
        kt_text(ui, btn, "Click Me");
        kt_end_row(ui);
        kt_end_row(ui);

        kt_end(ui);
        for (int i = 0; i < kt_cmd_count(ui); i++) {
            kt_Cmd cmd = kt_cmd_get(ui, i);
            draw_rect(cmd.bounds, cmd.color, cmd.radius);
        }
        kt_present(ui);
    }
    kt_free(ui);
    return 0;
}
```

**Source:** `MASTER_API.md` — "The Complete 10-Year-Old Frame Loop."

### 5.7 What the API Does NOT Have

| Removed | Why |
|---------|-----|
| `kt_push_style` / `kt_pop_style` | State stack confuses. Set attrs per-element. |
| `kt_layout_row` with width array | Too many params. Use `kt_width`/`kt_height` per child. |
| `kt_window` | `kt_row` with kind="window". Simpler. |
| `kt_menu` / `kt_dialog` | Owned by Kain components (`std::kaintana::widgets.kn`). |
| `kt_image` / `kt_texture` | Phase 2. Phase 1 is geometry + text. |
| `kt_scroll` | Phase 2. Phase 1 is non-scrollable. |
| `kt_hit_test` | Internal to damage.c. Users don't call this. |
| All 174 `abi_ui_*` | Replaced by 24 vtable slots + 34 public functions. |

---

## 6. The Surface Types (World Surfaces)

A surface is the compile-time binding that maps a Kain `world` to a Kaintana rendering target. The Kain compiler routes component frame loops through the backend identified by the surface name.

### 6.1 `native_ui` — Desktop Windows (Primary)

**Target:** `<world> surface native_ui => Component`
**Backend:** Defaults to software rasterizer (GDI on Windows, Cocoa on macOS).
**When implemented:** Phase 1 (P0, primary desktop)
**Kain code:**
```kain
world MyApp:
    surface native_ui => MyComponent
```

### 6.2 `shader_canvas` — Fully GPU-Accelerated UI

**Target:** `<world> surface shader_canvas => Component`
**Backend:** Resolves to hardware-accelerated backends (Vulkan, D3D12, WebGPU). Falls back to software if no GPU dispatch available.
**When implemented:** Phase 2 (P2, GPU accel)
**Kain code:**
```kain
world MyApp:
    surface shader_canvas => MyComponent
```

### 6.3 `web` — WASM / Browser Target

**Target:** `<world> surface web => Component`
**Backend:** WebAssembly shim via Canvas 2D API or DOM rendering.
**When implemented:** Phase 2 (P2, experimental)
**Kain code:**
```kain
world MyApp:
    surface web => MyComponent
```

### 6.4 `terminal` — TUI / CLI Surface

**Target:** `<world> surface terminal => Component`
**Backend:** ANSI escape codes and terminal grid cells. Proves the architecture is platform-agnostic — same `world MyApp`, different surface = completely different rendering.
**When implemented:** Phase 2 (P2, proof)
**Kain code:**
```kain
world MyApp:
    surface terminal => MyComponent
```

### 6.5 `ue5` — Unreal Engine 5 Widget

**Target:** `<world> surface ue5 => Component`
**Backend:** Slate/UMG bridge in the UE5 integration layer.
**When implemented:** Phase 3 (P3, nice-to-have)
**Kain code:**
```kain
world MyApp:
    surface ue5 => LoadingScreenHUD
```

### Surface Resolution Chain (Fixed Priority Order)

**CORRECTED.** Backend selection follows a 4-layer stack. The env var is
NOT the primary mechanism — it is a DEBUG/CI override only.

```
LAYER 0: COMPILE-TIME — link only what you need
  Release for Windows: host_win32.c + render_vulkan.c
  CI: host_null.c only
  Backends not linked CANNOT be selected.

LAYER 1: CODE-TIME — the APPLICATION chooses (PRIMARY)
  kt_backend_register(session, "vulkan", &vk_vtable)
  kt_backend_select(session, "vulkan")     // explicit, no env var

LAYER 2: PLATFORM DEFAULT — if app didn't call select()
  Windows    → "win32"
  Linux      → "x11" or "wayland"
  macOS      → "macos"
  Unknown    → "null"

LAYER 3: FALLBACK CHAIN (last resort) — first-to-init wins
  Registered backends tried in registration order
  Guarantees null or terminal always work

LAYER 4: ENV VAR OVERRIDE (debug/CI only — not for production)
  RENDERER_BACKEND=vulkan ./myapp    # debug override
  RENDERER_BACKEND=null   ./myapp    # CI headless
  Ignored if app called kt_backend_select()
```

**Source:** `MASTER_OS_AND_CONTRACT.md` §4, `MASTER_PLATFORM.md` §12.

---

## 7. The Kain-to-C Flow (End-to-End)

The following sequence shows how an authored Kain component traverses the entire stack — from `.kn` source to pixels on screen.

```
Kain source code:
┌───────────────────────────────────────────────────────┐
│ component Button(label: String):                      │
│     state hovered: Bool = false                       │
│                                                       │
│     render <box kind="button" width={120} height={32}>│
│         fill="accent"                                 │
│         if hovered: fill="accent_hover"               │
│         <text>{label}</text>                          │
│     </box>                                            │
└───────────────────────┬───────────────────────────────┘
                        │ Kain compiler
                        ▼
Compiler emits LLVM IR that calls vtable slots:
┌───────────────────────────────────────────────────────┐
│ Slot  0: session_create("MyApp", 800, 600)            │
│ Slot 15: window_open(sid, "MyApp", 800, 600)          │
│                                                       │
│ Frame loop:                                           │
│ Slot 10: begin_frame(sid, 16.0)                       │
│   Slot  2: element_begin(sid, -1, "box", "root")      │
│     Slot  2: element_begin(sid, root, "box", "btn_0") │
│     Slot  5: element_set_attr_i64(sid, btn,            │
│                "width", 120)                           │
│     Slot  5: element_set_attr_i64(sid, btn,            │
│                "height", 32)                           │
│     Slot  7: element_set_attr_string(sid, btn,         │
│                "fill", "accent")                       │
│     Slot  8: state_get_i64(sid, "hovered") → 0        │
│     Slot  4: element_set_text(sid, btn, "Click")       │
│     Slot 23: element_set_callback(sid, btn,            │
│                "click", on_click_fn)                   │
│     Slot  3: element_end(sid, btn)                    │
│   Slot  3: element_end(sid, root)                     │
│ Slot 11: end_frame(sid)                               │
│                                                       │
│ Slot 12: present(sid)                                  │
└───────────────────────┬───────────────────────────────┘
                        │ Kaintana C substrate
                        ▼
Layer 1 — tree.c dispatches vtable calls:
┌───────────────────────────────────────────────────────┐
│ kaintana_element_begin() → hash stable key             │
│   → find-or-create node in arena                       │
│ kaintana_set_attr_string("fill", "accent")              │
│   → attr_table → invalidation cascade                  │
│   → kaintana_mark_node_dirty(LAYOUT | PAINT)           │
│ kaintana_end_frame() →                                  │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
Layer 1 — damage.c processes dirty nodes:
┌───────────────────────────────────────────────────────┐
│ Phase 1: PreUpdate → structural changes               │
│ Phase 2: Prepass  → bottom-up desired sizes           │
│ Phase 3: PostUpdate → top-down arrange + paint         │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
Layer 1 — box_math.c computes layout:
┌───────────────────────────────────────────────────────┐
│ Yoga-inspired two-pass flex resolution:                │
│ Pass 1: bottom-up → compute desired sizes             │
│ Pass 2: top-down  → arrange children + distribute     │
│                                                       │
│ Result: KaintanaLayout[#nodes] with final positions   │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
Layer 1 — draw_pixels.c generates commands:
┌───────────────────────────────────────────────────────┐
│ Walk final node list → emit typed draw commands:      │
│   KAINTANA_CMD_FILL_ROUNDED_RECT{120,32,r=4,accent}   │
│   KAINTANA_CMD_TEXT{"Click", font_id=0, size=14}      │
│                                                       │
│ Auto-merge adjacent same-color rects → <50 commands   │
│                                                       │
│ Output: KaintanaDrawData{cmds, textures, vertices}     │
└───────────────────────┬───────────────────────────────┘
                        │
                        ▼
Layer 0 — Platform Backend:
┌───────────────────────────────────────────────────────┐
│ Software path (draw_pixels.c → DIB → BitBlt):         │
│   kaintana_execute_software(cmdlist, framebuffer)      │
│   → InvalidateRect(hwnd, dirty_rect, FALSE)            │
│   → WM_PAINT → BitBlt to screen                       │
│                                                       │
│ GPU path (draw_pixels.c → vertex buffer → GPU):       │
│   kaintana_cmds_to_vertex_buffer(cmdlist)              │
│   → DMA vertices/indices to GPU                       │
│   → vkCmdDrawIndexed() → swapchain present            │
└───────────────────────────────────────────────────────┘
```

**Source:** `_ARCHITECTURE.md` — "The Data Flow," `MASTER_PIXELS_AND_GEO.md` §10.3.

---

## 8. File Inventory with Rationale

### 8.1 Core Substrate (ui_v2/)

| File | Lines | Purpose | Master Doc Justification | What KUIF Had Instead |
|------|-------|---------|--------------------------|----------------------|
| `kaintana.h` | ~600 | THE public header. 24-slot vtable + all types + backend registry. Twin to `component_surface.h`. | `MASTER_API.md` (34 functions, 6 types), `MASTER_CONTRACT.md` §4 Path B (vtable alignment), `MASTER_GIT.md` §2 (freeze slot order) | 25 headers in `include/` + 18 twin copies in `src/ui/kain/` |
| `internal.h` | ~400 | Private header: KaintanaNode (32B), KaintanaLayout (48B), arena, hash table, damage pipeline, session struct. | `MASTER_MEMORY_AND_ARENA.md` §2 (node sizing, cache line alignment), `MASTER_INVALIDATION_AND_DAMAGE.md` §1 (node bitfields) | inline in `ui_system_internal.h` (252 lines) |
| `tree.c` | ~500 | ABI ingestion: element_begin/end, set_attr, state_get/set, stable key hash, begin/end/present dispatch. | `MASTER_CONTRACT.md` §4 Path B (vtable impl), `MASTER_API.md` (function signatures), `MASTER_MEMORY_AND_ARENA.md` §4 (FNV-1a hash) | `ui_system.c` (3,034 lines — session, nodes, styles, events, focus, IME, menus, dialogs, fonts, all-in-one) |
| `box_math.c` | ~600 | Two-pass flexbox constraint solver. Pure math, zero platform headers. Yoga-inspired layout algorithm. | `MASTER_SPATIAL_LAYOUT.md` (all 49 formulas — flex basis, grow/shrink, auto-min, line wrapping, cross-axis alignment), `MASTER_MEMORY_AND_ARENA.md` §7 (SoA layout arena) | `ui_layout.c` (199 lines, basic flexbox), `flexbox.c` (separate file) |
| `damage.c` | ~350 | Three-phase invalidation pipeline (Slate-style). Dirty flag propagation, 64-rect damage accumulator, lazy sleep. | `MASTER_INVALIDATION_AND_DAMAGE.md` (all 7 sections — propagation, three-phase pipeline, 64-rect ceiling, wasRead, lazy sleep, layout cache, rebuild threshold) | spread across `ui_system.c` and `kain_compositor.c` |
| `draw_pixels.c` | ~500 | 16 draw primitives with write-pointer reservation and auto-merge. Software rasterizer and GPU vertex converter. | `MASTER_PIXELS_AND_GEO.md` (all 221 formulas — SDF, edge functions, vertex packing, write-pointer, merge), `MASTER_COLOR_AND_BLEND.md` (SrcOver, div255, premultiplied convention) | `ui_renderer.c` (244 lines) + `kain_render_software.c` (582 lines) |
| `arena.h/c` | ~150 | Grow-only arena allocator delegating to `kain_arena_alloc_lo()` from core runtime. Frame markers. | `MASTER_CONTRACT.md` §4 Path A (arena integration, 833 CBMC proofs), `MASTER_MEMORY_AND_ARENA.md` §1 (1.5x growth, 512 initial nodes, 32B node alignment) | hand-rolled fixed arrays in `KainNativeUiSession` |
| `hash_table.h/c` | ~200 | FNV-1a open-addressing hash table for stable key → node lookup. 4096 slots, 0.0625 max load. | `MASTER_MEMORY_AND_ARENA.md` §3-4 (FNV-1a constants, collision probability, Z3-proven probe bounds), `MASTER_CONTRACT.md` §4 Path A (use same FNV-1a hash as input_system) | linear scan in `ui_system.c` |

**Core substrate total: ~3,300 lines** (down from KUIF's ~11,000 lines of `src/ui/` + `src/ui/kain/` + `src/ui/widgets/` + 25 headers).

### 8.2 Backends (backends/)

| File | Lines | Purpose | Master Doc Justification | Core Runtime Integration |
|------|-------|---------|--------------------------|------------------------|
| `host_null.c` | ~100 | Testing backend. No-op all 4 functions. Guaranteed to compile on any C11 compiler. | `MASTER_PLATFORM.md` §11 (testing backend pattern), `MASTER_GIT.md` §4 (build testing_backend.c during Phase 1) | None (standalone) |
| `host_win32.c` | ~800 | Win32 GDI window + DIB framebuffer + message pump. Implements `kainHostVTable`. | `MASTER_OS_AND_CONTRACT.md` §3 (DIB creation math, BitBlt, dirty rect optimization), §7 (DPI chain), `MASTER_CONTRACT.md` §4 Path C (input funnel via abi_input_push_event) | `kain_host_get()` for framebuffer, `abi_input_push_event()` for input, `kain_platform_current_kind()` for detection |
| `render_vulkan.c` | ~2,000 | Vulkan GPU renderer. Pipelines, descriptor pools, swapchain, buffer upload. | `MASTER_OS_AND_CONTRACT.md` §6 (GPU upload math, vertex/index buffer size, descriptor binding), `MASTER_PIXELS_AND_GEO.md` §6 (vertex packing) | `gpu_surface_extension.h` slot 18, `renderer_session_boot()` for backend resolution |
| `render_d3d12.c` | ~1,000 | DirectX 12 GPU renderer. | `MASTER_OS_AND_CONTRACT.md` §5 (backend complexity scaling), `MASTER_PLATFORM.md` §10 (complexity budgets) | Same GPU integration pattern as Vulkan |
| `render_webgpu.c` | ~1,200 | WebGPU cross-platform renderer. | `MASTER_PLATFORM.md` §9 (GPU-only pattern, Vello Recording IR) | Same GPU integration pattern |
| `surface_terminal.c` | ~300 | ANSI escape code terminal renderer. | `_KAINTANA.md` §II.4 (terminal surface), `MASTER_PLATFORM.md` §11 (terminal as proof of zero-GPU) | None (standalone ANSI output) |

### 8.3 Support Infrastructure

| File | Lines | Purpose | Master Doc Justification |
|------|-------|---------|--------------------------|
| `tests/python_abi/` | ~500 | Python ctypes tests driving the 24-slot vtable from Python. | `MASTER_GIT.md` §4 (testing from day one), `MASTER_CONTRACT.md` §6 (Phase 3: P1-24) |
| `tests/fuzzer/` | ~500 | libFuzzer targets bombing element_set_attr_string with random keys. | `MASTER_CONTRACT.md` §6 (Phase 3: P1-25), `MASTER_GIT.md` §4 (fuzz early) |
| `z3/` | ~10 files | Proof packs for box_math invariants, damage state machine, arena bounds, hash table collisions. | `MASTER_CONTRACT.md` Appendix A (905 proofs inherited from core runtime), `MASTER_MEMORY_AND_ARENA.md` Appendix B (Z3 proof index) |
| `stdlib/kaintana/` | ~800 lines .kn | The Kain-side stdlib: core.kn (24 @extern), theme.kn, layout.kn, widgets.kn, animation.kn. | `_KAINTANA.md` §VIII (language bridge), `_ARCHITECTURE.md` — "What Kain Owns vs What C Owns," `MASTER_INTERACTION_AND_MOTION.md` §8 (Spring physics in Kain) |

### 8.4 Core Runtime Integration Points

Every core runtime file that Kaintana calls into (from `MASTER_CONTRACT.md` §3):

| Core File | Lines | Kaintana Role | Integration Path |
|-----------|-------|---------------|-----------------|
| `arena.c` | 205 | ALL per-frame allocation via `kain_arena_alloc_lo()`. Frame markers. | `MASTER_CONTRACT.md` §4 Path A — 833 CBMC proofs inherited. |
| `component_surface.c` | 201 | Vtable registration and resolution. Kaintana registers as "kaintana" surface. | `MASTER_CONTRACT.md` §4 Path B — slot order frozen. |
| `input_system.c` | 875 | Event routing via `abi_input_push_event/begin_frame/action_pressed`. | `MASTER_CONTRACT.md` §4 Path C — Z3-proven collision-free. |
| `services.c` | 1,350 | Register "ui.kaintana" service key. Capability query before platform use. | `MASTER_CONTRACT.md` §3 — perfect-hash O(1) lookup. |
| `diagnostics.c` | 514 | Error reporting via `kain_diagnostic_create()`, subsystem 5000-5999. | `MASTER_CONTRACT.md` §3 — structured diagnostics, not printf. |
| `profile.c` | 120 | Scoped profiling zones around box_math, damage, draw_pixels hot paths. | `MASTER_CONTRACT.md` §3 — `KAIN_PROFILE_SCOPE("kaintana_layout")`. |
| `machine_stones.c` | 653 | Pulse animation timing, teleport for zero-copy surface handoff. | `MASTER_CONTRACT.md` §3 — 6 Z3 proofs on pulse missed-beat math. |
| `renderer_session.c` | 397 | Backend selection via `renderer_session_boot()`, `RENDERER_BACKEND` env var. | `MASTER_CONTRACT.md` §3 — entry point for Kaintana backend init. |
| `handle.c` | 161 | Generation-tagged stable key → node mapping. | `MASTER_CONTRACT.md` §3 — 4 Z3 proofs on stale handle rejection. |

**Source:** `MASTER_CONTRACT.md` §3 — Complete File Index (15 directly needed, 10 maybe, 34 not relevant).

---

## 9. The Phases

Informed by 36,651 commits across 5 frameworks (MASTER_GIT.md) and validated against all 11 MASTER_* documents.

### Phase 1: Design + kaintana.h + Core Substrate (Weeks 1-2)

**What:** The 4 C files, 1 public header, arena, hash table. The null backend.

**Files:**
- [ ] `kaintana.h` — 24-slot vtable + all types
- [ ] `internal.h` — KaintanaNode, KaintanaLayout, session struct
- [ ] `tree.c` — ABI ingestion, stable key hash
- [ ] `box_math.c` — Two-pass flex (Yoga-inspired)
- [ ] `damage.c` — Three-phase pipeline, 64-rect accumulator
- [ ] `draw_pixels.c` — 16 draw primitives, write-pointer, merge
- [ ] `arena.h/c` — Arena wrappers for `kain_arena_alloc_lo()`
- [ ] `hash_table.h/c` — FNV-1a open-addressing
- [ ] `backends/null/host_null.c` — ~100 lines, testing backend

**Core integration:** P0 items from `MASTER_CONTRACT.md`:
- `kain_arena_alloc_lo()` replacing ALL malloc (P0-1)
- `kaintana.h` includes `component_surface.h` (P0-2)
- Surface registration via `kain_component_surface_register()` (P0-3)
- Platform detection via `kain_platform_current_kind()` (P0-6)
- TOML manifest update + `update_runtime.py` (P0-7)

**Verification:** `host_null.c` compiles on any C11 compiler. `make tests` runs Python ABI driver. Arena stress test passes (10,000 nodes, no overflow). **MASTER_GIT.md:** "Write testing_backend.c BEFORE any GPU backend. It saves 4-6 years of deferred testing pain."

### Phase 2: Backend + Core Integration (Weeks 2-4)

**What:** Win32 backend, core runtime integration, input pipeline, diagnostics.

**Files:**
- [ ] `backends/win32/host_win32.c` — Win32 GDI window + DIB framebuffer
- [ ] `backends/win32/render_gdi.c` — GDI draw calls
- [ ] `backends/x11/host_x11.c` — X11 window (P1)
- [ ] `tests/python_abi/test_session.py` — vtable driver
- [ ] `tests/fuzzer/fuzzer.c` — ABI fuzz target

**Core integration:**
- `abi_input_push_event()` replacing `abi_ui_push_event` (P0-8)
- `abi_input_action_pressed()` for slot 23 callbacks (P0-9)
- `abi_input_begin_frame()` at top of `kaintana_begin_frame` (P0-10)
- `kain_host_get()` framebuffer access in Win32 backend (P0-4)
- `kain_virtual_reserve_and_commit()` for arena backing (P0-17)
- `kain_handle_table_acquire()` for stable key mapping (P0-14/15)
- `kain_diagnostic_create()` for error reporting (P0-11)
- `renderer_session_boot()` for backend resolution (P0-5)
- Register `"ui.kaintana"` service key (P0-18)

**Verification:** Win32 backend creates a real window with GDI output. Null backend and Win32 backend produce identical layout output for the same input. Python ABI tests pass on both backends.

### Phase 3: Kain Bridge + Stdlib (Weeks 4-6)

**What:** 24 @extern bindings in Kain, world surface end-to-end, std::kaintana core.

**Files:**
- [ ] `stdlib/kaintana/core.kn` — 24 @extern bindings (1:1 with vtable slots)
- [ ] `stdlib/kaintana/theme.kn` — Color, Spacing, Theme, DEFAULT_THEME
- [ ] `stdlib/kaintana/layout.kn` — HStack, VStack, Grid, Padding
- [ ] `stdlib/kaintana/widgets.kn` — Button, Label, TextInput, Slider

**Compiler integration:**
- Wire the `component` keyword through the 24-slot vtable
- World surface test: `surface native_ui => MyComponent` works end-to-end
- State persistence: `state_get_*`/`state_set_*` slots wired through compiler

**Verification:** Kain app with Button compiles and renders. Writing `surface native_ui => MyComponent` produces a real window. Python ABI tests drive the vtable from `core.kn` via ctypes.

### Phase 4: GPU Backends + Terminal (Weeks 6-10)

**What:** Vulkan, D3D12, WebGPU, terminal. Parallelizable.

**Files:**
- [ ] `backends/vulkan/render_vulkan.c` — ~2,000 lines
- [ ] `backends/d3d12/render_d3d12.c` — ~1,000 lines
- [ ] `backends/webgpu/render_webgpu.c` — ~1,200 lines
- [ ] `backends/terminal/surface_terminal.c` — ~300 lines

**Core integration:**
- Slot 18 (`get_gpu_extension`) for shader_canvas (P2-38)
- Slot 23 (`element_set_callback`) for event→callback dispatch (P2-39)
- GPU surface shim integration (P2-40)
- `kain_machine_pulse_start()` for animation timing (P1-29)
- `kain_machine_teleport_ptr()` for surface handoff (P1-35)
- `kain_machine_axiom_accept()` for GPU capability gating (axiom/know)

**Verification:** Each GPU backend renders the same KaintanaDrawData identically. Terminal backend proves zero-GPU path works. `KAINTANA_BACKEND=vulkan` env var selects GPU path.

### Phase 5: Stabilization + Proof (Weeks 10-14)

**What:** Z3 proofs, per-backend CI, snapshot testing, fuzz qualification, benchmark.

**Files:**
- [ ] `z3/box_math_proofs.yaml` — Layout bounds safety
- [ ] `z3/damage_proofs.yaml` — Pipeline state machine total
- [ ] `z3/arena_proofs.yaml` — Arena overflow safety
- [ ] `z3/hash_table_proofs.yaml` — No false negatives, O(1) lookup
- [ ] `tests/golden_images/` — Known-good renders for snapshot diffing

**Core integration:**
- `KAIN_PROFILE_SCOPE("kaintana_*")` on hot paths (P1-22)
- Extend CBMC arena harness with Kaintana grow/reset patterns (P2-44)
- ABI version check at startup via `version_check_abi_compatibility()` (P1-32)

**Verification:** All Z3 proofs pass. All CI backends compile on every commit. Fuzz suite runs 500K iterations with zero crashes. Snapshot tests pass across all backends.

### Phase 6: Archive KUIF (Week 14)

**What:** Archive old `src/ui/`, 25 headers from `include/`, rename `ui_v2/` to `ui/`.

**Actions:**
- [ ] `src/ui/` → `archive/legacy/ui/` (entire KUIF C runtime, 12,000+ lines)
- [ ] 25 UI headers from `include/` → `archive/legacy/ui_headers/`
- [ ] `include/` goes from 84 headers to 59 (pure core runtime, zero UI pollution)
- [ ] `src/ui_v2/` → `src/ui/` (kaintana becomes the canonical UI directory)
- [ ] Update `native_core_runtime.toml` to point at new file layout
- [ ] Run `py -3 scripts/python/update_runtime.py` to regenerate Bazel BUILD files

**Source:** `_KAINTANA.md` §XII — "The Archive."

---

## 10. Where Kaintana Deploys

### 10.1 The Backend Selection Chain

```c
KaintanaBackend* kaintana_select_backend(void) {
    // 1. Env var override (highest priority)
    const char* override = getenv("KAINTANA_BACKEND");
    if (override) {
        for each backend: if matching, probe and return
        fprintf(stderr, "KAINTANA_BACKEND=%s not available\n", override);
    }

    // 2. Capability-based fallback chain
    KaintanaBackendProbe probes[] = {
        // GPU backends
        { "vulkan",  REQUIRES: VULKAN_LOADER | DISPLAY,        probe_vulkan },
        { "d3d12",   REQUIRES: D3D12 | WIN32,                  probe_d3d12 },
        // Software backends
        { "win32",   REQUIRES: WIN32 | DISPLAY,                probe_win32 },
        { "x11",     REQUIRES: X11 | DISPLAY,                  probe_x11 },
        { "wayland", REQUIRES: WAYLAND | DISPLAY,              probe_wayland },
        { "macos",   REQUIRES: MACOS | DISPLAY,                probe_macos },
        // Universal
        { "terminal", REQUIRES: CONSOLE, FORBIDS: DISPLAY,     probe_terminal },
        // Always works
        { "testing", REQUIRES: 0,                               probe_testing },
    };

    for each probe in probes:
        if capabilities match AND probe() == 0:
            return load_backend(probe);

    return NULL;  // Unreachable: testing backend always succeeds
}
```

**Source:** `MASTER_OS_AND_CONTRACT.md` §4 — "Backend Selection Logic."

### 10.2 The Backend Contract (Every Backend Implements)

```c
int  kain_backend_init(const KainBackendConfig* config);
void kain_backend_shutdown(void);
void kain_backend_new_frame(KaintanaInput* input);
void kain_backend_render(const KaintanaDrawData* draw_data);
```

**Proof of minimality:** ImGui's null backend (102 lines) implements all 4 functions as no-ops. If a backend needs more than 4 functions, it's not a backend — it's leaking platform concerns into core.

**Source:** `MASTER_OS_AND_CONTRACT.md` §1 — "The 4-Function Contract."

### 10.3 The Input Funnel

Every platform backend fills a `KaintanaInput` struct before calling `begin_frame`:

```c
typedef struct KaintanaInput {
    float mouse_x, mouse_y;          // Logical points (pixels / dpi_scale)
    bool  mouse_down[5];             // Left, right, middle, x1, x2
    float scroll_dx, scroll_dy;      // Normalized (1.0 = one notch)
    bool  keys[256];                 // Key down state
    uint32_t input_chars[32];        // UTF-32 codepoints typed this frame
    int   input_char_count;
    bool  focus_gained;
    double delta_seconds;            // Time since last frame
    float display_width, display_height; // Logical points
    float scale_factor;              // DPI scale (physical / logical)
} KaintanaInput;
```

The 10 vtable-equivalent `Add*Event()` functions from ImGui's pattern are replaced by direct struct filling — simpler, faster, no function call overhead per event.

**Source:** `MASTER_OS_AND_CONTRACT.md` §2 — "Input Funnel Math," `MASTER_PLATFORM.md` §3 — "The Input Funnel Pattern."

---

## 11. Runtime Integration Summary

Kaintana gains the following proven-correct infrastructure FOR FREE by integrating with the core runtime instead of duplicating KUIF's mistakes:

| Core Subsystem | Proof Type | Count | What Kaintana Gets |
|---------------|------------|-------|---------------------|
| Arena allocator | CBMC | 833 assertions | O(1) frame cleanup, zero memory leaks |
| Arena allocation | Z3 | 6 proofs | Bump bounds, alignment, no overflow |
| Input system | Z3 | 2 proofs | Collision-free token dispatch, O(1) hash |
| Input event ring | CBMC (actor) | 5,676 assertions | FIFO order, capacity enforcement |
| Service registry | Z3 | 4 proofs | Perfect-hash O(1), spinlock safety |
| Machine stones | Z3 | 6 proofs | Pulse missed-beat, shatter bounds, teleport |
| Ownership | Z3 | 38 proofs | Observer count, state machine totality |
| Entangle | Z3 | 5 proofs | Text copy bounds, capacity limit |
| Convergence | Z3 | 5 proofs | Telemetry ring bounds, De Bruijn |
| Handles | Z3 | 4 proofs | Stale handle rejection, slot extraction |
| **Total** | CBMC + Z3 | **905 proofs** | **Inherited by Kaintana for free** |

**Source:** `MASTER_CONTRACT.md` Appendix A — "Proof Leverage Summary."

### Services Kaintana Registers

| Service Key | Status | Used By |
|-------------|--------|---------|
| `ui.kaintana` | New | Kaintana init, component surface registration |
| `platform.input` | Existing | Input dispatch via `abi_input_*` |
| `cpu.capabilities` | Existing | GPU backend probing |
| `machine.stones` | Existing | Pulse timing, teleport handoff |

### Diagnostics Subsystem Codes

| Range | Subsystem | Used By |
|-------|-----------|---------|
| 5000-5099 | Kaintana init/registration | `kaintana_init()` |
| 5100-5199 | Layout errors | `box_math.c` (overflow, min>max) |
| 5200-5299 | Damage pipeline errors | `damage.c` (heap overflow) |
| 5300-5399 | Render/attribute errors | `draw_pixels.c`, `tree.c` (invalid attr) |
| 5400-5499 | Backend errors | backends/ (init failure, capability miss) |

---

## 12. The Core Math Contracts

### 12.1 Memory — KaintanaNode Arena

```
sizeof(KaintanaNode)  = 32 bytes (2 per cache line)
sizeof(KaintanaLayout)= 48 bytes (1 per cache line)
sizeof(KaintanaDrawCmd)= 32 bytes (2 per cache line)

Arena growth: 1.5x geometric, initial 512 nodes
Frame markers: O(1) save/restore at begin_frame/end_frame
Hash table: 4096 slots, α_max = 0.0625, FNV-1a 64-bit + SplitMix64
Expected probes: 1.03 (successful), 1.067 (unsuccessful) — Z3-proven
```

**Source:** `MASTER_MEMORY_AND_ARENA.md` — All 37 formulas.

### 12.2 Layout — Two-Pass Flexbox

```
Phase 1 (bottom-up): compute desired sizes
  flex_basis → auto-min floor (CSS §4.5) → intrinsic content measurement
Phase 2 (top-down): arrange + distribute
  collect lines → first pass (tentative distribution + freeze)
  → second pass (final redistribution, proven 2-pass convergence)
  → justify-content → cross-axis alignment → line wrapping

Proof: 2 passes suffice for min/max convergence.
  Items frozen in pass 1 stay frozen. Remaining pool shrinks monotonically.
  No unfrozen item can hit a bound in pass 2.
```

**Source:** `MASTER_SPATIAL_LAYOUT.md` — All 49 formulas.

### 12.3 Damage — Three-Phase Pipeline

```
Phase 1 (PreUpdate):    structural changes (child order, visibility)
Phase 2 (Prepass):      bottom-up desired size (sort by depth descending)
Phase 3 (PostUpdate):   top-down arrange + paint (sort by depth ascending)

Dirty rect accumulator: 64-rect ceiling with overflow merging
Lazy sleep: skip frame if all heaps empty + no events + no pulses + host sleep
Cache hit: 1-slot layout cache with generation counter
```

**Source:** `MASTER_INVALIDATION_AND_DAMAGE.md` — All 7 sections.

### 12.4 Rendering — 16 Draw Primitives

```
Software path:  kaintana_execute_software() → write-pointer reserved
                → div255 integer SrcOver blend → DIB framebuffer
GPU path:       kaintana_cmds_to_vertex_buffer() → 16-byte vertex
                → DMA upload → vkCmdDrawIndexed

Dual-path architecture: typed commands (32B) → backend dispatch
Auto-merge: adjacent same-color rects merged at insertion (<50 commands/frame)
SDF corner coverage: fixed-point 8.8 radius, per-pixel SDF only at corners
```

**Source:** `MASTER_PIXELS_AND_GEO.md` — All 221 formulas, `MASTER_COLOR_AND_BLEND.md` — All blend modes.

### 12.5 Interaction — Hit-Testing and Spatial Queries

```
Point-in-rect:   px >= rx && px < rx+rw && py >= ry && py < ry+rh
Rect-overlaps:   a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y
Z-order:         reverse arena walk (last-drawn = topmost)
Active ID:       lock on pointer down, release on pointer up (ImGui pattern)
Spatial grid:    16x16 tile bins for sparse layouts, dense grid for small viewports
Spring physics:  pulse-driven critically-damped spring, all in Kain (not C)
```

**Source:** `MASTER_INTERACTION_AND_MOTION.md` — All 8 sections.

### 12.6 Typography

```
Scale:           pixelSize / (ascent - descent) — pixel-height scaling
Line height:     (ascent - descent + lineGap) × scale
Glyph advance:   stb_truetype's stbtt_GetPackedQuad semantics
Word wrap:       greedy blank/punct/other classifier (ImGui pattern)
Atlas packing:   stb_rect_pack skyline bottom-left fill, power-of-two sizing
UV mapping:      u = x0/atlasW, v = y0/atlasH (pre-computed at bake time)
```

**Source:** `MASTER_TYPOGRAPHY.md` — Full reference.

### 12.7 Color and Blend

```
Storage:         Premultiplied RGBA uint32 (0xAARRGGBB)
Default blend:   SrcOver in sRGB space — NO gamma conversion for UI
                 out = src + dst × (1 - src.a)
div255 fast:     (x + 1 + (x >> 8)) >> 8  — integer-only, ±0.5 error
Hardware blend:  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA for GPU
16 blend modes:  Normal, Multiply, Screen, Overlay, Darken, Lighten,
                 ColorDodge, ColorBurn, HardLight, SoftLight, Difference,
                 Exclusion, Hue, Saturation, Color, Luminosity
                 (Vello's complete WGSL implementation)
Opacity stack:   Multiplicative accumulation, no temp render targets for UI
```

**Source:** `MASTER_COLOR_AND_BLEND.md` — Complete reference.

---

## Appendix: Risk Register

| Risk | Likelihood | Impact | Mitigation | Source |
|------|-----------|--------|------------|--------|
| Vtable exceeds 24 slots in first year | High | Breaking | Reserve slots 24-31 from day one. Document append-only policy. | `MASTER_GIT.md` §11 |
| Testing deferred to Phase 4 | Medium | Slow feedback | Null backend built in Phase 1 (100 lines). Python ABI tests in Phase 2. | `MASTER_GIT.md` §4 |
| Backend proliferation without maintainers | High | 85K-line graveyard | Named maintainers per backend. CI-per-backend. 6-month deprecation timer. | `MASTER_GIT.md` §11, nuklear evidence |
| Generated file churn (BUILD manifests) | Medium | CI noise | Run `update_runtime.py` automatically. Don't hand-edit Bazel files. | `MASTER_GIT.md` §5, slint Cargo.lock |
| Arena overflow in complex UIs | Medium | Crash | 64KB default arena, growable via virtual_alloc. Fuzz test at 10,000+ nodes. | `MASTER_CONTRACT.md` §6 Risk #4 |
| `KAINTANA_BACKEND` env var collision with `RENDERER_BACKEND` | Low | Confusion | Document override chain: `KAINTANA_BACKEND` → capability probe → fallback. | `MASTER_OS_AND_CONTRACT.md` §4 |
| Core runtime ABI change breaks arena.h | Low | Compile error | arena.h is core runtime public ABI (ABI v0.1.0). Changes require ABI version bump. | `MASTER_CONTRACT.md` §6 Risk #1 |

---

*End of MASTER_KAINTANA.md — The definitive design document for Kaintana, Kain's clean-slate UI rendering substrate. Synthesized from 17 source documents across 13 master reference domains. 4 C files. 1 public header. 34 public functions. 24 vtable slots. 4-layer architecture. 6-phase plan. 905 Z3/CBMC proofs inherited from the core runtime. Total C substrate: ~3,300 lines.*
