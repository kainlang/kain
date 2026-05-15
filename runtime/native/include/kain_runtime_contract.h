#ifndef KAIN_RUNTIME_CONTRACT_H
#define KAIN_RUNTIME_CONTRACT_H

#include "kain_runtime_win32.h"
#include "kain_runtime_version.h"
#include "kain_runtime_services.h"
#include <stdint.h>

#define KAIN_RUNTIME_CONTRACT_ENV "KAIN_RUNTIME_CONTRACT"
#define KAIN_RUNTIME_CONTRACT_STRICT_ENV "KAIN_RUNTIME_CONTRACT_STRICT"
#define KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX ".runtime_contract.json"
#define KAIN_RUNTIME_CONTRACT_MAX_TARGET 32
#define KAIN_RUNTIME_CONTRACT_MAX_ORIGIN 32
#define KAIN_RUNTIME_CONTRACT_MAX_PATH 512
#define KAIN_RUNTIME_CONTRACT_MAX_MESSAGE 256
#define KAIN_RUNTIME_CONTRACT_MAX_DIAGNOSTICS 8

typedef uint64_t KainRuntimeServiceMask;

#define KAIN_RUNTIME_SERVICE_BASE_MEMORY         (UINT64_C(1) << 0)
#define KAIN_RUNTIME_SERVICE_MEMORY_OWNERSHIP    (UINT64_C(1) << 1)
#define KAIN_RUNTIME_SERVICE_BASE_DIAGNOSTICS    (UINT64_C(1) << 2)
#define KAIN_RUNTIME_SERVICE_CONTRACT            (UINT64_C(1) << 3)
#define KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST     (UINT64_C(1) << 4)
#define KAIN_RUNTIME_SERVICE_NATIVE_INPUT        (UINT64_C(1) << 5)
#define KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT     (UINT64_C(1) << 6)
#define KAIN_RUNTIME_SERVICE_GFX_RAW_NATIVE      (UINT64_C(1) << 7)
#define KAIN_RUNTIME_SERVICE_GFX_SHADER_SPIRV    (UINT64_C(1) << 8)
#define KAIN_RUNTIME_SERVICE_GFX_BACKEND_VULKAN  (UINT64_C(1) << 9)
#define KAIN_RUNTIME_SERVICE_GFX_BACKEND_D3D12   (UINT64_C(1) << 10)
#define KAIN_RUNTIME_SERVICE_SCENE_RUNTIME       (UINT64_C(1) << 11)
#define KAIN_RUNTIME_SERVICE_SCENE_QUERY         (UINT64_C(1) << 12)
#define KAIN_RUNTIME_SERVICE_SCENE_MUTATION      (UINT64_C(1) << 13)
#define KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF   (UINT64_C(1) << 14)
#define KAIN_RUNTIME_SERVICE_ASSET_INGESTION     (UINT64_C(1) << 15)
#define KAIN_RUNTIME_SERVICE_ASSET_REALTIME      (UINT64_C(1) << 16)
#define KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED  (UINT64_C(1) << 17)
#define KAIN_RUNTIME_SERVICE_REFLECTION          (UINT64_C(1) << 18)
#define KAIN_RUNTIME_SERVICE_RUNTIME_INSPECTION  (UINT64_C(1) << 19)
#define KAIN_RUNTIME_SERVICE_DEVICE_REFLECTION   (UINT64_C(1) << 20)
#define KAIN_RUNTIME_SERVICE_ACTOR_RUNTIME       (UINT64_C(1) << 21)
#define KAIN_RUNTIME_SERVICE_ACTOR_REGISTRY      (UINT64_C(1) << 22)
#define KAIN_RUNTIME_SERVICE_ASYNC_RUNTIME       (UINT64_C(1) << 23)
#define KAIN_RUNTIME_SERVICE_ASYNC_TIMERS        (UINT64_C(1) << 24)
#define KAIN_RUNTIME_SERVICE_IO_NET              (UINT64_C(1) << 25)
#define KAIN_RUNTIME_SERVICE_IO_PROCESS          (UINT64_C(1) << 26)
#define KAIN_RUNTIME_SERVICE_GFX_COMPUTE         (UINT64_C(1) << 27)
#define KAIN_RUNTIME_SERVICE_UI_COMPONENT        (UINT64_C(1) << 28)
#define KAIN_RUNTIME_SERVICE_COMPATIBILITY       (UINT64_C(1) << 29)
#define KAIN_RUNTIME_SERVICE_HOST_BRIDGE         (UINT64_C(1) << 30)

