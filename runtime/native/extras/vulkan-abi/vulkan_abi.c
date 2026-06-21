// ============================================================================
//  vulkan_abi.c — libkain-vulkan-abi.so/.dll implementation
// ============================================================================
//  9 sections:
//    1. Dynamic loader: dlopen/loadlibrary, resolve 43 PFNs
//    2. WSI surface creation: per-platform #ifdef
//    3. Physical device selection: enumerate, prefer discrete GPU
//    4. Logical device creation: vkCreateDevice with swapchain extension
//    5. Swapchain lifecycle: extent negotiation, present mode, image views,
//       framebuffers, recreation on resize
//    6. Frame submission: semaphores, fences, MAX_FRAMES_IN_FLIGHT=2 ring buffer
//    7. KainComponentSurface vtable fill: ALL 18 slots
//    8. Error handling: VkResult → string table
//    9. Static vtable instance + entry point
//
//  This file NEVER includes <vulkan/vulkan.h> and NEVER links the Vulkan SDK.
//  All Vulkan types are uintptr_t; all Vulkan calls resolved via PFN.
//  All Vk*CreateInfo structs are built as raw byte buffers with hardcoded
//  sType values because we don't have the Vulkan SDK headers.
// ============================================================================

#include "vulkan_abi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

// ============================================================================
//  SECTION 0: Hardcoded Vulkan structure definitions (no <vulkan/vulkan.h>)
// ============================================================================

typedef uint32_t VkStructureType;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;
typedef uint64_t VkDeviceAddress;
typedef uint64_t VkFramebufferCreateFlags;
typedef uint32_t VkSampleCountFlagBits;
typedef uint32_t VkImageCreateFlags;
typedef uint32_t VkImageViewCreateFlags;
typedef uint32_t VkPipelineStageFlags;
typedef uint32_t VkAccessFlags;
typedef uint32_t VkDependencyFlags;
typedef uint32_t VkQueueFlags;
typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkFormat;
typedef uint32_t VkColorSpaceKHR;
typedef uint32_t VkPresentModeKHR;
typedef uint32_t VkImageUsageFlags;
typedef uint32_t VkSurfaceTransformFlagBitsKHR;
typedef uint32_t VkCompositeAlphaFlagBitsKHR;
typedef uint32_t VkCommandPoolCreateFlags;
typedef uint32_t VkCommandBufferUsageFlags;
typedef uint32_t VkCommandBufferLevel;
typedef uint32_t VkFenceCreateFlags;
typedef uint32_t VkSemaphoreCreateFlags;
typedef uint32_t VkImageViewType;
typedef uint32_t VkComponentSwizzle;
typedef uint32_t VkImageAspectFlags;
typedef uint32_t VkSharingMode;
typedef uint32_t VkPhysicalDeviceType;
typedef uint32_t VkImageLayout;
typedef uint32_t VkSubpassContents;
typedef uint32_t VkAttachmentLoadOp;
typedef uint32_t VkAttachmentStoreOp;
typedef uint32_t VkPipelineBindPoint;
typedef uint32_t VkFilter;
typedef uint32_t VkSamplerMipmapMode;
typedef uint32_t VkSamplerAddressMode;
typedef uint32_t VkBorderColor;
typedef uint32_t VkCompareOp;
typedef uint32_t VkDescriptorType;
typedef uint32_t VkShaderStageFlags;

// ── VkApplicationInfo ─────────────────────────────────────────────

typedef struct VkApplicationInfo {
    VkStructureType    sType;
    const void*        pNext;
    const char*        pApplicationName;
    uint32_t           applicationVersion;
    const char*        pEngineName;
    uint32_t           engineVersion;
    uint32_t           apiVersion;
} VkApplicationInfo;

// ── VkInstanceCreateInfo ──────────────────────────────────────────

typedef struct VkInstanceCreateInfo {
    VkStructureType          sType;
    const void*              pNext;
    VkFlags                  flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t                 enabledLayerCount;
    const char* const*       ppEnabledLayerNames;
    uint32_t                 enabledExtensionCount;
    const char* const*       ppEnabledExtensionNames;
} VkInstanceCreateInfo;

// ── VkDeviceQueueCreateInfo ───────────────────────────────────────

typedef struct VkDeviceQueueCreateInfo {
    VkStructureType    sType;
    const void*        pNext;
    VkFlags            flags;
    uint32_t           queueFamilyIndex;
    uint32_t           queueCount;
    const float*       pQueuePriorities;
} VkDeviceQueueCreateInfo;

// ── VkDeviceCreateInfo ────────────────────────────────────────────

typedef struct VkDeviceCreateInfo {
    VkStructureType               sType;
    const void*                   pNext;
    VkFlags                       flags;
    uint32_t                      queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t                      enabledLayerCount;
    const char* const*            ppEnabledLayerNames;
    uint32_t                      enabledExtensionCount;
    const char* const*            ppEnabledExtensionNames;
    const void*                   pEnabledFeatures;
} VkDeviceCreateInfo;

// ── VkExtent2D ────────────────────────────────────────────────────

typedef struct VkExtent2D {
    uint32_t width;
    uint32_t height;
} VkExtent2D;

// ── VkSurfaceCapabilitiesKHR ──────────────────────────────────────

typedef struct VkSurfaceCapabilitiesKHR {
    uint32_t           minImageCount;
    uint32_t           maxImageCount;
    VkExtent2D         currentExtent;
    VkExtent2D         minImageExtent;
    VkExtent2D         maxImageExtent;
    uint32_t           maxImageArrayLayers;
    VkSurfaceTransformFlagBitsKHR supportedTransforms;
    VkSurfaceTransformFlagBitsKHR currentTransform;
    VkCompositeAlphaFlagBitsKHR   supportedCompositeAlpha;
    VkImageUsageFlags  supportedUsageFlags;
} VkSurfaceCapabilitiesKHR;

// ── VkSurfaceFormatKHR ────────────────────────────────────────────

typedef struct VkSurfaceFormatKHR {
    VkFormat        format;
    VkColorSpaceKHR colorSpace;
} VkSurfaceFormatKHR;

// ── VkSwapchainCreateInfoKHR ──────────────────────────────────────

typedef struct VkSwapchainCreateInfoKHR {
    VkStructureType              sType;
    const void*                  pNext;
    VkFlags                      flags;
    VkSurfaceKHR                 surface;
    uint32_t                     minImageCount;
    VkFormat                     imageFormat;
    VkColorSpaceKHR              imageColorSpace;
    VkExtent2D                   imageExtent;
    uint32_t                     imageArrayLayers;
    VkImageUsageFlags            imageUsage;
    VkSharingMode                imageSharingMode;
    uint32_t                     queueFamilyIndexCount;
    const uint32_t*              pQueueFamilyIndices;
    VkSurfaceTransformFlagBitsKHR preTransform;
    VkCompositeAlphaFlagBitsKHR   compositeAlpha;
    VkPresentModeKHR             presentMode;
    VkBool32                     clipped;
    VkSwapchainKHR               oldSwapchain;
} VkSwapchainCreateInfoKHR;

// ── VkComponentMapping ────────────────────────────────────────────

typedef struct VkComponentMapping {
    VkComponentSwizzle r;
    VkComponentSwizzle g;
    VkComponentSwizzle b;
    VkComponentSwizzle a;
} VkComponentMapping;

// ── VkImageSubresourceRange ───────────────────────────────────────

typedef struct VkImageSubresourceRange {
    VkImageAspectFlags aspectMask;
    uint32_t           baseMipLevel;
    uint32_t           levelCount;
    uint32_t           baseArrayLayer;
    uint32_t           layerCount;
} VkImageSubresourceRange;

// ── VkImageViewCreateInfo ─────────────────────────────────────────

typedef struct VkImageViewCreateInfo {
    VkStructureType            sType;
    const void*                pNext;
    VkImageViewCreateFlags     flags;
    VkImage                    image;
    VkImageViewType            viewType;
    VkFormat                   format;
    VkComponentMapping         components;
    VkImageSubresourceRange    subresourceRange;
} VkImageViewCreateInfo;

// ── VkWin32SurfaceCreateInfoKHR ───────────────────────────────────

typedef struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType             sType;
    const void*                 pNext;
    VkFlags                     flags;
    void*                       hinstance;
    void*                       hwnd;
} VkWin32SurfaceCreateInfoKHR;

// ── VkXlibSurfaceCreateInfoKHR ────────────────────────────────────

typedef struct VkXlibSurfaceCreateInfoKHR {
    VkStructureType             sType;
    const void*                 pNext;
    VkFlags                     flags;
    void*                       dpy;
    uintptr_t                   window;
} VkXlibSurfaceCreateInfoKHR;

// ── VkWaylandSurfaceCreateInfoKHR ─────────────────────────────────

typedef struct VkWaylandSurfaceCreateInfoKHR {
    VkStructureType             sType;
    const void*                 pNext;
    VkFlags                     flags;
    void*                       display;
    void*                       surface;
} VkWaylandSurfaceCreateInfoKHR;

// ── VkMacOSSurfaceCreateInfoMVK ───────────────────────────────────

typedef struct VkMacOSSurfaceCreateInfoMVK {
    VkStructureType             sType;
    const void*                 pNext;
    VkFlags                     flags;
    const void*                 pView;
} VkMacOSSurfaceCreateInfoMVK;

// ── VkPhysicalDeviceProperties ────────────────────────────────────

