#ifndef KAIN_D3D12_ABI_H
#define KAIN_D3D12_ABI_H

// ============================================================================
//  d3d12_abi.h — Public header for libkain-d3d12-abi.dll
// ============================================================================
//  This is the separately-linked D3D12 ABI library. It owns ALL actual
//  D3D12/DXGI driver calls: COM initialization (CreateDXGIFactory2,
//  D3D12CreateDevice), adapter selection, swapchain creation, render
//  target setup, command allocator/list, fence synchronization, and
//  frame submission (ExecuteCommandLists + Present).
//
//  The runtime shim (d3d12_surface_shim.c) LoadLibraryA's this library
//  and calls kain_d3d12_abi_get_vtable() to obtain a filled
//  KainComponentSurface vtable implementing all 18 surface trait slots.
//
//  Windows-only. The library loads d3d12.dll + dxgi.dll at runtime via
//  LoadLibraryA/GetProcAddress; it never requires the Windows SDK at
//  compile time (uses d3d12_loader_subset.h only for COM vtable layouts
//  and IID values).
// ============================================================================

#include "../../include/d3d12_loader_subset.h"
#include "../../include/component_surface.h"

#define KAIN_D3D12_ABI_VERSION 1
#define KAIN_D3D12_ABI_MAX_FRAMES_IN_FLIGHT 2
#define KAIN_D3D12_ABI_STATUS_MESSAGE_MAX 512

// ── Public vtable struct — MUST match d3d12_surface_shim.c exactly ──────────
//
// The shim's typedef (X:/runtime/native/src/core/d3d12_surface_shim.c):
//   typedef struct KainD3D12AbiVtable {
//       KainComponentSurface surface;
//       int64_t              abi_version;
//       int64_t              present_count;
//       int64_t              swapchain_recreations;
//       int64_t              last_status;
//       char                 last_error[KAIN_D3D12_STATUS_MESSAGE_MAX];
//   } KainD3D12AbiVtable;

typedef struct KainD3D12AbiVtable {
    KainComponentSurface surface;
    int64_t              abi_version;
    int64_t              present_count;
    int64_t              swapchain_recreations;
    int64_t              last_status;
    char                 last_error[KAIN_D3D12_ABI_STATUS_MESSAGE_MAX];
} KainD3D12AbiVtable;

// ── Per-session D3D12 state (lives in the library, not the shim) ──────────
//
// All handles are raw COM pointers (real Windows objects). Reference
// counting is via AddRef/Release on the vtable. The library tracks one
// KainD3d12Session per `session_create` call from the compiler.

#define KAIN_D3D12_MAX_SESSIONS 4
#define KAIN_D3D12_BACKBUFFER_COUNT 2

typedef struct KainD3d12Session {
    int64_t                    session_id;
    const char*                name;
    int64_t                    width;
    int64_t                    height;
    int64_t                    should_close;
    int                        platform_attached;
    int                        initialized;
    int                        first_present;     // tracks first-frame ClearColor initialization

    // ── D3D12/DXGI COM objects (raw pointers) ──
    IDXGIFactory2              dxgi_factory;
    IDXGIAdapter               adapter;
    ID3D12Device               device;
    ID3D12CommandQueue         command_queue;
    IDXGISwapChain3            swapchain;
    ID3D12Resource             render_targets[KAIN_D3D12_BACKBUFFER_COUNT];
    ID3D12DescriptorHeap       rtv_heap;
    uint64_t                   rtv_descriptor_size;
    ID3D12CommandAllocator     command_allocator;
    ID3D12GraphicsCommandList  command_list;
    ID3D12Fence                fence;
    uint64_t                   fence_value;
    HANDLE                     fence_event;

    // ── Frame state ──
    uint32_t                   frame_index;
    uint32_t                   back_buffer_index;

    // ── Platform window handle (Win32: HWND) ──
    HWND                       hwnd;
    void*                      hinstance;
} KainD3d12Session;

// ── The ONLY entry point exposed to the runtime shim ──────────────────────

const KainD3D12AbiVtable* kain_d3d12_abi_get_vtable(void);

// ── Optional: explicit init/shutdown for blade-level control ──────────────

int  kain_d3d12_abi_init(void);
void kain_d3d12_abi_shutdown(void);

#endif // KAIN_D3D12_ABI_H
