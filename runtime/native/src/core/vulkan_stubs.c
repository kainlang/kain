// ============================================================================
//  vulkan_stubs.c — Kain-runtime-resident stubs that proxy Vulkan ABI calls
//  through the already-loaded vtable from vulkan_surface_shim.c.
// ============================================================================
//  Kain @extern declarations for kain_vulkan_abi_load_shader and
//  kain_vulkan_abi_set_uniform emit LLVM `declare` statements, but these
//  symbols only exist in libkain-vulkan-abi.so/.dll (a separately-linked
//  shared library). The build system does not link the ABI library into
//  Kain programs, so the symbols are unresolved at link time.
//
//  This file defines those symbols as stubs that proxy through the vtable's
//  function pointers. The vtable is set up by vulkan_surface_shim.c at
//  dlopen time and is always available when the Vulkan backend is active.
//
//  If the Vulkan surface has not been resolved (g_vulkan_vtable == NULL),
//  the stubs return -1 gracefully rather than crashing.
// ============================================================================

#ifdef KAIN_RUNTIME_HAS_VULKAN_LOADER

#include "../../extras/vulkan-abi/vulkan_abi.h"

extern const KainVulkanAbiVtable* g_vulkan_vtable;

int64_t kain_vulkan_abi_load_shader(int64_t session_id, const char* spirv_hex) {
    if (g_vulkan_vtable == NULL) return -1;
    return g_vulkan_vtable->load_shader_fn
        ? g_vulkan_vtable->load_shader_fn(session_id, spirv_hex)
        : -2;
}

int64_t kain_vulkan_abi_set_uniform(int64_t session_id, int64_t binding,
                                     const void* data, int64_t size) {
    if (g_vulkan_vtable == NULL) return -1;
    return g_vulkan_vtable->set_uniform_fn
        ? g_vulkan_vtable->set_uniform_fn(session_id, (uint32_t)binding,
                                           data, (uint64_t)size)
        : -2;
}

#else /* !KAIN_RUNTIME_HAS_VULKAN_LOADER */

int64_t kain_vulkan_abi_load_shader(int64_t session_id, const char* spirv_hex) {
    (void)session_id;
    (void)spirv_hex;
    return -1;
}

int64_t kain_vulkan_abi_set_uniform(int64_t session_id, int64_t binding,
                                     const void* data, int64_t size) {
    (void)session_id;
    (void)binding;
    (void)data;
    (void)size;
    return -1;
}

#endif /* KAIN_RUNTIME_HAS_VULKAN_LOADER */
