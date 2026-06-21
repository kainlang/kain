# Kain Language Newsletter - Issue #2

**Date:** 2026-06-21
**Subject:** Component Surface -- The `component` Keyword Finally Reaches Pixels
**Philosophy:** Surface-agnostic rendering through a compiler-owned trait vtable. One trait, many backends. The compiler emits calls through the vtable; the backend implements them. Neither side knows the other's internals.

______________________________________________________________________

## Executive Summary

The `component` keyword has parsed and typechecked correctly since day one -- but produced **zero visual output**. JSX compiled to dead `i8*` strings. Component `state` was stack-local and died on return. No frame loop called component render functions.

**That changes today.** The `component` keyword now compiles to real UI element trees via a surface-agnostic vtable trait (`KainComponentSurface`). Every JSX element emits `element_begin` → attribute calls → children → `element_end` through the vtable. Component state persists across frames. World-surface declarations auto-generate full frame loops with session lifecycle.

**Net language surface change: 0% (zero new keywords).** Everything was done with existing constructs: `component`, `world`, `surface`, `render`. The changes are entirely in codegen and runtime.

______________________________________________________________________

## The Architecture

```
Kain source:  component Counter(label: String):       state count: Int = 0
              render <box><text value={label} /></box>

                    ↓ compile_jsx (codegen rewrite)

LLVM IR:      define void @Counter_render(%KainComponentSurface* %surface,
                                          i64 %session, i64 %parent, i8* %label)
                %sk = stable_key("Counter:box", %parent, 0)
                %box = surface→element_begin(%session, %parent, "box", %sk)
                surface→element_set_attr_string(%session, %box, "title", %label)
                %sk2 = stable_key("Counter:box:text", %box, 0)
                %text = surface→element_begin(%session, %box, "text", %sk2)
                surface→element_set_text(%session, %text, %label)
                surface→element_end(%session, %text)
                surface→element_end(%session, %box)
                ; Write-back: persist state to surface
                %val = load i64, i64* %count.addr
                surface→state_set_i64(%session, "Counter:count", %val)
                ret void

                    ↓ KainComponentSurface* trait (C runtime)

Surface backends:
  native_ui  → native_ui_surface  (wraps ui_system.h)
  web        → web_surface        (future WASM backend)
  viewport3d → 3d_surface         (future 3D backend)
  headless   → headless_surface   (future PDF/SVG export)
```

______________________________________________________________________

## What Changed

### 1. `KainComponentSurface` Trait -- The ABI Contract

**What it is:** A C struct with 15 function pointers forming the surface-agnostic rendering ABI. The compiler resolves a surface once at frame-loop init, then calls through the vtable every frame. The backend implements the trait; the compiler never knows which backend it's talking to.

**Where it lives:** `runtime/native/include/component_surface.h`

**The 15 vtable slots (in order):**

| Offset | Function | Purpose |
|--------|----------|---------|
| 0 | `session_create(name, w, h) → i64` | Create rendering session |
| 1 | `session_destroy(sid)` | Tear down session |
| 2 | `element_begin(sid, parent, kind, stable_key) → i64` | Create or reconcile tree node |
| 3 | `element_end(sid, el)` | Close element scope (no-op in retained mode) |
| 4 | `element_set_text(sid, el, text)` | Set text content on node |
| 5 | `element_set_attr_i64(sid, el, key, val)` | Set integer-valued attribute |
| 6 | `element_set_attr_f64(sid, el, key, val)` | Set float-valued attribute |
| 7 | `element_set_attr_string(sid, el, key, val)` | Set string-valued attribute |
| 8 | `state_get_i64(sid, key) → i64` | Load persisted component state |
| 9 | `state_set_i64(sid, key, val)` | Store persisted component state |
| 10 | `begin_frame(sid, delta_ms)` | Start new frame |
| 11 | `end_frame(sid)` | Complete frame tree walk |
| 12 | `present(sid)` | Present rendered frame |
| 13 | `poll_event(sid, out, max) → i64` | Dequeue input event |
| 14 | `should_close(sid) → i64` | Check if window should close |

