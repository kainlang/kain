#ifndef KAIN_RUNTIME_CONTRACT_H
#define KAIN_RUNTIME_CONTRACT_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
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

#define KAIN_RUNTIME_SERVICE_CORE_MASK ( \
    KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST | \
    KAIN_RUNTIME_SERVICE_NATIVE_INPUT | \
    KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT \
)

#define KAIN_RUNTIME_SERVICE_OPTIONAL_MASK ( \
    KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF | \
    KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED \
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
    unsigned int service_mask;
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
#endif

#endif
