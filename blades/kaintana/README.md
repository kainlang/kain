# Kaintana -- Kain's Desktop UI Framework

**Location:** `blades/ui/kaintana/`
**Package family:** `kaintana`, `kaintana-test`, `kaintana-vulkan`, `kaintana-vulkan-test`
**Entry:** `src/kaintana.kn`
**Build entry:** `src/main.kn`
**35 files · 27 modules · 4 color themes · 3 platform backends · 10 examples**

---

## What Is Kaintana?

Kaintana is a **blade-owned retained + immediate UI framework** built entirely in Kain, with Kain's compiler-owned semantic stack as its backbone. It is to `std::ui` what React is to the DOM ___ a retained-mode widget system on top of a lower-level immediate-mode host.

It runs on Windows today via a **GDI/GDI+ desktop bridge**, with additional platform backends for **Vulkan** (via `std::graphics` + `blades/vulkain`) and **Winit** (via `std::ui`). It supports hot reload, IME, clipboard, menus, dialogs, popovers, focus management, keyboard action binding, agent intent injection, frame/host reporting, screenshot capture, and harness artifacts for regression testing.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                    Kaintana (Kain)                        │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │        Semantic Layer (world/entangle/patch/      │   │
│  │          law/resonate/axiom)                      │   │
│  │  KaintanaReactivity → KaintanaReactivityMirror    │   │
│  └──────────────────────────────────────────────────┘   │
│                          │                                │
│  ┌───────────────────────┴────────────────────────┐      │
│  │              API Layer (builder pattern)         │      │
│  │  kaintana_panel/label/button/text_input/slider  │      │
│  │  (+ extras: toggle, checkbox, badge, metric,    │      │
│  │   chart_bar, progress, collapsing_header,       │      │
│  │   tooltip, spinner, toast, status_bar, toolbar, │      │
│  │   dropdown)                                      │      │
│  └───────────────────────┬────────────────────────┘      │
│                          │                                │
│  ┌───────────────────────┴────────────────────────┐      │
│  │            Core Engine                          │      │
│  │  ┌──────────┐ ┌──────────┐ ┌────────────────┐  │      │
│  │  │ Types    │ │ Layout   │ │ Reconciliation │  │      │
│  │  │ (rect,   │ │ (inset,  │ │ (stable-key    │  │      │
│  │  │  color,  │ │  split,  │ │  diff, slot    │  │      │
│  │  │  theme,  │ │  column, │ │  map, arena)   │  │      │
│  │  │  ctx)    │ │  grid)   │ │                │  │      │
│  │  └──────────┘ └──────────┘ └────────────────┘  │      │
│  │  ┌──────────────┐ ┌──────────────┐             │      │
│  │  │ Render Cmds  │ │ Widget Events│             │      │
│  │  │ (fill, text) │ │ (pointer,    │             │      │
│  │  │              │ │  slider,     │             │      │
│  │  │              │ │  activation) │             │      │
│  │  └──────────────┘ └──────────────┘             │      │
│  │  ┌───────────────────┐                         │      │
│  │  │ Input System      │                         │      │
│  │  │ (action binding,  │                         │      │
│  │  │  axis, agent int.)│                         │      │
│  │  └───────────────────┘                         │      │
│  └───────────────────────┬────────────────────────┘      │
│                          │                                │
│  ┌───────────────────────┴────────────────────────┐      │
│  │          Platform Adapters                      │      │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐        │      │
│  │  │ Desktop  │ │ Vulkan   │ │ Winit    │        │      │
│  │  │ (GDI/GDI+│ │ (std::   │ │ (std::ui)│        │      │
│  │  │  via C)  │ │ graphics)│ │          │        │      │
│  │  └──────────┘ └──────────┘ └──────────┘        │      │
│  └─────────────────────────────────────────────────┘      │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Build System (build.kn)                         │   │
│  │  check → compile → certify → capsule_set        │   │
│  └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

---

## The Four Authoring Modes

Kaintana supports four distinct ways to build a UI, from lowest to highest level.

### 1. Primitives (`kaintana_primitive_fill`, `kaintana_primitive_text`, `kaintana_render_focus_ring`)