typedef struct VkPhysicalDeviceLimits {
    uint32_t       maxImageDimension1D;
    uint32_t       maxImageDimension2D;
    uint32_t       maxImageDimension3D;
    uint32_t       maxImageDimensionCube;
    uint32_t       maxImageArrayLayers;
    uint32_t       maxTexelBufferElements;
    uint32_t       maxUniformBufferRange;
    uint32_t       maxStorageBufferRange;
    uint32_t       maxPushConstantsSize;
    uint32_t       maxMemoryAllocationCount;
    uint32_t       maxSamplerAllocationCount;
    VkDeviceSize   bufferImageGranularity;
    VkDeviceSize   sparseAddressSpaceSize;
    uint32_t       maxBoundDescriptorSets;
    uint32_t       maxPerStageDescriptorSamplers;
    uint32_t       maxPerStageDescriptorUniformBuffers;
    uint32_t       maxPerStageDescriptorStorageBuffers;
    uint32_t       maxPerStageDescriptorSampledImages;
    uint32_t       maxPerStageDescriptorStorageImages;
    uint32_t       maxPerStageDescriptorInputAttachments;
    uint32_t       maxPerStageResources;
    uint32_t       maxDescriptorSetSamplers;
    uint32_t       maxDescriptorSetUniformBuffers;
    uint32_t       maxDescriptorSetUniformBuffersDynamic;
    uint32_t       maxDescriptorSetStorageBuffers;
    uint32_t       maxDescriptorSetStorageBuffersDynamic;
    uint32_t       maxDescriptorSetSampledImages;
    uint32_t       maxDescriptorSetStorageImages;
    uint32_t       maxDescriptorSetInputAttachments;
    uint32_t       maxVertexInputAttributes;
    uint32_t       maxVertexInputBindings;
    uint32_t       maxVertexInputAttributeOffset;
    uint32_t       maxVertexInputBindingStride;
    uint32_t       maxVertexOutputComponents;
    uint32_t       maxTessellationGenerationLevel;
    uint32_t       maxTessellationPatchSize;
    uint32_t       maxTessellationControlPerVertexInputComponents;
    uint32_t       maxTessellationControlPerVertexOutputComponents;
    uint32_t       maxTessellationControlPerPatchOutputComponents;
    uint32_t       maxTessellationControlTotalOutputComponents;
    uint32_t       maxTessellationEvaluationInputComponents;
    uint32_t       maxTessellationEvaluationOutputComponents;
    uint32_t       maxGeometryShaderInvocations;
    uint32_t       maxGeometryInputComponents;
    uint32_t       maxGeometryOutputComponents;
    uint32_t       maxGeometryOutputVertices;
    uint32_t       maxGeometryTotalOutputComponents;
    uint32_t       maxFragmentInputComponents;
    uint32_t       maxFragmentOutputAttachments;
    uint32_t       maxFragmentDualSrcAttachments;
    uint32_t       maxFragmentCombinedOutputResources;
    uint32_t       maxComputeSharedMemorySize;
    uint32_t       maxComputeWorkGroupCount[3];
    uint32_t       maxComputeWorkGroupInvocations;
    uint32_t       maxComputeWorkGroupSize[3];
    uint32_t       subPixelPrecisionBits;
    uint32_t       subTexelPrecisionBits;
    uint32_t       mipmapPrecisionBits;
    uint32_t       maxDrawIndexedIndexValue;
    uint32_t       maxDrawIndirectCount;
    float          maxSamplerLodBias;
    float          maxSamplerAnisotropy;
    uint32_t       maxViewports;
    uint32_t       maxViewportDimensions[2];
    float          viewportBoundsRange[2];
    uint32_t       viewportSubPixelBits;
    size_t         minMemoryMapAlignment;
    VkDeviceSize   minTexelBufferOffsetAlignment;
    VkDeviceSize   minUniformBufferOffsetAlignment;
    VkDeviceSize   minStorageBufferOffsetAlignment;
    int32_t        minTexelOffset;
    uint32_t       maxTexelOffset;
    int32_t        minTexelGatherOffset;
    uint32_t       maxTexelGatherOffset;
    float          minInterpolationOffset;
    float          maxInterpolationOffset;
    uint32_t       subPixelInterpolationOffsetBits;
    uint32_t       maxFramebufferWidth;
    uint32_t       maxFramebufferHeight;
    uint32_t       maxFramebufferLayers;
    VkFlags        framebufferColorSampleCounts;
    VkFlags        framebufferDepthSampleCounts;
    VkFlags        framebufferStencilSampleCounts;
    VkFlags        framebufferNoAttachmentsSampleCounts;
    uint32_t       maxColorAttachments;
    VkFlags        sampledImageColorSampleCounts;
    VkFlags        sampledImageIntegerSampleCounts;
    VkFlags        sampledImageDepthSampleCounts;
    VkFlags        sampledImageStencilSampleCounts;
    VkFlags        storageImageSampleCounts;
    uint32_t       maxSampleMaskWords;
    VkBool32       timestampComputeAndGraphics;
    float          timestampPeriod;
    uint32_t       maxClipDistances;
    uint32_t       maxCullDistances;
    uint32_t       maxCombinedClipAndCullDistances;
    uint32_t       discreteQueuePriorities;
    float          pointSizeRange[2];
    float          lineWidthRange[2];
    float          pointSizeGranularity;
    float          lineWidthGranularity;
    VkBool32       strictLines;
    VkBool32       standardSampleLocations;
    VkDeviceSize   optimalBufferCopyOffsetAlignment;
    VkDeviceSize   optimalBufferCopyRowPitchAlignment;
    VkDeviceSize   nonCoherentAtomSize;
} VkPhysicalDeviceLimits;

typedef struct VkPhysicalDeviceSparseProperties {
    VkBool32 residencyStandard2DBlockShape;
    VkBool32 residencyStandard2DMultisampleBlockShape;
    VkBool32 residencyStandard3DBlockShape;
    VkBool32 residencyAlignedMipSize;
    VkBool32 residencyNonResidentStrict;
} VkPhysicalDeviceSparseProperties;

typedef struct VkPhysicalDeviceProperties {
    uint32_t                            apiVersion;
    uint32_t                            driverVersion;
    uint32_t                            vendorID;
    uint32_t                            deviceID;
    VkPhysicalDeviceType                deviceType;
    char                                deviceName[256];
    uint8_t                             pipelineCacheUUID[16];
    VkPhysicalDeviceLimits              limits;
    VkPhysicalDeviceSparseProperties    sparseProperties;
} VkPhysicalDeviceProperties;

// ── VkQueueFamilyProperties ───────────────────────────────────────

typedef struct VkQueueFamilyProperties {
    VkQueueFlags          queueFlags;
    uint32_t              queueCount;
    uint32_t              timestampValidBits;
    VkExtent2D            minImageTransferGranularity;
} VkQueueFamilyProperties;

// ── VkCommandPoolCreateInfo ───────────────────────────────────────

typedef struct VkCommandPoolCreateInfo {
    VkStructureType             sType;
    const void*                 pNext;
    VkCommandPoolCreateFlags    flags;
    uint32_t                    queueFamilyIndex;
} VkCommandPoolCreateInfo;

// ── VkCommandBufferAllocateInfo ───────────────────────────────────

typedef struct VkCommandBufferAllocateInfo {
    VkStructureType        sType;
    const void*            pNext;
    VkCommandPool          commandPool;
    VkCommandBufferLevel   level;
    uint32_t               commandBufferCount;
} VkCommandBufferAllocateInfo;

// ── VkCommandBufferBeginInfo ──────────────────────────────────────

typedef struct VkCommandBufferBeginInfo {
    VkStructureType               sType;
    const void*                   pNext;
    VkCommandBufferUsageFlags     flags;
    const void*                   pInheritanceInfo;
} VkCommandBufferBeginInfo;

// ── VkSemaphoreCreateInfo ─────────────────────────────────────────

typedef struct VkSemaphoreCreateInfo {
    VkStructureType          sType;
    const void*              pNext;
    VkSemaphoreCreateFlags   flags;
} VkSemaphoreCreateInfo;

// ── VkFenceCreateInfo ─────────────────────────────────────────────

typedef struct VkFenceCreateInfo {
    VkStructureType       sType;
    const void*           pNext;
    VkFenceCreateFlags    flags;
} VkFenceCreateInfo;

// ── VkSubmitInfo ──────────────────────────────────────────────────

typedef struct VkSubmitInfo {
    VkStructureType            sType;
    const void*                pNext;
    uint32_t                   waitSemaphoreCount;
    const VkSemaphore*         pWaitSemaphores;
    const VkPipelineStageFlags* pWaitDstStageMask;
    uint32_t                   commandBufferCount;
    const VkCommandBuffer*     pCommandBuffers;
    uint32_t                   signalSemaphoreCount;
    const VkSemaphore*         pSignalSemaphores;
} VkSubmitInfo;

// ── VkPresentInfoKHR ──────────────────────────────────────────────

typedef struct VkPresentInfoKHR {
    VkStructureType          sType;
    const void*              pNext;
    uint32_t                 waitSemaphoreCount;
    const VkSemaphore*       pWaitSemaphores;
    uint32_t                 swapchainCount;
    const VkSwapchainKHR*    pSwapchains;
    const uint32_t*          pImageIndices;
    VkResult*                pResults;
} VkPresentInfoKHR;

// ── VkClearColorValue ─────────────────────────────────────────────

typedef union VkClearColorValue {
    float    float32[4];
    int32_t  int32[4];
    uint32_t uint32[4];
} VkClearColorValue;

// ── VkClearValue ──────────────────────────────────────────────────

typedef union VkClearValue {
    VkClearColorValue color;
    struct { float depth; uint32_t stencil; } depthStencil;
} VkClearValue;

// ── VkOffset2D ────────────────────────────────────────────────────

typedef struct VkOffset2D {
    int32_t x;
    int32_t y;
} VkOffset2D;

// ── VkRect2D ──────────────────────────────────────────────────────

typedef struct VkRect2D {
    VkOffset2D offset;
    VkExtent2D extent;
} VkRect2D;

// ── VkRenderPassBeginInfo ─────────────────────────────────────────

typedef struct VkRenderPassBeginInfo {
    VkStructureType       sType;
    const void*           pNext;
    VkRenderPass          renderPass;
    VkFramebuffer         framebuffer;
    VkRect2D              renderArea;
    uint32_t              clearValueCount;
    const VkClearValue*   pClearValues;
} VkRenderPassBeginInfo;

// ── VkImageSubresourceLayers ──────────────────────────────────────

typedef struct VkImageSubresourceLayers {
    VkImageAspectFlags aspectMask;
    uint32_t           mipLevel;
    uint32_t           baseArrayLayer;
    uint32_t           layerCount;
} VkImageSubresourceLayers;

// ── VkImageMemoryBarrier ──────────────────────────────────────────

typedef struct VkImageMemoryBarrier {
    VkStructureType            sType;
    const void*                pNext;
    VkAccessFlags              srcAccessMask;
    VkAccessFlags              dstAccessMask;
    VkImageLayout              oldLayout;
    VkImageLayout              newLayout;
    uint32_t                   srcQueueFamilyIndex;
    uint32_t                   dstQueueFamilyIndex;
    VkImage                    image;
    VkImageSubresourceRange    subresourceRange;
} VkImageMemoryBarrier;

// ── VkAttachmentDescription ───────────────────────────────────────

typedef struct VkAttachmentDescription {
    VkFlags                flags;
    VkFormat               format;
    VkSampleCountFlagBits  samples;
    VkAttachmentLoadOp     loadOp;
    VkAttachmentStoreOp    storeOp;
    VkAttachmentLoadOp     stencilLoadOp;
    VkAttachmentStoreOp    stencilStoreOp;
    VkImageLayout          initialLayout;
    VkImageLayout          finalLayout;
} VkAttachmentDescription;

// ── VkAttachmentReference ─────────────────────────────────────────

typedef struct VkAttachmentReference {
    uint32_t       attachment;
    VkImageLayout  layout;
} VkAttachmentReference;

// ── VkSubpassDescription ──────────────────────────────────────────

typedef struct VkSubpassDescription {
    VkFlags                      flags;
    VkPipelineBindPoint          pipelineBindPoint;
    uint32_t                     inputAttachmentCount;
    const VkAttachmentReference* pInputAttachments;
    uint32_t                     colorAttachmentCount;
    const VkAttachmentReference* pColorAttachments;
    const VkAttachmentReference* pResolveAttachments;
    const VkAttachmentReference* pDepthStencilAttachment;
    uint32_t                     preserveAttachmentCount;
    const uint32_t*              pPreserveAttachments;
} VkSubpassDescription;

