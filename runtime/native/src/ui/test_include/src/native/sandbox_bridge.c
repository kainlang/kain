// ============================================================================
//  sandbox_bridge.c — Kain natural-include bridge companion for ui3d_sandbox.c
// ============================================================================
//  REFACTORED from ui3d_sandbox.c — wraps the full 3D UI sandbox demo
//  (rotating cube, starfield, particle fountain, isometric grid, floating
//  panels, widget controls) as a callable C API.
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

#include "sandbox_bridge.h"
#include "../../ui_system.h"
#include "../../ui_system_internal.h"
#include "../../ui_host_adapter.h"
#include "../../../include/ui_renderer.h"
#include "../../../include/ui_layout.h"
#include "../../../include/ui_color.h"
#include "../../../include/ui_font.h"
#include "../../widgets/ui_widget.h"

// ══════════════════════════════════════════════════════════════════════════
//  KainWin32UiHost (must match ui_host_adapter.c exactly)
// ══════════════════════════════════════════════════════════════════════════
typedef struct KainWin32UiHost {
    HWND hwnd; int width; int height; int running; int initialized;
    uint8_t* framebuffer; int fb_stride; HDC hdc_buffer; HBITMAP hbitmap;
    int64_t session_id; int64_t input_session_id; float dpi_scale;
} KainWin32UiHost;

// ══════════════════════════════════════════════════════════════════════════
//  3D MATH
// ══════════════════════════════════════════════════════════════════════════
typedef struct { double x, y, z; } Vec3;
static Vec3 vec3(double x, double y, double z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }
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
static Vec3 rotate_all(Vec3 v, double rx, double ry, double rz) {
    return rotate_z(rotate_y(rotate_x(v, rx), ry), rz);
}
static void project(Vec3 v, double focal, int sw, int sh, int* ox, int* oy, double sm) {
    double d = focal + v.z; if (d < 0.01) d = 0.01;
    double s = focal / d * sm;
    *ox = (int)(sw / 2.0 + v.x * s); *oy = (int)(sh / 2.0 - v.y * s);
}
static void project_ortho(Vec3 v, int sw, int sh, int* ox, int* oy, double sm) {
    *ox = (int)(sw / 2.0 + v.x * sm); *oy = (int)(sh / 2.0 - v.y * sm);
}
static int iclamp(int v, int lo, int hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }
static double dclamp(double v, double lo, double hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }

// ══════════════════════════════════════════════════════════════════════════
//  BRESENHAM LINE
// ══════════════════════════════════════════════════════════════════════════
static void draw_line_clip(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2, x = x1, y = y1;
    for (;;) {
        if (x >= 0 && x < fb_w && y >= 0 && y < fb_h) fb[y * stride + x] = color;
        if (x == x2 && y == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  FILLED CONVEX QUAD
// ══════════════════════════════════════════════════════════════════════════
static void fill_quad(uint32_t* fb, int stride, int fb_w, int fb_h,
                      int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3, uint32_t color) {
    int verts[4][2] = {{x0,y0},{x1,y1},{x2,y2},{x3,y3}};
    int min_y = y0, max_y = y0;
    for (int i = 0; i < 4; i++) {
        if (verts[i][1] < min_y) min_y = verts[i][1];
        if (verts[i][1] > max_y) max_y = verts[i][1];
    }
    min_y = iclamp(min_y, 0, fb_h - 1); max_y = iclamp(max_y, 0, fb_h - 1);
    if (min_y >= max_y) return;
    for (int py = min_y; py <= max_y; py++) {
        double ix[4]; int nix = 0;
        for (int e = 0; e < 4; e++) {
            int e1 = e, e2 = (e + 1) % 4;
            int y1 = verts[e1][1], y2 = verts[e2][1];
            if (y1 == y2) continue;
            if ((py < y1 && py < y2) || (py > y1 && py > y2)) continue;
            double t = (double)(py - y1) / (double)(y2 - y1);
            double xi = verts[e1][0] + t * (verts[e2][0] - verts[e1][0]);
            ix[nix++] = xi;
        }
        if (nix < 2) continue;
        for (int i = 0; i < nix - 1; i++)
            for (int j = i + 1; j < nix; j++)
                if (ix[j] < ix[i]) { double t = ix[i]; ix[i] = ix[j]; ix[j] = t; }
        for (int i = 0; i + 1 < nix; i += 2) {
            int x_lo = iclamp((int)(ix[i] + 0.5), 0, fb_w - 1);
            int x_hi = iclamp((int)(ix[i+1] + 0.5), 0, fb_w - 1);
            uint32_t* row = fb + py * stride;
            for (int px = x_lo; px <= x_hi; px++)
                row[px] = ui_color_blend(color, row[px]);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  APP STATE
// ══════════════════════════════════════════════════════════════════════════
#define MAX_PARTICLES 200
#define MAX_STARS     120
#define MAX_LOG        16
#define FONT_COUNT      4

typedef struct { double px, py, pz, vx, vy, vz, lifetime; uint32_t color; } Particle;
typedef struct { double x, y, z; uint8_t brightness; } Star;

typedef struct {
    double rot_x, rot_y, rot_z, speed_x, speed_y, speed_z, cube_size, explode_t;
    int wireframe_mode, explode_mode, show_stars, paused, panel_visible[4], ortho_mode;
    Star stars[MAX_STARS]; double star_phase;
    Particle particles[MAX_PARTICLES]; int particle_count;
    char log_lines[MAX_LOG][128]; int log_count;
    double fps; int frame_count, fps_counter; double fps_timer;
    int64_t font_ids[FONT_COUNT]; uint32_t face_colors[6]; int palette_idx;
} AppState;

static const Vec3 g_cube_verts[8] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
};
static const int g_cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};
static const int g_cube_faces[6][4] = {
    {0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{0,3,7,4},{1,2,6,5}
};
static const Vec3 g_face_normals[6] = {
    {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}
};
static const Vec3 g_face_centers[6] = {
    {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}
};
static const uint32_t g_palettes[4][6] = {
    {0x40FF4444,0x4044FF44,0x404444FF,0x40FFFF44,0x40FF44FF,0x4044FFFF},
    {0x40FF6B35,0x40F7C59F,0x40EFEFD0,0x4044BBA5,0x40335E7B,0x40252540},
    {0x40E63946,0x40457B9D,0x401D3557,0x40A8DADC,0x40F1FAEE,0x40E63946},
    {0x40FF006E,0x40FB5607,0x40FFBE0B,0x403A86FF,0x408335FF,0x40FF006E},
};

struct SandboxDemo {
    AppState state;
    KainWin32UiHost* host;
    double dpi_scale;
    int64_t session_id;
    KainUiWidgetContext* widget_ctx;
    WNDPROC orig_wndproc;
    int key_mask, mouse_x, mouse_y, mouse_down;
    LARGE_INTEGER freq, prev_time;
    double dt;
    int initialized;
    // Custom widget interaction state
    int btn_fountain, btn_wire, btn_cycle, btn_explode, btn_reset;
    int chk_ortho, sld_x, sld_y, sld_z, sld_size;
};

// ══════════════════════════════════════════════════════════════════════════
//  FORWARDS
// ══════════════════════════════════════════════════════════════════════════
static void log_msg(SandboxDemo* d, const char* msg);
static void init_stars(AppState* s);
static void apply_palette(SandboxDemo* d, int idx);
static void spawn_fountain(SandboxDemo* d);
static void update_particles(SandboxDemo* d, double dt);
static LRESULT CALLBACK sandbox_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ══════════════════════════════════════════════════════════════════════════
//  LOG
// ══════════════════════════════════════════════════════════════════════════
static void log_msg(SandboxDemo* d, const char* msg) {
    AppState* s = &d->state;
    if (s->log_count < MAX_LOG) {
        strncpy(s->log_lines[s->log_count], msg, 127);
        s->log_lines[s->log_count][127] = 0; s->log_count++;
    } else {
        for (int i = 0; i < MAX_LOG - 1; i++) memcpy(s->log_lines[i], s->log_lines[i+1], 128);
        strncpy(s->log_lines[MAX_LOG-1], msg, 127);
        s->log_lines[MAX_LOG-1][127] = 0;
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  STARFIELD
// ══════════════════════════════════════════════════════════════════════════
static void init_stars(AppState* s) {
    srand(42);
    for (int i = 0; i < MAX_STARS; i++) {
        s->stars[i].x = (double)(rand() % 4000 - 2000) / 10.0;
        s->stars[i].y = (double)(rand() % 4000 - 2000) / 10.0;
        s->stars[i].z = (double)(rand() % 2000 + 200) / 10.0;
        s->stars[i].brightness = (uint8_t)(rand() % 156 + 100);
    }
}

static void draw_stars(SandboxDemo* d, uint32_t* fb, int stride, int fb_w, int fb_h, double dt) {
    AppState* s = &d->state;
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
        double bf = dclamp(1.0 - rz / 150.0, 0.2, 1.0);
        uint8_t br = (uint8_t)(star->brightness * bf);
        uint32_t col = 0xFF000000 | ((uint32_t)br << 16) | ((uint32_t)br << 8) | br;
        for (int dy = 0; dy < size; dy++)
            for (int dx = 0; dx < size; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) fb[py * stride + px] = col;
            }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  ISOMETRIC GRID FLOOR
// ══════════════════════════════════════════════════════════════════════════
static void draw_isometric_grid(uint32_t* fb, int stride, int fb_w, int fb_h) {
    int cx = fb_w / 2, base_y = fb_h - 30, vanish_y = fb_h / 3;
    uint32_t col_a = 0xFF222238, col_b = 0xFF18182A;
    int n_radial = 36;
    for (int i = -n_radial/2; i <= n_radial/2; i++) {
        if (i == 0) continue;
        double t = (double)i / (double)(n_radial/2);
        int bx = cx + (int)(t * 350.0), vx = cx + (int)(t * 25.0);
        draw_line_clip(fb, stride, fb_w, fb_h, bx, base_y, vx, vanish_y, (abs(i)%2==0)?col_a:col_b);
    }
    for (int i = 0; i <= 22; i++) {
        double t = (double)i / 22.0;
        int y = base_y - (int)((double)(base_y - vanish_y) * (1.0 - pow(1.0 - t, 2.5)));
        if (y < vanish_y) y = vanish_y;
        int half_w = (int)(350.0 * (1.0 - t)); if (half_w < 3) half_w = 3;
        draw_line_clip(fb, stride, fb_w, fb_h, cx - half_w, y, cx + half_w, y, (i%2==0)?col_a:col_b);
    }
    draw_line_clip(fb, stride, fb_w, fb_h, cx, base_y, cx, vanish_y, 0xFF3A3A5C);
}

// ══════════════════════════════════════════════════════════════════════════
//  3D CUBE
// ══════════════════════════════════════════════════════════════════════════
static void draw_cube(SandboxDemo* d, uint32_t* fb, int stride, int fb_w, int fb_h, double dt) {
    AppState* s = &d->state;
    if (!s->paused) { s->rot_x += s->speed_x * dt; s->rot_y += s->speed_y * dt; s->rot_z += s->speed_z * dt; }
    double size = s->cube_size, focal = 3.0;
    int cx = fb_w/2, cy = fb_h/2 - 15;
    double pix_scale = size * 60.0;
    Vec3 tv[8]; int svx[8], svy[8];
    for (int i = 0; i < 8; i++) {
        tv[i] = rotate_all(g_cube_verts[i], s->rot_x, s->rot_y, s->rot_z);
        if (s->ortho_mode) project_ortho(tv[i], cx*2, cy*2, &svx[i], &svy[i], pix_scale);
        else project(tv[i], focal, cx*2, cy*2, &svx[i], &svy[i], pix_scale);
    }

    // Solid faces (painter's algorithm)
    if (!s->wireframe_mode || s->explode_mode) {
        typedef struct { int idx; double z; } FZ;
        FZ fz[6];
        for (int f = 0; f < 6; f++) {
            double az = 0;
            for (int j = 0; j < 4; j++) az += tv[g_cube_faces[f][j]].z;
            fz[f].idx = f; fz[f].z = az / 4.0;
        }
        for (int i = 0; i < 6; i++)
            for (int j = i+1; j < 6; j++)
                if (fz[j].z > fz[i].z) { FZ t = fz[i]; fz[i] = fz[j]; fz[j] = t; }
        for (int fi = 0; fi < 6; fi++) {
            int idx = fz[fi].idx;
            const int* quad = g_cube_faces[idx];
            uint32_t col = s->face_colors[idx];
            if (s->explode_mode) {
                double et = s->explode_t;
                double ox = g_face_centers[idx].x * et * 1.5;
                double oy = g_face_centers[idx].y * et * 1.5;
                double oz = g_face_centers[idx].z * et * 1.5;
                int ex[4], ey[4];
                for (int j = 0; j < 4; j++) {
                    Vec3 ov = tv[quad[j]]; ov.x += ox; ov.y += oy; ov.z += oz;
                    if (s->ortho_mode) project_ortho(ov, cx*2, cy*2, &ex[j], &ey[j], pix_scale);
                    else project(ov, focal, cx*2, cy*2, &ex[j], &ey[j], pix_scale);
                }
                fill_quad(fb, stride, fb_w, fb_h, ex[0],ey[0],ex[1],ey[1],ex[2],ey[2],ex[3],ey[3],col);
            } else {
                fill_quad(fb, stride, fb_w, fb_h,
                    svx[quad[0]],svy[quad[0]],svx[quad[1]],svy[quad[1]],
                    svx[quad[2]],svy[quad[2]],svx[quad[3]],svy[quad[3]],col);
            }
        }
    }

    // Wireframe
    uint32_t wire_col = s->wireframe_mode ? 0xFF00FFAA : 0xFFE8E8F0;
    for (int e = 0; e < 12; e++) {
        int v1 = g_cube_edges[e][0], v2 = g_cube_edges[e][1];
        draw_line_clip(fb, stride, fb_w, fb_h, svx[v1], svy[v1], svx[v2], svy[v2], wire_col);
    }

    // Vertex highlights
    for (int i = 0; i < 8; i++)
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                if (dx*dx+dy*dy <= 4) {
                    int px = svx[i]+dx, py = svy[i]+dy;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) fb[py*stride+px] = 0xFFFFDD44;
                }
}

// ══════════════════════════════════════════════════════════════════════════
//  PARTICLES
// ══════════════════════════════════════════════════════════════════════════
static void spawn_fountain(SandboxDemo* d) {
    AppState* s = &d->state;
    srand((unsigned int)(GetTickCount64() & 0x7FFFFFFF));
    int count = 50;
    for (int i = 0; i < count && s->particle_count < MAX_PARTICLES; i++) {
        Particle* p = &s->particles[s->particle_count];
        p->px = p->py = p->pz = 0;
        p->vx = (double)(rand()%400-200)/100.0;
        p->vy = (double)(rand()%200+80)/100.0 * 4.0;
        p->vz = (double)(rand()%400-200)/100.0;
        uint8_t r = (uint8_t)(rand()%256), g = (uint8_t)(rand()%256), b = (uint8_t)(rand()%256);
        p->color = 0xFF000000 | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b;
        p->lifetime = 1.5 + (double)(rand()%100)/100.0 * 2.0;
        s->particle_count++;
    }
    log_msg(d, "Fountain spawned!");
}

static void update_particles(SandboxDemo* d, double dt) {
    AppState* s = &d->state;
    double gravity = -8.0;
    for (int i = 0; i < s->particle_count; ) {
        Particle* p = &s->particles[i];
        if (!s->paused) {
            p->vx += (double)(rand()%100-50)/500.0;
            p->vy += gravity * dt;
            p->px += p->vx * dt * 50.0;
            p->py += p->vy * dt * 50.0;
            p->pz += p->vz * dt * 50.0;
            p->lifetime -= dt;
        }
        if (p->lifetime <= 0.0) s->particles[i] = s->particles[--s->particle_count];
        else i++;
    }
}

static void draw_particles(SandboxDemo* d, uint32_t* fb, int stride, int fb_w, int fb_h) {
    AppState* s = &d->state;
    double focal = 3.0, pix_scale = 50.0;
    int cx = fb_w/2, cy = fb_h/2 - 15;
    for (int i = 0; i < s->particle_count; i++) {
        Particle* p = &s->particles[i];
        Vec3 wv = vec3(p->px, p->py, p->pz);
        int sx, sy;
        if (s->ortho_mode) project_ortho(wv, cx*2, cy*2, &sx, &sy, pix_scale*0.3);
        else project(wv, focal, cx*2, cy*2, &sx, &sy, pix_scale*0.3);
        int size = s->ortho_mode ? 2 : (int)(2.0*focal/(focal+p->pz));
        if (size < 1) size = 1; if (size > 5) size = 5;
        uint32_t col = ui_color_with_opacity(p->color, dclamp(p->lifetime/2.0, 0.0, 1.0));
        for (int dy = -size; dy <= size; dy++)
            for (int dx = -size; dx <= size; dx++)
                if (dx*dx+dy*dy <= size*size) {
                    int px = sx+dx, py = sy+dy;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                        fb[py*stride+px] = ui_color_blend(col, fb[py*stride+px]);
                }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  CUSTOM WIDGET PRIMITIVES (direct framebuffer)
// ══════════════════════════════════════════════════════════════════════════
static int btn_widget(SandboxDemo* d, int x, int y, int w, int h,
                      const char* label, int* pressed) {
    KainUiWidgetContext* ctx = d->widget_ctx;
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4, fb_w = ctx->host->width, fb_h = ctx->host->height;
    int hovered = (ctx->mouse_x >= x && ctx->mouse_x < x+w && ctx->mouse_y >= y && ctx->mouse_y < y+h);
    int clicked = 0;
    if (ctx->mouse_down && !*pressed && hovered) *pressed = 1;
    if (!ctx->mouse_down && *pressed) { if (hovered) clicked = 1; *pressed = 0; }
    uint32_t bg = *pressed ? 0xFF505080 : hovered ? 0xFF404068 : 0xFF303050;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, bg, 4);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0x153A3A5C, 4);
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x+4, y+h-2, w-8, 2, 0xFF21D4A1);
    ui_widget_draw_text_centered(ctx, x, y, w, h, label, 0xFFE8E8F0, 13);
    return clicked;
}

static int slider_widget(SandboxDemo* d, int x, int y, int w, int h,
                         double* value, double lo, double hi, int* drag) {
    KainUiWidgetContext* ctx = d->widget_ctx;
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4, fb_w = ctx->host->width, fb_h = ctx->host->height;
    double range = (hi > lo) ? hi-lo : 1.0;
    double norm = dclamp((*value-lo)/range, 0.0, 1.0);
    int track_h = 6, track_y = y + h/2 - track_h/2;
    int thumb_w = 10, thumb_h = 16;
    int thumb_x = x + (int)(norm * (double)(w-thumb_w));
    int thumb_y = track_y + track_h/2 - thumb_h/2;
    int changed = 0;
    if (!*drag && ctx->mouse_down && !d->mouse_down &&
        ctx->mouse_x >= thumb_x && ctx->mouse_x < thumb_x+thumb_w &&
        ctx->mouse_y >= thumb_y && ctx->mouse_y < thumb_y+thumb_h) *drag = 1;
    if (*drag && ctx->mouse_down) {
        double nn = (ctx->mouse_x - x) / (double)(w-thumb_w);
        nn = dclamp(nn, 0.0, 1.0);
        double nv = lo + nn*range;
        if (fabs(nv - *value) > 0.001) { *value = nv; changed = 1; }
    }
    if (!ctx->mouse_down) *drag = 0;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, track_y, w, track_h, 0xFF2A2A44, 3);
    if (thumb_x > x) ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, track_y, thumb_x-x, track_h, 0xFF21D4A1, 3);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, thumb_x, thumb_y, thumb_w, thumb_h, *drag?0xFF21D4A1:0xFF4A90D9, 4);
    return changed;
}

static int checkbox_widget(SandboxDemo* d, int x, int y, const char* label, int* value, int* pressed) {
    KainUiWidgetContext* ctx = d->widget_ctx;
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4, fb_w = ctx->host->width, fb_h = ctx->host->height;
    int ch = 14;
    int hovered = (ctx->mouse_x >= x && ctx->mouse_x < x+ch+120 && ctx->mouse_y >= y && ctx->mouse_y < y+ch+4);
    int toggled = 0;
    if (ctx->mouse_down && !*pressed && hovered) *pressed = 1;
    if (!ctx->mouse_down && *pressed) { if (hovered) { *value = !*value; toggled = 1; } *pressed = 0; }
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, ch, ch, *value?0xFF21D4A1:0xFF303050, 3);
    if (*value) {
        for (int i = 0; i < 4; i++) { int px=x+3+i, py=y+6+i; if (px>=0&&px<fb_w&&py>=0&&py<fb_h) fb[py*stride+px]=0xFFFFFFFF; }
        for (int i = 0; i < 5; i++) { int px=x+5+i, py=y+10-i; if (px>=0&&px<fb_w&&py>=0&&py<fb_h) fb[py*stride+px]=0xFFFFFFFF; }
    }
    ui_widget_draw_text(ctx, x+ch+8, y-1, label, 0xFFE8E8F0, 11);
    return toggled;
}

// ══════════════════════════════════════════════════════════════════════════
//  FLOATING 3D PANEL
// ══════════════════════════════════════════════════════════════════════════
static void panel_3d_transform(int fb_w, int fb_h, double depth,
                               double bx, double by, double bw, double bh,
                               int* ox, int* oy, int* ow, int* oh) {
    double scale = 1.0 / (1.0 + depth * 0.003);
    int cx = fb_w/2, cy = fb_h/2;
    *ow = (int)(bw * scale + 0.5); *oh = (int)(bh * scale + 0.5);
    *ox = (int)(cx + (bx-cx)*scale + 0.5);
    *oy = (int)(cy + (by-cy)*scale + 0.5);
}

static void draw_panel_frame(SandboxDemo* d, int x, int y, int w, int h,
                              const char* title, uint32_t accent) {
    KainUiWidgetContext* ctx = d->widget_ctx;
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4, fb_w = ctx->host->width, fb_h = ctx->host->height;
    double ds = d->dpi_scale;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x+(int)(4*ds+0.5), y+(int)(4*ds+0.5), w, h, 0x40000000, 8);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0xFF1A1A2E, 8);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, 0x303A3A5C, 8);
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x+1, y+1, w-2, h-2, 0xFF1A1A2E, 7);
    int tb_h = (int)(26*ds+0.5);
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x+2, y+2, w-4, tb_h, 0xFF12121E);
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y+(int)(28*ds+0.5), w, 2, accent);
    ui_widget_draw_text(ctx, x+(int)(10*ds+0.5), y+(int)(6*ds+0.5), title, 0xFFE8E8F0, (int)(13*ds+0.5));
}

