#ifndef KAIN_RUNTIME_SERVICES_H
#define KAIN_RUNTIME_SERVICES_H

#include "kain_runtime_version.h"
#include "kain_runtime_diagnostics.h"
#include <stddef.h>

/*
 * KAIN Native Runtime Service Registry
 *
 * This header defines the canonical service table model for the KAIN native
 * runtime. All runtime services are registered through this interface, enabling
 * data-driven capability discovery, validation, and binding.
 *
 * Service Families:
 * - Base: Memory, allocation, lifetime, diagnostics
 * - Contract: Runtime contract and reflection loading
 * - Actor: Actor runtime (spawn, mailbox, supervision, registry)
 * - Async: Async/task/timer runtime
 * - Platform: App host, input, timing, window management
 * - Graphics: Viewport, rendering, shader, material, compute
 * - UI: UI bundle loading and component runtime
 * - Asset: Asset loading (glTF, realtime bundles)
 * - Host Bridge: Plugin and foreign service integration
 * - Compatibility: Hot reload, versioning, migration
 */

/* Service Keys (stable identifiers) */
#define KAIN_SERVICE_KEY_BASE_MEMORY            "base.memory"
#define KAIN_SERVICE_KEY_BASE_DIAGNOSTICS       "base.diagnostics"
#define KAIN_SERVICE_KEY_CONTRACT               "contract"
#define KAIN_SERVICE_KEY_REFLECTION             "reflection"
#define KAIN_SERVICE_KEY_ACTOR_RUNTIME          "actor.runtime"
#define KAIN_SERVICE_KEY_ACTOR_REGISTRY         "actor.registry"
#define KAIN_SERVICE_KEY_ASYNC_RUNTIME          "async.runtime"
#define KAIN_SERVICE_KEY_ASYNC_TIMERS           "async.timers"
#define KAIN_SERVICE_KEY_PLATFORM_APP_HOST      "platform.app-host"
#define KAIN_SERVICE_KEY_PLATFORM_INPUT         "platform.input"
#define KAIN_SERVICE_KEY_PLATFORM_WINDOW        "platform.window"
#define KAIN_SERVICE_KEY_GFX_VIEWPORT           "gfx.viewport"
#define KAIN_SERVICE_KEY_GFX_SHADER             "gfx.shader"
#define KAIN_SERVICE_KEY_GFX_MATERIAL           "gfx.material"
#define KAIN_SERVICE_KEY_GFX_COMPUTE            "gfx.compute"
#define KAIN_SERVICE_KEY_UI_BUNDLE              "ui.bundle"
#define KAIN_SERVICE_KEY_UI_COMPONENT           "ui.component"
#define KAIN_SERVICE_KEY_ASSET_GLTF             "asset.gltf"
#define KAIN_SERVICE_KEY_ASSET_REALTIME         "asset.realtime"
#define KAIN_SERVICE_KEY_HOST_BRIDGE            "host.bridge"
#define KAIN_SERVICE_KEY_COMPATIBILITY          "compatibility"

/* Service Provider Lanes */
typedef enum {
    KAIN_SERVICE_PROVIDER_UNKNOWN = 0,
    KAIN_SERVICE_PROVIDER_NATIVE_CORE,
    KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
    KAIN_SERVICE_PROVIDER_PLATFORM_LINUX,
    KAIN_SERVICE_PROVIDER_PLATFORM_MACOS,
    KAIN_SERVICE_PROVIDER_HOST_RUST,
    KAIN_SERVICE_PROVIDER_HOST_PYTHON,
    KAIN_SERVICE_PROVIDER_HOST_NODE,
    KAIN_SERVICE_PROVIDER_EXTERNAL,
} KainServiceProvider;

/* Service Status */
typedef enum {
    KAIN_SERVICE_STATUS_UNAVAILABLE = 0,
    KAIN_SERVICE_STATUS_AVAILABLE,
    KAIN_SERVICE_STATUS_DEGRADED,
    KAIN_SERVICE_STATUS_FAILED,
} KainServiceStatus;

/* Service Requirement Level */
typedef enum {
    KAIN_SERVICE_REQUIREMENT_OPTIONAL = 0,
    KAIN_SERVICE_REQUIREMENT_REQUIRED,
} KainServiceRequirement;

