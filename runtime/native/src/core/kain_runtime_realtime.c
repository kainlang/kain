#include "../../include/kain_runtime_realtime.h"

#ifdef _WIN32
static const char* kain_runtime_realtime_find_substring(
    const char* haystack,
    const char* haystack_end,
    const char* needle
) {
    size_t needle_len = needle ? strlen(needle) : 0u;
    const char* cursor = haystack;
    if (!haystack || !haystack_end || !needle || needle_len == 0u) {
        return NULL;
    }
    while (cursor && cursor + needle_len <= haystack_end) {
        if (memcmp(cursor, needle, needle_len) == 0) {
            return cursor;
        }
        ++cursor;
    }
    return NULL;
}

static const char* kain_runtime_realtime_skip_ws(const char* cursor, const char* end) {
    while (cursor && cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        ++cursor;
    }
    return cursor;
}

static const char* kain_runtime_realtime_find_matching(
    const char* start,
    const char* end,
    char open_char,
    char close_char
) {
    int depth = 0;
    const char* cursor = start;
    if (!start || !end || start >= end || *start != open_char) {
        return NULL;
    }
    while (cursor < end) {
        if (*cursor == open_char) {
            ++depth;
        } else if (*cursor == close_char) {
            --depth;
            if (depth == 0) {
                return cursor;
            }
        }
        ++cursor;
    }
    return NULL;
}

static const char* kain_runtime_realtime_find_value_start(
    const char* scope_start,
    const char* scope_end,
    const char* key
) {
    const char* key_pos = kain_runtime_realtime_find_substring(scope_start, scope_end, key);
    const char* colon;
    if (!key_pos) {
        return NULL;
    }
    colon = key_pos + strlen(key);
    while (colon < scope_end && *colon != ':') {
        ++colon;
    }
    if (colon >= scope_end) {
        return NULL;
    }
    return kain_runtime_realtime_skip_ws(colon + 1, scope_end);
}

static void kain_runtime_realtime_copy_cstr(char* out, size_t out_cap, const char* value) {
    if (!out || out_cap == 0u) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    strncpy_s(out, out_cap, value, _TRUNCATE);
}

static void kain_runtime_realtime_copy_string_value(
    const char* value_start,
    const char* scope_end,
    char* out,
    size_t out_cap
) {
    const char* end_quote;
    size_t length;
    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!value_start || value_start >= scope_end || *value_start != '"') {
        return;
    }
    ++value_start;
    end_quote = value_start;
    while (end_quote < scope_end && *end_quote != '"') {
        ++end_quote;
    }
    length = (size_t)(end_quote - value_start);
    if (length >= out_cap) {
        length = out_cap - 1u;
    }
    memcpy(out, value_start, length);
    out[length] = '\0';
}

static void kain_runtime_realtime_extract_string_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    char* out,
    size_t out_cap
) {
    const char* value_start =
        kain_runtime_realtime_find_value_start(scope_start, scope_end, key);
    kain_runtime_realtime_copy_string_value(value_start, scope_end, out, out_cap);
}

static int kain_runtime_realtime_count_array_objects(
    const char* array_start,
    const char* array_end
) {
    int count = 0;
    const char* cursor = array_start;
    while (cursor && cursor < array_end) {
        const char* object_start = kain_runtime_realtime_find_substring(cursor, array_end, "{");
        if (!object_start || object_start >= array_end) {
            break;
        }
        ++count;
        cursor = object_start + 1;
    }
    return count;
}

static void kain_runtime_realtime_join_string_array(
    const char* array_start,
    const char* array_end,
    char* out,
    size_t out_cap
) {
    const char* cursor = array_start;
    int wrote_any = 0;
    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!array_start || !array_end || array_start >= array_end || *array_start != '[') {
        return;
    }
    while (cursor < array_end) {
        char value[96];
        cursor = kain_runtime_realtime_find_substring(cursor, array_end, "\"");
        if (!cursor || cursor >= array_end) {
            break;
        }
        kain_runtime_realtime_copy_string_value(cursor, array_end, value, sizeof(value));
        if (value[0]) {
            if (wrote_any) {
                strncat_s(out, out_cap, ", ", _TRUNCATE);
            }
            strncat_s(out, out_cap, value, _TRUNCATE);
            wrote_any = 1;
        }
        ++cursor;
    }
}

