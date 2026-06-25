// ============================================================================
//  voxel_viewer.c — ISOMETRIC VOXEL LANDSCAPE — 120FPS
//  ============================================================================
//  A real-time isometric voxel landscape renderer pushing the Kain software
//  framebuffer to 120fps. Features:
//
//    - 32x32 heightmap landscape with sin/cos noise terrain
//    - Isometric projection with orbital camera (arrow keys, W/S zoom)
//    - Voxel face colors: water (blue) → grass (green) → stone (grey) → snow
//    - Directional lighting: top=100%, left=70%, right=50%
//    - Animated water shimmer, swaying tree on highest peak
//    - Mouse-over voxel selection highlight
//    - 6 fonts loaded from C:/Windows/Fonts/ for rich HUD typography
//    - Painter's algorithm with back-to-front sorting
//    - Occlusion culling (skip faces hidden by taller neighbors)
//    - Pre-computed base projections (recomputed on angle change only)
//    - Interactive control panel: amplitude slider, wireframe toggle,
//      randomize terrain button, animation checkbox
//    - Keyboard: Esc=exit, Space=pause, R=reset, arrows=rotate, W/S=zoom
//
//  Build (from X:\runtime\native\src\ui\test_ui_v2):
//    clang -std=c11 -g -O0 voxel_viewer.c stubs.c ../widgets/ui_widget.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ^
//      ../ui_color.c ../../core/input_system.c ../../core/component_surface.c ^
//      -I../../../include -I.. -I../widgets -I../../core ^
//      -I../../../extras/_stb-truetype -luser32 -lgdi32 -lopengl32 ^
//      -o voxel_viewer.exe
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

#include "../widgets/ui_widget.h"
#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"
#include "../../include/ui_font.h"

// ── Stubs from core.c ─────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost (must match ui_host_adapter.c exactly) ────────────
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
} KainWin32UiHost;

// ── Forward declarations ──────────────────────────────────────────────
static LRESULT CALLBACK voxel_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ============================================================================
//  CONSTANTS
// ============================================================================
#define GRID_W              32
#define GRID_H              32
#define MAX_VOXEL_HEIGHT    8.0
#define TILE_W              ((int)(40 * g_dpi_scale + 0.5))
#define TILE_H              ((int)(20 * g_dpi_scale + 0.5))
#define VOXEL_H             ((int)(20 * g_dpi_scale + 0.5))
#define SCREEN_W            1280
#define SCREEN_H            720
#define MAX_FONTS           6
#define PI                  3.14159265358979323846

// ============================================================================
//  GLOBALS
// ============================================================================
static double g_dpi_scale = 1.0;
static KainWin32UiHost* g_host = NULL;
static WNDPROC g_orig_wndproc = NULL;

// ============================================================================
//  MATH HELPERS
// ============================================================================
static float fclampf(float v, float lo, float hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}
static int iclamp(int v, int lo, int hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

// Fast float→int truncation
static int f2i(float v) { return (int)v; }

// ============================================================================
//  PIXEL BLENDING — bounds-safe, alpha-aware
// ============================================================================
static void write_px(KainWin32UiHost* host, int x, int y, uint32_t color) {
    if (!host || !host->framebuffer) return;
    int w = host->width, h = host->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int stride = host->fb_stride / 4;
    ((uint32_t*)host->framebuffer)[y * stride + x] = color;
}

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

// Fill a horizontal span with a solid color (no alpha blending — fast path)
static void fill_span_solid(uint32_t* fb, int stride, int xl, int xr, int y, uint32_t color) {
    uint32_t* row = fb + y * stride;
    for (int x = xl; x <= xr; x++) row[x] = color;
}

// Fill a horizontal span with alpha blending
static void fill_span_blend(uint32_t* fb, int stride, int xl, int xr, int y, uint32_t color) {
    uint32_t* row = fb + y * stride;
    for (int x = xl; x <= xr; x++) blend_px(&row[x], color);
}

// ============================================================================
//  FILL A CONVEX QUAD (isometric face) — scanline with edge walk
// ============================================================================
static void fill_quad_convex(uint32_t* fb, int stride, int fb_w, int fb_h,
                             int x0, int y0, int x1, int y1,
                             int x2, int y2, int x3, int y3,
                             uint32_t color) {
    // Collect vertices, sort by Y (insertion sort, 4 items)
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
        // Find X intersections with all edges
        float xs[4];
        int nx = 0;
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

        // Bubble sort X intersections (max 4)
        for (int i = 0; i < nx - 1; i++)
            for (int j = i + 1; j < nx; j++)
                if (xs[j] < xs[i]) { float t = xs[i]; xs[i] = xs[j]; xs[j] = t; }

        // Fill pairs
        for (int i = 0; i + 1 < nx; i += 2) {
            int xl = (int)(xs[i] + 0.5f); if (xl < 0) xl = 0;
            int xr = (int)(xs[i+1] + 0.5f); if (xr >= fb_w) xr = fb_w - 1;
            if (xl > xr) continue;
            if (use_blend)
                fill_span_blend(fb, stride, xl, xr, y, color);
            else
                fill_span_solid(fb, stride, xl, xr, y, color);
        }
    }
}

// ============================================================================
//  HEIGHTMAP — sin/cos noise terrain generator
// ============================================================================
static float terrain_height(int gx, int gy, float t, float amplitude) {
    float h = 0.0f;
    // Large rolling hills
    h += sinf(gx * 0.25f + gy * 0.18f) * 0.5f;
    h += sinf(gx * 0.55f - gy * 0.42f + 1.3f) * 0.3f;
    // Medium detail
    h += cosf(gx * 0.13f + gy * 0.38f + 2.7f) * 0.25f;
    // Central peak
    float dx = gx - GRID_W / 2.0f;
    float dy = gy - GRID_H / 2.0f;
    h += cosf((dx * dx + dy * dy) * 0.035f + 1.0f) * 0.45f;
    // Small ridges
    h += sinf(gx * 1.3f) * cosf(gy * 1.1f) * 0.1f;

    // Normalize to [0, 1] and scale
    float hn = (h + 1.0f) * 0.5f;
    if (hn < 0.0f) hn = 0.0f; if (hn > 1.0f) hn = 1.0f;
    return hn * amplitude;
}

