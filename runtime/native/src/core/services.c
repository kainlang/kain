#include "../../include/services.h"
#include "../../include/base.h"
#include "../../include/net_system.h"
#include "../../include/process_system.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#ifndef _WIN32
#include <sched.h>
#include <strings.h>
#endif

#ifdef _WIN32
#define ABI_NET_SERVICE_STATUS KAIN_SERVICE_STATUS_AVAILABLE
#define ABI_PROCESS_SERVICE_STATUS KAIN_SERVICE_STATUS_AVAILABLE
#else
#define ABI_NET_SERVICE_STATUS KAIN_SERVICE_STATUS_AVAILABLE
#define ABI_PROCESS_SERVICE_STATUS KAIN_SERVICE_STATUS_DEGRADED
#endif

typedef struct {
    const char* alias_key;
    const char* canonical_key;
} KainServiceKeyAlias;

static void kain_copy_text(char* out, size_t out_size, const char* text) {
    size_t length;

    if (!out || out_size == 0) {
        return;
    }

    if (!text) {
        out[0] = '\0';
        return;
    }

    length = strlen(text);
    /* Proof: runtime/native/src/core/z3/proofs/native-services-copy-text-fits-destination-before-null-write.yaml */
    if (length >= out_size) {
        length = out_size - 1;
    }

    memcpy(out, text, length);
    out[length] = '\0';
}

static int kain_text_equals_ci(const char* left, const char* right) {
    if (!left || !right) {
        return 0;
    }
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcasecmp(left, right) == 0;
#endif
}

static uint64_t kain_service_rotate_left_u64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t kain_service_magic_prefix_state(
    uint64_t word0,
    uint64_t word1,
    uint64_t word2,
    uint64_t word3,
    uint64_t length
) {
    const uint64_t magic = 0x64170d358aa115a1ULL;
    const uint64_t lane1 = 0x9e3779b97f4a7c15ULL;
    const uint64_t lane2 = 0xbf58476d1ce4e5b9ULL;
    const uint64_t lane3 = 0x94d049bb133111ebULL;
    const uint64_t lane4 = 0xd6e8feb86659fd93ULL;
    uint64_t folded0 = (word0 ^ length) * magic;
    uint64_t folded1 = (word1 ^ kain_service_rotate_left_u64(magic, 13u)) * lane1;
    uint64_t folded2 = (word2 ^ kain_service_rotate_left_u64(magic, 27u)) * lane2;
    uint64_t folded3 = (word3 ^ (magic ^ lane3)) * lane4;
    uint64_t state = folded0 ^ folded1 ^ folded2 ^ folded3;
    return ((state ^ (state >> 33u)) * 0xff51afd7ed558ccdULL) ^ (state >> 29u);
}

static void kain_service_key_metadata_ascii_lower(
    const char* key,
    size_t* out_length,
    uint64_t* out_state
) {
    size_t key_length = 0u;
    size_t prefix_length = 0u;
    uint64_t prefix_words[4] = {0u, 0u, 0u, 0u};
    unsigned char folded_prefix[32] = {0u};
    size_t i;

    if (key) {
        key_length = strlen(key);
        prefix_length = key_length < 32u ? key_length : 32u;
        for (i = 0u; i < prefix_length; ++i) {
            unsigned char byte = (unsigned char)key[i];
            if (byte >= 'A' && byte <= 'Z') {
                byte = (unsigned char)(byte + ('a' - 'A'));
            }
            folded_prefix[i] = byte;
        }
        if (prefix_length > 0u) {
            memcpy(prefix_words, folded_prefix, prefix_length);
        }
    }

    if (out_length) {
        *out_length = key_length;
    }
    if (out_state) {
        *out_state = kain_service_magic_prefix_state(
            prefix_words[0],
            prefix_words[1],
            prefix_words[2],
            prefix_words[3],
            (uint64_t)key_length
        );
    }
}

static void kain_service_descriptor_refresh_key_metadata(
    KainServiceDescriptor* descriptor
) {
    if (!descriptor) {
        return;
    }

    kain_service_key_metadata_ascii_lower(
        descriptor->key,
        &descriptor->key_length,
        &descriptor->key_state
    );
}

