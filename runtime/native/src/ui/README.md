# Kain Native UI Runtime — System Architecture

## Overview

The Kain Native UI runtime is a **retained-mode, cross-platform UI system** built into the native C runtime substrate. It provides the full pipeline from compiled UI definition (JSON bundles) through node tree management, layout resolution, hit-testing, event routing, software/GPU rendering, OS window backends, and live hot-reload.

The system is designed as a **layered architecture** with four tiers:

```
  APPLICATIONS / KAITANA BLADES
       │  (KainComponentSurface vtable)
       ▼
  ┌─────────────────────────────────┐
  │  native_ui_surface.c            │  ← Ecosystem layer: surface adapter
  │  (KainComponentSurface impl)    │
  └──────────┬──────────────────────┘
             │  abi_ui_* API calls
             ▼
  ┌─────────────────────────────────┐
  │  ui_system.c + ui_system_internal.h │  ← Core retained-mode session
  │  (nodes, styles, state, events,     │
  │   resources, menus, dialogs, IME)   │
  └──┬────┬────┬────┬────┬────┬────┬───┘
     │    │    │    │    │    │    │
     ▼    ▼    ▼    ▼    ▼    ▼    ▼
  ui_    ui_   ui_  ui_  ui_  ui_  ui_
  layout render color bundle hot_  runtime
  .c    .c    .c   .c    reload .c
                          .c
     │    │         │
     ▼    ▼         ▼
  ┌─────────────────────────┐
  │  ui_host_adapter.c      │  ← OS window bridge
  │  (Win32 GDI, Vulkan,    │
  │   D3D12, WebGPU)        │
  └─────────────────────────┘
```

## Directory Map

| File | Role | Lines | Dependencies |
|------|------|-------|--------------|
| `ui_system_internal.h` | Internal data structures (all KainNativeUi types) | ~210 | base.h, ui_system.h, component_surface.h |
| `ui_system.c` | Core retained-mode session engine | ~2550 | ui_system_internal.h, ui_host_adapter.h |
| `ui_host_adapter.h` | Host adapter interface (window backends) | ~15 | ui_system_internal.h |
| `ui_host_adapter.c` | Host adapter implementations | ~500 | ui_host_adapter.h, win32.h, ui_renderer.h, ui_layout.h |
| `native_ui_surface.c` | KainComponentSurface vtable for native_ui | ~280 | ui_system.h, component_surface.h |
| `ui_color.c` | Color parsing, blending, opacity | ~220 | ui_color.h |
| `ui_compiled_bundle.c` | JSON compiled bundle deserializer | ~610 | ui_bundle.h |
| `ui_hot_reload.c` | Hot-reload IPC + bundle watching | ~650 | ui_hot_reload.h |
| `ui_layout.c` | Node tree layout engine | ~220 | ui_layout.h, ui_system_internal.h |
| `ui_renderer.c` | Software framebuffer renderer | ~325 | ui_renderer.h, ui_color.h, ui_system_internal.h |
| `ui_runtime.c` | High-level compiled-bundle runtime + event routing | ~1000 | ui_runtime.h, version.h |

Total: **~6580 lines** of C11, all in one analysis-friendly directory.

---

## 1. `ui_system_internal.h` — Internal Data Structures

**Purpose:** Defines all internal types that back the retained-mode UI session. Not exposed outside the UI subsystem.

### Key Types

| Type | Fields | Purpose |
|------|--------|---------|
| `KainNativeUiNode` | id, parent_id, child_count, flags, revision, first_child, next_sibling, layout_dirty, x, y, width, height, kind, text, stable_key (+hash), accessibility_role/label | A single retained-mode UI node (element). Children are stored as a sibling-linked list (`first_child` → `next_sibling` chain) for O(child_count) enumeration instead of O(MAX_NODES). Each node carries a `stable_key` for cross-frame reconciliation. |
| `KainNativeUiStyleRecord` | node_id, value_kind (i64/f64/string), values, key | A style key-value pair associated with a node. Looked up via hash-table index. |
| `KainNativeUiStateRecord` | Same shape as StyleRecord | Node-persistent state separate from styles (survives across frames for input fields, selections, etc.). |
| `KainNativeUiEvent` | kind, target_node_id, key_code, x, y, text | A single UI event (mouse, keyboard, pointer). Stored in a fixed-size ring buffer. |
| `KainNativeUiDrawCommand` | kind, node_id, rect, resource_id, font_resource_id, text, style_key | Explicit draw command recorded by `std::ui` helpers (draw_rect, draw_text, draw_resource). |
| `KainNativeUiResource` | id, width, height, byte_length, scalar_value, bytes, bytes_revision, resource_type, key, aux | A typed resource (font, texture, canvas, shader). Owns a heap-allocated byte array for raw data. |
| `KainNativeUiMenu` / `KainNativeUiMenuItem` | id, item_count, open, position / menu_id, command_id, key, label | Context menu system: menus contain ordered menu items. |
| `KainNativeUiDialog` | id, result, response_ready, kind, title, message, response_text | Modal dialog tracking (info/warning/file picker/etc.). |
| `KainNativeUiSession` | **The master session struct** (~60 fields) | Encapsulates one UI window/session: node, style, state, draw command, event, resource, menu, dialog arrays + occupancy bitmaps + hash index tables + host adapter state + per-frame arena. |

### Design Features

