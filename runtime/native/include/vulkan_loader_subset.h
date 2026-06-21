#ifndef KAIN_VULKAN_LOADER_SUBSET_H
#define KAIN_VULKAN_LOADER_SUBSET_H

#include <stdint.h>

typedef uintptr_t VkInstance;
typedef uintptr_t VkDevice;
typedef uint32_t VkResult;
typedef uintptr_t VkLoaderProcAddress;

// ── Original 5 prototypes (preserved exactly as-is) ─────────────

VkLoaderProcAddress vkGetInstanceProcAddr(uintptr_t instance, const char* pName);
VkLoaderProcAddress vkGetDeviceProcAddr(uintptr_t device, const char* pName);
VkResult vkEnumerateInstanceVersion(uint32_t* pApiVersion);
VkResult vkEnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    void* pProperties
);
VkResult vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    void* pProperties
);

// ── Extended Vulkan handle types (all uintptr_t) ─────────────────

typedef uintptr_t VkPhysicalDevice;
typedef uintptr_t VkSurfaceKHR;
typedef uintptr_t VkQueue;
typedef uintptr_t VkCommandPool;
typedef uintptr_t VkCommandBuffer;
typedef uintptr_t VkFence;
typedef uintptr_t VkSemaphore;
typedef uintptr_t VkSwapchainKHR;
typedef uintptr_t VkImage;
typedef uintptr_t VkImageView;
typedef uintptr_t VkFramebuffer;
typedef uintptr_t VkRenderPass;
typedef uintptr_t VkPipeline;
typedef uintptr_t VkPipelineLayout;
typedef uintptr_t VkShaderModule;
typedef uintptr_t VkDescriptorSetLayout;
typedef uintptr_t VkDescriptorPool;
typedef uintptr_t VkDescriptorSet;
typedef uintptr_t VkPipelineCache;
typedef uint32_t VkPipelineBindPoint;
typedef uint32_t VkShaderStageFlags;
typedef uint32_t VkSubpassContents;
typedef uint32_t VkCommandBufferResetFlags;
typedef uintptr_t PFN_vkVoidFunction;
typedef uintptr_t VkDeviceMemory;
typedef uintptr_t VkBuffer;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkBufferUsageFlags;
typedef uint32_t VkBufferCreateFlags;
typedef uint32_t VkMemoryHeapFlags;
typedef uint32_t VkMemoryAllocateFlags;
typedef uint32_t VkDescriptorType;

// ── Instance functions ───────────────────────────────────────────

VkResult vkCreateInstance(const void* pCreateInfo, const void* pAllocator,
                          VkInstance* pInstance);
void     vkDestroyInstance(VkInstance instance, const void* pAllocator);
VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* count,
                                     VkPhysicalDevice* devices);
void     vkGetPhysicalDeviceProperties(VkPhysicalDevice device, void* props);
void     vkGetPhysicalDeviceFeatures(VkPhysicalDevice device, void* features);
void     vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device,
                                                   uint32_t* count, void* props);

// ── Device functions ─────────────────────────────────────────────

VkResult vkCreateDevice(VkPhysicalDevice physicalDevice,
                        const void* pCreateInfo, const void* pAllocator,
                        VkDevice* pDevice);
void     vkDestroyDevice(VkDevice device, const void* pAllocator);
void     vkGetDeviceQueue(VkDevice device, uint32_t queueFamily,
                          uint32_t queueIndex, VkQueue* pQueue);
VkResult vkDeviceWaitIdle(VkDevice device);

// ── WSI Surface (per-platform) ──────────────────────────────────

VkResult vkCreateWin32SurfaceKHR(VkInstance instance,
                                  const void* pCreateInfo,
                                  const void* pAllocator,
                                  VkSurfaceKHR* pSurface);
VkResult vkCreateXlibSurfaceKHR(VkInstance instance,
                                 const void* pCreateInfo,
                                 const void* pAllocator,
                                 VkSurfaceKHR* pSurface);
VkResult vkCreateWaylandSurfaceKHR(VkInstance instance,
                                    const void* pCreateInfo,
                                    const void* pAllocator,
                                    VkSurfaceKHR* pSurface);
VkResult vkCreateMacOSSurfaceMVK(VkInstance instance,
                                  const void* pCreateInfo,
                                  const void* pAllocator,
                                  VkSurfaceKHR* pSurface);
void     vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                              const void* pAllocator);
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice device,
                                               uint32_t queueFamily,
                                               VkSurfaceKHR surface,
                                               uint32_t* supported);
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice device,
                                                    VkSurfaceKHR surface,
                                                    void* pCapabilities);
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice device,
                                               VkSurfaceKHR surface,
                                               uint32_t* count,
                                               void* formats);
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice device,
                                                    VkSurfaceKHR surface,
                                                    uint32_t* count,
                                                    uint32_t* modes);

// ── Swapchain ────────────────────────────────────────────────────

VkResult vkCreateSwapchainKHR(VkDevice device, const void* pCreateInfo,
                               const void* pAllocator,
                               VkSwapchainKHR* pSwapchain);
void     vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                const void* pAllocator);
VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                  uint32_t* count, VkImage* images);
VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                uint64_t timeout, VkSemaphore semaphore,
                                VkFence fence, uint32_t* imageIndex);
