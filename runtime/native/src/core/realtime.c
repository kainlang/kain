#include "../../include/realtime.h"
#include "../../include/graphics_bundle.h"
#include <math.h>

static const char* realtime_find_substring(
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

static const char* realtime_skip_ws(const char* cursor, const char* end) {
    while (cursor && cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        ++cursor;
    }
    return cursor;
}

static const char* realtime_find_matching(
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

static const char* realtime_find_value_start(
    const char* scope_start,
    const char* scope_end,
    const char* key
) {
    const char* key_pos = realtime_find_substring(scope_start, scope_end, key);
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
    return realtime_skip_ws(colon + 1, scope_end);
}

static void realtime_copy_cstr(char* out, size_t out_cap, const char* value) {
    if (!out || out_cap == 0u) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    strncpy_s(out, out_cap, value, _TRUNCATE);
}

static int graphics_bundle_text_equals_ci(const char* left, const char* right) {
    if (!left || !right || !left[0] || !right[0]) {
        return 0;
    }
    return _stricmp(left, right) == 0;
}

static int graphics_bundle_inline_list_contains(
    const char* list,
    const char* value
) {
    const char* cursor;
    size_t value_length;
    if (!list || !value || !value[0]) {
        return 0;
    }
    value_length = strlen(value);
    cursor = list;
    while (*cursor) {
        const char* token_start = cursor;
        const char* token_end = strstr(cursor, ", ");
        size_t token_length;
        if (!token_end) {
            token_end = cursor + strlen(cursor);
        }
        token_length = (size_t)(token_end - token_start);
        if (token_length == value_length &&
            _strnicmp(token_start, value, value_length) == 0) {
            return 1;
        }
        if (!*token_end) {
            break;
        }
        cursor = token_end + 2;
    }
    return 0;
}

static void graphics_bundle_append_inline(
    char* out,
    size_t out_cap,
    const char* value
) {
    if (!out || out_cap == 0u || !value || !value[0]) {
        return;
    }
    if (out[0]) {
        strncat_s(out, out_cap, ", ", _TRUNCATE);
    }
    strncat_s(out, out_cap, value, _TRUNCATE);
}

static int graphics_bundle_append_unique_key(
    char* out,
    size_t out_cap,
    const char* value
) {
    if (!out || out_cap == 0u || !value || !value[0]) {
        return 0;
    }
    if (graphics_bundle_inline_list_contains(out, value)) {
        return 0;
    }
    graphics_bundle_append_inline(out, out_cap, value);
    return 1;
}

static void realtime_copy_string_value(
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

static void realtime_extract_string_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    char* out,
    size_t out_cap
) {
    const char* value_start =
        realtime_find_value_start(scope_start, scope_end, key);
    realtime_copy_string_value(value_start, scope_end, out, out_cap);
}

static int realtime_extract_double_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    double* out
) {
    const char* value_start =
        realtime_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    double value;
    if (!value_start || value_start >= scope_end || !out) {
        return 0;
    }
    value = strtod(value_start, &end_ptr);
    if (end_ptr == value_start) {
        return 0;
    }
    *out = value;
    return 1;
}

static int realtime_extract_int_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    int* out
) {
    const char* value_start =
        realtime_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    long value;
    if (!value_start || value_start >= scope_end || !out) {
        return 0;
    }
    value = strtol(value_start, &end_ptr, 10);
    if (end_ptr == value_start) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int realtime_count_array_objects(
    const char* array_start,
    const char* array_end
) {
    int count = 0;
    const char* cursor = array_start;
    while (cursor && cursor < array_end) {
        const char* object_start = realtime_find_substring(cursor, array_end, "{");
        const char* object_end;
        if (!object_start || object_start >= array_end) {
            break;
        }
        object_end = realtime_find_matching(object_start, array_end, '{', '}');
        if (!object_end) {
            break;
        }
        ++count;
        cursor = object_end + 1;
    }
    return count;
}

static void realtime_join_string_array(
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
        cursor = realtime_find_substring(cursor, array_end, "\"");
        if (!cursor || cursor >= array_end) {
            break;
        }
        realtime_copy_string_value(cursor, array_end, value, sizeof(value));
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

static int realtime_extract_number_array(
    const char* array_start,
    const char* array_end,
    double* values,
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
        double value;
        cursor = realtime_skip_ws(cursor, array_end);
        if (cursor >= array_end || *cursor == ']') {
            break;
        }
        value = strtod(cursor, &end_ptr);
        if (end_ptr == cursor) {
            break;
        }
        values[count++] = value;
        cursor = realtime_skip_ws(end_ptr, array_end);
        if (cursor < array_end && *cursor == ',') {
            ++cursor;
        }
    }
    return count;
}

static int graphics_bundle_count_string_array(
    const char* array_start,
    const char* array_end
) {
    int count = 0;
    const char* cursor = array_start;
    while (cursor && cursor < array_end) {
        const char* value_start;
        const char* value_end;
        value_start = realtime_find_substring(cursor, array_end, "\"");
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

static int graphics_bundle_extract_int_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    int fallback
) {
    const char* value_start =
        realtime_find_value_start(scope_start, scope_end, key);
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

static int graphics_bundle_count_material_shader_refs(
    const char* materials_start,
    const char* materials_end
) {
    int total = 0;
    const char* cursor = materials_start;
    while (cursor && cursor < materials_end) {
        const char* material_start = realtime_find_substring(cursor, materials_end, "{");
        const char* material_end;
        const char* refs_value;
        const char* refs_end;
        if (!material_start || material_start >= materials_end) {
            break;
        }
        material_end = realtime_find_matching(material_start, materials_end, '{', '}');
        if (!material_end) {
            break;
        }
        refs_value = realtime_find_value_start(
            material_start,
            material_end,
            "\"shader_bundle_ref_keys\""
        );
        if (refs_value && refs_value < material_end && *refs_value == '[') {
            refs_end = realtime_find_matching(refs_value, material_end, '[', ']');
            if (refs_end) {
                total += graphics_bundle_count_string_array(refs_value, refs_end);
            }
        }
        cursor = material_end + 1;
    }
    return total;
}

static int graphics_bundle_extract_int_array(
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
        cursor = realtime_skip_ws(cursor, array_end);
        if (cursor >= array_end || *cursor == ']') {
            break;
        }
        value = strtol(cursor, &end_ptr, 10);
        if (end_ptr == cursor) {
            break;
        }
        values[count++] = (int)value;
        cursor = realtime_skip_ws(end_ptr, array_end);
        if (cursor < array_end && *cursor == ',') {
            ++cursor;
        }
    }
    return count;
}

static void graphics_bundle_binding_init(KainRuntimeGraphicsBinding* binding) {
    if (!binding) {
        return;
    }
    ZeroMemory(binding, sizeof(*binding));
}

static int graphics_bundle_parse_binding_objects(
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
        const char* object_start = realtime_find_substring(cursor, array_end, "{");
        const char* object_end;
        KainRuntimeGraphicsBinding* binding;
        if (!object_start || object_start >= array_end) {
            break;
        }
        object_end = realtime_find_matching(object_start, array_end, '{', '}');
        if (!object_end) {
            break;
        }
        binding = &bindings[count];
        graphics_bundle_binding_init(binding);
        realtime_extract_string_field(object_start, object_end, "\"key\"", binding->key, sizeof(binding->key));
        realtime_extract_string_field(object_start, object_end, "\"type\"", binding->resource_type, sizeof(binding->resource_type));
        realtime_extract_string_field(object_start, object_end, "\"stage\"", binding->stage, sizeof(binding->stage));
        realtime_extract_string_field(object_start, object_end, "\"access\"", binding->access, sizeof(binding->access));
        binding->slot = graphics_bundle_extract_int_field(object_start, object_end, "\"slot\"", count);
        ++count;
        cursor = object_end + 1;
    }
    return count;
}

static int graphics_bundle_binding_is_valid(const KainRuntimeGraphicsBinding* binding) {
    if (!binding) {
        return 0;
    }
    if (!binding->key[0] ||
        !binding->resource_type[0] ||
        !binding->stage[0] ||
        !binding->access[0] ||
        binding->slot < 0) {
        return 0;
    }

    /* Keep stage/access semantics strict so the runtime doesn't "accept junk" and then fail later. */
    if (_stricmp(binding->stage, "vertex") != 0 &&
        _stricmp(binding->stage, "fragment") != 0 &&
        _stricmp(binding->stage, "compute") != 0) {
        return 0;
    }
    if (_stricmp(binding->access, "sample") != 0 &&
        _stricmp(binding->access, "read") != 0 &&
        _stricmp(binding->access, "write") != 0 &&
        _stricmp(binding->access, "read_write") != 0) {
        return 0;
    }

    return 1;
}

static int graphics_bundle_binding_stage_matches(
    const KainRuntimeGraphicsBinding* binding,
    const char* stage_name
) {
    if (!binding || !stage_name || !binding->stage[0]) {
        return 0;
    }
    return _stricmp(binding->stage, stage_name) == 0;
}

static int graphics_bundle_binding_array_is_valid(
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
        int j;
        if (!graphics_bundle_binding_is_valid(&bindings[i])) {
            return 0;
        }

        /* Resource binding plans must be stable and non-ambiguous. */
        for (j = 0; j < i; ++j) {
            if (bindings[j].slot == bindings[i].slot) {
                return 0;
            }
            if (bindings[j].key[0] && bindings[i].key[0] && strcmp(bindings[j].key, bindings[i].key) == 0) {
                return 0;
            }
        }

        if (required_stage &&
            graphics_bundle_binding_stage_matches(&bindings[i], required_stage)) {
            has_required_stage = 1;
        }
    }
    return required_stage ? has_required_stage : 1;
}

static int graphics_bundle_material_plan_is_valid(
    const KainRuntimeGraphicsMaterialPlan* material
) {
    if (!material ||
        !material->loaded ||
        !material->material_id[0] ||
        !material->source[0] ||
        material->shader_ref_count <= 0 ||
        !graphics_bundle_binding_array_is_valid(
            material->resource_bindings,
            material->resource_binding_count,
            "fragment"
        )) {
        return 0;
    }
    return graphics_bundle_binding_array_is_valid(
        material->resource_bindings,
        material->resource_binding_count,
        NULL
    ) && material->resource_binding_count > 0;
}

static int graphics_bundle_compute_plan_is_valid(
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
        !graphics_bundle_binding_array_is_valid(
            compute->resource_bindings,
            compute->resource_binding_count,
            "compute"
        ) ||
        compute->resource_binding_count <= 0) {
        return 0;
    }
    return 1;
}