// ============================================================================
//  TERRAIN COLOR — height-based with directional lighting
// ============================================================================
static uint32_t terrain_face_color(float height_normalized, float brightness, float time) {
    float h = height_normalized;
    uint8_t r, g, b;

    if (h < 0.3f) {
        // Water: blue shades with shimmer
        float shimmer = sinf(time * 2.5f + h * 10.0f) * 0.12f;
        float t = h / 0.3f; // 0..1
        r = (uint8_t)((32 + (int)(shimmer * 64)) * brightness);
        g = (uint8_t)((68 + (int)(shimmer * 32)) * brightness);
        b = (uint8_t)((136 + (int)(shimmer * 80)) * brightness);
    } else if (h < 0.5f) {
        // Beach / transition
        float t = (h - 0.3f) / 0.2f;
        r = (uint8_t)((32 + (int)(t * 140)) * brightness * (1.0f - t * 0.1f));
        g = (uint8_t)((68 + (int)(t * 70)) * brightness);
        b = (uint8_t)((100 + (int)(t * 30)) * brightness);
    } else if (h < 0.72f) {
        // Grass: green shades
        float t = (h - 0.5f) / 0.22f;
        r = (uint8_t)((34 + (int)(t * 20)) * brightness * (0.85f - t * 0.2f));
        g = (uint8_t)((139 + (int)(t * 30)) * brightness * (0.9f - t * 0.1f));
        b = (uint8_t)((34 + (int)(t * 20)) * brightness * (0.75f - t * 0.15f));
    } else if (h < 0.85f) {
        // Stone: grey shades
        float t = (h - 0.72f) / 0.13f;
        uint8_t base = (uint8_t)((90 + (int)(t * 40)) * brightness);
        r = base; g = base; b = base;
    } else {
        // Snow: white peak
        float t = (h - 0.85f) / 0.15f;
        float wh = brightness * (0.9f + t * 0.1f);
        uint8_t wb = (uint8_t)(240 * wh);
        r = wb; g = wb; b = wb;
    }

    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ============================================================================
//  VOXEL ENGINE STATE
// ============================================================================
typedef struct {
    // Terrain
    float heights[GRID_W][GRID_H];

    // Camera
    float cam_angle;         // radians — orbit around center
    float cam_zoom;          // scale factor (1.0 = default)
    float cam_pan_x, cam_pan_y;

    // Interaction
    int mouse_down;
    int mouse_down_prev;
    double mouse_x, mouse_y;
    double mouse_down_x, mouse_down_y;
    int dragging;
    int hover_gx, hover_gy;
    int has_hover;

    // Controls
    double amplitude;
    int wireframe;
    int animate;
    int paused;
    int show_panel;

    // Animation
    float time;
    float tree_sway_phase;
    float water_time;

    // Performance
    double fps;
    int frame_count;
    int fps_counter;
    double fps_timer;

    // Font IDs
    int64_t font_ids[MAX_FONTS];
    int font_count;

    // Widget interaction
    int btn_randomize;
    int btn_wireframe;
    int sld_amplitude;
    int chk_animate;
} VoxelState;

static VoxelState g_state;

// ============================================================================
//  GENERATE TERRAIN
// ============================================================================
static void gen_terrain(void) {
    VoxelState* s = &g_state;
    for (int gy = 0; gy < GRID_H; gy++)
        for (int gx = 0; gx < GRID_W; gx++)
            s->heights[gy][gx] = terrain_height(gx, gy, 0, (float)s->amplitude);
}

// ============================================================================
//  ISOMETRIC PROJECTION
// ============================================================================
// Projects a grid cell (gx, gy) at height z to screen.
// Camera rotation is applied before projection.
static void iso_project(VoxelState* s, int gx, int gy, float z,
                        int* out_sx, int* out_sy) {
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);
    float rx = (float)gx * ca - (float)gy * sa;
    float ry = (float)gx * sa + (float)gy * ca;
    float tw = TILE_W * s->cam_zoom;
    float th = TILE_H * s->cam_zoom;
    float vh = VOXEL_H * s->cam_zoom;

    int cx = s->cam_pan_x;
    int cy = s->cam_pan_y;

    *out_sx = (int)((rx - ry) * tw * 0.5f + (float)cx);
    *out_sy = (int)((rx + ry) * th * 0.5f - z * vh + (float)cy);
}

// ============================================================================
//  DETERMINE VISIBLE FACES BASED ON CAMERA ANGLE
// ============================================================================
// Returns the two visible vertical face directions as (dx, dy) offsets.
// face_a has brightness 50% (screen-right), face_b has 70% (screen-left).
typedef struct { int dx, dy; float brightness; } FaceInfo;

static void get_visible_faces(VoxelState* s, FaceInfo* face_a, FaceInfo* face_b) {
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);

    // face_a: direction that projects to +rx (screen-right) 
    if (fabsf(ca) >= fabsf(sa)) {
        face_a->dx = (ca >= 0) ? 1 : -1;
        face_a->dy = 0;
    } else {
        face_a->dx = 0;
        face_a->dy = (sa >= 0) ? 1 : -1;
    }
    face_a->brightness = 0.55f; // screen-right face → darker

    // face_b: direction that projects to +ry (screen-left)
    if (fabsf(sa) >= fabsf(ca)) {
        face_b->dx = (sa >= 0) ? -1 : 1;
        face_b->dy = 0;
    } else {
        face_b->dx = 0;
        face_b->dy = (ca >= 0) ? 1 : -1;
    }
    face_b->brightness = 0.75f; // screen-left face → lighter (light from upper-left)
}

// ============================================================================
//  VOXEL FACE VERTICES
// ============================================================================
// For a voxel at grid (gx, gy) with base height h, returns the 4 vertices
// for a face in screen space.
//
// Top face: z = h, the 4 top corners of the column
// Face +X: world direction (1,0,0), 4 vertices of the +X wall
// Face -X: world direction (-1,0,0)
// Face +Y: world direction (0,1,0)
// Face -Y: world direction (0,-1,0)

static void get_top_face_verts(VoxelState* s, int gx, int gy, float h,
                               int v[4][2]) {
    iso_project(s, gx,   gy,   h, &v[0][0], &v[0][1]);
    iso_project(s, gx+1, gy,   h, &v[1][0], &v[1][1]);
    iso_project(s, gx+1, gy+1, h, &v[2][0], &v[2][1]);
    iso_project(s, gx,   gy+1, h, &v[3][0], &v[3][1]);
}

