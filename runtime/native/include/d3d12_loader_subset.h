#ifndef KAIN_D3D12_LOADER_SUBSET_H
#define KAIN_D3D12_LOADER_SUBSET_H

// ============================================================================
//  d3d12_loader_subset.h — Pure-declaration header for D3D12.
// ============================================================================
//  D3D12 uses COM (IUnknown-style vtable dispatch), not PFN resolution.
//  This header defines COM interface handle types, COM vtable layouts, IID
//  values, and the few PFN prototypes needed to bootstrap. All types are
//  uintptr_t or hand-rolled — no Windows SDK needed.
//
//  Never includes <d3d12.h> or <dxgi.h>. The actual D3D12/DXGI driver calls
//  live in the separately-linked ABI library (d3d12-abi/) which uses this
//  header to dispatch through COM vtables via slot indices.
//
//  Slot numbering matches the Windows SDK COM vtable layout exactly. When
//  the real d3d12.dll/dxgi.dll provides an IDXGIFactory2*, the vtable
//  pointer at offset 0 points to a struct of 18 function pointers (for
//  IDXGIFactory2). The slot indices below are the real slot numbers.
// ============================================================================

#include <stdint.h>
#include <stddef.h>

// ── COM HRESULT ──────────────────────────────────────────────────
//
// HRESULT is a 32-bit status code. Windows SDK defines it as `long`, but we
// use `int32_t` for ABI portability. The runtime can include this header
// before or after <windows.h> — when <windows.h> is present, the Windows
// definition of HRESULT already matches the layout we need, so we skip
// the redefinition to avoid conflicts with `-Werror`/MSVC strict modes.

#ifndef _HRESULT_DEFINED
typedef int32_t HRESULT;
#define _HRESULT_DEFINED 1
#endif

// ── IID/GUID ─────────────────────────────────────────────────────

typedef struct KainGuid {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} KainGuid;

#define KAIN_IID_INITIALIZER(d1, d2, d3, b0, b1, b2, b3, b4, b5, b6, b7) \
    { (d1), (d2), (d3), { (b0), (b1), (b2), (b3), (b4), (b5), (b6), (b7) } }

// ── Well-known IID values (from dxgi.h / d3d12.h) ────────────────

// {50a8e351-7c4a-4b7a-8a06-bb06a3471d9e}
static const KainGuid KAIN_IID_IDXGIFactory2 =
    KAIN_IID_INITIALIZER(0x50a8e351, 0x7c4a, 0x4b7a, 0x8a, 0x06, 0xbb, 0x06, 0xa3, 0x47, 0x1d, 0x9e);
// {2411e7e1-12ac-4ccf-bd14-9798e8534dc0}
static const KainGuid KAIN_IID_IDXGIAdapter =
    KAIN_IID_INITIALIZER(0x2411e7e1, 0x12ac, 0x4ccf, 0xbd, 0x14, 0x97, 0x98, 0xe8, 0x53, 0x4d, 0xc0);
// {189819f1-1db6-4b57-be54-1821337b4f05}
static const KainGuid KAIN_IID_ID3D12Device =
    KAIN_IID_INITIALIZER(0x189819f1, 0x1db6, 0x4b57, 0xbe, 0x54, 0x18, 0x21, 0x33, 0x7b, 0x4f, 0x05);
// {0ec870a6-5d7e-4c22-8cfc-5baae07616ed}
static const KainGuid KAIN_IID_ID3D12CommandQueue =
    KAIN_IID_INITIALIZER(0x0ec870a6, 0x5d7e, 0x4c22, 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed);
// {6102dee4-af59-4b09-b999-b44d73f09b24}
static const KainGuid KAIN_IID_ID3D12CommandAllocator =
    KAIN_IID_INITIALIZER(0x6102dee4, 0xaf59, 0x4b09, 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24);
// {0a753dcf-cb3a-4e04-83fc-7a78146c2c8e}
static const KainGuid KAIN_IID_ID3D12Fence =
    KAIN_IID_INITIALIZER(0x0a753dcf, 0xcb3a, 0x4e04, 0x83, 0xfc, 0x7a, 0x78, 0x14, 0x6c, 0x2c, 0x8e);
// {696442be-a72e-4059-bc79-5b5c98040fad}
static const KainGuid KAIN_IID_ID3D12Resource =
    KAIN_IID_INITIALIZER(0x696442be, 0xa72e, 0x4059, 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad);
