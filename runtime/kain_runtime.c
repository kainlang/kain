// Compatibility umbrella for legacy single-file builds.
// The production runtime now lives in modular sources under runtime/native/src.

#include "native/src/core/kain_runtime_core.c"
#include "native/src/core/kain_runtime_contract.c"
#include "native/src/core/kain_runtime_realtime.c"
#include "native/src/core/kain_runtime_memory.c"
#include "native/src/core/kain_runtime_bitfield.c"
#include "native/src/core/kain_runtime_union.c"
#include "native/src/asset/kain_asset_gltf.c"
#include "native/src/gfx/opengl/kain_gl_win32_host.c"
#include "native/src/platform/win32/kain_win32_app_host.c"
#include "native/src/platform/win32/kain_win32_input_host.c"
#include "native/src/platform/win32/kain_runtime_win32_shared.c"
#include "native/src/ui/kain_ui_compiled_bundle.c"
#include "native/src/ui/kain_ui_compiled_overlay.c"
#include "native/src/ui/kain_ui_overlay.c"
#include "native/src/platform/win32/kain_runtime_viewport_win32.c"
#include "native/src/platform/win32/kain_runtime_sculpt_win32.c"
