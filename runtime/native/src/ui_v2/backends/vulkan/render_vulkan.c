// ============================================================================
//  render_vulkan.c — Self-contained Vulkan renderer backend for Kaintana
//
//  Implements the 4-function KaintanaBackendVTable contract with a Vulkan
//  GPU pipeline. V1 self-contained: creates its own VkInstance, VkDevice,
//  VkSwapchainKHR, and VkPipeline for the Kaintana vertex format.
//
//  Architecture:
//    - Self-contained V1 with direct Vulkan calls. Phase 2 integrates with
//      the ABI library's PFN table for cooperative frame lifecycle.
//    - Single vertex buffer upload per frame (ImGui's proven pattern).
//    - Push constants for orthographic projection.
//    - Premultiplied alpha SRC_OVER blending.
//    - Scissor clip stack via vkCmdSetScissor.
//
//  Vertex format (12 bytes): float2 pos + uint32_t packed ARGB color
//  Push constants (16 bytes): scale[2], translate[2]
// ============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../kaintana.h"

// ============================================================================
//  CONSTANTS
// ============================================================================

#define VK_BACKEND_DEFAULT_WIDTH        800
#define VK_BACKEND_DEFAULT_HEIGHT       600
#define VK_BACKEND_MAX_SWAPCHAIN_IMAGES 4
#define VK_BACKEND_MAX_FRAMES_IN_FLIGHT 2
#define VK_BACKEND_VERTEX_CAPACITY_INIT 4096
#define VK_BACKEND_VERTEX_CAPACITY_MAX  262144
#define VK_BACKEND_CLIP_STACK_MAX       32
#define VK_BACKEND_WINDOW_CLASS_NAME    L"KaintanaVulkanWindow"

// ============================================================================
//  VERTEX FORMAT — 12 bytes
// ============================================================================

typedef struct KaintanaVkVertex {
    float       x, y;
    uint32_t    color;      // Premultiplied ARGB
} KaintanaVkVertex;

// ============================================================================
//  PUSH CONSTANTS — 16 bytes
// ============================================================================

typedef struct KaintanaVkPushConstants {
    float scale[2];
    float translate[2];
} KaintanaVkPushConstants;

// ============================================================================
//  BACKEND STATE — Singleton
// ============================================================================

