#ifndef KAIN_COMPONENT_SURFACE_H
#define KAIN_COMPONENT_SURFACE_H

// ============================================================================
//  KainComponentSurface — Surface-agnostic component rendering trait.
// ============================================================================
//  This is the ABI contract between the Kain compiler and any surface backend
//  (native_ui, web, viewport3d, headless, tui, ...). The compiler emits calls
//  through this vtable; the backend implements them. Neither side knows the
//  other's internals.
//
//  Registration: call kain_component_surface_register("name", &surface)
//  Resolution:   call kain_component_surface_resolve("name") -> KainComponentSurface*
//
//  The compiler resolves the surface once at frame-loop init, then calls
//  through the vtable every frame. The trait is surface-agnostic — "kind"
//  strings and style keys are interpreted by the backend.
// ============================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KainComponentSurface {
    // ── Session lifecycle ──────────────────────────────────────
    int64_t (*session_create) (const char* name, int64_t width, int64_t height);
    void    (*session_destroy)(int64_t session_id);

    // ── Element tree (abstract — "kind" is surface-interpreted) ─
    int64_t (*element_begin)  (int64_t session_id, int64_t parent_id,
                               const char* kind, const char* stable_key);
    void    (*element_end)    (int64_t session_id, int64_t element_id);
    void    (*element_set_text)(int64_t session_id, int64_t element_id,
                                const char* text);

    // ── Style/attribute setters ────────────────────────────────
    void    (*element_set_attr_i64)   (int64_t session_id, int64_t element_id,
                                       const char* key, int64_t value);
    void    (*element_set_attr_f64)   (int64_t session_id, int64_t element_id,
                                       const char* key, double value);
    void    (*element_set_attr_string)(int64_t session_id, int64_t element_id,
                                       const char* key, const char* value);

    // ── State persistence (component `state` survives frames) ───
    int64_t (*state_get_i64)(int64_t session_id, const char* key);
    void    (*state_set_i64)(int64_t session_id, const char* key, int64_t value);

    // ── Frame lifecycle ────────────────────────────────────────
    void    (*begin_frame)(int64_t session_id, double delta_ms);
    void    (*end_frame)  (int64_t session_id);
    void    (*present)    (int64_t session_id);

    // ── Event pump (opaque — surface decodes its own event type) ─
    int64_t (*poll_event)  (int64_t session_id, void* out_event, int64_t max_size);
    int64_t (*should_close)(int64_t session_id);
} KainComponentSurface;

// ── Surface registry ──────────────────────────────────────────
// Called at startup to register surface backends.
// name is a borrowed pointer — caller must keep it alive.
// surface must be non-NULL and all function pointers must be non-NULL.
void kain_component_surface_register(const char* name,
                                     const KainComponentSurface* surface);

// Called by codegen at frame-loop init to resolve the surface for a world.
// Returns NULL if the named surface is not registered.
const KainComponentSurface* kain_component_surface_resolve(const char* name);

#ifdef __cplusplus
}
#endif

#endif // KAIN_COMPONENT_SURFACE_H