static int graphics_bundle_compute_tensor_metadata_is_valid(
    const KainRuntimeGraphicsComputePlan* compute
) {
    if (!compute || !compute->loaded) {
        return 0;
    }
    if (compute->tensor_binding_count < 0 ||
        compute->tensor_binding_count > compute->resource_binding_count) {
        return 0;
    }
    if (compute->execution_domain[0] &&
        _stricmp(compute->execution_domain, "tensor-stream") == 0 &&
        compute->tensor_binding_count <= 0) {
        return 0;
    }
    return 1;
}

static int graphics_bundle_compute_stream_metadata_is_valid(
    const KainRuntimeGraphicsComputePlan* compute
) {
    if (!compute || !compute->loaded) {
        return 0;
    }
    return compute->stream_binding_count >= 0 &&
        compute->stream_binding_count <= compute->resource_binding_count;
}

static int graphics_bundle_compute_neural_metadata_is_valid(
    const KainRuntimeGraphicsComputePlan* compute
) {
    if (!compute || !compute->loaded) {
        return 0;
    }
    if (compute->neural_node_count < 0) {
        return 0;
    }
    if (compute->neural_node_count > 0 && compute->tensor_binding_count <= 0) {
        return 0;
    }
    return 1;
}

static KainRuntimeGraphicsPassKind graphics_bundle_pass_kind_for_stage(
    const char* stage
) {
    if (graphics_bundle_text_equals_ci(stage, "compute")) {
        return GRAPHICS_BUNDLE_PASS_COMPUTE;
    }
    if (graphics_bundle_text_equals_ci(stage, "present")) {
        return GRAPHICS_BUNDLE_PASS_PRESENT;
    }
    if (graphics_bundle_text_equals_ci(stage, "transfer")) {
        return GRAPHICS_BUNDLE_PASS_TRANSFER;
    }
    if (stage && stage[0]) {
        return GRAPHICS_BUNDLE_PASS_RENDER;
    }
    return GRAPHICS_BUNDLE_PASS_UNKNOWN;
}

static KainRuntimeGraphicsQueueKind graphics_bundle_queue_kind_for_stage(
    const char* stage
) {
    if (graphics_bundle_text_equals_ci(stage, "compute")) {
        return GRAPHICS_BUNDLE_QUEUE_COMPUTE;
    }
    if (graphics_bundle_text_equals_ci(stage, "present")) {
        return GRAPHICS_BUNDLE_QUEUE_PRESENT;
    }
    if (graphics_bundle_text_equals_ci(stage, "transfer")) {
        return GRAPHICS_BUNDLE_QUEUE_TRANSFER;
    }
    if (stage && stage[0]) {
        return GRAPHICS_BUNDLE_QUEUE_GRAPHICS;
    }
    return GRAPHICS_BUNDLE_QUEUE_UNKNOWN;
}

static KainRuntimeGraphicsResidencyKind graphics_bundle_residency_kind_for_binding(
    const KainRuntimeGraphicsBinding* binding
) {
    if (!binding) {
        return GRAPHICS_BUNDLE_RESIDENCY_UNKNOWN;
    }
    if (graphics_bundle_text_equals_ci(binding->access, "write") ||
        graphics_bundle_text_equals_ci(binding->access, "read_write")) {
        return GRAPHICS_BUNDLE_RESIDENCY_GPU_ONLY;
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "uniform_buffer")) {
        return GRAPHICS_BUNDLE_RESIDENCY_CPU_TO_GPU;
    }
    if (graphics_bundle_text_equals_ci(binding->access, "sample") ||
        graphics_bundle_text_equals_ci(binding->access, "read")) {
        return GRAPHICS_BUNDLE_RESIDENCY_CPU_TO_GPU;
    }
    return GRAPHICS_BUNDLE_RESIDENCY_GPU_ONLY;
}

static unsigned long long graphics_bundle_estimate_binding_bytes(
    const KainRuntimeGraphicsBinding* binding,
    const KainRuntimeGraphicsComputePlan* compute
) {
    unsigned long long dispatch_product = 1ull;
    unsigned long long base_bytes = 256ull;
    if (!binding) {
        return 0ull;
    }
    if (compute && compute->loaded &&
        compute->dispatch_size[0] > 0 &&
        compute->dispatch_size[1] > 0 &&
        compute->dispatch_size[2] > 0) {
        dispatch_product =
            (unsigned long long)compute->dispatch_size[0] *
            (unsigned long long)compute->dispatch_size[1] *
            (unsigned long long)compute->dispatch_size[2];
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "storage_buffer")) {
        base_bytes = dispatch_product * 16ull;
    } else if (graphics_bundle_text_equals_ci(binding->resource_type, "sampled_texture")) {
        base_bytes = 4096ull;
    } else if (graphics_bundle_text_equals_ci(binding->resource_type, "storage_texture")) {
        base_bytes = dispatch_product * 8ull;
    } else if (graphics_bundle_text_equals_ci(binding->resource_type, "uniform_buffer")) {
        base_bytes = 256ull;
    }
    if (graphics_bundle_text_equals_ci(binding->access, "write")) {
        base_bytes *= 2ull;
    } else if (graphics_bundle_text_equals_ci(binding->access, "read_write")) {
        base_bytes *= 3ull;
    }
    return base_bytes;
}

static int graphics_bundle_binding_is_read_access(
    const KainRuntimeGraphicsBinding* binding
) {
    if (!binding) {
        return 0;
    }
    return graphics_bundle_text_equals_ci(binding->access, "read") ||
        graphics_bundle_text_equals_ci(binding->access, "sample") ||
        graphics_bundle_text_equals_ci(binding->access, "read_write");
}

static int graphics_bundle_binding_is_write_access(
    const KainRuntimeGraphicsBinding* binding
) {
    if (!binding) {
        return 0;
    }
    return graphics_bundle_text_equals_ci(binding->access, "write") ||
        graphics_bundle_text_equals_ci(binding->access, "read_write");
}

static int graphics_bundle_collect_binding_keys(
    const KainRuntimeGraphicsBinding* bindings,
    int binding_count,
    int include_reads,
    int include_writes,
    char* out,
    size_t out_cap
) {
    int i;
    int count = 0;
    if (!out || out_cap == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (!bindings || binding_count <= 0) {
        return 0;
    }
    for (i = 0; i < binding_count; ++i) {
        int include = 0;
        if ((include_reads && graphics_bundle_binding_is_read_access(&bindings[i])) ||
            (include_writes && graphics_bundle_binding_is_write_access(&bindings[i]))) {
            include = 1;
        }
        if (include &&
            graphics_bundle_append_unique_key(out, out_cap, bindings[i].key)) {
            ++count;
        }
    }
    return count;
}

static const KainRuntimeGraphicsBinding* graphics_bundle_find_first_write_binding(
    const KainRuntimeGraphicsBinding* bindings,
    int binding_count
) {
    int i;
    if (!bindings || binding_count <= 0) {
        return NULL;
    }
    for (i = 0; i < binding_count; ++i) {
        if (graphics_bundle_binding_is_write_access(&bindings[i])) {
            return &bindings[i];
        }
    }
    return NULL;
}

static const char* graphics_bundle_find_stage_object(
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
        const char* object_start = realtime_find_substring(cursor, array_end, "{");
        const char* end;
        char stage[GRAPHICS_BUNDLE_MAX_TAG];
        if (!object_start || object_start >= array_end) {
            break;
        }
        end = realtime_find_matching(object_start, array_end, '{', '}');
        if (!end) {
            break;
        }
        realtime_extract_string_field(object_start, end, "\"stage\"", stage, sizeof(stage));
        if (stage[0] && _stricmp(stage, stage_name) == 0) {
            *object_end = end;
            return object_start;
        }
        cursor = end + 1;
    }
    return NULL;
}

static int graphics_bundle_parse_material_plan(
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
    realtime_extract_string_field(material_start, material_end, "\"id\"", material->material_id, sizeof(material->material_id));
    realtime_extract_string_field(material_start, material_end, "\"source\"", material->source, sizeof(material->source));
    shader_refs_value = realtime_find_value_start(material_start, material_end, "\"shader_bundle_ref_keys\"");
    if (shader_refs_value && *shader_refs_value == '[') {
        shader_refs_end = realtime_find_matching(shader_refs_value, material_end, '[', ']');
        if (shader_refs_end) {
            material->shader_ref_count = graphics_bundle_count_string_array(
                shader_refs_value,
                shader_refs_end
            );
        }
    }
    parameters_value = realtime_find_value_start(material_start, material_end, "\"parameters\"");
    if (parameters_value && *parameters_value == '[') {
        parameters_end = realtime_find_matching(parameters_value, material_end, '[', ']');
        if (parameters_end) {
            material->parameter_count = realtime_count_array_objects(parameters_value, parameters_end);
        }
    }
    bindings_value = realtime_find_value_start(material_start, material_end, "\"resource_bindings\"");
    if (bindings_value && *bindings_value == '[') {
        bindings_end = realtime_find_matching(bindings_value, material_end, '[', ']');
        if (bindings_end) {
            material->resource_binding_count = graphics_bundle_parse_binding_objects(
                bindings_value,
                bindings_end,
                material->resource_bindings,
                GRAPHICS_BUNDLE_MAX_BINDINGS
            );
        }
    }
    return material->material_id[0] != '\0';
}