typedef struct KaintanaVulkanBackend {
    VkInstance              instance;
    VkPhysicalDevice        physical_device;
    VkDevice                device;
    VkQueue                 graphics_queue;
    uint32_t                graphics_queue_family;
    VkQueue                 present_queue;
    uint32_t                present_queue_family;
    VkSurfaceKHR            surface;
    VkSwapchainKHR          swapchain;
    VkFormat                swapchain_format;
    VkExtent2D              swapchain_extent;
    VkImage                 swapchain_images[VK_BACKEND_MAX_SWAPCHAIN_IMAGES];
    VkImageView             swapchain_image_views[VK_BACKEND_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer           swapchain_framebuffers[VK_BACKEND_MAX_SWAPCHAIN_IMAGES];
    uint32_t                swapchain_image_count;
    int                     swapchain_needs_rebuild;
    VkRenderPass            render_pass;
    VkPipelineLayout        pipeline_layout;
    VkPipeline              pipeline;
    VkCommandPool           command_pool;
    VkCommandBuffer         command_buffer;
    VkSemaphore             image_available[VK_BACKEND_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore             render_finished[VK_BACKEND_MAX_FRAMES_IN_FLIGHT];
    VkFence                 in_flight_fences[VK_BACKEND_MAX_FRAMES_IN_FLIGHT];
    uint32_t                current_frame;
    VkBuffer                vertex_buffer;
    VkDeviceMemory          vertex_memory;
    VkBuffer                staging_buffer;
    VkDeviceMemory          staging_memory;
    void*                   staging_mapped;
    int                     vertex_capacity;
    int                     vertex_count;
    HWND                    hwnd;
    HINSTANCE               hinstance;
    int                     window_width;
    int                     window_height;
    bool                    is_open;
    bool                    initialized;
    bool                    needs_present;
    int                     frame_number;
    VkRect2D                clip_stack[VK_BACKEND_CLIP_STACK_MAX];
    int                     clip_depth;
    float                   dpi_scale_x;
    float                   dpi_scale_y;
    kt_Session*             session;
} KaintanaVulkanBackend;

static KaintanaVulkanBackend g_vk;
static uint32_t g_vk_current_image_index;

// ============================================================================
//  VULKAN LOADING NOTE
// ============================================================================
//  V1 uses direct Vulkan function calls through vulkan.h prototypes.
//  Phase 2: migrate to dynamic PFN loading via ABI library's
//  KainVulkanPfnTable for cooperative frame lifecycle.
// ============================================================================

// VK_CHECK helper — returns -1 on failure
#define VK_CHECK(x) do { VkResult r__ = (x); if (r__ != VK_SUCCESS) return -1; } while(0)


// ============================================================================
//  INSTANCE CREATION
// ============================================================================

static int vk_create_instance(KaintanaVulkanBackend* bk) {
    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kaintana Vulkan";
    app_info.applicationVersion = 1;
    app_info.pEngineName = "Kaintana";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;

    const char* exts[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };

    VkInstanceCreateInfo ci = { 0 };
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = exts;

    VK_CHECK(vkCreateInstance(&ci, NULL, &bk->instance));
    return 0;
}

// ============================================================================
//  PHYSICAL DEVICE
// ============================================================================

static int vk_pick_physical_device(KaintanaVulkanBackend* bk) {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(bk->instance, &count, NULL));
    if (count == 0) return -1;

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(count * sizeof(VkPhysicalDevice));
    if (!devices) return -1;
    VK_CHECK(vkEnumeratePhysicalDevices(bk->instance, &count, devices));

    // Prefer discrete GPU
    int sel = -1;
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { sel = (int)i; break; }
        if (sel < 0) sel = (int)i;
    }
    if (sel < 0) { free(devices); return -1; }
    bk->physical_device = devices[sel];
    free(devices);

    // Queue families
    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(bk->physical_device, &qfc, NULL);
    VkQueueFamilyProperties* qf = (VkQueueFamilyProperties*)malloc(qfc * sizeof(VkQueueFamilyProperties));
    if (!qf) return -1;
    vkGetPhysicalDeviceQueueFamilyProperties(bk->physical_device, &qfc, qf);

    bk->graphics_queue_family = UINT32_MAX;
    bk->present_queue_family = UINT32_MAX;

    for (uint32_t i = 0; i < qfc; i++) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) bk->graphics_queue_family = i;
        if (bk->surface) {
            VkBool32 ps = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(bk->physical_device, i, bk->surface, &ps);
            if (ps && bk->present_queue_family == UINT32_MAX) bk->present_queue_family = i;
        }
    }
    free(qf);

    if (bk->graphics_queue_family == UINT32_MAX) return -1;
    if (bk->present_queue_family == UINT32_MAX) bk->present_queue_family = bk->graphics_queue_family;
    return 0;
}

// ============================================================================
//  LOGICAL DEVICE
// ============================================================================

static int vk_create_device(KaintanaVulkanBackend* bk) {
    float prio = 1.0f;
    uint32_t uf[2];
    int uc = 1;
    uf[0] = bk->graphics_queue_family;
    if (bk->present_queue_family != bk->graphics_queue_family) { uf[1] = bk->present_queue_family; uc = 2; }

    VkDeviceQueueCreateInfo qci[2];
    for (int i = 0; i < uc; i++) {
        qci[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci[i].queueFamilyIndex = uf[i];
        qci[i].queueCount = 1;
        qci[i].pQueuePriorities = &prio;
    }

    const char* dexs[] = { "VK_KHR_swapchain" };
    VkPhysicalDeviceFeatures feats = { 0 };

    VkDeviceCreateInfo ci = { 0 };
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)uc;
    ci.pQueueCreateInfos = qci;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = dexs;
    ci.pEnabledFeatures = &feats;

    VK_CHECK(vkCreateDevice(bk->physical_device, &ci, NULL, &bk->device));
    vkGetDeviceQueue(bk->device, bk->graphics_queue_family, 0, &bk->graphics_queue);
    vkGetDeviceQueue(bk->device, bk->present_queue_family, 0, &bk->present_queue);
    return 0;
}

// ============================================================================
//  SURFACE / SWAPCHAIN / RENDER PASS / PIPELINE
// ============================================================================

static int vk_create_surface(KaintanaVulkanBackend* bk) {
    if (!bk->hwnd) return -1;
    VkWin32SurfaceCreateInfoKHR ci = { 0 };
    ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hinstance = bk->hinstance;
    ci.hwnd = bk->hwnd;
    VK_CHECK(vkCreateWin32SurfaceKHR(bk->instance, &ci, NULL, &bk->surface));
    return 0;
}

