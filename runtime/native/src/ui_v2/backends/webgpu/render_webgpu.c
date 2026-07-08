// ============================================================================
//  render_webgpu.c — WebGPU KainComponentSurface vtable implementation
//
//  Implements the 24-slot KainComponentSurface vtable for WebGPU (wgpu-native
//  on native targets, browser WebGPU on WASM). Uses the existing ABI library
//  (extras/webgpu-abi/) for device/adapter/swapchain creation.
//
//  Approach A from webgpu.md: renders from kt_DrawData (intermediate command
//  buffer from draw_pixels.c). The WebGPU backend reads kt_Cmd array after
//  kt_end() and translates to WGSL draw calls.
//
//  Compilation:
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror
//        -I X:/runtime/native/include
//        -I X:/runtime/native/src/ui_v2
//        -I X:/runtime/native/extras/webgpu-abi
//        -fsyntax-only backends/webgpu/render_webgpu.c
//
//  P0 Tasks:
//    WGPU-001: All 24 vtable slots implemented
//    WGPU-004: Vertex buffer (64KB, CopyDst | Vertex)
//    WGPU-005: Uniform buffer (16 bytes, scale.xy + translate.xy)
//    WGPU-006: kt_DrawData -> draw calls (KT_CMD_FILL, KT_CMD_STROKE)
//    WGPU-007: Scissor/clip stack (32-depth CPU side)
//    WGPU-008: Full frame lifecycle: begin_frame -> present
//    WGPU-013: GCC -std=c11 -Wall -Wextra -pedantic clean
//
// ============================================================================

// ============================================================================
//  INCLUDES
// ============================================================================

#include "../../internal.h"                    // kaintana.h, core runtime
#include "../../../../include/webgpu_loader_subset.h"  // WGPU handle types
#include "../../../../extras/webgpu-abi/webgpu_abi.h" // ABI library types

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
//  LOCAL WGPU TYPE DEFINITIONS (subset of webgpu.h struct layouts)
//
//  The webgpu_loader_subset.h header provides handle types (uintptr_t) and
//  PFN prototypes for device/swapchain creation. Kaintana's backend needs
//  additional descriptor structs and render-pass PFNs not in the subset.
//
//  These type definitions match wgpu-native's C API ABI. All descriptor
//  structs use standard layout (no padding surprises on x64).
// ============================================================================

// ── Chained struct for descriptor extensions ─────────────────────────────
typedef struct WGPUChainedStruct {
    struct WGPUChainedStruct* next;
    uint32_t                 sType;
} WGPUChainedStruct;

// ── Shader module descriptor (WGSL) ──────────────────────────────────────
typedef struct WGPUShaderModuleWGSLDescriptor {
    WGPUChainedStruct chain;
    const char*       code;
} WGPUShaderModuleWGSLDescriptor;

typedef struct WGPUShaderModuleDescriptor {
    WGPUChainedStruct const* nextInChain;
    const char*              label;
} WGPUShaderModuleDescriptor;

// ── Buffer descriptor ────────────────────────────────────────────────────
typedef struct WGPUBufferDescriptor {
    WGPUChainedStruct const* nextInChain;
    const char*              label;
    uint64_t                 size;
    WGPUBufferUsageFlags     usage;
    bool                     mappedAtCreation;
} WGPUBufferDescriptor;

// ── Bind group layout ────────────────────────────────────────────────────
typedef struct WGPUBufferBindingLayout {
    WGPUChainedStruct nextInChain;
    uint32_t          type;              // WGPUBufferBindingType (0=uniform)
    bool              hasDynamicOffset;
    uint64_t          minBindingSize;
} WGPUBufferBindingLayout;
// Note: the nextInChain for buffer binding is prepadding; wgpu-native
// reads [next, sType] then [type, hasDynamicOffset, minBindingSize].

typedef struct WGPUBindGroupLayoutEntry {
    WGPUChainedStruct      nextInChain;  // padding: next+type
    uint32_t               binding;
    uint32_t               visibility;   // WGPUShaderStageFlags
    WGPUBufferBindingLayout buffer;      // inline sub-struct
    uint64_t               sampler[3];   // padding for unused fields
} WGPUBindGroupLayoutEntry;

typedef struct WGPUBindGroupLayoutDescriptor {
    WGPUChainedStruct const*     nextInChain;
    const char*                  label;
    uint32_t                     entryCount;
    const WGPUBindGroupLayoutEntry* entries;
} WGPUBindGroupLayoutDescriptor;

// ── Pipeline layout ──────────────────────────────────────────────────────
typedef struct WGPUPipelineLayoutDescriptor {
    WGPUChainedStruct const* nextInChain;
    const char*              label;
    uint32_t                 bindGroupLayoutCount;
    WGPUBindGroupLayout*     bindGroupLayouts;
} WGPUPipelineLayoutDescriptor;

// ── Bind group ───────────────────────────────────────────────────────────
typedef struct WGPUBindGroupEntry {
    WGPUChainedStruct nextInChain;       // padding: next+type
    uint32_t          binding;
    WGPUBuffer        buffer;
    uint64_t          offset;
    uint64_t          size;
    uint64_t          sampler;           // WGPUSampler (unused)
    uint64_t          textureView;       // WGPUTextureView (unused)
} WGPUBindGroupEntry;

typedef struct WGPUBindGroupDescriptor {
    WGPUChainedStruct const* nextInChain;
    const char*              label;
    WGPUPipelineLayout       layout;
    uint32_t                 entryCount;
    const WGPUBindGroupEntry* entries;
} WGPUBindGroupDescriptor;

// ── Vertex input ─────────────────────────────────────────────────────────
typedef struct WGPUVertexAttribute {
    WGPUVertexFormat format;
    uint64_t         offset;
    uint32_t         shaderLocation;
} WGPUVertexAttribute;

