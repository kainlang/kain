# External Dependencies — Kain Native UI System

> Generated 2026-06-24. Traces every external API call, library, system interface,
> and environment variable consumed by the production UI source files.
> Test files (`test_ui/`, `test_ui_v2/`, `debug_ui/`, `widgets/`) excluded except
> where noted — this document covers the 11 production source files only.

---

## 1. Win32 API Calls

### `ui_host_adapter.c` — Win32 Window Substrate

| Function | Line(s) | Purpose |
|----------|---------|---------|
| `GetModuleHandleA` | 253, 278 | Get HINSTANCE for window class registration and window creation |
| `LoadCursorA` | 254 | Load standard arrow cursor (`IDC_ARROW`) |
| `RegisterClassA` | 247 | Register `"KainWin32UI"` window class |
| `GetLastError` | 247, 302 | Check for `ERROR_CLASS_ALREADY_EXISTS`, `ERROR_ALREADY_EXISTS` |
| `GetProcAddress` | 258 | Defensive runtime resolution of `SetProcessDpiAwarenessContext` (safe on Win 8.1+) |
| `CreateWindowExA` | 272 | Create the actual Win32 overlay window |
| `GetClientRect` | 284 | Query actual client area after DPI scaling |
| `GetDC` | 176, 293, 298 | Screen DC for DIB creation and DPI baseline |
| `ReleaseDC` | 178, 295, 316 | Release screen DC |
| `GetDeviceCaps` | 297 | `LOGPIXELSX` for initial DPI scale detection |
| `CreateCompatibleDC` | 299 | Permanent memory DC for DIB selection |
| `CreateDIBSection` | 177, 309 | Allocate DIB framebuffer (32-bit top-down `BI_RGB`) |
| `SelectObject` | 162, 181, 312 | Select DIB into memory DC |
| `UpdateWindow` | 318 | Force initial window paint |
| `InvalidateRect` | 354, 553 | Trigger `WM_PAINT` (from render path and `abi_ui_invalidate_window`) |
| `BeginPaint` | 199 | Begin `WM_PAINT` painting |
| `EndPaint` | 210 | End `WM_PAINT` painting |
| `BitBlt` | 205 | Blit DIB → window DC (`SRCCOPY`) |
| `DestroyWindow` | 146, 332 | Window teardown |
| `PostQuitMessage` | 150 | Exit message loop |
| `DefWindowProcA` | 141, 233 | Default message handling |
| `GetWindowLongPtrA` | 132, 136 | Retrieve `GWLP_USERDATA` (host pointer) |
| `SetWindowLongPtrA` | 138 | Store host pointer in `GWLP_USERDATA` |
| `PeekMessageA` | 361 | Non-blocking message pump (`PM_REMOVE`) |
| `TranslateMessage` | 364 | Keyboard message translation |
| `DispatchMessageA` | 365 | Dispatch messages to window proc |
| `MapVirtualKeyA` | 271 | VK code → character mapping |
| `CreateBitmap` | 160 | Temporary 1×1 bitmap for DIB swap at window resize |
| `DeleteObject` | 163, 326 | Delete old DIB section |
| `DeleteDC` | 329 | Delete memory DC |
| `IsWindow` | 333 | Guard against stale HWND before `DestroyWindow` |
| `SetWindowPos` | 226 | DPI-change handling (`WM_DPICHANGED`) |

### `ui_hot_reload.c` — Shared Memory IPC

| Function | Line(s) | Purpose |
|----------|---------|---------|
| `CreateFileMappingA` | 302 | Create named shared memory for hot-reload IPC |
| `OpenFileMappingA` | (implied) | Open existing shared memory (consumer side) |
| `MapViewOfFile` | 310 | Map shared memory into process address space |
| `UnmapViewOfFile` | 398 | Unmap shared memory on teardown |
| `CloseHandle` | 312, 401 | Close mapping handle |
| `GetTickCount64` | 145 | Monotonic millisecond clock for poll rate limiting |
| `GetFileAttributesExA` | 173 | File metadata (size + last-write-time) for change detection |
| `InterlockedCompareExchange` | 93 | SeqCst atomic load from volatile `int32_t` |
| `InterlockedExchange` | 106 | SeqCst atomic store to volatile `int32_t` |
| `InterlockedIncrement64` | 116 | SeqCst atomic increment of `int64_t` event sequence |

### `native_ui_surface.c` — Static Initialization

| Feature | Line(s) | Purpose |
|---------|---------|---------|
| `#pragma section(".CRT$XCU")` | 114–118 | MSVC CRT initializer — registers `native_ui` surface before `main()` |
| `__declspec(allocate(...))` | 118 | Allocate function pointer in CRT init section |
| `__attribute__((constructor))` | 124 | GCC/Clang equivalent |

