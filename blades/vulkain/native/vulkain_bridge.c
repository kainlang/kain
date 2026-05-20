#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if !defined(_WIN32)
#error "vulkain_bridge currently targets the Win32 Vulkan surface path."
#endif

#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>

#include "vulkain_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VKN_MAX_PHYSICAL_DEVICES 16u
#define VKN_MAX_QUEUE_FAMILIES 32u
#define VKN_MAX_SURFACE_FORMATS 32u
#define VKN_MAX_PRESENT_MODES 16u
#define VKN_MAX_SWAPCHAIN_IMAGES 8u
#define VKN_MAX_SHADER_BYTES (16u * 1024u * 1024u)
#define VKN_MAX_TITLE_BYTES 256u
#define VKN_MAX_ENTRY_POINT_BYTES 128u
#define VKN_DEFAULT_DRAW_VERTICES 3u
#define VKN_MAX_DRAW_VERTICES 4096u

typedef struct VulkainPushConstants {
    float time_seconds;
    float accent_r;
    float accent_g;
    float accent_b;
    float camera_yaw;
    float camera_pitch;
    float mesh_scale;
    float mesh_twist;
    float depth_bias;
    float energy;
} VulkainPushConstants;

typedef struct VulkainApp {
    HINSTANCE hinstance;
    HWND hwnd;
    int closing;
    int32_t width;
    int32_t height;
    int32_t frame_budget;
    uint32_t draw_vertices;
    float clear_color[3];
    float accent_color[3];
    float camera_yaw;
    float camera_pitch;
    float mesh_scale;
    float mesh_twist;
    float depth_bias;
    float energy;
    char title[VKN_MAX_TITLE_BYTES];
    char vertex_entry_point[VKN_MAX_ENTRY_POINT_BYTES];
    char fragment_entry_point[VKN_MAX_ENTRY_POINT_BYTES];

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
    VkImage images[VKN_MAX_SWAPCHAIN_IMAGES];
    VkImageView image_views[VKN_MAX_SWAPCHAIN_IMAGES];
    VkFramebuffer framebuffers[VKN_MAX_SWAPCHAIN_IMAGES];

    VkRenderPass render_pass;
    VkShaderModule vertex_shader;
    VkShaderModule fragment_shader;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[VKN_MAX_SWAPCHAIN_IMAGES];
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    uint8_t* vertex_spv;
    size_t vertex_spv_size;
    uint8_t* fragment_spv;
    size_t fragment_spv_size;
} VulkainApp;

static HMODULE g_vulkan_module;
static char g_last_error[768] = "ok";
static char g_last_title[VKN_MAX_TITLE_BYTES] = "Vulkain";
static int32_t g_last_width = 0;
static int32_t g_last_height = 0;
static int32_t g_last_frame_budget = 0;
static int32_t g_last_draw_vertices = 0;
static int32_t g_last_camera_yaw_milli = 0;
static int32_t g_last_camera_pitch_milli = 0;
static int32_t g_last_mesh_scale_milli = 0;
static int32_t g_last_mesh_twist_milli = 0;
static int32_t g_last_depth_bias_milli = 0;
static int32_t g_last_energy = 0;
static char g_last_vertex_entry_point[VKN_MAX_ENTRY_POINT_BYTES] = "main";
static char g_last_fragment_entry_point[VKN_MAX_ENTRY_POINT_BYTES] = "main";
static int64_t g_frames_presented = 0;
static int64_t g_vertices_drawn = 0;

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

