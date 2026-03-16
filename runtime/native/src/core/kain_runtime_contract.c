#include "../../include/kain_runtime_contract.h"

#ifdef _WIN32
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
            if (_stricmp(service_name, "native.app-host") == 0) {
                bundle->has_native_app_host = 1;
            } else if (_stricmp(service_name, "native.input") == 0) {
                bundle->has_native_input = 1;
            } else if (_stricmp(service_name, "native.viewport") == 0) {
                bundle->has_native_viewport = 1;
            } else if (_stricmp(service_name, "native.asset.gltf") == 0) {
                bundle->has_native_asset_gltf = 1;
            } else if (_stricmp(service_name, "native.ui.compiled-bundle") == 0) {
                bundle->has_native_ui_compiled_bundle = 1;
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
    bundle->core_service_count =
        (bundle->has_native_app_host ? 1 : 0) +
        (bundle->has_native_input ? 1 : 0) +
        (bundle->has_native_viewport ? 1 : 0);
    bundle->optional_service_count =
        (bundle->has_native_asset_gltf ? 1 : 0) +
        (bundle->has_native_ui_compiled_bundle ? 1 : 0);
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
#endif
