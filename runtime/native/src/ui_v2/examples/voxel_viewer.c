// ============================================================================
//  voxel_viewer.c — 3D Voxel Viewer Demo for Kaintana Win32 GDI Backend
//
//  A software-rendered 3D voxel scene with perspective projection, orbiting
//  camera, painter's-algorithm depth sorting, back-face culling, and
//  per-face shading. Renders directly to the Win32 GDI framebuffer using
//  win32_fb_fill_rect() for clears and a scanline fill for arbitrary quads.
//
//  Controls:
//    Left-click + drag  — orbit camera (yaw/pitch)
//    W / S              — zoom in / out
//    A / D              — pan left / right
//    Arrow Up / Down    — move forward / backward
//    Arrow Left / Right — strafe
//    R / F              — move camera up / down
//    Escape             — quit
//
//  Features:
//    - 14x10x14 block voxel world with layered terrain (stone, dirt, grass)
//    - Decorative trees (wood trunk + leaf canopy)
//    - Auto-rotation when not dragging
//    - Console FPS + voxel count output
//    - Window resize + DPI aware (queries fb dimensions each frame)
//    - Dark navy background
//
//  Compile (from ui_v2/):
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror -Wno-unused-function
//        -I X:/runtime/native/include -I . -D_WIN32
//        tree.c box_math.c damage.c draw_pixels.c arena.c hash_table.c
//        color.c attr_table.c kaintana_runtime_stubs.c
//        ../../src/core/arena.c ../../src/core/version.c
//        ../../src/core/component_surface.c ../../src/core/handle.c
//        ../../src/core/input_system.c
//        examples/voxel_viewer.c -o examples/voxel_viewer.exe
//        -lgdi32 -lws2_32 -lopengl32
//
//  Run:
//    ./examples/voxel_viewer.exe
// ============================================================================


// ═══════════════════════════════════════════════════════════════════════════════
//  INCLUDES
// ═══════════════════════════════════════════════════════════════════════════════

// UNICODE must be defined BEFORE windows.h so MAKEINTRESOURCE macros
// (IDC_ARROW, IDI_APPLICATION) use WCHAR* compatible with LoadCursorW,
// LoadIconW. The backend files define this, but since we include
// windows.h first, we must match.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>       // Sleep(), QueryPerformance*, VK_*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "kaintana.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  BACKEND: include the Win32 GDI backend .c files directly
//  (same pattern as examples/demo_minecraft_ui.c, demo_dashboard.c, etc.)
// ═══════════════════════════════════════════════════════════════════════════════
#include "backends/win32/host_win32.c"
#include "backends/win32/render_gdi.c"

// Debug utilities (statically included after host_win32.c so g_pBits is accessible)
#include "kaintana_debug.h"


// ═══════════════════════════════════════════════════════════════════════════════
//  CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Window ───────────────────────────────────────────────────────────────────
#define WIN_W           1024
#define WIN_H           768

// ── Voxel world dimensions ──────────────────────────────────────────────────
#define VOX_W           14
#define VOX_H           10
#define VOX_D           14

// ── Camera ──────────────────────────────────────────────────────────────────
#define CAM_DIST_INIT   28.0f
#define CAM_DIST_MIN    6.0f
#define CAM_DIST_MAX    60.0f
#define FOCAL_SCALE     0.45f          // focal = fb_h * FOCAL_SCALE
#define AUTO_ROTATE_SPEED  0.35f       // radians/sec

// ── Materials ───────────────────────────────────────────────────────────────
#define MAT_AIR         0
#define MAT_DIRT        1
#define MAT_STONE       2
#define MAT_GRASS       3
#define MAT_LEAVES      4
#define MAT_WOOD        5
#define MAT_SAND        6
#define MAT_SNOW        7
#define MAT_WATER       8
#define MAT_NUM_MATERIALS  9

// ── Material colors (opaque ARGB) ───────────────────────────────────────────
static const uint32_t MATERIAL_COLORS[MAT_NUM_MATERIALS] = {
    0x00000000,          // 0: air (unused)
    0xFF5C3A1E,          // 1: dirt (warm brown)
    0xFF7A7A7A,          // 2: stone (gray)
    0xFF4CAF50,          // 3: grass (green)
    0xFF2D6B1E,          // 4: leaves (dark forest green)
    0xFF8B6B3B,          // 5: wood (brown)
    0xFFE8D68C,          // 6: sand (tan)
    0xFFF0F0F0,          // 7: snow (white)
    0xFF3366CC,          // 8: water (blue)
};

