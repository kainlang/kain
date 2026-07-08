# MASTER_ARCHITECTURE.md — The Definitive Kaintana Architecture

**Date:** 2026-06-27
**Status:** THE capstone document. Synthesis of 18 research documents, 6 framework analyses, 8 master math docs, and 2 API analyses.
**Synthesis of:** `_ARCHITECTURE.md`, `_KAINTANA.md`, `MASTER_CONTRACT.md`, `MASTER_PLATFORM.md`, `MASTER_GIT.md`, `MASTER_API.md`, `MASTER_MEMORY_AND_ARENA.md`, `MASTER_PIXELS_AND_GEO.md`, `MASTER_SPATIAL_LAYOUT.md`, `MASTER_INVALIDATION_AND_DAMAGE.md`, `MASTER_OS_AND_CONTRACT.md`, `MASTER_TYPOGRAPHY.md`, `MASTER_COLOR_AND_BLEND.md`, `API_ANALYSIS_P1.md`, `API_ANALYSIS_P2.md`, `_ASSESSMENT_SUBSTRATE.md`, `_ASSESSMENT_GRAPHICS.md`, `_ASSESSMENT_PLATFORM.md`
**Purpose:** File-by-file architecture, creation order, and rationale for Kaintana — the clean-slate replacement for KUIF.

---

## Table of Contents