Raw drawing commands. Draw a colored rectangle or text at a pixel position. No reconciliation, no hit testing. Use for custom visuals that don't need to be interactive.

```kn
let _bar = kaintana_primitive_fill(session, parent, "wave.bar0",
    kaintana_rect(wave_rect.x + 22.0, wave_rect.y + 84.0, 60.0, 52.0), theme.signal)
let _note = kaintana_primitive_text(session, parent, "wave.note",
    "desktop bridge primitives keep pace", kaintana_rect(x, y, w, h), theme.muted, font, 12.0)
```

### 2. Immediate Widgets (`kaintana_immediate_*`)

One-shot interactive widgets. You call a function that takes state, a rect, and a theme, and returns a `KaintanaRenderResult` with the updated context and activation state. Widget state (checked, toggled, open) is persisted across frames via `ui_state_bool` / `ui_state_set_bool` behind a stable key.

```kn
let result = kaintana_immediate_button(session, parent, "my.button", "Click",
    kaintana_rect(10.0, 10.0, 120.0, 36.0), theme, font, 22.0)
if result.activated != 0:
    // button was clicked
```

### 3. Retained Widgets (`kaintana_retained_*`)

Managed nodes with stable keys that survive across frames. These use `kaintana_reconcile_visual_node` which reconciles by stable key * * * same as React's keyed diff. The node stays alive until the key disappears.

```kn
let node = kaintana_retained_region(session, parent, "showcase.sidebar",
    "sidebar", rect, theme)
let title = kaintana_retained_label(session, node, "sidebar.title",
    "HOT RELOAD", title_rect, theme, font, 18.0)
```

### 4. Builder-Pattern API (`kaintana_panel/label/button/text_input/slider`)

A fluent builder layer over the immediate widgets. Each widget has setters (`_key`, `_rect`, `_font`, `_muted`) that return a new builder struct, then `_render` executes it. This is the most ergonomic way to assemble UIs.

```kn
let b0 = kaintana_button(kaintana_ui_state(ctx), "Click")
let b1 = kaintana_button_key(b0, "demo.btn")
let b2 = kaintana_button_rect(b1, kaintana_rect(x, y, 120.0, 36.0))
let b3 = kaintana_button_font(b2, body_font, 22.0)
let result = kaintana_button_render(ctx, b3)
```

---

## Semantic Layer (Kain's Superpower)

What makes Kaintana unique is that it's built on Kain's compiler-owned semantic stack. No other UI framework has this.

### Worlds + Entangle (Reactive State Authority)

```kn
world KaintanaReactivity:
    state frame: Int = 0
    state signal: Int = 0
    state layout_revision: Int = 0
    state draw_command_count: Int = 0
    surface native_ui => KaintanaReactivityPanel

world KaintanaReactivityMirror:
    state signal_copy: Int = 0
    surface web => KaintanaReactivityPanel

entangle KaintanaReactivity.signal <-> KaintanaReactivityMirror.signal_copy
    with single_writer
```

The authority world owns mutable state. The mirror world receives entangled updates. This is compiler-tracked propagation ->> not a runtime observer pattern.

### Patches (Journaled Mutation)

```kn
patch kaintana_reactivity_commit(authority: KaintanaReactivity, value: Int) -> Int:
    authority.signal = value
    return authority.signal
```

Every patch bumps the journal. You can inspect it at runtime: `kaintana_semantic_patch_journal_depth()`.

### Laws (Invariant Enforcement)

```kn
law frame_budget_valid(budget: Int) -> Bool:
    return budget >= 8 and budget <= 1000
```

Invariants are part of the runtime contract, not hidden in `if` statements.

### Resonate (Dampened Reactive Handlers)

```kn
resonate KaintanaReactivity.signal dampen 16 ms:
    KaintanaReactivity.layout_revision =
        KaintanaReactivity.layout_revision + resonate_new_i64

resonate KaintanaReactivity.draw_command_count dampen 8 ms:
    KaintanaReactivity.frame = KaintanaReactivity.frame + 1
```