- **All fixed-size arrays**: `nodes[4096]`, `styles[8192]`, `state[8192]`, `draw_commands[8192]`, `events[1024]`, `resources[2048]`, `menus[256]`, `menu_items[2048]`, `dialogs[128]`. No malloc in the hot path.
- **Occupancy bitmaps**: Separate bitsets per array type for O(1) free-slot search using `count trailing zeros` / De Bruijn sequences.
- **Hash-index tables**: Open-addressing hash tables for node-by-id, stable-key-by-hash, style/state/resource/menu/dialog lookups. All use `mix_u64` → power-of-two masking → linear probing. Expected ~1.07 probes at typical load factors.
- **Per-frame arena**: 4KB bump allocator for zero-copy return strings. Resets every frame via `begin_frame`. Tagged pointers bypass RC guard.
- **Sibling-linked children**: `first_child` + `next_sibling` on nodes enables O(child_count) child enumeration instead of O(MAX_NODES) linear scan.
- All capacities are **power-of-two** (compile-time checked) for efficient masking.

---

## 2. `ui_system.c` — Core Retained-Mode Session

**Purpose:** The central engine. Manages session lifecycle, node CRUD, style/state storage, event queue, focus, hit-testing, resources, IME, drag-drop, menus, dialogs, clipboard, and hot-reload markers.

~~2550 lines, ~80 public functions. Largest file in the UI subsystem.

### Session Lifecycle

| Function | What It Does |
|----------|-------------|
| `abi_ui_session_create()` | Allocates a slot in the static `g_sessions[16]` array, initializes counters, sets host_backend to "memory". Returns session_id. |
| `abi_ui_session_destroy()` | Calls `abi_ui_release_session()` → host adapter shutdown → resource cleanup → zeroes the slot. |
| `abi_ui_reset()` | Tears down all active sessions and resets the global state. |
| `abi_ui_session_count()` | Returns count of active sessions. |

### Frame Lifecycle

| Function | What It Does |
|----------|-------------|
| `abi_ui_begin_frame()` | Increments frame_index, stores delta_ms, clears draw_commands, resets per-frame arena offset. |
| `abi_ui_end_frame()` | Returns draw_command_count. |
| `abi_ui_present()` | Marks frame as presented, clears dirty count. |
| `abi_ui_host_attach()` | Attaches an OS window backend ("software", "winit", "vulkan", "d3d12", "webgpu"). Delegates to `abi_ui_host_adapter_attach()`. |
| `abi_ui_host_pump()` | Delegates to host adapter for OS message pumping. Returns event count. |
| `abi_ui_host_present()` | Computes a hash over all draw commands (content fingerprint), then delegates to host adapter for actual presentation (framebuffer blit/swapchain present). |

### Node Management

| Function | What It Does |
|----------|-------------|
| `abi_ui_node_create()` | Finds free slot via occupancy bitmap, zeroes, sets id, flags (FOCUSABLE\|INTERACTIVE), kind, inserts into hash index. Returns node_id. |
| `abi_ui_node_destroy()` | Orphans children (sibling-list traversal → sets parent_id=0), unlinks from parent's sibling list, removes stable-key index entry incrementally, clears focus/IME/drag references, removes node index incrementally. |
| `abi_ui_node_set_parent()` | Unlinks from old parent's sibling list, links into new parent's sibling list (prepend), cycle-detects self-referencing. |
| `abi_ui_node_set_stable_key()` | Computes 64-bit FNV-1a hash once, stores both key and hash, inserts into index incrementally. |
| `abi_ui_node_find_by_stable_key()` | Hash-table lookup with 64-bit hash pre-check before strcmp (~99.9% of probes skip strcmp at 6.25% load). |

### Style & State Storage

Each node can have named style and state key-value records. The system uses a **hash-table index** (open-addressing) over compact arrays:

- `abi_ui_find_style(session, node_id, key)` — hash probe using `hash_node_key(node_id, key)`.
- `abi_ui_ensure_style(session, node_id, key)` — find or create, inserts into index.
- Same pattern for state via `abi_ui_find_state()` / `abi_ui_ensure_state()`.
- **Three value kinds**: `ABI_UI_STYLE_I64`, `ABI_UI_STYLE_F64`, `ABI_UI_STYLE_STRING`.

Getters return **tagged pointers** from the per-frame arena — zero-copy, no RC overhead.

### Focus & Hit-Testing

| Function | What It Does |
|----------|-------------|
| `abi_ui_focus()` | Sets focused_node_id, rejects disabled nodes. |
| `abi_ui_focused_node()` | Returns current focus. |
| `abi_ui_hit_test(x, y)` | Reverse scan (topmost first) for visible nodes containing the point. O(MAX_NODES). |

### Event Queue

Ring buffer of `KainNativeUiEvent` (1024 entries):

| Function | What It Does |
|----------|-------------|
| `abi_ui_push_event()` | Writes to tail, advances tail modulo power-of-two. Z3-verified capacity bound. |
| `abi_ui_poll_event()` | Reads from head into `active_event`, clears slot, advances head. Returns 1 if event available. |
| Accessors | `event_kind()`, `event_target()`, `event_x()`, `event_y()`, `event_key_code()`, `event_text()`. |

### Resources, Fonts, Textures