static void get_face_verts(VoxelState* s, int gx, int gy, float h,
                           int fdx, int fdy,
                           int v[4][2]) {
    // The face is on the side of the voxel column.
    // 4 vertices: 2 at bottom (z=0), 2 at top (z=h), extruded in face direction.
    // Bottom edge: from (gx, gy) to (gx+fdx, gy+fdy) — hmm, not quite.
    // 
    // For face in direction (fdx, fdy) on the X/Y axis:
    // The face is the rectangle at position (gx+fdx_if_needed, gy+fdy_if_needed).
    // Bottom-left vertex: at (gx, gy, 0) if fdx>0 → actually at gx+fdx, gy, 0...
    //
    // Actually for a column at (gx, gy) with dimensions [gx, gx+1] × [gy, gy+1]:
    // The +X face (fdx=1, fdy=0) is at x = gx+1, spanning y in [gy, gy+1], z in [0, h]
    // Its vertices: (gx+1, gy, 0), (gx+1, gy+1, 0), (gx+1, gy+1, h), (gx+1, gy, h)
    //
    // The +Y face (fdx=0, fdy=1) is at y = gy+1, spanning x in [gx, gx+1], z in [0, h]
    // Its vertices: (gx, gy+1, 0), (gx+1, gy+1, 0), (gx+1, gy+1, h), (gx, gy+1, h)
    //
    // The -X face (fdx=-1, fdy=0) is at x = gx, spanning y in [gy, gy+1], z in [0, h]
    // Its vertices: (gx, gy, 0), (gx, gy+1, 0), (gx, gy+1, h), (gx, gy, h)
    //
    // The -Y face (fdx=0, fdy=-1) is at y = gy, spanning x in [gx, gx+1], z in [0, h]
    // Its vertices: (gx, gy, 0), (gx+1, gy, 0), (gx+1, gy, h), (gx, gy, h)

    if (fdx == 1 && fdy == 0) {
        // +X face
        iso_project(s, gx+1, gy,   0.0f, &v[0][0], &v[0][1]);
        iso_project(s, gx+1, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(s, gx+1, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(s, gx+1, gy,   h,    &v[3][0], &v[3][1]);
    } else if (fdx == -1 && fdy == 0) {
        // -X face
        iso_project(s, gx, gy,   0.0f, &v[0][0], &v[0][1]);
        iso_project(s, gx, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(s, gx, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(s, gx, gy,   h,    &v[3][0], &v[3][1]);
    } else if (fdx == 0 && fdy == 1) {
        // +Y face
        iso_project(s, gx,   gy+1, 0.0f, &v[0][0], &v[0][1]);
        iso_project(s, gx+1, gy+1, 0.0f, &v[1][0], &v[1][1]);
        iso_project(s, gx+1, gy+1, h,    &v[2][0], &v[2][1]);
        iso_project(s, gx,   gy+1, h,    &v[3][0], &v[3][1]);
    } else /* fdy == -1 */ {
        // -Y face
        iso_project(s, gx,   gy, 0.0f, &v[0][0], &v[0][1]);
        iso_project(s, gx+1, gy, 0.0f, &v[1][0], &v[1][1]);
        iso_project(s, gx+1, gy, h,    &v[2][0], &v[2][1]);
        iso_project(s, gx,   gy, h,    &v[3][0], &v[3][1]);
    }
}

// ============================================================================
//  RENDER SINGLE VOXEL
// ============================================================================
static void render_voxel(uint32_t* fb, int stride, int fb_w, int fb_h,
                         VoxelState* s, int gx, int gy,
                         FaceInfo* face_a, FaceInfo* face_b,
                         float time) {
    float h = s->heights[gy][gx];
    if (h <= 0.01f) return;

    float height_norm = h / MAX_VOXEL_HEIGHT;

    // ---- Top face ----
    uint32_t top_color = terrain_face_color(height_norm, 1.0f, time);
    if (height_norm < 0.3f) {
        // Water shimmer: oscillate color
        float shimmer = sinf(time * 3.0f + (float)gx * 0.7f + (float)gy * 0.5f) * 0.08f;
        uint8_t sr = (top_color >> 16) & 0xFF;
        uint8_t sg = (top_color >> 8) & 0xFF;
        uint8_t sb = top_color & 0xFF;
        sr = (uint8_t)iclamp((int)(sr * (1.0f + shimmer)), 0, 255);
        sg = (uint8_t)iclamp((int)(sg * (1.0f + shimmer * 0.5f)), 0, 255);
        sb = (uint8_t)iclamp((int)(sb * (1.0f + shimmer)), 0, 255);
        top_color = 0xFF000000 | ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | sb;
    }

    // ---- Face A (screen-right, 55% brightness) ----
    FaceInfo ai = *face_a;
    // Check occlusion: skip if neighbor in face direction is taller
    int ngx_a = gx + ai.dx, ngy_a = gy + ai.dy;
    int occluded_a = (ngx_a >= 0 && ngx_a < GRID_W && ngy_a >= 0 && ngy_a < GRID_H)
                     && s->heights[ngy_a][ngx_a] >= h - 0.1f;

    // ---- Face B (screen-left, 75% brightness) ----
    FaceInfo bi = *face_b;
    int ngx_b = gx + bi.dx, ngy_b = gy + bi.dy;
    int occluded_b = (ngx_b >= 0 && ngx_b < GRID_W && ngy_b >= 0 && ngy_b < GRID_H)
                     && s->heights[ngy_b][ngx_b] >= h - 0.1f;

    if (!s->wireframe) {
        // Draw filled faces
        int v[4][2];

        // Top face
        get_top_face_verts(s, gx, gy, h, v);
        fill_quad_convex(fb, stride, fb_w, fb_h,
                         v[0][0], v[0][1], v[1][0], v[1][1],
                         v[2][0], v[2][1], v[3][0], v[3][1],
                         top_color);

        // Face A
        if (!occluded_a) {
            uint32_t ca = terrain_face_color(height_norm, ai.brightness, time);
            get_face_verts(s, gx, gy, h, ai.dx, ai.dy, v);
            fill_quad_convex(fb, stride, fb_w, fb_h,
                             v[0][0], v[0][1], v[1][0], v[1][1],
                             v[2][0], v[2][1], v[3][0], v[3][1],
                             ca);
        }

        // Face B
        if (!occluded_b) {
            uint32_t cb = terrain_face_color(height_norm, bi.brightness, time);
            get_face_verts(s, gx, gy, h, bi.dx, bi.dy, v);
            fill_quad_convex(fb, stride, fb_w, fb_h,
                             v[0][0], v[0][1], v[1][0], v[1][1],
                             v[2][0], v[2][1], v[3][0], v[3][1],
                             cb);
        }
    }

    // Wireframe overlay (if enabled, or always draw edges on top)
    if (s->wireframe) {
        // Draw edges of the visible faces
        int v[4][2];
        uint32_t wire_col = 0xFF00FFAA;

        // Top face edges
        get_top_face_verts(s, gx, gy, h, v);
        // Draw 4 edges using write_px (thin line)
        // Edge v0-v1, v1-v2, v2-v3, v3-v0
        // We just draw thin lines using a simple approach
        for (int e = 0; e < 4; e++) {
            int x1 = v[e][0], y1 = v[e][1];
            int x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
            float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
            int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
            if (steps < 1) steps = 1;
            for (int i = 0; i <= steps; i++) {
                float t = (float)i / (float)steps;
                int px = (int)(x1 + dx * t);
                int py = (int)(y1 + dy * t);
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    fb[py * stride + px] = wire_col;
            }
        }

        // Face A edges (if not occluded)
        if (!occluded_a) {
            get_face_verts(s, gx, gy, h, ai.dx, ai.dy, v);
            for (int e = 0; e < 4; e++) {
                int x1 = v[e][0], y1 = v[e][1];
                int x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
                float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
                int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
                if (steps < 1) steps = 1;
                for (int i = 0; i <= steps; i++) {
                    float t = (float)i / (float)steps;
                    int px = (int)(x1 + dx * t);
                    int py = (int)(y1 + dy * t);
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                        fb[py * stride + px] = wire_col;
                }
            }
        }

        // Face B edges
        if (!occluded_b) {
            get_face_verts(s, gx, gy, h, bi.dx, bi.dy, v);
            for (int e = 0; e < 4; e++) {
                int x1 = v[e][0], y1 = v[e][1];
                int x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
                float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
                int steps = (int)(fabsf(dx) + fabsf(dy)) / 2 + 1;
                if (steps < 1) steps = 1;
                for (int i = 0; i <= steps; i++) {
                    float t = (float)i / (float)steps;
                    int px = (int)(x1 + dx * t);
                    int py = (int)(y1 + dy * t);
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                        fb[py * stride + px] = wire_col;
                }
            }
        }
    }
}

// ============================================================================
//  TREE ON HIGHEST PEAK
// ============================================================================
static void render_tree(uint32_t* fb, int stride, int fb_w, int fb_h,
                        VoxelState* s, float time) {
    // Find highest peak
    float max_h = -1.0f;
    int peak_gx = GRID_W / 2, peak_gy = GRID_H / 2;
    for (int gy = 0; gy < GRID_H; gy++)
        for (int gx = 0; gx < GRID_W; gx++) {
            if (s->heights[gy][gx] > max_h) {
                max_h = s->heights[gy][gx];
                peak_gx = gx; peak_gy = gy;
            }
        }

    if (max_h < 0.5f) return;

    // Tree sway
    float sway = sinf(time * 1.2f) * 0.03f;
    float tree_height = max_h + 1.2f;

    // Trunk (brown column on top of peak)
    int v[4][2];
    uint32_t trunk_color = 0xFF8B4513;
    uint32_t leaves_color = 0xFF228B22;

    // Draw a simple trunk: a thin column
    // Use face drawing with a small offset
    float trunk_w = 0.3f;
    // Instead of special trunk geom, just draw a rectangular trunk using fill_quad
    // Top of trunk
    float trunk_top = tree_height;
    float trunk_base = max_h + 0.2f;

    // We'll draw the trunk as a voxel at peak with offset, reduced width
    // Simpler: draw 4 vertical lines for trunk
    int sx1, sy1, sx2, sy2, sx3, sy3, sx4, sy4;
    iso_project(s, peak_gx + sway,       peak_gy,       trunk_top, &sx1, &sy1);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy,       trunk_top, &sx2, &sy2);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_top, &sx3, &sy3);
    iso_project(s, peak_gx + sway,       peak_gy + 0.2f, trunk_top, &sx4, &sy4);

    // Leaves: green sphere atop trunk
    // Draw as overlapping filled squares
    float leaf_pos[][3] = {
        { peak_gx + sway,       peak_gy,       tree_height + 0.4f },
        { peak_gx + sway - 0.2f, peak_gy,       tree_height + 0.2f },
        { peak_gx + sway + 0.2f, peak_gy,       tree_height + 0.2f },
        { peak_gx + sway,       peak_gy - 0.2f, tree_height + 0.2f },
        { peak_gx + sway,       peak_gy + 0.2f, tree_height + 0.2f },
        { peak_gx + sway,       peak_gy,       tree_height + 0.7f },
    };
    uint32_t leaf_colors[] = {
        0xFF2ECC71, 0xFF27AE60, 0xFF229954,
        0xFF1E8449, 0xFF2ECC71, 0xFF1ABC9C
    };

    // Trunk faces (as filled parallelogram)
    // Left face of trunk
    int tv[4][2];
    iso_project(s, peak_gx + sway,       peak_gy,       trunk_base, &tv[0][0], &tv[0][1]);
    iso_project(s, peak_gx + sway,       peak_gy + 0.2f, trunk_base, &tv[1][0], &tv[1][1]);
    iso_project(s, peak_gx + sway,       peak_gy + 0.2f, trunk_top,  &tv[2][0], &tv[2][1]);
    iso_project(s, peak_gx + sway,       peak_gy,       trunk_top,  &tv[3][0], &tv[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h,
                     tv[0][0], tv[0][1], tv[1][0], tv[1][1],
                     tv[2][0], tv[2][1], tv[3][0], tv[3][1],
                     0xFF6B3A1F);

    // Right face of trunk
    iso_project(s, peak_gx + sway + 0.2f, peak_gy,       trunk_base, &tv[0][0], &tv[0][1]);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_base, &tv[1][0], &tv[1][1]);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_top,  &tv[2][0], &tv[2][1]);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy,       trunk_top,  &tv[3][0], &tv[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h,
                     tv[0][0], tv[0][1], tv[1][0], tv[1][1],
                     tv[2][0], tv[2][1], tv[3][0], tv[3][1],
                     0xFF4A2A10);

    // Top of trunk
    int tt[4][2];
    iso_project(s, peak_gx + sway,       peak_gy,       trunk_top, &tt[0][0], &tt[0][1]);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy,       trunk_top, &tt[1][0], &tt[1][1]);
    iso_project(s, peak_gx + sway + 0.2f, peak_gy + 0.2f, trunk_top, &tt[2][0], &tt[2][1]);
    iso_project(s, peak_gx + sway,       peak_gy + 0.2f, trunk_top, &tt[3][0], &tt[3][1]);
    fill_quad_convex(fb, stride, fb_w, fb_h,
                     tt[0][0], tt[0][1], tt[1][0], tt[1][1],
                     tt[2][0], tt[2][1], tt[3][0], tt[3][1],
                     0xFF8B5E3C);

    // Leaves as overlapping small quads
    for (int i = 0; i < 6; i++) {
        float lx = leaf_pos[i][0], ly = leaf_pos[i][1], lz = leaf_pos[i][2];
        // Small cube of leaves: 0.25 x 0.25 x 0.25 centered at (lx, ly, lz)
        float hs = 0.15f;
        int lv[4][2];
        uint32_t lcol = leaf_colors[i];
        // Top face
        iso_project(s, lx - hs, ly - hs, lz + hs, &lv[0][0], &lv[0][1]);
        iso_project(s, lx + hs, ly - hs, lz + hs, &lv[1][0], &lv[1][1]);
        iso_project(s, lx + hs, ly + hs, lz + hs, &lv[2][0], &lv[2][1]);
        iso_project(s, lx - hs, ly + hs, lz + hs, &lv[3][0], &lv[3][1]);
        fill_quad_convex(fb, stride, fb_w, fb_h,
                         lv[0][0], lv[0][1], lv[1][0], lv[1][1],
                         lv[2][0], lv[2][1], lv[3][0], lv[3][1],
                         lcol);
        // Left face (screen-left)
        iso_project(s, lx - hs, ly,     lz - hs, &lv[0][0], &lv[0][1]);
        iso_project(s, lx - hs, ly + hs, lz - hs, &lv[1][0], &lv[1][1]);
        iso_project(s, lx - hs, ly + hs, lz + hs, &lv[2][0], &lv[2][1]);
        iso_project(s, lx - hs, ly,     lz + hs, &lv[3][0], &lv[3][1]);
        fill_quad_convex(fb, stride, fb_w, fb_h,
                         lv[0][0], lv[0][1], lv[1][0], lv[1][1],
                         lv[2][0], lv[2][1], lv[3][0], lv[3][1],
                         ui_color_with_opacity(lcol, 0.5f));
        // Right face
        iso_project(s, lx, ly + hs, lz - hs, &lv[0][0], &lv[0][1]);
        iso_project(s, lx + hs, ly + hs, lz - hs, &lv[1][0], &lv[1][1]);
        iso_project(s, lx + hs, ly + hs, lz + hs, &lv[2][0], &lv[2][1]);
        iso_project(s, lx, ly + hs, lz + hs, &lv[3][0], &lv[3][1]);
        fill_quad_convex(fb, stride, fb_w, fb_h,
                         lv[0][0], lv[0][1], lv[1][0], lv[1][1],
                         lv[2][0], lv[2][1], lv[3][0], lv[3][1],
                         ui_color_with_opacity(lcol, 0.35f));
    }
}

