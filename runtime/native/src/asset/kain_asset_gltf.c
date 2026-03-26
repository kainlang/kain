#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/kain_runtime_asset.h"

#ifdef _WIN32
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf.h"

typedef struct {
    KainNativeSceneAsset* asset;
    int render_blend_pass;
    int collect_stats;
    int skip_sky_primitives;
} KainNativeAssetCompileContext;

static double kain_native_asset_max3(double a, double b, double c) {
    double result = a;
    if (b > result) result = b;
    if (c > result) result = c;
    return result;
}

static float kain_native_asset_clampf(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int kain_native_asset_contains_ci(const char* text, const char* token) {
    size_t text_length;
    size_t token_length;
    size_t start;
    size_t index;

    if (!text || !token || !text[0] || !token[0]) {
        return 0;
    }

    text_length = strlen(text);
    token_length = strlen(token);
    if (token_length > text_length) {
        return 0;
    }

    for (start = 0; start + token_length <= text_length; ++start) {
        for (index = 0; index < token_length; ++index) {
            char a = text[start + index];
            char b = token[index];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) {
                break;
            }
        }
        if (index == token_length) {
            return 1;
        }
    }

    return 0;
}

static unsigned int kain_native_asset_hash_string(const char* value) {
    unsigned int hash = 2166136261u;
    while (value && *value) {
        hash ^= (unsigned char)(*value++);
        hash *= 16777619u;
    }
    return hash;
}

static void kain_native_asset_pick_fallback_color(
    KainNativeSceneAsset* asset,
    const cgltf_material* material,
    float out_rgba[4]
) {
    const char* material_name = material && material->name ? material->name : "";
    unsigned int hash = kain_native_asset_hash_string(material_name);

    asset->used_fallback_colors = 1;
    out_rgba[0] = 0.68f;
    out_rgba[1] = 0.72f;
    out_rgba[2] = 0.78f;
    out_rgba[3] = 1.0f;

    if (kain_native_asset_contains_ci(material_name, "vegetation") || kain_native_asset_contains_ci(material_name, "tree")) {
        out_rgba[0] = 0.22f;
        out_rgba[1] = 0.48f;
        out_rgba[2] = 0.24f;
    } else if (kain_native_asset_contains_ci(material_name, "window") || kain_native_asset_contains_ci(material_name, "glass")) {
        out_rgba[0] = 0.32f;
        out_rgba[1] = 0.56f;
        out_rgba[2] = 0.80f;
        out_rgba[3] = 0.88f;
    } else if (kain_native_asset_contains_ci(material_name, "road") || kain_native_asset_contains_ci(material_name, "street")) {
        out_rgba[0] = 0.18f;
        out_rgba[1] = 0.19f;
        out_rgba[2] = 0.21f;
    } else if (kain_native_asset_contains_ci(material_name, "blue")) {
        out_rgba[0] = 0.20f;
        out_rgba[1] = 0.38f;
        out_rgba[2] = 0.70f;
    } else if (kain_native_asset_contains_ci(material_name, "red")) {
        out_rgba[0] = 0.76f;
        out_rgba[1] = 0.16f;
        out_rgba[2] = 0.14f;
    } else if (kain_native_asset_contains_ci(material_name, "yellow") || kain_native_asset_contains_ci(material_name, "orange")) {
        out_rgba[0] = 0.86f;
        out_rgba[1] = 0.62f;
        out_rgba[2] = 0.18f;
    } else if (kain_native_asset_contains_ci(material_name, "white")) {
        out_rgba[0] = 0.92f;
        out_rgba[1] = 0.92f;
        out_rgba[2] = 0.90f;
    } else if (kain_native_asset_contains_ci(material_name, "black")) {
        out_rgba[0] = 0.08f;
        out_rgba[1] = 0.08f;
        out_rgba[2] = 0.09f;
    } else if (material_name[0]) {
        out_rgba[0] = 0.28f + ((float)((hash >> 0) & 0xFF) / 255.0f) * 0.52f;
        out_rgba[1] = 0.26f + ((float)((hash >> 8) & 0xFF) / 255.0f) * 0.48f;
        out_rgba[2] = 0.24f + ((float)((hash >> 16) & 0xFF) / 255.0f) * 0.50f;
    }
}

