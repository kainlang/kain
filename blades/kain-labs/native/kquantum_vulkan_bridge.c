#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if !defined(_WIN32)
#error "kquantum_vulkan_bridge currently targets the Win32 Vulkan surface path."
#endif

#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>

#include "kquantum_vulkan_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KQV_MAX_PHYSICAL_DEVICES 16u
#define KQV_MAX_QUEUE_FAMILIES 32u
#define KQV_MAX_SURFACE_FORMATS 32u
#define KQV_MAX_PRESENT_MODES 16u
#define KQV_MAX_SWAPCHAIN_IMAGES 8u
#define KQV_MAX_PARTICLES 1048576u
#define KQV_MAX_SHADER_BYTES (16u * 1024u * 1024u)

typedef struct KqvPushConstants {
    float time_seconds;
    float particle_count;
    int32_t mode;
    float chaos;
} KqvPushConstants;

typedef struct KqvApp {
    HINSTANCE hinstance;
    HWND hwnd;
    int closing;
    int32_t width;
    int32_t height;
    uint32_t particle_count;
    int32_t mode;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    uint32_t queue_family_index;
    VkDevice device;
    VkQueue graphics_queue;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage images[KQV_MAX_SWAPCHAIN_IMAGES];
    VkImageView image_views[KQV_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer framebuffers[KQV_MAX_SWAPCHAIN_IMAGES];

    VkRenderPass render_pass;
    VkShaderModule vertex_shader;
    VkShaderModule fragment_shader;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[KQV_MAX_SWAPCHAIN_IMAGES];
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    uint8_t* vertex_spv;
    size_t vertex_spv_size;
    uint8_t* fragment_spv;
    size_t fragment_spv_size;
} KqvApp;

static HMODULE g_vulkan_module;
static char g_last_error[768] = "ok";
static int64_t g_frames_presented = 0;
static int64_t g_particles_drawn = 0;

static PFN_vkGetInstanceProcAddr q_vkGetInstanceProcAddr;
static PFN_vkGetDeviceProcAddr q_vkGetDeviceProcAddr;
static PFN_vkCreateInstance q_vkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties q_vkEnumerateInstanceExtensionProperties;
static PFN_vkDestroyInstance q_vkDestroyInstance;
static PFN_vkCreateWin32SurfaceKHR q_vkCreateWin32SurfaceKHR;
static PFN_vkDestroySurfaceKHR q_vkDestroySurfaceKHR;
static PFN_vkEnumeratePhysicalDevices q_vkEnumeratePhysicalDevices;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties q_vkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR q_vkGetPhysicalDeviceSurfaceSupportKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR q_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR q_vkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR q_vkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkCreateDevice q_vkCreateDevice;
static PFN_vkDestroyDevice q_vkDestroyDevice;
static PFN_vkGetDeviceQueue q_vkGetDeviceQueue;
static PFN_vkCreateSwapchainKHR q_vkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR q_vkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR q_vkGetSwapchainImagesKHR;
static PFN_vkCreateImageView q_vkCreateImageView;
static PFN_vkDestroyImageView q_vkDestroyImageView;
static PFN_vkCreateRenderPass q_vkCreateRenderPass;
static PFN_vkDestroyRenderPass q_vkDestroyRenderPass;
static PFN_vkCreateShaderModule q_vkCreateShaderModule;
static PFN_vkDestroyShaderModule q_vkDestroyShaderModule;
static PFN_vkCreatePipelineLayout q_vkCreatePipelineLayout;
static PFN_vkDestroyPipelineLayout q_vkDestroyPipelineLayout;
static PFN_vkCreateGraphicsPipelines q_vkCreateGraphicsPipelines;
static PFN_vkDestroyPipeline q_vkDestroyPipeline;
static PFN_vkCreateFramebuffer q_vkCreateFramebuffer;
static PFN_vkDestroyFramebuffer q_vkDestroyFramebuffer;
static PFN_vkCreateCommandPool q_vkCreateCommandPool;
static PFN_vkDestroyCommandPool q_vkDestroyCommandPool;
static PFN_vkAllocateCommandBuffers q_vkAllocateCommandBuffers;
static PFN_vkResetCommandBuffer q_vkResetCommandBuffer;
static PFN_vkBeginCommandBuffer q_vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer q_vkEndCommandBuffer;
static PFN_vkCmdBeginRenderPass q_vkCmdBeginRenderPass;
static PFN_vkCmdEndRenderPass q_vkCmdEndRenderPass;
static PFN_vkCmdBindPipeline q_vkCmdBindPipeline;
static PFN_vkCmdPushConstants q_vkCmdPushConstants;
static PFN_vkCmdDraw q_vkCmdDraw;
static PFN_vkCreateSemaphore q_vkCreateSemaphore;
static PFN_vkDestroySemaphore q_vkDestroySemaphore;
static PFN_vkCreateFence q_vkCreateFence;
static PFN_vkDestroyFence q_vkDestroyFence;
static PFN_vkWaitForFences q_vkWaitForFences;
static PFN_vkResetFences q_vkResetFences;
static PFN_vkAcquireNextImageKHR q_vkAcquireNextImageKHR;
static PFN_vkQueueSubmit q_vkQueueSubmit;
static PFN_vkQueuePresentKHR q_vkQueuePresentKHR;
static PFN_vkDeviceWaitIdle q_vkDeviceWaitIdle;

static const char* kqv_result_name(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        default: return "VK_RESULT_UNKNOWN";
    }
}