static const char* vkn_result_name(VkResult result) {
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

static int32_t vkn_fail_text(const char* stage, const char* message) {
    snprintf(g_last_error, sizeof(g_last_error), "%s: %s", stage, message);
    return -1;
}

static int32_t vkn_fail_vk(const char* stage, VkResult result) {
    snprintf(g_last_error, sizeof(g_last_error), "%s: %s (%d)", stage, vkn_result_name(result), (int)result);
    return -1;
}

static void vkn_ok(void) {
    snprintf(g_last_error, sizeof(g_last_error), "ok");
}

static uint32_t vkn_clamp_dimension(int32_t requested) {
    if (requested < 1) {
        return 1u;
    }
    if (requested > 16384) {
        return 16384u;
    }
    return (uint32_t)requested;
}

static uint32_t vkn_safe_frame_budget(int32_t requested) {
    if (requested < 1) {
        return 1u;
    }
    if (requested > 4096) {
        return 4096u;
    }
    return (uint32_t)requested;
}

static uint32_t vkn_clamp_color_u8(int32_t requested) {
    if (requested < 0) {
        return 0u;
    }
    if (requested > 255) {
        return 255u;
    }
    return (uint32_t)requested;
}

static uint32_t vkn_safe_draw_vertices(int32_t requested) {
    if (requested < 3) {
        return 3u;
    }
    if (requested > (int32_t)VKN_MAX_DRAW_VERTICES) {
        return VKN_MAX_DRAW_VERTICES;
    }
    return (uint32_t)requested;
}

static float vkn_milli_to_float(int32_t requested) {
    return (float)requested / 1000.0f;
}

static uint32_t vkn_safe_swapchain_image_count(const VulkainApp* app) {
    if (!app || app->image_count < 1u) {
        return 1u;
    }
    if (app->image_count > VKN_MAX_SWAPCHAIN_IMAGES) {
        return VKN_MAX_SWAPCHAIN_IMAGES;
    }
    return app->image_count;
}

static void vkn_copy_title(char* dest, size_t dest_cap, const char* src) {
    if (!dest || dest_cap == 0u) {
        return;
    }
    if (!src || !src[0]) {
        snprintf(dest, dest_cap, "Vulkain");
        return;
    }
    strncpy(dest, src, dest_cap - 1u);
    dest[dest_cap - 1u] = '\0';
}

static void vkn_blade_path(char* dest, size_t dest_cap, const char* suffix) {
    const char* root;
    if (!dest || dest_cap == 0u) {
        return;
    }
    root = getenv("VULKAIN_BLADE_ROOT");
    if (root && root[0]) {
        snprintf(dest, dest_cap, "%s/%s", root, suffix);
    } else {
        snprintf(dest, dest_cap, "%s", suffix);
    }
    dest[dest_cap - 1u] = '\0';
}

static int32_t vkn_read_binary_file(const char* path, uint8_t** out_bytes, size_t* out_size) {
    FILE* file;
    long end;
    uint8_t* bytes;
    size_t read_count;
    if (!path || !path[0] || !out_bytes || !out_size) {
        return vkn_fail_text("read-spv", "missing shader path");
    }
    *out_bytes = NULL;
    *out_size = 0u;
    file = fopen(path, "rb");
    if (!file) {
        return vkn_fail_text("read-spv", path);
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return vkn_fail_text("read-spv", "fseek failed");
    }
    end = ftell(file);
    if (end <= 0) {
        fclose(file);
        return vkn_fail_text("read-spv", "empty shader");
    }
    if ((unsigned long)end > (unsigned long)VKN_MAX_SHADER_BYTES) {
        fclose(file);
        return vkn_fail_text("read-spv", "shader too large");
    }
    if ((end & 3L) != 0L) {
        fclose(file);
        return vkn_fail_text("read-spv", "shader byte count must be divisible by 4");
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return vkn_fail_text("read-spv", "fseek rewind failed");
    }
    bytes = (uint8_t*)malloc((size_t)end);
    if (!bytes) {
        fclose(file);
        return vkn_fail_text("read-spv", "malloc failed");
    }
    read_count = fread(bytes, 1u, (size_t)end, file);
    fclose(file);
    if (read_count != (size_t)end) {
        free(bytes);
        return vkn_fail_text("read-spv", "fread failed");
    }
    *out_bytes = bytes;
    *out_size = (size_t)end;
    return 0;
}

static void vkn_free_shader_bytes(VulkainApp* app) {
    if (!app) {
        return;
    }
    if (app->vertex_spv) {
        free(app->vertex_spv);
        app->vertex_spv = NULL;
    }
    if (app->fragment_spv) {
        free(app->fragment_spv);
        app->fragment_spv = NULL;
    }
    app->vertex_spv_size = 0u;
    app->fragment_spv_size = 0u;
}

#define VKN_LOAD_GLOBAL(name) \
    do { \
        q_##name = (PFN_##name)GetProcAddress(g_vulkan_module, #name); \
        if (!q_##name) { \
            return vkn_fail_text("load-global", #name); \
        } \
    } while (0)

#define VKN_LOAD_INSTANCE(instance, name) \
    do { \
        q_##name = (PFN_##name)q_vkGetInstanceProcAddr(instance, #name); \
        if (!q_##name) { \
            return vkn_fail_text("load-instance", #name); \
        } \
    } while (0)

#define VKN_LOAD_DEVICE(device, name) \
    do { \
        q_##name = (PFN_##name)q_vkGetDeviceProcAddr(device, #name); \
        if (!q_##name) { \
            return vkn_fail_text("load-device", #name); \
        } \
    } while (0)

static int32_t vkn_load_vulkan_loader(void) {
    if (!g_vulkan_module) {
        g_vulkan_module = LoadLibraryA("vulkan-1.dll");
        if (!g_vulkan_module) {
            return vkn_fail_text("LoadLibraryA", "vulkan-1.dll");
        }
    }
    VKN_LOAD_GLOBAL(vkGetInstanceProcAddr);
    VKN_LOAD_GLOBAL(vkCreateInstance);
    VKN_LOAD_GLOBAL(vkEnumerateInstanceExtensionProperties);
    q_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)GetProcAddress(g_vulkan_module, "vkGetDeviceProcAddr");
    if (!q_vkGetDeviceProcAddr) {
        return vkn_fail_text("load-global", "vkGetDeviceProcAddr");
    }
    return 0;
}

static int32_t vkn_load_instance_functions(VkInstance instance) {
    VKN_LOAD_INSTANCE(instance, vkDestroyInstance);
    VKN_LOAD_INSTANCE(instance, vkCreateWin32SurfaceKHR);
    VKN_LOAD_INSTANCE(instance, vkDestroySurfaceKHR);
    VKN_LOAD_INSTANCE(instance, vkEnumeratePhysicalDevices);
    VKN_LOAD_INSTANCE(instance, vkGetPhysicalDeviceQueueFamilyProperties);
    VKN_LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfaceSupportKHR);
    VKN_LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    VKN_LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
    VKN_LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
    VKN_LOAD_INSTANCE(instance, vkCreateDevice);
    return 0;
}

static int32_t vkn_load_device_functions(VkDevice device) {
    VKN_LOAD_DEVICE(device, vkDestroyDevice);
    VKN_LOAD_DEVICE(device, vkGetDeviceQueue);
    VKN_LOAD_DEVICE(device, vkCreateSwapchainKHR);
    VKN_LOAD_DEVICE(device, vkDestroySwapchainKHR);
    VKN_LOAD_DEVICE(device, vkGetSwapchainImagesKHR);
    VKN_LOAD_DEVICE(device, vkCreateImageView);
    VKN_LOAD_DEVICE(device, vkDestroyImageView);
    VKN_LOAD_DEVICE(device, vkCreateRenderPass);
    VKN_LOAD_DEVICE(device, vkDestroyRenderPass);
    VKN_LOAD_DEVICE(device, vkCreateShaderModule);
    VKN_LOAD_DEVICE(device, vkDestroyShaderModule);
    VKN_LOAD_DEVICE(device, vkCreatePipelineLayout);
    VKN_LOAD_DEVICE(device, vkDestroyPipelineLayout);
    VKN_LOAD_DEVICE(device, vkCreateGraphicsPipelines);
    VKN_LOAD_DEVICE(device, vkDestroyPipeline);
    VKN_LOAD_DEVICE(device, vkCreateFramebuffer);
    VKN_LOAD_DEVICE(device, vkDestroyFramebuffer);
    VKN_LOAD_DEVICE(device, vkCreateCommandPool);
    VKN_LOAD_DEVICE(device, vkDestroyCommandPool);
    VKN_LOAD_DEVICE(device, vkAllocateCommandBuffers);
    VKN_LOAD_DEVICE(device, vkResetCommandBuffer);
    VKN_LOAD_DEVICE(device, vkBeginCommandBuffer);
    VKN_LOAD_DEVICE(device, vkEndCommandBuffer);
    VKN_LOAD_DEVICE(device, vkCmdBeginRenderPass);
    VKN_LOAD_DEVICE(device, vkCmdEndRenderPass);
    VKN_LOAD_DEVICE(device, vkCmdBindPipeline);
    VKN_LOAD_DEVICE(device, vkCmdPushConstants);
    VKN_LOAD_DEVICE(device, vkCmdDraw);
    VKN_LOAD_DEVICE(device, vkCreateSemaphore);
    VKN_LOAD_DEVICE(device, vkDestroySemaphore);
    VKN_LOAD_DEVICE(device, vkCreateFence);
    VKN_LOAD_DEVICE(device, vkDestroyFence);
    VKN_LOAD_DEVICE(device, vkWaitForFences);
    VKN_LOAD_DEVICE(device, vkResetFences);
    VKN_LOAD_DEVICE(device, vkAcquireNextImageKHR);
    VKN_LOAD_DEVICE(device, vkQueueSubmit);
    VKN_LOAD_DEVICE(device, vkQueuePresentKHR);
    VKN_LOAD_DEVICE(device, vkDeviceWaitIdle);
    return 0;
}

static LRESULT CALLBACK vkn_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    VulkainApp* app = (VulkainApp*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (message) {
        case WM_NCCREATE: {
            const CREATESTRUCTA* create = (const CREATESTRUCTA*)lparam;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
            return DefWindowProcA(hwnd, message, wparam, lparam);
        }
        case WM_CLOSE:
            if (app) {
                app->closing = 1;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (app) {
                app->closing = 1;
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

static int32_t vkn_create_window(VulkainApp* app, const char* title) {
    WNDCLASSEXA wc;
    RECT rect;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const char* class_name = "VulkainRawWindow";
    if (!app) {
        return vkn_fail_text("window", "missing app");
    }
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = vkn_window_proc;
    wc.hInstance = app->hinstance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassExA(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return vkn_fail_text("RegisterClassExA", "window class registration failed");
        }
    }
    rect.left = 0;
    rect.top = 0;
    rect.right = (LONG)app->width;
    rect.bottom = (LONG)app->height;
    AdjustWindowRect(&rect, style, FALSE);
    app->hwnd = CreateWindowExA(
        0,
        class_name,
        title && title[0] ? title : "Vulkain",
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
        return vkn_fail_text("CreateWindowExA", "window creation failed");
    }
    ShowWindow(app->hwnd, SW_SHOW);
    UpdateWindow(app->hwnd);
    return 0;
}

static void vkn_pump_messages(VulkainApp* app) {
    MSG msg;
    (void)app;
    while (PeekMessageA(&msg, NULL, 0u, 0u, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static uint32_t vkn_select_queue_family(VulkainApp* app) {
    uint32_t count = 0u;
    VkQueueFamilyProperties props[VKN_MAX_QUEUE_FAMILIES];
    uint32_t index;
    q_vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &count, NULL);
    if (count > VKN_MAX_QUEUE_FAMILIES) {
        count = VKN_MAX_QUEUE_FAMILIES;
    }
    q_vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &count, props);
    for (index = 0u; index < count; ++index) {
        VkBool32 present_supported = VK_FALSE;
        q_vkGetPhysicalDeviceSurfaceSupportKHR(app->physical_device, index, app->surface, &present_supported);
        if ((props[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_supported == VK_TRUE) {
            return index;
        }
    }
    return UINT32_MAX;
}

static VkSurfaceFormatKHR vkn_select_surface_format(const VkSurfaceFormatKHR* formats, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; ++index) {
        if (formats[index].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[index];
        }
    }
    return formats[0u];
}

static VkPresentModeKHR vkn_select_present_mode(const VkPresentModeKHR* present_modes, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; ++index) {
        if (present_modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_modes[index];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static int32_t vkn_create_instance(VulkainApp* app) {
    const char* extensions[2];
    VkApplicationInfo app_info;
    VkInstanceCreateInfo create_info;
    VkResult result;
    extensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
    extensions[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app->title;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "vulkain";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = 2u;
    create_info.ppEnabledExtensionNames = extensions;
    result = q_vkCreateInstance(&create_info, NULL, &app->instance);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateInstance", result);
    }
    return vkn_load_instance_functions(app->instance);
}

static int32_t vkn_create_surface(VulkainApp* app) {
    VkWin32SurfaceCreateInfoKHR create_info;
    VkResult result;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = app->hinstance;
    create_info.hwnd = app->hwnd;
    result = q_vkCreateWin32SurfaceKHR(app->instance, &create_info, NULL, &app->surface);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateWin32SurfaceKHR", result);
    }
    return 0;
}

static int32_t vkn_pick_physical_device(VulkainApp* app) {
    VkPhysicalDevice devices[VKN_MAX_PHYSICAL_DEVICES];
    uint32_t count = 0u;
    uint32_t index;
    VkResult result = q_vkEnumeratePhysicalDevices(app->instance, &count, NULL);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkEnumeratePhysicalDevices", result);
    }
    if (count == 0u) {
        return vkn_fail_text("vkEnumeratePhysicalDevices", "no Vulkan physical devices");
    }
    if (count > VKN_MAX_PHYSICAL_DEVICES) {
        count = VKN_MAX_PHYSICAL_DEVICES;
    }
    result = q_vkEnumeratePhysicalDevices(app->instance, &count, devices);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkEnumeratePhysicalDevices", result);
    }
    for (index = 0u; index < count; ++index) {
        app->physical_device = devices[index];
        app->queue_family_index = vkn_select_queue_family(app);
        if (app->queue_family_index != UINT32_MAX) {
            return 0;
        }
    }
    return vkn_fail_text("physical-device", "no graphics+present queue family");
}

static int32_t vkn_create_device(VulkainApp* app) {
    const char* extensions[1];
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo create_info;
    VkResult result;
    extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = app->queue_family_index;
    queue_info.queueCount = 1u;
    queue_info.pQueuePriorities = &priority;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1u;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = 1u;
    create_info.ppEnabledExtensionNames = extensions;
    result = q_vkCreateDevice(app->physical_device, &create_info, NULL, &app->device);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateDevice", result);
    }
    if (vkn_load_device_functions(app->device) != 0) {
        return -1;
    }
    q_vkGetDeviceQueue(app->device, app->queue_family_index, 0u, &app->graphics_queue);
    return 0;
}

static int32_t vkn_create_swapchain(VulkainApp* app) {
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR formats[VKN_MAX_SURFACE_FORMATS];
    VkPresentModeKHR present_modes[VKN_MAX_PRESENT_MODES];
    VkSurfaceFormatKHR chosen_format;
    VkSwapchainCreateInfoKHR create_info;
    VkExtent2D extent;
    uint32_t format_count = 0u;
    uint32_t present_mode_count = 0u;
    uint32_t desired_image_count;
    VkPresentModeKHR chosen_present_mode;
    VkResult result;
    uint32_t index;

    result = q_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &caps);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
    }

    result = q_vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0u) {
        return vkn_fail_text("surface-formats", "missing surface formats");
    }
    if (format_count > VKN_MAX_SURFACE_FORMATS) {
        format_count = VKN_MAX_SURFACE_FORMATS;
    }
    result = q_vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkGetPhysicalDeviceSurfaceFormatsKHR", result);
    }

    result = q_vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &present_mode_count, NULL);
    if (result != VK_SUCCESS || present_mode_count == 0u) {
        return vkn_fail_text("present-modes", "missing present modes");
    }
    if (present_mode_count > VKN_MAX_PRESENT_MODES) {
        present_mode_count = VKN_MAX_PRESENT_MODES;
    }
    result = q_vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &present_mode_count, present_modes);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkGetPhysicalDeviceSurfacePresentModesKHR", result);
    }

    chosen_format = vkn_select_surface_format(formats, format_count);
    chosen_present_mode = vkn_select_present_mode(present_modes, present_mode_count);
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = vkn_clamp_dimension(app->width);
        extent.height = vkn_clamp_dimension(app->height);
    }

    desired_image_count = caps.minImageCount + 1u;
    if (caps.maxImageCount > 0u && desired_image_count > caps.maxImageCount) {
        desired_image_count = caps.maxImageCount;
    }
    if (desired_image_count < 1u) {
        desired_image_count = 1u;
    }
    if (desired_image_count > VKN_MAX_SWAPCHAIN_IMAGES) {
        desired_image_count = VKN_MAX_SWAPCHAIN_IMAGES;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = app->surface;
    create_info.minImageCount = desired_image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1u;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = chosen_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    result = q_vkCreateSwapchainKHR(app->device, &create_info, NULL, &app->swapchain);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateSwapchainKHR", result);
    }

    app->swapchain_format = chosen_format.format;
    app->extent = extent;
    app->image_count = VKN_MAX_SWAPCHAIN_IMAGES;
    result = q_vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkGetSwapchainImagesKHR", result);
    }
    if (app->image_count < 1u || app->image_count > VKN_MAX_SWAPCHAIN_IMAGES) {
        app->image_count = vkn_safe_swapchain_image_count(app);
        return vkn_fail_text("swapchain", "swapchain image count exceeded bridge budget");
    }

    for (index = 0u; index < app->image_count; ++index) {
        VkImageViewCreateInfo view_info;
        memset(&view_info, 0, sizeof(view_info));
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = app->images[index];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = app->swapchain_format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0u;
        view_info.subresourceRange.levelCount = 1u;
        view_info.subresourceRange.baseArrayLayer = 0u;
        view_info.subresourceRange.layerCount = 1u;
        result = q_vkCreateImageView(app->device, &view_info, NULL, &app->image_views[index]);
        if (result != VK_SUCCESS) {
            return vkn_fail_vk("vkCreateImageView", result);
        }
    }
    return 0;
}