static int graphics_bundle_parse_compute_plan(
    const char* shader_ref_start,
    const char* shader_ref_end,
    KainRuntimeGraphicsComputePlan* compute
) {
    const char* bindings_value;
    const char* bindings_end;
    const char* tensors_value;
    const char* tensors_end;
    const char* streams_value;
    const char* streams_end;
    const char* neural_nodes_value;
    const char* neural_nodes_end;
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
    realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"key\"", compute->shader_key, sizeof(compute->shader_key));
    realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"module_name\"", compute->module_name, sizeof(compute->module_name));
    realtime_extract_string_field(shader_ref_start, shader_ref_end, "\"entry_point\"", compute->entry_point, sizeof(compute->entry_point));
    realtime_extract_string_field(
        shader_ref_start,
        shader_ref_end,
        "\"execution_domain\"",
        compute->execution_domain,
        sizeof(compute->execution_domain)
    );
    workgroup_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"workgroup_size\"");
    if (workgroup_value && *workgroup_value == '[') {
        workgroup_end = realtime_find_matching(workgroup_value, shader_ref_end, '[', ']');
        if (workgroup_end) {
            compute->workgroup_size[0] = 0;
            compute->workgroup_size[1] = 0;
            compute->workgroup_size[2] = 0;
            graphics_bundle_extract_int_array(workgroup_value, workgroup_end, workgroup_values, 3);
            compute->workgroup_size[0] = workgroup_values[0];
            compute->workgroup_size[1] = workgroup_values[1];
            compute->workgroup_size[2] = workgroup_values[2];
        }
    }
    dispatch_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"dispatch_size\"");
    if (dispatch_value && *dispatch_value == '[') {
        dispatch_end = realtime_find_matching(dispatch_value, shader_ref_end, '[', ']');
        if (dispatch_end) {
            compute->dispatch_size[0] = 0;
            compute->dispatch_size[1] = 0;
            compute->dispatch_size[2] = 0;
            graphics_bundle_extract_int_array(dispatch_value, dispatch_end, dispatch_values, 3);
            compute->dispatch_size[0] = dispatch_values[0];
            compute->dispatch_size[1] = dispatch_values[1];
            compute->dispatch_size[2] = dispatch_values[2];
        }
    }
    bindings_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"resource_bindings\"");
    if (bindings_value && *bindings_value == '[') {
        bindings_end = realtime_find_matching(bindings_value, shader_ref_end, '[', ']');
        if (bindings_end) {
            compute->resource_binding_count = graphics_bundle_parse_binding_objects(
                bindings_value,
                bindings_end,
                compute->resource_bindings,
                GRAPHICS_BUNDLE_MAX_BINDINGS
            );
        }
    }
    tensors_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"tensor_bindings\"");
    if (tensors_value && *tensors_value == '[') {
        tensors_end = realtime_find_matching(tensors_value, shader_ref_end, '[', ']');
        if (tensors_end) {
            compute->tensor_binding_count = realtime_count_array_objects(
                tensors_value,
                tensors_end
            );
        }
    }
    streams_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"stream_bindings\"");
    if (streams_value && *streams_value == '[') {
        streams_end = realtime_find_matching(streams_value, shader_ref_end, '[', ']');
        if (streams_end) {
            compute->stream_binding_count = realtime_count_array_objects(
                streams_value,
                streams_end
            );
        }
    }
    neural_nodes_value = realtime_find_value_start(shader_ref_start, shader_ref_end, "\"neural_nodes\"");
    if (neural_nodes_value && *neural_nodes_value == '[') {
        neural_nodes_end = realtime_find_matching(neural_nodes_value, shader_ref_end, '[', ']');
        if (neural_nodes_end) {
            compute->neural_node_count = realtime_count_array_objects(
                neural_nodes_value,
                neural_nodes_end
            );
        }
    }
    return compute->shader_key[0] != '\0' && compute->entry_point[0] != '\0';
}

static void graphics_bundle_count_shader_stage_refs(
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
            realtime_find_substring(cursor, shader_refs_end, "{");
        const char* shader_ref_end;
        char stage[GRAPHICS_BUNDLE_MAX_TAG];
        if (!shader_ref_start || shader_ref_start >= shader_refs_end) {
            break;
        }
        shader_ref_end = realtime_find_matching(
            shader_ref_start,
            shader_refs_end,
            '{',
            '}'
        );
        if (!shader_ref_end) {
            break;
        }
        ++total;
        realtime_extract_string_field(
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

void realtime_init(KainRuntimeRealtimeBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

int realtime_load_from_json(const char* json, KainRuntimeRealtimeBundle* bundle) {
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
    const char* camera_value;
    const char* camera_end;
    const char* camera_position_value;
    const char* camera_position_end;
    const char* camera_target_value;
    const char* camera_target_end;
    const char* presentation_value;
    const char* presentation_end;
    const char* material_refs_value;
    const char* material_refs_end;
    const char* shader_keys_value;
    const char* shader_keys_end;
    double camera_position_values[3] = {0.0, 0.0, 0.0};
    double camera_target_values[3] = {0.0, 0.0, 0.0};
    if (!json || !bundle) {
        return 0;
    }
    realtime_init(bundle);
    json_end = json + strlen(json);

    target_value = realtime_find_value_start(json, json_end, "\"target\"");
    realtime_copy_string_value(
        target_value,
        json_end,
        bundle->target,
        sizeof(bundle->target)
    );
    bundle->valid_for_native_viewport = (strcmp(bundle->target, "llvm") == 0);

    scenes_value = realtime_find_value_start(json, json_end, "\"scenes\"");
    if (scenes_value && scenes_value < json_end && *scenes_value == '[') {
        scenes_end = realtime_find_matching(scenes_value, json_end, '[', ']');
        if (scenes_end) {
            bundle->scene_count = realtime_count_array_objects(scenes_value, scenes_end);
            first_scene_start =
                realtime_find_substring(scenes_value, scenes_end, "{");
            if (first_scene_start) {
                first_scene_end = realtime_find_matching(
                    first_scene_start,
                    scenes_end,
                    '{',
                    '}'
                );
                if (first_scene_end) {
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"viewport_node\"",
                        bundle->primary_viewport_node,
                        sizeof(bundle->primary_viewport_node)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"viewport_kind\"",
                        bundle->primary_viewport_kind,
                        sizeof(bundle->primary_viewport_kind)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"scene\"",
                        bundle->primary_scene,
                        sizeof(bundle->primary_scene)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"title\"",
                        bundle->primary_title,
                        sizeof(bundle->primary_title)
                    );

                    camera_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"camera\""
                    );
                    if (camera_value && *camera_value == '{') {
                        camera_end = realtime_find_matching(
                            camera_value,
                            first_scene_end,
                            '{',
                            '}'
                        );
                        if (camera_end) {
                            camera_position_value = realtime_find_value_start(
                                camera_value,
                                camera_end,
                                "\"position\""
                            );
                            if (camera_position_value && *camera_position_value == '[') {
                                camera_position_end = realtime_find_matching(
                                    camera_position_value,
                                    camera_end,
                                    '[',
                                    ']'
                                );
                                if (camera_position_end &&
                                    realtime_extract_number_array(
                                        camera_position_value,
                                        camera_position_end,
                                        camera_position_values,
                                        3
                                    ) == 3) {
                                    bundle->primary_camera_has_position = 1;
                                    memcpy(
                                        bundle->primary_camera_position,
                                        camera_position_values,
                                        sizeof(bundle->primary_camera_position)
                                    );
                                }
                            }

                            camera_target_value = realtime_find_value_start(
                                camera_value,
                                camera_end,
                                "\"target\""
                            );
                            if (camera_target_value && *camera_target_value == '[') {
                                camera_target_end = realtime_find_matching(
                                    camera_target_value,
                                    camera_end,
                                    '[',
                                    ']'
                                );
                                if (camera_target_end &&
                                    realtime_extract_number_array(
                                        camera_target_value,
                                        camera_target_end,
                                        camera_target_values,
                                        3
                                    ) == 3) {
                                    bundle->primary_camera_has_target = 1;
                                    memcpy(
                                        bundle->primary_camera_target,
                                        camera_target_values,
                                        sizeof(bundle->primary_camera_target)
                                    );
                                }
                            }

                            bundle->primary_camera_has_fov_y_degrees =
                                realtime_extract_double_field(
                                    camera_value,
                                    camera_end,
                                    "\"fov_y_degrees\"",
                                    &bundle->primary_camera_fov_y_degrees
                                );
                            bundle->primary_camera_has_near_plane =
                                realtime_extract_double_field(
                                    camera_value,
                                    camera_end,
                                    "\"near_plane\"",
                                    &bundle->primary_camera_near_plane
                                );
                            bundle->primary_camera_has_far_plane =
                                realtime_extract_double_field(
                                    camera_value,
                                    camera_end,
                                    "\"far_plane\"",
                                    &bundle->primary_camera_far_plane
                                );
                        }
                    }

                    presentation_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"presentation\""
                    );
                    if (presentation_value && *presentation_value == '{') {
                        presentation_end = realtime_find_matching(
                            presentation_value,
                            first_scene_end,
                            '{',
                            '}'
                        );
                        if (presentation_end) {
                            realtime_extract_string_field(
                                presentation_value,
                                presentation_end,
                                "\"profile\"",
                                bundle->primary_presentation_profile,
                                sizeof(bundle->primary_presentation_profile)
                            );
                            bundle->primary_presentation_has_profile =
                                bundle->primary_presentation_profile[0] != '\0';
                            bundle->primary_presentation_has_fog_density =
                                realtime_extract_double_field(
                                    presentation_value,
                                    presentation_end,
                                    "\"fog_density\"",
                                    &bundle->primary_presentation_fog_density
                                );
                            bundle->primary_presentation_has_particle_budget =
                                realtime_extract_int_field(
                                    presentation_value,
                                    presentation_end,
                                    "\"particle_budget\"",
                                    &bundle->primary_presentation_particle_budget
                                );
                        }
                    }

                    material_refs_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"material_refs\""
                    );
                    if (material_refs_value && *material_refs_value == '[') {
                        material_refs_end = realtime_find_matching(
                            material_refs_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (material_refs_end) {
                            realtime_join_string_array(
                                material_refs_value,
                                material_refs_end,
                                bundle->primary_material_refs,
                                sizeof(bundle->primary_material_refs)
                            );
                        }
                    }

                    shader_keys_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"shader_bundle_ref_keys\""
                    );
                    if (shader_keys_value && *shader_keys_value == '[') {
                        shader_keys_end = realtime_find_matching(
                            shader_keys_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (shader_keys_end) {
                            realtime_join_string_array(
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

    materials_value = realtime_find_value_start(json, json_end, "\"materials\"");
    if (materials_value && materials_value < json_end && *materials_value == '[') {
        materials_end = realtime_find_matching(materials_value, json_end, '[', ']');
        if (materials_end) {
            bundle->material_count = realtime_count_array_objects(materials_value, materials_end);
        }
    }

    shader_refs_value = realtime_find_value_start(json, json_end, "\"shader_bundle_refs\"");
    if (shader_refs_value && shader_refs_value < json_end && *shader_refs_value == '[') {
        shader_refs_end = realtime_find_matching(shader_refs_value, json_end, '[', ']');
        if (shader_refs_end) {
            bundle->shader_ref_count = realtime_count_array_objects(shader_refs_value, shader_refs_end);
        }
    }

    bundle->loaded = 1;
    return 1;
}

int realtime_load_from_path(const char* path, KainRuntimeRealtimeBundle* bundle) {
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
    loaded = realtime_load_from_json(json, bundle);
    if (loaded) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "path"
        );
        realtime_copy_cstr(
            bundle->source_path,
            sizeof(bundle->source_path),
            path
        );
    }
    free(json);
    return loaded;
}

int realtime_load_from_env(const char* env_name, KainRuntimeRealtimeBundle* bundle) {
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
    loaded = realtime_load_from_path(path, bundle);
    if (loaded) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "env"
        );
    }
    kain_env_free(path);
    return loaded;
}

int realtime_load_for_current_process(
    const char* env_name,
    KainRuntimeRealtimeBundle* bundle
) {
    char sidecar_path[REALTIME_MAX_PATH];
    if (!bundle) {
        return 0;
    }
    if (realtime_load_from_env(env_name, bundle)) {
        return 1;
    }
    if (kain_win32_get_executable_sidecar_path(
            REALTIME_SIDECAR_SUFFIX,
            sidecar_path,
            sizeof(sidecar_path)
        ) &&
        realtime_load_from_path(sidecar_path, bundle)) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "sidecar"
        );
        return 1;
    }
    realtime_init(bundle);
    return 0;
}

static int graphics_bundle_render_graph_has_pass(
    const KainRuntimeGraphicsRenderGraphContract* contract,
    const char* key
) {
    int i;
    if (!contract || !key || !key[0]) {
        return 0;
    }
    for (i = 0; i < contract->pass_count; ++i) {
        if (contract->passes[i].loaded &&
            strcmp(contract->passes[i].key, key) == 0) {
            return 1;
        }
    }
    return 0;
}

static KainRuntimeGraphicsResidencyResourceDescriptor*
graphics_bundle_find_residency_resource(
    KainRuntimeGraphicsResidencyContract* contract,
    const char* key
) {
    int i;
    if (!contract || !key || !key[0]) {
        return NULL;
    }
    for (i = 0; i < contract->resource_count; ++i) {
        if (contract->resources[i].loaded &&
            strcmp(contract->resources[i].key, key) == 0) {
            return &contract->resources[i];
        }
    }
    return NULL;
}

static const char* graphics_bundle_descriptor_kind_for_binding(
    const KainRuntimeGraphicsBinding* binding
) {
    if (!binding) {
        return "resource";
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "storage_buffer")) {
        return "storage_buffer";
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "storage_texture")) {
        return "storage_texture";
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "sampled_texture")) {
        return "sampled_texture";
    }
    if (graphics_bundle_text_equals_ci(binding->resource_type, "uniform_buffer")) {
        return "uniform_buffer";
    }
    return binding->resource_type[0] ? binding->resource_type : "resource";
}