static int32_t kqv_fail_text(const char* stage, const char* message) {
    snprintf(g_last_error, sizeof(g_last_error), "%s: %s", stage, message);
    return -1;
}

static int32_t kqv_fail_vk(const char* stage, VkResult result) {
    snprintf(g_last_error, sizeof(g_last_error), "%s: %s (%d)", stage, kqv_result_name(result), (int)result);
    return -1;
}

static void kqv_ok(void) {
    snprintf(g_last_error, sizeof(g_last_error), "ok");
}

static uint32_t kqv_clamp_u32(uint32_t value, uint32_t lo, uint32_t hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static uint32_t kqv_particle_budget(int64_t requested) {
    if (requested <= 0) {
        return 1u;
    }
    if ((uint64_t)requested > (uint64_t)KQV_MAX_PARTICLES) {
        return KQV_MAX_PARTICLES;
    }
    return (uint32_t)requested;
}

static uint32_t kqv_safe_swapchain_image_count(const KqvApp* app) {
    if (!app || app->image_count > KQV_MAX_SWAPCHAIN_IMAGES) {
        return KQV_MAX_SWAPCHAIN_IMAGES;
    }
    return app->image_count;
}

static int32_t kqv_read_binary_file(const char* path, uint8_t** out_bytes, size_t* out_size) {
    FILE* file;
    long end;
    uint8_t* bytes;
    size_t read_count;
    if (!path || !path[0] || !out_bytes || !out_size) {
        return kqv_fail_text("read-spv", "missing shader path");
    }
    *out_bytes = NULL;
    *out_size = 0u;
    file = fopen(path, "rb");
    if (!file) {
        return kqv_fail_text("read-spv", path);
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return kqv_fail_text("read-spv", "fseek failed");
    }
    end = ftell(file);
    if (end <= 0 || (unsigned long)end > KQV_MAX_SHADER_BYTES || ((unsigned long)end % 4u) != 0u) {
        fclose(file);
        return kqv_fail_text("read-spv", "shader size is invalid");
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return kqv_fail_text("read-spv", "rewind failed");
    }
    bytes = (uint8_t*)malloc((size_t)end);
    if (!bytes) {
        fclose(file);
        return kqv_fail_text("read-spv", "shader allocation failed");
    }
    read_count = fread(bytes, 1u, (size_t)end, file);
    fclose(file);
    if (read_count != (size_t)end) {
        free(bytes);
        return kqv_fail_text("read-spv", "shader read was truncated");
    }
    *out_bytes = bytes;
    *out_size = (size_t)end;
    return 0;
}

static int32_t kqv_load_global_vulkan(void) {
    if (!g_vulkan_module) {
        g_vulkan_module = LoadLibraryA("vulkan-1.dll");
    }
    if (!g_vulkan_module) {
        return kqv_fail_text("vulkan-loader", "vulkan-1.dll was not found");
    }
    q_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(g_vulkan_module, "vkGetInstanceProcAddr");
    if (!q_vkGetInstanceProcAddr) {
        return kqv_fail_text("vulkan-loader", "vkGetInstanceProcAddr was not exported");
    }
    q_vkCreateInstance = (PFN_vkCreateInstance)q_vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    q_vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)q_vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");
    if (!q_vkCreateInstance || !q_vkEnumerateInstanceExtensionProperties) {
        return kqv_fail_text("vulkan-loader", "required global Vulkan commands were not exported");
    }
    return 0;
}

