#include "../../include/renderer_backend.h"
#include "../../include/services.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define KAIN_RENDERER_TEXT_EQUALS_CI _stricmp
#else
#include <strings.h>
#define KAIN_RENDERER_TEXT_EQUALS_CI strcasecmp
#endif

static const KainRendererBackendDescriptor g_kain_renderer_backend_catalog[] = {
    {
        KAIN_RENDERER_BACKEND_VULKAN,
        "vulkan",
        "Vulkan Backend Target",
        "runtime-native",
        KAIN_SERVICE_KEY_GFX_BACKEND_VULKAN,
        "Runtime-owned Vulkan backend identity and capability target; concrete presenters live outside the C runtime",
        1
    },
    {
        KAIN_RENDERER_BACKEND_D3D12,
        "d3d12",
        "DirectX 12 Backend Target",
        "runtime-native",
        KAIN_SERVICE_KEY_GFX_BACKEND_D3D12,
        "Runtime-owned DirectX 12 backend identity and capability target; concrete presenters live outside the C runtime",
#ifdef _WIN32
        1
#else
        0
#endif
    },
    {
        KAIN_RENDERER_BACKEND_WEBGPU,
        "webgpu",
        "WebGPU Backend Target",
        "runtime-native",
        KAIN_SERVICE_KEY_GFX_BACKEND_WEBGPU,
        "Runtime-owned WebGPU backend identity and capability target; concrete presenters live outside the C runtime",
        1
    },
};

const KainRendererBackendDescriptor* kain_renderer_backend_catalog(void) {
    return g_kain_renderer_backend_catalog;
}

size_t kain_renderer_backend_count(void) {
    return sizeof(g_kain_renderer_backend_catalog) /
           sizeof(g_kain_renderer_backend_catalog[0]);
}

const KainRendererBackendDescriptor* kain_renderer_backend_at(size_t index) {
    if (index >= kain_renderer_backend_count()) {
        return NULL;
    }
    return &g_kain_renderer_backend_catalog[index];
}

const KainRendererBackendDescriptor* kain_renderer_backend_lookup(const char* id) {
    size_t index;

    if (!id || !id[0]) {
        return NULL;
    }

    for (index = 0; index < kain_renderer_backend_count(); ++index) {
        const KainRendererBackendDescriptor* descriptor =
            &g_kain_renderer_backend_catalog[index];
        if (KAIN_RENDERER_TEXT_EQUALS_CI(descriptor->id, id) == 0) {
            return descriptor;
        }
    }

    return NULL;
}

const KainRendererBackendDescriptor* kain_renderer_backend_default(void) {
    size_t index;

    for (index = 0; index < kain_renderer_backend_count(); ++index) {
        const KainRendererBackendDescriptor* descriptor =
            &g_kain_renderer_backend_catalog[index];
        if (descriptor->available) {
            return descriptor;
        }
    }

    return &g_kain_renderer_backend_catalog[0];
}

const KainRendererBackendDescriptor* kain_renderer_backend_active(void) {
    const char* requested_backend =
        getenv(RENDERER_BACKEND_ENV);
    const KainRendererBackendDescriptor* descriptor =
        kain_renderer_backend_lookup(requested_backend);

    if (descriptor) {
        return descriptor;
    }

    return kain_renderer_backend_default();
}