When `signal` changes (via `patch`), the resonate handler fires after a 16ms dampen window. This updates the layout revision automatically ... no callback registry, no manual observer wiring.

### Axiom (Compile-Time Capability Gating)

```kn
axiom kaintana_ui_truth:
    when target("llvm")
    when capability("ui.components")
    when capability("ui.runtime-bundle")
    guarantee "kaintana desktop ui framework is supported"
    fallback kaintana_axiom_fallback
```

The entire framework is gated behind compile-time capability checks. Dead code is eliminated at compile time.

### Semantic Monitoring

```kn
kaintana_semantic_patch_journal_depth()    → patch_journal_count()
kaintana_semantic_entangle_propagation_count() → entangle_propagation_count()
kaintana_semantic_converge_mismatches()    → converge_mismatch_count()
kaintana_semantic_resonate_fires()         → resonate_fire_count()
kaintana_semantic_resonate_absorbs()       → resonate_absorb_count()
kaintana_semantic_orchestrate_stages()     → orchestrate_stage_count()
```

These wrappers let framework consumers prove that the semantic machinery is actually firing ⁓ not just that the UI looks right.

---

## Reconciliation Engine

Kaintana maintains a **slot map** (stable-ID map) of UI nodes per session. Each frame:

1. **`kaintana_context_begin_frame`** <--> resets the frame arena, begins a new `ui_frame`, pumps events.
2. **`kaintana_reconcile_node`** |-> looks up a stable key in the `typed_map`. If found, reconciles (updates rect/properties on the existing native node). If not found, creates a new native node and inserts it.
3. `kaintana_context_mark_command` ->> records each draw command with a checksum for shape verification.
4. **`kaintana_context_commit_frame`** - submits the frame and commits the hot-reload state.

This is **exactly React's keyed reconciliation** --- same idea, different language. Stable keys ensure that nodes survive across frames unless the key disappears, at which point they're garbage collected by the native UI runtime.

---

## Platform Backends

### Desktop (Default) 〰 `platform/desktop/desktop_adapter.kn`

- **Backend:** Windows GDI/GDI+ via `user32` + `gdi32`
- **Command buffer:** 2048-command ring buffer (`rect` + `text`)
- **Window:** `kaintana_desktop_host_run_window` creates a native Win32 window, pumps messages, and renders the command buffer each frame
- **Capture:** BMP screenshot via `kaintana_desktop_host_write_screenshot`
- **Reports:** Text report via `kaintana_desktop_host_write_report`

### Vulkan --- `platform/vulkan/vulkan_adapter.kn`

- **Bridge:** `blades/vulkain` via `use c::vulkain_bridge`
- **Session:** `graphics_session_create` with SPIR-V staging
- **Pipeline:** vertex shader + fragment shader → graphics pipeline → draw mesh → present
- **Proved via** `kaintana-vulkan-test` blade

### Winit ⁓ `platform/winit/winit_adapter.kn`

- **Bridge:** `std::ui` host session
- **Session lifecycle:** create → pump → present → destroy
- **Session can be borrowed** from an existing `KaintanaContext` via `kaintana_winit_adapter_from_context`

---

## Widget Inventory

