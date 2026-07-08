// vulkan_init.c — Vulkan initialization and device setup
// Instance creation, physical device selection (discrete GPU
// preferred), logical device with graphics/queue families,
// swapchain setup (surface format, present mode, extent),
// and pipeline cache loading. Separated from render_vulkan.c
// to keep init/present/teardown concerns decoupled.
#include "../../internal.h"
#include "../../kaintana.h"
#include <vulkan/vulkan.h>
