// ============================================================================
//  voxel_bridge.c — Kain natural-include bridge companion for voxel viewer
// ============================================================================
//  REFACTORED from voxel_viewer.c — wraps the full isometric voxel landscape
//  demo as a callable C API. All rendering, terrain generation, and the
//  Win32 window subclass are preserved exactly.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "voxel_bridge.h"
#include "../../widgets/ui_widget.h"
#include "../../ui_system.h"
#include "../../ui_system_internal.h"
#include "../../ui_host_adapter.h"
#include "../../../include/ui_renderer.h"
#include "../../../include/ui_layout.h"
#include "../../../include/ui_color.h"
#include "../../../include/ui_font.h"

// ══════════════════════════════════════════════════════════════════════════
//  CONSTANTS
// ══════════════════════════════════════════════════════════════════════════
#define GRID_W              32
#define GRID_H              32
#define MAX_VOXEL_HEIGHT    8.0
#define PI                  3.14159265358979323846
#define MAX_FONTS           6

// ══════════════════════════════════════════════════════════════════════════
//  KainWin32UiHost (must match ui_host_adapter.c exactly)
// ══════════════════════════════════════════════════════════════════════════
typedef struct KainWin32UiHost {
    HWND hwnd;
    int width;
    int height;
    int running;
    int initialized;
    uint8_t* framebuffer;
    int fb_stride;
    HDC hdc_buffer;
    HBITMAP hbitmap;
    int64_t session_id;
    int64_t input_session_id;
    float dpi_scale;
} KainWin32UiHost;

// ══════════════════════════════════════════════════════════════════════════
//  MATH HELPERS (inline)
// ══════════════════════════════════════════════════════════════════════════
static float fclampf(float v, float lo, float hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}
static int iclamp(int v, int lo, int hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}

// ══════════════════════════════════════════════════════════════════════════
//  PIXEL BLENDING 〰 bounds-safe, alpha-aware
// ══════════════════════════════════════════════════════════════════════════
static void blend_px(uint32_t* dst, uint32_t src) {
    uint8_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return;
    if (sa == 255) { *dst = src; return; }
    uint8_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    uint8_t da = 255 - sa;
    uint8_t dr = (uint8_t)(((uint16_t)sr * sa + ((*dst >> 16) & 0xFF) * da) / 255);
    uint8_t dg = (uint8_t)(((uint16_t)sg * sa + ((*dst >> 8) & 0xFF) * da) / 255);
    uint8_t db = (uint8_t)(((uint16_t)sb * sa + (*dst & 0xFF) * da) / 255);
    *dst = 0xFF000000 | ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | db;
}

static void blend_px_safe(KainWin32UiHost* host, int x, int y, uint32_t color) {
    if (!host || !host->framebuffer) return;
    int w = host->width, h = host->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int stride = host->fb_stride / 4;
    blend_px(&((uint32_t*)host->framebuffer)[y * stride + x], color);
}

static void fill_span_solid(uint32_t* fb, int stride, int xl, int xr, int y, uint32_t color) {
    uint32_t* row = fb + y * stride;
    for (int x = xl; x <= xr; x++) row[x] = color;
}

static void fill_span_blend(uint32_t* fb, int stride, int xl, int xr, int y, uint32_t color) {
    uint32_t* row = fb + y * stride;
    for (int x = xl; x <= xr; x++) blend_px(&row[x], color);
}

