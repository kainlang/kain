#ifndef KAIN_RUNTIME_CONTRACT_H
#define KAIN_RUNTIME_CONTRACT_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_RUNTIME_CONTRACT_ENV "KAIN_RUNTIME_CONTRACT"
#define KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX ".runtime_contract.json"
#define KAIN_RUNTIME_CONTRACT_MAX_TARGET 32
#define KAIN_RUNTIME_CONTRACT_MAX_ORIGIN 32
#define KAIN_RUNTIME_CONTRACT_MAX_PATH 512

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
    char target[KAIN_RUNTIME_CONTRACT_MAX_TARGET];
    char load_origin[KAIN_RUNTIME_CONTRACT_MAX_ORIGIN];
    char source_path[KAIN_RUNTIME_CONTRACT_MAX_PATH];
} KainRuntimeContractBundle;

void kain_runtime_contract_init(KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_json(const char* json, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_path(const char* path, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_from_env(const char* env_name, KainRuntimeContractBundle* bundle);
int kain_runtime_contract_load_for_current_process(
    const char* env_name,
    KainRuntimeContractBundle* bundle
);
#endif

#endif