typedef struct WGPUVertexBufferLayout {
    uint64_t                 arrayStride;
    uint32_t                 stepMode;      // WGPUVertexStepMode (0=vertex)
    uint32_t                 attributeCount;
    const WGPUVertexAttribute* attributes;
} WGPUVertexBufferLayout;

// ── Color blend state ────────────────────────────────────────────────────
typedef struct WGPUBlendComponent {
    uint32_t operation;  // WGPUBlendOperation (0=Add)
    uint32_t srcFactor;  // WGPUBlendFactor
    uint32_t dstFactor;  // WGPUBlendFactor
} WGPUBlendComponent;

typedef struct WGPUColorTargetState {
    WGPUChainedStruct const* nextInChain;
    WGPUTextureFormat        format;
    WGPUBlendComponent*      blend;
    uint32_t                 writeMask;     // WGPUColorWriteMaskFlags
} WGPUColorTargetState;

// ── Fragment state ───────────────────────────────────────────────────────
typedef struct WGPUFragmentState {
    WGPUChainedStruct const* nextInChain;
    WGPUShaderModule         module;
    const char*              entryPoint;
    uint32_t                 targetCount;
    const WGPUColorTargetState* targets;
} WGPUFragmentState;

// ── Full render pipeline descriptor ──────────────────────────────────────
typedef struct WGPURenderPipelineDescriptor {
    WGPUChainedStruct const*      nextInChain;
    const char*                   label;
    WGPUPipelineLayout            layout;
    WGPUVertexBufferLayout const* vertex;
    uint32_t                      vertexBufferCount;
    WGPUPrimitiveTopology         primitiveTopology;
    uint64_t                      stripIndexFormat;
    uint32_t                      frontFace;
    uint32_t                      cullMode;
    WGPUFragmentState*            fragment;
    uint64_t                      depthStencil[3]; // depthStencilAttachment + padding
    uint32_t                      multisample_count;
    uint32_t                      multisample_mask;
    uint32_t                      multisample_alphaToCoverageEnabled;
} WGPURenderPipelineDescriptor;

// ── Command encoder ──────────────────────────────────────────────────────
typedef struct WGPUCommandEncoderDescriptor {
    WGPUChainedStruct const* nextInChain;
    const char*              label;
} WGPUCommandEncoderDescriptor;

// ── Color + render pass ──────────────────────────────────────────────────
typedef struct WGPUColor {
    double r, g, b, a;
} WGPUColor;

typedef struct WGPURenderPassColorAttachment {
    WGPUTextureView  view;
    uint32_t         resolveTarget;   // padding
    uint32_t         _pad0;
    uint32_t         loadOp;          // WGPULoadOp (0=Load, 1=Clear)
    uint32_t         storeOp;         // WGPUStoreOp (0=Store)
    WGPUColor        clearValue;
} WGPURenderPassColorAttachment;

typedef struct WGPURenderPassDescriptor {
    WGPUChainedStruct const*           nextInChain;
    const char*                        label;
    uint32_t                           colorAttachmentCount;
    const WGPURenderPassColorAttachment* colorAttachments;
    uint64_t                           depthStencilAttachment[4];
    uint64_t                           occlusionQuerySet;
} WGPURenderPassDescriptor;

// ============================================================================
//  MISSING PFN DECLARATIONS (not in webgpu_loader_subset.h)
//
//  These are called by Kaintana's backend for rendering, not for device
//  setup. They match the wgpu-native C ABI and are resolved at link time
//  against wgpu_native.dll or Dawn.
// ============================================================================

// Buffer/queue operations
void wgpuQueueWriteBuffer(WGPUQueue queue,
                          WGPUBuffer buffer,
                          uint64_t bufferOffset,
                          const void* data,
                          uint64_t size);

// Texture view
void wgpuTextureViewRelease(WGPUTextureView view);

// Bind group and layout release (missing from subset)
void wgpuBindGroupRelease(WGPUBindGroup group);
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout layout);
void wgpuPipelineLayoutRelease(WGPUPipelineLayout layout);

// Render pass operations
void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder encoder,
                                      WGPURenderPipeline pipeline);
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder encoder,
                                       uint32_t groupIndex,
                                       WGPUBindGroup group,
                                       uint32_t dynamicOffsetCount,
                                       const uint32_t* dynamicOffsets);
void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder encoder,
                                          uint32_t slot,
                                          WGPUBuffer buffer,
                                          uint64_t offset,
                                          uint64_t size);
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder encoder,
                               uint32_t vertexCount,
                               uint32_t instanceCount,
                               uint32_t firstVertex,
                               uint32_t firstInstance);

// ============================================================================
//  WGSL SHADER SOURCE (embedded strings)
// ============================================================================

static const char g_webgpu_vert_wgsl[] =
    "struct VSInput {\n"
    "    @location(0) pos : vec2<f32>;\n"
    "    @location(1) color : u32;\n"
    "};\n"
    "struct VSOutput {\n"
    "    @builtin(position) position : vec4<f32>;\n"
    "    @location(0) color : vec4<f32>;\n"
    "};\n"
    "struct Uniforms {\n"
    "    scale_x : f32,\n"
    "    scale_y : f32,\n"
    "    translate_x : f32,\n"
    "    translate_y : f32,\n"
    "};\n"
    "@group(0) @binding(0) var<uniform> u : Uniforms;\n"
    "@vertex\n"
    "fn main(input : VSInput) -> VSOutput {\n"
    "    var output : VSOutput;\n"
    "    let r = f32((input.color >> 16u) & 0xFFu) / 255.0;\n"
    "    let g = f32((input.color >> 8u) & 0xFFu) / 255.0;\n"
    "    let b = f32(input.color & 0xFFu) / 255.0;\n"
    "    let a = f32((input.color >> 24u) & 0xFFu) / 255.0;\n"
    "    output.color = vec4<f32>(r, g, b, a);\n"
    "    output.position = vec4<f32>(\n"
    "        input.pos.x * u.scale_x + u.translate_x,\n"
    "        input.pos.y * u.scale_y + u.translate_y,\n"
    "        0.0, 1.0);\n"
    "    return output;\n"
    "}\n";

