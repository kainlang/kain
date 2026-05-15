#include "../../native/include/kain_runtime_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kGraphicsBundleJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"geometry_fixture\",\n"
    "        \"title\": \"Generic Scene\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::vertex\",\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": [\n"
    "      {\n"
    "        \"id\": \"terrain\",\n"
    "        \"source\": \"kain-core\",\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
    "    {\n"
    "      \"key\": \"shader::terrain::vertex\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"vertex\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::fragment\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"fragment\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::compute\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"compute\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"workgroup_size\": [8, 8, 1],\n"
    "      \"dispatch_size\": [16, 16, 1],\n"
    "      \"resource_bindings\": [\n"
    "        {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 0},\n"
    "        {\"key\": \"terrain.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 1}\n"
    "      ],\n"
    "      \"source\": \"kain-core\"\n"
    "    }\n"
    "  ],\n"
    "  \"assets\": [\n"
    "    {\n"
    "      \"key\": \"asset::terrain\",\n"
    "      \"kind\": \"runtime\",\n"
    "      \"source\": \"terrain.glb\"\n"
    "    }\n"
    "  ],\n"
    "  \"tool_caps\": [\"viewport.3d\", \"tool.graph\"],\n"
    "  \"requirements\": [\n"
    "    \"host.raw-native\",\n"
    "    \"runtime.contract.bundle\",\n"
    "    \"shader.bundle.metadata\"\n"
    "  ]\n"
    "}\n";

static const char* kRustGraphicsBundleJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"rust\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"geometry_fixture\",\n"
    "        \"title\": \"Generic Scene\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\"shader::terrain::vertex\"]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": []\n"
    "  },\n"
    "  \"shader_bundle_refs\": [],\n"
    "  \"assets\": [],\n"
    "  \"tool_caps\": [],\n"
    "  \"requirements\": []\n"
    "}\n";

static const char* kInvalidMaterialGraphicsBundleJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"geometry_fixture\",\n"
    "        \"title\": \"Generic Scene\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::vertex\",\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": [\n"
    "      {\n"
    "        \"id\": \"terrain\",\n"
    "        \"source\": \"kain-core\",\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
    "    {\n"
    "      \"key\": \"shader::terrain::vertex\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"vertex\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::fragment\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"fragment\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::compute\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"compute\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"workgroup_size\": [8, 8, 1],\n"
    "      \"dispatch_size\": [16, 16, 1],\n"
    "      \"resource_bindings\": [\n"
    "        {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 0},\n"
    "        {\"key\": \"terrain.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 1}\n"
    "      ],\n"
    "      \"source\": \"kain-core\"\n"
    "    }\n"
    "  ],\n"
    "  \"assets\": [\n"
    "    {\n"
    "      \"key\": \"asset::terrain\",\n"
    "      \"kind\": \"runtime\",\n"
    "      \"source\": \"terrain.glb\"\n"
    "    }\n"
    "  ],\n"
    "  \"tool_caps\": [\"viewport.3d\", \"tool.graph\"],\n"
    "  \"requirements\": [\n"
    "    \"host.raw-native\",\n"
    "    \"runtime.contract.bundle\",\n"
    "    \"shader.bundle.metadata\"\n"
    "  ]\n"
    "}\n";

static const char* kInvalidComputeGraphicsBundleJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"geometry_fixture\",\n"
    "        \"title\": \"Generic Scene\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::vertex\",\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": [\n"
    "      {\n"
    "        \"id\": \"terrain\",\n"
    "        \"source\": \"kain-core\",\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [\n"
    "          {\"key\": \"roughness\", \"type\": \"float\", \"default\": 0.65}\n"
    "        ],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 1}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
    "    {\n"
    "      \"key\": \"shader::terrain::vertex\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"vertex\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::fragment\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"fragment\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"source\": \"kain-core\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"shader::terrain::compute\",\n"
    "      \"shader\": \"terrain\",\n"
    "      \"module_name\": \"terrain\",\n"
    "      \"stage\": \"compute\",\n"
    "      \"entry_point\": \"main\",\n"
    "      \"workgroup_size\": [8, 8, 1],\n"
    "      \"dispatch_size\": [16, 16, 1],\n"
    "      \"resource_bindings\": [\n"
    "        {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"access\": \"read\", \"slot\": 0},\n"
    "        {\"key\": \"terrain.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 1}\n"
    "      ],\n"
    "      \"source\": \"kain-core\"\n"
    "    }\n"
    "  ],\n"
    "  \"assets\": [\n"
    "    {\n"
    "      \"key\": \"asset::terrain\",\n"
    "      \"kind\": \"runtime\",\n"
    "      \"source\": \"terrain.glb\"\n"
    "    }\n"
    "  ],\n"
    "  \"tool_caps\": [\"viewport.3d\", \"tool.graph\"],\n"
    "  \"requirements\": [\n"
    "    \"host.raw-native\",\n"
    "    \"runtime.contract.bundle\",\n"
    "    \"shader.bundle.metadata\"\n"
    "  ]\n"
    "}\n";

static int check_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "[FAIL] %s\n", label);
        return 0;
    }
    return 1;
}

