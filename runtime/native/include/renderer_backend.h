#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include <stddef.h>

#define RENDERER_BACKEND_ENV "RENDERER_BACKEND"

typedef enum {
    KAIN_RENDERER_BACKEND_UNKNOWN = 0,
    KAIN_RENDERER_BACKEND_VULKAN,
    KAIN_RENDERER_BACKEND_D3D12,
} KainRendererBackendKind;

typedef struct {
    KainRendererBackendKind kind;
    const char* id;
    const char* display_name;
    const char* runtime_name;
    const char* service_key;
    const char* summary;
    int available;
} KainRendererBackendDescriptor;

const KainRendererBackendDescriptor* kain_renderer_backend_catalog(void);
size_t kain_renderer_backend_count(void);
const KainRendererBackendDescriptor* kain_renderer_backend_at(size_t index);
const KainRendererBackendDescriptor* kain_renderer_backend_lookup(const char* id);
const KainRendererBackendDescriptor* kain_renderer_backend_default(void);
const KainRendererBackendDescriptor* kain_renderer_backend_active(void);

#endif /* RENDERER_BACKEND_H */