static const char g_webgpu_frag_wgsl[] =
    "@fragment\n"
    "fn main(@location(0) color : vec4<f32>) -> @location(0) vec4<f32> {\n"
    "    return color;\n"
    "}\n";

// ============================================================================
//  CONSTANTS
// ============================================================================

#define WEBGPU_VERTEX_BUFFER_SIZE       (64 * 1024)     // 64KB initial vertex buffer
#define WEBGPU_UNIFORM_SIZE             16               // 4 x f32
#define WEBGPU_CLIP_STACK_MAX           32               // max nested clip rects
#define WEBGPU_MAX_SESSIONS             4                // matches ABI library
#define WEBGPU_STAGING_MAX_VERTS        4096             // stack staging buffer cap

// Vertex format: 12 bytes (matches WGSL VSInput: vec2<f32> + uint32)
typedef struct WebGPUVertex {
    float       x;        // 4 bytes
    float       y;        // 4 bytes
    uint32_t    color;    // 4 bytes, packed premultiplied ARGB
} WebGPUVertex;

// Uniform buffer contents (written each frame via wgpuQueueWriteBuffer)
typedef struct WebGPUUniforms {
    float scale_x;        // 2.0f / width
    float scale_y;        // -2.0f / height
    float translate_x;    // -1.0f
    float translate_y;    // 1.0f
} WebGPUUniforms;

// ============================================================================
//  PER-SESSION STATE
// ============================================================================

typedef struct WebGPUSessionState {
    int64_t            session_id;
    int                slot_in_use;
    int                initialized;
    int                has_frame_in_flight;
    int64_t            width;
    int64_t            height;

    // ABI library session
    KainWebgpuSession* abi_session;

    // Paired Kaintana session for kt_DrawData access
    kt_Session*        kaintana_session;

    // Kaintana-owned WGPU resources
    WGPUBuffer         vertex_buffer;
    WGPUBuffer         uniform_buffer;
    WGPUShaderModule   vs_module;
    WGPUShaderModule   fs_module;
    WGPURenderPipeline pipeline;
    WGPUBindGroupLayout bind_group_layout;
    WGPUPipelineLayout pipeline_layout;
    WGPUBindGroup      bind_group;
    int                vertex_buffer_capacity;

    // Clip/scissor stack (CPU side)
    kt_Rect            clip_stack[WEBGPU_CLIP_STACK_MAX];
    int                clip_depth;

    // Uniforms
    WebGPUUniforms     uniforms;

    // DPI
    float              native_scale_x;
    float              native_scale_y;

    // Error count
    int                uncaptured_error_count;
} WebGPUSessionState;

// ============================================================================
//  STATIC STATE
// ============================================================================

static WebGPUSessionState  g_sessions[WEBGPU_MAX_SESSIONS];
static int                 g_session_count = 0;
static const KainWebgpuAbiVtable* g_abi_vtable = NULL;

// ============================================================================
//  SESSION MANAGEMENT HELPERS
// ============================================================================

static WebGPUSessionState* webgpu_find_session(int64_t session_id) {
    for (int i = 0; i < WEBGPU_MAX_SESSIONS; i++) {
        if (g_sessions[i].slot_in_use && g_sessions[i].session_id == session_id)
            return &g_sessions[i];
    }
    return NULL;
}

static WebGPUSessionState* webgpu_alloc_session(void) {
    for (int i = 0; i < WEBGPU_MAX_SESSIONS; i++) {
        if (!g_sessions[i].slot_in_use) {
            memset(&g_sessions[i], 0, sizeof(WebGPUSessionState));
            g_sessions[i].slot_in_use = 1;
            g_sessions[i].vertex_buffer_capacity = 0;
            g_sessions[i].clip_depth = 0;
            g_sessions[i].native_scale_x = 1.0f;
            g_sessions[i].native_scale_y = 1.0f;
            g_session_count++;
            return &g_sessions[i];
        }
    }
    return NULL;
}

static void webgpu_free_session(int64_t session_id) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws) return;

    if (ws->pipeline)          { wgpuRenderPipelineRelease(ws->pipeline);          ws->pipeline = 0; }
    if (ws->bind_group)        { wgpuBindGroupRelease(ws->bind_group);              ws->bind_group = 0; }
    if (ws->bind_group_layout) { wgpuBindGroupLayoutRelease(ws->bind_group_layout);  ws->bind_group_layout = 0; }
    if (ws->pipeline_layout)   { wgpuPipelineLayoutRelease(ws->pipeline_layout);     ws->pipeline_layout = 0; }
    if (ws->vs_module)         { wgpuShaderModuleRelease(ws->vs_module);            ws->vs_module = 0; }
    if (ws->fs_module)         { wgpuShaderModuleRelease(ws->fs_module);            ws->fs_module = 0; }
    if (ws->vertex_buffer)     { wgpuBufferDestroy(ws->vertex_buffer);              wgpuBufferRelease(ws->vertex_buffer); ws->vertex_buffer = 0; }
    if (ws->uniform_buffer)    { wgpuBufferDestroy(ws->uniform_buffer);             wgpuBufferRelease(ws->uniform_buffer); ws->uniform_buffer = 0; }

    ws->slot_in_use = 0;
    ws->initialized = 0;
    g_session_count--;
}

// ============================================================================
//  WGSL SHADER MODULE CREATION
// ============================================================================