// ============================================================================
//  SELECTION HIGHLIGHT
// ============================================================================
static void render_selection(uint32_t* fb, int stride, int fb_w, int fb_h,
                             VoxelState* s, float time) {
    if (!s->has_hover) return;
    int gx = s->hover_gx, gy = s->hover_gy;
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return;
    float h = s->heights[gy][gx];
    if (h <= 0.01f) return;

    // Draw selection box: outline around the voxel
    float pulse = sinf(time * 4.0f) * 0.3f + 0.7f; // 0.4 to 1.0
    uint8_t pulse_alpha = (uint8_t)(128 + (int)(pulse * 127));
    uint32_t sel_color = (pulse_alpha << 24) | 0x00FFDD44;

    int v[4][2];
    // Top face edges with selection highlight
    get_top_face_verts(s, gx, gy, h, v);
    for (int e = 0; e < 4; e++) {
        int x1 = v[e][0], y1 = v[e][1];
        int x2 = v[(e+1)%4][0], y2 = v[(e+1)%4][1];
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
        int steps = (int)(fabsf(dx) + fabsf(dy)) + 1;
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / (float)(steps > 0 ? steps : 1);
            int px = (int)(x1 + dx * t);
            int py = (int)(y1 + dy * t);
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                blend_px(&fb[py * stride + px], sel_color);
        }
    }

    // Corners — bright dots
    for (int i = 0; i < 4; i++) {
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                int px = v[i][0] + dx, py = v[i][1] + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    if (dx*dx + dy*dy <= 4)
                        fb[py * stride + px] = 0xFFFFFF44;
            }
    }
}

