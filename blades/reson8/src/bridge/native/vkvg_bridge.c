// vkvg_bridge.c — vkvg C bridge implementation for reson8
//
// Thin C wrapper around vkvg that:
// 1. Takes existing Vulkan handles from the Kain side
// 2. Creates/manages a vkvg device on that Vulkan context
// 3. Exposes all drawing operations as flat C functions
// 4. Casts opaque vkvg handles to/from uint64_t for Kain FFI

#include "vkvg_bridge.h"
#include <vkvg.h>

#include <stdlib.h>
#include <string.h>

// ── Global vkvg device (singleton per bridge instance) ──
static VkvgDevice g_vkvg_device = NULL;

// ── Helper: cast uint64_t ↔ pointer ──
#define H2P(h) ((void*)(uintptr_t)(h))
#define P2H(p) ((uint64_t)(uintptr_t)(p))

// ============================================================================
//  LIFECYCLE
// ============================================================================

int32_t vkvg_bridge_init(
    uint64_t vk_instance,
    uint64_t vk_physical_device,
    uint64_t vk_device,
    uint32_t queue_family_index,
    uint32_t queue_index,
    uint32_t multisample
) {
    // Clean up any previous device
    if (g_vkvg_device) {
        vkvg_device_destroy(g_vkvg_device);
        g_vkvg_device = NULL;
    }

    vkvg_device_create_info_t info;
    memset(&info, 0, sizeof(info));

    info.inst            = (VkInstance)H2P(vk_instance);
    info.phy             = (VkPhysicalDevice)H2P(vk_physical_device);
    info.vkdev           = (VkDevice)H2P(vk_device);
    info.qFamIdx         = queue_family_index;
    info.qIndex          = queue_index;
    info.samples         = (VkSampleCountFlags)multisample;
    info.deferredResolve = false;
    info.threadAware     = false;

    g_vkvg_device = vkvg_device_create(&info);
    if (!g_vkvg_device) {
        return VKVG_BRIDGE_ERR_VKVG;
    }

    // Check device status
    if (vkvg_device_status(g_vkvg_device) != VKVG_STATUS_SUCCESS) {
        vkvg_device_destroy(g_vkvg_device);
        g_vkvg_device = NULL;
        return VKVG_BRIDGE_ERR_VKVG;
    }

    return VKVG_BRIDGE_OK;
}

void vkvg_bridge_shutdown(void) {
    if (g_vkvg_device) {
        vkvg_device_destroy(g_vkvg_device);
        g_vkvg_device = NULL;
    }
}

int32_t vkvg_bridge_is_init(void) {
    return (g_vkvg_device != NULL) ? 1 : 0;
}

// ── Device queries ──
void vkvg_bridge_set_dpy(int32_t hdpy, int32_t vdpy) {
    if (g_vkvg_device) {
        vkvg_device_set_dpy(g_vkvg_device, hdpy, vdpy);
    }
}

int32_t vkvg_bridge_get_hdpy(void) {
    if (!g_vkvg_device) return 96;
    int h = 96, v = 96;
    vkvg_device_get_dpy(g_vkvg_device, &h, &v);
    return h;
}

int32_t vkvg_bridge_get_vdpy(void) {
    if (!g_vkvg_device) return 96;
    int h = 96, v = 96;
    vkvg_device_get_dpy(g_vkvg_device, &h, &v);
    return v;
}

// ============================================================================
//  SURFACE
// ============================================================================

uint64_t vkvg_bridge_surface_create(uint32_t width, uint32_t height) {
    if (!g_vkvg_device) return 0;
    VkvgSurface surf = vkvg_surface_create(g_vkvg_device, width, height);
    return P2H(surf);
}

void vkvg_bridge_surface_destroy(uint64_t surface) {
    if (surface) {
        vkvg_surface_destroy((VkvgSurface)H2P(surface));
    }
}

uint32_t vkvg_bridge_surface_get_width(uint64_t surface) {
    if (!surface) return 0;
    return vkvg_surface_get_width((VkvgSurface)H2P(surface));
}