static WGPUShaderModule webgpu_create_shader_module(WGPUDevice device,
                                                    const char* wgsl_source)
{
    WGPUShaderModuleWGSLDescriptor wgsl_desc;
    memset(&wgsl_desc, 0, sizeof(wgsl_desc));
    wgsl_desc.chain.sType = 0x00000005;  // WGPUSType_ShaderModuleWGSLDescriptor
    wgsl_desc.chain.next  = NULL;
    wgsl_desc.code        = wgsl_source;

    WGPUShaderModuleDescriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.nextInChain = (WGPUChainedStruct const*)&wgsl_desc;
    desc.label       = "kaintana_wgsl";

    return wgpuDeviceCreateShaderModule(device, &desc);
}

// ============================================================================
//  VERTEX BUFFER MANAGEMENT
// ============================================================================

static int webgpu_ensure_vertex_buffer(WGPUDevice device,
                                        WebGPUSessionState* ws,
                                        int needed_vertices)
{
    int needed_bytes = needed_vertices * (int)sizeof(WebGPUVertex);

    if (ws->vertex_buffer != 0 && needed_bytes <= ws->vertex_buffer_capacity)
        return 0;

    int new_capacity = WEBGPU_VERTEX_BUFFER_SIZE;
    if (needed_bytes > new_capacity)
        new_capacity = needed_bytes + needed_bytes / 2;  // 1.5x growth

    if (ws->vertex_buffer != 0) {
        wgpuBufferDestroy(ws->vertex_buffer);
        wgpuBufferRelease(ws->vertex_buffer);
        ws->vertex_buffer = 0;
    }

    WGPUBufferDescriptor buf_desc;
    memset(&buf_desc, 0, sizeof(buf_desc));
    buf_desc.nextInChain     = NULL;
    buf_desc.label           = "kaintana_vertex_buffer";
    buf_desc.size            = (uint64_t)new_capacity;
    buf_desc.usage           = (WGPUBufferUsageFlags)0x00000001  // CopyDst
                             | (WGPUBufferUsageFlags)0x00000010; // Vertex
    buf_desc.mappedAtCreation = false;

    ws->vertex_buffer = wgpuDeviceCreateBuffer(device, &buf_desc);
    ws->vertex_buffer_capacity = new_capacity;

    return (ws->vertex_buffer == 0) ? -1 : 0;
}

// ============================================================================
//  RENDER PIPELINE CREATION
// ============================================================================

