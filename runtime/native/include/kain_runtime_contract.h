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

#define KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST        (UINT64_C(1) << 0)
#define KAIN_RUNTIME_SERVICE_NATIVE_INPUT           (UINT64_C(1) << 1)
#define KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT        (UINT64_C(1) << 2)
#define KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF      (UINT64_C(1) << 3)
#define KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED     (UINT64_C(1) << 4)
#define KAIN_RUNTIME_SERVICE_GFX_COMPUTE            (UINT64_C(1) << 5)
#define KAIN_RUNTIME_SERVICE_SCENE_RUNTIME          (UINT64_C(1) << 6)
#define KAIN_RUNTIME_SERVICE_SCENE_QUERY            (UINT64_C(1) << 7)
#define KAIN_RUNTIME_SERVICE_SCENE_MUTATION         (UINT64_C(1) << 8)
#define KAIN_RUNTIME_SERVICE_RUNTIME_INSPECTION     (UINT64_C(1) << 9)
#define KAIN_RUNTIME_SERVICE_DEVICE_REFLECTION      (UINT64_C(1) << 10)
#define KAIN_RUNTIME_SERVICE_ASSET_INGESTION        (UINT64_C(1) << 11)
#define KAIN_RUNTIME_SERVICE_IO_LOOP                (UINT64_C(1) << 12)
#define KAIN_RUNTIME_SERVICE_IO_FS                  (UINT64_C(1) << 13)
#define KAIN_RUNTIME_SERVICE_IO_NET                 (UINT64_C(1) << 14)
#define KAIN_RUNTIME_SERVICE_IO_PROCESS             (UINT64_C(1) << 15)
#define KAIN_RUNTIME_SERVICE_IO_TIMERS              (UINT64_C(1) << 16)
#define KAIN_RUNTIME_SERVICE_SCRIPT_QUICKJS         (UINT64_C(1) << 17)
#define KAIN_RUNTIME_SERVICE_AUDIO_BACKEND          (UINT64_C(1) << 18)
#define KAIN_RUNTIME_SERVICE_AUDIO_GRAPH            (UINT64_C(1) << 19)
#define KAIN_RUNTIME_SERVICE_AUDIO_DEVICE           (UINT64_C(1) << 20)
#define KAIN_RUNTIME_SERVICE_AUDIO_ASSETS           (UINT64_C(1) << 21)
#define KAIN_RUNTIME_SERVICE_WASM_RUNTIME_LIGHT     (UINT64_C(1) << 22)
#define KAIN_RUNTIME_SERVICE_WASM_RUNTIME_FULL      (UINT64_C(1) << 23)
#define KAIN_RUNTIME_SERVICE_WASM_MODULE            (UINT64_C(1) << 24)
#define KAIN_RUNTIME_SERVICE_WASM_WASI              (UINT64_C(1) << 25)
#define KAIN_RUNTIME_SERVICE_ALLOCATOR_MIMALLOC     (UINT64_C(1) << 26)
#define KAIN_RUNTIME_SERVICE_ALLOCATOR_RPMALLOC     (UINT64_C(1) << 27)
#define KAIN_RUNTIME_SERVICE_GFX_BACKEND_BGFX       (UINT64_C(1) << 28)
#define KAIN_RUNTIME_SERVICE_GFX_BACKEND_FILAMENT   (UINT64_C(1) << 29)
#define KAIN_RUNTIME_SERVICE_GFX_BACKEND_DILIGENT   (UINT64_C(1) << 30)
#define KAIN_RUNTIME_SERVICE_ASSET_TEXTURE_BIMG     (UINT64_C(1) << 31)
#define KAIN_RUNTIME_SERVICE_UI_LAYOUT_YOGA         (UINT64_C(1) << 32)
#define KAIN_RUNTIME_SERVICE_UI_RENDER_SKIA         (UINT64_C(1) << 33)
#define KAIN_RUNTIME_SERVICE_UI_BACKEND_IMGUI       (UINT64_C(1) << 34)
#define KAIN_RUNTIME_SERVICE_UI_BACKEND_RMLUI       (UINT64_C(1) << 35)
#define KAIN_RUNTIME_SERVICE_UI_BACKEND_SLINT       (UINT64_C(1) << 36)
#define KAIN_RUNTIME_SERVICE_UI_BACKEND_QT          (UINT64_C(1) << 37)
#define KAIN_RUNTIME_SERVICE_UI_SURFACE_BROWSER_CEF (UINT64_C(1) << 38)
#define KAIN_RUNTIME_SERVICE_UI_DEVTOOLS            (UINT64_C(1) << 39)

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
    KAIN_RUNTIME_SERVICE_ASSET_INGESTION | \
    KAIN_RUNTIME_SERVICE_IO_LOOP | \
    KAIN_RUNTIME_SERVICE_IO_FS | \
    KAIN_RUNTIME_SERVICE_IO_NET | \
    KAIN_RUNTIME_SERVICE_IO_PROCESS | \
    KAIN_RUNTIME_SERVICE_IO_TIMERS | \
    KAIN_RUNTIME_SERVICE_SCRIPT_QUICKJS | \
    KAIN_RUNTIME_SERVICE_AUDIO_BACKEND | \
    KAIN_RUNTIME_SERVICE_AUDIO_GRAPH | \
    KAIN_RUNTIME_SERVICE_AUDIO_DEVICE | \
    KAIN_RUNTIME_SERVICE_AUDIO_ASSETS | \
    KAIN_RUNTIME_SERVICE_WASM_RUNTIME_LIGHT | \
    KAIN_RUNTIME_SERVICE_WASM_RUNTIME_FULL | \
    KAIN_RUNTIME_SERVICE_WASM_MODULE | \
    KAIN_RUNTIME_SERVICE_WASM_WASI | \
    KAIN_RUNTIME_SERVICE_ALLOCATOR_MIMALLOC | \
    KAIN_RUNTIME_SERVICE_ALLOCATOR_RPMALLOC | \
    KAIN_RUNTIME_SERVICE_GFX_BACKEND_BGFX | \
    KAIN_RUNTIME_SERVICE_GFX_BACKEND_FILAMENT | \
    KAIN_RUNTIME_SERVICE_GFX_BACKEND_DILIGENT | \
    KAIN_RUNTIME_SERVICE_ASSET_TEXTURE_BIMG | \
    KAIN_RUNTIME_SERVICE_UI_LAYOUT_YOGA | \
    KAIN_RUNTIME_SERVICE_UI_RENDER_SKIA | \
    KAIN_RUNTIME_SERVICE_UI_BACKEND_IMGUI | \
    KAIN_RUNTIME_SERVICE_UI_BACKEND_RMLUI | \
    KAIN_RUNTIME_SERVICE_UI_BACKEND_SLINT | \
    KAIN_RUNTIME_SERVICE_UI_BACKEND_QT | \
    KAIN_RUNTIME_SERVICE_UI_SURFACE_BROWSER_CEF | \
    KAIN_RUNTIME_SERVICE_UI_DEVTOOLS \
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
    int has_io_loop;
    int has_io_fs;
    int has_io_net;
    int has_io_process;
    int has_io_timers;
    int has_script_quickjs;
    int has_audio_backend;
    int has_audio_graph;
    int has_audio_device;
    int has_audio_assets;
    int has_wasm_runtime_light;
    int has_wasm_runtime_full;
    int has_wasm_module;
    int has_wasm_wasi;
    int has_allocator_mimalloc;
    int has_allocator_rpmalloc;
    int has_gfx_backend_bgfx;
    int has_gfx_backend_filament;
    int has_gfx_backend_diligent;
    int has_asset_texture_bimg;
    int has_ui_layout_yoga;
    int has_ui_render_skia;
    int has_ui_backend_imgui;
    int has_ui_backend_rmlui;
    int has_ui_backend_slint;
    int has_ui_backend_qt;
    int has_ui_surface_browser_cef;
    int has_ui_devtools;
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
    KainRuntimeServiceMask required_service_mask,
    KainRuntimeServiceMask optional_service_mask,
    KainStartupValidationResult* result
);

#endif /* KAIN_RUNTIME_CONTRACT_H */