static void apply_palette(SandboxDemo* d, int idx) {
    idx = idx % 4;
    memcpy(d->state.face_colors, g_palettes[idx], 6*sizeof(uint32_t));
    d->state.palette_idx = idx;
    char buf[64]; snprintf(buf, 64, "Palette %d applied", idx+1);
    log_msg(d, buf);
}

// ══════════════════════════════════════════════════════════════════════════
//  WNDPROC
// ══════════════════════════════════════════════════════════════════════════
static LRESULT CALLBACK sandbox_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    SandboxDemo* d = (SandboxDemo*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
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

SandboxDemo* sandbox_bridge_init(int width, int height) {
    SetProcessDPIAware();
    HDC dpi_dc = GetDC(NULL);
    float dpi = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi < 1.0f) dpi = 1.0f;
    int win_w = (int)(width * dpi + 0.5f);
    int win_h = (int)(height * dpi + 0.5f);

    SandboxDemo* d = (SandboxDemo*)calloc(1, sizeof(SandboxDemo));
    if (!d) return NULL;
    d->dpi_scale = dpi;

    AppState* s = &d->state;
    s->speed_x = 0.5; s->speed_y = 0.7; s->speed_z = 0.3;
    s->cube_size = 1.0; s->show_stars = 1;
    for (int i = 0; i < 4; i++) s->panel_visible[i] = 1;
    apply_palette(d, 0);
    log_msg(d, "=== 3D UI Sandbox (Kain include) ===");
    log_msg(d, "Keys: R=reset W=wireframe Space=pause 1-4=panels Esc=exit");

    init_stars(s);

    int64_t sid = abi_ui_session_create("sandbox_kain", win_w, win_h);
    if (sid <= 0) { free(d); return NULL; }
    d->session_id = sid;

    if (abi_ui_host_attach(sid, "winit") < 0) { abi_ui_session_destroy(sid); free(d); return NULL; }
    abi_ui_window_open(sid, "3D UI Sandbox (Kain include)", win_w, win_h);

    KainUiWidgetContext* ctx = ui_widget_create(sid);
    if (!ctx) { abi_ui_session_destroy(sid); free(d); return NULL; }
    d->widget_ctx = ctx;

    const char* font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/impact.ttf",
    };
    double font_sizes[] = {14.0*dpi, 12.0*dpi, 14.0*dpi, 16.0*dpi};
    for (int i = 0; i < FONT_COUNT; i++) {
        s->font_ids[i] = ui_widget_load_font(ctx, font_paths[i], font_sizes[i]);
        char buf[64];
        snprintf(buf, 64, "Font %d: %s", i, s->font_ids[i] > 0 ? "OK" : "FAIL");
        log_msg(d, buf);
    }
    if (ctx->default_font < 0)
        for (int i = 0; i < ctx->font_count; i++) { ctx->default_font = i; break; }

    KainNativeUiSession* ns = abi_ui_find_session(sid);
    if (!ns || !ns->host_state) {
        ui_widget_destroy(ctx); abi_ui_session_destroy(sid); free(d); return NULL;
    }
    KainWin32UiHost* khost = (KainWin32UiHost*)ns->host_state;
    d->host = khost;
    HWND hwnd = khost->hwnd;
    SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)d);
    d->orig_wndproc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)sandbox_wndproc);
    SetWindowTextA(hwnd, "3D UI Sandbox (Kain include)");

    QueryPerformanceFrequency(&d->freq);
    QueryPerformanceCounter(&d->prev_time);
    d->dt = 0.016;
    d->initialized = 1;

    return d;
}

