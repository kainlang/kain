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

DCC_SUITE_EXPORT void dcc_suite_apply_sculpt_stamp(uint8_t* pixels, size_t len, int accent);
DCC_SUITE_EXPORT const char* dcc_suite_signature(int width, int height, int accent);

#ifdef __cplusplus
}
#endif