| Widget | Function | Builder API Available | Interactive | Notes |
|--------|----------|----------------------|-------------|-------|
| **Panel** | `kaintana_immediate_panel` | ✅ | No | Colored rect with title + accent rule |
| **Label** | `kaintana_retained_label` | ✅ | No | Text node, supports muted variant |
| **Button** | `kaintana_immediate_button` | ✅ | ✅ | Hover/pressed states, activation callback |
| **Toolbar Button** | `kaintana_immediate_toolbar_button` | ❌ | ✅ | Shell-colored button with signal bottom rule |
| **Text Input** | `kaintana_immediate_text_input` | ✅ | ✅ | Focusable, IME-compatible, accent/signal rule |
| **Slider** | `kaintana_immediate_slider` | ✅ | ✅ | Draggable, track + fill + knob, value return |
| **Toggle** | `kaintana_widget_toggle` | ❌ | ✅ | Switch-style on/off, state persisted across frames |
| **Checkbox** | `kaintana_widget_checkbox` | ❌ | ✅ | Checked state, signal mark |
| **Badge** | `kaintana_widget_badge` | ❌ | No | Shell-colored status label |
| **Metric** | `kaintana_widget_metric` | ❌ | No | Label + right-aligned value |
| **Chart Bar** | `kaintana_widget_chart_bar` | ❌ | No | Horizontal bar with label + value |
| **Separator** | `kaintana_widget_separator` | ❌ | No | 1px horizontal rule |
| **Progress Bar** | `kaintana_widget_progress_bar` | ✅ `kaintana_progress_bar` | No | Track + fill with centered value label |
| **Collapsing Header** | `kaintana_widget_collapsing_header` | ❌ | ✅ | Accordion header, open/close toggle. Use `kaintana_collapsing_header_begin` for auto-child hiding |
| **Tooltip** | `kaintana_widget_tooltip` | ✅ `kaintana_tooltip` | Hover | Appears above anchor on hover. Builder resolves anchor by stable key |
| **Spinner** | `kaintana_widget_spinner` | ❌ | No | Animated dot, frame index tracked |
| **Toast** | `kaintana_widget_toast` | ❌ | No | Signal-left-border notification, auto-hides after 180 frames |
| **Status Bar** | `kaintana_widget_status_bar` | ❌ | No | Shell-colored bar with left/right text |
| **Toolbar** | `kaintana_widget_toolbar` | ❌ | No | Shell bar with bottom rule |
| **Dropdown** | `kaintana_widget_dropdown` | ✅ `kaintana_dropdown` | ✅ | Opens inline popup with items |
| **Scroll Area** | `kaintana_scroll_area` | ✅ `kaintana_scroll_area_builder` | No | Viewport + content height, scrollbar thumb, frustum culling helper |

### Extra Systems

| System | API | Notes |
|--------|-----|-------|
| **Focus Management** | `kaintana_focus_node`, `kaintana_focused_node`, `kaintana_render_focus_ring` | Keyboard-navigable focus ring |
| **Clipboard** | `kaintana_clipboard_copy_text`, `kaintana_clipboard_text` | System clipboard via `ui_clipboard_*` |
| **IME** | `kaintana_ime_begin`, `kaintana_ime_commit_text`, `kaintana_ime_active_node`, `kaintana_ime_text` | CJK text input support |
| **Menu System** | `kaintana_menu_create`, `kaintana_menu_add_item`, `kaintana_menu_open_below_node`, `kaintana_menu_active`, `kaintana_menu_item_count`, `kaintana_menu_item_command` | Programmatic context menus |
| **Dialog System** | `kaintana_dialog_request`, `kaintana_dialog_respond`, `kaintana_dialog_poll_response`, `kaintana_dialog_response_text` | Request/respond/poll dialogs |
| **Popover** | `kaintana_popover_open`, `kaintana_popover_close`, `kaintana_popover_is_open`, `kaintana_popover_rect` | Attachment-based popups |
| **Action Binding** | `kaintana_action_bind`, `kaintana_key_down_binding`, `kaintana_key_up_binding`, `kaintana_action_pressed` | Declarative key → action map |
| **Axis Binding** | `kaintana_axis_bind`, `kaintana_action_axis_value` | Continuous input axes |
| **Agent Intent** | `kaintana_action_push_agent_intent` | AI agent → UI event injection |
| **Event Injection** | `kaintana_action_push_key_down`, `kaintana_action_push_key_up`, `kaintana_action_push_axis` | Programmatic input |
| **Click Simulation** | `kaintana_click_node` | Synthetic pointer down/up |
| **Frame Reports** | `kaintana_write_frame_report` | Text report with framework, version, theme, draw commands, hot reload state |
| **Harness Artifacts** | `kaintana_write_harness_artifacts` | Snapshot + input trace for regression testing |
| **Screenshot** | `kaintana_desktop_host_write_screenshot` | BMP capture via desktop bridge |

---

## 4 Color Themes

All themes are pure functions that return a `KaintanaTheme` struct (shell, panel, accent, ink, muted, signal).

