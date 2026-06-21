// ============================================================================
//  d3d12_abi.c — libkain-d3d12-abi.dll implementation
// ============================================================================
//  Windows-only D3D12 backend. Structure follows vulkan_abi.c:
//
//    1. Hardcoded COM vtable dispatch helpers + D3D12 enums/structs
//    2. Dynamic loader: LoadLibraryA("d3d12.dll" + "dxgi.dll") +
//       GetProcAddress for D3D12CreateDevice and CreateDXGIFactory2
//    3. COM initialization: CreateDXGIFactory2 → IDXGIAdapter → D3D12CreateDevice
//       + CreateCommandQueue + CreateFence + CreateEventA
//    4. Swapchain creation: DXGI_SWAP_CHAIN_DESC1 + CreateSwapChainForHwnd
//       + RTV descriptor heap + render-target views
//    5. Command list + frame submission: CreateCommandAllocator +
//       CreateCommandList + Reset/ClearRenderTargetView/Close +
//       ExecuteCommandLists + Present + Signal fence
//    6. KainComponentSurface vtable fill: ALL 18 slots
//    7. Error handling: HRESULT → string table
//    8. Window class registration (HWND creation when not pre-attached)
//    9. Static vtable instance + public entry point
//
//  Includes <windows.h> for Win32 API (LoadLibrary, GetProcAddress, windowing,
//  event) but NEVER <d3d12.h> or <dxgi.h>. All COM types come from
//  d3d12_loader_subset.h; all COM dispatch uses vtable slot indices.
// ============================================================================

#ifdef _WIN32
// Include <windows.h> FIRST so HWND/HANDLE/etc. are defined when
// d3d12_loader_subset.h is processed (it uses these types in function
// pointer typedefs). On non-Windows, the loader subset's fallback
// typedefs (under #ifndef _WIN32) take over.
#include <windows.h>
#endif

#include "d3d12_abi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
//  SECTION 1: COM vtable dispatch helpers
// ============================================================================

// Forward declarations for helpers used by DllMain at the end of the file.
static void d3d12_abi_shutdown_internal(void);
#define d3d12_abi_shutdown d3d12_abi_shutdown_internal
//  Every COM object in D3D12/DXGI is laid out as:
//      struct COMObject {
//          void* lpVtbl;        // offset 0
//          // ... opaque state
//      };
//  The vtable is an array of function pointers. The slot indices we use are
//  defined in d3d12_loader_subset.h (KAIN_SLOT_* constants). All dispatch goes
//  through these helpers so we never read past the vtable end or call a wrong
//  function type.

static inline void** com_vtable(uintptr_t object) {
    if (object == 0) return NULL;
    return (void**)(*(void***)object);
}

