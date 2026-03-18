#include "../../include/kain_runtime_realtime.h"
#include "../../include/kain_runtime_graphics.h"

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
        const char* object_end;
        if (!object_start || object_start >= array_end) {
            break;
        }
        object_end = kain_runtime_realtime_find_matching(object_start, array_end, '{', '}');
        if (!object_end) {
            break;
        }
        ++count;
        cursor = object_end + 1;
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

static int kain_runtime_graphics_count_string_array(
    const char* array_start,
    const char* array_end
) {
    int count = 0;
    const char* cursor = array_start;
    while (cursor && cursor < array_end) {
        const char* value_start;
        const char* value_end;
        value_start = kain_runtime_realtime_find_substring(cursor, array_end, "\"");
        if (!value_start || value_start >= array_end) {
            break;
        }
        ++count;
        value_end = value_start + 1;
        while (value_end < array_end && *value_end != '"') {
            ++value_end;
        }
        if (value_end >= array_end) {
            break;
        }
        cursor = value_end + 1;
    }
    return count;
}

static int kain_runtime_graphics_extract_int_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    int fallback
) {
    const char* value_start =
        kain_runtime_realtime_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    long value;
    if (!value_start || value_start >= scope_end) {
        return fallback;
    }
    value = strtol(value_start, &end_ptr, 10);
    if (end_ptr == value_start) {
        return fallback;
    }
    return (int)value;
}

static int kain_runtime_graphics_count_material_shader_refs(
    const char* materials_start,
    const char* materials_end
) {
    int total = 0;
    const char* cursor = materials_start;
    while (cursor && cursor < materials_end) {
        const char* material_start = kain_runtime_realtime_find_substring(cursor, materials_end, "{");
        const char* material_end;
        const char* refs_value;
        const char* refs_end;
        if (!material_start || material_start >= materials_end) {
            break;
        }
        material_end = kain_runtime_realtime_find_matching(material_start, materials_end, '{', '}');
        if (!material_end) {
            break;
        }
        refs_value = kain_runtime_realtime_find_value_start(
            material_start,
            material_end,
            "\"shader_bundle_ref_keys\""
        );
        if (refs_value && refs_value < material_end && *refs_value == '[') {
            refs_end = kain_runtime_realtime_find_matching(refs_value, material_end, '[', ']');
            if (refs_end) {
                total += kain_runtime_graphics_count_string_array(refs_value, refs_end);
            }
        }
        cursor = material_end + 1;
    }
    return total;
}

static int kain_runtime_graphics_extract_int_array(
    const char* array_start,
    const char* array_end,
    int* values,
    int max_values
) {
    const char* cursor;
    int count = 0;
    if (!array_start || !array_end || array_start >= array_end || *array_start != '[' || !values || max_values <= 0) {
        return 0;
    }
    cursor = array_start + 1;
    while (cursor < array_end && count < max_values) {
        char* end_ptr = NULL;
        long value;
        cursor = kain_runtime_realtime_skip_ws(cursor, array_end);
        if (cursor >= array_end || *cursor == ']') {
            break;
        }
        value = strtol(cursor, &end_ptr, 10);
        if (end_ptr == cursor) {
            break;
        }
        values[count++] = (int)value;
        cursor = kain_runtime_realtime_skip_ws(end_ptr, array_end);
        if (cursor < array_end && *cursor == ',') {
            ++cursor;
        }
    }
    return count;
}

static void kain_runtime_graphics_binding_init(KainRuntimeGraphicsBinding* binding) {
    if (!binding) {
        return;
    }
    ZeroMemory(binding, sizeof(*binding));
}