static int32_t kqv_load_instance_vulkan(VkInstance instance) {
#define KQV_LOAD_INSTANCE(name) \
    do { \
        q_##name = (PFN_##name)q_vkGetInstanceProcAddr(instance, #name); \
        if (!q_##name) { return kqv_fail_text("vulkan-instance-loader", #name); } \
    } while (0)
    KQV_LOAD_INSTANCE(vkDestroyInstance);
    KQV_LOAD_INSTANCE(vkCreateWin32SurfaceKHR);
    KQV_LOAD_INSTANCE(vkDestroySurfaceKHR);
    KQV_LOAD_INSTANCE(vkEnumeratePhysicalDevices);
    KQV_LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
    KQV_LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
    KQV_LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    KQV_LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
    KQV_LOAD_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
    KQV_LOAD_INSTANCE(vkCreateDevice);
    q_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)q_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    if (!q_vkGetDeviceProcAddr) {
        return kqv_fail_text("vulkan-instance-loader", "vkGetDeviceProcAddr");
    }
#undef KQV_LOAD_INSTANCE
    return 0;
}

static int32_t kqv_load_device_vulkan(VkDevice device) {
#define KQV_LOAD_DEVICE(name) \
    do { \
        q_##name = (PFN_##name)q_vkGetDeviceProcAddr(device, #name); \
        if (!q_##name) { return kqv_fail_text("vulkan-device-loader", #name); } \
    } while (0)
    KQV_LOAD_DEVICE(vkDestroyDevice);
    KQV_LOAD_DEVICE(vkGetDeviceQueue);
    KQV_LOAD_DEVICE(vkCreateSwapchainKHR);
    KQV_LOAD_DEVICE(vkDestroySwapchainKHR);
    KQV_LOAD_DEVICE(vkGetSwapchainImagesKHR);
    KQV_LOAD_DEVICE(vkCreateImageView);
    KQV_LOAD_DEVICE(vkDestroyImageView);
    KQV_LOAD_DEVICE(vkCreateRenderPass);
    KQV_LOAD_DEVICE(vkDestroyRenderPass);
    KQV_LOAD_DEVICE(vkCreateShaderModule);
    KQV_LOAD_DEVICE(vkDestroyShaderModule);
    KQV_LOAD_DEVICE(vkCreatePipelineLayout);
    KQV_LOAD_DEVICE(vkDestroyPipelineLayout);
    KQV_LOAD_DEVICE(vkCreateGraphicsPipelines);
    KQV_LOAD_DEVICE(vkDestroyPipeline);
    KQV_LOAD_DEVICE(vkCreateFramebuffer);
    KQV_LOAD_DEVICE(vkDestroyFramebuffer);
    KQV_LOAD_DEVICE(vkCreateCommandPool);
    KQV_LOAD_DEVICE(vkDestroyCommandPool);
    KQV_LOAD_DEVICE(vkAllocateCommandBuffers);
    KQV_LOAD_DEVICE(vkResetCommandBuffer);
    KQV_LOAD_DEVICE(vkBeginCommandBuffer);
    KQV_LOAD_DEVICE(vkEndCommandBuffer);
    KQV_LOAD_DEVICE(vkCmdBeginRenderPass);
    KQV_LOAD_DEVICE(vkCmdEndRenderPass);
    KQV_LOAD_DEVICE(vkCmdBindPipeline);
    KQV_LOAD_DEVICE(vkCmdPushConstants);
    KQV_LOAD_DEVICE(vkCmdDraw);
    KQV_LOAD_DEVICE(vkCreateSemaphore);
    KQV_LOAD_DEVICE(vkDestroySemaphore);
    KQV_LOAD_DEVICE(vkCreateFence);
    KQV_LOAD_DEVICE(vkDestroyFence);
    KQV_LOAD_DEVICE(vkWaitForFences);
    KQV_LOAD_DEVICE(vkResetFences);
    KQV_LOAD_DEVICE(vkAcquireNextImageKHR);
    KQV_LOAD_DEVICE(vkQueueSubmit);
    KQV_LOAD_DEVICE(vkQueuePresentKHR);
    KQV_LOAD_DEVICE(vkDeviceWaitIdle);
#undef KQV_LOAD_DEVICE
    return 0;
}

static LRESULT CALLBACK kqv_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    KqvApp* app = (KqvApp*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTA* create = (CREATESTRUCTA*)lparam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
    if (message == WM_CLOSE) {
        if (app) {
            app->closing = 1;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    if (message == WM_DESTROY) {
        if (app) {
            app->closing = 1;
        }
        return 0;
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        if (app) {
            app->closing = 1;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int32_t kqv_create_window(KqvApp* app, const char* title) {
    static int registered = 0;
    const char* class_name = "KQuantumVulkanParticleWindow";
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT rect;
    if (!registered) {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = kqv_window_proc;
        wc.hInstance = app->hinstance;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if (!RegisterClassExA(&wc)) {
            return kqv_fail_text("win32-window", "RegisterClassExA failed");
        }
        registered = 1;
    }
    rect.left = 0;
    rect.top = 0;
    rect.right = app->width;
    rect.bottom = app->height;
    AdjustWindowRectEx(&rect, style, FALSE, 0);
    app->hwnd = CreateWindowExA(
        0,
        class_name,
        title && title[0] ? title : "KQuantum Vulkan Particle Field",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        app->hinstance,
        app
    );
    if (!app->hwnd) {
        return kqv_fail_text("win32-window", "CreateWindowExA failed");
    }
    ShowWindow(app->hwnd, SW_SHOW);
    UpdateWindow(app->hwnd);
    return 0;
}

static void kqv_pump_window(KqvApp* app) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (!IsWindow(app->hwnd)) {
        app->closing = 1;
    }
}

static int32_t kqv_create_instance(KqvApp* app) {
    VkApplicationInfo application_info;
    VkInstanceCreateInfo create_info;
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    VkResult result;
    memset(&application_info, 0, sizeof(application_info));
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "KQuantum Vulkan C FFI";
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "Kain";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_1;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledExtensionCount = 2u;
    create_info.ppEnabledExtensionNames = extensions;

    result = q_vkCreateInstance(&create_info, NULL, &app->instance);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateInstance", result);
    }
    return kqv_load_instance_vulkan(app->instance);
}

static int32_t kqv_create_surface(KqvApp* app) {
    VkWin32SurfaceCreateInfoKHR create_info;
    VkResult result;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = app->hinstance;
    create_info.hwnd = app->hwnd;
    result = q_vkCreateWin32SurfaceKHR(app->instance, &create_info, NULL, &app->surface);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateWin32SurfaceKHR", result);
    }
    return 0;
}

static int32_t kqv_choose_physical_device(KqvApp* app) {
    VkPhysicalDevice devices[KQV_MAX_PHYSICAL_DEVICES];
    VkQueueFamilyProperties queues[KQV_MAX_QUEUE_FAMILIES];
    uint32_t device_count = 0u;
    uint32_t query_count;
    uint32_t device_index;
    VkResult result = q_vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);
    if (result != VK_SUCCESS || device_count == 0u) {
        return kqv_fail_vk("vkEnumeratePhysicalDevices", result);
    }
    query_count = device_count > KQV_MAX_PHYSICAL_DEVICES ? KQV_MAX_PHYSICAL_DEVICES : device_count;
    result = q_vkEnumeratePhysicalDevices(app->instance, &query_count, devices);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        return kqv_fail_vk("vkEnumeratePhysicalDevices(list)", result);
    }
    for (device_index = 0u; device_index < query_count; device_index += 1u) {
        uint32_t queue_count = 0u;
        uint32_t queue_query_count;
        uint32_t queue_index;
        q_vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &queue_count, NULL);
        queue_query_count = queue_count > KQV_MAX_QUEUE_FAMILIES ? KQV_MAX_QUEUE_FAMILIES : queue_count;
        q_vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &queue_query_count, queues);
        for (queue_index = 0u; queue_index < queue_query_count; queue_index += 1u) {
            VkBool32 present_supported = VK_FALSE;
            q_vkGetPhysicalDeviceSurfaceSupportKHR(devices[device_index], queue_index, app->surface, &present_supported);
            if ((queues[queue_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_supported) {
                app->physical_device = devices[device_index];
                app->queue_family_index = queue_index;
                return 0;
            }
        }
    }
    return kqv_fail_text("physical-device", "no graphics+present queue family was found");
}

static int32_t kqv_create_device(KqvApp* app) {
    float queue_priority = 1.0f;
    const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo create_info;
    VkResult result;
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = app->queue_family_index;
    queue_info.queueCount = 1u;
    queue_info.pQueuePriorities = &queue_priority;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1u;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = 1u;
    create_info.ppEnabledExtensionNames = extensions;

    result = q_vkCreateDevice(app->physical_device, &create_info, NULL, &app->device);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateDevice", result);
    }
    if (kqv_load_device_vulkan(app->device) != 0) {
        return -1;
    }
    q_vkGetDeviceQueue(app->device, app->queue_family_index, 0u, &app->graphics_queue);
    return 0;
}