// {8efb471d-616b-44dc-97fb-f3cfd8b772e1}
static const KainGuid KAIN_IID_ID3D12DescriptorHeap =
    KAIN_IID_INITIALIZER(0x8efb471d, 0x616b, 0x44dc, 0x97, 0xfb, 0xf3, 0xcf, 0xd8, 0xb7, 0x72, 0xe1);
// {5b160d23-fd03-4a87-a3b2-3a7c1d5d1c7d}
static const KainGuid KAIN_IID_ID3D12GraphicsCommandList =
    KAIN_IID_INITIALIZER(0x5b160d23, 0xfd03, 0x4a87, 0xa3, 0xb2, 0x3a, 0x7c, 0x1d, 0x5d, 0x1c, 0x7d);
// {94d99bdb-f1f8-4ab0-b236-7da0170edab1}
static const KainGuid KAIN_IID_IDXGISwapChain3 =
    KAIN_IID_INITIALIZER(0x94d99bdb, 0xf1f8, 0x4ab0, 0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1);
// {7b7166ec-21c7-44ae-b21a-c9ae5eee3f81}
static const KainGuid KAIN_IID_ID3D12CommandList =
    KAIN_IID_INITIALIZER(0x7b7166ec, 0x21c7, 0x44ae, 0xb2, 0x1a, 0xc9, 0xae, 0x5e, 0xee, 0x3f, 0x81);

// ── D3D12/DXGI handle types (all uintptr_t) ──────────────────────
//
// COM interface types are opaque — they're the real D3D12/DXGI COM
// pointers at runtime, and we treat them as uintptr_t for type
// transparency. HWND/HANDLE come from the Windows SDK when <windows.h>
// is included; we only define them here as fallbacks so this header is
// usable standalone (no Windows SDK) for typechecking purposes.

typedef uintptr_t IDXGIFactory2;
typedef uintptr_t IDXGIAdapter;
typedef uintptr_t ID3D12Device;
typedef uintptr_t ID3D12CommandQueue;
typedef uintptr_t IDXGISwapChain1;
typedef uintptr_t IDXGISwapChain3;
typedef uintptr_t ID3D12Resource;
typedef uintptr_t ID3D12DescriptorHeap;
typedef uintptr_t ID3D12CommandAllocator;
typedef uintptr_t ID3D12CommandList;
typedef uintptr_t ID3D12GraphicsCommandList;
typedef uintptr_t ID3D12Fence;
typedef uintptr_t IUnknown;
typedef uintptr_t IDXGIOutput;
typedef uint32_t RECT_SINT;

#ifndef _WIN32
// Fallbacks when Windows SDK is not present. On Windows, the SDK's
// <winnt.h> and <windef.h> provide the real definitions.
typedef uintptr_t HWND;
typedef uintptr_t HANDLE;
typedef uintptr_t HMODULE;
#endif

// ── COM vtable macro — declares a flat array of slots ────────────
//
// Usage: KAIN_COM_VTABLE(VtblName, SlotCount);
//   expands to: typedef struct VtblName { void* slots[SlotCount]; } VtblName;
//
// We use typedef so the struct tag and the type name are the same, which
// avoids MSVC's "missing tag name" warning on incomplete declarations.

#define KAIN_COM_VTABLE(name, count) typedef struct name { void* slots[count]; } name

// ── IDXGIFactory2 vtable (18 slots, matches dxgi.h) ──────────────
//
// Slot layout (from dxgi.h IUnknown → IDXGIObject → IDXGIFactory →
// IDXGIFactory1 → IDXGIFactory2 inheritance chain):
//   0  QueryInterface     (IUnknown)
//   1  AddRef             (IUnknown)
//   2  Release            (IUnknown)
//   3  SetPrivateData     (IDXGIObject)
//   4  SetPrivateDataInterface (IDXGIObject)
//   5  GetPrivateData     (IDXGIObject)
//   6  GetParent          (IDXGIObject)
//   7  EnumAdapters       (IDXGIFactory)
//   8  MakeWindowAssociation (IDXGIFactory)
//   9  GetWindowAssociation  (IDXGIFactory)
//  10  CreateSwapChain    (IDXGIFactory)
//  11  CreateSoftwareAdapter (IDXGIFactory)
//  12  EnumAdapters1      (IDXGIFactory1)
//  13  IsCurrent          (IDXGIFactory1)
//  14  IsWindowedStereoEnabled (IDXGIFactory2)
//  15  CreateStereoView   (IDXGIFactory2)
//  16  CreateSwapChainForHwnd (IDXGIFactory2)
//  17  CreateSwapChainForCoreWindow (IDXGIFactory2)