static int kain_runtime_graphics_parse_binding_objects(
    const char* array_start,
    const char* array_end,
    KainRuntimeGraphicsBinding* bindings,
    int max_bindings
) {
    const char* cursor = array_start;
    int count = 0;
    if (!array_start || !array_end || array_start >= array_end || *array_start != '[' || !bindings || max_bindings <= 0) {
        return 0;
    }
    while (cursor < array_end && count < max_bindings) {
        const char* object_start = kain_runtime_realtime_find_substring(cursor, array_end, "{");
        const char* object_end;
        KainRuntimeGraphicsBinding* binding;
        if (!object_start || object_start >= array_end) {
            break;
        }
        object_end = kain_runtime_realtime_find_matching(object_start, array_end, '{', '}');
        if (!object_end) {
            break;
        }
        binding = &bindings[count];
        kain_runtime_graphics_binding_init(binding);
        kain_runtime_realtime_extract_string_field(object_start, object_end, "\"key\"", binding->key, sizeof(binding->key));
        kain_runtime_realtime_extract_string_field(object_start, object_end, "\"type\"", binding->resource_type, sizeof(binding->resource_type));
        kain_runtime_realtime_extract_string_field(object_start, object_end, "\"stage\"", binding->stage, sizeof(binding->stage));
        kain_runtime_realtime_extract_string_field(object_start, object_end, "\"access\"", binding->access, sizeof(binding->access));
        binding->slot = kain_runtime_graphics_extract_int_field(object_start, object_end, "\"slot\"", count);
        ++count;
        cursor = object_end + 1;
    }
    return count;
}

static int kain_runtime_graphics_binding_is_valid(const KainRuntimeGraphicsBinding* binding) {
    if (!binding) {
        return 0;
    }
    return binding->key[0] &&
        binding->resource_type[0] &&
        binding->stage[0] &&
        binding->access[0] &&
        binding->slot >= 0;
}

static int kain_runtime_graphics_binding_stage_matches(
    const KainRuntimeGraphicsBinding* binding,
    const char* stage_name
) {
    if (!binding || !stage_name || !binding->stage[0]) {
        return 0;
    }
    return _stricmp(binding->stage, stage_name) == 0;
}

static int kain_runtime_graphics_binding_array_is_valid(
    const KainRuntimeGraphicsBinding* bindings,
    int binding_count,
    const char* required_stage
) {
    int i;
    int has_required_stage = 0;
    if (!bindings || binding_count <= 0) {
        return 0;
    }
    for (i = 0; i < binding_count; ++i) {
        if (!kain_runtime_graphics_binding_is_valid(&bindings[i])) {
            return 0;
        }
        if (required_stage &&
            kain_runtime_graphics_binding_stage_matches(&bindings[i], required_stage)) {
            has_required_stage = 1;
        }
    }
    return required_stage ? has_required_stage : 1;
}

static int kain_runtime_graphics_material_plan_is_valid(
    const KainRuntimeGraphicsMaterialPlan* material
) {
    if (!material ||
        !material->loaded ||
        !material->material_id[0] ||
        !material->source[0] ||
        material->shader_ref_count <= 0 ||
        !kain_runtime_graphics_binding_array_is_valid(
            material->resource_bindings,
            material->resource_binding_count,
            "fragment"
        )) {
        return 0;
    }
    return kain_runtime_graphics_binding_array_is_valid(
        material->resource_bindings,
        material->resource_binding_count,
        NULL
    ) && material->resource_binding_count > 0;
}

static int kain_runtime_graphics_compute_plan_is_valid(
    const KainRuntimeGraphicsComputePlan* compute
) {
    if (!compute ||
        !compute->loaded ||
        !compute->shader_key[0] ||
        !compute->module_name[0] ||
        !compute->entry_point[0] ||
        compute->workgroup_size[0] <= 0 ||
        compute->workgroup_size[1] <= 0 ||
        compute->workgroup_size[2] <= 0 ||
        compute->dispatch_size[0] <= 0 ||
        compute->dispatch_size[1] <= 0 ||
        compute->dispatch_size[2] <= 0 ||
        !kain_runtime_graphics_binding_array_is_valid(
            compute->resource_bindings,
            compute->resource_binding_count,
            "compute"
        ) ||
        compute->resource_binding_count <= 0) {
        return 0;
    }
    return 1;
}

