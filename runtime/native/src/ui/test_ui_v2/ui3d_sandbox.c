// ============================================================================
//  ui3d_sandbox.c — 3D UI SANDBOX
//  ============================================================================
//  A completely original 3D experiment that pushes the Kain 2D software
//  framebuffer to do things it was never designed for:
//
//    - Rotating 3D wireframe/face-colored cube with depth-sorted faces
//    - Isometric grid floor with checkerboard alternating colors
//    - Floating 3D UI panels with perspective transform (depth = scale)
//    - Animated starfield background with parallax depth cue
//    - Particle fountain with gravity, colors, lifetime
//    - Interactive sliders, buttons, checkboxes drawn directly
//    - 4+ fonts from C:/Windows/Fonts/ for rich typography
//    - Keyboard controls (R, W, Space, 1-4, Esc)
//
//  Build:
//    clang -std=c11 -g -O0 ui3d_sandbox.c ..\widgets\stubs.c ^
//      ..\widgets\ui_widget.c ..\ui_system.c ..\ui_host_adapter.c ^
//      ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ..\..\core\component_surface.c ^
//      -I ..\..\..\include -I .. -I ..\..\core -I ..\..\..\extras\_stb-truetype ^
//      -luser32 -lgdi32 -lopengl32 -o ui3d_sandbox.exe
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

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"
#include "../../include/ui_font.h"
#include "../widgets/ui_widget.h"

// ── KainWin32UiHost (must match ui_host_adapter.c exactly) ──────
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

// ── Global state for wndproc access ────────────────────────────
static KainWin32UiHost* g_sandbox_host = NULL;
static WNDPROC g_orig_sandbox_wndproc = NULL;

// ── Stubs from core.c ────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ============================================================================
//  3D MATH — pure C, zero deps
// ============================================================================
typedef struct { double x, y, z; } Vec3;