static const char* graphics_bundle_residency_role_for_binding(
    const KainRuntimeGraphicsBinding* binding
) {
    if (!binding) {
        return "resource";
    }
    if (graphics_bundle_text_equals_ci(binding->access, "read")) {
        return "required_input";
    }
    if (graphics_bundle_text_equals_ci(binding->access, "sample")) {
        return "sampled_input";
    }
    if (graphics_bundle_text_equals_ci(binding->access, "write")) {
        return "required_output";
    }
    if (graphics_bundle_text_equals_ci(binding->access, "read_write")) {
        return "scratch_state";
    }
    return "resource";
}

static void graphics_bundle_merge_access_mode(
    char* access_mode,
    size_t access_mode_cap,
    const char* incoming_access
) {
    if (!access_mode || access_mode_cap == 0u || !incoming_access || !incoming_access[0]) {
        return;
    }
    if (!access_mode[0]) {
        realtime_copy_cstr(access_mode, access_mode_cap, incoming_access);
        return;
    }
    if (graphics_bundle_text_equals_ci(access_mode, incoming_access)) {
        return;
    }
    realtime_copy_cstr(access_mode, access_mode_cap, "read_write");
}

static void graphics_bundle_collect_residency_resource_keys(
    const KainRuntimeGraphicsResidencyContract* contract,
    char* out,
    size_t out_cap
) {
    int i;
    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!contract) {
        return;
    }
    for (i = 0; i < contract->resource_count; ++i) {
        if (contract->resources[i].loaded) {
            graphics_bundle_append_unique_key(
                out,
                out_cap,
                contract->resources[i].key
            );
        }
    }
}

static void graphics_bundle_add_residency_binding(
    KainRuntimeGraphicsResidencyContract* contract,
    const KainRuntimeGraphicsBinding* binding,
    const KainRuntimeGraphicsComputePlan* compute
) {
    KainRuntimeGraphicsResidencyResourceDescriptor* resource;
    unsigned long long estimated_bytes;
    if (!contract || !binding || !binding->key[0]) {
        return;
    }
    resource = graphics_bundle_find_residency_resource(contract, binding->key);
    if (!resource) {
        if (contract->resource_count >= GRAPHICS_BUNDLE_MAX_RESIDENCY_RESOURCES) {
            return;
        }
        resource = &contract->resources[contract->resource_count++];
        ZeroMemory(resource, sizeof(*resource));
        resource->loaded = 1;
        realtime_copy_cstr(resource->key, sizeof(resource->key), binding->key);
        realtime_copy_cstr(
            resource->descriptor_kind,
            sizeof(resource->descriptor_kind),
            graphics_bundle_descriptor_kind_for_binding(binding)
        );
        realtime_copy_cstr(resource->stage, sizeof(resource->stage), binding->stage);
        resource->slot = binding->slot;
        resource->gpu_resident = 1;
    }
    estimated_bytes = graphics_bundle_estimate_binding_bytes(binding, compute);
    graphics_bundle_merge_access_mode(
        resource->access_mode,
        sizeof(resource->access_mode),
        binding->access
    );
    if (!resource->residency_role[0] ||
        graphics_bundle_text_equals_ci(resource->residency_role, "required_input")) {
        realtime_copy_cstr(
            resource->residency_role,
            sizeof(resource->residency_role),
            graphics_bundle_residency_role_for_binding(binding)
        );
    }
    if (!resource->stage[0]) {
        realtime_copy_cstr(resource->stage, sizeof(resource->stage), binding->stage);
    }
    if (estimated_bytes > resource->byte_length) {
        resource->byte_length = estimated_bytes;
    }
    resource->residency_kind = graphics_bundle_residency_kind_for_binding(binding);
    resource->cpu_visible =
        resource->residency_kind == GRAPHICS_BUNDLE_RESIDENCY_CPU_TO_GPU ||
        resource->residency_kind == GRAPHICS_BUNDLE_RESIDENCY_READBACK;
    resource->transient_resource =
        graphics_bundle_text_equals_ci(resource->residency_role, "scratch_state");
}

static int graphics_bundle_count_schedule_queues(
    const KainRuntimeGraphicsComputeSchedule* schedule
) {
    int seen_graphics = 0;
    int seen_compute = 0;
    int seen_transfer = 0;
    int seen_present = 0;
    int i;
    if (!schedule) {
        return 0;
    }
    for (i = 0; i < schedule->step_count; ++i) {
        if (!schedule->steps[i].loaded) {
            continue;
        }
        if (schedule->steps[i].queue == GRAPHICS_BUNDLE_QUEUE_GRAPHICS) {
            seen_graphics = 1;
        } else if (schedule->steps[i].queue == GRAPHICS_BUNDLE_QUEUE_COMPUTE) {
            seen_compute = 1;
        } else if (schedule->steps[i].queue == GRAPHICS_BUNDLE_QUEUE_TRANSFER) {
            seen_transfer = 1;
        } else if (schedule->steps[i].queue == GRAPHICS_BUNDLE_QUEUE_PRESENT) {
            seen_present = 1;
        }
    }
    return seen_graphics + seen_compute + seen_transfer + seen_present;
}

void graphics_bundle_render_graph_init(KainRuntimeGraphicsRenderGraphContract* contract) {
    if (!contract) {
        return;
    }
    ZeroMemory(contract, sizeof(*contract));
}

void graphics_bundle_residency_init(KainRuntimeGraphicsResidencyContract* contract) {
    if (!contract) {
        return;
    }
    ZeroMemory(contract, sizeof(*contract));
}

void graphics_bundle_compute_schedule_init(KainRuntimeGraphicsComputeSchedule* schedule) {
    if (!schedule) {
        return;
    }
    ZeroMemory(schedule, sizeof(*schedule));
}

int graphics_bundle_render_graph_is_valid(
    const KainRuntimeGraphicsRenderGraphContract* contract
) {
    int i;
    int has_attachment_consumer = 0;
    int primary_pass_found = 0;
    int requires_attachment = 0;
    if (!contract || !contract->loaded ||
        contract->pass_count <= 0 ||
        contract->pass_count > GRAPHICS_BUNDLE_MAX_RENDER_PASSES ||
        contract->attachment_count < 0 ||
        contract->attachment_count > GRAPHICS_BUNDLE_MAX_RENDER_ATTACHMENTS ||
        contract->dependency_count < 0 ||
        contract->dependency_count > GRAPHICS_BUNDLE_MAX_RENDER_DEPENDENCIES) {
        return 0;
    }
    for (i = 0; i < contract->pass_count; ++i) {
        const KainRuntimeGraphicsRenderPassDescriptor* pass = &contract->passes[i];
        if (!pass->loaded || !pass->key[0] ||
            pass->kind == GRAPHICS_BUNDLE_PASS_UNKNOWN ||
            pass->queue == GRAPHICS_BUNDLE_QUEUE_UNKNOWN) {
            return 0;
        }
        if (pass->kind == GRAPHICS_BUNDLE_PASS_RENDER ||
            pass->kind == GRAPHICS_BUNDLE_PASS_PRESENT) {
            requires_attachment = 1;
        }
        if (strcmp(pass->key, contract->primary_pass_key) == 0) {
            primary_pass_found = 1;
        }
    }
    for (i = 0; i < contract->attachment_count; ++i) {
        const KainRuntimeGraphicsAttachmentDescriptor* attachment = &contract->attachments[i];
        if (!attachment->loaded || !attachment->key[0] ||
            attachment->kind == GRAPHICS_BUNDLE_ATTACHMENT_UNKNOWN ||
            attachment->lifetime == GRAPHICS_BUNDLE_LIFETIME_UNKNOWN) {
            return 0;
        }
        if (attachment->producer_pass[0] &&
            !graphics_bundle_render_graph_has_pass(contract, attachment->producer_pass)) {
            return 0;
        }
        has_attachment_consumer = 1;
    }
    for (i = 0; i < contract->dependency_count; ++i) {
        const KainRuntimeGraphicsRenderDependencyDescriptor* dependency =
            &contract->dependencies[i];
        if (!dependency->loaded ||
            !dependency->from_pass[0] ||
            !dependency->to_pass[0] ||
            dependency->barrier_kind == GRAPHICS_BUNDLE_BARRIER_UNKNOWN ||
            !graphics_bundle_render_graph_has_pass(contract, dependency->from_pass) ||
            !graphics_bundle_render_graph_has_pass(contract, dependency->to_pass)) {
            return 0;
        }
    }
    return primary_pass_found && (!requires_attachment || has_attachment_consumer);
}

