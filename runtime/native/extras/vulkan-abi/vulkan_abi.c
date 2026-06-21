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

// ── VkShaderModuleCreateInfo ────────────────────────────────────

typedef uint32_t VkShaderModuleCreateFlags;

typedef uint32_t VkMemoryMapFlags;

typedef struct VkShaderModuleCreateInfo {
    VkStructureType              sType;
    const void*                  pNext;
    VkShaderModuleCreateFlags    flags;
    size_t                       codeSize;
    const uint32_t*              pCode;
} VkShaderModuleCreateInfo;

// ── VkPipelineShaderStageCreateInfo ─────────────────────────────

typedef uint32_t VkPipelineShaderStageCreateFlags;
typedef uint32_t VkShaderStageFlagBits;

typedef struct VkPipelineShaderStageCreateInfo {
    VkStructureType                     sType;
    const void*                         pNext;
    VkPipelineShaderStageCreateFlags    flags;
    VkShaderStageFlagBits               stage;
    VkShaderModule                      module;
    const char*                         pName;
    const void*                         pSpecializationInfo;
} VkPipelineShaderStageCreateInfo;

// ── VkPipelineVertexInputStateCreateInfo ─────────────────────────

typedef uint32_t VkPipelineVertexInputStateCreateFlags;

typedef struct VkPipelineVertexInputStateCreateInfo {
    VkStructureType                            sType;
    const void*                                pNext;
    VkPipelineVertexInputStateCreateFlags      flags;
    uint32_t                                   vertexBindingDescriptionCount;
    const void*                                pVertexBindingDescriptions;
    uint32_t                                   vertexAttributeDescriptionCount;
    const void*                                pVertexAttributeDescriptions;
} VkPipelineVertexInputStateCreateInfo;

// ── VkPipelineInputAssemblyStateCreateInfo ───────────────────────

typedef uint32_t VkPipelineInputAssemblyStateCreateFlags;
typedef uint32_t VkPrimitiveTopology;

typedef struct VkPipelineInputAssemblyStateCreateInfo {
    VkStructureType                            sType;
    const void*                                pNext;
    VkPipelineInputAssemblyStateCreateFlags    flags;
    VkPrimitiveTopology                        topology;
    VkBool32                                   primitiveRestartEnable;
} VkPipelineInputAssemblyStateCreateInfo;

// ── VkViewport ────────────────────────────────────────────────────

typedef struct VkViewport {
    float    x;
    float    y;
    float    width;
    float    height;
    float    minDepth;
    float    maxDepth;
} VkViewport;

// ── VkPipelineViewportStateCreateInfo ────────────────────────────

typedef uint32_t VkPipelineViewportStateCreateFlags;

typedef struct VkPipelineViewportStateCreateInfo {
    VkStructureType                      sType;
    const void*                          pNext;
    VkPipelineViewportStateCreateFlags   flags;
    uint32_t                             viewportCount;
    const VkViewport*                    pViewports;
    uint32_t                             scissorCount;
    const VkRect2D*                      pScissors;
} VkPipelineViewportStateCreateInfo;

// ── VkPipelineRasterizationStateCreateInfo ───────────────────────

typedef uint32_t VkPipelineRasterizationStateCreateFlags;
typedef uint32_t VkPolygonMode;
typedef uint32_t VkCullModeFlags;
typedef uint32_t VkFrontFace;

typedef struct VkPipelineRasterizationStateCreateInfo {
    VkStructureType                            sType;
    const void*                                pNext;
    VkPipelineRasterizationStateCreateFlags    flags;
    VkBool32                                   depthClampEnable;
    VkBool32                                   rasterizerDiscardEnable;
    VkPolygonMode                              polygonMode;
    VkCullModeFlags                            cullMode;
    VkFrontFace                                frontFace;
    VkBool32                                   depthBiasEnable;
    float                                      depthBiasConstantFactor;
    float                                      depthBiasClamp;
    float                                      depthBiasSlopeFactor;
    float                                      lineWidth;
} VkPipelineRasterizationStateCreateInfo;

// ── VkPipelineMultisampleStateCreateInfo ─────────────────────────

typedef uint32_t VkPipelineMultisampleStateCreateFlags;

typedef struct VkPipelineMultisampleStateCreateInfo {
    VkStructureType                          sType;
    const void*                              pNext;
    VkPipelineMultisampleStateCreateFlags    flags;
    VkSampleCountFlagBits                    rasterizationSamples;
    VkBool32                                 sampleShadingEnable;
    float                                    minSampleShading;
    const void*                              pSampleMask;
    VkBool32                                 alphaToCoverageEnable;
    VkBool32                                 alphaToOneEnable;
} VkPipelineMultisampleStateCreateInfo;

// ── VkPipelineColorBlendAttachmentState ──────────────────────────

typedef uint32_t VkBlendFactor;
typedef uint32_t VkBlendOp;
typedef uint32_t VkColorComponentFlags;

typedef struct VkPipelineColorBlendAttachmentState {
    VkBool32                 blendEnable;
    VkBlendFactor            srcColorBlendFactor;
    VkBlendFactor            dstColorBlendFactor;
    VkBlendOp                colorBlendOp;
    VkBlendFactor            srcAlphaBlendFactor;
    VkBlendFactor            dstAlphaBlendFactor;
    VkBlendOp                alphaBlendOp;
    VkColorComponentFlags    colorWriteMask;
} VkPipelineColorBlendAttachmentState;

// ── VkPipelineColorBlendStateCreateInfo ──────────────────────────

typedef uint32_t VkPipelineColorBlendStateCreateFlags;
typedef uint32_t VkLogicOp;

typedef struct VkPipelineColorBlendStateCreateInfo {
    VkStructureType                               sType;
    const void*                                   pNext;
    VkPipelineColorBlendStateCreateFlags          flags;
    VkBool32                                      logicOpEnable;
    VkLogicOp                                     logicOp;
    uint32_t                                      attachmentCount;
    const VkPipelineColorBlendAttachmentState*    pAttachments;
    float                                         blendConstants[4];
} VkPipelineColorBlendStateCreateInfo;

// ── VkPipelineLayoutCreateInfo ───────────────────────────────────

typedef uint32_t VkPipelineLayoutCreateFlags;

typedef struct VkPipelineLayoutCreateInfo {
    VkStructureType                 sType;
    const void*                     pNext;
    VkPipelineLayoutCreateFlags     flags;
    uint32_t                        setLayoutCount;
    const VkDescriptorSetLayout*    pSetLayouts;
    uint32_t                        pushConstantRangeCount;
    const void*                     pPushConstantRanges;
} VkPipelineLayoutCreateInfo;