uint32_t vkvg_bridge_surface_get_height(uint64_t surface) {
    if (!surface) return 0;
    return vkvg_surface_get_height((VkvgSurface)H2P(surface));
}

uint64_t vkvg_bridge_surface_get_vk_image(uint64_t surface) {
    if (!surface) return 0;
    return P2H(vkvg_surface_get_vk_image((VkvgSurface)H2P(surface)));
}

void vkvg_bridge_surface_clear(uint64_t surface) {
    if (surface) {
        vkvg_surface_clear((VkvgSurface)H2P(surface));
    }
}

int32_t vkvg_bridge_surface_write_to_png(uint64_t surface, const char* path) {
    if (!surface) return VKVG_BRIDGE_ERR_NULL_HANDLE;
    vkvg_status_t status = vkvg_surface_write_to_png((VkvgSurface)H2P(surface), path);
    return (status == VKVG_STATUS_SUCCESS) ? VKVG_BRIDGE_OK : VKVG_BRIDGE_ERR_VKVG;
}

// ============================================================================
//  CONTEXT
// ============================================================================

uint64_t vkvg_bridge_context_create(uint64_t surface) {
    if (!surface) return 0;
    VkvgContext ctx = vkvg_create((VkvgSurface)H2P(surface));
    return P2H(ctx);
}

void vkvg_bridge_context_destroy(uint64_t ctx) {
    if (ctx) {
        vkvg_destroy((VkvgContext)H2P(ctx));
    }
}

int32_t vkvg_bridge_context_status(uint64_t ctx) {
    if (!ctx) return VKVG_BRIDGE_ERR_NULL_HANDLE;
    vkvg_status_t s = vkvg_status((VkvgContext)H2P(ctx));
    return (int32_t)s;
}

void vkvg_bridge_flush(uint64_t ctx) {
    if (ctx) {
        vkvg_flush((VkvgContext)H2P(ctx));
    }
}

// ============================================================================
//  SOURCE COLOR
// ============================================================================

void vkvg_bridge_set_source_rgba(uint64_t ctx, float r, float g, float b, float a) {
    if (ctx) vkvg_set_source_rgba((VkvgContext)H2P(ctx), r, g, b, a);
}

void vkvg_bridge_set_source_rgb(uint64_t ctx, float r, float g, float b) {
    if (ctx) vkvg_set_source_rgb((VkvgContext)H2P(ctx), r, g, b);
}

void vkvg_bridge_set_source_color(uint64_t ctx, uint32_t rgba) {
    if (ctx) vkvg_set_source_color((VkvgContext)H2P(ctx), rgba);
}

void vkvg_bridge_set_source_surface(uint64_t ctx, uint64_t surf, float x, float y) {
    if (ctx && surf) {
        vkvg_set_source_surface((VkvgContext)H2P(ctx), (VkvgSurface)H2P(surf), x, y);
    }
}

// ============================================================================
//  STROKE CONFIGURATION
// ============================================================================

void vkvg_bridge_set_line_width(uint64_t ctx, float width) {
    if (ctx) vkvg_set_line_width((VkvgContext)H2P(ctx), width);
}

float vkvg_bridge_get_line_width(uint64_t ctx) {
    if (!ctx) return 2.0f;
    return vkvg_get_line_width((VkvgContext)H2P(ctx));
}

void vkvg_bridge_set_line_cap(uint64_t ctx, int32_t cap) {
    if (ctx) vkvg_set_line_cap((VkvgContext)H2P(ctx), (vkvg_line_cap_t)cap);
}

int32_t vkvg_bridge_get_line_cap(uint64_t ctx) {
    if (!ctx) return VKVG_CAP_BUTT;
    return (int32_t)vkvg_get_line_cap((VkvgContext)H2P(ctx));
}

void vkvg_bridge_set_line_join(uint64_t ctx, int32_t join) {
    if (ctx) vkvg_set_line_join((VkvgContext)H2P(ctx), (vkvg_line_join_t)join);
}

int32_t vkvg_bridge_get_line_join(uint64_t ctx) {
    if (!ctx) return VKVG_JOIN_MITER;
    return (int32_t)vkvg_get_line_join((VkvgContext)H2P(ctx));
}