static void kain_native_asset_resolve_material_color(
    KainNativeSceneAsset* asset,
    const cgltf_material* material,
    float out_rgba[4],
    int* out_double_sided,
    int* out_blended
) {
    float brightness;

    out_rgba[0] = 0.78f;
    out_rgba[1] = 0.80f;
    out_rgba[2] = 0.84f;
    out_rgba[3] = 1.0f;
    if (out_double_sided) *out_double_sided = 0;
    if (out_blended) *out_blended = 0;

    if (!material) {
        return;
    }

    if (out_double_sided) {
        *out_double_sided = material->double_sided ? 1 : 0;
    }
    if (out_blended) {
        *out_blended = material->alpha_mode == cgltf_alpha_mode_blend ? 1 : 0;
    }

    if (material->has_pbr_metallic_roughness) {
        out_rgba[0] = material->pbr_metallic_roughness.base_color_factor[0];
        out_rgba[1] = material->pbr_metallic_roughness.base_color_factor[1];
        out_rgba[2] = material->pbr_metallic_roughness.base_color_factor[2];
        out_rgba[3] = material->pbr_metallic_roughness.base_color_factor[3];
    } else {
        kain_native_asset_pick_fallback_color(asset, material, out_rgba);
    }

    if (material->emissive_factor[0] > 0.001f || material->emissive_factor[1] > 0.001f || material->emissive_factor[2] > 0.001f) {
        out_rgba[0] = kain_native_asset_clampf(out_rgba[0] * 0.75f + material->emissive_factor[0] * 0.45f, 0.0f, 1.0f);
        out_rgba[1] = kain_native_asset_clampf(out_rgba[1] * 0.75f + material->emissive_factor[1] * 0.45f, 0.0f, 1.0f);
        out_rgba[2] = kain_native_asset_clampf(out_rgba[2] * 0.75f + material->emissive_factor[2] * 0.45f, 0.0f, 1.0f);
    }

    brightness = (out_rgba[0] * 0.299f) + (out_rgba[1] * 0.587f) + (out_rgba[2] * 0.114f);
    if (brightness < 0.08f) {
        out_rgba[0] = kain_native_asset_clampf(out_rgba[0] + 0.12f, 0.0f, 1.0f);
        out_rgba[1] = kain_native_asset_clampf(out_rgba[1] + 0.12f, 0.0f, 1.0f);
        out_rgba[2] = kain_native_asset_clampf(out_rgba[2] + 0.12f, 0.0f, 1.0f);
    }
}

static KainVec3 kain_native_asset_transform_point(const cgltf_float matrix[16], const cgltf_float point[3]) {
    return kain_vec3_make(
        (double)(matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12]),
        (double)(matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13]),
        (double)(matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14])
    );
}

static void kain_native_asset_update_bounds(KainNativeSceneAsset* asset, KainVec3 point) {
    if (point.x < asset->raw_bounds_min.x) asset->raw_bounds_min.x = point.x;
    if (point.y < asset->raw_bounds_min.y) asset->raw_bounds_min.y = point.y;
    if (point.z < asset->raw_bounds_min.z) asset->raw_bounds_min.z = point.z;
    if (point.x > asset->raw_bounds_max.x) asset->raw_bounds_max.x = point.x;
    if (point.y > asset->raw_bounds_max.y) asset->raw_bounds_max.y = point.y;
    if (point.z > asset->raw_bounds_max.z) asset->raw_bounds_max.z = point.z;
}

static void kain_native_asset_record_primitive_stats(
    KainNativeSceneAsset* asset,
    const cgltf_node* node,
    const cgltf_primitive* primitive,
    const cgltf_accessor* positions
) {
    cgltf_float world_matrix[16];
    cgltf_size vertex_index;
    cgltf_size element_count;

    if (!asset || !node || !primitive || !positions) {
        return;
    }

    asset->primitive_count += 1;
    asset->vertex_count += (unsigned long long)positions->count;
    cgltf_node_transform_world(node, world_matrix);

    for (vertex_index = 0; vertex_index < positions->count; ++vertex_index) {
        cgltf_float point[3];
        if (cgltf_accessor_read_float(positions, vertex_index, point, 3)) {
            kain_native_asset_update_bounds(asset, kain_native_asset_transform_point(world_matrix, point));
        }
    }

    element_count = primitive->indices ? primitive->indices->count : positions->count;
    switch (primitive->type == cgltf_primitive_type_invalid ? cgltf_primitive_type_triangles : primitive->type) {
        case cgltf_primitive_type_triangles:
            asset->triangle_count += (unsigned long long)(element_count / 3);
            break;
        case cgltf_primitive_type_triangle_strip:
        case cgltf_primitive_type_triangle_fan:
            if (element_count >= 3) {
                asset->triangle_count += (unsigned long long)(element_count - 2);
            }
            break;
        default:
            break;
    }
}