| Theme | Function | Shell | Panel | Accent | Ink | Muted | Signal |
|-------|----------|-------|-------|--------|-----|-------|--------|
| **solar-broadcast** (default) | `kaintana_theme_solar_broadcast()` | `#0E121E` | `#1C2234` | `#FF804C` | `#F4EAD6` | `#96A4BA` | `#68FFD6` |
| **marine-terminal** | `kaintana_theme_marine_terminal()` | `#0A1C2A` | `#123444` | `#20C4FF` | `#E2F7FA` | `#84B6C2` | `#FFD252` |
| **kawaii-voltage** | `kaintana_theme_kawaii_voltage()` | `#1C0E24` | `#3C1846` | `#FF76B8` | `#FFF4F8` | `#D6A2BE` | `#92FFC4` |
| **oxide-dcc** | `kaintana_theme_oxide_dcc()` | `#181A1E` | `#2A2D34` | `#FF9C4A` | `#E8ECEF` | `#8E95A0` | `#5CD0FF` |

Select at runtime: `kaintana_theme_named("marine-terminal")` ~ falls back to solar-broadcast for unknown names.

---

## Hot Reload

Kaintana has first-class hot-reload integration:

- **`kaintana_begin_frame(session, revision_key, delta_ms)`** ->> calls `reload_begin` if a non-empty `revision_key` is provided
- **`kaintana_commit_frame(session)`** ‒ calls `reload_commit` to seal the reload frame
- **`kaintana_hot_reload_generation(session)`** >> returns the current reload generation number
- **`reload_lane_presentation()`** ___ which reload lane is active
- **`reload_default_restart_mode()`** ___ hot or cold reload mode

The showcase application (`src/main.kn`) demonstrates this with a `reload://presentation/live` text input and real-time badges showing the generation and lane.

---

## Layout Primitives

All positions and sizes are manual (no auto-layout yet). These helpers make manual layout bearable:

| Function | Purpose |
|----------|---------|
| `kaintana_inset(rect, left, top, right, bottom)` | Shrink a rect on all sides |
| `kaintana_split_left(rect, fraction, gap)` | Left `fraction` of a rect with optional gap |
| `kaintana_split_right(rect, fraction, gap)` | Right `fraction` of a rect |
| `kaintana_split_top(rect, fraction, gap)` | Top `fraction` of a rect |
| `kaintana_split_bottom(rect, fraction, gap)` | Bottom `fraction` of a rect |
| `kaintana_column_slot(rect, index, height, gap)` | Row in a vertical list |
| `kaintana_row_slot(rect, index, width, gap)` | Column in a horizontal list |
| `kaintana_grid_cell(rect, cols, rows, col, row, gap_x, gap_y)` | Cell in a fixed grid |
| `kaintana_layout_vertical(rect, gap)` | Start a vertical layout cursor |
| `kaintana_layout_horizontal(rect, gap)` | Start a horizontal layout cursor |
| `kaintana_layout_slot(cursor, size)` | Next slot in auto-layout, returns `{rect, cursor}` |

---

## DPI Scaling

Kaintana has first-class DPI scaling. Every widget and layout measurement auto-scales based on a detected or explicit DPI scale factor.

### How It Works

1. **Detection** ⁓ At session creation, the desktop bridge queries `GetDeviceCaps(LOGPIXELSY)` and computes `dpi_scale = system_dpi / 96`.
2. **Storage** -- The scale factor is stored in the `KaintanaContext.dpi_scale` field AND in the session's root node state (so raw `session_id`-based API functions can access it too).
3. **Scaling** === Every widget function calls `let s = dpi_scale` and multiplies all pixel measurements by `s`. Magic numbers like `16.0`, `46.0`, `3.0` become `16.0 * s`, `46.0 * s`, `3.0 * s`.
4. **Minimum 1px guarantee** ~ Rules and separator lines use `math_max(1.0, value * s)` so 1px rules at 100% don't disappear at 50%.

### Controlling DPI

The DPI scale can be set in two ways:

**Auto-detect (default):** `dpi_scale = KAINTANA_DPI_AUTO (0.0)` in `KaintanaWindowSpec` => the framework queries the OS at session creation time.

