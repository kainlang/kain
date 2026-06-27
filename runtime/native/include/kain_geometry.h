#ifndef KAIN_GEOMETRY_H
#define KAIN_GEOMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════════════════
//  kain_geometry.h — Primitive geometry types for the Kain UI substrate
// ══════════════════════════════════════════════════════════════════════════
//  All coordinates are float (single-precision) for GPU compatibility.
//  Colors are float [0..1] RGBA for backend-agnostic representation.
//  Matrices are 2D affine (translate, scale, rotate) — row-major layout.
// ══════════════════════════════════════════════════════════════════════════

// ── Primitive types ──────────────────────────────────────────────────────

typedef struct kainRect {
    float x, y, w, h;
} kainRect;

typedef struct kainPoint {
    float x, y;
} kainPoint;

typedef struct kainSize {
    float w, h;
} kainSize;

// 2D affine matrix: [a b tx; c d ty; 0 0 1]
// Stored row-major: m[0]=a,  m[1]=b,  m[2]=tx,
//                    m[3]=c,  m[4]=d,  m[5]=ty
typedef struct kainMatrix {
    float m[6];
} kainMatrix;

// Color in float [0..1] space (GPU backend compatible)
typedef struct kainColor {
    float r, g, b, a;
} kainColor;

// Predefined color constants
#define KAIN_COLOR_TRANSPARENT ((kainColor){0.0f, 0.0f, 0.0f, 0.0f})
#define KAIN_COLOR_BLACK       ((kainColor){0.0f, 0.0f, 0.0f, 1.0f})
#define KAIN_COLOR_WHITE       ((kainColor){1.0f, 1.0f, 1.0f, 1.0f})
#define KAIN_COLOR_RED         ((kainColor){1.0f, 0.0f, 0.0f, 1.0f})
#define KAIN_COLOR_GREEN       ((kainColor){0.0f, 1.0f, 0.0f, 1.0f})
#define KAIN_COLOR_BLUE        ((kainColor){0.0f, 0.0f, 1.0f, 1.0f})
#define KAIN_COLOR_DARK_BG     ((kainColor){0.102f, 0.102f, 0.141f, 1.0f}) // #1A1A24

// ── Rect construction & operations ──────────────────────────────────────

static inline kainRect kain_rect_make(float x, float y, float w, float h) {
    kainRect r; r.x = x; r.y = y; r.w = w; r.h = h; return r;
}

static inline bool kain_rect_contains(kainRect r, kainPoint p) {
    return p.x >= r.x && p.x <= r.x + r.w &&
           p.y >= r.y && p.y <= r.y + r.h;
}