// ── VkRenderPassCreateInfo ────────────────────────────────────────

typedef struct VkRenderPassCreateInfo {
    VkStructureType                 sType;
    const void*                     pNext;
    VkFlags                         flags;
    uint32_t                        attachmentCount;
    const VkAttachmentDescription*  pAttachments;
    uint32_t                        subpassCount;
    const VkSubpassDescription*     pSubpasses;
    uint32_t                        dependencyCount;
    const void*                     pDependencies;
} VkRenderPassCreateInfo;

// ── VkFramebufferCreateInfo ───────────────────────────────────────

typedef struct VkFramebufferCreateInfo {
    VkStructureType            sType;
    const void*                pNext;
    VkFramebufferCreateFlags   flags;
    VkRenderPass               renderPass;
    uint32_t                   attachmentCount;
    const VkImageView*         pAttachments;
    uint32_t                   width;
    uint32_t                   height;
    uint32_t                   layers;
} VkFramebufferCreateInfo;

// ── Hardcoded sType values (Vulkan 1.3) ───────────────────────────

#define S_TYPE_APPLICATION_INFO                          0
#define S_TYPE_INSTANCE_CREATE_INFO                      1
#define S_TYPE_DEVICE_QUEUE_CREATE_INFO                  2
#define S_TYPE_DEVICE_CREATE_INFO                        3
#define S_TYPE_SUBMIT_INFO                               4
#define S_TYPE_SEMAPHORE_CREATE_INFO                     8
#define S_TYPE_FENCE_CREATE_INFO                         9
#define S_TYPE_COMMAND_POOL_CREATE_INFO                  10
#define S_TYPE_COMMAND_BUFFER_ALLOCATE_INFO              11
#define S_TYPE_COMMAND_BUFFER_BEGIN_INFO                 12
#define S_TYPE_RENDER_PASS_CREATE_INFO                   13
#define S_TYPE_IMAGE_VIEW_CREATE_INFO                    14
#define S_TYPE_FRAMEBUFFER_CREATE_INFO                   20
#define S_TYPE_SWAPCHAIN_CREATE_INFO_KHR                 1000001000
#define S_TYPE_PRESENT_INFO_KHR                          1000001001
#define S_TYPE_XLIB_SURFACE_CREATE_INFO_KHR              1000004000
#define S_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR           1000006000
#define S_TYPE_WIN32_SURFACE_CREATE_INFO_KHR             1000009000
#define S_TYPE_MACOS_SURFACE_CREATE_INFO_MVK             1000123000

// ── Hardcoded Vulkan enum values ──────────────────────────────────

#define VK_SUCCESS                                       0
#define VK_NOT_READY                                     1
#define VK_TIMEOUT                                       2
#define VK_EVENT_SET                                     3
#define VK_EVENT_RESET                                   4
#define VK_INCOMPLETE                                    5
#define VK_ERROR_OUT_OF_HOST_MEMORY                      (-1)
#define VK_ERROR_OUT_OF_DEVICE_MEMORY                    (-2)
#define VK_ERROR_INITIALIZATION_FAILED                   (-3)
#define VK_ERROR_DEVICE_LOST                             (-4)
#define VK_ERROR_MEMORY_MAP_FAILED                       (-5)
#define VK_ERROR_LAYER_NOT_PRESENT                       (-6)
#define VK_ERROR_EXTENSION_NOT_PRESENT                   (-7)
#define VK_ERROR_FEATURE_NOT_PRESENT                     (-8)
#define VK_ERROR_INCOMPATIBLE_DRIVER                     (-9)
#define VK_ERROR_TOO_MANY_OBJECTS                        (-10)
#define VK_ERROR_FORMAT_NOT_SUPPORTED                    (-11)
#define VK_ERROR_FRAGMENTED_POOL                         (-12)
#define VK_ERROR_UNKNOWN                                 (-13)
#define VK_ERROR_OUT_OF_POOL_MEMORY                      (-1000069000)
#define VK_ERROR_INVALID_EXTERNAL_HANDLE                 (-1000072003)
#define VK_ERROR_FRAGMENTATION                           (-1000161000)
#define VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS          (-1000257000)
#define VK_ERROR_SURFACE_LOST_KHR                        (-1000000000)
#define VK_ERROR_NATIVE_WINDOW_IN_USE_KHR                (-1000000001)
#define VK_SUBOPTIMAL_KHR                                1000001003
#define VK_ERROR_OUT_OF_DATE_KHR                         (-1000001004)
#define VK_ERROR_INCOMPATIBLE_DISPLAY_KHR                (-1000003001)
#define VK_ERROR_VALIDATION_FAILED_EXT                   (-1000011001)
#define VK_ERROR_INVALID_SHADER_NV                       (-1000012000)

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES   49
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES   50
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES   57

#define VK_PHYSICAL_DEVICE_TYPE_OTHER                0
#define VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU       1
#define VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU         2
#define VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU          3
#define VK_PHYSICAL_DEVICE_TYPE_CPU                  4

#define VK_QUEUE_GRAPHICS_BIT                        0x00000001
#define VK_QUEUE_COMPUTE_BIT                         0x00000002
#define VK_QUEUE_TRANSFER_BIT                        0x00000004

#define VK_FORMAT_B8G8R8A8_SRGB                      44
#define VK_FORMAT_B8G8R8A8_UNORM                     50

#define VK_COLOR_SPACE_SRGB_NONLINEAR_KHR             0

#define VK_PRESENT_MODE_IMMEDIATE_KHR                 0
#define VK_PRESENT_MODE_MAILBOX_KHR                   1
#define VK_PRESENT_MODE_FIFO_KHR                      2
#define VK_PRESENT_MODE_FIFO_RELAXED_KHR              3

#define VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT           0x00000010
#define VK_IMAGE_USAGE_TRANSFER_SRC_BIT               0x00000001
#define VK_IMAGE_USAGE_TRANSFER_DST_BIT               0x00000002

#define VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR         0x00000001
#define VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR             0x00000001

#define VK_SHARING_MODE_EXCLUSIVE                     0
#define VK_SHARING_MODE_CONCURRENT                    1

#define VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT  0x00000001
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT  0x00000001
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY               0

#define VK_FENCE_CREATE_SIGNALED_BIT                  0x00000001

#define VK_IMAGE_VIEW_TYPE_2D                         1

#define VK_COMPONENT_SWIZZLE_IDENTITY                 0

#define VK_IMAGE_ASPECT_COLOR_BIT                     0x00000001

#define VK_IMAGE_LAYOUT_UNDEFINED                     0
#define VK_IMAGE_LAYOUT_PRESENT_SRC_KHR               1000001002
#define VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL      2

#define VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 0x00000400
#define VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT             0x00000001
#define VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT          0x00002000

#define VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT          0x00000080
#define VK_ACCESS_MEMORY_READ_BIT                     0x00000800
#define VK_ACCESS_COLOR_ATTACHMENT_READ_BIT           0x00000080

#define VK_DEPENDENCY_BY_REGION_BIT                   0x00000001

#define VK_ATTACHMENT_LOAD_OP_CLEAR                   1
#define VK_ATTACHMENT_STORE_OP_STORE                  0

#define VK_PIPELINE_BIND_POINT_GRAPHICS               0

#define VK_SUBPASS_CONTENTS_INLINE                    0

#define VK_ATTACHMENT_LOAD_OP_DONT_CARE               0
#define VK_ATTACHMENT_STORE_OP_DONT_CARE              0
#define VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL      5

#define VK_API_VERSION_1_0    (1u << 22)
#define VK_API_VERSION_1_3    (1u << 22) | (3u << 12)

// ============================================================================
//  SECTION 1: Dynamic loader (~180 lines)
// ============================================================================

#ifdef _WIN32
static HMODULE g_vulkan_loader = NULL;
#else
static void* g_vulkan_loader = NULL;
#endif

// ── 43 PFN function pointer typedefs ──────────────────────────────

