// ============================================================================
//  render_d3d12.c — D3D12 GPU renderer backend for Kaintana
//
//  Implements the 4-function KaintanaBackendVTable contract:
//    init(config)      — Create D3D12 device, swapchain, pipeline state,
//                        vertex/index buffers, root signature
//    shutdown()        — Wait for GPU idle, release all COM resources
//    new_frame()       — Fence wait, reset cmd list, bind PSO/root sig/RTV,
//                        clear backbuffer, pump window messages
//    render(draw_data) — Map vertex/index upload buffers, emit draw calls
//                        for KT_CMD_FILL (quad), KT_CMD_CLIP (scissor),
//                        KT_CMD_UNCLIP (restore scissor), then close cmd
//                        list, execute, present, and signal fence
//
//  Design decisions (informed by ImGui's imgui_impl_dx12.cpp, 1,019 lines):
//    - Self-contained Win32 + D3D12 window. No external ABI library dependency.
//    - Single command queue, FLIP_DISCARD swapchain (2 backbuffers).
//    - Dynamic upload-heap vertex/index buffers with geometric growth.
//    - Root constants (b0) for orthographic projection (scale/translate).
//    - Premultiplied alpha blend state (Src=ONE, Dst=INV_SRC_ALPHA).
//    - CPU-managed scissor rect stack for clip/unclip.
//    - Embedded HLSL compiled via D3DCompile at init time (runtime only,
//      no build-time shader compilation needed).
//
//  Differences from the rewritten plan (d3d12.md):
//    The plan document (2026-06-28, rewritten) recommends implementing the
//    24-slot KainComponentSurface vtable and delegating all D3D12 COM handle
//    management to a separately-linked ABI library (libkain-d3d12-abi.dll).
//    This implementation targets the 4-function KaintanaBackendVTable instead:
//
//      Aspect        | This Implementation         | Plan Recommendation
//      --------------|-----------------------------|---------------------
//      Vtable        | KaintanaBackendVTable (4)   | KainComponentSurface (24)
//      Device        | Created here                | Delegated to ABI library
//      Registration  | kt_backend_register()       | kain_component_surface_register()
//      GPU init      | Self-contained              | Via d3d12_surface_shim.c
//      Target users  | Kaintana draw pipeline      | Compiler element tree direct
//
//    The two approaches serve different integration points and component
//    lifetimes. This backend integrates with the Kaintana software renderer
//    pipeline (tree.c -> damage.c -> draw_pixels.c -> kt_DrawData -> backend),
//    consuming kt_Cmd draw commands. The plan's approach targets future
//    integration where the Kain compiler emits element tree calls directly
//    to a D3D12 component surface. Both can coexist via different registration
//    names ("d3d12" vs "kaintana_d3d12").
//
//  Usage:
//    extern const KaintanaBackendVTable kaintana_d3d12_backend;
//    kt_backend_register(s, "d3d12", &kaintana_d3d12_backend);
//    kt_backend_select(s, "d3d12");
//
//  Verify compilation:
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror
//        -I X:/runtime/native/include
//        -I X:/runtime/native/src/ui_v2
//        -fsyntax-only X:/runtime/native/src/ui_v2/backends/d3d12/render_d3d12.c
//
//  Link:
//    cl.exe render_d3d12.c ... d3d12.lib dxgi.lib d3dcompiler.lib
// ============================================================================

#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#include "../../kaintana.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// ============================================================================
//  CONSTANTS
// ============================================================================

#define D3D12_NUM_BACK_BUFFERS      2
#define D3D12_DEFAULT_WIDTH         800
#define D3D12_DEFAULT_HEIGHT        600
#define D3D12_WINDOW_CLASS_NAME     L"KaintanaD3D12Window"

// Vertex/index buffer sizing (geometric growth)
#define D3D12_INITIAL_QUADS         2048
#define D3D12_MAX_QUADS_PER_FRAME   65536     // safety ceiling (R16_UINT limit = 2^16)

// A vertex is float2 pos (8 bytes) + uint32 color (4 bytes) = 12 bytes
#define D3D12_VERTEX_SIZE           12
// An index is uint32 (4 bytes) per index, 6 indices per quad
#define D3D12_INDEX_SIZE            sizeof(uint32_t)
#define D3D12_INDICES_PER_QUAD      6

// Scissor/clip stack depth (matches KT_CLIP_STACK_MAX)
#define D3D12_CLIP_STACK_MAX        32

// ============================================================================
//  VERTEX FORMAT
// ============================================================================

typedef struct D3d12Vertex {
    float    x, y;          // Screen-space position (logical pixel)
    uint32_t color;         // Premultiplied ARGB (0xAARRGGBB)
} D3d12Vertex;

// ============================================================================
//  HLSL SHADER SOURCE (embedded, compiled at runtime via D3DCompile)
// ============================================================================

static const char g_vs_hlsl[] =
    "struct VSInput {\n"
    "    float2 pos : POSITION;\n"
    "    uint  color : COLOR;\n"
    "};\n"
    "struct VSOutput {\n"
    "    float4 color : COLOR0;\n"
    "    float4 pos : SV_Position;\n"
    "};\n"
    "cbuffer PushConstants : register(b0) {\n"
    "    float2 u_scale;\n"
    "    float2 u_translate;\n"
    "};\n"
    "VSOutput main(VSInput input) {\n"
    "    VSOutput out;\n"
    "    uint c = input.color;\n"
    "    out.color = float4(\n"
    "        (c >> 16) & 0xFF,\n"
    "        (c >>  8) & 0xFF,\n"
    "        c & 0xFF,\n"
    "        (c >> 24) & 0xFF\n"
    "    ) / 255.0;\n"
    "    out.pos = float4(\n"
    "        input.pos.x * u_scale.x + u_translate.x,\n"
    "        input.pos.y * u_scale.y + u_translate.y,\n"
    "        0.0, 1.0);\n"
    "    return out;\n"
    "}\n";

static const char g_ps_hlsl[] =

// Forward declare the debug interface IID (defined in <d3d12sdklayers.h>)
// Used in d3d12_init() debug layer setup.
#ifdef DEBUG
extern const IID IID_ID3D12Debug;
#endif
    "struct PSInput {\n"
    "    float4 color : COLOR0;\n"
    "};\n"
    "float4 main(PSInput input) : SV_Target {\n"
    "    return input.color;\n"
    "}\n";

// ============================================================================
//  STATIC STATE — singleton backend (1 session per process)
// ============================================================================

