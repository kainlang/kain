#include "../../include/kain_runtime_contract.h"
#include "../../include/kain_runtime_services.h"

#ifdef _WIN32
typedef struct {
    const char* key;
    unsigned int mask;
    int is_core;
} KainRuntimeServiceSpec;

static const KainRuntimeServiceSpec g_kain_runtime_service_specs[] = {
    {"native.app-host", KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST, 1},
    {"native.input", KAIN_RUNTIME_SERVICE_NATIVE_INPUT, 1},
    {"native.viewport", KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT, 1},
    {"native.asset.gltf", KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF, 0},
    {"native.ui.compiled-bundle", KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED, 0},
};

static const size_t g_kain_runtime_service_spec_count =
    sizeof(g_kain_runtime_service_specs) / sizeof(g_kain_runtime_service_specs[0]);

static const char* kain_runtime_contract_find_substring(
    const char* start,
    const char* end,
    const char* needle
) {
    size_t needle_len;
    const char* cursor;

    if (!start || !end || !needle) {
        return NULL;
    }

    needle_len = strlen(needle);
    if (needle_len == 0 || start >= end || (size_t)(end - start) < needle_len) {
        return NULL;
    }

    for (cursor = start; cursor + needle_len <= end; ++cursor) {
        if (memcmp(cursor, needle, needle_len) == 0) {
            return cursor;
        }
    }
    return NULL;
}

static const char* kain_runtime_contract_skip_ws(const char* cursor, const char* end) {
    while (cursor && cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        cursor += 1;
    }
    return cursor;
}

static const char* kain_runtime_contract_find_matching(
    const char* start,
    const char* end,
    char open_ch,
    char close_ch
) {
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char* cursor;

    if (!start || start >= end || *start != open_ch) {
        return NULL;
    }

    for (cursor = start; cursor < end; ++cursor) {
        char ch = *cursor;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            in_string = 1;
            continue;
        }

        if (ch == open_ch) {
            depth += 1;
        } else if (ch == close_ch) {
            depth -= 1;
            if (depth == 0) {
                return cursor;
            }
        }
    }

    return NULL;
}

static const char* kain_runtime_contract_find_value_start(
    const char* scope_start,
    const char* scope_end,
    const char* key
) {
    const char* key_pos = kain_runtime_contract_find_substring(scope_start, scope_end, key);
    const char* colon;

    if (!key_pos) {
        return NULL;
    }

    colon = key_pos + strlen(key);
    while (colon < scope_end && *colon != ':') {
        colon += 1;
    }
    if (colon >= scope_end || *colon != ':') {
        return NULL;
    }

    return kain_runtime_contract_skip_ws(colon + 1, scope_end);
}

static void kain_runtime_contract_copy_string_value(
    const char* value_start,
    const char* scope_end,
    char* out,
    size_t out_cap
) {
    const char* cursor;
    size_t written = 0;

    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!value_start || value_start >= scope_end) {
        return;
    }

    if (value_start + 4 <= scope_end && memcmp(value_start, "null", 4) == 0) {
        return;
    }
    if (*value_start != '"') {
        return;
    }

    cursor = value_start + 1;
    while (cursor < scope_end && *cursor != '"' && written + 1 < out_cap) {
        char ch = *cursor++;
        if (ch == '\\' && cursor < scope_end) {
            char escaped = *cursor++;
            switch (escaped) {
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case '\\': ch = '\\'; break;
                case '"': ch = '"'; break;
                default: ch = escaped; break;
            }
        }
        out[written++] = ch;
    }
    out[written] = '\0';
}

static void kain_runtime_contract_copy_cstr(char* out, size_t out_cap, const char* value) {
    size_t length;
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!value) {
        return;
    }
    length = strlen(value);
    if (length >= out_cap) {
        length = out_cap - 1;
    }
    memcpy(out, value, length);
    out[length] = '\0';
}

static int kain_runtime_contract_count_bits(unsigned int value) {
    int count = 0;
    while (value) {
        count += (value & 1u) != 0u ? 1 : 0;
        value >>= 1;
    }
    return count;
}

