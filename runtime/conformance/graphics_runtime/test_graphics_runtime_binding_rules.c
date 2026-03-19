#include "../../native/include/kain_runtime_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kOverflowBindingsJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"magma_terraces\",\n"
    "        \"title\": \"Overflow Bindings\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\n"
    "          \"shader::terrain::fragment\",\n"
    "          \"shader::terrain::compute\"\n"
    "        ],\n"
    "        \"parameters\": [],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"scene.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"scene.normals\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 1},\n"
    "          {\"key\": \"scene.roughness\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 2},\n"
    "          {\"key\": \"scene.metallic\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 3},\n"
    "          {\"key\": \"scene.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 4},\n"
    "          {\"key\": \"scene.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 5},\n"
    "          {\"key\": \"scene.instances\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 6},\n"
    "          {\"key\": \"scene.tiles\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 7},\n"
    "          {\"key\": \"scene.overflowA\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 8},\n"
    "          {\"key\": \"scene.overflowB\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 9}\n"
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
    "        \"parameters\": [],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.normals\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 1},\n"
    "          {\"key\": \"terrain.roughness\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 2},\n"
    "          {\"key\": \"terrain.metallic\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 3},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 4},\n"
    "          {\"key\": \"terrain.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 5},\n"
    "          {\"key\": \"terrain.instances\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 6},\n"
    "          {\"key\": \"terrain.tiles\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 7},\n"
    "          {\"key\": \"terrain.overflowA\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 8},\n"
    "          {\"key\": \"terrain.overflowB\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 9}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
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
    "        {\"key\": \"terrain.dispatch\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 1},\n"
    "        {\"key\": \"terrain.extraA\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 2},\n"
    "        {\"key\": \"terrain.extraB\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 3},\n"
    "        {\"key\": \"terrain.extraC\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 4},\n"
    "        {\"key\": \"terrain.extraD\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 5},\n"
    "        {\"key\": \"terrain.extraE\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 6},\n"
    "        {\"key\": \"terrain.extraF\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 7},\n"
    "        {\"key\": \"terrain.overflowA\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 8},\n"
    "        {\"key\": \"terrain.overflowB\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"write\", \"slot\": 9}\n"
    "      ],\n"
    "      \"source\": \"kain-core\"\n"
    "    }\n"
    "  ],\n"
    "  \"assets\": [],\n"
    "  \"tool_caps\": [],\n"
    "  \"requirements\": []\n"
    "}\n";

static const char* kDuplicateSlotJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"magma_terraces\",\n"
    "        \"title\": \"Duplicate Slot\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\"shader::terrain::fragment\", \"shader::terrain::compute\"],\n"
    "        \"parameters\": [],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 0}\n"
    "        ]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": [\n"
    "      {\n"
    "        \"id\": \"terrain\",\n"
    "        \"source\": \"kain-core\",\n"
    "        \"shader_bundle_ref_keys\": [\"shader::terrain::fragment\", \"shader::terrain::compute\"],\n"
    "        \"parameters\": [],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"fragment\", \"access\": \"sample\", \"slot\": 0},\n"
    "          {\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read_write\", \"slot\": 0}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
    "    {\"key\": \"shader::terrain::fragment\", \"shader\": \"terrain\", \"module_name\": \"terrain\", \"stage\": \"fragment\", \"entry_point\": \"main\", \"source\": \"kain-core\"},\n"
    "    {\"key\": \"shader::terrain::compute\", \"shader\": \"terrain\", \"module_name\": \"terrain\", \"stage\": \"compute\", \"entry_point\": \"main\", \"workgroup_size\": [8, 8, 1], \"dispatch_size\": [16, 16, 1], \"resource_bindings\": [{\"key\": \"terrain.heightfield\", \"type\": \"storage_buffer\", \"stage\": \"compute\", \"access\": \"read\", \"slot\": 0}], \"source\": \"kain-core\"}\n"
    "  ],\n"
    "  \"assets\": [],\n"
    "  \"tool_caps\": [],\n"
    "  \"requirements\": []\n"
    "}\n";

static const char* kUnknownStageJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\n"
    "    \"scenes\": [\n"
    "      {\n"
    "        \"viewport_node\": \"surface.node.9\",\n"
    "        \"viewport_kind\": \"viewport3d\",\n"
    "        \"scene\": \"magma_terraces\",\n"
    "        \"title\": \"Unknown Stage\",\n"
    "        \"material_refs\": [\"terrain\"],\n"
    "        \"shader_bundle_ref_keys\": [\"shader::terrain::fragment\"],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"mesh\", \"access\": \"sample\", \"slot\": 0}\n"
    "        ]\n"
    "      }\n"
    "    ],\n"
    "    \"materials\": [\n"
    "      {\n"
    "        \"id\": \"terrain\",\n"
    "        \"source\": \"kain-core\",\n"
    "        \"shader_bundle_ref_keys\": [\"shader::terrain::fragment\"],\n"
    "        \"resource_bindings\": [\n"
    "          {\"key\": \"terrain.albedo\", \"type\": \"texture2d\", \"stage\": \"mesh\", \"access\": \"sample\", \"slot\": 0}\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"shader_bundle_refs\": [\n"
    "    {\"key\": \"shader::terrain::fragment\", \"shader\": \"terrain\", \"module_name\": \"terrain\", \"stage\": \"fragment\", \"entry_point\": \"main\", \"source\": \"kain-core\"}\n"
    "  ]\n"
    "}\n";

static const char* kEmptyRenderJson =
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"target\": \"llvm\",\n"
    "  \"render\": {\"scenes\": [], \"materials\": []},\n"
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

static int test_overflow_bindings_clamp(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;
    if (!check_true(kain_runtime_graphics_load_from_json(kOverflowBindingsJson, &bundle), "load_overflow_bindings")) return 0;
    if (!check_true(bundle.primary_material.loaded == 1, "bundle.primary_material.loaded")) return 0;
    if (!check_true(bundle.primary_compute.loaded == 1, "bundle.primary_compute.loaded")) return 0;
    if (!check_true(bundle.primary_material.resource_binding_count == KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS, "material bindings clamped")) return 0;
    if (!check_true(bundle.primary_compute.resource_binding_count == KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS, "compute bindings clamped")) return 0;
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation), "validate_overflow_bindings")) return 0;
    if (!check_true(validation.gl_lane_ready == 1, "validation.gl_lane_ready(overflow)")) return 0;
    return 1;
}