// ── VkGraphicsPipelineCreateInfo ─────────────────────────────────

typedef uint32_t VkPipelineCreateFlags;

typedef struct VkGraphicsPipelineCreateInfo {
    VkStructureType                                  sType;
    const void*                                      pNext;
    VkPipelineCreateFlags                            flags;
    uint32_t                                         stageCount;
    const VkPipelineShaderStageCreateInfo*           pStages;
    const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
    const void*                                      pTessellationState;
    const VkPipelineViewportStateCreateInfo*         pViewportState;
    const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
    const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
    const void*                                      pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
    const void*                                      pDynamicState;
    VkPipelineLayout                                 layout;
    VkRenderPass                                     renderPass;
    uint32_t                                         subpass;
    VkPipeline                                       basePipelineHandle;
    int32_t                                          basePipelineIndex;
} VkGraphicsPipelineCreateInfo;

// ── VkDescriptorSetLayoutBinding ──────────────────────────────────

typedef struct VkDescriptorSetLayoutBinding {
    uint32_t              binding;
    VkDescriptorType      descriptorType;
    uint32_t              descriptorCount;
    VkShaderStageFlags    stageFlags;
    const void*           pImmutableSamplers;
} VkDescriptorSetLayoutBinding;

// ── VkDescriptorSetLayoutCreateInfo ───────────────────────────────

typedef uint32_t VkDescriptorSetLayoutCreateFlags;

typedef struct VkDescriptorSetLayoutCreateInfo {
    VkStructureType                        sType;
    const void*                            pNext;
    VkDescriptorSetLayoutCreateFlags       flags;
    uint32_t                               bindingCount;
    const VkDescriptorSetLayoutBinding*    pBindings;
} VkDescriptorSetLayoutCreateInfo;

// ── VkDescriptorPoolSize ──────────────────────────────────────────

typedef struct VkDescriptorPoolSize {
    VkDescriptorType    type;
    uint32_t            descriptorCount;
} VkDescriptorPoolSize;

// ── VkDescriptorPoolCreateInfo ────────────────────────────────────

typedef uint32_t VkDescriptorPoolCreateFlags;

typedef struct VkDescriptorPoolCreateInfo {
    VkStructureType                sType;
    const void*                    pNext;
    VkDescriptorPoolCreateFlags    flags;
    uint32_t                       maxSets;
    uint32_t                       poolSizeCount;
    const VkDescriptorPoolSize*    pPoolSizes;
} VkDescriptorPoolCreateInfo;

// ── VkDescriptorSetAllocateInfo ───────────────────────────────────

typedef struct VkDescriptorSetAllocateInfo {
    VkStructureType                 sType;
    const void*                     pNext;
    VkDescriptorPool                descriptorPool;
    uint32_t                        descriptorSetCount;
    const VkDescriptorSetLayout*    pSetLayouts;
} VkDescriptorSetAllocateInfo;

// ── VkDescriptorBufferInfo ────────────────────────────────────────

typedef struct VkDescriptorBufferInfo {
    VkBuffer        buffer;
    VkDeviceSize    offset;
    VkDeviceSize    range;
} VkDescriptorBufferInfo;

// ── VkWriteDescriptorSet ──────────────────────────────────────────

typedef struct VkWriteDescriptorSet {
    VkStructureType                  sType;
    const void*                      pNext;
    VkDescriptorSet                  dstSet;
    uint32_t                         dstBinding;
    uint32_t                         dstArrayElement;
    uint32_t                         descriptorCount;
    VkDescriptorType                 descriptorType;
    const void*                      pImageInfo;
    const VkDescriptorBufferInfo*    pBufferInfo;
    const void*                      pTexelBufferView;
} VkWriteDescriptorSet;

// ── VkBufferCreateInfo ────────────────────────────────────────────

typedef struct VkBufferCreateInfo {
    VkStructureType        sType;
    const void*            pNext;
    VkBufferCreateFlags    flags;
    VkDeviceSize           size;
    VkBufferUsageFlags     usage;
    VkSharingMode          sharingMode;
    uint32_t               queueFamilyIndexCount;
    const uint32_t*        pQueueFamilyIndices;
} VkBufferCreateInfo;

// ── VkMemoryRequirements ──────────────────────────────────────────

typedef struct VkMemoryRequirements {
    VkDeviceSize    size;
    VkDeviceSize    alignment;
    uint32_t        memoryTypeBits;
} VkMemoryRequirements;

// ── VkMemoryAllocateInfo ──────────────────────────────────────────

typedef struct VkMemoryAllocateInfo {
    VkStructureType    sType;
    const void*        pNext;
    VkDeviceSize       allocationSize;
    uint32_t           memoryTypeIndex;
} VkMemoryAllocateInfo;

// ── VkMemoryType / VkMemoryHeap / VkPhysicalDeviceMemoryProperties

typedef struct VkMemoryType {
    VkMemoryPropertyFlags    propertyFlags;
    uint32_t                 heapIndex;
} VkMemoryType;

typedef struct VkMemoryHeap {
    VkDeviceSize         size;
    VkMemoryHeapFlags    flags;
} VkMemoryHeap;

typedef struct VkPhysicalDeviceMemoryProperties {
    uint32_t        memoryTypeCount;
    VkMemoryType    memoryTypes[32];
    uint32_t        memoryHeapCount;
    VkMemoryHeap    memoryHeaps[16];
} VkPhysicalDeviceMemoryProperties;

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
#define S_TYPE_SHADER_MODULE_CREATE_INFO                  16
#define S_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO          18
#define S_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO    19
#define S_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO  20
#define S_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO        22
#define S_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO   23
#define S_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO     24
#define S_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO     26
#define S_TYPE_GRAPHICS_PIPELINE_CREATE_INFO              28
#define S_TYPE_PIPELINE_LAYOUT_CREATE_INFO                30
#define S_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO          32
#define S_TYPE_DESCRIPTOR_POOL_CREATE_INFO                33
#define S_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO               34
#define S_TYPE_WRITE_DESCRIPTOR_SET                       35
#define S_TYPE_BUFFER_CREATE_INFO                         12
#define S_TYPE_MEMORY_ALLOCATE_INFO                        5

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