static VkSurfaceFormatKHR kqv_choose_surface_format(VkSurfaceFormatKHR* formats, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; index += 1u) {
        if (formats[index].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[index];
        }
    }
    return formats[0];
}

static VkPresentModeKHR kqv_choose_present_mode(VkPresentModeKHR* modes, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; index += 1u) {
        if (modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static int32_t kqv_create_swapchain(KqvApp* app) {
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR formats[KQV_MAX_SURFACE_FORMATS];
    VkPresentModeKHR present_modes[KQV_MAX_PRESENT_MODES];
    VkSurfaceFormatKHR chosen_format;
    VkPresentModeKHR present_mode;
    VkSwapchainCreateInfoKHR create_info;
    uint32_t format_count = 0u;
    uint32_t present_mode_count = 0u;
    uint32_t image_count;
    VkResult result;

    result = q_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &caps);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
    }
    result = q_vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0u) {
        return kqv_fail_vk("vkGetPhysicalDeviceSurfaceFormatsKHR(count)", result);
    }
    format_count = format_count > KQV_MAX_SURFACE_FORMATS ? KQV_MAX_SURFACE_FORMATS : format_count;
    result = q_vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        return kqv_fail_vk("vkGetPhysicalDeviceSurfaceFormatsKHR(list)", result);
    }
    result = q_vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &present_mode_count, NULL);
    if (result != VK_SUCCESS || present_mode_count == 0u) {
        return kqv_fail_vk("vkGetPhysicalDeviceSurfacePresentModesKHR(count)", result);
    }
    present_mode_count = present_mode_count > KQV_MAX_PRESENT_MODES ? KQV_MAX_PRESENT_MODES : present_mode_count;
    result = q_vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &present_mode_count, present_modes);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        return kqv_fail_vk("vkGetPhysicalDeviceSurfacePresentModesKHR(list)", result);
    }

    chosen_format = kqv_choose_surface_format(formats, format_count);
    present_mode = kqv_choose_present_mode(present_modes, present_mode_count);
    if (caps.currentExtent.width != UINT32_MAX) {
        app->extent = caps.currentExtent;
    } else {
        app->extent.width = kqv_clamp_u32((uint32_t)app->width, caps.minImageExtent.width, caps.maxImageExtent.width);
        app->extent.height = kqv_clamp_u32((uint32_t)app->height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    image_count = caps.minImageCount + 1u;
    if (caps.maxImageCount > 0u && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }
    if (image_count > KQV_MAX_SWAPCHAIN_IMAGES) {
        image_count = KQV_MAX_SWAPCHAIN_IMAGES;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = app->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = app->extent;
    create_info.imageArrayLayers = 1u;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    result = q_vkCreateSwapchainKHR(app->device, &create_info, NULL, &app->swapchain);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateSwapchainKHR", result);
    }
    app->swapchain_format = chosen_format.format;
    app->image_count = KQV_MAX_SWAPCHAIN_IMAGES;
    result = q_vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        app->image_count = kqv_safe_swapchain_image_count(app);
        return kqv_fail_vk("vkGetSwapchainImagesKHR", result);
    }
    if (app->image_count == 0u || app->image_count > KQV_MAX_SWAPCHAIN_IMAGES) {
        app->image_count = kqv_safe_swapchain_image_count(app);
        return kqv_fail_text("swapchain", "swapchain image count exceeded bridge budget");
    }
    return 0;
}