static const KainRuntimeServiceSpec* kain_runtime_contract_find_service_spec(const char* key) {
    size_t i;
    if (!key || !key[0]) {
        return NULL;
    }
    for (i = 0; i < g_kain_runtime_service_spec_count; ++i) {
        if (_stricmp(g_kain_runtime_service_specs[i].key, key) == 0) {
            return &g_kain_runtime_service_specs[i];
        }
    }
    return NULL;
}

static void kain_runtime_contract_append_message(
    char* out,
    size_t out_cap,
    const char* value
) {
    size_t length;
    size_t value_length;
    if (!out || out_cap == 0 || !value || !value[0]) {
        return;
    }
    length = strlen(out);
    value_length = strlen(value);
    if (length + value_length + 1 >= out_cap) {
        value_length = out_cap - length - 1;
    }
    if (value_length > 0) {
        memcpy(out + length, value, value_length);
        out[length + value_length] = '\0';
    }
}

static void kain_runtime_contract_add_warning(
    KainRuntimeContractValidation* validation,
    const char* warning
) {
    if (!validation || !warning || !warning[0]) {
        return;
    }
    if (validation->warning_count >= KAIN_RUNTIME_CONTRACT_MAX_DIAGNOSTICS) {
        return;
    }
    kain_runtime_contract_copy_cstr(
        validation->warnings[validation->warning_count],
        sizeof(validation->warnings[validation->warning_count]),
        warning
    );
    validation->warning_count += 1;
}

static void kain_runtime_contract_extract_string_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    char* out,
    size_t out_cap
) {
    const char* value_start =
        kain_runtime_contract_find_value_start(scope_start, scope_end, key);
    kain_runtime_contract_copy_string_value(value_start, scope_end, out, out_cap);
}

static int kain_runtime_contract_count_array_objects(
    const char* array_start,
    const char* array_end
) {
    int count = 0;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char* cursor;

    if (!array_start || !array_end || array_start >= array_end || *array_start != '[') {
        return 0;
    }

    for (cursor = array_start + 1; cursor < array_end; ++cursor) {
        char ch = *cursor;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            in_string = 1;
            continue;
        }

        if (ch == '{') {
            if (depth == 0) {
                count += 1;
            }
            depth += 1;
        } else if (ch == '}') {
            if (depth > 0) {
                depth -= 1;
            }
        }
    }

    return count;
}

static void kain_runtime_contract_analyze_services(
    const char* array_start,
    const char* array_end,
    KainRuntimeContractBundle* bundle
) {
    const char* cursor;

    if (!array_start || !array_end || !bundle || *array_start != '[') {
        return;
    }

    cursor = array_start + 1;
    while (cursor < array_end) {
        char service_name[96];
        char lane_name[48];
        const char* object_start = kain_runtime_contract_find_substring(cursor, array_end, "{");
        const char* object_end;
        if (!object_start || object_start >= array_end) {
            break;
        }
        object_end = kain_runtime_contract_find_matching(object_start, array_end, '{', '}');
        if (!object_end) {
            break;
        }

        service_name[0] = '\0';
        lane_name[0] = '\0';
        kain_runtime_contract_extract_string_field(
            object_start,
            object_end,
            "\"service\"",
            service_name,
            sizeof(service_name)
        );
        kain_runtime_contract_extract_string_field(
            object_start,
            object_end,
            "\"lane\"",
            lane_name,
            sizeof(lane_name)
        );

        if (service_name[0]) {
            const KainRuntimeServiceSpec* spec =
                kain_runtime_contract_find_service_spec(service_name);
            if (spec) {
                bundle->service_mask |= spec->mask;
            }
        }

        cursor = object_end + 1;
    }
}