static int32_t vkn_create_render_pass(VulkainApp* app) {
    VkAttachmentDescription color_attachment;
    VkAttachmentReference color_reference;
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

    memset(&color_reference, 0, sizeof(color_reference));
    color_reference.attachment = 0u;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1u;
    subpass.pColorAttachments = &color_reference;

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
        return vkn_fail_vk("vkCreateRenderPass", result);
    }
    return 0;
}

static int32_t vkn_create_shader_modules(VulkainApp* app) {
    VkShaderModuleCreateInfo vertex_info;
    VkShaderModuleCreateInfo fragment_info;
    VkResult result;
    memset(&vertex_info, 0, sizeof(vertex_info));
    vertex_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_info.codeSize = app->vertex_spv_size;
    vertex_info.pCode = (const uint32_t*)app->vertex_spv;
    result = q_vkCreateShaderModule(app->device, &vertex_info, NULL, &app->vertex_shader);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateShaderModule(vertex)", result);
    }

    memset(&fragment_info, 0, sizeof(fragment_info));
    fragment_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_info.codeSize = app->fragment_spv_size;
    fragment_info.pCode = (const uint32_t*)app->fragment_spv;
    result = q_vkCreateShaderModule(app->device, &fragment_info, NULL, &app->fragment_shader);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateShaderModule(fragment)", result);
    }
    return 0;
}

