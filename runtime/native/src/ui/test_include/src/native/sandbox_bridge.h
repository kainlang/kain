#ifndef SANDBOX_BRIDGE_H
#define SANDBOX_BRIDGE_H

// ============================================================================
//  sandbox_bridge.h — Kain natural-include bridge for ui3d_sandbox.c
// ============================================================================
//  Wraps the 3D UI sandbox demo (ui3d_sandbox.c) into a Kain-callable C API.
//  The Kain file uses:
//    include native/sandbox_bridge.h as sand
//  and then calls sand_init, sand_frame, sand_destroy.
// ============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Lifecycle ─────────────────────────────────────────────────────────
void* sandbox_bridge_init(int width, int height);
void  sandbox_bridge_destroy(void* demo);

// ── Frame ────────────────────────────────────────────────────────────
int sandbox_bridge_frame(void* demo);
int sandbox_bridge_running(void* demo);

#ifdef __cplusplus
}
#endif

#endif /* SANDBOX_BRIDGE_H */