static Vec3 vec3(double x, double y, double z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

static Vec3 rotate_x(Vec3 v, double a) {
    double c = cos(a), s = sin(a);
    return vec3(v.x, v.y * c - v.z * s, v.y * s + v.z * c);
}

static Vec3 rotate_y(Vec3 v, double a) {
    double c = cos(a), s = sin(a);
    return vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

static Vec3 rotate_z(Vec3 v, double a) {
    double c = cos(a), s = sin(a);
    return vec3(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
}

/* Compose all three rotations */
static Vec3 rotate_all(Vec3 v, double rx, double ry, double rz) {
    return rotate_z(rotate_y(rotate_x(v, rx), ry), rz);
}

/* Perspective projection: 3D → screen coords */
static void project(Vec3 v, double focal, int sw, int sh,
                    int* out_x, int* out_y, double scale_mul) {
    double d = focal + v.z;
    if (d < 0.01) d = 0.01;
    double s = focal / d * scale_mul;
    *out_x = (int)(sw / 2.0 + v.x * s);
    *out_y = (int)(sh / 2.0 - v.y * s);
}

/* Orthographic projection */
static void project_ortho(Vec3 v, int sw, int sh,
                          int* out_x, int* out_y, double scale_mul) {
    *out_x = (int)(sw / 2.0 + v.x * scale_mul);
    *out_y = (int)(sh / 2.0 - v.y * scale_mul);
}

/* Simple integer clamp */
static int iclamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Double clamp */
static double dclamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ============================================================================
//  BRESENHAM LINE — clipped to framebuffer bounds
// ============================================================================
static void draw_line_clip(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    int x = x1, y = y1;
    for (;;) {
        if (x >= 0 && x < fb_w && y >= 0 && y < fb_h)
            fb[y * stride + x] = color;
        if (x == x2 && y == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

// ============================================================================
//  FILLED CONVEX QUAD — scanline fill with edge walking
// ============================================================================
/* Fill a convex quadrilateral defined by 4 2D points.
 * Uses a simple active-edge approach: sort vertices by Y, then fill spans.
 * Works correctly for convex quads (cube faces after projection). */
static void fill_quad(uint32_t* fb, int stride, int fb_w, int fb_h,
                      int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3,
                      uint32_t color) {
    /* Collect and sort edges by Y min */
    /* We use a simple scanline fill — walk each Y from min to max,
     * compute X intersections with all 4 edges, sort, fill spans. */
    int min_y = y0, max_y = y0;
    int verts[4][2] = {{x0,y0},{x1,y1},{x2,y2},{x3,y3}};
    for (int i = 0; i < 4; i++) {
        if (verts[i][1] < min_y) min_y = verts[i][1];
        if (verts[i][1] > max_y) max_y = verts[i][1];
    }
    min_y = iclamp(min_y, 0, fb_h - 1);
    max_y = iclamp(max_y, 0, fb_h - 1);
    if (min_y >= max_y) return;

    for (int py = min_y; py <= max_y; py++) {
        /* Find intersections with each edge */
        double ix[4];
        int nix = 0;
        for (int e = 0; e < 4; e++) {
            int e1 = e, e2 = (e + 1) % 4;
            int y1 = verts[e1][1], y2 = verts[e2][1];
            if (y1 == y2) continue; /* horizontal edge — skip */
            if ((py < y1 && py < y2) || (py > y1 && py > y2)) continue;
            double t = (double)(py - y1) / (double)(y2 - y1);
            double xi = verts[e1][0] + t * (verts[e2][0] - verts[e1][0]);
            if (nix < 4) ix[nix++] = xi;
        }
        if (nix < 2) continue;

        /* Sort intersections */
        for (int i = 0; i < nix - 1; i++) {
            for (int j = i + 1; j < nix; j++) {
                if (ix[j] < ix[i]) {
                    double t = ix[i]; ix[i] = ix[j]; ix[j] = t;
                }
            }
        }

        /* Fill spans between pairs */
        for (int i = 0; i + 1 < nix; i += 2) {
            int x_lo = (int)(ix[i] + 0.5);
            int x_hi = (int)(ix[i + 1] + 0.5);
            x_lo = iclamp(x_lo, 0, fb_w - 1);
            x_hi = iclamp(x_hi, 0, fb_w - 1);
            uint32_t* row = fb + py * stride;
            for (int px = x_lo; px <= x_hi; px++)
                row[px] = ui_color_blend(color, row[px]);
        }
    }
}

// ============================================================================
//  APPLICATION STATE
// ============================================================================
#define MAX_PARTICLES 200
#define MAX_STARS     120
#define MAX_LOG        16
#define FONT_COUNT      4

typedef struct {
    double px, py, pz;      /* position (world space) */
    double vx, vy, vz;      /* velocity */
    uint32_t color;         /* 0xAARRGGBB */
    double lifetime;         /* remaining seconds */
} Particle;

typedef struct {
    double x, y, z;          /* 3D position */
    uint8_t brightness;      /* 0-255 */
} Star;

typedef struct {
    /* Cube */
    double rot_x, rot_y, rot_z;
    double speed_x, speed_y, speed_z;
    double cube_size;
    int wireframe_mode;
    int explode_mode;
    double explode_t;

    /* Scene */
    int show_stars;
    int paused;
    int panel_visible[4];
    int ortho_mode;

    /* Stars */
    Star stars[MAX_STARS];
    double star_phase;

    /* Particles */
    Particle particles[MAX_PARTICLES];
    int particle_count;

    /* Log ring buffer */
    char log_lines[MAX_LOG][128];
    int log_count;

    /* Perf */
    double fps;
    int frame_count;
    int fps_counter;
    double fps_timer;

    /* Font resource IDs loaded via widget */
    int64_t font_ids[FONT_COUNT];

    /* Cube face colors (6 faces, back/front/bottom/top/left/right) */
    uint32_t face_colors[6];

    /* Color palette cycling */
    int palette_idx;
} AppState;

static AppState g_state;

// ============================================================================
//  LOGGING
// ============================================================================
static void log_msg(const char* msg) {
    AppState* s = &g_state;
    if (s->log_count < MAX_LOG) {
        strncpy(s->log_lines[s->log_count], msg, 127);
        s->log_lines[s->log_count][127] = '\0';
        s->log_count++;
    } else {
        for (int i = 0; i < MAX_LOG - 1; i++)
            memcpy(s->log_lines[i], s->log_lines[i + 1], 128);
        strncpy(s->log_lines[MAX_LOG - 1], msg, 127);
        s->log_lines[MAX_LOG - 1][127] = '\0';
    }
}

// ============================================================================
//  STARFIELD
// ============================================================================
static void init_stars(void) {
    AppState* s = &g_state;
    srand(42);
    for (int i = 0; i < MAX_STARS; i++) {
        s->stars[i].x = (double)(rand() % 4000 - 2000) / 10.0;
        s->stars[i].y = (double)(rand() % 4000 - 2000) / 10.0;
        s->stars[i].z = (double)(rand() % 2000 + 200) / 10.0;
        s->stars[i].brightness = (uint8_t)(rand() % 156 + 100);
    }
}

static void draw_stars(uint32_t* fb, int stride, int fb_w, int fb_h, double dt) {
    AppState* s = &g_state;
    if (!s->show_stars) return;

    s->star_phase += dt * 0.015;
    double rot = s->star_phase;
    int cx = fb_w / 2, cy = fb_h / 2;

    for (int i = 0; i < MAX_STARS; i++) {
        Star* star = &s->stars[i];
        double rx = star->x * cos(rot) - star->z * sin(rot);
        double rz = star->x * sin(rot) + star->z * cos(rot);
        if (rz <= 0.5) continue;
        double scale = 12.0 / rz;
        int sx = (int)(cx + rx * scale * 4);
        int sy = (int)(cy - star->y * scale * 4);
        if (sx < 0 || sx >= fb_w || sy < 0 || sy >= fb_h) continue;

        int size = (rz < 30.0) ? 3 : (rz < 70.0) ? 2 : 1;
        double bright_factor = dclamp(1.0 - rz / 150.0, 0.2, 1.0);
        uint8_t br = (uint8_t)(star->brightness * bright_factor);
        uint32_t col = (0xFF << 24) | ((uint32_t)br << 16) | ((uint32_t)br << 8) | br;

        for (int dy = 0; dy < size; dy++)
            for (int dx = 0; dx < size; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    fb[py * stride + px] = col;
            }
    }
}

// ============================================================================
//  ISOMETRIC GRID FLOOR
// ============================================================================
static void draw_isometric_grid(uint32_t* fb, int stride, int fb_w, int fb_h,
                                 double time) {
    (void)time;
    int cx = fb_w / 2;
    int base_y = fb_h - 30;
    int vanish_y = fb_h / 3;

    /* Radiating floor lines from vanishing point */
    uint32_t col_a = 0xFF222238;
    uint32_t col_b = 0xFF18182A;
    int n_radial = 36;
    for (int i = -n_radial / 2; i <= n_radial / 2; i++) {
        if (i == 0) continue;
        double t = (double)i / (double)(n_radial / 2);
        int bx = cx + (int)(t * 350.0);
        int vx = cx + (int)(t * 25.0);
        uint32_t c = (abs(i) % 2 == 0) ? col_a : col_b;
        draw_line_clip(fb, stride, fb_w, fb_h, bx, base_y, vx, vanish_y, c);
    }

    /* Horizontal depth lines (exponential spacing) */
    for (int i = 0; i <= 22; i++) {
        double t = (double)i / 22.0;
        int y = base_y - (int)((double)(base_y - vanish_y) * (1.0 - pow(1.0 - t, 2.5)));
        if (y < vanish_y) y = vanish_y;
        int half_w = (int)(350.0 * (1.0 - t));
        if (half_w < 3) half_w = 3;
        uint32_t c = (i % 2 == 0) ? col_a : col_b;
        draw_line_clip(fb, stride, fb_w, fb_h, cx - half_w, y,
                       cx + half_w, y, c);
    }

    /* Bright center dash */
    draw_line_clip(fb, stride, fb_w, fb_h, cx, base_y, cx, vanish_y, 0xFF3A3A5C);
}

// ============================================================================
//  3D CUBE GEOMETRY
// ============================================================================
/* 8 vertices of a unit cube centered at origin */
static const Vec3 g_cube_verts[8] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},  /* back face */
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}   /* front face */
};

/* 12 edges as pairs of vertex indices */
static const int g_cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},  /* back */
    {4,5},{5,6},{6,7},{7,4},  /* front */
    {0,4},{1,5},{2,6},{3,7}   /* connecting */
};

/* 6 faces as quads (winding: CCW outward) */
static const int g_cube_faces[6][4] = {
    {0,1,2,3},  /* back  (-Z) */
    {4,5,6,7},  /* front (+Z) */
    {0,1,5,4},  /* bottom (-Y) */
    {2,3,7,6},  /* top   (+Y) */
    {0,3,7,4},  /* left  (-X) */
    {1,2,6,5}   /* right (+X) */
};

/* Face normals (for back-face culling) */
static const Vec3 g_face_normals[6] = {
    { 0, 0,-1}, { 0, 0, 1}, { 0,-1, 0},
    { 0, 1, 0}, {-1, 0, 0}, { 1, 0, 0}
};

/* Center of each face (used for explode offset) */
static const Vec3 g_face_centers[6] = {
    { 0, 0,-1}, { 0, 0, 1}, { 0,-1, 0},
    { 0, 1, 0}, {-1, 0, 0}, { 1, 0, 0}
};

/* Active face colors — initialized from palettes */
static uint32_t g_face_colors[6];

static void draw_cube(uint32_t* fb, int stride, int fb_w, int fb_h, double dt) {
    AppState* s = &g_state;

    if (!s->paused) {
        s->rot_x += s->speed_x * dt;
        s->rot_y += s->speed_y * dt;
        s->rot_z += s->speed_z * dt;
    }

    double size = s->cube_size;
    double focal = 3.0;
    int cx = fb_w / 2;
    int cy = fb_h / 2 - 15;
    double pix_scale = size * 60.0;

    /* Transform vertices */
    Vec3 tv[8];
    int sx[8], sy[8];
    for (int i = 0; i < 8; i++) {
        tv[i] = rotate_all(g_cube_verts[i], s->rot_x, s->rot_y, s->rot_z);
        if (s->ortho_mode)
            project_ortho(tv[i], cx * 2, cy * 2, &sx[i], &sy[i], pix_scale);
        else
            project(tv[i], focal, cx * 2, cy * 2, &sx[i], &sy[i], pix_scale);
    }

    /* ---- Draw filled faces (painter's algorithm, back to front) ---- */
    if (!s->wireframe_mode || s->explode_mode) {
        /* Compute depth (average Z of face) for sorting */
        typedef struct { int idx; double z; } FZ;
        FZ fz[6];
        for (int f = 0; f < 6; f++) {
            double az = 0;
            for (int j = 0; j < 4; j++) az += tv[g_cube_faces[f][j]].z;
            fz[f].idx = f;
            fz[f].z = az / 4.0;
        }
        /* Sort faces back-to-front (descending Z) — bubble sort, 6 items */
        for (int i = 0; i < 6; i++)
            for (int j = i + 1; j < 6; j++)
                if (fz[j].z > fz[i].z) {
                    FZ t = fz[i]; fz[i] = fz[j]; fz[j] = t;
                }

        /* Draw sorted faces */
        for (int fi = 0; fi < 6; fi++) {
            int idx = fz[fi].idx;
            const int* quad = g_cube_faces[idx];
            uint32_t col = g_face_colors[idx];

            if (s->explode_mode) {
                /* Explode: offset face by its normal * explode_t */
                double et = s->explode_t;
                double ox = g_face_centers[idx].x * et * 1.5;
                double oy = g_face_centers[idx].y * et * 1.5;
                double oz = g_face_centers[idx].z * et * 1.5;

                int ex[4], ey[4];
                for (int j = 0; j < 4; j++) {
                    Vec3 ov = tv[quad[j]];
                    ov.x += ox; ov.y += oy; ov.z += oz;
                    if (s->ortho_mode)
                        project_ortho(ov, cx * 2, cy * 2, &ex[j], &ey[j], pix_scale);
                    else
                        project(ov, focal, cx * 2, cy * 2, &ex[j], &ey[j], pix_scale);
                }
                fill_quad(fb, stride, fb_w, fb_h,
                          ex[0], ey[0], ex[1], ey[1],
                          ex[2], ey[2], ex[3], ey[3], col);
            } else {
                fill_quad(fb, stride, fb_w, fb_h,
                          sx[quad[0]], sy[quad[0]],
                          sx[quad[1]], sy[quad[1]],
                          sx[quad[2]], sy[quad[2]],
                          sx[quad[3]], sy[quad[3]], col);
            }
        }
    }

    /* ---- Draw wireframe ---- */
    uint32_t wire_col = s->wireframe_mode ? 0xFF00FFAA : 0xFFE8E8F0;
    for (int e = 0; e < 12; e++) {
        int v1 = g_cube_edges[e][0], v2 = g_cube_edges[e][1];

        if (s->explode_mode) {
            double et = s->explode_t;
            /* Each edge belongs to one or two faces — offset by average */
            Vec3 ov1 = tv[v1], ov2 = tv[v2];
            double ox = sin(s->rot_x + e * 2.17) * et * 1.5;
            double oy = cos(s->rot_y + e * 1.31) * et * 1.5;
            double oz = sin(s->rot_z + e * 3.73) * et * 1.5;
            ov1.x += ox; ov1.y += oy; ov1.z += oz;
            ov2.x += ox; ov2.y += oy; ov2.z += oz;
            int ex1, ey1, ex2, ey2;
            if (s->ortho_mode) {
                project_ortho(ov1, cx * 2, cy * 2, &ex1, &ey1, pix_scale);
                project_ortho(ov2, cx * 2, cy * 2, &ex2, &ey2, pix_scale);
            } else {
                project(ov1, focal, cx * 2, cy * 2, &ex1, &ey1, pix_scale);
                project(ov2, focal, cx * 2, cy * 2, &ex2, &ey2, pix_scale);
            }
            draw_line_clip(fb, stride, fb_w, fb_h, ex1, ey1, ex2, ey2, wire_col);
        } else {
            draw_line_clip(fb, stride, fb_w, fb_h,
                           sx[v1], sy[v1], sx[v2], sy[v2], wire_col);
        }
    }

    /* ---- Draw vertex highlights ---- */
    for (int i = 0; i < 8; i++) {
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                if (dx * dx + dy * dy <= 4) {
                    int px = sx[i] + dx, py = sy[i] + dy;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                        fb[py * stride + px] = 0xFFFFDD44;
                }
    }
}

// ============================================================================
//  PARTICLES
// ============================================================================
static void spawn_fountain(void) {
    AppState* s = &g_state;
    srand((unsigned int)(GetTickCount64() & 0x7FFFFFFF));
    int count = 50;
    for (int i = 0; i < count && s->particle_count < MAX_PARTICLES; i++) {
        Particle* p = &s->particles[s->particle_count];
        p->px = 0; p->py = 0; p->pz = 0;
        p->vx = (double)(rand() % 400 - 200) / 100.0;
        p->vy = (double)(rand() % 200 + 80) / 100.0 * 4.0;
        p->vz = (double)(rand() % 400 - 200) / 100.0;
        uint8_t r = (uint8_t)(rand() % 256);
        uint8_t g = (uint8_t)(rand() % 256);
        uint8_t b = (uint8_t)(rand() % 256);
        p->color = (0xFF << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        p->lifetime = 1.5 + (double)(rand() % 100) / 100.0 * 2.0;
        s->particle_count++;
    }
    log_msg("Fountain spawned!");
}

static void update_particles(double dt) {
    AppState* s = &g_state;
    double gravity = -8.0;
    for (int i = 0; i < s->particle_count; ) {
        Particle* p = &s->particles[i];
        if (!s->paused) {
            p->vx += (double)(rand() % 100 - 50) / 500.0;
            p->vy += gravity * dt;
            p->px += p->vx * dt * 50.0;
            p->py += p->vy * dt * 50.0;
            p->pz += p->vz * dt * 50.0;
            p->lifetime -= dt;
        }
        if (p->lifetime <= 0.0) {
            s->particles[i] = s->particles[--s->particle_count];
        } else {
            i++;
        }
    }
}

static void draw_particles(uint32_t* fb, int stride, int fb_w, int fb_h) {
    AppState* s = &g_state;
    double focal = 3.0;
    double pix_scale = 50.0;
    int cx = fb_w / 2, cy = fb_h / 2 - 15;

    for (int i = 0; i < s->particle_count; i++) {
        Particle* p = &s->particles[i];
        Vec3 wv = vec3(p->px, p->py, p->pz);
        int sx, sy;
        if (s->ortho_mode)
            project_ortho(wv, cx * 2, cy * 2, &sx, &sy, pix_scale * 0.3);
        else
            project(wv, focal, cx * 2, cy * 2, &sx, &sy, pix_scale * 0.3);

        int size = s->ortho_mode ? 2 : (int)(2.0 * focal / (focal + p->pz));
        if (size < 1) size = 1; if (size > 5) size = 5;

        uint32_t col = ui_color_with_opacity(p->color,
                        dclamp(p->lifetime / 2.0, 0.0, 1.0));

        for (int dy = -size; dy <= size; dy++)
            for (int dx = -size; dx <= size; dx++)
                if (dx * dx + dy * dy <= size * size) {
                    int px = sx + dx, py = sy + dy;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                        fb[py * stride + px] = ui_color_blend(col, fb[py * stride + px]);
                }
    }
}

// ============================================================================
//  CUSTOM WIDGET PRIMITIVES (framebuffer direct)
// ============================================================================

/* ── Button ────────────────────────────────────────────────────────── */
static int btn_widget(KainUiWidgetContext* ctx,
                      int x, int y, int w, int h,
                      const char* label, int* pressed_state) {
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width, fb_h = ctx->host->height;

    int hovered = (ctx->mouse_x >= x && ctx->mouse_x < x + w &&
                   ctx->mouse_y >= y && ctx->mouse_y < y + h);
    int clicked = 0;

    if (ctx->mouse_down && !*pressed_state && hovered) *pressed_state = 1;
    if (!ctx->mouse_down && *pressed_state) {
        if (hovered) clicked = 1;
        *pressed_state = 0;
    }

    uint32_t bg = *pressed_state ? 0xFF505080 :
                  hovered        ? 0xFF404068 : 0xFF303050;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, bg, 4);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0x153A3A5C, 4);

    /* Bottom accent line */
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + h - 2, w - 8, 2, 0xFF21D4A1);

    ui_widget_draw_text_centered(ctx, x, y, w, h, label, 0xFFE8E8F0, 13);
    return clicked;
}