void sandbox_bridge_destroy(SandboxDemo* d) {
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

int sandbox_bridge_frame(SandboxDemo* d) {
    if (!d || !d->initialized) return -1;
    if (!d->host || !d->host->running) return -1;

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { d->host->running = 0; return -1; }
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    Sleep(1);
    if (!d->host->running) return -1;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { d->host->running = 0; return -1; }

    LARGE_INTEGER ct;
    QueryPerformanceCounter(&ct);
    d->dt = (double)(ct.QuadPart - d->prev_time.QuadPart) / (double)d->freq.QuadPart;
    if (d->dt > 0.05) d->dt = 0.05;
    if (d->dt < 0.001) d->dt = 0.001;
    d->prev_time = ct;

    AppState* s = &d->state;
    s->fps_counter++; s->fps_timer += d->dt;
    if (s->fps_timer >= 1.0) { s->fps = (double)s->fps_counter / s->fps_timer; s->fps_counter = 0; s->fps_timer = 0; }
    s->frame_count++;

    // Keyboard — use GetAsyncKeyState directly
    static int prev_k[8];
    int nR = (GetAsyncKeyState('R') & 0x8000) ? 1 : 0;
    int nW = (GetAsyncKeyState('W') & 0x8000) ? 1 : 0;
    int nS = (GetAsyncKeyState(VK_SPACE) & 0x8000) ? 1 : 0;
    if (nR && !prev_k[0]) { s->rot_x=s->rot_y=s->rot_z=0; log_msg(d,"Rotation reset"); }
    if (nW && !prev_k[1]) { s->wireframe_mode=!s->wireframe_mode; log_msg(d,s->wireframe_mode?"Wireframe ON":"Wireframe OFF"); }
    if (nS && !prev_k[2]) { s->paused=!s->paused; log_msg(d,s->paused?"PAUSED":"Resumed"); }
    for (int i = 0; i < 4; i++) {
        int vk = '1' + i;
        int n = (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
        if (n && !prev_k[4+i]) {
            s->panel_visible[i] = !s->panel_visible[i];
            char buf[64]; snprintf(buf, 64, "Panel %d %s", i+1, s->panel_visible[i]?"shown":"hidden");
            log_msg(d, buf);
        }
        prev_k[4+i] = n;
    }
    prev_k[0]=nR; prev_k[1]=nW; prev_k[2]=nS;

    // Explode
    if (s->explode_mode) {
        s->explode_t += d->dt * 0.4; if (s->explode_t > 1.0) s->explode_t = 1.0;
    } else {
        s->explode_t *= 0.96; if (s->explode_t < 0.005) s->explode_t = 0.0;
    }

    int64_t sid = d->session_id;
    KainWin32UiHost* host = d->host;
    abi_ui_begin_frame(sid, d->dt * 1000.0);
    ui_widget_begin_frame(d->widget_ctx);

    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width, fb_h = host->height;

    if (fb && fb_w > 0 && fb_h > 0) {
        // Clear
        uint64_t pat = ((uint64_t)0xFF08081A << 32) | 0xFF08081A;
        int total = fb_w * fb_h;
        for (int i = 0; i < total/2; i++) memcpy(&fb[i*2], &pat, 8);
        if (total & 1) fb[total-1] = 0xFF08081A;

        draw_stars(d, fb, stride, fb_w, fb_h, d->dt);
        draw_isometric_grid(fb, stride, fb_w, fb_h);
        if (!s->paused) update_particles(d, d->dt);

        // Floating panels
        double ds = d->dpi_scale;
        struct { double depth, bx, by, bw, bh; uint32_t accent; const char* title; } pdef[4] = {
            {50, 200*ds, 150*ds, 270*ds, 210*ds, 0xFF21D4A1, "Transform Controls"},
            {100, 540*ds, 130*ds, 240*ds, 200*ds, 0xFF4A90D9, "Color Picker"},
            {160, 190*ds, 390*ds, 250*ds, 180*ds, 0xFFE8914A, "Scene Info"},
            {220, 510*ds, 390*ds, 280*ds, 210*ds, 0xFFE84A5F, "Log"},
        };
        for (int pi = 3; pi >= 0; pi--) {
            if (!s->panel_visible[pi]) continue;
            int px, py, pw, ph;
            panel_3d_transform(fb_w, fb_h, pdef[pi].depth, pdef[pi].bx, pdef[pi].by,
                               pdef[pi].bw, pdef[pi].bh, &px, &py, &pw, &ph);
            if (pw < 40 || ph < 30) continue;
            if (px+pw < 0 || px > fb_w) continue;
            draw_panel_frame(d, px, py, pw, ph, pdef[pi].title, pdef[pi].accent);
            int cx = px+12, cy = py+34, cw = pw-24;
            KainUiWidgetContext* ctx = d->widget_ctx;
            switch (pi) {
            case 0:
                ui_widget_draw_text(ctx, cx, cy, "X Rotation Speed", 0xFF8888A0, 11); cy+=14;
                slider_widget(d, cx, cy, cw, 18, &s->speed_x, 0.0, 3.0, &d->sld_x); cy+=24;
                ui_widget_draw_text(ctx, cx, cy, "Y Rotation Speed", 0xFF8888A0, 11); cy+=14;
                slider_widget(d, cx, cy, cw, 18, &s->speed_y, 0.0, 3.0, &d->sld_y); cy+=24;
                ui_widget_draw_text(ctx, cx, cy, "Z Rotation Speed", 0xFF8888A0, 11); cy+=14;
                slider_widget(d, cx, cy, cw, 18, &s->speed_z, 0.0, 3.0, &d->sld_z); cy+=24;
                ui_widget_draw_text(ctx, cx, cy, "Cube Size", 0xFF8888A0, 11); cy+=14;
                slider_widget(d, cx, cy, cw, 18, &s->cube_size, 0.3, 3.0, &d->sld_size); cy+=24;
                { int bw2=(cw-8)/2;
                  if (btn_widget(d, cx, cy, bw2, 24, "Explode", &d->btn_explode)) {
                      s->explode_mode=!s->explode_mode; s->explode_t=s->explode_mode?0.01:s->explode_t;
                      log_msg(d, s->explode_mode?"Explode ON":"Explode OFF"); }
                  if (btn_widget(d, cx+bw2+8, cy, bw2, 24, "Reset View", &d->btn_reset)) {
                      s->rot_x=s->rot_y=s->rot_z=0; s->speed_x=0.5; s->speed_y=0.7; s->speed_z=0.3; s->cube_size=1.0;
                      log_msg(d, "View reset"); } }
                break;
            case 1:
                if (btn_widget(d, cx, cy, cw, 22, "Cycle Colors", &d->btn_cycle)) apply_palette(d, s->palette_idx+1);
                cy+=28;
                if (btn_widget(d, cx, cy, cw, 22, "Toggle Wireframe", &d->btn_wire)) {
                    s->wireframe_mode=!s->wireframe_mode; log_msg(d, s->wireframe_mode?"Wireframe ON":"Wireframe OFF"); }
                cy+=30;
                checkbox_widget(d, cx, cy, "Orthographic", &s->ortho_mode, &d->chk_ortho); cy+=22;
                if (btn_widget(d, cx, cy, cw, 22, "Particle Fountain!", &d->btn_fountain)) spawn_fountain(d);
                cy+=28;
                { int sw2=(cw-15)/6; if (sw2<8) sw2=8;
                  for (int fi=0; fi<6; fi++) {
                      int sx2=cx+fi*(sw2+3); uint32_t sc=s->face_colors[fi]|0xFF000000;
                      ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, sx2, cy, sw2, sw2, sc, 3); } }
                break;
            case 2: {
                char buf[128]; int64_t fid = s->font_ids[1];
                snprintf(buf, 128, "RX:%6.1f  RY:%6.1f  RZ:%6.1f", s->rot_x*180.0/3.14159, s->rot_y*180.0/3.14159, s->rot_z*180.0/3.14159);
                if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF21D4A1, 0, fid); else ui_widget_draw_text(ctx, cx, cy, buf, 0xFF21D4A1, 11); cy+=16;
                snprintf(buf, 128, "FPS:%6.1f", s->fps);
                if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF4A90D9, 0, fid); else ui_widget_draw_text(ctx, cx, cy, buf, 0xFF4A90D9, 11); cy+=16;
                snprintf(buf, 128, "Frame:%7d", s->frame_count);
                if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFF8888A0, 0, fid); else ui_widget_draw_text(ctx, cx, cy, buf, 0xFF8888A0, 11); cy+=16;
                snprintf(buf, 128, "Particles:%3d", s->particle_count);
                if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFFE8914A, 0, fid); else ui_widget_draw_text(ctx, cx, cy, buf, 0xFFE8914A, 11); cy+=16;
                snprintf(buf, 128, "Mode: %s | %s", s->ortho_mode?"Ortho":"Persp", s->wireframe_mode?"Wire":"Solid");
                if (fid) ui_widget_draw_text_ex(ctx, cx, cy, buf, 0xFFE84A5F, 0, fid); else ui_widget_draw_text(ctx, cx, cy, buf, 0xFFE84A5F, 11);
                if (s->paused) ui_widget_draw_text(ctx, cx, cy+16, " >> PAUSED << ", 0xFFFF4444, 14);
                break;
            }
            case 3: {
                int64_t fid = s->font_ids[1]; int ly = cy;
                for (int i = 0; i < s->log_count; i++) {
                    if (ly > py+ph-8) break;
                    if (fid) ui_widget_draw_text_ex(ctx, cx, ly, s->log_lines[i], 0xFF8888A0, 0, fid);
                    else ui_widget_draw_text(ctx, cx, ly, s->log_lines[i], 0xFF8888A0, 10);
                    ly+=15;
                }
                break;
            }
            }
        }

        draw_cube(d, fb, stride, fb_w, fb_h, d->dt);
        draw_particles(d, fb, stride, fb_w, fb_h);

        // HUD
        {
            char buf[64];
            snprintf(buf, 64, "FPS: %.0f", s->fps);
            ui_widget_draw_text(d->widget_ctx, 8, 8, buf, 0xFF21D4A1, 12);
            if (s->paused)
                ui_widget_draw_text(d->widget_ctx, fb_w/2-40, 8, ">> PAUSED <<", 0xFFFF4444, 14);
            ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, 0, fb_h-22, fb_w, 22, 0x60000000, 0);
            ui_widget_draw_text(d->widget_ctx, 10, fb_h-18,
                "R:Reset  W:Wire  SPACE:Pause  1-4:Panels  ESC:Exit (Kain include)", 0xFF444466, 11);
        }
    }

    ui_widget_end_frame(d->widget_ctx);
    abi_ui_end_frame(sid);

    InvalidateRect(host->hwnd, NULL, FALSE);
    Sleep(16);

    return 0;
}

int sandbox_bridge_running(SandboxDemo* d) {
    return (d && d->host && d->host->running) ? 1 : 0;
}

void sandbox_bridge_set_keys(SandboxDemo* d, int key_mask) { if (d) d->key_mask = key_mask; }
void sandbox_bridge_set_mouse(SandboxDemo* d, int mx, int my, int md) { if (d) { d->mouse_x = mx; d->mouse_y = my; d->mouse_down = md; } }