static void kain_runtime_contract_finalize(KainRuntimeContractBundle* bundle) {
    if (!bundle) {
        return;
    }

    bundle->target_is_llvm = bundle->target[0] && _stricmp(bundle->target, "llvm") == 0;
    bundle->has_native_app_host =
        (bundle->service_mask & KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST) != 0u;
    bundle->has_native_input =
        (bundle->service_mask & KAIN_RUNTIME_SERVICE_NATIVE_INPUT) != 0u;
    bundle->has_native_viewport =
        (bundle->service_mask & KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT) != 0u;
    bundle->has_native_asset_gltf =
        (bundle->service_mask & KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF) != 0u;
    bundle->has_native_ui_compiled_bundle =
        (bundle->service_mask & KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED) != 0u;
    bundle->core_service_count = kain_runtime_contract_count_bits(
        bundle->service_mask & KAIN_RUNTIME_SERVICE_CORE_MASK
    );
    bundle->optional_service_count = kain_runtime_contract_count_bits(
        bundle->service_mask & KAIN_RUNTIME_SERVICE_OPTIONAL_MASK
    );
    bundle->missing_core_service_count = 3 - bundle->core_service_count;
    if (bundle->missing_core_service_count < 0) {
        bundle->missing_core_service_count = 0;
    }
    bundle->valid_for_raw_native =
        bundle->target_is_llvm && bundle->core_service_count == 3;
}

void kain_runtime_contract_init(KainRuntimeContractBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

int kain_runtime_contract_load_from_json(
    const char* json,
    KainRuntimeContractBundle* bundle
) {
    const char* json_end;
    const char* target_value;
    const char* capabilities_value;
    const char* services_value;
    const char* items_value;
    const char* capabilities_end;
    const char* services_end;
    const char* items_end;

    if (!json || !bundle) {
        return 0;
    }

    kain_runtime_contract_init(bundle);
    json_end = json + strlen(json);

    target_value = kain_runtime_contract_find_value_start(json, json_end, "\"target\"");
    kain_runtime_contract_copy_string_value(
        target_value,
        json_end,
        bundle->target,
        sizeof(bundle->target)
    );

    capabilities_value = kain_runtime_contract_find_value_start(
        json,
        json_end,
        "\"required_capabilities\""
    );
    if (capabilities_value && capabilities_value < json_end && *capabilities_value == '[') {
        capabilities_end = kain_runtime_contract_find_matching(
            capabilities_value,
            json_end,
            '[',
            ']'
        );
        if (capabilities_end) {
            bundle->required_capability_count =
                kain_runtime_contract_count_array_objects(capabilities_value, capabilities_end);
        }
    }

    services_value = kain_runtime_contract_find_value_start(
        json,
        json_end,
        "\"service_bindings\""
    );
    if (services_value && services_value < json_end && *services_value == '[') {
        services_end = kain_runtime_contract_find_matching(
            services_value,
            json_end,
            '[',
            ']'
        );
        if (services_end) {
            bundle->service_count =
                kain_runtime_contract_count_array_objects(services_value, services_end);
            kain_runtime_contract_analyze_services(services_value, services_end, bundle);
        }
    }

    items_value = kain_runtime_contract_find_value_start(json, json_end, "\"items\"");
    if (items_value && items_value < json_end && *items_value == '[') {
        items_end = kain_runtime_contract_find_matching(items_value, json_end, '[', ']');
        if (items_end) {
            bundle->item_count =
                kain_runtime_contract_count_array_objects(items_value, items_end);
        }
    }

    bundle->loaded = bundle->target[0] != '\0' || bundle->service_count > 0 || bundle->item_count > 0;
    kain_runtime_contract_finalize(bundle);
    return bundle->loaded;
}

int kain_runtime_contract_load_from_path(
    const char* path,
    KainRuntimeContractBundle* bundle
) {
    FILE* file = NULL;
    long file_size;
    char* json = NULL;
    size_t bytes_read;
    int loaded = 0;

    if (!path || !path[0] || !bundle) {
        return 0;
    }

    if (fopen_s(&file, path, "rb") != 0 || !file) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    json = (char*)malloc((size_t)file_size + 1);
    if (!json) {
        fclose(file);
        return 0;
    }

    bytes_read = fread(json, 1, (size_t)file_size, file);
    fclose(file);
    if (bytes_read != (size_t)file_size) {
        free(json);
        return 0;
    }
    json[file_size] = '\0';

    loaded = kain_runtime_contract_load_from_json(json, bundle);
    if (loaded) {
        kain_runtime_contract_copy_cstr(
            bundle->source_path,
            sizeof(bundle->source_path),
            path
        );
        if (!bundle->load_origin[0]) {
            kain_runtime_contract_copy_cstr(
                bundle->load_origin,
                sizeof(bundle->load_origin),
                "path"
            );
        }
    }

    free(json);
    return loaded;
}

int kain_runtime_contract_load_from_env(
    const char* env_name,
    KainRuntimeContractBundle* bundle
) {
    char* path = NULL;
    int loaded = 0;

    if (!env_name || !env_name[0] || !bundle) {
        return 0;
    }

    path = kain_env_dup(env_name);
    if (!path) {
        return 0;
    }

    loaded = kain_runtime_contract_load_from_path(path, bundle);
    if (loaded) {
        kain_runtime_contract_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "env"
        );
    }

    kain_env_free(path);
    return loaded;
}

