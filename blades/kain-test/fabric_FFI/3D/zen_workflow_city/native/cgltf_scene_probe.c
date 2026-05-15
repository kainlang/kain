#include "cgltf_scene_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define CGLTF_IMPLEMENTATION
#include "vendor/cgltf.h"

typedef struct CgltfSceneProbe {
    cgltf_data* data;
    int node_count;
    int mesh_count;
    int primitive_count;
    int material_count;
    int vertex_count;
    char scene_name[128];
    char signature[256];
} CgltfSceneProbe;

static void cgltf_probe_count_node(CgltfSceneProbe* probe, const cgltf_node* node) {
    cgltf_size child_index;
    probe->node_count += 1;

    if (node->mesh) {
        cgltf_size primitive_index;
        probe->mesh_count += 1;
        for (primitive_index = 0; primitive_index < node->mesh->primitives_count; ++primitive_index) {
            const cgltf_primitive* primitive = &node->mesh->primitives[primitive_index];
            const cgltf_accessor* positions = NULL;
            cgltf_size attribute_index;
            probe->primitive_count += 1;
            for (attribute_index = 0; attribute_index < primitive->attributes_count; ++attribute_index) {
                const cgltf_attribute* attribute = &primitive->attributes[attribute_index];
                if (attribute->type == cgltf_attribute_type_position) {
                    positions = attribute->data;
                    break;
                }
            }
            if (positions) {
                probe->vertex_count += (int)positions->count;
            }
        }
    }

    for (child_index = 0; child_index < node->children_count; ++child_index) {
        cgltf_probe_count_node(probe, node->children[child_index]);
    }
}

static void cgltf_probe_finalize(CgltfSceneProbe* probe) {
    const char* scene_name = probe->scene_name[0] ? probe->scene_name : "default_scene";
    snprintf(
        probe->signature,
        sizeof(probe->signature),
        "scene=%s|nodes=%d|meshes=%d|primitives=%d|materials=%d|vertices=%d",
        scene_name,
        probe->node_count,
        probe->mesh_count,
        probe->primitive_count,
        probe->material_count,
        probe->vertex_count
    );
}

CGLTF_PROBE_EXPORT CgltfSceneProbe* cgltf_probe_open(const char* path) {
    cgltf_options options;
    cgltf_data* data = NULL;
    cgltf_result result;
    const cgltf_scene* scene;
    cgltf_size root_index;
    CgltfSceneProbe* probe;

    if (!path || !path[0]) {
        return NULL;
    }

    memset(&options, 0, sizeof(options));
    result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success || !data) {
        return NULL;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return NULL;
    }

    probe = (CgltfSceneProbe*)calloc(1, sizeof(CgltfSceneProbe));
    if (!probe) {
        cgltf_free(data);
        return NULL;
    }

    probe->data = data;
    probe->material_count = (int)data->materials_count;
    scene = data->scene ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : NULL);
    if (scene && scene->name && scene->name[0]) {
        strncpy(probe->scene_name, scene->name, sizeof(probe->scene_name) - 1);
        probe->scene_name[sizeof(probe->scene_name) - 1] = '\0';
    } else {
        strncpy(probe->scene_name, "default_scene", sizeof(probe->scene_name) - 1);
        probe->scene_name[sizeof(probe->scene_name) - 1] = '\0';
    }

    if (scene) {
        for (root_index = 0; root_index < scene->nodes_count; ++root_index) {
            cgltf_probe_count_node(probe, scene->nodes[root_index]);
        }
    }

    cgltf_probe_finalize(probe);
    return probe;
}

CGLTF_PROBE_EXPORT int cgltf_probe_node_count(CgltfSceneProbe* probe) {
    return probe ? probe->node_count : 0;
}

CGLTF_PROBE_EXPORT int cgltf_probe_mesh_count(CgltfSceneProbe* probe) {
    return probe ? probe->mesh_count : 0;
}

CGLTF_PROBE_EXPORT int cgltf_probe_primitive_count(CgltfSceneProbe* probe) {
    return probe ? probe->primitive_count : 0;
}

CGLTF_PROBE_EXPORT int cgltf_probe_material_count(CgltfSceneProbe* probe) {
    return probe ? probe->material_count : 0;
}

CGLTF_PROBE_EXPORT int cgltf_probe_vertex_count(CgltfSceneProbe* probe) {
    return probe ? probe->vertex_count : 0;
}

CGLTF_PROBE_EXPORT const char* cgltf_probe_scene_name(CgltfSceneProbe* probe) {
    return probe ? probe->scene_name : "";
}

CGLTF_PROBE_EXPORT const char* cgltf_probe_signature(CgltfSceneProbe* probe) {
    return probe ? probe->signature : "";
}

CGLTF_PROBE_EXPORT void cgltf_probe_close(CgltfSceneProbe* probe) {
    if (!probe) {
        return;
    }
    if (probe->data) {
        cgltf_free(probe->data);
    }
    free(probe);
}