static int kain_service_descriptor_matches_lookup(
    const KainServiceDescriptor* descriptor,
    const char* key,
    size_t key_length,
    uint64_t key_state
) {
    if (!descriptor || !key) {
        return 0;
    }
    if (descriptor->key_length != key_length || descriptor->key_state != key_state) {
        return 0;
    }
    return kain_text_equals_ci(descriptor->key, key);
}

static void kain_service_descriptor_copy(
    KainServiceDescriptor* destination,
    const KainServiceDescriptor* source
) {
    if (!destination || !source) {
        return;
    }

    ZeroMemory(destination, sizeof(*destination));
    kain_copy_text(destination->key, sizeof(destination->key), source->key);
    kain_copy_text(destination->name, sizeof(destination->name), source->name);
    kain_copy_text(
        destination->description,
        sizeof(destination->description),
        source->description
    );
    destination->provider = source->provider;
    destination->status = source->status;
    destination->requirement = source->requirement;
    destination->abi_version = source->abi_version;
    destination->function_table = source->function_table;
    kain_service_descriptor_refresh_key_metadata(destination);
}

static void kain_service_registry_spin_pause(unsigned int spin_index) {
#ifdef _WIN32
    if ((spin_index & 63u) == 63u) {
        SwitchToThread();
    } else {
        YieldProcessor();
    }
#else
    if ((spin_index & 63u) == 63u) {
        sched_yield();
    } else {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#endif
    }
#endif
}

static int kain_service_registry_is_initialized(
    const KainServiceRegistry* registry
) {
    return registry != NULL &&
           atomic_load_explicit(&registry->initialized, memory_order_acquire) != 0u;
}

static int kain_service_registry_count_load(
    const KainServiceRegistry* registry
) {
    if (registry == NULL) {
        return 0;
    }
    return atomic_load_explicit(&registry->service_count, memory_order_acquire);
}

static void kain_service_registry_lock(KainServiceRegistry* registry) {
    unsigned int spin_index = 0u;

    if (registry == NULL) {
        return;
    }

    for (;;) {
        unsigned int expected = 0u;
        if (atomic_compare_exchange_weak_explicit(
                &registry->mutation_gate,
                &expected,
                1u,
                memory_order_acquire,
                memory_order_relaxed
            )) {
            return;
        }
        kain_service_registry_spin_pause(spin_index++);
    }
}

static void kain_service_registry_unlock(KainServiceRegistry* registry) {
    if (registry == NULL) {
        return;
    }
    atomic_store_explicit(&registry->mutation_gate, 0u, memory_order_release);
}

static void kain_service_registry_ensure_initialized(
    KainServiceRegistry* registry
) {
    if (registry == NULL || kain_service_registry_is_initialized(registry)) {
        return;
    }

    kain_service_registry_lock(registry);
    if (!kain_service_registry_is_initialized(registry)) {
        int i;
        memset(registry->services, 0, sizeof(registry->services));
        for (i = 0; i < 256; ++i) {
            registry->hash_to_service[i] = -1;
        }
        atomic_store_explicit(&registry->service_count, 0, memory_order_relaxed);
        atomic_store_explicit(&registry->initialized, 1u, memory_order_release);
    }
    kain_service_registry_unlock(registry);
}

static KainServiceDescriptor* kain_service_registry_find_mutable_unlocked(
    KainServiceRegistry* registry,
    const char* canonical_key
) {
    size_t canonical_key_length;
    uint64_t canonical_key_state;
    int service_count;
    int i;

    if (!registry || !canonical_key) {
        return NULL;
    }

    kain_service_key_metadata_ascii_lower(
        canonical_key,
        &canonical_key_length,
        &canonical_key_state
    );

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; ++i) {
        if (kain_service_descriptor_matches_lookup(
                &registry->services[i],
                canonical_key,
                canonical_key_length,
                canonical_key_state
            )) {
            return &registry->services[i];
        }
    }

    return NULL;
}