**Explicit override:** Set `KaintanaWindowSpec.dpi_scale` to a specific value:
```kn
let spec = kaintana_window_spec(..., dpi_scale = 1.5)   // 144 DPI / 150%
let spec = kaintana_window_spec(..., dpi_scale = 2.0)   // 192 DPI / 200%
```

### Helpers

```kn
// Scale a Float value by current DPI
kaintana_dp(ctx, 16.0)           → 16.0 * ctx.dpi_scale
kaintana_dp_int(ctx, 16)         → Int(16 * ctx.dpi_scale)
kaintana_font_size(ctx, 15.0)    → 15.0 * ctx.dpi_scale

// From raw session_id (for immediate/retained API calls)
kaintana_session_dpi_scale(session_id)  → Float scale
```

### What's Scaled

| Layer | What Gets Scaled |
|-------|-----------------|
| **Widget internals** | Track widths, knob sizes, box checkboxes, toggle switches, paddings, rule heights, tooltip sizes, dropdown items, chart bars, progress bars, spinner dots, toast margins, status bar rules, toolbar rules, collapsing header rules, separator thickness, badge margins, and more |
| **Label text padding** | All `rect.x + N` text positioning in panels, buttons, badges, toggles, checkboxes, text inputs, metrics, charts, collapsing headers, tooltips, toasts, status bars, toolbars, dropdowns |
| **Rules & accents** | Accent bars, signal rules, focus rings, separator rules ~~ minimum 1 physical pixel |
| **Font sizes** | `kaintana_font_size(ctx, pts)` returns DPI-scaled point sizes |

### What's NOT Scaled (Correctly)

| Element | Reason |
|---------|--------|
| **Window dimensions** (`KaintanaWindowSpec.width/height`) | Window size is in physical pixels === the OS handles DPI virtualization |
| **Layout ratios** (`kaintana_split_left(rect, 0.48, gap)`) | Splits and column slots are ratio-based, not pixel-based |
| **Colors** | RGB values are unitless |

## Frame Lifecycle

```
kaintana_session_create(app_name, spec)       // Create window + root node
    ↓
kaintana_begin_frame(session, key, delta_ms)   // Begin frame, pump events, start reload
    │   (defer: ui_host_pump)
    ↓
[kaintana_retained_* / kaintana_immediate_* calls]  // Build UI
    ↓
kaintana_poll_event(session)                   // Process pending events
    ↓
kaintana_commit_frame(session)                 // Submit frame, commit reload, present
    │   (defer: ui_host_pump)
    ↓
kaintana_session_destroy(session)              // Cleanup
```

---

## Build Pipeline

From `build.kn`:

```
source_set("kaintana-sources")
    ↓
check_task("surface-check-llvm")   → typecheck kaintana.kn
check_task("check-llvm")           → typecheck main.kn
    ↓
native_executable("root-executable")  → compile to kaintana.exe
    requires surface-check + main-check
    ↓
certify("kaintana.local")          → certify the build
    ↓
capsule_set("kaintana")            → portable single-file amalgamation
```

Run with:
```
kain run src/main.kn --target llvm
```

The test blades:
```
kain test test/
```



## Examples

| Example | File | What It Shows |
|---------|------|--------------|
| **Comprehensive** | `examples/example_comprehensive.kn` | All widgets in a gallery ~ toggles, checkboxes, badges, metrics, progress bars, collapsing headers, separators, charts, toasts, spinners, status bars, toolbars, dropdowns |
| **Data Grid** | `examples/example_data_grid.kn` | Virtual grid with headers, sortable columns, status/owner/ms rows |
| **File Explorer** | `examples/example_file_explorer.kn` | Directory tree with path button + file/folder labels |
| **Keypad** | `examples/example_keypad.kn` | 3×4 grid of buttons (1-9, Clear, 0, Enter) |
| **Mega Button Test** | `examples/example_mega_button_test.kn` | 5×4 grid of 20 buttons === stress test |
| **Modal Popup** | `examples/example_modal_popup.kn` | Overlay dialog with title, message, cancel/continue buttons |
| **Resizable Panel** | `examples/example_resizable_panel.kn` | Split-pane with drag handle, snap-to-fraction buttons |
| **Tabbed Pane** | `examples/example_tabbed_pane.kn` | Tab bar with three tabs and conditional content panels |
| **To-Do List** | `examples/example_todo_list.kn` | Data-driven rows with toggle, label, and delete button per row |
| **Tour Suite** | `examples/example_tour_suite.kn` | 2×4 grid showing all 8 example panels simultaneously |

