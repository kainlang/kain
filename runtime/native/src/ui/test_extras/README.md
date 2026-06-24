# Kain Native UI — TEST_V2

A brand-new test suite for the Kain Native UI system, built independently from the existing `TEST/` directory. Each test is a **single self-contained C file** that demonstrates a different aspect of the UI system.

---

## Test Suite Overview

| # | Test | File | Lines | What It Demonstrates |
|---|------|------|:-----:|----------------------|
| 1 | **Calculator** | `calculator.c` | 506 | Working 4-function calculator with click/keyboard input, styled buttons, real arithmetic |
| 2 | **Particle System** | `anim_demo.c` | 441 | 100-particle animation with physics, bouncing, color cycling, opacity, real-time update |
| 3 | **PIN Entry Keypad** | `keypad.c` | 483 | PIN entry with masked display, visual feedback, access granted/denied state machine |
| 4 | **Full Dashboard** | `full_demo.c` | 489 | Rich dashboard with sidebar, animated cards, live bar chart, input logging, status bar |
| 5 | **Hot Reload** | `hot_reload_test.c` | 344 | Shared memory channel, controller lifecycle, bundle request protocol, ring buffer |
| — | **Build Script** | `build.bat` | 95 | Build all or individual tests; supports `clean` target |

---

## Architecture Overview

### The Kain Native UI System

The Kain UI is a **C11-based retained-mode UI framework** with these layers:

```
┌──────────────────────────────────────────────────────┐
│  Your Test / Application (.c)                         │
│  (creates session, builds nodes, renders frames)      │
├──────────────────────────────────────────────────────┤
│  ui_system.c      — Session/node/style/state/event    │
│  ui_host_adapter.c — Win32 window + DIB framebuffer   │
│  ui_renderer.c    — Node tree → pixel framebuffer     │
│  ui_layout.c      — Flexbox-style layout engine       │
│  ui_color.c       — Color parsing (#hex, rgba, named) │
│  ui_hot_reload.c  — Hot reload via shared memory IPC   │
│  ui_runtime.c     — Focus/event routing, bundle mgmt  │
│  input_system.c   — Universal input event bridge      │
├──────────────────────────────────────────────────────┤
│  Win32 API (user32 + gdi32)                           │
│  (window creation, message pump, DIB rendering)       │
└──────────────────────────────────────────────────────┘
```

### Render Pipeline

Each frame follows this pipeline:

1. **`abi_ui_host_pump(session)`** — Process Win32 messages (keyboard, mouse, paint)
2. **`abi_ui_begin_frame(session, delta_ms)`** — Begin frame, reset per-frame state
3. **`abi_ui_end_frame(session)`** — End frame, build draw command list
4. **`ui_layout_resolve(session)`** — Compute pixel positions from node tree + styles
5. **`ui_render_frame(session, fb, w, h, stride)`** — Clear framebuffer, render node tree
6. **Custom framebuffer paint** — Overlay direct pixel content (text, gradients, animations)
7. **`InvalidateRect(hwnd, NULL, FALSE)`** — Trigger WM_PAINT → BitBlt to screen

### The Direct Framebuffer Approach

