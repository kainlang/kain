# Kain Native UI Runtime — Ground-Truth Inventory

**Date:** 2026-06-21
**Status:** BRUTALLY HONEST. No aspirations. Code on disk only.
**Total source:** 10,254 lines across 17 files (8 C sources, 5 C headers, 1 Kain stdlib, 3 docs)

______________________________________________________________________

## 1. Executive Summary

The Kain native UI runtime is **two separate, largely disconnected systems** that share terminology but not data flow:

### System A: Retained-Mode UI Engine (`ui_system.c` + `ui_host_adapter.c`)

A **fully operational retained-mode UI data store** with 112 public C functions for managing sessions, nodes, styles, state, draw commands, events, resources (fonts/textures/canvases/shaders), menus, dialogs, drag-and-drop, IME, accessibility, clipboard, and hot-reload. It stores everything in flat arrays with open-addressing hash indices. It can record draw commands (rect, text, resource) with full metadata. **However, it has NO renderer that reads the node tree or draw commands and turns them into pixels.** The one actual pixel-producing path (`win32_host_render_framebuffer`) ignores the entire node tree and draws a hardcoded gradient with hardcoded text.

### System B: Compiled Bundle System (`ui_runtime.c` + `ui_compiled_bundle.c` + `ui_hot_reload.c`)

A **compile-time node manifest system** that parses JSON "compiled bundles" (hand-written JSON parser ~800 lines), validates them, builds component state records with capability flags (focusable, editable), and supports hot-reload via shared-memory IPC channels. **It does not connect to System A's node tree, styles, or draw commands at all.** It maintains its own parallel data structures (`KainUiRuntimeState`, `KainUiRuntimeComponentState`, `KainUiCompiledBundle`).

### System C: Component Surface Bridge (`native_ui_surface.c`)

The **17-slot vtable** that connects the Rust codegen's `KainComponentSurface*` trait calls to System A's `abi_ui_*` functions. This is the bridge that makes `component` + `world` + `surface native_ui => Component` actually create windows. **It is thin wrappers — it delegates to System A for everything and System A delegates to the host adapter for window creation.**

### What Actually Works End-to-End

1. **Window creation** — `world` + `surface native_ui => Component` creates a real Win32 HWND with a GDI framebuffer. Oracle-verified.
1. **Node tree mutation** — The component surface vtable stores elements, styles, text, rects, stable keys in System A's data structures.
1. **State persistence** — Component `state` fields survive across frames via a hidden `__kain_state_root` node.
1. **Hardcoded rendering** — The GDI host adapter paints a gradient + text, proving the window is alive. But it renders NOTHING from the node tree.

### What Does NOT Work

1. **No renderer reads the node tree** — `win32_host_render_framebuffer` ignores session->nodes, session->styles, session->draw_commands. Every pixel is hardcoded.
1. **Style attributes are write-only** — `fill_color`, `border_color`, `ink_color`, `title` are stored via `abi_ui_node_set_style_string` but never consumed by any renderer.
1. **System B has no renderer at all** — The compiled bundle system has focus routing, editable controls, event routing, but zero pixel output.
1. **Draw commands are never consumed** — `abi_ui_draw_rect` / `abi_ui_draw_text` produce `KainNativeUiDrawCommand` records. Nothing reads them to produce pixels.
1. **Most `native_ui_set_attr_string` keys are silently dropped** — Only 4 keys (`fill_color`, `border_color`, `ink_color`, `title`) are forwarded to style storage. All others (including `background`, `color`, `border`, etc.) are silently ignored at the C surface layer.

______________________________________________________________________

## 2. File-by-File Inventory

### 2.1 `X:/runtime/native/src/ui/ui_system.c` — 2,308 lines

**What it actually does:** A complete retained-mode UI data store. Every function validates session/node IDs, then reads/writes the appropriate array slot. There is no rendering logic anywhere in this file.

**Key subsystems (all data management, zero rendering):**

| Subsystem | Functions | What It Stores |
|-----------|-----------|----------------|
| Session lifecycle | `session_create`, `session_destroy`, `session_count`, `reset` | 16-slot global array of `KainNativeUiSession` |
| Frame lifecycle | `begin_frame`, `end_frame`, `present`, `frame_index`, `last_presented_frame` | Frame counter, draw command reset, dirty counter |
| Host management | `host_attach`, `host_pump`, `host_present`, `host_should_close`, `host_backend`, `host_presented_draw_count`, `host_frame_hash` | Delegates to host adapter; `host_present` hashes draw commands into a frame hash but doesn't render |
| Node tree | `node_create`, `node_destroy`, `node_set_parent`, `node_set_rect`, `node_set_text`, `node_set_flag`, `node_set_stable_key`, `node_find_by_stable_key`, plus getters | 4096-slot node array with open-addressing hash index, parent/child links, x/y/width/height, kind, text, stable_key, accessibility_role/label, flags (hidden/focusable/interactive/disabled/hovered/pressed) |
| Style system | `node_set_style_i64`, `node_set_style_f64`, `node_set_style_string`, plus getters | 8192-slot style array, keyed by (node_id, key). Values can be i64, f64, or string (up to 256 chars) |
| State system | `node_set_state_i64/f64/string`, plus getters | 8192-slot state array, identical structure to styles but separate namespace. Persisted for component state across frames |
| Draw commands | `draw_rect`, `draw_text`, `draw_resource`, plus getters | 8192-slot draw command array. Records rect/text/resource with full position, size, style_key, text, resource_id metadata. **These are stored but never consumed by a renderer** |
| Events | `push_event`, `poll_event`, `event_kind/target/x/y/key_code/text` | 1024-slot ring buffer. **Events are pushed from Kain code, NOT from the Win32 message pump** — the host adapter handles WM\_\* messages internally but does not translate them into ui_system events |
| Resources | `resource_create`, `font_create`, `texture_create`, `canvas_create`, `shader_create`, `resource_set_bytes/hex`, plus getters | 2048-slot resource array. Fonts store family+size in aux/scalar_value. Textures store format in aux + raw bytes. Shaders store stage in aux + raw bytes |
| Menus | `menu_create`, `menu_add_item`, `menu_open`, plus getters | 256-slot menu array + 2048-slot menu item array. Menu items have key, label, command_id |
| Dialogs | `dialog_request`, `dialog_respond`, `dialog_poll_response`, plus getters | 128-slot dialog array with kind, title, message, result, response_text |
| Focus | `focus`, `focused_node` | Single focused_node_id per session. Hit-test iterates nodes in reverse Z-order, tests rect containment |
| Drag-and-drop | `drag_begin/update/drop`, plus getters | Single active drag per session: source node, drop target, position, payload string |
| IME | `ime_begin/commit_text/end`, plus getters | Single IME-active node per session + composition text buffer |
| Clipboard | `clipboard_set_text`, `clipboard_text` | Internal buffer + optional host adapter delegation. **Host adapter clipboard is stubbed (returns 0)** |
| Accessibility | `accessibility_set_role/label`, plus getters | Per-node role/label strings. Stored, never consumed |
| Hot reload | `hot_reload_begin/commit`, `hot_reload_generation/key` | Simple generation counter + revision key string. **This is the ui_system hot reload, NOT the compiled bundle hot reload subsystem** |

**Hash table implementation detail:** The file contains ~200 lines of custom hash table infrastructure: De Bruijn bit-scan, mix64, FNV-1a-style hashing, open-addressing linear probe, occupancy bitmaps (one uint64_t per 64 slots). Every lookup (nodes, styles, state, resources, menus, dialogs) uses this. The token matcher (`abi_ui_flag_info`) identifies flag strings ("hidden", "visible", "focusable", etc.) via cryptographic hash matching — no string comparisons.

**Critical finding — `abi_ui_host_present`:** This function iterates ALL draw commands, hashes their contents into a frame hash, updates counters, then calls `abi_ui_host_adapter_present(session)`. The hash is for dirty detection, not rendering. The actual rendering is in the host adapter, which receives the session pointer but **reads nothing from the node tree or draw commands**.

### 2.2 `X:/runtime/native/src/ui/ui_system_internal.h` — 207 lines

**What it actually does:** Defines every data structure in the retained-mode UI engine.

**Key structs:**