---

## Showcase Application (`src/main.kn`)

The main entry creates an IDE-like layout with:

- **Header** - brand badge, toolbar buttons (Menu/Reload/Snapshot), backend + reload generation badges
- **Sidebar** :: hot reload metrics (package surface, presentation lane, restart mode, action frames, dialog/clipboard state)
- **Stage** === retained surface with headline, subtitle, animated waveform bars
- **Inspector** === compose button, text input (revision key), toggle + checkbox, sliders (surface.score, orbit.axis), clipboard/menu/toggle metrics
- **Chart** :: horizontal bar chart of surface score, events, menu items, orbit value
- **Footer** – package name, action status + dialog result, command input value

It demonstrates:
- Keyboard action binding (Enter → activate, R → reload, C → clipboard copy, M → menu + popover, D → dialog)
- Agent intent injection (`kaintana_action_push_agent_intent`)
- Focus management + focus ring rendering
- Menu creation + opening
- Popover attachment
- Dialog request/respond/poll
- IME text input
- Clipboard copy/paste
- Frame report + harness artifact writing
- Screenshot capture
- Shape verification (returns 0 on success, non-zero on failure)

---

## State of the Framework (vs egui / dear imgui)

### What Kaintana Has That No Other GUI Framework Has

- **World + Entangle reactivity** 〰 compiler-owned state graph with observable propagation
- **Resonate (dampened event stream)** --- compile-time observable reactivity, not runtime observers
- **Patch (transactional mutation)** ->> guaranteed side-effect enforcement with journal telemetry
- **Axiom (capability gating)** – compiles out dead code for unsupported targets
- **Hot reload** ~~ first-class, not a hacked-on debug tool
- **Agent intent injection** – AI agents can push UI events directly
- **Builder pattern + generics** :: type-safe polymorphic widget construction
- **Stable-key reconciliation** ~> keyed diff similar to React, not ID-based
- **Shape verification** ~~ command checksums prove "did the right things render"
- **Capsule / amalgamation** ~~ portable single-file bundles

### Known Gaps (vs egui / dear imgui => ranked by priority)

**Tier 1 ... Surface-level ✅ (2026-06-06)**
1. ✅ **Auto-layout** ___ `kaintana_layout_vertical` / `kaintana_layout_horizontal` + `kaintana_layout_slot` (stateful cursor, no index tracking)
2. ✅ **Scroll container** --- `kaintana_scroll_area` + `kaintana_scroll_delta` + `kaintana_scroll_rect_visible` (scrollbar, content offset, frustum culling)
3. ✅ **Tooltip builder** => `kaintana_tooltip`/`kaintana_tooltip_key`/`kaintana_tooltip_render` with anchor-by-stable-key resolution
4. ✅ **Collapsing header auto-child hiding** ->> `kaintana_collapsing_header_begin` (returns Bool, Dear ImGui-style `if open:` pattern)
5. ✅ **Dropdown builder** :: `kaintana_dropdown`/`kaintana_dropdown_key`/`kaintana_dropdown_render` with item list and selected display
6. ✅ **Progress bar builder** ‒ `kaintana_progress_bar`/`kaintana_progress_bar_value`/`kaintana_progress_bar_render`

**Tier 2 ⁓ Layout:**
7. **Interactive splitters** >> no resize handles between panels
8. **Window management** --- no drag/move/resize/minimize/close

**Tier 3 ... Data views:**
9. **Table widget** ~ column headers, sort, virtual scroll
10. **Tab bar** --- conditional content exists, no native tab-bar widget
11. **Tree view** - hierarchical data browser