void vkvg_bridge_set_miter_limit(uint64_t ctx, float limit) {
    if (ctx) vkvg_set_miter_limit((VkvgContext)H2P(ctx), limit);
}

void vkvg_bridge_set_opacity(uint64_t ctx, float opacity) {
    if (ctx) vkvg_set_opacity((VkvgContext)H2P(ctx), opacity);
}

float vkvg_bridge_get_opacity(uint64_t ctx) {
    if (!ctx) return 1.0f;
    return vkvg_get_opacity((VkvgContext)H2P(ctx));
}

void vkvg_bridge_set_fill_rule(uint64_t ctx, int32_t rule) {
    if (ctx) vkvg_set_fill_rule((VkvgContext)H2P(ctx), (vkvg_fill_rule_t)rule);
}

int32_t vkvg_bridge_get_fill_rule(uint64_t ctx) {
    if (!ctx) return VKVG_FILL_NON_ZERO;
    return (int32_t)vkvg_get_fill_rule((VkvgContext)H2P(ctx));
}

void vkvg_bridge_set_operator(uint64_t ctx, int32_t op) {
    if (ctx) vkvg_set_operator((VkvgContext)H2P(ctx), (vkvg_operator_t)op);
}

int32_t vkvg_bridge_get_operator(uint64_t ctx) {
    if (!ctx) return VKVG_OP_OVER;
    return (int32_t)vkvg_get_operator((VkvgContext)H2P(ctx));
}

// ── Dashes ──
void vkvg_bridge_set_dash(uint64_t ctx, const float* dashes, uint32_t count, float offset) {
    if (ctx) vkvg_set_dash((VkvgContext)H2P(ctx), dashes, count, offset);
}

uint32_t vkvg_bridge_get_dash_count(uint64_t ctx) {
    if (!ctx) return 0;
    uint32_t count = 0;
    float offset = 0;
    vkvg_get_dash((VkvgContext)H2P(ctx), NULL, &count, &offset);
    return count;
}

float vkvg_bridge_get_dash_offset(uint64_t ctx) {
    if (!ctx) return 0.0f;
    uint32_t count = 0;
    float offset = 0;
    vkvg_get_dash((VkvgContext)H2P(ctx), NULL, &count, &offset);
    return offset;
}

// ============================================================================
//  PATH CONSTRUCTION
// ============================================================================

void vkvg_bridge_new_path(uint64_t ctx) {
    if (ctx) vkvg_new_path((VkvgContext)H2P(ctx));
}

void vkvg_bridge_new_sub_path(uint64_t ctx) {
    if (ctx) vkvg_new_sub_path((VkvgContext)H2P(ctx));
}

void vkvg_bridge_close_path(uint64_t ctx) {
    if (ctx) vkvg_close_path((VkvgContext)H2P(ctx));
}

void vkvg_bridge_move_to(uint64_t ctx, float x, float y) {
    if (ctx) vkvg_move_to((VkvgContext)H2P(ctx), x, y);
}

void vkvg_bridge_rel_move_to(uint64_t ctx, float dx, float dy) {
    if (ctx) vkvg_rel_move_to((VkvgContext)H2P(ctx), dx, dy);
}

void vkvg_bridge_line_to(uint64_t ctx, float x, float y) {
    if (ctx) vkvg_line_to((VkvgContext)H2P(ctx), x, y);
}

void vkvg_bridge_rel_line_to(uint64_t ctx, float dx, float dy) {
    if (ctx) vkvg_rel_line_to((VkvgContext)H2P(ctx), dx, dy);
}

void vkvg_bridge_curve_to(uint64_t ctx, float x1, float y1, float x2, float y2, float x3, float y3) {
    if (ctx) vkvg_curve_to((VkvgContext)H2P(ctx), x1, y1, x2, y2, x3, y3);
}

void vkvg_bridge_rel_curve_to(uint64_t ctx, float x1, float y1, float x2, float y2, float x3, float y3) {
    if (ctx) vkvg_rel_curve_to((VkvgContext)H2P(ctx), x1, y1, x2, y2, x3, y3);
}