int graphics_bundle_residency_is_valid(
    const KainRuntimeGraphicsResidencyContract* contract
) {
    int i;
    if (!contract || !contract->loaded ||
        contract->resource_count <= 0 ||
        contract->resource_count > GRAPHICS_BUNDLE_MAX_RESIDENCY_RESOURCES) {
        return 0;
    }
    for (i = 0; i < contract->resource_count; ++i) {
        const KainRuntimeGraphicsResidencyResourceDescriptor* resource =
            &contract->resources[i];
        if (!resource->loaded || !resource->key[0] ||
            !resource->descriptor_kind[0] ||
            !resource->access_mode[0] ||
            !resource->residency_role[0] ||
            resource->residency_kind == GRAPHICS_BUNDLE_RESIDENCY_UNKNOWN ||
            resource->slot < 0 ||
            resource->byte_length <= 0ull) {
            return 0;
        }
    }
    return 1;
}

int graphics_bundle_compute_schedule_is_valid(
    const KainRuntimeGraphicsComputeSchedule* schedule
) {
    int i;
    int primary_step_found = 0;
    if (!schedule || !schedule->loaded ||
        schedule->step_count <= 0 ||
        schedule->step_count > GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS ||
        schedule->barrier_count < 0 ||
        schedule->barrier_count > GRAPHICS_BUNDLE_MAX_SCHEDULE_BARRIERS) {
        return 0;
    }
    for (i = 0; i < schedule->step_count; ++i) {
        const KainRuntimeGraphicsScheduleStepDescriptor* step = &schedule->steps[i];
        if (!step->loaded || !step->key[0] ||
            step->queue == GRAPHICS_BUNDLE_QUEUE_UNKNOWN) {
            return 0;
        }
        if (step->dispatch_size[0] < 0 || step->dispatch_size[1] < 0 ||
            step->dispatch_size[2] < 0 || step->workgroup_size[0] < 0 ||
            step->workgroup_size[1] < 0 || step->workgroup_size[2] < 0) {
            return 0;
        }
        if (strcmp(step->key, schedule->primary_step_key) == 0) {
            primary_step_found = 1;
        }
    }
    for (i = 0; i < schedule->barrier_count; ++i) {
        const KainRuntimeGraphicsScheduleBarrierDescriptor* barrier =
            &schedule->barriers[i];
        if (!barrier->loaded || !barrier->key[0] ||
            !barrier->from_step[0] || !barrier->to_step[0] ||
            barrier->barrier_kind == GRAPHICS_BUNDLE_BARRIER_UNKNOWN) {
            return 0;
        }
    }
    return primary_step_found;
}

static void graphics_bundle_synthesize_render_graph(
    KainRuntimeGraphicsBundle* bundle
) {
    KainRuntimeGraphicsRenderGraphContract* contract;
    int has_render_scene;
    int has_compute_plan;
    const KainRuntimeGraphicsBinding* storage_output;
    if (!bundle) {
        return;
    }
    contract = &bundle->render_graph;
    graphics_bundle_render_graph_init(contract);
    contract->loaded = 1;
    contract->synthesized_from_bundle = 1;
    has_render_scene = bundle->scene_count > 0 && bundle->primary_scene[0];
    has_compute_plan =
        bundle->shader_compute_ref_count > 0 &&
        bundle->primary_compute.loaded &&
        bundle->primary_compute.shader_key[0];

    if (has_compute_plan && contract->pass_count < GRAPHICS_BUNDLE_MAX_RENDER_PASSES) {
        KainRuntimeGraphicsRenderPassDescriptor* pass =
            &contract->passes[contract->pass_count++];
        ZeroMemory(pass, sizeof(*pass));
        pass->loaded = 1;
        pass->kind = GRAPHICS_BUNDLE_PASS_COMPUTE;
        pass->queue = GRAPHICS_BUNDLE_QUEUE_COMPUTE;
        pass->async_capable = 1;
        pass->read_count = graphics_bundle_collect_binding_keys(
            bundle->primary_compute.resource_bindings,
            bundle->primary_compute.resource_binding_count,
            1,
            0,
            pass->reads,
            sizeof(pass->reads)
        );
        pass->write_count = graphics_bundle_collect_binding_keys(
            bundle->primary_compute.resource_bindings,
            bundle->primary_compute.resource_binding_count,
            0,
            1,
            pass->writes,
            sizeof(pass->writes)
        );
        realtime_copy_cstr(pass->key, sizeof(pass->key), "primary_compute");
        realtime_copy_cstr(
            pass->label,
            sizeof(pass->label),
            bundle->primary_compute.module_name[0]
                ? bundle->primary_compute.module_name
                : "Primary Compute"
        );
        realtime_copy_cstr(
            pass->capture_hook,
            sizeof(pass->capture_hook),
            "compute_dispatch"
        );
        contract->capture_hook_count += 1;
    }

    if (has_render_scene && contract->pass_count < GRAPHICS_BUNDLE_MAX_RENDER_PASSES) {
        KainRuntimeGraphicsRenderPassDescriptor* pass =
            &contract->passes[contract->pass_count++];
        ZeroMemory(pass, sizeof(*pass));
        pass->loaded = 1;
        pass->kind = GRAPHICS_BUNDLE_PASS_RENDER;
        pass->queue = GRAPHICS_BUNDLE_QUEUE_GRAPHICS;
        pass->read_count = graphics_bundle_collect_binding_keys(
            bundle->primary_material.resource_bindings,
            bundle->primary_material.resource_binding_count,
            1,
            0,
            pass->reads,
            sizeof(pass->reads)
        );
        graphics_bundle_append_unique_key(
            pass->writes,
            sizeof(pass->writes),
            "color_target"
        );
        graphics_bundle_append_unique_key(
            pass->writes,
            sizeof(pass->writes),
            "depth_target"
        );
        pass->write_count = 2;
        realtime_copy_cstr(pass->key, sizeof(pass->key), "primary_scene_render");
        realtime_copy_cstr(
            pass->label,
            sizeof(pass->label),
            bundle->primary_title[0] ? bundle->primary_title : "Primary Scene Render"
        );
        realtime_copy_cstr(
            pass->capture_hook,
            sizeof(pass->capture_hook),
            "viewport_frame"
        );
        contract->capture_hook_count += 1;
    }

    if (has_render_scene && contract->pass_count < GRAPHICS_BUNDLE_MAX_RENDER_PASSES) {
        KainRuntimeGraphicsRenderPassDescriptor* pass =
            &contract->passes[contract->pass_count++];
        ZeroMemory(pass, sizeof(*pass));
        pass->loaded = 1;
        pass->kind = GRAPHICS_BUNDLE_PASS_PRESENT;
        pass->queue = GRAPHICS_BUNDLE_QUEUE_PRESENT;
        pass->read_count = 1;
        pass->write_count = 1;
        realtime_copy_cstr(pass->key, sizeof(pass->key), "present");
        realtime_copy_cstr(pass->label, sizeof(pass->label), "Present Viewport");
        realtime_copy_cstr(pass->reads, sizeof(pass->reads), "color_target");
        realtime_copy_cstr(pass->writes, sizeof(pass->writes), "swapchain_target");
        realtime_copy_cstr(
            pass->capture_hook,
            sizeof(pass->capture_hook),
            "present_swapchain"
        );
        contract->capture_hook_count += 1;
    }

    if (has_render_scene && contract->attachment_count + 3 <=
        GRAPHICS_BUNDLE_MAX_RENDER_ATTACHMENTS) {
        KainRuntimeGraphicsAttachmentDescriptor* attachment;

        attachment = &contract->attachments[contract->attachment_count++];
        ZeroMemory(attachment, sizeof(*attachment));
        attachment->loaded = 1;
        attachment->kind = GRAPHICS_BUNDLE_ATTACHMENT_COLOR;
        attachment->lifetime = GRAPHICS_BUNDLE_LIFETIME_FRAME_TRANSIENT;
        attachment->transient_attachment = 1;
        attachment->consumer_count = 2;
        realtime_copy_cstr(attachment->key, sizeof(attachment->key), "color_target");
        realtime_copy_cstr(attachment->format, sizeof(attachment->format), "rgba8_unorm");
        realtime_copy_cstr(
            attachment->producer_pass,
            sizeof(attachment->producer_pass),
            "primary_scene_render"
        );
        realtime_copy_cstr(
            attachment->consumer_passes,
            sizeof(attachment->consumer_passes),
            "primary_scene_render, present"
        );

        attachment = &contract->attachments[contract->attachment_count++];
        ZeroMemory(attachment, sizeof(*attachment));
        attachment->loaded = 1;
        attachment->kind = GRAPHICS_BUNDLE_ATTACHMENT_DEPTH;
        attachment->lifetime = GRAPHICS_BUNDLE_LIFETIME_FRAME_TRANSIENT;
        attachment->transient_attachment = 1;
        attachment->consumer_count = 1;
        realtime_copy_cstr(attachment->key, sizeof(attachment->key), "depth_target");
        realtime_copy_cstr(attachment->format, sizeof(attachment->format), "depth24");
        realtime_copy_cstr(
            attachment->producer_pass,
            sizeof(attachment->producer_pass),
            "primary_scene_render"
        );
        realtime_copy_cstr(
            attachment->consumer_passes,
            sizeof(attachment->consumer_passes),
            "primary_scene_render"
        );

        attachment = &contract->attachments[contract->attachment_count++];
        ZeroMemory(attachment, sizeof(*attachment));
        attachment->loaded = 1;
        attachment->kind = GRAPHICS_BUNDLE_ATTACHMENT_SWAPCHAIN;
        attachment->lifetime = GRAPHICS_BUNDLE_LIFETIME_IMPORTED;
        attachment->consumer_count = 1;
        realtime_copy_cstr(
            attachment->key,
            sizeof(attachment->key),
            "swapchain_target"
        );
        realtime_copy_cstr(
            attachment->format,
            sizeof(attachment->format),
            "bgra8_unorm"
        );
        realtime_copy_cstr(
            attachment->producer_pass,
            sizeof(attachment->producer_pass),
            "present"
        );
        realtime_copy_cstr(
            attachment->consumer_passes,
            sizeof(attachment->consumer_passes),
            "present"
        );
    }

    storage_output = graphics_bundle_find_first_write_binding(
        bundle->primary_compute.resource_bindings,
        bundle->primary_compute.resource_binding_count
    );
    if (storage_output &&
        contract->attachment_count < GRAPHICS_BUNDLE_MAX_RENDER_ATTACHMENTS) {
        KainRuntimeGraphicsAttachmentDescriptor* attachment =
            &contract->attachments[contract->attachment_count++];
        ZeroMemory(attachment, sizeof(*attachment));
        attachment->loaded = 1;
        attachment->kind = GRAPHICS_BUNDLE_ATTACHMENT_STORAGE;
        attachment->lifetime = GRAPHICS_BUNDLE_LIFETIME_PERSISTENT;
        attachment->consumer_count = has_render_scene ? 2 : 1;
        realtime_copy_cstr(
            attachment->key,
            sizeof(attachment->key),
            storage_output->key
        );
        realtime_copy_cstr(
            attachment->format,
            sizeof(attachment->format),
            storage_output->resource_type
        );
        realtime_copy_cstr(
            attachment->producer_pass,
            sizeof(attachment->producer_pass),
            "primary_compute"
        );
        realtime_copy_cstr(
            attachment->consumer_passes,
            sizeof(attachment->consumer_passes),
            has_render_scene ? "primary_compute, primary_scene_render" : "primary_compute"
        );
    }

    if (has_compute_plan && has_render_scene &&
        contract->dependency_count < GRAPHICS_BUNDLE_MAX_RENDER_DEPENDENCIES) {
        KainRuntimeGraphicsRenderDependencyDescriptor* dependency =
            &contract->dependencies[contract->dependency_count++];
        ZeroMemory(dependency, sizeof(*dependency));
        dependency->loaded = 1;
        dependency->barrier_kind = GRAPHICS_BUNDLE_BARRIER_BUFFER;
        realtime_copy_cstr(
            dependency->from_pass,
            sizeof(dependency->from_pass),
            "primary_compute"
        );
        realtime_copy_cstr(
            dependency->to_pass,
            sizeof(dependency->to_pass),
            "primary_scene_render"
        );
        realtime_copy_cstr(
            dependency->reason,
            sizeof(dependency->reason),
            "publish compute outputs to the render scene"
        );
    }

    if (has_render_scene &&
        contract->dependency_count < GRAPHICS_BUNDLE_MAX_RENDER_DEPENDENCIES) {
        KainRuntimeGraphicsRenderDependencyDescriptor* dependency =
            &contract->dependencies[contract->dependency_count++];
        ZeroMemory(dependency, sizeof(*dependency));
        dependency->loaded = 1;
        dependency->barrier_kind = GRAPHICS_BUNDLE_BARRIER_TEXTURE;
        realtime_copy_cstr(
            dependency->from_pass,
            sizeof(dependency->from_pass),
            "primary_scene_render"
        );
        realtime_copy_cstr(
            dependency->to_pass,
            sizeof(dependency->to_pass),
            "present"
        );
        realtime_copy_cstr(
            dependency->reason,
            sizeof(dependency->reason),
            "present rendered color output"
        );
    }

    if (has_compute_plan) {
        realtime_copy_cstr(
            contract->primary_pass_key,
            sizeof(contract->primary_pass_key),
            "primary_compute"
        );
    } else if (has_render_scene) {
        realtime_copy_cstr(
            contract->primary_pass_key,
            sizeof(contract->primary_pass_key),
            "primary_scene_render"
        );
    }
}