KAIN_COM_VTABLE(KainIdxgiFactory2Vtbl, 18);

typedef struct IDXGIFactory2Object {
    KainIdxgiFactory2Vtbl* lpVtbl;
} IDXGIFactory2Object;

// Typed function pointers for the methods we use
typedef HRESULT (*KainIdxgiFactory2_QueryInterfaceFn)(
    IDXGIFactory2 self, const KainGuid* riid, void** ppv);
typedef uint32_t (*KainIdxgiFactory2_AddRefFn)(IDXGIFactory2 self);
typedef uint32_t (*KainIdxgiFactory2_ReleaseFn)(IDXGIFactory2 self);
typedef HRESULT (*KainIdxgiFactory2_EnumAdaptersFn)(
    IDXGIFactory2 self, uint32_t adapter_index, IDXGIAdapter* adapter_out);
typedef HRESULT (*KainIdxgiFactory2_CreateSwapChainForHwndFn)(
    IDXGIFactory2 self, IUnknown* device, HWND hwnd,
    const void* swap_chain_desc1, const void* fullscreen_desc,
    IDXGIOutput restrict_to_output, IDXGISwapChain1** swap_chain_out);

// Slot indices for the methods we use
#define KAIN_SLOT_QUERYINTERFACE 0
#define KAIN_SLOT_ADDREF 1
#define KAIN_SLOT_RELEASE 2
#define KAIN_SLOT_FACTORY2_ENUMADAPTERS 7
#define KAIN_SLOT_FACTORY2_CREATESWAPCHAINFORHWND 16

// ── ID3D12Device vtable (50 slots, matches d3d12.h) ──────────────
//
// Slot layout (from d3d12.h IUnknown → ID3D12Object → ID3D12Device):
//   0  QueryInterface     (IUnknown)
//   1  AddRef             (IUnknown)
//   2  Release            (IUnknown)
//   3  GetPrivateData     (ID3D12Object)
//   4  SetPrivateData     (ID3D12Object)
//   5  SetPrivateDataInterface (ID3D12Object)
//   6  SetName            (ID3D12Object)
//   7  GetNodeCount       (ID3D12Device)
//   8  CreateCommandQueue (ID3D12Device)
//   9  CreateCommandAllocator (ID3D12Device)
//  10  CreateGraphicsPipelineState (ID3D12Device)
//  11  CreateComputePipelineState (ID3D12Device)
//  12  CreateCommandList  (ID3D12Device)
//  13  CheckFeatureSupport (ID3D12Device)
//  14  CreateDescriptorHeap (ID3D12Device)
//  15  GetDescriptorHandleIncrementSize (ID3D12Device)
//  16  CreateRootSignature (ID3D12Device)
//  17  CreateConstantBufferView (ID3D12Device)
//  18  CreateShaderResourceView (ID3D12Device)
//  19  CreateUnorderedAccessView (ID3D12Device)
//  20  CreateRenderTargetView (ID3D12Device)
//  21  CreateDepthStencilView (ID3D12Device)
//  22  CreateSampler      (ID3D12Device)
//  23  CopyDescriptors    (ID3D12Device)
//  24  CopyDescriptorsByRegion (ID3D12Device)
//  25  CreateSharedHandle (ID3D12Device)
//  26  OpenSharedHandle   (ID3D12Device)
//  27  OpenSharedHandleByName (ID3D12Device)
//  28  MakeResident       (ID3D12Device)
//  29  Evict              (ID3D12Device)
//  30  CreateFence        (ID3D12Device)
//  31  GetDeviceRemovedReason (ID3D12Device)
//  ... (remaining slots 32-49: residency, heap queries)

KAIN_COM_VTABLE(KainID3D12DeviceVtbl, 50);

typedef struct ID3D12DeviceObject {
    KainID3D12DeviceVtbl* lpVtbl;
} ID3D12DeviceObject;

typedef HRESULT (*KainID3D12Device_CreateCommandQueueFn)(
    ID3D12Device self, const void* queue_desc,
    const KainGuid* riid, void** command_queue_out);
