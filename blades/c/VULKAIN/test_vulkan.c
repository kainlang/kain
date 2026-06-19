#include <stdio.h>
#include <vulkan/vulkan.h>

int main() {
    printf("Testing Vulkan loader...\n");
    
    PFN_vkVoidFunction addr = vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    printf("vkGetInstanceProcAddr(NULL, \"vkCreateInstance\") = %p\n", (void*)addr);
    
    VkInstanceCreateInfo ci = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkInstance inst;
    VkResult r = vkCreateInstance(&ci, NULL, &inst);
    printf("vkCreateInstance = %d\n", r);
    
    if (r == VK_SUCCESS) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(inst, &count, NULL);
        printf("Physical device count: %u\n", count);
        vkDestroyInstance(inst, NULL);
    }
    
    printf("Done.\n");
    return 0;
}