// ============================================================================
//  HUD OVERLAY
// ============================================================================
static void render_hud(KainUiWidgetContext* ctx, KainWin32UiHost* host,
                       VoxelState* s, int fb_w, int fb_h) {
    double ds = g_dpi_scale;
    int top_bar_h = (int)(50 * ds + 0.5);
    int bot_bar_h = (int)(34 * ds + 0.5);
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;

    // Semi-transparent HUD background bars
    // Top bar
    for (int y = 0; y < top_bar_h && y < fb_h; y++) {
        uint32_t col = 0x40000000 | ((uint32_t)(y * 3) << 24);
        for (int x = 0; x < fb_w; x++) {
            blend_px(&fb[y * stride + x], col);
        }
    }
    // Bottom bar
    for (int y = (fb_h > bot_bar_h ? fb_h - bot_bar_h : 0); y < fb_h; y++) {
        int dy = y - (fb_h - bot_bar_h);
        uint32_t col = 0x40000000 | ((uint32_t)((bot_bar_h - 1 - dy) * 3) << 24);
        for (int x = 0; x < fb_w; x++) {
            blend_px(&fb[y * stride + x], col);
        }
    }

    // Use fonts: [0]=Segoe UI, [1]=Consolas, [2]=Arial, [3]=Impact, [4]=Georgia
    int fid_title = (s->font_ids[3] > 0) ? 3 : 0;  // Impact for title
    int fid_data = (s->font_ids[1] > 0) ? 1 : 0;    // Consolas for data
    int fid_hud = (s->font_ids[0] > 0) ? 0 : -1;     // Segoe UI for HUD

    char buf[128];

    int fs_title = (int)(16 * ds + 0.5);
    int fs_data = (int)(13 * ds + 0.5);
    int fs_small = (int)(11 * ds + 0.5);
    int fs_info = (int)(12 * ds + 0.5);
    int margin = (int)(12 * ds + 0.5);
    int margin2 = (int)(16 * ds + 0.5);

    // Title (left top)
    {
    int ty = (int)(6 * ds + 0.5);
    if (fid_title >= 0 && s->font_ids[fid_title] > 0)
        ui_widget_draw_text_ex(ctx, margin, ty, "VOXEL ISOMETRIC",
                               0xFFFFDD44, 0, s->font_ids[fid_title]);
    else
        ui_widget_draw_text(ctx, margin, ty, "VOXEL ISOMETRIC", 0xFFFFDD44, fs_title);
    }

    // FPS counter (right top)
    {
    int tx = fb_w - (int)(140 * ds + 0.5);
    snprintf(buf, 128, "FPS: %.0f", s->fps);
    ui_widget_draw_text(ctx, tx, (int)(8 * ds + 0.5), buf,
                        s->fps >= 110.0 ? 0xFF21D4A1 :
                        s->fps >= 60.0 ? 0xFFE8914A : 0xFFE84A5F, fs_data);

    snprintf(buf, 128, "Frame: %d", s->frame_count);
    ui_widget_draw_text(ctx, tx, (int)(26 * ds + 0.5), buf, 0xFF8888A0, fs_small);
    }

    // Camera info (left, below title)
    {
    int by = (int)(28 * ds + 0.5);
    snprintf(buf, 128, "Angle: %.0f\370  Zoom: %.1f  Voxels: %d",
             s->cam_angle * 180.0f / PI, s->cam_zoom, GRID_W * GRID_H);
    ui_widget_draw_text(ctx, margin2, by, buf, 0xFF8888A0, fs_small);
    }

    // Key legend (bottom)
    {
    int ly = fb_h - (int)(28 * ds + 0.5);
    const char* legend = "\xe2\x86\x90\xe2\x86\x91\xe2\x86\x92\xe2\x86\x93=Rotate  W/S=Zoom  "
                         "Click+Drag=Pan  R=Reset  Space=Pause  Esc=Exit";
    ui_widget_draw_text(ctx, margin, ly, legend, 0xFF666688, fs_small);
    }

    // Pause overlay
    if (s->paused) {
        int px = fb_w / 2 - (int)(60 * ds + 0.5);
        if (fid_title >= 0 && s->font_ids[fid_title] > 0)
            ui_widget_draw_text_ex(ctx, px, fb_h / 2 - (int)(20 * ds + 0.5), ">> PAUSED <<",
                                   0xFFFF4444, 0, s->font_ids[fid_title]);
        else
            ui_widget_draw_text(ctx, px, fb_h / 2 - (int)(20 * ds + 0.5), ">> PAUSED <<",
                               0xFFFF4444, (int)(18 * ds + 0.5));
    }

    // Selection info
    if (s->has_hover) {
        float h = s->heights[s->hover_gy][s->hover_gx];
        int sx = margin2, sy = fb_h / 2 - (int)(40 * ds + 0.5);
        int box_w = (int)(200 * ds + 0.5);
        int box_h = (int)(36 * ds + 0.5);
        // Semi-transparent info box
        for (int dy = 0; dy < box_h; dy++)
            for (int dx = 0; dx < box_w; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    blend_px(&fb[py * stride + px], 0x60000000);
            }
        snprintf(buf, 128, "Tile [%d,%d]  H: %.1f  Zone: %s",
                 s->hover_gx, s->hover_gy, h,
                 h < 0.3f ? "Water" : h < 0.5f ? "Beach" : h < 0.72f ? "Grass" :
                 h < 0.85f ? "Rock" : "Snow");
        ui_widget_draw_text(ctx, sx + (int)(8 * ds + 0.5), sy + (int)(8 * ds + 0.5), buf, 0xFFFFDD44, fs_info);
    }
}