static void graphics_bundle_synthesize_residency(
    KainRuntimeGraphicsBundle* bundle
) {
    KainRuntimeGraphicsResidencyContract* contract;
    int i;
    if (!bundle) {
        return;
    }
    contract = &bundle->residency;
    graphics_bundle_residency_init(contract);
    contract->loaded = 1;
    contract->synthesized_from_bundle = 1;

    for (i = 0; i < bundle->primary_material.resource_binding_count; ++i) {
        graphics_bundle_add_residency_binding(
            contract,
            &bundle->primary_material.resource_bindings[i],
            NULL
        );
    }
    for (i = 0; i < bundle->primary_compute.resource_binding_count; ++i) {
        graphics_bundle_add_residency_binding(
            contract,
            &bundle->primary_compute.resource_bindings[i],
            &bundle->primary_compute
        );
    }

    contract->async_stream_count = bundle->primary_compute.stream_binding_count;
    contract->estimated_bytes = 0ull;
    contract->transient_pool_count = 0;
    contract->transient_pool_bytes = 0ull;
    for (i = 0; i < contract->resource_count; ++i) {
        contract->estimated_bytes += contract->resources[i].byte_length;
        if (contract->resources[i].transient_resource) {
            contract->resources[i].residency_kind =
                GRAPHICS_BUNDLE_RESIDENCY_TRANSIENT_POOL;
            contract->transient_pool_count += 1;
            contract->transient_pool_bytes += contract->resources[i].byte_length;
        }
    }
}

static void graphics_bundle_synthesize_compute_schedule(
    KainRuntimeGraphicsBundle* bundle
) {
    KainRuntimeGraphicsComputeSchedule* schedule;
    int has_render_scene;
    int has_compute_plan;
    char residency_keys[GRAPHICS_BUNDLE_MAX_INLINE];
    if (!bundle) {
        return;
    }
    schedule = &bundle->primary_schedule;
    graphics_bundle_compute_schedule_init(schedule);
    schedule->loaded = 1;
    schedule->synthesized_from_bundle = 1;
    has_render_scene = bundle->scene_count > 0 && bundle->primary_scene[0];
    has_compute_plan =
        bundle->shader_compute_ref_count > 0 &&
        bundle->primary_compute.loaded &&
        bundle->primary_compute.shader_key[0];
    graphics_bundle_collect_residency_resource_keys(
        &bundle->residency,
        residency_keys,
        sizeof(residency_keys)
    );

    if (bundle->residency.resource_count > 0 &&
        schedule->step_count < GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS) {
        KainRuntimeGraphicsScheduleStepDescriptor* step =
            &schedule->steps[schedule->step_count++];
        ZeroMemory(step, sizeof(*step));
        step->loaded = 1;
        step->queue = GRAPHICS_BUNDLE_QUEUE_TRANSFER;
        step->async_capable = 1;
        step->resource_count = bundle->residency.resource_count;
        realtime_copy_cstr(step->key, sizeof(step->key), "prepare_residency");
        realtime_copy_cstr(
            step->label,
            sizeof(step->label),
            "Prepare Residency"
        );
        realtime_copy_cstr(
            step->resource_keys,
            sizeof(step->resource_keys),
            residency_keys
        );
    }

    if (has_compute_plan &&
        schedule->step_count < GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS) {
        KainRuntimeGraphicsScheduleStepDescriptor* step =
            &schedule->steps[schedule->step_count++];
        ZeroMemory(step, sizeof(*step));
        step->loaded = 1;
        step->queue = GRAPHICS_BUNDLE_QUEUE_COMPUTE;
        step->async_capable = 1;
        step->resource_count = graphics_bundle_collect_binding_keys(
            bundle->primary_compute.resource_bindings,
            bundle->primary_compute.resource_binding_count,
            1,
            1,
            step->resource_keys,
            sizeof(step->resource_keys)
        );
        step->dispatch_size[0] = bundle->primary_compute.dispatch_size[0];
        step->dispatch_size[1] = bundle->primary_compute.dispatch_size[1];
        step->dispatch_size[2] = bundle->primary_compute.dispatch_size[2];
        step->workgroup_size[0] = bundle->primary_compute.workgroup_size[0];
        step->workgroup_size[1] = bundle->primary_compute.workgroup_size[1];
        step->workgroup_size[2] = bundle->primary_compute.workgroup_size[2];
        realtime_copy_cstr(step->key, sizeof(step->key), "primary_compute");
        realtime_copy_cstr(
            step->label,
            sizeof(step->label),
            bundle->primary_compute.module_name[0]
                ? bundle->primary_compute.module_name
                : "Primary Compute"
        );
        realtime_copy_cstr(
            step->shader_key,
            sizeof(step->shader_key),
            bundle->primary_compute.shader_key
        );
        realtime_copy_cstr(
            schedule->primary_step_key,
            sizeof(schedule->primary_step_key),
            step->key
        );
    }

    if (has_render_scene && has_compute_plan &&
        schedule->step_count < GRAPHICS_BUNDLE_MAX_SCHEDULE_STEPS) {
        KainRuntimeGraphicsScheduleStepDescriptor* step =
            &schedule->steps[schedule->step_count++];
        ZeroMemory(step, sizeof(*step));
        step->loaded = 1;
        step->queue = GRAPHICS_BUNDLE_QUEUE_GRAPHICS;
        step->resource_count = graphics_bundle_collect_binding_keys(
            bundle->primary_material.resource_bindings,
            bundle->primary_material.resource_binding_count,
            1,
            0,
            step->resource_keys,
            sizeof(step->resource_keys)
        );
        realtime_copy_cstr(
            step->key,
            sizeof(step->key),
            "publish_render_inputs"
        );
        realtime_copy_cstr(
            step->label,
            sizeof(step->label),
            "Publish Render Inputs"
        );
    }

    if (bundle->residency.resource_count > 0 && has_compute_plan &&
        schedule->barrier_count < GRAPHICS_BUNDLE_MAX_SCHEDULE_BARRIERS) {
        KainRuntimeGraphicsScheduleBarrierDescriptor* barrier =
            &schedule->barriers[schedule->barrier_count++];
        ZeroMemory(barrier, sizeof(*barrier));
        barrier->loaded = 1;
        barrier->barrier_kind = GRAPHICS_BUNDLE_BARRIER_BUFFER;
        realtime_copy_cstr(
            barrier->key,
            sizeof(barrier->key),
            "residency_to_compute"
        );
        realtime_copy_cstr(
            barrier->from_step,
            sizeof(barrier->from_step),
            "prepare_residency"
        );
        realtime_copy_cstr(
            barrier->to_step,
            sizeof(barrier->to_step),
            "primary_compute"
        );
        realtime_copy_cstr(
            barrier->resource_key,
            sizeof(barrier->resource_key),
            bundle->residency.resource_count > 0 ? bundle->residency.resources[0].key : ""
        );
        realtime_copy_cstr(
            barrier->reason,
            sizeof(barrier->reason),
            "stage residency before compute dispatch"
        );
    }

    if (has_render_scene && has_compute_plan &&
        schedule->barrier_count < GRAPHICS_BUNDLE_MAX_SCHEDULE_BARRIERS) {
        KainRuntimeGraphicsScheduleBarrierDescriptor* barrier =
            &schedule->barriers[schedule->barrier_count++];
        ZeroMemory(barrier, sizeof(*barrier));
        barrier->loaded = 1;
        barrier->barrier_kind = GRAPHICS_BUNDLE_BARRIER_EXECUTION;
        realtime_copy_cstr(
            barrier->key,
            sizeof(barrier->key),
            "compute_to_graphics"
        );
        realtime_copy_cstr(
            barrier->from_step,
            sizeof(barrier->from_step),
            "primary_compute"
        );
        realtime_copy_cstr(
            barrier->to_step,
            sizeof(barrier->to_step),
            "publish_render_inputs"
        );
        realtime_copy_cstr(
            barrier->resource_key,
            sizeof(barrier->resource_key),
            bundle->primary_compute.resource_binding_count > 0
                ? bundle->primary_compute.resource_bindings[0].key
                : ""
        );
        realtime_copy_cstr(
            barrier->reason,
            sizeof(barrier->reason),
            "publish compute outputs to graphics"
        );
    }

    schedule->queue_count = graphics_bundle_count_schedule_queues(schedule);
    schedule->async_step_count = 0;
    if (schedule->primary_step_key[0] == '\0' && schedule->step_count > 0) {
        realtime_copy_cstr(
            schedule->primary_step_key,
            sizeof(schedule->primary_step_key),
            schedule->steps[0].key
        );
    }
    if (schedule->step_count > 0) {
        int i;
        for (i = 0; i < schedule->step_count; ++i) {
            if (schedule->steps[i].async_capable) {
                schedule->async_step_count += 1;
            }
        }
    }
}

