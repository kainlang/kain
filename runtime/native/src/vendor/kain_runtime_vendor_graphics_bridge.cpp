#include "../../include/kain_runtime_vendor_graphics_bridge.h"
#include "../../include/kain_runtime_vendor_lane.h"

#include <cstdio>

#if KAIN_VENDOR_HAS_BGFX && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <bgfx/c99/bgfx.h>
#include <bgfx/defines.h>
#endif

extern "C" {

#if KAIN_VENDOR_HAS_BGFX && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
const char* kain_vendor_bgfx_version_string(void) {
    static char version_buffer[32];
    static int initialized = 0;

    if (!initialized) {
        std::snprintf(version_buffer, sizeof(version_buffer), "api-%u", BGFX_API_VERSION);
        initialized = 1;
    }

    return version_buffer;
}

int kain_vendor_bgfx_probe(void) {
    return bgfx_get_interface(BGFX_API_VERSION) != nullptr;
}
#else
const char* kain_vendor_bgfx_version_string(void) {
    return "bgfx-unavailable";
}

int kain_vendor_bgfx_probe(void) {
    return 0;
}
#endif

const char* kain_vendor_filament_version_string(void) {
    return "filament-core-staged";
}

int kain_vendor_filament_probe(void) {
    return 0;
}

const char* kain_vendor_diligent_version_string(void) {
    return "diligentcore-staged";
}

int kain_vendor_diligent_probe(void) {
    return 0;
}

const char* kain_vendor_bimg_version_string(void) {
    return "bimg-staged";
}

int kain_vendor_bimg_probe(void) {
    return 0;
}

} /* extern "C" */
