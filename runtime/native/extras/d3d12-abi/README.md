# libkain-d3d12-abi — Direct3D 12 ABI Shared Library

## Overview

`libkain-d3d12-abi.dll` is the separately-linked Direct3D 12 ABI library for
Windows. It owns ALL actual D3D12/DXGI driver calls: COM initialization
(`CreateDXGIFactory2`, `D3D12CreateDevice`), adapter selection, swapchain
creation, render target setup, command allocator/list, fence synchronization,
and frame submission (`ExecuteCommandLists` + `Present`).

The runtime shim (`d3d12_surface_shim.c`) `LoadLibraryA`'s this library and
calls the single entry point `kain_d3d12_abi_get_vtable()` to obtain a filled
`KainComponentSurface` vtable implementing all 18 surface trait slots.

## Entry Points

| Symbol | Purpose |
|--------|---------|
| `kain_d3d12_abi_get_vtable()` | Returns pointer to static `KainD3d12AbiVtable` with filled vtable |
| `kain_d3d12_abi_init()` | Load `d3d12.dll` + `dxgi.dll`, initialize session table |
| `kain_d3d12_abi_shutdown()` | Destroy all sessions, release library handles |

## Architecture

```
d3d12_surface_shim.c (runtime contract, in //runtime/native/src/core)
    │ LoadLibraryA("libkain-d3d12-abi.dll")
    │ GetProcAddress("kain_d3d12_abi_get_vtable")
    ▼
d3d12_abi.c (this library — implementation)
    │ LoadLibraryA("d3d12.dll")
    │ LoadLibraryA("dxgi.dll")
    │ GetProcAddress("D3D12CreateDevice", "CreateDXGIFactory2")
    │ COM vtable dispatch (CreateCommandQueue, CreateFence, Present, ...)
    ▼
D3D12 driver (vendor D3D12 runtime)
```

## Platform Support

| Platform | Status |
|----------|--------|
| Windows x64 | Full support |
| Windows x86 | Full support |
| Windows ARM64 | Full support (Xbox + Windows-on-ARM use this backend) |
| Linux / macOS | N/A — `target_compatible_with = ["@platforms//os:windows"]` |

The D3D12 backend is **mandatory** for:
- **Xbox** — only graphics API available.
- **Windows on ARM** — Qualcomm Adreno drivers ship D3D12; Vulkan often missing.
- **Game Pass / Microsoft Store certification**.
- **Enterprise IT** — many corporate Windows images lack Vulkan runtime.

## COM Vtable Dispatch

D3D12/DXGI are COM-based. The library dispatches every call through the
vtable pointer at offset 0 of the COM object. Slot indices match the
Windows SDK layout exactly:

| Interface | Slots | Methods Used |
|-----------|-------|--------------|
| `IDXGIFactory2` | 18 | `EnumAdapters` (7), `CreateSwapChainForHwnd` (16) |
| `ID3D12Device` | 50 | `CreateCommandQueue` (8), `CreateCommandAllocator` (9), `CreateCommandList` (12), `CreateDescriptorHeap` (14), `CreateRenderTargetView` (20), `CreateFence` (30), `GetDescriptorHandleIncrementSize` (15) |
| `ID3D12CommandQueue` | 12 | `ExecuteCommandLists` (9), `Signal` (10) |
| `ID3D12CommandAllocator` | 8 | `Reset` (7) |
| `ID3D12GraphicsCommandList` | 60 | `Close` (7), `Reset` (8), `ClearRenderTargetView` (40) |
| `ID3D12Fence` | 10 | `GetCompletedValue` (7), `SetEventOnCompletion` (8), `Signal` (9) |
| `IDXGISwapChain3` | 28 | `Present` (7), `GetBuffer` (8) |

All slot indices are defined in `d3d12_loader_subset.h` (the runtime's
pure-typedef header). The library uses these via the `KAIN_COM_CALL` macro
which performs a typed vtable dispatch.