typedef VkLoaderProcAddress (*PFN_vkGetInstanceProcAddr)(VkInstance, const char*);
typedef VkLoaderProcAddress (*PFN_vkGetDeviceProcAddr)(VkDevice, const char*);
typedef VkResult (*PFN_vkEnumerateInstanceVersion)(uint32_t*);
typedef VkResult (*PFN_vkEnumerateInstanceExtensionProperties)(const char*, uint32_t*, void*);
typedef VkResult (*PFN_vkEnumerateInstanceLayerProperties)(uint32_t*, void*);
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo*, const void*, VkInstance*);
typedef void     (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void     (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, VkPhysicalDeviceProperties*);
typedef void     (*PFN_vkGetPhysicalDeviceFeatures)(VkPhysicalDevice, void*);
typedef void     (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, uint32_t*, VkQueueFamilyProperties*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const void*, VkDevice*);
typedef void     (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void     (*PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef VkResult (*PFN_vkDeviceWaitIdle)(VkDevice);
typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
typedef VkResult (*PFN_vkCreateXlibSurfaceKHR)(VkInstance, const VkXlibSurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
typedef VkResult (*PFN_vkCreateWaylandSurfaceKHR)(VkInstance, const VkWaylandSurfaceCreateInfoKHR*, const void*, VkSurfaceKHR*);
typedef VkResult (*PFN_vkCreateMacOSSurfaceMVK)(VkInstance, const VkMacOSSurfaceCreateInfoMVK*, const void*, VkSurfaceKHR*);
typedef void     (*PFN_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const void*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, uint32_t*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, VkSurfaceFormatKHR*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, uint32_t*);
typedef VkResult (*PFN_vkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR*, const void*, VkSwapchainKHR*);
typedef void     (*PFN_vkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const void*);
typedef VkResult (*PFN_vkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
typedef VkResult (*PFN_vkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*);
typedef VkResult (*PFN_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*);
typedef void     (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
typedef VkResult (*PFN_vkCreateSemaphore)(VkDevice, const VkSemaphoreCreateInfo*, const void*, VkSemaphore*);
typedef void     (*PFN_vkDestroySemaphore)(VkDevice, VkSemaphore, const void*);
typedef VkResult (*PFN_vkCreateFence)(VkDevice, const VkFenceCreateInfo*, const void*, VkFence*);
typedef void     (*PFN_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice, uint32_t, const VkFence*, uint32_t, uint64_t);
typedef VkResult (*PFN_vkResetFences)(VkDevice, uint32_t, const VkFence*);
typedef VkResult (*PFN_vkCreateImageView)(VkDevice, const VkImageViewCreateInfo*, const void*, VkImageView*);
typedef void     (*PFN_vkDestroyImageView)(VkDevice, VkImageView, const void*);

// ── Static PFN table ──────────────────────────────────────────────

static PFN_vkGetInstanceProcAddr                    pfn_vkGetInstanceProcAddr = NULL;
static PFN_vkGetDeviceProcAddr                      pfn_vkGetDeviceProcAddr = NULL;
static PFN_vkEnumerateInstanceVersion               pfn_vkEnumerateInstanceVersion = NULL;
static PFN_vkEnumerateInstanceExtensionProperties   pfn_vkEnumerateInstanceExtensionProperties = NULL;
static PFN_vkEnumerateInstanceLayerProperties       pfn_vkEnumerateInstanceLayerProperties = NULL;
static PFN_vkCreateInstance                         pfn_vkCreateInstance = NULL;
static PFN_vkDestroyInstance                        pfn_vkDestroyInstance = NULL;
static PFN_vkEnumeratePhysicalDevices               pfn_vkEnumeratePhysicalDevices = NULL;
static PFN_vkGetPhysicalDeviceProperties            pfn_vkGetPhysicalDeviceProperties = NULL;
static PFN_vkGetPhysicalDeviceFeatures              pfn_vkGetPhysicalDeviceFeatures = NULL;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties = NULL;
static PFN_vkCreateDevice                           pfn_vkCreateDevice = NULL;
static PFN_vkDestroyDevice                          pfn_vkDestroyDevice = NULL;
static PFN_vkGetDeviceQueue                         pfn_vkGetDeviceQueue = NULL;
static PFN_vkDeviceWaitIdle                         pfn_vkDeviceWaitIdle = NULL;
static PFN_vkCreateWin32SurfaceKHR                  pfn_vkCreateWin32SurfaceKHR = NULL;
static PFN_vkCreateXlibSurfaceKHR                   pfn_vkCreateXlibSurfaceKHR = NULL;
static PFN_vkCreateWaylandSurfaceKHR                pfn_vkCreateWaylandSurfaceKHR = NULL;
static PFN_vkCreateMacOSSurfaceMVK                  pfn_vkCreateMacOSSurfaceMVK = NULL;
static PFN_vkDestroySurfaceKHR                      pfn_vkDestroySurfaceKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR     pfn_vkGetPhysicalDeviceSurfaceSupportKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR     pfn_vkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_vkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
static PFN_vkCreateSwapchainKHR                     pfn_vkCreateSwapchainKHR = NULL;
static PFN_vkDestroySwapchainKHR                    pfn_vkDestroySwapchainKHR = NULL;
static PFN_vkGetSwapchainImagesKHR                  pfn_vkGetSwapchainImagesKHR = NULL;
static PFN_vkAcquireNextImageKHR                    pfn_vkAcquireNextImageKHR = NULL;
static PFN_vkQueuePresentKHR                        pfn_vkQueuePresentKHR = NULL;
static PFN_vkCreateCommandPool                      pfn_vkCreateCommandPool = NULL;
static PFN_vkDestroyCommandPool                     pfn_vkDestroyCommandPool = NULL;
static PFN_vkAllocateCommandBuffers                 pfn_vkAllocateCommandBuffers = NULL;
static PFN_vkBeginCommandBuffer                     pfn_vkBeginCommandBuffer = NULL;
static PFN_vkEndCommandBuffer                       pfn_vkEndCommandBuffer = NULL;
static PFN_vkQueueSubmit                            pfn_vkQueueSubmit = NULL;
static PFN_vkCreateSemaphore                        pfn_vkCreateSemaphore = NULL;
static PFN_vkDestroySemaphore                       pfn_vkDestroySemaphore = NULL;
static PFN_vkCreateFence                            pfn_vkCreateFence = NULL;
static PFN_vkDestroyFence                           pfn_vkDestroyFence = NULL;
static PFN_vkWaitForFences                          pfn_vkWaitForFences = NULL;
static PFN_vkResetFences                            pfn_vkResetFences = NULL;
static PFN_vkCreateImageView                        pfn_vkCreateImageView = NULL;
static PFN_vkDestroyImageView                       pfn_vkDestroyImageView = NULL;

/* Rendering pipeline PFNs (for blade-level raw Vulkan consumers) */
typedef VkResult (*PFN_vkCreateRenderPass)(VkDevice, const VkRenderPassCreateInfo*, const void*, VkRenderPass*);
typedef void     (*PFN_vkDestroyRenderPass)(VkDevice, VkRenderPass, const void*);
typedef VkResult (*PFN_vkCreateShaderModule)(VkDevice, const VkShaderModuleCreateInfo*, const void*, VkShaderModule*);
typedef void     (*PFN_vkDestroyShaderModule)(VkDevice, VkShaderModule, const void*);
typedef VkResult (*PFN_vkCreatePipelineLayout)(VkDevice, const VkPipelineLayoutCreateInfo*, const void*, VkPipelineLayout*);
typedef void     (*PFN_vkDestroyPipelineLayout)(VkDevice, VkPipelineLayout, const void*);
typedef VkResult (*PFN_vkCreateGraphicsPipelines)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const void*, VkPipeline*);
typedef void     (*PFN_vkDestroyPipeline)(VkDevice, VkPipeline, const void*);
typedef VkResult (*PFN_vkCreateFramebuffer)(VkDevice, const VkFramebufferCreateInfo*, const void*, VkFramebuffer*);
typedef void     (*PFN_vkDestroyFramebuffer)(VkDevice, VkFramebuffer, const void*);
typedef VkResult (*PFN_vkResetCommandBuffer)(VkCommandBuffer, VkCommandBufferResetFlags);
typedef void     (*PFN_vkCmdBeginRenderPass)(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
typedef void     (*PFN_vkCmdEndRenderPass)(VkCommandBuffer);
typedef void     (*PFN_vkCmdBindPipeline)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
typedef void     (*PFN_vkCmdPushConstants)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
typedef void     (*PFN_vkCmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);

static PFN_vkCreateRenderPass        pfn_vkCreateRenderPass = NULL;
static PFN_vkDestroyRenderPass       pfn_vkDestroyRenderPass = NULL;
static PFN_vkCreateShaderModule      pfn_vkCreateShaderModule = NULL;
static PFN_vkDestroyShaderModule     pfn_vkDestroyShaderModule = NULL;
static PFN_vkCreatePipelineLayout    pfn_vkCreatePipelineLayout = NULL;
static PFN_vkDestroyPipelineLayout   pfn_vkDestroyPipelineLayout = NULL;
static PFN_vkCreateGraphicsPipelines pfn_vkCreateGraphicsPipelines = NULL;
static PFN_vkDestroyPipeline         pfn_vkDestroyPipeline = NULL;
static PFN_vkCreateFramebuffer       pfn_vkCreateFramebuffer = NULL;
static PFN_vkDestroyFramebuffer      pfn_vkDestroyFramebuffer = NULL;
static PFN_vkResetCommandBuffer      pfn_vkResetCommandBuffer = NULL;
static PFN_vkCmdBeginRenderPass      pfn_vkCmdBeginRenderPass = NULL;
static PFN_vkCmdEndRenderPass        pfn_vkCmdEndRenderPass = NULL;
static PFN_vkCmdBindPipeline         pfn_vkCmdBindPipeline = NULL;
static PFN_vkCmdPushConstants        pfn_vkCmdPushConstants = NULL;
static PFN_vkCmdDraw                 pfn_vkCmdDraw = NULL;

static int g_vulkan_loader_opened = 0;

// ── vulkan_abi_open_loader ────────────────────────────────────────

static int vulkan_abi_open_loader(void) {
    if (g_vulkan_loader_opened) return 1;

#ifdef _WIN32
    g_vulkan_loader = LoadLibraryA("vulkan-1.dll");
    if (g_vulkan_loader == NULL) return 0;

    pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        GetProcAddress(g_vulkan_loader, "vkGetInstanceProcAddr");
#else
    g_vulkan_loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#if defined(__APPLE__)
    if (g_vulkan_loader == NULL) {
        g_vulkan_loader = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    }
#endif
    if (g_vulkan_loader == NULL) return 0;

    pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        dlsym(g_vulkan_loader, "vkGetInstanceProcAddr");
#endif

    if (pfn_vkGetInstanceProcAddr == NULL) return 0;

    /* Global (pre-instance) PFNs from the loader */
    pfn_vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)
        pfn_vkGetInstanceProcAddr((VkInstance)0, "vkEnumerateInstanceVersion");
    pfn_vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)
        pfn_vkGetInstanceProcAddr((VkInstance)0, "vkEnumerateInstanceExtensionProperties");
    pfn_vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)
        pfn_vkGetInstanceProcAddr((VkInstance)0, "vkEnumerateInstanceLayerProperties");
    pfn_vkCreateInstance = (PFN_vkCreateInstance)
        pfn_vkGetInstanceProcAddr((VkInstance)0, "vkCreateInstance");

    g_vulkan_loader_opened = 1;
    vulkan_abi_fill_pfn_table();  /* publish loader-level PFNs immediately */
    return 1;
}

// ── vulkan_abi_resolve_instance_pfns ──────────────────────────────

static void vulkan_abi_resolve_instance_pfns(VkInstance instance) {
    if (instance == (VkInstance)0) return;

    pfn_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)
        pfn_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    pfn_vkDestroyInstance = (PFN_vkDestroyInstance)
        pfn_vkGetInstanceProcAddr(instance, "vkDestroyInstance");
    pfn_vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)
        pfn_vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    pfn_vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    pfn_vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures");
    pfn_vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    pfn_vkCreateDevice = (PFN_vkCreateDevice)
        pfn_vkGetInstanceProcAddr(instance, "vkCreateDevice");

#ifdef _WIN32
    pfn_vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
#endif
#if defined(__linux__) && !defined(VK_USE_PLATFORM_WAYLAND_KHR)
    pfn_vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
#endif
#if defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    pfn_vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
#endif
#if defined(__APPLE__)
    pfn_vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)
        pfn_vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");
#endif

    pfn_vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    pfn_vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)
        pfn_vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
}

// ── vulkan_abi_resolve_device_pfns ────────────────────────────────