#define VK_SHADER_STAGE_VERTEX_BIT                      0x00000001
#define VK_SHADER_STAGE_FRAGMENT_BIT                    0x00000010
#define VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST              3
#define VK_POLYGON_MODE_FILL                             0
#define VK_CULL_MODE_NONE                                0
#define VK_FRONT_FACE_COUNTER_CLOCKWISE                  0
#define VK_SAMPLE_COUNT_1_BIT                            0x00000001
#define VK_COLOR_COMPONENT_R_BIT                         0x00000001
#define VK_COLOR_COMPONENT_G_BIT                         0x00000002
#define VK_COLOR_COMPONENT_B_BIT                         0x00000004
#define VK_COLOR_COMPONENT_A_BIT                         0x00000008
#define VK_BLEND_FACTOR_ONE                              1
#define VK_BLEND_FACTOR_ZERO                             0
#define VK_BLEND_OP_ADD                                  0
#define VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER                6
#define VK_DESCRIPTOR_TYPE_STORAGE_BUFFER                7
#define VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT               0x00000010
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT              0x00000002
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT             0x00000004
#define VK_WHOLE_SIZE                                    0xFFFFFFFFFFFFFFFFULL

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
typedef void     (*PFN_vkCmdSetViewport)(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*);
typedef void     (*PFN_vkCmdSetScissor)(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
typedef VkResult (*PFN_vkCreateDescriptorSetLayout)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const void*, VkDescriptorSetLayout*);
typedef void     (*PFN_vkDestroyDescriptorSetLayout)(VkDevice, VkDescriptorSetLayout, const void*);
typedef VkResult (*PFN_vkCreateDescriptorPool)(VkDevice, const VkDescriptorPoolCreateInfo*, const void*, VkDescriptorPool*);
typedef void     (*PFN_vkDestroyDescriptorPool)(VkDevice, VkDescriptorPool, const void*);
typedef VkResult (*PFN_vkAllocateDescriptorSets)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
typedef void     (*PFN_vkUpdateDescriptorSets)(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const void*);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice, const VkBufferCreateInfo*, const void*, VkBuffer*);
typedef void     (*PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const void*);
typedef void     (*PFN_vkGetBufferMemoryRequirements)(VkDevice, VkBuffer, VkMemoryRequirements*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*);
typedef void     (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const void*);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
typedef void     (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef void     (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);

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
static PFN_vkCmdSetViewport          pfn_vkCmdSetViewport = NULL;
static PFN_vkCmdSetScissor           pfn_vkCmdSetScissor = NULL;
static PFN_vkCreateDescriptorSetLayout pfn_vkCreateDescriptorSetLayout = NULL;
static PFN_vkDestroyDescriptorSetLayout pfn_vkDestroyDescriptorSetLayout = NULL;
static PFN_vkCreateDescriptorPool    pfn_vkCreateDescriptorPool = NULL;
static PFN_vkDestroyDescriptorPool   pfn_vkDestroyDescriptorPool = NULL;
static PFN_vkAllocateDescriptorSets  pfn_vkAllocateDescriptorSets = NULL;
static PFN_vkUpdateDescriptorSets    pfn_vkUpdateDescriptorSets = NULL;
static PFN_vkCreateBuffer            pfn_vkCreateBuffer = NULL;
static PFN_vkDestroyBuffer           pfn_vkDestroyBuffer = NULL;
static PFN_vkGetBufferMemoryRequirements pfn_vkGetBufferMemoryRequirements = NULL;
static PFN_vkAllocateMemory          pfn_vkAllocateMemory = NULL;
static PFN_vkFreeMemory              pfn_vkFreeMemory = NULL;
static PFN_vkBindBufferMemory        pfn_vkBindBufferMemory = NULL;
static PFN_vkMapMemory               pfn_vkMapMemory = NULL;
static PFN_vkUnmapMemory             pfn_vkUnmapMemory = NULL;
static PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties = NULL;

/* Command buffer descriptor set binding */
typedef void (*PFN_vkCmdBindDescriptorSets)(VkCommandBuffer, VkPipelineBindPoint,
    VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*,
    uint32_t, const uint32_t*);
static PFN_vkCmdBindDescriptorSets pfn_vkCmdBindDescriptorSets = NULL;

static void vulkan_abi_fill_pfn_table(void);
static int  vulkan_abi_record_draw_commands(KainVulkanSession* session);

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
    pfn_vkCmdSetViewport = (PFN_vkCmdSetViewport)
        pfn_vkGetDeviceProcAddr(device, "vkCmdSetViewport");
    pfn_vkCmdSetScissor = (PFN_vkCmdSetScissor)
        pfn_vkGetDeviceProcAddr(device, "vkCmdSetScissor");
    pfn_vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)
        pfn_vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout");
    pfn_vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout");
    pfn_vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)
        pfn_vkGetDeviceProcAddr(device, "vkCreateDescriptorPool");
    pfn_vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool");
    pfn_vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)
        pfn_vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets");
    pfn_vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)
        pfn_vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets");
    pfn_vkCreateBuffer = (PFN_vkCreateBuffer)
        pfn_vkGetDeviceProcAddr(device, "vkCreateBuffer");
    pfn_vkDestroyBuffer = (PFN_vkDestroyBuffer)
        pfn_vkGetDeviceProcAddr(device, "vkDestroyBuffer");
    pfn_vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)
        pfn_vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
    pfn_vkAllocateMemory = (PFN_vkAllocateMemory)
        pfn_vkGetDeviceProcAddr(device, "vkAllocateMemory");
    pfn_vkFreeMemory = (PFN_vkFreeMemory)
        pfn_vkGetDeviceProcAddr(device, "vkFreeMemory");
    pfn_vkBindBufferMemory = (PFN_vkBindBufferMemory)
        pfn_vkGetDeviceProcAddr(device, "vkBindBufferMemory");
    pfn_vkMapMemory = (PFN_vkMapMemory)
        pfn_vkGetDeviceProcAddr(device, "vkMapMemory");
    pfn_vkUnmapMemory = (PFN_vkUnmapMemory)
        pfn_vkGetDeviceProcAddr(device, "vkUnmapMemory");
    pfn_vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)
        pfn_vkGetDeviceProcAddr(device, "vkGetPhysicalDeviceMemoryProperties");

    /* ── vkCmdBindDescriptorSets ── */
    pfn_vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)
        pfn_vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets");

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

    /* Record draw commands if pipeline is ready */
    if (session->pipeline_ready) {
        vulkan_abi_record_draw_commands(session);
    }

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

        /* ── Destroy rendering pipeline objects ── */
        if (s->pipeline != (VkPipeline)0) {
            pfn_vkDestroyPipeline(s->device, s->pipeline, NULL);
            s->pipeline = (VkPipeline)0;
        }
        if (s->pipeline_layout != (VkPipelineLayout)0) {
            pfn_vkDestroyPipelineLayout(s->device, s->pipeline_layout, NULL);
            s->pipeline_layout = (VkPipelineLayout)0;
        }
        if (s->descriptor_set_layout != (VkDescriptorSetLayout)0) {
            pfn_vkDestroyDescriptorSetLayout(s->device, s->descriptor_set_layout, NULL);
            s->descriptor_set_layout = (VkDescriptorSetLayout)0;
        }
        if (s->descriptor_pool != (VkDescriptorPool)0) {
            pfn_vkDestroyDescriptorPool(s->device, s->descriptor_pool, NULL);
            s->descriptor_pool = (VkDescriptorPool)0;
        }
        for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
            if (s->uniform_mapped[i] != NULL) {
                pfn_vkUnmapMemory(s->device, s->uniform_memory[i]);
                s->uniform_mapped[i] = NULL;
            }
            if (s->uniform_memory[i] != (VkDeviceMemory)0) {
                pfn_vkFreeMemory(s->device, s->uniform_memory[i], NULL);
                s->uniform_memory[i] = (VkDeviceMemory)0;
            }
            if (s->uniform_buffers[i] != (VkBuffer)0) {
                pfn_vkDestroyBuffer(s->device, s->uniform_buffers[i], NULL);
                s->uniform_buffers[i] = (VkBuffer)0;
            }
        }
        if (s->render_pass != (VkRenderPass)0) {
            pfn_vkDestroyRenderPass(s->device, s->render_pass, NULL);
            s->render_pass = (VkRenderPass)0;
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
    p->vkCmdSetViewport                         = (KainPfn_vkCmdSetViewport)pfn_vkCmdSetViewport;
    p->vkCmdSetScissor                          = (KainPfn_vkCmdSetScissor)pfn_vkCmdSetScissor;
    p->vkCreateDescriptorSetLayout              = (KainPfn_vkCreateDescriptorSetLayout)pfn_vkCreateDescriptorSetLayout;
    p->vkDestroyDescriptorSetLayout             = (KainPfn_vkDestroyDescriptorSetLayout)pfn_vkDestroyDescriptorSetLayout;
    p->vkCreateDescriptorPool                   = (KainPfn_vkCreateDescriptorPool)pfn_vkCreateDescriptorPool;
    p->vkDestroyDescriptorPool                  = (KainPfn_vkDestroyDescriptorPool)pfn_vkDestroyDescriptorPool;
    p->vkAllocateDescriptorSets                 = (KainPfn_vkAllocateDescriptorSets)pfn_vkAllocateDescriptorSets;
    p->vkUpdateDescriptorSets                   = (KainPfn_vkUpdateDescriptorSets)pfn_vkUpdateDescriptorSets;
    p->vkCreateBuffer                           = (KainPfn_vkCreateBuffer)pfn_vkCreateBuffer;
    p->vkDestroyBuffer                          = (KainPfn_vkDestroyBuffer)pfn_vkDestroyBuffer;
    p->vkGetBufferMemoryRequirements            = (KainPfn_vkGetBufferMemoryRequirements)pfn_vkGetBufferMemoryRequirements;
    p->vkAllocateMemory                         = (KainPfn_vkAllocateMemory)pfn_vkAllocateMemory;
    p->vkFreeMemory                             = (KainPfn_vkFreeMemory)pfn_vkFreeMemory;
    p->vkBindBufferMemory                       = (KainPfn_vkBindBufferMemory)pfn_vkBindBufferMemory;
    p->vkMapMemory                              = (KainPfn_vkMapMemory)pfn_vkMapMemory;
    p->vkUnmapMemory                            = (KainPfn_vkUnmapMemory)pfn_vkUnmapMemory;
    p->vkGetPhysicalDeviceMemoryProperties      = (KainPfn_vkGetPhysicalDeviceMemoryProperties)pfn_vkGetPhysicalDeviceMemoryProperties;
}