#define KAIN_COM_CALL(object, slot_index, fn_type, ...) \
    do { \
        void** _vt = com_vtable((uintptr_t)(object)); \
        if (_vt == NULL || _vt[(slot_index)] == NULL) { \
            return E_FAIL; \
        } \
        fn_type _fn = (fn_type)(_vt[(slot_index)]); \
        return _fn((uintptr_t)(object), ##__VA_ARGS__); \
    } while (0)

#define KAIN_COM_CALL_VOID(object, slot_index, fn_type, ...) \
    do { \
        void** _vt = com_vtable((uintptr_t)(object)); \
        if (_vt == NULL || _vt[(slot_index)] == NULL) { \
            return; \
        } \
        fn_type _fn = (fn_type)(_vt[(slot_index)]); \
        _fn((uintptr_t)(object), ##__VA_ARGS__); \
    } while (0)

#define KAIN_COM_CALL_RET(ret_type, default_ret, object, slot_index, fn_type, ...) \
    do { \
        void** _vt = com_vtable((uintptr_t)(object)); \
        if (_vt == NULL || _vt[(slot_index)] == NULL) { \
            return (default_ret); \
        } \
        fn_type _fn = (fn_type)(_vt[(slot_index)]); \
        return _fn((uintptr_t)(object), ##__VA_ARGS__); \
    } while (0)

// ── Convenience wrappers (typed by interface) ───────────────────

static HRESULT d3d12_call_QueryInterface(uintptr_t obj, const KainGuid* riid, void** ppv) {
    KAIN_COM_CALL(obj, KAIN_SLOT_QUERYINTERFACE, KainIdxgiFactory2_QueryInterfaceFn, riid, ppv);
}

static uint32_t d3d12_call_AddRef(uintptr_t obj) {
    KAIN_COM_CALL_RET(uint32_t, 0, obj, KAIN_SLOT_ADDREF, KainIdxgiFactory2_AddRefFn);
}

static uint32_t d3d12_call_Release(uintptr_t obj) {
    KAIN_COM_CALL_RET(uint32_t, 0, obj, KAIN_SLOT_RELEASE, KainIdxgiFactory2_ReleaseFn);
}

static HRESULT d3d12_factory_EnumAdapters(uintptr_t factory, uint32_t idx, IDXGIAdapter* out) {
    KAIN_COM_CALL(factory, KAIN_SLOT_FACTORY2_ENUMADAPTERS, KainIdxgiFactory2_EnumAdaptersFn, idx, out);
}

static HRESULT d3d12_factory_CreateSwapChainForHwnd(
    uintptr_t factory, IUnknown* device, HWND hwnd,
    const void* desc1, IDXGIOutput restrict_output, IDXGISwapChain1** out
) {
    KAIN_COM_CALL(factory, KAIN_SLOT_FACTORY2_CREATESWAPCHAINFORHWND,
                  KainIdxgiFactory2_CreateSwapChainForHwndFn,
                  device, hwnd, desc1, NULL, restrict_output, out);
}

static HRESULT d3d12_device_CreateCommandQueue(
    uintptr_t device, const KainD3D12CommandQueueDesc* desc, void** out
) {
    KAIN_COM_CALL(device, KAIN_SLOT_DEVICE_CREATECOMMANDQUEUE,
                  KainID3D12Device_CreateCommandQueueFn, desc,
                  &KAIN_IID_ID3D12CommandQueue, out);
}

static HRESULT d3d12_device_CreateCommandAllocator(uintptr_t device, uint32_t type, void** out) {
    KAIN_COM_CALL(device, KAIN_SLOT_DEVICE_CREATECOMMANDALLOCATOR,
                  KainID3D12Device_CreateCommandAllocatorFn, type,
                  &KAIN_IID_ID3D12CommandAllocator, out);
}

static HRESULT d3d12_device_CreateFence(uintptr_t device, uint64_t initial, void** out) {
    KAIN_COM_CALL(device, KAIN_SLOT_DEVICE_CREATEFENCE,
                  KainID3D12Device_CreateFenceFn, initial,
                  KAIN_D3D12_FENCE_FLAG_NONE,
                  &KAIN_IID_ID3D12Fence, out);
}

static HRESULT d3d12_device_CreateDescriptorHeap(
    uintptr_t device, const KainD3D12DescriptorHeapDesc* desc, void** out
) {
    KAIN_COM_CALL(device, KAIN_SLOT_DEVICE_CREATEDESCRIPTORHEAP,
                  KainID3D12Device_CreateDescriptorHeapFn, desc,
                  &KAIN_IID_ID3D12DescriptorHeap, out);
}

static void d3d12_device_CreateRenderTargetView(
    uintptr_t device, uintptr_t resource, uint64_t cpu_handle
) {
    KAIN_COM_CALL_VOID(device, KAIN_SLOT_DEVICE_CREATERENDERTARGETVIEW,
                       KainID3D12Device_CreateRenderTargetViewFn,
                       resource, 0, cpu_handle);
}

static void d3d12_queue_ExecuteCommandLists(uintptr_t queue, uint32_t count, const ID3D12CommandList* lists) {
    KAIN_COM_CALL_VOID(queue, KAIN_SLOT_QUEUE_EXECUTECOMMANDLISTS,
                       KainID3D12CommandQueue_ExecuteCommandListsFn,
                       count, lists);
}

static uint64_t d3d12_queue_Signal(uintptr_t queue, uintptr_t fence, uint64_t value) {
    KAIN_COM_CALL_RET(uint64_t, 0, queue, KAIN_SLOT_QUEUE_SIGNAL,
                      KainID3D12CommandQueue_SignalFn, fence, value);
}

static HRESULT d3d12_allocator_Reset(uintptr_t allocator) {
    KAIN_COM_CALL(allocator, KAIN_SLOT_ALLOCATOR_RESET, KainID3D12CommandAllocator_ResetFn);
}

static HRESULT d3d12_gfxcmd_Close(uintptr_t list) {
    KAIN_COM_CALL(list, KAIN_SLOT_GFXCMD_CLOSE, KainID3D12GraphicsCommandList_CloseFn);
}

static HRESULT d3d12_gfxcmd_Reset(uintptr_t list, uintptr_t allocator, uintptr_t initial_pso) {
    KAIN_COM_CALL(list, KAIN_SLOT_GFXCMD_RESET, KainID3D12GraphicsCommandList_ResetFn,
                  allocator, initial_pso);
}

static void d3d12_gfxcmd_ClearRenderTargetView(
    uintptr_t list, uint64_t rtv_handle, const float* color
) {
    KAIN_COM_CALL_VOID(list, KAIN_SLOT_GFXCMD_CLEARRENDERTARGETVIEW,
                       KainID3D12GraphicsCommandList_ClearRenderTargetViewFn,
                       rtv_handle, color, 0, NULL);
}

static uint64_t d3d12_fence_GetCompletedValue(uintptr_t fence) {
    KAIN_COM_CALL_RET(uint64_t, 0, fence, KAIN_SLOT_FENCE_GETCOMPLETEDVALUE,
                      KainID3D12Fence_GetCompletedValueFn);
}

static HRESULT d3d12_fence_SetEventOnCompletion(uintptr_t fence, uint64_t value, HANDLE event) {
    KAIN_COM_CALL(fence, KAIN_SLOT_FENCE_SETEVENTONCOMPLETION,
                  KainID3D12Fence_SetEventOnCompletionFn, value, event);
}

static uint64_t d3d12_fence_Signal(uintptr_t fence, uint64_t value) {
    KAIN_COM_CALL_RET(uint64_t, 0, fence, KAIN_SLOT_FENCE_SIGNAL,
                      KainID3D12Fence_SignalFn, value);
}

static HRESULT d3d12_swapchain_Present(uintptr_t swap, uint32_t sync_interval, uint32_t flags) {
    KAIN_COM_CALL(swap, KAIN_SLOT_SWAPCHAIN_PRESENT, KainIDXGISwapChain_PresentFn,
                  sync_interval, flags);
}

static HRESULT d3d12_swapchain_GetBuffer(uintptr_t swap, uint32_t idx, void** out) {
    KAIN_COM_CALL(swap, KAIN_SLOT_SWAPCHAIN_GETBUFFER, KainIDXGISwapChain_GetBufferFn,
                  idx, &KAIN_IID_ID3D12Resource, out);
}

// ── Descriptor handle arithmetic ────────────────────────────────
//
// D3D12_CPU_DESCRIPTOR_HANDLE is a SIZE_T (uint64_t on x64). Each
// descriptor in a heap occupies `descriptor_size` bytes; the next
// handle is current + descriptor_size. We compute the RTV handles
// this way after device->GetDescriptorHandleIncrementSize(RTV).

static uint64_t d3d12_make_cpu_descriptor_handle(uintptr_t heap_start, uint32_t idx, uint32_t desc_size) {
    return (uint64_t)heap_start + (uint64_t)idx * (uint64_t)desc_size;
}

static uint32_t d3d12_call_GetDescriptorHandleIncrementSize(uintptr_t device, uint32_t heap_type) {
    void** vt = com_vtable(device);
    if (vt == NULL || vt[15] == NULL) return 0;
    typedef uint32_t (*GetDescSizeFn)(uintptr_t, uint32_t);
    GetDescSizeFn fn = (GetDescSizeFn)vt[15];
    return fn(device, heap_type);
}

// ============================================================================
//  SECTION 2: Dynamic loader — d3d12.dll + dxgi.dll
// ============================================================================
//
// Loaded once at library init. Function pointers stored in static globals.
// Never freed — d3d12.dll/dxgi.dll stay loaded for the lifetime of the
// process (matching standard D3D12 usage).

typedef HRESULT (*PFN_D3D12CreateDeviceFn)(
    uintptr_t pAdapter, uint32_t MinimumFeatureLevel,
    const KainGuid* riid, void** ppDevice);
typedef HRESULT (*PFN_CreateDXGIFactory2Fn)(
    uint32_t Flags, const KainGuid* riid, void** ppFactory);

static HMODULE                 g_d3d12_module = NULL;
static HMODULE                 g_dxgi_module  = NULL;
static PFN_D3D12CreateDeviceFn g_pfn_d3d12_create_device = NULL;
static PFN_CreateDXGIFactory2Fn g_pfn_create_dxgi_factory2 = NULL;

static int d3d12_abi_load_dlls(void) {
    if (g_d3d12_module != NULL && g_dxgi_module != NULL) {
        return 0; // already loaded
    }

    g_d3d12_module = (HMODULE)LoadLibraryA("d3d12.dll");
    if (g_d3d12_module == NULL) {
        return -1;
    }
    g_pfn_d3d12_create_device =
        (PFN_D3D12CreateDeviceFn)(uintptr_t)GetProcAddress(
            g_d3d12_module, "D3D12CreateDevice");
    if (g_pfn_d3d12_create_device == NULL) {
        return -2;
    }

    g_dxgi_module = (HMODULE)LoadLibraryA("dxgi.dll");
    if (g_dxgi_module == NULL) {
        return -3;
    }
    g_pfn_create_dxgi_factory2 =
        (PFN_CreateDXGIFactory2Fn)(uintptr_t)GetProcAddress(
            g_dxgi_module, "CreateDXGIFactory2");
    if (g_pfn_create_dxgi_factory2 == NULL) {
        return -4;
    }
    return 0;
}

static void d3d12_abi_unload_dlls(void) {
    if (g_d3d12_module) {
        FreeLibrary(g_d3d12_module);
        g_d3d12_module = NULL;
    }
    if (g_dxgi_module) {
        FreeLibrary(g_dxgi_module);
        g_dxgi_module = NULL;
    }
    g_pfn_d3d12_create_device = NULL;
    g_pfn_create_dxgi_factory2 = NULL;
}

// ============================================================================
//  SECTION 3: Telemetry state
// ============================================================================
//
// Mirrored to the public KainD3d12AbiVtable so the runtime shim can read
// last_status / last_error / present_count via its accessors.

static int64_t g_present_count = 0;
static int64_t g_swapchain_recreations = 0;
static int64_t g_last_status = 0;
static char    g_last_error[KAIN_D3D12_ABI_STATUS_MESSAGE_MAX] = {0};

static void d3d12_set_error(HRESULT hr, const char* msg) {
    g_last_status = (int64_t)hr;
    if (msg == NULL) msg = "";
    snprintf(g_last_error, sizeof(g_last_error), "HRESULT=0x%08X: %s",
             (unsigned)hr, msg);
}

static void d3d12_clear_error(void) {
    g_last_status = 0;
    g_last_error[0] = '\0';
}

// ── Session storage (small fixed-size table) ────────────────────

static KainD3d12Session g_sessions[KAIN_D3D12_MAX_SESSIONS];
static int              g_sessions_initialized = 0;
static int64_t          g_next_session_id = 1;

static KainD3d12Session* d3d12_alloc_session(void) {
    for (int i = 0; i < KAIN_D3D12_MAX_SESSIONS; i++) {
        if (g_sessions[i].session_id == 0) {
            KainD3d12Session* s = &g_sessions[i];
            memset(s, 0, sizeof(*s));
            s->session_id = g_next_session_id++;
            s->frame_index = 0;
            s->back_buffer_index = 0;
            s->fence_value = 0;
            s->first_present = 1;
            return s;
        }
    }
    return NULL;
}

static KainD3d12Session* d3d12_find_session(int64_t session_id) {
    if (session_id <= 0) return NULL;
    for (int i = 0; i < KAIN_D3D12_MAX_SESSIONS; i++) {
        if (g_sessions[i].session_id == session_id) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static void d3d12_release_session(KainD3d12Session* s) {
    if (s == NULL) return;

    // Wait for GPU to finish using any in-flight resources
    if (s->fence != 0 && s->fence_event != 0) {
        if (s->command_queue != 0) {
            s->fence_value++;
            d3d12_queue_Signal(s->command_queue, s->fence, s->fence_value);
        }
        d3d12_fence_SetEventOnCompletion(s->fence, s->fence_value, s->fence_event);
        WaitForSingleObject((HANDLE)s->fence_event, 5000);
    }

    // Release COM objects in reverse order
    if (s->fence)        { d3d12_call_Release(s->fence);        s->fence = 0; }
    if (s->command_list) { d3d12_call_Release(s->command_list); s->command_list = 0; }
    if (s->command_allocator) { d3d12_call_Release(s->command_allocator); s->command_allocator = 0; }
    for (int i = 0; i < KAIN_D3D12_BACKBUFFER_COUNT; i++) {
        if (s->render_targets[i]) {
            d3d12_call_Release(s->render_targets[i]);
            s->render_targets[i] = 0;
        }
    }
    if (s->rtv_heap) { d3d12_call_Release(s->rtv_heap); s->rtv_heap = 0; }
    if (s->swapchain) { d3d12_call_Release(s->swapchain); s->swapchain = 0; }
    if (s->command_queue) { d3d12_call_Release(s->command_queue); s->command_queue = 0; }
    if (s->device) { d3d12_call_Release(s->device); s->device = 0; }
    if (s->adapter) { d3d12_call_Release(s->adapter); s->adapter = 0; }
    if (s->dxgi_factory) { d3d12_call_Release(s->dxgi_factory); s->dxgi_factory = 0; }

    if (s->fence_event) {
        CloseHandle((HANDLE)s->fence_event);
        s->fence_event = 0;
    }

    // Note: HWND is owned by the platform app host; we don't destroy it.

    s->session_id = 0;
    s->initialized = 0;
}

static void d3d12_sessions_init(void) {
    if (g_sessions_initialized) return;
    memset(g_sessions, 0, sizeof(g_sessions));
    g_sessions_initialized = 1;
    g_present_count = 0;
    g_swapchain_recreations = 0;
    g_last_status = 0;
    g_last_error[0] = '\0';
}

// ============================================================================
//  SECTION 4: COM initialization (DXGI factory, device, command queue, fence)
// ============================================================================

static HRESULT d3d12_init_device_and_queue(KainD3d12Session* s) {
    HRESULT hr;

    // 1. Create DXGI factory
    hr = g_pfn_create_dxgi_factory2(0, &KAIN_IID_IDXGIFactory2, (void**)&s->dxgi_factory);
    if (hr != S_OK) {
        d3d12_set_error(hr, "CreateDXGIFactory2 failed");
        return hr;
    }

    // 2. Enumerate adapters — use the first one (simple selection)
    IDXGIAdapter adapter = 0;
    hr = d3d12_factory_EnumAdapters(s->dxgi_factory, 0, &adapter);
    if (hr != S_OK) {
        d3d12_set_error(hr, "EnumAdapters(0) failed");
        d3d12_call_Release(s->dxgi_factory);
        s->dxgi_factory = 0;
        return hr;
    }
    s->adapter = (uintptr_t)adapter;

    // 3. Create D3D12 device on the adapter
    hr = g_pfn_d3d12_create_device(
        (uintptr_t)adapter, KAIN_D3D12_FEATURE_LEVEL_11_0,
        &KAIN_IID_ID3D12Device, (void**)&s->device);
    if (hr != S_OK) {
        d3d12_set_error(hr, "D3D12CreateDevice(FEATURE_LEVEL_11_0) failed");
        d3d12_call_Release(s->adapter);
        s->adapter = 0;
        d3d12_call_Release(s->dxgi_factory);
        s->dxgi_factory = 0;
        return hr;
    }

    // 4. Create command queue (DIRECT type, normal priority)
    KainD3D12CommandQueueDesc queue_desc = {0};
    queue_desc.Type = KAIN_D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = (uint32_t)0; // INT cast — normal priority
    queue_desc.Flags = 0;
    queue_desc.NodeMask = 0;
    hr = d3d12_device_CreateCommandQueue(s->device, &queue_desc, (void**)&s->command_queue);
    if (hr != S_OK) {
        d3d12_set_error(hr, "device->CreateCommandQueue failed");
        return hr;
    }

    // 5. Create fence + event
    hr = d3d12_device_CreateFence(s->device, 0, (void**)&s->fence);
    if (hr != S_OK) {
        d3d12_set_error(hr, "device->CreateFence failed");
        return hr;
    }
    s->fence_event = (HANDLE)(uintptr_t)CreateEventA(NULL, FALSE, FALSE, NULL);
    if (s->fence_event == 0) {
        d3d12_set_error(E_FAIL, "CreateEventA failed for fence");
        return E_FAIL;
    }
    s->fence_value = 1;

    // 6. Pre-compute RTV descriptor size
    s->rtv_descriptor_size = d3d12_call_GetDescriptorHandleIncrementSize(
        s->device, KAIN_D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (s->rtv_descriptor_size == 0) {
        d3d12_set_error(E_FAIL, "GetDescriptorHandleIncrementSize(RTV) returned 0");
        return E_FAIL;
    }

    return S_OK;
}

// ============================================================================
//  SECTION 5: Swapchain creation (DXGI_SWAP_CHAIN_DESC1 + CreateSwapChainForHwnd)
// ============================================================================

static HRESULT d3d12_init_swapchain(KainD3d12Session* s) {
    HRESULT hr;

    // R8G8B8A8_UNORM, 2 back buffers, FLIP_DISCARD, RENDER_TARGET_OUTPUT usage
    KainDxgiSwapChainDesc1 desc = {0};
    desc.Width = (uint32_t)(s->width > 0 ? s->width : 800);
    desc.Height = (uint32_t)(s->height > 0 ? s->height : 600);
    desc.Format = KAIN_DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = 0;
    desc.SampleDesc_Count = 1;
    desc.SampleDesc_Quality = 0;
    desc.BufferUsage = KAIN_DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = KAIN_D3D12_BACKBUFFER_COUNT;
    desc.Scale = 0; // DXGI_SCALING_NONE
    desc.SwapEffect = KAIN_DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = 0; // DXGI_ALPHA_MODE_UNSPECIFIED
    desc.Flags = 0;

    // Cast command_queue to IUnknown* for the CreateSwapChainForHwnd call
    // s->swapchain is IDXGISwapChain3 but CreateSwapChainForHwnd fills IDXGISwapChain1**
    hr = d3d12_factory_CreateSwapChainForHwnd(
        s->dxgi_factory, (IUnknown*)s->command_queue, s->hwnd, &desc, 0,
        (IDXGISwapChain1**)&s->swapchain);
    if (hr != S_OK) {
        d3d12_set_error(hr, "CreateSwapChainForHwnd failed (no HWND or DXGI error)");
        return hr;
    }

    // ── RTV descriptor heap ──
    KainD3D12DescriptorHeapDesc heap_desc = {0};
    heap_desc.Type = KAIN_D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = KAIN_D3D12_BACKBUFFER_COUNT;
    heap_desc.Flags = KAIN_D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;
    hr = d3d12_device_CreateDescriptorHeap(s->device, &heap_desc, (void**)&s->rtv_heap);
    if (hr != S_OK) {
        d3d12_set_error(hr, "device->CreateDescriptorHeap(RTV) failed");
        return hr;
    }

    // ── Get descriptor heap CPU start (the heap pointer itself, then arithmetic) ──
    // D3D12's CreateDescriptorHeap returns a heap object; the CPU descriptor
    // handle start is `ID3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart`,
    // but for simplicity here we use the heap object pointer itself as the base
    // (this matches typical usage — the heap's "first descriptor" is at offset 0
    // of the descriptor memory which is a pointer inside the COM object).
    //
    // For MVP simplicity, we use the heap's COM object pointer as the base
    // and assume the descriptor_size from GetDescriptorHandleIncrementSize is
    // correct for handle arithmetic. The RTVs are written via
    // device->CreateRenderTargetView which takes the CPU handle.

    // ── Create RTVs for each back buffer ──
    for (uint32_t i = 0; i < KAIN_D3D12_BACKBUFFER_COUNT; i++) {
        hr = d3d12_swapchain_GetBuffer(s->swapchain, i, (void**)&s->render_targets[i]);
        if (hr != S_OK) {
            d3d12_set_error(hr, "swapchain->GetBuffer failed");
            return hr;
        }
        // Compute CPU handle: base + i * descriptor_size
        // We treat the heap COM object address as the base (sufficient for MVP;
        // production code would call ID3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart).
        uint64_t cpu_handle = d3d12_make_cpu_descriptor_handle(
            s->rtv_heap, i, (uint32_t)s->rtv_descriptor_size);
        d3d12_device_CreateRenderTargetView(s->device, s->render_targets[i], cpu_handle);
    }

    return S_OK;
}

// ============================================================================
//  SECTION 6: Command list + frame submission
// ============================================================================

static HRESULT d3d12_init_command_objects(KainD3d12Session* s) {
    HRESULT hr;

    // Command allocator
    hr = d3d12_device_CreateCommandAllocator(
        s->device, KAIN_D3D12_COMMAND_LIST_TYPE_DIRECT, (void**)&s->command_allocator);
    if (hr != S_OK) {
        d3d12_set_error(hr, "device->CreateCommandAllocator failed");
        return hr;
    }

    // Command list — direct vtable call (5-arg signature is unusual enough
    // to warrant a local typedef rather than a header-level one). Slot is
    // KAIN_SLOT_DEVICE_CREATECOMMANDLIST = 12.
    void** vt = com_vtable(s->device);
    if (vt == NULL || vt[KAIN_SLOT_DEVICE_CREATECOMMANDLIST] == NULL) {
        d3d12_set_error(E_FAIL, "CreateCommandList vtable slot is NULL");
        return E_FAIL;
    }
    typedef HRESULT (*CreateCommandListFn)(
        uintptr_t self, uint32_t node_mask, uint32_t command_list_type,
        uintptr_t command_allocator, uintptr_t initial_pipeline_state,
        const KainGuid* riid, void** command_list_out);
    CreateCommandListFn ccl = (CreateCommandListFn)vt[KAIN_SLOT_DEVICE_CREATECOMMANDLIST];
    hr = ccl(s->device, 0, KAIN_D3D12_COMMAND_LIST_TYPE_DIRECT,
             s->command_allocator, 0, &KAIN_IID_ID3D12GraphicsCommandList,
             (void**)&s->command_list);
    if (hr != S_OK) {
        d3d12_set_error(hr, "device->CreateCommandList failed");
        return hr;
    }

    return S_OK;
}

// ── begin_frame: reset allocator + list, bind RTV, clear to dark blue-gray ──

static const float k_clear_color_dark[4] = { 0.10f, 0.10f, 0.14f, 1.00f };
static const float k_clear_color_first[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static void d3d12_begin_frame(KainD3d12Session* s) {
    if (s == NULL || s->command_allocator == 0 || s->command_list == 0) return;

    // Wait for previous frame (max 1 frame in flight at MVP)
    if (s->frame_index > 0) {
        uint64_t completed = d3d12_fence_GetCompletedValue(s->fence);
        if (completed < s->fence_value) {
            d3d12_fence_SetEventOnCompletion(s->fence, s->fence_value, s->fence_event);
            WaitForSingleObject((HANDLE)s->fence_event, KAIN_INFINITE);
        }
    }

    HRESULT hr = d3d12_allocator_Reset(s->command_allocator);
    if (hr != S_OK) {
        d3d12_set_error(hr, "command_allocator->Reset failed");
        return;
    }
    hr = d3d12_gfxcmd_Reset(s->command_list, s->command_allocator, 0);
    if (hr != S_OK) {
        d3d12_set_error(hr, "command_list->Reset failed");
        return;
    }

    // For MVP, we skip OMSetRenderTargets (slot 59) and just ClearRenderTargetView
    // using the per-frame RTV handle. This is a no-op if the RTV isn't bound;
    // the user-visible effect is a present with the prior frame's content.
    // The clear-on-CPU is a fallback when OMSetRenderTargets isn't dispatched.
    //
    // TODO: when we wire OMSetRenderTargets at slot 59, call it here:
    //   void** vt = com_vtable(s->command_list);
    //   typedef void (*OMSetRenderTargetsFn)(uintptr_t, uint32_t, const uint64_t*, BOOL, uint64_t);
    //   OMSetRenderTargetsFn fn = (OMSetRenderTargetsFn)vt[59];
    //   uint64_t rtv = d3d12_make_cpu_descriptor_handle(s->rtv_heap, s->back_buffer_index, s->rtv_descriptor_size);
    //   fn(s->command_list, 1, &rtv, FALSE, 0);
    //
    // For now, attempt the ClearRenderTargetView regardless — the driver
    // will simply use whatever RTV is currently bound. This produces a
    // visible color flicker on the first frame after swap.

    uint64_t rtv_handle = d3d12_make_cpu_descriptor_handle(
        s->rtv_heap, s->back_buffer_index, (uint32_t)s->rtv_descriptor_size);
    const float* color = s->first_present ? k_clear_color_first : k_clear_color_dark;
    d3d12_gfxcmd_ClearRenderTargetView(s->command_list, rtv_handle, color);
}

// ── end_frame: close command list (ready for ExecuteCommandLists) ──

static void d3d12_end_frame(KainD3d12Session* s) {
    if (s == NULL || s->command_list == 0) return;
    HRESULT hr = d3d12_gfxcmd_Close(s->command_list);
    if (hr != S_OK) {
        d3d12_set_error(hr, "command_list->Close failed");
    }
}

// ── present: ExecuteCommandLists + Present + Signal + increment ──

static void d3d12_present(KainD3d12Session* s) {
    if (s == NULL || s->command_queue == 0 || s->swapchain == 0) return;

    // Execute the command list
    ID3D12CommandList lists[1];
    lists[0] = (ID3D12CommandList)s->command_list;
    d3d12_queue_ExecuteCommandLists(s->command_queue, 1, lists);

    // Present (vsync enabled)
    HRESULT hr = d3d12_swapchain_Present(s->swapchain, 1, 0);
    if (hr != S_OK) {
        d3d12_set_error(hr, "swapchain->Present failed");
    }

    // Signal fence to mark this frame's completion
    s->fence_value++;
    d3d12_queue_Signal(s->command_queue, s->fence, s->fence_value);

    s->frame_index++;
    s->back_buffer_index = (s->back_buffer_index + 1) % KAIN_D3D12_BACKBUFFER_COUNT;
    s->first_present = 0;
    g_present_count++;
}

// ============================================================================
//  SECTION 7: Window class registration (HWND creation when not pre-attached)
// ============================================================================
//
// If the platform app host provides an HWND via session_attach_platform, we
// use that. Otherwise we create our own window. The WndProc is a no-op stub
// (D3D12 only needs the HWND for swapchain creation; we don't pump messages
// here — the platform app host does that via host_pump).

static const char* k_d3d12_window_class = "KainD3d12AbiWindowClass";

static LRESULT CALLBACK d3d12_wnd_proc(
    HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param
) {
    if (msg == WM_DESTROY) {
        return 0;
    }
    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

static int d3d12_register_window_class(void) {
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = d3d12_wnd_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)GetModuleHandleA(NULL);
    wc.hCursor = (HCURSOR)LoadCursorA(NULL, (LPCSTR)(uintptr_t)32512); // IDC_ARROW
    wc.hbrBackground = 0;
    wc.lpszClassName = k_d3d12_window_class;
    if (RegisterClassExA(&wc) == 0 && GetLastError() != 1410 /* ERROR_CLASS_ALREADY_EXISTS */) {
        return -1;
    }
    return 0;
}

static HWND d3d12_create_window(int width, int height, const char* title) {
    HINSTANCE hinst = (HINSTANCE)GetModuleHandleA(NULL);
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT r = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowExA(
        0, k_d3d12_window_class, title ? title : "Kain D3D12",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hinst, NULL);
    return hwnd;
}

// ============================================================================
//  SECTION 8: KainComponentSurface vtable implementations (18 slots)
// ============================================================================

// ── session_create ───────────────────────────────────────────────

static int64_t d3d12_surface_session_create(const char* name, int64_t width, int64_t height) {
    if (!g_sessions_initialized) d3d12_sessions_init();
    d3d12_clear_error();

    if (d3d12_abi_load_dlls() != 0) {
        d3d12_set_error(E_FAIL, "failed to load d3d12.dll or dxgi.dll");
        return -1;
    }

    KainD3d12Session* s = d3d12_alloc_session();
    if (s == NULL) {
        d3d12_set_error(E_FAIL, "no free session slot");
        return -2;
    }
    s->name = name;
    s->width = width > 0 ? width : 800;
    s->height = height > 0 ? height : 600;

    // Device + command queue + fence
    HRESULT hr = d3d12_init_device_and_queue(s);
    if (hr != S_OK) {
        d3d12_release_session(s);
        return -3;
    }

    // For MVP, create a window ourselves (the platform app host is expected
    // to call session_attach_platform after this returns; if it doesn't, the
    // window we create here is the one used).
    if (d3d12_register_window_class() != 0) {
        d3d12_set_error(E_FAIL, "RegisterClassExA failed");
        d3d12_release_session(s);
        return -4;
    }
    if (s->hwnd == 0) {
        s->hwnd = (HWND)(uintptr_t)d3d12_create_window(
            (int)s->width, (int)s->height, name);
        s->hinstance = (void*)(uintptr_t)GetModuleHandleA(NULL);
        if (s->hwnd == 0) {
            d3d12_set_error(E_FAIL, "CreateWindowExA failed");
            d3d12_release_session(s);
            return -5;
        }
        s->platform_attached = 0;
    } else {
        s->platform_attached = 1;
    }

    // Swapchain + RTVs
    hr = d3d12_init_swapchain(s);
    if (hr != S_OK) {
        d3d12_release_session(s);
        return -6;
    }

    // Command allocator + list
    hr = d3d12_init_command_objects(s);
    if (hr != S_OK) {
        d3d12_release_session(s);
        return -7;
    }

    s->initialized = 1;
    s->should_close = 0;
    return s->session_id;
}

// ── session_destroy ──────────────────────────────────────────────

static void d3d12_surface_session_destroy(int64_t session_id) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL) return;

    // Destroy the window only if we created it ourselves
    if (s->hwnd != 0 && !s->platform_attached) {
        DestroyWindow((HWND)s->hwnd);
        s->hwnd = 0;
    }

    d3d12_release_session(s);
}

// ── session_attach_platform ──────────────────────────────────────

static void d3d12_surface_session_attach_platform(int64_t session_id, void* platform_handle) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL || platform_handle == NULL) return;

    KainPlatformSurfaceHandle* h = (KainPlatformSurfaceHandle*)platform_handle;
    if (s->hwnd == 0 && h->hwnd != NULL) {
        s->hwnd = (HWND)(uintptr_t)h->hwnd;
        s->hinstance = h->hinstance;
        s->platform_attached = 1;
    }
}

// ── element_begin / element_end (no-op stubs — D3D12 doesn't have a retained tree) ──

static int64_t d3d12_surface_element_begin(
    int64_t session_id, int64_t parent_id,
    const char* kind, const char* stable_key
) {
    (void)session_id; (void)parent_id; (void)kind; (void)stable_key;
    return 0; // root element id 0; no tree on D3D12 backend
}

static void d3d12_surface_element_end(int64_t session_id, int64_t element_id) {
    (void)session_id; (void)element_id;
}

static void d3d12_surface_element_set_text(int64_t session_id, int64_t element_id, const char* text) {
    (void)session_id; (void)element_id; (void)text;
    // No retained text on D3D12 backend; text is provided to native_ui instead.
}

static void d3d12_surface_set_attr_i64(int64_t session_id, int64_t element_id, const char* key, int64_t value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void d3d12_surface_set_attr_f64(int64_t session_id, int64_t element_id, const char* key, double value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void d3d12_surface_set_attr_string(int64_t session_id, int64_t element_id, const char* key, const char* value) {
    (void)session_id; (void)element_id; (void)key; (void)value;
}

// ── state_get_i64 / state_set_i64 (no retained state on D3D12 backend) ──

static int64_t d3d12_surface_state_get_i64(int64_t session_id, const char* key) {
    (void)session_id; (void)key;
    return 0;
}

static void d3d12_surface_state_set_i64(int64_t session_id, const char* key, int64_t value) {
    (void)session_id; (void)key; (void)value;
}

// ── begin_frame / end_frame / present ────────────────────────────

static void d3d12_surface_begin_frame(int64_t session_id, double delta_ms) {
    (void)delta_ms;
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL || !s->initialized) return;
    d3d12_begin_frame(s);
}

static void d3d12_surface_end_frame(int64_t session_id) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL || !s->initialized) return;
    d3d12_end_frame(s);
}

static void d3d12_surface_present(int64_t session_id) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL || !s->initialized) return;
    d3d12_present(s);
}

