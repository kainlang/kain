# KUIF MASTER GUIDE — The Rosetta Stone

> **The document that defines the Kain UI Framework's identity. Read this first, read this last, read it whenever you're lost.**
>
> **Status:** Canonical. Written 2026-06-25. Covers the complete KUIF redesign — from C widgets to Kain components.
> **Based on:** KUIF_IMPLEMENTATION_PLAN.md (unified plan), WIDGETS.md (manifesto, 1401 lines), RENDER-AND-UI-MAP.md (122-file map, 16 layers), RULEBOOK.md (decision ladder), KAIN_BY_EXAMPLE.md (canonical examples), COMPONENT.MD, WORLD.MD, ENTANGLE.MD, PATCH.MD, LAW.MD, PULSE.MD, RESONATE.MD, CONVERGE.MD, ORCHESTRATE.MD, keyword_crucible.kn (108/110 keywords), fusion_chain.kn (all 7 layers fused).

---

## Table of Contents

1. [The Central Inversion — Why C Widgets Must Die](#1-the-central-inversion--why-c-widgets-must-die)
2. [The Decision Ladder — Every Construct Has a Place](#2-the-decision-ladder--every-construct-has-a-place)
3. [The Target Architecture — The 4-Layer Stack](#3-the-target-architecture--the-4-layer-stack)
4. [How Each Ladder Rung Replaces a C Pattern](#4-how-each-ladder-rung-replaces-a-c-pattern)
5. [The Component Is the Atom of UI](#5-the-component-is-the-atom-of-ui)
6. [State Authority — `world` + `entangle` Replace C State Machines](#6-state-authority--world--entangle-replace-c-state-machines)
7. [State Integrity — `patch` + `law` Replace Ad-Hoc Mutation](#7-state-integrity--patch--law-replace-ad-hoc-mutation)
8. [Temporal — `pulse` + `resonate` Replace setTimeout/requestAnimationFrame](#8-temporal--pulse--resonate-replace-settimeoutrequestanimationframe)
9. [Implementation Phases — The 10-Phase Roadmap](#9-implementation-phases--the-10-phase-roadmap)
10. [The C Substrate — What Stays, What Goes, What's New](#10-the-c-substrate--what-stays-what-goes-whats-new)
11. [Gotchas and Pitfalls — What Will Break If You're Not Careful](#11-gotchas-and-pitfalls--what-will-break-if-youre-not-careful)
12. [Testing and Proof Strategy](#12-testing-and-proof-strategy)
13. [The Grand Vision — What KUIF Enables That Nothing Else Can](#13-the-grand-vision--what-kuif-enables-that-nothing-else-can)
14. [Research Document Index — Every File and Why It Matters](#14-research-document-index--every-file-and-why-it-matters)

---

## 1. The Central Inversion — Why C Widgets Must Die

### The Current Architecture (Wrong)

```
┌─────────────────────────────┐
│  Kain std::ui component.kn  │  ← Thin wrapper, ~158 lines
│  Kain std::ui widget.kn     │  ← Thin wrapper, ~185 lines
├─────────────────────────────┤
│  C ui_widget.c (1,559 loc)  │  ← THE ACTUAL WIDGETS: button, slider, checkbox,
│  C ui_widget.h (273 loc)    │     label, panel, textbox, progress, toggle,
│  C ui_layout.c (199 loc)    │     picker, stepper, icon, image, scroll, list,
│  C ui_renderer.c            │     tree, tabs, menu, divider, badge, tooltip,
│  C ui_host_adapter.c        │     alert, dialog, sheet, navigation, toolbar,
│  C ui_system.c              │     statusbar — ALL IN C
└─────────────────────────────┘
```

**The inversion:** Kain is the thin caller. C is the thick implementation. The language with `world`, `entangle`, `patch`, `law`, `resonate`, `pulse`, `converge`, `orchestrate`, `teleport`, `shatter`, `collapse`/`observe`/`decay` — 15+ compiler-owned semantic constructs — is doing **nothing but passing strings to C functions**.

This is the equivalent of writing a React app where every component is just `document.createElement()`. It works, but it misses the entire point of the platform.

### The Target Architecture (Right)

```
┌─────────────────────────────────────────────┐
│  L3: Kain Components (stdlib/ui/)            │
│  button.kn, slider.kn, checkbox.kn, ...     │  ← ALL 30+ WIDGETS IN KAIN
│  theme.kn, animation.kn, layout/*           │  ← THEME, ANIMATION, LAYOUT
├─────────────────────────────────────────────┤
│  L2: Kain Semantic Graph (compiler-owned)    │
│  world, entangle, patch, law, resonate,      │  ← STATE, REACTIVITY, ANIMATION
│  pulse, converge, orchestrate, teleport      │
├─────────────────────────────────────────────┤
│  L1: Component Surface Vtable               │
│  KainComponentSurface (18→24 slots)          │  ← ABI CONTRACT: compiler→backend
│  JSX → vtable call lowering                  │
├─────────────────────────────────────────────┤
│  L0: C Rendering Substrate (~1,500 loc)      │
│  kain_render_*, kain_compositor_*,           │  ← DRAW PRIMITIVES ONLY
│  kain_input_*, kain_font_*, kain_host_*      │     No widgets. No layout.
└─────────────────────────────────────────────┘
```

**The inversion flipped:** Kain is the thick implementation. C is the thin substrate. The C layer provides draw primitives, damage tracking, input events, font rasterization, and a platform host interface — and **nothing more**. Kain owns widgets, layout, state, reactivity, animation, theming, accessibility, and the full component composition graph.

### Why This Inversion Matters

| Concern | C Widgets (Current) | Kain Components (Target) |
|---------|---------------------|--------------------------|
| **Button logic** | 80 lines of C state machine | 30 lines of declarative Kain |
| **Color/size constants** | 19 `#define`s in C header | Pure Kain structs in `theme.kn` |
| **Layout computation** | 199 lines of C flexbox | Kain HStack/VStack/ZStack/Grid components |
| **Animation** | Ad-hoc timer callbacks | `pulse` clocks + `SpringValue` components |
| **State change detection** | Manual `if old != new` | `resonate` tripwires with dampening |
| **State synchronization** | Manual copy functions | `entangle` compiler-owned propagation |
| **Mutation auditing** | Nonexistent | `patch` journaled, undoable, replayable |
| **Invariant checking** | `assert()` calls | `law` predicates verified by Z3 |
| **Hot reload** | File-level bundle swaps | Component-level render function pointer swaps |
| **Cross-platform** | `#ifdef _WIN32` gating | Vtable backend selection at startup |
| **Accessibility** | Nonexistent | World-graph accessibility — same state, two trees |

### The Kain Advantage in One Sentence

> **SwiftUI is a library layered on top of a language not designed for UI. KUIF is a language where UI semantics are first-class compiler constructs. That's the difference between a framework and a platform.**

---

## 2. The Decision Ladder — Every Construct Has a Place

Kain's decision ladder is **the** tool for deciding which construct to use. Every KUIF developer must internalize this. The ladder is ordered from highest to lowest semantic power — always start at the top.

```
                    ┌──────────────────────────────┐
                    │ "Am I rendering UI?"          │──▶ component
                    ├──────────────────────────────┤
LAYER 7: SYSTEMS    │ "Concurrent message state?"   │──▶ actor
                    │ "Raw memory lifecycle?"       │──▶ collapse/observe/decay
                    ├──────────────────────────────┤
LAYER 6: MACHINE    │ "Capability assumption?"      │──▶ axiom
  STONES            │ "Hot-data layout?"            │──▶ shatter struct
                    │ "Cross-world zero-copy?"      │──▶ teleport
                    ├──────────────────────────────┤
LAYER 5: TEMPORAL   │ "Timed recurrence?"           │──▶ pulse
                    │ "React to state change?"      │──▶ resonate
                    ├──────────────────────────────┤
LAYER 4: STAGE      │ "Multi-stage pipeline?"       │──▶ orchestrate
  GRAPH             │ "Cross-runtime scheduling?"   │──▶ orchestrate
                    ├──────────────────────────────┤
LAYER 3: DISPATCH   │ "Spec + fast lanes?"          │──▶ converge
                    │ "Platform-specific perf?"     │──▶ converge
                    ├──────────────────────────────┤
LAYER 2: STATE      │ "Journaled mutation?"         │──▶ patch
  INTEGRITY         │ "Invariant predicate?"        │──▶ law
                    ├──────────────────────────────┤
LAYER 1: STATE      │ "Global named state?"         │──▶ world
  AUTHORITY         │ "Mirrored state?"             │──▶ world + entangle
                    │ "Coupled fields?"             │──▶ entangle
                    ├──────────────────────────────┤
LAYER 0: PLAIN      │ None of the above?            │──▶ fn, struct, let, etc.
  CODE              │                              │──▶ Use effects for intent
                    └──────────────────────────────┘
```

### The KUIF-Specific Ladder

When you're building UI specifically, here's the refined ladder:

```text
"Am I building UI?"
  ├── Is it a reusable widget with its own state?            → component
  ├── Is it application state behind the UI?                 → world + surface => Component
  ├── Is it state that must mirror across surfaces?           → world + entangle
  ├── Is it a mutation that should be journaled/undoable?    → patch
  ├── Is it a constraint on state validity?                  → law
  ├── Is it a reaction to state change?                      → resonate
  ├── Is it a timed recurrence (animation, heartbeat)?       → pulse
  ├── Is it a platform-specific rendering optimization?      → converge
  ├── Is it a multi-runtime pipeline (CPU→GPU→render)?       → orchestrate
  ├── Is it a layout container?                              → component (HStack/VStack/Grid)
  ├── Is it a theme or design token?                         → struct + const (pure Kain data)
  └── Is it computation that feeds the UI?                   → fn (called from {expr})
```

### The Anti-Ladder — What NOT to Do

```
COMMON ANTI-PATTERNS — REACH FOR THE LADDER INSTEAD:

  "I'll write a C function for this widget state machine..."
    → NO. That's a `component` with `state` fields.

  "I'll store widget state in a global C struct..."
    → NO. That's a `world` with `state` fields.

  "I'll use a while-loop to poll for state changes..."
    → NO. That's `resonate` on the world field.

  "I'll use setTimeout/requestAnimationFrame for animation..."
    → NO. That's a `pulse` clock.

  "I'll write an observer registry for property bindings..."
    → NO. That's `entangle` between worlds.

  "I'll use assert() to check layout bounds..."
    → NO. That's a `law` predicate, checked at compile time by Z3.

  "I'll put widget constants in a C header..."
    → NO. That's a `struct` or `const` in `theme.kn`.
```

---

## 3. The Target Architecture — The 4-Layer Stack

### Layer Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  L3: COMPONENTS (Kain)                                                  │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  component Button(label, on_click, variant, disabled)            │   │
│  │    state hovered: Bool = false                                    │   │
│  │    state pressed: Bool = false                                    │   │
│  │    render <RoundedRect fill={compute_fill()} ...>                 │   │
│  │           <Text value={label} ... />                               │   │
│  │    </RoundedRect>                                                  │   │
│  │                                                                   │   │
│  │  component HStack(children, spacing, alignment)                   │   │
│  │  component Slider(value: Entangle<Float>, min, max, step)         │   │
│  │  component SpringValue(initial, stiffness, damping)               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                          │                                              │
│                          │ JSX → vtable call lowering                   │
│                          ▼                                              │
│  L2: SEMANTIC GRAPH (Kain, compiler-owned)                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  world AppState:           entangle AppState.count               │   │
│  │    state count: Int = 0       <-> Mirror.count_copy              │   │
│  │    surface native_ui => App    with single_writer                 │   │
│  │                                                                   │   │
│  │  patch increment(world):      law count_in_bounds(v) -> Bool:     │   │
│  │    world.count += 1              return v >= 0                    │   │
│  │                                                                   │   │
│  │  resonate AppState.count:     pulse animation_clock every 16ms:   │   │
│  │    Mirror.shadow = compute()     AppState.tick += pulse_tick      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                          │                                              │
│                          │ LLVM IR → native ABI calls                   │
│                          ▼                                              │
│  L1: COMPONENT SURFACE (C vtable + Rust compiler)                       │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  KainComponentSurface vtable (18→24 slots)                       │   │
│  │  session_create → begin_frame → element_begin/end → set_attr    │   │
│  │  → set_text → set_frame → set_padding → set_flag → end_frame    │   │
│  │  → present → should_close → state_get/set_i64/f64/String        │   │
│  │  → element_set/invoke_callback                                  │   │
│  │                                                                   │   │
│  │  map_jsx_attr_to_surface_key():                                  │   │
│  │    "background"   → fill_color    "font_size"   → OFF_ATTR_F64  │   │
│  │    "padding"      → OFF_ATTR_F64  "title"       → OFF_ATTR_STR  │   │
│  │    "direction"    → OFF_ATTR_I64  "disabled"    → OFF_ATTR_I64  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                          │                                              │
│                          │ C function calls                             │
│                          ▼                                              │
│  L0: RENDERING SUBSTRATE (C — ~1,500 lines total)                       │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  Renderer     Compositor    Input        Font        Geometry    │   │
│  │  ─────────    ──────────    ─────        ────        ────────    │   │
│  │  fill_rect    damage_rect   poll_event   font_load   Rect{x,y,w,h}│
│  │  rounded_rect damage_node   hit_test     font_measure Point{x,y} │   │
│  │  circle       damaged_region push_event  font_metrics Size{w,h}  │   │
│  │  text         clear_damage  route_event  font_unload Color{r,g,b,a}│
│  │  gradient     submit_frame  event_type   glyph_run   Matrix[6]   │   │
│  │  blur                                                           │   │
│  │  push/pop_clip      Host              Surface                    │   │
│  │  push/pop_transform ────              ───────                    │   │
│  │  submit/present     create_window     create(width, h, backend)  │   │
│  │                     pump_messages     resize/destroy              │   │
│  │                     dpi_scale         pixels()→framebuffer        │   │
│  │                     set_title/size                               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Layer Responsibilities — Hard Boundary

| Layer | What It Owns | What It Does NOT Own |
|-------|-------------|---------------------|
| **L3 — Components** | Widget definitions, JSX composition, prop/state/effect declarations, render tree construction, event handler binding, layout container logic | Draw primitives, platform windows, font rasterization, damage tracking, GPU resource management, input device polling |
| **L2 — Semantic Graph** | World state graphs, entangle propagation, patch journaling, law invariants, resonate tripwires, pulse clocks, converge lane selection, orchestrate pipeline stages | Component render trees (those are L3), draw commands (those are L0), window handles (those are L0) |
| **L1 — Component Surface** | Vtable ABI contract, JSX→vtable lowering, attribute→slot mapping, state persistence (i64/f64/String), callback binding/invocation | Widget logic (that's L3), state graphs (that's L2), pixel pushing (that's L0) |
| **L0 — Rendering Substrate** | Draw primitives (rect, circle, text, gradient, blur, blit), damage region tracking, input event collection/routing, font glyph rasterization, platform window creation, framebuffer/swapchain management | Widget behavior, layout math, theme colors/sizes, state management, animation, reactivity, component composition |

---

## 4. How Each Ladder Rung Replaces a C Pattern

This section maps every Kain semantic construct to the C pattern it replaces, with concrete before/after examples.

### `world` → Replaces C Global Widget State

**Before (C):**
```c
// ui_widget.c — global state scattered across structs
static int ui_button_hover_id = -1;
static int ui_slider_drag_id = -1;
static float ui_slider_drag_start = 0.0f;
// ... 20 more scattered state variables
```

**After (Kain):**
```kn
// All widget-visible state is compiler-owned
world AppState:
    state count: Int = 0
    state username: String = "guest"
    state slider_value: Float = 50.0
    surface native_ui => App
```

### `entangle` → Replaces Manual Property Binding

**Before (C):**
```c
// Copy state from authority to mirror — manually, every frame
void sync_state(KainUiWidgetContext* ctx) {
    ctx->mirror_count = ctx->authority_count;
    ctx->mirror_value = ctx->authority_value;
    // ... 15 more manual copies, easily forgotten
}
```

**After (Kain):**
```kn
// Compiler-owned reactive sync — one line per field
entangle AppState.count <-> Mirror.count_copy with single_writer
entangle AppState.value <-> Mirror.value_copy with single_writer
// Compiler guarantees: mirror writes rejected at typecheck time
// Runtime guarantees: propagation on every authority write
```

### `patch` → Replaces Ad-Hoc Widget Mutation

**Before (C):**
```c
// Direct mutation — no journal, no undo, no telemetry
void increment_counter(KainUiWidgetContext* ctx) {
    ctx->counter = ctx->counter + 1;
    // Was this recorded? No.
    // Can we undo? No.
    // Can we prove it fired? No.
}
```

**After (Kain):**
```kn
// Journaled, undoable, telemetry-instrumented
patch increment(world: AppState) -> Int:
    world.count = world.count + 1     // recorded old→new in journal
    world.epoch = world.epoch + 1     // epoch bump signals change
    return world.count

// Prove it fired:
let delta = patch_journal_count() - before
if delta < 1: return -20   // patch never fired
```

### `law` → Replaces `assert()` + Ad-Hoc Bounds Checks

**Before (C):**
```c
// Runtime-only, no compile-time verification
void set_value(KainUiWidgetContext* ctx, int value) {
    assert(value >= 0 && value < 1000);  // crash in debug, silent in release
    ctx->value = value;
}
```

**After (Kain):**
```kn
// Compiler-witnessable, Z3-provable, runtime-checkable
law value_in_range(v: Int) -> Bool:
    return v >= 0 and v < 1000

// Used in orchestrate stages, patch guards, runtime telemetry
orchestrate guarded_set(value: Int) -> Int:
    stage check: law value_in_range(value)
    stage mutate: patch set_value(world, value)
        requires check
    return mutate
```

### `resonate` → Replaces Polling-Based Change Detection

**Before (C):**
```c
// Every frame: check if state changed, fire handler if so
void update_slider(KainUiWidgetContext* ctx) {
    if (ctx->slider_value != ctx->last_slider_value) {
        ctx->slider_thumb_x = compute_thumb(ctx->slider_value);
        ctx->last_slider_value = ctx->slider_value;
    }
}
```

**After (Kain):**
```kn
// Compiler-owned tripwire — fires ONLY when state changes
resonate AppState.slider_value dampen 16ms:
    let new_val: Int = resonate_new_i64
    AppState.slider_thumb_x = compute_thumb(new_val)
    // No polling. No manual old-value tracking.
    // Dampening absorbs rapid-fire changes.
```

### `pulse` → Replaces setTimeout / requestAnimationFrame

**Before (C):**
```c
// Hand-rolled timer callback
static DWORD WINAPI animation_thread(LPVOID param) {
    while (running) {
        Sleep(16);
        // No jitter tolerance, no missed-beat tracking, no timing locals
        animate();
    }
}
```

**After (Kain):**
```kn
// Compiler-owned temporal beat with first-class timing locals
pulse animation_clock every 16ms jitter 2ms:
    let dt: Int = pulse_dt_ms        // actual elapsed time
    let tick: Int = pulse_tick       // monotonic counter
    let missed: Int = pulse_missed   // beats skipped (overload signal)
    // The runtime owns the scheduler thread, timing arithmetic, and jitter window
    AppState.anim_progress = (AppState.anim_progress + dt) % 1000
```

### `component` → Replaces C Widget Functions

**Before (C — 80 lines for a button):**
```c
int ui_button(KainUiWidgetContext* ctx, const char* label, int enabled) {
    // Hit-test
    int hovered = ui_point_in_rect(ctx->mouse_x, ctx->mouse_y, ctx->x, ctx->y, ctx->w, ctx->h);
    // State tracking
    int pressed = hovered && ctx->mouse_down && !ctx->mouse_was_down;
    // Color selection
    uint32_t fill = enabled ? (hovered ? THEME_BUTTON_HOVER : THEME_BUTTON_NORMAL) : THEME_BUTTON_DISABLED;
    // Draw
    ui_fill_rounded_rect(ctx, ctx->x, ctx->y, ctx->w, ctx->h, 6, fill);
    ui_draw_text(ctx, ctx->x + ctx->w/2, ctx->y + ctx->h/2, label, THEME_TEXT);
    // Click detection
    if (pressed) return 1;
    return 0;
}
```

**After (Kain — 30 lines, all declarative):**
```kn
component Button(label: String, on_click: () -> Void, variant: String = "primary", disabled: Bool = false):
    state hovered: Bool = false
    state pressed: Bool = false

    fn compute_fill(_self: Self_) -> kainColor:
        if _self.disabled: return theme.button_disabled
        if _self.pressed:  return theme.button_pressed
        if _self.hovered:  return theme.button_hover
        return theme.button_normal

    render <InteractiveArea
        on_hover={|h| self.hovered = h}
        on_press={|p| self.pressed = p}
        on_click={on_click}
    >
        <RoundedRect
            width={theme.button_width}
            height={theme.button_height}
            radius={theme.button_radius}
            fill={compute_fill()}
        >
            <Text value={label}
                  font_size={theme.button_font_size}
                  color={theme.button_text_color}
                  align="center" />
        </RoundedRect>
    </InteractiveArea>
```

### `converge` → Replaces `#ifdef` Platform Gating

**Before (C):**
```c
#ifdef __AVX2__
    // AVX2 fast path
#else
    // Scalar fallback
#endif
```

**After (Kain):**
```kn
converge render_blur(surface: kainSurface, rect: kainRect, radius: Float) -> Void:
    spec reference:
        return kain_render_blur_cpu(surface, rect, radius)
    fast gpu_lane when capability("gpu.compute"):
        return kain_render_blur_gpu(surface, rect, radius)
    verify random(8)
    // Runtime probes capabilities, selects lane, falls back to spec
    // verify random(8) fuzz-tests fast lane against spec at startup
```

### `orchestrate` → Replaces Sequential Function Pipelines

**Before (C):**
```c
// Sequential calls — no dependency graph, no residency tracking, no fallback
void render_frame() {
    int data = compute_cpu();
    int checked = check_bounds(data);   // hope you remembered this
    apply_patch(checked);
    draw_commands(checked);
}
```

**After (Kain):**
```kn
orchestrate render_pipeline(surface: kainSurface) -> Void:
    stage compute: cpu process_input(world) residency host policy static
    stage verify: law bounds_check(compute) deps [compute] residency host
    stage mutate: patch apply_state(world, verify) deps [verify] requires verify
    stage draw: gpu dispatch "render::main" [64, 64, 1] deps [mutate] residency shared
    // Compiler validates: no cycles, valid deps, residency/transfer compatibility
    // Runtime telemetry: stage timing, fallback activation, transfer counts
```

---

## 5. The Component Is the Atom of UI

### Component Anatomy — The Complete Picture

```kn
component Name(props...):
    state field: Type = initial...
    fn method(_self: Self_) -> Return...
    render <jsx>...</jsx>
```

Every component is:
- **Self-contained** — owns its state, methods, and render tree
- **Composable** — calls other components via uppercase JSX tags
- **Renderable** — produces a JSX visual tree lowered to native nodes
- **Free-standing** — does NOT require a world to exist

### JSX Composition — Components Calling Components

**Tag case is the dispatch mechanism:**
- `<panel>`, `<text>`, `<box>`, `<stack>` → lowercase → **native UI elements**
- `<Button>`, `<Slider>`, `<Dashboard>` → uppercase → **component calls**

```kn
component Toolbar():
    render <HStack spacing={8}>
        <Button label="Save" kind="primary" on_click={save} />
        <Button label="Load" kind="secondary" on_click={load} />
        <Button label="Export" kind="ghost" on_click={export} />
    </HStack>

component App():
    render <Window title="Dashboard" width={800} height={600}>
        <VStack spacing={16}>
            <Toolbar />
            <Divider />
            <Counter initial={0} label="Items" />
            <Slider value={entangle(AppState.volume)} min={0.0} max={100.0} />
        </VStack>
    </Window>
```

### JSX Control Flow

```kn
render <VStack>
    for item in items:
        <Card title={item.title} body={item.body} />

    if is_loading():
        <Spinner />
    elif is_empty():
        <Text value="No items" color={theme.text_dim} />
    else:
        <Text value={str(len(items)) + " items"} />
</VStack>
```

### The Component Decision Table

| You want to... | Use | Because |
|---|---|---|
| Define a reusable widget | `component` | Owns render tree, state, methods |
| Track app-level state | `world` + `surface => Component` | World is authority; component is view |
| Compose UI from pieces | `<OtherComponent />` in JSX | Uppercase tags dispatch to components |
| Bind world to root view | `surface native_ui => ComponentName` | Canonical world→UI wiring |
| Handle live events | `component` with `state` + event bridge | Component state + event system |
| Animate over time | `pulse` + component state | Pulse updates; component re-renders |
| Do computation (no UI) | `fn` called from `{expr}` | Components are for rendering |

### Component Limitations (From Fuzz Testing)

These are real compiler constraints discovered through the component fuzz blade:

1. **`_self` is NOT in scope inside JSX.** Use getter methods:
   ```kn
   // ❌ render <text value={_self.count} />
   // ✅ fn count_str(_self) -> String: return str(_self.count)
   //    render <text value={count_str()} />
   ```

2. **JSX `if` conditions can only use method calls returning `Bool` or simple identifiers** — not inline operators like `>`, `<`, `==`.

3. **JSX `for` loop variables have limited binding** for complex types — pre-compute in methods.

4. **`weak` state is actor-only** — components use regular `state`.

5. **Component names cannot shadow builtin types** (e.g., `Void`).

---

## 6. State Authority — `world` + `entangle` Replace C State Machines

### The Dual-World Pattern — THE Canonical State Pattern

Every non-trivial KUIF application should follow this pattern:

```kn
// ── Authority World (owns mutable state) ──
world AppState:
    state count: Int = 0
    state username: String = "guest"
    state theme: String = "dark"
    state epoch: Int = 0
    surface native_ui => App

// ── Mirror World (receives state via entangle) ──
world AppMirror:
    state count_copy: Int = 0
    state username_copy: String = "guest"
    state theme_copy: String = "dark"
    state epoch_copy: Int = 0
    surface web => App

// ── Entangle: authority → mirror ──
entangle AppState.count <-> AppMirror.count_copy with single_writer
entangle AppState.username <-> AppMirror.username_copy with single_writer
entangle AppState.theme <-> AppMirror.theme_copy with single_writer
entangle AppState.epoch <-> AppMirror.epoch_copy with single_writer
```

**Why the mirror pattern:**
- Authority: mutable state with `native_ui` surface (main thread)
- Mirror: read-only state propagated via entangle (zero cost, compiler-owned)
- Mirror writes are rejected at compile time by the typechecker
- Same component can serve both surfaces — write once, render everywhere

### What Worlds Hold in KUIF

| What | Where | Why |
|------|-------|-----|
| UI state (counter, toggle, slider value) | `world` state fields | Compiler-owned, persistent across frames |
| Theme tokens (colors, sizes, fonts) | `struct` + `const` in `theme.kn` | Pure data, no reactivity needed |
| Raw GPU buffers | `world` state as `ptr<T>` | Worlds can hold raw pointers — use `collapse`/`observe`/`decay` |
| Animation state (spring velocity, elapsed) | `component` local state | Per-instance, not global |
| Keyboard/mouse input state | `world` state fields | Shared across components |
| Accessibility tree | Mirror world + entangle | Same state, two trees, zero duplication |

### Selective Entanglement — Don't Mirror Everything

Not every world field needs a mirror. Raw buffers (`ptr<T>`) and internal counters typically stay local:

```kn
world Engine:
    state vertex_buffer: ptr<Float> = int_to_ptr(0, "Float")   // NOT entangled
    state index_buffer: ptr<Int> = int_to_ptr(0, "Int")        // NOT entangled
    state triangle_count: Int = 0                                // entangled → debug mirror
    state fps: Int = 60                                          // entangled → debug mirror
    surface native_ui => DebugPanel

world DebugMirror:
    state triangle_count_copy: Int = 0
    state fps_copy: Int = 60
    surface web => DebugPanel

entangle Engine.triangle_count <-> DebugMirror.triangle_count_copy with single_writer
entangle Engine.fps <-> DebugMirror.fps_copy with single_writer
```

---

## 7. State Integrity — `patch` + `law` Replace Ad-Hoc Mutation

### Pattern: Every Patch Bumps an Epoch

```kn
patch update_counter(world: AppState, delta: Int) -> Int:
    world.count = world.count + delta
    world.epoch = world.epoch + 1    // ← ALWAYS bump epoch
    return world.count
```

**Why epoch bumps matter:**
- Entangle propagation detects the epoch change
- Resonate handlers on `epoch` can fire cascading effects
- Orchestrate stage deps can watch epoch for stage invalidation
- The journal records every bump — replayable, undoable

### Pattern: Law Guards Before Patch

```kn
law count_in_bounds(v: Int) -> Bool:
    return v >= 0 and v < 1000000

law username_valid(name: String) -> Bool:
    return len(name) > 0 and len(name) <= 64

// In an orchestrate pipeline:
orchestrate guarded_update(delta: Int) -> Int:
    stage pre_check: law count_in_bounds(AppState.count + delta)
    stage mutate: patch update_counter(AppState, delta)
        requires pre_check           // ← won't execute if law fails
    return mutate
```

### Pattern: The Telemetry Delta Guard — Prove It Fired

```kn
fn verify_semantic_stack() -> Int:
    let patch_before = patch_journal_count()
    let entangle_before = entangle_propagation_count()
    let resonate_before = resonate_fire_count()
    let pulse_before = runtime_machine_pulse_total_fire_count()

    // ... run the causal chain ...

    let patch_delta = patch_journal_count() - patch_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let resonate_delta = resonate_fire_count() - resonate_before
    let pulse_delta = runtime_machine_pulse_total_fire_count() - pulse_before

    if patch_delta < 1:    return -20   // patch journal silent
    if entangle_delta < 1: return -21   // entangle never propagated
    if resonate_delta < 1: return -22   // resonate never fired
    if pulse_delta < 1:    return -23   // pulse never ticked

    return 0   // all layers proved active
```

---

## 8. Temporal — `pulse` + `resonate` Replace setTimeout/requestAnimationFrame

### The Animation Pattern — `pulse` + Component State

```kn
component SpringValue(initial: Float, stiffness: Float = 170.0, damping: Float = 15.0):
    state value: Float = initial
    state velocity: Float = 0.0
    state target: Float = initial

    pulse physics every 16ms jitter 2ms:
        let dt = pulse_dt_ms / 1000.0
        let force = stiffness * (self.target - self.value)
        let damping_force = damping * self.velocity
        self.velocity = self.velocity + (force - damping_force) * dt
        self.value = self.value + self.velocity * dt

    fn animate_to(_self: Self_, new_target: Float):
        _self.target = new_target

// Usage — spring-animated button
component SpringButton(label: String, on_click: () -> Void):
    state spring: SpringValue = SpringValue(1.0)

    render <InteractiveArea
        on_press={|| self.spring.animate_to(0.92)}
        on_release={|| self.spring.animate_to(1.0)}
    >
        <RoundedRect width={100.0 * spring.value} height={36.0 * spring.value}
                     radius={6.0} fill={theme.accent}>
            <Text value={label} color="#FFFFFF" align="center" />
        </RoundedRect>
    </InteractiveArea>
```

### The Reactivity Pattern — `resonate` + Cascading Effects

```kn
// When slider value changes, update the fill width
resonate AppState.slider_value dampen 0ms:
    let new_val: Int = resonate_new_i64
    let old_val: Int = resonate_old_i64
    // Cascading effect: slider value → fill width → trigger re-render
    AppState.slider_fill = compute_fill_ratio(new_val)
    AppState.slider_epoch = AppState.slider_epoch + 1

// When theme changes, invalidate all themed components
resonate AppState.theme dampen 100ms:
    // Dampened — rapid theme toggles don't cause re-render storms
    AppState.theme_epoch = AppState.theme_epoch + 1
    // Components watch theme_epoch to know when to recompute styles
```

### Timing Reference

| Construct | When It Fires | Locals Available | Dampening |
|-----------|--------------|------------------|-----------|
| `pulse every Nms` | Recurring on schedule + once immediately | `pulse_tick`, `pulse_dt_ms`, `pulse_missed` | Jitter tolerance via `jitter Nms` |
| `resonate field` | After every write to the watched field | `resonate_new_i64`, `resonate_old_i64`, `resonate_fired` | `dampen Nms` absorbs rapid re-fires |

---

## 9. Implementation Phases — The 10-Phase Roadmap

### Phase Summary Table

| Phase | What | Net Lines | Gates |
|-------|------|-----------|-------|
| **1: Substrate Extraction** | Extract C UI into clean, widget-free primitives | ~1,460 new, ~1,832 deleted | All existing demos still render identically |
| **2: GPU Backend** | Vulkan + WebGPU renderers | ~2,500 new | Identical output to software renderer |
| **3: Compiler Pipeline** | JSX attr expansion, pulse/resonate in components, f64/String state | ~400 Rust | Components typecheck + codegen with all new features |
| **4: Kain Widget Library** | 25+ components in `stdlib/ui/components/` | ~1,700 Kain | Each component typechecks, has theme vars, has test |
| **5: Delete C Widgets** | Remove `ui_widget.c`, `ui_widget.h`, `ui_layout.c` | ~2,000 deleted | All demos migrated to Kain components |
| **6: Animation + Pulse** | `animation.kn`, easing curves, SpringValue, Transition | ~800 Kain | 60fps animation with no frame drops |
| **7: Hot Reload** | Component-level render fn pointer swap | ~500 C + Rust | Edit→save→see change without app restart |
| **8: Accessibility** | World-graph → UIA/AT-SPI/AX bridge | ~800 C + Kain | Screen reader navigates all KUIF components |
| **9: Portability** | X11, Wayland, macOS, WASM host backends | ~2,000 C | Same app runs on 5 platforms |
| **10: Advanced** | Cross-process UI, GPU compute in components, time-travel debugging | TBD | Features SwiftUI can't do at all |

### Phase 1 Detail — The Critical Foundation

**Phase 1 is non-negotiable. Everything else depends on it.**

Goal: Extract the C substrate into clean, minimal, widget-free primitives. After Phase 1, `kain_render_rounded_rect()` exists but `ui_button()` does not.

**New files:**
```
runtime/native/src/ui/kain/
├── kain_geometry.h          ← kainRect, kainPoint, kainSize, kainColor, kainMatrix
├── kain_render_software.c   ← 16 draw primitives extracted from ui_renderer.c
├── kain_render_software.h
├── kain_compositor.c        ← damage region tracking
├── kain_compositor.h
├── kain_input.c             ← thin wrapper over event queue
├── kain_input.h
├── kain_font.c              ← font path search extracted from ui_widget.c
├── kain_font.h
├── kain_surface.h           ← forward-looking GPU surface abstraction
├── kain_host.h              ← host interface vtable typedef
└── kain_host_win32.c        ← refactored from ui_host_adapter.c
```

**Files deprecated (kept, excluded from default build):**
- `widgets/ui_widget.c` (1,559 lines)
- `widgets/ui_widget.h` (273 lines, 19 color + 11 size #defines → `theme.kn`)
- `ui_layout.c` (199 lines → Kain HStack/VStack/ZStack/Grid)

**Files refactored:**
- `component_surface.h` → +3 vtable slots (19: set_frame, 20: set_padding, 21: set_flag)
- `ui_renderer.c` → refactored to call `kain_render_*` primitives

**Phase 1 gate:** All 6 existing C demos (cosmic dashboard, retrowave, etc.) compile and render identically. Zero visual regression.

### Phase 3 Detail — Compiler Pipeline

**Goal:** The Kain compiler emits layout + draw calls, pulse/resonate inside components.

**Vtable expansion (18 → 24 slots):**

| Slot | Name | Purpose |
|------|------|---------|
| 19 | `state_get_f64` | Float state (slider value, opacity, animation progress) |
| 20 | `state_set_f64` | Float state write-back |
| 21 | `state_get_string` | String state (textbox content) |
| 22 | `state_set_string` | String state write-back |
| 23 | `element_set_callback` | Bind function pointer to event on node |
| 24 | `element_invoke_callback` | Invoke callback by event name |

**Frontend changes (`crates/core/`):**
- `ast.rs`: Component gets `+pulses`, `+resonates`, `+dimensions` fields
- `parser.rs`: Parse `pulse` / `resonate` blocks inside component bodies
- `types.rs`: State type validation (Float/String/Bool), event handler attr checking

**Codegen changes (`crates/sys-codegen/`):**
- `component.rs`: 5 new `OFF_*` constants, expanded `map_jsx_attr_to_surface_key()`, callback codegen, pulse/resonate emission in render loop
- `mod.rs`: Reuse pulse/resonate lowering for component-internal

### Phase 4 Detail — Kain Widget Library

**Directory structure:**
```
stdlib/ui/
├── core.kn               ← 83+ @extern declarations (ABI bridge to L0)
├── theme.kn              ← Color, Spacing, Theme, DEFAULT_THEME
├── primitives/
│   ├── rect.kn           ← RoundedRect component
│   ├── circle.kn         ← Circle component
│   ├── text.kn           ← Text component
│   ├── interactive.kn    ← InteractiveArea component (hover/press/click/drag)
│   ├── image.kn          ← Image component
│   └── gradient.kn       ← Gradient component
├── layout/
│   ├── stack.kn          ← HStack, VStack, ZStack
│   ├── grid.kn           ← Grid component
│   ├── spacer.kn         ← Spacer component
│   ├── padding.kn        ← Padding component
│   ├── scroll.kn         ← ScrollView component
│   └── divider.kn        ← Divider component
├── components/
│   ├── label.kn          ← Label
│   ├── button.kn         ← Button
│   ├── textinput.kn      ← TextInput
│   ├── checkbox.kn       ← Checkbox
│   ├── slider.kn         ← Slider
│   ├── toggle.kn         ← Toggle
│   ├── progress.kn       ← ProgressBar
│   ├── spinner.kn        ← Spinner
│   ├── tooltip.kn        ← Tooltip
│   ├── badge.kn          ← Badge
│   └── (20+ more in later sub-phases)
├── animation.kn          ← Easing curves, Animated, Transition, SpringValue
├── accessibility.kn      ← Accessibility helpers
└── preview.kn            ← Hot-reload preview harness
```

### Phase 10 — Advanced Features (SwiftUI Can't Do These)

1. **Cross-process UI** — `teleport` state between processes. A Kain component on machine A can display data from a world on machine B.
2. **GPU compute in components** — `dispatch` a compute shader from a component's render. Particle effects, physics, image processing in UI.
3. **Multi-language UI** — `orchestrate` calling Python for data, Rust for rendering, Kain for composition.
4. **Compile-time layout verification** — Z3 checks every layout at compile time. "This VStack will overflow" is a compile error.
5. **Time-travel debugging** — rewind state via the `patch` journal. Step forward and backward through every mutation.
6. **Distributed UI** — render a component tree across multiple displays, synchronized via `entangle` over network.

---

## 10. The C Substrate — What Stays, What Goes, What's New

### What Stays

| File | Why |
|------|-----|
| `ui_system.c` | Retained-mode node tree — solid, Z3-proven, 90+ ABI functions |
| `ui_runtime.c` | UI runtime lifecycle |
| `ui_host_adapter.c` | Refactored into `kain_host_win32.c` — core logic preserved |
| `ui_hot_reload.c` | Hot-reload infrastructure — extended for component-level swaps |
| `ui_renderer.c` | Refactored to call `kain_render_*` primitives |
| `component_surface.c` | Vtable registry — expanded from 18 to 24 slots |
| `native_ui_surface.c` | GDI backend — stays as software fallback |
| `graphics_system.c` | Raw graphics kernel for GPU backends |
| `input_system.c` | Input session management |
| All Z3 proof packs | 42+ proofs remain valuable and valid |

### What Goes — The Kill List

| File | Lines | Replaced By |
|------|-------|-------------|
| `widgets/ui_widget.c` | 1,559 | `stdlib/ui/components/*.kn` (all 30+ widgets) |
| `widgets/ui_widget.h` | 273 | `stdlib/ui/theme.kn` (19 colors + 11 sizes → Kain structs) |
| `ui_layout.c` | 199 | `stdlib/ui/layout/*.kn` (HStack/VStack/ZStack/Grid) |
| `ui_color.c` | 121 | Colors are `kainColor {r,g,b,a}` — no C-level constants |
| Draw command ring buffer tree-walker | ~300 | Compiler emits draw calls directly |

### What's New

| File | Lines | Purpose |
|------|-------|---------|
| `kain_geometry.h` | 100 | kainRect, kainPoint, kainSize, kainColor, kainMatrix |
| `kain_render_software.c` | 500 | 16 draw primitives, no tree-walking |
| `kain_compositor.c` | 150 | Damage region tracking |
| `kain_input.c` | 120 | Thin wrapper over existing event queue |
| `kain_font.c` | 200 | Font path search extracted from ui_widget.c |
| `kain_host.h` | 80 | Host interface vtable typedef |
| `kain_host_win32.c` | 600 | Refactored from ui_host_adapter.c |
| `kain_render_vulkan.c` | ~1,500 | Vulkan renderer (Phase 2) |
| `kain_render_webgpu.c` | ~1,000 | WebGPU renderer (Phase 2) |

### The C API — One Page Reference

```c
// Session lifecycle
kainSession* kain_create(int width, int height, const char* app_name);
void kain_destroy(kainSession* session);
void kain_begin_frame(kainSession* s, float delta_ms);
void kain_end_frame(kainSession* s);
void kain_present(kainSession* s);

// Node tree (called by generated Kain code via the vtable)
int64_t kain_node_create(kainSession* s, const char* kind);
void kain_node_set_frame(kainSession* s, int64_t node, kainRect rect);
void kain_node_set_text(kainSession* s, int64_t node, const char* text);
void kain_node_set_style(kainSession* s, int64_t node, const char* key, int64_t val);
void kain_node_set_padding(kainSession* s, int64_t node, float t, float r, float b, float l);
void kain_node_set_flag(kainSession* s, int64_t node, const char* flag, int enabled);

// Render primitives (16 commands, backend-agnostic)
void kain_render_clear(kainSession* s, kainColor c);
void kain_render_rect(kainSession* s, kainRect rect, kainColor c, kainBlendMode blend);
void kain_render_rounded_rect(kainSession* s, kainRect rect, float r, kainColor c);
void kain_render_circle(kainSession* s, kainPoint center, float radius, kainColor c);
void kain_render_text(kainSession* s, kainPoint pos, const char* text, int64_t font, float size, kainColor c);
void kain_render_gradient(kainSession* s, kainRect rect, const kainColor* stops, const float* pos, int n);
void kain_render_blit(kainSession* s, kainRect src, kainRect dst, int64_t texture);
void kain_render_blur(kainSession* s, kainRect rect, float radius);
void kain_clip_push(kainSession* s, kainRect rect);
void kain_clip_pop(kainSession* s);
void kain_transform_push(kainSession* s, kainMatrix m);
void kain_transform_pop(kainSession* s);
void kain_render_submit(kainSession* s);

// Compositor (damage tracking)
void kain_damage(kainSession* s, kainRect rect);
void kain_damage_node(kainSession* s, int64_t node);
kainRect kain_damaged_region(kainSession* s);

// Input
kainEvent kain_poll_event(kainSession* s);
int64_t kain_hit_test(kainSession* s, kainPoint p, int64_t root);

// Font
int64_t kain_font_load(kainSession* s, const uint8_t* ttf, int len, float size);
float kain_font_measure(kainSession* s, int64_t font_id, const char* text);
kainFontMetrics kain_font_metrics(kainSession* s, int64_t font_id);
```

---

## 11. Gotchas and Pitfalls — What Will Break If You're Not Careful

### Pitfall 1: The C `#define` Migration Must Be Complete

**The problem:** `ui_widget.h` has 19 `#define UI_COLOR_*` constants and 11 `#define UI_*_SIZE` constants. If even ONE hardcoded constant remains in C after Phase 4, you'll have two sources of truth for theme values — C and Kain.

**The fix:** After Phase 4, grep `runtime/native/` for any remaining `#define UI_COLOR` or `#define UI_WIDTH`. There should be ZERO. Every color and size must come from `stdlib/ui/theme.kn`.

```bash
# The gate command — run after Phase 4:
rg "#define UI_COLOR|#define UI_.*_WIDTH|#define UI_.*_HEIGHT|#define UI_.*_SIZE" runtime/native/
# Expected output: NOTHING
```

### Pitfall 2: Vtable Slot Order Must Match Exactly

**The problem:** The `KainComponentSurface` vtable in `component_surface.h` and the `OFF_*` constants in `crates/sys-codegen/src/codegen_llvm/component.rs` are an ABI contract. If they drift by even one slot, every component renders garbage or crashes.

**The fix:** Phase 3 adds slots 19-24. After EVERY vtable change, run the LLVM codegen test:

```bash
cargo test -p kain-sys-codegen llvm_lowers_native_ui_primitives
cargo test -p kain-sys-codegen llvm_generates_component_and_jsx_calls
```

**The invariant:** `OFF_SESSION_CREATE` = 0. `OFF_*` values must be sequential integers matching the `KainComponentSurface` field order. Never insert, reorder, or delete slots — only append.

### Pitfall 3: JSX Attribute Mapping Is a Union of All Backends

**The problem:** `map_jsx_attr_to_surface_key()` in `component.rs` maps JSX attribute names to style keys. These style keys must be understood by the C backend. If you add a new JSX attribute without also adding its style key handler in `native_ui_surface.c`, the attribute is silently ignored.

**The fix:** Every JSX attribute in `map_jsx_attr_to_surface_key()` must have a corresponding handler in the C backend's `element_set_attr_*` functions. Maintain a cross-reference table.

### Pitfall 4: Resonate Self-Feedback Is a Compile Error

**The problem:** A `resonate` handler that writes to its own trigger field creates an infinite recursion or is rejected at compile time.

**The fix:** Resonate handlers should write to DIFFERENT world fields:
```kn
// ✅ CORRECT: resonate on slider_value → writes to slider_fill (different field)
resonate AppState.slider_value dampen 0ms:
    AppState.slider_fill = compute_fill(resonate_new_i64)

// ❌ WRONG: resonate on count → writes to count (self-loop)
resonate AppState.count dampen 0ms:
    AppState.count = AppState.count + 1   // compile error
```

### Pitfall 5: Dampening Covers Legitimate Rapid Changes

**The problem:** Setting `dampen` too high (e.g., `dampen 500ms`) means rapid user interactions (typing, slider dragging) appear laggy because the handler only fires every 500ms.

**The fix:** Use `dampen 0ms` for interactions that must feel instantaneous (typing, dragging). Use `dampen 16-32ms` for visual updates driven by fast-changing data. Use `dampen 100ms+` only for genuinely expensive operations.

### Pitfall 6: The `ask()` Single-Payload Constraint

**The problem:** `ask(actor, "MessageName", payload)` accepts a single `Int` payload. Multi-field messages require packing.

**The fix:** Use the packing pattern from `fusion_chain.kn`:
```kn
const PACK_SHIFT: Int = 100000
fn pack(a: Int, b: Int) -> Int: return a + b * PACK_SHIFT
fn unpack_a(packed: Int) -> Int: return packed % PACK_SHIFT
fn unpack_b(packed: Int) -> Int: return packed / PACK_SHIFT
```

### Pitfall 7: World Fields Must Be Initialized

**The problem:** Every `state field: Type = initial` must have an initializer. Uninitialized world state is a compile error. The initializer must match the declared type.

**The fix:** Always provide explicit initializers. For `ptr<T>` fields, use `int_to_ptr(0, "Type")` as the sentinel.

### Pitfall 8: Entangle Endpoints Must Have Matching Types

**The problem:** `entangle A.field <-> B.field with single_writer` requires both fields to have the same type. Mismatched types are a compile error.

**The fix:** The mirror field must have the same type as the authority field. For `String` fields, this is a deep-equality check — both must be `String`.

### Pitfall 9: Phase 1 Must Preserve Exports

**The problem:** The `abi_ui_*` functions in `ui_host_adapter.c` are the ABI that LLVM-emitted code calls. If Phase 1 renames or removes any of these ABI exports, all existing compiled Kain code breaks.

**The fix:** Phase 1 is an **internal refactor** — the public `abi_ui_*` surface must remain ABI-stable. New `kain_render_*` functions are ADDITIONS, not replacements. Internal implementation can change; public ABI signatures cannot.

### Pitfall 10: Test Before Delete

**The problem:** Deleting `ui_widget.c` in Phase 5 without verifying that all its callers have been migrated will leave dangling references.

**The fix:** Before Phase 5 deletion:
1. Inventory every call to `ui_button()`, `ui_slider()`, etc. in the codebase
2. Verify each has a Kain component equivalent
3. Run the full test suite
4. Only then delete

---

## 12. Testing and Proof Strategy

### The 4-Level Verification Ladder

| Level | What | Tool | Evidence |
|-------|------|------|----------|
| **L0: Typecheck** | Components parse + typecheck correctly | `kain check --json` | Zero errors |
| **L1: Unit Tests** | Component behavior is correct | `kain test` with `std::test` | Zero failures |
| **L2: Oracle Proof** | UI actually renders on screen | `oracle scan → launch → debug → matrix → verify → delta` | OS-level telemetry |
| **L3: Z3 Proof** | Invariants are mathematically proven | `z3 prove` on layout bounds, state machines | UNSAT (no counterexample) |

### Z3 Proof Targets for KUIF

| Proof | What's Checked | Priority |
|-------|---------------|----------|
| Component instance bounds | No component creates more than MAX_CHILDREN children | High |
| World state type safety | `patch` mutations match declared state types | High |
| Entangle cycle freedom | No circular entanglements between worlds | High |
| Entangle synchronous depth | Propagation chain depth < MAX_DEPTH | Medium |
| Pulse clock period | `pulse every Nms` where N ≥ 1 | Medium |
| Render command buffer bounds | Frame's draw commands fit in command buffer | Medium |
| Clip rect stack depth | `push_clip`/`pop_clip` nesting ≤ MAX_CLIP_DEPTH | Medium |
| Transform stack depth | Stack nesting ≤ MAX_TRANSFORM_DEPTH | Low |
| Glyph cache bounds | Atlas doesn't exceed texture size | Low |
| Framebuffer write bounds | All coordinates inside framebuffer | Low |

### Oracle Verification — The Mandatory Workflow

Every Kain-built KUIF `.exe` must be Oracle-verified:

```bash
# Step 1: Scan for the freshest .exe
oracle scan --dir <project> --limit 5

# Step 2: Launch it
oracle launch <freshest.exe> --wait 3000

# Step 3: Prove window creation
oracle debug --pid <pid>

# Step 4: Find valid GUI window
oracle find --pid <pid> --timeout 10000

# Step 5: Prove rendering (not black screen)
oracle matrix --handle <handle> --text

# Step 6: Prove UI responds to input
oracle verify --handle <handle> --do "click:400,300" --expect "pixels>100"

# Step 7: Prove render loop is alive
oracle delta --handle <handle> --interval 200
```

### The Telemetry Delta Guard — Prove Semantics Fired

Every benchmark and test that claims to exercise semantic features MUST use the delta guard pattern:

```kn
fn test_results() -> Int:
    let patch_before = patch_journal_count()
    let entangle_before = entangle_propagation_count()
    let resonate_before = resonate_fire_count()
    let pulse_before = runtime_machine_pulse_total_fire_count()

    // ... exercise the system ...

    let patch_delta = patch_journal_count() - patch_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let resonate_delta = resonate_fire_count() - resonate_before
    let pulse_delta = runtime_machine_pulse_total_fire_count() - pulse_before

    if patch_delta < 1:    return -20
    if entangle_delta < 1: return -21
    if resonate_delta < 1: return -22
    if pulse_delta < 1:    return -23
    return 0
```

---

## 13. The Grand Vision — What KUIF Enables That Nothing Else Can

### SwiftUI Is the Floor, Not the Ceiling

SwiftUI took Apple 5 years and a trillion-dollar valuation to achieve. It's declarative, reactive, GPU-accelerated, and composable. It is the best UI framework ever shipped.

**Here's what KUIF can do that SwiftUI cannot:**

| Capability | Why SwiftUI Can't | Why KUIF Can |
|------------|-------------------|-------------|
| **Journaled state** | SwiftUI @State has zero introspection. You can't replay, undo, or audit mutations. | Every `patch` records old→new in a compiler-managed journal. Rewind state, step forward, prove what changed. |
| **Compile-time layout verification** | SwiftUI crashes at runtime on layout overflow. | Z3 checks layout bounds at compile time. "This VStack will overflow" is a compile error, not a runtime crash. |
| **Zero-copy cross-process UI** | SwiftUI is process-bound. | `teleport` moves state between machines via typed buses. A component on machine A renders data from a world on machine B. |
| **GPU compute in UI components** | SwiftUI layout + rendering is CPU-only (Metal is just for rendering). | `dispatch` a compute shader from a component's render. Particle effects, physics, image processing — first-class. |
| **Multi-language orchestration** | SwiftUI is Swift-only. | `orchestrate` stages can call Python (NumPy, PyTorch), Rust, C, CUDA — all in one component pipeline. |
| **First-class accessibility** | SwiftUI accessibility is bolted on; the visual and accessibility trees can drift. | World-graph accessibility — the same `world` drives the visual tree AND the accessibility tree. No duplication, no drift. |
| **Hot reload without IDE** | SwiftUI preview requires Xcode. | `kain watch` + component-level function pointer swap. Any text editor. No IDE required. |
| **Cross-platform from one codebase** | SwiftUI is Apple-only (SwiftUI on Windows/Linux is nonexistent). | Same `component Button` renders identically on Win32, X11, Wayland, macOS, and WASM. |
| **Single-writer guarantee at compile time** | SwiftUI @Binding can create circular update paths that crash at runtime. | `entangle ... with single_writer` enforces unidirectional data flow at the typechecker level. Mirror writes are compile errors. |
| **Dampened reactivity** | SwiftUI @Published fires every change synchronously. Mutate 1000 times = re-render 1000 times. | `resonate field dampen Nms` absorbs storms. Configure per-field. |

### The Platform Advantage

SwiftUI is a **framework** layered on UIKit/AppKit, which is layered on Core Animation/Metal, which is layered on the OS. Each layer adds indirection, copies state, and hides bugs behind opaque runtime behavior.

KUIF is a **language platform**. The `component` keyword, `world` state graphs, `entangle` bindings, `patch` journaling, `resonate` tripwires, `pulse` clocks — these are **compiler semantics**, not library abstractions. The compiler:

1. **Desugars** components into world state graphs
2. **Verifies** layout bounds with Z3 at compile time
3. **Compiles** JSX into straight-line C vtable calls — no interpreter, no VDOM, no reconciliation loop
4. **Lowers** `resonate` handlers to guarded post-store calls — no observer registry, no heap allocations
5. **Emits** `pulse` blocks as fixed-size state machines with scheduler registration
6. **Records** every `patch` mutation in a bounded, Z3-proven journal

**This is why KUIF will surpass SwiftUI.** Not because we're smarter than Apple's engineers. Because we built the language around the UI paradigm, instead of trying to retrofit declarative UI onto a language not designed for it.

Property wrappers, result builders, and `@main` are band-aids over language gaps. `component`, `world`, `entangle`, `patch`, `resonate`, and `pulse` are first-class semantics.

---

## 14. Research Document Index — Every File and Why It Matters

### Primary Documents — Read These First

| File | What It Is | Why It Matters |
|------|-----------|---------------|
| **`X:/runtime/native/src/ui/WIDGETS.md`** | The KUIF manifesto — 1,401 lines of architectural vision | Defines the entire target architecture, the inversion, layer responsibilities, component signatures, the C substrate API, layout system, animation model, hot reload design, portability model, and 10-phase roadmap. This is the original vision document. |
| **`X:/runtime/native/src/ui/research/KUIF_IMPLEMENTATION_PLAN.md`** | Unified implementation plan from 3 runtime-agent assessments | Maps every file that changes, net new/deleted lines per phase, vtable slot expansion, compiler frontend/codegen changes, stdlib migration path. The concrete implementation blueprint. |
| **`X:/runtime/native/src/ui/research/RENDER-AND-UI-MAP.md`** | Complete 122-file map across 16 layers | Every Rust crate file and C runtime file related to UI. From parser to SPIR-V codegen. The file-level dependency graph of the entire UI pipeline. |
| **`X:/docs/RULEBOOK.md`** | The Kain decision ladder — 1,485 lines | Which construct for which problem. The definitive reference for the semantic stack. Every KUIF developer must internalize this. |
| **`X:/docs/KAIN_BY_EXAMPLE.md`** | Every Kain feature with a compilable snippet | Canonical syntax for every construct. Use this when writing any KUIF code. |

### Semantic Construct Deep Dives

| File | Construct | Lines | Key Patterns |
|------|-----------|-------|-------------|
| `X:/docs/COMPONENT.MD` | `component` | 1,513 | Props, state, methods, JSX, tag dispatch, world wiring, anti-patterns, limitations from fuzz testing |
| `X:/docs/WORLD.MD` | `world` | 1,552 | Authority+mirror pattern, surface projections, LLVM codegen, runtime contract metadata |
| `X:/docs/ENTANGLE.MD` | `entangle` | 1,146 | Single-writer policy, endpoint resolution, native registry (128 max), dual-world pattern |
| `X:/docs/PATCH.MD` | `patch` | 1,788 | Journal semantics, undo modes, epoch counters, telemetry, orchestrate integration |
| `X:/docs/LAW.MD` | `law` | ~850 | Invariant predicates, Z3 integration, orchestrate stage guards |
| `X:/docs/RESONATE.MD` | `resonate` | 1,747 | Dampening, reentry guard, body locals, self-feedback rule, broad effect permissions |
| `X:/docs/PULSE.MD` | `pulse` | 1,183 | Temporal beat, jitter tolerance, scheduler thread, timing locals, animation driver |
| `X:/docs/CONVERGE.MD` | `converge` | ~1,200 | Spec + fast lanes, `verify random(N)`, lane selection, capability probing |
| `X:/docs/ORCHESTRATE.MD` | `orchestrate` | ~1,400 | Multi-runtime pipeline, residency/transfer/fallback, stage graph validation |
| `X:/docs/TELEPORT.MD` | `teleport` | ~800 | Zero-copy cross-world handoff, typed bus, pointer routing |
| `X:/docs/SHATTER.MD` | `shatter` | ~600 | Structure-of-Arrays layout, GPU/SIMD, SoA metadata |

### Canonical Code References

| File | What It Is | Why It Matters |
|------|-----------|---------------|
| `X:/benchmark/cases_v2/keyword_crucible.kn` | 108/110 keywords exercised in 7 cases | The definitive syntax reference. Every KUIF component should pattern-match against this. |
| `X:/benchmark/cases_v2/fusion_chain.kn` | All 7 semantic layers fused in one causal chain | The proof that all constructs compose. Resonate → orchestrate → converge → actor → teleport → world in one loop iteration. |
| `X:/blades/window_proof/src/main.kn` | 17-line minimal window — Oracle-verified | The canonical world→surface→component wiring proof. |
| `X:/blades/window_proof/README.md` | Vtable slot table, known gaps, working demos | The platform-level documentation for component rendering. |
| `X:/blades/kain/component_fuzz/src/components.kn` | 40+ components across 10 categories | Fuzz-tested component patterns: naked, stateful, computational, recursive, fragments, pointer-laden, deeply nested. |

### Runtime and Compiler References

| File | What It Is | Why It Matters |
|------|-----------|---------------|
| `X:/runtime/native/include/component_surface.h` | 18-slot vtable struct | The ABI contract between compiler and backend. Must match `OFF_*` in `component.rs`. |
| `X:/runtime/native/src/ui/native_ui_surface.c` | GDI backend implementation | The working software renderer. Reference for new backends. |
| `X:/crates/sys-codegen/src/codegen_llvm/component.rs` | Component LLVM codegen — vtable lowering | Where JSX becomes vtable calls. Map of attribute names to style keys. |
| `X:/crates/core/src/ast.rs` | Component/JSX/State AST nodes | What the parser produces. Lines 543-617 for component types. |
| `X:/crates/core/src/parser.rs` | Component/JSX parsing | Where `component` becomes AST. Line 2541 for `parse_component()`. |
| `X:/crates/core/src/types.rs` | TypedComponent, JSX typechecking | Where components are validated. Line 7955 for `check_component()`. |
| `X:/stdlib/ui.kn` | Current stdlib UI module — 1,677 lines | The old bridge. Shrinks to ~30-line re-export hub in Phase 4. |
| `X:/stdlib/ui/component.kn` | Current component bridge — 158 lines | The old immediate-mode wrapper. Deprecated in Phase 4. |

### Architecture and Process

| File | What It Is |
|------|-----------|
| `X:/runtime/native/src/ui/BASELINE.md` | Baseline state of the current UI system |
| `X:/runtime/native/src/ui/gap_analysis.md` | Gap analysis: current state vs KUIF target |
| `X:/runtime/native/src/ui/research/KUIF_MASTER_GUIDE.md` | **THIS DOCUMENT** — the Rosetta Stone |
| `X:/GLOSSARY.MD` | Maps every Kain term to its physical location |
| `X:/CATALOG.MD` | Language surface quick reference |
| `X:/MEMORY.md` | Distilled lessons from complex work |
| `X:/FEEDBACK.md` | Systemic language/toolchain blockers |
| `X:/BUGS.md` | Confirmed defects and sharp edges |

---

## Epilogue — Stop Writing C Widgets

The current `ui_widget.h` has 19 color `#define`s and 11 size `#define`s. It's a 55KB `.c` file with 20 widgets crammed together, held together by C state machines and manual copy functions.

This was fine for bootstrapping. It proved the C substrate works, the vtable ABI is solid, the Z3 proofs hold. But it's not the destination.

The destination is a UI platform where:

- **Every widget is a Kain `component`** with typed props, local state, methods, and a JSX render tree
- **Every state change flows through `patch`** — journaled, undoable, provable
- **Every reactive binding is `entangle`** — compiler-owned, zero-cost, single-writer guaranteed
- **Every animation is `pulse`** — scheduler-owned, jitter-tolerant, first-class timing
- **Every state change reaction is `resonate`** — no polling, no observer registries, dampened
- **Every invariant is `law`** — compile-time verifiable, Z3-proved
- **Every platform optimization is `converge`** — spec lane + fuzz-tested fast lanes
- **Every cross-runtime pipeline is `orchestrate`** — typed stages, explicit deps, residency tracking
- **Every cross-world transfer is `teleport`** — zero-copy, compiler-owned

**SwiftUI is what you build when you have a trillion dollars and 5 years. KUIF is what you build when you have a language that understands UI at the semantic level.**

One is a framework. The other is a platform.

**Stop writing C widgets. Start writing Kain components.** The future of KUIF is written in `.kn`, not `.c`.

---

> *"The compiler owns the truth. The C substrate pushes the pixels. The components are the identity."*
>
> — The KUIF Master Guide, 2026-06-25