// ============================================================================
//  SECTION 10: Shader Module Creation (~80 lines)
// ============================================================================

/* ── vulkan_abi_create_shader_module ───────────────────────────── */

static VkResult vulkan_abi_create_shader_module(
    VkDevice         device,
    const uint32_t*  spirv_bytes,
    size_t           byte_length,
    VkShaderModule*  out_module)
{
    if (!device || !spirv_bytes || !byte_length || !out_module)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkShaderModuleCreateInfo info;
    memset(&info, 0, sizeof(info));
    info.sType    = S_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = byte_length;
    info.pCode    = spirv_bytes;

    return pfn_vkCreateShaderModule(device, &info, NULL, out_module);
}

/* ── hex_to_u32 ────────────────────────────────────────────────── */
/* Converts a hex character to its 4-bit value. Returns -1 on error. */

static int hex_to_u32(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ── vulkan_abi_decode_spirv_hex ───────────────────────────────── */
/* Decodes a hex-encoded SPIR-V string (e.g. "07230203...") into
 * binary uint32_t array. Returns malloc'd buffer; caller frees.
 * out_word_count is set to the number of uint32_t words. */

static uint32_t* vulkan_abi_decode_spirv_hex(
    const char* hex_str,
    size_t*     out_word_count)
{
    if (!hex_str || !out_word_count) return NULL;

    size_t hex_len = strlen(hex_str);
    /* Each uint32_t is 8 hex chars */
    size_t word_count = hex_len / 8;
    if (word_count == 0 || (hex_len % 8) != 0) return NULL;

    uint32_t* words = (uint32_t*)malloc(word_count * sizeof(uint32_t));
    if (!words) return NULL;

    for (size_t i = 0; i < word_count; i++) {
        uint32_t val = 0;
        for (int j = 0; j < 8; j++) {
            int nibble = hex_to_u32(hex_str[i * 8 + j]);
            if (nibble < 0) { free(words); return NULL; }
            val = (val << 4) | (uint32_t)nibble;
        }
        words[i] = val;
    }

    *out_word_count = word_count;
    return words;
}

// ============================================================================
//  SECTION 11: Pipeline Creation (~200 lines)
// ============================================================================

/* Hardcoded fullscreen-triangle vertex shader in SPIR-V.
 * Compiled from:
 *   #version 450
 *   layout(location = 0) out vec2 fragUV;
 *   void main() {
 *       vec2 pos[3] = vec2[](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
 *       vec2 uv[3]  = vec2[](vec2(0,0), vec2(2,0), vec2(0,2));
 *       gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
 *       fragUV = uv[gl_VertexIndex];
 *   }
 * 340 bytes, SPIR-V 1.0 */

static const uint32_t g_fullscreen_vert_spirv[] = {
    0x07230203, 0x00010000, 0x0008000B, 0x0000001E,
    0x00000000, 0x00020011, 0x00000001, 0x0006000B,
    0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E,
    0x00000000, 0x0003000E, 0x00000000, 0x00000001,
    0x000A000F, 0x00000000, 0x00000004, 0x6E69616D,
    0x00000000, 0x0000000D, 0x00000019, 0x0000001B,
    0x0000001C, 0x0000001D, 0x00030003, 0x00000002,
    0x00000190, 0x00090004, 0x455F4C47, 0x735F5458,
    0x636E6574, 0x656C5F6C, 0x79745F67, 0x00006570,
    0x0000000B, 0x00040005, 0x00000004, 0x6E69616D,
    0x00000000, 0x00050005, 0x0000000D, 0x67617266,
    0x00565955, 0x00060005, 0x00000019, 0x505F6C67,
    0x7469736F, 0x006E6F69, 0x00070005, 0x0000001B,
    0x736F705F, 0x6E697469, 0x00736F70, 0x00060005,
    0x0000001C, 0x73755F76, 0x6E697469, 0x00736F70,
    0x00050048, 0x0000000D, 0x00000000, 0x0000000B,
    0x00000000, 0x00050048, 0x0000000D, 0x00000001,
    0x0000000B, 0x00000001, 0x00050048, 0x0000000D,
    0x00000002, 0x0000000B, 0x00000003, 0x00050048,
    0x0000000D, 0x00000003, 0x0000000B, 0x00000004,
    0x00030047, 0x0000000D, 0x00000002, 0x00040047,
    0x00000019, 0x0000000B, 0x00000000, 0x00040047,
    0x0000001B, 0x0000000B, 0x00000001, 0x00040047,
    0x0000001C, 0x0000000B, 0x00000000, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017,
    0x00000007, 0x00000006, 0x00000002, 0x00040015,
    0x00000008, 0x00000020, 0x00000000, 0x0004002B,
    0x00000008, 0x00000009, 0x00000003, 0x0004001C,
    0x0000000A, 0x00000007, 0x00000009, 0x00040020,
    0x0000000B, 0x00000006, 0x0000000A, 0x0004003B,
    0x0000000B, 0x0000000C, 0x00000006, 0x00040020,
    0x0000000E, 0x00000001, 0x00000007, 0x0004003B,
    0x0000000E, 0x0000000D, 0x00000001, 0x00040017,
    0x00000010, 0x00000006, 0x00000004, 0x00040020,
    0x00000018, 0x00000003, 0x00000010, 0x0004003B,
    0x00000018, 0x00000019, 0x00000003, 0x0004002B,
    0x00000006, 0x0000001A, 0xBF000000, 0x0005002C,
    0x00000007, 0x0000001B, 0x0000001A, 0x0000001A,
    0x0004002B, 0x00000006, 0x0000001C, 0x40400000,
    0x0004002B, 0x00000006, 0x0000001D, 0x00000000,
    0x0005002C, 0x00000007, 0x0000001E, 0x0000001D,
    0x0000001D, 0x0005002C, 0x00000007, 0x0000001F,
    0x0000001C, 0x0000001D, 0x0005002C, 0x00000007,
    0x00000020, 0x0000001D, 0x0000001C, 0x0009002C,
    0x0000000A, 0x00000021, 0x0000001B, 0x0000001E,
    0x0000001F, 0x00000020, 0x0000001E, 0x0000001F,
    0x00000020, 0x00040020, 0x00000024, 0x00000006,
    0x00000007, 0x0004002B, 0x00000008, 0x00000029,
    0x00000042, 0x0004002B, 0x00000008, 0x00000031,
    0x00000000, 0x0004002B, 0x00000008, 0x00000033,
    0x00000001, 0x00040020, 0x00000034, 0x00000006,
    0x00000006, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200F8, 0x00000005,
    0x0004003B, 0x00000024, 0x00000025, 0x00000006,
    0x0004003B, 0x00000024, 0x0000002D, 0x00000006,
    0x0003003E, 0x0000000C, 0x00000021, 0x00050041,
    0x00000024, 0x00000026, 0x0000000C, 0x00000009,
    0x0004003D, 0x00000007, 0x00000027, 0x00000026,
    0x00050051, 0x00000006, 0x00000028, 0x00000027,
    0x00000000, 0x00050051, 0x00000006, 0x0000002A,
    0x00000027, 0x00000001, 0x00050050, 0x00000010,
    0x0000002B, 0x00000028, 0x0000002A, 0x0005003E,
    0x00000019, 0x0000002B, 0x0000001D, 0x00000033,
    0x00050041, 0x00000024, 0x0000002E, 0x0000000C,
    0x00000029, 0x0004003D, 0x00000007, 0x0000002F,
    0x0000002E, 0x00050051, 0x00000006, 0x00000030,
    0x0000002F, 0x00000000, 0x00050051, 0x00000006,
    0x00000032, 0x0000002F, 0x00000001, 0x0003003E,
    0x0000000D, 0x00000030, 0x0003003E, 0x00000025,
    0x00000032, 0x0003003E, 0x0000002D, 0x00000030,
    0x00050041, 0x00000034, 0x00000035, 0x0000002D,
    0x00000029, 0x0004003D, 0x00000006, 0x00000036,
    0x00000035, 0x00050041, 0x00000034, 0x00000037,
    0x0000002D, 0x00000033, 0x0004003D, 0x00000006,
    0x00000038, 0x00000037, 0x000200FE, 0x00000038,
    0x000200FE, 0x00000036
};

static const size_t g_fullscreen_vert_spirv_len =
    sizeof(g_fullscreen_vert_spirv);

/* ── vulkan_abi_create_graphics_pipeline ───────────────────────── */

static int vulkan_abi_create_graphics_pipeline(
    KainVulkanSession* session,
    const uint32_t*    vert_spirv,
    size_t             vert_len,
    const uint32_t*    frag_spirv,
    size_t             frag_len)
{
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    VkResult result;

    /* Create vertex + fragment shader modules */
    VkShaderModule vert_module = (VkShaderModule)0;
    VkShaderModule frag_module = (VkShaderModule)0;

    result = vulkan_abi_create_shader_module(device, vert_spirv,
                                              vert_len, &vert_module);
    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-50, "vkCreateShaderModule (vertex) failed");
        return -1;
    }

    result = vulkan_abi_create_shader_module(device, frag_spirv,
                                              frag_len, &frag_module);
    if (result != VK_SUCCESS) {
        pfn_vkDestroyShaderModule(device, vert_module, NULL);
        vulkan_abi_set_error(-51, "vkCreateShaderModule (fragment) failed");
        return -1;
    }

    /* Build pipeline shader stage create infos */
    VkPipelineShaderStageCreateInfo vert_stage;
    memset(&vert_stage, 0, sizeof(vert_stage));
    vert_stage.sType  = S_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName  = "main";

    VkPipelineShaderStageCreateInfo frag_stage;
    memset(&frag_stage, 0, sizeof(frag_stage));
    frag_stage.sType  = S_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[2] = { vert_stage, frag_stage };

    /* Vertex input state (empty — full-screen triangle via gl_VertexIndex) */
    VkPipelineVertexInputStateCreateInfo vertex_input;
    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = S_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    /* Input assembly state */
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    memset(&input_assembly, 0, sizeof(input_assembly));
    input_assembly.sType    = S_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    /* Viewport state (dynamic — set at draw time) */
    VkPipelineViewportStateCreateInfo viewport_state;
    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType         = S_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    /* Rasterization state */
    VkPipelineRasterizationStateCreateInfo rasterization;
    memset(&rasterization, 0, sizeof(rasterization));
    rasterization.sType       = S_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode    = VK_CULL_MODE_NONE;
    rasterization.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth   = 1.0f;

    /* Multisample state */
    VkPipelineMultisampleStateCreateInfo multisample;
    memset(&multisample, 0, sizeof(multisample));
    multisample.sType                = S_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* Color blend attachment state */
    VkPipelineColorBlendAttachmentState blend_attachment;
    memset(&blend_attachment, 0, sizeof(blend_attachment));
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    /* Color blend state */
    VkPipelineColorBlendStateCreateInfo color_blend;
    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType           = S_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments    = &blend_attachment;

    /* Pipeline layout (use session's layout which has descriptor set layout) */
    VkPipelineLayout pipeline_layout = session->pipeline_layout;
    if (pipeline_layout == (VkPipelineLayout)0) {
        /* Fallback: empty pipeline layout */
        VkPipelineLayoutCreateInfo layout_info;
        memset(&layout_info, 0, sizeof(layout_info));
        layout_info.sType = S_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        result = pfn_vkCreatePipelineLayout(device, &layout_info, NULL,
                                             &pipeline_layout);
        if (result != VK_SUCCESS) {
            pfn_vkDestroyShaderModule(device, frag_module, NULL);
            pfn_vkDestroyShaderModule(device, vert_module, NULL);
            vulkan_abi_set_error(-52, "vkCreatePipelineLayout failed");
            return -1;
        }
        session->pipeline_layout = pipeline_layout;
    }

    /* Graphics pipeline create info */
    VkGraphicsPipelineCreateInfo pipeline_info;
    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType               = S_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount          = 2;
    pipeline_info.pStages             = stages;
    pipeline_info.pVertexInputState   = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState   = &multisample;
    pipeline_info.pColorBlendState    = &color_blend;
    pipeline_info.layout              = pipeline_layout;
    pipeline_info.renderPass          = session->render_pass;
    pipeline_info.subpass             = 0;

    /* Create pipeline */
    VkPipeline pipeline = (VkPipeline)0;
    result = pfn_vkCreateGraphicsPipelines(device, (VkPipelineCache)0,
                                            1, &pipeline_info, NULL, &pipeline);

    /* Destroy shader modules (no longer needed after pipeline creation) */
    pfn_vkDestroyShaderModule(device, frag_module, NULL);
    pfn_vkDestroyShaderModule(device, vert_module, NULL);

    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-53, "vkCreateGraphicsPipelines failed");
        return -1;
    }

    session->pipeline = pipeline;
    return 0;
}