int kain_runtime_contract_load_for_current_process(
    const char* env_name,
    KainRuntimeContractBundle* bundle
) {
    char sidecar_path[KAIN_RUNTIME_CONTRACT_MAX_PATH];

    if (!bundle) {
        return 0;
    }

    if (kain_runtime_contract_load_from_env(env_name, bundle)) {
        return 1;
    }

    if (kain_win32_get_executable_sidecar_path(
            KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX,
            sidecar_path,
            sizeof(sidecar_path)
        ) &&
        kain_runtime_contract_load_from_path(sidecar_path, bundle)) {
        kain_runtime_contract_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "exe-sidecar"
        );
        return 1;
    }

    kain_runtime_contract_init(bundle);
    return 0;
}

unsigned int kain_runtime_contract_service_mask(const KainRuntimeContractBundle* bundle) {
    if (!bundle) {
        return 0u;
    }
    return bundle->service_mask;
}

void kain_runtime_contract_validation_init(KainRuntimeContractValidation* validation) {
    if (!validation) {
        return;
    }
    ZeroMemory(validation, sizeof(*validation));
}

void kain_runtime_contract_format_service_mask(
    unsigned int service_mask,
    char* out,
    size_t out_cap
) {
    size_t i;
    int wrote_any = 0;
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    for (i = 0; i < g_kain_runtime_service_spec_count; ++i) {
        const KainRuntimeServiceSpec* spec = &g_kain_runtime_service_specs[i];
        if ((service_mask & spec->mask) == 0u) {
            continue;
        }
        if (wrote_any) {
            kain_runtime_contract_append_message(out, out_cap, ", ");
        }
        kain_runtime_contract_append_message(out, out_cap, spec->key);
        wrote_any = 1;
    }
    if (!wrote_any) {
        kain_runtime_contract_copy_cstr(out, out_cap, "none");
    }
}