static const char* kain_runtime_graphics_find_stage_object(
    const char* array_start,
    const char* array_end,
    const char* stage_name,
    const char** object_end
) {
    const char* cursor = array_start;
    if (!object_end) {
        return NULL;
    }
    *object_end = NULL;
    if (!array_start || !array_end || array_start >= array_end || !stage_name) {
        return NULL;
    }
    while (cursor < array_end) {
        const char* object_start = kain_runtime_realtime_find_substring(cursor, array_end, "{");
        const char* end;
        char stage[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
        if (!object_start || object_start >= array_end) {
            break;
        }
        end = kain_runtime_realtime_find_matching(object_start, array_end, '{', '}');
        if (!end) {
            break;
        }
        kain_runtime_realtime_extract_string_field(object_start, end, "\"stage\"", stage, sizeof(stage));
        if (stage[0] && _stricmp(stage, stage_name) == 0) {
            *object_end = end;
            return object_start;
        }
        cursor = end + 1;
    }
    return NULL;
}

static int kain_runtime_graphics_parse_material_plan(
    const char* material_start,
    const char* material_end,
    KainRuntimeGraphicsMaterialPlan* material
) {
    const char* bindings_value;
    const char* bindings_end;
    const char* parameters_value;
    const char* parameters_end;
    const char* shader_refs_value;
    const char* shader_refs_end;
    if (!material_start || !material_end || !material || material_start >= material_end) {
        return 0;
    }
    ZeroMemory(material, sizeof(*material));
    material->loaded = 1;
    kain_runtime_realtime_extract_string_field(material_start, material_end, "\"id\"", material->material_id, sizeof(material->material_id));
    kain_runtime_realtime_extract_string_field(material_start, material_end, "\"source\"", material->source, sizeof(material->source));
    shader_refs_value = kain_runtime_realtime_find_value_start(material_start, material_end, "\"shader_bundle_ref_keys\"");
    if (shader_refs_value && *shader_refs_value == '[') {
        shader_refs_end = kain_runtime_realtime_find_matching(shader_refs_value, material_end, '[', ']');
        if (shader_refs_end) {
            material->shader_ref_count = kain_runtime_graphics_count_string_array(
                shader_refs_value,
                shader_refs_end
            );
        }
    }
    parameters_value = kain_runtime_realtime_find_value_start(material_start, material_end, "\"parameters\"");
    if (parameters_value && *parameters_value == '[') {
        parameters_end = kain_runtime_realtime_find_matching(parameters_value, material_end, '[', ']');
        if (parameters_end) {
            material->parameter_count = kain_runtime_realtime_count_array_objects(parameters_value, parameters_end);
        }
    }
    bindings_value = kain_runtime_realtime_find_value_start(material_start, material_end, "\"resource_bindings\"");
    if (bindings_value && *bindings_value == '[') {
        bindings_end = kain_runtime_realtime_find_matching(bindings_value, material_end, '[', ']');
        if (bindings_end) {
            material->resource_binding_count = kain_runtime_graphics_parse_binding_objects(
                bindings_value,
                bindings_end,
                material->resource_bindings,
                KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS
            );
        }
    }
    return material->material_id[0] != '\0';
}

static int kain_runtime_graphics_parse_compute_plan(
    const char* shader_ref_start,
    const char* shader_ref_end,
    KainRuntimeGraphicsComputePlan* compute
) {
    const char* bindings_value;
    const char* bindings_end;
    const char* workgroup_value;
    const char* workgroup_end;
    const char* dispatch_value;
    const char* dispatch_end;
    int workgroup_values[3] = {0, 0, 0};
    int dispatch_values[3] = {0, 0, 0};
    if (!shader_ref_start || !shader_ref_end || !compute || shader_ref_start >= shader_ref_end) {
        return 0;
    }
    ZeroMemory(compute, sizeof(*compute));
    compute->loaded = 1;
    kain_runtime_realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"key\"", compute->shader_key, sizeof(compute->shader_key));
    kain_runtime_realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"module_name\"", compute->module_name, sizeof(compute->module_name));
    kain_runtime_realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"entry_point\"", compute->entry_point, sizeof(compute->entry_point));
    workgroup_value = kain_runtime_realtime_find_value_start(shader_ref_start, shader_ref_end, "\"workgroup_size\"");
    if (workgroup_value && *workgroup_value == '[') {
        workgroup_end = kain_runtime_realtime_find_matching(workgroup_value, shader_ref_end, '[', ']');
        if (workgroup_end) {
            compute->workgroup_size[0] = 0;
            compute->workgroup_size[1] = 0;
            compute->workgroup_size[2] = 0;
            kain_runtime_graphics_extract_int_array(workgroup_value, workgroup_end, workgroup_values, 3);
            compute->workgroup_size[0] = workgroup_values[0];
            compute->workgroup_size[1] = workgroup_values[1];
            compute->workgroup_size[2] = workgroup_values[2];
        }
    }
    dispatch_value = kain_runtime_realtime_find_value_start(shader_ref_start, shader_ref_end, "\"dispatch_size\"");
    if (dispatch_value && *dispatch_value == '[') {
        dispatch_end = kain_runtime_realtime_find_matching(dispatch_value, shader_ref_end, '[', ']');
        if (dispatch_end) {
            compute->dispatch_size[0] = 0;
            compute->dispatch_size[1] = 0;
            compute->dispatch_size[2] = 0;
            kain_runtime_graphics_extract_int_array(dispatch_value, dispatch_end, dispatch_values, 3);
            compute->dispatch_size[0] = dispatch_values[0];
            compute->dispatch_size[1] = dispatch_values[1];
            compute->dispatch_size[2] = dispatch_values[2];
        }
    }
    bindings_value = kain_runtime_realtime_find_value_start(shader_ref_start, shader_ref_end, "\"resource_bindings\"");
    if (bindings_value && *bindings_value == '[') {
        bindings_end = kain_runtime_realtime_find_matching(bindings_value, shader_ref_end, '[', ']');
        if (bindings_end) {
            compute->resource_binding_count = kain_runtime_graphics_parse_binding_objects(
                bindings_value,
                bindings_end,
                compute->resource_bindings,
                KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS
            );
        }
    }
    return compute->shader_key[0] != '\0' && compute->entry_point[0] != '\0';
}