| Function | What It Does |
|----------|-------------|
| `abi_ui_resource_create()` | Allocates resource slot, sets type/key/dimensions. |
| `abi_ui_font_create()` | Creates a "font" resource with family name and point size (default 14.0). |
| `abi_ui_texture_create()` | Creates a "texture" resource with dimensions and pixel format string. |
| `abi_ui_canvas_create()` | Creates a "canvas" resource. |
| `abi_ui_shader_create()` | Creates a "shader" resource with stage metadata (vertex/fragment/compute). |
| `abi_ui_resource_set_bytes()` | Sets raw byte data on a resource (malloc + memcpy). |
| `abi_ui_resource_set_bytes_hex()` | Decodes hex string → bytes, then calls set_bytes. |

### Draw Commands

Explicit drawing primitives recorded per frame (cleared at `begin_frame`):

| Function | What It Does |
|----------|-------------|
| `abi_ui_draw_rect()` | Appends a "rect" draw command with style_key for color lookup. |
| `abi_ui_draw_text()` | Appends a "text" draw command with font reference + style_key. |
| `abi_ui_draw_resource()` | Appends a "resource" draw command referencing a texture/canvas. |

### IME (Input Method Editor)

| `abi_ui_ime_begin(node_id)` | Activates IME for a node, clears text. |
| `abi_ui_ime_commit_text()` | Stores composed IME text. |
| `abi_ui_ime_end()` | Deactivates IME. |

### Drag-and-Drop

| `abi_ui_drag_begin()` | Initiates drag from node, stores payload string + position. |
| `abi_ui_drag_update()` | Updates drag position + current drop target. |
| `abi_ui_drag_drop()` | Finalizes drag with target node. |

### Menus

| `abi_ui_menu_create(key)` | Creates a context menu. |
| `abi_ui_menu_add_item()` | Adds a labeled item with command_id to a menu. |
| `abi_ui_menu_open()` | Opens menu at (x, y). |

### Dialogs

| `abi_ui_dialog_request()` | Creates a modal dialog (info/warning/file etc.) with title + message. |
| `abi_ui_dialog_respond()` | Sets dialog result + response text, marks response ready. |
| `abi_ui_dialog_poll_response()` | Non-blocking poll for dialog completion. |

### Clipboard

In-memory clipboard with optional delegation to OS (via host adapter when live backend attached).

### Hot Reload Markers

`abi_ui_hot_reload_begin(key)` / `abi_ui_hot_reload_commit()` — tracks revision generation on the session.

### Hash Utilities

The file implements several cryptographic-quality hash primitives:

- **FNV-1a 64-bit** (`abi_ui_hash_text`, `abi_ui_hash_u64`, `abi_ui_hash_i64`, `abi_ui_hash_f64`) — used for stable keys and content fingerprinting.
- **Mix64** (`abi_ui_mix_u64`) — splitmix64-style mixing for hash table indexing.
- **Token16** (`abi_ui_token_from_text16()` + `abi_ui_token_match_bit()`) — branchless text-to-flag-bit matching using a mini-hash of the first 16 bytes. Used for flag names like "hidden", "focusable", "interactive", "disabled", "hovered", "pressed". Z3-proven branchless dispatch.
- **De Bruijn sequence** for O(1) `low_bit_index` from a uint64_t isolator.

---

## 3. `ui_host_adapter.h` — Host Adapter Interface

**Purpose:** Minimal header exposing 7 functions that bridge the retained-mode session to OS-level window backends.

### Functions

| Function | Purpose |
|----------|---------|
| `abi_ui_host_adapter_is_live_backend()` | Returns 1 if backend is "winit", "vulkan", "d3d12", or "webgpu". |
| `abi_ui_host_adapter_attach()` | Attaches a named backend to a session. |
| `abi_ui_host_adapter_pump()` | Pumps OS messages for a session. |
| `abi_ui_host_adapter_present()` | Triggers rendering + presentation for a session. |
| `abi_ui_host_adapter_shutdown()` | Tears down the host backend. |
| `abi_ui_host_adapter_clipboard_set/get_text()` | OS clipboard integration. |

---

## 4. `ui_host_adapter.c` — Host Adapter Implementation

**Purpose:** Implements all OS-specific window backends. Currently supports **Win32 GDI (winit)** for software rendering, plus delegation to GPU backends (Vulkan, D3D12, WebGPU) via the `KainComponentSurface` registry.

### Backends

| Backend ID | Implementation | What It Does |
|------------|---------------|--------------|
| `"software"` | Passive stub | Sets `host_attached=1`, stores "software" as backend name. No OS window. |
| `"headless"`, `"memory"` | Same as software | Aliases for passive mode. |
| `"winit"` | **Win32 GDI (full)** | Creates a real HWND with framebuffer (CreateDIBSection). Pumps messages via `PeekMessageA` / `DispatchMessageA`. Renders via `ui_layout_resolve()` + `ui_render_frame()` → `InvalidateRect` → `WM_PAINT` → `BitBlt`. Translates all input events (keyboard, mouse, wheel) into the universal input system (`abi_input_push_event()`). |
| `"vulkan"` | Delegation | Resolves `"vulkan"` via `kain_component_surface_resolve()`, creates a GPU session, stores the vtable. |
| `"d3d12"` | Delegation | Same pattern but for Direct3D 12. |
| `"webgpu"` | Delegation | Same pattern for WebGPU. |

### Win32 Implementation Details

