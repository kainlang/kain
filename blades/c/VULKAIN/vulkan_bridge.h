#ifndef KAIN_VULKAN_BRIDGE_H
#define KAIN_VULKAN_BRIDGE_H

#include <vulkan/vulkan.h>

// Bridge functions that expose Vulkan to Kain via the C ABI
// These are thin wrappers that Kain calls via include

static VkResult kain_vk_result = VK_SUCCESS;

inline int vulkan_bridge_has_loader(void) {
    PFN_vkVoidFunction addr = vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    return addr != NULL ? 1 : 0;
}

inline int vulkan_bridge_instance_size(void) {
    return (int)sizeof(VkInstance);
}

inline int vulkan_bridge_physical_device_size(void) {
    return (int)sizeof(VkPhysicalDevice);
}

#endif