**Registration API:**

```c
// Register a surface backend at startup
void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface);

// Resolve a surface by name (called by compiler-emitted code)
const KainComponentSurface* kain_component_surface_resolve(const char* name);
```

**Design decision:** All fire-and-forget operations use `void` return in the trait, but the underlying `abi_ui_*` functions return `int64_t` status codes. Trait implementations use thin wrappers that discard the return value -- this avoids UB from calling through mismatched function pointer types.

______________________________________________________________________

### 2. World-Surface Frame Loop -- Auto-Generated

**What it is:** When the compiler encounters `surface native_ui => ComponentName` on a world, it emits a full frame loop function that:

1. Resolves the `KainComponentSurface*` from the registry
1. Creates a session with the world's name and default dimensions (1280×720)
1. Loops: `begin_frame` → render root component → `end_frame` → `present` → `should_close`
1. On close: `session_destroy` and exit

**The emitted LLVM IR (simplified):**

```llvm
define void @__kain_world_surface_loop_App() {
entry:
  %surface = call @kain_component_surface_resolve("native_ui")
  ; null check → panic if surface not registered
  %sid = call surface→session_create("App", 1280, 720)
  ; error check → panic if < 0

frame_loop:
  %delta = call @__kain_frame_delta_ms()
  call surface→begin_frame(%sid, %delta)
  call void @RootPanel_render(%surface, %sid, 0)
  call surface→end_frame(%sid)
  call surface→present(%sid)
  %close = call surface→should_close(%sid)
  %keep = icmp eq i64 %close, 0
  br i1 %keep, label %frame_loop, label %shutdown

shutdown:
  call surface→session_destroy(%sid)
  ret void
}
```

**Injection point:** Frame loop calls are emitted in `main()` after `abi_runtime_init()` (which registers the built-in `native_ui` surface) and before any patch/param setup. Multiple worlds with surfaces → multiple frame loops.

**No new syntax.** `surface native_ui => Component` always parsed correctly -- the codegen just didn't do anything with it until now.

______________________________________________________________________

### 3. Component State Persistence -- Survives Across Frames

**Before (broken):**

- `state count: Int = 0` lowered to stack `alloca` -- died on return
- Mutations (`self.count = self.count + 1`) persisted within a single frame, lost on next render

**After (fixed):**

- State loaded via `surface→state_get_i64(session, "ComponentName:field_name")`
- PHI node merges initial value (first frame) with persisted value (subsequent frames)
- Stored to entry-block `alloca` -- always written before any reader accesses it
- Write-back loop at end of render: every state field's current value is persisted via `surface→state_set_i64()`

**The emitted LLVM IR for state init:**

```llvm
; ── Load persisted state value through vtable ──
%stored = call surface→state_get_i64(%sid, "Counter:count")

; ── First frame detection (0 = unset) ──
%is_first = icmp eq i64 %stored, 0
br i1 %is_first, label %init, label %load

init:
  ; Store initial value via vtable
  call surface→state_set_i64(%sid, "Counter:count", 0)
  br label %load

load:
  ; PHI: merge init_val from init_block, stored_val from entry
  %val = phi i64 [ 0, %init ], [ %stored, %entry ]
  store i64 %val, i64* %count.addr    ; ← always written before any read

; ... JSX body compiles here, may mutate %count.addr ...

; ── Write-back: persist current value at end of render ──
%current = load i64, i64* %count.addr
call surface→state_set_i64(%sid, "Counter:count", %current)
ret void
```

**Known limitation:** First-frame detection uses `state_get_i64 == 0` as the "unset" sentinel. If your initial value IS 0, the field re-initializes every frame. Future: use a separate init-flag key per field.

**Key format:** `"ComponentName:field_name"` -- deterministic, unique per component type. Note: two instances of the same component in the same session share state keys. Instance-scoped prefixes are planned for Phase 2.

______________________________________________________________________

### 4. Stable Keys -- Retained-Mode Element Reconciliation

