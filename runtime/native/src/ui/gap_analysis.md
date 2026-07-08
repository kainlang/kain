# Kain UI System — Complete Gap Analysis

> **Author:** BIG PICKLE #2  
> **Date:** 2026-06-25  
> **Scope:** Full audit of all docs (`UI.MD`), C runtime README, KUIF manifesto (`widgets/README.md`), and stdlib (`stdlib/ui/`, `stdlib/ui.kn`, `stdlib/input.kn`)  
> **Method:** Systematic comparison against SwiftUI-tier UI framework requirements

---

## How to Read This Document

Each gap is rated by severity:

| Rating | Meaning |
|--------|---------|
| 🔴 **CRITICAL** | Shipping without this makes the framework non-viable for production apps. Users will pick another framework. |
| 🟡 **IMPORTANT** | Major ergonomic or capability gap. Users will feel its absence daily. Should ship before public launch. |
| 🟢 **NICE** | Important for a polished, mature framework. Safe to defer to post-v1. |

---

## 1. 🏆 What Already EXISTS (Don't Re-Design)

These are real, working pieces. Don't throw them away:

- ✅ **Retained-mode node tree** (`ui_system.c`, ~2,600 lines) — session lifecycle, node CRUD, style/state, events, focus, hit-testing. Z3-proven.
- ✅ **8 immediate-mode widgets** (`ui_widget.c`, ~1,200 lines) — button, label, checkbox, slider, textbox, panel, progress, window.
- ✅ **Font rasterization** (`stb_truetype.h`, 21 Z3 proof packs) — glyph loading, text measurement, vertical metrics.
- ✅ **Software framebuffer** (`ui_renderer.c`, ~350 lines) — clear, fill rect, rounded rect, glyph text.
- ✅ **DPI scaling** (per-monitor v2, WM_DPICHANGED, logical→physical pixel mapping).
- ✅ **Clipboard ABI** — `abi_ui_clipboard_set_text` / `abi_ui_clipboard_text`.
- ✅ **IME ABI** — `abi_ui_ime_begin/commit_text/end/active_node/text` (state tracked, but **not rendered**).
- ✅ **Drag-drop ABI** — `abi_ui_drag_begin/update/drop` (state tracked, but **not rendered**).
- ✅ **Menu ABI** — `abi_ui_menu_create/add_item/open` (state tracked, but **not rendered**).
- ✅ **Dialog ABI** — `abi_ui_dialog_request/respond/poll_response` (state tracked, but **not rendered**).
- ✅ **Accessibility ABI** — `abi_ui_accessibility_set_role/set_label/role/label (minimal, no platform integration).
- ✅ **Hot-reload** (`ui_hot_reload.c`, ~650 lines) — shared-memory IPC for live bundle swapping.
- ✅ **KainComponentSurface vtable** — 19-slot rendering backend trait, compiler-agnostic.
- ✅ **Vulkan ABI loader** (`vulkan_abi.c`, ~2,050 lines) — 43 PFNs, dlopen-based, no SDK dependency.
- ✅ **Graphics bundle system** — JSON sidecar for GPU pipeline descriptors.
- ✅ **Input system** (`std::input`) — universal input abstraction, action bindings, axes, text commits.
- ✅ **Color helpers** (`ui_color.c`, `std::ui::style`) — alpha blending (Z3-proven `div255_fast`), lerp, component extraction.
- ✅ **Layout helpers** (`ui_layout.c`, ~220 lines) — basic flexbox row/column.
- ✅ **Kain stdlib wrappers** (`stdlib/ui.kn`, `stdlib/ui/`) — 5 files, ~1,677+ lines of Kain wrapping C ABI.
- ✅ **3 C demos** (cosmic dashboard, retro wave, 3D sandbox) — pushing the renderer to its limits.
- ✅ **9 C tests** — calculator, keypad, particle system, widget demo, etc.

---

## 2. 🔴 CRITICAL GAPS — Must Have for v1

### 2.1 Accessibility (a11y) — Platform Integration

**Current state:** ABI has `set_role(role_string)` and `set_label(label_string)` on nodes. That's it. No accessibility tree is ever exposed to the OS.

**What's missing:**

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Screen reader integration** | No UIA (Windows), AT-SPI (Linux), or AX (macOS) bridge. Blind users cannot use Kain apps. | 🔴 |
| **Accessibility tree** | No hierarchical accessibility tree exposed to platform APIs. The node tree exists but its a11y mirror does not. | 🔴 |
| **Focus navigation** | Tab-key cycling between controls, arrow keys in groups, Enter/Space to activate. Currently focus tracking exists but no keyboard navigation. | 🔴 |
| **Focus ring** | No visual focus indicator (dashed outline, highlight). Focused nodes are invisible to sighted keyboard users. | 🔴 |
| **Keyboard shortcuts / accelerators** | No system for menu keyboard shortcuts (Ctrl+C, Ctrl+V, Alt+F4) or mnemonics (Alt+F → File menu). | 🟡 |
| **High contrast detection** | No OS-level high-contrast mode detection or automatic theme switching. | 🟡 |
| **Reduce motion detection** | No respect for OS "reduce motion" accessibility setting. | 🟢 |
| **VoiceOver / TalkBack hints** | No accessibility hints, traits, or custom actions. | 🟡 |
| **`accessibility.kn` module** | Listed in KUIF roadmap (Phase 8). Doesn't exist yet. | 🔴 |

**What it takes:** A new `ui_accessibility.c` file (~1,500 lines) implementing the platform a11y bridge for each OS. The C runtime already has `KainComponentSurface` — a similar pattern for accessibility backends would work. On Windows, this means `UIAutomation` COM interface implementation. On Linux, AT-SPI D-Bus protocol. On macOS, NSAccessibility protocol.

---

### 2.2 Clipping / Scrolling / Overflow

**Current state:** The renderer draws all nodes regardless of parent bounds. `RENDERER_ASSESSMENT.md` explicitly calls this out as missing.

**What's missing:**

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Clip rect / scissor test** | No way to constrain rendering to a parent's bounds. Content overflows containers visually. | 🔴 |
| **Scrollable container** | No ScrollView component. Content that exceeds viewport is invisible/unreachable. | 🔴 |
| **Scrollbar widget** | No scrollbar to indicate position or allow drag-scrolling. | 🔴 |
| **Momentum scrolling** | No physics-based scroll (inertia, bounce, overscroll). | 🟡 |
| **Scroll indicators** | No visual flash-scroll-indicators on content overflow. | 🟢 |
| **`scroll.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🔴 |