void vkvg_bridge_quadratic_to(uint64_t ctx, float x1, float y1, float x2, float y2) {
    if (ctx) vkvg_quadratic_to((VkvgContext)H2P(ctx), x1, y1, x2, y2);
}

void vkvg_bridge_rel_quadratic_to(uint64_t ctx, float x1, float y1, float x2, float y2) {
    if (ctx) vkvg_rel_quadratic_to((VkvgContext)H2P(ctx), x1, y1, x2, y2);
}

void vkvg_bridge_arc(uint64_t ctx, float xc, float yc, float radius, float a1, float a2) {
    if (ctx) vkvg_arc((VkvgContext)H2P(ctx), xc, yc, radius, a1, a2);
}

void vkvg_bridge_arc_negative(uint64_t ctx, float xc, float yc, float radius, float a1, float a2) {
    if (ctx) vkvg_arc_negative((VkvgContext)H2P(ctx), xc, yc, radius, a1, a2);
}

// ── Shapes ──
int32_t vkvg_bridge_rectangle(uint64_t ctx, float x, float y, float w, float h) {
    if (!ctx) return VKVG_BRIDGE_ERR_NULL_HANDLE;
    vkvg_status_t s = vkvg_rectangle((VkvgContext)H2P(ctx), x, y, w, h);
    return (s == VKVG_STATUS_SUCCESS) ? VKVG_BRIDGE_OK : (int32_t)s;
}

int32_t vkvg_bridge_rounded_rectangle(uint64_t ctx, float x, float y, float w, float h, float radius) {
    if (!ctx) return VKVG_BRIDGE_ERR_NULL_HANDLE;
    vkvg_status_t s = vkvg_rounded_rectangle((VkvgContext)H2P(ctx), x, y, w, h, radius);
    return (s == VKVG_STATUS_SUCCESS) ? VKVG_BRIDGE_OK : (int32_t)s;
}

void vkvg_bridge_rounded_rectangle2(uint64_t ctx, float x, float y, float w, float h, float rx, float ry) {
    if (ctx) vkvg_rounded_rectangle2((VkvgContext)H2P(ctx), x, y, w, h, rx, ry);
}

void vkvg_bridge_ellipse(uint64_t ctx, float rx, float ry, float x, float y, float rotation) {
    if (ctx) vkvg_ellipse((VkvgContext)H2P(ctx), rx, ry, x, y, rotation);
}

int32_t vkvg_bridge_has_current_point(uint64_t ctx) {
    if (!ctx) return 0;
    return vkvg_has_current_point((VkvgContext)H2P(ctx)) ? 1 : 0;
}

// ============================================================================
//  STROKE / FILL / PAINT
// ============================================================================

void vkvg_bridge_stroke(uint64_t ctx) {
    if (ctx) vkvg_stroke((VkvgContext)H2P(ctx));
}

void vkvg_bridge_stroke_preserve(uint64_t ctx) {
    if (ctx) vkvg_stroke_preserve((VkvgContext)H2P(ctx));
}

void vkvg_bridge_fill(uint64_t ctx) {
    if (ctx) vkvg_fill((VkvgContext)H2P(ctx));
}

void vkvg_bridge_fill_preserve(uint64_t ctx) {
    if (ctx) vkvg_fill_preserve((VkvgContext)H2P(ctx));
}

void vkvg_bridge_paint(uint64_t ctx) {
    if (ctx) vkvg_paint((VkvgContext)H2P(ctx));
}

void vkvg_bridge_clear(uint64_t ctx) {
    if (ctx) vkvg_clear((VkvgContext)H2P(ctx));
}

// ============================================================================
//  CLIPPING
// ============================================================================

void vkvg_bridge_clip(uint64_t ctx) {
    if (ctx) vkvg_clip((VkvgContext)H2P(ctx));
}

void vkvg_bridge_clip_preserve(uint64_t ctx) {
    if (ctx) vkvg_clip_preserve((VkvgContext)H2P(ctx));
}

void vkvg_bridge_reset_clip(uint64_t ctx) {
    if (ctx) vkvg_reset_clip((VkvgContext)H2P(ctx));
}