static int32_t kqv_create_image_views(KqvApp* app) {
    uint32_t index;
    for (index = 0u; index < app->image_count; index += 1u) {
        VkImageViewCreateInfo create_info;
        VkResult result;
        memset(&create_info, 0, sizeof(create_info));
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = app->images[index];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = app->swapchain_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0u;
        create_info.subresourceRange.levelCount = 1u;
        create_info.subresourceRange.baseArrayLayer = 0u;
        create_info.subresourceRange.layerCount = 1u;
        result = q_vkCreateImageView(app->device, &create_info, NULL, &app->image_views[index]);
        if (result != VK_SUCCESS) {
            return kqv_fail_vk("vkCreateImageView", result);
        }
    }
    return 0;
}

static int32_t kqv_create_render_pass(KqvApp* app) {
    VkAttachmentDescription color_attachment;
    VkAttachmentReference color_ref;
    VkSubpassDescription subpass;
    VkSubpassDependency dependency;
    VkRenderPassCreateInfo create_info;
    VkResult result;
    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.format = app->swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    memset(&color_ref, 0, sizeof(color_ref));
    color_ref.attachment = 0u;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1u;
    subpass.pColorAttachments = &color_ref;

    memset(&dependency, 0, sizeof(dependency));
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0u;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1u;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1u;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1u;
    create_info.pDependencies = &dependency;

    result = q_vkCreateRenderPass(app->device, &create_info, NULL, &app->render_pass);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateRenderPass", result);
    }
    return 0;
}

