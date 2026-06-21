# libkain-webgpu-abi — WebGPU ABI Shared Library

## Overview

`libkain-webgpu-abi.so` (Linux/macOS) / `libkain-webgpu-abi.dll` (Windows) is
the separately-linked WebGPU ABI library. It owns ALL actual wgpu-native
(or Dawn) calls: instance creation, adapter request, device creation,
surface creation, swapchain lifecycle, frame submission, and present.

The runtime shim (`webgpu_surface_shim.c`) dlopens this library on native
targets and calls the single entry point `kain_webgpu_abi_get_vtable()` to
obtain a filled `KainComponentSurface` vtable implementing all 18 surface
trait slots.

The WASM variant (`kain_webgpu_abi_wasm`) is statically linked — the browser
provides `navigator.gpu` natively so there is no dlopen.

## Entry Points

| Symbol | Purpose |
|--------|---------|
| `kain_webgpu_abi_get_vtable()` | Returns pointer to static `KainWebgpuAbiVtable` with filled vtable |
| `kain_webgpu_abi_init()` | Load wgpu-native library, resolve all PFNs (native) / no-op (WASM) |
| `kain_webgpu_abi_shutdown()` | Destroy all sessions, close loader handle (native) / no-op (WASM) |

## Architecture

```
webgpu_surface_shim.c (runtime contract)
    │ dlopen("libkain-webgpu-abi.so")                        (native)
    │ dlsym("kain_webgpu_abi_get_vtable")                    (native)
    │ static link against webgpu_abi_wasm.c                  (WASM)
    ▼
webgpu_abi.c (this library — implementation)
    │ dlopen("libwgpu_native.so" / "libdawn.so" / "wgpu_native.dll" / "dawn.dll")
    │ 30+ PFNs resolved via dlsym/GetProcAddress
    │ KainComponentSurface vtable filled with real WebGPU calls
    ▼
wgpu-native or Dawn (vendor driver)

webgpu_abi_wasm.c (this library — WASM path)
    │ emscripten_webgpu_get_device() / navigator.gpu
    │ No dynamic loader — browser provides the implementation
    ▼
Browser WebGPU implementation (Chrome / Firefox / Safari)
```

## Supported Platforms

| Target | Loader | Surface Source | Native Handle |
|--------|--------|----------------|---------------|
| Windows (native) | `wgpu_native.dll` → `dawn.dll` (fallback) | `WGPUSurfaceSourceWindowsHWND` | `HINSTANCE` + `HWND` |
| Linux (native) | `libwgpu_native.so` → `libdawn.so` (fallback) | `WGPUSurfaceSourceXlibWindow` | `Display*` + `Window` |
| Linux Wayland (native) | `libwgpu_native.so` → `libdawn.so` (fallback) | `WGPUSurfaceSourceWaylandSurface` | `wl_display*` + `wl_surface*` |
| macOS (native) | `libwgpu_native.dylib` → `libdawn.dylib` (fallback) | `WGPUSurfaceSourceMetalLayer` | `CAMetalLayer*` |
| WASM (browser) | **none** — browser provides WebGPU | `navigator.gpu` + canvas | DOM CSS selector (e.g. `"#canvas"`) |

## Critical Rules

- **NEVER** includes `<webgpu/webgpu.h>` or `<wgpu.h>`
- **NEVER** links the wgpu-native SDK at compile time
- All WGPU handle types are `uintptr_t` (from `webgpu_loader_subset.h`)
- All WGPU descriptor structs are built with raw `void*` and hand-laid
  layout (no struct definitions, no enum values — implementation-specific)
- The WASM path uses `#ifdef __wasm__` to compile in browser mode
- Native path uses `#ifndef __wasm__` to compile for desktop

## Vtable Slots (18 of 18 filled)

