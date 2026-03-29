#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define DCC_SUITE_EXPORT __declspec(dllexport)
#else
#define DCC_SUITE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

DCC_SUITE_EXPORT const char* dcc_suite_sculpt_signature(int grid_resolution, int checksum, int accent);
DCC_SUITE_EXPORT const char* dcc_suite_sculpt_report(int grid_resolution, int active_samples, int checksum, int accent);
DCC_SUITE_EXPORT const char* dcc_suite_mesh_edit_target_signature(const char* mesh_resource_uri, const char* edit_target_uri, const char* topology_uri);
DCC_SUITE_EXPORT const char* dcc_suite_mesh_topology_signature(const char* topology_policy, int target_subdivision_level, int uv_islands, int edit_target_checksum);
DCC_SUITE_EXPORT const char* dcc_suite_mesh_runtime_signature(const char* subdivision_policy, int target_subdivision_level, int uv_islands, int edit_target_checksum);

#ifdef __cplusplus
}
#endif
