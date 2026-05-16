#ifndef CONTRACT_H
#define CONTRACT_H

#include "win32.h"
#include "version.h"
#include "services.h"
#include <stdint.h>

#define CONTRACT_ENV "RUNTIME_CONTRACT"
#define CONTRACT_STRICT_ENV "CONTRACT_STRICT"
#define CONTRACT_SIDECAR_SUFFIX ".runtime_contract.json"
#define CONTRACT_MAX_TARGET 32
#define CONTRACT_MAX_ORIGIN 32
#define CONTRACT_MAX_PATH 512
#define CONTRACT_MAX_MESSAGE 256
#define CONTRACT_MAX_DIAGNOSTICS 8

typedef uint64_t KainRuntimeServiceMask;

#define RUNTIME_SERVICE_BASE_MEMORY         (UINT64_C(1) << 0)
#define RUNTIME_SERVICE_MEMORY_OWNERSHIP    (UINT64_C(1) << 1)
#define RUNTIME_SERVICE_BASE_DIAGNOSTICS    (UINT64_C(1) << 2)
#define RUNTIME_SERVICE_CONTRACT            (UINT64_C(1) << 3)
#define RUNTIME_SERVICE_NATIVE_APP_HOST     (UINT64_C(1) << 4)
#define RUNTIME_SERVICE_NATIVE_INPUT        (UINT64_C(1) << 5)
#define RUNTIME_SERVICE_NATIVE_VIEWPORT     (UINT64_C(1) << 6)
#define RUNTIME_SERVICE_GFX_RAW_NATIVE      (UINT64_C(1) << 7)
#define RUNTIME_SERVICE_GFX_SHADER_SPIRV    (UINT64_C(1) << 8)
#define RUNTIME_SERVICE_GFX_BACKEND_VULKAN  (UINT64_C(1) << 9)
#define RUNTIME_SERVICE_GFX_BACKEND_D3D12   (UINT64_C(1) << 10)
#define RUNTIME_SERVICE_SCENE_RUNTIME       (UINT64_C(1) << 11)
#define RUNTIME_SERVICE_SCENE_QUERY         (UINT64_C(1) << 12)
#define RUNTIME_SERVICE_SCENE_MUTATION      (UINT64_C(1) << 13)
#define RUNTIME_SERVICE_NATIVE_ASSET_GLTF   (UINT64_C(1) << 14)
#define RUNTIME_SERVICE_ASSET_INGESTION     (UINT64_C(1) << 15)
#define RUNTIME_SERVICE_ASSET_REALTIME      (UINT64_C(1) << 16)
#define RUNTIME_SERVICE_NATIVE_UI_COMPILED  (UINT64_C(1) << 17)
#define RUNTIME_SERVICE_REFLECTION          (UINT64_C(1) << 18)
#define RUNTIME_SERVICE_RUNTIME_INSPECTION  (UINT64_C(1) << 19)
#define RUNTIME_SERVICE_DEVICE_REFLECTION   (UINT64_C(1) << 20)
#define RUNTIME_SERVICE_ACTOR_RUNTIME       (UINT64_C(1) << 21)
#define RUNTIME_SERVICE_ACTOR_REGISTRY      (UINT64_C(1) << 22)
#define RUNTIME_SERVICE_ASYNC_RUNTIME       (UINT64_C(1) << 23)
#define RUNTIME_SERVICE_ASYNC_TIMERS        (UINT64_C(1) << 24)
#define RUNTIME_SERVICE_IO_NET              (UINT64_C(1) << 25)
#define RUNTIME_SERVICE_IO_PROCESS          (UINT64_C(1) << 26)
#define RUNTIME_SERVICE_GFX_COMPUTE         (UINT64_C(1) << 27)
#define RUNTIME_SERVICE_UI_COMPONENT        (UINT64_C(1) << 28)
#define RUNTIME_SERVICE_COMPATIBILITY       (UINT64_C(1) << 29)
#define RUNTIME_SERVICE_HOST_BRIDGE         (UINT64_C(1) << 30)

#define RUNTIME_SERVICE_CORE_MASK ( \
    RUNTIME_SERVICE_BASE_MEMORY | \
    RUNTIME_SERVICE_MEMORY_OWNERSHIP | \
    RUNTIME_SERVICE_BASE_DIAGNOSTICS | \
    RUNTIME_SERVICE_CONTRACT \
)