- `KainNativeUiNode` — 18 fields: id, parent_id, child_count, flags (bitmask), dirty_reason, revision, x/y/width/height (double), kind[96], text[256], stable_key[96], accessibility_role[96], accessibility_label[256]
- `KainNativeUiStyleRecord` — 7 fields: node_id, value_kind (i64/f64/string), i64_value, f64_value, key[96], string_value[256]
- `KainNativeUiStateRecord` — Identical structure to StyleRecord, separate namespace
- `KainNativeUiDrawCommand` — 11 fields: kind[96] ("rect"/"text"/"resource"), node_id, x/y/width/height (double), resource_id, font_resource_id, text[256], style_key[96]
- `KainNativeUiEvent` — 6 fields: kind[96], target_node_id, key_code, x/y (double), text[256]
- `KainNativeUiResource` — 10 fields: id, width/height/byte_length (int64), scalar_value (double), bytes pointer + bytes_revision, resource_type[96], key[96], aux[256]
- `KainNativeUiMenu` — 6 fields: id, item_count, open, x/y (double), key[96]
- `KainNativeUiMenuItem` — 6 fields: id, menu_id, command_id, key[96], label[256]
- `KainNativeUiDialog` — 7 fields: id, result, response_ready, kind[96], title[256], message[256], response_text[256]
- `KainNativeUiSession` — The massive top-level struct: 80+ fields including all arrays, all indices, all occupancy bitmaps, counters, host state pointer, active event, drag/dialog/IME state. This is the entire UI state for one "app window."

**What's missing:** No renderer state. No GPU context. No framebuffer pointer (that's in the host adapter's private `KainWin32UiHost`). No layout engine. No text shaping state.

### 2.3 `X:/runtime/native/src/ui/ui_host_adapter.c` — 324 lines

**What it actually does:** Win32 window creation and a hardcoded software renderer.

**Key functions:**

- `win32_host_create(width, height)` — Registers window class "KainWin32UI", creates `WS_OVERLAPPEDWINDOW | WS_VISIBLE` window, creates a 32-bit BGRA GDI DIB section as a software framebuffer. The framebuffer is `width * height * 4` bytes of `uint32_t` ARGB pixels. Returns a `KainWin32UiHost` struct stored in `session->host_state`.

- `win32_host_render_framebuffer(host)` — **THE CRITICAL FUNCTION.** Fills the framebuffer with:

  1. Dark blue-gray background (`0xFF1A1A24` for every pixel)
  1. Position-based gradient (r from x, g from y, b from x+y) overwriting every pixel
  1. Three hardcoded `DrawTextA` GDI calls: "Kain UI Window (winit backend)", "Rendered by Pure Kain :: std::ui", "Resolution: WxH"
  1. Calls `InvalidateRect` to trigger `WM_PAINT` → `BitBlt`
     **It does NOT read `session->nodes`, `session->styles`, `session->draw_commands`, or any session data other than width/height.**

- `win32_host_pump_messages(host)` — Standard `PeekMessageA` → `TranslateMessage` → `DispatchMessageA` loop. Sets `host->running = 0` on `WM_QUIT`. **Does not translate messages into ui_system events.**

- `win32_host_destroy(host)` — Deletes GDI objects, destroys window, frees host.

- `abi_ui_host_adapter_attach(session, backend_id)` — Dispatches to `win32_host_create` for "winit" backend, or sets host to passive/memory for "headless"/"memory"/"software"/"auto". Currently only "winit" is a live backend.

- `abi_ui_host_adapter_present(session)` — Calls `win32_host_render_framebuffer` for winit backend. No-op for other backends.

- `abi_ui_host_adapter_pump(session)` — Calls `win32_host_pump_messages` for winit backend.

- `abi_ui_host_adapter_clipboard_set_text/get_text` — **Both are stubs.** `(void)text; return 0;` and `(void)out_text; (void)out_text_cap; return 0;`. Clipboard does not work.

- `abi_ui_host_adapter_is_live_backend(backend_id)` — Returns 1 only for "winit". All others return 0.