static int32_t vkn_create_pipeline(VulkainApp* app) {
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

    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = app->vertex_shader;
    stages[0].pName = app->vertex_entry_point[0] ? app->vertex_entry_point : "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = app->fragment_shader;
    stages[1].pName = app->fragment_entry_point[0] ? app->fragment_entry_point : "main";

    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    memset(&input_assembly, 0, sizeof(input_assembly));
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)app->extent.width;
    viewport.height = (float)app->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = app->extent;

    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1u;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1u;
    viewport_state.pScissors = &scissor;

    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    memset(&multisample, 0, sizeof(multisample));
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample.sampleShadingEnable = VK_FALSE;

    memset(&color_blend_attachment, 0, sizeof(color_blend_attachment));
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = VK_FALSE;
    color_blend.attachmentCount = 1u;
    color_blend.pAttachments = &color_blend_attachment;

    memset(&push_range, 0, sizeof(push_range));
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0u;
    push_range.size = sizeof(VulkainPushConstants);

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1u;
    layout_info.pPushConstantRanges = &push_range;
    result = q_vkCreatePipelineLayout(app->device, &layout_info, NULL, &app->pipeline_layout);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreatePipelineLayout", result);
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
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    result = q_vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1u, &pipeline_info, NULL, &app->pipeline);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateGraphicsPipelines", result);
    }
    return 0;
}