**What it is:** Every element in the tree gets a deterministic, unique stable key so the surface backend can reconcile the element tree across frames -- find existing nodes, update them, create new ones.

**Format:** `"ComponentName:element_path:parent_id:sibling_index"`

**Examples:**
| JSX Context | Stable Key |
|---|---|
| `<box>` root in Counter | `"Counter:box:0:0"` |
| `<text>` child of that box | `"Counter:box:text:7:0"` |
| `<text>` in `for` loop at index 3 | `"Counter:box:text:7:3"` |
| `<text>` in `if` branch | `"Counter:box:text:7:5"` |

**The reconciliation pattern (in `native_ui_element_begin`):**

```c
// Try to find existing node by stable key
int64_t existing = abi_ui_node_find_by_stable_key(session_id, stable_key);
if (existing > 0) {
    // Node survived from previous frame -- update parent, reuse
    abi_ui_node_set_parent(session_id, existing, parent_id);
    return existing;
}
// First frame -- create new node, set stable key for future frames
int64_t node = abi_ui_node_create(session_id, kind);
abi_ui_node_set_stable_key(session_id, node, stable_key);
abi_ui_node_set_parent(session_id, node, parent_id);
return node;
```

**Performance note:** `abi_ui_node_set_stable_key` triggers a full hash table rebuild (O(n) for n ≤ 4096 nodes). This is fine for typical UI trees. Large trees (>1000 nodes) may benefit from an incremental rebuild optimization in a future release.

______________________________________________________________________

### 5. JSX Attribute → Surface Call Mapping

**What it is:** A compile-time table in the codegen maps every JSX attribute to the correct vtable call. Unknown attributes pass through with the attribute name as the key -- the surface backend decides what to do.

**The full mapping (16 attributes):**

| JSX Attribute | Vtable Call | Style Key | Value Type |
|---|---|---|---|
| `padding={8}` | `element_set_attr_f64` | `"padding"` | Float |
| `spacing={4}` | `element_set_attr_f64` | `"spacing"` | Float |
| `corner_radius={4}` | `element_set_attr_f64` | `"corner_radius"` | Float |
| `font_size={14}` | `element_set_attr_f64` | `"font_size"` | Float |
| `opacity={0.5}` | `element_set_attr_f64` | `"opacity"` | Float |
| `border={1}` | `element_set_attr_f64` | `"border_width"` | Float |
| `width={400}` | `element_set_attr_f64` | `"width"` | Float |
| `height={300}` | `element_set_attr_f64` | `"height"` | Float |
| `background="#FFF"` | `element_set_attr_string` | `"fill_color"` | String |
| `border_color="#333"` | `element_set_attr_string` | `"border_color"` | String |
| `color="#333"` | `element_set_attr_string` | `"ink_color"` | String |
| `title="Submit"` | `element_set_attr_string` | `"title"` | String |
| `direction="vertical"` | `element_set_attr_i64` | `"layout.direction"` | Int (0=H, 1=V) |
| `disabled` (bare) | `element_set_attr_i64` | `"disabled"` | Int (1=true) |
| `value={"Hello"}` | `element_set_text` | (uses set_text path) | String |
| Unknown attr | `element_set_attr_string` | attr name as key | String |

**Type coercion:** `i1` → `i64` via `zext`, `i64` → `double` via `sitofp`. The codegen automatically promotes types to match the vtable slot.

______________________________________________________________________

### 6. Full JSX Construct Support -- 7 Node Types

Every JSX construct now compiles to correct surface vtable calls:

| JSX Construct | Emitted Pattern |
|---|---|
| `<box><text/></box>` | `element_begin("box")` → attrs → `element_begin("text")` → `set_text` → `element_end` → `element_end` |
| `<Button label="X"/>` | `call void @Button_render(surface, session, parent, "X")` |
| `value={"Hello " + name}` | Evaluate expression → `element_set_text(session, el, result)` |
| `for item in items:` | Runtime loop: `runtime_array_get(iter, idx)` → `element_begin` with index-based key |
| `if cond: ... elif: ... else:` | LLVM branches with distinct sibling_index per arm |
| `<Fragment>...</Fragment>` | Children emit directly to parent -- no wrapper element |
| `{expression}` as child | Evaluate → stringify if needed → `element_begin("text")` → `set_text` → `element_end` |