static void vulkan_abi_resolve_device_pfns(VkDevice device) {
    if (device == (VkDevice)0) return;

    pfn_vkDestroyDevice = (PFN_vkDestroyDevice)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyDevice");
    pfn_vkGetDeviceQueue = (PFN_vkGetDeviceQueue)
        pfn_vkGetDeviceProcAddr(device, "vkGetDeviceQueue");
    pfn_vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)
        pfn_vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
    pfn_vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)
        pfn_vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
    pfn_vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)
        pfn_vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR");
    pfn_vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)
        pfn_vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
    pfn_vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)
        pfn_vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
    pfn_vkQueuePresentKHR = (PFN_vkQueuePresentKHR)
        pfn_vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
    pfn_vkCreateCommandPool = (PFN_vkCreateCommandPool)
        pfn_vkGetDeviceProcAddr(device, "vkCreateCommandPool");
    pfn_vkDestroyCommandPool = (PFN_vkDestroyCommandPool)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyCommandPool");
    pfn_vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)
        pfn_vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
    pfn_vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)
        pfn_vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
    pfn_vkEndCommandBuffer = (PFN_vkEndCommandBuffer)
        pfn_vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
    pfn_vkQueueSubmit = (PFN_vkQueueSubmit)
        pfn_vkGetDeviceProcAddr(device, "vkQueueSubmit");
    pfn_vkCreateSemaphore = (PFN_vkCreateSemaphore)
        pfn_vkGetDeviceProcAddr(device, "vkCreateSemaphore");
    pfn_vkDestroySemaphore = (PFN_vkDestroySemaphore)
        pfn_vkGetDeviceProcAddr(device, "vkDestroySemaphore");
    pfn_vkCreateFence = (PFN_vkCreateFence)
        pfn_vkGetDeviceProcAddr(device, "vkCreateFence");
    pfn_vkDestroyFence = (PFN_vkDestroyFence)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyFence");
    pfn_vkWaitForFences = (PFN_vkWaitForFences)
        pfn_vkGetDeviceProcAddr(device, "vkWaitForFences");
    pfn_vkResetFences = (PFN_vkResetFences)
        pfn_vkGetDeviceProcAddr(device, "vkResetFences");
    pfn_vkCreateImageView = (PFN_vkCreateImageView)
        pfn_vkGetDeviceProcAddr(device, "vkCreateImageView");
    pfn_vkDestroyImageView = (PFN_vkDestroyImageView)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyImageView");

    /* Rendering pipeline PFNs (for blade-level raw Vulkan consumers) */
    pfn_vkCreateRenderPass = (PFN_vkCreateRenderPass)
        pfn_vkGetDeviceProcAddr(device, "vkCreateRenderPass");
    pfn_vkDestroyRenderPass = (PFN_vkDestroyRenderPass)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyRenderPass");
    pfn_vkCreateShaderModule = (PFN_vkCreateShaderModule)
        pfn_vkGetDeviceProcAddr(device, "vkCreateShaderModule");
    pfn_vkDestroyShaderModule = (PFN_vkDestroyShaderModule)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyShaderModule");
    pfn_vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)
        pfn_vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
    pfn_vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout");
    pfn_vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)
        pfn_vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines");
    pfn_vkDestroyPipeline = (PFN_vkDestroyPipeline)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyPipeline");
    pfn_vkCreateFramebuffer = (PFN_vkCreateFramebuffer)
        pfn_vkGetDeviceProcAddr(device, "vkCreateFramebuffer");
    pfn_vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyFramebuffer");
    pfn_vkResetCommandBuffer = (PFN_vkResetCommandBuffer)
        pfn_vkGetDeviceProcAddr(device, "vkResetCommandBuffer");
    pfn_vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)
        pfn_vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass");
    pfn_vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)
        pfn_vkGetDeviceProcAddr(device, "vkCmdEndRenderPass");
    pfn_vkCmdBindPipeline = (PFN_vkCmdBindPipeline)
        pfn_vkGetDeviceProcAddr(device, "vkCmdBindPipeline");
    pfn_vkCmdPushConstants = (PFN_vkCmdPushConstants)
        pfn_vkGetDeviceProcAddr(device, "vkCmdPushConstants");
    pfn_vkCmdDraw = (PFN_vkCmdDraw)
        pfn_vkGetDeviceProcAddr(device, "vkCmdDraw");

    vulkan_abi_fill_pfn_table();  /* re-publish with device-level PFNs */
}

// ── vulkan_abi_close_loader ───────────────────────────────────────

static void vulkan_abi_close_loader(void) {
    if (!g_vulkan_loader_opened) return;
    g_vulkan_loader_opened = 0;
    if (g_vulkan_loader != NULL) {
#ifdef _WIN32
        FreeLibrary(g_vulkan_loader);
#else
        dlclose(g_vulkan_loader);
#endif
        g_vulkan_loader = NULL;
    }
}

// ============================================================================
//  SECTION 2: WSI surface creation (~100 lines)
// ============================================================================

static VkResult vulkan_abi_create_surface(KainVulkanSession* session) {
    VkResult result = VK_SUCCESS;

#ifdef _WIN32
    if (session->hwnd == NULL || session->hinstance == NULL)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkWin32SurfaceCreateInfoKHR info;
    memset(&info, 0, sizeof(info));
    info.sType     = S_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hinstance = session->hinstance;
    info.hwnd      = session->hwnd;

    result = pfn_vkCreateWin32SurfaceKHR(session->instance, &info, NULL,
                                          &session->surface);
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    /* Wayland — requires wl_display + wl_surface on the session */
    if (session->x11_display == NULL)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkWaylandSurfaceCreateInfoKHR info;
    memset(&info, 0, sizeof(info));
    info.sType   = S_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    info.display = session->x11_display;
    info.surface = (void*)session->x11_window;

    result = pfn_vkCreateWaylandSurfaceKHR(session->instance, &info, NULL,
                                            &session->surface);
#elif defined(__linux__)
    /* X11 */
    if (session->x11_display == NULL)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkXlibSurfaceCreateInfoKHR info;
    memset(&info, 0, sizeof(info));
    info.sType  = S_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    info.dpy    = session->x11_display;
    info.window = session->x11_window;

    result = pfn_vkCreateXlibSurfaceKHR(session->instance, &info, NULL,
                                         &session->surface);
#elif defined(__APPLE__)
    /* MoltenVK: CAMetalLayer */
    VkMacOSSurfaceCreateInfoMVK info;
    memset(&info, 0, sizeof(info));
    info.sType = S_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    info.pView = session->x11_display; /* reused as metal_layer */

    result = pfn_vkCreateMacOSSurfaceMVK(session->instance, &info, NULL,
                                          &session->surface);
#endif

    return result;
}

// ============================================================================
//  SECTION 3: Physical device selection (~80 lines)
// ============================================================================

static int vulkan_abi_select_physical_device(
    VkInstance        instance,
    VkSurfaceKHR      surface,
    VkPhysicalDevice* out_device,
    uint32_t*         out_graphics_qf,
    uint32_t*         out_present_qf)
{
    uint32_t device_count = 0;
    VkResult result = pfn_vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (result != VK_SUCCESS || device_count == 0) return -1;

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(
        sizeof(VkPhysicalDevice) * device_count);
    if (!devices) return -1;

    result = pfn_vkEnumeratePhysicalDevices(instance, &device_count, devices);
    if (result != VK_SUCCESS) { free(devices); return -1; }

    VkPhysicalDevice best_device   = (VkPhysicalDevice)0;
    uint32_t         best_graphics = 0xFFFFFFFF;
    uint32_t         best_present  = 0xFFFFFFFF;
    int              best_score    = -1;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        pfn_vkGetPhysicalDeviceProperties(devices[i], &props);

        /* Check for graphics + present queue families */
        uint32_t qf_count = 0;
        pfn_vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qf_count, NULL);
        if (qf_count == 0) continue;

        VkQueueFamilyProperties* qf_props = (VkQueueFamilyProperties*)
            malloc(sizeof(VkQueueFamilyProperties) * qf_count);
        if (!qf_props) continue;

        pfn_vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qf_count, qf_props);

        uint32_t graphics_family = 0xFFFFFFFF;
        uint32_t present_family  = 0xFFFFFFFF;

        for (uint32_t j = 0; j < qf_count; j++) {
            if (qf_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (graphics_family == 0xFFFFFFFF) graphics_family = j;
            }
        }

        /* Find a queue family that can present to our surface */
        for (uint32_t j = 0; j < qf_count; j++) {
            uint32_t supports = 0;
            if (surface != (VkSurfaceKHR)0 &&
                pfn_vkGetPhysicalDeviceSurfaceSupportKHR != NULL) {
                pfn_vkGetPhysicalDeviceSurfaceSupportKHR(
                    devices[i], j, surface, &supports);
            }
            if (supports && present_family == 0xFFFFFFFF) {
                present_family = j;
            }
        }
        /* If no dedicated present family, reuse graphics */
        if (present_family == 0xFFFFFFFF) present_family = graphics_family;

        free(qf_props);

        if (graphics_family == 0xFFFFFFFF) continue;

        /* Score: discrete GPU wins; integrated GPU next; others catch-all */
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score = 3;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            score = 2;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            score = 1;

        if (score > best_score) {
            best_score    = score;
            best_device   = devices[i];
            best_graphics = graphics_family;
            best_present  = present_family;
        }
    }

    free(devices);

    if (best_device == (VkPhysicalDevice)0) return -1;

    *out_device       = best_device;
    *out_graphics_qf  = best_graphics;
    *out_present_qf   = best_present;
    return 0;
}

// ============================================================================
//  SECTION 4: Logical device creation (~80 lines)
// ============================================================================

static int vulkan_abi_create_device(
    VkPhysicalDevice physical_device,
    uint32_t         graphics_qf,
    uint32_t         present_qf,
    VkDevice*        out_device,
    VkQueue*         out_graphics_queue,
    VkQueue*         out_present_queue)
{
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo qf_infos[2];
    memset(qf_infos, 0, sizeof(qf_infos));
    uint32_t qf_count = (graphics_qf == present_qf) ? 1 : 2;

    qf_infos[0].sType            = S_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qf_infos[0].queueFamilyIndex = graphics_qf;
    qf_infos[0].queueCount       = 1;
    qf_infos[0].pQueuePriorities = &queue_priority;

    if (qf_count > 1) {
        qf_infos[1].sType            = S_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qf_infos[1].queueFamilyIndex = present_qf;
        qf_infos[1].queueCount       = 1;
        qf_infos[1].pQueuePriorities = &queue_priority;
    }

    const char* extensions[] = { "VK_KHR_swapchain" };

    VkDeviceCreateInfo device_info;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType                   = S_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount    = qf_count;
    device_info.pQueueCreateInfos       = qf_infos;
    device_info.enabledExtensionCount   = 1;
    device_info.ppEnabledExtensionNames = extensions;

    VkResult result = pfn_vkCreateDevice(physical_device, &device_info,
                                          NULL, out_device);
    if (result != VK_SUCCESS) return -1;

    pfn_vkGetDeviceQueue(*out_device, graphics_qf, 0, out_graphics_queue);
    pfn_vkGetDeviceQueue(*out_device, present_qf, 0, out_present_queue);

    return 0;
}

// ============================================================================
//  SECTION 5: Swapchain lifecycle (~200 lines)
// ============================================================================