#define RUNTIME_SERVICE_OPTIONAL_MASK ( \
    RUNTIME_SERVICE_NATIVE_APP_HOST | \
    RUNTIME_SERVICE_NATIVE_INPUT | \
    RUNTIME_SERVICE_NATIVE_VIEWPORT | \
    RUNTIME_SERVICE_GFX_RAW_NATIVE | \
    RUNTIME_SERVICE_GFX_SHADER_SPIRV | \
    RUNTIME_SERVICE_GFX_BACKEND_VULKAN | \
    RUNTIME_SERVICE_GFX_BACKEND_D3D12 | \
    RUNTIME_SERVICE_SCENE_RUNTIME | \
    RUNTIME_SERVICE_SCENE_QUERY | \
    RUNTIME_SERVICE_SCENE_MUTATION | \
    RUNTIME_SERVICE_NATIVE_ASSET_GLTF | \
    RUNTIME_SERVICE_ASSET_INGESTION | \
    RUNTIME_SERVICE_ASSET_REALTIME | \
    RUNTIME_SERVICE_NATIVE_UI_COMPILED | \
    RUNTIME_SERVICE_REFLECTION | \
    RUNTIME_SERVICE_RUNTIME_INSPECTION | \
    RUNTIME_SERVICE_DEVICE_REFLECTION | \
    RUNTIME_SERVICE_ACTOR_RUNTIME | \
    RUNTIME_SERVICE_ACTOR_REGISTRY | \
    RUNTIME_SERVICE_ASYNC_RUNTIME | \
    RUNTIME_SERVICE_ASYNC_TIMERS | \
    RUNTIME_SERVICE_IO_NET | \
    RUNTIME_SERVICE_IO_PROCESS | \
    RUNTIME_SERVICE_GFX_COMPUTE | \
    RUNTIME_SERVICE_UI_COMPONENT | \
    RUNTIME_SERVICE_COMPATIBILITY | \
    RUNTIME_SERVICE_HOST_BRIDGE \
)

typedef struct {
    int loaded;
    int target_is_llvm;
    int valid_for_raw_native;
    int required_capability_count;
    int service_count;
    int item_count;
    int core_service_count;
    int optional_service_count;
    int missing_core_service_count;
    KainRuntimeServiceMask service_mask;
    unsigned int required_abi_version;
    char target[CONTRACT_MAX_TARGET];
    char load_origin[CONTRACT_MAX_ORIGIN];
    char source_path[CONTRACT_MAX_PATH];
} KainRuntimeContractBundle;

typedef struct {
    int strict_mode;
    int contract_present;
    int fatal_error;
    KainRuntimeServiceMask required_service_mask;
    KainRuntimeServiceMask optional_service_mask;
    KainRuntimeServiceMask available_service_mask;
    KainRuntimeServiceMask missing_required_mask;
    KainRuntimeServiceMask downgraded_optional_mask;
    int warning_count;
    int abi_compatible;
    unsigned int runtime_abi_version;
    unsigned int contract_abi_version;
    char runtime_abi_version_string[VERSION_STRING_MAX];
    char contract_abi_version_string[VERSION_STRING_MAX];
    char fatal_message[CONTRACT_MAX_MESSAGE];
    char warnings[CONTRACT_MAX_DIAGNOSTICS][CONTRACT_MAX_MESSAGE];
} KainRuntimeContractValidation;

void contract_init(KainRuntimeContractBundle* bundle);
int contract_load_from_json(const char* json, KainRuntimeContractBundle* bundle);
int contract_load_from_path(const char* path, KainRuntimeContractBundle* bundle);
int contract_load_from_env(const char* env_name, KainRuntimeContractBundle* bundle);
int contract_load_for_current_process(
    const char* env_name,
    KainRuntimeContractBundle* bundle
);
KainRuntimeServiceMask contract_service_mask(const KainRuntimeContractBundle* bundle);
void contract_validation_init(KainRuntimeContractValidation* validation);
int contract_validate_startup(
    const KainRuntimeContractBundle* bundle,
    KainRuntimeServiceMask required_service_mask,
    KainRuntimeServiceMask optional_service_mask,
    KainRuntimeContractValidation* validation
);
void contract_format_service_mask(
    KainRuntimeServiceMask service_mask,
    char* out,
    size_t out_cap
);

/*
 * Populate Service Registry
 *
 * Registers the live lean native runtime catalog with the canonical service
 * registry. This keeps startup validation and host discovery data-driven.
 */
void contract_populate_service_registry(KainServiceRegistry* registry);

/*
 * Check Service Availability
 *
 * Queries the service registry for service availability. Falls back to
 * canonical key mapping when the registry has not been populated yet.
 */
int contract_is_service_available(const char* service_key);

/*
 * Enhanced Startup Validation with Diagnostic Collection
 *
 * Extended validation that populates a KainStartupValidationResult with
 * comprehensive version information, service status, and structured diagnostics.
 * This is the preferred validation function for new code.
 */
int contract_validate_startup_enhanced(
    const KainRuntimeContractBundle* bundle,
    KainRuntimeServiceMask required_service_mask,
    KainRuntimeServiceMask optional_service_mask,
    KainStartupValidationResult* result
);

#endif /* CONTRACT_H */