static int webgpu_create_pipeline(WGPUDevice device, WebGPUSessionState* ws) {
    if (device == 0 || !ws) return -1;

    // ---- Shader modules ----
    ws->vs_module = webgpu_create_shader_module(device, g_webgpu_vert_wgsl);
    ws->fs_module = webgpu_create_shader_module(device, g_webgpu_frag_wgsl);
    if (ws->vs_module == 0 || ws->fs_module == 0) return -2;

    // ---- Uniform buffer (16 bytes) ----
    {
        WGPUBufferDescriptor ub_desc;
        memset(&ub_desc, 0, sizeof(ub_desc));
        ub_desc.nextInChain      = NULL;
        ub_desc.label            = "kaintana_uniform_buffer";
        ub_desc.size             = WEBGPU_UNIFORM_SIZE;
        ub_desc.usage            = (WGPUBufferUsageFlags)0x00000001  // CopyDst
                                 | (WGPUBufferUsageFlags)0x00000008; // Uniform
        ub_desc.mappedAtCreation = false;
        ws->uniform_buffer = wgpuDeviceCreateBuffer(device, &ub_desc);
        if (ws->uniform_buffer == 0) return -3;
    }

    // ---- Bind group layout (1 uniform binding) ----
    {
        WGPUBindGroupLayoutEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.nextInChain.next  = NULL;
        entry.nextInChain.sType = 0;  // padding
        entry.binding           = 0;
        entry.visibility        = 0x00000001;  // WGPUShaderStage_Vertex
        entry.buffer.nextInChain.next  = NULL;
        entry.buffer.nextInChain.sType = 0;
        entry.buffer.type              = 0;   // Uniform
        entry.buffer.hasDynamicOffset  = false;
        entry.buffer.minBindingSize    = WEBGPU_UNIFORM_SIZE;

        WGPUBindGroupLayoutDescriptor bg_layout_desc;
        memset(&bg_layout_desc, 0, sizeof(bg_layout_desc));
        bg_layout_desc.nextInChain = NULL;
        bg_layout_desc.label       = "kaintana_bind_group_layout";
        bg_layout_desc.entryCount  = 1;
        bg_layout_desc.entries     = &entry;

        ws->bind_group_layout = wgpuDeviceCreateBindGroupLayout(device, &bg_layout_desc);
        if (ws->bind_group_layout == 0) return -4;
    }

    // ---- Pipeline layout ----
    {
        WGPUBindGroupLayout layouts[1] = { ws->bind_group_layout };

        WGPUPipelineLayoutDescriptor pl_desc;
        memset(&pl_desc, 0, sizeof(pl_desc));
        pl_desc.nextInChain          = NULL;
        pl_desc.label                = "kaintana_pipeline_layout";
        pl_desc.bindGroupLayoutCount = 1;
        pl_desc.bindGroupLayouts     = layouts;

        ws->pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &pl_desc);
        if (ws->pipeline_layout == 0) return -5;
    }

    // ---- Bind group (bind uniform to group 0, binding 0) ----
    {
        WGPUBindGroupEntry bg_entry;
        memset(&bg_entry, 0, sizeof(bg_entry));
        bg_entry.nextInChain.next  = NULL;
        bg_entry.nextInChain.sType = 0;
        bg_entry.binding           = 0;
        bg_entry.buffer            = ws->uniform_buffer;
        bg_entry.offset            = 0;
        bg_entry.size              = WEBGPU_UNIFORM_SIZE;
        bg_entry.sampler           = 0;
        bg_entry.textureView       = 0;

        WGPUBindGroupDescriptor bg_desc;
        memset(&bg_desc, 0, sizeof(bg_desc));
        bg_desc.nextInChain = NULL;
        bg_desc.label       = "kaintana_bind_group";
        bg_desc.layout      = ws->pipeline_layout;
        bg_desc.entryCount  = 1;
        bg_desc.entries     = &bg_entry;

        ws->bind_group = wgpuDeviceCreateBindGroup(device, &bg_desc);
        if (ws->bind_group == 0) return -6;
    }

    // ---- Full render pipeline ----
    {
        // Vertex attributes: position (vec2) + color (u32)
        WGPUVertexAttribute attrs[2];
        memset(attrs, 0, sizeof(attrs));
        attrs[0].format         = 0x00000003;  // WGPUVertexFormat_Float32x2
        attrs[0].offset         = 0;
        attrs[0].shaderLocation = 0;

        attrs[1].format         = 0x00000009;  // WGPUVertexFormat_Uint32
        attrs[1].offset         = 8;
        attrs[1].shaderLocation = 1;

        WGPUVertexBufferLayout vb_layout;
        memset(&vb_layout, 0, sizeof(vb_layout));
        vb_layout.arrayStride    = sizeof(WebGPUVertex);
        vb_layout.stepMode       = 0;             // Vertex
        vb_layout.attributeCount = 2;
        vb_layout.attributes     = attrs;

        // SRC_OVER blend
        WGPUBlendComponent blend;
        memset(&blend, 0, sizeof(blend));
        blend.operation = 0;                       // Add
        blend.srcFactor = 5;                       // SrcAlpha
        blend.dstFactor = 6;                       // OneMinusSrcAlpha

        WGPUColorTargetState color_target;
        memset(&color_target, 0, sizeof(color_target));
        color_target.nextInChain = NULL;
        color_target.format      = 0x0000001D;    // BGRA8Unorm
        color_target.blend       = &blend;
        color_target.writeMask   = 0x0000000F;    // All

        WGPUFragmentState frag_state;
        memset(&frag_state, 0, sizeof(frag_state));
        frag_state.nextInChain = NULL;
        frag_state.module      = ws->fs_module;
        frag_state.entryPoint  = "main";
        frag_state.targetCount = 1;
        frag_state.targets     = &color_target;

        WGPURenderPipelineDescriptor rp_desc;
        memset(&rp_desc, 0, sizeof(rp_desc));
        rp_desc.nextInChain                = NULL;
        rp_desc.label                      = "kaintana_render_pipeline";
        rp_desc.layout                     = ws->pipeline_layout;
        rp_desc.vertex                     = &vb_layout;
        rp_desc.vertexBufferCount          = 1;
        rp_desc.primitiveTopology          = 0x00000003;  // TriangleList
        rp_desc.stripIndexFormat           = 0;
        rp_desc.frontFace                  = 0;           // CCW
        rp_desc.cullMode                   = 0;           // None
        rp_desc.fragment                   = &frag_state;
        rp_desc.multisample_count          = 1;
        rp_desc.multisample_mask           = 0xFFFFFFFF;
        rp_desc.multisample_alphaToCoverageEnabled = false;

        ws->pipeline = wgpuDeviceCreateRenderPipeline(device, &rp_desc);
        if (ws->pipeline == 0) return -7;
    }

    // ---- Initial vertex buffer ----
    if (webgpu_ensure_vertex_buffer(device, ws, 1024) != 0) return -8;

    ws->initialized = 1;
    return 0;
}

// ============================================================================
//  CLIP / SCISSOR STACK
// ============================================================================

static void webgpu_push_clip(WebGPUSessionState* ws, kt_Rect clip) {
    if (ws->clip_depth >= WEBGPU_CLIP_STACK_MAX) return;

    if (ws->clip_depth > 0) {
        kt_Rect prev = ws->clip_stack[ws->clip_depth - 1];
        float nx = (clip.x > prev.x) ? clip.x : prev.x;
        float ny = (clip.y > prev.y) ? clip.y : prev.y;
        float nw = ((clip.x + clip.w) < (prev.x + prev.w))
                    ? (clip.x + clip.w) - nx
                    : (prev.x + prev.w) - nx;
        float nh = ((clip.y + clip.h) < (prev.y + prev.h))
                    ? (clip.y + clip.h) - ny
                    : (prev.y + prev.h) - ny;
        nw = (nw < 0.0f) ? 0.0f : nw;
        nh = (nh < 0.0f) ? 0.0f : nh;
        ws->clip_stack[ws->clip_depth].x = nx;
        ws->clip_stack[ws->clip_depth].y = ny;
        ws->clip_stack[ws->clip_depth].w = nw;
        ws->clip_stack[ws->clip_depth].h = nh;
    } else {
        ws->clip_stack[0] = clip;
    }
    ws->clip_depth++;
}

static void webgpu_pop_clip(WebGPUSessionState* ws) {
    if (ws->clip_depth <= 0) return;
    ws->clip_depth--;
}

// ============================================================================
//  DRAW COMMAND HELPERS
// ============================================================================

// Emit 6 vertices for a filled quad (two triangles)
static int webgpu_emit_fill_quad(WebGPUVertex* verts, int offset,
                                  kt_Rect r, uint32_t color)
{
    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;

    verts[offset + 0] = (WebGPUVertex){ x0, y0, color };  // TL
    verts[offset + 1] = (WebGPUVertex){ x1, y0, color };  // TR
    verts[offset + 2] = (WebGPUVertex){ x0, y1, color };  // BL
    verts[offset + 3] = (WebGPUVertex){ x1, y0, color };  // TR
    verts[offset + 4] = (WebGPUVertex){ x1, y1, color };  // BR
    verts[offset + 5] = (WebGPUVertex){ x0, y1, color };  // BL
    return 6;
}