static int vulkan_abi_create_swapchain(KainVulkanSession* session) {
    VkPhysicalDevice physical_device = session->physical_device;
    VkDevice         device         = session->device;
    VkSurfaceKHR     surface        = session->surface;
    int64_t          width          = session->width;
    int64_t          height         = session->height;

    if (width <= 0 || height <= 0) return -1;

    /* ── Query surface capabilities ── */
    VkSurfaceCapabilitiesKHR caps;
    memset(&caps, 0, sizeof(caps));
    VkResult result = pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, surface, &caps);
    if (result != VK_SUCCESS) return -2;

    /* Choose extent */
    VkExtent2D extent;
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        extent = caps.currentExtent;
    } else {
        extent.width  = (uint32_t)(width > 0 ? width : 800);
        extent.height = (uint32_t)(height > 0 ? height : 600);
        if (extent.width  < caps.minImageExtent.width)  extent.width  = caps.minImageExtent.width;
        if (extent.width  > caps.maxImageExtent.width)  extent.width  = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }

    /* ── Choose surface format ── */
    uint32_t format_count = 0;
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                              &format_count, NULL);
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)
        malloc(sizeof(VkSurfaceFormatKHR) * (format_count > 0 ? format_count : 1));
    if (!formats) return -3;

    VkSurfaceFormatKHR chosen_format;
    if (format_count == 0) {
        chosen_format.format     = VK_FORMAT_B8G8R8A8_UNORM;
        chosen_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    } else {
        pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                                  &format_count, formats);
        chosen_format = formats[0]; /* default to first */
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_format = formats[i];
                break;
            }
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_format = formats[i];
            }
        }
    }
    free(formats);

    /* ── Choose present mode ── */
    uint32_t present_mode_count = 0;
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                                   &present_mode_count, NULL);
    uint32_t* present_modes = (uint32_t*)
        malloc(sizeof(uint32_t) * (present_mode_count > 0 ? present_mode_count : 1));
    if (!present_modes) return -3;

    VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (present_mode_count > 0) {
        pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                                       &present_mode_count,
                                                       present_modes);
        for (uint32_t i = 0; i < present_mode_count; i++) {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                chosen_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                chosen_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }
    free(present_modes);

    /* ── Choose image count ── */
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;
    if (image_count < 2) image_count = 2;
    if (image_count > KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES)
        image_count = KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES;

    /* ── Build swapchain create info ── */
    VkSwapchainCreateInfoKHR swapchain_info;
    memset(&swapchain_info, 0, sizeof(swapchain_info));
    swapchain_info.sType            = S_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface          = surface;
    swapchain_info.minImageCount    = image_count;
    swapchain_info.imageFormat      = chosen_format.format;
    swapchain_info.imageColorSpace  = chosen_format.colorSpace;
    swapchain_info.imageExtent      = extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform     = caps.currentTransform;
    swapchain_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode      = chosen_present_mode;
    swapchain_info.clipped          = 1;
    swapchain_info.oldSwapchain     = (VkSwapchainKHR)0;

    if (session->graphics_queue_family != session->present_queue_family) {
        uint32_t families[] = { session->graphics_queue_family,
                                 session->present_queue_family };
        swapchain_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices   = families;
    }

    /* ── Create swapchain ── */
    result = pfn_vkCreateSwapchainKHR(device, &swapchain_info, NULL,
                                       &session->swapchain);
    if (result != VK_SUCCESS) return -4;

    /* ── Get swapchain images ── */
    session->swapchain_image_count = 0;
    result = pfn_vkGetSwapchainImagesKHR(device, session->swapchain,
                                          &session->swapchain_image_count, NULL);
    if (result != VK_SUCCESS || session->swapchain_image_count == 0) return -5;

    if (session->swapchain_image_count > KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES)
        session->swapchain_image_count = KAIN_VULKAN_ABI_MAX_SWAPCHAIN_IMAGES;

    result = pfn_vkGetSwapchainImagesKHR(device, session->swapchain,
                                          &session->swapchain_image_count,
                                          session->swapchain_images);
    if (result != VK_SUCCESS) return -5;

    /* ── Create image views ── */
    VkComponentMapping components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
    VkImageSubresourceRange subresource = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
    };

    for (uint32_t i = 0; i < session->swapchain_image_count; i++) {
        VkImageViewCreateInfo view_info;
        memset(&view_info, 0, sizeof(view_info));
        view_info.sType      = S_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image      = session->swapchain_images[i];
        view_info.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format     = chosen_format.format;
        view_info.components = components;
        view_info.subresourceRange = subresource;

        result = pfn_vkCreateImageView(device, &view_info, NULL,
                                        &session->swapchain_image_views[i]);
        if (result != VK_SUCCESS) return -6;
    }

    session->width  = (int64_t)extent.width;
    session->height = (int64_t)extent.height;

    return 0;
}

// ── vulkan_abi_recreate_swapchain ─────────────────────────────────

static int vulkan_abi_recreate_swapchain(KainVulkanSession* session) {
    if (session->device == (VkDevice)0) return -1;

    /* Wait for device idle before tearing down */
    pfn_vkDeviceWaitIdle(session->device);

    /* Destroy old image views */
    for (uint32_t i = 0; i < session->swapchain_image_count; i++) {
        if (session->framebuffers[i] != (VkFramebuffer)0) {
            /* Framebuffers will be destroyed in session_destroy */
            session->framebuffers[i] = (VkFramebuffer)0;
        }
        if (session->swapchain_image_views[i] != (VkImageView)0) {
            pfn_vkDestroyImageView(session->device,
                                    session->swapchain_image_views[i], NULL);
            session->swapchain_image_views[i] = (VkImageView)0;
        }
    }

    /* Recreate swapchain */
    int rc = vulkan_abi_create_swapchain(session);
    if (rc != 0) return rc;

    /* Increment telemetry counter */
    /* g_vulkan_abi_vtable.swapchain_recreations++ is done by caller */

    return 0;
}

// ============================================================================
//  SECTION 6: Frame submission (~120 lines)
// ============================================================================

static int vulkan_abi_create_sync_objects(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    /* ── Command pool ── */
    VkCommandPoolCreateInfo pool_info;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType            = S_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = session->graphics_queue_family;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = pfn_vkCreateCommandPool(device, &pool_info, NULL,
                                               &session->command_pool);
    if (result != VK_SUCCESS) return -1;

    /* ── Command buffers ── */
    VkCommandBufferAllocateInfo alloc_info;
    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType              = S_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = session->command_pool;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT;

    result = pfn_vkAllocateCommandBuffers(device, &alloc_info,
                                           session->command_buffers);
    if (result != VK_SUCCESS) return -1;

    /* ── Semaphores + fences ── */
    VkSemaphoreCreateInfo sem_info;
    memset(&sem_info, 0, sizeof(sem_info));
    sem_info.sType = S_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = S_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
        result = pfn_vkCreateSemaphore(device, &sem_info, NULL,
                                        &session->image_available[i]);
        if (result != VK_SUCCESS) return -2;

        result = pfn_vkCreateSemaphore(device, &sem_info, NULL,
                                        &session->render_finished[i]);
        if (result != VK_SUCCESS) return -2;

        result = pfn_vkCreateFence(device, &fence_info, NULL,
                                    &session->in_flight_fences[i]);
        if (result != VK_SUCCESS) return -2;
    }

    return 0;
}

// ── begin_frame ──────────────────────────────────────────────────

static int vulkan_abi_begin_frame(KainVulkanSession* session) {
    VkDevice device = session->device;
    uint32_t frame  = session->current_frame;

    /* Wait for in-flight fence */
    pfn_vkWaitForFences(device, 1, &session->in_flight_fences[frame],
                         1, UINT64_MAX);

    /* Acquire next image */
    VkResult result = pfn_vkAcquireNextImageKHR(
        device, session->swapchain, UINT64_MAX,
        session->image_available[frame], (VkFence)0,
        &session->current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return -1; /* signal swapchain recreation needed */
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return -2;

    /* Reset fence */
    pfn_vkResetFences(device, 1, &session->in_flight_fences[frame]);

    /* Begin command buffer */
    VkCommandBufferBeginInfo begin_info;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = S_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = pfn_vkBeginCommandBuffer(session->command_buffers[frame],
                                       &begin_info);
    if (result != VK_SUCCESS) return -3;

    return 0;
}

// ── end_frame ────────────────────────────────────────────────────

static int vulkan_abi_end_frame(KainVulkanSession* session) {
    VkResult result = pfn_vkEndCommandBuffer(
        session->command_buffers[session->current_frame]);
    return (result == VK_SUCCESS) ? 0 : -1;
}

// ── present ──────────────────────────────────────────────────────

static int vulkan_abi_present(KainVulkanSession* session) {
    VkDevice device = session->device;
    uint32_t frame  = session->current_frame;
    uint32_t image  = session->current_image_index;

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit_info;
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType                = S_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &session->image_available[frame];
    submit_info.pWaitDstStageMask    = wait_stages;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &session->command_buffers[frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &session->render_finished[frame];

    VkResult result = pfn_vkQueueSubmit(session->graphics_queue, 1,
                                         &submit_info,
                                         session->in_flight_fences[frame]);
    if (result != VK_SUCCESS) return -1;

    VkPresentInfoKHR present_info;
    memset(&present_info, 0, sizeof(present_info));
    present_info.sType              = S_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &session->render_finished[frame];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &session->swapchain;
    present_info.pImageIndices      = &image;

    result = pfn_vkQueuePresentKHR(session->present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return -2; /* signal swapchain recreation needed */
    }
    if (result != VK_SUCCESS) return -3;

    /* Advance frame ring */
    session->current_frame = (session->current_frame + 1) %
                              KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT;

    return 0;
}

// ============================================================================
//  SECTION 8: Error handling (~40 lines)
// ============================================================================

/* Forward declaration — vtable defined in Section 9 */
extern KainVulkanAbiVtable g_vulkan_abi_vtable;

static void vulkan_abi_set_error(int64_t status, const char* message) {
    g_vulkan_abi_vtable.last_status = status;
    if (message) {
        snprintf(g_vulkan_abi_vtable.last_error,
                 sizeof(g_vulkan_abi_vtable.last_error), "%s", message);
    } else {
        g_vulkan_abi_vtable.last_error[0] = '\0';
    }
}

static const char* vulkan_abi_result_name(VkResult result) {
    switch ((int32_t)result) {
        case VK_SUCCESS:                            return "VK_SUCCESS";
        case VK_NOT_READY:                          return "VK_NOT_READY";
        case VK_TIMEOUT:                            return "VK_TIMEOUT";
        case VK_EVENT_SET:                          return "VK_EVENT_SET";
        case VK_EVENT_RESET:                        return "VK_EVENT_RESET";
        case VK_INCOMPLETE:                         return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:           return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:         return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:        return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:                  return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:        return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:          return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:          return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:             return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:         return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:              return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN:                      return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY:           return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:      return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR:             return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:     return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:                     return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:              return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:     return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:        return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:            return "VK_ERROR_INVALID_SHADER_NV";
        default:                                    return "VK_UNKNOWN_ERROR";
    }
}

// ============================================================================
//  SECTION 7: KainComponentSurface vtable fill (~250 lines)
// ============================================================================

/* ── Session storage ─────────────────────────────────────────────── */

static KainVulkanSession g_sessions[KAIN_VULKAN_MAX_SESSIONS];
static int64_t           g_next_session_id = 1;

/* ── Forward declarations ────────────────────────────────────────── */

static int64_t vulkan_session_create(const char* name, int64_t width, int64_t height);
static void    vulkan_session_attach_platform(int64_t sid, void* platform_handle);
static void    vulkan_session_destroy(int64_t sid);
static int64_t vulkan_element_begin(int64_t sid, int64_t parent_id,
                                     const char* kind, const char* stable_key);
static void    vulkan_element_end(int64_t sid, int64_t element_id);
static void    vulkan_element_set_text(int64_t sid, int64_t element_id,
                                        const char* text);
static void    vulkan_element_set_attr_i64(int64_t sid, int64_t element_id,
                                            const char* key, int64_t value);
static void    vulkan_element_set_attr_f64(int64_t sid, int64_t element_id,
                                            const char* key, double value);
static void    vulkan_element_set_attr_string(int64_t sid, int64_t element_id,
                                               const char* key, const char* value);
static int64_t vulkan_state_get_i64(int64_t sid, const char* key);
static void    vulkan_state_set_i64(int64_t sid, const char* key, int64_t value);
static void    vulkan_begin_frame(int64_t sid, double delta_ms);
static void    vulkan_end_frame(int64_t sid);
static void    vulkan_present(int64_t sid);
static int64_t vulkan_poll_event(int64_t sid, void* out_event, int64_t max_size);
static int64_t vulkan_should_close(int64_t sid);
static int64_t vulkan_window_open(int64_t sid, const char* title,
                                   int64_t width, int64_t height);
static int64_t vulkan_host_pump(int64_t sid);

/* ── Helper: find session by ID ──────────────────────────────────── */

static KainVulkanSession* vulkan_find_session(int64_t sid) {
    if (sid < 1 || sid > KAIN_VULKAN_MAX_SESSIONS) return NULL;
    KainVulkanSession* s = &g_sessions[sid - 1];
    if (!s->initialized) return NULL;
    return s;
}

/* ── session_create ──────────────────────────────────────────────── */

static int64_t vulkan_session_create(const char* name, int64_t width, int64_t height) {
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < KAIN_VULKAN_MAX_SESSIONS; i++) {
        if (!g_sessions[i].initialized) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        vulkan_abi_set_error(-1, "vulkan: max sessions reached");
        return -1;
    }

    KainVulkanSession* s = &g_sessions[slot];
    memset(s, 0, sizeof(*s));
    s->session_id = g_next_session_id++;
    s->name       = name;
    s->width      = width;
    s->height     = height;
    s->initialized = 1;

    /* NOTE: Vulkan objects are created in session_attach_platform,
     *       after the platform app host provides the window handle. */
    return s->session_id;
}

/* ── session_attach_platform ─────────────────────────────────────── */

static void vulkan_session_attach_platform(int64_t sid, void* platform_handle) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) {
        vulkan_abi_set_error(-2, "vulkan: invalid session for attach_platform");
        return;
    }

    KainPlatformSurfaceHandle* handle = (KainPlatformSurfaceHandle*)platform_handle;
    if (!handle) {
        vulkan_abi_set_error(-3, "vulkan: null platform handle");
        return;
    }

    /* Store platform handles on the session */