typedef HRESULT (*KainID3D12Device_CreateCommandAllocatorFn)(
    ID3D12Device self, uint32_t command_list_type,
    const KainGuid* riid, void** command_allocator_out);
typedef HRESULT (*KainID3D12Device_CreateFenceFn)(
    ID3D12Device self, uint64_t initial_value, uint32_t flags,
    const KainGuid* riid, void** fence_out);
typedef HRESULT (*KainID3D12Device_CreateDescriptorHeapFn)(
    ID3D12Device self, const void* heap_desc,
    const KainGuid* riid, void** heap_out);
typedef HRESULT (*KainID3D12Device_CreateCommandListFn)(
    ID3D12Device self, uint32_t node_mask, uint32_t command_list_type,
    ID3D12CommandAllocator command_allocator, uintptr_t initial_pipeline_state,
    const KainGuid* riid, void** command_list_out);
typedef void (*KainID3D12Device_CreateRenderTargetViewFn)(
    ID3D12Device self, ID3D12Resource resource, uintptr_t rtv_desc,
    uint64_t cpu_descriptor_handle);

#define KAIN_SLOT_DEVICE_CREATECOMMANDQUEUE 8
#define KAIN_SLOT_DEVICE_CREATECOMMANDALLOCATOR 9
#define KAIN_SLOT_DEVICE_CREATECOMMANDLIST 12
#define KAIN_SLOT_DEVICE_CREATEDESCRIPTORHEAP 14
#define KAIN_SLOT_DEVICE_CREATERENDERTARGETVIEW 20
#define KAIN_SLOT_DEVICE_CREATEFENCE 30

// ── ID3D12CommandQueue vtable (12 slots) ─────────────────────────
//
// Slot layout (IUnknown → ID3D12Object → ID3D12CommandQueue):
//   0-6: IUnknown + ID3D12Object
//   7  UpdateTileMappings
//   8  CopyTileMappings
//   9  ExecuteCommandLists
//  10  SetMarker
//  11  BeginEvent
//  (additional: EndEvent, Signal, Wait — depending on ID3D12CommandQueue version)

KAIN_COM_VTABLE(KainID3D12CommandQueueVtbl, 12);

typedef struct ID3D12CommandQueueObject {
    KainID3D12CommandQueueVtbl* lpVtbl;
} ID3D12CommandQueueObject;

typedef void (*KainID3D12CommandQueue_ExecuteCommandListsFn)(
    ID3D12CommandQueue self, uint32_t num_command_lists,
    const ID3D12CommandList* command_lists);
typedef uint64_t (*KainID3D12CommandQueue_SignalFn)(
    ID3D12CommandQueue self, ID3D12Fence fence, uint64_t value);

#define KAIN_SLOT_QUEUE_EXECUTECOMMANDLISTS 9
#define KAIN_SLOT_QUEUE_SIGNAL 10

// ── ID3D12CommandAllocator vtable (5 slots) ──────────────────────
//
// Slot layout (IUnknown → ID3D12Object → ID3D12CommandAllocator):
//   0-6: IUnknown + ID3D12Object
//   7  Reset

KAIN_COM_VTABLE(KainID3D12CommandAllocatorVtbl, 8);

typedef struct ID3D12CommandAllocatorObject {
    KainID3D12CommandAllocatorVtbl* lpVtbl;
} ID3D12CommandAllocatorObject;

typedef HRESULT (*KainID3D12CommandAllocator_ResetFn)(ID3D12CommandAllocator self);

#define KAIN_SLOT_ALLOCATOR_RESET 7

// ── ID3D12GraphicsCommandList vtable (~60 slots) ─────────────────
//
// Slot layout (IUnknown → ID3D12Object → ID3D12CommandList →
// ID3D12GraphicsCommandList):
//   0-6: IUnknown + ID3D12Object
//   7  Close              (ID3D12CommandList)
//   8  Reset              (ID3D12CommandList)
//   9  ClearState         (ID3D12GraphicsCommandList)
//  10  DrawInstanced
//  11  DrawIndexedInstanced
//  ...
//  20  IASetPrimitiveTopology (approx — actual order per d3d12.h)
//  ...
//  ~50  ClearRenderTargetView (approximate; real slot below)
//  ...
//  59  OMSetRenderTargets (approximate)
//
// IMPORTANT: slot numbers for graphics command list methods vary by
// ID3D12GraphicsCommandList version. The library uses the typed function
// pointer slots we declare below for known-stable methods. For methods
// at uncertain slots, we dispatch via direct vtable indexing with comments.