// Emit 24 vertices for a stroked rect (4 thin quads)
static int webgpu_emit_stroke_quads(WebGPUVertex* verts, int offset,
                                     kt_Rect r, uint32_t color, float thickness)
{
    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, t = thickness;
    int o = offset;
    o += webgpu_emit_fill_quad(verts, o, (kt_Rect){ x0, y0, r.w, t }, color);
    o += webgpu_emit_fill_quad(verts, o, (kt_Rect){ x0, y1 - t, r.w, t }, color);
    o += webgpu_emit_fill_quad(verts, o, (kt_Rect){ x0, y0 + t, t, r.h - 2.0f * t }, color);
    o += webgpu_emit_fill_quad(verts, o, (kt_Rect){ x1 - t, y0 + t, t, r.h - 2.0f * t }, color);
    return o - offset;
}

// ============================================================================
//  RENDER — translate kt_DrawData into vertex buffer + uniforms
// ============================================================================

static int g_current_frame_vertex_count = 0;  // set by render_draw_data, read by present

static void webgpu_render_draw_data(WebGPUSessionState* ws) {
    g_current_frame_vertex_count = 0;

    if (!ws || !ws->kaintana_session || !ws->initialized) return;
    if (!ws->abi_session || ws->abi_session->device == 0) return;

    WGPUDevice device = ws->abi_session->device;
    WGPUQueue  queue  = ws->abi_session->queue;

    int cmd_count = kt_cmd_count(ws->kaintana_session);
    if (cmd_count <= 0) return;

    // Count needed vertices
    int needed_verts = 0;
    for (int i = 0; i < cmd_count; i++) {
        kt_Cmd cmd = kt_cmd_get(ws->kaintana_session, i);
        switch (cmd.type) {
            case KT_CMD_FILL:   needed_verts += 6;  break;
            case KT_CMD_STROKE: needed_verts += 24; break;
            default: break;
        }
    }
    if (needed_verts == 0) return;

    // Ensure vertex buffer capacity
    if (webgpu_ensure_vertex_buffer(device, ws, needed_verts) != 0) return;

    // Write vertices into stack staging buffer
    WebGPUVertex staging[WEBGPU_STAGING_MAX_VERTS];
    int staging_capacity = WEBGPU_STAGING_MAX_VERTS;
    int staging_count = 0;

    if (needed_verts > staging_capacity) return;  // skip oversized frames

    ws->clip_depth = 0;

    for (int i = 0; i < cmd_count; i++) {
        kt_Cmd cmd = kt_cmd_get(ws->kaintana_session, i);
        switch (cmd.type) {
            case KT_CMD_FILL:
                staging_count += webgpu_emit_fill_quad(staging, staging_count,
                    cmd.bounds, cmd.color);
                break;
            case KT_CMD_STROKE:
                staging_count += webgpu_emit_stroke_quads(staging, staging_count,
                    cmd.bounds, cmd.color, cmd.thickness);
                break;
            case KT_CMD_CLIP:
                webgpu_push_clip(ws, cmd.bounds);
                break;
            case KT_CMD_UNCLIP:
                webgpu_pop_clip(ws);
                break;
            case KT_CMD_TEXT:
            case KT_CMD_IMAGE:
                // Phase 2: font atlas + image textures
                break;
        }
    }

    // Upload vertices
    wgpuQueueWriteBuffer(queue, ws->vertex_buffer, 0,
                         (const uint8_t*)staging,
                         (uint64_t)(staging_count * (int)sizeof(WebGPUVertex)));

    // Update uniform buffer
    if (ws->width > 0 && ws->height > 0) {
        WebGPUUniforms u;
        u.scale_x     = 2.0f / (float)ws->width;
        u.scale_y     = -2.0f / (float)ws->height;
        u.translate_x = -1.0f;
        u.translate_y = 1.0f;
        ws->uniforms  = u;

        wgpuQueueWriteBuffer(queue, ws->uniform_buffer, 0,
                             (const uint8_t*)&u, WEBGPU_UNIFORM_SIZE);
    }

    g_current_frame_vertex_count = staging_count;
}

// ============================================================================
//  SLOT 18 — GPU SURFACE EXTENSION HELPERS
// ============================================================================

static int64_t webgpu_load_shader_impl(int64_t session_id, const char* wgsl_source) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->initialized || !ws->abi_session || ws->abi_session->device == 0)
        return -1;

    WGPUDevice device = ws->abi_session->device;
    WGPUShaderModule module = webgpu_create_shader_module(device, wgsl_source);
    if (module == 0) return -2;

    wgpuShaderModuleRelease(module);
    return 0;
}

static int64_t webgpu_set_uniform_impl(int64_t session_id, uint32_t binding,
                                        const void* data, uint64_t size)
{
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->initialized || !ws->abi_session || ws->abi_session->queue == 0)
        return -1;

    if (binding == 0 && size <= WEBGPU_UNIFORM_SIZE) {
        wgpuQueueWriteBuffer(ws->abi_session->queue, ws->uniform_buffer, 0,
                             (const uint8_t*)data, size);
        return 0;
    }
    return -2;
}

static const KainGpuSurfaceExtension g_webgpu_gpu_ext = {
    .load_shader  = webgpu_load_shader_impl,
    .set_uniform  = webgpu_set_uniform_impl,
};

// ============================================================================
//  SLOTS 0-1: SESSION LIFECYCLE
// ============================================================================

