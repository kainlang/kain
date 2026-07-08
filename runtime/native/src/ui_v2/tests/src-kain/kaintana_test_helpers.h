// ============================================================================
//  kaintana_test_helpers.h — Expose framebuffer pointer to Kain
//
//  These three functions expose the null backend's static globals so that
//  Kain can read pixels back for golden-file comparison.
//
//  Usage (from Kain):
//    include "../../kaintana_test_helpers.h" as test
//    let fb_ptr  = test.kaintana_test_get_fb_ptr()
//    let fb_w    = test.kaintana_test_get_fb_width()
//    let fb_h    = test.kaintana_test_get_fb_height()
// ============================================================================

#ifndef KAINTANA_TEST_HELPERS_H
#define KAINTANA_TEST_HELPERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Return pointer to the null backend's framebuffer (row-major uint32_t ARGB).
uint32_t* kaintana_test_get_fb_ptr(void);

/// Return the framebuffer width in pixels.
int kaintana_test_get_fb_width(void);

/// Return the framebuffer height in pixels.
int kaintana_test_get_fb_height(void);

#ifdef __cplusplus
}
#endif

#endif // KAINTANA_TEST_HELPERS_H
