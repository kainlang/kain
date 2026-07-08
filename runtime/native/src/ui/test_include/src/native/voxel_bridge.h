#ifndef VOXEL_BRIDGE_H
#define VOXEL_BRIDGE_H

// ============================================================================
//  voxel_bridge.h — Kain natural-include bridge for voxel_viewer.c
// ============================================================================
//  Wraps the isometric voxel landscape demo (voxel_viewer.c) into a
//  Kain-callable C API. The Kain file uses:
//    include native/voxel_bridge.h as vox
//  and then calls vox_init, vox_frame, vox_shutdown.
// ============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Lifecycle ─────────────────────────────────────────────────────────
void* voxel_bridge_init(int width, int height);
void  voxel_bridge_destroy(void* demo);

// ── Frame ────────────────────────────────────────────────────────────
int voxel_bridge_frame(void* demo);
int voxel_bridge_running(void* demo);

#ifdef __cplusplus
}
#endif

#endif /* VOXEL_BRIDGE_H */