- **Window class**: `"KainWin32UI"` with `CS_HREDRAW | CS_VREDRAW | CS_OWNDC`.
- **Framebuffer**: 32-bit top-down DIB section (`CreateDIBSection`), stride = width × 4.
- **Rendering pipeline** (`win32_host_render_framebuffer`):
  1. `ui_layout_resolve(session)` — compute node positions
  2. `ui_render_frame(session, framebuffer, ...)` — draw node tree to pixels
  3. `InvalidateRect(hwnd, NULL, FALSE)` — trigger WM_PAINT
- **WM_PAINT**: `BeginPaint` → `BitBlt` from the DIB to the window DC → `EndPaint`.
- **Input bridging**: Maps all WM_KEYDOWN/UP, WM_CHAR, WM_LBUTTONDOWN/UP, WM_RBUTTONDOWN/UP, WM_MBUTTONDOWN/UP, WM_MOUSEMOVE, WM_MOUSEWHEEL to the universal `abi_input_push_event()` API. Includes a `win32_vk_to_key_string()` lut for common VK codes → "Enter", "ArrowLeft", "F1"–"F12".

### Screenshot (code-level call graph for the Win32 path)

```
abi_ui_host_present()
  → session->component_surface->present()        // GPU: delegate to renderer
  → win32_host_render_framebuffer(host, session)  // GDI: CPU path
       → ui_layout_resolve(session)
       → ui_render_frame(session, fb, w, h, stride)
       → InvalidateRect(hwnd)
  → (later) WM_PAINT → BitBlt(dib → window DC)
```

---

## 5. `native_ui_surface.c` — KainComponentSurface Adapter

**Purpose:** Implements the `KainComponentSurface` vtable (the trait the Kain compiler calls through) by wrapping `abi_ui_*` functions. This is the **boundary between the compiler's component system and the native UI runtime**.

### Key Design