---

## 2. GDI Dependencies

All GDI calls originate from `ui_host_adapter.c` (the `"winit"` backend).

| Function | Purpose |
|----------|---------|
| `GetDC(NULL)` | Get screen DC for DIB creation |
| `ReleaseDC(NULL, ...)` | Release screen DC |
| `CreateCompatibleDC(hdc)` | Create permanent memory DC for DIB |
| `CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, ...)` | Allocate 32-bit top-down DIB framebuffer |
| `SelectObject(hdc, hbitmap)` | Select DIB into memory DC |
| `DeleteObject(hbitmap)` | Free DIB section |
| `DeleteDC(hdc_buffer)` | Free memory DC |
| `BitBlt(hdc, ..., SRCCOPY)` | DIB → window blit during `WM_PAINT` |
| `BeginPaint(hwnd, &ps)` | Begin paint cycle |
| `EndPaint(hwnd, &ps)` | End paint cycle |
| `InvalidateRect(hwnd, NULL, FALSE)` | Trigger `WM_PAINT` |
| `GetDeviceCaps(hdc, LOGPIXELSX)` | DPI detection |
| `CreateBitmap(1, 1, 1, 1, NULL)` | Temporary bitmap for DIB swap during resize |

**The UI framebuffer is pure software** — the DIB is written pixel-by-pixel by `ui_renderer.c` using `uint32_t` writes with alpha blending. GDI only handles the final `BitBlt` to screen.

---

## 3. stb_truetype Font Rasterizer

> Single implementation via `#define STB_TRUETYPE_IMPLEMENTATION` in `ui_system.c:21`
> Header at `X:/runtime/native/extras/_stb-truetype/stb_truetype.h`

| Function | ui_system.c Line(s) | Purpose |
|----------|---------------------|---------|
| `stbtt_InitFont` | 2013, 2017 | Initialize TTF font from raw byte data (tries index 0, then `GetFontOffsetForIndex` for TTC) |
| `stbtt_GetFontOffsetForIndex` | 2015 | TTC (TrueType Collection) font offset discovery |
| `stbtt_ScaleForPixelHeight` | 2024 | Compute scale factor for target pixel height |
| `stbtt_GetFontVMetrics` | 2025 | Query ascent/descent/line-gap in design units, cached in `KainUiFontData` |
| `stbtt_GetCodepointHMetrics` | 2150, 2842 | Per-glyph horizontal advance width |
| `stbtt_GetCodepointBitmap` | 2831 | Rasterize glyph as alpha-mask bitmap (returns `unsigned char*`) |
| `stbtt_FreeBitmap` | 867, 2824, 2835 | Free glyph bitmap (cache eviction, resource teardown, cache miss retry) |

The stb_truetype header (`~12KB C/C++ header`) is the **only third-party C dependency** of the UI system. 21 Z3 proof packs verify the math in `stb_truetype.h`.

---

## 4. Core Runtime Dependencies

### From `ui_host_adapter.c`

| Function | Source File | Purpose |
|----------|-------------|---------|
| `kain_component_surface_resolve("vulkan"/"d3d12"/"webgpu")` | `component_surface.c` | Resolve GPU backend vtable for GPU rendering paths |
| `abi_input_session_create(name)` | `input_system.c:136` | Create companion input session for keyboard/mouse events |
| `abi_input_push_event(isid, "keyboard"/"pointer", ...)` | `input_system.c` | Push OS input events into universal input system |
| `abi_input_begin_frame(...)` | `input_system.c` | Begin input frame for this pump cycle |

### From `ui_system.c`

| Function | Source File | Purpose |
|----------|-------------|---------|
| `abi_ui_host_adapter_attach/pump/present/shutdown` | `ui_host_adapter.c` | Host backend lifecycle (called from `ui_system.c`) |
| `abi_ui_host_adapter_is_live_backend` | `ui_host_adapter.c` | Check whether backend creates an OS window |
| `kain_component_surface_register` | `component_surface.c` | Registration (via `native_ui_surface.c` CRT init) |

### From `ui_hot_reload.c`

| Function | Source File | Purpose |
|----------|-------------|---------|
| `kain_env_int(env_name, fallback)` | `core.c` or `os_system.c` | Read env var as integer |
| `kain_env_dup(env_name)` | same | Duplicate env var value (malloc) |
| `kain_env_free(ptr)` | same | Free env var buffer |
| `kain_ui_runtime_reload_init` | `ui_runtime.c` | Initialize reload report |
| `kain_ui_runtime_reload_from_path` | `ui_runtime.c` | Apply a new compiled bundle |