// ═══════════════════════════════════════════════════════════════════════════════
//  3D MATH
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct { float x, y, z; } Vec3;

static Vec3 vec3(float x, float y, float z) {
    Vec3 v = {x, y, z};
    return v;
}

// ── Cube corner offsets (relative to center, half-size 0.5) ──────────────────
// Order: 0=bbl, 1=bbr, 2=tbl, 3=tbr, 4=bfl, 5=bfr, 6=tfl, 7=tfr
static const float CUBE_VERTS[8][3] = {
    {-0.5f, -0.5f, -0.5f},  // 0: bottom-back-left
    { 0.5f, -0.5f, -0.5f},  // 1: bottom-back-right
    {-0.5f,  0.5f, -0.5f},  // 2: top-back-left
    { 0.5f,  0.5f, -0.5f},  // 3: top-back-right
    {-0.5f, -0.5f,  0.5f},  // 4: bottom-front-left
    { 0.5f, -0.5f,  0.5f},  // 5: bottom-front-right
    {-0.5f,  0.5f,  0.5f},  // 6: top-front-left
    { 0.5f,  0.5f,  0.5f},  // 7: top-front-right
};

// ── Cube faces (4 vertex indices each, CCW outward winding) ──────────────────
static const int CUBE_FACES[6][4] = {
    {4, 5, 7, 6},  // front   (+Z)  normal = ( 0,  0,  1)
    {1, 0, 2, 3},  // back    (-Z)  normal = ( 0,  0, -1)
    {6, 7, 3, 2},  // top     (+Y)  normal = ( 0,  1,  0)
    {4, 0, 1, 5},  // bottom  (-Y)  normal = ( 0, -1,  0)
    {5, 1, 3, 7},  // right   (+X)  normal = ( 1,  0,  0)
    {4, 6, 2, 0},  // left    (-X)  normal = (-1,  0,  0)
};

// ── Face normals (outward, matches CUBE_FACES order) ────────────────────────
static const float FACE_NORMALS[6][3] = {
    { 0,  0,  1},  // front
    { 0,  0, -1},  // back
    { 0,  1,  0},  // top
    { 0, -1,  0},  // bottom
    { 1,  0,  0},  // right
    {-1,  0,  0},  // left
};

// Pre-rotated vertex offsets and normals (computed each frame)
static float g_rox[8], g_roy[8], g_roz[8];
static float g_rnx[6], g_rny[6], g_rnz[6];

// ═══════════════════════════════════════════════════════════════════════════════
//  VOXEL WORLD
// ═══════════════════════════════════════════════════════════════════════════════

// 3D grid: 0 = air, otherwise material index (1..MAT_NUM_MATERIALS-1)
static uint8_t g_world[VOX_W][VOX_H][VOX_D];
static int     g_voxel_count = 0;     // number of non-air voxels

// Simple height-map terrain generator using sine-based "noise"
static int terrain_height(int x, int z) {
    float fx = (float)(x - VOX_W/2) / (float)VOX_W * 6.2832f;
    float fz = (float)(z - VOX_D/2) / (float)VOX_D * 6.2832f;
    float h = 5.0f
        + 2.2f * sinf(fx * 0.7f + 1.3f) * cosf(fz * 0.8f + 0.5f)
        + 1.5f * cosf(fx * 1.2f + fz * 0.9f + 2.1f)
        + 0.8f * sinf(fx * 2.3f + fz * 1.7f + 3.7f);
    int ih = (int)(h + 0.5f);
    if (ih < 1)   ih = 1;
    if (ih >= VOX_H - 1) ih = VOX_H - 2;
    return ih;
}

// Material for a given (x,y,z) column at layer y
static int column_material(int x, int y, int z, int top) {
    (void)x; (void)z;
    if (y == top) {
        // Surface layer
        return MAT_GRASS;
    } else if (y >= top - 2) {
        // Sub-surface
        return MAT_DIRT;
    } else {
        // Deep
        return MAT_STONE;
    }
}