static int32_t kqv_create_shader_module(KqvApp* app, const uint8_t* bytes, size_t size, VkShaderModule* out_module) {
    VkShaderModuleCreateInfo create_info;
    VkResult result;
    if (!bytes || size == 0u || (size % 4u) != 0u || !out_module) {
        return kqv_fail_text("shader-module", "invalid SPIR-V payload");
    }
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = (const uint32_t*)bytes;
    result = q_vkCreateShaderModule(app->device, &create_info, NULL, out_module);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateShaderModule", result);
    }
    return 0;
}

static int32_t kqv_create_pipeline(KqvApp* app) {
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkGraphicsPipelineCreateInfo pipeline_info;
    VkResult result;

    if (kqv_create_shader_module(app, app->vertex_spv, app->vertex_spv_size, &app->vertex_shader) != 0 ||
        kqv_create_shader_module(app, app->fragment_spv, app->fragment_spv_size, &app->fragment_shader) != 0) {
        return -1;
    }

    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = app->vertex_shader;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = app->fragment_shader;
    stages[1].pName = "main";

    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    memset(&input_assembly, 0, sizeof(input_assembly));
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)app->extent.width;
    viewport.height = (float)app->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    memset(&scissor, 0, sizeof(scissor));
    scissor.extent = app->extent;

    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1u;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1u;
    viewport_state.pScissors = &scissor;

    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    memset(&multisample, 0, sizeof(multisample));
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    memset(&color_blend_attachment, 0, sizeof(color_blend_attachment));
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1u;
    color_blend.pAttachments = &color_blend_attachment;

    memset(&push_range, 0, sizeof(push_range));
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_range.offset = 0u;
    push_range.size = sizeof(KqvPushConstants);

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1u;
    layout_info.pPushConstantRanges = &push_range;
    result = q_vkCreatePipelineLayout(app->device, &layout_info, NULL, &app->pipeline_layout);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreatePipelineLayout", result);
    }

    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2u;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.layout = app->pipeline_layout;
    pipeline_info.renderPass = app->render_pass;
    pipeline_info.subpass = 0u;
    pipeline_info.basePipelineIndex = -1;

    result = q_vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1u, &pipeline_info, NULL, &app->pipeline);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateGraphicsPipelines", result);
    }
    return 0;
}

static int32_t kqv_create_framebuffers(KqvApp* app) {
    uint32_t index;
    for (index = 0u; index < app->image_count; index += 1u) {
        VkImageView attachments[] = { app->image_views[index] };
        VkFramebufferCreateInfo create_info;
        VkResult result;
        memset(&create_info, 0, sizeof(create_info));
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = app->render_pass;
        create_info.attachmentCount = 1u;
        create_info.pAttachments = attachments;
        create_info.width = app->extent.width;
        create_info.height = app->extent.height;
        create_info.layers = 1u;
        result = q_vkCreateFramebuffer(app->device, &create_info, NULL, &app->framebuffers[index]);
        if (result != VK_SUCCESS) {
            return kqv_fail_vk("vkCreateFramebuffer", result);
        }
    }
    return 0;
}

static int32_t kqv_create_commands_and_sync(KqvApp* app) {
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo alloc_info;
    VkSemaphoreCreateInfo semaphore_info;
    VkFenceCreateInfo fence_info;
    VkResult result;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = app->queue_family_index;
    result = q_vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateCommandPool", result);
    }

    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = app->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = app->image_count;
    result = q_vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkAllocateCommandBuffers", result);
    }

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = q_vkCreateSemaphore(app->device, &semaphore_info, NULL, &app->image_available);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateSemaphore(image)", result);
    }
    result = q_vkCreateSemaphore(app->device, &semaphore_info, NULL, &app->render_finished);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateSemaphore(render)", result);
    }

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = q_vkCreateFence(app->device, &fence_info, NULL, &app->in_flight);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkCreateFence", result);
    }
    return 0;
}