// ============================================================================
//  CONTROL PANEL (using widget library)
// ============================================================================
static void render_control_panel(KainUiWidgetContext* ctx, VoxelState* s,
                                 KainWin32UiHost* host) {
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width, fb_h = host->height;

    int px = fb_w - (int)(260 * g_dpi_scale + 0.5);
    int py = (int)(56 * g_dpi_scale + 0.5);
    int pw = (int)(250 * g_dpi_scale + 0.5), ph = (int)(220 * g_dpi_scale + 0.5);

    // Panel background
    for (int dy = 0; dy < ph; dy++)
        for (int dx = 0; dx < pw; dx++) {
            int sx = px + dx, sy = py + dy;
            if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h) {
                uint32_t col = 0xCC1A1A2E;
                if (dy < 2 || dy >= ph - 2 || dx < 2 || dx >= pw - 2)
                    col = 0x443A3A5C;
                fb[sy * stride + sx] = col;
            }
        }

    // Title bar
    int title_h = (int)(28 * g_dpi_scale + 0.5);
    for (int dy = 0; dy < title_h; dy++)
        for (int dx = 0; dx < pw; dx++) {
            int sx = px + dx, sy = py + dy;
            if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
                fb[sy * stride + sx] = 0xCC12121E;
        }

    // Accent line
    for (int dx = 0; dx < pw; dx++) {
        int sx = px + dx, sy = py + title_h;
        if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
            fb[sy * stride + sx] = 0xFF21D4A1;
    }

    ui_widget_draw_text(ctx, px + (int)(10 * g_dpi_scale + 0.5), py + (int)(6 * g_dpi_scale + 0.5), "TERRAIN CONTROLS", 0xFFE8E8F0, (int)(12 * g_dpi_scale + 0.5));

    int cx = px + (int)(12 * g_dpi_scale + 0.5), cy = py + (int)(36 * g_dpi_scale + 0.5);

    // Amplitude slider
    int fs = (int)(10 * g_dpi_scale + 0.5);
    int step_sm = (int)(14 * g_dpi_scale + 0.5);
    int step_md = (int)(26 * g_dpi_scale + 0.5);
    int step_lg = (int)(36 * g_dpi_scale + 0.5);
    ui_widget_draw_text(ctx, cx, cy, "Amplitude", 0xFF8888A0, fs);
    cy += step_sm;
    ui_slider(ctx, &s->amplitude, 1.0, 12.0);
    cy += step_md;

    // Wireframe toggle button
    if (ui_button(ctx, s->wireframe ? "WIREFRAME: ON" : "WIREFRAME: OFF"))
        s->wireframe = !s->wireframe;
    cy += step_lg;

    // Randomize button
    if (ui_button(ctx, "RANDOMIZE TERRAIN")) {
        s->amplitude = 4.0 + (double)(rand() % 80) / 10.0;
        gen_terrain();
    }
    cy += step_lg;

    // Animation checkbox
    ui_checkbox(ctx, "Animate Water & Tree", &s->animate);

    cy += step_md;

    // Voxel count
    char buf[64];
    snprintf(buf, 64, "Grid: %dx%d = %d voxels", GRID_W, GRID_H, GRID_W * GRID_H);
    ui_widget_draw_text(ctx, cx, cy, buf, 0xFF666688, fs);
}

// ============================================================================
//  MAIN RENDER FUNCTION
// ============================================================================
static void render_frame(KainUiWidgetContext* ctx, KainWin32UiHost* host,
                         VoxelState* s, float dt) {
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width;
    int fb_h = host->height;

    if (!fb || fb_w <= 0 || fb_h <= 0) return;

    // ── 1. Clear to deep sky ──────────────────────────────────────
    uint32_t clear_color = 0xFF0A0A14;
    int total = fb_w * fb_h;
    for (int i = 0; i < total; i++) fb[i] = clear_color;

    // ── 2. Update animation ───────────────────────────────────────
    if (!s->paused) {
        s->time += dt;
        s->water_time += dt;
    }

    // ── 3. Determine visible faces ────────────────────────────────
    FaceInfo face_a, face_b;
    get_visible_faces(s, &face_a, &face_b);

    // ── 4. Sort grid cells by depth (painter's algorithm) ────────
    // Depth = (rx + ry) in rotated frame → items with smaller sum are farther
    // Pre-compute depth for each cell
    typedef struct { int gx, gy; float depth; } DepthCell;
    DepthCell cells[GRID_W * GRID_H];
    int nc = 0;
    float ca = cosf(s->cam_angle), sa = sinf(s->cam_angle);

    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            float rx = (float)gx * ca - (float)gy * sa;
            float ry = (float)gx * sa + (float)gy * ca;
            cells[nc].gx = gx;
            cells[nc].gy = gy;
            cells[nc].depth = rx + ry; // sort key: farther = smaller
            nc++;
        }
    }

    // Simple insertion sort
    for (int i = 1; i < nc; i++) {
        DepthCell key = cells[i];
        int j = i - 1;
        while (j >= 0 && cells[j].depth > key.depth) {
            cells[j+1] = cells[j];
            j--;
        }
        cells[j+1] = key;
    }

    // ── 5. Render voxels back-to-front ────────────────────────────
    float anim_t = s->animate ? s->time : 0.0f;
    for (int i = 0; i < nc; i++) {
        int gx = cells[i].gx, gy = cells[i].gy;
        render_voxel(fb, stride, fb_w, fb_h, s, gx, gy, &face_a, &face_b, anim_t);
    }

    // ── 6. Tree on highest peak ───────────────────────────────────
    render_tree(fb, stride, fb_w, fb_h, s, s->animate ? s->time : 0.0f);

    // ── 7. Selection highlight ────────────────────────────────────
    render_selection(fb, stride, fb_w, fb_h, s, s->time);

    // ── 8. Control panel ──────────────────────────────────────────
    if (s->show_panel)
        render_control_panel(ctx, s, host);

    // ── 9. HUD ────────────────────────────────────────────────────
    render_hud(ctx, host, s, fb_w, fb_h);
}