/* ── Slider ────────────────────────────────────────────────────────── */
static int slider_widget(KainUiWidgetContext* ctx,
                         int x, int y, int w, int h,
                         double* value, double lo, double hi,
                         int* drag_active) {
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width, fb_h = ctx->host->height;
    double range = (hi > lo) ? hi - lo : 1.0;
    double norm = dclamp((*value - lo) / range, 0.0, 1.0);
    int track_h = 6;
    int track_y = y + h / 2 - track_h / 2;
    int thumb_w = 10, thumb_h = 16;
    int thumb_x = x + (int)(norm * (double)(w - thumb_w));
    int thumb_y = track_y + track_h / 2 - thumb_h / 2;

    int changed = 0;

    if (!*drag_active && ctx->mouse_down && !ctx->mouse_down_prev &&
        ctx->mouse_x >= thumb_x && ctx->mouse_x < thumb_x + thumb_w &&
        ctx->mouse_y >= thumb_y && ctx->mouse_y < thumb_y + thumb_h)
        *drag_active = 1;

    if (*drag_active && ctx->mouse_down) {
        double nn = (ctx->mouse_x - x) / (double)(w - thumb_w);
        nn = dclamp(nn, 0.0, 1.0);
        double nv = lo + nn * range;
        if (fabs(nv - *value) > 0.001) { *value = nv; changed = 1; }
    }

    if (!ctx->mouse_down) *drag_active = 0;

    /* Track bg */
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, track_y, w, track_h, 0xFF2A2A44, 3);
    /* Fill */
    int fill_w = thumb_x - x;
    if (fill_w > 0)
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, track_y, fill_w, track_h, 0xFF21D4A1, 3);
    /* Thumb */
    uint32_t tc = *drag_active ? 0xFF21D4A1 : 0xFF4A90D9;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, thumb_x, thumb_y, thumb_w, thumb_h, tc, 4);

    return changed;
}