static int check_str_eq(const char* actual, const char* expected, const char* label) {
    if (!actual || !expected || strcmp(actual, expected) != 0) {
        fprintf(stderr, "[FAIL] %s expected '%s' got '%s'\n",
            label,
            expected ? expected : "<null>",
            actual ? actual : "<null>");
        return 0;
    }
    return 1;
}

static int check_contains(const char* actual, const char* needle, const char* label) {
    if (!actual || !needle || strstr(actual, needle) == NULL) {
        fprintf(stderr, "[FAIL] %s expected substring '%s' in '%s'\n",
            label,
            needle ? needle : "<null>",
            actual ? actual : "<null>");
        return 0;
    }
    return 1;
}

static int write_text_file(const char* path, const char* text) {
    FILE* file;
    size_t text_len;
    if (!path || !text) {
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        return 0;
    }
    text_len = strlen(text);
    if (fwrite(text, 1, text_len, file) != text_len) {
        fclose(file);
        remove(path);
        return 0;
    }
    fclose(file);
    return 1;
}

static int test_graphics_bundle_from_json(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;
    char summary[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];

    if (!check_true(kain_runtime_graphics_load_from_json(kGraphicsBundleJson, &bundle), "load_from_json")) {
        return 0;
    }
    if (!check_true(bundle.loaded == 1, "bundle.loaded")) return 0;
    if (!check_true(bundle.schema_version == 1, "bundle.schema_version")) return 0;
    if (!check_str_eq(bundle.target, "llvm", "bundle.target")) return 0;
    if (!check_true(bundle.scene_count == 1, "bundle.scene_count")) return 0;
    if (!check_true(bundle.material_count == 1, "bundle.material_count")) return 0;
    if (!check_true(bundle.shader_bundle_ref_count == 3, "bundle.shader_bundle_ref_count")) return 0;
    if (!check_true(bundle.shader_vertex_ref_count == 1, "bundle.shader_vertex_ref_count")) return 0;
    if (!check_true(bundle.shader_fragment_ref_count == 1, "bundle.shader_fragment_ref_count")) return 0;
    if (!check_true(bundle.shader_compute_ref_count == 1, "bundle.shader_compute_ref_count")) return 0;
    if (!check_true(bundle.asset_count == 1, "bundle.asset_count")) return 0;
    if (!check_true(bundle.tool_cap_count == 2, "bundle.tool_cap_count")) return 0;
    if (!check_true(bundle.requirement_count == 3, "bundle.requirement_count")) return 0;
    if (!check_true(bundle.material_shader_ref_key_count == 2, "bundle.material_shader_ref_key_count")) return 0;
    if (!check_true(bundle.primary_material_ref_count == 1, "bundle.primary_material_ref_count")) return 0;
    if (!check_true(bundle.primary_shader_ref_key_count == 3, "bundle.primary_shader_ref_key_count")) return 0;
    if (!check_true(bundle.primary_material.loaded == 1, "bundle.primary_material.loaded")) return 0;
    if (!check_true(bundle.primary_material.shader_ref_count == 2, "bundle.primary_material.shader_ref_count")) return 0;
    if (!check_true(bundle.primary_material.parameter_count == 1, "bundle.primary_material.parameter_count")) return 0;
    if (!check_true(bundle.primary_material.resource_binding_count == 2, "bundle.primary_material.resource_binding_count")) return 0;
    if (!check_str_eq(bundle.primary_material.material_id, "terrain", "bundle.primary_material.material_id")) return 0;
    if (!check_contains(bundle.primary_material.resource_bindings[0].key, "terrain.albedo", "bundle.primary_material.resource_bindings[0].key")) return 0;
    if (!check_str_eq(bundle.primary_material.resource_bindings[1].access, "read_write", "bundle.primary_material.resource_bindings[1].access")) return 0;
    if (!check_true(bundle.primary_compute.loaded == 1, "bundle.primary_compute.loaded")) return 0;
    if (!check_str_eq(bundle.primary_compute.shader_key, "shader::terrain::compute", "bundle.primary_compute.shader_key")) return 0;
    if (!check_str_eq(bundle.primary_compute.entry_point, "main", "bundle.primary_compute.entry_point")) return 0;
    if (!check_true(bundle.primary_compute.workgroup_size[0] == 8, "bundle.primary_compute.workgroup_size[0]")) return 0;
    if (!check_true(bundle.primary_compute.workgroup_size[1] == 8, "bundle.primary_compute.workgroup_size[1]")) return 0;
    if (!check_true(bundle.primary_compute.workgroup_size[2] == 1, "bundle.primary_compute.workgroup_size[2]")) return 0;
    if (!check_true(bundle.primary_compute.dispatch_size[0] == 16, "bundle.primary_compute.dispatch_size[0]")) return 0;
    if (!check_true(bundle.primary_compute.dispatch_size[1] == 16, "bundle.primary_compute.dispatch_size[1]")) return 0;
    if (!check_true(bundle.primary_compute.dispatch_size[2] == 1, "bundle.primary_compute.dispatch_size[2]")) return 0;
    if (!check_true(bundle.primary_compute.resource_binding_count == 2, "bundle.primary_compute.resource_binding_count")) return 0;
    if (!check_str_eq(bundle.primary_compute.resource_bindings[0].resource_type, "storage_buffer", "bundle.primary_compute.resource_bindings[0].resource_type")) return 0;
    if (!check_str_eq(bundle.primary_compute.resource_bindings[1].access, "write", "bundle.primary_compute.resource_bindings[1].access")) return 0;
    if (!check_str_eq(bundle.primary_viewport_kind, "viewport3d", "bundle.primary_viewport_kind")) return 0;
    if (!check_str_eq(bundle.primary_scene, "geometry_fixture", "bundle.primary_scene")) return 0;
    if (!check_str_eq(bundle.primary_title, "Generic Scene", "bundle.primary_title")) return 0;
    if (!check_str_eq(bundle.primary_material_refs, "terrain", "bundle.primary_material_refs")) return 0;
    if (!check_contains(bundle.primary_shader_ref_keys, "shader::terrain::compute", "bundle.primary_shader_ref_keys")) return 0;

    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation), "validate_bundle")) return 0;
    if (!check_true(validation.graphics_lane_ready == 1, "validation.graphics_lane_ready")) return 0;
    if (!check_true(validation.compute_metadata_valid == 1, "validation.compute_metadata_valid")) return 0;
    if (!check_true(validation.has_compute_artifacts == 1, "validation.has_compute_artifacts")) return 0;
    if (!check_true(validation.has_material_bindings == 1, "validation.has_material_bindings")) return 0;
    if (!check_true(validation.material_binding_valid == 1, "validation.material_binding_valid")) return 0;
    if (!check_true(validation.compute_plan_valid == 1, "validation.compute_plan_valid")) return 0;
    if (!check_true(kain_runtime_viewport_supports_graphics_bundle(&bundle) == 1, "viewport_supports_graphics_bundle")) return 0;
    if (!check_contains(validation.reason, "ready", "validation.reason")) return 0;

    kain_runtime_graphics_format_summary(&bundle, summary, sizeof(summary));
    if (!check_contains(summary, "shader refs v/f/c=1/1/1", "summary shader refs")) return 0;
    if (!check_contains(summary, "compute bind/t/s/n=2/0/0/0", "summary compute bindings")) return 0;
    if (!check_contains(summary, "graph p/a/d=3/4/2", "summary render graph")) return 0;
    if (!check_contains(summary, "schedule s/b=3/2", "summary compute schedule")) return 0;

    return 1;
}