// ══════════════════════════════════════════════════════════════════════════
//  FILL A CONVEX QUAD (isometric face) --- scanline with edge walk
// ══════════════════════════════════════════════════════════════════════════
static void fill_quad_convex(uint32_t* fb, int stride, int fb_w, int fb_h,
                             int x0, int y0, int x1, int y1,
                             int x2, int y2, int x3, int y3,
                             uint32_t color) {
    int vx[4] = {x0, x1, x2, x3};
    int vy[4] = {y0, y1, y2, y3};
    for (int i = 1; i < 4; i++) {
        int tx = vx[i], ty = vy[i];
        int j = i - 1;
        while (j >= 0 && vy[j] > ty) {
            vx[j+1] = vx[j]; vy[j+1] = vy[j];
            j--;
        }
        vx[j+1] = tx; vy[j+1] = ty;
    }
    int sy0 = vy[0]; if (sy0 < 0) sy0 = 0; if (sy0 >= fb_h) return;
    int sy1 = vy[3]; if (sy1 >= fb_h) sy1 = fb_h - 1; if (sy1 < sy0) return;
    int use_blend = (color >> 24) < 255;
    for (int y = sy0; y <= sy1; y++) {
        float xs[4]; int nx = 0;
        for (int e = 0; e < 4; e++) {
            int e1 = e, e2 = (e + 1) % 4;
            int ya = vy[e1], yb = vy[e2];
            if (ya == yb) continue;
            if ((y < ya && y < yb) || (y > ya && y > yb)) continue;
            float t = (float)(y - ya) / (float)(yb - ya);
            float xi = vx[e1] + t * (float)(vx[e2] - vx[e1]);
            xs[nx++] = xi;
        }
        if (nx < 2) continue;
        for (int i = 0; i < nx - 1; i++)
            for (int j = i + 1; j < nx; j++)
                if (xs[j] < xs[i]) { float t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
        for (int i = 0; i + 1 < nx; i += 2) {
            int xl = (int)(xs[i] + 0.5f); if (xl < 0) xl = 0;
            int xr = (int)(xs[i+1] + 0.5f); if (xr >= fb_w) xr = fb_w - 1;
            if (xl > xr) continue;
            if (use_blend) fill_span_blend(fb, stride, xl, xr, y, color);
            else           fill_span_solid(fb, stride, xl, xr, y, color);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  TERRAIN — sin/cos noise heightmap generator (preserved from original)
// ══════════════════════════════════════════════════════════════════════════
static float terrain_height(int gx, int gy, float amplitude) {
    float h = 0.0f;
    h += sinf(gx * 0.25f + gy * 0.18f) * 0.5f;
    h += sinf(gx * 0.55f - gy * 0.42f + 1.3f) * 0.3f;
    h += cosf(gx * 0.13f + gy * 0.38f + 2.7f) * 0.25f;
    float dx = gx - GRID_W / 2.0f;
    float dy = gy - GRID_H / 2.0f;
    h += cosf((dx * dx + dy * dy) * 0.035f + 1.0f) * 0.45f;
    h += sinf(gx * 1.3f) * cosf(gy * 1.1f) * 0.1f;
    float hn = (h + 1.0f) * 0.5f;
    if (hn < 0.0f) hn = 0.0f; if (hn > 1.0f) hn = 1.0f;
    return hn * amplitude;
}

static uint32_t terrain_face_color(float height_normalized, float brightness, float time) {
    float h = height_normalized;
    uint8_t r, g, b;
    if (h < 0.3f) {
        float shimmer = sinf(time * 2.5f + h * 10.0f) * 0.12f;
        r = (uint8_t)((32 + (int)(shimmer * 64)) * brightness);
        g = (uint8_t)((68 + (int)(shimmer * 32)) * brightness);
        b = (uint8_t)((136 + (int)(shimmer * 80)) * brightness);
    } else if (h < 0.5f) {
        float t = (h - 0.3f) / 0.2f;
        r = (uint8_t)((32 + (int)(t * 140)) * brightness * (1.0f - t * 0.1f));
        g = (uint8_t)((68 + (int)(t * 70)) * brightness);
        b = (uint8_t)((100 + (int)(t * 30)) * brightness);
    } else if (h < 0.72f) {
        float t = (h - 0.5f) / 0.22f;
        r = (uint8_t)((34 + (int)(t * 20)) * brightness * (0.85f - t * 0.2f));
        g = (uint8_t)((139 + (int)(t * 30)) * brightness * (0.9f - t * 0.1f));
        b = (uint8_t)((34 + (int)(t * 20)) * brightness * (0.75f - t * 0.15f));
    } else if (h < 0.85f) {
        float t = (h - 0.72f) / 0.13f;
        uint8_t base = (uint8_t)((90 + (int)(t * 40)) * brightness);
        r = base; g = base; b = base;
    } else {
        float t = (h - 0.85f) / 0.15f;
        float wh = brightness * (0.9f + t * 0.1f);
        uint8_t wb = (uint8_t)(240 * wh);
        r = wb; g = wb; b = wb;
    }
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ══════════════════════════════════════════════════════════════════════════
//  VOXEL DEMO STATE (encapsulates all globals from voxel_viewer.c)
// ══════════════════════════════════════════════════════════════════════════
typedef struct {
    int dx, dy;
    float brightness;
} FaceInfo;

typedef struct {
    float heights[GRID_W][GRID_H];
    float cam_angle, cam_zoom;
    float cam_pan_x, cam_pan_y;
    int mouse_down, mouse_down_prev;
    double mouse_x, mouse_y;
    double mouse_down_x, mouse_down_y;
    int dragging;
    int hover_gx, hover_gy;
    int has_hover;
    double amplitude;
    int wireframe, animate, paused, show_panel;
    float time, tree_sway_phase, water_time;
    double fps;
    int frame_count, fps_counter;
    double fps_timer;
    int64_t font_ids[MAX_FONTS];
    int font_count;
} VoxelState;

struct VoxelDemo {
    VoxelState state;
    KainWin32UiHost* host;
    double dpi_scale;
    int tile_w, tile_h, voxel_h;
    int64_t session_id;
    KainUiWidgetContext* widget_ctx;
    WNDPROC orig_wndproc;
    int key_mask;
    int mouse_x, mouse_y;
    int mouse_down;
    LARGE_INTEGER freq, prev_time;
    float dt;
    int initialized;
};

// ══════════════════════════════════════════════════════════════════════════
//  FORWARD DECLS
// ══════════════════════════════════════════════════════════════════════════
static void gen_terrain(VoxelState* s);
static void iso_project(VoxelDemo* d, VoxelState* s, int gx, int gy, float z, int* out_sx, int* out_sy);
static void get_visible_faces(VoxelState* s, FaceInfo* fa, FaceInfo* fb);
static void get_top_face_verts(VoxelDemo* d, VoxelState* s, int gx, int gy, float h, int v[4][2]);
static void get_face_verts(VoxelDemo* d, VoxelState* s, int gx, int gy, float h, int fdx, int fdy, int v[4][2]);
static void render_voxel(uint32_t* fb, int stride, int fb_w, int fb_h, VoxelDemo* d, VoxelState* s, int gx, int gy, FaceInfo* fa, FaceInfo* fb, float time);
static void render_tree(uint32_t* fb, int stride, int fb_w, int fb_h, VoxelDemo* d, VoxelState* s, float time);
static void render_selection(uint32_t* fb, int stride, int fb_w, int fb_h, VoxelDemo* d, VoxelState* s, float time);
static void render_frame(VoxelDemo* d);
static void handle_input(VoxelDemo* d);
static LRESULT CALLBACK voxel_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ══════════════════════════════════════════════════════════════════════════
//  TERRAIN GENERATION
// ══════════════════════════════════════════════════════════════════════════
static void gen_terrain(VoxelState* s) {
    for (int gy = 0; gy < GRID_H; gy++)
        for (int gx = 0; gx < GRID_W; gx++)
            s->heights[gy][gx] = terrain_height(gx, gy, (float)s->amplitude);
}

// ══════════════════════════════════════════════════════════════════════════
//  ISOMETRIC PROJECTION
// ══════════════════════════════════════════════════════════════════════════
static void iso_project(VoxelDemo* d, VoxelState* s, int gx, int gy, float z, int* out_sx, int* out_sy) {
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);
    float rx = (float)gx * ca - (float)gy * sa;
    float ry = (float)gx * sa + (float)gy * ca;
    float tw = (float)d->tile_w * s->cam_zoom;
    float th = (float)d->tile_h * s->cam_zoom;
    float vh = (float)d->voxel_h * s->cam_zoom;
    int cx = (int)s->cam_pan_x;
    int cy = (int)s->cam_pan_y;
    *out_sx = (int)((rx - ry) * tw * 0.5f + (float)cx);
    *out_sy = (int)((rx + ry) * th * 0.5f - z * vh + (float)cy);
}

static void get_visible_faces(VoxelState* s, FaceInfo* fa, FaceInfo* fb) {
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);
    if (fabsf(ca) >= fabsf(sa)) {
        fa->dx = (ca >= 0) ? 1 : -1; fa->dy = 0;
    } else {
        fa->dx = 0; fa->dy = (sa >= 0) ? 1 : -1;
    }
    fa->brightness = 0.55f;
    if (fabsf(sa) >= fabsf(ca)) {
        fb->dx = (sa >= 0) ? -1 : 1; fb->dy = 0;
    } else {
        fb->dx = 0; fb->dy = (ca >= 0) ? 1 : -1;
    }
    fb->brightness = 0.75f;
}

static void get_top_face_verts(VoxelDemo* d, VoxelState* s, int gx, int gy, float h, int v[4][2]) {
    iso_project(d, s, gx,   gy,   h, &v[0][0], &v[0][1]);
    iso_project(d, s, gx+1, gy,   h, &v[1][0], &v[1][1]);
    iso_project(d, s, gx+1, gy+1, h, &v[2][0], &v[2][1]);
    iso_project(d, s, gx,   gy+1, h, &v[3][0], &v[3][1]);
}

static void get_face_verts(VoxelDemo* d, VoxelState* s, int gx, int gy, float h, int fdx, int fdy, int v[4][2]) {
    if (fdx == 1 && fdy == 0) {
        iso_project(d, s, gx+1, gy,   0.0f, &v[0][0], &v[0][1]);
        iso_project(d, s, gx+1, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(d, s, gx+1, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(d, s, gx+1, gy,   h,    &v[3][0], &v[3][1]);
    } else if (fdx == -1 && fdy == 0) {
        iso_project(d, s, gx, gy,   0.0f, &v[0][0], &v[0][1]);
        iso_project(d, s, gx, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(d, s, gx, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(d, s, gx, gy,   h,    &v[3][0], &v[3][1]);
    } else if (fdx == 0 && fdy == 1) {
        iso_project(d, s, gx,   gy+1, 0.0f, &v[0][0], &v[0][1]);
        iso_project(d, s, gx+1, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(d, s, gx+1, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(d, s, gx,   gy+1, h,    &v[3][0], &v[3][1]);
    } else {
        iso_project(d, s, gx,   gy, 0.0f, &v[0][0], &v[0][1]);
        iso_project(d, s, gx+1, gy, 0.0f, &v[1][0], &v[1][1]);
        iso_project(d, s, gx+1, gy, h,    &v[2][0], &v[2][1]);
        iso_project(d, s, gx,   gy, h,    &v[3][0], &v[3][1]);
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  RENDER SINGLE VOXEL
// ══════════════════════════════════════════════════════════════════════════
static void render_voxel(uint32_t* fb, int stride, int fb_w, int fb_h,
                         VoxelDemo* d, VoxelState* s, int gx, int gy,
                         FaceInfo* fa, FaceInfo* fb, float time) {
    float h = s->heights[gy][gx];
    if (h <= 0.01f) return;
    float height_norm = h / MAX_VOXEL_HEIGHT;
    uint32_t top_color = terrain_face_color(height_norm, 1.0f, time);
    if (height_norm < 0.3f) {
        float shimmer = sinf(time * 3.0f + (float)gx * 0.7f + (float)gy * 0.5f) * 0.08f;
        uint8_t sr = (top_color >> 16) & 0xFF, sg = (top_color >> 8) & 0xFF, sb = top_color & 0xFF;
        sr = (uint8_t)iclamp((int)(sr * (1.0f + shimmer)), 0, 255);
        sg = (uint8_t)iclamp((int)(sg * (1.0f + shimmer * 0.5f)), 0, 255);
        sb = (uint8_t)iclamp((int)(sb * (1.0f + shimmer)), 0, 255);
        top_color = 0xFF000000 | ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | sb;
    }
    int ngx_a = gx + fa->dx, ngy_a = gy + fa->dy;
    int occluded_a = (ngx_a >= 0 && ngx_a < GRID_W && ngy_a >= 0 && ngy_a < GRID_H)
                     && s->heights[ngy_a][ngx_a] >= h - 0.1f;
    int ngx_b = gx + fb->dx, ngy_b = gy + fb->dy;
    int occluded_b = (ngx_b >= 0 && ngx_b < GRID_W && ngy_b >= 0 && ngy_b < GRID_H)
                     && s->heights[ngy_b][ngx_b] >= h - 0.1f;
    if (!s->wireframe) {
        int v[4][2];
        get_top_face_verts(d, s, gx, gy, h, v);
        fill_quad_convex(fb, stride, fb_w, fb_h, v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1], v[3][0], v[3][1], top_color);
        if (!occluded_a) {
            uint32_t ca = terrain_face_color(height_norm, fa->brightness, time);
            get_face_verts(d, s, gx, gy, h, fa->dx, fa->dy, v);
            fill_quad_convex(fb, stride, fb_w, fb_h, v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1], v[3][0], v[3][1], ca);
        }
        if (!occluded_b) {
            uint32_t cb = terrain_face_color(height_norm, fb->brightness, time);
            get_face_verts(d, s, gx, gy, h, fb->dx, fb->dy, v);
            fill_quad_convex(fb, stride, fb_w, fb_h, v[0][0], v[0][1], v[1][0], v[1][1], v[2][0], v[2][1], v[3][0], v[3][1], cb);
        }
    }
    if (s->wireframe) {
        int v[4][2]; uint32_t wire_col = 0xFF00FFAA;
        get_top_face_verts(d, s, gx, gy, h, v);
        for (int e = 0; e < 4; e++) {
            int x1 = v[e][0], y1 = v[e][1], x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
            float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
            int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
            if (steps < 1) steps = 1;
            for (int i = 0; i <= steps; i++) {
                float t = (float)i / (float)steps;
                int px = (int)(x1 + dx * t), py = (int)(y1 + dy * t);
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) fb[py * stride + px] = wire_col;
            }
        }
        if (!occluded_a) {
            get_face_verts(d, s, gx, gy, h, fa->dx, fa->dy, v);
            for (int e = 0; e < 4; e++) {
                int x1 = v[e][0], y1 = v[e][1], x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
                float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
                int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
                if (steps < 1) steps = 1;
                for (int i = 0; i <= steps; i++) {
                    float t = (float)i / (float)steps;
                    int px = (int)(x1 + dx * t), py = (int)(y1 + dy * t);
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) fb[py * stride + px] = wire_col;
                }
            }
        }
        if (!occluded_b) {
            get_face_verts(d, s, gx, gy, h, fb->dx, fb->dy, v);
            for (int e = 0; e < 4; e++) {
                int x1 = v[e][0], y1 = v[e][1], x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
                float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
                int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
                if (steps < 1) steps = 1;
                for (int i = 0; i <= steps; i++) {
                    float t = (float)i / (float)steps;
                    int px = (int)(x1 + dx * t), py = (int)(y1 + dy * t);
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) fb[py * stride + px] = wire_col;
                }
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  TREE ON HIGHEST PEAK
// ══════════════════════════════════════════════════════════════════════════
static void render_tree(uint32_t* fb, int stride, int fb_w, int fb_h,
                        VoxelDemo* d, VoxelState* s, float time) {
    float max_h = -1.0f;
    int peak_gx = GRID_W / 2, peak_gy = GRID_H / 2;
    for (int gy = 0; gy < GRID_H; gy++)
        for (int gx = 0; gx < GRID_W; gx++)
            if (s->heights[gy][gx] > max_h) { max_h = s->heights[gy][gx]; peak_gx = gx; peak_gy = gy; }
    if (max_h < 0.5f) return;
    float sway = sinf(time * 1.2f) * 0.03f;
    float tree_height = max_h + 1.2f;
    float trunk_w = 0.3f;
    float trunk_top = tree_height;
    float trunk_base = max_h + 0.2f;
    int tv[4][2];
    iso_project(d, s, peak_gx + sway,       peak_gy,       trunk_base, &tv[0][0], &tv[0][1]);
    iso_project(d, s, peak_gx + sway,       peak_gy + 0.2f, trunk_base, &tv[1][0], &tv[1][1]);
    iso_project(d, s, peak_gx + sway,       peak_gy + 0.2f, trunk_top,  &tv[2][0], &tv[2][1]);
    iso_project(d, s, peak_gx + sway,       peak_gy,       trunk_top,  &tv[3][0], &tv[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h, tv[0][0], tv[0][1], tv[1][0], tv[1][1], tv[2][0], tv[2][1], tv[3][0], tv[3][1], 0xFF6B3A1F);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy,       trunk_base, &tv[0][0], &tv[0][1]);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_base, &tv[1][0], &tv[1][1]);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_top,  &tv[2][0], &tv[2][1]);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy,       trunk_top,  &tv[3][0], &tv[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h, tv[0][0], tv[0][1], tv[1][0], tv[1][1], tv[2][0], tv[2][1], tv[3][0], tv[3][1], 0xFF4A2A10);
    int tt[4][2];
    iso_project(d, s, peak_gx + sway,       peak_gy,       trunk_top, &tt[0][0], &tt[0][1]);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy,       trunk_top, &tt[1][0], &tt[1][1]);
    iso_project(d, s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_top, &tt[2][0], &tt[2][1]);
    iso_project(d, s, peak_gx + sway,       peak_gy + 0.2f, trunk_top, &tt[3][0], &tt[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h, tt[0][0], tt[0][1], tt[1][0], tt[1][1], tt[2][0], tt[2][1], tt[3][0], tt[3][1], 0xFF8B5E3C);
    float leaf_pos[][3] = {
        { peak_gx + sway,       peak_gy,       tree_height + 0.4f },
        { peak_gx + sway - 0.2f, peak_gy,       tree_height + 0.2f },
        { peak_gx + sway + 0.2f, peak_gy,       tree_height + 0.2f },
        { peak_gx + sway,       peak_gy - 0.2f, tree_height + 0.2f },
        { peak_gx + sway,       peak_gy + 0.2f, tree_height + 0.2f },
        { peak_gx + sway,       peak_gy,       tree_height + 0.7f },
    };
    uint32_t lc[] = { 0xFF2ECC71, 0xFF27AE60, 0xFF229954, 0xFF1E8449, 0xFF2ECC71, 0xFF1ABC9C };
    for (int i = 0; i < 6; i++) {
        float lx = leaf_pos[i][0], ly = leaf_pos[i][1], lz = leaf_pos[i][2];
        float hs = 0.15f;
        int lv[4][2];
        iso_project(d, s, lx - hs, ly - hs, lz + hs, &lv[0][0], &lv[0][1]);
        iso_project(d, s, lx + hs, ly - hs, lz + hs, &lv[1][0], &lv[1][1]);
        iso_project(d, s, lx + hs, ly + hs, lz + hs, &lv[2][0], &lv[2][1]);
        iso_project(d, s, lx - hs, ly + hs, lz + hs, &lv[3][0], &lv[3][1]);
        fill_quad_convex(fb, stride, fb_w, fb_h, lv[0][0], lv[0][1], lv[1][0], lv[1][1], lv[2][0], lv[2][1], lv[3][0], lv[3][1], lc[i]);
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  SELECTION HIGHLIGHT
// ══════════════════════════════════════════════════════════════════════════
static void render_selection(uint32_t* fb, int stride, int fb_w, int fb_h,
                             VoxelDemo* d, VoxelState* s, float time) {
    if (!s->has_hover) return;
    int gx = s->hover_gx, gy = s->hover_gy;
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return;
    float h = s->heights[gy][gx];
    if (h <= 0.01f) return;
    float pulse = sinf(time * 4.0f) * 0.3f + 0.7f;
    uint8_t pulse_alpha = (uint8_t)(128 + (int)(pulse * 127));
    uint32_t sel_color = (pulse_alpha << 24) | 0x00FFDD44;
    int v[4][2];
    get_top_face_verts(d, s, gx, gy, h, v);
    for (int e = 0; e < 4; e++) {
        int x1 = v[e][0], y1 = v[e][1], x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
        int steps = (int)(fabsf(dx) + fabsf(dy)) + 1;
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / (float)(steps > 0 ? steps : 1);
            int px = (int)(x1 + dx * t), py = (int)(y1 + dy * t);
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                blend_px(&fb[py * stride + px], sel_color);
        }
    }
    for (int i = 0; i < 4; i++)
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                int px = v[i][0] + dx, py = v[i][1] + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    if (dx*dx + dy*dy <= 4) fb[py * stride + px] = 0xFFFFFF44;
            }
}

// ══════════════════════════════════════════════════════════════════════════
//  HUD OVERLAY (simplified from original)
// ══════════════════════════════════════════════════════════════════════════
static void render_hud(VoxelDemo* d) {
    KainUiWidgetContext* ctx = d->widget_ctx;
    KainWin32UiHost* host = d->host;
    VoxelState* s = &d->state;
    double ds = d->dpi_scale;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width, fb_h = host->height;
    int top_bar_h = (int)(50 * ds + 0.5);
    for (int y = 0; y < top_bar_h && y < fb_h; y++) {
        uint32_t col = 0x40000000;
        for (int x = 0; x < fb_w; x++) blend_px(&fb[y * stride + x], col);
    }
    int margin = (int)(12 * ds + 0.5);
    int fs_title = (int)(16 * ds + 0.5);
    int fs_data = (int)(13 * ds + 0.5);
    int fs_small = (int)(11 * ds + 0.5);
    int ty = (int)(6 * ds + 0.5);
    int fid_title = (s->font_ids[3] > 0) ? 3 : 0;
    int fid_data = (s->font_ids[1] > 0) ? 1 : 0;
    if (fid_title < MAX_FONTS && s->font_ids[fid_title] > 0)
        ui_widget_draw_text_ex(ctx, margin, ty, "VOXEL ISOMETRIC (Kain include)", 0xFFFFDD44, 0, s->font_ids[fid_title]);
    else
        ui_widget_draw_text(ctx, margin, ty, "VOXEL ISOMETRIC (Kain include)", 0xFFFFDD44, fs_title);
    char buf[128];
    int tx = fb_w - (int)(140 * ds + 0.5);
    snprintf(buf, 128, "FPS: %.0f", s->fps);
    ui_widget_draw_text(ctx, tx, (int)(8 * ds + 0.5), buf,
        s->fps >= 110.0 ? 0xFF21D4A1 : s->fps >= 60.0 ? 0xFFE8914A : 0xFFE84A5F, fs_data);
    snprintf(buf, 128, "Angle: %.0f deg  Zoom: %.1f  Voxels: %d",
             s->cam_angle * 180.0f / PI, s->cam_zoom, GRID_W * GRID_H);
    int margin2 = (int)(16 * ds + 0.5);
    ui_widget_draw_text(ctx, margin2, (int)(28 * ds + 0.5), buf, 0xFF8888A0, fs_small);
    int ly = fb_h - (int)(28 * ds + 0.5);
    ui_widget_draw_text(ctx, margin, ly, "Arrows=Rotate  W/S=Zoom  Click+Drag=Pan  R=Reset  Space=Pause  Esc=Exit", 0xFF666688, fs_small);
    if (s->paused)
        ui_widget_draw_text(ctx, fb_w / 2 - (int)(60 * ds + 0.5), fb_h / 2 - (int)(20 * ds + 0.5),
                            ">> PAUSED <<", 0xFFFF4444, (int)(18 * ds + 0.5));
}

// ══════════════════════════════════════════════════════════════════════════
//  MAIN RENDER
// ══════════════════════════════════════════════════════════════════════════
static void render_frame(VoxelDemo* d) {
    KainWin32UiHost* host = d->host;
    VoxelState* s = &d->state;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width, fb_h = host->height;
    if (!fb || fb_w <= 0 || fb_h <= 0) return;
    int total = fb_w * fb_h;
    for (int i = 0; i < total; i++) fb[i] = 0xFF0A0A14;
    FaceInfo face_a, face_b;
    get_visible_faces(s, &face_a, &face_b);
    typedef struct { int gx, gy; float depth; } DepthCell;
    DepthCell cells[GRID_W * GRID_H];
    int nc = 0;
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);
    for (int gy = 0; gy < GRID_H; gy++)
        for (int gx = 0; gx < GRID_W; gx++) {
            float rx = (float)gx * ca - (float)gy * sa;
            float ry = (float)gx * sa + (float)gy * ca;
            cells[nc].gx = gx; cells[nc].gy = gy;
            cells[nc].depth = rx + ry; nc++;
        }
    for (int i = 1; i < nc; i++) {
        DepthCell key = cells[i]; int j = i - 1;
        while (j >= 0 && cells[j].depth > key.depth) { cells[j+1] = cells[j]; j--; }
        cells[j+1] = key;
    }
    float anim_t = s->animate ? s->time : 0.0f;
    for (int i = 0; i < nc; i++)
        render_voxel(fb, stride, fb_w, fb_h, d, s, cells[i].gx, cells[i].gy, &face_a, &face_b, anim_t);
    render_tree(fb, stride, fb_w, fb_h, d, s, s->animate ? s->time : 0.0f);
    render_selection(fb, stride, fb_w, fb_h, d, s, s->time);
    render_hud(d);
}

// ══════════════════════════════════════════════════════════════════════════
//  INPUT HANDLING
// ══════════════════════════════════════════════════════════════════════════
static void handle_input(VoxelDemo* d) {
    VoxelState* s = &d->state;
    float dt = d->dt;
    float rot_speed = 1.5f * dt;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  s->cam_angle -= rot_speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) s->cam_angle += rot_speed;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    s->cam_angle -= rot_speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  s->cam_angle += rot_speed;
    float zoom_speed = 0.5f * dt;
    if (GetAsyncKeyState('W') & 0x8000) s->cam_zoom += zoom_speed;
    if (GetAsyncKeyState('S') & 0x8000) s->cam_zoom -= zoom_speed;
    s->cam_zoom = fclampf(s->cam_zoom, 0.3f, 4.0f);
    static int prev_r = 0, prev_space = 0;
    int r_now = (GetAsyncKeyState('R') & 0x8000) ? 1 : 0;
    int sp_now = (GetAsyncKeyState(VK_SPACE) & 0x8000) ? 1 : 0;
    if (r_now && !prev_r) {
        s->cam_angle = 0.0f; s->cam_zoom = 1.0f;
        s->cam_pan_x = d->host ? (float)d->host->width * 0.5f : 640.0f;
        s->cam_pan_y = d->host ? (float)d->host->height * 0.5f + 30.0f : 390.0f;
    }
    if (sp_now && !prev_space) s->paused = !s->paused;
    prev_r = r_now; prev_space = sp_now;
    // Mouse state from Win32
    POINT mp;
    GetCursorPos(&mp);
    if (d->host && d->host->hwnd) ScreenToClient(d->host->hwnd, &mp);
    s->mouse_x = (double)mp.x;
    s->mouse_y = (double)mp.y;
    s->mouse_down_prev = s->mouse_down;
    s->mouse_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;
    if (s->mouse_down && !s->mouse_down_prev) {
        s->dragging = 1;
        s->mouse_down_x = s->mouse_x;
        s->mouse_down_y = s->mouse_y;
    }
    if (s->dragging && s->mouse_down) {
        float dx = (float)(s->mouse_x - s->mouse_down_x);
        float dy = (float)(s->mouse_y - s->mouse_down_y);
        s->cam_pan_x += dx; s->cam_pan_y += dy;
        s->mouse_down_x = s->mouse_x; s->mouse_down_y = s->mouse_y;
    }
    if (!s->mouse_down) s->dragging = 0;
    if (!s->dragging && !s->mouse_down) {
        s->has_hover = 0;
        int mx = (int)s->mouse_x, my = (int)s->mouse_y;
        KainWin32UiHost* host = d->host;
        if (host && mx >= 0 && mx < host->width && my >= 0 && my < host->height) {
            int best_dist = 100;
            for (int gy = 0; gy < GRID_H; gy++)
                for (int gx = 0; gx < GRID_W; gx++) {
                    float h = s->heights[gy][gx];
                    if (h <= 0.01f) continue;
                    int sx, sy;
                    iso_project(d, s, gx, gy, h * 0.5f, &sx, &sy);
                    int dx = mx - sx, dy = my - sy;
                    int dist = dx * dx + dy * dy;
                    if (dist < best_dist && dist < 800) {
                        best_dist = dist; s->hover_gx = gx; s->hover_gy = gy; s->has_hover = 1;
                    }
                }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  SUBCLASSED WNDPROC
// ══════════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK voxel_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    VoxelDemo* d = (VoxelDemo*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!d || !d->host) return DefWindowProcA(hwnd, msg, w, l);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        if (d->host->hdc_buffer)
            BitBlt(hdc, 0, 0, d->host->width, d->host->height, d->host->hdc_buffer, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_CLOSE: d->host->running = 0; DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return d->orig_wndproc ? CallWindowProcA(d->orig_wndproc, hwnd, msg, w, l) : DefWindowProcA(hwnd, msg, w, l);
}

// ══════════════════════════════════════════════════════════════════════════
//  PUBLIC API
// ══════════════════════════════════════════════════════════════════════════

VoxelDemo* voxel_bridge_init(int width, int height) {
    SetProcessDPIAware();
    HDC dpi_dc = GetDC(NULL);
    float dpi = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi < 1.0f) dpi = 1.0f;
    int win_w = (int)(width * dpi + 0.5f);
    int win_h = (int)(height * dpi + 0.5f);

    VoxelDemo* d = (VoxelDemo*)calloc(1, sizeof(VoxelDemo));
    if (!d) return NULL;
    d->dpi_scale = dpi;
    d->tile_w = (int)(40 * dpi + 0.5);
    d->tile_h = (int)(20 * dpi + 0.5);
    d->voxel_h = (int)(20 * dpi + 0.5);

    VoxelState* s = &d->state;
    s->cam_zoom = 1.0f;
    s->amplitude = (double)MAX_VOXEL_HEIGHT;
    s->animate = 1;
    s->show_panel = 0;  // no control panel in Kain version
    s->cam_pan_x = (float)win_w * 0.5f;
    s->cam_pan_y = (float)win_h * 0.5f + 30.0f;

    srand((unsigned int)time(NULL));
    gen_terrain(s);

    int64_t sid = abi_ui_session_create("voxel_kain", win_w, win_h);
    if (sid <= 0) { free(d); return NULL; }
    d->session_id = sid;

    if (abi_ui_host_attach(sid, "winit") < 0) {
        abi_ui_session_destroy(sid); free(d); return NULL;
    }
    abi_ui_window_open(sid, "Isometric Voxel Viewer (Kain include)", win_w, win_h);

    KainUiWidgetContext* ctx = ui_widget_create(sid);
    if (!ctx) { abi_ui_session_destroy(sid); free(d); return NULL; }
    d->widget_ctx = ctx;

    const char* font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/impact.ttf",
        "C:/Windows/Fonts/georgia.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
    };
    double font_sizes[] = {12.0*dpi, 11.0*dpi, 12.0*dpi, 18.0*dpi, 11.0*dpi, 11.0*dpi};
    for (int i = 0; i < MAX_FONTS; i++) {
        s->font_ids[i] = ui_widget_load_font(ctx, font_paths[i], font_sizes[i]);
        if (s->font_ids[i] > 0) s->font_count = i + 1;
    }
    if (ctx->default_font < 0 && s->font_ids[0] > 0) ctx->default_font = 0;

    KainNativeUiSession* ns = abi_ui_find_session(sid);
    if (!ns || !ns->host_state) {
        ui_widget_destroy(ctx); abi_ui_session_destroy(sid); free(d); return NULL;
    }
    KainWin32UiHost* khost = (KainWin32UiHost*)ns->host_state;
    d->host = khost;
    HWND hwnd = khost->hwnd;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)d);
    d->orig_wndproc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)voxel_wndproc);
    SetWindowTextA(hwnd, "Isometric Voxel Viewer (Kain include)");

    QueryPerformanceFrequency(&d->freq);
    QueryPerformanceCounter(&d->prev_time);
    d->dt = 0.016f;
    d->initialized = 1;

    return d;
}

void voxel_bridge_destroy(VoxelDemo* d) {
    if (!d) return;
    if (d->host) {
        HWND hwnd = d->host->hwnd;
        if (d->orig_wndproc && hwnd)
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)d->orig_wndproc);
    }
    if (d->widget_ctx) ui_widget_destroy(d->widget_ctx);
    if (d->session_id > 0) abi_ui_session_destroy(d->session_id);
    free(d);
}

int voxel_bridge_frame(VoxelDemo* d) {
    if (!d || !d->initialized) return -1;
    if (!d->host || !d->host->running) return -1;

    // Message pump
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { d->host->running = 0; return -1; }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (!d->host->running) return -1;
    if (d->key_mask & 256) { d->host->running = 0; return -1; }  // ESC

    // Timing
    LARGE_INTEGER ct;
    QueryPerformanceCounter(&ct);
    d->dt = (float)((double)(ct.QuadPart - d->prev_time.QuadPart) / (double)d->freq.QuadPart);
    if (d->dt > 0.05f) d->dt = 0.05f;
    if (d->dt < 0.001f) d->dt = 0.001f;
    d->prev_time = ct;

    VoxelState* s = &d->state;
    if (!s->paused) { s->time += d->dt; s->water_time += d->dt; }
    s->fps_counter++;
    s->fps_timer += d->dt;
    if (s->fps_timer >= 1.0) { s->fps = (double)s->fps_counter / s->fps_timer; s->fps_counter = 0; s->fps_timer = 0.0; }
    s->frame_count++;

    handle_input(d);

    int64_t sid = d->session_id;
    abi_ui_begin_frame(sid, d->dt * 1000.0);
    ui_widget_begin_frame(d->widget_ctx);
    render_frame(d);
    ui_widget_end_frame(d->widget_ctx);
    abi_ui_end_frame(sid);

    InvalidateRect(d->host->hwnd, NULL, FALSE);
    Sleep(6);

    return 0;
}

int voxel_bridge_running(VoxelDemo* d) {
    return (d && d->host && d->host->running) ? 1 : 0;
}

void voxel_bridge_set_keys(VoxelDemo* d, int key_mask) {
    if (d) d->key_mask = key_mask;
}

void voxel_bridge_set_mouse(VoxelDemo* d, int mouse_x, int mouse_y, int mouse_down) {
    if (d) { d->mouse_x = mouse_x; d->mouse_y = mouse_y; d->mouse_down = mouse_down; }
}