/* ── Checkbox ──────────────────────────────────────────────────────── */
static int checkbox_widget(KainUiWidgetContext* ctx,
                           int x, int y, const char* label, int* value,
                           int* pressed_state) {
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width, fb_h = ctx->host->height;
    int ch = 14;
    int hovered = (ctx->mouse_x >= x && ctx->mouse_x < x + ch + 120 &&
                   ctx->mouse_y >= y && ctx->mouse_y < y + ch + 4);
    int toggled = 0;

    if (ctx->mouse_down && !*pressed_state && hovered) *pressed_state = 1;
    if (!ctx->mouse_down && *pressed_state) {
        if (hovered) { *value = !*value; toggled = 1; }
        *pressed_state = 0;
    }

    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, ch, ch,
                                 *value ? 0xFF21D4A1 : 0xFF303050, 3);
    if (*value) {
        /* Checkmark */
        for (int i = 0; i < 4; i++) {
            int px = x + 3 + i, py = y + 6 + i;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                fb[py * stride + px] = 0xFFFFFFFF;
        }
        for (int i = 0; i < 5; i++) {
            int px = x + 5 + i, py = y + 10 - i;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                fb[py * stride + px] = 0xFFFFFFFF;
        }
    }
    ui_widget_draw_text(ctx, x + ch + 8, y - 1, label, 0xFFE8E8F0, 11);
    return toggled;
}

