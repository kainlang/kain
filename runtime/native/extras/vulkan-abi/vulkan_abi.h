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

#include "vulkan_loader_subset.h"
#include "component_surface.h"

#define KAIN_VULKAN_ABI_VERSION 1
#define KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES 4
#define KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT 2
#define KAIN_VULKAN_ABI_STATUS_MESSAGE_MAX 512

// ── Raw PFN function pointer typedefs (for blade-level raw Vulkan access) ──
// These mirror the 50+ PFNs resolved internally by vulkan_abi.c.
// All struct params are void* to avoid depending on <vulkan/vulkan.h>.

typedef VkLoaderProcAddress (*KainPfn_vkGetInstanceProcAddr)(VkInstance, const char*);
typedef VkLoaderProcAddress (*KainPfn_vkGetDeviceProcAddr)(VkDevice, const char*);
typedef VkResult (*KainPfn_vkCreateInstance)(const void*, const void*, VkInstance*);
typedef void     (*KainPfn_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*KainPfn_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void     (*KainPfn_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, uint32_t*, void*);
typedef VkResult (*KainPfn_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, uint32_t*);
typedef VkResult (*KainPfn_vkCreateDevice)(VkPhysicalDevice, const void*, const void*, VkDevice*);
typedef void     (*KainPfn_vkDestroyDevice)(VkDevice, const void*);
typedef void     (*KainPfn_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef VkResult (*KainPfn_vkDeviceWaitIdle)(VkDevice);

/* WSI surfaces */
typedef VkResult (*KainPfn_vkCreateWin32SurfaceKHR)(VkInstance, const void*, const void*, VkSurfaceKHR*);
typedef void     (*KainPfn_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const void*);
typedef VkResult (*KainPfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, void*);
typedef VkResult (*KainPfn_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, void*);
typedef VkResult (*KainPfn_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, uint32_t*);

/* Swapchain */
typedef VkResult (*KainPfn_vkCreateSwapchainKHR)(VkDevice, const void*, const void*, VkSwapchainKHR*);
typedef void     (*KainPfn_vkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const void*);
typedef VkResult (*KainPfn_vkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
typedef VkResult (*KainPfn_vkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*);
typedef VkResult (*KainPfn_vkQueuePresentKHR)(VkQueue, const void*);

/* Command buffers */
typedef VkResult (*KainPfn_vkCreateCommandPool)(VkDevice, const void*, const void*, VkCommandPool*);
typedef void     (*KainPfn_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*KainPfn_vkAllocateCommandBuffers)(VkDevice, const void*, VkCommandBuffer*);
typedef VkResult (*KainPfn_vkResetCommandBuffer)(VkCommandBuffer, uint32_t);
typedef VkResult (*KainPfn_vkBeginCommandBuffer)(VkCommandBuffer, const void*);
typedef VkResult (*KainPfn_vkEndCommandBuffer)(VkCommandBuffer);
typedef VkResult (*KainPfn_vkQueueSubmit)(VkQueue, uint32_t, const void*, VkFence);

/* Synchronization */
typedef VkResult (*KainPfn_vkCreateSemaphore)(VkDevice, const void*, const void*, VkSemaphore*);
typedef void     (*KainPfn_vkDestroySemaphore)(VkDevice, VkSemaphore, const void*);
typedef VkResult (*KainPfn_vkCreateFence)(VkDevice, const void*, const void*, VkFence*);
typedef void     (*KainPfn_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*KainPfn_vkWaitForFences)(VkDevice, uint32_t, const VkFence*, uint32_t, uint64_t);
typedef VkResult (*KainPfn_vkResetFences)(VkDevice, uint32_t, const VkFence*);

/* Image views */
typedef VkResult (*KainPfn_vkCreateImageView)(VkDevice, const void*, const void*, VkImageView*);
typedef void     (*KainPfn_vkDestroyImageView)(VkDevice, VkImageView, const void*);

/* Rendering pipeline (for blade-level raw Vulkan consumers like chronosim) */
typedef VkResult (*KainPfn_vkCreateRenderPass)(VkDevice, const void*, const void*, VkRenderPass*);
typedef void     (*KainPfn_vkDestroyRenderPass)(VkDevice, VkRenderPass, const void*);
typedef VkResult (*KainPfn_vkCreateShaderModule)(VkDevice, const void*, const void*, VkShaderModule*);
typedef void     (*KainPfn_vkDestroyShaderModule)(VkDevice, VkShaderModule, const void*);
typedef VkResult (*KainPfn_vkCreatePipelineLayout)(VkDevice, const void*, const void*, VkPipelineLayout*);
typedef void     (*KainPfn_vkDestroyPipelineLayout)(VkDevice, VkPipelineLayout, const void*);
typedef VkResult (*KainPfn_vkCreateGraphicsPipelines)(VkDevice, VkPipelineCache, uint32_t, const void*, const void*, VkPipeline*);
typedef void     (*KainPfn_vkDestroyPipeline)(VkDevice, VkPipeline, const void*);
typedef VkResult (*KainPfn_vkCreateFramebuffer)(VkDevice, const void*, const void*, VkFramebuffer*);
typedef void     (*KainPfn_vkDestroyFramebuffer)(VkDevice, VkFramebuffer, const void*);
typedef void     (*KainPfn_vkCmdBeginRenderPass)(VkCommandBuffer, const void*, uint32_t);
typedef void     (*KainPfn_vkCmdEndRenderPass)(VkCommandBuffer);
typedef void     (*KainPfn_vkCmdBindPipeline)(VkCommandBuffer, uint32_t, VkPipeline);
typedef void     (*KainPfn_vkCmdPushConstants)(VkCommandBuffer, VkPipelineLayout, uint32_t, uint32_t, uint32_t, const void*);
typedef void     (*KainPfn_vkCmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);

// ── Raw PFN table — all 57 resolved PFNs accessible to blade bridges ───────

typedef struct KainVulkanPfnTable {
    KainPfn_vkGetInstanceProcAddr                    vkGetInstanceProcAddr;
    KainPfn_vkGetDeviceProcAddr                      vkGetDeviceProcAddr;
    KainPfn_vkCreateInstance                         vkCreateInstance;
    KainPfn_vkDestroyInstance                        vkDestroyInstance;
    KainPfn_vkEnumeratePhysicalDevices               vkEnumeratePhysicalDevices;
    KainPfn_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    KainPfn_vkGetPhysicalDeviceSurfaceSupportKHR     vkGetPhysicalDeviceSurfaceSupportKHR;
    KainPfn_vkCreateDevice                           vkCreateDevice;
    KainPfn_vkDestroyDevice                          vkDestroyDevice;
    KainPfn_vkGetDeviceQueue                         vkGetDeviceQueue;
    KainPfn_vkDeviceWaitIdle                         vkDeviceWaitIdle;
    KainPfn_vkCreateWin32SurfaceKHR                  vkCreateWin32SurfaceKHR;
    KainPfn_vkDestroySurfaceKHR                      vkDestroySurfaceKHR;
    KainPfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    KainPfn_vkGetPhysicalDeviceSurfaceFormatsKHR     vkGetPhysicalDeviceSurfaceFormatsKHR;
    KainPfn_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    KainPfn_vkCreateSwapchainKHR                     vkCreateSwapchainKHR;
    KainPfn_vkDestroySwapchainKHR                    vkDestroySwapchainKHR;
    KainPfn_vkGetSwapchainImagesKHR                  vkGetSwapchainImagesKHR;
    KainPfn_vkAcquireNextImageKHR                    vkAcquireNextImageKHR;
    KainPfn_vkQueuePresentKHR                        vkQueuePresentKHR;
    KainPfn_vkCreateCommandPool                      vkCreateCommandPool;
    KainPfn_vkDestroyCommandPool                     vkDestroyCommandPool;
    KainPfn_vkAllocateCommandBuffers                 vkAllocateCommandBuffers;
    KainPfn_vkResetCommandBuffer                     vkResetCommandBuffer;
    KainPfn_vkBeginCommandBuffer                     vkBeginCommandBuffer;
    KainPfn_vkEndCommandBuffer                       vkEndCommandBuffer;
    KainPfn_vkQueueSubmit                            vkQueueSubmit;
    KainPfn_vkCreateSemaphore                        vkCreateSemaphore;
    KainPfn_vkDestroySemaphore                       vkDestroySemaphore;
    KainPfn_vkCreateFence                            vkCreateFence;
    KainPfn_vkDestroyFence                           vkDestroyFence;
    KainPfn_vkWaitForFences                          vkWaitForFences;
    KainPfn_vkResetFences                            vkResetFences;
    KainPfn_vkCreateImageView                        vkCreateImageView;
    KainPfn_vkDestroyImageView                       vkDestroyImageView;
    /* Rendering pipeline PFNs */
    KainPfn_vkCreateRenderPass                       vkCreateRenderPass;
    KainPfn_vkDestroyRenderPass                      vkDestroyRenderPass;
    KainPfn_vkCreateShaderModule                     vkCreateShaderModule;
    KainPfn_vkDestroyShaderModule                    vkDestroyShaderModule;
    KainPfn_vkCreatePipelineLayout                   vkCreatePipelineLayout;
    KainPfn_vkDestroyPipelineLayout                  vkDestroyPipelineLayout;
    KainPfn_vkCreateGraphicsPipelines                vkCreateGraphicsPipelines;
    KainPfn_vkDestroyPipeline                        vkDestroyPipeline;
    KainPfn_vkCreateFramebuffer                      vkCreateFramebuffer;
    KainPfn_vkDestroyFramebuffer                     vkDestroyFramebuffer;
    KainPfn_vkCmdBeginRenderPass                     vkCmdBeginRenderPass;
    KainPfn_vkCmdEndRenderPass                       vkCmdEndRenderPass;
    KainPfn_vkCmdBindPipeline                        vkCmdBindPipeline;
    KainPfn_vkCmdPushConstants                       vkCmdPushConstants;
    KainPfn_vkCmdDraw                                vkCmdDraw;
} KainVulkanPfnTable;

// ── Public vtable struct — MUST match vulkan_surface_shim.c exactly ────────

typedef struct KainVulkanAbiVtable {
    KainComponentSurface surface;
    KainVulkanPfnTable   pfns;         /* All 57 resolved PFNs */
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

// ── Export annotation ────────────────────────────────────────────
#ifdef _WIN32
#define KAIN_VULKAN_ABI_EXPORT __declspec(dllexport)
#else
#define KAIN_VULKAN_ABI_EXPORT __attribute__((visibility("default")))
#endif

// ── The ONLY entry point exposed to the runtime shim ───────────────────────

KAIN_VULKAN_ABI_EXPORT const KainVulkanAbiVtable* kain_vulkan_abi_get_vtable(void);

// ── Optional: explicit init/shutdown for blade-level control ───────────────

KAIN_VULKAN_ABI_EXPORT int  kain_vulkan_abi_init(void);
KAIN_VULKAN_ABI_EXPORT void kain_vulkan_abi_shutdown(void);

#endif // KAIN_VULKAN_ABI_H
