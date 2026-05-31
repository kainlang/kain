#ifndef KAIN_VULKAN_LOADER_SUBSET_H
#define KAIN_VULKAN_LOADER_SUBSET_H

#include <stdint.h>

typedef uintptr_t VkInstance;
typedef uintptr_t VkDevice;
typedef uint32_t VkResult;
typedef uintptr_t VkLoaderProcAddress;

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

#endif
