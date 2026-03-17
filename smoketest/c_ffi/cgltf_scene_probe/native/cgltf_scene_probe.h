#if defined(_WIN32)
#define CGLTF_PROBE_EXPORT __declspec(dllexport)
#else
#define CGLTF_PROBE_EXPORT
#endif

typedef struct CgltfSceneProbe CgltfSceneProbe;

CGLTF_PROBE_EXPORT CgltfSceneProbe* cgltf_probe_open(const char* path);
CGLTF_PROBE_EXPORT int cgltf_probe_node_count(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT int cgltf_probe_mesh_count(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT int cgltf_probe_primitive_count(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT int cgltf_probe_material_count(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT int cgltf_probe_vertex_count(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT const char* cgltf_probe_scene_name(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT const char* cgltf_probe_signature(CgltfSceneProbe* probe);
CGLTF_PROBE_EXPORT void cgltf_probe_close(CgltfSceneProbe* probe);