void kain_runtime_realtime_init(KainRuntimeRealtimeBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

int kain_runtime_realtime_load_from_json(const char* json, KainRuntimeRealtimeBundle* bundle) {
    const char* json_end;
    const char* target_value;
    const char* scenes_value;
    const char* scenes_end;
    const char* materials_value;
    const char* materials_end;
    const char* shader_refs_value;
    const char* shader_refs_end;
    const char* first_scene_start;
    const char* first_scene_end;
    const char* material_refs_value;
    const char* material_refs_end;
    const char* shader_keys_value;
    const char* shader_keys_end;
    if (!json || !bundle) {
        return 0;
    }
    kain_runtime_realtime_init(bundle);
    json_end = json + strlen(json);

    target_value = kain_runtime_realtime_find_value_start(json, json_end, "\"target\"");
    kain_runtime_realtime_copy_string_value(
        target_value,
        json_end,
        bundle->target,
        sizeof(bundle->target)
    );
    bundle->valid_for_native_viewport = (strcmp(bundle->target, "llvm") == 0);

    scenes_value = kain_runtime_realtime_find_value_start(json, json_end, "\"scenes\"");
    if (scenes_value && scenes_value < json_end && *scenes_value == '[') {
        scenes_end = kain_runtime_realtime_find_matching(scenes_value, json_end, '[', ']');
        if (scenes_end) {
            bundle->scene_count = kain_runtime_realtime_count_array_objects(scenes_value, scenes_end);
            first_scene_start =
                kain_runtime_realtime_find_substring(scenes_value, scenes_end, "{");
            if (first_scene_start) {
                first_scene_end = kain_runtime_realtime_find_matching(
                    first_scene_start,
                    scenes_end,
                    '{',
                    '}'
                );
                if (first_scene_end) {
                    kain_runtime_realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"viewport_node\"",
                        bundle->primary_viewport_node,
                        sizeof(bundle->primary_viewport_node)
                    );
                    kain_runtime_realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"scene\"",
                        bundle->primary_scene,
                        sizeof(bundle->primary_scene)
                    );
                    kain_runtime_realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"title\"",
                        bundle->primary_title,
                        sizeof(bundle->primary_title)
                    );

                    material_refs_value = kain_runtime_realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"material_refs\""
                    );
                    if (material_refs_value && *material_refs_value == '[') {
                        material_refs_end = kain_runtime_realtime_find_matching(
                            material_refs_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (material_refs_end) {
                            kain_runtime_realtime_join_string_array(
                                material_refs_value,
                                material_refs_end,
                                bundle->primary_material_refs,
                                sizeof(bundle->primary_material_refs)
                            );
                        }
                    }

                    shader_keys_value = kain_runtime_realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"shader_bundle_ref_keys\""
                    );
                    if (shader_keys_value && *shader_keys_value == '[') {
                        shader_keys_end = kain_runtime_realtime_find_matching(
                            shader_keys_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (shader_keys_end) {
                            kain_runtime_realtime_join_string_array(
                                shader_keys_value,
                                shader_keys_end,
                                bundle->primary_shader_ref_keys,
                                sizeof(bundle->primary_shader_ref_keys)
                            );
                        }
                    }
                }
            }
        }
    }

    materials_value = kain_runtime_realtime_find_value_start(json, json_end, "\"materials\"");
    if (materials_value && materials_value < json_end && *materials_value == '[') {
        materials_end = kain_runtime_realtime_find_matching(materials_value, json_end, '[', ']');
        if (materials_end) {
            bundle->material_count = kain_runtime_realtime_count_array_objects(materials_value, materials_end);
        }
    }

    shader_refs_value = kain_runtime_realtime_find_value_start(json, json_end, "\"shader_bundle_refs\"");
    if (shader_refs_value && shader_refs_value < json_end && *shader_refs_value == '[') {
        shader_refs_end = kain_runtime_realtime_find_matching(shader_refs_value, json_end, '[', ']');
        if (shader_refs_end) {
            bundle->shader_ref_count = kain_runtime_realtime_count_array_objects(shader_refs_value, shader_refs_end);
        }
    }

    bundle->loaded = 1;
    return 1;
}

int kain_runtime_realtime_load_from_path(const char* path, KainRuntimeRealtimeBundle* bundle) {
    FILE* file = NULL;
    long length = 0;
    char* json = NULL;
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
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    json = (char*)malloc((size_t)length + 1u);
    if (!json) {
        fclose(file);
        return 0;
    }
    if (fread(json, 1, (size_t)length, file) != (size_t)length) {
        free(json);
        fclose(file);
        return 0;
    }
    json[length] = '\0';
    fclose(file);
    loaded = kain_runtime_realtime_load_from_json(json, bundle);
    if (loaded) {
        kain_runtime_realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "path"
        );
        kain_runtime_realtime_copy_cstr(
            bundle->source_path,
            sizeof(bundle->source_path),
            path
        );
    }
    free(json);
    return loaded;
}

int kain_runtime_realtime_load_from_env(const char* env_name, KainRuntimeRealtimeBundle* bundle) {
    char* path = NULL;
    int loaded = 0;
    if (!env_name || !bundle) {
        return 0;
    }
    path = kain_env_dup(env_name);
    if (!path || !path[0]) {
        kain_env_free(path);
        return 0;
    }
    loaded = kain_runtime_realtime_load_from_path(path, bundle);
    if (loaded) {
        kain_runtime_realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "env"
        );
    }
    kain_env_free(path);
    return loaded;
}

int kain_runtime_realtime_load_for_current_process(
    const char* env_name,
    KainRuntimeRealtimeBundle* bundle
) {
    char sidecar_path[KAIN_RUNTIME_REALTIME_MAX_PATH];
    if (!bundle) {
        return 0;
    }
    if (kain_runtime_realtime_load_from_env(env_name, bundle)) {
        return 1;
    }
    if (kain_win32_get_executable_sidecar_path(
            KAIN_RUNTIME_REALTIME_SIDECAR_SUFFIX,
            sidecar_path,
            sizeof(sidecar_path)
        ) &&
        kain_runtime_realtime_load_from_path(sidecar_path, bundle)) {
        kain_runtime_realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "sidecar"
        );
        return 1;
    }
    kain_runtime_realtime_init(bundle);
    return 0;
}
#endif