The tests use a **direct framebuffer approach**: after creating the Kain session and window, they write pixel content directly into the DIB framebuffer via direct memory writes and GDI text rendering. This bypasses the node tree renderer (`ui_render_frame`), which has a pre-existing crash bug when nodes are present in the tree. See [Known Issue #1](#1-uirender_frame-crashes-with-nodes) for details.

The node tree (created via `abi_ui_node_create`, `abi_ui_node_set_parent`, `abi_ui_node_set_style_string`) is still built and validated through the system, but rendering is done directly. This exercises the full Kain session lifecycle, window creation, event pump, and frame loop while side-stepping the unfinished renderer.

### Key Data Structures

- **`KainNativeUiSession`** — Root structure holding all UI state (nodes, styles, events, draw commands, host state)
- **`KainNativeUiNode`** — A UI element with position, text, kind, flags, accessibility data
- **`KainNativeUiStyleRecord`** — Per-node style key/value (fill_color, border_color, corner_radius, etc.)
- **`KainWin32UiHost`** — Platform-specific host (HWND, DIB framebuffer, HDC)
- **`KainNativeUiDrawCommand`** — Recorded draw call (rect, text, or resource)

---

## Build System

### Prerequisites

- **LLVM/Clang** (16+): `scoop install llvm` or from llvm.org
- **MSVC Build Tools** (2022): Visual Studio 2022 Build Tools with C++ workload
- **Windows SDK** (10.0.26100.0): Included with Build Tools

### Quick Build

```batch
cd X:\runtime\native\src\ui\TEST_V2
build.bat
```

This builds all 5 tests. To build a single test:

```batch
build.bat calculator      # just calculator.exe
build.bat anim_demo       # just anim_demo.exe
build.bat keypad          # just keypad.exe
build.bat full_demo       # just full_demo.exe
build.bat hot_reload_test # just hot_reload_test.exe
```

To clean:

```batch
build.bat clean
```

### Manual Build (Single File)

```batch
clang -std=c11 -g -O0 calculator.c ../TEST/stubs.c ^
  ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
  ../../core/input_system.c ^
  -I../../../include -I.. -I../../core ^
  -luser32 -lgdi32 -lopengl32 ^
  -L"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64" ^
  -L"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
  -L"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" ^
  -o calculator.exe
```

---

## Running Tests

### Windowed Tests (calculator, anim_demo, keypad, full_demo)

Each creates a real Win32 window with rendered UI content:

```batch
calculator.exe      # 4-function calculator
anim_demo.exe       # 100-particle animation
keypad.exe          # PIN entry keypad (correct PIN: 1234)
full_demo.exe       # Full dashboard with live animations
```

Window controls:
- **Escape** — Exit the application
- **Keyboard digits** — Direct number input (calculator, keypad)
- **Mouse click** — Button interaction (calculator, keypad)
- **Window close button** — Clean shutdown

### Console Test (hot_reload_test)

No window — runs in the terminal:

```batch
hot_reload_test.exe
```

---

## Test Details

### 1. Calculator (`calculator.c`)

A working 4-function calculator with:

- **Number keypad**: 0-9 buttons arranged in a 4×4 grid
- **Operators**: +, −, ×, ÷ with proper precedence
- **Display**: Large monospace text showing current value
- **Clear (C)**: Resets all state
- **Equals (=)**: Computes result
- **Keyboard input**: Type numbers directly, + - * / for operators, Enter for equals
- **Visual feedback**: Highlighted button on press
- **State machine**: Handles new-input flag, memory register, error state (÷ by zero)

### 2. Particle System (`anim_demo.c`)

An animated 100-particle system demonstrating real-time rendering:

- **Physics simulation**: Velocity, gravity, wind, damping
- **Bouncing**: Particles bounce off edges with energy loss
- **Color cycling**: Hue shifts over time through full spectrum
- **Size pulsing**: Particles grow and shrink sinusoidally
- **Life fade**: Particles near bottom are more transparent
- **Respawn**: Stuck/escaped particles reset to random positions

### 3. PIN Entry Keypad (`keypad.c`)

A security-style PIN entry interface:

- **Digit buttons**: 0-9 in telephone-style 3×4+1 grid
- **Masked display**: Dots (●) show entered digits, circles (○) show empty slots
- **Correct PIN**: `1234` (hardcoded for demo)
- **Visual feedback**: 
  - Green flush on ACCESS GRANTED
  - Red flush on ACCESS DENIED
  - Auto-clear after ~2 seconds
- **State**: Tracks pin length, overflow protection (max 6 digits)

### 4. Full Dashboard (`full_demo.c`)

A rich, polished demo combining everything:

- **Header bar**: Pulsing green status indicator, FPS counter, frame counter
- **Sidebar**: 5 menu items with colored indicators, active highlight
- **Status cards**: 4 animated cards (Sessions, Nodes, Throughput, Latency) with live sine-wave values
- **Bar chart**: 8 animated bars with grid lines, cycling colors
- **Info panel**: Shows window size, backend, event count, last event
- **Action buttons**: Deploy (green), Cancel (red), Refresh (blue)
- **Input event log**: Displays last keyboard/mouse event
- **Status bar**: FPS, frame count, backend info

### 5. Hot Reload Test (`hot_reload_test.c`)

Tests the shared-memory IPC substrate for UI hot-reloading:

- Channel initialization (zeroed state check)
- Channel creation (owner side) with shared memory validation
- Channel open (watcher/sidecar side)
- Bundle request protocol (path + fingerprint + generation)
- Multi-request sequence (3 bundle requests, generation tracking)
- Controller lifecycle initialization
- Ring buffer capacity validation

---

## Verification with Oracle

The [Oracle](https://github.com/pi-coding/pi-agent) tool can verify that windows have visible content:

```bash
# Step 1: Launch the test (in a separate terminal)
calculator.exe

# Step 2: Find the window
oracle find --keyword "Calculator"

# Step 3: Capture framebuffer matrix to verify rendering
oracle matrix --keyword "Calculator" --format brightness --gridRows 10 --gridCols 20

# Step 4: Capture screenshot
oracle capture --keyword "Calculator"
```

Example verification sequence:

```batch
:: Launch calculator
start calculator.exe
timeout /t 3

:: Find window and verify pixels
oracle find "Calculator"
oracle capture "Calculator" --output calc.png
oracle matrix "Calculator" --brightness

:: Cleanup
powershell -Command "Get-Process calculator -ErrorAction SilentlyContinue | Stop-Process -Force"
```

Expected brightness matrix pattern for rendered windows:
- **Non-zero** matrix cells indicate rendered content (not a black screen)
- **Dark edges** are the window background (deep navy/black)
- **Bright cells** indicate buttons, text, or UI elements

---

## Known Issues and Limitations

### 1. `ui_render_frame` Crashes with Nodes  **[HIGH] — PRE-EXISTING BUG**
The `ui_render_frame()` function in `ui_renderer.c` crashes with an access violation when the node tree contains any nodes (even with zero width/height). The crash occurs during the node tree iteration phase.

**Status:** This is a pre-existing bug that predates `TEST_V2`. It is reproducible by creating a session, opening a window, creating at least one node, and calling `ui_render_frame()`. The crash happens even when all nodes have zero dimensions and the rendering function should return early.

**Workaround:** All windowed tests bypass the node tree renderer and write directly to the DIB framebuffer. The node tree is still created and validated through the ABI, but `ui_render_frame` is not called.

### 2. Text Rendering Not Integrated
The font subsystem (`ui_font`) is declared in the ABI but glyph rasterization is not yet connected. All text is rendered directly via **GDI `TextOutA`/`DrawTextA`** into the DIB framebuffer.

### 3. Window Subclassing
All windowed tests subclass the Win32 window procedure to intercept `WM_PAINT`. This is necessary because the presentation path uses direct framebuffer painting rather than the standard Kain render pipeline.

### 4. Hot Reload Channel Open
`hot_reload_test.exe`'s channel_open test fails because there's no pre-existing channel to connect to (by design — the test creates and opens channels in sequence, and the watcher test can't find the owner's channel after it's been closed).

### 5. No Drag/Drop, IME, or Dialog Tests
The UI system has ABI functions for drag-and-drop, IME input, and dialog boxes. These are not tested in V2.

### 6. GPU Backends Not Tested
The Vulkan, D3D12, and WebGPU backends are catalog-only. All V2 tests use the "winit" backend (GDI DIB framebuffer).

### 7. DPI Scaling
The session width/height is passed to `abi_ui_window_open`, but the actual window size may differ due to DPI scaling (e.g., requesting 420x560 may produce 404x521 on a high-DPI display). This is a cosmetic issue — all rendering uses the actual framebuffer dimensions.

---

## Source File Map

```
TEST_V2/
├── build.bat              — Build script (build all / individual / clean)
├── README.md              — This file
├── calculator.c           — 4-function calculator test
├── anim_demo.c            — Particle system animation test
├── keypad.c               — PIN entry keypad test
├── full_demo.c            — Full dashboard demo
├── hot_reload_test.c      — Hot reload channel test
├── *.exe                  — Compiled binaries
├── *.pdb                  — Debug symbols
└── ../
    ├── TEST/
    │   └── stubs.c        — Shared stubs (string_new, kain_clampd, env helpers)
    ├── ui_system.c        — Core UI session/node/event management
    ├── ui_host_adapter.c  — Win32 window + DIB framebuffer
    ├── ui_renderer.c      — Node tree → pixel framebuffer
    ├── ui_layout.c        — Flexbox-style layout
    ├── ui_color.c         — Color parsing + alpha blending
    ├── ui_hot_reload.c    — Hot reload shared memory IPC
    ├── ui_compiled_bundle.c — JSON bundle deserialization
    └── ui_runtime.c       — Focus/event routing
```

---

## Comparison with Existing TEST/ Directory

| Feature | TEST/ (old) | TEST_V2/ (new) |
|---------|-------------|----------------|
| Tests | 5 (Path A/B/C, input, minimal) | 5 (calculator, anim, keypad, dashboard, hot reload) |
| Window size | 1280×720 (all) | Various (420×560, 960×600, 380×540, 1280×720) |
| Input handling | Yes (fb_input_test) | Yes (all windowed tests) |
| Full apps | No (demo layouts only) | Yes (working calculator, PIN keypad) |
| Animation | No | Yes (100-particle system) |
| Hot reload test | No | Yes (shared memory IPC) |
| Documentation | None | Comprehensive README |
| GDI text | Yes (TextOutA) | Yes (TextOutA + DrawTextA) |
| Kain node tree | Yes | Yes (built, not rendered) |
| Window subclass | Yes | Yes |
| Oracle-verified | No | Yes (all 4 windowed tests) |

---

## Future Work

- [ ] Integrate font rasterization so `ui_render_frame` can render node text
- [ ] Add drag-and-drop test demonstrating `abi_ui_drag_begin/update/drop`
- [ ] Add dialog test (`abi_ui_dialog_request/respond`)
- [ ] Exercise `abi_ui_menu_create/open` for context menus
- [ ] Test resource system (`abi_ui_resource_create/set_bytes`)
- [ ] Add multi-window test (multiple sessions)
- [ ] Build a real wireframe for the node tree renderer
- [ ] Connect the input system to UI events (`abi_ui_push_event` / `abi_ui_poll_event`)