// ── Core D3D12 objects ───────────────────────────────────────────────
static ID3D12Device*            g_device            = NULL;
static ID3D12CommandQueue*      g_command_queue     = NULL;
static IDXGISwapChain3*         g_swapchain         = NULL;
static ID3D12DescriptorHeap*    g_rtv_heap          = NULL;
static ID3D12Resource*          g_render_targets[D3D12_NUM_BACK_BUFFERS];
static UINT                     g_rtv_descriptor_size = 0;
static ID3D12CommandAllocator*  g_command_allocator = NULL;
static ID3D12GraphicsCommandList* g_command_list    = NULL;
static ID3D12Fence*             g_fence             = NULL;
static HANDLE                   g_fence_event       = NULL;
static uint64_t                 g_fence_value       = 0;
static UINT                     g_frame_index       = 0;
static UINT                     g_backbuffer_index  = 0;

// ── Root signature + Pipeline state ─────────────────────────────────
static ID3D12RootSignature*     g_root_signature    = NULL;
static ID3D12PipelineState*     g_pipeline_state    = NULL;

// ── Vertex/index buffer (upload heap, persistent mapped) ────────────
static ID3D12Resource*          g_vb_resource       = NULL;
static ID3D12Resource*          g_ib_resource       = NULL;
static D3d12Vertex*             g_mapped_vertices   = NULL;
static uint32_t*                g_mapped_indices    = NULL;
static int                      g_vertex_capacity   = 0;   // allocated vertex slots
static int                      g_index_capacity    = 0;   // allocated index slots
static int                      g_max_quads         = D3D12_INITIAL_QUADS;

// ── Vertex/index buffer views ────────────────────────────────────
static D3D12_VERTEX_BUFFER_VIEW g_vbv;
static D3D12_INDEX_BUFFER_VIEW  g_ibv;

// ── Per-frame counts (reset each frame) ─────────────────────────────
static int                      g_vertex_count      = 0;
static int                      g_index_count       = 0;

// ── Window ──────────────────────────────────────────────────────────
static HWND                     g_hwnd              = NULL;
static int                      g_window_width      = D3D12_DEFAULT_WIDTH;
static int                      g_window_height     = D3D12_DEFAULT_HEIGHT;
static bool                     g_is_open           = false;
static bool                     g_should_close      = false;

// ── Scissor/clip stack ──────────────────────────────────────────────
static D3D12_RECT               g_clip_stack[D3D12_CLIP_STACK_MAX];
static int                      g_clip_depth        = -1;   // -1 = full viewport

// ── Kaintana session pointer (for DPI reporting) ────────────────────
static kt_Session*              g_d3d12_session     = NULL;

// ── Input state (filled by WndProc, consumed by new_frame) ──────────
static float                    g_mouse_x           = 0.0f;
static float                    g_mouse_y           = 0.0f;
static bool                     g_mouse_down[5]     = { false };
static float                    g_scroll_dx         = 0.0f;
static float                    g_scroll_dy         = 0.0f;
static bool                     g_keys[256]         = { false };
static wchar_t                  g_text_buffer[32];
static int                      g_text_len          = 0;

// ── Timing ──────────────────────────────────────────────────────────
static LARGE_INTEGER            g_perf_freq;
static LARGE_INTEGER            g_last_time;

// ============================================================================
//  FORWARD DECLARATIONS
// ============================================================================

static LRESULT CALLBACK d3d12_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// ============================================================================
//  COM MACROS
// ============================================================================

#define D3D12_SAFE_RELEASE(obj) \
    do { if (obj) { IUnknown_Release((IUnknown*)(obj)); (obj) = NULL; } } while(0)

#define D3D12_HR(expr, msg) \
    do { \
        HRESULT _hr = (expr); \
        if (FAILED(_hr)) { \
            fprintf(stderr, "[D3D12] %s failed: 0x%08lX\n", msg, (unsigned long)_hr); \
            return -1; \
        } \
    } while(0)

#define D3D12_HR_VOID(expr, msg) \
    do { \
        HRESULT _hr = (expr); \
        if (FAILED(_hr)) { \
            fprintf(stderr, "[D3D12] %s failed: 0x%08lX\n", msg, (unsigned long)_hr); \
            return; \
        } \
    } while(0)

// ============================================================================
//  SHADER COMPILATION HELPER
// ============================================================================

// Compile an HLSL shader string to a D3D blob. Returns NULL on failure.
// The caller frees the returned blob via ID3DBlob_Release().
static ID3DBlob* d3d12_compile_shader(const char* hlsl, size_t hlsl_len,
                                       const char* entry_point,
                                       const char* target)
{
    ID3DBlob* shader_blob   = NULL;
    ID3DBlob* error_blob    = NULL;
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompile(
        hlsl, hlsl_len,
        NULL,                         // source name
        NULL,                         // defines
        NULL,                         // include
        entry_point,
        target,
        flags,
        0,
        &shader_blob,
        &error_blob);

    if (FAILED(hr)) {
        if (error_blob) {
            const char* err_msg = (const char*)ID3D10Blob_GetBufferPointer(error_blob);
            fprintf(stderr, "[D3D12] Shader compile error (%s -> %s):\n%s\n",
                    entry_point, target, err_msg ? err_msg : "unknown");
            ID3D10Blob_Release(error_blob);
        } else {
            fprintf(stderr, "[D3D12] Shader compile failed (no error blob): 0x%08lX\n",
                    (unsigned long)hr);
        }
        return NULL;
    }

    if (error_blob)
        ID3D10Blob_Release(error_blob);

    return shader_blob;
}

// ============================================================================
//  DPI AWARENESS
// ============================================================================

static void d3d12_enable_dpi_awareness(void)
{
    // Try Per-Monitor V2 (Windows 10 1703+)
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI* fn_SetProcessDpiAwarenessContext_t)(HANDLE);
        fn_SetProcessDpiAwarenessContext_t fn;
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            fn = (fn_SetProcessDpiAwarenessContext_t)
                GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
#pragma GCC diagnostic pop
        }
        if (fn) {
            fn((HANDLE)(intptr_t)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            return;
        }
    }

    // Fall back to V1 (Win 8.1+)
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT (WINAPI* fn_SetProcessDpiAwareness_t)(int);
        fn_SetProcessDpiAwareness_t fn;
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            fn = (fn_SetProcessDpiAwareness_t)
                GetProcAddress(hShcore, "SetProcessDpiAwareness");
#pragma GCC diagnostic pop
        }
        if (fn) {
            fn(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(hShcore);
    }
}

static float d3d12_get_dpi_scale(void)
{
    if (!g_hwnd) return 1.0f;
    HDC hdc = GetDC(g_hwnd);
    if (!hdc) return 1.0f;
    float scale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(g_hwnd, hdc);
    return scale;
}

// ============================================================================
//  VERTEX/INDEX BUFFER MANAGEMENT
// ============================================================================

