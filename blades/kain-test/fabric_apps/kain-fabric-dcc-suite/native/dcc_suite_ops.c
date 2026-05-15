#include "dcc_suite_ops.h"

#include <stdio.h>

static char G_DCC_SUITE_SIGNATURE[160];
static char G_DCC_SUITE_REPORT[192];
static char G_DCC_SUITE_MESH_SIGNATURE[256];
static char G_DCC_SUITE_TOPOLOGY_SIGNATURE[256];
static char G_DCC_SUITE_RUNTIME_SIGNATURE[256];

const char* dcc_suite_sculpt_signature(int grid_resolution, int checksum, int accent) {
    snprintf(
        G_DCC_SUITE_SIGNATURE,
        sizeof(G_DCC_SUITE_SIGNATURE),
        "dcc-suite:gpu-sculpt:grid=%d:checksum=%d:accent=%d",
        grid_resolution,
        checksum,
        accent
    );
    return G_DCC_SUITE_SIGNATURE;
}

const char* dcc_suite_sculpt_report(int grid_resolution, int active_samples, int checksum, int accent) {
    snprintf(
        G_DCC_SUITE_REPORT,
        sizeof(G_DCC_SUITE_REPORT),
        "sculpt-report:grid=%d:active=%d:checksum=%d:accent=%d",
        grid_resolution,
        active_samples,
        checksum,
        accent
    );
    return G_DCC_SUITE_REPORT;
}

const char* dcc_suite_mesh_edit_target_signature(const char* mesh_resource_uri, const char* edit_target_uri, const char* topology_uri) {
    snprintf(
        G_DCC_SUITE_MESH_SIGNATURE,
        sizeof(G_DCC_SUITE_MESH_SIGNATURE),
        "mesh-contract:mesh=%s:edit-target=%s:topology=%s",
        mesh_resource_uri,
        edit_target_uri,
        topology_uri
    );
    return G_DCC_SUITE_MESH_SIGNATURE;
}

const char* dcc_suite_mesh_topology_signature(const char* topology_policy, int target_subdivision_level, int uv_islands, int edit_target_checksum) {
    snprintf(
        G_DCC_SUITE_TOPOLOGY_SIGNATURE,
        sizeof(G_DCC_SUITE_TOPOLOGY_SIGNATURE),
        "mesh-topology:policy=%s:subdivision=%d:uv-islands=%d:edit-checksum=%d",
        topology_policy,
        target_subdivision_level,
        uv_islands,
        edit_target_checksum
    );
    return G_DCC_SUITE_TOPOLOGY_SIGNATURE;
}

const char* dcc_suite_mesh_runtime_signature(const char* subdivision_policy, int target_subdivision_level, int uv_islands, int edit_target_checksum) {
    snprintf(
        G_DCC_SUITE_RUNTIME_SIGNATURE,
        sizeof(G_DCC_SUITE_RUNTIME_SIGNATURE),
        "mesh-runtime:subdivision=%s:%d:uv-islands=%d:edit-checksum=%d",
        subdivision_policy,
        target_subdivision_level,
        uv_islands,
        edit_target_checksum
    );
    return G_DCC_SUITE_RUNTIME_SIGNATURE;
}
