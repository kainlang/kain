#include "../../include/kain_runtime_renderer_backend.h"
#include "../../include/kain_runtime_services.h"
#include "../../include/kain_runtime_vendor_lane.h"

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
        KAIN_RENDERER_BACKEND_BGFX,
        "bgfx",
        "bgfx Runtime",
        "bgfx",
        KAIN_SERVICE_KEY_GFX_BACKEND_BGFX,
        "Cross-platform baseline renderer backend for viewport, swapchain, and debug-draw work",
        KAIN_VENDOR_HAS_BGFX
    },
    {
        KAIN_RENDERER_BACKEND_FILAMENT,
        "filament",
        "Filament Renderer",
        "filament-core",
        KAIN_SERVICE_KEY_GFX_BACKEND_FILAMENT,
        "Premium scene, lighting, and material presentation lane",
        KAIN_VENDOR_HAS_FILAMENT
    },
    {
        KAIN_RENDERER_BACKEND_DILIGENT,
        "diligent",
        "Diligent Renderer",
        "diligentcore",
        KAIN_SERVICE_KEY_GFX_BACKEND_DILIGENT,
        "Explicit render-graph, compute, and pipeline-control lane",
        KAIN_VENDOR_HAS_DILIGENT
    },
    {
        KAIN_RENDERER_BACKEND_FORGE,
        "forge",
        "The Forge Renderer",
        "the-forge",
        KAIN_SERVICE_KEY_GFX_BACKEND_FORGE,
        "Low-level cross-platform renderer substrate staged for a future Kain-owned backend lane",
        KAIN_VENDOR_HAS_FORGE
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
        getenv(KAIN_RUNTIME_RENDERER_BACKEND_ENV);
    const KainRendererBackendDescriptor* descriptor =
        kain_renderer_backend_lookup(requested_backend);

    if (descriptor) {
        return descriptor;
    }

    return kain_renderer_backend_default();
}