// ============================================================================
//  TRANSFORM
// ============================================================================

void vkvg_bridge_save(uint64_t ctx) {
    if (ctx) vkvg_save((VkvgContext)H2P(ctx));
}

void vkvg_bridge_restore(uint64_t ctx) {
    if (ctx) vkvg_restore((VkvgContext)H2P(ctx));
}

void vkvg_bridge_translate(uint64_t ctx, float dx, float dy) {
    if (ctx) vkvg_translate((VkvgContext)H2P(ctx), dx, dy);
}

void vkvg_bridge_scale(uint64_t ctx, float sx, float sy) {
    if (ctx) vkvg_scale((VkvgContext)H2P(ctx), sx, sy);
}

void vkvg_bridge_rotate(uint64_t ctx, float radians) {
    if (ctx) vkvg_rotate((VkvgContext)H2P(ctx), radians);
}

void vkvg_bridge_identity_matrix(uint64_t ctx) {
    if (ctx) vkvg_identity_matrix((VkvgContext)H2P(ctx));
}

// ============================================================================
//  TEXT
// ============================================================================

void vkvg_bridge_select_font_face(uint64_t ctx, const char* name) {
    if (ctx) vkvg_select_font_face((VkvgContext)H2P(ctx), name);
}

void vkvg_bridge_load_font_from_path(uint64_t ctx, const char* path, const char* name) {
    if (ctx) vkvg_load_font_from_path((VkvgContext)H2P(ctx), path, name);
}

void vkvg_bridge_set_font_size(uint64_t ctx, uint32_t size) {
    if (ctx) vkvg_set_font_size((VkvgContext)H2P(ctx), size);
}

void vkvg_bridge_show_text(uint64_t ctx, const char* utf8) {
    if (ctx) vkvg_show_text((VkvgContext)H2P(ctx), utf8);
}

float vkvg_bridge_text_extents_width(uint64_t ctx, const char* utf8) {
    if (!ctx || !utf8) return 0.0f;
    vkvg_text_extents_t ext;
    vkvg_text_extents((VkvgContext)H2P(ctx), utf8, &ext);
    return ext.width;
}

float vkvg_bridge_text_extents_height(uint64_t ctx, const char* utf8) {
    if (!ctx || !utf8) return 0.0f;
    vkvg_text_extents_t ext;
    vkvg_text_extents((VkvgContext)H2P(ctx), utf8, &ext);
    return ext.height;
}

// ============================================================================
//  GRADIENTS
// ============================================================================

uint64_t vkvg_bridge_gradient_create_linear(float x0, float y0, float x1, float y1) {
    VkvgPattern pat = vkvg_pattern_create_linear(x0, y0, x1, y1);
    return P2H(pat);
}

uint64_t vkvg_bridge_gradient_create_radial(float cx0, float cy0, float r0, float cx1, float cy1, float r1) {
    VkvgPattern pat = vkvg_pattern_create_radial(cx0, cy0, r0, cx1, cy1, r1);
    return P2H(pat);
}

int32_t vkvg_bridge_gradient_add_stop(uint64_t pat, float offset, float r, float g, float b, float a) {
    if (!pat) return VKVG_BRIDGE_ERR_NULL_HANDLE;
    vkvg_status_t s = vkvg_pattern_add_color_stop((VkvgPattern)H2P(pat), offset, r, g, b, a);
    return (s == VKVG_STATUS_SUCCESS) ? VKVG_BRIDGE_OK : (int32_t)s;
}

void vkvg_bridge_pattern_set_extend(uint64_t pat, int32_t extend) {
    if (pat) vkvg_pattern_set_extend((VkvgPattern)H2P(pat), (vkvg_extend_t)extend);
}

void vkvg_bridge_pattern_destroy(uint64_t pat) {
    if (pat) vkvg_pattern_destroy((VkvgPattern)H2P(pat));
}

void vkvg_bridge_set_source(uint64_t ctx, uint64_t pat) {
    if (ctx && pat) {
        vkvg_set_source((VkvgContext)H2P(ctx), (VkvgPattern)H2P(pat));
    }
}