static int64_t webgpu_session_create(const char* name, int64_t width, int64_t height) {
    (void)name;

    WebGPUSessionState* ws = webgpu_alloc_session();
    if (!ws) return -1;

    if (g_abi_vtable == NULL)
        g_abi_vtable = kain_webgpu_abi_get_vtable();
    if (g_abi_vtable == NULL) return -2;

    int64_t abi_session_id = g_abi_vtable->surface.session_create(name, width, height);
    if (abi_session_id < 0) return -3;

    ws->session_id = abi_session_id;
    ws->width      = width;
    ws->height     = height;

    // ABI session and kaintana session are wired externally via
    // webgpu_set_abi_session() and webgpu_set_kaintana_session().

    return abi_session_id;
}

static void webgpu_session_destroy(int64_t session_id) {
    if (g_abi_vtable)
        g_abi_vtable->surface.session_destroy(session_id);
    webgpu_free_session(session_id);
}

// ============================================================================
//  EXTERNAL SETTERS (called by Kaintana init system)
// ============================================================================

void webgpu_set_abi_session(int64_t session_id, KainWebgpuSession* abi_session) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws) return;
    ws->abi_session = abi_session;
    if (abi_session && abi_session->device != 0 && ws->kaintana_session && !ws->initialized)
        webgpu_create_pipeline(abi_session->device, ws);
}

void webgpu_set_kaintana_session(int64_t session_id, kt_Session* ui_session) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws) return;
    ws->kaintana_session = ui_session;
    if (ws->abi_session && ws->abi_session->device != 0 && ui_session && !ws->initialized)
        webgpu_create_pipeline(ws->abi_session->device, ws);
}

// ============================================================================
//  SLOTS 2-4: ELEMENT TREE (no-op — handled by tree.c substrate)
// ============================================================================

static int64_t webgpu_element_begin(int64_t session_id, int64_t parent_id,
                                    const char* kind, const char* stable_key)
{
    (void)session_id; (void)parent_id; (void)kind; (void)stable_key;
    return 0;
}

static void webgpu_element_end(int64_t session_id, int64_t element_id) {
    (void)session_id; (void)element_id;
}

static void webgpu_element_set_text(int64_t session_id, int64_t element_id,
                                     const char* text)
{
    (void)session_id; (void)element_id; (void)text;
}

// ============================================================================
//  SLOTS 5-7: ATTRIBUTE SETTERS (no-op — handled by tree.c substrate)
// ============================================================================

static void webgpu_element_set_attr_i64(int64_t session_id, int64_t element_id,
                                        const char* key, int64_t value)
{
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_element_set_attr_f64(int64_t session_id, int64_t element_id,
                                        const char* key, double value)
{
    (void)session_id; (void)element_id; (void)key; (void)value;
}

static void webgpu_element_set_attr_string(int64_t session_id, int64_t element_id,
                                           const char* key, const char* value)
{
    (void)session_id; (void)element_id; (void)key; (void)value;
}

// ============================================================================
//  SLOTS 8-9: STATE PERSISTENCE (i64)
// ============================================================================

static int64_t webgpu_state_get_i64(int64_t session_id, const char* key) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return 0;
    return kt_get(ws->kaintana_session, key, 0);
}

static void webgpu_state_set_i64(int64_t session_id, const char* key, int64_t value) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return;
    kt_put(ws->kaintana_session, key, value);
}

// ============================================================================
//  SLOTS 10-12: FRAME LIFECYCLE
// ============================================================================

static void webgpu_begin_frame(int64_t session_id, double delta_ms) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->initialized) return;

    ws->has_frame_in_flight = 1;
    ws->clip_depth = 0;
    ws->uncaptured_error_count = 0;

    if (g_abi_vtable)
        g_abi_vtable->surface.begin_frame(session_id, delta_ms);

    (void)delta_ms;
}

static void webgpu_end_frame(int64_t session_id) {
    (void)session_id;
    // No-op: tree.c handles kt_end() which finalizes the cmd buffer.
}

static void webgpu_present(int64_t session_id) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->has_frame_in_flight || !ws->initialized) return;
    if (!ws->abi_session || ws->abi_session->device == 0 || ws->abi_session->queue == 0)
        return;

    WGPUDevice device   = ws->abi_session->device;
    WGPUQueue  queue    = ws->abi_session->queue;
    WGPUSwapChain swapchain = ws->abi_session->swapchain;

    // ---- Step 1: Render draw data (translate kt_DrawData -> vertices) ----
    webgpu_render_draw_data(ws);

    // ---- Step 2: Acquire backbuffer ----
    WGPUTextureView backbuffer = wgpuSwapChainGetCurrentTextureView(swapchain);
    if (backbuffer == 0) {
        ws->has_frame_in_flight = 0;
        return;
    }

    // ---- Step 3: Create command encoder ----
    WGPUCommandEncoderDescriptor enc_desc;
    enc_desc.nextInChain = NULL;
    enc_desc.label       = "kaintana_frame_encoder";

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);
    if (encoder == 0) {
        wgpuTextureViewRelease(backbuffer);
        ws->has_frame_in_flight = 0;
        return;
    }

    // ---- Step 4: Begin render pass (clear to dark background) ----
    WGPURenderPassColorAttachment color_att;
    memset(&color_att, 0, sizeof(color_att));
    color_att.view          = backbuffer;
    color_att.resolveTarget = 0;
    color_att.loadOp        = 1;              // Clear
    color_att.storeOp       = 0;              // Store
    color_att.clearValue.r  = 0.18;
    color_att.clearValue.g  = 0.18;
    color_att.clearValue.b  = 0.20;
    color_att.clearValue.a  = 1.0;

    WGPURenderPassDescriptor rp_desc;
    memset(&rp_desc, 0, sizeof(rp_desc));
    rp_desc.nextInChain          = NULL;
    rp_desc.label                = "kaintana_render_pass";
    rp_desc.colorAttachmentCount = 1;
    rp_desc.colorAttachments     = &color_att;

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &rp_desc);
    if (render_pass == 0) {
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(backbuffer);
        ws->has_frame_in_flight = 0;
        return;
    }

    // ---- Step 5: Issue draw calls ----
    if (g_current_frame_vertex_count > 0) {
        wgpuRenderPassEncoderSetPipeline(render_pass, ws->pipeline);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, ws->bind_group, 0, NULL);
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, ws->vertex_buffer, 0, 0);
        wgpuRenderPassEncoderDraw(render_pass,
                                  (uint32_t)g_current_frame_vertex_count,
                                  1, 0, 0);
    }

    // ---- Step 6: End render pass, submit, present ----
    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBuffer cmd_buf = wgpuCommandEncoderFinish(encoder, NULL);
    wgpuCommandEncoderRelease(encoder);

    if (cmd_buf != 0) {
        wgpuQueueSubmit(queue, 1, &cmd_buf);
        wgpuCommandBufferRelease(cmd_buf);
    }

    wgpuSwapChainPresent(swapchain);
    wgpuTextureViewRelease(backbuffer);
    ws->has_frame_in_flight = 0;
}