static void kain_runtime_graphics_count_shader_stage_refs(
    const char* shader_refs_start,
    const char* shader_refs_end,
    int* shader_ref_count,
    int* vertex_ref_count,
    int* fragment_ref_count,
    int* compute_ref_count
) {
    const char* cursor = shader_refs_start;
    int total = 0;
    int vertex = 0;
    int fragment = 0;
    int compute = 0;
    while (cursor && cursor < shader_refs_end) {
        const char* shader_ref_start =
            kain_runtime_realtime_find_substring(cursor, shader_refs_end, "{");
        const char* shader_ref_end;
        char stage[KAIN_RUNTIME_GRAPHICS_MAX_TAG];
        if (!shader_ref_start || shader_ref_start >= shader_refs_end) {
            break;
        }
        shader_ref_end = kain_runtime_realtime_find_matching(
            shader_ref_start,
            shader_refs_end,
            '{',
            '}'
        );
        if (!shader_ref_end) {
            break;
        }
        ++total;
        kain_runtime_realtime_extract_string_field(
            shader_ref_start,
            shader_ref_end,
            "\"stage\"",
            stage,
            sizeof(stage)
        );
        if (_stricmp(stage, "vertex") == 0) {
            ++vertex;
        } else if (_stricmp(stage, "fragment") == 0) {
            ++fragment;
        } else if (_stricmp(stage, "compute") == 0) {
            ++compute;
        }
        cursor = shader_ref_end + 1;
    }
    if (shader_ref_count) {
        *shader_ref_count = total;
    }
    if (vertex_ref_count) {
        *vertex_ref_count = vertex;
    }
    if (fragment_ref_count) {
        *fragment_ref_count = fragment;
    }
    if (compute_ref_count) {
        *compute_ref_count = compute;
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

void kain_runtime_graphics_init(KainRuntimeGraphicsBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

void kain_runtime_graphics_validation_init(KainRuntimeGraphicsValidation* validation) {
    if (!validation) {
        return;
    }
    ZeroMemory(validation, sizeof(*validation));
}

static void kain_runtime_graphics_copy_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
) {
    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!bundle) {
        return;
    }
    snprintf(
        out,
        out_cap,
        "schema %d | target %s | scenes %d | materials %d | material bindings %d | shader refs %d (vertex %d fragment %d compute %d) | compute bindings %d | compute wg %d,%d,%d | compute dispatch %d,%d,%d | assets %d | caps %d | reqs %d | viewport %s/%s",
        bundle->schema_version,
        bundle->target[0] ? bundle->target : "unknown",
        bundle->scene_count,
        bundle->material_count,
        bundle->primary_material.resource_binding_count,
        bundle->shader_bundle_ref_count,
        bundle->shader_vertex_ref_count,
        bundle->shader_fragment_ref_count,
        bundle->shader_compute_ref_count,
        bundle->primary_compute.resource_binding_count,
        bundle->primary_compute.workgroup_size[0],
        bundle->primary_compute.workgroup_size[1],
        bundle->primary_compute.workgroup_size[2],
        bundle->primary_compute.dispatch_size[0],
        bundle->primary_compute.dispatch_size[1],
        bundle->primary_compute.dispatch_size[2],
        bundle->asset_count,
        bundle->tool_cap_count,
        bundle->requirement_count,
        bundle->primary_viewport_kind[0] ? bundle->primary_viewport_kind : "none",
        bundle->primary_scene[0] ? bundle->primary_scene : "none"
    );
}

void kain_runtime_graphics_format_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
) {
    kain_runtime_graphics_copy_summary(bundle, out, out_cap);
}

