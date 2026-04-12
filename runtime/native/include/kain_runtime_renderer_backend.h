#ifndef KAIN_RUNTIME_RENDERER_BACKEND_H
#define KAIN_RUNTIME_RENDERER_BACKEND_H

#include <stddef.h>

#define KAIN_RUNTIME_RENDERER_BACKEND_ENV "KAIN_RUNTIME_RENDERER_BACKEND"

typedef enum {
    KAIN_RENDERER_BACKEND_UNKNOWN = 0,
    KAIN_RENDERER_BACKEND_BGFX,
    KAIN_RENDERER_BACKEND_FILAMENT,
    KAIN_RENDERER_BACKEND_DILIGENT,
} KainRendererBackendKind;

typedef struct {
    KainRendererBackendKind kind;
    const char* id;
    const char* display_name;
    const char* vendor_name;
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

#endif /* KAIN_RUNTIME_RENDERER_BACKEND_H */