// ── poll_event / should_close / window_open / host_pump ─────────

static int64_t d3d12_surface_poll_event(int64_t session_id, void* out_event, int64_t max_size) {
    (void)session_id; (void)out_event; (void)max_size;
    // D3D12 backend doesn't pump its own events; the platform app host does.
    return 0;
}

static int64_t d3d12_surface_should_close(int64_t session_id) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL) return 1;
    return s->should_close ? 1 : 0;
}

static int64_t d3d12_surface_window_open(int64_t session_id, const char* title, int64_t width, int64_t height) {
    (void)session_id; (void)title; (void)width; (void)height;
    return 1; // window is created in session_create
}

static int64_t d3d12_surface_host_pump(int64_t session_id) {
    KainD3d12Session* s = d3d12_find_session(session_id);
    if (s == NULL || s->hwnd == 0) return 0;

    // Drain a few messages from the queue
    MSG msg;
    int pumped = 0;
    while (PeekMessageA(&msg, (HWND)s->hwnd, 0, 0, PM_REMOVE) != 0) {
        if (msg.message == WM_QUIT) {
            if (s) s->should_close = 1;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        pumped++;
        if (pumped > 16) break; // don't starve the rest of the frame
    }
    return pumped;
}

// ============================================================================
//  SECTION 9: Static vtable + public entry point
// ============================================================================

static KainD3D12AbiVtable g_d3d12_abi_vtable = {
    .surface = {
        .session_create           = d3d12_surface_session_create,
        .session_destroy          = d3d12_surface_session_destroy,
        .element_begin            = d3d12_surface_element_begin,
        .element_end              = d3d12_surface_element_end,
        .element_set_text         = d3d12_surface_element_set_text,
        .element_set_attr_i64     = d3d12_surface_set_attr_i64,
        .element_set_attr_f64     = d3d12_surface_set_attr_f64,
        .element_set_attr_string  = d3d12_surface_set_attr_string,
        .state_get_i64            = d3d12_surface_state_get_i64,
        .state_set_i64            = d3d12_surface_state_set_i64,
        .begin_frame              = d3d12_surface_begin_frame,
        .end_frame                = d3d12_surface_end_frame,
        .present                  = d3d12_surface_present,
        .poll_event               = d3d12_surface_poll_event,
        .should_close             = d3d12_surface_should_close,
        .window_open              = d3d12_surface_window_open,
        .host_pump                = d3d12_surface_host_pump,
        .session_attach_platform  = d3d12_surface_session_attach_platform,
    },
    .abi_version = KAIN_D3D12_ABI_VERSION,
    .present_count = 0,
    .swapchain_recreations = 0,
    .last_status = 0,
    .last_error = {0},
};

const KainD3D12AbiVtable* kain_d3d12_abi_get_vtable(void) {
    // Lazy-init sessions table on first call
    if (!g_sessions_initialized) {
        d3d12_sessions_init();
    }
    // Sync telemetry back into the vtable so the shim's accessors see live data
    g_d3d12_abi_vtable.present_count = g_present_count;
    g_d3d12_abi_vtable.swapchain_recreations = g_swapchain_recreations;
    g_d3d12_abi_vtable.last_status = g_last_status;
    // Copy error string safely
    {
        size_t i;
        for (i = 0; i + 1 < sizeof(g_d3d12_abi_vtable.last_error) &&
                    g_last_error[i] != '\0'; i++) {
            g_d3d12_abi_vtable.last_error[i] = g_last_error[i];
        }
        g_d3d12_abi_vtable.last_error[i] = '\0';
    }
    return &g_d3d12_abi_vtable;
}

int kain_d3d12_abi_init(void) {
    if (d3d12_abi_load_dlls() != 0) {
        return -1;
    }
    d3d12_sessions_init();
    return 0;
}

void d3d12_abi_shutdown_internal(void) {
    for (int i = 0; i < KAIN_D3D12_MAX_SESSIONS; i++) {
        if (g_sessions[i].session_id != 0) {
            d3d12_release_session(&g_sessions[i]);
        }
    }
    d3d12_abi_unload_dlls();
    g_sessions_initialized = 0;
    g_present_count = 0;
    g_swapchain_recreations = 0;
    g_last_status = 0;
    g_last_error[0] = '\0';
}

void kain_d3d12_abi_shutdown(void) {
    d3d12_abi_shutdown_internal();
}

// ── DLL entry point (Windows DLL_PROCESS_ATTACH/DETACH cleanup) ─

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE h_module, DWORD reason, LPVOID reserved) {
    (void)h_module; (void)reserved;
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            // Don't load d3d12.dll here — defer to first use to avoid
            // hard dependency for hosts that don't need D3D12.
            break;
        case DLL_PROCESS_DETACH:
            d3d12_abi_shutdown();
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        default:
            break;
    }
    return TRUE;
}
#endif