static int test_graphics_material_and_compute_snapshot_persistence(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsBundle snapshot;
    KainRuntimeGraphicsValidation validation;
    char summary[KAIN_RUNTIME_GRAPHICS_MAX_SUMMARY];

    if (!check_true(kain_runtime_graphics_load_from_json(kGraphicsBundleJson, &bundle), "load_snapshot_bundle")) {
        return 0;
    }

    snapshot = bundle;
    memset(&bundle, 0, sizeof(bundle));

    if (!check_true(snapshot.loaded == 1, "snapshot.loaded")) return 0;
    if (!check_true(snapshot.primary_material.loaded == 1, "snapshot.primary_material.loaded")) return 0;
    if (!check_true(snapshot.primary_material.resource_binding_count == 2, "snapshot.primary_material.resource_binding_count")) return 0;
    if (!check_true(snapshot.primary_compute.loaded == 1, "snapshot.primary_compute.loaded")) return 0;
    if (!check_true(snapshot.primary_compute.resource_binding_count == 2, "snapshot.primary_compute.resource_binding_count")) return 0;
    if (!check_true(snapshot.primary_compute.workgroup_size[0] == 8, "snapshot.primary_compute.workgroup_size[0]")) return 0;
    if (!check_true(snapshot.primary_compute.dispatch_size[0] == 16, "snapshot.primary_compute.dispatch_size[0]")) return 0;
    if (!check_true(kain_runtime_graphics_validate_bundle(&snapshot, &validation), "validate_snapshot_bundle")) return 0;
    if (!check_true(validation.graphics_lane_ready == 1, "validation.graphics_lane_ready(snapshot)")) return 0;
    if (!check_true(validation.material_binding_valid == 1, "validation.material_binding_valid(snapshot)")) return 0;
    if (!check_true(validation.compute_plan_valid == 1, "validation.compute_plan_valid(snapshot)")) return 0;
    if (!check_true(kain_runtime_viewport_supports_graphics_bundle(&snapshot) == 1, "viewport_supports_graphics_bundle(snapshot)")) return 0;

    kain_runtime_graphics_format_summary(&snapshot, summary, sizeof(summary));
    if (!check_contains(summary, "shader refs v/f/c=1/1/1", "snapshot summary shader refs")) return 0;
    if (!check_contains(summary, "compute bind/t/s/n=2/0/0/0", "snapshot summary compute bindings")) return 0;
    if (!check_contains(summary, "schedule s/b=3/2", "snapshot summary compute schedule")) return 0;

    return 1;
}