1. [Architecture Overview — The 4-Layer Stack](#1-architecture-overview--the-4-layer-stack)
2. [The File Layout — Every File Explained](#2-the-file-layout--every-file-explained)
3. [Creation Order — The Build Sequence](#3-creation-order--the-build-sequence)
4. [What KUIF Got Wrong](#4-what-kuif-got-wrong)
5. [The API Schema — Conventions From 9 Frameworks](#5-the-api-schema--conventions-from-9-frameworks)
6. [File Dependency Graph](#6-file-dependency-graph)
7. [The Risk Register](#7-the-risk-register)
8. [The Archive Plan](#8-the-archive-plan)
9. [Verification Strategy](#9-verification-strategy)

---

## 1. Architecture Overview — The 4-Layer Stack

Kaintana's architecture emerged from studying **9 UI frameworks** (ImGui, nuklear, clay, microui, yoga, egui, slint, vello, OpenSwiftUI) across **36,651 commits** and cross-referencing **86 integration points** into the Kain core runtime. The result is a strict 4-layer stack where each layer has zero knowledge of the layers above it:

```
LAYER 3: std::kaintana (Kain .kn code)
──────────────────────────────────────────────────
  24-slot vtable · Kain compiler emits calls
──────────────────────────────────────────────────
LAYER 2: kaintana.h — THE ABI contract (1 header)
──────────────────────────────────────────────────
  4 C files · no platform headers · pure math
──────────────────────────────────────────────────
LAYER 1: C substrate (tree.c, box_math.c, damage.c, draw_pixels.c)
──────────────────────────────────────────────────
  ImGui 4-function backend contract
──────────────────────────────────────────────────
LAYER 0: OS backends (backends/win32/, backends/null/, etc.)
```

### Layer 3: std::kaintana (Kain)

Authored in pure `.kn` code. Widgets, layout composition, themes, animation — **zero C code**. The `component` keyword emits LLVM IR that calls through the 24-slot vtable. Every widget is a pure Kain component using `state`, `patch`, `pulse`, and JSX.

### Layer 2: kaintana.h (The ABI Contract)

**One public header.** Contains the 24-slot `KainComponentSurface` vtable, all public types (`kt_Rect`, `kt_Color`, `kt_Cmd`, `kt_Input`), and the backend registry. This is the **blood-brain barrier** between Kain and C. The vtable slot order is **absolute** — never reorder, only append.

### Layer 1: C Substrate (4 .c files)

Four files. No platform headers. No `<windows.h>`, no `<X11/>`, no `<vulkan/>`. Pure C11 math that compiles on any freestanding compiler:

| File | Lines | What it does | Platform deps? |
|------|-------|-------------|:--------------:|
| `tree.c` | ~300 | ABI ingestion — element_begin/end, set_attr, stable key hash | **Zero** |
| `box_math.c` | ~400 | Two-pass flexbox layout — Yoga's 49 formulas, arena alloc | **Zero** |
| `damage.c` | ~250 | Three-phase dirty pipeline — Slate's cascade + lazy sleep | **Zero** |
| `draw_pixels.c` | ~500 | 16 draw primitives, write-pointer, auto-merge, SDF eval | **Zero** |

### Layer 0: OS Backends

Platform-specific code **exiled** to `backends/`. Every backend implements exactly 4 functions (from ImGui's proven pattern):

```c
int  kain_backend_init(KainBackendConfig* cfg);
void kain_backend_shutdown(void);
void kain_backend_new_frame(KaintanaInput* input);     // platform
void kain_backend_render(KaintanaDrawData* draw_data);  // renderer
```

Win32 is P0. Null is P0. GPU backends are Phase 5.

---

## 2. The File Layout — Every File Explained

```
X:\runtime\native\src\ui_v2\
│
├── kaintana.h               ← THE ONE public header (400-600 lines)
├── internal.h               ← Private shared types (300-400 lines)
│
├── tree.c                   ← ABI ingestion: element_begin/end, set_attr, stable key hash
├── box_math.c               ← Layout engine: two-pass flex, constraint solving (pure math)
├── damage.c                 ← Invalidation: three-phase dirty pipeline, lazy sleep
├── draw_pixels.c            ← Rendering: 16 draw primitives, write-pointer, auto-merge
│
├── arena.c / arena.h        ← Grow-only arena allocator (wraps core/arena.h)
├── hash_table.c / hash_table.h  ← FNV-1a stable key lookup (O(1) find-or-create)
├── attr_table.c             ← Data-driven attribute → invalidation mapping (ZERO #defines)
│
├── backends/                ← Platform code exiled here
│   ├── null/host_null.c     ← P0: Headless testing, ~100 lines
│   ├── win32/
│   │   ├── host_win32.c     ← P0: Win32 window, DIB framebuffer, message pump
│   │   └── render_gdi.c     ← P0: GDI software renderer
│   ├── x11/host_x11.c       ← P1: Linux X11
│   ├── wayland/host_wayland.c ← P1: Linux Wayland
│   ├── macos/host_macos.m   ← P1: macOS Cocoa
│   ├── terminal/host_terminal.c ← P5: ANSI TUI
│   ├── wasm/host_wasm.c     ← P5: WebAssembly
│   ├── vulkan/render_vulkan.c ← P5: GPU compute pipeline
│   ├── d3d12/render_d3d12.c  ← P5: Windows GPU
│   └── webgpu/render_webgpu.c ← P5: Cross-platform GPU
│
├── std/                     ← Kain stdlib (develop HERE first, then promote)
│   ├── core.kn              ← P3: 24 @extern bindings, 1:1 with vtable
│   ├── theme.kn             ← P4: Color, Spacing, Theme, DEFAULT_THEME
│   ├── layout.kn            ← P4: HStack, VStack, Grid, Padding (pure Kain)
│   ├── widgets.kn           ← P4: Button, Label, TextInput, Slider, Checkbox
│   └── kaintana.kn          ← Re-export hub
│
├── tests/                   ← Verification
│   ├── python_abi/          ← P2: Python ctypes driving the vtable
│   ├── golden_images/       ← P6: Snapshot regression
│   ├── fuzzer/              ← P2: libFuzzer bombing element_set_attr
│   └── regression/          ← P6: C regression tests
│
├── z3/                      ← Proof packs
│   ├── box_math_proofs.yaml
│   ├── damage_proofs.yaml
│   ├── arena_proofs.yaml
│   └── hash_table_proofs.yaml
│
├── z___research/            ← All research docs (never deleted)
└── demos/                   ← Kain-authored UI demos (.kn, not .c)
```

### 2.1 kaintana.h — The One Public Header

**Source:** `_ARCHITECTURE.md`, `_KAINTANA.md`, `MASTER_API.md`, `API_ANALYSIS_P1.md`

**WHY:** Every C UI framework uses a single public header. ImGui's `imgui.h` survived 12 years unchanged. Nuklear's `nuklear.h` is the canonical single-header library. Clay's `clay.h` is ~4800 lines of pure API. **KUIF had 25 headers in `include/`** — a catastrophic organizational failure that MASTER_GIT proves is the #1 churn source.

**What it contains:**
- 24-slot `KainComponentSurface` vtable — **must match** `component_surface.h` exactly
- Public types: `kt_Rect`, `kt_Color`, `kt_Vec2`, `kt_Matrix`, `kt_Input`, `kt_Cmd`, `kt_Session`
- Registry functions: `kaintana_surface_register()`, `kaintana_surface_resolve()`
- Frame functions: `kt_make()`, `kt_free()`, `kt_begin()`, `kt_end()`, `kt_present()`, `kt_should_close()`
- Input functions: `kt_input_mouse_move()`, `kt_input_mouse_down()`, etc. (7 total)
- Element functions: `kt_row()`, `kt_end_row()`, `kt_text()` (2 + text)
- Layout functions: `kt_width()`, `kt_height()`, `kt_pad()`, `kt_gap()`, `kt_direction()` (6 total)
- Style functions: `kt_fill()`, `kt_stroke()`, `kt_radius()`, `kt_opacity()`, `kt_font()` (5 total)
- State functions: `kt_put()`, `kt_put_f()`, `kt_put_s()`, `kt_get()`, `kt_get_f()`, `kt_get_s()` (6 total)
- Draw query: `kt_cmd_count()`, `kt_cmd_get()` (2 total)

**Total: 34 public functions** — down from KUIF's **174 `abi_ui_*` exports**.

**API naming (from API_ANALYSIS_P1):**
- Prefix: `kt_` — 3 characters, Goldilocks zone (matches `nk_`, `mu_`, distinct from `Im`, `YG`)
- Types: PascalCase — `kt_Rect`, `kt_Color`, `kt_Cmd`
- Functions: snake_case verb-noun — `kt_begin()`, `kt_end()`, `kt_make()`
- Internal: `__` prefixed — `kt__node_find()`, `kt__layout_pass1()`
- Named colors (passed as strings): `"bg"`, `"surface"`, `"accent"`, `"text"`, `"border"`, `"button"`, etc.

**Size target:** ~400-600 lines (types + 24-slot vtable + registry + 34 public functions).

**The 10-year-old test (from MASTER_API.md):**
- "What does `kt_make` do?" -> "Makes something."
- "What does `kt_fill` do?" -> "Fills something with color."
- "What does `kt_width` do?" -> "Sets the width."
- "What does `kt_put` do?" -> "Puts something somewhere."
If a 10-year-old can't guess, rename it.

### 2.2 internal.h — The Private Header

**Source:** `_ARCHITECTURE.md`, `MASTER_MEMORY_AND_ARENA.md`, `MASTER_INVALIDATION_AND_DAMAGE.md`

**WHY:** Shared by exactly 4 `.c` files. No need for a separate `include/` directory. Same pattern as Slint's `internal/core/`.

**What it contains:**
- `KaintanaNode` (32 bytes) — the arena element. 2 per cache line.
- `KaintanaLayout` (48 bytes) — constraint solver output. SoA separate from nodes.
- `KaintanaDrawCmd` (32 bytes) — typed render command. 2 per cache line.
- `KaintanaSession` — the one big context. Arena, heaps, platform vtable.
- `KaintanaDamagePipeline` — three-phase heap set.
- `KaintanaArena` arena wrapper — delegates to `kain_arena_alloc_lo()` from core.
- `KaintanaHashTable` — open-addressing FNV-1a stable key lookup.
- `KaintanaAttrDef` — data-driven attribute -> invalidation mapping.
- `KaintanaStateMap` — key -> typed value hash map for component state.

**Size target:** ~300-400 lines.

**Cache line alignment (from MASTER_MEMORY_AND_ARENA.md):**

| Struct | Size | Per cache line | Why this size |
|--------|------|:--------------:|---------------|
| `KaintanaNode` | **32 bytes** | 2 | Matches Slate's `FWidgetProxy`. Topology + invalidation + layout arena index |
| `KaintanaDrawCmd` | **32 bytes** | 2 | Matches ImGui's write-pointer packing. Typed command (fill/stroke/text/clip) |
| `KaintanaLayout` | **48 bytes** | 1 | SoA from nodes. Layout solver iterates this arena independently |

### 2.3 tree.c — ABI Ingestion

**Source:** `_ARCHITECTURE.md`, `MASTER_CONTRACT.md` (Path A+B), `_KAINTANA.md`

**WHY:** Receives vtable calls from the Kain compiler's LLVM codegen. Manages the node arena. Reconciles stable keys.

**Core runtime integration (from MASTER_CONTRACT.md):**
- **P0-1:** Arena allocation via `kain_arena_alloc_lo()` — replaces ALL `malloc`/`free`
- **P0-2:** Vtable contract alignment — `kaintana.h` includes `component_surface.h`
- **P0-3:** Surface registration — `kain_component_surface_register("kaintana", &vtable)`
- **P0-8/9/10:** Input via `abi_input_push_event()` / `abi_input_begin_frame()` / `abi_input_action_pressed()`
- **P0-11:** Diagnostics via `kain_diagnostic_create()` with `KAIN_DIAG_SUBSYSTEM_UI`
- **P0-14/15:** Handle table via `kain_handle_table_acquire()` for stable key -> node
- **P0-16:** FNV-1a hash matching input_system's hash

**Public API:**
```c
// Element tree (vtable slots 2-4)
kt_Session* kt_make(const char* name, int w, int h);
void        kt_free(kt_Session* s);
int         kt_row(kt_Session* s, int parent, const char* kind, const char* key);
void        kt_end_row(kt_Session* s);
void        kt_text(kt_Session* s, int elem, const char* text);

// Attributes (vtable slots 5-7)
void kt_fill(kt_Session* s, int elem, const char* color);
void kt_stroke(kt_Session* s, int elem, const char* color, float w);
void kt_radius(kt_Session* s, int elem, float r);
void kt_opacity(kt_Session* s, int elem, float a);
void kt_font(kt_Session* s, int elem, float size);
void kt_width(kt_Session* s, int elem, float w);
void kt_height(kt_Session* s, int elem, float h);
void kt_pad(kt_Session* s, int elem, float all);
void kt_pad_xy(kt_Session* s, int elem, float x, float y);
void kt_gap(kt_Session* s, int elem, float gap);
void kt_direction(kt_Session* s, int elem, int dir);

// State (vtable slots 8-9, 19-22)
void        kt_put(kt_Session* s, const char* key, int64_t v);
void        kt_put_f(kt_Session* s, const char* key, double v);
void        kt_put_s(kt_Session* s, const char* key, const char* v);
int64_t     kt_get(kt_Session* s, const char* key, int64_t fallback);
double      kt_get_f(kt_Session* s, const char* key, double fallback);
const char* kt_get_s(kt_Session* s, const char* key, const char* fallback);
```

**Dependencies:** `kaintana.h`, `internal.h`, `hash_table.h`, `arena.h`

**Frame lifecycle (from MASTER_MEMORY_AND_ARENA.md §1.3):**
```c
void kt_begin(kt_Session* s, double delta_ms) {
    // Arena marker: saves bump ptrs
    kain_frame_set_marker(&s->arena);
    // Input begin_frame
    abi_input_begin_frame(s->input_sid, delta_ms);
}

void kt_end(kt_Session* s) {
    kaintana_process_damage(s);     // damage.c: three-phase pipeline
    kaintana__draw_generate(s);     // draw_pixels.c: emit commands
    kain_frame_release_to_last_marker(&s->arena);  // O(1) cleanup
}
```

### 2.4 box_math.c — The Layout Engine

**Source:** `MASTER_SPATIAL_LAYOUT.md`, `MASTER_MEMORY_AND_ARENA.md`, `_ARCHITECTURE.md`

**WHY:** Pure flexbox math. Zero platform headers. Two-pass constraint solving from Yoga's `CalculateLayout.cpp` (~2800 lines). **49 unique formulas** extracted across 6 frameworks.

**Core runtime integration:**
- Arena allocation via `kain_arena_alloc_lo()` for the layout arena (SoA)
- Profile scopes: `KAIN_PROFILE_SCOPE("kaintana_desired_sizes")`
- Tier-gated assertions: `KAIN_RUNTIME_DIAG_ENABLED()`

**Two-pass algorithm (from MASTER_SPATIAL_LAYOUT.md §2.1):**

```
Phase 1: Bottom-up (kaintana_compute_desired_sizes)
  For each node (children first):
    1. Resolve flex-basis cascade (style.flexBasis > axis width/height > 0)
    2. Resolve percentages against parent dimension
    3. Measure intrinsic size (content) or apply StretchFit
    4. Cache result in 1-slot layout cache (generation-tagged)

Phase 2: Top-down (kaintana_arrange_children)
  For each flex container:
    1. Collect flex lines (kaintana_collect_flex_lines)
    2. Distribute free space - Pass 1 (kaintana_distribute_free_space_first_pass)
       - Tentative distribution to all flexible items
       - Freeze items that hit min/max bounds
       - 2 passes max (proven: monotonic convergence)
    3. Distribute free space - Pass 2 (kaintana_distribute_free_space_second_pass)
       - Final distribution to unfrozen items only
       - Prove: no item can hit a bound in pass 2
    4. Justify main axis (kaintana_justify_main_axis)
    5. Align cross axis per line (kaintana_align_child_in_cross_axis)
```

**Key formulas (all from MASTER_SPATIAL_LAYOUT.md):**
- Flex-basis cascade (§1.1.1): `flex_basis = explicit > axis_dim > 0`
- Grow distribution (§1.2.2): `child_i = base_i + (grow_i/sum_grow) * remaining`
- Shrink distribution (§1.2.3): `child_i = base_i + remaining * shrink_i * base_i / sum(scaled_shrink)`
- Auto-minimum floor (§3.2.1): CSS §4.5 — prevents `flex:1` text from collapsing to width 0
- Clamp (§3.1.1): `min(max(proposal, min), max)` — the single most common operation
- Padding/border floor (§3.1.2): `final = max(measured, padding+border)`

**Cache:** 1-slot layout cache per node. Generation-tagged. Start with 1 slot (most nodes get same constraint every frame), profile to add more.

**Dependencies:** `kaintana.h`, `internal.h`, `arena.h`

### 2.5 damage.c — The Invalidation Pipeline

**Source:** `MASTER_INVALIDATION_AND_DAMAGE.md`, `_ARCHITECTURE.md`, `MASTER_CONTRACT.md`

**WHY:** Three-phase dirty pipeline from Slate. 64-rect damage accumulator from Clay. Lazy sleep optimization. Only processes dirty nodes — not the full tree.

**Core runtime integration:**
- Input begin_frame as first phase trigger
- Deferred free list for damage tracking (P1-33)
- Atomic state flags for dirty rect synchronization (P1-36)

**Three-phase pipeline (from MASTER_INVALIDATION_AND_DAMAGE.md §2):**

```
Phase 1: PreUpdate (structural changes)
  - Child order changes
  - Visibility flips
  - Attribute registration changes
  - Complexity: O(m1) where m1 = pre_update_count

Phase 2: Prepass (bottom-up sizing)
  - Sorted by depth descending (children first)
  - Desired size recalculation via box_math.c
  - If size changed -> parent needs re-arrange
  - Complexity: O(m2 x avg_children)

Phase 3: PostUpdate (top-down arrange + paint)
  - Sorted by depth ascending (parents first)
  - Generate draw commands via draw_pixels.c
  - Complexity: O(m3 x draw_cmds_per_node)
```

**Reason cascade (from MASTER_INVALIDATION_AND_DAMAGE.md §1.3):**
```
Layout      -> also sets Prepass | Paint
Volatility  -> also sets Paint
ChildOrder  -> also sets Prepass | Layout
Prepass     -> also sets Layout | Paint
Visibility  -> also sets Prepass | Layout (if collapsed)
```

**Lazy sleep condition (from MASTER_INVALIDATION_AND_DAMAGE.md §5):**
```c
bool kaintana_should_sleep(KaintanaSession* s) {
    return s->damage_pipeline.is_clean          // No dirty nodes
        && s->event_queue.count == 0            // No pending events
        && !s->has_active_pulses                // No running animations
        && s->host->should_close_signal;        // Platform says no events
}
```

**Dependencies:** `kaintana.h`, `internal.h`

### 2.6 draw_pixels.c — The Renderer

**Source:** `MASTER_PIXELS_AND_GEO.md`, `MASTER_COLOR_AND_BLEND.md`, `_ARCHITECTURE.md`

**WHY:** 16 draw primitives. Write-pointer reservation (ImGui pattern). Auto-merge at insertion. Software rasterizer doubles as headless test backend.

**Core runtime integration:**
- Strict-aliasing-safe pixel ops via `memcpy` (dual-pixel fill, not `uint64_t*` casts)
- Profile scopes: `KAIN_PROFILE_SCOPE("kaintana_present")`
- `kain_arena_alloc_lo()` for draw command arena

**16 draw primitives:**
```c
void kaintana_draw_fill_rect(renderer, kt_Rect rect, kt_Color color);
void kaintana_draw_fill_rounded_rect(renderer, kt_Rect, float radius, kt_Color);
void kaintana_draw_stroke_rect(renderer, kt_Rect, float thickness, kt_Color);
void kaintana_draw_fill_circle(renderer, kt_Point, float radius, kt_Color);
void kaintana_draw_stroke_circle(renderer, kt_Point, float radius, float thickness, kt_Color);
void kaintana_draw_blit(renderer, kt_Rect src, kt_Rect dst, int64_t texture_id);
void kaintana_draw_text(renderer, kt_Point pos, const char* text, int64_t font, float size, kt_Color);
void kaintana_draw_gradient_rect(renderer, kt_Rect, const kt_Color* stops, int count);
void kaintana_draw_blur(renderer, kt_Rect, float radius);
// Clip stack (max 16)
void kaintana_draw_push_clip(renderer, kt_Rect);
void kaintana_draw_pop_clip(renderer);
// Transform stack (max 16)
void kaintana_draw_push_transform(renderer, kt_Matrix);
void kaintana_draw_pop_transform(renderer);
```

**Write-pointer pattern (from MASTER_PIXELS_AND_GEO.md §8.4):**
```c
void kaintana_draw_batch_reserve(KaintanaDrawBatch* batch, int count) {
    // Grow geometrically (1.5x) if needed
    if (batch->count + count > batch->capacity) {
        int new_cap = batch->capacity + batch->capacity / 2;
        batch->buf = realloc(batch->buf, new_cap * sizeof(KaintanaDrawCmd));
        batch->capacity = new_cap;
    }
    batch->write_ptr = &batch->buf[batch->count];
    batch->count += count;
}

// Auto-merge at insertion (from MASTER_PIXELS_AND_GEO.md §8.3):
// If new command has same ClipRect + TextureId + sequential index range,
// merge into previous command instead of pushing new one.
```

**Color pipeline (from MASTER_COLOR_AND_BLEND.md):**
- `KaintanaDrawCmd.color` stored as **premultiplied uint32** (direct GPU upload, no per-pixel conversion)
- Named colors resolved via `kaintana_resolve_color("accent")` -> premultiplied uint32
- Hex colors (`"#21D4A1FF"`) parsed, converted to premultiplied
- SDF anti-aliasing via coverage = clamp(0.5 - distance, 0, 1) (Clay's pattern §4.3)
- Dual-pixel fill via `memcpy` (strict-aliasing safe, Z3-proven)

**Dependencies:** `kaintana.h`, `internal.h`

### 2.7 arena.c / arena.h — Arena Allocator

**Source:** `MASTER_CONTRACT.md` (Path A), `MASTER_MEMORY_AND_ARENA.md`

**WHY:** Wraps `core/arena.h` — the CBMC-proven (833 assertions) arena allocator. No `malloc` per node. Per-frame O(1) cleanup.

**Core runtime integration (from MASTER_CONTRACT.md §4 Path A):**
```c
void kt_begin(kt_Session* s, double delta_ms) {
    kain_frame_set_marker(&s->arena);          // save bump ptrs
}

void kt_end(kt_Session* s) {
    kain_frame_release_to_last_marker(&s->arena);  // O(1) rollback
}

KaintanaNode* kaintana_node_alloc(KaintanaSession* s) {
    return (KaintanaNode*)kain_arena_alloc_lo(
        &s->arena, sizeof(KaintanaNode), _Alignof(KaintanaNode));
}
```

**Arena sizing (from MASTER_MEMORY_AND_ARENA.md §1.2):**
- Initial capacity: 512 nodes (~16KB for 32-byte nodes)
- Growth factor: 1.5x (33% waste vs 2x's 50%)
- Growth method: `kain_virtual_reserve_and_commit()` for large dynamic arenas
- Frame markers: max depth 8 (`KAIN_FRAME_MAX_DEPTH`)

### 2.8 hash_table.c / hash_table.h — Stable Key Hash Table

**Source:** `MASTER_MEMORY_AND_ARENA.md` (§3-4), `MASTER_CONTRACT.md` (Path B)

**WHY:** O(1) find-or-create for stable key reconciliation. FNV-1a 64-bit + SplitMix64 post-processing. Power-of-two capacity with open-addressing.

**Hash algorithm (from MASTER_MEMORY_AND_ARENA.md §4):**
```c
uint64_t kaintana_hash_stable_key(const char* key) {
    uint64_t hash = 0xcbf29ce484222325;  // FNV offset basis
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 0x100000001b3;            // FNV prime
    }
    // SplitMix64 post-processing (proven bijective by Z3)
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111eb;
    hash ^= hash >> 31;
    return hash;
}
```

**Table sizing (from MASTER_MEMORY_AND_ARENA.md §3):**
- Capacity: 4096 (2^12, power of two for bitwise AND mask)
- Max load: 256 entries (alpha = 0.0625 — deliberately very sparse)
- Expected probes at alpha=0.0625: 1.03 successful, 1.067 unsuccessful
- 99.9th percentile: 3 probes
- All bounds Z3-proven

### 2.9 attr_table.c — Attribute System

**Source:** `_ARCHITECTURE.md`, `MASTER_GIT.md` (§10)

**WHY:** Data-driven attribute -> invalidation mapping. **Zero `#define` constants.** KUIF had 18 `#define UI_COLOR_*` and 11 `#define UI_*_SIZE` — every one was hardcoded and unmovable. Kaintana's attributes are strings resolved through a typed hash table.

**How it works:**
- Each attribute name (e.g., `"fill_color"`, `"width"`) maps to an `AttrKey` enum value
- The mapping is data-driven: `{key: "fill_color", invalidation: LAYOUT|PAINT, type: STRING}`
- Setting an attribute marks the node dirty with the correct invalidation reason
- No hardcoded if-ladders — just hash lookups through `kaintana__attr_lookup()`

---

## 3. Creation Order — The Build Sequence

Derived from MASTER_GIT.md's 6-phase plan, cross-referenced with every master doc's dependency requirements.

### Phase 1: Core C Substrate (NOW)

**Order is hard — each file depends on the previous ones.**

#### Step 1: kaintana.h + internal.h

**What:** THE one header (400-600 lines) + private types (300-400 lines).

**Public API (from MASTER_API.md):**
```c
// Types
typedef struct { float x, y; }             kt_Vec2;
typedef struct { float x, y, w, h; }       kt_Rect;
typedef struct { float r, g, b, a; }       kt_Color;
typedef struct { float m[6]; }             kt_Matrix;
typedef struct { float x, y; int b[5]; ... } kt_Input;
typedef struct { kt_CmdType type; kt_Rect bounds; uint32_t color; ... } kt_Cmd;
typedef struct kt_Session kt_Session;  // opaque

// 24-slot vtable + 34 public functions (see §2.1)
```

**Core integration (from MASTER_CONTRACT.md):**
- `kaintana.h` must include `component_surface.h` — single source of truth
- Slot order must match `component_surface.h` **exactly** — reordering breaks compiled Kain
- Reserve slots 24-31 for future expansion

**Test strategy:** Compile test. Verify `sizeof(KaintanaNode) == 32`, `sizeof(KaintanaDrawCmd) == 32`, `sizeof(kainComponentSurface) == 24 * sizeof(void*)`.

#### Step 2: tree.c

**What:** ABI ingestion. ~300 lines.

**Core integration (from MASTER_CONTRACT.md Paths A+B+C):**
- Arena: `kain_arena_alloc_lo()` in `kaintana_node_alloc()`
- Vtable: implement all 24 slots, register via `kain_component_surface_register()`
- Input: `abi_input_push_event()` replaces `abi_ui_push_event()`
- FNV-1a hash: same function as `input_system.c`
- Handles: `kain_handle_table_acquire()` for stable key -> node

**Dependencies:** `kaintana.h`, `internal.h`, `hash_table.h`, `arena.h`

**Test strategy:** Python ctypes test driving all 24 vtable slots. Verify 10,000 element_begin/end calls don't leak arena. Verify stable key reconciliation: same key = same node_id across frames.

#### Step 3: box_math.c

**What:** Layout engine. ~400 lines.

**Core integration (from MASTER_CONTRACT.md):**
- Arena: layout arena via `kain_arena_alloc_lo()`
- Profiling: `KAIN_PROFILE_SCOPE("kaintana_desired_sizes")`
- Generation counter for layout cache

**Dependencies:** `kaintana.h`, `internal.h`, `arena.h`

**Test strategy:** Yoga's test pattern — feed known layout inputs, assert known layout outputs. 180+ test cases from Yoga's `tests/generated/`. All tests headless (pure float math, no window needed).

**Z3 proofs (from MASTER_PIXELS_AND_GEO.md, MASTER_SPATIAL_LAYOUT.md):**
- `box_math_proofs.yaml`: bounds safety, no overflow, correct clamping
- Two-pass convergence proof (from MASTER_SPATIAL_LAYOUT.md §2.1)
- Auto-minimum floor prevents zero-width text (CSS §4.5, proven by every shipping browser)

#### Step 4: damage.c

**What:** Invalidation pipeline. ~250 lines.

**Core integration (from MASTER_CONTRACT.md):**
- Phase 0: input begin_frame `abi_input_begin_frame()` triggers pipeline
- Phase 1: PreUpdate — structural changes (child order, visibility)
- Phase 2: Prepass — bottom-up desired sizes → calls box_math.c
- Phase 3: PostUpdate — top-down arrange + paint → calls draw_pixels.c
- Lazy sleep: `kaintana_should_sleep()` — skip frame when nothing changed

**Dependencies:** `kaintana.h`, `internal.h`

**Test strategy:** Slate's pattern — mark nodes dirty, verify only dirty nodes processed. 10,000-node tree, 200 dirty nodes -> verify O(200) work, not O(10,000). Heap insertion sort verified by expectation.

**Z3 proofs:**
- `damage_proofs.yaml`: pipeline state machine total, no orphan dirty nodes

#### Step 5: draw_pixels.c

**What:** Renderer. ~500 lines.

**Core integration (from MASTER_CONTRACT.md):**
- Strict-aliasing-safe pixel ops (memcpy, not uint64_t*)
- Profile scopes on hot paths
- Write-pointer reservation + auto-merge

**Dependencies:** `kaintana.h`, `internal.h`

**Test strategy:** Headless rendering to CPU buffer. Compare 16 draw primitives against golden images. Fuzz: random element_set_attr_* -> verify no crash, arena integrity.

**Z3 proofs:**
- `draw_proofs.yaml`: draw command count bounded (<50 per frame target), merge correctness
- Strict-aliasing: `memcpy` dual-pixel fill proven safe (from MASTER_PIXELS_AND_GEO.md)

### Phase 2: Backend + Testing (Week 1-2)

#### Step 6: backends/null/host_null.c

**What:** ~100 lines. Headless testing backend. Proves the 4-function contract is minimal.

**Core integration:** Implements `kaintanaBackend` vtable with no-ops. Self-contains in-memory framebuffer for CI.

**WHY first (from MASTER_GIT.md §4):** Testing is ALWAYS deferred — egui waited 6 years, slint waited 4. Build it first. The software renderer IS the testing backend — same `uint32_t*` framebuffer, no GPU needed.

**Test strategy:** CI compiles and runs all tests through null backend. No DISPLAY, no GPU, no desktop session required.

#### Step 7: backends/win32/host_win32.c + render_gdi.c

**What:** ~800 lines combined. Win32 window + GDI software renderer.

**Core integration (from MASTER_CONTRACT.md):**
- `kainHostVTable::get_framebuffer()` — get DC/DIB pointer
- `abi_input_push_event()` — replacement for old `abi_ui_push_event`
- `kain_virtual_reserve_and_commit()` — for arena backing buffer
- `KaintanaDrawCmd` iteration — render each command to DIB -> BitBlt

#### Step 8: tests/

**What:** Python ctypes ABI tests + libFuzzer targets.

**Core integration:**
- `tests/python_abi/` — drive 24-slot vtable from Python. Generate 10,000 nodes, run frame, assert layout math.
- `tests/fuzzer/` — random `element_set_attr_string()` calls, verify arena integrity.
- `tests/regression/` — C tests for specific layout/invalidation/render bugs.

### Phase 3: std::kaintana::core.kn (Week 3-4)

**What:** 24 @extern bindings mapping 1:1 to `kaintana.h` vtable slots.

**Core integration (from MASTER_CONTRACT.md §4 Path B):**
- `component` keyword surface: `surface native_ui => MyComponent` works end-to-end
- State persistence: `state_get_*`/`state_set_*` slots wired through compiler
- Frame loop auto-emission from compiler

**Test strategy:** `kain build` a Kain file with `<button>` -> `kain run` -> Oracle-verify window.

### Phase 4: Kaintana Widgets (Week 5-8)

**What:** theme.kn -> layout.kn -> widgets.kn. All pure Kain. ZERO C code for widgets.

**Core integration (from MASTER_CONTRACT.md):**
- `converge kaintana_backend()` block for capability-gated backend selection
- `pulse every 16ms` for animation timing
- `resonate World.field` for reactive state bindings

**Test strategy:** Kain-authored demos in `demos/` — widget_showcase.kn, dashboard.kn, terminal_tui.kn. All compiled through `kain build`.

### Phase 5: GPU Backends (Week 9-16)

**What:** Vulkan, D3D12, WebGPU, Terminal, WASM.

**Core integration (from MASTER_CONTRACT.md §4 Path B):**
- Slot 18 (`get_gpu_extension`) for `shader_canvas` backend
- Slot 23 (`element_set_callback`) for event -> callback dispatch
- Azim capability gating via `kain_machine_axiom_accept()`
- Surface shim resolver for Vulkan/D3D12/WebGPU

**Test strategy:** GPU rendering to in-memory buffer then grab via `vkGetBufferMemoryProperties` + memcpy. Compare against golden images. Oracle-verify window per backend.

### Phase 6: Stabilization (Week 17-20)

**What:** Snapshot testing, Z3 proofs, per-backend CI, archive legacy KUIF.

**Proof packs (from MASTER_MEMORY_AND_ARENA.md Appendix B):**
- `arena_proofs.yaml`: arena overflow/bounds safety
- `hash_table_proofs.yaml`: no false negatives, load factor bounded, O(1) lookup
- `box_math_proofs.yaml`: layout bounds safety, two-pass convergence
- `damage_proofs.yaml`: pipeline state machine total
- `draw_proofs.yaml`: draw command count bounded, merge correctness

---

## 4. What KUIF Got Wrong

Cross-referenced from all masters, organized by severity.

### Architectural Mistakes

| # | KUIF Mistake | Detail | What Kaintana Does |
|---|-------------|--------|-------------------|
| 1 | **25 headers in `include/`** | UI headers polluted the 84-header `include/` directory. `kain_geometry.h`, `kain_render_software.h`, `kain_host.h`, `kain_compositor.h`, `kain_font.h`, `kain_input.h`, `kain_surface.h` plus 18 ABI headers | **One header: `kaintana.h`**. The 24-slot vtable + all types. Nothing else. |
| 2 | **Twin header copies** | Every `kain_*.h` existed in BOTH `src/ui/kain/` AND `include/`. Two sources of truth. Sync script rotted. | **One source of truth.** `kaintana.h` lives once. No twin, no sync script. |
| 3 | **174 `abi_ui_*` exports** | ABI surface of 174 functions pinned in `ui_system.h`. Every function had to be maintained. Most were unused. | **24 vtable slots.** Append-only. Slots 24-31 reserved for future expansion. |
| 4 | **3,162-line `ui_system.c`** | The god-file. Session lifecycle, node CRUD, style/state, events, focus, IME, drag-drop, menus, dialogs, resources, fonts, callback dispatch — ALL in one file. | **4 focused .c files** at ~300-500 lines each. `tree.c` (ingestion), `box_math.c` (layout), `damage.c` (invalidation), `draw_pixels.c` (rendering). |
| 5 | **Widget code in C** | 1,559 lines in `widgets/ui_widget.c`. Hardcoded colors (`#define UI_COLOR_BUTTON 0xFF4A90D9`), hardcoded sizes (`#define UI_BUTTON_HEIGHT 28`). Every widget change required C recompile. | **All widgets in Kain** (`std::kaintana::widgets.kn`). C draws boxes, circles, text. It doesn't know what a "button" is. |
| 6 | **Hardcoded colors/sizes** | 18 `#define UI_COLOR_*` and 11 `#define UI_*_SIZE` in `ui_widget.h`. Every one was unmovable without editing C headers. | **Zero `#define` in C.** All values data-driven via attribute table. Theme values in Kain `theme.kn`. |
| 7 | **No arena integration** | Fixed arrays in session struct + `malloc`/`free` for oversize nodes. Zero CBMC proofs on KUIF's memory management. | `kain_arena_alloc_lo()` from `core/arena.h`. 833 CBMC assertions proven. Per-frame O(1) cleanup via markers. |
| 8 | **No core runtime integration** | Built own input (`abi_ui_push_event`), own hash tables, own memory. Ignored the Z3-proven `input_system.c`, `services.h`, `diagnostics.h`, `profile.h`, `handle.h`. | **Integrates with ALL of them.** Arena, input funnel, service registry, diagnostics, profiling, handle tables — all from core runtime. 905 Z3+CBMC proofs inherited. |
| 9 | **No testing backend for 6 years** | All tests required a real Win32 window. No headless CI. Manual visual inspection was the "test suite." | **`backends/null/host_null.c` built in Phase 2.** ~100 lines. CI runs all tests through it. Software renderer IS the headless test backend. |
| 10 | **Widget library in C with hardcoded theme** | `widgets/ui_widget.c` and `widgets/ui_widget.h` — deprecated but survived. Couldn't theme without C changes. | **No C widget library.** Theme values are passed as strings from Kain. `kt_fill(s, elem, "accent")` — the C layer doesn't know what "accent" means. |

### Process Mistakes

| # | KUIF Mistake | What Kaintana Does |
|---|-------------|-------------------|
| 11 | **No phase plan** | Grew organically over years. No build order. | **6-phase plan** derived from 36,651 commits of git history analysis (MASTER_GIT.md). |
| 12 | **No generation counter** | Layout cache had no invalidation guard. Stale cache bugs. | **Global generation counter** per frame (from Yoga's `incrementGenerationCount()`). Cache lines tagged with generation — stale ones are invisible. |
| 13 | **Renamed mid-stream** | KUIF started as one thing, became another, names changed. | **Freeze `kt_` prefix now.** Never rename. Renames cost 10-100x more than expected (MASTER_GIT.md §8). |
| 14 | **Testing always deferred** | Egui waited 6 years for headless testing. Slint waited 4. KUIF never had it. | **Null backend in Phase 2.** Before any GPU backend. Before win32 backend. The null backend IS the test harness. |

---

## 5. The API Schema — Conventions From 9 Frameworks

Derived from `API_ANALYSIS_P1.md` (9 frameworks, ~25K lines of header analyzed) and `MASTER_API.md`.

### Prefix

| Aspect | Decision | Rationale |
|--------|----------|-----------|
| Prefix | `kt_` | 3 characters = Goldilocks zone (matches `nk_`, `mu_`, distinct from `Im`, `YG`, `kn_`) |
| Types | PascalCase | `kt_Rect`, `kt_Color`, `kt_Context` (4/5 C frameworks use this) |
| Functions | snake_case | `kt_make()`, `kt_begin()`, `kt_end()` (shorter, matches `nk_`/`mu_`) |
| Internal | `kaintana__` | Double underscore separates public from private (matches Clay's `Clay__`, nuklear's `nk__`) |
| Macros | `KT_` ALL_CAPS | `KT_API`, `KT_VERSION` (matches `NK_`, `CLAY_`, `IMGUI_`) |
| Enums | PascalCase | `KaintanaCmdType`, not `KAINTANA_CMD_TYPE_ALL_CAPS` (modern convention) |

### Type Naming

| Concept | Name | Size | Source |
|---------|------|:----:|--------|
| Point | `kt_Vec2` | 8 bytes | Universal (ImVec2, nk_vec2, Clay_Vector2) |
| Rectangle | `kt_Rect` | 16 bytes | Universal (nk_rect, mu_Rect, egui::Rect) |
| Color | `kt_Color` | 16 bytes (float) / 4 bytes (packed) | Universal (nk_color, Clay_Color, mu_Color) |
| 2D affine | `kt_Matrix` | 24 bytes | Vello's Transform, Slate's FSlateRenderTransform |
| Draw command | `kt_Cmd` | 32 bytes | ImGui's ImDrawCmd, Clay_RenderCommand |
| Input bundle | `kt_Input` | varies | ImGuiIO, egui::RawInput |
| Session | `kt_Session` | opaque | mu_Context, Clay_Context, nk_context |
| Element ID | `int` | 4 bytes | mu_Id, Clay_ElementId (int is enough for 4B nodes) |

### Function Verbs

| Operation | Verb | Example | Source |
|-----------|------|---------|--------|
| Create | `make` | `kt_make()` | mu_init, Clay_Initialize |
| Destroy | `free` | `kt_free()` | nk_free, YGNodeFree |
| Begin frame | `begin` | `kt_begin()` | mu_begin, Clay_BeginLayout |
| End frame | `end` | `kt_end()` | mu_end, Clay_EndLayout |
| Present | `present` | `kt_present()` | ImGui (platform) |
| Set attribute | verb only | `kt_fill()`, `kt_width()` | Clay (no "Set" prefix) |
| Get state | `get` | `kt_get()` | nk_window_get_bounds, YGNodeLayoutGetLeft |
| Put state | `put` | `kt_put()` | Not in any framework — chosen for brevity |
| Query | `should` | `kt_should_close()` | ImGui (should_close) |

### The 10-Year-Old Rule (from MASTER_API.md)

> Every function must be obvious from its name alone. No abbreviations. No tricks. If a 10-year-old can't guess what it does, rename it.

---

## 6. File Dependency Graph

```
NO DEPS:
  kaintana.h                    [standalone — includes only <stdint.h>, <stdbool.h>, <component_surface.h>]

DEPENDS ON kaintana.h:
  internal.h                    [depends on kaintana.h types]

DEPENDS ON internal.h:
  arena.h/c                     [uses KaintanaSession, KaintanaArena]
  hash_table.h/c                [uses KaintanaNode (stable_key_hash)]

DEPENDS ON internal.h + kaintana.h + arena.h + hash_table.h:
  tree.c                        [element_begin/end, set_attr, state, frame lifecycle]
  box_math.c                    [two-pass layout — reads KaintanaNode/KaintanaLayout]
  damage.c                      [three-phase pipeline — reads KaintanaNode/KaintanaDamagePipeline]
  draw_pixels.c                 [16 primitives — reads KaintanaDrawCmd, KaintanaDrawBatch]

DEPENDS ON tree.c + box_math.c + damage.c + draw_pixels.c:
  backends/null/host_null.c     [implements KaintanaBackend vtable — ~100 lines, all no-ops]

DEPENDS ON internal.h + backends/null:
  backends/win32/host_win32.c   [implements KaintanaBackend vtable — ~800 lines]
  backends/win32/render_gdi.c   [consumes KaintanaDrawCmd array — ~400 lines]

DEPENDS ON kaintana.h + compiler codegen:
  std/core.kn                   [24 @extern bindings — 1:1 with vtable slots]
  std/theme.kn                  [uses std::kaintana::core — color values as strings]
  std/layout.kn                 [uses std::kaintana::core — HStack, VStack pure Kain math]
  std/widgets.kn                [uses std::kaintana::core + theme + layout — Button, Label, etc.]

DEPENDS ON std/ + compiler:
  demos/*.kn                    [Kain-authored demos — compiled through kain build]

DEPENDS ON null backend + kaintana.h:
  tests/python_abi/             [Python ctypes driving the vtable]
  tests/fuzzer/                 [libFuzzer bombing element_set_attr]

INDEPENDENT (can be built in any order):
  z3/                           [Z3 proof packs — same as core runtime's z3/ approach]
```

### Build order for Phase 1:

```
kaintana.h
  -> internal.h
    -> arena.h/c
    -> hash_table.h/c
      -> tree.c
      -> box_math.c
      -> damage.c
      -> draw_pixels.c
```

Total Phase 1: **1 header + 1 private header + 6 .c/h flies = ~2,000 lines** (vs. KUIF's Phase 1 which was already at 12,000+ lines)

---

## 7. The Risk Register

From MASTER_CONTRACT.md §6, MASTER_GIT.md §11, cross-referenced with every master doc.

### P0 Risks (Ship-Stoppers)

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|:----------:|:------:|------------|
| 1 | Vtable slot drift between `kaintana.h` and `component_surface.h` | **Medium** | **Critical** — broken Kain compilation | Single include (`#include <component_surface.h>`). Slot order changes = compile error. Python ABI test validates layout at runtime. |
| 2 | Arena API changes from core/arena.h | **Low** | **High** — box_math.c, tree.c break | `arena.h` is ABI v0.1.0. Kaintana includes it directly — changes caught at compile time. |
| 3 | Input system ABI changes | **Low** | **High** | `input_system.h` is ABI v0.1.0 with Z3-proven hashes. ABI check via `version_check_abi_compatibility()`. |
| 4 | Backend selection converge block not supported by LLVM codegen | **Medium** | **High** | Fall back to compile-time + code-time selection via `kt_backend_select()`. The C `renderer_session_boot()` also handles `RENDERER_BACKEND` env var for debugging. The env var is a DEBUG override only — production code must use `kt_backend_select()`. |
| 5 | Old `abi_ui_*` ABI calls retained from KUIF's `backends/win32/` | **Medium** | **Medium** | Systematic audit: replace every `abi_ui_push_event` with `abi_input_push_event`. Remove `#include "ui_system.h"` from backend files. |

### P1 Risks (Significant)

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|:----------:|:------:|------------|
| 6 | Arena capacity overflow in complex UIs | **Medium** | **Medium** | Start with 64KB default. Growable via `kain_virtual_reserve_and_commit()`. Fuzz at 10,000+ nodes. |
| 7 | Testing deferred past Phase 2 | **Medium** | **High** — egui waited 6 years | **Hard requirement:** null backend MUST exist before win32 backend. CI gates on null backend tests. |
| 8 | Backend proliferation without maintainers | **High** | **High** — nuklear's 85K-line graveyard | Named maintainers per backend. CI builds ALL backends. 6-month deprecation timer for unmaintained backends. |
| 9 | Backend independence qualification | **Guaranteed** | **Medium** — 0.5-0.7 coupling is healthy | Accept that contract changes ripple. Reserve expansion slots. Document vtable contracts. |
| 10 | Prefix rename | **Low** (just don't do it) | **Extreme** — yoga 700 files, nuklear 20K lines | **FREEZE `kt_` PREFIX BEFORE FIRST COMMIT.** Never rename. Rename cost is 10-100x mechanical cost. |

### P2 Risks (Worth Tracking)

| # | Risk | Mitigation |
|---|------|------------|
| 11 | Pulse animation conflicts with frame timer | `kain_machine_pulse_start()` runs background thread. Kaintana reads `pulse_total_fire_count()` in begin_frame. No direct conflict. |
| 12 | Service `"ui.kaintana"` key collision | Perfect-hash registration in `services.h`. Collision detected at compile time by Z3 checks. |
| 13 | CRT constructor auto-registration conflict | Ensure legacy `native_ui_surface.c` auto-registration removed before Kaintana takes over. Register `"kaintana"` not `"native_ui"` during transition. |

---

## 8. The Archive Plan

Once Kaintana is verified and the compiler points at `kaintana.h`:

### Step 1: Archive KUIF Source
```
src/ui/  -->  archive/legacy/ui/
```
The entire KUIF C runtime (12,000+ lines across 12 source files) moves here. Never deleted — history matters.

### Step 2: Archive KUIF Headers
All 25 UI headers from `include/` move to `archive/legacy/ui_headers/`:
- `ui_system.h`, `ui_layout.h`, `ui_renderer.h`, `ui_color.h`, `ui_font.h`
- `ui_bundle.h`, `ui_runtime.h`, `ui_theme.h`, `ui_components.h`, `ui_debug.h`, `ui_hot_reload.h`
- `kain_compositor.h`, `kain_font.h`, `kain_geometry.h`, `kain_host.h`, `kain_input.h`, `kain_render_software.h`, `kain_surface.h`
- `flexbox.h`, `layout_engine.h`, `scene.h`, `render_command.h`, `renderer_backend.h`, `renderer_session.h`
- `component_surface.h` (replaced by `kaintana.h`)

### Step 3: Clean Include Directory
`include/` retains ONLY the 59 core runtime headers (actors, arena, ownership, GPU, networking, etc.):

```
include/
  actor.h    arena.h    async.h    ...  (59 core headers, zero UI)
archive/legacy/
  ui/                                    (entire src/ui/ — 12,000+ lines)
  ui_headers/                            (all 25 UI headers)
```

### Step 4: Rename ui_v2 -> ui
```
src/ui_v2/  -->  src/ui/
```

### Step 5: Update TOML Manifest
Replace old `src/ui/*.c` entries in `native_core_runtime.toml` with `src/ui/*.c` (pointing at Kaintana). Run `py -3 scripts/python/update_runtime.py` to regenerate Bazel BUILD files.

---

## 9. Verification Strategy

### Per-File Verification

| File | How Verified | When | Tools |
|------|-------------|------|-------|
| `kaintana.h` | Compile test: struct size assertions. Python ctypes: slot layout validation | Phase 1 | `static_assert`, ctypes |
| `tree.c` | 10,000 element_begin/end -> arena integrity check. Stable key reconciliation test | Phase 1 | null backend, Python |
| `box_math.c` | 180+ Yoga test patterns. Known input -> known output. Z3 proof | Phase 1 | null backend, Z3 |
| `damage.c` | 10,000 nodes, 200 dirty -> verify O(200) work. Z3 proof | Phase 1 | null backend, Z3 |
| `draw_pixels.c` | Golden image comparison (16 primitives). Fuzz: random attributes -> no crash | Phase 1 | null backend, fuzzer |
| `backends/null/` | All tests run through null backend. CI block | Phase 2 | CI, no GPU |
| `backends/win32/` | Oracle verify: spawn window, matrix brightness, delta alive | Phase 2 | Oracle |
| `std/core.kn` | `kain build` + `kain run` + Oracle verify | Phase 3 | Kain CLI, Oracle |
| `std/widgets.kn` | Snapshot test of each widget in known state. Golden compare | Phase 4 | null backend, Python |
| GPU backends | Oracle verify per backend: window, matrix, delta, click | Phase 5 | Oracle |
| Z3 proofs | Run `z3-mcp` proof verifier on all proof packs | Phase 6 | Z3 |

### The Ghost Harness (from _KAINTANA.md)

Kaintana's verification uses a **ghost harness** — Python ctypes loading `kaintana.dll` or `libkaintana.so`, driving the 24-slot vtable programmatically:

```python
# tests/python_abi/test_session.py
import ctypes
lib = ctypes.CDLL("libkaintana.so")

# Create session
lib.kt_make.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
sid = lib.kt_make(b"Test", 800, 600)

# Build UI
lib.kt_begin(sid, ctypes.c_double(16.0))
root = lib.kt_row(sid, -1, b"box", b"root")
child = lib.kt_row(sid, root, b"box", b"child1")
lib.kt_fill(sid, child, b"accent")
lib.kt_width(sid, child, ctypes.c_float(100))
lib.kt_height(sid, child, ctypes.c_float(30))
lib.kt_end_row(sid)
lib.kt_end_row(sid)
lib.kt_end(sid)

# Verify layout
assert lib.kt_cmd_count(sid) > 0, "No draw commands generated!"
cmd = lib.kt_cmd_get(sid, 0)
# Verify fill_rect at expected position
```

This enables:
- **CI unit tests** — no DISPLAY, no GPU, no desktop session
- **ABI fuzzing** — inject random attribute values, verify arena integrity
- **Snapshot testing** — render to CPU buffer, compare with golden image
- **Layout regression** — run box_math on 1000 layouts, compare output vectors
- **10,000+ test cases per CI run** — impossible with window-based testing

### Z3 Proof Arch + CBMC Inheritance

By integrating with the core runtime instead of duplicating, Kaintana inherits:

| Subsystem | Proof Type | Count | What's Proven |
|-----------|-----------|:-----:|---------------|
| Arena allocator | CBMC | 833 assertions | Init, frame markers, alloc_lo/alloc_hi, reset |
| Arena allocation | Z3 | 6 proofs | Bump bounds, alignment, overflow |
| Input system | Z3 | 2 proofs | Token collision-free, hash probe bounds |
| Event ring | CBMC | 5,676 assertions | FIFO order, capacity, bounded/unbounded |
| Service registry | Z3 | 4 proofs | Perfect-hash, spinlock safety, buffer bounds |
| Machine stones | Z3 | 6 proofs | Pulse missed-beat, shatter lane bounds, teleport |
| Ownership | Z3 | 38 proofs | Observer counts, state machine totality |
| Entangle | Z3 | 5 proofs | Text copy bounds, index bounds, atomic |

**Total: 833 CBMC + 61 Z3 proofs = 894 verified invariants inherited.**

Kaintana will add its own:
- `arena_proofs.yaml` — arena overflow, growth safety
- `hash_table_proofs.yaml` — FNV-1a collision bounds, probe chain bounds
- `box_math_proofs.yaml` — two-pass convergence, clamp safety
- `damage_proofs.yaml` — pipeline state machine totality, lazy sleep invariants
- `draw_proofs.yaml` — command count bounded, merge correctness, strict-aliasing

---

*End of MASTER_ARCHITECTURE.md — the definitive file-by-file architecture, creation order, and rationale for Kaintana. 18 research documents synthesized into one capstone. Total KUIF reduction: 25 headers -> 1. 174 exports -> 34 functions. 12,000+ lines of C -> ~2,000. Widget code: exiled from C to Kain forever.*