void graphics_bundle_init(KainRuntimeGraphicsBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
    graphics_bundle_render_graph_init(&bundle->render_graph);
    graphics_bundle_residency_init(&bundle->residency);
    graphics_bundle_compute_schedule_init(&bundle->primary_schedule);
}

void graphics_bundle_validation_init(KainRuntimeGraphicsValidation* validation) {
    if (!validation) {
        return;
    }
    ZeroMemory(validation, sizeof(*validation));
}

static void graphics_bundle_copy_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
) {
    char contract_summary[GRAPHICS_BUNDLE_MAX_SUMMARY];
    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!bundle) {
        return;
    }
    graphics_bundle_format_contract_summary(
        bundle,
        contract_summary,
        sizeof(contract_summary)
    );
    snprintf(
        out,
        out_cap,
        "schema %d | target %s | scenes %d | mats %d | shader refs v/f/c=%d/%d/%d | compute bind/t/s/n=%d/%d/%d/%d | viewport %s/%s | %s",
        bundle->schema_version,
        bundle->target[0] ? bundle->target : "unknown",
        bundle->scene_count,
        bundle->material_count,
        bundle->shader_vertex_ref_count,
        bundle->shader_fragment_ref_count,
        bundle->shader_compute_ref_count,
        bundle->primary_compute.resource_binding_count,
        bundle->primary_compute.tensor_binding_count,
        bundle->primary_compute.stream_binding_count,
        bundle->primary_compute.neural_node_count,
        bundle->primary_viewport_kind[0] ? bundle->primary_viewport_kind : "none",
        bundle->primary_scene[0] ? bundle->primary_scene : "none",
        contract_summary
    );
}

void graphics_bundle_format_summary(
    const KainRuntimeGraphicsBundle* bundle,
    char* out,
    size_t out_cap
) {
    graphics_bundle_copy_summary(bundle, out, out_cap);
}

void graphics_bundle_format_contract_summary(
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
        "graph p/a/d=%d/%d/%d %s | residency r/b=%d/%llu %s | schedule s/b=%d/%d %s",
        bundle->render_graph.pass_count,
        bundle->render_graph.attachment_count,
        bundle->render_graph.dependency_count,
        bundle->render_graph.primary_pass_key[0]
            ? bundle->render_graph.primary_pass_key
            : "none",
        bundle->residency.resource_count,
        bundle->residency.estimated_bytes,
        bundle->residency.transient_pool_count > 0 ? "transient" : "stable",
        bundle->primary_schedule.step_count,
        bundle->primary_schedule.barrier_count,
        bundle->primary_schedule.primary_step_key[0]
            ? bundle->primary_schedule.primary_step_key
            : "none"
    );
}

void graphics_bundle_execution_state_init(KainRuntimeGraphicsExecutionState* state) {
    if (!state) {
        return;
    }
    ZeroMemory(state, sizeof(*state));
}

int graphics_bundle_execute_primary_compute(
    const KainRuntimeGraphicsBundle* bundle,
    double frame_delta,
    double total_time,
    KainRuntimeGraphicsExecutionState* state
) {
    unsigned long long dispatch_invocations;
    double safe_dt;

    if (!bundle || !state) {
        return 0;
    }

    graphics_bundle_execution_state_init(state);
    if (!bundle->loaded || bundle->shader_compute_ref_count <= 0) {
        snprintf(state->summary, sizeof(state->summary), "no compute plan loaded");
        return 0;
    }
    if (!bundle->primary_compute.loaded ||
        !graphics_bundle_compute_plan_is_valid(&bundle->primary_compute) ||
        !graphics_bundle_compute_tensor_metadata_is_valid(&bundle->primary_compute) ||
        !graphics_bundle_compute_stream_metadata_is_valid(&bundle->primary_compute) ||
        !graphics_bundle_compute_neural_metadata_is_valid(&bundle->primary_compute)) {
        snprintf(state->summary, sizeof(state->summary), "compute plan present but invalid");
        return 0;
    }

    dispatch_invocations =
        (unsigned long long)bundle->primary_compute.dispatch_size[0] *
        (unsigned long long)bundle->primary_compute.dispatch_size[1] *
        (unsigned long long)bundle->primary_compute.dispatch_size[2];
    safe_dt = frame_delta > 0.0001 ? frame_delta : 0.0001;

    state->executed = 1;
    state->dispatch_invocations = dispatch_invocations;
    state->accumulated_invocations = dispatch_invocations;
    state->throughput = (double)dispatch_invocations / safe_dt;
    state->phase = fmod(total_time * (1.0 + (double)bundle->primary_compute.neural_node_count), 1.0);
    state->tensor_binding_count = bundle->primary_compute.tensor_binding_count;
    state->stream_binding_count = bundle->primary_compute.stream_binding_count;
    state->neural_node_count = bundle->primary_compute.neural_node_count;
    state->schedule_step_count = bundle->primary_schedule.step_count;
    state->schedule_barrier_count = bundle->primary_schedule.barrier_count;
    realtime_copy_cstr(
        state->schedule_key,
        sizeof(state->schedule_key),
        bundle->primary_schedule.primary_step_key[0]
            ? bundle->primary_schedule.primary_step_key
            : "primary_compute"
    );
    snprintf(
        state->summary,
        sizeof(state->summary),
        "%s | dispatch %llux | tensor %d | stream %d | neural %d | schedule %d/%d %s",
        bundle->primary_compute.execution_domain[0] ? bundle->primary_compute.execution_domain : "compute",
        dispatch_invocations,
        state->tensor_binding_count,
        state->stream_binding_count,
        state->neural_node_count,
        state->schedule_step_count,
        state->schedule_barrier_count,
        state->schedule_key
    );
    return 1;
}

int graphics_bundle_validate_bundle(
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
    int tensor_metadata_valid;
    int stream_metadata_valid;
    int neural_metadata_valid;
    int has_render_graph_contract;
    int render_graph_valid;
    int has_residency_contract;
    int residency_valid;
    int has_compute_schedule_contract;
    int compute_schedule_valid;
    if (!bundle || !validation) {
        return 0;
    }
    graphics_bundle_validation_init(validation);

    validation->loaded = bundle->loaded;
    validation->target_is_llvm = (bundle->target[0] && _stricmp(bundle->target, "llvm") == 0);
    has_compute_artifacts = bundle->shader_compute_ref_count > 0;
    has_render_scene = bundle->scene_count > 0 &&
        bundle->primary_viewport_node[0] &&
        bundle->primary_scene[0];
    has_viewport3d = bundle->primary_viewport_kind[0] &&
        _stricmp(bundle->primary_viewport_kind, "viewport3d") == 0;
    has_material_bindings = graphics_bundle_material_plan_is_valid(&bundle->primary_material);
    has_compute_plan = bundle->primary_compute.loaded &&
        bundle->primary_compute.shader_key[0] &&
        bundle->primary_compute.module_name[0] &&
        bundle->primary_compute.entry_point[0];
    material_binding_valid = has_material_bindings;
    compute_plan_valid = !has_compute_artifacts ||
        (has_compute_plan && graphics_bundle_compute_plan_is_valid(&bundle->primary_compute));
    tensor_metadata_valid = !has_compute_artifacts ||
        (has_compute_plan && graphics_bundle_compute_tensor_metadata_is_valid(&bundle->primary_compute));
    stream_metadata_valid = !has_compute_artifacts ||
        (has_compute_plan && graphics_bundle_compute_stream_metadata_is_valid(&bundle->primary_compute));
    neural_metadata_valid = !has_compute_artifacts ||
        (has_compute_plan && graphics_bundle_compute_neural_metadata_is_valid(&bundle->primary_compute));
    has_render_graph_contract = bundle->render_graph.loaded;
    render_graph_valid = has_render_graph_contract &&
        graphics_bundle_render_graph_is_valid(&bundle->render_graph);
    has_residency_contract = bundle->residency.loaded;
    residency_valid = !has_compute_artifacts ||
        (has_residency_contract && graphics_bundle_residency_is_valid(&bundle->residency));
    has_compute_schedule_contract = bundle->primary_schedule.loaded;
    compute_schedule_valid = !has_compute_artifacts ||
        (has_compute_schedule_contract &&
         graphics_bundle_compute_schedule_is_valid(&bundle->primary_schedule));
    validation->has_render_scene = has_render_scene;
    validation->has_viewport3d = has_viewport3d;
    validation->has_material_bindings = has_material_bindings;
    validation->has_compute_artifacts = has_compute_artifacts;
    validation->material_binding_valid = material_binding_valid;
    validation->compute_plan_valid = compute_plan_valid;
    validation->tensor_metadata_valid = tensor_metadata_valid;
    validation->stream_metadata_valid = stream_metadata_valid;
    validation->neural_metadata_valid = neural_metadata_valid;
    validation->has_render_graph_contract = has_render_graph_contract;
    validation->render_graph_valid = render_graph_valid;
    validation->has_residency_contract = has_residency_contract;
    validation->residency_valid = residency_valid;
    validation->has_compute_schedule_contract = has_compute_schedule_contract;
    validation->compute_schedule_valid = compute_schedule_valid;
    validation->graphics_lane_ready = bundle->loaded &&
        bundle->schema_version == 1 &&
        validation->target_is_llvm &&
        has_render_scene &&
        has_viewport3d &&
        material_binding_valid &&
        render_graph_valid &&
        residency_valid &&
        compute_plan_valid &&
        compute_schedule_valid &&
        tensor_metadata_valid &&
        stream_metadata_valid &&
        neural_metadata_valid;
    validation->compute_metadata_valid = bundle->loaded &&
        bundle->schema_version == 1 &&
        render_graph_valid &&
        residency_valid &&
        compute_plan_valid &&
        compute_schedule_valid &&
        tensor_metadata_valid &&
        stream_metadata_valid &&
        neural_metadata_valid;
    graphics_bundle_copy_summary(bundle, validation->summary, sizeof(validation->summary));

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
            "current native graphics lane requires target llvm",
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
    if (!render_graph_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle render graph contract is missing or invalid",
            _TRUNCATE
        );
        return 0;
    }
    if (!residency_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle residency contract is missing or invalid",
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
    if (!compute_schedule_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle compute schedule contract is missing or invalid",
            _TRUNCATE
        );
        return 0;
    }
    if (!tensor_metadata_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle tensor metadata is inconsistent with compute bindings",
            _TRUNCATE
        );
        return 0;
    }
    if (!stream_metadata_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle stream metadata is inconsistent with compute bindings",
            _TRUNCATE
        );
        return 0;
    }
    if (!neural_metadata_valid) {
        strncpy_s(
            validation->reason,
            sizeof(validation->reason),
            "graphics bundle neural metadata requires tensor bindings",
            _TRUNCATE
        );
        return 0;
    }

    strncpy_s(
        validation->reason,
        sizeof(validation->reason),
        "graphics bundle ready for the current native graphics lane",
        _TRUNCATE
    );
    return 1;
}