**Critical finding:** The `win32_host_render_framebuffer` function has a double-paint bug: it fills the framebuffer with background color, then overwrites EVERY pixel with the gradient, then draws text via GDI `DrawTextA` (which draws to the window HDC, not the framebuffer). The GDI text calls draw directly to the screen DC, bypassing the DIB framebuffer. The `BitBlt` in `WM_PAINT` then blits the gradient framebuffer over the GDI text. The net effect: gradient background with text overlaid (text survives because GDI draws after the gradient fill, and WM_PAINT blits the framebuffer which was filled BEFORE the GDI text — actually the GDI text draws to a DIFFERENT DC, so the text appears on screen but isn't in the framebuffer at all).

### 2.4 `X:/runtime/native/src/ui/ui_host_adapter.h` — 21 lines

**What it actually does:** Declares 6 functions: `is_live_backend`, `attach`, `pump`, `present`, `shutdown`, `clipboard_set_text`, `clipboard_get_text`. All implemented in `ui_host_adapter.c`.

### 2.5 `X:/runtime/native/src/ui/native_ui_surface.c` — 291 lines

**What it actually does:** The 17-slot `KainComponentSurface` vtable implementation for the "native_ui" backend. This is the bridge between the Rust codegen's vtable calls and System A's `abi_ui_*` functions.

**Key functions:**

- `native_ui_element_begin(session_id, parent_id, kind, stable_key)` — **The reconciliation algorithm.** If stable_key is non-empty, calls `abi_ui_node_find_by_stable_key` to find an existing node. If found, updates parent and returns it. If not found, creates a new node via `abi_ui_node_create`, sets stable key, sets parent. This is how the retained-mode tree survives frame boundaries.

- `native_ui_element_end(session_id, element_id)` — **No-op.** `(void)session_id; (void)element_id;` Nodes persist across frames.

- `native_ui_set_attr_string(session_id, element_id, key, value)` — **Only forwards 4 known keys:** `fill_color`, `border_color`, `ink_color`, `title`. All other string attributes are silently ignored. This is a critical gap — JSX attributes like `background`, `color`, `direction`, `padding`, `spacing`, `font_size`, `corner_radius`, `opacity`, `border`, `width`, `height` are either handled by other vtable slots (i64/f64) or silently dropped here.

- `get_state_root(session_id)` — Creates or finds a hidden node with stable key `__kain_state_root`. Component state is stored as node state on this hidden node with keys like `"ComponentName:field_name"`.

- `native_ui_state_get_i64` / `native_ui_state_set_i64` — Read/write component state through the hidden state root node.

- `native_ui_session_create` — Calls `abi_ui_session_create`, then on Win32 auto-attaches the "winit" host adapter, which calls `win32_host_create` → `RegisterClassA` → `CreateWindowExA`. **This is where the real OS window is born.**

- `native_ui_present` — Calls both `abi_ui_present` (frame counter update) and `abi_ui_host_present` (which calls `win32_host_render_framebuffer` → hardcoded gradient → `InvalidateRect` → `BitBlt`).

- `wrap_poll_event` — Ignores `out_event`/`max_size` params, delegates to `abi_ui_poll_event`. The trait signature has extra params for future backends that serialize events into caller-owned buffers.

**Vtable slots filled (all 17 non-NULL):**

| Offset | Slot | Implementation |
|--------|------|----------------|
| 0 | `session_create` | `native_ui_session_create` (creates session + auto-attaches winit) |
| 1 | `session_destroy` | `wrap_session_destroy` → `abi_ui_session_destroy` |
| 2 | `element_begin` | `native_ui_element_begin` (reconciliation via stable key) |
| 3 | `element_end` | `native_ui_element_end` (no-op) |
| 4 | `element_set_text` | `wrap_element_set_text` → `abi_ui_node_set_text` |
| 5 | `element_set_attr_i64` | `wrap_set_attr_i64` → `abi_ui_node_set_style_i64` |
| 6 | `element_set_attr_f64` | `wrap_set_attr_f64` → `abi_ui_node_set_style_f64` |
| 7 | `element_set_attr_string` | `native_ui_set_attr_string` (4-key allowlist) |
| 8 | `state_get_i64` | `native_ui_state_get_i64` |
| 9 | `state_set_i64` | `native_ui_state_set_i64` |
| 10 | `begin_frame` | `wrap_begin_frame` → `abi_ui_begin_frame` |
| 11 | `end_frame` | `wrap_end_frame` → `abi_ui_end_frame` |
| 12 | `present` | `native_ui_present` (present + host_present) |
| 13 | `poll_event` | `wrap_poll_event` → `abi_ui_poll_event` |
| 14 | `should_close` | `abi_ui_host_should_close` (direct, no wrapper) |
| 15 | `window_open` | `abi_ui_window_open` (direct) |
| 16 | `host_pump` | `abi_ui_host_pump` (direct) |

**Auto-registration:** On Win32, uses `__declspec(allocate(".CRT$XCU"))` to register the surface before `main()`. On Linux/macOS, uses `__attribute__((constructor))`.

### 2.6 `X:/runtime/native/src/ui/ui_runtime.c` — 1,283 lines

**What it actually does:** A compile-time node manifest runtime. Parses "compiled bundles" (JSON), validates them, builds KainUiRuntimeComponentState records with capability flags, routes focus/editable/text events between components. **This system is parallel to and disconnected from the retained-mode ui_system.c.**

**Key subsystems:**

- **Kind profiles** — 14 entries mapping `KainUiCompiledNodeKind` (PANEL, INSPECTOR, GRAPH, TIMELINE, TABLE, TREE, VIEWPORT2D, VIEWPORT3D, OVERLAY, SLOT, ELEMENT, COMPONENT_REF, TEXT, UNKNOWN) to roles, capability flags, focusable/editable booleans.

- **Focusable/editable tag lists** — Hardcoded string arrays of tag names that are focusable (button, control, field, graph, input, inspector, panel, search, slider, table, textbox, text-input, timeline, tree, viewport, viewport2d, viewport3d, editable) and editable (editable, field, input, search, text-input, textbox, textarea, value).

- **Bundle validation** — `kain_ui_runtime_validate_bundle` checks: bundle loaded, has root_id, root node exists, no duplicate IDs, no orphan parent references, warning if no focusable/editable nodes.

- **Component state building** — `kain_ui_runtime_build_component_state` converts a `KainUiCompiledNode` into a `KainUiRuntimeComponentState` with role, capabilities, title, text, tag, scene, layout_kind, persistent_layout_id.

- **Focus routing** — `kain_ui_runtime_route_focus_event` handles focus_next/focus_prev (Tab/Shift+Tab cycling through focusable components), blur, focus_request, pointer_down.

- **Editable text** — `kain_ui_runtime_route_text_event` handles text input (append to component value), backspace/delete (remove last char), enter (commit). Uses `kain_ui_runtime_mark_dirty` to track changes.

- **Hot reload transfer** — `kain_ui_runtime_reload_bundle` rebuilds components from a new bundle while preserving focus, active edit component, hovered component, component values, and dirty state from the old bundle.

- **Load paths** — From JSON string, from file path, from env var.

**What it does NOT do:** Render anything. Connect to ui_system.c nodes. Produce pixels. Handle Win32 events. It's a pure data model + event routing layer for a compile-time-defined UI tree.

### 2.7 `X:/runtime/native/src/ui/ui_compiled_bundle.c` — 778 lines

**What it actually does:** A hand-written JSON parser for "compiled bundle" files. Parses a specific JSON schema with `output.tree.nodes`, extracts node ids, kinds, props (title, text, tag, scene), layout (kind, dock, split_ratio, resizable, persistent_layout_id, tab_group_id, tab_label, tab_order, tab_default_active, tab_closable), and children arrays (flat arrays of numeric child IDs). Reconstructs parent relationships and computes node depths.

**Key functions:**

- `kain_ui_compiled_bundle_load_from_json(json, bundle)` — Full JSON parser. Walks the JSON string character by character (no library dependency). Finds `"output"` → `"tree"` → `"nodes"`, then iterates numeric keys, parses each node object. After parsing all nodes, does a second pass to wire parent_id from children arrays, then computes depth for each node.

- `kain_ui_compiled_bundle_load_from_path/from_env` — File/ENV wrappers that read the file into memory then call `load_from_json`.

- `kain_ui_compile_bundle_find_first_kind` — Linear scan for first node of a given kind.

- `kain_ui_parse_node_kind` — String-to-enum mapping for 14 node kinds (Element, ComponentRef, Text, Panel, Inspector, Graph, Timeline, Table, Tree, Viewport2D, Viewport3D, Overlay, Slot, Unknown).

**What the JSON format looks like:** `{"output":{"tree":{"root":1,"nodes":{"0":{"id":1,"kind":"Element","props":{"title":"Dashboard"},"layout":{"kind":"panel"},"children":[2,3]}}}}},"window_title":"My App"}`

**Critical:** This system does not connect to `ui_system.c`'s `abi_ui_node_create` or any of the retained-mode functions. The `KainUiCompiledBundle` is a completely separate data structure from `KainNativeUiSession`. The compiled bundle is a static manifest; the retained-mode tree is a dynamic frame-by-frame data store.

### 2.8 `X:/runtime/native/src/ui/ui_hot_reload.c` — 710 lines

**What it actually does:** A shared-memory IPC hot-reload channel. On Win32, uses `CreateFileMappingA`/`OpenFileMappingA`/`MapViewOfFile` with names like `Local\kain-ui-reload.appname`. On POSIX, uses `shm_open`/`mmap`. The shared memory contains a `KainUiHotReloadSharedControl` struct with: magic/version, atomic generation counters (request/applied/failed), fingerprints, watched file signature, requested bundle path, last status/error strings, and a 128-slot ring buffer of events.

**Key functions:**

- `kain_ui_hot_reload_channel_create/open` — Creates or opens the named shared memory region.

- `kain_ui_hot_reload_controller_boot` — Initializes a controller with app name, bundle path from env var, file watch enabled, poll interval from env var (default 125ms). Creates the shared memory channel.

- `kain_ui_hot_reload_controller_apply_pending` — Poll loop: checks shared memory for new request generation, checks file signature for changes, loads the new bundle, validates it, transfers component state from old to new (preserving focus/edit/hover/values/dirty), swaps the runtime state atomically. Pushes events (REQUESTED/APPLIED/REJECTED) into the ring buffer.

- `kain_ui_hot_reload_channel_request_bundle` — The other side: writes a bundle path + fingerprint into shared memory, increments the request generation counter. The controller picks this up on its next poll.

- File signature: hash of path + file size + last write time (Win32) or path + file size + mtime seconds + mtime nanos (POSIX).

**What it connects to:** The `ui_runtime.c` system — it reloads `KainUiRuntimeState` from `KainUiCompiledBundle`. It does NOT connect to the retained-mode `ui_system.c` at all. The hot reload in `ui_system.c` (`abi_ui_hot_reload_begin/commit`) is a separate, simpler mechanism using just a generation counter.

______________________________________________________________________

### 2.9 `X:/runtime/native/include/ui_system.h` — 261 lines

**What it actually does:** Public header declaring all 112 `abi_ui_*` functions and capacity constants.

**Function count by category:**

| Category | Count | Key Functions |
|----------|-------|---------------|
| Session lifecycle | 5 | `reset`, `session_create`, `session_destroy`, `session_count`, `window_open`, `window_close` |
| Frame lifecycle | 5 | `begin_frame`, `end_frame`, `present`, `frame_index`, `last_presented_frame` |
| Host management | 7 | `host_attach`, `host_pump`, `host_present`, `host_presented_draw_count`, `host_frame_hash`, `host_should_close`, `host_backend` |
| Node tree | 11 | `node_create`, `node_destroy`, `node_count`, `node_exists`, `node_set_parent`, `node_parent`, `node_child_count`, `node_set_rect`, accessors (x/y/width/height) |
| Node text/kind/stable key | 7 | `node_set_text`, `node_text`, `node_kind`, `node_set_stable_key`, `node_stable_key`, `node_find_by_stable_key` |
| Node flags | 2 | `node_set_flag`, `node_has_flag` |
| Style (per-node key-value) | 6 | `node_set_style_i64`, `node_set_style_f64`, `node_set_style_string`, plus getters |
| State (per-node key-value) | 7 | `node_set_state_i64/f64/string`, plus getters, `state_count` |
| Focus/hit-test/dirty | 5 | `focus`, `focused_node`, `hit_test`, `mark_dirty`, `dirty_count` |
| Events | 7 | `push_event`, `poll_event`, event kind/target/x/y/key_code/text accessors |
| Draw commands (record) | 12 | `draw_rect`, `draw_text`, `draw_resource`, `draw_command_count`, accessors (kind/node/resource/x/y/width/height/text/style/font) |
| Resources | 12 | `resource_create`, `font_create`, `texture_create`, `canvas_create`, `shader_create`, `resource_set_bytes/hex`, `resource_count/exists`, accessors (type/key/width/height/byte_length) |
| Text measurement | 2 | `text_measure_width`, `text_measure_height` |
| Clipboard | 2 | `clipboard_set_text`, `clipboard_text` |
| IME | 5 | `ime_begin`, `ime_commit_text`, `ime_end`, `ime_active_node`, `ime_text` |
| Drag-and-drop | 7 | `drag_begin/update/drop`, accessors (active_node/drop_target/x/y/payload) |
| Menus | 6 | `menu_create`, `menu_add_item`, `menu_open`, `menu_active`, `menu_item_count`, accessors (label/command) |
| Dialogs | 7 | `dialog_request`, `dialog_active`, accessors (kind/title/message), `dialog_respond`, `dialog_poll_response`, `dialog_response_text` |
| Hot reload | 4 | `hot_reload_begin`, `hot_reload_commit`, `hot_reload_generation`, `hot_reload_key` |
| Accessibility | 4 | `accessibility_set_role/label`, accessors |
| **Total** | **~123** | |

### 2.10 `X:/runtime/native/include/ui_runtime.h` — 193 lines

**What it actually does:** Public header for System B (compiled bundle runtime). Declares `KainUiRuntimeState`, `KainUiRuntimeComponentState`, `KainUiRuntimeValidationReport`, `KainUiRuntimeReloadOptions`, `KainUiRuntimeReloadReport`, `KainUiRuntimeEvent`, `KainUiRuntimeEventResult`. All state is held in `KainUiRuntimeState` which contains a `KainUiCompiledBundle` and an array of `KainUiRuntimeComponentState`. **No connection to ui_system.h types.**

### 2.11 `X:/runtime/native/include/ui_bundle.h` — 83 lines

**What it actually does:** Public header for the compiled bundle data model. Declares `KainUiCompiledNodeKind` (14 enum values), `KainUiCompiledNode`, `KainUiCompiledTabGroup`, `KainUiCompiledBundle`. Maximum 128 nodes per bundle. Loads from JSON string, file path, or env var.

### 2.12 `X:/runtime/native/include/ui_hot_reload.h` — 118 lines

**What it actually does:** Public header for the shared-memory hot reload IPC system. Declares `KainUiHotReloadSharedControl` (the shared memory layout with atomic counters and 128-slot ring buffer), `KainUiHotReloadChannel`, `KainUiHotReloadController`. The controller wraps a channel + file watch state.

### 2.13 `X:/runtime/native/include/component_surface.h` — 85 lines

**What it actually does:** The `KainComponentSurface` trait struct — 17 function pointers forming the vtable. Plus `kain_component_surface_register` and `kain_component_surface_resolve` registry functions. Plus `extern const KainComponentSurface native_ui_surface` declaration. **This is the ABI contract between the Rust codegen and the C runtime.** The compiler only knows `KainComponentSurface*` — it never calls `abi_ui_*` directly.

### 2.14 `X:/stdlib/ui.kn` — 1,569 lines

**What it actually does:** The Kain `std::ui` module. Three layers:

1. **Layer 1 — Raw @extern declarations (lines 1-340):** ~123 `@extern fn abi_ui_*` declarations mapping every C function in `ui_system.h` to Kain. Types are translated: `int64_t` → `Int`, `double` → `Float`, `const char*` → `String`, `const uint8_t*` → (not exposed — `resource_set_bytes_hex` takes hex string instead).

1. **Layer 2 — Thin wrappers (lines 341-1350):** `pub fn native_ui_*` functions that are direct passthroughs to the `@extern` functions. This is boilerplate to make the raw externs available as public API.

1. **Layer 3 — Higher-level helpers (lines 1350-1569):**

   - `ui_host_session_create` — Creates session + opens window + attaches host in one call
   - `ui_frame_begin` / `ui_frame_submit` — Begin frame + present to host
   - `ui_node_from_stable_key` / `ui_reconcile_node` / `ui_reconcile_text_node` / `ui_reconcile_labeled_node` / `ui_reconcile_focusable_node` — Stable-key-based node reconciliation with optional text, accessibility role/label, focusable flag
   - `ui_reconcile_stateful_node` — Reconcile node with state kind/payload
   - `ui_style_color_rgba` / `ui_style_metric` / `ui_style_padding` / `ui_style_spacing` — Style helpers that store decomposed color (r,g,b,a as separate f64 keys) and metrics under style_key prefixes
   - `ui_style_inherit_color_rgba` — Inherit color from parent if child doesn't override
   - `ui_style_copy_color_rgba` — Copy color between nodes
   - `ui_state_set/get` — High-level state accessors (i64, f64, string, bool, toggle, counter, reference)
   - `ui_state_shape/hit/draw/resource` — Structured state set with kind+payload pattern
   - `ui_state_shared_buffer_resource` / `ui_state_shared_image_resource` — Graphics shared resource integration with `std::graphics::shared`
   - `ui_render_box` / `ui_render_box_at` / `ui_render_text` / `ui_render_text_value` / `ui_render_resource` / `ui_render_resource_in_node` / `ui_render_text_in_box` — Render helpers that call `abi_ui_draw_rect` / `abi_ui_draw_text` / `abi_ui_draw_resource`
   - `ui_event_kind_is` / `ui_event_targets` / `ui_focus_if_event_targets` — Event matching helpers
   - `ui_node_contains_point` / `ui_node_is_hit` / `ui_apply_hover_flag` / `ui_apply_pressed_flag_from_event` — Hit-test and flag management
   - `ui_drain_events_for_node` — Poll and process events for a specific node
   - `ui_present_to_attached_host` — End frame + present + host_present in one call
   - Layout helpers: `ui_layout_column_y`, `ui_layout_row_x`, `ui_float_min/max/clamp`, `ui_rect_right/bottom/contains`, `ui_layout_inset_*`, `ui_layout_center_*`, `ui_layout_column_height`, `ui_layout_row_width`, `ui_layout_split_*`
   - `ui_text_width` / `ui_text_height` — Text measurement wrappers
   - `ui_texture_rgba8_from_hex` / `native_ui_texture_create_from_hex` — Texture creation from hex strings

**Missing from std::ui:** No high-level widget library. No layout engine (only helper math functions). No theme system. No animation system. All of that belongs in Kaintana (`blades/kaintana/`), not in stdlib.

### 2.15 `X:/docs/COMPONENT.MD` — 1,511 lines

**What it actually does:** Comprehensive documentation of the `component` keyword. Covers: component anatomy, JSX syntax, tag dispatch, world+component wiring, patterns, known gaps (6 documented), anti-patterns, source anchors. Already read by the agent.

### 2.16 `X:/blades/window_proof/README.md` — 98 lines

**What it actually does:** Documents the 3 working demos (minimal window, world state, dashboard), the architecture diagram, 6 known gaps, the 17-slot vtable table, build/verify commands, and a note about the planned Vulkan backend.

### 2.17 `X:/blades/NEWSLETTER-2.md` — 414 lines

**What it actually does:** The merge story for the component surface pipeline. Documents the "Plumber A vs Plumber B" divergence (C runtime built the trait, Rust codegen emitted direct abi_ui\_\* calls instead of vtable calls), the reconciliation, bugs found during audit, files touched, and what didn't change.

______________________________________________________________________

## 3. The Node Tree

### 3.1 Data Structures

The retained-mode node tree lives in `KainNativeUiSession`:

```
KainNativeUiSession
├── nodes[4096]              ← KainNativeUiNode (flat array, not a pointer tree)
│   ├── id, parent_id        ← tree linking via integer IDs
│   ├── child_count          ← counter, not child list
│   ├── flags                ← bitmask: hidden, focusable, interactive, disabled, hovered, pressed
│   ├── x, y, width, height  ← double-precision floats
│   ├── kind[96]             ← e.g., "box", "text", "panel", "stack"
│   ├── text[256]            ← text content
│   ├── stable_key[96]       ← deterministic key for cross-frame reconciliation
│   ├── accessibility_role[96], accessibility_label[256]
│   └── dirty_reason, revision
├── styles[8192]             ← KainNativeUiStyleRecord, keyed by (node_id, key)
│   └── value_kind: i64 | f64 | string
├── state[8192]              ← KainNativeUiStateRecord, same structure as styles
├── draw_commands[8192]      ← KainNativeUiDrawCommand, per-frame recording
├── events[1024]             ← ring buffer
├── resources[2048]          ← fonts, textures, canvases, shaders
├── menus[256] + menu_items[2048]
├── dialogs[128]
└── [Hash indices for nodes, stable keys, styles, state, resources, menus, dialogs]
```

### 3.2 Tree Operations

| Operation | Supported? | Implementation |
|-----------|-----------|----------------|
| Create node | Yes | `abi_ui_node_create` — finds free slot, assigns ID, stores kind |
| Destroy node | Yes | `abi_ui_node_destroy` — orphans children, clears focus/drag/IME refs, releases styles/state payloads |
| Set parent | Yes | `abi_ui_node_set_parent` — cycle detection included |
| Get parent | Yes | `abi_ui_node_parent` |
| Count children | Yes | `abi_ui_node_child_count` — stored as counter, not enumerated |
| Set rect | Yes | `abi_ui_node_set_rect` — x, y, width, height as doubles |
| Set text | Yes | `abi_ui_node_set_text` — stored in node.text[256] |
| Set flag | Yes | `abi_ui_node_set_flag` — cryptographic hash matching for flag names |
| Set stable key | Yes | `abi_ui_node_set_stable_key` — triggers full index rebuild |
| Find by stable key | Yes | `abi_ui_node_find_by_stable_key` — hash table lookup |
| Hit test | Yes | `abi_ui_hit_test` — reverse Z-order rect containment |
| Set style | Yes | `abi_ui_node_set_style_i64/f64/string` — stored in styles array |
| Set state | Yes | `abi_ui_node_set_state_i64/f64/string` — stored in state array |
| **Enumerate children** | **No** | No `abi_ui_node_get_child(node_id, index)` — you must scan all 4096 nodes |
| **Walk tree** | **No** | No iterator, no visitor pattern. Linear scan is the only option. |
| **Layout engine** | **No** | Rects are set externally. No flexbox, grid, or constraint solver. |
| **Style cascade** | **No** | No inheritance, no computed styles. Each node has its own style records. |
| **Render tree** | **No** | Draw commands are recorded separately; the tree structure is not used for rendering. |

### 3.3 Draw Command Recording

Draw commands are recorded per-frame via:

- `abi_ui_draw_rect(session, node_id, x, y, w, h, style_key)` → appends `KainNativeUiDrawCommand{kind="rect", ...}`
- `abi_ui_draw_text(session, node_id, font_resource_id, x, y, text, style_key)` → appends `KainNativeUiDrawCommand{kind="text", ...}`
- `abi_ui_draw_resource(session, node_id, resource_id, x, y, w, h, style_key)` → appends `KainNativeUiDrawCommand{kind="resource", ...}`

These are cleared at `begin_frame` (`draw_command_count = 0`). They accumulate during the frame. They are hashed in `host_present` for dirty detection. **They are never read by any renderer.**

______________________________________________________________________

## 4. The Render Path

### 4.1 What Actually Happens (End-to-End Trace)

```
1. Kain source:    world W: surface native_ui => MyComponent
                   component MyComponent(): render <panel title="Hi" />

2. Rust codegen:   emits frame loop:
     surface = kain_component_surface_resolve("native_ui")    → native_ui_surface*
     sid = surface→session_create("W", 1280, 720)
       → native_ui_session_create
         → abi_ui_session_create("W", 1280, 720)             → session_id=1, host_backend="memory"
         → abi_ui_host_attach(1, "winit")                    → win32_host_create(1280,720)
           → RegisterClassA("KainWin32UI")                   → OK
           → CreateWindowExA(WS_OVERLAPPEDWINDOW|WS_VISIBLE) → HWND appears on screen
           → CreateDIBSection → 1280*720*4 byte framebuffer  → host->framebuffer
           → UpdateWindow                                    → WM_PAINT → BitBlt (black screen)
     surface→window_open(1, "W", 1280, 720)                  → session->open = 1

   LOOP:
     surface→host_pump(1)
       → abi_ui_host_pump → win32_host_pump_messages
         → PeekMessageA → TranslateMessage → DispatchMessageA
         → WM_CLOSE → host->running = 0 → session->host_should_close = 1
     surface→begin_frame(1, delta_ms)
       → abi_ui_begin_frame → session->draw_command_count = 0
     MyComponent_render(surface, 1, 0)
       → surface→element_begin(1, 0, "panel", "MyComponent:panel:0:0")
         → native_ui_element_begin
           → abi_ui_node_find_by_stable_key("MyComponent:panel:0:0") → 0 (first frame)
           → abi_ui_node_create(1, "panel") → node_id = 1
           → abi_ui_node_set_stable_key(1, 1, "MyComponent:panel:0:0")
           → abi_ui_node_set_parent(1, 1, 0)
       → surface→element_set_attr_string(1, 1, "title", "Hi")
         → native_ui_set_attr_string → abi_ui_node_set_style_string(1, 1, "title", "Hi")
       → surface→element_end(1, 1) → (no-op)
     surface→end_frame(1) → abi_ui_end_frame → returns draw_command_count
     surface→present(1)
       → native_ui_present
         → abi_ui_present(1)              → frame counter update, dirty_count = 0
         → abi_ui_host_present(1)
           → hashes draw_commands (none recorded, so hash=0)
           → abi_ui_host_adapter_present(1)
             → win32_host_render_framebuffer(host)
               → fill framebuffer with 0xFF1A1A24 (dark blue-gray)
               → fill framebuffer with position-based gradient
               → DrawTextA("Kain UI Window (winit backend)")  ← HARDCODED
               → DrawTextA("Rendered by Pure Kain :: std::ui") ← HARDCODED
               → DrawTextA("Resolution: 1280x720")            ← HARDCODED
               → InvalidateRect → WM_PAINT → BitBlt(framebuffer→screen)
     surface→should_close(1) → 0 → continue loop
```

### 4.2 The Render Chain Gap

```
Component JSX     →  vtable calls  →  ui_system.c nodes/styles/draw_commands
                                            │
                                     STORED BUT NEVER READ BY RENDERER
                                            │
                                     ┌──────┘
                                     ▼
                              ui_host_adapter.c
                              win32_host_render_framebuffer()
                              │
                              ├── Background fill: HARDCODED 0xFF1A1A24
                              ├── Gradient:       HARDCODED (r=x/w, g=y/h, b=(x+y)/(w+h))
                              └── Text:           HARDCODED 3 strings
```

**The node tree is a write-only data structure from the renderer's perspective.** Everything stored via `abi_ui_node_set_style_string`, `abi_ui_node_set_text`, `abi_ui_node_set_rect`, `abi_ui_draw_rect`, `abi_ui_draw_text` is invisible to the renderer.

### 4.3 What WOULD Need to Change

For `win32_host_render_framebuffer` to actually render the node tree, it would need to:

1. Access `session->nodes[]` to get each node's rect, kind, text, flags
1. Access `session->styles[]` to get `fill_color`, `border_color`, `ink_color`, `font_size`, etc.
1. Walk the tree in some order (e.g., parent-first with child z-ordering)
1. For each node, fill its rect with `fill_color`, draw border with `border_color`, draw text with `ink_color` and `font_size`
1. Optionally process `session->draw_commands[]` for explicit draw calls

**None of this exists today.** The renderer has zero awareness of the node tree.

### 4.4 Style Attribute Flow

```
Kain JSX:     background="#FF0000"
    ↓
Codegen:      element_set_attr_string(sid, el, "fill_color", "#FF0000")
    ↓
native_ui_surface.c:  native_ui_set_attr_string → key "fill_color" IS in allowlist
    ↓
ui_system.c:  abi_ui_node_set_style_string(sid, el, "fill_color", "#FF0000")
    ↓
              Stored in session->styles[slot].string_value = "#FF0000"
              Stored in session->styles[slot].key = "fill_color"
    ↓
Renderer:     NEVER READ. win32_host_render_framebuffer uses hardcoded colors.
```

For keys NOT in the allowlist (background, color, border, padding, spacing, font_size, corner_radius, opacity, direction, width, height as strings):

```
JSX attribute → codegen → element_set_attr_string → native_ui_set_attr_string → SILENTLY IGNORED
```

Note: many of these are routed through `element_set_attr_f64` or `element_set_attr_i64` by the codegen, not through `element_set_attr_string`. The 4-key allowlist only applies to string-valued attributes that happen to be routed through the string slot.

______________________________________________________________________

## 5. What's Connected vs What's Disconnected

### 5.1 Connected (Call Chain Exists)

| From | Through | To | Status |
|------|---------|-----|--------|
| Rust codegen `surface→session_create` | `native_ui_surface.c` | `abi_ui_session_create` + `abi_ui_host_attach("winit")` → `win32_host_create` → `CreateWindowExA` | ✅ Working |
| Rust codegen `surface→element_begin` | `native_ui_surface.c` | `abi_ui_node_create` + stable key reconciliation | ✅ Working |
| Rust codegen `surface→element_set_text` | `native_ui_surface.c` | `abi_ui_node_set_text` | ✅ Working |
| Rust codegen `surface→element_set_attr_*` | `native_ui_surface.c` | `abi_ui_node_set_style_*` (with 4-key allowlist for strings) | ⚠️ Partial |
| Rust codegen `surface→state_get/set_i64` | `native_ui_surface.c` | `abi_ui_node_state_i64/set_i64` on hidden `__kain_state_root` node | ✅ Working |
| Rust codegen `surface→begin/end_frame` | `native_ui_surface.c` | `abi_ui_begin/end_frame` | ✅ Working |
| Rust codegen `surface→present` | `native_ui_surface.c` | `abi_ui_present` + `abi_ui_host_present` → `win32_host_render_framebuffer` | ✅ Working (hardcoded content) |
| Rust codegen `surface→should_close` | `native_ui_surface.c` | `abi_ui_host_should_close` | ✅ Working |
| Rust codegen `surface→host_pump` | `native_ui_surface.c` | `abi_ui_host_pump` → `win32_host_pump_messages` → `PeekMessageA` | ✅ Working |
| `stdlib/ui.kn` `ui_present_to_attached_host` | Direct call | `abi_ui_end_frame` + `abi_ui_present` + `abi_ui_host_present` | ✅ Working |
| `stdlib/ui.kn` `ui_render_box` | Direct call | `abi_ui_draw_rect` → stored in draw_commands | ✅ Stored, never rendered |
| `stdlib/ui.kn` `ui_render_text` | Direct call | `abi_ui_draw_text` → stored in draw_commands | ✅ Stored, never rendered |

### 5.2 Disconnected (Available But Never Called)

| Function | Declared In | Available? | Called By Any Code Path? |
|----------|------------|------------|--------------------------|
| `abi_ui_draw_rect` | `ui_system.h` | Yes | Called by `stdlib/ui.kn` helpers, but renderer ignores draw commands |
| `abi_ui_draw_text` | `ui_system.h` | Yes | Same |
| `abi_ui_draw_resource` | `ui_system.h` | Yes | Same |
| `abi_ui_focus` | `ui_system.h` | Yes | Called by `stdlib/ui.kn` `ui_focus_if_event_targets` |
| `abi_ui_hit_test` | `ui_system.h` | Yes | Called by `stdlib/ui.kn` `ui_node_is_hit` |
| `abi_ui_push_event` | `ui_system.h` | Yes | Available, no evidence of use in current demos |
| `abi_ui_poll_event` | `ui_system.h` | Yes | Called by component surface vtable slot 13 |
| `abi_ui_clipboard_set_text` | `ui_system.h` | Yes | Available, host adapter clipboard is stubbed |
| `abi_ui_font_create` | `ui_system.h` | Yes | Available, renderer uses GDI fonts instead |
| `abi_ui_texture_create` | `ui_system.h` | Yes | Available, no renderer consumes textures |
| `abi_ui_canvas_create` | `ui_system.h` | Yes | Available, no consumer |
| `abi_ui_shader_create` | `ui_system.h` | Yes | Available, no consumer |
| `abi_ui_resource_set_bytes` | `ui_system.h` | Yes | Available, no consumer |
| `abi_ui_menu_create/add_item/open` | `ui_system.h` | Yes | Available, no context menu integration in host adapter |
| `abi_ui_dialog_request` | `ui_system.h` | Yes | Available, no native dialog integration |
| `abi_ui_drag_begin/update/drop` | `ui_system.h` | Yes | Available, no drag image rendering |
| `abi_ui_ime_begin/commit/end` | `ui_system.h` | Yes | Available, no IME integration in host adapter |
| `abi_ui_accessibility_set_role/label` | `ui_system.h` | Yes | Available, no accessibility bridge to OS |

### 5.3 Stubs (Declared But Body Returns Constant)

| Function | What It Does |
|----------|-------------|
| `abi_ui_host_adapter_clipboard_set_text` | `(void)text; return 0;` — completely non-functional |
| `abi_ui_host_adapter_clipboard_get_text` | `(void)out_text; (void)out_text_cap; return 0;` — completely non-functional |
| `native_ui_element_end` | `(void)session_id; (void)element_id;` — intentional no-op |
| `abi_ui_text_measure_width` | Estimates width as `length * font_size * 0.56` — no actual text shaping |
| `abi_ui_text_measure_height` | Returns `font_size * 1.25` — no actual font metrics |
| `abi_ui_resource_set_bytes_hex` | `returns ABI_UI_INVALID_ARGUMENT;` — hex decode path exists but function stub in header (actually the implementation in ui_system.c DOES work, it's a decoder) |

### 5.4 The Two Parallel UI Systems

```
SYSTEM A (Retained-Mode)              SYSTEM B (Compiled Bundle)
========================              ==========================
ui_system.h / ui_system.c            ui_bundle.h / ui_compiled_bundle.c
ui_system_internal.h                 ui_runtime.h / ui_runtime.c
ui_host_adapter.h / .c               ui_hot_reload.h / ui_hot_reload.c
native_ui_surface.c (bridge)
stdlib/ui.kn (wraps System A)

Data: KainNativeUiSession           Data: KainUiRuntimeState
      KainNativeUiNode                    KainUiRuntimeComponentState
      KainNativeUiStyleRecord             KainUiCompiledNode
      KainNativeUiStateRecord             KainUiCompiledBundle
      KainNativeUiDrawCommand

API: 112 abi_ui_* functions         API: kain_ui_runtime_* functions

Connected to:                        Connected to:
  component surface vtable             NOTHING in System A
  std::ui Kain module                  Hot-reload IPC channel
  win32 host adapter                   JSON bundle files

Renders: YES (hardcoded)             Renders: NO

Purpose: Dynamic per-frame            Purpose: Static compile-time
         retained UI store                   node manifest with
                                             focus/editable routing
```

**These systems do not exchange data.** A node created in System A via `abi_ui_node_create` is invisible to System B's `KainUiRuntimeState`. A bundle loaded in System B via `kain_ui_compiled_bundle_load_from_json` is invisible to System A's session nodes.

______________________________________________________________________

## 6. The std::ui Module

### 6.1 @extern Function Count and Mapping

The `stdlib/ui.kn` module declares approximately **123 `@extern fn abi_ui_*`** functions. Every one maps 1:1 to a C function in `ui_system.h`. Additionally, a few functions declared in the header are MISSING from the Kain @extern declarations:

- `abi_ui_resource_set_bytes` — takes `const uint8_t*` which has no direct Kain type mapping (only `resource_set_bytes_hex` is exposed)
- `abi_ui_draw_command_resource` — declared in header but not in Kain stdlib
- `abi_ui_draw_command_x/y/width/height/text/style/font` — draw command accessors partially exposed

### 6.2 Higher-Level Kain Functions

The module provides ~60 higher-level Kain functions that compose `@extern` calls. These are the "real" API for Kain authors:

| Function | What It Does | Works? |
|----------|-------------|--------|
| `ui_host_session_create` | Create session + window + attach host | ✅ |
| `ui_frame_begin` / `ui_frame_submit` | Begin frame + present to host | ✅ |
| `ui_node_from_stable_key` | Create-or-find node by stable key, set rect+text+parent | ✅ |
| `ui_reconcile_*` variants | Node reconciliation with different attribute sets | ✅ |
| `ui_style_color_rgba` | Store RGBA color as 4 f64 values under style_key.color.{r,g,b,a} | ✅ Stored |
| `ui_style_metric` | Store metric as f64 under style_key.metric_name | ✅ Stored |
| `ui_style_padding` | Store padding as 4 f64 values | ✅ Stored |
| `ui_style_spacing` | Store gap as f64 | ✅ Stored |
| `ui_style_inherit_color_rgba` | Read parent color, use as fallback for child | ✅ |
| `ui_state_*` family | Read/write state as i64, f64, string, bool, toggle, counter | ✅ |
| `ui_render_box` | Draw rect at node's own position/size with style_key | ✅ Recorded, not rendered |
| `ui_render_text` | Draw text at position with node's text content | ✅ Recorded, not rendered |
| `ui_render_resource` | Draw resource (texture/canvas) at position | ✅ Recorded, not rendered |
| `ui_event_*` helpers | Event kind matching, target matching, focus on event | ✅ |
| `ui_node_contains_point` / `ui_apply_hover_flag` | Hit test + flag management | ✅ |
| `ui_present_to_attached_host` | End frame + present + host_present | ✅ |
| Layout helpers | Math for column/row/split/inset/center calculations | ✅ Pure math |

### 6.3 What std::ui Does NOT Provide

- No widget library (Button, Slider, Checkbox, etc.) — that's Kaintana
- No layout engine (flexbox, grid) — only helper math functions
- No theme system
- No animation/tweening
- No text input widget (though IME/editable plumbing exists)
- No scroll view
- No window management (multiple windows per session not supported)

______________________________________________________________________

## 7. Gap Analysis

### 7.1 What Would Need to Change for the Renderer to Use the Node Tree

**Current state:** `win32_host_render_framebuffer` fills a hardcoded gradient and draws 3 hardcoded strings via GDI `DrawTextA`. It receives `session` but does not read from it.

**Gap 1 — No tree walker:** There is no function that iterates the node tree in paint order. You would need to write a recursive or iterative walker that:

- Finds the root node (parent_id == 0)
- Visits children in some order
- For each visible node (not hidden), computes its absolute position (parent-relative or absolute?)
- Issues draw calls based on node kind, style, and text

**Gap 2 — No style-to-pixel mapping:** Even if you read `session->styles`, the values are raw strings/i64s/f64s. There is no code that:

- Parses `"#FF0000"` into RGB bytes
- Maps `fill_color` to a `FillRect` or pixel loop
- Maps `border_color` to a border drawing routine
- Maps `font_size` to a `CreateFont` / `SelectObject` sequence
- Maps `corner_radius` to rounded rectangle drawing
- Maps `opacity` to alpha blending

**Gap 3 — No font management:** `abi_ui_font_create` stores a font family string and size, but the renderer uses GDI's default system font. There is no:

- `CreateFontA` call based on the stored font resource
- Font cache
- Text measurement using actual font metrics (the current `text_measure_width` uses a hardcoded 0.56 multiplier)

**Gap 4 — No draw command consumer:** The draw commands (`abi_ui_draw_rect/text/resource`) accumulate in `session->draw_commands` but no code reads them to produce pixels. A renderer would need to iterate draw commands and convert each to pixel operations.

**Gap 5 — No z-ordering:** The hit-test function scans nodes in reverse array order (last created = on top), but the renderer doesn't use the node tree at all. There is no explicit z-order field.

**Gap 6 — No damage tracking for partial updates:** The `dirty_count` and per-node `revision` fields exist, but the renderer redraws the entire framebuffer every frame. There is no damage-rect system.

**Gap 7 — No texture/compositing:** `abi_ui_texture_create` and `abi_ui_resource_set_bytes` store raw bytes, but the GDI renderer has no code to blit a texture to the framebuffer. The DIB section is a flat BGRA framebuffer — you'd need to write pixel-copy loops for each texture draw.

**Gap 8 — Double paint in current code:** The current `win32_host_render_framebuffer` fills every pixel twice (background then gradient), draws GDI text that bypasses the framebuffer, then triggers `BitBlt`. The gradient and text compete in undefined ways depending on `WM_PAINT` timing.

### 7.2 What Would Need to Change for System B to Connect to System A

**Current state:** System B (compiled bundles) and System A (retained-mode nodes) are completely separate.

**Gap:** There is no bridge that takes a `KainUiCompiledBundle` and creates corresponding `KainNativeUiNode` entries in a `KainNativeUiSession`. The compiled bundle has layout information (dock, split_ratio, tab groups) that has no equivalent in System A.

**To connect them, you would need:**

1. A function that iterates `KainUiCompiledNode` entries and calls `abi_ui_node_create` + `abi_ui_node_set_rect` + `abi_ui_node_set_style_*` for each
1. A layout solver that converts layout metadata (dock placement, split ratios, tab groups) into absolute rects
1. Event routing that translates System A's `KainNativeUiEvent` into System B's `KainUiRuntimeEvent` and back

### 7.3 Known Gaps (From window_proof/README.md + COMPONENT.MD)

| # | Issue | Root Cause | Fix Location |
|---|-------|-----------|-------------|
| 1 | Component methods from `{expr}` fail | Codegen doesn't auto-pass `_self` | `crates/sys-codegen/src/codegen_llvm/component.rs` |
| 2 | `pulse` runtime crash | Runtime bug in pulse subsystem | `runtime/native/src/core/pulse.c` |
| 3 | JSX `if` with operators rejected | Parser limitation | `crates/core/src/parser.rs` |
| 4 | JSX `for` with ranges rejected | Parser limitation | `crates/core/src/parser.rs` |
| 5 | **GDI backend ignores element styles** | **Renderer reads nothing from node tree** | **`runtime/native/src/ui/ui_host_adapter.c:win32_host_render_framebuffer`** |
| 6 | Ternary `? :` in JSX rejected | Parser limitation | `crates/core/src/parser.rs` |
| 7 | `native_ui_set_attr_string` drops unknown keys | 4-key allowlist in surface bridge | `runtime/native/src/ui/native_ui_surface.c` |
| 8 | Host adapter clipboard is stubbed | `(void)text; return 0;` | `runtime/native/src/ui/ui_host_adapter.c` |
| 9 | Text measurement uses hardcoded 0.56 multiplier | No actual font metrics | `runtime/native/src/ui/ui_system.c:abi_ui_text_measure_width` |
| 10 | System A and System B don't connect | Two parallel architectures | Architecture decision needed |

______________________________________________________________________

## 8. Research Doc Cross-Reference — Claims vs Code

This section cross-references the claims made in the research documents (`X:/research/component/`) and `X:/runtime/native/README.md` against what the actual C code on disk implements. Discrepancies are flagged.

### 8.1 WIRING_CONTRACT.md

**12 contracts specified. Status against actual code:**

| Contract | Claim | Code Reality | Match? |
|----------|-------|-------------|--------|
| 1 — Component signature | `void @Name_render(i64 session, i64 parent, props...)` | Matches `component.rs` — renders take session+parent+props | ✅ |
| 2 — Native element | `element_begin` → attrs → children → `element_end` | Matches `component.rs` codegen + `native_ui_surface.c` vtable | ✅ |
| 3 — Component call | Direct fn call `@Name_render(session, parent, props)` | Matches codegen | ✅ |
| 4 — Text interpolation | `{expr}` evaluates, passes to `element_set_text` | Matches codegen | ✅ |
| 5 — `for` loop | Runtime loop with index-based stable keys | Matches codegen (Phase 1 — no list reconciliation) | ✅ |
| 6 — `if`/`elif`/`else` | Branch + distinct stable key suffixes per arm | Matches codegen | ✅ |
| 7 — `<Fragment>` | No wrapper, children emit to parent | Matches codegen | ✅ |
| 8 — State persistence | `state_get_i64` returns 0 on first frame → store initial | Matches `native_ui_surface.c` state root pattern. **Known limitation:** if initial value IS 0, re-initializes every frame (noted in NEWSLETTER-2.md) | ⚠️ |
| 9 — World-Surface Frame Loop | Calls `surface->poll_event` in loop | **FALSE.** `blades/window_proof/README.md` confirms poll_event (offset 13) is **NOT emitted** by current codegen. The vtable slot is filled, but no caller exists | ❌ |
| 10 — Stable key format | `"ComponentName:element_path:parent_id:sibling_index"` | Matches codegen's `__kain_stable_key` helper | ✅ |
| 11 — JSX Attribute Mapping | Maps `background` → `fill_color`, `border` → `border_width`, etc. | Partially matches. f64/i64 attributes are forwarded through the vtable correctly. String attributes go through `native_ui_set_attr_string` which has a **4-key allowlist** (`fill_color`, `border_color`, `ink_color`, `title`). The contract lists 16 attributes — the allowlist only covers 4 of the string ones. f64 and i64 attributes bypass this filter | ⚠️ |
| 12 — Error handling | Surface resolution fails → `kain_runtime_panic` | Matches codegen's null-check after `kain_component_surface_resolve` | ✅ |

**Key discrepancy: Contract 9** claims `surface->poll_event` is called in the frame loop. It is not. The codegen emits `begin_frame` → render → `end_frame` → `present` → `should_close`. No `poll_event` call. This means input events from the Win32 message pump (`WM_KEYDOWN`, `WM_MOUSEMOVE`, etc.) are never translated into ui_system events and never reach component handlers.

### 8.2 IMPLEMENTATION_PLAN.md

| Claim | Code Reality | Match? |
|-------|-------------|--------|
| Phase 1.3: `native_ui_surface.c` lives in `blades/kaintana/` | **Moved.** Now at `runtime/native/src/ui/native_ui_surface.c`. MERGE_PLAN.md and NEWSLETTER-2.md document this move. The plan's decision point #1 recommended `blades/kaintana/` but the actual merge placed it in runtime | ⚠️ Location changed |
| Phase 1.4: Codegen emits `surface->element_begin()` etc. | **Done.** `component.rs` emits vtable calls with correct offsets | ✅ |
| Phase 1.5: World-surface frame loop auto-emitted | **Done.** Injected into `main()` via `pending_frame_loops` drain | ✅ |
| Phase 1.6: State persistence via `state_get_i64`/`state_set_i64` | **Done.** With the 0-sentinel limitation | ✅ |
| Phase 2: Layout resolver `native_ui_layout_resolve` | **NOT IMPLEMENTED.** No layout resolver exists anywhere. No code walks the node tree post-reconciliation to compute pixel rects from layout.\* style keys. The only rect-setting happens through `abi_ui_node_set_rect` called from Kain code | ❌ |
| Phase 2: JSX Attribute → Style Key Mapping table | **Partially implemented in codegen.** The codegen maps attribute names to vtable calls (f64/i64/string). But the C backend (`native_ui_set_attr_string`) only forwards 4 string keys. The plan listed 16 attributes; 12 are handled by i64/f64 slots correctly, 4 by the string allowlist. `direction` as string (`width="fill"`) → i64 mapping is NOT in the string path | ⚠️ |
| Phase 3.1: Event dispatch from frame loop | **NOT IMPLEMENTED.** The plan describes `surface->poll_event` → hit-test → dispatch to component handler. None of this exists. The frame loop doesn't call poll_event, and there is no event-to-component dispatch logic | ❌ |
| Phase 3.2: List reconciliation (`reconcile_list_begin/item/end`) | **NOT IMPLEMENTED.** No such functions exist in any C file. `for` loops create/destroy nodes every frame with index-based stable keys | ❌ |
| Step 1.7: Oracle verification of component_minimal | **Done.** `blades/window_proof/` has 3 Oracle-verified demos (minimal window, world state, dashboard) | ✅ |

**Key discrepancy:** The plan's Phase 2 (layout resolver) and Phase 3 (event dispatch + list reconciliation) are documented as future work but the plan reads as if they were part of the implementation. They were not. Only Phase 1 was implemented.

### 8.3 MERGE_PLAN.md

| Claim | Code Reality | Match? |
|-------|-------------|--------|
| Plumber A built `KainComponentSurface` trait with 15 slots | **Correct.** The header now has 17 slots (window_open and host_pump were added later per NEWSLETTER-2.md) | ✅ (with additions) |
| Plumber B emitted direct `abi_ui_*` calls instead of vtable calls | **Was true, now fixed.** `component.rs` emits `getelementptr` + `load` + indirect `call` through the vtable | ✅ Fixed |
| Fix: keep trait, rewrite codegen to call through vtable | **Done.** | ✅ |
| Vtable offsets: session_create=0, session_destroy=1, ..., should_close=14 | **Correct, now extended to 16** (window_open=15, host_pump=16 added). The merge plan listed 15 slots but the actual vtable has 17 | ⚠️ Extended |
| Surface param threaded as first arg to all render functions | **Done.** `@Name_render(%KainComponentSurface* %surface, i64 %session, i64 %parent, props...)` | ✅ |
| Plumber A's C files: ZERO changes | **Partially true.** `native_ui_surface.c` was moved from `blades/kaintana/` to `runtime/native/src/ui/`. Content changed: added `native_ui_session_create` with auto-attach, `native_ui_present` with host_present call, auto-registration constructor | ⚠️ Content changed |

### 8.4 README.md (Research Overview)

Minimal — just an index pointing to IMPLEMENTATION_PLAN.md and WIRING_CONTRACT.md. No disputed claims.

### 8.5 runtime/native/README.md

| Claim | Code Reality | Match? |
|-------|-------------|--------|
| `ui_host_adapter.c` — "Translates platform events into component updates" | **FALSE.** `win32_host_pump_messages` only handles `WM_QUIT` → `host->running = 0`. It does NOT translate `WM_KEYDOWN`, `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, or any other input messages into ui_system events. No `abi_ui_push_event` call exists in the host adapter. The clipboard functions are stubbed. | ❌ |
| `ui_hot_reload.c` — "Monitors compiled UI bundles for changes and triggers seamless component swaps" | **Partially true.** It monitors via file signature polling and shared memory IPC. The "seamless component swaps" work through `ui_runtime.c`'s reload functions which preserve state across bundle swaps. But this is System B (compiled bundles), not System A (retained-mode nodes) | ⚠️ |
| `ui_system.c` — "UI component system: component state management, invalidation tracking, focus routing, and event dispatch" | **True for data model.** All these features exist as data operations. But the README omits that **none of this data is consumed by the renderer** | ⚠️ Incomplete |
| `ui_runtime.c` — "UI runtime lifecycle: initialization, termination, top-level component tree management" | **True for System B.** But the README doesn't mention that System B is disconnected from System A | ⚠️ Incomplete |
| Service `ui.component` — "Component state, invalidation, focus" with status "available" | **True.** The service is registered and the APIs exist | ✅ |
| Service `ui.bundle` — "Compiled UI bundle loading" with status "available" | **True.** `ui_compiled_bundle.c` loads and validates JSON bundles | ✅ |

**Key discrepancy:** The README claims `ui_host_adapter.c` translates platform events into component updates. It does not. The host adapter is a pure window-creation + message-pump layer. The only "event translation" is `WM_CLOSE` → close flag. No keyboard, mouse, or other input events are translated.

______________________________________________________________________

## 9. Summary Verdict

The Kain native UI runtime is a **data-rich, render-poor** system. It has:

- **Excellent data infrastructure:** 112 functions, 7 independently-indexed object pools, cryptographic hash tables, occupancy bitmaps, ring buffers, hex decoders, stable-key reconciliation, retained-mode tree with dirty tracking
- **A working window creation path:** Win32 `RegisterClassA` + `CreateWindowExA` + GDI DIB section framebuffer
- **A working component surface bridge:** 17-slot vtable connecting Rust codegen to C runtime
- **A working state persistence system:** Component state survives across frames via hidden node state

**What it lacks:**

- **A renderer that reads the node tree** — the single biggest gap. Every style, text, and draw command is stored but invisible to pixels.
- **Integration between its two parallel subsystems** — retained-mode nodes and compiled bundles are separate worlds.
- **Real font/text measurement** — uses hardcoded multipliers instead of actual font metrics.
- **Functional clipboard** — stubbed out.
- **Any consumer for most stored data** — styles, accessibility, IME, drag-and-drop, menus, dialogs, shaders, textures are all stored but never used by any downstream code.

The foundation is solid. The plumbing is connected end-to-end for window creation and component state. The rendering is the missing piece.