int kain_runtime_graphics_validate_bundle(
    const KainRuntimeGraphicsBundle* bundle,
    KainRuntimeGraphicsValidation* validation
) {
    int has_render_scene;
    int has_viewport3d;
    int has_material_bindings;
    int has_compute_plan;
    int material_binding_valid;
    int compute_plan_valid;
    int has_compute_artifacts;
    if (!bundle || !validation) {
        return 0;
    }
    kain_runtime_graphics_validation_init(validation);

    validation->loaded = bundle->loaded;
    validation->target_is_llvm = (bundle->target[0] && _stricmp(bundle->target, "llvm") == 0);
    has_compute_artifacts = bundle->shader_compute_ref_count > 0;
    has_render_scene = bundle->scene_count > 0 &&
        bundle->primary_viewport_node[0] &&
        bundle->primary_scene[0];
    has_viewport3d = bundle->primary_viewport_kind[0] &&
        _stricmp(bundle->primary_viewport_kind, "viewport3d") == 0;
    has_material_bindings = kain_runtime_graphics_material_plan_is_valid(&bundle->primary_material);
    has_compute_plan = bundle->primary_compute.loaded &&
        bundle->primary_compute.shader_key[0] &&
        bundle->primary_compute.module_name[0] &&
        bundle->primary_compute.entry_point[0];
    material_binding_valid = has_material_bindings;
    compute_plan_valid = !has_compute_artifacts ||
        (has_compute_plan && kain_runtime_graphics_compute_plan_is_valid(&bundle->primary_compute));
    validation->has_render_scene = has_render_scene;
    validation->has_viewport3d = has_viewport3d;
    validation->has_material_bindings = has_material_bindings;
    validation->has_compute_artifacts = has_compute_artifacts;
    validation->material_binding_valid = material_binding_valid;
    validation->compute_plan_valid = compute_plan_valid;
    validation->gl_lane_ready = bundle->loaded &&
        bundle->schema_version == 1 &&
        validation->target_is_llvm &&
        has_render_scene &&
        has_viewport3d &&
        material_binding_valid &&
        compute_plan_valid;
    validation->compute_metadata_valid = bundle->loaded &&
        bundle->schema_version == 1 &&
        compute_plan_valid;
    kain_runtime_graphics_copy_summary(bundle, validation->summary, sizeof(validation->summary));

    if (!bundle->loaded) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle not loaded",
            _TRUNCATE
        );
        return 0;
    }
    if (bundle->schema_version != 1) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "unsupported graphics schema version",
            _TRUNCATE
        );
        return 0;
    }
    if (!validation->target_is_llvm) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "current GL lane requires target llvm",
            _TRUNCATE
        );
        return 0;
    }
    if (!has_render_scene) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle is missing a render scene",
            _TRUNCATE
        );
        return 0;
    }
    if (!has_viewport3d) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "primary viewport kind must be viewport3d",
            _TRUNCATE
        );
        return 0;
    }
    if (!material_binding_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle is missing a valid material binding plan",
            _TRUNCATE
        );
        return 0;
    }
    if (!compute_plan_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle is missing a valid compute dispatch plan",
            _TRUNCATE
        );
        return 0;
    }

    strncpy_s(
        validation->reason,
        sizeof(validation->reason),
        "graphics bundle ready for the current GL lane",
        _TRUNCATE
    );
    return 1;
}

