// Kauri runtime stubs — replaces missing UI symbols for headless builds.
// The precompiled kain_runtime.lib includes ui_host_adapter.obj which
// references ui_render_frame and ui_layout_resolve. These stubs satisfy
// the linker without pulling in the full UI runtime.

#include <stdint.h>

int64_t ui_render_frame(int64_t surface_id, int64_t frame_token) {
    (void)surface_id;
    (void)frame_token;
    return 0;  // no-op: no UI surface to render
}

int64_t ui_layout_resolve(int64_t root_node_id, int64_t width, int64_t height) {
    (void)root_node_id;
    (void)width;
    (void)height;
    return 0;  // no-op: no layout tree to resolve
}
