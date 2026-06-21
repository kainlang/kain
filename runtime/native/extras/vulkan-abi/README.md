# libkain-vulkan-abi — Vulkan ABI Shared Library

## Overview

`libkain-vulkan-abi.so` (Linux/macOS) / `libkain-vulkan-abi.dll` (Windows) is
the separately-linked Vulkan ABI library. It owns ALL actual Vulkan driver
calls: instance creation, physical device selection, logical device creation,
WSI surface creation, swapchain lifecycle, frame submission, and present.

The runtime shim (`vulkan_surface_shim.c`) dlopens this library and calls the
single entry point `kain_vulkan_abi_get_vtable()` to obtain a filled
`KainComponentSurface` vtable implementing all 18 surface trait slots.

## Entry Points

| Symbol | Purpose |
|--------|---------|
| `kain_vulkan_abi_get_vtable()` | Returns pointer to static `KainVulkanAbiVtable` with filled vtable |
| `kain_vulkan_abi_init()` | Initialize session storage (optional, called by blade-level control) |
| `kain_vulkan_abi_shutdown()` | Destroy all sessions, close Vulkan loader handle |

## Architecture

```
vulkan_surface_shim.c (runtime contract)
    │ dlopen("libkain-vulkan-abi.so")
    │ dlsym("kain_vulkan_abi_get_vtable")
    ▼
vulkan_abi.c (this library — implementation)
    │ dlopen("vulkan-1.dll" / "libvulkan.so.1" / "libMoltenVK.dylib")
    │ 43 PFNs resolved via vkGetInstanceProcAddr
    │ KainComponentSurface vtable filled with real Vulkan calls
    ▼
Vulkan driver (vendor ICD)
```

## Supported Platforms

| Platform | Surface Extension | Native Handle |
|----------|-------------------|---------------|
| Windows | `VK_KHR_win32_surface` | `HINSTANCE` + `HWND` |
| Linux (X11) | `VK_KHR_xlib_surface` | `Display*` + `Window` |
| Linux (Wayland) | `VK_KHR_wayland_surface` | `wl_display*` + `wl_surface*` |
| macOS | `VK_MVK_macos_surface` | `CAMetalLayer*` (via MoltenVK) |

## Critical Rules

- **NEVER** includes `<vulkan/vulkan.h>`
- **NEVER** links the Vulkan SDK at compile time
- All Vulkan types are `uintptr_t` (from `vulkan_loader_subset.h`)
- All `Vk*CreateInfo` structs are built with hardcoded sType values
- PFN resolution is split: instance-level PFNs after `vkCreateInstance`,
  device-level PFNs after `vkCreateDevice`

## Vtable Slots (18 of 18 filled)

All 18 `KainComponentSurface` vtable slots are implemented:
`session_create`, `session_destroy`, `session_attach_platform`,
`element_begin`, `element_end`, `element_set_text`,
`element_set_attr_i64`, `element_set_attr_f64`, `element_set_attr_string`,
`state_get_i64`, `state_set_i64`, `begin_frame`, `end_frame`, `present`,
`poll_event`, `should_close`, `window_open`, `host_pump`.

## Build

```powershell
bazel build //runtime/extras/vulkan-abi:kain_vulkan_abi --config=dev
```

```bash
bazel build //runtime/extras/vulkan-abi:kain_vulkan_abi --config=dev
```

Output: `bazel-bin/runtime/extras/vulkan-abi/libkain-vulkan-abi.so` (or `.dll`)

## Implementation Sections (~2,050 lines total)

| Section | Lines | Description |
|---------|-------|-------------|
| 0: Structures | ~350 | Hardcoded Vulkan struct definitions (no headers) |
| 1: Dynamic loader | ~180 | dlopen/LoadLibrary, 43 PFN typedefs + resolution |
| 2: WSI surfaces | ~100 | Per-platform surface creation (#ifdef matrix) |
| 3: Physical device | ~80 | Enumerate, prefer discrete GPU, queue family query |
| 4: Logical device | ~80 | vkCreateDevice with VK_KHR_swapchain |
| 5: Swapchain | ~200 | Extent negotiation, present mode, image views, recreation |
| 6: Frame submission | ~120 | MAX_FRAMES_IN_FLIGHT=2 ring buffer, fences, semaphores |
| 7: Vtable fill | ~250 | All 18 slot implementations |
| 8: Error handling | ~40 | VkResult → string table (29 entries) |
| 9: Static vtable | ~40 | Global vtable instance + public entry points |
| _Implementation logic_ | ~610 | Boot sequence, session management, etc. |

## PFNs Resolved: 43

vkGetInstanceProcAddr, vkGetDeviceProcAddr, vkEnumerateInstanceVersion,
vkEnumerateInstanceExtensionProperties, vkEnumerateInstanceLayerProperties,
vkCreateInstance, vkDestroyInstance, vkEnumeratePhysicalDevices,
vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceFeatures,
vkGetPhysicalDeviceQueueFamilyProperties, vkCreateDevice, vkDestroyDevice,
vkGetDeviceQueue, vkDeviceWaitIdle, vkCreateWin32SurfaceKHR,
vkCreateXlibSurfaceKHR, vkCreateWaylandSurfaceKHR, vkCreateMacOSSurfaceMVK,
vkDestroySurfaceKHR, vkGetPhysicalDeviceSurfaceSupportKHR,
vkGetPhysicalDeviceSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceFormatsKHR,
vkGetPhysicalDeviceSurfacePresentModesKHR, vkCreateSwapchainKHR,
vkDestroySwapchainKHR, vkGetSwapchainImagesKHR, vkAcquireNextImageKHR,
vkQueuePresentKHR, vkCreateCommandPool, vkDestroyCommandPool,
vkAllocateCommandBuffers, vkBeginCommandBuffer, vkEndCommandBuffer,
vkQueueSubmit, vkCreateSemaphore, vkDestroySemaphore, vkCreateFence,
vkDestroyFence, vkWaitForFences, vkResetFences, vkCreateImageView,
vkDestroyImageView.