// ============================================================================
//  SLOTS 13-14: EVENT PUMP
// ============================================================================

static int64_t webgpu_poll_event(int64_t session_id, void* out_event, int64_t max_size) {
    if (g_abi_vtable)
        return g_abi_vtable->surface.poll_event(session_id, out_event, max_size);
    (void)session_id; (void)out_event; (void)max_size;
    return 0;
}

static int64_t webgpu_should_close(int64_t session_id) {
    if (g_abi_vtable)
        return g_abi_vtable->surface.should_close(session_id);
    (void)session_id;
    return 0;
}

// ============================================================================
//  SLOTS 15-17: WINDOW LIFECYCLE + PLATFORM ATTACHMENT
// ============================================================================

static int64_t webgpu_window_open(int64_t session_id, const char* title,
                                  int64_t width, int64_t height)
{
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (ws) { ws->width = width; ws->height = height; }

    if (g_abi_vtable)
        return g_abi_vtable->surface.window_open(session_id, title, width, height);
    return 0;
}

static int64_t webgpu_host_pump(int64_t session_id) {
    if (g_abi_vtable)
        return g_abi_vtable->surface.host_pump(session_id);
    (void)session_id;
    return 0;
}

static void webgpu_attach_platform(int64_t session_id, void* platform_handle) {
    if (g_abi_vtable)
        g_abi_vtable->surface.session_attach_platform(session_id, platform_handle);
    (void)session_id; (void)platform_handle;
}

// ============================================================================
//  SLOT 18: GPU SURFACE EXTENSION
// ============================================================================

static const KainGpuSurfaceExtension* webgpu_get_gpu_extension(int64_t session_id) {
    (void)session_id;
    return &g_webgpu_gpu_ext;
}

// ============================================================================
//  SLOTS 19-22: EXPANDED STATE (f64, string)
// ============================================================================

static double webgpu_state_get_f64(int64_t session_id, const char* key) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return 0.0;
    return kt_get_f(ws->kaintana_session, key, 0.0);
}

static void webgpu_state_set_f64(int64_t session_id, const char* key, double value) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return;
    kt_put_f(ws->kaintana_session, key, value);
}

static const char* webgpu_state_get_string(int64_t session_id, const char* key) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return "";
    return kt_get_s(ws->kaintana_session, key, "");
}

static void webgpu_state_set_string(int64_t session_id, const char* key, const char* value) {
    WebGPUSessionState* ws = webgpu_find_session(session_id);
    if (!ws || !ws->kaintana_session) return;
    kt_put_s(ws->kaintana_session, key, value);
}

// ============================================================================
//  SLOT 23: EVENT CALLBACK BINDING (stub)
// ============================================================================

static void webgpu_element_set_callback(int64_t session_id, int64_t element_id,
                                        const char* event_name, void* callback_fn)
{
    (void)session_id; (void)element_id; (void)event_name; (void)callback_fn;
}

// ============================================================================
//  24-SLOT VTABLE (must match component_surface.h slot order exactly)
// ============================================================================

static const KainComponentSurface kaintana_webgpu_surface = {
    .session_create          = webgpu_session_create,
    .session_destroy         = webgpu_session_destroy,
    .element_begin           = webgpu_element_begin,
    .element_end             = webgpu_element_end,
    .element_set_text        = webgpu_element_set_text,
    .element_set_attr_i64    = webgpu_element_set_attr_i64,
    .element_set_attr_f64    = webgpu_element_set_attr_f64,
    .element_set_attr_string = webgpu_element_set_attr_string,
    .state_get_i64           = webgpu_state_get_i64,
    .state_set_i64           = webgpu_state_set_i64,
    .begin_frame             = webgpu_begin_frame,
    .end_frame               = webgpu_end_frame,
    .present                 = webgpu_present,
    .poll_event              = webgpu_poll_event,
    .should_close            = webgpu_should_close,
    .window_open             = webgpu_window_open,
    .host_pump               = webgpu_host_pump,
    .session_attach_platform = webgpu_attach_platform,
    .get_gpu_extension       = webgpu_get_gpu_extension,
    .state_get_f64           = webgpu_state_get_f64,
    .state_set_f64           = webgpu_state_set_f64,
    .state_get_string        = webgpu_state_get_string,
    .state_set_string        = webgpu_state_set_string,
    .element_set_callback    = webgpu_element_set_callback,
};

// ============================================================================
//  REGISTRATION — called by Kaintana init system
// ============================================================================

void kaintana_webgpu_register(void) {
    kain_component_surface_register("webgpu", &kaintana_webgpu_surface);

#ifdef __wasm__
    kain_component_surface_register("webgpu_default", &kaintana_webgpu_surface);
#endif
}

// ============================================================================
//  END — render_webgpu.c
// ============================================================================