static int test_graphics_bundle_from_path(void) {
    const char* temp_path = "graphics_runtime_smoke_bundle.realtime_app.json";
    KainRuntimeGraphicsBundle bundle;

    remove(temp_path);
    if (!check_true(write_text_file(temp_path, kGraphicsBundleJson), "write_text_file")) {
        return 0;
    }
    if (!check_true(kain_runtime_graphics_load_from_path(temp_path, &bundle), "load_from_path")) {
        remove(temp_path);
        return 0;
    }
    if (!check_str_eq(bundle.load_origin, "path", "bundle.load_origin")) {
        remove(temp_path);
        return 0;
    }
    if (!check_str_eq(bundle.source_path, temp_path, "bundle.source_path")) {
        remove(temp_path);
        return 0;
    }
    if (!check_true(kain_runtime_viewport_supports_graphics_bundle(&bundle) == 1, "viewport_supports_graphics_bundle(path)")) {
        remove(temp_path);
        return 0;
    }
    remove(temp_path);
    return 1;
}

static int test_graphics_bundle_from_env(void) {
    const char* temp_path = "graphics_runtime_smoke_env_bundle.realtime_app.json";
    KainRuntimeGraphicsBundle bundle;

    remove(temp_path);
    if (!check_true(write_text_file(temp_path, kGraphicsBundleJson), "write_text_file(env)")) {
        return 0;
    }
    if (!check_true(_putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, temp_path) == 0, "_putenv_s(set)")) {
        remove(temp_path);
        return 0;
    }
    if (!check_true(kain_runtime_graphics_load_from_env(KAIN_RUNTIME_GRAPHICS_ENV, &bundle), "load_from_env")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }
    if (!check_str_eq(bundle.load_origin, "env", "bundle.load_origin(env)")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }
    if (!check_str_eq(bundle.source_path, temp_path, "bundle.source_path(env)")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }
    if (!check_true(kain_runtime_graphics_load_for_current_process(KAIN_RUNTIME_GRAPHICS_ENV, &bundle), "load_for_current_process")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }
    if (!check_str_eq(bundle.load_origin, "env", "bundle.load_origin(current_process)")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }
    if (!check_true(kain_runtime_viewport_supports_graphics_bundle(&bundle) == 1, "viewport_supports_graphics_bundle(env)")) {
        _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
        remove(temp_path);
        return 0;
    }

    _putenv_s(KAIN_RUNTIME_GRAPHICS_ENV, "");
    remove(temp_path);
    return 1;
}