## Frame Loop

```
session_create(name, w, h)
  → LoadLibraryA("d3d12.dll" + "dxgi.dll")
  → CreateDXGIFactory2 → IDXGIAdapter → D3D12CreateDevice
  → device->CreateCommandQueue(DIRECT)
  → device->CreateFence(0) + CreateEventA()
  → CreateWindowExA (HWND for swapchain)
  → factory->CreateSwapChainForHwnd(queue, hwnd, FLIP_DISCARD, R8G8B8A8, 2 buffers)
  → device->CreateDescriptorHeap(RTV, 2 descriptors)
  → swapchain->GetBuffer(0, 1) + device->CreateRenderTargetView × 2
  → device->CreateCommandAllocator(DIRECT) + device->CreateCommandList(0, DIRECT, allocator, NULL)

begin_frame(sid, dt)
  → Wait for previous frame's fence
  → allocator->Reset() + list->Reset(allocator, NULL)
  → list->ClearRenderTargetView(rtv_handle, dark_color)

end_frame(sid)
  → list->Close()

present(sid)
  → queue->ExecuteCommandLists(1, &[list])
  → swapchain->Present(1, 0)   // vsync on
  → queue->Signal(fence, ++fence_value)
```

## Critical Rules

- **NEVER** includes `<d3d12.h>` or `<dxgi.h>` — COM vtable layouts in
  `d3d12_loader_subset.h` only.
- **NEVER** links the D3D12 SDK at compile time.
- All D3D12/DXGI types are `uintptr_t` or hand-rolled structs.
- All IID values are real Microsoft GUIDs (defined in `d3d12_loader_subset.h`).
- D3D12.dll and dxgi.dll are loaded at runtime via `LoadLibraryA`.

## Vtable Slots (18 of 18 filled)

All 18 `KainComponentSurface` vtable slots are implemented:
`session_create`, `session_destroy`, `session_attach_platform`,
`element_begin`, `element_end`, `element_set_text`,
`element_set_attr_i64`, `element_set_attr_f64`, `element_set_attr_string`,
`state_get_i64`, `state_set_i64`, `begin_frame`, `end_frame`, `present`,
`poll_event`, `should_close`, `window_open`, `host_pump`.

The 8 `element_*`/`state_*` slots are no-ops for the D3D12 backend
(those are retained-tree semantics that live in `native_ui`, not the
GPU presenter). The 3 frame slots + 4 lifecycle slots are fully wired.

## Build

```powershell
bazel build //runtime/extras/d3d12-abi:kain_d3d12_abi --config=dev
```

Output: `bazel-bin/runtime/extras/d3d12-abi/libkain-d3d12-abi.dll`

## Implementation Sections (~1,014 lines)

| Section | Lines | Description |
|---------|-------|-------------|
| 1: COM vtable helpers | ~140 | KAIN_COM_CALL macro + typed wrappers for every method used |
| 2: Dynamic loader | ~50 | LoadLibraryA(d3d12.dll + dxgi.dll) + GetProcAddress |
| 3: Telemetry + session table | ~85 | g_present_count, g_last_status, g_sessions[4] |
| 4: Device + queue + fence | ~80 | CreateDXGIFactory2 → D3D12CreateDevice → CreateCommandQueue → CreateFence + CreateEventA |
| 5: Swapchain + RTVs | ~95 | CreateSwapChainForHwnd + RTV heap + 2 render target views |
| 6: Command list | ~50 | CreateCommandAllocator + CreateCommandList |
| 7: Frame submission | ~95 | begin_frame/end_frame/present with fence sync |
| 8: Window class | ~60 | RegisterClassExA + CreateWindowExA + WndProc |
| 9: Vtable fill (18 slots) | ~220 | All 18 KainComponentSurface implementations |
| 10: Static vtable + entry | ~70 | Global vtable, get_vtable(), init/shutdown, DllMain |