**What it takes:** Clip rect stack in `ui_renderer.c` (~100 lines), a `ScrollView` component in Kain (~300 lines), and scrollbar widget (~200 lines). The `ui_system.c` already has `push_clip/pop_clip` functions conceptually proposed in the KUIF substrate API.

---

### 2.3 Text Editing — Multi-Line, Selection, Undo

**Current state:** One single-line textbox widget. Cursor exists but no selection. No IME composition rendering. No undo.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Multi-line text** | No `TextEditor` / text area widget. Cannot edit paragraphs. | 🔴 |
| **Text selection** | No selection highlight, no drag handles, no Shift+arrow selection. | 🔴 |
| **Clipboard operations** | Ctrl+C/V/X not wired through text widgets. ABI exists but no UI integration. | 🔴 |
| **Undo/redo stack** | No undo history for text edits. One mistake and the entire field is lost. | 🔴 |
| **IME composition rendering** | IME state is tracked (`abi_ui_ime_*`) but the composition underline/preedit string is never rendered. Chinese/Japanese/Korean IME is broken. | 🔴 |
| **Spellcheck / autocorrect** | No platform spellcheck integration (OS spellcheck APIs). | 🟢 |
| **Auto-capitalization** | No sentence/word auto-capitalization for text fields. | 🟢 |
| **`textbox.kn` as Kain component** | Currently a thin C wrapper. Doesn't exist as a proper Kain component. | 🟡 |

**What it takes:** A text editing engine in C (~800 lines) with gap buffer or piece table, selection rendering, IME composition overlay, undo stack, and clipboard integration. The current `textbox` widget would need a full rewrite.

---

### 2.4 Multi-Touch / Gesture Input

**Current state:** Win32 mouse click + keyboard polling (`GetAsyncKeyState`). No touch at all.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Multi-touch** | No touch events, no pinch/zoom, no pan/swipe, no rotate, no long-press. Windows touch apps need `WM_TOUCH` / `WM_POINTER`. | 🔴 |
| **Gesture recognizer system** | No unified gesture detection (tap, double-tap, swipe, pinch, pan, rotate, long-press). Each widget must reimplement hit-test math. | 🔴 |
| **Pen / stylus** | No `WM_POINTER` handling for pen input. No pressure/tilt/barrel-button. | 🟡 |
| **Game controller** | No gamepad/controller input for UI navigation (DPad, A/B, joystick). | 🟢 |
| **Voice input** | No speech-to-text or voice command integration. | 🟢 |
| **Eye tracking / switch access** | No OS-level accessibility switch/eye-gaze input support. | 🟢 |
| **Haptic feedback** | No haptic feedback API for touch interactions (button press → haptic click). | 🟢 |
| **Force touch** | No pressure-sensitive click handling (3D Touch, right-click pressure). | 🟢 |