/* Proof: runtime/native/src/core/z3/proofs/native-services-commit-gate-prevents-slot-overwrite.yaml */
static int kain_service_registry_commit_descriptor_unlocked(
    KainServiceRegistry* registry,
    const KainServiceDescriptor* source,
    int allow_refresh
) {
    KainServiceDescriptor* existing;
    KainServiceDescriptor* destination;
    int service_count;

    if (!registry || !source || !source->key[0]) {
        return -1;
    }

    existing = kain_service_registry_find_mutable_unlocked(registry, source->key);
    if (existing != NULL) {
        if (!allow_refresh) {
            return -3;
        }
        kain_service_descriptor_copy(existing, source);
        return 0;
    }

    service_count = kain_service_registry_count_load(registry);
    if (service_count >= KAIN_SERVICE_REGISTRY_MAX_SERVICES) {
        return -2;
    }

    destination = &registry->services[service_count];
    kain_service_descriptor_copy(destination, source);
    /*
     * Populate perfect-hash lookup for this service.
     * The Z3 proof shows all 31 active services have collision-free top-8-bit
     * hashes, so we don't need to detect collisions here.  The hash table is
     * built under the mutation_gate spinlock; readers see it via the release
     * store on service_count.
     */
    {
        uint8_t slot = (uint8_t)(destination->key_state >> 56);
        registry->hash_to_service[slot] = (int16_t)service_count;
    }
    atomic_store_explicit(
        &registry->service_count,
        service_count + 1,
        memory_order_release
    );
    return 0;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
static const KainServiceDescriptor g_kain_native_runtime_service_catalog[] = {
    {
        KAIN_SERVICE_KEY_BASE_MEMORY,
        "Base Memory Services",
        "Core allocation, retain/release, and memory management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_MEMORY_OWNERSHIP,
        "Ownership State Guards",
        "Native collapse/observe/decay guards for helper-owned heap regions and imported pointer lifetimes",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_BASE_DIAGNOSTICS,
        "Base Diagnostics",
        "Structured diagnostics and error reporting",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_CONTRACT,
        "Runtime Contract",
        "Runtime contract bundle loading and validation",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_PLATFORM_APP_HOST,
        "Native App Host",
        "Raw Win32 app/window host substrate without baked presenters or app policy",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_PLATFORM_INPUT,
        "Native Input",
        "Canonical Kain input sessions, semantic actions, replay traces, and native platform event handling",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_VIEWPORT,
        "Native Viewport",
        "Platform window handles and presenter attachment contract; concrete presenters now live in blades or packages",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_RAW_NATIVE,
        "Raw Native Graphics",
        "Catalog-free graphics kernel for Kain-authored engines, buffers, SPIR-V modules, pipelines, and draw commands",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_SHADER_SPIRV,
        "SPIR-V Shader Modules",
        "Canonical native shader payload registration for Kain-authored graphics and compute pipelines",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_VULKAN,
        "Vulkan Backend Target",
        "Runtime-owned Vulkan backend identity and capability target with surface shim + ABI library; concrete presenters live outside the C runtime",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_D3D12,
        "DirectX 12 Backend Target",
        "Runtime-owned DirectX 12 backend identity and capability target with surface shim + ABI library; concrete presenters live outside the C runtime",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_WEBGPU,
        "WebGPU Backend Target",
        "Runtime-owned WebGPU backend identity and capability target with surface shim + ABI library; concrete presenters live outside the C runtime",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_RUNTIME,
        "Scene Runtime",
        "Stable native scene handles and runtime-owned scene state access",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_QUERY,
        "Scene Query",
        "Picking, raycast, bounds, visibility, and selection query contracts",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_MUTATION,
        "Scene Mutation",
        "Transactional scene mutation requests and receipts",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_GLTF,
        "glTF Asset Loader",
        "The legacy runtime-owned glTF loader was removed from the active runtime; authored packages or blade-owned loaders must satisfy this seam",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_INGESTION,
        "Asset Ingestion",
        "Canonical descriptor-driven entry path for assets and emitted bundles",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_REALTIME,
        "Realtime Bundle Loader",
        "Realtime bundle loading and scene management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_UI_BUNDLE,
        "Compiled UI Bundle",
        "Compiled UI bundle loading and overlay rendering",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_REFLECTION,
        "Reflection Runtime",
        "Reflection payload loading and runtime type lookup",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_RUNTIME_INSPECTION,
        "Runtime Inspection",
        "Runtime-owned scene, resource, and binding inspection queries",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_DEVICE_REFLECTION,
        "Device Reflection",
        "Backend, GPU, display, and hotplug capability descriptors",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ACTOR_RUNTIME,
        "Actor Runtime",
        "Actor spawn, mailbox, lifecycle, and scheduling",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ACTOR_REGISTRY,
        "Actor Registry",
        "Named actor registration and lookup",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASYNC_RUNTIME,
        "Async Runtime",
        "Task and future execution with wake/poll handling",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASYNC_TIMERS,
        "Async Timers",
        "Timer registration, wake delivery, and async sleep support",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_IO_NET,
        "IO Network",
        "Native TCP, protocol-aware HTTP client/server, capability-query, and Windows-first HTTPS/HTTP2 client primitives",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        ABI_NET_SERVICE_STATUS,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_native_net_function_table
    },
    {
        KAIN_SERVICE_KEY_IO_PROCESS,
        "IO Process",
        "Native child-process, pipe, and PTY session management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        ABI_PROCESS_SERVICE_STATUS,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_native_process_function_table
    },
    {
        KAIN_SERVICE_KEY_GFX_COMPUTE,
        "Compute Runtime",
        "Compute bundle validation, dispatch planning, and native runtime handoff",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_UI_COMPONENT,
        "UI Component Runtime",
        "Component state, invalidation, focus, and event routing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_COMPATIBILITY,
        "Compatibility and Hot Reload",
        "Version validation, migration, hot reload, and snapshot flow",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_HOST_BRIDGE,
        "Host Bridge",
        "Plugin and foreign service integration",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* Global service registry singleton */
static KainServiceRegistry g_service_registry = {0};

const char* kain_service_registry_canonicalize_key(const char* key) {
    size_t key_length;
    uint64_t key_state;

    if (!key || !key[0]) {
        return key;
    }

    kain_service_key_metadata_ascii_lower(key, &key_length, &key_state);

    switch (key_state) {
        case 0xe967a2e7a5088d07ULL:
            if (key_length == 15u && kain_text_equals_ci(key, "native.app-host")) {
                return KAIN_SERVICE_KEY_PLATFORM_APP_HOST;
            }
            break;
        case 0x1c9e242eb4645378ULL:
            if (key_length == 12u && kain_text_equals_ci(key, "native.input")) {
                return KAIN_SERVICE_KEY_PLATFORM_INPUT;
            }
            break;
        case 0x8140fe9573cec064ULL:
            if (key_length == 15u && kain_text_equals_ci(key, "native.viewport")) {
                return KAIN_SERVICE_KEY_GFX_VIEWPORT;
            }
            break;
        case 0x52b4f4dbb3337bfbULL:
            if (key_length == 15u && kain_text_equals_ci(key, "native.graphics")) {
                return KAIN_SERVICE_KEY_GFX_RAW_NATIVE;
            }
            break;
        case 0x9b6bbed0fbf8a1ddULL:
            if (key_length == 12u && kain_text_equals_ci(key, "native.scene")) {
                return KAIN_SERVICE_KEY_SCENE_RUNTIME;
            }
            break;
        case 0xcccf3d4aaed22219ULL:
            if (key_length == 18u && kain_text_equals_ci(key, "native.scene.query")) {
                return KAIN_SERVICE_KEY_SCENE_QUERY;
            }
            break;
        case 0xf26120689e22a9e2ULL:
            if (key_length == 21u && kain_text_equals_ci(key, "native.scene.mutation")) {
                return KAIN_SERVICE_KEY_SCENE_MUTATION;
            }
            break;
        case 0xf42f6791bc7ef2bdULL:
            if (key_length == 25u && kain_text_equals_ci(key, "native.runtime.inspection")) {
                return KAIN_SERVICE_KEY_RUNTIME_INSPECTION;
            }
            break;
        case 0x7a425942690ea4d7ULL:
            if (key_length == 24u && kain_text_equals_ci(key, "native.device.reflection")) {
                return KAIN_SERVICE_KEY_DEVICE_REFLECTION;
            }
            break;
        case 0x5b2990da90ab1f38ULL:
            if (key_length == 17u && kain_text_equals_ci(key, "native.asset.gltf")) {
                return KAIN_SERVICE_KEY_ASSET_GLTF;
            }
            break;
        case 0x403bc9addf0d3a57ULL:
            if (key_length == 22u && kain_text_equals_ci(key, "native.asset.ingestion")) {
                return KAIN_SERVICE_KEY_ASSET_INGESTION;
            }
            break;
        case 0xe764215896fc05bbULL:
            if (key_length == 26u && kain_text_equals_ci(key, "native.ui.compiled-bundle")) {
                return KAIN_SERVICE_KEY_UI_BUNDLE;
            }
            break;
        case 0x83303d876aa8e678ULL:
            if (key_length == 14u && kain_text_equals_ci(key, "native.compute")) {
                return KAIN_SERVICE_KEY_GFX_COMPUTE;
            }
            break;
        case 0x25be923470113a81ULL:
            if (key_length == 20u && kain_text_equals_ci(key, "native.shader.spirv")) {
                return KAIN_SERVICE_KEY_GFX_SHADER_SPIRV;
            }
            break;
        case 0x0d2f647f2745c670ULL:
            if (key_length == 13u && kain_text_equals_ci(key, "native.vulkan")) {
                return KAIN_SERVICE_KEY_GFX_BACKEND_VULKAN;
            }
            break;
        case 0x249604c6dc88fc47ULL:
            if (key_length == 11u && kain_text_equals_ci(key, "native.dx12")) {
                return KAIN_SERVICE_KEY_GFX_BACKEND_D3D12;
            }
            break;
        case 0x5a3a87a1ea23aab6ULL:
            if (key_length == 12u && kain_text_equals_ci(key, "native.d3d12")) {
                return KAIN_SERVICE_KEY_GFX_BACKEND_D3D12;
            }
            break;
        case 0x0c640679c3e22ec0ULL:
            if (key_length == 13u && kain_text_equals_ci(key, "native.webgpu")) {
                return KAIN_SERVICE_KEY_GFX_BACKEND_WEBGPU;
            }
            break;
        default:
            break;
    }

    return key;
}

void kain_service_registry_init(KainServiceRegistry* registry) {
    if (!registry) {
        return;
    }
    ZeroMemory(registry, sizeof(*registry));
    atomic_init(&registry->initialized, 1u);
    atomic_init(&registry->mutation_gate, 0u);
    atomic_init(&registry->service_count, 0);
}

static KainServiceDescriptor* kain_service_registry_lookup_mutable(
    KainServiceRegistry* registry,
    const char* key
) {
    return kain_service_registry_find_mutable_unlocked(
        registry,
        kain_service_registry_canonicalize_key(key)
    );
}

int kain_service_registry_register(
    KainServiceRegistry* registry,
    const char* key,
    const char* name,
    const char* description,
    KainServiceProvider provider,
    KainServiceStatus status,
    KainServiceRequirement requirement,
    unsigned int abi_version,
    void* function_table
) {
    const char* canonical_key;
    KainServiceDescriptor source;
    int result;

    if (!registry || !key || !name) {
        return -1;
    }

    canonical_key = kain_service_registry_canonicalize_key(key);
    if (!canonical_key || !canonical_key[0]) {
        return -1;
    }

    kain_service_registry_ensure_initialized(registry);

    memset(&source, 0, sizeof(source));
    kain_copy_text(source.key, sizeof(source.key), canonical_key);
    kain_copy_text(source.name, sizeof(source.name), name);
    kain_copy_text(source.description, sizeof(source.description), description);
    source.provider = provider;
    source.status = status;
    source.requirement = requirement;
    source.abi_version = abi_version;
    source.function_table = function_table;
    kain_service_descriptor_refresh_key_metadata(&source);

    if (kain_service_registry_lookup(registry, canonical_key) != NULL) {
        return -3;
    }

    kain_service_registry_lock(registry);
    result = kain_service_registry_commit_descriptor_unlocked(
        registry,
        &source,
        0
    );
    kain_service_registry_unlock(registry);
    return result;
}

int kain_service_registry_register_descriptor(
    KainServiceRegistry* registry,
    const KainServiceDescriptor* descriptor
) {
    if (!descriptor) {
        return -1;
    }

    return kain_service_registry_register(
        registry,
        descriptor->key,
        descriptor->name,
        descriptor->description,
        descriptor->provider,
        descriptor->status,
        descriptor->requirement,
        descriptor->abi_version,
        descriptor->function_table
    );
}

const KainServiceDescriptor* kain_service_registry_lookup(
    const KainServiceRegistry* registry,
    const char* key
) {
    int i;
    const char* canonical_key = kain_service_registry_canonicalize_key(key);
    size_t canonical_key_length;
    uint64_t canonical_key_state;
    int service_count;
    uint8_t slot;
    int16_t hash_index;

    if (!registry || !canonical_key) {
        return NULL;
    }

    kain_service_key_metadata_ascii_lower(
        canonical_key,
        &canonical_key_length,
        &canonical_key_state
    );

    /*
     * Direct-mapped perfect hash: top 8 bits of the already-computed key_state.
     * No multiply needed — just a shift from the 64-bit hash.
     * Proof: runtime/native/src/core/z3/proofs/native-services-perfect-hash-top-eight-bits.yaml
     *
     * The hash_to_service table is populated under the mutation_gate spinlock
     * during registration.  The acquire-load on service_count guarantees
     * visibility of the hash table writes.
     */
    slot = (uint8_t)(canonical_key_state >> 56);
    hash_index = registry->hash_to_service[slot];
    if (hash_index >= 0) {
        const KainServiceDescriptor* candidate = &registry->services[hash_index];
        if (kain_service_descriptor_matches_lookup(
                candidate,
                canonical_key,
                canonical_key_length,
                canonical_key_state
            )) {
            return candidate;
        }
    }

    /*
     * Fallback linear scan for safety.  Catches extracanonical keys not in the
     * active catalog (audio.device, audio.midi) and any registry entries added
     * by host bridge plugins that don't the active-catalog hash layout.
     */
    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        if (kain_service_descriptor_matches_lookup(
                &registry->services[i],
                canonical_key,
                canonical_key_length,
                canonical_key_state
            )) {
            return &registry->services[i];
        }
    }

    return NULL;
}

static int kain_service_registry_register_or_refresh_descriptor(
    KainServiceRegistry* registry,
    const KainServiceDescriptor* descriptor
) {
    if (!registry || !descriptor) {
        return -1;
    }
    kain_service_registry_ensure_initialized(registry);
    return kain_service_registry_commit_descriptor_unlocked(registry, descriptor, 1);
}

static int kain_service_registry_probe_native_net_service(
    const KainServiceDescriptor* service,
    int* out_probe_passed
) {
    const KainNativeNetFunctionTable* function_table;

    if (!service || !out_probe_passed) {
        return 0;
    }
    if (service->function_table != (void*)&g_kain_native_net_function_table) {
        return 0;
    }

    function_table = (const KainNativeNetFunctionTable*)service->function_table;
    if (!function_table || !function_table->platform_available) {
        return 0;
    }

    *out_probe_passed = function_table->platform_available() ? 1 : 0;
    return 1;
}

static int kain_service_registry_probe_native_process_service(
    const KainServiceDescriptor* service,
    int* out_probe_passed
) {
    const KainNativeProcessFunctionTable* function_table;

    if (!service || !out_probe_passed) {
        return 0;
    }
    if (service->function_table != (void*)&g_kain_native_process_function_table) {
        return 0;
    }

    function_table =
        (const KainNativeProcessFunctionTable*)service->function_table;
    if (!function_table || !function_table->platform_available) {
        return 0;
    }

    *out_probe_passed = function_table->platform_available() ? 1 : 0;
    return 1;
}

static void kain_service_registry_refresh_runtime_probe_statuses(
    KainServiceRegistry* registry
) {
    int i;
    int service_count;

    if (!registry) {
        return;
    }

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; ++i) {
        KainServiceDescriptor* service = &registry->services[i];
        int probe_passed;
        int probed = 0;

        if (!service->function_table) {
            continue;
        }

        if (kain_service_registry_probe_native_net_service(service, &probe_passed)) {
            probed = 1;
        } else if (kain_service_registry_probe_native_process_service(
                       service,
                       &probe_passed
                   )) {
            probed = 1;
        }

        if (!probed) {
            continue;
        }

        if (probe_passed) {
            service->status = KAIN_SERVICE_STATUS_AVAILABLE;
        } else if (service->status == KAIN_SERVICE_STATUS_AVAILABLE) {
            service->status = KAIN_SERVICE_STATUS_DEGRADED;
        }
    }
}

int kain_service_registry_register_native_runtime_services(
    KainServiceRegistry* registry
) {
    size_t i;
    int result = 0;

    if (!registry) {
        return -1;
    }

    kain_service_registry_ensure_initialized(registry);
    kain_service_registry_lock(registry);

    for (i = 0; i < sizeof(g_kain_native_runtime_service_catalog) / sizeof(g_kain_native_runtime_service_catalog[0]); ++i) {
        result = kain_service_registry_commit_descriptor_unlocked(
            registry,
            &g_kain_native_runtime_service_catalog[i],
            1
        );
        if (result != 0) {
            kain_service_registry_unlock(registry);
            return -1;
        }
    }

    kain_service_registry_refresh_runtime_probe_statuses(registry);
    kain_service_registry_unlock(registry);

    return (int)(sizeof(g_kain_native_runtime_service_catalog) / sizeof(g_kain_native_runtime_service_catalog[0]));
}

int kain_service_registry_is_available(
    const KainServiceRegistry* registry,
    const char* key
) {
    const KainServiceDescriptor* descriptor = kain_service_registry_lookup(registry, key);
    if (!descriptor) {
        return 0;
    }
    return descriptor->status == KAIN_SERVICE_STATUS_AVAILABLE;
}

KainServiceStatus kain_service_registry_get_status(
    const KainServiceRegistry* registry,
    const char* key
) {
    const KainServiceDescriptor* descriptor = kain_service_registry_lookup(registry, key);
    if (!descriptor) {
        return KAIN_SERVICE_STATUS_UNAVAILABLE;
    }
    return descriptor->status;
}

int kain_service_registry_count_by_status(
    const KainServiceRegistry* registry,
    KainServiceStatus status
) {
    int count = 0;
    int i;
    int service_count;

    if (!registry) {
        return 0;
    }

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        if (registry->services[i].status == status) {
            count++;
        }
    }

    return count;
}