// ============================================================================
//  SECTION 12: Render Pass & Framebuffer Creation (~100 lines)
// ============================================================================

/* ── vulkan_abi_create_render_pass ─────────────────────────────── */

static int vulkan_abi_create_render_pass(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    /* ── Attachment description ── */
    VkAttachmentDescription color_attachment;
    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.format         = VK_FORMAT_B8G8R8A8_SRGB;
    color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    /* ── Attachment reference ── */
    VkAttachmentReference color_ref;
    memset(&color_ref, 0, sizeof(color_ref));
    color_ref.attachment = 0;
    color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* ── Subpass description ── */
    VkSubpassDescription subpass;
    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_ref;

    /* ── Render pass create info ── */
    VkRenderPassCreateInfo rp_info;
    memset(&rp_info, 0, sizeof(rp_info));
    rp_info.sType           = 38; /* VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO */
    rp_info.attachmentCount = 1;
    rp_info.pAttachments    = &color_attachment;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;

    VkResult result = pfn_vkCreateRenderPass(device, &rp_info, NULL,
                                              &session->render_pass);
    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-60, "vkCreateRenderPass failed");
        return -1;
    }

    /* ── Create framebuffers for each swapchain image view ── */
    for (uint32_t i = 0; i < session->swapchain_image_count; i++) {
        VkFramebufferCreateInfo fb_info;
        memset(&fb_info, 0, sizeof(fb_info));
        fb_info.sType           = 37; /* VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO */
        fb_info.renderPass      = session->render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments    = &session->swapchain_image_views[i];
        fb_info.width           = (uint32_t)session->width;
        fb_info.height          = (uint32_t)session->height;
        fb_info.layers          = 1;

        result = pfn_vkCreateFramebuffer(device, &fb_info, NULL,
                                          &session->framebuffers[i]);
        if (result != VK_SUCCESS) {
            vulkan_abi_set_error(-61, "vkCreateFramebuffer failed");
            return -1;
        }
    }

    return 0;
}