VkResult vkQueuePresentKHR(VkQueue queue, const void* pPresentInfo);

// ── Command buffers ──────────────────────────────────────────────

VkResult vkCreateCommandPool(VkDevice device, const void* pCreateInfo,
                              const void* pAllocator, VkCommandPool* pPool);
void     vkDestroyCommandPool(VkDevice device, VkCommandPool pool,
                               const void* pAllocator);
VkResult vkAllocateCommandBuffers(VkDevice device,
                                   const void* pAllocateInfo,
                                   VkCommandBuffer* pBuffers);
VkResult vkBeginCommandBuffer(VkCommandBuffer buffer,
                               const void* pBeginInfo);
VkResult vkEndCommandBuffer(VkCommandBuffer buffer);
VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                        const void* pSubmits, VkFence fence);

// ── Synchronization ──────────────────────────────────────────────

VkResult vkCreateSemaphore(VkDevice device, const void* pCreateInfo,
                            const void* pAllocator, VkSemaphore* pSemaphore);
void     vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                             const void* pAllocator);
VkResult vkCreateFence(VkDevice device, const void* pCreateInfo,
                        const void* pAllocator, VkFence* pFence);
void     vkDestroyFence(VkDevice device, VkFence fence,
                         const void* pAllocator);
VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount,
                          const VkFence* fences, uint32_t waitAll,
                          uint64_t timeout);
VkResult vkResetFences(VkDevice device, uint32_t fenceCount,
                        const VkFence* fences);

// ── Image views ──────────────────────────────────────────────────

VkResult vkCreateImageView(VkDevice device, const void* pCreateInfo,
                            const void* pAllocator, VkImageView* pView);
void     vkDestroyImageView(VkDevice device, VkImageView view,
                             const void* pAllocator);

// ── Rendering pipeline ──────────────────────────────────────────

VkResult vkCreateRenderPass(VkDevice device, const void* pCreateInfo,
                             const void* pAllocator, VkRenderPass* pRenderPass);
void     vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                              const void* pAllocator);
VkResult vkCreateShaderModule(VkDevice device, const void* pCreateInfo,
                               const void* pAllocator, VkShaderModule* pShaderModule);
void     vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
                                const void* pAllocator);
VkResult vkCreatePipelineLayout(VkDevice device, const void* pCreateInfo,
                                 const void* pAllocator, VkPipelineLayout* pPipelineLayout);
void     vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                                  const void* pAllocator);
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                    uint32_t createInfoCount, const void* pCreateInfos,
                                    const void* pAllocator, VkPipeline* pPipelines);
void     vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
                            const void* pAllocator);
VkResult vkCreateFramebuffer(VkDevice device, const void* pCreateInfo,
                              const void* pAllocator, VkFramebuffer* pFramebuffer);
void     vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                               const void* pAllocator);

// ── Command recording ───────────────────────────────────────────

void     vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                               const void* pRenderPassBegin, VkSubpassContents contents);
void     vkCmdEndRenderPass(VkCommandBuffer commandBuffer);
void     vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                            VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
void     vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                           uint32_t viewportCount, const void* pViewports);
void     vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                          uint32_t scissorCount, const void* pScissors);
void     vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                             VkShaderStageFlags stageFlags, uint32_t offset,
                             uint32_t size, const void* pValues);
void     vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                    uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);

// ── Descriptor sets ─────────────────────────────────────────────

VkResult vkCreateDescriptorSetLayout(VkDevice device, const void* pCreateInfo,
                                      const void* pAllocator,
                                      VkDescriptorSetLayout* pSetLayout);
void     vkDestroyDescriptorSetLayout(VkDevice device,
                                       VkDescriptorSetLayout descriptorSetLayout,
                                       const void* pAllocator);
VkResult vkCreateDescriptorPool(VkDevice device, const void* pCreateInfo,
                                 const void* pAllocator, VkDescriptorPool* pDescriptorPool);
void     vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                  const void* pAllocator);
VkResult vkAllocateDescriptorSets(VkDevice device, const void* pAllocateInfo,
                                   VkDescriptorSet* pDescriptorSets);
void     vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
                                 const void* pDescriptorWrites,
                                 uint32_t descriptorCopyCount,
                                 const void* pDescriptorCopies);

// ── Buffer + memory ─────────────────────────────────────────────

VkResult vkCreateBuffer(VkDevice device, const void* pCreateInfo,
                         const void* pAllocator, VkBuffer* pBuffer);
void     vkDestroyBuffer(VkDevice device, VkBuffer buffer, const void* pAllocator);
void     vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                        void* pMemoryRequirements);
VkResult vkAllocateMemory(VkDevice device, const void* pAllocateInfo,
                           const void* pAllocator, VkDeviceMemory* pMemory);
void     vkFreeMemory(VkDevice device, VkDeviceMemory memory, const void* pAllocator);
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer,
                             VkDeviceMemory memory, VkDeviceSize memoryOffset);
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                      VkDeviceSize size, uint32_t flags, void** ppData);
void     vkUnmapMemory(VkDevice device, VkDeviceMemory memory);
void     vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                              void* pMemoryProperties);
VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                               VkCommandBufferResetFlags flags);

#endif