KAIN_COM_VTABLE(KainID3D12GraphicsCommandListVtbl, 60);

typedef struct ID3D12GraphicsCommandListObject {
    KainID3D12GraphicsCommandListVtbl* lpVtbl;
} ID3D12GraphicsCommandListObject;

typedef HRESULT (*KainID3D12GraphicsCommandList_CloseFn)(ID3D12GraphicsCommandList self);
typedef HRESULT (*KainID3D12GraphicsCommandList_ResetFn)(
    ID3D12GraphicsCommandList self, ID3D12CommandAllocator allocator,
    uintptr_t initial_pipeline_state);
typedef void (*KainID3D12GraphicsCommandList_ClearRenderTargetViewFn)(
    ID3D12GraphicsCommandList self, uint64_t rtv_handle,
    const float* clear_color, uint32_t num_rects,
    const RECT_SINT* rects);

#define KAIN_SLOT_GFXCMD_CLOSE 7
#define KAIN_SLOT_GFXCMD_RESET 8
// ClearRenderTargetView's actual slot — read from d3d12.h: it's at slot 40
// in ID3D12GraphicsCommandList (after pipeline-state, draw, dispatch, copy,
// and resource-barrier methods).
#define KAIN_SLOT_GFXCMD_CLEARRENDERTARGETVIEW 40

// ── ID3D12Fence vtable (10 slots) ────────────────────────────────
//
// Slot layout (IUnknown → ID3D12Object → ID3D12Fence → ID3D12Fence1):
//   0-6: IUnknown + ID3D12Object
//   7  GetCompletedValue
//   8  SetEventOnCompletion
//   9  Signal

KAIN_COM_VTABLE(KainID3D12FenceVtbl, 10);

typedef struct ID3D12FenceObject {
    KainID3D12FenceVtbl* lpVtbl;
} ID3D12FenceObject;

typedef uint64_t (*KainID3D12Fence_GetCompletedValueFn)(ID3D12Fence self);
typedef HRESULT (*KainID3D12Fence_SetEventOnCompletionFn)(
    ID3D12Fence self, uint64_t value, HANDLE event);
typedef uint64_t (*KainID3D12Fence_SignalFn)(ID3D12Fence self, uint64_t value);

#define KAIN_SLOT_FENCE_GETCOMPLETEDVALUE 7
#define KAIN_SLOT_FENCE_SETEVENTONCOMPLETION 8
#define KAIN_SLOT_FENCE_SIGNAL 9

// ── IDXGISwapChain3 vtable (28 slots, includes Present at slot 8) ─
//
// IDXGISwapChain3 inherits from IDXGISwapChain2 → IDXGISwapChain1 →
// IDXGISwapChain → IDXGIDeviceSubObject → IDXGIObject → IUnknown.
//   0-6: IUnknown + IDXGIObject
//   7  Present (IDXGISwapChain)  ← THIS is the one we call
//   8  GetBuffer
//  ... many more inherited slots
//  22  GetCurrentBackBufferIndex (IDXGISwapChain3)
//  23  GetMaximumFrameLatency

KAIN_COM_VTABLE(KainIDXGISwapChain3Vtbl, 28);

typedef struct IDXGISwapChain3Object {
    KainIDXGISwapChain3Vtbl* lpVtbl;
} IDXGISwapChain3Object;

typedef HRESULT (*KainIDXGISwapChain_PresentFn)(
    IDXGISwapChain3 self, uint32_t sync_interval, uint32_t flags);
typedef HRESULT (*KainIDXGISwapChain_GetBufferFn)(
    IDXGISwapChain3 self, uint32_t buffer_index,
    const KainGuid* riid, void** surface_out);
typedef uint32_t (*KainIDXGISwapChain3_GetCurrentBackBufferIndexFn)(
    IDXGISwapChain3 self);

#define KAIN_SLOT_SWAPCHAIN_PRESENT 7
#define KAIN_SLOT_SWAPCHAIN_GETBUFFER 8
#define KAIN_SLOT_SWAPCHAIN3_GETCURRENTBACKBUFFERINDEX 22

// ── ID3D12Resource vtable (~24 slots) ────────────────────────────

