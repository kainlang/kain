#include "../../include/entangle.h"

#include <string.h>

static KainRuntimeEntangleBinding g_kain_entangle_bindings[ENTANGLE_MAX_BINDINGS];
static size_t g_kain_entangle_binding_count = 0;

static int runtime_copy_entangle_text(char* dst, size_t dst_cap, const char* src) {
    if (dst == 0 || dst_cap == 0 || src == 0 || src[0] == '\0') {
        return -1;
    }

    size_t len = strlen(src);
    if (len >= dst_cap) {
        return -2;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

void entangle_registry_reset(void) {
    memset(g_kain_entangle_bindings, 0, sizeof(g_kain_entangle_bindings));
    g_kain_entangle_binding_count = 0;
}

size_t entangle_registry_count(void) {
    return g_kain_entangle_binding_count;
}

int entangle_registry_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
) {
    if (g_kain_entangle_binding_count >= ENTANGLE_MAX_BINDINGS) {
        return -3;
    }

    KainRuntimeEntangleBinding binding;
    memset(&binding, 0, sizeof(binding));

    int status = runtime_copy_entangle_text(
        binding.authority,
        sizeof(binding.authority),
        authority
    );
    if (status != 0) {
        return status;
    }

    status = runtime_copy_entangle_text(binding.mirror, sizeof(binding.mirror), mirror);
    if (status != 0) {
        return status;
    }

    status = runtime_copy_entangle_text(binding.policy, sizeof(binding.policy), policy);
    if (status != 0) {
        return status;
    }

    status = runtime_copy_entangle_text(
        binding.type_name,
        sizeof(binding.type_name),
        type_name
    );
    if (status != 0) {
        return status;
    }

    g_kain_entangle_bindings[g_kain_entangle_binding_count] = binding;
    g_kain_entangle_binding_count += 1;
    return 0;
}

int entangle_registry_get(size_t index, KainRuntimeEntangleBinding* out_binding) {
    if (out_binding == 0 || index >= g_kain_entangle_binding_count) {
        return -1;
    }

    *out_binding = g_kain_entangle_bindings[index];
    return 0;
}