static int vk_create_swapchain(KaintanaVulkanBackend* bk, int w, int h) {
    VkSurfaceCapabilitiesKHR caps;
    memset(&caps, 0, sizeof(caps));
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(bk->physical_device, bk->surface, &caps));

    uint32_t fc = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(bk->physical_device, bk->surface, &fc, NULL));
    VkSurfaceFormatKHR* fmts = (VkSurfaceFormatKHR*)malloc(fc * sizeof(VkSurfaceFormatKHR));
    if (!fmts) return -1;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(bk->physical_device, bk->surface, &fc, fmts));

    VkSurfaceFormatKHR fmt = fmts[0];
    for (uint32_t i = 0; i < fc; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = fmts[i]; break;
        }
    }
    bk->swapchain_format = fmt.format;
    free(fmts);

    uint32_t pc = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(bk->physical_device, bk->surface, &pc, NULL));
    VkPresentModeKHR* pms = (VkPresentModeKHR*)malloc(pc * sizeof(VkPresentModeKHR));
    if (!pms) return -1;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(bk->physical_device, bk->surface, &pc, pms));

    VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < pc; i++) { if (pms[i] == VK_PRESENT_MODE_MAILBOX_KHR) { pm = pms[i]; break; } }
    free(pms);

    if (caps.currentExtent.width != UINT32_MAX) {
        bk->swapchain_extent = caps.currentExtent;
    } else {
        bk->swapchain_extent.width = (uint32_t)(w > 0 ? w : VK_BACKEND_DEFAULT_WIDTH);
        bk->swapchain_extent.height = (uint32_t)(h > 0 ? h : VK_BACKEND_DEFAULT_HEIGHT);
    }

    uint32_t ic = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && ic > caps.maxImageCount) ic = caps.maxImageCount;
    if (ic > VK_BACKEND_MAX_SWAPCHAIN_IMAGES) ic = VK_BACKEND_MAX_SWAPCHAIN_IMAGES;

    VkSwapchainCreateInfoKHR sci = { 0 };
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = bk->surface;
    sci.minImageCount = ic;
    sci.imageFormat = fmt.format;
    sci.imageColorSpace = fmt.colorSpace;
    sci.imageExtent = bk->swapchain_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = pm;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = bk->swapchain;

    VK_CHECK(vkCreateSwapchainKHR(bk->device, &sci, NULL, &bk->swapchain));

    vkGetSwapchainImagesKHR(bk->device, bk->swapchain, &bk->swapchain_image_count, NULL);
    if (bk->swapchain_image_count > VK_BACKEND_MAX_SWAPCHAIN_IMAGES)
        bk->swapchain_image_count = VK_BACKEND_MAX_SWAPCHAIN_IMAGES;
    vkGetSwapchainImagesKHR(bk->device, bk->swapchain, &bk->swapchain_image_count, bk->swapchain_images);
    return 0;
}

static int vk_create_image_views(KaintanaVulkanBackend* bk) {
    for (uint32_t i = 0; i < bk->swapchain_image_count; i++) {
        VkImageViewCreateInfo ci = { 0 };
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = bk->swapchain_images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = bk->swapchain_format;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(bk->device, &ci, NULL, &bk->swapchain_image_views[i]));
    }
    return 0;
}

static int vk_create_render_pass(KaintanaVulkanBackend* bk) {
    VkAttachmentDescription att = { 0 };
    att.format = bk->swapchain_format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref = { 0 };
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sp = { 0 };
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ref;

    VkSubpassDependency dep = { 0 };
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci = { 0 };
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &att;
    ci.subpassCount = 1;
    ci.pSubpasses = &sp;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    VK_CHECK(vkCreateRenderPass(bk->device, &ci, NULL, &bk->render_pass));
    return 0;
}

// ============================================================================
//  SHADER BLOBS (placeholder SPIR-V)
// ============================================================================
//
//  === vertex.glsl ===
//  #version 450
//  layout(location=0) in vec2 aPos;
//  layout(location=1) in uint aColor;
//  layout(push_constant) uniform PC { vec2 scale; vec2 translate; } pc;
//  layout(location=0) out vec4 vColor;
//  void main() {
//      vColor = unpackUnorm4x8(aColor).bgra;
//      gl_Position = vec4(aPos*pc.scale+pc.translate, 0, 1);
//  }
//
//  === fragment.glsl ===
//  #version 450
//  layout(location=0) in vec4 vColor;
//  layout(location=0) out vec4 fColor;
//  void main() { fColor = vColor; }
//
//  Build: glslangValidator -V -o vert.spv vertex.glsl && glslangValidator -V -o frag.spv fragment.glsl
//  Embed: xxd -i vert.spv > shaders_vert.h && xxd -i frag.spv > shaders_frag.h