### From `native_ui_surface.c`

| Function | Source File | Purpose |
|----------|-------------|---------|
| `kain_component_surface_register` | `component_surface.c` | Register `"native_ui"` surface at startup |

### From `component_surface.c` (called transitively)

| Function | Source File | Purpose |
|----------|-------------|---------|
| `getenv("RENDERER_BACKEND")` | C standard library | Read GPU backend selection |
| `fprintf(stderr, ...)` / `fflush` / `abort()` | C standard library | Runtime panic on surface resolution failure |
| GPU shim `extern` functions | `vulkan_surface_shim.c`, `d3d12_surface_shim.c`, `webgpu_surface_shim.c` | GPU backend resolution |

---

## 5. Complete Include Chain

### `ui_system_internal.h` (the hub header)

```
ui_system_internal.h
├── "base.h"  ──→  <math.h>, <stdint.h>, <stdio.h>, <stdlib.h>, <string.h>, <time.h>
│                 Win32: <winsock2.h>, <windows.h>, <windowsx.h>, <ws2tcpip.h>, <gl/GL.h>
│                 POSIX: <errno.h>, <limits.h>, <pthread.h>, <arpa/inet.h>, <netdb.h>,
│                        <sys/socket.h>, <unistd.h>, <sys/types.h>, <strings.h>
│                 Defines: RC header, KainArray, KainMap, MessageQueue, ThreadArgs,
│                          POSIX→Win32 shims (kain_fopen_s, kain_dupenv_s, etc.)
│
├── "ui_system.h"  ──→  <stdint.h>
│                       4096 nodes, 8192 styles, 8192 draw commands, etc.
│
├── "component_surface.h"  ──→  "gpu_surface_extension.h" → <stdint.h>
│                               KainComponentSurface vtable, surface registry
│                               KainPlatformSurfaceHandle (HWND, X11, Wayland, Metal)
│
├── <stddef.h>
└── <stdint.h>
```

### Per-file include summary

| File | Includes |
|------|----------|
| `ui_system.c` | `ui_system_internal.h`, `ui_host_adapter.h`, `ui_font.h`, `<stddef.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<math.h>`, `stb_truetype.h` |
| `ui_host_adapter.c` | `ui_host_adapter.h`, `ui_system_internal.h`, `win32.h`, `ui_renderer.h`, `ui_layout.h`, `input_system.h`, `<stdio.h>`, `<string.h>`, `<windows.h>` |
| `ui_hot_reload.c` | `ui_hot_reload.h`, `<stdio.h>`, `<string.h>`, `<windows.h>`, `<errno.h>`, `<fcntl.h>`, `<sys/mman.h>`, `<sys/stat.h>`, `<time.h>`, `<unistd.h>` |
| `native_ui_surface.c` | `ui_system.h`, `component_surface.h`, `<stdlib.h>`, `<string.h>`, `<windows.h>` |
| `ui_renderer.c` | `ui_renderer.h`, `ui_color.h`, `ui_font.h`, `ui_system_internal.h`, `<string.h>`, `<math.h>` |
| `ui_layout.c` | `ui_layout.h`, `ui_system_internal.h`, `<string.h>`, `<stdlib.h>` |
| `ui_color.c` | `ui_color.h`, `<ctype.h>`, `<string.h>`, `<stdio.h>`, `<stdlib.h>` |
| `ui_runtime.c` | `ui_runtime.h`, `version.h` |
| `ui_compiled_bundle.c` | `ui_bundle.h` |

---

## 6. Library Link Requirements

### From Makefile (`LDFLAGS`)

| Library | Target | Used By |
|---------|--------|---------|
| `user32` | Win32 | All windowing calls: `CreateWindowExA`, `PeekMessageA`, `DispatchMessageA`, `DefWindowProcA`, `GetModuleHandleA`, `DestroyWindow`, `SetWindowPos`, `InvalidateRect`, `GetClientRect`, `MapVirtualKeyA`, `PostQuitMessage`, `LoadCursorA`, `RegisterClassA`, `GetWindowLongPtrA`, `SetWindowLongPtrA`, `BeginPaint`, `EndPaint` |
| `gdi32` | Win32 | All GDI calls: `CreateDIBSection`, `CreateCompatibleDC`, `BitBlt`, `SelectObject`, `DeleteObject`, `DeleteDC`, `GetDC`, `ReleaseDC`, `GetDeviceCaps`, `CreateBitmap` |
| `opengl32` | Win32 | Listed in LDFLAGS for GPU backend compatibility; not directly called by UI code |