static int test_duplicate_slots_rejected(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;
    if (!check_true(kain_runtime_graphics_load_from_json(kDuplicateSlotJson, &bundle), "load_duplicate_slot")) return 0;
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation) == 0, "validate_duplicate_slot")) return 0;
    if (!check_true(validation.material_binding_valid == 0, "validation.material_binding_valid(duplicate slot)")) return 0;
    if (!check_contains(validation.reason, "material binding plan", "validation.reason(duplicate slot)")) return 0;
    return 1;
}

static int test_unknown_stage_rejected(void) {
    KainRuntimeGraphicsBundle bundle;
    KainRuntimeGraphicsValidation validation;
    if (!check_true(kain_runtime_graphics_load_from_json(kUnknownStageJson, &bundle), "load_unknown_stage")) return 0;
    if (!check_true(kain_runtime_graphics_validate_bundle(&bundle, &validation) == 0, "validate_unknown_stage")) return 0;
    if (!check_true(validation.material_binding_valid == 0, "validation.material_binding_valid(unknown stage)")) return 0;
    return 1;
}

static int test_reload_resets_state(void) {
    KainRuntimeGraphicsBundle bundle;
    if (!check_true(kain_runtime_graphics_load_from_json(kOverflowBindingsJson, &bundle), "load_first")) return 0;
    if (!check_true(bundle.primary_compute.loaded == 1, "bundle.primary_compute.loaded(first)")) return 0;
    if (!check_true(bundle.primary_material.resource_binding_count == KAIN_RUNTIME_GRAPHICS_MAX_BINDINGS, "bundle.primary_material.resource_binding_count(first)")) return 0;

    if (!check_true(kain_runtime_graphics_load_from_json(kEmptyRenderJson, &bundle), "load_second")) return 0;
    if (!check_true(bundle.loaded == 1, "bundle.loaded(second)")) return 0;
    if (!check_true(bundle.scene_count == 0, "bundle.scene_count(second)")) return 0;
    if (!check_true(bundle.material_count == 0, "bundle.material_count(second)")) return 0;
    if (!check_true(bundle.primary_material.loaded == 0, "bundle.primary_material.loaded(second)")) return 0;
    if (!check_true(bundle.primary_material.resource_binding_count == 0, "bundle.primary_material.resource_binding_count(second)")) return 0;
    if (!check_true(bundle.primary_compute.loaded == 0, "bundle.primary_compute.loaded(second)")) return 0;
    if (!check_true(bundle.primary_compute.resource_binding_count == 0, "bundle.primary_compute.resource_binding_count(second)")) return 0;
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 4;

    printf("Running graphics binding rules tests\n");

    if (test_overflow_bindings_clamp()) {
        printf("[PASS] overflow bindings clamp\n");
        ++passed;
    }
    if (test_duplicate_slots_rejected()) {
        printf("[PASS] duplicate slots rejected\n");
        ++passed;
    }
    if (test_unknown_stage_rejected()) {
        printf("[PASS] unknown stage rejected\n");
        ++passed;
    }
    if (test_reload_resets_state()) {
        printf("[PASS] reload resets state\n");
        ++passed;
    }

    printf("Graphics binding rules summary: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}