static int32_t vkn_create_framebuffers(VulkainApp* app) {
    uint32_t index;
    for (index = 0u; index < app->image_count; ++index) {
        VkImageView attachments[1];
        VkFramebufferCreateInfo framebuffer_info;
        VkResult result;
        attachments[0] = app->image_views[index];
        memset(&framebuffer_info, 0, sizeof(framebuffer_info));
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = app->render_pass;
        framebuffer_info.attachmentCount = 1u;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = app->extent.width;
        framebuffer_info.height = app->extent.height;
        framebuffer_info.layers = 1u;
        result = q_vkCreateFramebuffer(app->device, &framebuffer_info, NULL, &app->framebuffers[index]);
        if (result != VK_SUCCESS) {
            return vkn_fail_vk("vkCreateFramebuffer", result);
        }
    }
    return 0;
}

static int32_t vkn_create_command_pool_and_buffers(VulkainApp* app) {
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo alloc_info;
    VkResult result;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = app->queue_family_index;
    result = q_vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateCommandPool", result);
    }

    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = app->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = app->image_count;
    result = q_vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkAllocateCommandBuffers", result);
    }
    return 0;
}

static int32_t vkn_create_sync_objects(VulkainApp* app) {
    VkSemaphoreCreateInfo semaphore_info;
    VkFenceCreateInfo fence_info;
    VkResult result;
    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = q_vkCreateSemaphore(app->device, &semaphore_info, NULL, &app->image_available);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateSemaphore(image_available)", result);
    }
    result = q_vkCreateSemaphore(app->device, &semaphore_info, NULL, &app->render_finished);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateSemaphore(render_finished)", result);
    }

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = q_vkCreateFence(app->device, &fence_info, NULL, &app->in_flight);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkCreateFence", result);
    }
    return 0;
}