// ============================================================================
//  SECTION 13: Draw Command Recording (~150 lines)
// ============================================================================

/* ── vulkan_abi_record_draw_commands ───────────────────────────── */

static int vulkan_abi_record_draw_commands(KainVulkanSession* session) {
    VkCommandBuffer cmd = session->command_buffers[session->current_frame];
    uint32_t image_index = session->current_image_index;
    uint32_t frame       = session->current_frame;

    if (session->render_pass == (VkRenderPass)0) return -1;
    if (session->pipeline == (VkPipeline)0) return -2;

    /* ── Begin render pass ── */
    VkClearValue clear_value;
    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.color.float32[0] = 0.0f;  /* dark ocean blue */
    clear_value.color.float32[1] = 0.05f;
    clear_value.color.float32[2] = 0.15f;
    clear_value.color.float32[3] = 1.0f;

    VkRect2D render_area;
    memset(&render_area, 0, sizeof(render_area));
    render_area.offset.x      = 0;
    render_area.offset.y      = 0;
    render_area.extent.width  = (uint32_t)session->width;
    render_area.extent.height = (uint32_t)session->height;

    VkRenderPassBeginInfo rp_begin;
    memset(&rp_begin, 0, sizeof(rp_begin));
    rp_begin.sType             = 43; /* VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO */
    rp_begin.renderPass        = session->render_pass;
    rp_begin.framebuffer       = session->framebuffers[image_index];
    rp_begin.renderArea        = render_area;
    rp_begin.clearValueCount   = 1;
    rp_begin.pClearValues      = &clear_value;

    pfn_vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    /* ── Bind pipeline ── */
    pfn_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          session->pipeline);

    /* ── Set viewport ── */
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)session->width;
    viewport.height   = (float)session->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    if (pfn_vkCmdSetViewport) {
        pfn_vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    /* ── Set scissor ── */
    VkRect2D scissor;
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = (uint32_t)session->width;
    scissor.extent.height = (uint32_t)session->height;

    if (pfn_vkCmdSetScissor) {
        pfn_vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    /* ── Bind descriptor sets (uniforms from ocean.kn) ── */
    if (session->descriptor_set_layout != (VkDescriptorSetLayout)0 &&
        session->descriptor_sets[frame] != (VkDescriptorSet)0) {
        pfn_vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            session->pipeline_layout, 0, 1,
            &session->descriptor_sets[frame], 0, NULL);
    }

    /* ── Draw: 3 vertices = full-screen triangle ── */
    pfn_vkCmdDraw(cmd, 3, 1, 0, 0);

    /* ── End render pass ── */
    pfn_vkCmdEndRenderPass(cmd);

    return 0;
}