int kain_runtime_graphics_load_from_json(const char* json, KainRuntimeGraphicsBundle* bundle) {
    const char* json_end;
    const char* render_value;
    const char* render_end;
    const char* scenes_value;
    const char* scenes_end;
    const char* first_scene_start;
    const char* first_scene_end;
    const char* materials_value;
    const char* materials_end;
    const char* shader_refs_value;
    const char* shader_refs_end;
    const char* first_material_start;
    const char* first_material_end;
    const char* compute_ref_start;
    const char* compute_ref_end;
    const char* assets_value;
    const char* assets_end;
    const char* tool_caps_value;
    const char* tool_caps_end;
    const char* requirements_value;
    const char* requirements_end;
    const char* material_refs_value;
    const char* material_refs_end;
    const char* shader_keys_value;
    const char* shader_keys_end;
    if (!json || !bundle) {
        return 0;
    }

    kain_runtime_graphics_init(bundle);
    json_end = json + strlen(json);

    bundle->schema_version = kain_runtime_graphics_extract_int_field(
        json,
        json_end,
        "\"schema_version\"",
        0
    );
    kain_runtime_realtime_extract_string_field(
        json,
        json_end,
        "\"target\"",
        bundle->target,
        sizeof(bundle->target)
    );

    render_value = kain_runtime_realtime_find_value_start(json, json_end, "\"render\"");
    if (!render_value || *render_value != '{') {
        return 0;
    }
    render_end = kain_runtime_realtime_find_matching(render_value, json_end, '{', '}');
    if (!render_end) {
        return 0;
    }

    scenes_value = kain_runtime_realtime_find_value_start(render_value, render_end, "\"scenes\"");
    if (scenes_value && scenes_value < render_end && *scenes_value == '[') {
        scenes_end = kain_runtime_realtime_find_matching(scenes_value, render_end, '[', ']');
        if (scenes_end) {
            bundle->scene_count = kain_runtime_realtime_count_array_objects(
                scenes_value,
                scenes_end
            );
            first_scene_start = kain_runtime_realtime_find_substring(scenes_value, scenes_end, "{");
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
                        "\"viewport_kind\"",
                        bundle->primary_viewport_kind,
                        sizeof(bundle->primary_viewport_kind)
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
                            bundle->primary_material_ref_count =
                                kain_runtime_graphics_count_string_array(
                                    material_refs_value,
                                    material_refs_end
                                );
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
                            bundle->primary_shader_ref_key_count =
                                kain_runtime_graphics_count_string_array(
                                    shader_keys_value,
                                    shader_keys_end
                                );
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

    materials_value = kain_runtime_realtime_find_value_start(render_value, render_end, "\"materials\"");
    if (materials_value && *materials_value == '[') {
        materials_end = kain_runtime_realtime_find_matching(materials_value, render_end, '[', ']');
        if (materials_end) {
            bundle->material_count = kain_runtime_realtime_count_array_objects(
                materials_value,
                materials_end
            );
            first_material_start = kain_runtime_realtime_find_substring(materials_value, materials_end, "{");
            if (first_material_start) {
                first_material_end = kain_runtime_realtime_find_matching(
                    first_material_start,
                    materials_end,
                    '{',
                    '}'
                );
                if (first_material_end) {
                    kain_runtime_graphics_parse_material_plan(
                        first_material_start,
                        first_material_end,
                        &bundle->primary_material
                    );
                }
            }
            bundle->material_shader_ref_key_count =
                kain_runtime_graphics_count_material_shader_refs(
                    materials_value,
                    materials_end
                );
        }
    }

    shader_refs_value = kain_runtime_realtime_find_value_start(json, json_end, "\"shader_bundle_refs\"");
    if (shader_refs_value && *shader_refs_value == '[') {
        shader_refs_end = kain_runtime_realtime_find_matching(shader_refs_value, json_end, '[', ']');
        if (shader_refs_end) {
            kain_runtime_graphics_count_shader_stage_refs(
                shader_refs_value,
                shader_refs_end,
                &bundle->shader_bundle_ref_count,
                &bundle->shader_vertex_ref_count,
                &bundle->shader_fragment_ref_count,
                &bundle->shader_compute_ref_count
            );
            compute_ref_start = kain_runtime_graphics_find_stage_object(
                shader_refs_value,
                shader_refs_end,
                "compute",
                &compute_ref_end
            );
            if (compute_ref_start && compute_ref_end) {
                kain_runtime_graphics_parse_compute_plan(
                    compute_ref_start,
                    compute_ref_end,
                    &bundle->primary_compute
                );
            }
        }
    }

    assets_value = kain_runtime_realtime_find_value_start(json, json_end, "\"assets\"");
    if (assets_value && *assets_value == '[') {
        assets_end = kain_runtime_realtime_find_matching(assets_value, json_end, '[', ']');
        if (assets_end) {
            bundle->asset_count = kain_runtime_realtime_count_array_objects(assets_value, assets_end);
        }
    }

    tool_caps_value = kain_runtime_realtime_find_value_start(json, json_end, "\"tool_caps\"");
    if (tool_caps_value && *tool_caps_value == '[') {
        tool_caps_end = kain_runtime_realtime_find_matching(tool_caps_value, json_end, '[', ']');
        if (tool_caps_end) {
            bundle->tool_cap_count = kain_runtime_graphics_count_string_array(
                tool_caps_value,
                tool_caps_end
            );
        }
    }

    requirements_value = kain_runtime_realtime_find_value_start(json, json_end, "\"requirements\"");
    if (requirements_value && *requirements_value == '[') {
        requirements_end = kain_runtime_realtime_find_matching(
            requirements_value,
            json_end,
            '[',
            ']'
        );
        if (requirements_end) {
            bundle->requirement_count = kain_runtime_graphics_count_string_array(
                requirements_value,
                requirements_end
            );
        }
    }

    if (!bundle->schema_version || !bundle->target[0]) {
        kain_runtime_graphics_init(bundle);
        return 0;
    }

    bundle->loaded = 1;
    kain_runtime_realtime_copy_cstr(
        bundle->load_origin,
        sizeof(bundle->load_origin),
        "json"
    );
    return 1;
}

int kain_runtime_graphics_load_from_path(const char* path, KainRuntimeGraphicsBundle* bundle) {
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
    loaded = kain_runtime_graphics_load_from_json(json, bundle);
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

int kain_runtime_graphics_load_from_env(const char* env_name, KainRuntimeGraphicsBundle* bundle) {
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
    loaded = kain_runtime_graphics_load_from_path(path, bundle);
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

int kain_runtime_graphics_load_for_current_process(
    const char* env_name,
    KainRuntimeGraphicsBundle* bundle
) {
    char sidecar_path[KAIN_RUNTIME_GRAPHICS_MAX_PATH];
    if (!bundle) {
        return 0;
    }
    if (kain_runtime_graphics_load_from_env(env_name, bundle)) {
        return 1;
    }
    if (kain_win32_get_executable_sidecar_path(
            KAIN_RUNTIME_GRAPHICS_SIDECAR_SUFFIX,
            sidecar_path,
            sizeof(sidecar_path)
        ) &&
        kain_runtime_graphics_load_from_path(sidecar_path, bundle)) {
        kain_runtime_realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "sidecar"
        );
        return 1;
    }
    kain_runtime_graphics_init(bundle);
    return 0;
}
#endif