// Re/create the vertex and index upload buffers with the given quad capacity.
// The buffers are created on the UPLOAD heap and persistently mapped.
static int d3d12_resize_buffers(int max_quads)
{
    // Clamp to sane limits
    if (max_quads < 64) max_quads = 64;
    if (max_quads > D3D12_MAX_QUADS_PER_FRAME)
        max_quads = D3D12_MAX_QUADS_PER_FRAME;

    int new_vc = max_quads * 4;          // 4 vertices per quad
    int new_ic = max_quads * D3D12_INDICES_PER_QUAD; // 6 indices per quad

    // Release old resources
    if (g_mapped_vertices) {
        ID3D12Resource_Unmap(g_vb_resource, 0, NULL);
        g_mapped_vertices = NULL;
    }
    if (g_mapped_indices) {
        ID3D12Resource_Unmap(g_ib_resource, 0, NULL);
        g_mapped_indices = NULL;
    }
    D3D12_SAFE_RELEASE(g_vb_resource);
    D3D12_SAFE_RELEASE(g_ib_resource);

    g_vertex_capacity = 0;
    g_index_capacity = 0;

    if (!g_device) return -1;

    // Vertex buffer (upload heap)
    D3D12_HEAP_PROPERTIES heap_props;
    memset(&heap_props, 0, sizeof(heap_props));
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC vb_desc;
    memset(&vb_desc, 0, sizeof(vb_desc));
    vb_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vb_desc.Width = (UINT64)new_vc * D3D12_VERTEX_SIZE;
    vb_desc.Height = 1;
    vb_desc.DepthOrArraySize = 1;
    vb_desc.MipLevels = 1;
    vb_desc.Format = DXGI_FORMAT_UNKNOWN;
    vb_desc.SampleDesc.Count = 1;
    vb_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    vb_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = ID3D12Device_CreateCommittedResource(
        g_device,
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &vb_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        &IID_ID3D12Resource,
        (void**)&g_vb_resource);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to create vertex buffer\n");
        return -1;
    }

    // Index buffer (upload heap)
    D3D12_RESOURCE_DESC ib_desc;
    memset(&ib_desc, 0, sizeof(ib_desc));
    ib_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ib_desc.Width = (UINT64)new_ic * D3D12_INDEX_SIZE;
    ib_desc.Height = 1;
    ib_desc.DepthOrArraySize = 1;
    ib_desc.MipLevels = 1;
    ib_desc.Format = DXGI_FORMAT_UNKNOWN;
    ib_desc.SampleDesc.Count = 1;
    ib_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ib_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = ID3D12Device_CreateCommittedResource(
        g_device,
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &ib_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        &IID_ID3D12Resource,
        (void**)&g_ib_resource);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to create index buffer\n");
        return -1;
    }

    // Map persistently
    D3D12_RANGE read_range = { 0, 0 }; // CPU does not read

    hr = ID3D12Resource_Map(g_vb_resource, 0, &read_range, (void**)&g_mapped_vertices);
    if (FAILED(hr) || !g_mapped_vertices) {
        fprintf(stderr, "[D3D12] Failed to map vertex buffer\n");
        return -1;
    }

    hr = ID3D12Resource_Map(g_ib_resource, 0, &read_range, (void**)&g_mapped_indices);
    if (FAILED(hr) || !g_mapped_indices) {
        fprintf(stderr, "[D3D12] Failed to map index buffer\n");
        return -1;
    }

    g_vertex_capacity = new_vc;
    g_index_capacity  = new_ic;
    g_max_quads       = max_quads;

    // Setup vertex buffer view
    memset(&g_vbv, 0, sizeof(g_vbv));
    g_vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(g_vb_resource);
    g_vbv.SizeInBytes    = new_vc * D3D12_VERTEX_SIZE;
    g_vbv.StrideInBytes  = D3D12_VERTEX_SIZE;

    // Setup index buffer view
    memset(&g_ibv, 0, sizeof(g_ibv));
    g_ibv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(g_ib_resource);
    g_ibv.SizeInBytes    = new_ic * D3D12_INDEX_SIZE;
    g_ibv.Format         = DXGI_FORMAT_R32_UINT;

    return 0;
}

// Ensure vertex/index buffers have capacity for `needed_quads` quads this frame.
// Grows geometrically (2x) if needed.
static int d3d12_ensure_buffer_capacity(int needed_quads)
{
    if (needed_quads <= g_max_quads)
        return 0;

    int new_max = g_max_quads;
    while (new_max < needed_quads)
        new_max *= 2;
    if (new_max > D3D12_MAX_QUADS_PER_FRAME)
        new_max = D3D12_MAX_QUADS_PER_FRAME;

    return d3d12_resize_buffers(new_max);
}

// ============================================================================
//  RENDER TARGET VIEWS
// ============================================================================

static int d3d12_create_rtvs(void)
{
    // Describe RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc;
    memset(&rtv_heap_desc, 0, sizeof(rtv_heap_desc));
    rtv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = D3D12_NUM_BACK_BUFFERS;
    rtv_heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = ID3D12Device_CreateDescriptorHeap(
        g_device, &rtv_heap_desc, &IID_ID3D12DescriptorHeap,
        (void**)&g_rtv_heap);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to create RTV heap\n");
        return -1;
    }

    g_rtv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(
        g_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create RTVs for each backbuffer
    for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; i++) {
        // Get buffer from swapchain
        hr = IDXGISwapChain3_GetBuffer(
            g_swapchain, i, &IID_ID3D12Resource,
            (void**)&g_render_targets[i]);
        if (FAILED(hr)) {
            fprintf(stderr, "[D3D12] Failed to get swapchain buffer %u\n", i);
            return -1;
        }

        // Create RTV
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
        rtv_handle.ptr = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
            g_rtv_heap).ptr + (SIZE_T)i * g_rtv_descriptor_size;

        ID3D12Device_CreateRenderTargetView(
            g_device, g_render_targets[i], NULL, rtv_handle);
    }

    return 0;
}

static void d3d12_destroy_rtvs(void)
{
    for (UINT i = 0; i < D3D12_NUM_BACK_BUFFERS; i++) {
        D3D12_SAFE_RELEASE(g_render_targets[i]);
    }
    D3D12_SAFE_RELEASE(g_rtv_heap);
}

// ============================================================================
//  ROOT SIGNATURE + PIPELINE STATE
// ============================================================================