#ifdef _WIN32
    s->hwnd      = handle->hwnd;
    s->hinstance = handle->hinstance;
#elif defined(__linux__)
    s->x11_display = handle->x11_display;
    s->x11_window  = handle->x11_window;
#elif defined(__APPLE__)
    s->x11_display = handle->metal_layer; /* reused field */
#endif

    /* ── Boot sequence: open loader if needed ── */
    if (!g_vulkan_loader_opened) {
        if (!vulkan_abi_open_loader()) {
            vulkan_abi_set_error(-4, "vulkan: failed to open vulkan loader library");
            return;
        }
    }

    /* ── Create instance ── */
    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType              = S_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "Kain";
    app_info.applicationVersion = VK_API_VERSION_1_0;
    app_info.pEngineName        = "Kain";
    app_info.engineVersion      = VK_API_VERSION_1_0;
    app_info.apiVersion         = VK_API_VERSION_1_3;

    const char* extensions[8];
    uint32_t ext_count = 0;

    /* Platform-specific surface extension */
#ifdef _WIN32
    extensions[ext_count++] = "VK_KHR_surface";
    extensions[ext_count++] = "VK_KHR_win32_surface";
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    extensions[ext_count++] = "VK_KHR_surface";
    extensions[ext_count++] = "VK_KHR_wayland_surface";
#elif defined(__linux__)
    extensions[ext_count++] = "VK_KHR_surface";
    extensions[ext_count++] = "VK_KHR_xlib_surface";
#elif defined(__APPLE__)
    extensions[ext_count++] = "VK_KHR_surface";
    extensions[ext_count++] = "VK_MVK_macos_surface";
#endif

    VkInstanceCreateInfo instance_info;
    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType                   = S_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo        = &app_info;
    instance_info.enabledExtensionCount   = ext_count;
    instance_info.ppEnabledExtensionNames = extensions;

    VkResult result = pfn_vkCreateInstance(&instance_info, NULL, &s->instance);
    if (result != VK_SUCCESS) {
        char msg[256];
        snprintf(msg, sizeof(msg), "vkCreateInstance failed: %s",
                 vulkan_abi_result_name(result));
        vulkan_abi_set_error(-5, msg);
        return;
    }

    /* Resolve instance-level PFNs */
    vulkan_abi_resolve_instance_pfns(s->instance);

    /* ── Create surface ── */
    result = vulkan_abi_create_surface(s);
    if (result != VK_SUCCESS) {
        char msg[256];
        snprintf(msg, sizeof(msg), "vkCreateSurface failed: %s",
                 vulkan_abi_result_name(result));
        vulkan_abi_set_error(-6, msg);
        return;
    }

    /* ── Select physical device ── */
    if (vulkan_abi_select_physical_device(s->instance, s->surface,
                                           &s->physical_device,
                                           &s->graphics_queue_family,
                                           &s->present_queue_family) != 0) {
        vulkan_abi_set_error(-7, "vulkan: no suitable physical device found");
        return;
    }

    /* ── Create logical device ── */
    if (vulkan_abi_create_device(s->physical_device,
                                  s->graphics_queue_family,
                                  s->present_queue_family,
                                  &s->device,
                                  &s->graphics_queue,
                                  &s->present_queue) != 0) {
        vulkan_abi_set_error(-8, "vulkan: vkCreateDevice failed");
        return;
    }

    /* Resolve device-level PFNs */
    vulkan_abi_resolve_device_pfns(s->device);

    /* ── Create swapchain ── */
    if (vulkan_abi_create_swapchain(s) != 0) {
        vulkan_abi_set_error(-9, "vulkan: swapchain creation failed");
        return;
    }

    /* ── Create sync objects ── */
    if (vulkan_abi_create_sync_objects(s) != 0) {
        vulkan_abi_set_error(-10, "vulkan: sync object creation failed");
        return;
    }

    /* Success */
    vulkan_abi_set_error(0, "vulkan: session_attach_platform success");
}

/* ── session_destroy ─────────────────────────────────────────────── */

static void vulkan_session_destroy(int64_t sid) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return;

    if (s->device != (VkDevice)0) {
        pfn_vkDeviceWaitIdle(s->device);

        /* Destroy framebuffers */
        for (uint32_t i = 0; i < s->swapchain_image_count; i++) {
            if (s->framebuffers[i] != (VkFramebuffer)0) {
                /* No destroy function in our subset — framebuffer
                 * destruction requires render pass context which
                 * we don't track in MVP. Let the driver clean up. */
                s->framebuffers[i] = (VkFramebuffer)0;
            }
        }

        /* Destroy image views */
        for (uint32_t i = 0; i < s->swapchain_image_count; i++) {
            if (s->swapchain_image_views[i] != (VkImageView)0) {
                pfn_vkDestroyImageView(s->device,
                                        s->swapchain_image_views[i], NULL);
                s->swapchain_image_views[i] = (VkImageView)0;
            }
        }

        /* Destroy sync objects */
        for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
            if (s->in_flight_fences[i] != (VkFence)0) {
                pfn_vkDestroyFence(s->device, s->in_flight_fences[i], NULL);
                s->in_flight_fences[i] = (VkFence)0;
            }
            if (s->render_finished[i] != (VkSemaphore)0) {
                pfn_vkDestroySemaphore(s->device, s->render_finished[i], NULL);
                s->render_finished[i] = (VkSemaphore)0;
            }
            if (s->image_available[i] != (VkSemaphore)0) {
                pfn_vkDestroySemaphore(s->device, s->image_available[i], NULL);
                s->image_available[i] = (VkSemaphore)0;
            }
        }

        /* Destroy command pool (also frees command buffers) */
        if (s->command_pool != (VkCommandPool)0) {
            pfn_vkDestroyCommandPool(s->device, s->command_pool, NULL);
            s->command_pool = (VkCommandPool)0;
        }

        /* Destroy swapchain */
        if (s->swapchain != (VkSwapchainKHR)0) {
            pfn_vkDestroySwapchainKHR(s->device, s->swapchain, NULL);
            s->swapchain = (VkSwapchainKHR)0;
        }

        /* Destroy device */
        pfn_vkDestroyDevice(s->device, NULL);
        s->device = (VkDevice)0;
    }

    /* Destroy surface */
    if (s->instance != (VkInstance)0 && s->surface != (VkSurfaceKHR)0) {
        pfn_vkDestroySurfaceKHR(s->instance, s->surface, NULL);
        s->surface = (VkSurfaceKHR)0;
    }

    /* Destroy instance */
    if (s->instance != (VkInstance)0) {
        pfn_vkDestroyInstance(s->instance, NULL);
        s->instance = (VkInstance)0;
    }

    s->initialized = 0;
    vulkan_abi_set_error(0, "vulkan: session destroyed");
}

/* ── element_begin / end — stubs for MVP ─────────────────────────── */

static int64_t vulkan_element_begin(int64_t sid, int64_t parent_id,
                                     const char* kind, const char* stable_key) {
    (void)sid; (void)parent_id; (void)kind; (void)stable_key;
    return 1; /* Return a synthetic element ID */
}

static void vulkan_element_end(int64_t sid, int64_t element_id) {
    (void)sid; (void)element_id;
}

static void vulkan_element_set_text(int64_t sid, int64_t element_id,
                                     const char* text) {
    (void)sid; (void)element_id; (void)text;
}

static void vulkan_element_set_attr_i64(int64_t sid, int64_t element_id,
                                         const char* key, int64_t value) {
    (void)sid; (void)element_id; (void)key; (void)value;
}

static void vulkan_element_set_attr_f64(int64_t sid, int64_t element_id,
                                         const char* key, double value) {
    (void)sid; (void)element_id; (void)key; (void)value;
}

static void vulkan_element_set_attr_string(int64_t sid, int64_t element_id,
                                            const char* key, const char* value) {
    (void)sid; (void)element_id; (void)key; (void)value;
}

/* ── state_get / set — stubs for MVP ─────────────────────────────── */

static int64_t vulkan_state_get_i64(int64_t sid, const char* key) {
    (void)sid; (void)key;
    return 0;
}

static void vulkan_state_set_i64(int64_t sid, const char* key, int64_t value) {
    (void)sid; (void)key; (void)value;
}

/* ── Frame lifecycle ─────────────────────────────────────────────── */

static void vulkan_begin_frame(int64_t sid, double delta_ms) {
    (void)delta_ms;
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return;

    int rc = vulkan_abi_begin_frame(s);
    if (rc == -1) {
        /* Swapchain recreation needed */
        vulkan_abi_recreate_swapchain(s);
        g_vulkan_abi_vtable.swapchain_recreations++;
        vulkan_abi_begin_frame(s);
    }
}