static int32_t kqv_record_command_buffer(KqvApp* app, uint32_t image_index, int32_t frame_index) {
    VkCommandBuffer command_buffer = app->command_buffers[image_index];
    VkCommandBufferBeginInfo begin_info;
    VkClearValue clear;
    VkRenderPassBeginInfo render_pass_info;
    KqvPushConstants push;
    VkResult result;

    result = q_vkResetCommandBuffer(command_buffer, 0u);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkResetCommandBuffer", result);
    }
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = q_vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkBeginCommandBuffer", result);
    }

    memset(&clear, 0, sizeof(clear));
    clear.color.float32[0] = 0.004f;
    clear.color.float32[1] = 0.000f;
    clear.color.float32[2] = 0.012f;
    clear.color.float32[3] = 1.000f;

    memset(&render_pass_info, 0, sizeof(render_pass_info));
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = app->render_pass;
    render_pass_info.framebuffer = app->framebuffers[image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = app->extent;
    render_pass_info.clearValueCount = 1u;
    render_pass_info.pClearValues = &clear;

    q_vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    q_vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);

    push.time_seconds = (float)frame_index * 0.0166667f;
    push.particle_count = (float)app->particle_count;
    push.mode = app->mode;
    push.chaos = 0.84f;
    q_vkCmdPushConstants(command_buffer, app->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(push), &push);
    q_vkCmdDraw(command_buffer, app->particle_count, 1u, 0u, 0u);
    q_vkCmdEndRenderPass(command_buffer);

    result = q_vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkEndCommandBuffer", result);
    }
    return 0;
}

static int32_t kqv_render_frame(KqvApp* app, int32_t frame_index) {
    uint32_t image_index = 0u;
    VkResult result;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info;
    VkPresentInfoKHR present_info;

    result = q_vkWaitForFences(app->device, 1u, &app->in_flight, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkWaitForFences", result);
    }
    result = q_vkResetFences(app->device, 1u, &app->in_flight);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkResetFences", result);
    }

    result = q_vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->image_available, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        app->closing = 1;
        return 0;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return kqv_fail_vk("vkAcquireNextImageKHR", result);
    }
    if (image_index >= app->image_count) {
        return kqv_fail_text("vkAcquireNextImageKHR", "image index exceeded swapchain budget");
    }
    if (kqv_record_command_buffer(app, image_index, frame_index) != 0) {
        return -1;
    }

    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1u;
    submit_info.pWaitSemaphores = &app->image_available;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &app->command_buffers[image_index];
    submit_info.signalSemaphoreCount = 1u;
    submit_info.pSignalSemaphores = &app->render_finished;

    result = q_vkQueueSubmit(app->graphics_queue, 1u, &submit_info, app->in_flight);
    if (result != VK_SUCCESS) {
        return kqv_fail_vk("vkQueueSubmit", result);
    }

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1u;
    present_info.pWaitSemaphores = &app->render_finished;
    present_info.swapchainCount = 1u;
    present_info.pSwapchains = &app->swapchain;
    present_info.pImageIndices = &image_index;

    result = q_vkQueuePresentKHR(app->graphics_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        app->closing = 1;
        return 0;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return kqv_fail_vk("vkQueuePresentKHR", result);
    }
    g_frames_presented += 1;
    g_particles_drawn += app->particle_count;
    return 0;
}

static int32_t kqv_init_vulkan(KqvApp* app, const char* vertex_spv_path, const char* fragment_spv_path) {
    if (kqv_load_global_vulkan() != 0 ||
        kqv_read_binary_file(vertex_spv_path, &app->vertex_spv, &app->vertex_spv_size) != 0 ||
        kqv_read_binary_file(fragment_spv_path, &app->fragment_spv, &app->fragment_spv_size) != 0 ||
        kqv_create_instance(app) != 0 ||
        kqv_create_surface(app) != 0 ||
        kqv_choose_physical_device(app) != 0 ||
        kqv_create_device(app) != 0 ||
        kqv_create_swapchain(app) != 0 ||
        kqv_create_image_views(app) != 0 ||
        kqv_create_render_pass(app) != 0 ||
        kqv_create_pipeline(app) != 0 ||
        kqv_create_framebuffers(app) != 0 ||
        kqv_create_commands_and_sync(app) != 0) {
        return -1;
    }
    return 0;
}