// ============================================================================
//  KEYBOARD HANDLING
// ============================================================================
static void handle_keyboard(void) {
    AppState* s = &g_state;
    static int prev[512];

#define KEY(vk) do { \
    int n = (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0; \
    int p = prev[vk]; \
    if (n && !p) { /* key just pressed */  /* action set in switch */ } \
    prev[vk] = n; \
} while(0)

    KEY('R'); if ((GetAsyncKeyState('R')&0x8001) && !prev['R']) { /* already handled via just-pressed logic */ }
    /* Re-check with just-pressed detection */
    {
        int nR = (GetAsyncKeyState('R') & 0x8000) ? 1 : 0;
        if (nR && !prev[256]) { s->rot_x = 0; s->rot_y = 0; s->rot_z = 0; log_msg("Rotation reset"); }
        prev[256] = nR;
    }
    {
        int nW = (GetAsyncKeyState('W') & 0x8000) ? 1 : 0;
        if (nW && !prev[257]) { s->wireframe_mode = !s->wireframe_mode; log_msg(s->wireframe_mode ? "Wireframe ON" : "Wireframe OFF"); }
        prev[257] = nW;
    }
    {
        int nS = (GetAsyncKeyState(VK_SPACE) & 0x8000) ? 1 : 0;
        if (nS && !prev[258]) { s->paused = !s->paused; log_msg(s->paused ? "PAUSED" : "Resumed"); }
        prev[258] = nS;
    }
    for (int i = 0; i < 4; i++) {
        int vk = '1' + i;
        int n = (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
        if (n && !prev[259 + i]) {
            s->panel_visible[i] = !s->panel_visible[i];
            char buf[64]; snprintf(buf, 64, "Panel %d %s", i+1, s->panel_visible[i] ? "shown" : "hidden");
            log_msg(buf);
        }
        prev[259 + i] = n;
    }
}

// ============================================================================
//  FLOATING 3D PANEL
// ============================================================================
/* Compute 2D position and size of a panel at given 3D depth */
static void panel_3d_transform(int fb_w, int fb_h,
                               double depth,      /* higher = further */
                               double base_x, double base_y,
                               double base_w, double base_h,
                               int* out_x, int* out_y,
                               int* out_w, int* out_h) {
    double scale = 1.0 / (1.0 + depth * 0.003);
    int cx = fb_w / 2, cy = fb_h / 2;
    *out_w = (int)(base_w * scale + 0.5);
    *out_h = (int)(base_h * scale + 0.5);
    *out_x = (int)(cx + (base_x - cx) * scale + 0.5);
    *out_y = (int)(cy + (base_y - cy) * scale + 0.5);
}

/* Draw a floating panel frame with title, accent bar, border */
static void draw_panel_frame(KainUiWidgetContext* ctx,
                             int x, int y, int w, int h,
                             const char* title, uint32_t accent) {
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width, fb_h = ctx->host->height;

    /* Shadow */
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x + 4, y + 4, w, h, 0x40000000, 8);
    /* Surface */
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0xFF1A1A2E, 8);
    /* Border */
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0x303A3A5C, 8);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x + 1, y + 1, w - 2, h - 2, 0xFF1A1A2E, 7);

    /* Title bar */
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x + 2, y + 2, w - 4, 26, 0xFF12121E);
    /* Accent underline */
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y + 28, w, 2, accent);
    /* Title text (using Segoe UI / default font) */
    ui_widget_draw_text(ctx, x + 10, y + 6, title, 0xFFE8E8F0, 13);
}

// ============================================================================
//  COLOR PALETTES
// ============================================================================
static const uint32_t g_palettes[4][6] = {
    {0x40FF4444, 0x4044FF44, 0x404444FF, 0x40FFFF44, 0x40FF44FF, 0x4044FFFF},  /* Classic */
    {0x40FF6B35, 0x40F7C59F, 0x40EFEFD0, 0x4044BBA5, 0x40335E7B, 0x40252540},  /* Warm earth */
    {0x40E63946, 0x40457B9D, 0x401D3557, 0x40A8DADC, 0x40F1FAEE, 0x40E63946},  /* Ocean */
    {0x40FF006E, 0x40FB5607, 0x40FFBE0B, 0x403A86FF, 0x408335FF, 0x40FF006E},  /* Neon */
};

static void apply_palette(int idx) {
    idx = idx % 4;
    memcpy(g_face_colors, g_palettes[idx], 6 * sizeof(uint32_t));
    g_state.palette_idx = idx;
    char buf[64];
    snprintf(buf, 64, "Palette %d applied", idx + 1);
    log_msg(buf);
}

// ============================================================================
//  ENTRY POINT
// ============================================================================

/* Forward declaration of our subclassed window proc */
static LRESULT CALLBACK sandbox_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