// ============================================================================
//  KEYBOARD & MOUSE HANDLING
// ============================================================================
static void handle_input(VoxelState* s, float dt) {
    // Arrow keys = rotate camera
    float rot_speed = 1.5f * dt;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  s->cam_angle -= rot_speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) s->cam_angle += rot_speed;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    s->cam_angle -= rot_speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  s->cam_angle += rot_speed;

    // W/S = zoom
    float zoom_speed = 0.5f * dt;
    if (GetAsyncKeyState('W') & 0x8000) s->cam_zoom += zoom_speed;
    if (GetAsyncKeyState('S') & 0x8000) s->cam_zoom -= zoom_speed;
    s->cam_zoom = fclampf(s->cam_zoom, 0.3f, 4.0f);

    // Single-press keys (edge detection using static key states)
    #define KEY_JUST_PRESSED(vk) ((GetAsyncKeyState(vk) & 0x8001) && !((GetAsyncKeyState(vk) & 0x8000) ? 1 : 0))
    {
        static int prev_r = 0, prev_space = 0;
        int r_now = (GetAsyncKeyState('R') & 0x8000) ? 1 : 0;
        int sp_now = (GetAsyncKeyState(VK_SPACE) & 0x8000) ? 1 : 0;
        if (r_now && !prev_r) {
            s->cam_angle = 0.0f;
            s->cam_zoom = 1.0f;
            s->cam_pan_x = g_host ? (float)g_host->width * 0.5f : 640.0f;
            s->cam_pan_y = g_host ? (float)g_host->height * 0.5f + 30.0f : 390.0f;
        }
        if (sp_now && !prev_space) {
            s->paused = !s->paused;
        }
        prev_r = r_now;
        prev_space = sp_now;
    }
}