int kain_service_registry_count_by_requirement(
    const KainServiceRegistry* registry,
    KainServiceRequirement requirement
) {
    int count = 0;
    int i;
    int service_count;

    if (!registry) {
        return 0;
    }

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        if (registry->services[i].requirement == requirement) {
            count++;
        }
    }

    return count;
}

int kain_service_registry_validate_required(
    const KainServiceRegistry* registry,
    KainDiagnostic* diagnostics,
    int max_diagnostics,
    int* diagnostic_count
) {
    int i;
    int failures = 0;
    int diag_idx = 0;
    int service_count;

    if (!registry) {
        return -1;
    }

    if (diagnostic_count) {
        *diagnostic_count = 0;
    }

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];

        /* Only check required services */
        if (service->requirement != KAIN_SERVICE_REQUIREMENT_REQUIRED) {
            continue;
        }

        /* Check if service is available */
        if (service->status == KAIN_SERVICE_STATUS_AVAILABLE) {
            continue;
        }

        /* Service is required but not available */
        failures++;

        /* Add diagnostic if space available */
        if (diagnostics && diag_idx < max_diagnostics) {
            char message[KAIN_DIAG_MESSAGE_MAX];
            char detail[KAIN_DIAG_DETAIL_MAX];

            snprintf(message, sizeof(message),
                "Required service '%s' is not available", service->key);

            snprintf(detail, sizeof(detail),
                "Service: %s\nStatus: %s\nProvider: %d",
                service->name,
                service->status == KAIN_SERVICE_STATUS_UNAVAILABLE ? "unavailable" :
                service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" : "failed",
                service->provider);

            kain_diagnostic_create(
                &diagnostics[diag_idx],
                KAIN_DIAG_SUBSYSTEM_CONTRACT,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
                message,
                detail,
                NULL
            );

            diag_idx++;
        }
    }

    if (diagnostic_count) {
        *diagnostic_count = diag_idx;
    }

    return failures;
}