static const uint32_t g_vk_spv_vert[] = { 0x07230203, 0x00010000, 0, 0 };
static const uint32_t g_vk_spv_frag[] = { 0x07230203, 0x00010000, 0, 0 };
#define SPV_VERT_SZ ((int)sizeof(g_vk_spv_vert))
#define SPV_FRAG_SZ ((int)sizeof(g_vk_spv_frag))

static int vk_create_pipeline(KaintanaVulkanBackend* bk) {
    VkShaderModule vs, fs;

    VkShaderModuleCreateInfo vsci = { 0 };
    vsci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vsci.codeSize = SPV_VERT_SZ; vsci.pCode = g_vk_spv_vert;
    VK_CHECK(vkCreateShaderModule(bk->device, &vsci, NULL, &vs));

    VkShaderModuleCreateInfo fsci = { 0 };
    fsci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fsci.codeSize = SPV_FRAG_SZ; fsci.pCode = g_vk_spv_frag;
    if (vkCreateShaderModule(bk->device, &fsci, NULL, &fs) != VK_SUCCESS)
        { vkDestroyShaderModule(bk->device, vs, NULL); return -1; }

    VkPipelineShaderStageCreateInfo stg[2];
    stg[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stg[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stg[0].module = vs; stg[0].pName = "main";
    stg[0].pSpecializationInfo = NULL; stg[0].flags = 0; stg[0].pNext = NULL;
    stg[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stg[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stg[1].module = fs; stg[1].pName = "main";
    stg[1].pSpecializationInfo = NULL; stg[1].flags = 0; stg[1].pNext = NULL;

    VkVertexInputBindingDescription bd = { 0 };
    bd.binding = 0; bd.stride = sizeof(KaintanaVkVertex); bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription at[2];
    at[0].location = 0; at[0].binding = 0; at[0].format = VK_FORMAT_R32G32_SFLOAT;
    at[0].offset = offsetof(KaintanaVkVertex, x);
    at[1].location = 1; at[1].binding = 0; at[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    at[1].offset = offsetof(KaintanaVkVertex, color);

    VkPipelineVertexInputStateCreateInfo vi = { 0 };
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &bd;
    vi.vertexAttributeDescriptionCount = 2; vi.pVertexAttributeDescriptions = at;

    VkPipelineInputAssemblyStateCreateInfo ia = { 0 };
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = { 0 };
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = { 0 };
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = { 0 };
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState bl = { 0 };
    bl.blendEnable = VK_TRUE;
    bl.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    bl.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    bl.colorBlendOp = VK_BLEND_OP_ADD;
    bl.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    bl.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    bl.alphaBlendOp = VK_BLEND_OP_ADD;
    bl.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT
                       |VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb = { 0 };
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &bl;

    VkPushConstantRange pr = { 0 };
    pr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pr.offset = 0; pr.size = sizeof(KaintanaVkPushConstants);

    VkPipelineLayoutCreateInfo pl = { 0 };
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1; pl.pPushConstantRanges = &pr;
    pl.setLayoutCount = 0;

    if (vkCreatePipelineLayout(bk->device, &pl, NULL, &bk->pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(bk->device, fs, NULL);
        vkDestroyShaderModule(bk->device, vs, NULL);
        return -1;
    }

    VkGraphicsPipelineCreateInfo gp = { 0 };
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2; gp.pStages = stg;
    gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp; gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms; gp.pColorBlendState = &cb;
    gp.layout = bk->pipeline_layout;
    gp.renderPass = bk->render_pass; gp.subpass = 0;

    VkResult r = vkCreateGraphicsPipelines(bk->device, VK_NULL_HANDLE, 1, &gp, NULL, &bk->pipeline);
    vkDestroyShaderModule(bk->device, fs, NULL);
    vkDestroyShaderModule(bk->device, vs, NULL);
    if (r != VK_SUCCESS) { vkDestroyPipelineLayout(bk->device, bk->pipeline_layout, NULL); return -1; }
    return 0;
}

static int vk_create_framebuffers(KaintanaVulkanBackend* bk) {
    for (uint32_t i = 0; i < bk->swapchain_image_count; i++) {
        VkFramebufferCreateInfo ci = { 0 };
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = bk->render_pass;
        ci.attachmentCount = 1; ci.pAttachments = &bk->swapchain_image_views[i];
        ci.width = bk->swapchain_extent.width;
        ci.height = bk->swapchain_extent.height;
        ci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(bk->device, &ci, NULL, &bk->swapchain_framebuffers[i]));
    }
    return 0;
}

static int vk_create_command_objects(KaintanaVulkanBackend* bk) {
    VkCommandPoolCreateInfo ci = { 0 };
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = bk->graphics_queue_family;
    VK_CHECK(vkCreateCommandPool(bk->device, &ci, NULL, &bk->command_pool));

    VkCommandBufferAllocateInfo ai = { 0 };
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = bk->command_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(bk->device, &ai, &bk->command_buffer));
    return 0;
}

static int vk_create_sync_objects(KaintanaVulkanBackend* bk) {
    for (int i = 0; i < VK_BACKEND_MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphoreCreateInfo ci = { 0 };
        ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(bk->device, &ci, NULL, &bk->image_available[i]));
        VK_CHECK(vkCreateSemaphore(bk->device, &ci, NULL, &bk->render_finished[i]));

        VkFenceCreateInfo fci = { 0 };
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(bk->device, &fci, NULL, &bk->in_flight_fences[i]));
    }
    return 0;
}

// ============================================================================
//  BUFFER MANAGEMENT
// ============================================================================

static int vk_create_buffers(KaintanaVulkanBackend* bk, int vc) {
    VkDeviceSize sz = (VkDeviceSize)vc * sizeof(KaintanaVkVertex);

    // Staging
    VkBufferCreateInfo bci = { 0 };
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sz; bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VK_CHECK(vkCreateBuffer(bk->device, &bci, NULL, &bk->staging_buffer));

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(bk->device, bk->staging_buffer, &mr);
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(bk->physical_device, &mp);

    uint32_t mt = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { mt = i; break; }
    if (mt == UINT32_MAX) return -1;

    VkMemoryAllocateInfo mai = { 0 };
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size; mai.memoryTypeIndex = mt;
    VK_CHECK(vkAllocateMemory(bk->device, &mai, NULL, &bk->staging_memory));
    VK_CHECK(vkBindBufferMemory(bk->device, bk->staging_buffer, bk->staging_memory, 0));
    VK_CHECK(vkMapMemory(bk->device, bk->staging_memory, 0, VK_WHOLE_SIZE, 0, &bk->staging_mapped));

    // Device-local vertex
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK(vkCreateBuffer(bk->device, &bci, NULL, &bk->vertex_buffer));

    vkGetBufferMemoryRequirements(bk->device, bk->vertex_buffer, &mr);
    mt = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            { mt = i; break; }
    if (mt == UINT32_MAX) return -1;

    mai.memoryTypeIndex = mt; mai.allocationSize = mr.size;
    VK_CHECK(vkAllocateMemory(bk->device, &mai, NULL, &bk->vertex_memory));
    VK_CHECK(vkBindBufferMemory(bk->device, bk->vertex_buffer, bk->vertex_memory, 0));

    bk->vertex_capacity = vc;
    return 0;
}

static void vk_destroy_buffers(KaintanaVulkanBackend* bk) {
    if (bk->staging_mapped) { vkUnmapMemory(bk->device, bk->staging_memory); bk->staging_mapped = NULL; }
    if (bk->staging_buffer) { vkDestroyBuffer(bk->device, bk->staging_buffer, NULL); bk->staging_buffer = VK_NULL_HANDLE; }
    if (bk->staging_memory) { vkFreeMemory(bk->device, bk->staging_memory, NULL); bk->staging_memory = VK_NULL_HANDLE; }
    if (bk->vertex_buffer) { vkDestroyBuffer(bk->device, bk->vertex_buffer, NULL); bk->vertex_buffer = VK_NULL_HANDLE; }
    if (bk->vertex_memory) { vkFreeMemory(bk->device, bk->vertex_memory, NULL); bk->vertex_memory = VK_NULL_HANDLE; }
    bk->vertex_capacity = 0;
}

// ============================================================================
//  SWAPCHAIN REBUILD
// ============================================================================

static void vk_rebuild_swapchain(KaintanaVulkanBackend* bk) {
    vkDeviceWaitIdle(bk->device);
    for (uint32_t i = 0; i < bk->swapchain_image_count; i++) {
        if (bk->swapchain_framebuffers[i]) { vkDestroyFramebuffer(bk->device, bk->swapchain_framebuffers[i], NULL); bk->swapchain_framebuffers[i] = VK_NULL_HANDLE; }
        if (bk->swapchain_image_views[i]) { vkDestroyImageView(bk->device, bk->swapchain_image_views[i], NULL); bk->swapchain_image_views[i] = VK_NULL_HANDLE; }
    }
    VkSwapchainKHR old = bk->swapchain; bk->swapchain = VK_NULL_HANDLE;
    int w = bk->window_width > 0 ? bk->window_width : VK_BACKEND_DEFAULT_WIDTH;
    int h = bk->window_height > 0 ? bk->window_height : VK_BACKEND_DEFAULT_HEIGHT;
    if (vk_create_swapchain(bk, w, h) != 0) { bk->swapchain = old; return; }
    if (old) vkDestroySwapchainKHR(bk->device, old, NULL);
    vk_create_image_views(bk); vk_create_framebuffers(bk);
    bk->swapchain_needs_rebuild = 0;
}

// ============================================================================
//  WINDOW
// ============================================================================

static LRESULT CALLBACK vk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE: g_vk.is_open = false; DestroyWindow(hwnd); return 0;
    case WM_DESTROY: g_vk.is_open = false; PostQuitMessage(0); return 0;
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        if (w > 0 && h > 0 && (w != g_vk.window_width || h != g_vk.window_height))
            { g_vk.window_width = w; g_vk.window_height = h; g_vk.swapchain_needs_rebuild = 1; }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int vk_create_window(KaintanaVulkanBackend* bk, const char* title, int w, int h) {
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc = vk_wndproc; wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = VK_BACKEND_WINDOW_CLASS_NAME;
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return -1;

    wchar_t wt[256] = L"Kaintana Vulkan";
    if (title) MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, 255);

    RECT rc = { 0, 0, w, h };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    bk->hwnd = CreateWindowExW(0, VK_BACKEND_WINDOW_CLASS_NAME, wt,
        WS_OVERLAPPEDWINDOW, 100, 100, rc.right-rc.left, rc.bottom-rc.top,
        NULL, NULL, hi, NULL);
    if (!bk->hwnd) return -1;
    bk->hinstance = hi; bk->window_width = w; bk->window_height = h;
    bk->is_open = true;
    ShowWindow(bk->hwnd, SW_SHOW); UpdateWindow(bk->hwnd);
    return 0;
}

// ============================================================================
//  4-FUNCTION VTABLE
// ============================================================================

static int vulkan_init(const KaintanaBackendConfig* config) {
    if (!config) return -1;
    memset(&g_vk, 0, sizeof(g_vk));
    g_vk.session = (kt_Session*)config->platform_handle;
    int w = config->width > 0 ? config->width : VK_BACKEND_DEFAULT_WIDTH;
    int h = config->height > 0 ? config->height : VK_BACKEND_DEFAULT_HEIGHT;

    if (vk_create_instance(&g_vk) != 0) return -1;

    const char* title = config->title ? config->title : "Kaintana Vulkan";
    if (vk_create_window(&g_vk, title, w, h) != 0) return -1;
    if (vk_create_surface(&g_vk) != 0) return -1;
    if (vk_pick_physical_device(&g_vk) != 0) return -1;
    if (vk_create_device(&g_vk) != 0) return -1;
    if (vk_create_swapchain(&g_vk, w, h) != 0) return -1;
    if (vk_create_image_views(&g_vk) != 0) return -1;
    if (vk_create_render_pass(&g_vk) != 0) return -1;
    if (vk_create_pipeline(&g_vk) != 0) return -1;
    if (vk_create_framebuffers(&g_vk) != 0) return -1;
    if (vk_create_command_objects(&g_vk) != 0) return -1;
    if (vk_create_sync_objects(&g_vk) != 0) return -1;
    if (vk_create_buffers(&g_vk, VK_BACKEND_VERTEX_CAPACITY_INIT) != 0) return -1;

    g_vk.initialized = true;
    return 0;
}

static void vulkan_shutdown(void) {
    if (!g_vk.initialized) return;
    vkDeviceWaitIdle(g_vk.device);

    vk_destroy_buffers(&g_vk);
    for (int i = 0; i < VK_BACKEND_MAX_FRAMES_IN_FLIGHT; i++) {
        if (g_vk.in_flight_fences[i]) vkDestroyFence(g_vk.device, g_vk.in_flight_fences[i], NULL);
        if (g_vk.render_finished[i]) vkDestroySemaphore(g_vk.device, g_vk.render_finished[i], NULL);
        if (g_vk.image_available[i]) vkDestroySemaphore(g_vk.device, g_vk.image_available[i], NULL);
    }
    if (g_vk.command_pool) vkDestroyCommandPool(g_vk.device, g_vk.command_pool, NULL);
    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        if (g_vk.swapchain_framebuffers[i]) vkDestroyFramebuffer(g_vk.device, g_vk.swapchain_framebuffers[i], NULL);
        if (g_vk.swapchain_image_views[i]) vkDestroyImageView(g_vk.device, g_vk.swapchain_image_views[i], NULL);
    }
    if (g_vk.pipeline) vkDestroyPipeline(g_vk.device, g_vk.pipeline, NULL);
    if (g_vk.pipeline_layout) vkDestroyPipelineLayout(g_vk.device, g_vk.pipeline_layout, NULL);
    if (g_vk.render_pass) vkDestroyRenderPass(g_vk.device, g_vk.render_pass, NULL);
    if (g_vk.swapchain) vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, NULL);
    if (g_vk.surface) vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, NULL);
    if (g_vk.device) vkDestroyDevice(g_vk.device, NULL);
    if (g_vk.instance) vkDestroyInstance(g_vk.instance, NULL);
    if (g_vk.hwnd) DestroyWindow(g_vk.hwnd);
    memset(&g_vk, 0, sizeof(g_vk));
}