int kain_runtime_contract_validate_startup(
    const KainRuntimeContractBundle* bundle,
    unsigned int required_service_mask,
    unsigned int optional_service_mask,
    KainRuntimeContractValidation* validation
) {
    char services_buffer[192];
    KainRuntimeVersionInfo version_info;
    
    if (!validation) {
        return 0;
    }

    kain_runtime_contract_validation_init(validation);
    
    /* Get current runtime version information */
    if (kain_runtime_version_get_info(&version_info) == 0) {
        validation->runtime_abi_version = version_info.abi_version_encoded;
        kain_runtime_contract_copy_cstr(
            validation->runtime_abi_version_string,
            sizeof(validation->runtime_abi_version_string),
            version_info.abi_version_string
        );
    }
    
    validation->strict_mode = kain_env_flag(KAIN_RUNTIME_CONTRACT_STRICT_ENV, 1);
    validation->required_service_mask = required_service_mask;
    validation->optional_service_mask = optional_service_mask;
    validation->contract_present = bundle && bundle->loaded;
    validation->available_service_mask = bundle ? bundle->service_mask : 0u;
    validation->missing_required_mask =
        required_service_mask & ~validation->available_service_mask;
    validation->downgraded_optional_mask =
        optional_service_mask & ~validation->available_service_mask;

    /* Check ABI compatibility if contract is present */
    if (validation->contract_present && bundle->required_abi_version != 0) {
        validation->contract_abi_version = bundle->required_abi_version;
        kain_runtime_version_format_abi(
            bundle->required_abi_version,
            validation->contract_abi_version_string,
            sizeof(validation->contract_abi_version_string)
        );
        
        validation->abi_compatible = kain_runtime_version_check_abi_compatibility(
            bundle->required_abi_version
        );
        
        if (!validation->abi_compatible) {
            validation->fatal_error = 1;
            snprintf(
                validation->fatal_message,
                sizeof(validation->fatal_message),
                "ABI version mismatch. Runtime ABI: %s, Contract requires: %s.",
                validation->runtime_abi_version_string,
                validation->contract_abi_version_string
            );
            return 0;
        }
    } else {
        /* No ABI version in contract, assume compatible */
        validation->abi_compatible = 1;
    }

    if (!validation->contract_present) {
        if (validation->strict_mode) {
            validation->fatal_error = 1;
            snprintf(
                validation->fatal_message,
                sizeof(validation->fatal_message),
                "Runtime contract missing. Keep %s beside the executable or set %s.",
                KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX,
                KAIN_RUNTIME_CONTRACT_ENV
            );
            return 0;
        }
        kain_runtime_contract_add_warning(
            validation,
            "Runtime contract missing; running raw native lane without contract enforcement."
        );
        return 1;
    }

    if (!bundle->target_is_llvm) {
        if (validation->strict_mode) {
            validation->fatal_error = 1;
            snprintf(
                validation->fatal_message,
                sizeof(validation->fatal_message),
                "Runtime contract target mismatch. Expected llvm, got %s.",
                bundle->target[0] ? bundle->target : "unknown"
            );
            return 0;
        }
        snprintf(
            services_buffer,
            sizeof(services_buffer),
            "Runtime contract target is %s instead of llvm; continuing because %s=0.",
            bundle->target[0] ? bundle->target : "unknown",
            KAIN_RUNTIME_CONTRACT_STRICT_ENV
        );
        kain_runtime_contract_add_warning(validation, services_buffer);
    }

    if (validation->missing_required_mask != 0u) {
        kain_runtime_contract_format_service_mask(
            validation->missing_required_mask,
            services_buffer,
            sizeof(services_buffer)
        );
        if (validation->strict_mode) {
            validation->fatal_error = 1;
            snprintf(
                validation->fatal_message,
                sizeof(validation->fatal_message),
                "Runtime contract is missing required services: %s.",
                services_buffer
            );
            return 0;
        }
        snprintf(
            validation->fatal_message,
            sizeof(validation->fatal_message),
            "Missing required services but continuing because %s=0: %s.",
            KAIN_RUNTIME_CONTRACT_STRICT_ENV,
            services_buffer
        );
        kain_runtime_contract_add_warning(validation, validation->fatal_message);
        validation->fatal_message[0] = '\0';
    }

    if (validation->downgraded_optional_mask != 0u) {
        kain_runtime_contract_format_service_mask(
            validation->downgraded_optional_mask,
            services_buffer,
            sizeof(services_buffer)
        );
        snprintf(
            validation->fatal_message,
            sizeof(validation->fatal_message),
            "Optional runtime services unavailable; related features will be disabled: %s.",
            services_buffer
        );
        kain_runtime_contract_add_warning(validation, validation->fatal_message);
        validation->fatal_message[0] = '\0';
    }

    return 1;
}
#endif

/*
 * Populate Service Registry with Native Runtime Services
 *
 * This function registers all current native runtime services with the
 * canonical service registry. It preserves the existing service handling
 * while enabling registry-driven resolution.
 */