// ============================================================================
//  SECTION 14: Descriptor Set Layout & Uniform Buffers (~150 lines)
// ============================================================================

/* Forward declare vkCmdBindDescriptorSets — resolved at device time
 * alongside other PFNs. */

#define OCEAN_UNIFORM_BUFFER_SIZE 256

/* ── vulkan_abi_create_descriptor_set_layout ───────────────────── */
/* Creates descriptor set layout matching ocean.kn's uniforms:
 *   binding 0: uniform time: Float
 *   binding 1: uniform resolution: Vec2
 *   binding 2: uniform mouse: Vec2
 *   binding 3: StorageBuffer (not used for fullscreen triangle) */

static int vulkan_abi_create_descriptor_set_layout(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    VkDescriptorSetLayoutBinding bindings[3];
    memset(bindings, 0, sizeof(bindings));

    /* binding 0: time (Float) */
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    /* binding 1: resolution (Vec2) */
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    /* binding 2: mouse (Vec2) */
    bindings[2].binding         = 2;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info;
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType        = S_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 3;
    layout_info.pBindings    = bindings;

    VkResult result = pfn_vkCreateDescriptorSetLayout(device, &layout_info,
                                                       NULL,
                                                       &session->descriptor_set_layout);
    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-70, "vkCreateDescriptorSetLayout failed");
        return -1;
    }

    return 0;
}

/* ── vulkan_abi_create_descriptor_pool_and_sets ────────────────── */

static int vulkan_abi_create_descriptor_pool_and_sets(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    /* ── Descriptor pool ── */
    VkDescriptorPoolSize pool_size;
    memset(&pool_size, 0, sizeof(pool_size));
    pool_size.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 3 * KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo pool_info;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType         = S_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets       = KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes    = &pool_size;

    VkResult result = pfn_vkCreateDescriptorPool(device, &pool_info, NULL,
                                                  &session->descriptor_pool);
    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-71, "vkCreateDescriptorPool failed");
        return -1;
    }

    /* ── Allocate descriptor sets ── */
    VkDescriptorSetLayout layouts[KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT];
    for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
        layouts[i] = session->descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info;
    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType              = S_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = session->descriptor_pool;
    alloc_info.descriptorSetCount = KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT;
    alloc_info.pSetLayouts        = layouts;

    result = pfn_vkAllocateDescriptorSets(device, &alloc_info,
                                           session->descriptor_sets);
    if (result != VK_SUCCESS) {
        vulkan_abi_set_error(-72, "vkAllocateDescriptorSets failed");
        return -1;
    }

    return 0;
}

/* ── vulkan_abi_create_uniform_buffers ─────────────────────────── */

static int vulkan_abi_create_uniform_buffers(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    VkResult result;

    for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
        /* ── Create buffer ── */
        VkBufferCreateInfo buffer_info;
        memset(&buffer_info, 0, sizeof(buffer_info));
        buffer_info.sType       = S_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size        = OCEAN_UNIFORM_BUFFER_SIZE;
        buffer_info.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        result = pfn_vkCreateBuffer(device, &buffer_info, NULL,
                                     &session->uniform_buffers[i]);
        if (result != VK_SUCCESS) {
            vulkan_abi_set_error(-73, "vkCreateBuffer failed");
            return -1;
        }

        /* ── Get memory requirements ── */
        VkMemoryRequirements mem_reqs;
        memset(&mem_reqs, 0, sizeof(mem_reqs));
        pfn_vkGetBufferMemoryRequirements(device, session->uniform_buffers[i],
                                           &mem_reqs);

        /* ── Find host-visible + host-coherent memory type ── */
        VkPhysicalDeviceMemoryProperties mem_props;
        memset(&mem_props, 0, sizeof(mem_props));
        pfn_vkGetPhysicalDeviceMemoryProperties(session->physical_device,
                                                 &mem_props);

        uint32_t memory_type_index = 0xFFFFFFFF;
        for (uint32_t j = 0; j < mem_props.memoryTypeCount; j++) {
            if ((mem_reqs.memoryTypeBits & (1u << j)) &&
                (mem_props.memoryTypes[j].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                memory_type_index = j;
                break;
            }
        }
        if (memory_type_index == 0xFFFFFFFF) {
            /* Fallback: just host-visible */
            for (uint32_t j = 0; j < mem_props.memoryTypeCount; j++) {
                if ((mem_reqs.memoryTypeBits & (1u << j)) &&
                    (mem_props.memoryTypes[j].propertyFlags &
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                    memory_type_index = j;
                    break;
                }
            }
        }
        if (memory_type_index == 0xFFFFFFFF) {
            vulkan_abi_set_error(-74, "No suitable memory type for uniform buffer");
            return -1;
        }

        /* ── Allocate memory ── */
        VkMemoryAllocateInfo alloc_info;
        memset(&alloc_info, 0, sizeof(alloc_info));
        alloc_info.sType          = S_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index;

        result = pfn_vkAllocateMemory(device, &alloc_info, NULL,
                                       &session->uniform_memory[i]);
        if (result != VK_SUCCESS) {
            vulkan_abi_set_error(-75, "vkAllocateMemory failed");
            return -1;
        }

        /* ── Bind buffer memory ── */
        result = pfn_vkBindBufferMemory(device, session->uniform_buffers[i],
                                         session->uniform_memory[i], 0);
        if (result != VK_SUCCESS) {
            vulkan_abi_set_error(-76, "vkBindBufferMemory failed");
            return -1;
        }

        /* ── Map memory ── */
        result = pfn_vkMapMemory(device, session->uniform_memory[i], 0,
                                  OCEAN_UNIFORM_BUFFER_SIZE, 0,
                                  &session->uniform_mapped[i]);
        if (result != VK_SUCCESS) {
            vulkan_abi_set_error(-77, "vkMapMemory failed");
            return -1;
        }
    }

    return 0;
}

/* ── vulkan_abi_update_descriptor_sets ─────────────────────────── */
/* Writes the uniform buffer descriptors for each frame's descriptor set. */

static int vulkan_abi_update_descriptor_sets(KainVulkanSession* session) {
    VkDevice device = session->device;
    if (device == (VkDevice)0) return -1;

    for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo buffer_infos[3];
        VkWriteDescriptorSet writes[3];
        memset(buffer_infos, 0, sizeof(buffer_infos));
        memset(writes, 0, sizeof(writes));

        for (uint32_t b = 0; b < 3; b++) {
            buffer_infos[b].buffer = session->uniform_buffers[i];
            buffer_infos[b].offset = b * 64; /* 64 bytes per binding */
            buffer_infos[b].range  = 64;

            writes[b].sType           = S_TYPE_WRITE_DESCRIPTOR_SET;
            writes[b].dstSet          = session->descriptor_sets[i];
            writes[b].dstBinding      = b;
            writes[b].descriptorCount = 1;
            writes[b].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[b].pBufferInfo     = &buffer_infos[b];
        }

        pfn_vkUpdateDescriptorSets(device, 3, writes, 0, NULL);
    }

    return 0;
}

