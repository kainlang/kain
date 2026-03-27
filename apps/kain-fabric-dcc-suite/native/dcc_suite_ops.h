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

#ifdef __cplusplus
}
#endif