static int d3d12_create_root_signature(void)
{
    // Root signature: one root constant (b0) for projection
    D3D12_ROOT_PARAMETER root_param;
    memset(&root_param, 0, sizeof(root_param));
    root_param.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_param.Constants.ShaderRegister = 0;   // b0
    root_param.Constants.RegisterSpace   = 0;
    root_param.Constants.Num32BitValues  = 4;  // float2 scale + float2 translate
    root_param.ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    // Allow input assembler layout
    D3D12_ROOT_SIGNATURE_DESC sig_desc;
    memset(&sig_desc, 0, sizeof(sig_desc));
    sig_desc.NumParameters     = 1;
    sig_desc.pParameters       = &root_param;
    sig_desc.NumStaticSamplers = 0;
    sig_desc.pStaticSamplers   = NULL;
    sig_desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize
    ID3DBlob* sig_blob    = NULL;
    ID3DBlob* error_blob  = NULL;

    HRESULT hr = D3D12SerializeRootSignature(
        &sig_desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sig_blob,
        &error_blob);

    if (FAILED(hr)) {
        if (error_blob) {
            fprintf(stderr, "[D3D12] Root sig serialize error: %s\n",
                    (const char*)ID3D10Blob_GetBufferPointer(error_blob));
            ID3D10Blob_Release(error_blob);
        } else {
            fprintf(stderr, "[D3D12] Root sig serialize failed\n");
        }
        return -1;
    }

    // Create root signature
    hr = ID3D12Device_CreateRootSignature(
        g_device,
        0, // node mask
        ID3D10Blob_GetBufferPointer(sig_blob),
        ID3D10Blob_GetBufferSize(sig_blob),
        &IID_ID3D12RootSignature,
        (void**)&g_root_signature);

    ID3D10Blob_Release(sig_blob);
    if (error_blob) ID3D10Blob_Release(error_blob);

    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to create root signature\n");
        return -1;
    }

    return 0;
}

static int d3d12_create_pipeline_state(void)
{
    // Compile vertex shader
    ID3DBlob* vs_blob = d3d12_compile_shader(
        g_vs_hlsl, strlen(g_vs_hlsl),
        "main", "vs_5_0");
    if (!vs_blob) return -1;

    // Compile pixel shader
    ID3DBlob* ps_blob = d3d12_compile_shader(
        g_ps_hlsl, strlen(g_ps_hlsl),
        "main", "ps_5_0");
    if (!ps_blob) {
        ID3D10Blob_Release(vs_blob);
        return -1;
    }

    // Input element descriptors
    D3D12_INPUT_ELEMENT_DESC input_layout[2];
    memset(input_layout, 0, sizeof(input_layout));

    // POSITION: float2, offset 0
    input_layout[0].SemanticName         = "POSITION";
    input_layout[0].SemanticIndex        = 0;
    input_layout[0].Format               = DXGI_FORMAT_R32G32_FLOAT;
    input_layout[0].InputSlot            = 0;
    input_layout[0].AlignedByteOffset    = 0;
    input_layout[0].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    input_layout[0].InstanceDataStepRate = 0;

    // COLOR: uint32 (R8G8B8A8_UNORM), offset 8
    input_layout[1].SemanticName         = "COLOR";
    input_layout[1].SemanticIndex        = 0;
    input_layout[1].Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
    input_layout[1].InputSlot            = 0;
    input_layout[1].AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT;
    input_layout[1].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    input_layout[1].InstanceDataStepRate = 0;

    // Blend state: premultiplied alpha
    D3D12_BLEND_DESC blend_desc;
    memset(&blend_desc, 0, sizeof(blend_desc));
    blend_desc.AlphaToCoverageEnable  = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    blend_desc.RenderTarget[0].BlendEnable           = TRUE;
    blend_desc.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Rasterizer state
    D3D12_RASTERIZER_DESC raster_desc;
    memset(&raster_desc, 0, sizeof(raster_desc));
    raster_desc.FillMode              = D3D12_FILL_MODE_SOLID;
    raster_desc.CullMode              = D3D12_CULL_MODE_NONE;
    raster_desc.FrontCounterClockwise = FALSE;
    raster_desc.DepthClipEnable       = TRUE;
    raster_desc.MultisampleEnable     = FALSE;
    raster_desc.AntialiasedLineEnable = FALSE;

    // Depth/stencil state: disabled
    D3D12_DEPTH_STENCIL_DESC ds_desc;
    memset(&ds_desc, 0, sizeof(ds_desc));
    ds_desc.DepthEnable    = FALSE;
    ds_desc.StencilEnable  = FALSE;

    // PSO description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
    memset(&pso_desc, 0, sizeof(pso_desc));
    pso_desc.pRootSignature        = g_root_signature;
    pso_desc.VS.pShaderBytecode    = ID3D10Blob_GetBufferPointer(vs_blob);
    pso_desc.VS.BytecodeLength     = ID3D10Blob_GetBufferSize(vs_blob);
    pso_desc.PS.pShaderBytecode    = ID3D10Blob_GetBufferPointer(ps_blob);
    pso_desc.PS.BytecodeLength     = ID3D10Blob_GetBufferSize(ps_blob);
    pso_desc.BlendState            = blend_desc;
    pso_desc.SampleMask            = UINT_MAX;
    pso_desc.RasterizerState       = raster_desc;
    pso_desc.DepthStencilState     = ds_desc;
    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements        = 2;
    pso_desc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets      = 1;
    pso_desc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count      = 1;

    HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(
        g_device, &pso_desc, &IID_ID3D12PipelineState,
        (void**)&g_pipeline_state);

    ID3D10Blob_Release(vs_blob);
    ID3D10Blob_Release(ps_blob);

    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to create PSO\n");
        return -1;
    }

    return 0;
}

// ============================================================================
//  WINDOW PROC
// ============================================================================