All 18 `KainComponentSurface` vtable slots are implemented:
`session_create`, `session_destroy`, `session_attach_platform`,
`element_begin`, `element_end`, `element_set_text`,
`element_set_attr_i64`, `element_set_attr_f64`, `element_set_attr_string`,
`state_get_i64`, `state_set_i64`, `begin_frame`, `end_frame`, `present`,
`poll_event`, `should_close`, `window_open`, `host_pump`.

The MVP does not exercise element-level rendering — element tree calls
return stable IDs without persistent state. The GPU presenter focuses on
swapchain + frame submission; UI composition is delegated to the
`native_ui_surface` reference backend or future UI backends.

## Build

### Native (Windows / Linux / macOS)

```powershell
bazel build //runtime/native/extras/webgpu-abi:kain_webgpu_abi --config=dev
```

```bash
bazel build //runtime/native/extras/webgpu-abi:kain_webgpu_abi --config=dev
```

Output: `bazel-bin/runtime/native/extras/webgpu-abi/libkain-webgpu-abi.so` (or `.dll`).

### WASM (browser)

```bash
bazel build //runtime/native/extras/webgpu-abi:kain_webgpu_abi_wasm --platforms=@platforms//os:wasm
```

The WASM binary is statically linked into the final `kain.wasm` artifact.
The shim's `#ifdef __wasm__` branch calls `kain_webgpu_abi_get_vtable()`
directly without dlopen.

## Implementation Sections

### `webgpu_abi.c` — Native Path (~870 lines)

| Section | Description |
|---------|-------------|
| 1: Dynamic loader | dlopen/LoadLibrary for `libwgpu_native` + `libdawn`, 30+ PFN typedefs and resolution |
| 2: Session table | `KainWebgpuSession` storage, allocator, lookup helpers |
| 3: Surface + swapchain | Per-platform WGPUSurface source layout, BGRA8Unorm + Fifo present |
| 4: Session lifecycle | `wgpuCreateInstance` → `wgpuInstanceRequestAdapter` → `wgpuAdapterRequestDevice` → `wgpuDeviceGetQueue` |
| 5: Element tree | No-op stubs (GPU presenter, not UI tree) |
| 6: Frame lifecycle | Command encoder → render pass with clear color → finish → submit + present |
| 7: Event pump | No-ops (platform host owns event loop) |
| 8: Init / shutdown | Loader setup, session teardown |
| 9: Static vtable | Global `g_webgpu_abi_vtable` + entry point |

### `webgpu_abi_wasm.c` — WASM Path (~385 lines)

| Section | Description |
|---------|-------------|
| 1: Session lifecycle | `emscripten_webgpu_get_device`, canvas surface from selector |
| 2: Element tree | No-op stubs (mirrors native) |
| 3: Frame lifecycle | Browser-driven via `requestAnimationFrame` |
| 4: Event pump | No-op (browser DOM event loop) |
| 5: Init / shutdown | No-op (browser-managed) |
| 6: Static vtable | Global `g_webgpu_abi_wasm_vtable` + entry point |

## WGPU Functions Resolved (Native Path)

`wgpuCreateInstance`, `wgpuInstanceRelease`, `wgpuInstanceCreateSurface`,
`wgpuInstanceProcessEvents`, `wgpuInstanceRequestAdapter`, `wgpuAdapterRelease`,
`wgpuAdapterRequestDevice`, `wgpuDeviceRelease`, `wgpuDeviceGetQueue`,
`wgpuDeviceCreateSwapChain`, `wgpuSwapChainRelease`,
`wgpuSwapChainGetCurrentTextureView`, `wgpuSwapChainPresent`,
`wgpuDeviceCreateCommandEncoder`, `wgpuCommandEncoderRelease`,
`wgpuCommandEncoderBeginRenderPass`, `wgpuRenderPassEncoderEnd`,
`wgpuRenderPassEncoderRelease`, `wgpuCommandEncoderFinish`,
`wgpuCommandBufferRelease`, `wgpuRenderPassEncoderClearColor`,
`wgpuQueueSubmit`, `wgpuQueueRelease`,
`wgpuDeviceCreateShaderModule`, `wgpuShaderModuleRelease`,
`wgpuDeviceCreateRenderPipeline`, `wgpuRenderPipelineRelease`,
`wgpuDeviceCreateBuffer`, `wgpuBufferRelease`,
`wgpuDeviceCreateBindGroupLayout`, `wgpuDeviceCreateBindGroup`,
`wgpuDeviceCreatePipelineLayout`.