**What it takes:** Extend `ui_host_adapter.c` to handle `WM_TOUCH`/`WM_POINTER` (~300 lines). Add a gesture recognizer subsystem to `ui_system.c` (~600 lines). Add touch event types to the event ring buffer. Extend `std::input` with touch event source.

---

### 2.5 Portability — Only Win32 Exists

**Current state:** `ui_host_adapter.c` is pure Win32 GDI. The KUIF README lists stubs for X11, Wayland, macOS, WASM — none exist.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Linux X11 backend** | No X11 window creation, no Xlib message pump, no DIB equivalent. | 🔴 |
| **Linux Wayland backend** | No Wayland surface, no wl_display integration. | 🔴 |
| **macOS backend** | No Cocoa/CAMetalLayer window, no NSApplication run loop. | 🔴 |
| **WebAssembly backend** | No HTML Canvas or WebGPU surface. No browser event loop. | 🔴 |
| **Mobile (Android/iOS)** | No touch-first mobile UI backend at all. | 🔴 |
| **CI cross-platform build** | No CI matrix for Linux/macOS/WASM builds of the UI runtime. | 🔴 |
| **Cross-platform abstractions** | `KainPlatformSurfaceHandle` exists but only Win32 fills it. | 🟡 |

**What it takes:** 4 platform backends, each ~500-800 lines of C implementing `KaifHostInterface`. The KAIF manifesto provides the vtable contract (`kaif_host_win32`, `kaif_host_x11`, etc.). This is the single largest engineering effort outside the compiler.

---

### 2.6 Navigation System

**Current state:** None. Zero navigation primitives exist.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **NavigationStack** | No push/pop navigation. No "back" button, no navigation bar with title. | 🔴 |
| **NavigationSplitView** | No sidebar + detail + inspector split with collapse/expand. | 🔴 |
| **Tab navigation** | No tab bar / segmented control for top-level sections. | 🔴 |
| **Wizard / Stepper** | No multi-step form wizard with forward/back. | 🟡 |
| **Deep linking / URL routing** | No URL-based navigation state. No deeplink handling. | 🟢 |
| **`navigation.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🔴 |

**What it takes:** A navigation controller in Kain (~500 lines) managing a stack of views, with transition animations. This is fundamentally a Kain component library concern, not C.

---

### 2.7 Digital Ink / Rich Widget Set

**Current state:** 8 widgets. That's it.

| Missing Widget | Detail | Rating |
|----------------|--------|--------|
| **List / TableView** | Virtualized, reusable cells for large data sets. Essential for any data app. | 🔴 |
| **Picker / Dropdown / ComboBox** | Selecting from a list of options. No select/option widget exists. | 🔴 |
| **DatePicker / Calendar** | Date and time selection UI. Essential for forms. | 🔴 |
| **SearchBar** | Search field with suggestions, clear button, scope bar. | 🔴 |
| **FilePicker** | System open/save dialog integration. | 🔴 |
| **SplitView** | Resizable panes with drag handle. | 🟡 |
| **Toolbar** | System-level bar with action items. | 🟡 |
| **Stepper / SegmentedControl** | Increment/decrement value, switch between segments. | 🟡 |
| **ColorPicker** | Color selection UI (hue wheel, sliders, hex input). | 🟡 |
| **TreeView** | Hierarchical data with expand/collapse. | 🟡 |
| **CollectionView / GridView** | Multi-column layout of reusable cells. | 🟡 |
| **Rating control** | Star-based rating input. | 🟢 |
| **Carousel / PageView** | Horizontal paged scrolling of views. | 🟢 |
| **Gauge / Knob** | Circular gauge / rotary knob input. | 🟢 |
| **Timeline / Gantt** | Time-based horizontal strips. | 🟢 |
| **Code editor** | Syntax-highlighted text input. | 🟢 |

**What it takes:** 15+ Kain component files in `stdlib/ui/components/`. Each is 50-200 lines of Kain code using the primitive rendering API. This is the bulk of Phase 4 in the KUIF roadmap.

---

## 3. 🟡 IMPORTANT GAPS — Should Have for v2

### 3.1 Modals / Sheets / Overlays

**Current state:** Dialog ABI exists but is **never rendered**. No other modal system.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Alert dialog** | System modal with title, message, buttons. ABI exists but C code doesn't render it. | 🟡 |
| **Sheet / Popover** | Slide-up sheet (bottom) or anchored popover. | 🟡 |
| **Toast / Snackbar** | Temporary notification bar at top or bottom. | 🟡 |
| **Confirmation dialog** | "Are you sure?" with Cancel/Destructive/Confirm. | 🟡 |
| **Action sheet** | List of choices presented as an overlay. | 🟢 |
| **Modal presentation** | View presented modally on top of current context. | 🟡 |
| **`alert.kn`, `dialog.kn`, `sheet.kn`** | Listed in KUIF roadmap. Don't exist. | 🟡 |

**What it takes:** Dialog rendering in `ui_renderer.c` (~200 lines) plus Kain component wrappers (~100 lines each). The dialog ABI skeleton exists — it just needs visual rendering.

### 3.2 Theme / Styling System

**Current state:** 19 `#define` color constants and 11 `#define` size constants in C code. No runtime theme switching. No dark/light mode.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Runtime theme switching** | Switch colors/sizes at runtime without recompile. | 🟡 |
| **Dark / Light mode** | Detect OS preference, auto-switch palette. | 🟡 |
| **High contrast mode** | Detect OS accessibility high-contrast setting, switch to high-contrast palette. | 🟡 |
| **Cascading styles** | Parent styles inherited by children (font, color, spacing). Partial via node style system but not ergonomic. | 🟡 |
| **Custom component styling API** | Override default colors/sizes per component instance. | 🟡 |
| **Dynamic type / font scaling** | Respect OS font size setting (large text accessibility). | 🟡 |
| **`theme.kn` module** | Listed in KUIF roadmap. Doesn't exist. | 🟡 |