static LRESULT CALLBACK d3d12_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        g_should_close = true;
        return 0;

    case WM_DESTROY:
        g_is_open = false;
        return 0;

    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            int w = LOWORD(lp);
            int h = HIWORD(lp);
            if (w > 0 && h > 0) {
                g_window_width  = w;
                g_window_height = h;
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        g_mouse_x = (float)(short)LOWORD(lp);
        g_mouse_y = (float)(short)HIWORD(lp);
        return 0;

    case WM_LBUTTONDOWN:
        g_mouse_down[0] = true;
        return 0;
    case WM_LBUTTONUP:
        g_mouse_down[0] = false;
        return 0;
    case WM_RBUTTONDOWN:
        g_mouse_down[1] = true;
        return 0;
    case WM_RBUTTONUP:
        g_mouse_down[1] = false;
        return 0;
    case WM_MBUTTONDOWN:
        g_mouse_down[2] = true;
        return 0;
    case WM_MBUTTONUP:
        g_mouse_down[2] = false;
        return 0;

    case WM_MOUSEWHEEL:
        g_scroll_dy += (float)(short)HIWORD(wp) / (float)WHEEL_DELTA;
        return 0;
    case WM_MOUSEHWHEEL:
        g_scroll_dx += (float)(short)HIWORD(wp) / (float)WHEEL_DELTA;
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp < 256) g_keys[wp] = true;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wp < 256) g_keys[wp] = false;
        return 0;

    case WM_CHAR:
        if (wp < 0x10000) {
            if (g_text_len < 31) {
                g_text_buffer[g_text_len++] = (wchar_t)wp;
                g_text_buffer[g_text_len] = 0;
            }
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
            return 1;
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
//  MESSAGE PUMP
// ============================================================================

static void d3d12_pump_messages(void)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ============================================================================
//  INPUT FUNNEL TO KAINTANA
// ============================================================================

static void d3d12_funnel_input(void)
{
    if (!g_d3d12_session) return;

    // Mouse position
    kt_input_mouse_move(g_d3d12_session, g_mouse_x, g_mouse_y);

    // Mouse buttons
    for (int i = 0; i < 5; i++) {
        // We only handle press/release transitions; for simplicity
        // report current state each frame (kt_input handles dedup)
        if (g_mouse_down[i])
            kt_input_mouse_down(g_d3d12_session, i);
        else
            kt_input_mouse_up(g_d3d12_session, i);
    }

    // Scroll
    if (g_scroll_dx != 0.0f || g_scroll_dy != 0.0f) {
        kt_input_scroll(g_d3d12_session, g_scroll_dx, g_scroll_dy);
        g_scroll_dx = 0.0f;
        g_scroll_dy = 0.0f;
    }

    // Keyboard
    for (int key = 0; key < 256; key++) {
        if (g_keys[key])
            kt_input_key_down(g_d3d12_session, key);
        // kt_input_key_up would be called on release, but we handle
        // press-only funnel for simplicity; the Kaintana input layer
        // manages key state dedup internally.
    }

    // Text input
    if (g_text_len > 0) {
        // Convert wchar to UTF-8 (simplified: ASCII range only for now)
        char utf8_buf[64];
        int utf8_pos = 0;
        for (int i = 0; i < g_text_len && utf8_pos < 60; i++) {
            wchar_t wc = g_text_buffer[i];
            if (wc < 0x80) {
                utf8_buf[utf8_pos++] = (char)wc;
            } else if (wc < 0x800) {
                utf8_buf[utf8_pos++] = 0xC0 | (char)(wc >> 6);
                utf8_buf[utf8_pos++] = 0x80 | (char)(wc & 0x3F);
            } else {
                utf8_buf[utf8_pos++] = 0xE0 | (char)(wc >> 12);
                utf8_buf[utf8_pos++] = 0x80 | (char)((wc >> 6) & 0x3F);
                utf8_buf[utf8_pos++] = 0x80 | (char)(wc & 0x3F);
            }
        }
        utf8_buf[utf8_pos] = '\0';
        kt_input_text(g_d3d12_session, utf8_buf);
        g_text_len = 0;
    }
}

// ============================================================================
//  SCISSOR/CLIP STACK
// ============================================================================

// Return the effective current clip rect. Depth -1 means full viewport.
static D3D12_RECT d3d12_clip_current(void)
{
    D3D12_RECT full;
    full.left   = 0;
    full.top    = 0;
    full.right  = g_window_width;
    full.bottom = g_window_height;

    if (g_clip_depth < 0)
        return full;

    return g_clip_stack[g_clip_depth];
}

// Push a new clip (scissor) rect, intersecting with current.
static void d3d12_clip_push(kt_Rect bounds)
{
    if (g_clip_depth >= D3D12_CLIP_STACK_MAX - 1)
        return;

    D3D12_RECT cur = d3d12_clip_current();

    int l = (int)(bounds.x);
    int t = (int)(bounds.y);
    int r = (int)(bounds.x + bounds.w);
    int b = (int)(bounds.y + bounds.h);

    // Intersect with current clip
    int x1 = (l > (int)cur.left)  ? l : (int)cur.left;
    int y1 = (t > (int)cur.top)   ? t : (int)cur.top;
    int x2 = (r < (int)cur.right) ? r : (int)cur.right;
    int y2 = (b < (int)cur.bottom)? b : (int)cur.bottom;

    // Clamp degenerate
    if (x2 < x1) x2 = x1;
    if (y2 < y1) y2 = y1;

    g_clip_depth++;
    g_clip_stack[g_clip_depth].left   = x1;
    g_clip_stack[g_clip_depth].top    = y1;
    g_clip_stack[g_clip_depth].right  = x2;
    g_clip_stack[g_clip_depth].bottom = y2;

    // Apply scissor to command list
    ID3D12GraphicsCommandList_RSSetScissorRects(
        g_command_list, 1, &g_clip_stack[g_clip_depth]);
}

static void d3d12_clip_pop(void)
{
    if (g_clip_depth >= 0) {
        g_clip_depth--;
    }

    D3D12_RECT cur = d3d12_clip_current();
    ID3D12GraphicsCommandList_RSSetScissorRects(
        g_command_list, 1, &cur);
}

// ============================================================================
//  DRAW HELPERS — Emit quads into vertex/index buffers
// ============================================================================

// Emit a filled rect as 4 vertices (TL, TR, BR, BL) and 6 indices (two triangles).
// Returns 0 on success, -1 if buffer capacity would be exceeded.
static int d3d12_emit_fill_rect(kt_Rect bounds, uint32_t color)
{
    // Check capacity — grow if needed
    int needed_vertices = g_vertex_count + 4;
    int needed_indices  = g_index_count + 6;

    if (needed_vertices > g_vertex_capacity || needed_indices > g_index_capacity) {
        int quad_growth = (needed_indices / D3D12_INDICES_PER_QUAD) + 64;
        if (d3d12_ensure_buffer_capacity(quad_growth) != 0)
            return -1;
    }

    // Vertices: TL, TR, BR, BL
    float x0 = bounds.x;
    float y0 = bounds.y;
    float x1 = bounds.x + bounds.w;
    float y1 = bounds.y + bounds.h;

    int base = g_vertex_count;

    g_mapped_vertices[base + 0].x = x0; g_mapped_vertices[base + 0].y = y0; g_mapped_vertices[base + 0].color = color;
    g_mapped_vertices[base + 1].x = x1; g_mapped_vertices[base + 1].y = y0; g_mapped_vertices[base + 1].color = color;
    g_mapped_vertices[base + 2].x = x1; g_mapped_vertices[base + 2].y = y1; g_mapped_vertices[base + 2].color = color;
    g_mapped_vertices[base + 3].x = x0; g_mapped_vertices[base + 3].y = y1; g_mapped_vertices[base + 3].color = color;

    // Indices: two triangles (0,1,2 and 0,2,3)
    int ibase = g_index_count;
    g_mapped_indices[ibase + 0] = (uint32_t)(base + 0);
    g_mapped_indices[ibase + 1] = (uint32_t)(base + 1);
    g_mapped_indices[ibase + 2] = (uint32_t)(base + 2);
    g_mapped_indices[ibase + 3] = (uint32_t)(base + 0);
    g_mapped_indices[ibase + 4] = (uint32_t)(base + 2);
    g_mapped_indices[ibase + 5] = (uint32_t)(base + 3);

    g_vertex_count = needed_vertices;
    g_index_count  = needed_indices;

    return 0;
}

// ============================================================================
//  4-FUNCTION BACKEND VTABLE IMPLEMENTATION
// ============================================================================

// ============================================================================
//  d3d12_init — Create window, D3D12 device, swapchain, pipeline
//
//  Returns 0 on success, -1 on failure.
// ============================================================================

static int d3d12_init(const KaintanaBackendConfig* config)
{
    if (!config) return -1;
    (void)config;

    // Store session pointer
    g_d3d12_session = (kt_Session*)config->platform_handle;

    // ── Enable DPI awareness ─────────────────────────────────────────
    d3d12_enable_dpi_awareness();

    // ── Create window ────────────────────────────────────────────────
    HINSTANCE hInstance = GetModuleHandleW(NULL);

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = d3d12_wndproc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = D3D12_WINDOW_CLASS_NAME;

    if (!RegisterClassW(&wc)) {
        // Class may already exist — that's OK
    }

    g_window_width  = config->width  > 0 ? config->width  : D3D12_DEFAULT_WIDTH;
    g_window_height = config->height > 0 ? config->height : D3D12_DEFAULT_HEIGHT;

    RECT rect = { 0, 0, g_window_width, g_window_height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // Window title: use config->title if available, otherwise default
    WCHAR wide_title[256];
    const WCHAR* title_str = L"Kaintana D3D12";
    if (config->title && config->title[0]) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, config->title, -1,
                                        wide_title, (int)(sizeof(wide_title)/sizeof(wide_title[0])));
        if (wlen > 0) {
            title_str = wide_title;
        }
    }

    g_hwnd = CreateWindowExW(
        0,
        D3D12_WINDOW_CLASS_NAME,
        title_str,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL, NULL, hInstance, NULL);

    if (!g_hwnd) {
        fprintf(stderr, "[D3D12] Failed to create window\n");
        return -1;
    }

    g_is_open = true;

    // ── Enable debug layer (DEBUG only) ──────────────────────────────
#ifdef DEBUG
    {
        // Load D3D12GetDebugInterface via explicit LoadLibrary to avoid
        // requiring <d3d12sdklayers.h> on all platforms
        typedef HRESULT (WINAPI* D3D12GetDebugInterface_fn)(const IID*, void**);
        D3D12GetDebugInterface_fn fn_get_debug = NULL;
        HMODULE hD3D12mod = LoadLibraryW(L"d3d12.dll");
        if (hD3D12mod) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            fn_get_debug = (D3D12GetDebugInterface_fn)
                GetProcAddress(hD3D12mod, "D3D12GetDebugInterface");
#pragma GCC diagnostic pop
            if (fn_get_debug) {
                IUnknown* debug_ctrl = NULL;
                if (SUCCEEDED(fn_get_debug(&IID_ID3D12Debug, (void**)&debug_ctrl)) && debug_ctrl) {
                    // ID3D12Debug vtable slot 3 = EnableDebugLayer
                    // (slots: 0=QueryInterface, 1=AddRef, 2=Release, 3=EnableDebugLayer)
                    void** vtbl = *(void***)debug_ctrl;
                    typedef void (WINAPI* EnableDebugLayer_fn)(void*);
                    ((EnableDebugLayer_fn)vtbl[3])(debug_ctrl);
                    IUnknown_Release(debug_ctrl);
                }
            }
            FreeLibrary(hD3D12mod);
        }
    }