// Initialize the voxel world with terrain + decorations
static void world_init(void) {
    g_voxel_count = 0;
    memset(g_world, 0, sizeof(g_world));

    // ── Terrain layer ────────────────────────────────────────────────────
    for (int x = 0; x < VOX_W; x++) {
        for (int z = 0; z < VOX_D; z++) {
            int top = terrain_height(x, z);
            for (int y = 0; y <= top; y++) {
                int mat = column_material(x, y, z, top);
                g_world[x][y][z] = (uint8_t)mat;
                g_voxel_count++;
            }
        }
    }

    // ── Decorative trees on high terrain ─────────────────────────────────
    for (int x = 3; x < VOX_W - 3; x += 2) {
        for (int z = 3; z < VOX_D - 3; z += 2) {
            int top = terrain_height(x, z);
            if (top >= 5 && top <= 7 && (x + z) % 3 == 0) {
                // Trunk (3-4 blocks tall)
                int trunk_h = 3 + (x % 2);
                for (int ty = 1; ty <= trunk_h; ty++) {
                    int yy = top + ty;
                    if (yy < VOX_H) {
                        g_world[x][yy][z] = MAT_WOOD;
                        g_voxel_count++;
                    }
                }
                // Canopy (3x3x2 leaves)
                int base_y = top + trunk_h - 1;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        for (int dy = 0; dy <= 1; dy++) {
                            int lx = x + dx;
                            int lz = z + dz;
                            int ly = base_y + dy;
                            if (lx >= 0 && lx < VOX_W &&
                                lz >= 0 && lz < VOX_D &&
                                ly >= 0 && ly < VOX_H &&
                                g_world[lx][ly][lz] == MAT_AIR) {
                                g_world[lx][ly][lz] = MAT_LEAVES;
                                g_voxel_count++;
                            }
                        }
                    }
                }
            }
        }
    }
}


// ═══════════════════════════════════════════════════════════════════════════════
//  SOFTWARE RASTERIZER — scanline fill for convex quads
// ═══════════════════════════════════════════════════════════════════════════════

// Shade an ARGB color by a brightness factor [0..1]
static uint32_t shade_color(uint32_t color, float brightness) {
    if (brightness >= 1.0f) return color;
    if (brightness <= 0.0f) return 0xFF000000;
    uint32_t r = (uint32_t)(((color >> 16) & 0xFF) * brightness);
    uint32_t g = (uint32_t)(((color >>  8) & 0xFF) * brightness);
    uint32_t b = (uint32_t)((color & 0xFF) * brightness);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Fill a horizontal span of pixels at scanline y from x1 to x2 (exclusive x2)
static void fill_span(int x1, int x2, int y, uint32_t color) {
    if (y < 0 || y >= g_fb_height) return;
    if (x1 < 0) x1 = 0;
    if (x2 > g_fb_width) x2 = g_fb_width;
    if (x1 >= x2) return;
    uint32_t* row = (uint32_t*)((uint8_t*)g_pBits + y * g_fb_stride);
    uint32_t* ptr = row + x1;
    int count = x2 - x1;
    while (count--) { *ptr++ = color; }
}

// Fill a convex quadrilateral given 4 screen-space vertices.
// Uses bottom-exclusive edge rule to avoid double-fill on shared edges.
static void fill_quad(int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3,
                      uint32_t color)
{
    int xs[4] = {x0, x1, x2, x3};
    int ys[4] = {y0, y1, y2, y3};

    // Find Y range
    int y_min = ys[0], y_max = ys[0];
    for (int i = 1; i < 4; i++) {
        if (ys[i] < y_min) y_min = ys[i];
        if (ys[i] > y_max) y_max = ys[i];
    }
    if (y_min < 0) y_min = 0;
    if (y_max >= g_fb_height) y_max = g_fb_height - 1;
    if (y_min > y_max) return;

    // Scanline fill
    for (int y = y_min; y <= y_max; y++) {
        float x_vals[8];
        int n = 0;
        float yf = (float)y + 0.5f;  // pixel center

        for (int e = 0; e < 4; e++) {
            int e1 = e;
            int e2 = (e + 1) % 4;
            int ay = ys[e1], by = ys[e2];

            // Skip horizontal edges
            if (ay == by) continue;

            // Bottom-exclusive: include top vertex, exclude bottom vertex
            if ((ay < by && (yf < ay || yf >= by)) ||
                (ay > by && (yf > ay || yf <= by))) {
                continue;
            }

            float t = (yf - (float)ay) / (float)(by - ay);
            if (t >= 0.0f && t <= 1.0f) {
                x_vals[n++] = (float)xs[e1] + t * (float)(xs[e2] - xs[e1]);
            }
        }

        if (n < 2) continue;

        // Sort intersections (bubble sort for n=2 or 4)
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                if (x_vals[i] > x_vals[j]) {
                    float tmp = x_vals[i];
                    x_vals[i] = x_vals[j];
                    x_vals[j] = tmp;
                }
            }
        }

        // Fill spans between pairs
        for (int i = 0; i + 1 < n; i += 2) {
            int x_start = (int)(x_vals[i] + 0.5f);
            int x_end   = (int)(x_vals[i+1] + 0.5f);
            fill_span(x_start, x_end, y, color);
        }
    }
}