static void vulkan_new_frame(void) {
    if (!g_vk.initialized || !g_vk.is_open) return;
    if (g_vk.swapchain_needs_rebuild) { vk_rebuild_swapchain(&g_vk); }

    vkWaitForFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame], VK_TRUE, UINT64_MAX);

    g_vk_current_image_index = 0;
    VkResult res = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
        g_vk.image_available[g_vk.current_frame], VK_NULL_HANDLE, &g_vk_current_image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        g_vk.swapchain_needs_rebuild = 1; return;
    }

    vkResetFences(g_vk.device, 1, &g_vk.in_flight_fences[g_vk.current_frame]);
    vkResetCommandPool(g_vk.device, g_vk.command_pool, 0);

    VkCommandBufferBeginInfo bi = { 0 };
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_vk.command_buffer, &bi);

    VkClearValue cv = { 0 };
    VkRenderPassBeginInfo rp = { 0 };
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = g_vk.render_pass;
    rp.framebuffer = g_vk.swapchain_framebuffers[g_vk_current_image_index];
    rp.renderArea.extent = g_vk.swapchain_extent;
    rp.clearValueCount = 1; rp.pClearValues = &cv;
    vkCmdBeginRenderPass(g_vk.command_buffer, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = { 0 };
    vp.width = (float)g_vk.swapchain_extent.width;
    vp.height = (float)g_vk.swapchain_extent.height;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(g_vk.command_buffer, 0, 1, &vp);

    g_vk.clip_stack[0].extent = g_vk.swapchain_extent;
    g_vk.clip_depth = 1;
    g_vk.vertex_count = 0;
    g_vk.frame_number++;
}