int main(void) {
    SetProcessDPIAware();

    /* ── Query DPI scale for 4K displays ─────────────────────── */
    HDC dpi_dc = GetDC(NULL);
    float dpi_scale = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;
    int win_w = (int)(1280 * dpi_scale + 0.5f);
    int win_h = (int)(720 * dpi_scale + 0.5f);

    AppState* s = &g_state;
    memset(s, 0, sizeof(AppState));
    s->speed_x = 0.5; s->speed_y = 0.7; s->speed_z = 0.3;
    s->cube_size = 1.0;
    s->show_stars = 1;
    for (int i = 0; i < 4; i++) s->panel_visible[i] = 1;
    apply_palette(0);

    log_msg("=== 3D UI Sandbox ===");
    log_msg("Keys: R=reset  W=wireframe  Space=pause");
    log_msg("1-4=toggle panels  Esc=exit");
    log_msg("Click fountain for particles!");

    init_stars();

    /* ── Create UI Session (DPI-scaled) ───────────────────────── */
    int64_t sid = abi_ui_session_create("ui3d_sandbox", win_w, win_h);
    if (sid <= 0) { MessageBoxA(NULL, "Session create failed", "Error", MB_OK); return 1; }

    if (abi_ui_host_attach(sid, "winit") < 0) {
        MessageBoxA(NULL, "Host attach failed", "Error", MB_OK); return 1;
    }
    abi_ui_window_open(sid, "3D UI Sandbox", win_w, win_h);

    /* ── Widget Context ────────────────────────────────────────── */
    KainUiWidgetContext* ctx = ui_widget_create(sid);
    if (!ctx) { MessageBoxA(NULL, "Widget ctx failed", "Error", MB_OK); return 1; }

    /* ── Load Fonts ────────────────────────────────────────────── */
    const char* font_paths[FONT_COUNT] = {
        "C:/Windows/Fonts/segoeui.ttf",   /* 0: headings / default */
        "C:/Windows/Fonts/consola.ttf",    /* 1: data readouts     */
        "C:/Windows/Fonts/arial.ttf",     /* 2: labels             */
        "C:/Windows/Fonts/impact.ttf",    /* 3: large titles       */
    };
    double font_sizes[FONT_COUNT] = {14.0, 12.0, 14.0, 16.0};
    const char* font_labels[FONT_COUNT] = {"Segoe UI","Consolas","Arial","Impact"};

    for (int i = 0; i < FONT_COUNT; i++) {
        s->font_ids[i] = ui_widget_load_font(ctx, font_paths[i], font_sizes[i]);
        char buf[64];
        snprintf(buf, 64, "Font %s: %s", font_labels[i],
                 s->font_ids[i] > 0 ? "OK" : "FAIL");
        log_msg(buf);
    }
    if (ctx->default_font < 0) {
        /* Set first loaded font as default */
        for (int i = 0; i < ctx->font_count; i++) {
            ctx->default_font = i;
            break;
        }
    }

    /* ── Subclass the Win32 window for our own WM_PAINT ───── */
    /* The host adapter's internal WM_PAINT has a SelectObject bug
     * that causes silent BitBlt failure. We subclass to handle
     * painting ourselves, and manage our own message pump. */
    KainNativeUiSession* ns = abi_ui_find_session(sid);
    if (!ns || !ns->host_state) {
        MessageBoxA(NULL, "No host state", "Error", MB_OK); return 1;
    }
    KainWin32UiHost* khost = (KainWin32UiHost*)ns->host_state;
    g_sandbox_host = khost;
    HWND hwnd = khost->hwnd;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)khost);
    g_orig_sandbox_wndproc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                                                        (LONG_PTR)sandbox_wndproc);
    SetWindowTextA(hwnd, "3D UI Sandbox");

    /* ── Custom interaction state ──────────────────────────────── */
    int btn_fountain = 0, btn_wire = 0, btn_cycle = 0, btn_explode = 0, btn_reset = 0;
    int chk_ortho = 0;
    int sld_x = 0, sld_y = 0, sld_z = 0, sld_size = 0;

    /* ── Timing ────────────────────────────────────────────────── */
    LARGE_INTEGER freq, pt, ct;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&pt);
    double dt = 0.016;

    /* ── Main Loop (own pump, own present) ──────────────────── */
    while (g_sandbox_host && g_sandbox_host->running) {
        /* ── Window message pump ────────────────────────────── */
        {
            MSG msg;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { g_sandbox_host->running = 0; break; }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        /* Brief sleep if no messages — prevents busy-wait and lets
         * other threads/processes work. 1ms is imperceptible to the
         * user but saves ~90% CPU vs spin-waiting. */
        if (!g_sandbox_host || !g_sandbox_host->running) break;
        Sleep(1);
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            g_sandbox_host->running = 0;
            break;
        }

        /* ── Frame timing ──────────────────────────────────────── */
        QueryPerformanceCounter(&ct);
        dt = (double)(ct.QuadPart - pt.QuadPart) / (double)freq.QuadPart;
        if (dt > 0.05) dt = 0.05;
        if (dt < 0.001) dt = 0.001;
        pt = ct;

        s->fps_counter++;
        s->fps_timer += dt;
        if (s->fps_timer >= 1.0) {
            s->fps = (double)s->fps_counter / s->fps_timer;
            s->fps_counter = 0; s->fps_timer = 0;
        }
        s->frame_count++;

        /* ── Input ─────────────────────────────────────────────── */
        handle_keyboard();

        /* ── Begin frame ───────────────────────────────────────── */
        abi_ui_begin_frame(sid, dt * 1000.0);
        ui_widget_begin_frame(ctx);

        /* ── Framebuffer access (direct from KainWin32UiHost) ── */
        uint32_t* fb = (uint32_t*)khost->framebuffer;
        int stride = khost->fb_stride / 4;
        int fb_w = khost->width;
        int fb_h = khost->height;
        if (!fb || fb_w <= 0 || fb_h <= 0) continue;

        /* ── 1. Clear to deep space ───────────────────────────── */
        {
            uint64_t pat = ((uint64_t)0xFF08081A << 32) | 0xFF08081A;
            int total = fb_w * fb_h;
            for (int i = 0; i < total / 2; i++)
                memcpy(&fb[i * 2], &pat, 8);
            if (total & 1) fb[total - 1] = 0xFF08081A;
        }

        /* ── 2. Starfield ──────────────────────────────────────── */
        draw_stars(fb, stride, fb_w, fb_h, dt);

        /* ── 3. Isometric grid ────────────────────────────────── */
        draw_isometric_grid(fb, stride, fb_w, fb_h, s->frame_count * dt);

        /* ── 4. Update particles ──────────────────────────────── */
        if (!s->paused) update_particles(dt);

        /* ── Update explode animation ──────────────────────────── */
        if (s->explode_mode) {
            s->explode_t += dt * 0.4;
            if (s->explode_t > 1.0) s->explode_t = 1.0;
        } else {
            s->explode_t *= 0.96;
            if (s->explode_t < 0.005) s->explode_t = 0.0;
        }

        /* ── 5. Floating 3D panels (drawn deepest first) ─────── */
        struct { double depth, bx, by, bw, bh; uint32_t accent; const char* title; } pdef[4] = {
            { 50, 200, 150, 270, 210, 0xFF21D4A1, "Transform Controls" },
            {100, 540, 130, 240, 200, 0xFF4A90D9, "Color Picker" },
            {160, 190, 390, 250, 180, 0xFFE8914A, "Scene Info" },
            {220, 510, 390, 280, 210, 0xFFE84A5F, "Log" },
        };

        for (int pi = 3; pi >= 0; pi--) {
            if (!s->panel_visible[pi]) continue;

            int px, py, pw, ph;
            panel_3d_transform(fb_w, fb_h,
                               pdef[pi].depth, pdef[pi].bx, pdef[pi].by,
                               pdef[pi].bw, pdef[pi].bh,
                               &px, &py, &pw, &ph);
            if (pw < 40 || ph < 30) continue;
            if (px + pw < 0 || px > fb_w) continue;

            draw_panel_frame(ctx, px, py, pw, ph, pdef[pi].title, pdef[pi].accent);

            int cx = px + 12, cy = py + 34, cw = pw - 24;

            switch (pi) {
            case 0: /* ── Transform Controls ───────────────── */
                ui_widget_draw_text(ctx, cx, cy, "X Rotation Speed", 0xFF8888A0, 11);
                cy += 14;
                slider_widget(ctx, cx, cy, cw, 18, &s->speed_x, 0.0, 3.0, &sld_x);
                cy += 24;

                ui_widget_draw_text(ctx, cx, cy, "Y Rotation Speed", 0xFF8888A0, 11);
                cy += 14;
                slider_widget(ctx, cx, cy, cw, 18, &s->speed_y, 0.0, 3.0, &sld_y);
                cy += 24;

                ui_widget_draw_text(ctx, cx, cy, "Z Rotation Speed", 0xFF8888A0, 11);
                cy += 14;
                slider_widget(ctx, cx, cy, cw, 18, &s->speed_z, 0.0, 3.0, &sld_z);
                cy += 24;

                ui_widget_draw_text(ctx, cx, cy, "Cube Size", 0xFF8888A0, 11);
                cy += 14;
                slider_widget(ctx, cx, cy, cw, 18, &s->cube_size, 0.3, 3.0, &sld_size);
                cy += 24;

                /* Buttons row */
                int bw2 = (cw - 8) / 2;
                if (btn_widget(ctx, cx, cy, bw2, 24, "Explode", &btn_explode)) {
                    s->explode_mode = !s->explode_mode;
                    s->explode_t = s->explode_mode ? 0.01 : s->explode_t;
                    log_msg(s->explode_mode ? "Explode ON" : "Explode OFF");
                }
                if (btn_widget(ctx, cx + bw2 + 8, cy, bw2, 24, "Reset View", &btn_reset)) {
                    s->rot_x = s->rot_y = s->rot_z = 0;
                    s->speed_x = 0.5; s->speed_y = 0.7; s->speed_z = 0.3;
                    s->cube_size = 1.0;
                    log_msg("View reset");
                }
                break;

            case 1: /* ── Color Picker ──────────────────────── */
                if (btn_widget(ctx, cx, cy, cw, 22, "Cycle Colors", &btn_cycle)) {
                    apply_palette(s->palette_idx + 1);
                }
                cy += 28;

                if (btn_widget(ctx, cx, cy, cw, 22, "Toggle Wireframe", &btn_wire)) {
                    s->wireframe_mode = !s->wireframe_mode;
                    log_msg(s->wireframe_mode ? "Wireframe ON" : "Wireframe OFF");
                }
                cy += 30;

                checkbox_widget(ctx, cx, cy, "Orthographic", &s->ortho_mode, &chk_ortho);
                cy += 22;

                if (btn_widget(ctx, cx, cy, cw, 22, "Particle Fountain!", &btn_fountain)) {
                    spawn_fountain();
                }
                cy += 28;

                /* Mini color swatches */
                int sw = (cw - 15) / 6;
                if (sw < 8) sw = 8;
                for (int fi = 0; fi < 6; fi++) {
                    int sx = cx + fi * (sw + 3);
                    uint32_t sc = g_face_colors[fi];
                    sc = (sc & 0xFFFFFF) | 0xFF000000; /* full opacity for swatch */
                    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                                 sx, cy, sw, sw, sc, 3);
                }
                break;

            case 2: /* ── Scene Info ────────────────────────── */
                {
                    char buf[128];
                    int df = 1; /* Consolas for data */
                    int64_t fid = (df < FONT_COUNT && s->font_ids[df] > 0) ? s->font_ids[df] : 0;

                    snprintf(buf, 128, "RX: %6.1f  RY: %6.1f  RZ: %6.1f",
                             s->rot_x * 180.0 / 3.14159,
                             s->rot_y * 180.0 / 3.14159,
                             s->rot_z * 180.0 / 3.14159);
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF21D4A1, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFF21D4A1, 11);
                    cy += 16;

                    snprintf(buf, 128, "FPS: %6.1f", s->fps);
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF4A90D9, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFF4A90D9, 11);
                    cy += 16;

                    snprintf(buf, 128, "Frame: %7d", s->frame_count);
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF8888A0, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFF8888A0, 11);
                    cy += 16;

                    snprintf(buf, 128, "Particles: %3d", s->particle_count);
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFFE8914A, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFFE8914A, 11);
                    cy += 16;

                    snprintf(buf, 128, "Verts: 8  Edges: 12  Faces: 6");
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF8888A0, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFF8888A0, 11);
                    cy += 16;

                    snprintf(buf, 128, "Mode: %s | %s",
                             s->ortho_mode ? "Ortho" : "Persp",
                             s->wireframe_mode ? "Wire" : "Solid");
                    if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFFE84A5F, 0, fid);
                    else    ui_widget_draw_text(ctx, cx, cy, buf, 0xFFE84A5F, 11);
                    cy += 16;

                    if (s->paused) {
                        int bf = 3; /* Impact for pause text */
                        int64_t pfid = (bf < FONT_COUNT && s->font_ids[bf] > 0) ? s->font_ids[bf] : 0;
                        if (pfid) ui_widget_draw_text_ex(ctx, cx, cy, " >> PAUSED << ", 0xFFFF4444, 0, pfid);
                        else      ui_widget_draw_text(ctx, cx, cy, " >> PAUSED << ", 0xFFFF4444, 14);
                    }
                }
                break;

            case 3: /* ── Log ───────────────────────────────── */
                {
                    int df = 1;
                    int64_t fid = (df < FONT_COUNT && s->font_ids[df] > 0) ? s->font_ids[df] : 0;
                    int ly = cy;
                    for (int i = 0; i < s->log_count; i++) {
                        if (ly > py + ph - 8) break;
                        if (fid) ui_widget_draw_text_ex(ctx, cx, ly, s->log_lines[i], 0xFF8888A0, 0, fid);
                        else     ui_widget_draw_text(ctx, cx, ly, s->log_lines[i], 0xFF8888A0, 10);
                        ly += 15;
                    }
                }
                break;
            }
        }

        /* ── 6. Draw 3D cube (over panels, under particles) ──── */
        draw_cube(fb, stride, fb_w, fb_h, dt);

        /* ── 7. Draw particles (on top of everything) ────────── */
        draw_particles(fb, stride, fb_w, fb_h);

        /* ── 8. HUD overlay ───────────────────────────────────── */
        {
            char buf[64];
            snprintf(buf, 64, "FPS: %.0f", s->fps);
            ui_widget_draw_text(ctx, 8, 8, buf, 0xFF21D4A1, 12);

            snprintf(buf, 64, "Frame: %d", s->frame_count);
            ui_widget_draw_text(ctx, 8, 24, buf, 0xFF8888A0, 11);

            if (s->paused)
                ui_widget_draw_text(ctx, fb_w / 2 - 40, 8, ">> PAUSED <<", 0xFFFF4444, 14);

            /* Legend bar at bottom */
            ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                        0, fb_h - 22, fb_w, 22, 0x60000000, 0);
            ui_widget_draw_text(ctx, 10, fb_h - 18,
                                "R:Reset  W:Wire  SPACE:Pause  1-4:Panels  ESC:Exit",
                                0xFF444466, 11);
            ui_widget_draw_text(ctx, fb_w - 200, fb_h - 18,
                                "3D UI Sandbox v1.0", 0xFF2A2A44, 10);
        }

        /* ── End frame ─────────────────────────────────────────── */
        ui_widget_end_frame(ctx);
        abi_ui_end_frame(sid);

        /* ── Present via InvalidateRect (triggers our WM_PAINT) ─ */
        InvalidateRect(hwnd, NULL, FALSE);
        Sleep(16);  /* ~60 FPS cap */
    }

    /* ── Cleanup ──────────────────────────────────────────────── */
    ui_widget_destroy(ctx);
    abi_ui_session_destroy(sid);
    return 0;
}


