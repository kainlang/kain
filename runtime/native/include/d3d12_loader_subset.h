#ifndef KAIN_D3D12_LOADER_SUBSET_H
#define KAIN_D3D12_LOADER_SUBSET_H

// ============================================================================
//  d3d12_loader_subset.h — Pure-declaration header for D3D12.
// ============================================================================
//  D3D12 uses COM (IUnknown-style vtable dispatch), not PFN resolution.
//  This header defines COM interface handle types and the few PFN prototypes
//  needed to bootstrap. All types are uintptr_t — no Windows SDK needed.
//
//  Never includes <d3d12.h> or <dxgi.h>. The actual COM vtable layouts
//  are only needed in the separately-linked ABI library (d3d12-abi/).
// ============================================================================

#include <stdint.h>

// ── COM HRESULT ──────────────────────────────────────────────────

typedef int32_t HRESULT;

// ── D3D12/DXGI handle types (all uintptr_t) ──────────────────────

typedef uintptr_t IDXGIFactory2;
typedef uintptr_t IDXGIAdapter;
typedef uintptr_t ID3D12Device;
typedef uintptr_t ID3D12CommandQueue;
typedef uintptr_t IDXGISwapChain3;
typedef uintptr_t ID3D12Resource;
typedef uintptr_t ID3D12DescriptorHeap;
typedef uintptr_t ID3D12CommandAllocator;
typedef uintptr_t ID3D12GraphicsCommandList;
typedef uintptr_t ID3D12Fence;
typedef uintptr_t HANDLE;
typedef uintptr_t HWND;

// ── DXGI PFN prototypes ──────────────────────────────────────────

HRESULT CreateDXGIFactory2(uint32_t Flags, uintptr_t riid,
                           void** ppFactory);
HRESULT D3D12CreateDevice(uintptr_t pAdapter,
                          uint32_t MinimumFeatureLevel,
                          uintptr_t riid, void** ppDevice);

#endif /* KAIN_D3D12_LOADER_SUBSET_H */