static void vk_push_vertex(float x, float y, uint32_t color) {
    if (g_vk.vertex_count >= g_vk.vertex_capacity) {
        int nc = g_vk.vertex_capacity * 2;
        if (nc > VK_BACKEND_VERTEX_CAPACITY_MAX) return;
        void* old = g_vk.staging_mapped; int oc = g_vk.vertex_count;
        vk_destroy_buffers(&g_vk);
        if (vk_create_buffers(&g_vk, nc) != 0) return;
        if (old && g_vk.staging_mapped) memcpy(g_vk.staging_mapped, old, (size_t)oc * sizeof(KaintanaVkVertex));
        g_vk.vertex_count = oc;
    }
    KaintanaVkVertex* v = (KaintanaVkVertex*)g_vk.staging_mapped;
    v[g_vk.vertex_count].x = x; v[g_vk.vertex_count].y = y; v[g_vk.vertex_count].color = color;
    g_vk.vertex_count++;
}

static void vk_emit_fill_rect(float x1, float y1, float x2, float y2, uint32_t c) {
    vk_push_vertex(x1, y1, c); vk_push_vertex(x2, y1, c); vk_push_vertex(x2, y2, c);
    vk_push_vertex(x1, y1, c); vk_push_vertex(x2, y2, c); vk_push_vertex(x1, y2, c);
}