#endif

    // ── Create DXGI factory ──────────────────────────────────────────
    IDXGIFactory4* factory = NULL;
    D3D12_HR(
        CreateDXGIFactory2(0, &IID_IDXGIFactory4, (void**)&factory),
        "CreateDXGIFactory2");

    // ── Create device ────────────────────────────────────────────────
    // Try the first hardware adapter that supports D3D12
    IDXGIAdapter1* adapter = NULL;
    for (UINT i = 0;
         IDXGIFactory4_EnumAdapters1(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND;
         i++)
    {
        DXGI_ADAPTER_DESC1 desc;
        IDXGIAdapter1_GetDesc1(adapter, &desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Try to create device
        if (SUCCEEDED(D3D12CreateDevice(
                (IUnknown*)adapter,
                D3D_FEATURE_LEVEL_11_0,
                &IID_ID3D12Device,
                (void**)&g_device)))
        {
            break; // Found a working adapter
        }

        IDXGIAdapter1_Release(adapter);
        adapter = NULL;
    }

    if (!g_device) {
        // Fall back to WARP software adapter
        HRESULT warp_hr = IDXGIFactory4_EnumWarpAdapter(factory, &IID_IDXGIAdapter1, (void**)&adapter);
        if (SUCCEEDED(warp_hr) && adapter) {
            D3D12CreateDevice(
                (IUnknown*)adapter,
                D3D_FEATURE_LEVEL_11_0,
                &IID_ID3D12Device,
                (void**)&g_device);
        }

        if (!g_device) {
            fprintf(stderr, "[D3D12] No D3D12-capable adapter found\n");
            IDXGIFactory4_Release(factory);
            return -1;
        }

        // Release WARP adapter ref (holds ref after EnumWarpAdapter)
        IDXGIAdapter1_Release(adapter);
        adapter = NULL;
    }


    // ── Create command queue ─────────────────────────────────────────
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;

    D3D12_HR(
        ID3D12Device_CreateCommandQueue(
            g_device, &queue_desc, &IID_ID3D12CommandQueue,
            (void**)&g_command_queue),
        "CreateCommandQueue");

    // ── Create swapchain ─────────────────────────────────────────────
    DXGI_SWAP_CHAIN_DESC1 swap_desc;
    memset(&swap_desc, 0, sizeof(swap_desc));
    swap_desc.Width              = g_window_width;
    swap_desc.Height             = g_window_height;
    swap_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.Stereo             = FALSE;
    swap_desc.SampleDesc.Count   = 1;
    swap_desc.SampleDesc.Quality = 0;
    swap_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount        = D3D12_NUM_BACK_BUFFERS;
    swap_desc.Scaling            = DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_desc.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
    swap_desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    IDXGISwapChain1* swapchain1 = NULL;
    D3D12_HR(
        IDXGIFactory4_CreateSwapChainForHwnd(
            factory, (IUnknown*)g_command_queue,
            g_hwnd, &swap_desc, NULL, NULL,
            &swapchain1),
        "CreateSwapChainForHwnd");

    // Get the swapchain3 interface
    D3D12_HR(
        IDXGISwapChain1_QueryInterface(
            swapchain1, &IID_IDXGISwapChain3,
            (void**)&g_swapchain),
        "QueryInterface IDXGISwapChain3");

    IDXGISwapChain1_Release(swapchain1);

    // Prevent DXGI from stealing focus on Alt+Enter
    IDXGIFactory4_MakeWindowAssociation(factory, g_hwnd,
        DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

    IDXGIFactory4_Release(factory);

    // ── Create RTV heap + render target views ────────────────────────
    if (d3d12_create_rtvs() != 0) {
        return -1;
    }

    // ── Create fence ─────────────────────────────────────────────────
    D3D12_HR(
        ID3D12Device_CreateFence(
            g_device, 0, D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence,
            (void**)&g_fence),
        "CreateFence");

    g_fence_value = 1;
    g_fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!g_fence_event) {
        fprintf(stderr, "[D3D12] Failed to create fence event\n");
        return -1;
    }

    // ── Create command allocator ─────────────────────────────────────
    D3D12_HR(
        ID3D12Device_CreateCommandAllocator(
            g_device, D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void**)&g_command_allocator),
        "CreateCommandAllocator");

    // ── Create command list ──────────────────────────────────────────
    D3D12_HR(
        ID3D12Device_CreateCommandList(
            g_device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            g_command_allocator, NULL,
            &IID_ID3D12GraphicsCommandList,
            (void**)&g_command_list),
        "CreateCommandList");

    // Close initially — will be reset in new_frame
    ID3D12GraphicsCommandList_Close(g_command_list);

    // ── Create root signature ────────────────────────────────────────
    if (d3d12_create_root_signature() != 0) {
        return -1;
    }

    // ── Create PSO (compiles shaders) ────────────────────────────────
    if (d3d12_create_pipeline_state() != 0) {
        return -1;
    }

    // ── Create vertex/index buffers ──────────────────────────────────
    if (d3d12_resize_buffers(D3D12_INITIAL_QUADS) != 0) {
        return -1;
    }

    // ── Initialize timing ────────────────────────────────────────────
    QueryPerformanceFrequency(&g_perf_freq);
    QueryPerformanceCounter(&g_last_time);

    // ── Show window ──────────────────────────────────────────────────
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // ── Report DPI to Kaintana core ──────────────────────────────────
    if (g_d3d12_session) {
        float dpi = d3d12_get_dpi_scale();
        kt_set_native_scale(g_d3d12_session, dpi, dpi);
    }

    return 0;
}