static int32_t vkn_record_command_buffer(VulkainApp* app, uint32_t image_index, uint32_t frame_index) {
    VkCommandBuffer command_buffer = app->command_buffers[image_index];
    VkCommandBufferBeginInfo begin_info;
    VkClearValue clear_value;
    VkRenderPassBeginInfo render_pass_begin;
    VulkainPushConstants push;
    VkResult result;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = q_vkResetCommandBuffer(command_buffer, 0u);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkResetCommandBuffer", result);
    }
    result = q_vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkBeginCommandBuffer", result);
    }

    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.color.float32[0] = app->clear_color[0];
    clear_value.color.float32[1] = app->clear_color[1];
    clear_value.color.float32[2] = app->clear_color[2];
    clear_value.color.float32[3] = 1.0f;

    memset(&render_pass_begin, 0, sizeof(render_pass_begin));
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.renderPass = app->render_pass;
    render_pass_begin.framebuffer = app->framebuffers[image_index];
    render_pass_begin.renderArea.offset.x = 0;
    render_pass_begin.renderArea.offset.y = 0;
    render_pass_begin.renderArea.extent = app->extent;
    render_pass_begin.clearValueCount = 1u;
    render_pass_begin.pClearValues = &clear_value;

    q_vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    q_vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);

    push.time_seconds = (float)frame_index * 0.016666667f;
    push.accent_r = app->accent_color[0];
    push.accent_g = app->accent_color[1];
    push.accent_b = app->accent_color[2];
    push.camera_yaw = app->camera_yaw;
    push.camera_pitch = app->camera_pitch;
    push.mesh_scale = app->mesh_scale;
    push.mesh_twist = app->mesh_twist;
    push.depth_bias = app->depth_bias;
    push.energy = app->energy;
    q_vkCmdPushConstants(
        command_buffer,
        app->pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0u,
        sizeof(push),
        &push
    );
    q_vkCmdDraw(command_buffer, app->draw_vertices, 1u, 0u, 0u);
    q_vkCmdEndRenderPass(command_buffer);

    result = q_vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkEndCommandBuffer", result);
    }
    return 0;
}

static int32_t vkn_draw_frame(VulkainApp* app, uint32_t frame_index) {
    uint32_t image_index = 0u;
    VkSubmitInfo submit_info;
    VkPresentInfoKHR present_info;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkResult result;
    result = q_vkWaitForFences(app->device, 1u, &app->in_flight, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkWaitForFences", result);
    }
    result = q_vkResetFences(app->device, 1u, &app->in_flight);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkResetFences", result);
    }

    result = q_vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->image_available, VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkAcquireNextImageKHR", result);
    }
    if (image_index >= app->image_count) {
        return vkn_fail_text("vkAcquireNextImageKHR", "image index exceeded swapchain budget");
    }
    if (vkn_record_command_buffer(app, image_index, frame_index) != 0) {
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
        return vkn_fail_vk("vkQueueSubmit", result);
    }

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1u;
    present_info.pWaitSemaphores = &app->render_finished;
    present_info.swapchainCount = 1u;
    present_info.pSwapchains = &app->swapchain;
    present_info.pImageIndices = &image_index;
    result = q_vkQueuePresentKHR(app->graphics_queue, &present_info);
    if (result != VK_SUCCESS) {
        return vkn_fail_vk("vkQueuePresentKHR", result);
    }

    g_frames_presented += 1;
    g_vertices_drawn += (int64_t)app->draw_vertices;
    return 0;
}

static int32_t vkn_init_vulkan(VulkainApp* app) {
    if (vkn_create_instance(app) != 0) {
        return -1;
    }
    if (vkn_create_surface(app) != 0) {
        return -1;
    }
    if (vkn_pick_physical_device(app) != 0) {
        return -1;
    }
    if (vkn_create_device(app) != 0) {
        return -1;
    }
    if (vkn_create_swapchain(app) != 0) {
        return -1;
    }
    if (vkn_create_render_pass(app) != 0) {
        return -1;
    }
    if (vkn_create_shader_modules(app) != 0) {
        return -1;
    }
    if (vkn_create_pipeline(app) != 0) {
        return -1;
    }
    if (vkn_create_framebuffers(app) != 0) {
        return -1;
    }
    if (vkn_create_command_pool_and_buffers(app) != 0) {
        return -1;
    }
    if (vkn_create_sync_objects(app) != 0) {
        return -1;
    }
    return 0;
}

