#include "../../include/kain_runtime_services.h"
#include "../../include/kain_runtime_base.h"
#include "../../include/kain_runtime_vendor_lane.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#ifndef _WIN32
#include <strings.h>
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
}

static const KainServiceKeyAlias g_kain_native_runtime_service_aliases[] = {
    {"native.app-host", KAIN_SERVICE_KEY_PLATFORM_APP_HOST},
    {"native.input", KAIN_SERVICE_KEY_PLATFORM_INPUT},
    {"native.viewport", KAIN_SERVICE_KEY_GFX_VIEWPORT},
    {"native.scene", KAIN_SERVICE_KEY_SCENE_RUNTIME},
    {"native.scene.query", KAIN_SERVICE_KEY_SCENE_QUERY},
    {"native.scene.mutation", KAIN_SERVICE_KEY_SCENE_MUTATION},
    {"native.runtime.inspection", KAIN_SERVICE_KEY_RUNTIME_INSPECTION},
    {"native.device.reflection", KAIN_SERVICE_KEY_DEVICE_REFLECTION},
    {"native.asset.gltf", KAIN_SERVICE_KEY_ASSET_GLTF},
    {"native.asset.ingestion", KAIN_SERVICE_KEY_ASSET_INGESTION},
    {"native.ui.compiled-bundle", KAIN_SERVICE_KEY_UI_BUNDLE},
    {"native.compute", KAIN_SERVICE_KEY_GFX_COMPUTE},
};