______________________________________________________________________

## What You Can Do Now

### Declare a world with a surface and component

```kn
use std::runtime
use std::ui

component Dashboard():
    state counter: Int = 0

    render <panel title="Dashboard">
        <stack direction="vertical" padding={16} spacing={8}>
            <text value={"Counter: " + str($self.counter)} font_size={18} />
            <button title="Increment" disabled={false}>
                <text value="Click Me" />
            </button>
        </stack>
    </panel>

world AppWorld:
    state signal: Int = 1
    surface native_ui => Dashboard

fn main() -> Int:
    let init = runtime_init()
    if init != 0:
        return 100 + init
    // Frame loop auto-generated, renders Dashboard every frame
    return 0
```

### What happens when you run this

1. `runtime_init()` registers `native_ui_surface` in the surface registry
1. The auto-generated frame loop calls `kain_component_surface_resolve("native_ui")`
1. The `native_ui_surface` vtable wraps all `abi_ui_*` calls to `ui_system.h`
1. `Dashboard_render()` is called every frame, emitting elements through the vtable
1. `state counter` loads from `state_get_i64("Dashboard:counter")` via the vtable
1. Any mutation to counter is persisted via `state_set_i64` at the end of the frame
1. The `native_ui_surface` backend wraps `ui_system.h` -- the same retained-mode UI runtime that powers Kaintana

### Declare a component without a world (standalone)

```kn
component HelloWidget():
    render <panel title="Hello">
        <text value="This component exists independently." />
        <text value="No world needed to declare it." />
    </panel>
```

Components are first-class declarations. They don't need a world to exist. Without a world+surface, they're passive -- they emit a render function (`void @HelloWidget_render(surface, session, parent)`) that something external must call.

### Create a custom surface backend (for other platforms)

Implement the `KainComponentSurface` trait and register it:

```c
// my_custom_surface.c
#include "component_surface.h"

static int64_t my_session_create(const char* name, int64_t w, int64_t h) {
    // Initialize your platform's windowing system
    return 1;
}

// ... implement all 15 vtable slots ...

KainComponentSurface my_custom_surface = {
    .session_create = my_session_create,
    // ... fill all slots ...
};

// Auto-register before main()
__attribute__((constructor))
static void register_my_surface(void) {
    kain_component_surface_register("my_platform", &my_custom_surface);
}
```

Then in Kain: `surface my_platform => App` -- the compiler doesn't care which backend is registered. It always calls through the vtable.

______________________________________________________________________

## The Merge Story -- How We Got Here

This feature was implemented in parallel by two agents with zero coordination, producing a classic "the wire doesn't connect" divergence:

**Plumber A (C Runtime)** built the `KainComponentSurface` trait -- 15 function pointers via vtable, a surface registry, and the `native_ui_surface` backend wrapping `ui_system.h`. All in C. All correct.

**Plumber B (Rust Codegen)** rewrote `compile_jsx` to emit surface calls -- but emitted **direct `abi_ui_*` function calls** instead of calling through the `KainComponentSurface*` trait vtable. The functions Plumber B called didn't match what Plumber A built. The codegen never resolved a surface, never loaded the vtable, and called functions by names that didn't exist in the C runtime.

**The reconciliation:**

1. Kept Plumber A's C files as-is (trait was correct)
1. Rewrote Plumber B's codegen to call through the vtable: `getelementptr` → `bitcast` → `load` → indirect `call`
1. Threaded `%KainComponentSurface*` as the first parameter through every render function
1. Added vtable offset constants (0-14) matching the C struct field order exactly
1. Moved `native_ui_surface.c` from `blades/kaintana/` to `runtime/native/src/ui/` -- it's a runtime-level backend, not a framework-specific one

**Bugs found during review (4-agent audit):**