## What the Shim Sees (Symmetric Vtable Shape)

Both the native and WASM paths produce a `KainWebgpuAbiVtable` with
identical layout:

```c
typedef struct KainWebgpuAbiVtable {
    KainComponentSurface surface;             // 18 function pointers
    int64_t              abi_version;        // KAIN_WEBGPU_ABI_VERSION = 1
    int64_t              present_count;      // telemetry: frames presented
    int64_t              last_status;        // telemetry: last call status
    char                 last_error[512];    // telemetry: last error message
} KainWebgpuAbiVtable;
```

The shim's `kain_webgpu_surface_shim_resolve()` calls
`kain_webgpu_abi_get_vtable()` and registers the resulting vtable as
both `"webgpu"` and `"webgpu_default"`. Compiler codegen resolves
the surface by name and calls through the vtable every frame.

## WASM Browser API Mapping

| Kain ABI | Browser API | Notes |
|----------|-------------|-------|
| `wgpuCreateInstance` | implicit | Browser owns the instance |
| `wgpuInstanceRequestAdapter` | `navigator.gpu.requestAdapter()` | Async, bridged via callback |
| `wgpuAdapterRequestDevice` | `adapter.requestDevice()` | Async, bridged via callback |
| `wgpuInstanceCreateSurface` | `canvas.getContext('webgpu')` | Canvas identified by CSS selector |
| `wgpuDeviceCreateSwapChain` | `context.configure(...)` | Browser-managed swapchain |
| `wgpuSwapChainGetCurrentTextureView` | `context.getCurrentTexture().createView()` | Per-frame |
| `wgpuSwapChainPresent` | implicit | rAF callback ends frame |
| `wgpuQueueSubmit` | implicit | Browser submits command buffers |

The WASM ABI library hides the async-bridging complexity behind the
synchronous KainComponentSurface trait. The shim and the compiler see
the same surface API regardless of target.

## Differences From Vulkan ABI

- **No `barrier_json` consumption** — WebGPU's `wgpuQueueSubmit` handles
  synchronization internally; the runtime's CUDA-style `barrier_json`
  passthrough is not used.
- **No `DispatchIndirect`** — WebGPU does not have indirect dispatch in
  the MVP; the runtime's `abi_gpu_dispatch_indirect` path is Vulkan/D3D12-only.
- **No WSI surface extension matrix** — wgpu-native uses opaque
  `WGPUSurfaceSource*` chained structs, not extension dispatch tables.
- **No manual frame-in-flight tracking** — WebGPU's swapchain present
  is browser/driver-managed; the explicit `MAX_FRAMES_IN_FLIGHT` ring
  buffer from Vulkan is not needed.
- **Element tree is no-op** — the GPU presenter doesn't consume element
  state. The `native_ui_surface` reference backend owns the UI tree.

## Known Limitations (MVP)

- Pipeline / shader / bind group creation is in the loader but not
  wired into the frame loop. The MVP renders a clear color only.
- `wgpuInstanceRequestAdapter` callback is bridged via a busy-wait loop
  (`webgpu_drain_events`). wgpu-native typically calls the callback
  synchronously so this is a no-op in practice.
- WGPU descriptor layouts (`WGPUSurfaceDescriptor`, `WGPUSwapChainDescriptor`,
  `WGPURenderPassDescriptor`) are hand-laid in raw byte buffers. If a
  wgpu-native version changes the struct layout, the surface / swapchain
  creation paths will need to be updated. This is the same risk the
  Vulkan ABI carries with its hardcoded `Vk*CreateInfo` sType values.