static const KainServiceDescriptor g_kain_native_runtime_service_catalog[] = {
    {
        KAIN_SERVICE_KEY_BASE_MEMORY,
        "Base Memory Services",
        "Core allocation, retain/release, and memory management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_BASE_DIAGNOSTICS,
        "Base Diagnostics",
        "Structured diagnostics and error reporting",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_CONTRACT,
        "Runtime Contract",
        "Runtime contract bundle loading and validation",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_PLATFORM_APP_HOST,
        "Native App Host",
        "Win32 application host and window management",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_PLATFORM_INPUT,
        "Native Input",
        "Win32 input capture and event handling",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_VIEWPORT,
        "Native Viewport",
        "Win32 viewport host and OpenGL rendering",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_RUNTIME,
        "Scene Runtime",
        "Stable native scene handles and runtime-owned scene state access",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_QUERY,
        "Scene Query",
        "Picking, raycast, bounds, visibility, and selection query contracts",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCENE_MUTATION,
        "Scene Mutation",
        "Transactional scene mutation requests and receipts",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_GLTF,
        "glTF Asset Loader",
        "glTF 2.0 asset loading and parsing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_IMAGE_BIMG,
        "bimg Image Runtime",
        "Image decoding and staging lane reserved for renderer-facing asset ingestion",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_BIMG ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_asset_image_bimg_service
    },
    {
        KAIN_SERVICE_KEY_ASSET_TEXTURE_BIMG,
        "bimg Texture Runtime",
        "Texture container and mip/format staging lane reserved for renderer asset flow",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_BIMG ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_asset_texture_bimg_service
    },
    {
        KAIN_SERVICE_KEY_ASSET_INGESTION,
        "Asset Ingestion",
        "Canonical descriptor-driven entry path for assets and emitted bundles",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASSET_REALTIME,
        "Realtime Bundle Loader",
        "Realtime bundle loading and scene management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_UI_BUNDLE,
        "Compiled UI Bundle",
        "Compiled UI bundle loading and overlay rendering",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_REFLECTION,
        "Reflection Runtime",
        "Reflection payload loading and runtime type lookup",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_RUNTIME_INSPECTION,
        "Runtime Inspection",
        "Runtime-owned scene, resource, and binding inspection queries",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_DEVICE_REFLECTION,
        "Device Reflection",
        "Backend, GPU, display, and hotplug capability descriptors",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ACTOR_RUNTIME,
        "Actor Runtime",
        "Actor spawn, mailbox, lifecycle, and scheduling",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ACTOR_REGISTRY,
        "Actor Registry",
        "Named actor registration and lookup",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASYNC_RUNTIME,
        "Async Runtime",
        "Task and future execution with wake/poll handling",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_ASYNC_TIMERS,
        "Async Timers",
        "Timer registration, wake delivery, and async sleep support",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_IO_LOOP,
        "IO Loop",
        "Vendor-backed event loop and wake delivery substrate",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_LIBUV ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_io_loop_service
    },
    {
        KAIN_SERVICE_KEY_IO_FS,
        "IO Filesystem",
        "Vendor-backed filesystem operations and async file dispatch",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_LIBUV ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_io_fs_service
    },
    {
        KAIN_SERVICE_KEY_IO_NET,
        "IO Network",
        "Vendor-backed TCP, UDP, and name-resolution primitives",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_LIBUV ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_io_net_service
    },
    {
        KAIN_SERVICE_KEY_IO_PROCESS,
        "IO Process",
        "Vendor-backed process spawning, pipes, and child lifecycle management",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_LIBUV ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_io_process_service
    },
    {
        KAIN_SERVICE_KEY_IO_TIMERS,
        "IO Timers",
        "Vendor-backed timer wheel and deadline wake integration",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_LIBUV ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_io_timers_service
    },
    {
        KAIN_SERVICE_KEY_GFX_COMPUTE,
        "Compute Runtime",
        "Compute bundle validation, dispatch planning, and native runtime handoff",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_BGFX,
        "bgfx Renderer Backend",
        "Cross-platform baseline renderer backend for viewport, swapchain, and debug draw",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_BGFX ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_gfx_backend_bgfx_service
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_FILAMENT,
        "Filament Renderer Backend",
        "Premium scene, material, and lighting presentation lane staged behind Kain contracts",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_FILAMENT ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_gfx_backend_filament_service
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_DILIGENT,
        "Diligent Renderer Backend",
        "Explicit render-graph and compute-oriented renderer lane staged for deeper GPU control",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_DILIGENT ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_gfx_backend_diligent_service
    },
    {
        KAIN_SERVICE_KEY_GFX_BACKEND_FORGE,
        "The Forge Renderer Backend",
        "Low-level cross-platform renderer substrate staged behind the Kain renderer backend seam",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_FORGE ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_gfx_backend_forge_service
    },
    {
        KAIN_SERVICE_KEY_UI_COMPONENT,
        "UI Component Runtime",
        "Component state, invalidation, focus, and event routing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_COMPATIBILITY,
        "Compatibility and Hot Reload",
        "Version validation, migration, hot reload, and snapshot flow",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_HOST_BRIDGE,
        "Host Bridge",
        "Plugin and foreign service integration",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    },
    {
        KAIN_SERVICE_KEY_SCRIPT_QUICKJS,
        "QuickJS Script Runtime",
        "Embedded JavaScript runtime for host automation and dynamic scripting",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_QUICKJS ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_script_quickjs_service
    },
    {
        KAIN_SERVICE_KEY_AUDIO_BACKEND,
        "Audio Backend",
        "Vendor-backed device and backend abstraction for realtime audio",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_MINIAUDIO ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_audio_backend_service
    },
    {
        KAIN_SERVICE_KEY_AUDIO_GRAPH,
        "Audio Graph",
        "Vendor-backed audio graph and node processing substrate",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_MINIAUDIO ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_audio_graph_service
    },
    {
        KAIN_SERVICE_KEY_AUDIO_DEVICE,
        "Audio Device",
        "Vendor-backed playback and capture device lifecycle",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_MINIAUDIO ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_audio_device_service
    },
    {
        KAIN_SERVICE_KEY_AUDIO_ASSETS,
        "Audio Assets",
        "Vendor-backed audio asset decoding and resource loading",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_MINIAUDIO ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_audio_assets_service
    },
    {
        KAIN_SERVICE_KEY_WASM_RUNTIME_LIGHT,
        "WASM Runtime Light",
        "Lightweight WebAssembly runtime for small sandboxed modules",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_WASM3 ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_wasm_runtime_light_service
    },
    {
        KAIN_SERVICE_KEY_WASM_RUNTIME_FULL,
        "WASM Runtime Full",
        "Full WebAssembly runtime lane staged for richer module hosting",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_WAMR ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_wasm_runtime_full_service
    },
    {
        KAIN_SERVICE_KEY_WASM_MODULE,
        "WASM Module",
        "Kain-owned WebAssembly module loading and execution seam",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_WASM3 ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_wasm_module_service
    },
    {
        KAIN_SERVICE_KEY_WASM_WASI,
        "WASM WASI",
        "WASI-flavored runtime lane staged behind the WebAssembly service family",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_WAMR ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_wasm_wasi_service
    },
    {
        KAIN_SERVICE_KEY_ALLOCATOR_MIMALLOC,
        "Allocator Mimalloc",
        "Mimalloc-backed allocation lane for Kain runtime experiments",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_MIMALLOC ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_allocator_mimalloc_service
    },
    {
        KAIN_SERVICE_KEY_ALLOCATOR_RPMALLOC,
        "Allocator Rpmalloc",
        "Rpmalloc-backed allocation lane for Kain runtime experiments",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_VENDOR_HAS_RPMALLOC ? KAIN_SERVICE_STATUS_AVAILABLE : KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        (void*)&g_kain_vendor_allocator_rpmalloc_service
    },
};

/* Global service registry singleton */
static KainServiceRegistry g_service_registry = {0};