- State alloca was never written on frames 2+ (read garbage -- fixed with PHI node)
- State mutations were never persisted (added write-back loop at end of render)
- Sibling elements got identical stable keys (`child_si = 0` reset inside for loop -- moved outside)
- `title` attribute silently dropped by C backend string filter (added to allowlist)

______________________________________________________________________

## Files Touched

| Layer | File | Lines | Action |
|-------|------|-------|--------|
| **C Runtime** | `runtime/native/include/component_surface.h` | 80 | CREATE -- trait struct + registry API |
| | `runtime/native/src/core/component_surface.c` | 110 | CREATE -- surface registry (16 slots) |
| | `runtime/native/src/ui/native_ui_surface.c` | 250 | CREATE → MOVED -- wraps `ui_system.h` behind trait vtable |
| | `runtime/native/src/core/stdlib_abi.c` | +3 | EDIT -- auto-register `native_ui` surface in `abi_runtime_init()` |
| | `runtime/native_core_runtime.toml` | +1 | EDIT -- add `native_ui_surface.c` to manifest |
| | `runtime/runtime_manifest_data.bzl` | +1 | EDIT -- Bazel mirror of manifest |
| **Rust Codegen** | `crates/sys-codegen/src/codegen_llvm/component.rs` | 1150 | CREATE -- full component→surface codegen via vtable |
| | `crates/sys-codegen/src/codegen_llvm/mod.rs` | +15 | EDIT -- `pending_frame_loops`, `abi_runtime_init()` preamble, component dispatch |
| **Cascade Fixes** | `crates/fmt/src/lib.rs` | +8 | EDIT -- explicit shader stage arms (Mesh→Callable) |
| | `crates/cli/src/selfhost.rs` | +18 | EDIT -- `DispatchSize::Fixed` pattern matching |
| | `crates/cli/src/import_c.rs` | +20 | EDIT -- same |
| | `crates/cli/src/import_rust.rs` | +20 | EDIT -- same |
| | `crates/gpu/src/codegen_spirv.rs` | ~50 | EDIT -- `SpecConstantPlan.span` → `Span::default()` |
| | 8 other crates | ~30 | EDIT -- `ShaderStage`, `Subgroup`, `DispatchSize` cascade |

**Total:** ~1,700 lines of new code, ~200 lines of cascade fixes, 0 new keywords.

______________________________________________________________________

## What Did NOT Change

| Idea | Why Rejected | Instead |
|------|-------------|--------|
| Direct `abi_ui_*` calls in codegen | Bypasses surface-agnostic trait -- locks compiler to one backend | Vtable calls through `KainComponentSurface*` |
| `native_ui_surface.c` in `blades/kaintana/` | Framework-specific location -- surface backends are runtime infrastructure | Moved to `runtime/native/src/ui/` |
| Separate keywords for surface kinds | The `surface <kind> => Component` syntax already parsed correctly -- no new keywords needed | Existing `surface` declaration on worlds |
| Codegen-embedded UI runtime | Violates surface-agnosticism -- the compiler shouldn't know about `ui_system.h` | All UI ops through vtable; compiler only knows `KainComponentSurface*` |
| `component` state as `world` state | Components are presentation-layer; worlds are state authority -- different decision ladder layers | Component state persists via surface trait's `state_get_i64`/`state_set_i64` |

______________________________________________________________________

## Research & Documentation

- Implementation plan: `research/component/IMPLEMENTATION_PLAN.md`
- Wiring contract (12 IR contracts): `research/component/WIRING_CONTRACT.md`
- Merge plan (reconciliation): `research/component/MERGE_PLAN.md`
- Component surface overview: `research/component/README.md`
- C trait header: `runtime/native/include/component_surface.h`
- Reference backend: `runtime/native/src/ui/native_ui_surface.c`
- Codegen (all 12 contracts): `crates/sys-codegen/src/codegen_llvm/component.rs`
- Integration wiring: `crates/sys-codegen/src/codegen_llvm/mod.rs`

______________________________________________________________________