static void kqv_cleanup(KqvApp* app) {
    uint32_t index;
    uint32_t safe_image_count;
    if (!app) {
        return;
    }
    safe_image_count = kqv_safe_swapchain_image_count(app);
    if (app->device && q_vkDeviceWaitIdle) {
        q_vkDeviceWaitIdle(app->device);
    }
    if (app->device && q_vkDestroyFence && app->in_flight) {
        q_vkDestroyFence(app->device, app->in_flight, NULL);
    }
    if (app->device && q_vkDestroySemaphore && app->render_finished) {
        q_vkDestroySemaphore(app->device, app->render_finished, NULL);
    }
    if (app->device && q_vkDestroySemaphore && app->image_available) {
        q_vkDestroySemaphore(app->device, app->image_available, NULL);
    }
    if (app->device && q_vkDestroyCommandPool && app->command_pool) {
        q_vkDestroyCommandPool(app->device, app->command_pool, NULL);
    }
    if (app->device && q_vkDestroyFramebuffer) {
        for (index = 0u; index < safe_image_count; index += 1u) {
            if (app->framebuffers[index]) {
                q_vkDestroyFramebuffer(app->device, app->framebuffers[index], NULL);
            }
        }
    }
    if (app->device && q_vkDestroyPipeline && app->pipeline) {
        q_vkDestroyPipeline(app->device, app->pipeline, NULL);
    }
    if (app->device && q_vkDestroyPipelineLayout && app->pipeline_layout) {
        q_vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    }
    if (app->device && q_vkDestroyShaderModule && app->fragment_shader) {
        q_vkDestroyShaderModule(app->device, app->fragment_shader, NULL);
    }
    if (app->device && q_vkDestroyShaderModule && app->vertex_shader) {
        q_vkDestroyShaderModule(app->device, app->vertex_shader, NULL);
    }
    if (app->device && q_vkDestroyRenderPass && app->render_pass) {
        q_vkDestroyRenderPass(app->device, app->render_pass, NULL);
    }
    if (app->device && q_vkDestroyImageView) {
        for (index = 0u; index < safe_image_count; index += 1u) {
            if (app->image_views[index]) {
                q_vkDestroyImageView(app->device, app->image_views[index], NULL);
            }
        }
    }
    if (app->device && q_vkDestroySwapchainKHR && app->swapchain) {
        q_vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    }
    if (app->device && q_vkDestroyDevice) {
        q_vkDestroyDevice(app->device, NULL);
    }
    if (app->instance && q_vkDestroySurfaceKHR && app->surface) {
        q_vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    }
    if (app->instance && q_vkDestroyInstance) {
        q_vkDestroyInstance(app->instance, NULL);
    }
    if (app->hwnd && IsWindow(app->hwnd)) {
        DestroyWindow(app->hwnd);
    }
    free(app->vertex_spv);
    free(app->fragment_spv);
}

int32_t kqvulkan_probe(void) {
    if (kqv_load_global_vulkan() != 0) {
        return 0;
    }
    kqv_ok();
    return 1;
}

const char* kqvulkan_backend_name(void) {
    return "vulkan-win32-cffi";
}

const char* kqvulkan_last_error(void) {
    return g_last_error;
}

int64_t kqvulkan_frames_presented(void) {
    return g_frames_presented;
}

int64_t kqvulkan_particles_drawn(void) {
    return g_particles_drawn;
}

int32_t kqvulkan_run_particle_window(
    const char* title,
    int32_t width,
    int32_t height,
    int64_t particle_count,
    int32_t frame_budget,
    int32_t mode,
    const char* vertex_spv_path,
    const char* fragment_spv_path
) {
    KqvApp app;
    int32_t frame_index;
    memset(&app, 0, sizeof(app));
    g_frames_presented = 0;
    g_particles_drawn = 0;
    kqv_ok();

    app.hinstance = GetModuleHandleA(NULL);
    app.width = width > 64 ? width : 64;
    app.height = height > 64 ? height : 64;
    app.particle_count = kqv_particle_budget(particle_count);
    app.mode = mode;

    if (kqv_create_window(&app, title) != 0) {
        kqv_cleanup(&app);
        return -1;
    }
    if (kqv_init_vulkan(&app, vertex_spv_path, fragment_spv_path) != 0) {
        kqv_cleanup(&app);
        return -2;
    }

    if (frame_budget <= 0) {
        frame_budget = 3600;
    }
    for (frame_index = 0; frame_index < frame_budget && !app.closing; frame_index += 1) {
        kqv_pump_window(&app);
        if (app.closing) {
            break;
        }
        if (kqv_render_frame(&app, frame_index) != 0) {
            kqv_cleanup(&app);
            return -3;
        }
        Sleep(1);
    }

    kqv_cleanup(&app);
    kqv_ok();
    return 0;
}

int32_t kqvulkan_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        return kqv_fail_text("vulkan-report", "missing report path");
    }
    file = fopen(path, "wb");
    if (!file) {
        return kqv_fail_text("vulkan-report", path);
    }
    fprintf(file, "KQuantum Vulkan C FFI report\n");
    fprintf(file, "backend=%s\n", kqvulkan_backend_name());
    fprintf(file, "frames=%lld\n", (long long)g_frames_presented);
    fprintf(file, "particles.drawn=%lld\n", (long long)g_particles_drawn);
    fprintf(file, "particle.cap=%u\n", (unsigned)KQV_MAX_PARTICLES);
    fprintf(file, "last_error=%s\n", g_last_error);
    fclose(file);
    return 0;
}