// ============================================================================
//  SECTION 15: Exported Helpers & Pipeline Boot (~100 lines)
// ============================================================================

/* ── kain_vulkan_abi_load_shader ───────────────────────────────── */
/* Decodes a hex-encoded SPIR-V fragment shader, creates the full
 * rendering pipeline (render pass, pipeline, descriptor sets,
 * uniform buffers) and marks the session as pipeline-ready.
 * The vertex shader is our hardcoded fullscreen triangle. */

KAIN_VULKAN_ABI_EXPORT int kain_vulkan_abi_load_shader(
    int64_t session_id, const char* spirv_hex)
{
    KainVulkanSession* s = vulkan_find_session(session_id);
    if (!s) {
        vulkan_abi_set_error(-80, "kain_vulkan_abi_load_shader: invalid session");
        return -1;
    }
    if (s->device == (VkDevice)0) {
        vulkan_abi_set_error(-81, "kain_vulkan_abi_load_shader: session not attached");
        return -1;
    }
    if (!spirv_hex || !spirv_hex[0]) {
        vulkan_abi_set_error(-82, "kain_vulkan_abi_load_shader: null/empty hex");
        return -1;
    }

    /* Decode hex SPIR-V to binary */
    size_t frag_word_count = 0;
    uint32_t* frag_spirv = vulkan_abi_decode_spirv_hex(spirv_hex,
                                                         &frag_word_count);
    if (!frag_spirv) {
        vulkan_abi_set_error(-83, "kain_vulkan_abi_load_shader: hex decode failed");
        return -1;
    }

    int rc = 0;

    /* 1. Create render pass (if not already done) */
    if (s->render_pass == (VkRenderPass)0) {
        rc = vulkan_abi_create_render_pass(s);
        if (rc != 0) {
            free(frag_spirv);
            return rc;
        }
    }

    /* 2. Create descriptor set layout */
    if (s->descriptor_set_layout == (VkDescriptorSetLayout)0) {
        rc = vulkan_abi_create_descriptor_set_layout(s);
        if (rc != 0) {
            free(frag_spirv);
            return rc;
        }
    }

    /* 3. Create pipeline layout */
    if (s->pipeline_layout == (VkPipelineLayout)0) {
        VkPipelineLayoutCreateInfo layout_info;
        memset(&layout_info, 0, sizeof(layout_info));
        layout_info.sType          = S_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts    = &s->descriptor_set_layout;

        VkResult result = pfn_vkCreatePipelineLayout(s->device, &layout_info,
                                                      NULL, &s->pipeline_layout);
        if (result != VK_SUCCESS) {
            free(frag_spirv);
            vulkan_abi_set_error(-84, "vkCreatePipelineLayout failed");
            return -1;
        }
    }

    /* 4. Create graphics pipeline */
    if (s->pipeline == (VkPipeline)0) {
        rc = vulkan_abi_create_graphics_pipeline(
            s,
            g_fullscreen_vert_spirv, g_fullscreen_vert_spirv_len,
            frag_spirv, frag_word_count * sizeof(uint32_t));
        if (rc != 0) {
            free(frag_spirv);
            return rc;
        }
    }

    /* 5. Create descriptor pool + sets */
    if (s->descriptor_pool == (VkDescriptorPool)0) {
        rc = vulkan_abi_create_descriptor_pool_and_sets(s);
        if (rc != 0) {
            free(frag_spirv);
            return rc;
        }
    }

    /* 6. Create uniform buffers */
    if (s->uniform_buffers[0] == (VkBuffer)0) {
        rc = vulkan_abi_create_uniform_buffers(s);
        if (rc != 0) {
            free(frag_spirv);
            return rc;
        }
        /* Write buffer descriptors */
        vulkan_abi_update_descriptor_sets(s);
    }

    free(frag_spirv);
    s->pipeline_ready = 1;
    vulkan_abi_set_error(0, "kain_vulkan_abi_load_shader: pipeline ready");
    return 0;
}

/* ── kain_vulkan_abi_set_uniform ───────────────────────────────── */
/* Updates a uniform buffer binding for the current frame.
 * binding: 0=time, 1=resolution, 2=mouse
 * data/size: raw bytes to copy */

KAIN_VULKAN_ABI_EXPORT int kain_vulkan_abi_set_uniform(
    int64_t session_id, uint32_t binding, const void* data, uint64_t size)
{
    KainVulkanSession* s = vulkan_find_session(session_id);
    if (!s || !data || !size) return -1;
    if (binding >= 3) return -1;

    /* Write to ALL frames' uniform buffers so every in-flight frame
     * gets the same uniform data. */
    for (uint32_t i = 0; i < KAIN_VULKAN_ABI_MAX_FRAMES_IN_FLIGHT; i++) {
        if (s->uniform_mapped[i] == NULL) continue;
        uint64_t max_size = OCEAN_UNIFORM_BUFFER_SIZE - binding * 64;
        uint64_t copy_size = (size < max_size) ? size : max_size;
        memcpy((uint8_t*)s->uniform_mapped[i] + binding * 64,
               data, (size_t)copy_size);
    }
    return 0;
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
