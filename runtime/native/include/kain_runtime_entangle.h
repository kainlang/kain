#ifndef KAIN_RUNTIME_ENTANGLE_H
#define KAIN_RUNTIME_ENTANGLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_RUNTIME_ENTANGLE_MAX_BINDINGS 128
#define KAIN_RUNTIME_ENTANGLE_MAX_PATH 256
#define KAIN_RUNTIME_ENTANGLE_MAX_POLICY 64
#define KAIN_RUNTIME_ENTANGLE_MAX_TYPE 128

typedef struct KainRuntimeEntangleBinding {
    char authority[KAIN_RUNTIME_ENTANGLE_MAX_PATH];
    char mirror[KAIN_RUNTIME_ENTANGLE_MAX_PATH];
    char policy[KAIN_RUNTIME_ENTANGLE_MAX_POLICY];
    char type_name[KAIN_RUNTIME_ENTANGLE_MAX_TYPE];
} KainRuntimeEntangleBinding;

void kain_runtime_entangle_registry_reset(void);
size_t kain_runtime_entangle_registered_count(void);
int kain_runtime_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
);
int kain_runtime_entangle_get(size_t index, KainRuntimeEntangleBinding* out_binding);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RUNTIME_ENTANGLE_H */
