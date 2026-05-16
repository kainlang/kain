#ifndef ENTANGLE_H
#define ENTANGLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTANGLE_MAX_BINDINGS 128
#define ENTANGLE_MAX_PATH 256
#define ENTANGLE_MAX_POLICY 64
#define ENTANGLE_MAX_TYPE 128

typedef struct KainRuntimeEntangleBinding {
    char authority[ENTANGLE_MAX_PATH];
    char mirror[ENTANGLE_MAX_PATH];
    char policy[ENTANGLE_MAX_POLICY];
    char type_name[ENTANGLE_MAX_TYPE];
} KainRuntimeEntangleBinding;

void entangle_registry_reset(void);
size_t entangle_registry_count(void);
int entangle_registry_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
);
int entangle_registry_get(size_t index, KainRuntimeEntangleBinding* out_binding);

#ifdef __cplusplus
}
#endif

#endif /* ENTANGLE_H */