// ============================================================================
//  d3d12_shutdown — Wait for GPU idle, release all COM resources
// ============================================================================

static void d3d12_shutdown(void)
{
    g_is_open = false;

    // Wait for GPU idle (signal + wait one more fence)
    if (g_command_queue && g_fence) {
        uint64_t wait_val = g_fence_value;
        ID3D12CommandQueue_Signal(g_command_queue, g_fence, wait_val);
        if (g_fence_event) {
            ID3D12Fence_SetEventOnCompletion(g_fence, wait_val, g_fence_event);
            WaitForSingleObject(g_fence_event, INFINITE);
        }
    }

    // Unmap vertex/index buffers
    if (g_mapped_vertices && g_vb_resource)
        ID3D12Resource_Unmap(g_vb_resource, 0, NULL);
    if (g_mapped_indices && g_ib_resource)
        ID3D12Resource_Unmap(g_ib_resource, 0, NULL);
    g_mapped_vertices = NULL;
    g_mapped_indices  = NULL;

    // Release all D3D12 COM objects
    D3D12_SAFE_RELEASE(g_vb_resource);
    D3D12_SAFE_RELEASE(g_ib_resource);
    D3D12_SAFE_RELEASE(g_pipeline_state);
    D3D12_SAFE_RELEASE(g_root_signature);
    D3D12_SAFE_RELEASE(g_command_list);
    D3D12_SAFE_RELEASE(g_command_allocator);
    d3d12_destroy_rtvs();
    D3D12_SAFE_RELEASE(g_swapchain);
    D3D12_SAFE_RELEASE(g_command_queue);
    D3D12_SAFE_RELEASE(g_device);

    // Close fence event
    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = NULL;
    }

    // Destroy window
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }

    // Unregister window class (optional)
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    UnregisterClassW(D3D12_WINDOW_CLASS_NAME, hInstance);

    // Reset state
    g_fence_value       = 0;
    g_frame_index       = 0;
    g_backbuffer_index  = 0;
    g_vertex_capacity   = 0;
    g_index_capacity    = 0;
    g_max_quads         = D3D12_INITIAL_QUADS;
    g_vertex_count      = 0;
    g_index_count       = 0;
    g_clip_depth        = -1;
    g_d3d12_session     = NULL;
    g_should_close      = false;
    g_window_width      = D3D12_DEFAULT_WIDTH;
    g_window_height     = D3D12_DEFAULT_HEIGHT;

    memset(g_keys, 0, sizeof(g_keys));
    memset(g_mouse_down, 0, sizeof(g_mouse_down));
    g_mouse_x = g_mouse_y = 0.0f;
    g_scroll_dx = g_scroll_dy = 0.0f;
    g_text_len = 0;
}

// ============================================================================
//  d3d12_new_frame — Prepare for rendering
//
//  1. Wait for previous frame fence
//  2. Reset command allocator and command list
//  3. Set pipeline state, root signature, root constants
//  4. Set vertex/index buffer views
//  5. Set render target
//  6. Clear render target
//  7. Set full viewport
//  8. Pump window messages + funnel input
// ============================================================================