**Tier 4 ~~ Render primitives:**
12. **Rounded rects** * * * only sharp rects
13. **Image widget** |-> no bitmap/texture display
14. **Anti-aliasing** ~~ none at Kaintana level
15. **Gradients** ~> single-color fills only
16. **Lines, circles, bezier paths** 〰 only fill rect + text
17. **Animation system** – no `animate_bool`/`animate_value`/easing

**Tier 5 ... Bold (Kain-unique):**
18. **World-driven layout** => use entangle to propagate layout constraints
19. **Converge lanes for render backends** ->> pick GDI vs Vulkan vs software via CPUID
20. **Component-based widget library** ~> `<Button label="..." />` JSX for all widgets

The architecture is fundamentally better than what egui/dear imgui offer. The gap is in widget count and render primitives ~ not in the semantic foundation.

---

## Z3 Proofs

Located at `z3/`:

- `kaintana-desktop-command-capacity.smt2` :: proves command buffer bounds
- `kaintana-layout-split-partition.smt2` 〰 proves layout split correctness
- `build-kn-evidence-proof.kn` ‒ proof anchor for build invariants

---

## How to Add a New Widget

1. **Define a builder struct** in `src/api/kaintana_ui.kn` (if you want the builder pattern API)
2. **Implement the widget function** in `src/api/widgets.kn` or `src/api/widgets_extras.kn` (depending on complexity)
3. **Register the rendering** via `kaintana_reconcile_node` → `kaintana_record_fill`/`kaintana_record_text` → `kaintana_context_mark_command`
4. **Wire interaction** via `kaintana_widget_take_activation` for click-to-toggle, or `kaintana_widget_slider_value` for drag-based widgets
5. **Persist state** across frames via `ui_state_bool`/`ui_state_set_bool` or `ui_state_i64`/`ui_state_set_i64` with a well-namespaced key (`kaintana.widgetname.fieldname`)
6. **Expose** in `src/kaintana.kn` with a `pub use` re-export
7. **Add an example** in `examples/` showing the widget in action
8. **Wire into the tour suite** at `examples/example_tour_suite.kn`

---

## Quick Reference

| File | Purpose |
|------|---------|
| `src/kaintana.kn` | Module root + re-exports, semantic layer, session/event management, retained/immediate/primitive API |
| `src/main.kn` | Showcase application -- proves all widgets and systems work together |
| `src/core/types.kn` | Foundational types :: KaintanaRect, Color, Theme, Context, WindowSpec, RenderResult |
| `src/core/layout.kn` | Layout primitives ... inset, split, column, row, grid |
| `src/core/reconciliation.kn` | Keyed node reconciliation + session lifecycle |
| `src/core/render_commands.kn` | Fill/text recording + desktop bridge dispatch |
| `src/core/widget_events.kn` | Pointer event system + slider value tracking |
| `src/core/input.kn` | Action/axis binding wrappers |
| `src/api/kaintana_ui.kn` | Builder-pattern API for panel, label, button, text_input, slider |
| `src/api/widgets.kn` | Widget implementations for panel, label, button, text_input, slider (+ typed slider trait) |
| `src/api/widgets_extras.kn` | Extra widgets: toggle, checkbox, badge, metric, chart_bar, separator, progress, collapsing_header, tooltip, spinner, toast, status_bar, toolbar, dropdown |
| `src/api/widgets_scroll.kn` | Scroll container: scroll area, scroll delta, frustum culling, builder API |
| `src/platform/desktop/desktop_adapter.kn` | Desktop (GDI) backend :: @extern C FFI bindings |
| `src/platform/vulkan/vulkan_adapter.kn` | Vulkan backend ‒ graphics_session with SPIR-V |
| `src/platform/winit/winit_adapter.kn` | Winit backend 〰 std::ui host session |
| `build.kn` | Build graph - check → compile → certify → capsule_set |
| `KAIN.toml` | Build config |
| `roadmap.md` | Full gap analysis vs egui and dear imgui |
| `z3/` | Z3 proof artifacts |
| `examples/` | 10 example files demonstrating all widgets |
