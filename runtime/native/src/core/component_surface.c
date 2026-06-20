// ============================================================================
//  component_surface.c — Surface registry for KainComponentSurface backends.
// ============================================================================
//  Maps human-readable surface names ("native_ui", "web", ...) to
//  KainComponentSurface vtable pointers. The compiler resolves a surface
//  once at frame-loop init via kain_component_surface_resolve(), then calls
//  through the vtable every frame.
//
//  Registration happens at startup — typically from a blade's init function
//  or from the platform app host before the main frame loop begins.
// ============================================================================

#include "component_surface.h"
#include <string.h>

// ── Constants ──────────────────────────────────────────────────

#define KAIN_MAX_SURFACES 16

// ── Registry state ─────────────────────────────────────────────

static struct {
    const char*                name;    // borrowed pointer — caller owns lifetime
    const KainComponentSurface* surface; // borrowed pointer — static or heap, caller owns
} g_surface_registry[KAIN_MAX_SURFACES];

static int g_surface_count = 0;

// ── Public API ─────────────────────────────────────────────────

void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface) {
    // Safety: reject NULL pointers. The codegen trusts this.
    if (!name || !surface) {
        return;
    }

    // Prevent duplicate registration — silently overwrite if already present.
    for (int i = 0; i < g_surface_count; i++) {
        if (strcmp(g_surface_registry[i].name, name) == 0) {
            g_surface_registry[i].surface = surface;
            return;
        }
    }

    // Append new entry if there is room.
    if (g_surface_count >= KAIN_MAX_SURFACES) {
        return; // registry full — surface not available
    }

    g_surface_registry[g_surface_count].name    = name;
    g_surface_registry[g_surface_count].surface = surface;
    g_surface_count++;
}

const KainComponentSurface* kain_component_surface_resolve(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < g_surface_count; i++) {
        if (strcmp(g_surface_registry[i].name, name) == 0) {
            return g_surface_registry[i].surface;
        }
    }

    return NULL; // Codegen checks for NULL, emits runtime error.
}
