#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dcc_suite_apply_sculpt_stamp(uint8_t* pixels, size_t len, int accent);
const char* dcc_suite_signature(int width, int height, int accent);

#ifdef __cplusplus
}
#endif
