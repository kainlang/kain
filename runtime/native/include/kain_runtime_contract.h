#ifndef KAIN_RUNTIME_CONTRACT_H
#define KAIN_RUNTIME_CONTRACT_H

#include "kain_runtime_win32.h"
#include "kain_runtime_version.h"
#include "kain_runtime_services.h"

#define KAIN_RUNTIME_CONTRACT_ENV "KAIN_RUNTIME_CONTRACT"
#define KAIN_RUNTIME_CONTRACT_STRICT_ENV "KAIN_RUNTIME_CONTRACT_STRICT"
#define KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX ".runtime_contract.json"
#define KAIN_RUNTIME_CONTRACT_MAX_TARGET 32
#define KAIN_RUNTIME_CONTRACT_MAX_ORIGIN 32
#define KAIN_RUNTIME_CONTRACT_MAX_PATH 512
#define KAIN_RUNTIME_CONTRACT_MAX_MESSAGE 256
#define KAIN_RUNTIME_CONTRACT_MAX_DIAGNOSTICS 8

#define KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST        (1u << 0)
#define KAIN_RUNTIME_SERVICE_NATIVE_INPUT           (1u << 1)
#define KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT        (1u << 2)
#define KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF      (1u << 3)
#define KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED     (1u << 4)
#define KAIN_RUNTIME_SERVICE_GFX_COMPUTE            (1u << 5)
#define KAIN_RUNTIME_SERVICE_SCENE_RUNTIME          (1u << 6)
#define KAIN_RUNTIME_SERVICE_SCENE_QUERY            (1u << 7)
#define KAIN_RUNTIME_SERVICE_SCENE_MUTATION         (1u << 8)
#define KAIN_RUNTIME_SERVICE_RUNTIME_INSPECTION     (1u << 9)
#define KAIN_RUNTIME_SERVICE_DEVICE_REFLECTION      (1u << 10)
#define KAIN_RUNTIME_SERVICE_ASSET_INGESTION        (1u << 11)

#define KAIN_RUNTIME_SERVICE_CORE_MASK ( \
    KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST | \
    KAIN_RUNTIME_SERVICE_NATIVE_INPUT | \
    KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT \
)

#define KAIN_RUNTIME_SERVICE_OPTIONAL_MASK ( \
    KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF | \
    KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED | \
    KAIN_RUNTIME_SERVICE_GFX_COMPUTE | \
    KAIN_RUNTIME_SERVICE_SCENE_RUNTIME | \
    KAIN_RUNTIME_SERVICE_SCENE_QUERY | \
    KAIN_RUNTIME_SERVICE_SCENE_MUTATION | \
    KAIN_RUNTIME_SERVICE_RUNTIME_INSPECTION | \
    KAIN_RUNTIME_SERVICE_DEVICE_REFLECTION | \
    KAIN_RUNTIME_SERVICE_ASSET_INGESTION \
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
    int has_native_app_host;
    int has_native_input;
    int has_native_viewport;
    int has_native_asset_gltf;
    int has_native_ui_compiled_bundle;
    int has_gfx_compute;
    int has_scene_runtime;
    int has_scene_queries;
    int has_scene_mutation;
    int has_runtime_inspection;
    int has_device_reflection;
    int has_asset_ingestion;
    unsigned int service_mask;
    unsigned int required_abi_version;
    char target[KAIN_RUNTIME_CONTRACT_MAX_TARGET];
    char load_origin[KAIN_RUNTIME_CONTRACT_MAX_ORIGIN];
    char source_path[KAIN_RUNTIME_CONTRACT_MAX_PATH];
} KainRuntimeContractBundle;

typedef struct {
    int strict_mode;
    int contract_present;
    int fatal_error;
    unsigned int required_service_mask;
    unsigned int optional_service_mask;
    unsigned int available_service_mask;
    unsigned int missing_required_mask;
    unsigned int downgraded_optional_mask;
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
unsigned int kain_runtime_contract_service_mask(const KainRuntimeContractBundle* bundle);
void kain_runtime_contract_validation_init(KainRuntimeContractValidation* validation);
int kain_runtime_contract_validate_startup(
    const KainRuntimeContractBundle* bundle,
    unsigned int required_service_mask,
    unsigned int optional_service_mask,
    KainRuntimeContractValidation* validation
);
void kain_runtime_contract_format_service_mask(
    unsigned int service_mask,
    char* out,
    size_t out_cap
);

/*
 * Populate Service Registry
 *
 * Registers all current native runtime services with the canonical service
 * registry. This enables registry-driven service resolution while preserving
 * existing service handling.
 */
void kain_runtime_contract_populate_service_registry(KainServiceRegistry* registry);

/*
 * Check Service Availability
 *
 * Queries the service registry for service availability. Falls back to
 * legacy hardcoded checks if registry is not available.
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
    unsigned int required_service_mask,
    unsigned int optional_service_mask,
    KainStartupValidationResult* result
);

#endif /* KAIN_RUNTIME_CONTRACT_H */