static int kain_native_asset_should_skip_primitive(
    const KainNativeAssetCompileContext* context,
    const cgltf_node* node,
    const cgltf_primitive* primitive,
    const cgltf_accessor* positions
) {
    const char* node_name;
    const char* mesh_name;
    const char* material_name;
    double extent_x;
    double extent_y;
    double extent_z;
    double max_extent;

    if (!context || !primitive || !positions || !context->skip_sky_primitives) {
        return 0;
    }

    node_name = node && node->name ? node->name : "";
    mesh_name = node && node->mesh && node->mesh->name ? node->mesh->name : "";
    material_name = primitive->material && primitive->material->name ? primitive->material->name : "";
    if (!positions->has_min || !positions->has_max) {
        return 0;
    }

    extent_x = positions->max[0] - positions->min[0];
    extent_y = positions->max[1] - positions->min[1];
    extent_z = positions->max[2] - positions->min[2];
    max_extent = kain_native_asset_max3(extent_x, extent_y, extent_z);

    if ((kain_native_asset_contains_ci(material_name, "sky") || kain_native_asset_contains_ci(node_name, "sky")) && max_extent > 120.0) {
        return 1;
    }

    if ((kain_native_asset_contains_ci(node_name, "sphere") || kain_native_asset_contains_ci(mesh_name, "sphere")) && max_extent > 180.0 && positions->count <= 2048) {
        return 1;
    }

    if (extent_y < 0.5 && max_extent > 220.0 && positions->count <= 256 && (!material_name[0] || kain_native_asset_contains_ci(material_name, "sky"))) {
        return 1;
    }

    return 0;
}

static GLenum kain_native_asset_gl_mode(cgltf_primitive_type primitive_type) {
    switch (primitive_type) {
        case cgltf_primitive_type_points: return GL_POINTS;
        case cgltf_primitive_type_lines: return GL_LINES;
        case cgltf_primitive_type_line_loop: return GL_LINE_LOOP;
        case cgltf_primitive_type_line_strip: return GL_LINE_STRIP;
        case cgltf_primitive_type_triangle_strip: return GL_TRIANGLE_STRIP;
        case cgltf_primitive_type_triangle_fan: return GL_TRIANGLE_FAN;
        case cgltf_primitive_type_invalid:
        case cgltf_primitive_type_triangles:
        default:
            return GL_TRIANGLES;
    }
}