/* String Buffer Sizes */
#define KAIN_SERVICE_KEY_MAX        64
#define KAIN_SERVICE_NAME_MAX       128
#define KAIN_SERVICE_DESCRIPTION_MAX 256

/*
 * Service Descriptor
 *
 * Describes a runtime service, its provider, status, and requirements.
 * Services are registered at runtime initialization and queried during
 * startup validation and capability discovery.
 */
typedef struct {
    char key[KAIN_SERVICE_KEY_MAX];
    char name[KAIN_SERVICE_NAME_MAX];
    char description[KAIN_SERVICE_DESCRIPTION_MAX];
    KainServiceProvider provider;
    KainServiceStatus status;
    KainServiceRequirement requirement;
    unsigned int abi_version;
    void* function_table;  /* Opaque pointer to service-specific function table */
} KainServiceDescriptor;

/*
 * Service Registry
 *
 * Central registry for all runtime services. Populated during runtime
 * initialization and used for capability discovery and validation.
 */
#define KAIN_SERVICE_REGISTRY_MAX_SERVICES 64

typedef struct {
    int initialized;
    int service_count;
    KainServiceDescriptor services[KAIN_SERVICE_REGISTRY_MAX_SERVICES];
} KainServiceRegistry;

/*
 * Initialize Service Registry
 *
 * Clears the registry and prepares it for service registration.
 * Must be called before any service registration.
 */
void kain_service_registry_init(KainServiceRegistry* registry);

/*
 * Register Service
 *
 * Registers a service with the runtime. Returns 0 on success, non-zero on error.
 * Fails if the registry is full or if a service with the same key already exists.
 */
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
);

/*
 * Lookup Service by Key
 *
 * Finds a service by its key. Returns NULL if not found.
 */
const KainServiceDescriptor* kain_service_registry_lookup(
    const KainServiceRegistry* registry,
    const char* key
);

/*
 * Check Service Availability
 *
 * Returns 1 if the service is available, 0 otherwise.
 */
int kain_service_registry_is_available(
    const KainServiceRegistry* registry,
    const char* key
);

/*
 * Get Service Status
 *
 * Returns the status of a service, or UNAVAILABLE if not found.
 */
KainServiceStatus kain_service_registry_get_status(
    const KainServiceRegistry* registry,
    const char* key
);

/*
 * Count Services by Status
 *
 * Returns the number of services with the given status.
 */
int kain_service_registry_count_by_status(
    const KainServiceRegistry* registry,
    KainServiceStatus status
);

/*
 * Count Services by Requirement
 *
 * Returns the number of services with the given requirement level.
 */
int kain_service_registry_count_by_requirement(
    const KainServiceRegistry* registry,
    KainServiceRequirement requirement
);

/*
 * Validate Required Services
 *
 * Checks that all required services are available. Returns 0 if all required
 * services are available, non-zero otherwise. Populates diagnostics array
 * with missing/failed services.
 */
int kain_service_registry_validate_required(
    const KainServiceRegistry* registry,
    KainDiagnostic* diagnostics,
    int max_diagnostics,
    int* diagnostic_count
);

/*
 * Validate Required Services (Collector)
 *
 * Checks that all required services are available and adds diagnostics to
 * the provided collector. Returns the number of failures (missing/failed
 * required services).
 */
int kain_service_registry_validate_required_collector(
    const KainServiceRegistry* registry,
    KainDiagnosticCollector* collector
);

/*
 * Format Service List
 *
 * Formats a human-readable list of services into the output buffer.
 * Returns number of characters written (excluding null terminator).
 */
int kain_service_registry_format_list(
    const KainServiceRegistry* registry,
    char* out,
    size_t out_size
);

/*
 * Print Service Registry
 *
 * Prints the service registry to stdout for diagnostics.
 */
void kain_service_registry_print(const KainServiceRegistry* registry);

/*
 * Get Global Service Registry
 *
 * Returns a pointer to the global service registry singleton.
 * The registry is initialized on first access.
 */
KainServiceRegistry* kain_service_registry_global(void);

#endif /* KAIN_RUNTIME_SERVICES_H */