static void vk_emit_stroke_rect(float x1, float y1, float x2, float y2, float t, uint32_t c) {
    if (t < 1.0f) t = 1.0f;
    vk_emit_fill_rect(x1, y1, x2, y1+t, c);
    vk_emit_fill_rect(x1, y2-t, x2, y2, c);
    vk_emit_fill_rect(x1, y1+t, x1+t, y2-t, c);
    vk_emit_fill_rect(x2-t, y1+t, x2, y2-t, c);
}

static void vulkan_render(const kt_DrawData* dd) {
    if (!g_vk.initialized || !g_vk.is_open) return;
    if (g_vk.swapchain_needs_rebuild) return;

    float fw = (float)g_vk.swapchain_extent.width;
    float fh = (float)g_vk.swapchain_extent.height;
    if (fw <= 0.0f || fh <= 0.0f) goto finish;

    if (dd && dd->cmds && dd->cmd_count > 0) {
        for (int i = 0; i < dd->cmd_count; i++) {
            const kt_Cmd* c = &dd->cmds[i];
            float x1 = c->bounds.x, y1 = c->bounds.y;
            float x2 = c->bounds.x + c->bounds.w, y2 = c->bounds.y + c->bounds.h;
            switch (c->type) {
            case KT_CMD_FILL: vk_emit_fill_rect(x1, y1, x2, y2, c->color); break;
            case KT_CMD_STROKE: vk_emit_stroke_rect(x1, y1, x2, y2, c->thickness, c->color); break;
            case KT_CMD_CLIP: {
                VkRect2D s;
                s.offset.x = (int32_t)(x1 < 0.0f ? 0.0f : x1);
                s.offset.y = (int32_t)(y1 < 0.0f ? 0.0f : y1);
                s.extent.width  = (uint32_t)((x2 > fw ? fw : x2) - s.offset.x);
                s.extent.height = (uint32_t)((y2 > fh ? fh : y2) - s.offset.y);
                if (g_vk.clip_depth < VK_BACKEND_CLIP_STACK_MAX) g_vk.clip_stack[g_vk.clip_depth++] = s;
                vkCmdSetScissor(g_vk.command_buffer, 0, 1, &s); break;
            }
            case KT_CMD_UNCLIP:
                if (g_vk.clip_depth > 1) {
                    g_vk.clip_depth--;
                    vkCmdSetScissor(g_vk.command_buffer, 0, 1, &g_vk.clip_stack[g_vk.clip_depth-1]);
                } break;
            default: break;
            }
        }
    }

    if (g_vk.vertex_count > 0) {
        VkDeviceSize bs = (VkDeviceSize)g_vk.vertex_count * sizeof(KaintanaVkVertex);
        VkBufferCopy bc = { 0 }; bc.size = bs;
        vkCmdCopyBuffer(g_vk.command_buffer, g_vk.staging_buffer, g_vk.vertex_buffer, 1, &bc);

        VkBufferMemoryBarrier br = { 0 };
        br.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        br.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        br.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        br.buffer = g_vk.vertex_buffer; br.size = bs;
        vkCmdPipelineBarrier(g_vk.command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL, 1, &br, 0, NULL);

        vkCmdBindPipeline(g_vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
        VkDeviceSize off[1] = { 0 };
        vkCmdBindVertexBuffers(g_vk.command_buffer, 0, 1, &g_vk.vertex_buffer, off);

        KaintanaVkPushConstants pc;
        pc.scale[0] = 2.0f/fw; pc.scale[1] = -2.0f/fh;
        pc.translate[0] = -1.0f; pc.translate[1] = 1.0f;
        vkCmdPushConstants(g_vk.command_buffer, g_vk.pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(g_vk.command_buffer, (uint32_t)g_vk.vertex_count, 1, 0, 0);
    }

finish:
    vkCmdEndRenderPass(g_vk.command_buffer);
    vkEndCommandBuffer(g_vk.command_buffer);
    g_vk.needs_present = true;

    // Submit + Present
    VkPipelineStageFlags ws = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { 0 };
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &g_vk.image_available[g_vk.current_frame];
    si.pWaitDstStageMask = &ws;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_vk.command_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &g_vk.render_finished[g_vk.current_frame];
    vkQueueSubmit(g_vk.graphics_queue, 1, &si, g_vk.in_flight_fences[g_vk.current_frame]);

    VkPresentInfoKHR pi = { 0 };
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &g_vk.render_finished[g_vk.current_frame];
    pi.swapchainCount = 1;
    pi.pSwapchains = &g_vk.swapchain;
    pi.pImageIndices = &g_vk_current_image_index;
    VkResult r2 = vkQueuePresentKHR(g_vk.present_queue, &pi);
    if (r2 == VK_ERROR_OUT_OF_DATE_KHR || r2 == VK_SUBOPTIMAL_KHR)
        g_vk.swapchain_needs_rebuild = 1;

    g_vk.current_frame = (g_vk.current_frame + 1) % VK_BACKEND_MAX_FRAMES_IN_FLIGHT;
    g_vk.needs_present = false;
}

// ============================================================================
//  VTABLE SINGLETON
// ============================================================================

const KaintanaBackendVTable kaintana_vulkan_backend = {
    .init      = vulkan_init,
    .shutdown  = vulkan_shutdown,
    .new_frame = vulkan_new_frame,
    .render    = vulkan_render
};