int viewport_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle) {
    KainRuntimeGraphicsValidation validation;

    if (!bundle) {
        return 0;
    }
    if (!graphics_bundle_validate_bundle(bundle, &validation)) {
        return 0;
    }
    return validation.graphics_lane_ready;
}

int graphics_bundle_load_from_json(const char* json, KainRuntimeGraphicsBundle* bundle) {
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

    graphics_bundle_init(bundle);
    json_end = json + strlen(json);

    bundle->schema_version = graphics_bundle_extract_int_field(
        json,
        json_end,
        "\"schema_version\"",
        0
    );
    realtime_extract_string_field(
        json,
        json_end,
        "\"target\"",
        bundle->target,
        sizeof(bundle->target)
    );

    render_value = realtime_find_value_start(json, json_end, "\"render\"");
    if (!render_value || *render_value != '{') {
        return 0;
    }
    render_end = realtime_find_matching(render_value, json_end, '{', '}');
    if (!render_end) {
        return 0;
    }

    scenes_value = realtime_find_value_start(render_value, render_end, "\"scenes\"");
    if (scenes_value && scenes_value < render_end && *scenes_value == '[') {
        scenes_end = realtime_find_matching(scenes_value, render_end, '[', ']');
        if (scenes_end) {
            bundle->scene_count = realtime_count_array_objects(
                scenes_value,
                scenes_end
            );
            first_scene_start = realtime_find_substring(scenes_value, scenes_end, "{");
            if (first_scene_start) {
                first_scene_end = realtime_find_matching(
                    first_scene_start,
                    scenes_end,
                    '{',
                    '}'
                );
                if (first_scene_end) {
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"viewport_node\"",
                        bundle->primary_viewport_node,
                        sizeof(bundle->primary_viewport_node)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"viewport_kind\"",
                        bundle->primary_viewport_kind,
                        sizeof(bundle->primary_viewport_kind)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"scene\"",
                        bundle->primary_scene,
                        sizeof(bundle->primary_scene)
                    );
                    realtime_extract_string_field(
                        first_scene_start,
                        first_scene_end,
                        "\"title\"",
                        bundle->primary_title,
                        sizeof(bundle->primary_title)
                    );

                    material_refs_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"material_refs\""
                    );
                    if (material_refs_value && *material_refs_value == '[') {
                        material_refs_end = realtime_find_matching(
                            material_refs_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (material_refs_end) {
                            bundle->primary_material_ref_count =
                                graphics_bundle_count_string_array(
                                    material_refs_value,
                                    material_refs_end
                                );
                            realtime_join_string_array(
                                material_refs_value,
                                material_refs_end,
                                bundle->primary_material_refs,
                                sizeof(bundle->primary_material_refs)
                            );
                        }
                    }

                    shader_keys_value = realtime_find_value_start(
                        first_scene_start,
                        first_scene_end,
                        "\"shader_bundle_ref_keys\""
                    );
                    if (shader_keys_value && *shader_keys_value == '[') {
                        shader_keys_end = realtime_find_matching(
                            shader_keys_value,
                            first_scene_end,
                            '[',
                            ']'
                        );
                        if (shader_keys_end) {
                            bundle->primary_shader_ref_key_count =
                                graphics_bundle_count_string_array(
                                    shader_keys_value,
                                    shader_keys_end
                                );
                            realtime_join_string_array(
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

    materials_value = realtime_find_value_start(render_value, render_end, "\"materials\"");
    if (materials_value && *materials_value == '[') {
        materials_end = realtime_find_matching(materials_value, render_end, '[', ']');
        if (materials_end) {
            bundle->material_count = realtime_count_array_objects(
                materials_value,
                materials_end
            );
            first_material_start = realtime_find_substring(materials_value, materials_end, "{");
            if (first_material_start) {
                first_material_end = realtime_find_matching(
                    first_material_start,
                    materials_end,
                    '{',
                    '}'
                );
                if (first_material_end) {
                    graphics_bundle_parse_material_plan(
                        first_material_start,
                        first_material_end,
                        &bundle->primary_material
                    );
                }
            }
            bundle->material_shader_ref_key_count =
                graphics_bundle_count_material_shader_refs(
                    materials_value,
                    materials_end
                );
        }
    }

    shader_refs_value = realtime_find_value_start(json, json_end, "\"shader_bundle_refs\"");
    if (shader_refs_value && *shader_refs_value == '[') {
        shader_refs_end = realtime_find_matching(shader_refs_value, json_end, '[', ']');
        if (shader_refs_end) {
            graphics_bundle_count_shader_stage_refs(
                shader_refs_value,
                shader_refs_end,
                &bundle->shader_bundle_ref_count,
                &bundle->shader_vertex_ref_count,
                &bundle->shader_fragment_ref_count,
                &bundle->shader_compute_ref_count
            );
            compute_ref_start = graphics_bundle_find_stage_object(
                shader_refs_value,
                shader_refs_end,
                "compute",
                &compute_ref_end
            );
            if (compute_ref_start && compute_ref_end) {
                graphics_bundle_parse_compute_plan(
                    compute_ref_start,
                    compute_ref_end,
                    &bundle->primary_compute
                );
            }
        }
    }

    assets_value = realtime_find_value_start(json, json_end, "\"assets\"");
    if (assets_value && *assets_value == '[') {
        assets_end = realtime_find_matching(assets_value, json_end, '[', ']');
        if (assets_end) {
            bundle->asset_count = realtime_count_array_objects(assets_value, assets_end);
        }
    }

    tool_caps_value = realtime_find_value_start(json, json_end, "\"tool_caps\"");
    if (tool_caps_value && *tool_caps_value == '[') {
        tool_caps_end = realtime_find_matching(tool_caps_value, json_end, '[', ']');
        if (tool_caps_end) {
            bundle->tool_cap_count = graphics_bundle_count_string_array(
                tool_caps_value,
                tool_caps_end
            );
        }
    }

    requirements_value = realtime_find_value_start(json, json_end, "\"requirements\"");
    if (requirements_value && *requirements_value == '[') {
        requirements_end = realtime_find_matching(
            requirements_value,
            json_end,
            '[',
            ']'
        );
        if (requirements_end) {
            bundle->requirement_count = graphics_bundle_count_string_array(
                requirements_value,
                requirements_end
            );
        }
    }

    if (!bundle->schema_version || !bundle->target[0]) {
        graphics_bundle_init(bundle);
        return 0;
    }

    graphics_bundle_synthesize_render_graph(bundle);
    graphics_bundle_synthesize_residency(bundle);
    graphics_bundle_synthesize_compute_schedule(bundle);
    bundle->loaded = 1;
    realtime_copy_cstr(
        bundle->load_origin,
        sizeof(bundle->load_origin),
        "json"
    );
    return 1;
}

int graphics_bundle_load_from_path(const char* path, KainRuntimeGraphicsBundle* bundle) {
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
    loaded = graphics_bundle_load_from_json(json, bundle);
    if (loaded) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "path"
        );
        realtime_copy_cstr(
            bundle->source_path,
            sizeof(bundle->source_path),
            path
        );
    }
    free(json);
    return loaded;
}

int graphics_bundle_load_from_env(const char* env_name, KainRuntimeGraphicsBundle* bundle) {
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
    loaded = graphics_bundle_load_from_path(path, bundle);
    if (loaded) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "env"
        );
    }
    kain_env_free(path);
    return loaded;
}

int graphics_bundle_load_for_current_process(
    const char* env_name,
    KainRuntimeGraphicsBundle* bundle
) {
    char sidecar_path[GRAPHICS_BUNDLE_MAX_PATH];
    if (!bundle) {
        return 0;
    }
    if (graphics_bundle_load_from_env(env_name, bundle)) {
        return 1;
    }
    if (kain_win32_get_executable_sidecar_path(
            GRAPHICS_BUNDLE_SIDECAR_SUFFIX,
            sidecar_path,
            sizeof(sidecar_path)
        ) &&
        graphics_bundle_load_from_path(sidecar_path, bundle)) {
        realtime_copy_cstr(
            bundle->load_origin,
            sizeof(bundle->load_origin),
            "sidecar"
        );
        return 1;
    }
    graphics_bundle_init(bundle);
    return 0;
}