static void kain_native_asset_emit_primitive(
    KainNativeAssetCompileContext* context,
    const cgltf_node* node,
    const cgltf_primitive* primitive
) {
    const cgltf_accessor* positions;
    const cgltf_accessor* normals;
    float color[4];
    int double_sided = 0;
    int blended = 0;
    GLenum mode;
    cgltf_size element_count;
    cgltf_size element_index;

    if (!context || !context->asset || !primitive) {
        return;
    }

    positions = cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
    normals = cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0);
    if (!positions || positions->type != cgltf_type_vec3) {
        return;
    }

    kain_native_asset_resolve_material_color(context->asset, primitive->material, color, &double_sided, &blended);
    if (blended != context->render_blend_pass) {
        return;
    }

    if (context->collect_stats) {
        kain_native_asset_record_primitive_stats(context->asset, node, primitive, positions);
    }

    if (double_sided) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }

    if (blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    glColor4f(color[0], color[1], color[2], color[3]);
    mode = kain_native_asset_gl_mode(primitive->type);
    element_count = primitive->indices ? primitive->indices->count : positions->count;
    glBegin(mode);
    for (element_index = 0; element_index < element_count; ++element_index) {
        cgltf_size accessor_index = primitive->indices ? cgltf_accessor_read_index(primitive->indices, element_index) : element_index;
        cgltf_float point[3];
        cgltf_float normal[3];

        if (!cgltf_accessor_read_float(positions, accessor_index, point, 3)) {
            continue;
        }

        if (normals && cgltf_accessor_read_float(normals, accessor_index, normal, 3)) {
            glNormal3f(normal[0], normal[1], normal[2]);
        }

        glVertex3f(point[0], point[1], point[2]);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    if (!context->render_blend_pass) {
        glDisable(GL_BLEND);
    }
}

static void kain_native_asset_compile_node(
    KainNativeAssetCompileContext* context,
    const cgltf_node* node
) {
    cgltf_float local_matrix[16];
    cgltf_size primitive_index;
    cgltf_size child_index;

    if (!context || !node) {
        return;
    }

    cgltf_node_transform_local(node, local_matrix);
    glPushMatrix();
    glMultMatrixf(local_matrix);

    if (node->mesh) {
        for (primitive_index = 0; primitive_index < node->mesh->primitives_count; ++primitive_index) {
            const cgltf_primitive* primitive = &node->mesh->primitives[primitive_index];
            const cgltf_accessor* positions = cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
            int blended = primitive->material && primitive->material->alpha_mode == cgltf_alpha_mode_blend;
            if (!positions || positions->type != cgltf_type_vec3 || blended != context->render_blend_pass) {
                continue;
            }
            if (kain_native_asset_should_skip_primitive(context, node, primitive, positions)) {
                continue;
            }
            kain_native_asset_emit_primitive(context, node, primitive);
        }
    }

    for (child_index = 0; child_index < node->children_count; ++child_index) {
        kain_native_asset_compile_node(context, node->children[child_index]);
    }

    glPopMatrix();
}

static void kain_native_asset_count_nodes(KainNativeSceneAsset* asset, const cgltf_node* node) {
    cgltf_size child_index;
    if (!asset || !node) {
        return;
    }
    asset->node_count += 1;
    if (node->mesh) {
        asset->mesh_count += 1;
    }
    for (child_index = 0; child_index < node->children_count; ++child_index) {
        kain_native_asset_count_nodes(asset, node->children[child_index]);
    }
}

static void kain_native_asset_capture_label(const char* path, KainNativeSceneAsset* asset) {
    const char* file_name;
    size_t length;

    if (!asset) {
        return;
    }

    file_name = path;
    if (path) {
        const char* slash = strrchr(path, '/');
        const char* backslash = strrchr(path, '\\');
        if (slash && (!backslash || slash > backslash)) {
            file_name = slash + 1;
        } else if (backslash) {
            file_name = backslash + 1;
        }
    }

    if (!file_name || !file_name[0]) {
        file_name = "world.glb";
    }

    length = strlen(file_name);
    if (length >= sizeof(asset->asset_label)) {
        length = sizeof(asset->asset_label) - 1;
    }
    memcpy(asset->asset_label, file_name, length);
    asset->asset_label[length] = '\0';
}

void kain_native_scene_asset_init(KainNativeSceneAsset* asset) {
    if (!asset) {
        return;
    }

    ZeroMemory(asset, sizeof(*asset));
    asset->raw_bounds_min = kain_vec3_make(1.0e30, 1.0e30, 1.0e30);
    asset->raw_bounds_max = kain_vec3_make(-1.0e30, -1.0e30, -1.0e30);
    asset->world_scale = 1.0;
    asset->recommended_spawn_distance = 24.0;
    asset->recommended_far_clip = 180.0;
}

void kain_runtime_ingestion_descriptor_init(KainRuntimeIngestionDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    ZeroMemory(descriptor, sizeof(*descriptor));
}

void kain_runtime_ingestion_descriptor_from_path(
    KainRuntimeIngestionDescriptor* descriptor,
    KainRuntimeIngestionPayloadKind payload_kind,
    KainRuntimeIngestionSourceKind source_kind,
    const char* source_path,
    const char* logical_name
) {
    if (!descriptor) {
        return;
    }

    kain_runtime_ingestion_descriptor_init(descriptor);
    descriptor->declared = 1;
    descriptor->payload_kind = payload_kind;
    descriptor->source_kind = source_kind;
    descriptor->target_kind = KAIN_SCENE_RESOURCE_SCENE;
    if (source_path && source_path[0]) {
        strncpy_s(descriptor->source_path, sizeof(descriptor->source_path), source_path, _TRUNCATE);
    }
    if (logical_name && logical_name[0]) {
        strncpy_s(descriptor->logical_name, sizeof(descriptor->logical_name), logical_name, _TRUNCATE);
    }
}

void kain_native_scene_asset_describe_ingestion(
    const KainNativeSceneAsset* asset,
    KainRuntimeIngestionDescriptor* descriptor
) {
    if (!descriptor) {
        return;
    }

    kain_runtime_ingestion_descriptor_init(descriptor);
    descriptor->declared = asset && asset->loaded;
    descriptor->payload_kind = KAIN_RUNTIME_INGESTION_PAYLOAD_SCENE_ASSET;
    descriptor->source_kind = KAIN_RUNTIME_INGESTION_SOURCE_HOST_STAGED;
    descriptor->target_kind = KAIN_SCENE_RESOURCE_SCENE;
    if (!asset || !asset->loaded) {
        strncpy_s(descriptor->detail, sizeof(descriptor->detail), "scene asset not loaded", _TRUNCATE);
        return;
    }

    descriptor->target_scene = kain_scene_handle_make(KAIN_SCENE_RESOURCE_SCENE, 1u, 1u);
    strncpy_s(descriptor->source_path, sizeof(descriptor->source_path), asset->source_path, _TRUNCATE);
    strncpy_s(descriptor->logical_name, sizeof(descriptor->logical_name), asset->asset_label, _TRUNCATE);
    strncpy_s(
        descriptor->detail,
        sizeof(descriptor->detail),
        "host-staged glTF scene asset compiled into native viewport geometry",
        _TRUNCATE
    );
}

void kain_native_scene_asset_shutdown(KainNativeSceneAsset* asset) {
    if (!asset) {
        return;
    }

    if (asset->opaque_display_list != 0) {
        glDeleteLists(asset->opaque_display_list, 1);
    }
    if (asset->blend_display_list != 0) {
        glDeleteLists(asset->blend_display_list, 1);
    }
    kain_native_scene_asset_init(asset);
}

int kain_native_scene_asset_load_from_path(const char* path, KainNativeSceneAsset* asset) {
    cgltf_options options;
    cgltf_data* data = NULL;
    const cgltf_scene* scene;
    KainNativeAssetCompileContext context;
    cgltf_result result;
    cgltf_size root_index;
    double raw_extent_x;
    double raw_extent_y;
    double raw_extent_z;
    double raw_max_extent;
    char* explicit_scale_value = NULL;

    if (!path || !path[0] || !asset) {
        return 0;
    }

    kain_native_scene_asset_shutdown(asset);
    ZeroMemory(&options, sizeof(options));

    result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success || !data) {
        printf("[KAIN] Failed to parse GLB asset '%s' (cgltf result %d)\n", path, (int)result);
        return 0;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        printf("[KAIN] Failed to load GLB buffers for '%s' (cgltf result %d)\n", path, (int)result);
        cgltf_free(data);
        return 0;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        printf("[KAIN] Warning: GLB validation reported issues for '%s' (cgltf result %d)\n", path, (int)result);
    }

    scene = data->scene ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : NULL);
    if (!scene || scene->nodes_count == 0) {
        printf("[KAIN] GLB asset '%s' does not contain a usable scene.\n", path);
        cgltf_free(data);
        return 0;
    }

    asset->opaque_display_list = glGenLists(1);
    asset->blend_display_list = glGenLists(1);
    if (asset->opaque_display_list == 0 || asset->blend_display_list == 0) {
        printf("[KAIN] Failed to allocate OpenGL display lists for '%s'.\n", path);
        cgltf_free(data);
        return 0;
    }

    for (root_index = 0; root_index < scene->nodes_count; ++root_index) {
        kain_native_asset_count_nodes(asset, scene->nodes[root_index]);
    }

    ZeroMemory(&context, sizeof(context));
    context.asset = asset;
    context.skip_sky_primitives = kain_env_flag(KAIN_NATIVE_WORLD_SKIP_SKY_ENV, 1);
    context.render_blend_pass = 0;
    context.collect_stats = 1;
    glNewList(asset->opaque_display_list, GL_COMPILE);
    for (root_index = 0; root_index < scene->nodes_count; ++root_index) {
        kain_native_asset_compile_node(&context, scene->nodes[root_index]);
    }
    glEndList();

    context.render_blend_pass = 1;
    context.collect_stats = 1;
    glNewList(asset->blend_display_list, GL_COMPILE);
    for (root_index = 0; root_index < scene->nodes_count; ++root_index) {
        kain_native_asset_compile_node(&context, scene->nodes[root_index]);
    }
    glEndList();

    if (asset->primitive_count == 0) {
        printf("[KAIN] GLB asset '%s' did not produce any renderable primitives.\n", path);
        cgltf_free(data);
        return 0;
    }

    raw_extent_x = asset->raw_bounds_max.x - asset->raw_bounds_min.x;
    raw_extent_y = asset->raw_bounds_max.y - asset->raw_bounds_min.y;
    raw_extent_z = asset->raw_bounds_max.z - asset->raw_bounds_min.z;
    raw_max_extent = kain_native_asset_max3(raw_extent_x, raw_extent_y, raw_extent_z);
    if (raw_max_extent <= 0.0001) {
        raw_max_extent = 1.0;
    }

    explicit_scale_value = kain_env_dup(KAIN_NATIVE_WORLD_SCALE_ENV);
    if (explicit_scale_value && explicit_scale_value[0]) {
        asset->world_scale = atof(explicit_scale_value);
    } else {
        double target_extent = kain_env_double(KAIN_NATIVE_WORLD_TARGET_EXTENT_ENV, 88.0);
        if (target_extent < 8.0) {
            target_extent = 8.0;
        }
        asset->world_scale = target_extent / raw_max_extent;
    }
    kain_env_free(explicit_scale_value);

    asset->raw_origin_offset = kain_vec3_make(
        -((asset->raw_bounds_min.x + asset->raw_bounds_max.x) * 0.5),
        -(asset->raw_bounds_min.y),
        -((asset->raw_bounds_min.z + asset->raw_bounds_max.z) * 0.5)
    );
    asset->world_bounds_min = kain_vec3_make(
        (asset->raw_bounds_min.x + asset->raw_origin_offset.x) * asset->world_scale,
        (asset->raw_bounds_min.y + asset->raw_origin_offset.y) * asset->world_scale,
        (asset->raw_bounds_min.z + asset->raw_origin_offset.z) * asset->world_scale
    );
    asset->world_bounds_max = kain_vec3_make(
        (asset->raw_bounds_max.x + asset->raw_origin_offset.x) * asset->world_scale,
        (asset->raw_bounds_max.y + asset->raw_origin_offset.y) * asset->world_scale,
        (asset->raw_bounds_max.z + asset->raw_origin_offset.z) * asset->world_scale
    );
    asset->world_center = kain_vec3_make(
        (asset->world_bounds_min.x + asset->world_bounds_max.x) * 0.5,
        (asset->world_bounds_min.y + asset->world_bounds_max.y) * 0.5,
        (asset->world_bounds_min.z + asset->world_bounds_max.z) * 0.5
    );
    asset->ground_height = asset->world_bounds_min.y;
    asset->recommended_spawn_distance = kain_clampd(
        (asset->world_bounds_max.y - asset->world_bounds_min.y) * 0.65 + 10.0,
        18.0,
        42.0
    );
    asset->recommended_far_clip =
        kain_native_asset_max3(
            asset->world_bounds_max.x - asset->world_bounds_min.x,
            asset->world_bounds_max.y - asset->world_bounds_min.y,
            asset->world_bounds_max.z - asset->world_bounds_min.z
        ) * 5.0 + 64.0;

    strncpy_s(asset->source_path, sizeof(asset->source_path), path, _TRUNCATE);
    kain_native_asset_capture_label(path, asset);
    asset->loaded = 1;

    printf(
        "[KAIN] Loaded world asset '%s' | nodes=%llu meshes=%llu primitives=%llu tris=%llu scale=%.4f\n",
        asset->asset_label,
        asset->node_count,
        asset->mesh_count,
        asset->primitive_count,
        asset->triangle_count,
        asset->world_scale
    );

    cgltf_free(data);
    return 1;
}

int kain_native_scene_asset_load_from_env(const char* env_name, KainNativeSceneAsset* asset) {
    char* path;
    int result;

    if (!asset) {
        return 0;
    }

    path = kain_env_dup(env_name && env_name[0] ? env_name : KAIN_NATIVE_WORLD_ASSET_ENV);
    if (!path) {
        return 0;
    }

    result = kain_native_scene_asset_load_from_path(path, asset);
    kain_env_free(path);
    return result;
}

void kain_native_scene_asset_render(const KainNativeSceneAsset* asset) {
    if (!asset || !asset->loaded) {
        return;
    }

    glPushMatrix();
    glScaled(asset->world_scale, asset->world_scale, asset->world_scale);
    glTranslated(asset->raw_origin_offset.x, asset->raw_origin_offset.y, asset->raw_origin_offset.z);
    if (asset->opaque_display_list != 0) {
        glCallList(asset->opaque_display_list);
    }
    if (asset->blend_display_list != 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glCallList(asset->blend_display_list);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glPopMatrix();
}
#endif
