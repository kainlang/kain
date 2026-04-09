#include "../../../include/kain_runtime_graphics.h"

#if defined(__linux__)
int kain_win32_gl_surface_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle) {
    KainRuntimeGraphicsValidation validation;

    if (!bundle) {
        return 0;
    }
    if (!kain_runtime_graphics_validate_bundle(bundle, &validation)) {
        return 0;
    }
    return validation.gl_lane_ready;
}
#endif