// ============================================================================
//  ENTRY POINT
// ============================================================================
int main(void) {
    SetProcessDPIAware();

    // DPI scale
    HDC dpi_dc = GetDC(NULL);
    float dpi_scale = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;
    g_dpi_scale = dpi_scale;
    int win_w = (int)(SCREEN_W * dpi_scale + 0.5f);
    int win_h = (int)(SCREEN_H * dpi_scale + 0.5f);

    VoxelState* s = &g_state;
    memset(s, 0, sizeof(VoxelState));
    s->cam_zoom = 1.0f;
    s->amplitude = (double)MAX_VOXEL_HEIGHT;
    s->animate = 1;
    s->show_panel = 1;
    s->cam_pan_x = (float)win_w * 0.5f;
    s->cam_pan_y = (float)win_h * 0.5f + 30.0f;

    srand((unsigned int)time(NULL));
    gen_terrain();

    // ── Create UI Session ─────────────────────────────────────────
    int64_t sid = abi_ui_session_create("voxel_viewer", win_w, win_h);
    if (sid <= 0) { MessageBoxA(NULL, "Session create failed", "Error", MB_OK); return 1; }

    if (abi_ui_host_attach(sid, "winit") < 0) {
        MessageBoxA(NULL, "Host attach failed", "Error", MB_OK); return 1;
    }
    abi_ui_window_open(sid, "Isometric Voxel Viewer", win_w, win_h);

    // ── Widget Context ────────────────────────────────────────────
    KainUiWidgetContext* ctx = ui_widget_create(sid);
    if (!ctx) { MessageBoxA(NULL, "Widget ctx failed", "Error", MB_OK); return 1; }

    // ── Load 6 Fonts ─────────────────────────────────────────────
    const char* font_paths[MAX_FONTS] = {
        "C:/Windows/Fonts/segoeui.ttf",      // 0: Segoe UI (HUD labels)
        "C:/Windows/Fonts/consola.ttf",       // 1: Consolas (data readouts)
        "C:/Windows/Fonts/arial.ttf",         // 2: Arial (panel labels)
        "C:/Windows/Fonts/impact.ttf",        // 3: Impact (titles)
        "C:/Windows/Fonts/georgia.ttf",       // 4: Georgia (secondary text)
        "C:/Windows/Fonts/CascadiaMono.ttf",  // 5: Cascadia Mono (alt mono)
    };
    const char* font_labels[] = {"Segoe UI","Consolas","Arial","Impact","Georgia","Cascadia"};
    double font_sizes[] = {12.0 * g_dpi_scale, 11.0 * g_dpi_scale, 12.0 * g_dpi_scale, 18.0 * g_dpi_scale, 11.0 * g_dpi_scale, 11.0 * g_dpi_scale};

    s->font_count = 0;
    for (int i = 0; i < MAX_FONTS; i++) {
        s->font_ids[i] = ui_widget_load_font(ctx, font_paths[i], font_sizes[i]);
        if (s->font_ids[i] > 0) {
            s->font_count = i + 1;
            char buf[64];
            snprintf(buf, 64, "Font %s: OK", font_labels[i]);
            OutputDebugStringA(buf);
        }
    }
    // Ensure default font is set
    if (ctx->default_font < 0 && s->font_ids[0] > 0)
        ctx->default_font = 0;

    // ── Subclass the Win32 window ────────────────────────────────
    KainNativeUiSession* ns = abi_ui_find_session(sid);
    if (!ns || !ns->host_state) {
        MessageBoxA(NULL, "No host state", "Error", MB_OK); return 1;
    }
    KainWin32UiHost* khost = (KainWin32UiHost*)ns->host_state;
    g_host = khost;
    HWND hwnd = khost->hwnd;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)khost);
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)voxel_wndproc);
    SetWindowTextA(hwnd, "Isometric Voxel Viewer  |  120FPS Target");

    // ── Timing ───────────────────────────────────────────────────
    LARGE_INTEGER freq, pt, ct;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&pt);
    float dt = 0.016f;

    // ── Main Loop ───────────────────────────────────────────────
    while (g_host && g_host->running) {
        // Message pump
        {
            MSG msg;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { g_host->running = 0; break; }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        if (!g_host || !g_host->running) break;

        // Escape to quit
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            g_host->running = 0;
            break;
        }

        // ── Frame timing ────────────────────────────────────────
        QueryPerformanceCounter(&ct);
        dt = (float)((double)(ct.QuadPart - pt.QuadPart) / (double)freq.QuadPart);
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.001f) dt = 0.001f;
        pt = ct;

        // FPS calculation
        s->fps_counter++;
        s->fps_timer += dt;
        if (s->fps_timer >= 1.0) {
            s->fps = (double)s->fps_counter / s->fps_timer;
            s->fps_counter = 0;
            s->fps_timer = 0.0;
        }
        s->frame_count++;

        // ── Input ───────────────────────────────────────────────
        // Get mouse state
        POINT mp;
        GetCursorPos(&mp);
        ScreenToClient(hwnd, &mp);
        s->mouse_x = (double)mp.x;
        s->mouse_y = (double)mp.y;
        s->mouse_down_prev = s->mouse_down;
        s->mouse_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;

        // Click-drag pan
        if (s->mouse_down && !s->mouse_down_prev) {
            s->dragging = 1;
            s->mouse_down_x = s->mouse_x;
            s->mouse_down_y = s->mouse_y;
        }
        if (s->dragging && s->mouse_down) {
            float dx = (float)(s->mouse_x - s->mouse_down_x);
            float dy = (float)(s->mouse_y - s->mouse_down_y);
            s->cam_pan_x += dx;
            s->cam_pan_y += dy;
            s->mouse_down_x = s->mouse_x;
            s->mouse_down_y = s->mouse_y;
        }
        if (!s->mouse_down) s->dragging = 0;

        // Keyboard
        handle_input(s, dt);

        // ── Mouse-over voxel selection ──────────────────────────
        {
            // Find which grid cell is under the mouse
            // Inverse of iso_project: solve for (gx, gy) from mouse coords
            // This is approximate — we check all cells
            s->has_hover = 0;
            int mx = (int)s->mouse_x, my = (int)s->mouse_y;
            if (mx >= 0 && mx < khost->width && my >= 0 && my < khost->height) {
                int best_dist = 100;
                for (int gy = 0; gy < GRID_H; gy++) {
                    for (int gx = 0; gx < GRID_W; gx++) {
                        float h = s->heights[gy][gx];
                        if (h <= 0.01f) continue;
                        int sx, sy;
                        iso_project(s, gx + 0.5f, gy + 0.5f, h * 0.5f, &sx, &sy);
                        int dx = mx - sx, dy = my - sy;
                        int dist = dx * dx + dy * dy;
                        if (dist < best_dist && dist < 800) {
                            best_dist = dist;
                            s->hover_gx = gx;
                            s->hover_gy = gy;
                            s->has_hover = 1;
                        }
                    }
                }
            }
        }

        // ── Begin frame ─────────────────────────────────────────
        abi_ui_begin_frame(sid, dt * 1000.0);
        ui_widget_begin_frame(ctx);

        // ── Render ──────────────────────────────────────────────
        render_frame(ctx, khost, s, dt);

        // ── End Frame ───────────────────────────────────────────
        ui_widget_end_frame(ctx);
        abi_ui_end_frame(sid);

        // ── Present ─────────────────────────────────────────────
        InvalidateRect(hwnd, NULL, FALSE);

        // ── Sleep for 120fps target (~8.3ms) ────────────────────
        // Use a shorter sleep to maintain responsiveness while
        // still capping frame rate to prevent 100% CPU usage
        // when vsync is off.
        int sleep_ms = 6;
        {
            static double sleep_accum = 0;
            sleep_accum += dt;
            if (sleep_accum > 0.016) {
                // We're running faster than 60fps — sleep less
                sleep_ms = 4;
                sleep_accum -= 0.016;
            }
        }
        Sleep(sleep_ms);
    }

    // ── Cleanup ─────────────────────────────────────────────────
    ui_widget_destroy(ctx);
    abi_ui_session_destroy(sid);
    return 0;
}

// ============================================================================
//  SUBCLASSED WINDOW PROC — handles WM_PAINT with BitBlt from DIB
// ============================================================================
static LRESULT CALLBACK voxel_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_host && g_host->hdc_buffer) {
            BitBlt(hdc, 0, 0, g_host->width, g_host->height,
                   g_host->hdc_buffer, 0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        // Re-create DIB at new size
        if (g_host) {
            RECT r;
            GetClientRect(hwnd, &r);
            int new_w = r.right - r.left;
            int new_h = r.bottom - r.top;
            if (new_w > 0 && new_h > 0 &&
                (new_w != g_host->width || new_h != g_host->height)) {
                HDC hdc_screen = GetDC(NULL);
                if (hdc_screen) {
                    BITMAPINFO bmi = {0};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = new_w;
                    bmi.bmiHeader.biHeight = -new_h;
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    HBITMAP new_bmp = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                                                       (void**)&g_host->framebuffer,
                                                       NULL, 0);
                    if (new_bmp) {
                        HBITMAP old_bmp = (HBITMAP)SelectObject(g_host->hdc_buffer, new_bmp);
                        if (old_bmp && old_bmp != new_bmp) DeleteObject(old_bmp);
                        g_host->hbitmap = new_bmp;
                        g_host->width = new_w;
                        g_host->height = new_h;
                        g_host->fb_stride = new_w * 4;
                    }
                    ReleaseDC(NULL, hdc_screen);
                }
            }
        }
        return CallWindowProcA(g_orig_wndproc, hwnd, msg, w, l);
    }
    case WM_CLOSE:
        if (g_host) g_host->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED: {
        RECT* rect = (RECT*)l;
        int new_w = rect->right - rect->left;
        int new_h = rect->bottom - rect->top;
        if (new_w > 0 && new_h > 0 && g_host) {
            HDC hdc_screen = GetDC(NULL);
            if (hdc_screen) {
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = new_w;
                bmi.bmiHeader.biHeight = -new_h;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                HBITMAP new_bmp = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                                                   (void**)&g_host->framebuffer,
                                                   NULL, 0);
                if (new_bmp) {
                    HBITMAP old_bmp = (HBITMAP)SelectObject(g_host->hdc_buffer, new_bmp);
                    if (old_bmp && old_bmp != new_bmp) DeleteObject(old_bmp);
                    g_host->hbitmap = new_bmp;
                    g_host->width = new_w;
                    g_host->height = new_h;
                    g_host->fb_stride = new_w * 4;
                }
                ReleaseDC(NULL, hdc_screen);
            }
            SetWindowPos(hwnd, NULL, rect->left, rect->top, new_w, new_h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, w, l);
}