const char* kain_service_registry_canonicalize_key(const char* key) {
    size_t i;

    if (!key || !key[0]) {
        return key;
    }

    for (i = 0; i < sizeof(g_kain_native_runtime_service_aliases) / sizeof(g_kain_native_runtime_service_aliases[0]); ++i) {
        const KainServiceKeyAlias* alias = &g_kain_native_runtime_service_aliases[i];
        if (kain_text_equals_ci(key, alias->alias_key)) {
            return alias->canonical_key;
        }
    }

    return key;
}

void kain_service_registry_init(KainServiceRegistry* registry) {
    if (!registry) {
        return;
    }
    ZeroMemory(registry, sizeof(*registry));
    registry->initialized = 1;
}

static KainServiceDescriptor* kain_service_registry_lookup_mutable(
    KainServiceRegistry* registry,
    const char* key
) {
    const char* canonical_key = kain_service_registry_canonicalize_key(key);
    int i;

    if (!registry || !canonical_key) {
        return NULL;
    }

    for (i = 0; i < registry->service_count; ++i) {
        if (kain_text_equals_ci(registry->services[i].key, canonical_key)) {
            return &registry->services[i];
        }
    }

    return NULL;
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
    KainServiceDescriptor* descriptor;
    const char* canonical_key;
    
    if (!registry || !key || !name) {
        return -1;
    }

    canonical_key = kain_service_registry_canonicalize_key(key);
    if (!canonical_key || !canonical_key[0]) {
        return -1;
    }
    
    if (!registry->initialized) {
        kain_service_registry_init(registry);
    }
    
    /* Check if registry is full */
    if (registry->service_count >= KAIN_SERVICE_REGISTRY_MAX_SERVICES) {
        return -2;
    }
    
    /* Check if service already exists */
    if (kain_service_registry_lookup(registry, canonical_key) != NULL) {
        return -3;
    }
    
    /* Register the service */
    descriptor = &registry->services[registry->service_count];
    ZeroMemory(descriptor, sizeof(*descriptor));
    
    kain_copy_text(descriptor->key, sizeof(descriptor->key), canonical_key);
    kain_copy_text(descriptor->name, sizeof(descriptor->name), name);
    kain_copy_text(descriptor->description, sizeof(descriptor->description), description);
    
    descriptor->provider = provider;
    descriptor->status = status;
    descriptor->requirement = requirement;
    descriptor->abi_version = abi_version;
    descriptor->function_table = function_table;
    
    registry->service_count++;
    return 0;
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
    
    if (!registry || !canonical_key) {
        return NULL;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        if (kain_text_equals_ci(registry->services[i].key, canonical_key)) {
            return &registry->services[i];
        }
    }
    
    return NULL;
}

static int kain_service_registry_register_or_refresh_descriptor(
    KainServiceRegistry* registry,
    const KainServiceDescriptor* descriptor
) {
    KainServiceDescriptor* existing;

    if (!registry || !descriptor) {
        return -1;
    }

    existing = kain_service_registry_lookup_mutable(registry, descriptor->key);
    if (existing) {
        kain_service_descriptor_copy(existing, descriptor);
        return 0;
    }

    return kain_service_registry_register_descriptor(registry, descriptor);
}

int kain_service_registry_register_native_runtime_services(
    KainServiceRegistry* registry
) {
    size_t i;

    if (!registry) {
        return -1;
    }

    if (!registry->initialized) {
        kain_service_registry_init(registry);
    }

    for (i = 0; i < sizeof(g_kain_native_runtime_service_catalog) / sizeof(g_kain_native_runtime_service_catalog[0]); ++i) {
        if (kain_service_registry_register_or_refresh_descriptor(
                registry,
                &g_kain_native_runtime_service_catalog[i]
            ) != 0) {
            return -1;
        }
    }

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
    
    if (!registry) {
        return 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
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
    
    if (!registry) {
        return 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
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
    
    if (!registry) {
        return -1;
    }
    
    if (diagnostic_count) {
        *diagnostic_count = 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
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
    
    if (!registry || !collector) {
        return -1;
    }
    
    for (i = 0; i < registry->service_count; i++) {
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
    
    if (!registry || !out || out_size == 0) {
        return 0;
    }
    
    out[0] = '\0';
    
    for (i = 0; i < registry->service_count; i++) {
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
    
    if (!registry) {
        printf("Service registry is NULL\n");
        return;
    }
    
    printf("=== KAIN Service Registry ===\n");
    printf("Services registered: %d / %d\n\n",
        registry->service_count, KAIN_SERVICE_REGISTRY_MAX_SERVICES);
    
    for (i = 0; i < registry->service_count; i++) {
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
    if (!g_service_registry.initialized) {
        kain_service_registry_init(&g_service_registry);
    }
    return &g_service_registry;
}