int kain_service_registry_validate_required_collector(
    const KainServiceRegistry* registry,
    KainDiagnosticCollector* collector
) {
    int i;
    int failures = 0;
    int service_count;

    if (!registry || !collector) {
        return -1;
    }

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];

        /* Only check required services */
        if (service->requirement != KAIN_SERVICE_REQUIREMENT_REQUIRED) {
            continue;
        }

        /* Check if service is available */
        if (service->status == KAIN_SERVICE_STATUS_AVAILABLE) {
            continue;
        }

        /* Service is required but not available */
        failures++;

        /* Add diagnostic to collector */
        char message[KAIN_DIAG_MESSAGE_MAX];
        char detail[KAIN_DIAG_DETAIL_MAX];

        snprintf(message, sizeof(message),
            "Required service '%s' is not available", service->key);

        snprintf(detail, sizeof(detail),
            "Service: %s\nStatus: %s\nProvider: %d",
            service->name,
            service->status == KAIN_SERVICE_STATUS_UNAVAILABLE ? "unavailable" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" : "failed",
            service->provider);

        kain_diagnostic_collector_add_new(
            collector,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
            message,
            detail,
            NULL
        );
    }

    return failures;
}

int kain_service_registry_format_list(
    const KainServiceRegistry* registry,
    char* out,
    size_t out_size
) {
    int i;
    size_t written = 0;
    int service_count;

    if (!registry || !out || out_size == 0) {
        return 0;
    }

    out[0] = '\0';

    service_count = kain_service_registry_count_load(registry);
    for (i = 0; i < service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];
        char line[256];
        int line_len;

        line_len = snprintf(line, sizeof(line),
            "%s%s (%s) - %s\n",
            i > 0 ? "" : "",
            service->key,
            service->status == KAIN_SERVICE_STATUS_AVAILABLE ? "available" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" :
            service->status == KAIN_SERVICE_STATUS_FAILED ? "failed" : "unavailable",
            service->name);

        if (written + line_len >= out_size - 1) {
            break;
        }

        memcpy(out + written, line, (size_t)line_len);
        written += (size_t)line_len;
        out[written] = '\0';
    }

    return (int)written;
}