### From `native_core_runtime.toml` (Windows only)

| Library | Purpose |
|---------|---------|
| `user32` | Windowing and messaging |
| `gdi32` | GDI rendering |
| `shell32` | Shell operations (indirect) |
| `ws2_32` | Sockets (indirect) |
| `winhttp` | HTTP (indirect) |
| `advapi32` | Registry/Security (indirect) |
| `ole32` | COM (indirect) |
| `winmm` | Multimedia timers (indirect) |

The UI system itself only directly requires **`user32`** and **`gdi32`**.

### POSIX (Linux/macOS — no production UI backend yet)

When built for POSIX, UI `"winit"` backend is unavailable. Passive backends (`"software"`, `"headless"`, `"memory"`) require no graphics libraries. Hot-reload uses `shm_open`, `mmap`, `munmap`, `shm_unlink`, `close`, `ftruncate`, `fstat`, `stat`, `clock_gettime` (from `librt` on older glibc).

---

## 7. Environment Variables

| Variable | Default | Defined In | Read In | Purpose |
|----------|---------|------------|---------|---------|
| `ABI_UI_BUNDLE` | (none) | `ui_bundle.h:15` | `ui_hot_reload.c` (via `kain_env_dup`) | Path to compiled UI bundle JSON |
| `ABI_UI_HOT_RELOAD_CHANNEL` | `kain-ui-reload.<sanitized_app_name>` | `ui_hot_reload.h:13` | `ui_hot_reload.c` | Named shared memory channel for IPC-reload |
| `ABI_UI_HOT_RELOAD_POLL_INTERVAL_MS` | `125` | `ui_hot_reload.h:12` | `ui_hot_reload.c` (via `kain_env_int`) | Poll interval for file/IPC change detection |
| `RENDERER_BACKEND` | (none) | — | `component_surface.c:118` | GPU backend selection (`vulkan`, `d3d12`, `webgpu`) |

---

## 8. Compile-Time Defines

| Define | File | Purpose |
|--------|------|---------|
| `WIN32_LEAN_AND_MEAN` | `base.h` (before `#include <windows.h>`) | Shrink Windows header to minimal set |
| `_CRT_SECURE_NO_WARNINGS` | `ui_color.c:1` | Suppress MSVC deprecation warnings for `sscanf`, `fopen` |
| `STB_TRUETYPE_IMPLEMENTATION` | `ui_system.c:21` | Expand stb_truetype into this translation unit |
| `KAIN_RUNTIME_HAS_VULKAN_LOADER` | build gate | Enable Vulkan surface shim |
| `KAIN_RUNTIME_HAS_D3D12` | build gate | Enable D3D12 surface shim |
| `KAIN_RUNTIME_HAS_WEBGPU` | build gate | Enable WebGPU surface shim |

---

## 9. Summary

```
                           Kain UI System
                              │
              ┌───────────────┼───────────────┐
              │               │               │
          Win32 API       GDI32 API     stb_truetype
        (user32.dll)    (gdi32.dll)   (single-header)
              │               │               │
  CreateWindowExA    CreateDIBSection    stbtt_InitFont
  PeekMessageA       BitBlt             stbtt_GetCodepointBitmap
  DefWindowProcA     SelectObject       stbtt_GetFontVMetrics
  GetProcAddress     DeleteObject       stbtt_ScaleForPixelHeight
  GetModuleHandleA   CreateCompatibleDC stbtt_FreeBitmap
  ...                GetDC/ReleaseDC    ...
              │               │
              └───────────────┴───────────────┐
                                              │
                                      Core Runtime
                                      (src/core/)
                                      │
                          ┌───────────┼───────────┐
                          │           │           │
                   input_system   component_   os_system
                   .c              surface.c   /win32.h
                          │           │           │
                   abi_input_   kain_       kain_env_int
                   session_     component_   kain_env_dup
                   create        surface_    kain_env_free
                   abi_input_   register/
                   push_event   resolve
                   abi_input_
                   begin_frame

  3rd-party libraries:   0 (stb_truetype is single-header, bundled)
  System libraries:      2 (user32, gdi32)
  Core runtime deps:     3 subsystems (input, surface, env)
  Env vars:              4 (ABI_UI_*, RENDERER_BACKEND)
  Build gates:           3 platform + 3 backend
  Platform backends:     3 passive (software/headless/memory)
                         1 GDI (winit/Win32, ~500 lines)
                         3 GPU delegates (vulkan/d3d12/webgpu via shims)
```