/* ── Subclassed window procedure ─────────────────────────────── */
/* The host adapter's WM_PAINT has a SelectObject bug. We subclass
 * the window to do BitBlt directly from the DIB framebuffer.
 * Non-handled messages chain to the original wndproc. */
static LRESULT CALLBACK sandbox_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_sandbox_host && g_sandbox_host->hdc_buffer) {
            BitBlt(hdc, 0, 0,
                   g_sandbox_host->width, g_sandbox_host->height,
                   g_sandbox_host->hdc_buffer,
                   0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        /* Prevent framebuffer overrun: the DIB was allocated at the
         * original window size. On maximize/resize, the original
         * wndproc updates host->width/height past the DIB bounds,
         * causing out-of-bounds writes. We intercept and clamp the
         * host dimensions to the actual DIB allocation, then let
         * the original handler also update them (but we'll re-clamp
         * after). We just return 0 to consume the message entirely
         * — the DIB is fixed-size so the host dimensions MUST stay
         * at the original allocation size. */
        /* Let original handler update, then immediately clamp back */
        LRESULT r = CallWindowProcA(g_orig_sandbox_wndproc, hwnd, msg, w, l);
        /* The original handler set host->width/height to the new
         * client area. But the DIB framebuffer is still at the
         * original size. We MUST clamp the host dimensions back
         * to what the framebuffer can actually hold. */
        if (g_sandbox_host) {
            /* Compute actual DIB dimensions from fb_stride */
            int dib_w = g_sandbox_host->fb_stride / 4;
            int dib_h = g_sandbox_host->height;
            /* If we can't determine from stride, use a safe fixed cap.
             * The DIB was originally created at (dib_w x dib_h).
             * We approximate: stride/4 = original width in pixels.
             * Unfortunately the DIB height isn't stored separately,
             * so we use the original width and derive height from
             * total_fb_bytes / (stride). We DON'T know total_fb_bytes.
             *
             * Simplest safe approach: just re-create the DIB at the
             * new size, exactly like win32_host_create does. */
            RECT r;
            GetClientRect(hwnd, &r);
            int new_w = r.right - r.left;
            int new_h = r.bottom - r.top;
            if (new_w > 0 && new_h > 0 &&
                (new_w != g_sandbox_host->width || new_h != g_sandbox_host->height)) {
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
                                                       (void**)&g_sandbox_host->framebuffer,
                                                       NULL, 0);
                    if (new_bmp) {
                        HBITMAP old_bmp = (HBITMAP)SelectObject(g_sandbox_host->hdc_buffer, new_bmp);
                        if (old_bmp && old_bmp != new_bmp)
                            DeleteObject(old_bmp);
                        g_sandbox_host->hbitmap = new_bmp;
                        g_sandbox_host->width = new_w;
                        g_sandbox_host->height = new_h;
                        g_sandbox_host->fb_stride = new_w * 4;
                    }
                    ReleaseDC(NULL, hdc_screen);
                }
            }
        }
        return 0;
    }
    case WM_CLOSE:
        if (g_sandbox_host) g_sandbox_host->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED: {
        /* Window was moved to a monitor with different DPI.
         * Re-create framebuffer at the DPI-scaled size. */
        RECT* rect = (RECT*)l;
        int new_w = rect->right - rect->left;
        int new_h = rect->bottom - rect->top;
        if (new_w > 0 && new_h > 0 && g_sandbox_host) {
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
                                                   (void**)&g_sandbox_host->framebuffer,
                                                   NULL, 0);
                if (new_bmp) {
                    HBITMAP old_bmp = (HBITMAP)SelectObject(g_sandbox_host->hdc_buffer, new_bmp);
                    if (old_bmp && old_bmp != new_bmp) DeleteObject(old_bmp);
                    g_sandbox_host->hbitmap = new_bmp;
                    g_sandbox_host->width = new_w;
                    g_sandbox_host->height = new_h;
                    g_sandbox_host->fb_stride = new_w * 4;
                }
                ReleaseDC(NULL, hdc_screen);
            }
            SetWindowPos(hwnd, NULL, rect->left, rect->top, new_w, new_h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    }
    return CallWindowProcA(g_orig_sandbox_wndproc, hwnd, msg, w, l);
}