static void vkn_shutdown(VulkainApp* app) {
    uint32_t index;
    if (!app) {
        return;
    }
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
        for (index = 0u; index < vkn_safe_swapchain_image_count(app); ++index) {
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
        for (index = 0u; index < vkn_safe_swapchain_image_count(app); ++index) {
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
    if (app->hwnd) {
        DestroyWindow(app->hwnd);
        app->hwnd = NULL;
    }
    vkn_free_shader_bytes(app);
}

int32_t vulkain_native_probe(void) {
    uint32_t count = 0u;
    if (vkn_load_vulkan_loader() != 0) {
        return 0;
    }
    if (q_vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS) {
        return 0;
    }
    vkn_ok();
    return 1;
}

int64_t vulkain_native_frames_presented(void) {
    return g_frames_presented;
}

int64_t vulkain_native_vertices_drawn(void) {
    return g_vertices_drawn;
}

static int32_t vkn_run_scene(
    const char* title,
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    int32_t draw_vertices,
    int32_t camera_yaw_milli,
    int32_t camera_pitch_milli,
    int32_t mesh_scale_milli,
    int32_t mesh_twist_milli,
    int32_t depth_bias_milli,
    int32_t energy,
    const char* vertex_spv_path,
    const char* fragment_spv_path,
    const char* vertex_entry_point,
    const char* fragment_entry_point
) {
    VulkainApp app;
    uint32_t frame_index = 0u;
    int32_t status = 0;
    memset(&app, 0, sizeof(app));
    vkn_ok();
    if (vkn_load_vulkan_loader() != 0) {
        return -1;
    }
    if (vkn_read_binary_file(vertex_spv_path, &app.vertex_spv, &app.vertex_spv_size) != 0) {
        return -1;
    }
    if (vkn_read_binary_file(fragment_spv_path, &app.fragment_spv, &app.fragment_spv_size) != 0) {
        vkn_free_shader_bytes(&app);
        return -1;
    }
    app.hinstance = GetModuleHandleA(NULL);
    app.width = (int32_t)vkn_clamp_dimension(width);
    app.height = (int32_t)vkn_clamp_dimension(height);
    app.frame_budget = (int32_t)vkn_safe_frame_budget(frame_budget);
    app.draw_vertices = vkn_safe_draw_vertices(draw_vertices);
    app.clear_color[0] = (float)vkn_clamp_color_u8(clear_red) / 255.0f;
    app.clear_color[1] = (float)vkn_clamp_color_u8(clear_green) / 255.0f;
    app.clear_color[2] = (float)vkn_clamp_color_u8(clear_blue) / 255.0f;
    app.accent_color[0] = (float)vkn_clamp_color_u8(accent_red) / 255.0f;
    app.accent_color[1] = (float)vkn_clamp_color_u8(accent_green) / 255.0f;
    app.accent_color[2] = (float)vkn_clamp_color_u8(accent_blue) / 255.0f;
    app.camera_yaw = vkn_milli_to_float(camera_yaw_milli);
    app.camera_pitch = vkn_milli_to_float(camera_pitch_milli);
    app.mesh_scale = vkn_milli_to_float(mesh_scale_milli);
    app.mesh_twist = vkn_milli_to_float(mesh_twist_milli);
    app.depth_bias = vkn_milli_to_float(depth_bias_milli);
    app.energy = vkn_milli_to_float(energy);
    vkn_copy_title(app.title, sizeof(app.title), title);
    vkn_copy_title(
        app.vertex_entry_point,
        sizeof(app.vertex_entry_point),
        vertex_entry_point && vertex_entry_point[0] ? vertex_entry_point : "main"
    );
    vkn_copy_title(
        app.fragment_entry_point,
        sizeof(app.fragment_entry_point),
        fragment_entry_point && fragment_entry_point[0] ? fragment_entry_point : "main"
    );
    vkn_copy_title(g_last_title, sizeof(g_last_title), app.title);
    vkn_copy_title(
        g_last_vertex_entry_point,
        sizeof(g_last_vertex_entry_point),
        app.vertex_entry_point
    );
    vkn_copy_title(
        g_last_fragment_entry_point,
        sizeof(g_last_fragment_entry_point),
        app.fragment_entry_point
    );
    g_last_width = app.width;
    g_last_height = app.height;
    g_last_frame_budget = app.frame_budget;
    g_last_draw_vertices = (int32_t)app.draw_vertices;
    g_last_camera_yaw_milli = camera_yaw_milli;
    g_last_camera_pitch_milli = camera_pitch_milli;
    g_last_mesh_scale_milli = mesh_scale_milli;
    g_last_mesh_twist_milli = mesh_twist_milli;
    g_last_depth_bias_milli = depth_bias_milli;
    g_last_energy = energy;
    g_frames_presented = 0;
    g_vertices_drawn = 0;

    if (vkn_create_window(&app, app.title) != 0) {
        vkn_shutdown(&app);
        return -1;
    }
    if (vkn_init_vulkan(&app) != 0) {
        vkn_shutdown(&app);
        return -1;
    }

    while (!app.closing && frame_index < (uint32_t)app.frame_budget) {
        vkn_pump_messages(&app);
        if (app.closing) {
            break;
        }
        status = vkn_draw_frame(&app, frame_index);
        if (status != 0) {
            break;
        }
        frame_index += 1u;
        Sleep(16u);
    }

    vkn_shutdown(&app);
    return status;
}

int32_t vulkain_native_run_window(
    const char* title,
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    const char* vertex_spv_path,
    const char* fragment_spv_path,
    const char* vertex_entry_point,
    const char* fragment_entry_point
) {
    return vkn_run_scene(
        title,
        width,
        height,
        frame_budget,
        clear_red,
        clear_green,
        clear_blue,
        accent_red,
        accent_green,
        accent_blue,
        (int32_t)VKN_DEFAULT_DRAW_VERTICES,
        0,
        0,
        1000,
        0,
        0,
        1000,
        vertex_spv_path,
        fragment_spv_path,
        vertex_entry_point,
        fragment_entry_point
    );
}

int32_t vulkain_native_run_mesh_scene(
    const char* title,
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    int32_t draw_vertices,
    int32_t camera_yaw_milli,
    int32_t camera_pitch_milli,
    int32_t mesh_scale_milli,
    int32_t mesh_twist_milli,
    int32_t depth_bias_milli,
    int32_t energy,
    const char* vertex_spv_path,
    const char* fragment_spv_path,
    const char* vertex_entry_point,
    const char* fragment_entry_point
) {
    return vkn_run_scene(
        title,
        width,
        height,
        frame_budget,
        clear_red,
        clear_green,
        clear_blue,
        accent_red,
        accent_green,
        accent_blue,
        draw_vertices,
        camera_yaw_milli,
        camera_pitch_milli,
        mesh_scale_milli,
        mesh_twist_milli,
        depth_bias_milli,
        energy,
        vertex_spv_path,
        fragment_spv_path,
        vertex_entry_point,
        fragment_entry_point
    );
}

int32_t vulkain_native_run_authored_mesh_scene(
    int32_t width,
    int32_t height,
    int32_t frame_budget,
    int32_t clear_red,
    int32_t clear_green,
    int32_t clear_blue,
    int32_t accent_red,
    int32_t accent_green,
    int32_t accent_blue,
    int32_t draw_vertices,
    int32_t camera_yaw_milli,
    int32_t camera_pitch_milli,
    int32_t mesh_scale_milli,
    int32_t mesh_twist_milli,
    int32_t depth_bias_milli,
    int32_t energy
) {
    char vertex_spv_path[1024];
    char fragment_spv_path[1024];
    vkn_blade_path(vertex_spv_path, sizeof(vertex_spv_path), ".kain/gpu/basic_window/vulkain_basic.vert.spv");
    vkn_blade_path(fragment_spv_path, sizeof(fragment_spv_path), ".kain/gpu/basic_window/vulkain_basic.frag.spv");
    return vkn_run_scene(
        "Vulkain // Kain Authored 3D Mesh",
        width,
        height,
        frame_budget,
        clear_red,
        clear_green,
        clear_blue,
        accent_red,
        accent_green,
        accent_blue,
        draw_vertices,
        camera_yaw_milli,
        camera_pitch_milli,
        mesh_scale_milli,
        mesh_twist_milli,
        depth_bias_milli,
        energy,
        vertex_spv_path,
        fragment_spv_path,
        "main",
        "main"
    );
}

int32_t vulkain_native_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        return vkn_fail_text("vulkain-report", "missing report path");
    }
    file = fopen(path, "wb");
    if (!file) {
        return vkn_fail_text("vulkain-report", path);
    }
    fprintf(file, "library=vulkain_bridge\n");
    fprintf(file, "backend=vulkan\n");
    fprintf(file, "title=%s\n", g_last_title);
    fprintf(file, "window_width=%d\n", g_last_width);
    fprintf(file, "window_height=%d\n", g_last_height);
    fprintf(file, "frame_budget=%d\n", g_last_frame_budget);
    fprintf(file, "draw_vertices=%d\n", g_last_draw_vertices);
    fprintf(file, "camera_yaw_milli=%d\n", g_last_camera_yaw_milli);
    fprintf(file, "camera_pitch_milli=%d\n", g_last_camera_pitch_milli);
    fprintf(file, "mesh_scale_milli=%d\n", g_last_mesh_scale_milli);
    fprintf(file, "mesh_twist_milli=%d\n", g_last_mesh_twist_milli);
    fprintf(file, "depth_bias_milli=%d\n", g_last_depth_bias_milli);
    fprintf(file, "energy=%d\n", g_last_energy);
    fprintf(file, "vertex_entry_point=%s\n", g_last_vertex_entry_point);
    fprintf(file, "fragment_entry_point=%s\n", g_last_fragment_entry_point);
    fprintf(file, "frames_presented=%lld\n", (long long)g_frames_presented);
    fprintf(file, "vertices_drawn=%lld\n", (long long)g_vertices_drawn);
    fprintf(file, "last_error=%s\n", g_last_error);
    fclose(file);
    return 0;
}

int32_t vulkain_native_write_default_mesh_report(void) {
    return vulkain_native_write_report(".kain/run/vulkain_mesh_scene_report.txt");
}