static void vulkan_end_frame(int64_t sid) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return;
    vulkan_abi_end_frame(s);
}

static void vulkan_present(int64_t sid) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return;

    int rc = vulkan_abi_present(s);
    if (rc == -2) {
        /* Swapchain recreation needed */
        vulkan_abi_recreate_swapchain(s);
        g_vulkan_abi_vtable.swapchain_recreations++;
    }
    if (rc >= 0 || rc == -2) {
        g_vulkan_abi_vtable.present_count++;
    }
}

/* ── Event pump / should_close ───────────────────────────────────── */

static int64_t vulkan_poll_event(int64_t sid, void* out_event, int64_t max_size) {
    (void)sid; (void)out_event; (void)max_size;
    /* MVP: no event parsing from platform */
    return 0;
}

static int64_t vulkan_should_close(int64_t sid) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return 1;
    return s->should_close;
}

/* ── window_open / host_pump ─────────────────────────────────────── */

static int64_t vulkan_window_open(int64_t sid, const char* title,
                                   int64_t width, int64_t height) {
    KainVulkanSession* s = vulkan_find_session(sid);
    if (!s) return -1;

    s->width  = width;
    s->height = height;

    /* The platform app host already created the window and provided the
     * handle via session_attach_platform. This is just a hint that the
     * window is now visible. */
    (void)title;
    return 0;
}

static int64_t vulkan_host_pump(int64_t sid) {
    (void)sid;
    /* MVP: platform events handled by the app host's own message loop */
    return 0;
}

// ============================================================================
//  SECTION 9: Static vtable instance (~40 lines)
// ============================================================================

KainVulkanAbiVtable g_vulkan_abi_vtable = {
    .surface = {
        .session_create          = vulkan_session_create,
        .session_destroy         = vulkan_session_destroy,
        .element_begin           = vulkan_element_begin,
        .element_end             = vulkan_element_end,
        .element_set_text        = vulkan_element_set_text,
        .element_set_attr_i64    = vulkan_element_set_attr_i64,
        .element_set_attr_f64    = vulkan_element_set_attr_f64,
        .element_set_attr_string = vulkan_element_set_attr_string,
        .state_get_i64           = vulkan_state_get_i64,
        .state_set_i64           = vulkan_state_set_i64,
        .begin_frame             = vulkan_begin_frame,
        .end_frame               = vulkan_end_frame,
        .present                 = vulkan_present,
        .poll_event              = vulkan_poll_event,
        .should_close            = vulkan_should_close,
        .window_open             = vulkan_window_open,
        .host_pump               = vulkan_host_pump,
        .session_attach_platform = vulkan_session_attach_platform,
    },
    .pfns = {0}, /* filled at first get_vtable() call or loader init */
    .abi_version           = KAIN_VULKAN_ABI_VERSION,
    .present_count         = 0,
    .swapchain_recreations = 0,
    .last_status           = 0,
    .last_error            = "",
};

/* Fill the PFN table from the resolved static PFN variables.
 * Called after loader + instance + device PFNs are resolved. */
static void vulkan_abi_fill_pfn_table(void) {
    KainVulkanPfnTable* p = &g_vulkan_abi_vtable.pfns;
    p->vkGetInstanceProcAddr                    = (KainPfn_vkGetInstanceProcAddr)pfn_vkGetInstanceProcAddr;
    p->vkGetDeviceProcAddr                      = (KainPfn_vkGetDeviceProcAddr)pfn_vkGetDeviceProcAddr;
    p->vkCreateInstance                         = (KainPfn_vkCreateInstance)pfn_vkCreateInstance;
    p->vkDestroyInstance                        = (KainPfn_vkDestroyInstance)pfn_vkDestroyInstance;
    p->vkEnumeratePhysicalDevices               = (KainPfn_vkEnumeratePhysicalDevices)pfn_vkEnumeratePhysicalDevices;
    p->vkGetPhysicalDeviceQueueFamilyProperties = (KainPfn_vkGetPhysicalDeviceQueueFamilyProperties)pfn_vkGetPhysicalDeviceQueueFamilyProperties;
    p->vkGetPhysicalDeviceSurfaceSupportKHR     = (KainPfn_vkGetPhysicalDeviceSurfaceSupportKHR)pfn_vkGetPhysicalDeviceSurfaceSupportKHR;
    p->vkCreateDevice                           = (KainPfn_vkCreateDevice)pfn_vkCreateDevice;
    p->vkDestroyDevice                          = (KainPfn_vkDestroyDevice)pfn_vkDestroyDevice;
    p->vkGetDeviceQueue                         = (KainPfn_vkGetDeviceQueue)pfn_vkGetDeviceQueue;
    p->vkDeviceWaitIdle                         = (KainPfn_vkDeviceWaitIdle)pfn_vkDeviceWaitIdle;
    p->vkCreateWin32SurfaceKHR                  = (KainPfn_vkCreateWin32SurfaceKHR)pfn_vkCreateWin32SurfaceKHR;
    p->vkDestroySurfaceKHR                      = (KainPfn_vkDestroySurfaceKHR)pfn_vkDestroySurfaceKHR;
    p->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (KainPfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    p->vkGetPhysicalDeviceSurfaceFormatsKHR     = (KainPfn_vkGetPhysicalDeviceSurfaceFormatsKHR)pfn_vkGetPhysicalDeviceSurfaceFormatsKHR;
    p->vkGetPhysicalDeviceSurfacePresentModesKHR = (KainPfn_vkGetPhysicalDeviceSurfacePresentModesKHR)pfn_vkGetPhysicalDeviceSurfacePresentModesKHR;
    p->vkCreateSwapchainKHR                     = (KainPfn_vkCreateSwapchainKHR)pfn_vkCreateSwapchainKHR;
    p->vkDestroySwapchainKHR                    = (KainPfn_vkDestroySwapchainKHR)pfn_vkDestroySwapchainKHR;
    p->vkGetSwapchainImagesKHR                  = (KainPfn_vkGetSwapchainImagesKHR)pfn_vkGetSwapchainImagesKHR;
    p->vkAcquireNextImageKHR                    = (KainPfn_vkAcquireNextImageKHR)pfn_vkAcquireNextImageKHR;
    p->vkQueuePresentKHR                        = (KainPfn_vkQueuePresentKHR)pfn_vkQueuePresentKHR;
    p->vkCreateCommandPool                      = (KainPfn_vkCreateCommandPool)pfn_vkCreateCommandPool;
    p->vkDestroyCommandPool                     = (KainPfn_vkDestroyCommandPool)pfn_vkDestroyCommandPool;
    p->vkAllocateCommandBuffers                 = (KainPfn_vkAllocateCommandBuffers)pfn_vkAllocateCommandBuffers;
    p->vkResetCommandBuffer                     = (KainPfn_vkResetCommandBuffer)pfn_vkResetCommandBuffer;
    p->vkBeginCommandBuffer                     = (KainPfn_vkBeginCommandBuffer)pfn_vkBeginCommandBuffer;
    p->vkEndCommandBuffer                       = (KainPfn_vkEndCommandBuffer)pfn_vkEndCommandBuffer;
    p->vkQueueSubmit                            = (KainPfn_vkQueueSubmit)pfn_vkQueueSubmit;
    p->vkCreateSemaphore                        = (KainPfn_vkCreateSemaphore)pfn_vkCreateSemaphore;
    p->vkDestroySemaphore                       = (KainPfn_vkDestroySemaphore)pfn_vkDestroySemaphore;
    p->vkCreateFence                            = (KainPfn_vkCreateFence)pfn_vkCreateFence;
    p->vkDestroyFence                           = (KainPfn_vkDestroyFence)pfn_vkDestroyFence;
    p->vkWaitForFences                          = (KainPfn_vkWaitForFences)pfn_vkWaitForFences;
    p->vkResetFences                            = (KainPfn_vkResetFences)pfn_vkResetFences;
    p->vkCreateImageView                        = (KainPfn_vkCreateImageView)pfn_vkCreateImageView;
    p->vkDestroyImageView                       = (KainPfn_vkDestroyImageView)pfn_vkDestroyImageView;
    p->vkCreateRenderPass                       = (KainPfn_vkCreateRenderPass)pfn_vkCreateRenderPass;
    p->vkDestroyRenderPass                      = (KainPfn_vkDestroyRenderPass)pfn_vkDestroyRenderPass;
    p->vkCreateShaderModule                     = (KainPfn_vkCreateShaderModule)pfn_vkCreateShaderModule;
    p->vkDestroyShaderModule                    = (KainPfn_vkDestroyShaderModule)pfn_vkDestroyShaderModule;
    p->vkCreatePipelineLayout                   = (KainPfn_vkCreatePipelineLayout)pfn_vkCreatePipelineLayout;
    p->vkDestroyPipelineLayout                  = (KainPfn_vkDestroyPipelineLayout)pfn_vkDestroyPipelineLayout;
    p->vkCreateGraphicsPipelines                = (KainPfn_vkCreateGraphicsPipelines)pfn_vkCreateGraphicsPipelines;
    p->vkDestroyPipeline                        = (KainPfn_vkDestroyPipeline)pfn_vkDestroyPipeline;
    p->vkCreateFramebuffer                      = (KainPfn_vkCreateFramebuffer)pfn_vkCreateFramebuffer;
    p->vkDestroyFramebuffer                     = (KainPfn_vkDestroyFramebuffer)pfn_vkDestroyFramebuffer;
    p->vkCmdBeginRenderPass                     = (KainPfn_vkCmdBeginRenderPass)pfn_vkCmdBeginRenderPass;
    p->vkCmdEndRenderPass                       = (KainPfn_vkCmdEndRenderPass)pfn_vkCmdEndRenderPass;
    p->vkCmdBindPipeline                        = (KainPfn_vkCmdBindPipeline)pfn_vkCmdBindPipeline;
    p->vkCmdPushConstants                       = (KainPfn_vkCmdPushConstants)pfn_vkCmdPushConstants;
    p->vkCmdDraw                                = (KainPfn_vkCmdDraw)pfn_vkCmdDraw;
}

// ── Public entry points ────────────────────────────────────────────

KAIN_VULKAN_ABI_EXPORT const KainVulkanAbiVtable* kain_vulkan_abi_get_vtable(void) {
    return &g_vulkan_abi_vtable;
}

KAIN_VULKAN_ABI_EXPORT int kain_vulkan_abi_init(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    vulkan_abi_set_error(0, "vulkan_abi_init");
    return 0;
}

KAIN_VULKAN_ABI_EXPORT void kain_vulkan_abi_shutdown(void) {
    /* Destroy all active sessions */
    for (int i = 0; i < KAIN_VULKAN_MAX_SESSIONS; i++) {
        if (g_sessions[i].initialized) {
            vulkan_session_destroy(g_sessions[i].session_id);
        }
    }
    vulkan_abi_close_loader();
    vulkan_abi_set_error(0, "vulkan_abi_shutdown");
}