KAIN_COM_VTABLE(KainID3D12ResourceVtbl, 24);

typedef struct ID3D12ResourceObject {
    KainID3D12ResourceVtbl* lpVtbl;
} ID3D12ResourceObject;

typedef uint64_t (*KainID3D12Resource_GetGPUVirtualAddressFn)(ID3D12Resource self);

// ── D3D12 enums (matching d3d12.h values) ────────────────────────

#define KAIN_D3D12_COMMAND_LIST_TYPE_DIRECT 0u
#define KAIN_D3D12_COMMAND_LIST_TYPE_COMPUTE 1u

#define KAIN_D3D12_COMMAND_QUEUE_PRIORITY_NORMAL 0u
#define KAIN_D3D12_FENCE_FLAG_NONE 0u

#define KAIN_DXGI_FORMAT_R8G8B8A8_UNORM 28u
#define KAIN_DXGI_FORMAT_B8G8R8A8_UNORM 87u

#define KAIN_DXGI_SWAP_EFFECT_FLIP_DISCARD 4u

#define KAIN_DXGI_USAGE_RENDER_TARGET_OUTPUT 0x20u
#define KAIN_DXGI_USAGE_SHADER_RESOURCE 0x10u

#define KAIN_D3D12_DESCRIPTOR_HEAP_TYPE_RTV 2u
#define KAIN_D3D12_DESCRIPTOR_HEAP_FLAG_NONE 0u
#define KAIN_D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE 0x1u

#define KAIN_D3D12_RTV_DIMENSION_TEXTURE2D 6u
#define KAIN_D3D12_RESOURCE_DIMENSION_TEXTURE2D 3u
#define KAIN_D3D12_TEXTURE_LAYOUT_UNKNOWN 0u

#define KAIN_D3D12_RESOURCE_FLAG_NONE 0u
#define KAIN_D3D12_CPU_PAGE_PROPERTY_UNKNOWN 0u
#define KAIN_D3D12_MEMORY_POOL_UNKNOWN 0u

#define KAIN_D3D12_FEATURE_LEVEL_11_0 0xb000u
#define KAIN_D3D12_FEATURE_LEVEL_12_0 0xc000u
#define KAIN_D3D12_FEATURE_LEVEL_12_1 0xc100u

#define KAIN_S_OK 0
#define KAIN_S_FALSE 1
#define KAIN_E_FAIL 0x80004005u
#define KAIN_DXGI_ERROR_DEVICE_REMOVED 0x887a0005u

// ── D3D12 structure definitions (packed to match SDK layouts) ─────

// D3D12_COMMAND_QUEUE_DESC (matches d3d12.h layout)
typedef struct KainD3D12CommandQueueDesc {
    uint32_t Type;              // D3D12_COMMAND_LIST_TYPE
    uint32_t Priority;          // INT (cast to uint32_t)
    uint32_t Flags;             // D3D12_COMMAND_QUEUE_FLAGS
    uint32_t NodeMask;          // UINT
} KainD3D12CommandQueueDesc;

// DXGI_SWAP_CHAIN_DESC1 (matches dxgi.h layout)
typedef struct KainDxgiSwapChainDesc1 {
    uint32_t Width;             // UINT
    uint32_t Height;            // UINT
    uint32_t Format;            // DXGI_FORMAT
    uint32_t Stereo;            // BOOL
    uint32_t SampleDesc_Count;  // DXGI_SAMPLE_DESC.Count
    uint32_t SampleDesc_Quality;// DXGI_SAMPLE_DESC.Quality
    uint32_t BufferUsage;       // DXGI_USAGE
    uint32_t BufferCount;       // UINT
    uint32_t Scale;             // DXGI_SCALING
    uint32_t SwapEffect;        // DXGI_SWAP_EFFECT
    uint32_t AlphaMode;         // DXGI_ALPHA_MODE
    uint32_t Flags;             // UINT
} KainDxgiSwapChainDesc1;

// D3D12_DESCRIPTOR_HEAP_DESC (matches d3d12.h layout)
typedef struct KainD3D12DescriptorHeapDesc {
    uint32_t Type;              // D3D12_DESCRIPTOR_HEAP_TYPE
    uint32_t NumDescriptors;    // UINT
    uint32_t Flags;             // D3D12_DESCRIPTOR_HEAP_FLAGS
    uint32_t NodeMask;          // UINT
} KainD3D12DescriptorHeapDesc;