// ═══════════════════════════════════════════════════════════════════════════════
//  SCENE RENDERER
// ═══════════════════════════════════════════════════════════════════════════════

// Depth-sort entry
typedef struct {
    int     voxel_index;     // index into the flattened world (x*VOH*VOD + y*VOD + z)
    float   depth;           // camera-space Z (positive, higher = farther)
} DepthEntry;

// We need a linear index from (x,y,z). Since we scan all voxels sequentially,
// we can store the index computed during the first pass.
#define VOXEL_IDX(x,y,z)  ((x)*VOX_H*VOX_D + (y)*VOX_D + (z))

// Comparison for descending sort (farthest first)
static int depth_compare_desc(const void* a, const void* b) {
    float da = ((const DepthEntry*)a)->depth;
    float db = ((const DepthEntry*)b)->depth;
    if (da > db) return -1;
    if (da < db) return  1;
    return 0;
}

// Render one frame of the voxel scene into the GDI framebuffer.
// All rendering is done directly via pixel writes to g_pBits.
static void render_frame(int fb_w, int fb_h,
                          float yaw, float pitch, float cam_dist,
                          float target_x, float target_z,
                          float target_y)
{
    if (!g_pBits || fb_w <= 0 || fb_h <= 0) return;

    // ── Pre-compute sin/cos ──────────────────────────────────────────────
    float sy = sinf(yaw), cy = cosf(yaw);
    float sp = sinf(pitch), cp = cosf(pitch);
    float focal = (float)fb_h * FOCAL_SCALE;

    // ── Clear framebuffer to dark background ─────────────────────────────
    uint32_t bg = 0xFF1A1A2E;  // dark navy
    uint32_t* fb = (uint32_t*)g_pBits;
    int total_px = fb_w * fb_h;
    for (int i = 0; i < total_px; i++) fb[i] = bg;


    // ── Pre-rotate cube corner offsets ───────────────────────────────────
    for (int i = 0; i < 8; i++) {
        float x = CUBE_VERTS[i][0];
        float y = CUBE_VERTS[i][1];
        float z = CUBE_VERTS[i][2];
        // Rotate around Y
        float rx1 = x * cy - z * sy;
        float rz1 = x * sy + z * cy;
        // Rotate around X
        float ry2 = y * cp - rz1 * sp;
        float rz2 = y * sp + rz1 * cp;
        g_rox[i] = rx1;
        g_roy[i] = ry2;
        g_roz[i] = rz2;
    }

    // ── Pre-rotate face normals ──────────────────────────────────────────
    for (int i = 0; i < 6; i++) {
        float x = FACE_NORMALS[i][0];
        float y = FACE_NORMALS[i][1];
        float z = FACE_NORMALS[i][2];
        float rx1 = x * cy - z * sy;
        float rz1 = x * sy + z * cy;
        float ry2 = y * cp - rz1 * sp;
        float rz2 = y * sp + rz1 * cp;
        g_rnx[i] = rx1;
        g_rny[i] = ry2;
        g_rnz[i] = rz2;
    }

    // ── Pass 1: collect visible voxels with depth ────────────────────────
    // We allocate depth entries on the stack. Max non-air voxels = VOX_W*VOX_H*VOX_D.
    DepthEntry depth_buf[VOX_W * VOX_H * VOX_D];
    int n_visible = 0;

    for (int x = 0; x < VOX_W; x++) {
        for (int y = 0; y < VOX_H; y++) {
            for (int z = 0; z < VOX_D; z++) {
                uint8_t mat = g_world[x][y][z];
                if (mat == MAT_AIR) continue;

                // World position with panning offset
                float wx = (float)(x - VOX_W/2) + target_x;
                float wy = (float)(y - VOX_H/2) + target_y;
                float wz = (float)(z - VOX_D/2) + target_z;

                // Rotate center position
                float cz = wx * sy + wz * cy;
                float cz_rot = wy * sp + cz * cp;

                // Camera-space Z (positive = in front of camera)
                float depth = cz_rot + cam_dist;
                if (depth <= 0.5f) continue;  // behind camera

                depth_buf[n_visible].voxel_index = VOXEL_IDX(x, y, z);
                depth_buf[n_visible].depth = depth;
                n_visible++;
            }
        }
    }

    if (n_visible == 0) return;

    // Sort by depth descending (painter's algorithm: far to near)
    qsort(depth_buf, (size_t)n_visible, sizeof(DepthEntry), depth_compare_desc);

    // ── Pass 2: draw voxels back-to-front ────────────────────────────────
    float cx_off = (float)VOX_W * 0.5f;
    float cy_off = (float)VOX_H * 0.5f;
    float cz_off = (float)VOX_D * 0.5f;
    int half_w = fb_w / 2;
    int half_h = fb_h / 2;

    for (int vi = 0; vi < n_visible; vi++) {
        int idx = depth_buf[vi].voxel_index;
        int ix = idx / (VOX_H * VOX_D);
        int iy = (idx / VOX_D) % VOX_H;
        int iz = idx % VOX_D;

        uint8_t mat = g_world[ix][iy][iz];
        uint32_t base_color = MATERIAL_COLORS[mat];

        // World position with panning offset
        float wx = (float)(ix) - cx_off + target_x;
        float wy = (float)(iy) - cy_off + target_y;
        float wz = (float)(iz) - cz_off + target_z;

        // Rotate center
        float rcx = wx * cy - wz * sy;
        float rcz1 = wx * sy + wz * cy;
        float rcy = wy * cp - rcz1 * sp;
        float rcz = wy * sp + rcz1 * cp;

        // For each of the 6 cube faces, check visibility and draw
        for (int f = 0; f < 6; f++) {
            // Back-face culling: face is visible if rotated normal
            // points toward camera (camera at -Z looking +Z).
            // Simplified check: rnz < 0 means normal has a -Z component
            // (towards camera). For off-center voxels use full dot.
            float fc_x = rcx + g_rnx[f] * 0.5f;
            float fc_y = rcy + g_rny[f] * 0.5f;
            float fc_z = rcz + g_rnz[f] * 0.5f;

            // View direction = camera_pos - face_center
            // Camera at (0, 0, -cam_dist)
            float vx = -fc_x;
            float vy = -fc_y;
            float vz = -cam_dist - fc_z;

            float ndot = g_rnx[f]*vx + g_rny[f]*vy + g_rnz[f]*vz;
            if (ndot <= 0.0f) continue;  // back face

            // Compute brightness from normal alignment with view
            float vlen = sqrtf(vx*vx + vy*vy + vz*vz);
            float brightness;
            if (vlen > 0.0001f) {
                brightness = ndot / vlen;           // cos(angle) in [0,1]
                brightness = 0.55f + 0.45f * brightness;  // map to [0.55, 1.0]
            } else {
                brightness = 0.75f;
            }
            if (brightness < 0.4f) brightness = 0.4f;
            if (brightness > 1.0f) brightness = 1.0f;

            uint32_t face_color = shade_color(base_color, brightness);

            // Project face vertices to screen space
            int sx[4], sy_px[4];
            int valid = 1;

            for (int v = 0; v < 4; v++) {
                int vi_idx = CUBE_FACES[f][v];
                float px = rcx + g_rox[vi_idx];
                float py = rcy + g_roy[vi_idx];
                float pz = rcz + g_roz[vi_idx];

                // Perspective projection
                float cam_z = pz + cam_dist;
                if (cam_z <= 0.3f) { valid = 0; break; }

                sx[v] = half_w + (int)(px * focal / cam_z);
                sy_px[v] = half_h - (int)(py * focal / cam_z);
            }

            if (!valid) continue;

            // Draw the face as a filled quad
            fill_quad(sx[0], sy_px[0], sx[1], sy_px[1],
                      sx[2], sy_px[2], sx[3], sy_px[3],
                      face_color);
        }
    }
    // ── DIAGNOSTIC: bright white corner mark (top-left 4x4) ────────────
    // This is drawn ON TOP of voxels. If visible, pixel writes work.
    // If not visible, the framebuffer pointer or write pattern is wrong.
#if 1
    {
        int mk_size = 8;
        for (int dy = 0; dy < mk_size && dy < fb_h; dy++) {
            uint32_t* row = (uint32_t*)((uint8_t*)g_pBits + (size_t)dy * (size_t)g_fb_stride);
            for (int dx = 0; dx < mk_size && dx < fb_w; dx++) {
                row[dx] = 0xFFFFFFFF;  // solid white
            }
        }
    }
#endif
}


// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(void) {
    // ── Initialize Kaintana ──────────────────────────────────────────────
    kt_init();
    kt_Session* s = kt_make("voxel", WIN_W, WIN_H);
    if (!s) {
        fprintf(stderr, "FAIL: kt_make returned NULL\n");
        return 1;
    }

    // Register and select the Win32 GDI backend
    // (this creates the window and DIB framebuffer)
    kt_backend_register(s, "win32", &kaintana_win32_backend);
    kt_backend_select(s, "win32");

    // Set window title
    HWND hwnd = FindWindowW(L"KaintanaWin32Window", NULL);
    if (hwnd) {
        SetWindowTextW(hwnd, L"Kaintana Voxel Viewer");
    }

    // Give the window a moment to initialize
    Sleep(100);
    win32_pump_messages();

    // ── Initialize world ─────────────────────────────────────────────────
    world_init();
    printf("Voxel Viewer: %d voxels loaded (%dx%dx%d world)\n",
           g_voxel_count, VOX_W, VOX_H, VOX_D);

    // ── Camera state ─────────────────────────────────────────────────────
    float yaw          = 0.0f;
    float pitch        = 0.35f;         // slight downward angle
    float cam_dist     = CAM_DIST_INIT;
    float target_x     = 0.0f;
    float target_z     = 0.0f;
    float target_y     = 0.0f;
    float prev_mx      = 0.0f;
    float prev_my      = 0.0f;
    int   was_dragging = 0;
    int   frame_counter = 0;                  // incremented each frame for debug logging

    // ── Performance timer ────────────────────────────────────────────────
    LARGE_INTEGER perf_freq, perf_prev;
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&perf_prev);

    double fps_accum    = 0.0;
    int    fps_frames   = 0;
    double last_fps_log = 0.0;

    // ── Main loop ────────────────────────────────────────────────────────
    while (!win32_should_close()) {
        // Process Windows messages (input, resize, paint, etc.)
        win32_pump_messages();

        // Timing
        LARGE_INTEGER perf_now;
        QueryPerformanceCounter(&perf_now);
        double dt = (double)(perf_now.QuadPart - perf_prev.QuadPart)
                    / (double)perf_freq.QuadPart;
        perf_prev = perf_now;

        // Clamp dt to prevent huge jumps (e.g. after debugging breakpoint)
        if (dt > 0.1) dt = 0.016;

        // Get current framebuffer size (handles resize + DPI)
        int fb_w = win32_get_fb_width();
        int fb_h = win32_get_fb_height();
        if (fb_w <= 0 || fb_h <= 0) {
            Sleep(16);
            continue;
        }

        // ── Mouse input: orbit ───────────────────────────────────────────
        float mx = win32_get_mouse_x();
        float my = win32_get_mouse_y();
        int   left_down = win32_get_mouse_down(0);

        if (left_down) {
            if (was_dragging) {
                float dx = mx - prev_mx;
                float dy = my - prev_my;
                yaw   -= dx * 0.008f;
                pitch += dy * 0.008f;
            }
            was_dragging = 1;
        } else {
            was_dragging = 0;
            // Auto-rotate when not dragging
            yaw += AUTO_ROTATE_SPEED * (float)dt;
        }

        // Clamp pitch to prevent over-rotation
        if (pitch >  1.5f) pitch =  1.5f;
        if (pitch < -0.5f) pitch = -0.5f;

        prev_mx = mx;
        prev_my = my;

        // ── Keyboard input: camera movement ──────────────────────────────
        float move_speed = 6.0f * (float)dt;

        // Zoom (W/S)
        if (win32_get_key('W') || win32_get_key(VK_UP)) {
            cam_dist -= move_speed;
        }
        if (win32_get_key('S') || win32_get_key(VK_DOWN)) {
            cam_dist += move_speed;
        }
        if (cam_dist < CAM_DIST_MIN) cam_dist = CAM_DIST_MIN;
        if (cam_dist > CAM_DIST_MAX) cam_dist = CAM_DIST_MAX;

        // Pan (A/D, Left/Right)
        if (win32_get_key('A') || win32_get_key(VK_LEFT)) {
            target_x -= move_speed * 0.5f;
        }
        if (win32_get_key('D') || win32_get_key(VK_RIGHT)) {
            target_x += move_speed * 0.5f;
        }

        // Vertical adjustment (R/F)
        if (win32_get_key('R')) {
            target_y += move_speed * 0.3f;
        }
        if (win32_get_key('F')) {
            target_y -= move_speed * 0.3f;
        }

        // ── F12: dump framebuffer to .bin file ────────────────────────────
        if (win32_get_key(VK_F12)) {
            char dump_path[64];
            snprintf(dump_path, sizeof(dump_path),
                     "voxel_dump_%d.bin", frame_counter);
            int ret = kt_debug_dump_fb(dump_path, g_pBits,
                                        fb_w, fb_h, g_fb_stride);
            printf("F12: framebuffer dump -> %s (%s)\n",
                   dump_path, (ret == 0) ? "OK" : "FAIL");
        }


        // Quit on Escape
        if (win32_get_key(VK_ESCAPE)) {
            break;
        }

        // ── Render the voxel scene ───────────────────────────────────────
        render_frame(fb_w, fb_h, yaw, pitch, cam_dist,
                     target_x, target_z, target_y);

        // ── Present to screen ────────────────────────────────────────────
        // Since we wrote directly to the framebuffer, schedule a present.
        g_needs_present = true;
        win32_present_to_screen();

        // Increment frame counter for debug logging
        frame_counter++;

        // ── FPS counter ──────────────────────────────────────────────────
        fps_accum  += dt;
        fps_frames++;
        if (fps_accum - last_fps_log >= 1.0) {
            printf("Kaintana Voxel Viewer — FPS: %d | Voxels: %d | Drawn: ~%d\n",
                   fps_frames, g_voxel_count,
                   g_voxel_count * 3);  // ~3 visible faces per voxel average
            last_fps_log = fps_accum;
            fps_frames   = 0;

        // Debug trace (only when compiled with -DDEBUG)
        KT_DEBUG_LOG("frame=%d yaw=%.2f pitch=%.2f dist=%.1f",
                     frame_counter, yaw, pitch, cam_dist);
        }

        // ── Frame rate cap (target ~60 FPS) ──────────────────────────────
        LARGE_INTEGER now2;
        QueryPerformanceCounter(&now2);
        double elapsed = (double)(now2.QuadPart - perf_now.QuadPart)
                        / (double)perf_freq.QuadPart;
        double target_frame = 1.0 / 60.0;
        if (elapsed < target_frame) {
            DWORD sleep_ms = (DWORD)((target_frame - elapsed) * 1000.0);
            if (sleep_ms > 0 && sleep_ms < 50) {
                Sleep(sleep_ms);
            }
        }
    }

    // ── Cleanup ──────────────────────────────────────────────────────────
    printf("Voxel Viewer shutting down.\n");
    kt_free(s);
    return 0;
}
