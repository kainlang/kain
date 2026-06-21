#ifndef KAIN_VULKAN_ABI_H
#define KAIN_VULKAN_ABI_H

// ============================================================================
//  vulkan_abi.h — Public header for libkain-vulkan-abi.so/.dll
// ============================================================================
//  This is the separately-linked Vulkan ABI library. It owns ALL actual
//  Vulkan driver calls: instance creation, physical device selection,
//  logical device creation, WSI surface creation (per-platform), swapchain
//  lifecycle, frame submission, and present.
//
//  The runtime shim (vulkan_surface_shim.c) dlopens this library and calls
//  kain_vulkan_abi_get_vtable() to obtain a filled KainComponentSurface vtable.
//
//  This library NEVER includes <vulkan/vulkan.h> and NEVER links the Vulkan
//  SDK. Everything is dynamically resolved via 44 PFNs loaded through
//  vkGetInstanceProcAddr.
// ============================================================================

#include "../../include/vulkan_loader_subset.h"
#include "../../include/component_surface.h"

#define KAIN_VULKAN_ABI_VERSION 1
#define KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES 4
#define KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT 2
#define KAIN_VULKAN_ABI_STATUS_MESSAGE_MAX 512

// ── Public vtable struct — MUST match vulkan_surface_shim.c exactly ────────

typedef struct KainVulkanAbiVtable {
    KainComponentSurface surface;
    int64_t              abi_version;
    int64_t              present_count;
    int64_t              swapchain_recreations;
    int64_t              last_status;
    char                 last_error[KAIN_VULKAN_ABI_STATUS_MESSAGE_MAX];
} KainVulkanAbiVtable;

// ── Per-session Vulkan state (lives in the library, not the shim) ──────────

#define KAIN_VULKAN_MAX_SESSIONS 8

typedef struct KainVulkanSession {
    int64_t             session_id;
    const char*         name;
    int64_t             width;
    int64_t             height;
    VkInstance          instance;
    VkPhysicalDevice    physical_device;
    VkDevice            device;
    VkSurfaceKHR        surface;
    VkSwapchainKHR      swapchain;
    VkQueue             graphics_queue;
    VkQueue             present_queue;
    uint32_t            graphics_queue_family;
    uint32_t            present_queue_family;
    VkCommandPool       command_pool;
    VkCommandBuffer     command_buffers[KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         image_available[KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore         render_finished[KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT];
    VkFence             in_flight_fences[KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT];
    VkImage             swapchain_images[KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES];
    VkImageView         swapchain_image_views[KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer       framebuffers[KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES];
    uint32_t            swapchain_image_count;
    uint32_t            current_image_index;
    uint32_t            current_frame;
    int64_t             should_close;
#ifdef _WIN32
    void*               hwnd;
    void*               hinstance;
#endif
#ifdef __linux__
    void*               x11_display;
    uintptr_t           x11_window;
#endif
    int                 initialized;
} KainVulkanSession;

// ── The ONLY entry point exposed to the runtime shim ───────────────────────

const KainVulkanAbiVtable* kain_vulkan_abi_get_vtable(void);

// ── Optional: explicit init/shutdown for blade-level control ───────────────

int  kain_vulkan_abi_init(void);
void kain_vulkan_abi_shutdown(void);

#endif // KAIN_VULKAN_ABI_H