- **Registration**: Runs before `main()` via a CRT initializer (`#pragma section(".CRT$XCU")` on MSVC, `__attribute__((constructor))` on GCC/Clang). Registers as `"native_ui"`.
- **Reconciliation**: `element_begin()` uses `abi_ui_node_find_by_stable_key()` to find existing nodes across frames. If found, re-parents and returns existing. If not, creates a new node.
- **State persistence**: Component `state` fields are stored on a hidden `"__kain_state_root"` node (marked `"hidden"` flag so it's invisible to hit-testing, draw walk, and serialization). Keys are prefixed with component name.
- **Style dispatch**: `element_set_attr_string()` maps known keys (`"fill_color"`, `"border_color"`, `"ink_color"`, `"title"`) to `abi_ui_node_set_style_string()`. Unknown keys are silently ignored (future-proof).
- **Window creation**: `session_create()` calls `abi_ui_session_create()` (defaults to "memory" backend), then calls `abi_ui_host_attach(sid, "winit")` on Win32 to create a real OS window. Render intent comes from Kain source (`surface native_ui => Component`).
- **Present**: Calls both `abi_ui_present()` (counter update) and `abi_ui_host_present()` (blit → InvalidateRect → BitBlt).

### Vtable Slots (all 19 filled)

```
session_create, session_destroy,
element_begin, element_end, element_set_text,
element_set_attr_i64, element_set_attr_f64, element_set_attr_string,
state_get_i64, state_set_i64,
begin_frame, end_frame,
present, poll_event,
should_close, window_open, host_pump,
session_attach_platform, (get_gpu_extension — via base struct)
```

---

## 6. `ui_color.c` — Color Parsing & Blending

**Purpose:** Parse colors from multiple string formats into 0xAARRGGBB uint32_t packed pixels, blend source over destination, and apply opacity.

### Format Support

| Format | Examples |
|--------|----------|
| `#RGB` | `#F00` → `0xFFFF0000` (4-bit per channel, expanded: 0xF → 0xFF) |
| `#RRGGBB` | `#FF0000` → `0xFFFF0000` |
| `#RRGGBBAA` | `#FF000080` → `0x80FF0000` |
| `rgb(r,g,b)` | `rgb(255,0,0)` or `rgb(100%,0%,0%)` |
| `rgba(r,g,b,a)` | `rgba(255,0,0,0.5)` |
| Named | `"transparent"`, `"black"`, `"red"`, `"green"`, `"blue"`, `"yellow"`, `"cyan"`, `"magenta"`, `"gray"`, `"silver"`, `"maroon"`, `"purple"`, `"navy"`, `"teal"`, `"olive"`, `"lime"`, `"orange"`, `"pink"`, `"brown"`, `"gold"`, `"coral"`, `"salmon"`, `"turquoise"`, `"indigo"`, `"violet"`, `"tan"`, `"ivory"`, `"azure"`, `"lavender"`, `"khaki"`, `"crimson"`, `"chocolate"`, `"darkgray"`, `"lightgray"`, `"dimgray"`, `"slategray"` (~34 named) |

### Key Functions

| Function | Purpose |
|----------|---------|
| `ui_parse_color()` | Top-level dispatch: `#` → hex, `rgb` → function syntax, else → named. |
| `ui_color_r/g/b/a()` | Component extraction via bit shifts. |
| `ui_color_blend(src, dst)` | Straight alpha: `src OVER dst`. Uses Z3-proven `div255_fast()` (shift+add, ~5 cycles vs ~25 for DIV). |
| `ui_color_with_opacity()` | Multiplies alpha by a 0.0-1.0 factor. |

---

## 7. `ui_compiled_bundle.c` — JSON Bundle Deserializer

**Purpose:** Deserializes the Kain compiler's JSON-compiled UI tree (`KainUiCompiledBundle`) into structured C types for the runtime to consume.

### Input Format

The JSON follows a canonical schema:
```json
{
  "window_title": "...",
  "output": {
    "tree": {
      "root": 12345,
      "nodes": {
        "1": { "id": 1, "kind": { "Element": "panel" }, "children": [2, 3], ... },
        ...
      }
    }
  }
}
```

### Parsing Pipeline

1. **`kain_ui_compiled_bundle_load_from_json()`** — Entry point. Extracts `window_title`, `output.tree.root`, and iterates `output.tree.nodes` as a JSON object map.
2. **`kain_ui_parse_tree_node()`** — For each node entry: extracts `id`, `kind` (with inner tag like `Element`, `ComponentRef`), `props` (title, text, tag, scene), `layout` (kind, dock, split_ratio, resizable, persistent_layout_id, tab_group_id, tab_label, tab_order, tab_default_active, tab_closable), and counts `children` from the array.
3. **Parent resolution** — After all nodes are parsed, a second pass walks each node's `children` array and sets `parent_id` / `has_parent` on the referenced child nodes. Then computes `depth` via `kain_ui_compute_node_depth()`.

### Load Sources

| Function | Source |
|----------|--------|
| `kain_ui_compiled_bundle_load_from_json()` | In-memory JSON string |
| `kain_ui_compiled_bundle_load_from_path()` | File on disk |
| `kain_ui_compiled_bundle_load_from_env()` | Path from env var (default: `ABI_UI_BUNDLE`) |

### Validation

After loading, the bundle is validated:
- Must have `loaded == 1`.
- Must have `has_root_id == 1`.
- The root_id must match an existing node.

### Supported Node Kinds

| Kind Enum | Tag Label | Meaning |
|-----------|-----------|---------|
| `UNKNOWN` | "" | Unrecognized kind |
| `ELEMENT` | "element" | Generic HTML-like element |
| `COMPONENT_REF` | "component" | Reference to a Kain component |
| `TEXT` | "text" | Text block |
| `PANEL` | "panel" | Container panel |
| `INSPECTOR` | "inspector" | Property inspector |
| `GRAPH` | "graph" | Graph / visualization |
| `TIMELINE` | "timeline" | Timeline editor |
| `TABLE` | "table" | Data table |
| `TREE` | "tree" | Tree view |
| `VIEWPORT2D` | "viewport2d" | 2D rendering viewport |
| `VIEWPORT3D` | "viewport3d" | 3D rendering viewport |
| `OVERLAY` | "overlay" | Floating overlay panel |
| `SLOT` | "slot" | Layout slot placeholder |

---

## 8. `ui_hot_reload.c` — Hot-Reload System

**Purpose:** Enables live UI reloading without restarting the application. The system monitors a compiled bundle JSON file for changes and applies them to the runtime state. Also provides an **IPC channel** (shared memory) so external tools can trigger reloads.

### Architecture

The hot-reload system has two sides:

```
  EXTERNAL TOOL                RUNTIME PROCESS
  (editor, builder, CLI)       (running Kain app)
      │                              │
      │  ┌──────────────────┐        │
      │  │ Shared Memory    │        │
      │  │ Named:           │        │
      │  │ "kain-ui-reload. │        │
      │  │  <app_name>"     │        │
      │  │                  │        │
      │  │ request_generation│◄────  │  ← tool writes
      │  │ bundle_path      │        │     bundle_path
      │  │ fingerprint      │        │
      │  │                  │        │
      │  │ applied_generation──►──── │  → app reads
      │  │ last_status      │        │     generation
      │  │ events[]         │        │     bump
      │  └──────────────────┘        │
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `kain_ui_hot_reload_channel_create()` | Creates named shared memory (Win32: `CreateFileMappingA` / `OpenFileMappingA`, POSIX: `shm_open` + `mmap`). Initializes the `KainUiHotReloadSharedControl` header with magic/version. |
| `kain_ui_hot_reload_channel_open()` | Opens existing shared memory (consumer side). Validates magic + version. |
| `kain_ui_hot_reload_channel_close()` | Unmaps view, closes handle, optionally unlinks (POSIX). |
| `kain_ui_hot_reload_channel_request_bundle()` | Writes bundle_path, fingerprint, bumps request_generation. Pushes a REQUESTED event to the ring. |
| `kain_ui_hot_reload_controller_boot()` | Initializes controller. Reads `ABI_UI_BUNDLE` env for initial bundle path. Computes file signature (FNV-1a over path + size + mtime). Opens/create IPC channel named `"kain-ui-reload.<sanitized_app_name>"`. |
| `kain_ui_hot_reload_controller_apply_pending()` | **Main polling function.** Each call: (1) checks IPC channel for request_generation bump, (2) falls back to file-system watch on bundle_path if no IPC request. If a change is detected, calls `kain_ui_runtime_reload_from_path()` to apply the new bundle. On success/failure, updates IPC control with applied/failed generation + pushes APPLIED/REJECTED events. |

### File Signature

Detects changes using a hash of: file path + file size + last write time (Win32: `ftLastWriteTime`, POSIX: `st_mtim`). Z3-proven collision-resistant for practical UI workloads.

### Sanitized Channel Names

The channel name is derived from the app name: non-alphanumeric characters are replaced with `_`, and the result is prefixed as `"kain-ui-reload.<name>"`. On Win32: `Local\<name>`; on POSIX: `/name`.

---

## 9. `ui_layout.c` — Node Tree Layout Engine

**Purpose:** Walks the node tree and computes pixel positions (x, y, width, height) for every node based on style rules and parent-child relationships.

### Layout Algorithm

1. **Find root nodes** (parent_id == 0) and give them the session dimensions.
2. **`ui_layout_node()`** — recursive layout for each node:
   - Reads layout styles from hash-table:
     - `"layout.direction"` (i64): 0 = horizontal, 1 = vertical (default 1)
     - `"padding"` / `"padding.left"`, `.top`, `.right`, `.bottom` (f64)
     - `"spacing"` / `"gap"` (f64) — gap between children
     - `"width"` / `"height"` (f64) — explicit size override
   - Computes child rect: explicit sizes if set, otherwise equal share of available space.
   - **Horizontal layout**: children placed left-to-right, each gets equal share if no explicit width.
   - **Vertical layout**: children placed top-to-bottom, space split among auto-sized children.
   - Recurse into children.
   - **Clears `layout_dirty` flag** after computation — Z3-proven dirty-flag gating gives ~51× speedup on typical frames.

### Child Enumeration

- **Root nodes** (parent_id == 0): linear scan (typically 1–2 roots).
- **Non-root**: sibling-linked list traversal via `first_child` → `next_sibling` chain. Z3-proven ~4000× speedup over linear scan for deep trees.

### Style Key Reference

| Style Key | Type | Default | Meaning |
|-----------|------|---------|---------|
| `layout.direction` | i64 | 1 | 0=horizontal, 1=vertical |
| `padding` | f64 | -1 (unset) | Uniform padding (overrides per-side if ≥ 0) |
| `padding.left` | f64 | 0 | Left padding |
| `padding.top` | f64 | 0 | Top padding |
| `padding.right` | f64 | 0 | Right padding |
| `padding.bottom` | f64 | 0 | Bottom padding |
| `spacing` / `gap` | f64 | 0 | Gap between children |
| `width` | f64 | -1 (fill parent) | Explicit width |
| `height` | f64 | -1 (fill parent) | Explicit height |

---

## 10. `ui_renderer.c` — Software Framebuffer Renderer

**Purpose:** Walks the retained-mode node tree and renders every visible node into a caller-provided `uint32_t*` pixel buffer using the software rasterizer.

### Rendering Pipeline

1. **Framebuffer clear**: Fills the entire buffer with a dark background color (`0xFF1A1A24`). Uses 64-bit dual-pixel stores for 2× fewer store operations (~230K instead of 460K at 1280×720). Z3-proven equivalent to per-pixel fill.
2. **`ui_render_node()`** — recursive depth-first traversal per root node:
   - Skips nodes not `in_use` or with `ABI_UI_NODE_HIDDEN` flag (single-branch batch test, Z3-proven).
   - Resolves fill_color, border_color, ink_color, border_width, corner_radius, opacity via hash-table style lookups.
   - **Fill**: Calls `ui_parse_color()` → `ui_color_with_opacity()` → `ui_draw_fill_rect()` or `ui_draw_rounded_rect()`.
   - **Border**: Calls `ui_draw_border_rect()` if border_color + border_width > 0.
   - **Text**: Deferred — commented out with a `#if 0` awaiting font glyph rasterization.
   - **Children**: Sibling-linked list traversal (O(n) per node).
3. **Draw commands**: After the node tree, iterates `draw_commands[]` and processes `"rect"`, `"text"`, `"resource"` explicit draw commands by style-key-based color lookup.

### Drawing Primitives

| Primitive | Function | What It Does |
|-----------|----------|-------------|
| Fill rect | `ui_draw_fill_rect()` | Clamped rectangle fill with alpha blending (`ui_color_blend`). Z3-proven branchless edge clamp. |
| Border rect | `ui_draw_border_rect()` | Four edges drawn as thin fill rects, clamped to half-dimensions. |
| Rounded rect | `ui_draw_rounded_rect()` | Per-pixel corner test using `dx² + dy² ≤ r²` falloff, blending inside pixels. |

### Style Keys Consumed

| Style Key | Type | Purpose |
|-----------|------|---------|
| `fill_color` | string | Background fill color (any format accepted by `ui_parse_color`) |
| `border_color` | string | Border outline color |
| `ink_color` | string | Text color (reserved for future font integration) |
| `border_width` | f64 | Border thickness in pixels |
| `corner_radius` | f64 | Corner radius for rounded rects |
| `opacity` | f64 | Global opacity multiplier (0.0–1.0) |

---

## 11. `ui_runtime.c` — High-Level Compiled Bundle Runtime

**Purpose:** A higher-level abstraction layer over `KainUiCompiledBundle` that provides validation, component state tracking, focus routing, event routing, text editing, and hot-reload state transfer. This is the layer that `ui_hot_reload.c` drives.

### Core Concepts

- **`KainUiRuntimeState`** — Contains a loaded `KainUiCompiledBundle` plus an array of `KainUiRuntimeComponentState` (one per node) with runtime metadata (focusability, editability, role, dirty flags, value, cursor position, etc.).
- **`KainUiRuntimeComponentState`** — Extended per-node state with role detection, capability flags, value/text editing state, dirty tracking.
- **Kind profiles** — A static lookup table mapping each `KainUiCompiledNodeKind` to its runtime properties: role string, capability flags, focusable/editable defaults.
- **Capability flags** — Bitmask computed from the component tree: FOCUS_ROUTING, EVENT_ROUTING, EDITABLE_CONTROLS, OVERLAY_COMPAT, STATE_PERSISTENCE, etc.

### Key Functions

| Group | Function | Purpose |
|-------|----------|---------|
| Loading | `kain_ui_runtime_state_load_bundle()` | Loads a bundle, validates it, rebuilds component state array. |
| Loading | `kain_ui_runtime_state_load_from_json/path/env()` | Bundle loading from various sources. |
| Validation | `kain_ui_runtime_validate_bundle()` | Checks: bundle loaded, root present, no duplicate IDs, no orphan parents, window_title present. Produces diagnostic issues with severity levels. |
| Components | `kain_ui_runtime_find_component()` | Find by ID. |
| Components | `kain_ui_runtime_find_first_kind/focusable/editable()` | Search by type. |
| Focus | `kain_ui_runtime_request_focus()` | Set focus to a component. |
| Focus | `kain_ui_runtime_clear_focus()` | Clear focus + active edit. |
| Focus | `kain_ui_runtime_find_next_focusable_index()` | Tab-order traversal (wraps around). |
| Dirty | `kain_ui_runtime_mark_dirty()` | Mark component dirty with reason mask. |
| Event Routing | `kain_ui_runtime_route_event()` | Routes events through three handlers: focus → text → key-non-text. Returns `KainUiRuntimeEventResult`. |
| Event Routing | `kain_ui_runtime_route_focus_event()` | Handles FOCUS_REQUEST, FOCUS_NEXT/PREV, BLUR, POINTER_DOWN → focus transitions. |
| Event Routing | `kain_ui_runtime_route_text_event()` | Handles TEXT_INPUT (append), KEY_DOWN (backspace/delete/enter) on editable components. |
| Event Routing | `kain_ui_runtime_route_key_non_text_event()` | TAB → focus next/prev, EDIT_COMMIT/EDIT_CANCEL. |
| Reload | `kain_ui_runtime_reload_bundle()` | Reloads a bundle while preserving state: focus, active edit, hovered, values, dirty state. Transfers via `persistent_layout_id` or `id` matching. Produces detailed `KainUiRuntimeReloadReport`. |
| Reload | `kain_ui_runtime_reload_from_path()` | Loads bundle from file path then calls reload_bundle. |

### Capability Detection (Static)

The runtime scans the bundle and computes capabilities:

| Capability Flag | Set When |
|----------------|----------|
| `FOCUS_ROUTING` | Any focusable component exists |
| `EVENT_ROUTING` | Focusable or editable component exists |
| `EDITABLE_CONTROLS` | Any editable component exists |
| `OVERLAY_COMPAT` | Panel/Inspector/Viewport3D/Overlay kind exists |
| `STATE_PERSISTENCE` | Always set |

### Text Editing State

Editable components track:
- `value[]` — current text buffer
- `value_length` — strlen
- `cursor` — cursor position
- `revision` — bump counter
- `dirty` / `dirty_reason_mask`

Supports `append_text()` and `delete_last_char()`.

### Hot-Reload State Transfer

When a new bundle is applied via `kain_ui_runtime_reload_bundle()`:

1. Build a candidate state from the new bundle.
2. For each component in the candidate, look for a matching source component (by id or `persistent_layout_id`).
3. Transfer: value text, dirty state, revision counter.
4. Preserve: focused component, active edit component, hovered component.
5. Update sequence counter and dirty counts.

---

## Data Flow: Complete Frame

```
1. Kain compiler codegen loop:
   surface->begin_frame(session_id, delta_ms)
     → abi_ui_begin_frame()
       → frame_index++, arena reset, draw_commands = 0

2. For each component in the surface tree:
   surface->element_begin(parent_id, kind, stable_key)
     → abi_ui_node_find_by_stable_key() or abi_ui_node_create()
   surface->element_set_attr_string(key, value)     // per prop
   surface->element_set_attr_i64(key, value)
   surface->element_set_attr_f64(key, value)
   surface->element_set_text(text)
   → Recurse for children
   surface->element_end(element_id)                 // no-op

3. surface->state_set_i64(key, value)               // component state persistence
   surface->state_get_i64(key) → value              // on hidden "__kain_state_root" node

4. surface->end_frame(session_id)
     → abi_ui_end_frame()

5. surface->host_pump(session_id)
     → abi_ui_host_pump()
       → abi_ui_host_adapter_pump(session)
         → win32_host_pump_messages()
           → PeekMessage → TranslateMessage → DispatchMessage
           → Win32 messages → abi_input_push_event()
       → abi_ui_input_begin_frame()

6. surface->present(session_id)
     → abi_ui_present()                                // counter + dirty reset
     → abi_ui_host_present()                           // content fingerprint
       → abi_ui_host_adapter_present(session)
         → component_surface->present()                // GPU path
         → win32_host_render_framebuffer(host, session) // GDI path
           1. ui_layout_resolve(session)
           2. ui_render_frame(session, fb, w, h, stride)
           3. InvalidateRect(hwnd)

7. (async) WM_PAINT → BitBlt(dib → hdc)

8. surface->poll_event(session_id, ...)
     → abi_ui_poll_event()                            // drain events
```

## Z3 Proof Coverage

Several critical paths have Z3-verified proofs:

| Proof | Location | What It Proves |
|-------|----------|---------------|
| `ui-branchless-alpha-blend.smt2` | ui_color.c | `div255_fast(v) ≡ v/255` for v in [0, 130050] |
| `ui-branchless-clamp.smt2` | ui_renderer.c | Branchless clamp(x, lo, hi) is correct |
| `ui-branchless-flag-batch.smt2` | ui_renderer.c | Single branch for flag test = 4 separate branches |
| `ui-framebuffer-simd-fill.smt2` | ui_renderer.c | 64-bit dual-pixel fill = per-pixel fill |
| `ui-child-enumeration-worst-case.smt2` | ui_layout.c | Sibling-linked list is 4000× faster than linear scan |
| `ui-dirty-flag-layout-cache.smt2` | ui_layout.c | Dirty-flag gating gives 51× speedup |
| `ui-stable-key-collision-probability.smt2` | ui_system.c | 64-bit FNV-1a + mix_u64 is uniform |
| `ui-incremental-index-update.smt2` | ui_system.c | Incremental open-addressing insert/remove is safe |
| `ui-index-start-slot-u64-mask-bounds` | ui_system.c | `hash & mask` ≤ mask for power-of-two-minus-one |
| `ui-renderer-fill-color-double-parse.smt2` | ui_renderer.c | No double-parse of fill_color |
| `tagged-immediate-lowbits-defeat-heap-rc-guard.smt2` | ui_system.c | Tagged pointers (bit 0 set) safely bypass RC release |
| `per-frame-arena-vs-malloc.smt2` | ui_system.c | Arena is 25–30× cheaper than RC alloc |
| `ui_push_event_event_count_bounded` | ui_system.c | event_count never exceeds MAX_EVENTS |
| `ui_append_draw_command_count_bounded` | ui_system.c | draw_command_count never exceeds MAX_DRAW_COMMANDS |

## Key Design Decisions

1. **Retained mode, not immediate mode**: The node tree persists across frames. Nodes are created once (or found by stable key for reconciliation) and updated in place. This enables the compiler's component system to emit declarative frame code without rebuilding the tree every frame.

2. **All fixed-size arrays, no hot-path malloc**: Every system array is dimensioned at compile time with power-of-two capacities. Occupancy bitmaps + De Bruijn sequences give O(1) free-slot search. The only heap allocation is for resource byte data.

3. **Hash-indexed lookups**: Instead of linear scans over arrays (which would be O(4096) for node lookups), every system uses open-addressing hash tables with 64-bit mixing. Expected ~1.07 probes per lookup.

4. **Sibling-linked children**: Rather than storing children as a contiguous array (which requires reallocation on insertion/removal), each node has `first_child` and `next_sibling` fields forming a linked list. This makes reparenting O(1).

5. **Per-frame arena for return strings**: The MCP/LLVM bridge returns `const char*` strings to the Kain compiler. Instead of RC-allocating every string (which would be 25-30× more expensive), strings are copied into a 4KB arena per session that is reset every frame. Tagged pointers (bit 0 set) bypass RC release.

6. **Three-tier architecture**: The compiler talks to `KainComponentSurface` (vtable in `component_surface.h`). The `native_ui` surface wraps `abi_ui_*` calls. The `abi_ui_*` layer manages the retained-mode session. The host adapter provides OS backends. This allows swapping the entire UI backend (native_ui, web, headless, Vulkan, D3D12) without changing the compiler.

7. **Hot reload via shared memory**: External tools can trigger UI reloads by writing to a named shared memory segment. The runtime detects the change during `controller_apply_pending()` (called from the frame loop) and applies the new bundle with maximum state preservation (focus, edit, values, dirty state).

## Environment Variables

| Variable | Default | Used By | Purpose |
|----------|---------|---------|---------|
| `ABI_UI_BUNDLE` | — | ui_compiled_bundle, ui_hot_reload | Path to compiled bundle JSON |
| `ABI_UI_HOT_RELOAD_CHANNEL` | `kain-ui-reload.<app>` | ui_hot_reload | Named shared memory channel for IPC reload |
| `ABI_UI_HOT_RELOAD_POLL_INTERVAL_MS` | 125 | ui_hot_reload | Poll interval for file/ipc change detection |
| `RENDERER_BACKEND` | — | n/a (component_surface registry) | Selects GPU backend (vulkan/d3d12/webgpu) |

## Extending the UI System

### Adding a new host backend

1. Add a `"mybackend"` string case to `abi_ui_host_adapter_attach()` in `ui_host_adapter.c`.
2. Implement `session_create`, `pump`, `present`, `shutdown`, `clipboard_*`.
3. Optionally register as a `KainComponentSurface` via `kain_component_surface_register()`.

### Adding a new node kind

1. Add to `KainUiCompiledNodeKind` enum in `ui_bundle.h`.
2. Add tag string to `KIND_TAGS[]` in `ui_compiled_bundle.c`.
3. Add to `g_kain_ui_kind_profiles[]` in `ui_runtime.c`.
4. Handle rendering in `ui_render_node()` in `ui_renderer.c` (if needed).

### Adding a new style key

1. Add style key name as a string constant.
2. Set via `abi_ui_node_set_style_*()` or `element_set_attr_*()`.
3. Read in `ui_layout.c` (for layout-related) or `ui_renderer.c` (for rendering-related).

---

*Generated from source analysis. Total UI subsystem: ~6580 lines across 12 files in `X:\runtime\native\src\ui\` plus public headers in `X:\runtime\native\include\`.*
