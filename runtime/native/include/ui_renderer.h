#ifndef ABI_UI_RENDERER_H
#define ABI_UI_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct KainNativeUiSession;

// ── Universal node-tree renderer ──────────────────────────────────────
//
// Walks the retained-mode node tree (session->nodes[], session->styles[])
// and fills a caller-provided pixel buffer. Platform-agnostic: the caller
// is responsible for allocating the framebuffer and presenting it.
//
// Parameters:
//   session     — UI session with node tree, styles, draw commands
//   framebuffer — uint32_t* pixel buffer (0xAARRGGBB format)
//   fb_width    — framebuffer width in pixels
//   fb_height   — framebuffer height in pixels
//   fb_stride   — framebuffer stride in uint32_t elements (typically fb_width)
//
// Returns the number of rendered pixels (fb_width * fb_height), or 0 on error.

int64_t ui_render_frame(
    struct KainNativeUiSession* session,
    uint32_t* framebuffer,
    int fb_width,
    int fb_height,
    int fb_stride
);

#ifdef __cplusplus
}
#endif

#endif /* ABI_UI_RENDERER_H */