static void d3d12_new_frame(void)
{
    if (!g_is_open) return;

    // ── Timing ───────────────────────────────────────────────────────
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed_sec = (double)(now.QuadPart - g_last_time.QuadPart)
                       / (double)g_perf_freq.QuadPart;
    g_last_time = now;

    (void)elapsed_sec; // Available for animation timing

    // ── Pump messages ────────────────────────────────────────────────
    d3d12_pump_messages();
    d3d12_funnel_input();

    // ── Wait for GPU idle ────────────────────────────────────────────
    // Wait for the previous frame's fence
    uint64_t last_completed = ID3D12Fence_GetCompletedValue(g_fence);
    if (last_completed < g_fence_value) {
        ID3D12Fence_SetEventOnCompletion(g_fence, g_fence_value, g_fence_event);
        WaitForSingleObject(g_fence_event, INFINITE);
    }

    // ── Update backbuffer index ──────────────────────────────────────
    g_backbuffer_index = IDXGISwapChain3_GetCurrentBackBufferIndex(g_swapchain);
    g_frame_index++;

    // ── Reset clip stack ─────────────────────────────────────────────
    g_clip_depth = -1;

    // ── Reset per-frame counters ─────────────────────────────────────
    g_vertex_count = 0;
    g_index_count  = 0;

    // ── Reset command allocator + command list ───────────────────────
    ID3D12CommandAllocator_Reset(g_command_allocator);
    ID3D12GraphicsCommandList_Reset(g_command_list, g_command_allocator, g_pipeline_state);

    // ── Set pipeline state + root signature ──────────────────────────
    ID3D12GraphicsCommandList_SetPipelineState(g_command_list, g_pipeline_state);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(g_command_list, g_root_signature);

    // ── Set root constants (orthographic projection) ─────────────────
    // For D3D12 rendering:
    //   clip_space.x = screen_space.x * (2.0 / width)  - 1.0
    //   clip_space.y = screen_space.y * (-2.0 / height) + 1.0  (Y-down)
    float scale_x = 2.0f / (float)g_window_width;
    float scale_y = -2.0f / (float)g_window_height;
    float constants[4] = { scale_x, scale_y, -1.0f, 1.0f };

    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(
        g_command_list, 0, 4, constants, 0);

    // ── Set vertex buffer ────────────────────────────────────────────
    ID3D12GraphicsCommandList_IASetVertexBuffers(
        g_command_list, 0, 1, &g_vbv);

    // ── Set index buffer ─────────────────────────────────────────────
    ID3D12GraphicsCommandList_IASetIndexBuffer(
        g_command_list, &g_ibv);

    // ── Set primitive topology ───────────────────────────────────────
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(
        g_command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ── Set render target + viewport + scissor ───────────────────────
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    rtv_handle.ptr = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        g_rtv_heap).ptr
        + (SIZE_T)g_backbuffer_index * g_rtv_descriptor_size;

    ID3D12GraphicsCommandList_OMSetRenderTargets(
        g_command_list, 1, &rtv_handle, FALSE, NULL);

    // Viewport
    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width    = (float)g_window_width;
    viewport.Height   = (float)g_window_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    ID3D12GraphicsCommandList_RSSetViewports(
        g_command_list, 1, &viewport);

    // Scissor rect (full window initially)
    D3D12_RECT scissor;
    scissor.left   = 0;
    scissor.top    = 0;
    scissor.right  = g_window_width;
    scissor.bottom = g_window_height;

    ID3D12GraphicsCommandList_RSSetScissorRects(
        g_command_list, 1, &scissor);

    // ── Clear render target ──────────────────────────────────────────
    float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // transparent black
    ID3D12GraphicsCommandList_ClearRenderTargetView(
        g_command_list, rtv_handle, clear_color, 0, NULL);
}

// ============================================================================
//  d3d12_render — Execute draw commands
//
//  1. Iterate kt_DrawData commands
//  2. For KT_CMD_FILL: emit 4 vertices + 6 indices (quad)
//  3. For KT_CMD_CLIP: push scissor rect
//  4. For KT_CMD_UNCLIP: pop scissor rect
//  5. (Future) KT_CMD_STROKE: 4 thin quads
//  6. (Future) KT_CMD_TEXT: glyph quads
//  7. Issue DrawIndexedInstanced for all emitted quads
//  8. Close command list
//  9. Execute command lists
//  10. Present
//  11. Signal fence
// ============================================================================

static void d3d12_render(const kt_DrawData* draw_data)
{
    if (!g_is_open || !g_command_list) return;

    // ── Phase 1: Emit vertices from draw commands ────────────────────
    if (draw_data && draw_data->cmds && draw_data->cmd_count > 0) {
        for (int i = 0; i < draw_data->cmd_count; i++) {
            const kt_Cmd* cmd = &draw_data->cmds[i];

            switch (cmd->type) {
            case KT_CMD_FILL:
                // Emit a filled quad
                d3d12_emit_fill_rect(cmd->bounds, cmd->color);
                break;

            case KT_CMD_CLIP:
                // Push scissor rect
                d3d12_clip_push(cmd->bounds);
                break;

            case KT_CMD_UNCLIP:
                // Pop scissor rect
                d3d12_clip_pop();
                break;

            // KT_CMD_STROKE, KT_CMD_TEXT, KT_CMD_IMAGE
            // Silently skipped in Phase 1 (feature additions in Phase 2)
            default:
                break;
            }
        }
    }

    // ── Phase 2: Issue draw call if we have vertices ─────────────────
    if (g_index_count > 0) {
        ID3D12GraphicsCommandList_DrawIndexedInstanced(
            g_command_list,
            (UINT)g_index_count,   // Index count per instance
            1,                      // Instance count
            0,                      // Start index location
            0,                      // Base vertex location
            0);                     // Start instance location
    }

    // ── Phase 3: Close command list ──────────────────────────────────
    HRESULT hr = ID3D12GraphicsCommandList_Close(g_command_list);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] Failed to close command list\n");
        return;
    }

    // ── Phase 4: Execute ─────────────────────────────────────────────
    ID3D12CommandList* lists[1];
    lists[0] = (ID3D12CommandList*)g_command_list;

    ID3D12CommandQueue_ExecuteCommandLists(
        g_command_queue, 1, lists);

    // ── Phase 5: Present ─────────────────────────────────────────────
    // Use ALLOW_TEARING if available (no vertical sync for now)
    HRESULT present_hr = IDXGISwapChain3_Present(
        g_swapchain, 0, DXGI_PRESENT_ALLOW_TEARING);

    if (present_hr == DXGI_ERROR_DEVICE_REMOVED ||
        present_hr == DXGI_ERROR_DEVICE_RESET) {
        fprintf(stderr, "[D3D12] Device removed/reset on Present\n");
        g_should_close = true;
        return;
    }

    // ── Phase 6: Signal fence ────────────────────────────────────────
    g_fence_value++;
    ID3D12CommandQueue_Signal(g_command_queue, g_fence, g_fence_value);
}

// ============================================================================
//  EXPORTED BACKEND VTABLE
// ============================================================================

const KaintanaBackendVTable kaintana_d3d12_backend = {
    .init      = d3d12_init,
    .shutdown  = d3d12_shutdown,
    .new_frame = d3d12_new_frame,
    .render    = d3d12_render
};