static int test_graphics_invalid_and_rejected_target(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;

    if (!check_true(kain_runtime_graphics_load_from_json("{\"schema_version\": 1, \"target\": \"llvm\"", &bundle) == 0,
            "invalid_json_rejected")) {
        return 0;
    }
    if (!check_true(kain_runtime_graphics_load_from_json(kRustGraphicsBundleJson, &bundle), "load_rust_bundle")) {
        return 0;
    }
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation) == 0, "validate_rust_bundle")) {
        return 0;
    }
    if (!check_true(validation.graphics_lane_ready == 0, "validation.graphics_lane_ready(rust)")) {
        return 0;
    }
    if (!check_contains(validation.reason, "llvm", "validation.reason(rust)")) {
        return 0;
    }
    if (!check_true(kain_runtime_viewport_supports_graphics_bundle(&bundle) == 0, "viewport_supports_graphics_bundle(rust)")) {
        return 0;
    }
    return 1;
}

static int test_graphics_rejects_incomplete_material_plan(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;

    if (!check_true(kain_runtime_graphics_load_from_json(kInvalidMaterialGraphicsBundleJson, &bundle), "load_invalid_material_bundle")) {
        return 0;
    }
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation) == 0, "validate_invalid_material_bundle")) {
        return 0;
    }
    if (!check_true(validation.has_material_bindings == 0, "validation.has_material_bindings(invalid material)")) {
        return 0;
    }
    if (!check_true(validation.material_binding_valid == 0, "validation.material_binding_valid(invalid material)")) {
        return 0;
    }
    if (!check_true(validation.graphics_lane_ready == 0, "validation.graphics_lane_ready(invalid material)")) {
        return 0;
    }
    if (!check_contains(validation.reason, "material binding plan", "validation.reason(invalid material)")) {
        return 0;
    }
    return 1;
}

static int test_graphics_rejects_incomplete_compute_plan(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;

    if (!check_true(kain_runtime_graphics_load_from_json(kInvalidComputeGraphicsBundleJson, &bundle), "load_invalid_compute_bundle")) {
        return 0;
    }
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation) == 0, "validate_invalid_compute_bundle")) {
        return 0;
    }
    if (!check_true(validation.has_material_bindings == 1, "validation.has_material_bindings(invalid compute)")) {
        return 0;
    }
    if (!check_true(validation.compute_plan_valid == 0, "validation.compute_plan_valid(invalid compute)")) {
        return 0;
    }
    if (!check_true(validation.graphics_lane_ready == 0, "validation.graphics_lane_ready(invalid compute)")) {
        return 0;
    }
    if (!check_contains(validation.reason, "compute dispatch plan", "validation.reason(invalid compute)")) {
        return 0;
    }
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 7;

    printf("Running graphics runtime smoke tests\n");

    if (test_graphics_bundle_from_json()) {
        printf("[PASS] graphics bundle from json\n");
        ++passed;
    }
    if (test_graphics_material_and_compute_snapshot_persistence()) {
        printf("[PASS] graphics material and compute snapshot persistence\n");
        ++passed;
    }
    if (test_graphics_bundle_from_path()) {
        printf("[PASS] graphics bundle from path\n");
        ++passed;
    }
    if (test_graphics_bundle_from_env()) {
        printf("[PASS] graphics bundle from env\n");
        ++passed;
    }
    if (test_graphics_invalid_and_rejected_target()) {
        printf("[PASS] invalid and target-rejected bundles\n");
        ++passed;
    }
    if (test_graphics_rejects_incomplete_material_plan()) {
        printf("[PASS] incomplete material plan rejected\n");
        ++passed;
    }
    if (test_graphics_rejects_incomplete_compute_plan()) {
        printf("[PASS] incomplete compute plan rejected\n");
        ++passed;
    }

    printf("Graphics runtime smoke summary: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