#define KAIN_RUNTIME_SERVICE_CORE_MASK ( \
    KAIN_RUNTIME_SERVICE_BASE_MEMORY | \
    KAIN_RUNTIME_SERVICE_MEMORY_OWNERSHIP | \
    KAIN_RUNTIME_SERVICE_BASE_DIAGNOSTICS | \
    KAIN_RUNTIME_SERVICE_CONTRACT \
)

#define KAIN_RUNTIME_SERVICE_OPTIONAL_MASK ( \
    KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST | \
    KAIN_RUNTIME_SERVICE_NATIVE_INPUT | \
    KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT | \
    KAIN_RUNTIME_SERVICE_GFX_RAW_NATIVE | \
    KAIN_RUNTIME_SERVICE_GFX_SHADER_SPIRV | \
    KAIN_RUNTIME_SERVICE_GFX_BACKEND_VULKAN | \
    KAIN_RUNTIME_SERVICE_GFX_BACKEND_D3D12 | \
    KAIN_RUNTIME_SERVICE_SCENE_RUNTIME | \
    KAIN_RUNTIME_SERVICE_SCENE_QUERY | \
    KAIN_RUNTIME_SERVICE_SCENE_MUTATION | \
    KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF | \
    KAIN_RUNTIME_SERVICE_ASSET_INGESTION | \
    KAIN_RUNTIME_SERVICE_ASSET_REALTIME | \
    KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED | \
    KAIN_RUNTIME_SERVICE_REFLECTION | \
    KAIN_RUNTIME_SERVICE_RUNTIME_INSPECTION | \
    KAIN_RUNTIME_SERVICE_DEVICE_REFLECTION | \
    KAIN_RUNTIME_SERVICE_ACTOR_RUNTIME | \
    KAIN_RUNTIME_SERVICE_ACTOR_REGISTRY | \
    KAIN_RUNTIME_SERVICE_ASYNC_RUNTIME | \
    KAIN_RUNTIME_SERVICE_ASYNC_TIMERS | \
    KAIN_RUNTIME_SERVICE_IO_NET | \
    KAIN_RUNTIME_SERVICE_IO_PROCESS | \
    KAIN_RUNTIME_SERVICE_GFX_COMPUTE | \
    KAIN_RUNTIME_SERVICE_UI_COMPONENT | \
    KAIN_RUNTIME_SERVICE_COMPATIBILITY | \
    KAIN_RUNTIME_SERVICE_HOST_BRIDGE \
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
    char target[KAIN_RUNTIME_CONTRACT_MAX_TARGET];
    char load_origin[KAIN_RUNTIME_CONTRACT_MAX_ORIGIN];
    char source_path[KAIN_RUNTIME_CONTRACT_MAX_PATH];
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
    char runtime_abi_version_string[KAIN_RUNTIME_VERSION_STRING_MAX];
    char contract_abi_version_string[KAIN_RUNTIME_VERSION_STRING_MAX];
    char fatal_message[KAIN_RUNTIME_CONTRACT_MAX_MESSAGE];
    char warnings[KAIN_RUNTIME_CONTRACT_MAX_DIAGNOSTICS][KAIN_RUNTIME_CONTRACT_MAX_MESSAGE];
} KainRuntimeContractValidation;

void kain_runtime_contract_init(KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_json(const char* json, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_path(const char* path, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_env(const char* env_name, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_for_current_process(
    const char* env_name,
    KainRuntimeContractBundle* bundle
);
KainRuntimeServiceMask kain_runtime_contract_service_mask(const KainRuntimeContractBundle* bundle);
void kain_runtime_contract_validation_init(KainRuntimeContractValidation* validation);
int kain_runtime_contract_validate_startup(
    const KainRuntimeContractBundle* bundle,
    KainRuntimeServiceMask required_service_mask,
    KainRuntimeServiceMask optional_service_mask,
    KainRuntimeContractValidation* validation
);
void kain_runtime_contract_format_service_mask(
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
void kain_runtime_contract_populate_service_registry(KainServiceRegistry* registry);

/*
 * Check Service Availability
 *
 * Queries the service registry for service availability. Falls back to
 * canonical key mapping when the registry has not been populated yet.
 */
int kain_runtime_contract_is_service_available(const char* service_key);

/*
 * Enhanced Startup Validation with Diagnostic Collection
 *
 * Extended validation that populates a KainStartupValidationResult with
 * comprehensive version information, service status, and structured diagnostics.
 * This is the preferred validation function for new code.
 */
int kain_runtime_contract_validate_startup_enhanced(
    const KainRuntimeContractBundle* bundle,
    KainRuntimeServiceMask required_service_mask,
    KainRuntimeServiceMask optional_service_mask,
    KainStartupValidationResult* result
);

#endif /* KAIN_RUNTIME_CONTRACT_H */
