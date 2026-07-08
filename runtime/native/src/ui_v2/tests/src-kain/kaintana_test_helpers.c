// kaintana_test_helpers.c — Expose null backend framebuffer to Kain test runner
//
// Real implementation: references non-static globals from host_null.c
// (kaintana_null_fb, kaintana_null_width, kaintana_null_height) which are
// defined in the null backend and made externally visible for test access.
//
// This file is the companion C translation unit for kaintana_test_helpers.h,
// discovered automatically by Kain's `include "kaintana_test_helpers.h" as th`.

#include <stdint.h>

// Forward declarations of the non-static globals from host_null.c
extern uint32_t* kaintana_null_fb;
extern int       kaintana_null_width;
extern int       kaintana_null_height;

uint32_t* kaintana_test_get_fb_ptr(void) {
    return kaintana_null_fb;
}

int kaintana_test_get_fb_width(void) {
    return kaintana_null_width;
}

int kaintana_test_get_fb_height(void) {
    return kaintana_null_height;
}