void kain_service_registry_print(const KainServiceRegistry* registry) {
    int i;
    int service_count;

    if (!registry) {
        printf("Service registry is NULL\n");
        return;
    }

    service_count = kain_service_registry_count_load(registry);
    printf("=== KAIN Service Registry ===\n");
    printf("Services registered: %d / %d\n\n",
        service_count, KAIN_SERVICE_REGISTRY_MAX_SERVICES);

    for (i = 0; i < service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];

        printf("Service %d:\n", i + 1);
        printf("  Key:         %s\n", service->key);
        printf("  Name:        %s\n", service->name);
        printf("  Description: %s\n", service->description[0] ? service->description : "(none)");
        printf("  Provider:    %d\n", service->provider);
        printf("  Status:      %s\n",
            service->status == KAIN_SERVICE_STATUS_AVAILABLE ? "available" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" :
            service->status == KAIN_SERVICE_STATUS_FAILED ? "failed" : "unavailable");
        printf("  Requirement: %s\n",
            service->requirement == KAIN_SERVICE_REQUIREMENT_REQUIRED ? "required" : "optional");
        printf("  ABI Version: 0x%08X\n", service->abi_version);
        printf("\n");
    }
}

KainServiceRegistry* kain_service_registry_global(void) {
    kain_service_registry_ensure_initialized(&g_service_registry);
    return &g_service_registry;
}
