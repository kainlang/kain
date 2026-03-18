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
    "        \"scene\": \"magma_terraces\",\n"
    "        \"title\": \"Magma Terraces\",\n"
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
    "        \"scene\": \"magma_terraces\",\n"
    "        \"title\": \"Magma Terraces\",\n"
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
    if (!check_str_eq(bundle.primary_scene, "magma_terraces", "bundle.primary_scene")) return 0;
    if (!check_str_eq(bundle.primary_title, "Magma Terraces", "bundle.primary_title")) return 0;
    if (!check_str_eq(bundle.primary_material_refs, "terrain", "bundle.primary_material_refs")) return 0;
    if (!check_contains(bundle.primary_shader_ref_keys, "shader::terrain::compute", "bundle.primary_shader_ref_keys")) return 0;

    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation), "validate_bundle")) return 0;
    if (!check_true(validation.gl_lane_ready == 1, "validation.gl_lane_ready")) return 0;
    if (!check_true(validation.compute_metadata_valid == 1, "validation.compute_metadata_valid")) return 0;
    if (!check_true(validation.has_compute_artifacts == 1, "validation.has_compute_artifacts")) return 0;
    if (!check_true(validation.has_material_bindings == 1, "validation.has_material_bindings")) return 0;
    if (!check_true(validation.material_binding_valid == 1, "validation.material_binding_valid")) return 0;
    if (!check_true(validation.compute_plan_valid == 1, "validation.compute_plan_valid")) return 0;
    if (!check_true(kain_win32_gl_surface_supports_graphics_bundle(&bundle) == 1, "gl_surface_supports_graphics_bundle")) return 0;
    if (!check_contains(validation.reason, "ready", "validation.reason")) return 0;

    kain_runtime_graphics_format_summary(&bundle, summary, sizeof(summary));
    if (!check_contains(summary, "shader refs 3", "summary shader refs")) return 0;
    if (!check_contains(summary, "material bindings 2", "summary material bindings")) return 0;
    if (!check_contains(summary, "compute bindings 2", "summary compute bindings")) return 0;
    if (!check_contains(summary, "compute wg 8,8,1", "summary compute wg")) return 0;
    if (!check_contains(summary, "compute dispatch 16,16,1", "summary compute dispatch")) return 0;
    if (!check_contains(summary, "compute 1", "summary compute")) return 0;

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
    if (!check_true(kain_win32_gl_surface_supports_graphics_bundle(&bundle) == 1, "gl_surface_supports_graphics_bundle(path)")) {
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
    if (!check_true(kain_win32_gl_surface_supports_graphics_bundle(&bundle) == 1, "gl_surface_supports_graphics_bundle(env)")) {
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
    if (!check_true(validation.gl_lane_ready == 0, "validation.gl_lane_ready(rust)")) {
        return 0;
    }
    if (!check_contains(validation.reason, "llvm", "validation.reason(rust)")) {
        return 0;
    }
    if (!check_true(kain_win32_gl_surface_supports_graphics_bundle(&bundle) == 0, "gl_surface_supports_graphics_bundle(rust)")) {
        return 0;
    }
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 4;

    printf("Running graphics runtime smoke tests\n");

    if (test_graphics_bundle_from_json()) {
        printf("[PASS] graphics bundle from json\n");
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

    printf("Graphics runtime smoke summary: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