void kain_runtime_contract_populate_service_registry(KainServiceRegistry* registry) {
    if (!registry) {
        return;
    }
    
    /* Register native.app-host service */
    kain_service_registry_register(
        registry,
        KAIN_SERVICE_KEY_PLATFORM_APP_HOST,
        "Native App Host",
        "Win32 application host and window management",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );
    
    /* Register native.input service */
    kain_service_registry_register(
        registry,
        KAIN_SERVICE_KEY_PLATFORM_INPUT,
        "Native Input",
        "Win32 input capture and event handling",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );
    
    /* Register native.viewport service */
    kain_service_registry_register(
        registry,
        KAIN_SERVICE_KEY_GFX_VIEWPORT,
        "Native Viewport",
        "Win32 viewport host and OpenGL rendering",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );
    
    /* Register native.asset.gltf service */
    kain_service_registry_register(
        registry,
        KAIN_SERVICE_KEY_ASSET_GLTF,
        "glTF Asset Loader",
        "glTF 2.0 asset loading and parsing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );
    
    /* Register native.ui.compiled-bundle service */
    kain_service_registry_register(
        registry,
        KAIN_SERVICE_KEY_UI_BUNDLE,
        "Compiled UI Bundle",
        "Compiled UI bundle loading and overlay rendering",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );
}

/*
 * Lookup Service by Legacy Key
 *
 * Maps legacy service keys (native.app-host, etc.) to canonical service keys.
 * This preserves compatibility with existing contract validation while
 * enabling registry-driven resolution.
 */
static const char* kain_runtime_contract_map_legacy_service_key(const char* legacy_key) {
    if (!legacy_key) {
        return NULL;
    }
    
#ifdef _WIN32
    if (_stricmp(legacy_key, "native.app-host") == 0) {
#else
    if (strcasecmp(legacy_key, "native.app-host") == 0) {
#endif
        return KAIN_SERVICE_KEY_PLATFORM_APP_HOST;
    }
#ifdef _WIN32
    if (_stricmp(legacy_key, "native.input") == 0) {
#else
    if (strcasecmp(legacy_key, "native.input") == 0) {
#endif
        return KAIN_SERVICE_KEY_PLATFORM_INPUT;
    }
#ifdef _WIN32
    if (_stricmp(legacy_key, "native.viewport") == 0) {
#else
    if (strcasecmp(legacy_key, "native.viewport") == 0) {
#endif
        return KAIN_SERVICE_KEY_GFX_VIEWPORT;
    }
#ifdef _WIN32
    if (_stricmp(legacy_key, "native.asset.gltf") == 0) {
#else
    if (strcasecmp(legacy_key, "native.asset.gltf") == 0) {
#endif
        return KAIN_SERVICE_KEY_ASSET_GLTF;
    }
#ifdef _WIN32
    if (_stricmp(legacy_key, "native.ui.compiled-bundle") == 0) {
#else
    if (strcasecmp(legacy_key, "native.ui.compiled-bundle") == 0) {
#endif
        return KAIN_SERVICE_KEY_UI_BUNDLE;
    }
    
    return legacy_key;
}

/*
 * Check Service Availability via Registry
 *
 * Queries the service registry for service availability. Falls back to
 * legacy hardcoded checks if registry is not available.
 */
int kain_runtime_contract_is_service_available(const char* service_key) {
    KainServiceRegistry* registry;
    const char* canonical_key;
    
    if (!service_key) {
        return 0;
    }
    
    /* Map legacy key to canonical key */
    canonical_key = kain_runtime_contract_map_legacy_service_key(service_key);
    
    /* Try registry lookup first */
    registry = kain_service_registry_global();
    if (registry && registry->initialized && registry->service_count > 0) {
        return kain_service_registry_is_available(registry, canonical_key);
    }
    
    /* Fall back to legacy hardcoded checks */
#ifdef _WIN32
    if (_stricmp(service_key, "native.app-host") == 0 ||
        _stricmp(service_key, "native.input") == 0 ||
        _stricmp(service_key, "native.viewport") == 0 ||
        _stricmp(service_key, "native.asset.gltf") == 0 ||
        _stricmp(service_key, "native.ui.compiled-bundle") == 0) {
#else
    if (strcasecmp(service_key, "native.app-host") == 0 ||
        strcasecmp(service_key, "native.input") == 0 ||
        strcasecmp(service_key, "native.viewport") == 0 ||
        strcasecmp(service_key, "native.asset.gltf") == 0 ||
        strcasecmp(service_key, "native.ui.compiled-bundle") == 0) {
#endif
        return 1;
    }
    
    return 0;
}

/*
 * Enhanced Startup Validation with Diagnostic Collection
 *
 * This function extends the legacy validation with structured diagnostic
 * collection and comprehensive reporting. It populates a KainStartupValidationResult
 * with version information, service status, and collected diagnostics.
 */
int kain_runtime_contract_validate_startup_enhanced(
    const KainRuntimeContractBundle* bundle,
    unsigned int required_service_mask,
    unsigned int optional_service_mask,
    KainStartupValidationResult* result
) {
    KainRuntimeContractValidation legacy_validation;
    KainRuntimeVersionInfo version_info;
    int validation_passed;
    int i;
    
    if (!result) {
        return 0;
    }
    
    /* Initialize result structure */
    kain_startup_validation_result_init(result);
    
    /* Get runtime version information */
    if (kain_runtime_version_get_info(&version_info) == 0) {
        result->runtime_abi_version = version_info.abi_version_encoded;
        result->runtime_version = version_info.runtime_version_encoded;
    }
    
    /* Get bundle ABI version if present */
    if (bundle && bundle->loaded && bundle->required_abi_version != 0) {
        result->bundle_abi_version = bundle->required_abi_version;
    }
    
    /* Perform legacy validation */
    validation_passed = kain_runtime_contract_validate_startup(
        bundle,
        required_service_mask,
        optional_service_mask,
        &legacy_validation
    );
    
    /* Convert legacy validation to diagnostic collection */
    if (legacy_validation.fatal_error && legacy_validation.fatal_message[0]) {
        kain_diagnostic_collector_add_new(
            &result->diagnostics,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_FATAL,
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
            legacy_validation.fatal_message,
            NULL,
            bundle ? bundle->source_path : NULL
        );
    }
    
    /* Add warnings as diagnostics */
    for (i = 0; i < legacy_validation.warning_count; i++) {
        if (legacy_validation.warnings[i][0]) {
            kain_diagnostic_collector_add_new(
                &result->diagnostics,
                KAIN_DIAG_SUBSYSTEM_CONTRACT,
                KAIN_DIAG_SEVERITY_WARNING,
                KAIN_DIAG_CODE_SUCCESS,
                legacy_validation.warnings[i],
                NULL,
                bundle ? bundle->source_path : NULL
            );
        }
    }
    
    /* Add ABI compatibility diagnostic if mismatch */
    if (!legacy_validation.abi_compatible) {
        char detail[256];
        snprintf(detail, sizeof(detail),
            "Runtime ABI: %s, Contract requires: %s",
            legacy_validation.runtime_abi_version_string,
            legacy_validation.contract_abi_version_string
        );
        kain_diagnostic_collector_add_new(
            &result->diagnostics,
            KAIN_DIAG_SUBSYSTEM_COMPATIBILITY,
            KAIN_DIAG_SEVERITY_FATAL,
            KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH,
            "ABI version mismatch",
            detail,
            bundle ? bundle->source_path : NULL
        );
    }
    
    /* Count available services */
    if (bundle && bundle->loaded) {
        result->required_services_available = 
            __builtin_popcount(bundle->service_mask & required_service_mask);
        result->optional_services_available = 
            __builtin_popcount(bundle->service_mask & optional_service_mask);
        result->optional_services_degraded = 
            __builtin_popcount(optional_service_mask & ~bundle->service_mask);
    }
    
    /* Set validation status */
    result->validation_passed = validation_passed && !kain_diagnostic_collector_has_fatals(&result->diagnostics);
    
    /* Generate summary */
    if (result->validation_passed) {
        snprintf(result->summary, sizeof(result->summary),
            "Startup validation passed. %d required services, %d optional services available.",
            result->required_services_available,
            result->optional_services_available
        );
    } else {
        snprintf(result->summary, sizeof(result->summary),
            "Startup validation failed. See diagnostics for details."
        );
    }
    
    return result->validation_passed;
}