static inline bool kain_rect_overlaps(kainRect a, kainRect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

static inline kainRect kain_rect_intersect(kainRect a, kainRect b) {
    float x0 = a.x > b.x ? a.x : b.x;
    float y0 = a.y > b.y ? a.y : b.y;
    float x1 = (a.x + a.w < b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    float y1 = (a.y + a.h < b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x0 >= x1 || y0 >= y1) return kain_rect_make(0, 0, 0, 0);
    return kain_rect_make(x0, y0, x1 - x0, y1 - y0);
}

static inline kainRect kain_rect_union(kainRect a, kainRect b) {
    float x0 = a.x < b.x ? a.x : b.x;
    float y0 = a.y < b.y ? a.y : b.y;
    float x1 = (a.x + a.w > b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    float y1 = (a.y + a.h > b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    return kain_rect_make(x0, y0, x1 - x0, y1 - y0);
}

// ── Point construction & operations ─────────────────────────────────────

static inline kainPoint kain_point_make(float x, float y) {
    kainPoint p; p.x = x; p.y = y; return p;
}

static inline kainPoint kain_point_add(kainPoint a, kainPoint b) {
    return kain_point_make(a.x + b.x, a.y + b.y);
}

static inline kainPoint kain_point_sub(kainPoint a, kainPoint b) {
    return kain_point_make(a.x - b.x, a.y - b.y);
}

// ── Size construction ───────────────────────────────────────────────────

static inline kainSize kain_size_make(float w, float h) {
    kainSize s; s.w = w; s.h = h; return s;
}

// ── Color construction & operations ─────────────────────────────────────

static inline kainColor kain_color_rgba(float r, float g, float b, float a) {
    kainColor c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
}

// Convert 0xAARRGGBB uint32_t → float kainColor
static inline kainColor kain_color_from_u32(uint32_t argb) {
    return kain_color_rgba(
        (float)((argb >> 16) & 0xFF) / 255.0f,
        (float)((argb >>  8) & 0xFF) / 255.0f,
        (float)( argb        & 0xFF) / 255.0f,
        (float)((argb >> 24) & 0xFF) / 255.0f
    );
}

// Convert float kainColor → 0xAARRGGBB uint32_t
static inline uint32_t kain_color_to_u32(kainColor c) {
    uint32_t a = (uint32_t)(c.a * 255.0f + 0.5f);
    uint32_t r = (uint32_t)(c.r * 255.0f + 0.5f);
    uint32_t g = (uint32_t)(c.g * 255.0f + 0.5f);
    uint32_t b = (uint32_t)(c.b * 255.0f + 0.5f);
    if (a > 255) a = 255;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline kainColor kain_color_lerp(kainColor a, kainColor b, float t) {
    return kain_color_rgba(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    );
}

// Clamp a float to [0, 1]
static inline float kain_clampf(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline kainColor kain_color_clamp(kainColor c) {
    return kain_color_rgba(
        kain_clampf(c.r), kain_clampf(c.g),
        kain_clampf(c.b), kain_clampf(c.a)
    );
}

// ── Matrix construction & operations ────────────────────────────────────

static inline kainMatrix kain_matrix_identity(void) {
    kainMatrix m;
    m.m[0] = 1.0f; m.m[1] = 0.0f; m.m[2] = 0.0f;
    m.m[3] = 0.0f; m.m[4] = 1.0f; m.m[5] = 0.0f;
    return m;
}

static inline kainMatrix kain_matrix_translate(float tx, float ty) {
    kainMatrix m;
    m.m[0] = 1.0f; m.m[1] = 0.0f; m.m[2] = tx;
    m.m[3] = 0.0f; m.m[4] = 1.0f; m.m[5] = ty;
    return m;
}

static inline kainMatrix kain_matrix_scale(float sx, float sy) {
    kainMatrix m;
    m.m[0] = sx;   m.m[1] = 0.0f; m.m[2] = 0.0f;
    m.m[3] = 0.0f; m.m[4] = sy;   m.m[5] = 0.0f;
    return m;
}

static inline kainMatrix kain_matrix_rotate(float angle_rad) {
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    kainMatrix m;
    m.m[0] = c;    m.m[1] = -s;   m.m[2] = 0.0f;
    m.m[3] = s;    m.m[4] =  c;   m.m[5] = 0.0f;
    return m;
}

// Multiply two affine matrices: result = a * b
static inline kainMatrix kain_matrix_mul(kainMatrix a, kainMatrix b) {
    kainMatrix r;
    r.m[0] = a.m[0] * b.m[0] + a.m[1] * b.m[3];
    r.m[1] = a.m[0] * b.m[1] + a.m[1] * b.m[4];
    r.m[2] = a.m[0] * b.m[2] + a.m[1] * b.m[5] + a.m[2];
    r.m[3] = a.m[3] * b.m[0] + a.m[4] * b.m[3];
    r.m[4] = a.m[3] * b.m[1] + a.m[4] * b.m[4];
    r.m[5] = a.m[3] * b.m[2] + a.m[4] * b.m[5] + a.m[5];
    return r;
}

// Transform a point by an affine matrix
static inline kainPoint kain_matrix_transform_point(kainMatrix m, kainPoint p) {
    return kain_point_make(
        m.m[0] * p.x + m.m[1] * p.y + m.m[2],
        m.m[3] * p.x + m.m[4] * p.y + m.m[5]
    );
}

#ifdef __cplusplus
}
#endif

#endif /* KAIN_GEOMETRY_H */