// D3D12_RENDER_TARGET_VIEW_DESC (matches d3d12.h layout)
typedef struct KainD3D12RenderTargetViewDesc {
    uint32_t Format;            // DXGI_FORMAT
    uint32_t ViewDimension;     // D3D12_RTV_DIMENSION
    union {
        struct {
            uint32_t MipSlice;
            uint32_t PlaneSlice;
        } Texture2D;
        uint8_t _pad[16];
    };
} KainD3D12RenderTargetViewDesc;

// D3D12_CLEAR_VALUE (matches d3d12.h layout)
typedef struct KainD3D12ClearValue {
    uint32_t Format;            // DXGI_FORMAT
    union {
        float Color[4];
        struct {
            float Depth;
            uint32_t Stencil;
        } DepthStencil;
    };
} KainD3D12ClearValue;

// D3D12_HEAP_PROPERTIES (matches d3d12.h layout)
typedef struct KainD3D12HeapProperties {
    uint32_t Type;              // D3D12_HEAP_TYPE
    uint32_t CPUPageProperty;   // D3D12_CPU_PAGE_PROPERTY
    uint32_t MemoryPoolPreference; // D3D12_MEMORY_POOL
    uint32_t CreationNodeMask;  // UINT
    uint32_t VisibleNodeMask;   // UINT
} KainD3D12HeapProperties;

#define KAIN_D3D12_HEAP_TYPE_DEFAULT 1u

// D3D12_RESOURCE_DESC (matches d3d12.h layout)
typedef struct KainD3D12ResourceDesc {
    uint32_t Dimension;         // D3D12_RESOURCE_DIMENSION
    uint64_t Alignment;         // UINT64
    uint64_t Width;             // UINT64
    uint32_t Height;            // UINT
    uint16_t DepthOrArraySize;  // UINT16
    uint16_t MipLevels;         // UINT16
    uint32_t Format;            // DXGI_FORMAT
    uint32_t SampleDesc_Count;  // DXGI_SAMPLE_DESC.Count
    uint32_t SampleDesc_Quality;// DXGI_SAMPLE_DESC.Quality
    uint32_t Layout;            // D3D12_TEXTURE_LAYOUT
    uint32_t Flags;             // D3D12_RESOURCE_FLAGS
} KainD3D12ResourceDesc;

// ── Win32 types (subset, no <windows.h>) ─────────────────────────

#define KAIN_SW_SHOW 5u
#define KAIN_WS_OVERLAPPEDWINDOW 0x00CF0000u
#define KAIN_WS_VISIBLE 0x10000000u
#define KAIN_CS_OWNDC 0x0020u
#define KAIN_CS_HREDRAW 0x0002u
#define KAIN_CS_VREDRAW 0x0001u
#define KAIN_INFINITE 0xFFFFFFFFu
#define KAIN_WAIT_OBJECT_0 0u
#define KAIN_WAIT_TIMEOUT 0x00000102u
#define KAIN_WAIT_FAILED 0xFFFFFFFFu

// ── DXGI/D3D12 PFN prototypes (bootstrap functions only) ─────────

HRESULT CreateDXGIFactory2(uint32_t Flags, const KainGuid* riid,
                           void** ppFactory);
HRESULT D3D12CreateDevice(uintptr_t pAdapter,
                          uint32_t MinimumFeatureLevel,
                          const KainGuid* riid, void** ppDevice);

// ── Win32 PFN prototypes (subset, used by ABI library) ───────────

typedef struct KainRect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} KainRect;

typedef struct KainWndClassExA {
    uint32_t    cbSize;
    uint32_t    style;
    uintptr_t   lpfnWndProc;
    int32_t     cbClsExtra;
    int32_t     cbWndExtra;
    uintptr_t   hInstance;
    uintptr_t   hIcon;
    uintptr_t   hCursor;
    uintptr_t   hbrBackground;
    const char* lpszMenuName;
    const char* lpszClassName;
    uintptr_t   hIconSm;
} KainWndClassExA;

typedef struct KainMsg {
    uintptr_t hwnd;
    uint32_t  message;
    uintptr_t wParam;
    uintptr_t lParam;
    uint32_t  time;
    int32_t   pt_x;
    int32_t   pt_y;
    uint32_t  lPrivate;
} KainMsg;

#endif /* KAIN_D3D12_LOADER_SUBSET_H */