**What it takes:** A `theme.kn` Kain module (~200 lines) defining theme as data (structs, not #defines), a C theme struct (~100 lines), and a theme resolution pass in `ui_system.c` (~150 lines).

### 3.3 Animation System

**Current state:** No animation primitives. Frame timing (`delta_ms`) is available but no easing, no transitions, no interpolations.

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Easing curves** | easeInOut, easeIn, easeOut, spring, bounce, elastic. | 🟡 |
| **Property animations** | Animate position, size, color, opacity over time. | 🟡 |
| **Transition animations** | Animate view insertion/removal (fade, slide, scale). | 🟡 |
| **Spring physics** | Mass-spring-damper system for natural motion. | 🟡 |
| **Keyframe animations** | Multi-step animation with timing control. | 🟢 |
| **Layout animations** | Animate layout changes (HStack items reflow smoothly). | 🟢 |
| **`animation.kn` module** | Listed in KUIF roadmap. Doesn't exist. | 🟡 |
| **`pulse` clock system** | Compiler-owned animation timer. Referenced in KUIF but not implemented. | 🟡 |

**What it takes:** A `pulse` timer system in the compiler/c runtime (~300 lines), an easing functions library in Kain (pure math, ~100 lines), and interpolation helpers. The `ui_renderer.c` already renders every frame — animation is just delta-driven value interpolation.

### 3.4 Canvas / Custom Rendering API

**Current state:** No Path API, no gradient rendering (only solid fill rects and rounded rects), no shadow, no transform.

| Sub-gap | Detail | Rating |
|--------|--------|--------|
| **Gradient rendering** | No linear, radial, or conical gradients. | 🟡 |
| **Path API** | No bezier curves, arcs, or custom shapes. | 🟡 |
| **Drop shadow** | No shadow rendering (box shadow, text shadow). | 🟡 |
| **Blur / Backdrop blur** | No gaussian blur effect. No frosted-glass/vibrancy. | 🟡 |
| **Transform / Rotation** | No affine transform matrix. No rotation, scale, skew of nodes. | 🟡 |
| **Opacity / Alpha** | Opacity exists per-style-key but no inherited opacity. | 🟡 |
| **`canvas.kn`, `path.kn`** | Listed in KUIF roadmap. Don't exist. | 🟡 |

**What it takes:** Expand `ui_renderer.c` with gradient fill (~200 lines), path rasterizer (~400 lines, or integrate nanovg/nanosvg), blur kernel (~150 lines), and transform matrix application (~100 lines). The software renderer will become slow — this is where GPU acceleration becomes critical.

### 3.5 Image Loading and Display

**Current state:** Texture resource system exists (create, set_bytes_hex) but no image decoder. The `draw_resource` command type is **ignored** by the renderer.

| Sub-gap | Detail | Rating |
|--------|--------|--------|
| **Image decoder** | No PNG, JPEG, GIF, WebP, BMP decoding. | 🟡 |
| **SVG rendering** | No vector graphics rendering. | 🟢 |
| **Image view widget** | No `<Image>` component to display images. `image.kn` listed but doesn't exist. | 🟡 |
| **Async image loading** | No background thread for image decode, no placeholder/loading state. | 🟡 |
| **Image cache** | No LRU/MRU image cache for scrolling lists. | 🟢 |
| **`draw_resource` rendering** | The ABI command exists but `ui_render_frame()` ignores `DRAW_COMMAND_RESOURCE`. | 🟡 |

**What it takes:** Integrate stb_image.h (single-header, public domain) for PNG/JPEG decoding (~50 lines), implement `draw_resource` in the render loop (~50 lines), write an `<Image>` component in Kain (~80 lines), and add caching.

### 3.6 Internationalization (i18n)

**Current state:** Basic unicode support in `std::unicode` module. No text direction, no locale formatting, no RTL.

| Sub-gap | Detail | Rating |
|--------|--------|--------|
| **RTL layout** | Right-to-left layout mirroring for Arabic, Hebrew, Persian, Urdu. | 🟡 |
| **Unicode BiDi** | Bidirectional text rendering (Arabic + English in same line). Requires Unicode BiDi algorithm. | 🟡 |
| **Locale-aware formatting** | Number formatting (1,234.56 vs 1.234,56), date formatting (MM/DD vs DD/MM), currency. | 🟡 |
| **String table / localization** | `.strings` / `.po` / `.json` localization file loading. | 🟡 |
| **Text direction API** | `.environment(\.layoutDirection, .rightToLeft)` equivalent. | 🟡 |
| **ICU / libunibreak** | Word wrapping, grapheme clusters, line breaking rules per locale. | 🟢 |

**What it takes:** Unicode BiDi algorithm in Kain/C (~800 lines, or integrate fribidi). RTL layout flip in `ui_layout.c` (~100 lines). Locale formatting in `std::locale` module (~300 lines). String table loader (~150 lines).

### 3.7 Responsive / Adaptive Layout

**Current state:** Fixed-size row/column layout. No size classes, no breakpoints, no orientation handling.

| Sub-gap | Detail | Rating |
|--------|--------|--------|
| **Size classes** | No compact/regular width/height classification. | 🟡 |
| **Orientation handling** | No portrait/landscape detection or layout adaptation. | 🟡 |
| **Safe areas** | No safe area insets (notch, home indicator, status bar). | 🟡 |
| **Layout margins** | No system layout margins (readable content width). | 🟡 |
| **Dynamic layout switching** | No automatic switching between HStack and VStack based on available space. | 🟢 |
| **`hstack.kn`, `vstack.kn`, `zstack.kn`, `grid.kn`, `spacer.kn`** | Listed in KUIF roadmap. Don't exist. | 🟡 |

**What it takes:** The layout component files in `stdlib/ui/layout/` (~600 lines total for all 6 components) plus safe area tracking in `ui_host_adapter.c` (~100 lines) and orientation change handling in `ui_system.c` (~100 lines).

### 3.8 Modal / Overlay Windowing

| Sub-gap | Detail | Rating |
|--------|--------|--------|
| **`toolbar.kn`, `statusbar.kn`** | Listed in KUIF roadmap. Don't exist. | 🟡 |
| **`menu.kn` component** | Listed in KUIF roadmap. Doesn't exist. Current menu ABI is C-only and unrendered. | 🟡 |
| **`tooltip.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🟡 |
| **`badge.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🟢 |
| **`divider.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🟢 |
| **`tabs.kn` component** | Listed in KUIF roadmap. Doesn't exist. | 🟡 |

---

## 4. 🟢 NICE-TO-HAVE GAPS — Post-v1 Polish

### 4.1 Application Lifecycle

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **State restoration** | Save/restore UI state across app relaunches (window position, navigation stack). | 🟢 |
| **Memory pressure handling** | Respond to OS memory warnings, purge caches. | 🟢 |
| **Foreground/background** | Detect app foreground/background transitions, pause/resume animations. | 🟢 |
| **Window management** | Multi-window support, window tabs, window groups. | 🟢 |
| **Full-screen / Split-screen** | Full-screen mode, iPadOS split-view, macOS full-screen spaces. | 🟢 |

### 4.2 Testing and Debugging

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **UI testing framework** | Programmatic UI automation (find button, tap, assert). | 🟢 |
| **Screenshot testing** | Render-to-image comparison for visual regression. | 🟢 |
| **Accessibility inspector** | Debug tool showing a11y tree, roles, labels, focus order. | 🟢 |
| **Performance profiler** | Per-frame timing (layout, render, composite), draw call count, node count. | 🟢 |
| **UI debugger** | Visual tree inspector (like Safari Web Inspector, Xcode View Debugger). | 🟢 |
| **Overdraw visualization** | Show overdraw regions for performance optimization. | 🟢 |
| **`ui_render_frame` unit test** | RENDERER_ASSESSMENT.md notes no test calls `ui_render_frame` successfully. | 🟡 |

### 4.3 Web / Media / Printing

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **WebView** | Embed web content (Chromium Embedded, WebView2, WebKit). | 🟢 |
| **Video playback** | Video player widget with transport controls. | 🟢 |
| **Audio visualization** | Waveform display, spectrogram. | 🟢 |
| **Printing** | Print UI, page layout, PDF generation. | 🟢 |
| **Document-based apps** | Document browser, recent files, iCloud-like sync, UIDocument equivalent. | 🟢 |
| **SVG rendering** | Resolution-independent vector graphics. | 🟢 |

### 4.4 System Integration

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Desktop widgets** | Widget extensions for Windows Dashboard/Sidebar, macOS Today view, Linux GNOME extensions. | 🟢 |
| **Share extensions** | System share sheet integration. | 🟢 |
| **Spotlight / Search** | System search integration (Windows Search, macOS Spotlight). | 🟢 |
| **Drag-and-drop (cross-app)** | OLE drag-and-drop between Kain apps and other Windows apps. | 🟢 |
| **Sandboxing / Security-scoped** | File system sandbox, security-scoped bookmarks, permission dialogs. | 🟢 |
| **URL scheme handling** | Register custom URL schemes (`myapp://open?item=42`). | 🟢 |
| **Notifications** | System notification center integration (toast notifications). | 🟢 |

### 4.5 Help System

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Context-sensitive help** | F1 on a control shows its help topic. | 🟢 |
| **Searchable help index** | Full-text search over help content. | 🟢 |
| **Tooltip delay / positioning** | Configurable tooltip show delay, preferred edge, max width. | 🟢 |

### 4.6 Performance

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **GPU-accelerated rendering** | Vulkan/D3D12/Metal backend for UI rendering (not just compute shaders). | 🟡 |
| **Damage region tracking** | Only redraw changed regions (KUIF compositor concept). | 🟡 |
| **Layer compositing** | Composite offscreen layer bitmaps for complex sub-trees. | 🟢 |
| **Async texture loading** | Decode textures on background threads, upload to GPU without stalling. | 🟢 |
| **Virtualized lists** | Only create nodes for visible items. Recycle offscreen nodes. | 🟡 |
| **Frame pacing / vsync** | Synchronize presentation with display refresh for tear-free rendering. | 🟡 |

### 4.7 Cross-Process / Distributed UI

| Sub-gap | Detail | Rating |
|---------|--------|--------|
| **Cross-process UI (`teleport`)** | Render Kain components across process boundaries. | 🟢 |
| **Distributed UI** | Render component tree across multiple displays on multiple machines. | 🟢 |
| **Time-travel debugging** | Rewind and replay state changes via patch journal. | 🟢 |
| **Multi-language UI (`orchestrate`)** | Call Python for data processing, Rust for rendering in same component tree. | 🟢 |

---

## 5. 📋 KUIF Roadmap: Items Listed But NOT Yet Created

The KUIF manifesto (`widgets/README.md`) lists an ambitious directory of components. Here's the gap between what's listed and what exists:

### `stdlib/ui/components/` — 26 listed, 0 exist

| File | Listed? | Exists? | Gap |
|------|---------|---------|-----|
| `button.kn` | ✅ | ❌ | 🔴 |
| `label.kn` | ✅ | ❌ | 🔴 |
| `checkbox.kn` | ✅ | ❌ | 🔴 |
| `slider.kn` | ✅ | ❌ | 🔴 |
| `textbox.kn` | ✅ | ❌ | 🔴 |
| `toggle.kn` | ✅ | ❌ | 🟡 |
| `picker.kn` | ✅ | ❌ | 🔴 |
| `stepper.kn` | ✅ | ❌ | 🟡 |
| `progress.kn` | ✅ | ❌ | 🟡 |
| `spinner.kn` | ✅ | ❌ | 🟡 |
| `icon.kn` | ✅ | ❌ | 🟡 |
| `image.kn` | ✅ | ❌ | 🟡 |
| `canvas.kn` | ✅ | ❌ | 🟡 |
| `scroll.kn` | ✅ | ❌ | 🔴 |
| `list.kn` | ✅ | ❌ | 🔴 |
| `tree.kn` | ✅ | ❌ | 🟡 |
| `tabs.kn` | ✅ | ❌ | 🟡 |
| `menu.kn` | ✅ | ❌ | 🟡 |
| `divider.kn` | ✅ | ❌ | 🟢 |
| `badge.kn` | ✅ | ❌ | 🟢 |
| `tooltip.kn` | ✅ | ❌ | 🟡 |
| `alert.kn` | ✅ | ❌ | 🟡 |
| `dialog.kn` | ✅ | ❌ | 🟡 |
| `sheet.kn` | ✅ | ❌ | 🟡 |
| `navigation.kn` | ✅ | ❌ | 🔴 |
| `toolbar.kn` | ✅ | ❌ | 🟡 |
| `statusbar.kn` | ✅ | ❌ | 🟡 |

### `stdlib/ui/layout/` — 6 listed, 0 exist

| File | Exists? | Gap |
|------|---------|-----|
| `hstack.kn` | ❌ | 🟡 |
| `vstack.kn` | ❌ | 🟡 |
| `zstack.kn` | ❌ | 🟡 |
| `grid.kn` | ❌ | 🟡 |
| `spacer.kn` | ❌ | 🟡 |
| `padding.kn` | ❌ | 🟢 |

### `stdlib/ui/primitives/` — 7 listed, 0 exist

| File | Exists? | Gap |
|------|---------|-----|
| `rect.kn` | ❌ | 🟡 |
| `circle.kn` | ❌ | 🟡 |
| `ellipse.kn` | ❌ | 🟢 |
| `line.kn` | ❌ | 🟢 |
| `path.kn` | ❌ | 🟡 |
| `text.kn` | ❌ | 🟡 |
| `interactive.kn` | ❌ | 🟡 |

### `stdlib/ui/` — 4 listed, 0 exist

| File | Exists? | Gap |
|------|---------|-----|
| `theme.kn` | ❌ | 🟡 |
| `animation.kn` | ❌ | 🟡 |
| `accessibility.kn` | ❌ | 🔴 |
| `preview.kn` | ❌ | 🟢 |

---

## 6. 🔧 Infrastructure & Non-Feature Gaps

### 6.1 Build System Gaps

| Gap | Detail | Rating |
|-----|--------|--------|
| **No CI for UI tests** | All C tests must be compiled manually. No automated regression. | 🔴 |
| **No unit test for `ui_render_frame`** | The core render function has zero test coverage. | 🔴 |
| **Tests duplicate Win32 struct definitions** | 3 files redefine `KainWin32UiHost` (DRY violation). | 🟡 |
| **Window subclassing required** | All tests must subclass WM_PAINT to work around host adapter bug. | 🟡 |
| **Stubs needed for tests** | `string_new`, `kain_clampd` must be provided externally. | 🟢 |

### 6.2 Z3 Proof Gaps

| Missing Proof | Layer | Risk |
|---------------|-------|------|
| Sibling-linked list correctness under node destroy + slot reuse | L1 | Medium |
| Draw command count bounds | L1 | Low (8K is generous) |
| Style/state hash table probe bound under node destroy | L1 | Medium |
| Heap-free invariant for per-frame arena strings | L1 | Medium |
| Clip rect stack depth | L0 | Low |
| Transform stack depth | L0 | Low |
| Glyph cache bounds | L0 | Low |
| Input event queue overflow | L0 | Low |
| Text selection buffer bounds | L1 | Low (current textbox is tiny) |

### 6.3 ABI Functions with No Rendering Backing

These ABI functions are implemented in `ui_system.c` (state tracking) but **never visually rendered**:

| ABI | Status | Gap |
|-----|--------|-----|
| `abi_ui_draw_resource` | State tracked, command type **ignored** by renderer | 🟡 |
| `abi_ui_drag_begin/update/drop` | State tracked, no visual feedback | 🟡 |
| `abi_ui_menu_*` | State tracked, never rendered | 🟡 |
| `abi_ui_dialog_*` | State tracked, never rendered | 🟡 |
| `abi_ui_ime_*` | State tracked, composition never rendered | 🔴 |
| `abi_ui_accessibility_*` | Role/label stored, never exposed to OS | 🔴 |
| `abi_ui_texture_*` | Texture data stored, never rendered | 🟡 |

---

## 7. ⚡ Summary: Top 10 Things to Build First

If you only had time to fix 10 things before calling KUIF "v1 ready", in priority order:

1. **🔴 Clip rect / scissor stack** — Without this, everything overflows its parent. Everything looks broken.
2. **🔴 ScrollView** — Without scrolling, any app with more content than a postage stamp is unusable.
3. **🔴 Accessibility platform bridge** — Without screen reader support, Kain apps are illegal for government procurement and inaccessible to blind users.
4. **🔴 Multi-line text editing + IME rendering** — Without this, Kain apps cannot be used in CJK markets or for any serious text entry.
5. **🔴 Focus ring + keyboard navigation** — Without this, keyboard-only users cannot operate Kain apps. Also a legal requirement in many jurisdictions.
6. **🔴 Touch / gesture input** — Without touch, Kain apps cannot run on tablets, phones, or touch-screen laptops (>50% of Windows market).
7. **🔴 List/TableView with virtualization** — Without this, any data-driven app (settings, files, contacts, messages) is impossible.
8. **🔴 Linux/Wayland backend** — Without cross-platform support, Kain is a Windows-only curiosity, not a cross-platform UI framework.
9. **🟡 Dark/light mode + theme system** — Without this, Kain apps look wrong on dark-mode systems and users will complain.
10. **🟡 Navigation stack + tab bar** — Without navigation, apps are single-screen demos, not real applications.

---

## 8. 📐 Architecture Recommendation

**The KUIF manifesto is directionally correct.** The core insight — move widget logic from C to Kain components, make the C layer a thin render substrate — is the right long-term architecture.

**However, the manifesto's 10-phase plan is too slow for the critical gaps.** You cannot wait until Phase 6-8 (months 8-10) to ship accessibility, scrolling, and multi-line text. These must happen in parallel with Phase 1-2.

**Recommended parallel work streams:**

| Stream | Focus | Timeline | Depends On |
|--------|-------|----------|------------|
| **A** | C substrate extraction + clip rect + scrolling | Month 1-2 | Nothing |
| **B** | Accessibility platform bridge (UIA first) | Month 1-3 | Stream A (for clip rect) |
| **C** | Text editing rewrite (multi-line, IME, selection) | Month 2-4 | Stream A |
| **D** | Touch/gesture input pipeline | Month 2-4 | Stream A |
| **E** | Portability (Linux backend) | Month 3-6 | Stream A |
| **F** | Kain component library (all widgets as `.kn`) | Month 3-8 | Stream A + compiler component support |
| **G** | Theme system + dark mode | Month 4-6 | Stream F |
| **H** | GPU-accelerated rendering backend | Month 6-12 | Stream A |

**Total estimated engineering effort to reach "v1": ~18-24 engineer-months** (assuming compiler team supports component desugaring in parallel).

---

*This gap analysis was compiled from the following source documents:*
- *`X:/docs/UI.MD` (1,448 lines)*
- *`X:/runtime/native/src/ui/README.md` (full)*
- *`X:/runtime/native/src/ui/widgets/README.md` (1,401 lines — KUIF manifesto)*
- *`X:/runtime/native/src/ui/RENDERER_ASSESSMENT.md`*
- *`X:/runtime/native/src/ui/README_ANALYSIS.md`*
- *`X:/stdlib/ui.kn` (1,677 lines)*
- *`X:/stdlib/ui/widget.kn` (8.8 KB)*
- *`X:/stdlib/ui/component.kn` (7.1 KB)*
- *`X:/stdlib/ui/style.kn` (6.0 KB)*
- *`X:/stdlib/ui/font.kn` (11.3 KB)*
- *`X:/stdlib/input.kn`*
- *`X:/stdlib/no_std.kn`*
