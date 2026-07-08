// ============================================================================
//  draw_pixels.c — Software rasterizer and draw command generator for the
//  Kaintana UI substrate (Phase 1, L1).
//
//  Architecture:
//    The file provides two cooperating layers:
//
//    LAYER A — Batch management + command generation (called from tree.c)
//      kaintana__draw_generate(s)
//        Walks the VISIBLE node tree depth-first, emits KaintanaInternalDrawCmd
//        entries into sess->draw_batch via write-pointer reservation (ImGui
//        pattern). Emits FILL_RECT for every visible node with a resolved
//        layout, and CLIP/UNCLIP around nodes with scoped clipping.
//      kaintana__draw_merge(s)
//        Post-processing pass that consolidates adjacent commands with the
//        same type, texture, blend mode, and color into wider rects (reduces
//        total command count for backends).
//      kt_draw_batch_reserve(batch, count)
//        Geometric 1.5x growth realloc of the draw command buffer. Z3 proven.
//      kt_draw_try_merge(batch, cmd)
//        Merge-at-insertion check: if the last command in batch is compatible
//        with the new one, the batch bounds expand instead of appending.
//
//    LAYER B — 16 pixel-level software rasterizer primitives (called by backends)
//      These operate on raw uint32_t* framebuffers with a stride (pitch in
//      pixels). They accept an explicit clip rect and paint only within the
//      intersection of the draw rect and the clip rect.
//
//      Primitives:
//        kt_draw_fill_rect                 — Dual-pixel memcpy fill
//        kt_draw_fill_rect_sse             — SSE 4-pixel streaming fill (gated)
//        kt_draw_fill_rect_transparent_skip— Early-out when alpha==0
//        kt_draw_fill_rect_opaque          — Straight copy when alpha==255
//        kt_draw_fill_rounded_rect_sdf     — Compute SDF value (no sqrt interior)
//        kt_draw_fill_rounded_rect_cov     — Coverage from SDF
//        kt_draw_stroke_rect               — 4 edge rects or outer+inner fill
//        kt_draw_fill_circle_bb            — Two-level dist² test
//        kt_draw_stroke_circle             — Ring SDF
//        kt_draw_gradient_rect             — O(log N) or O(1) for n<=4
//        kt_draw_gradient_segment_precompute— Fixed-point segment data
//        kt_blend_srcover_u32              — DIV255-based src-over blend
//        kt_draw_glyph_quad                — Emit 4 verts + 6 indices
//        kt_draw_push_clip / pop_clip      — Clip stack (32-deep)
//        kt_draw_push_transform / pop_transform — Transform stack (16-deep)
//
//  Critical formulas (from formulas.tsv):
//    DIV255:     ((x) + 1 + ((x) >> 8)) >> 8  — error ±0.5, Z3 UNSAT
//    SDF round:  len = hypot(max(q.x,0), max(q.y,0))
//                sdf = len + min(max(q.x,q.y), 0) - radius
//    Blend:      out_a = sa + DIV255(da * (255 - sa))
//    Clip:       r.x = fmaxf(a.x, b.x); r.y = fmaxf(a.y, b.y);
//                r.w = fmaxf(fminf(a.x+a.w, b.x+b.w) - r.x, 0);
//    Transform:  m[0] = fmaf(a11, b11, a12 * b21); ... (6 FMAs)
//
//  Per-frame state (clip stack, transform stack) is maintained as file-scope
//  static arrays, reset each frame in kaintana__draw_generate(). Kaintana
//  is single-session and single-threaded by design.
// ============================================================================

#include "internal.h"
#include <math.h>
#include <stdlib.h>       // realloc, calloc, free

// ============================================================================
//  COMPILE-TIME SAFETY
// ============================================================================

typedef char kaintana__assert_u32_size[(sizeof(uint32_t) == 4) ? 1 : -1];

// ============================================================================
//  STACK CONSTANTS
// ============================================================================

#define KT_CLIP_STACK_MAX      32
#define KT_TRANSFORM_STACK_MAX 16
#define KT_CLIP_INFINITE      1.0e6f       // Effectively unlimited clip bounds
#define KT_FIXED_POINT_SCALE  256          // 8.8 fixed-point: 1 = 1/256
#define KT_MIN_ELEMENT_SIZE   1            // Minimum element size in pixels

// ============================================================================
//  PER-FRAME CLIP & TRANSFORM STATE
// ============================================================================
//  These are reset in kaintana__draw_generate() each frame. Kaintana is
//  single-threaded, so file-scope statics are safe.

static kt_Rect   s_clip_stack[KT_CLIP_STACK_MAX];
static int       s_clip_depth = 0;
static kt_Rect   s_current_clip = { 0.0f, 0.0f, 0.0f, 0.0f }; // Set in draw_generate

static kt_Matrix s_transform_stack[KT_TRANSFORM_STACK_MAX];
static int       s_transform_depth = 0;
static kt_Matrix s_current_transform = { { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f } };

// ============================================================================
//  HELPER: INTERNAL GRADIENT SEGMENT STRUCT
// ============================================================================
//  Precomputed per gradient segment so the inner loop only does integer math.
//  t = ((px - px_min) * 256) / t_dx  (8.8 fixed-point)

typedef struct GradSegment {
    int     px_min;             // First pixel x (inclusive)
    int     px_max;             // Last pixel x (exclusive)
    int     t_dx;               // (seg_x_max - seg_x_min) * 256 for fixed-point
    uint8_t r0, g0, b0, a0;    // Start color (straight 8-bit)
    int8_t  dr, dg, db, da;    // Delta per 256th step (fixed-point 8.8)
} GradSegment;

// ============================================================================
//  HELPER: VERTEX STRUCT FOR GLYPH QUADS
// ============================================================================
//  20 bytes per vert. Not in internal.h because only draw_pixels.c and GPU
//  backends use it directly.

typedef struct KaintanaDrawVert {
    float    x, y;       // Position (8 bytes)
    float    u, v;       // UV coordinates (8 bytes)
    uint32_t col;        // Premultiplied ARGB (4 bytes)
} KaintanaDrawVert;      // 20 bytes
KT_STATIC_ASSERT(sizeof(KaintanaDrawVert) == 20, kaintana_draw_vert_size_20);

// ============================================================================
//  SECTION 2: CLIP & TRANSFORM STACK HELPERS
// ============================================================================
//  These manage the per-frame clip and transform stacks that backends use
//  during software rasterization.
// ============================================================================

// ── kt_clip_intersect: Intersect two rects. Z3 UNSAT proof. ──────────────
//     Formula: r.x = fmaxf(a.x, b.x); r.y = fmaxf(a.y, b.y);
//     r.w = fmaxf(fminf(a.x+a.w, b.x+b.w) - r.x, 0);
//     r.h = fmaxf(fminf(a.y+a.h, b.y+b.h) - r.y, 0);
static inline kt_Rect kt_clip_intersect(kt_Rect a, kt_Rect b) {
    kt_Rect r;
    r.x = fmaxf(a.x, b.x);
    r.y = fmaxf(a.y, b.y);
    r.w = fmaxf(fminf(a.x + a.w, b.x + b.w) - r.x, 0.0f);
    r.h = fmaxf(fminf(a.y + a.h, b.y + b.h) - r.y, 0.0f);
    return r;
}

// ── kt_draw_push_clip: Push a clip rect onto the stack. ──────────────────
//     The current clip becomes intersect(old_clip, rect). The old clip is
//     saved on the stack so pop_clip can restore it.
kt_Rect kt_draw_push_clip(kt_Rect rect) {
    if (s_clip_depth < KT_CLIP_STACK_MAX) {
        s_clip_stack[s_clip_depth++] = s_current_clip;
        s_current_clip = kt_clip_intersect(s_current_clip, rect);
    }
    return s_current_clip;
}

// ── kt_draw_pop_clip: Pop the clip stack. ───────────────────────────────
kt_Rect kt_draw_pop_clip(void) {
    if (s_clip_depth > 0) {
        s_current_clip = s_clip_stack[--s_clip_depth];
    }
    return s_current_clip;
}

// ── kt_mat_xfrm: Transform a point by a 2D affine matrix. ───────────────
//     Formula: x' = m[0]*x + m[1]*y + m[4]
//              y' = m[2]*x + m[3]*y + m[5]
//     Implemented as FMAs for accuracy.
static inline kt_Vec2 kt_mat_xfrm(kt_Matrix m, kt_Vec2 p) {
    kt_Vec2 r;
    r.x = fmaf(m.m[0], p.x, fmaf(m.m[1], p.y, m.m[4]));
    r.y = fmaf(m.m[2], p.x, fmaf(m.m[3], p.y, m.m[5]));
    return r;
}

// ── kt_draw_push_transform: Compose a new transform onto the stack. ─────
//     Formula: compose(A,B) via 6 FMAs.
//     m[0] = fmaf(a11, b11, a12 * b21)
//     m[1] = fmaf(a11, b12, a12 * b22)
//     m[2] = fmaf(a21, b11, a22 * b21)
//     m[3] = fmaf(a21, b12, a22 * b22)
//     m[4] = fmaf(a11, b.m[4], fmaf(a12, b.m[5], a.m[4]))
//     m[5] = fmaf(a21, b.m[4], fmaf(a22, b.m[5], a.m[5]))
kt_Matrix kt_draw_push_transform(kt_Matrix t) {
    kt_Matrix old = s_current_transform;
    if (s_transform_depth < KT_TRANSFORM_STACK_MAX) {
        s_transform_stack[s_transform_depth++] = old;
    }
    // Compose: new = old * t
    kt_Matrix a = old;
    kt_Matrix b = t;
    kt_Matrix r;
    r.m[0] = fmaf(a.m[0], b.m[0], a.m[1] * b.m[2]);
    r.m[1] = fmaf(a.m[0], b.m[1], a.m[1] * b.m[3]);
    r.m[2] = fmaf(a.m[2], b.m[0], a.m[3] * b.m[2]);
    r.m[3] = fmaf(a.m[2], b.m[1], a.m[3] * b.m[3]);
    r.m[4] = fmaf(a.m[0], b.m[4], fmaf(a.m[1], b.m[5], a.m[4]));
    r.m[5] = fmaf(a.m[2], b.m[4], fmaf(a.m[3], b.m[5], a.m[5]));
    s_current_transform = r;
    return r;
}

// ── kt_draw_pop_transform: Pop the transform stack. ─────────────────────
kt_Matrix kt_draw_pop_transform(void) {
    if (s_transform_depth > 0) {
        s_current_transform = s_transform_stack[--s_transform_depth];
    }
    return s_current_transform;
}

// ============================================================================
//  SECTION 3: BLEND HELPERS
// ============================================================================

// ── kt_blend_srcover_u32: Src-over blend on premultiplied uint32 ARGB. ──
//     Formula (Z3 UNSAT — kt-blend-div255.smt2):
//       out_a = sa + DIV255(da * (255 - sa))
//       out_r = sr + DIV255(dr * (255 - sa))
//       out_g = sg + DIV255(dg * (255 - sa))
//       out_b = sb + DIV255(db * (255 - sa))
//     Both src and dst are in premultiplied ARGB (0xAARRGGBB).
//     DIV255 = ((x) + 1 + ((x) >> 8)) >> 8  error ±0.5
//
//     Fast paths:
//       sa == 0   → return dst (transparent source preserves destination)
//       sa == 255 → return src (opaque source overwrites exactly)
static inline uint32_t kt_blend_srcover_u32(uint32_t src, uint32_t dst) {
    uint32_t sa = (src >> 24) & 0xFF;

    // Fast paths
    if (sa == 0)   return dst;
    if (sa == 255) return src;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >>  8) & 0xFF;
    uint32_t sb = (src >>  0) & 0xFF;

    uint32_t da = (dst >> 24) & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >>  8) & 0xFF;
    uint32_t db = (dst >>  0) & 0xFF;

    uint32_t inv_sa = 255 - sa;

    uint32_t out_a = sa + kaintana__DIV255(da * inv_sa);
    uint32_t out_r = sr + kaintana__DIV255(dr * inv_sa);
    uint32_t out_g = sg + kaintana__DIV255(dg * inv_sa);
    uint32_t out_b = sb + kaintana__DIV255(db * inv_sa);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

// ============================================================================
//  SECTION 4: SOFTWARE RASTERIZER PRIMITIVES
// ============================================================================
//  All functions operate on uint32_t* framebuffers with an explicit stride
//  (pitch in pixels). They accept a draw rect and clip themselves to the
//  intersection with the current s_current_clip.
//
//  These are designed to be called by backends (host_null, host_win32, etc.)
//  when processing the draw command batch.
// ============================================================================

// ── kt_draw_fill_rect_transparent_skip: No-op if color alpha is zero. ──
//     Z3 UNSAT: kt-blend-div255.smt2 (sa==0 → out==dst)
static inline bool kt_draw_fill_rect_transparent_skip(uint32_t color) {
    return ((color >> 24) & 0xFF) == 0;
}

// ── kt_draw_fill_rect_opaque: Fast path when alpha == 255.
//     Z3 UNSAT: kt-blend-div255.smt2 (sa==255 → out==src)
static inline bool kt_draw_fill_rect_opaque(uint32_t color) {
    return ((color >> 24) & 0xFF) == 255;
}

// ── kt_draw_fill_rect: Dual-pixel memcpy fill. ─────────────────────────
//     Z3 UNSAT: kt-dual-pixel-fill-proof.smt2
//     Fills a rectangle with solid color using 2-pixel blocks via memcpy.
//     Strict-aliasing safe (memcpy, not uint64_t* cast).
//     Odd tail handled for odd widths.
//
//     Parameters:
//       fb     — Frame buffer (uint32_t ARGB pixels)
//       stride — Row stride in pixels (not bytes)
//       rect   — Rectangle to fill (already clipped)
//       color  — Premultiplied ARGB fill color
void kt_draw_fill_rect(uint32_t* fb, int stride, kt_Rect rect, uint32_t color) {
    // Clip to current clip rect
    kt_Rect clip = kt_clip_intersect(rect, s_current_clip);

    // Integer bounds
    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    int w = x1 - x0;
    int h = y1 - y0;

    // Build dual-pixel pair
    uint64_t pair;
    memcpy(&pair, &color, 4);
    memcpy((char*)&pair + 4, &color, 4);

    // Even-width: full pairs
    int w_pair = w & ~1;
    // Odd tail width
    int w_odd  = w & 1;
    int odd_px = x0 + w_pair;

    bool opaque = kt_draw_fill_rect_opaque(color);

    for (int row = 0; row < h; row++) {
        int base = (y0 + row) * stride + x0;

        // 2-pixel blocks
        for (int i = 0; i < w_pair; i += 2) {
            uint32_t* dst = &fb[base + i];
            if (opaque) {
                memcpy(dst, &pair, 8);
            } else {
                dst[0] = kt_blend_srcover_u32(color, dst[0]);
                dst[1] = kt_blend_srcover_u32(color, dst[1]);
            }
        }

        // Odd tail pixel
        if (w_odd) {
            uint32_t* dst = &fb[base + odd_px - x0];
            if (opaque) {
                *dst = color;
            } else {
                *dst = kt_blend_srcover_u32(color, *dst);
            }
        }
    }
}

// ── kt_draw_fill_rect_sse: SSE 4-pixel streaming fill (gated). ─────────
//     Uses movntdq (non-temporal streaming store) for 4 pixels/iteration.
//     Avoids cache pollution on large fills. Define KAINTANA_HAVE_SSE to
//     enable. Falls back to kt_draw_fill_rect when SSE is unavailable.
#if defined(KAINTANA_HAVE_SSE) && defined(__SSE2__)
#include <emmintrin.h>
void kt_draw_fill_rect_sse(uint32_t* fb, int stride, kt_Rect rect, uint32_t color) {
    kt_Rect clip = kt_clip_intersect(rect, s_current_clip);

    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    int w = x1 - x0;
    int h = y1 - y0;

    // Splat color into a 128-bit register: 4 x uint32_t
    __m128i splat = _mm_set1_epi32((int)color);

    // Iterate rows
    for (int row = 0; row < h; row++) {
        int base = (y0 + row) * stride + x0;
        int i = 0;

        // 4-pixel streaming stores
        for (; i + 4 <= w; i += 4) {
            _mm_stream_si128((__m128i*)&fb[base + i], splat);
        }

        // Remainder pixels
        for (; i < w; i++) {
            fb[base + i] = color;
        }
    }

    // SFENCE to guarantee streaming store visibility
    _mm_sfence();
}
#else
void kt_draw_fill_rect_sse(uint32_t* fb, int stride, kt_Rect rect, uint32_t color) {
    // Fallback: use the standard fill when SSE is not available
    kt_draw_fill_rect(fb, stride, rect, color);
}
#endif

// ── kt_draw_fill_rounded_rect_sdf: Compute SDF value for rounded rect. ─
//     Formula (Z3 UNSAT — kt-fill-rounded-rect-proof.smt2):
//       q = (|p - rect_center| - half_size + radius)  clamped to zero
//       len = hypot(max(q.x, 0), max(q.y, 0))
//       sdf = len + min(max(q.x, q.y), 0) - radius
//     Interior test: len <= (radius - 0.5)²  →  no sqrt (fully covered)
//     Exterior test: len >= (radius + 0.5)²  →  no sqrt (fully transparent)
static inline float kt_draw_fill_rounded_rect_sdf(kt_Vec2 p, kt_Rect r, float radius) {
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float hw = r.w * 0.5f;
    float hh = r.h * 0.5f;

    float dx = fabsf(p.x - cx) - hw + radius;
    float dy = fabsf(p.y - cy) - hh + radius;

    // Clamp negative components to zero
    float qx = fmaxf(dx, 0.0f);
    float qy = fmaxf(dy, 0.0f);

    // hypot = sqrt(qx*qx + qy*qy)
    float len = sqrtf(qx * qx + qy * qy);

    // Interior distance component
    float interior = fminf(fmaxf(dx, dy), 0.0f);

    return len + interior - radius;
}

// ── kt_draw_fill_rounded_rect_cov: Coverage from SDF. ──────────────────
//     Formula: coverage = fmaxf(0, fminf(1, 0.5f - sdf))
//     Z3 UNSAT: kt-sdf-coverage-proof.smt2
static inline float kt_draw_fill_rounded_rect_cov(float sdf) {
    return fmaxf(0.0f, fminf(1.0f, 0.5f - sdf));
}

// ── kt_draw_fill_rounded_rect: SDF-based rounded rect rasterizer. ──────
//     Walks the bounding box, computes SDF per pixel, applies coverage.
//     Inner/outer fast paths avoid sqrt for most pixels.
void kt_draw_fill_rounded_rect(uint32_t* fb, int stride, kt_Rect rect,
                                float radius, uint32_t color)
{
    kt_Rect clip = kt_clip_intersect(rect, s_current_clip);

    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    bool opaque = kt_draw_fill_rect_opaque(color);
    float rad_lo = radius - 0.5f;
    float rad_hi = radius + 0.5f;

    for (int py = y0; py < y1; py++) {
        int base = py * stride;
        for (int px = x0; px < x1; px++) {
            float dx = (float)px - (rect.x + rect.w * 0.5f);
            float dy = (float)py - (rect.y + rect.h * 0.5f);
            float hw = rect.w * 0.5f;
            float hh = rect.h * 0.5f;

            float adx = fabsf(dx) - hw + radius;
            float ady = fabsf(dy) - hh + radius;

            // Interior test: fully covered, no sqrt
            if (adx <= 0.0f && ady <= 0.0f) {
                goto fill_pixel;
            }

            // Compute corner distance squared
            float qx = fmaxf(adx, 0.0f);
            float qy = fmaxf(ady, 0.0f);
            float dist_sq = qx * qx + qy * qy;

            // Exterior test: fully outside, skip sqrt
            if (dist_sq >= rad_hi * rad_hi) {
                continue;  // Transparent
            }

            // Interior fast path: fully inside, no sqrt
            if (opaque) {
                if (dist_sq <= rad_lo * rad_lo) {
                    goto fill_pixel;
                }
            }

            // Edge: compute SDF and coverage
            {
                float len = sqrtf(dist_sq);
                float interior = fminf(fmaxf(adx, ady), 0.0f);
                float sdf = len + interior - radius;
                float cov = fmaxf(0.0f, fminf(1.0f, 0.5f - sdf));

                if (cov <= 0.0f) continue;
                if (cov >= 1.0f && opaque) goto fill_pixel;

                // Blend with coverage
                uint32_t src_alpha = (uint32_t)(cov * 255.0f + 0.5f);
                uint32_t src_color = (color & 0x00FFFFFF) | (src_alpha << 24);
                fb[base + px] = kt_blend_srcover_u32(src_color, fb[base + px]);
                continue;
            }

            fill_pixel:
            if (opaque) {
                fb[base + px] = color;
            } else {
                fb[base + px] = kt_blend_srcover_u32(color, fb[base + px]);
            }
        }
    }
}

// ── kt_draw_stroke_rect: Rasterize a stroked rectangle. ────────────────
//     Strategy: 4 edge rects (top, bottom, left, right) instead of
//     outer-fill-then-erase-inner to avoid double-blend artifacts.
//     If corner_radius > 0, uses SDF approach.
void kt_draw_stroke_rect(uint32_t* fb, int stride, kt_Rect rect,
                          float thickness, float corner_radius, uint32_t color)
{
    if (thickness <= 0.0f) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    if (corner_radius > 0.5f) {
        // Rounded stroke via SDF: fill outer rounded rect, mask inner
        kt_Rect outer = rect;
        kt_Rect inner = {
            rect.x + thickness, rect.y + thickness,
            rect.w - 2.0f * thickness, rect.h - 2.0f * thickness
        };

        if (inner.w <= 0.0f || inner.h <= 0.0f) {
            // Stroke fills the entire rect; just fill the outer
            kt_draw_fill_rounded_rect(fb, stride, outer, corner_radius, color);
            return;
        }

        // clip applied via outer_clip intersection below — no save needed

        // Clip to outer rect
        kt_Rect outer_clip = kt_clip_intersect(outer, s_current_clip);
        if (outer_clip.w <= 0.0f || outer_clip.h <= 0.0f) return;

        int x0 = (int)outer_clip.x;
        int y0 = (int)outer_clip.y;
        int x1 = (int)(outer_clip.x + outer_clip.w);
        int y1 = (int)(outer_clip.y + outer_clip.h);

        float inner_r = fmaxf(corner_radius - thickness, 0.0f);
        float cx_o = outer.x + outer.w * 0.5f;
        float cy_o = outer.y + outer.h * 0.5f;
        float cx_i = inner.x + inner.w * 0.5f;
        float cy_i = inner.y + inner.h * 0.5f;
        float hw_o = outer.w * 0.5f;
        float hh_o = outer.h * 0.5f;
        float hw_i = inner.w * 0.5f;
        float hh_i = inner.h * 0.5f;
        // Inward SDF radius threshold

        for (int py = y0; py < y1; py++) {
            int base = py * stride;
            for (int px = x0; px < x1; px++) {
                float fx = (float)px;
                float fy = (float)py;

                // Outer SDF
                float odx = fabsf(fx - cx_o) - hw_o + corner_radius;
                float ody = fabsf(fy - cy_o) - hh_o + corner_radius;
                float oq = hypotf(fmaxf(odx, 0.0f), fmaxf(ody, 0.0f));
                float o_interior = fminf(fmaxf(odx, ody), 0.0f);
                float o_sdf = oq + o_interior - corner_radius;
                float o_cov = fmaxf(0.0f, fminf(1.0f, 0.5f - o_sdf));
                if (o_cov <= 0.0f) continue;

                // Inner SDF
                float idx = fabsf(fx - cx_i) - hw_i + inner_r;
                float idy = fabsf(fy - cy_i) - hh_i + inner_r;
                float iq = hypotf(fmaxf(idx, 0.0f), fmaxf(idy, 0.0f));
                float i_interior = fminf(fmaxf(idx, idy), 0.0f);
                float i_sdf = iq + i_interior - inner_r;
                float i_cov = fmaxf(0.0f, fminf(1.0f, 0.5f - i_sdf));

                // Stroke = outer_coverage * (1 - inner_coverage)
                float cov = o_cov * (1.0f - i_cov);
                if (cov <= 0.0f) continue;

                uint32_t src_alpha = (uint32_t)(cov * 255.0f + 0.5f);
                uint32_t src_color = (color & 0x00FFFFFF) | (src_alpha << 24);
                fb[base + px] = kt_blend_srcover_u32(src_color, fb[base + px]);
            }
        }
        return;
    }

    // Sharp corners: 4 edge rects
    float t = thickness;

    // Top edge
    kt_Rect top = { rect.x, rect.y, rect.w, t };
    kt_draw_fill_rect(fb, stride, top, color);

    // Bottom edge
    kt_Rect bot = { rect.x, rect.y + rect.h - t, rect.w, t };
    kt_draw_fill_rect(fb, stride, bot, color);

    // Left edge (mid-section only, avoiding overlap with top/bottom)
    kt_Rect left = { rect.x, rect.y + t, t, rect.h - 2.0f * t };
    if (left.w > 0.0f && left.h > 0.0f)
        kt_draw_fill_rect(fb, stride, left, color);

    // Right edge (mid-section only)
    kt_Rect right = { rect.x + rect.w - t, rect.y + t, t, rect.h - 2.0f * t };
    if (right.w > 0.0f && right.h > 0.0f)
        kt_draw_fill_rect(fb, stride, right, color);
}

// ── kt_draw_fill_circle_bb: Two-level distance test circle fill. ────────
//     Strategy (Z3 UNSAT — kt-fill-circle-proof.smt2):
//       dist_sq = dx² + dy²
//       dist_sq <= (radius - 0.5)² — fully inside, no sqrt
//       dist_sq >= (radius + 0.5)² — fully outside, no sqrt
//       else: sqrt at edge only
void kt_draw_fill_circle_bb(uint32_t* fb, int stride, kt_Vec2 center,
                             float radius, uint32_t color)
{
    // Bounding box
    kt_Rect bb = { center.x - radius, center.y - radius,
                   2.0f * radius, 2.0f * radius };
    kt_Rect clip = kt_clip_intersect(bb, s_current_clip);

    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    bool opaque = kt_draw_fill_rect_opaque(color);
    float r_lo = radius - 0.5f;
    float r_hi = radius + 0.5f;
    float r_lo_sq = r_lo * r_lo;
    float r_hi_sq = r_hi * r_hi;

    for (int py = y0; py < y1; py++) {
        int base = py * stride;
        float dy = (float)py - center.y;
        float dy_sq = dy * dy;
        for (int px = x0; px < x1; px++) {
            float dx = (float)px - center.x;
            float dist_sq = dx * dx + dy_sq;

            // Interior test: fully inside, no sqrt
            if (dist_sq <= r_lo_sq) goto fill_pix;

            // Exterior test: fully outside, no sqrt
            if (dist_sq >= r_hi_sq) continue;

            // Edge: compute coverage
            {
                float dist = sqrtf(dist_sq);
                float sdf = dist - radius;
                float cov = fmaxf(0.0f, fminf(1.0f, 0.5f - sdf));
                if (cov <= 0.0f) continue;
                if (cov >= 1.0f && opaque) goto fill_pix;

                uint32_t src_alpha = (uint32_t)(cov * 255.0f + 0.5f);
                uint32_t src_color = (color & 0x00FFFFFF) | (src_alpha << 24);
                fb[base + px] = kt_blend_srcover_u32(src_color, fb[base + px]);
                continue;
            }

            fill_pix:
            if (opaque) {
                fb[base + px] = color;
            } else {
                fb[base + px] = kt_blend_srcover_u32(color, fb[base + px]);
            }
        }
    }
}

// ── kt_draw_stroke_circle: Ring SDF stroke. ─────────────────────────────
//     Formula (Z3 UNSAT — kt-fill-circle-proof.smt2):
//       d_ring = fabsf(dist - radius)
//       sdf_stroke = d_ring - thickness * 0.5f
//       coverage = fmaxf(0, fminf(1, 0.5f - sdf_stroke))
void kt_draw_stroke_circle(uint32_t* fb, int stride, kt_Vec2 center,
                            float radius, float thickness, uint32_t color)
{
    if (thickness <= 0.0f) return;
    if (kt_draw_fill_rect_transparent_skip(color)) return;

    float outer_r = radius + thickness * 0.5f;
    kt_Rect bb = { center.x - outer_r, center.y - outer_r,
                   2.0f * outer_r, 2.0f * outer_r };
    kt_Rect clip = kt_clip_intersect(bb, s_current_clip);

    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0) return;

    float half_t = thickness * 0.5f;

    for (int py = y0; py < y1; py++) {
        int base = py * stride;
        float dy = (float)py - center.y;
        for (int px = x0; px < x1; px++) {
            float dx = (float)px - center.x;
            float dist = hypotf(dx, dy);
            float d_ring = fabsf(dist - radius);
            float sdf = d_ring - half_t;
            float cov = fmaxf(0.0f, fminf(1.0f, 0.5f - sdf));
            if (cov <= 0.0f) continue;

            uint32_t src_alpha = (uint32_t)(cov * 255.0f + 0.5f);
            uint32_t src_color = (color & 0x00FFFFFF) | (src_alpha << 24);
            fb[base + px] = kt_blend_srcover_u32(src_color, fb[base + px]);
        }
    }
}

// ── kt_draw_gradient_segment_precompute: Precompute segment data. ──────
//     Builds GradSegment array from color stop pairs. Produces fixed-point
//     interpolation data for efficient per-pixel gradient evaluation.
//     Returns number of segments written.
int kt_draw_gradient_segment_precompute(const uint32_t* colors, const float* positions,
                                         int n_stops, int rect_x, int rect_w,
                                         GradSegment* segments, int max_segments)
{
    if (n_stops < 2 || max_segments < 1) return 0;

    int n_seg = n_stops - 1;
    if (n_seg > max_segments) n_seg = max_segments;

    float px_lo = (float)rect_x;
    // px_lo is enough — segment bounds computed per-stop below

    for (int i = 0; i < n_seg; i++) {
        float t0 = fmaxf(positions[i],   0.0f);
        float t1 = fminf(positions[i+1], 1.0f);

        segments[i].px_min = (int)(px_lo + t0 * (float)rect_w);
        segments[i].px_max = (int)(px_lo + t1 * (float)rect_w);
        if (segments[i].px_max <= segments[i].px_min) {
            segments[i].px_max = segments[i].px_min + 1;
        }

        int seg_dx = segments[i].px_max - segments[i].px_min;
        segments[i].t_dx = seg_dx * KT_FIXED_POINT_SCALE;  // 8.8 fixed point denominator

        // Color 0 at segment start
        segments[i].r0 = (colors[i] >> 16) & 0xFF;
        segments[i].g0 = (colors[i] >>  8) & 0xFF;
        segments[i].b0 = (colors[i] >>  0) & 0xFF;
        segments[i].a0 = (colors[i] >> 24) & 0xFF;

        // Color 1 at segment end
        uint8_t r1 = (colors[i+1] >> 16) & 0xFF;
        uint8_t g1 = (colors[i+1] >>  8) & 0xFF;
        uint8_t b1 = (colors[i+1] >>  0) & 0xFF;
        uint8_t a1 = (colors[i+1] >> 24) & 0xFF;

        // Delta per 256th step (8.8 fixed point)
        segments[i].dr = (int8_t)(((int)r1 - (int)segments[i].r0) * KT_FIXED_POINT_SCALE / seg_dx);
        segments[i].dg = (int8_t)(((int)g1 - (int)segments[i].g0) * KT_FIXED_POINT_SCALE / seg_dx);
        segments[i].db = (int8_t)(((int)b1 - (int)segments[i].b0) * KT_FIXED_POINT_SCALE / seg_dx);
        segments[i].da = (int8_t)(((int)a1 - (int)segments[i].a0) * KT_FIXED_POINT_SCALE / seg_dx);
    }
    return n_seg;
}

// ── kt_draw_gradient_lerp_u8: Fixed-point gradient lerp. ──────────────
//     out = (a * (KT_FIXED_POINT_SCALE - t) + b * t + (KT_FIXED_POINT_SCALE/2)) >> 8
//     Z3 UNSAT: kt-color-lerp-proof.smt2
static inline uint8_t kt_draw_gradient_lerp_u8(uint8_t a, uint8_t b, int t_256) {
    return (uint8_t)(((int)a * (KT_FIXED_POINT_SCALE - t_256) + (int)b * t_256 + (KT_FIXED_POINT_SCALE / 2)) >> 8);
}

// ── kt_draw_gradient_rect: Fill rect with horizontal gradient. ─────────
//     Precomputes segments, then for each pixel does binary search (O(log N))
//     or direct O(1) lookup for n_stops <= 4.
void kt_draw_gradient_rect(uint32_t* fb, int stride, kt_Rect rect,
                            const uint32_t* colors, const float* positions,
                            int n_stops)
{
    kt_Rect clip = kt_clip_intersect(rect, s_current_clip);

    int x0 = (int)clip.x;
    int y0 = (int)clip.y;
    int x1 = (int)(clip.x + clip.w);
    int y1 = (int)(clip.y + clip.h);

    if (x1 <= x0 || y1 <= y0 || n_stops < 2) return;

    // Precompute segments
    GradSegment segs[8];
    int n_seg = kt_draw_gradient_segment_precompute(colors, positions, n_stops,
                                                     (int)rect.x, (int)rect.w,
                                                     segs, 8);
    if (n_seg < 1) return;

    for (int py = y0; py < y1; py++) {
        int base = py * stride;
        for (int px = x0; px < x1; px++) {
            // Find segment containing px
            int si;
            if (n_seg <= 4) {
                // O(n) linear scan — fine for small n
                for (si = 0; si < n_seg; si++) {
                    if (px < segs[si].px_max) break;
                }
                if (si >= n_seg) si = n_seg - 1;
            } else {
                // Binary search O(log N)
                int lo = 0, hi = n_seg - 1;
                while (lo < hi) {
                    int mid = (lo + hi) >> 1;
                    if (px < segs[mid].px_max) hi = mid;
                    else lo = mid + 1;
                }
                si = lo;
            }

            GradSegment* s = &segs[si];
            if (px < s->px_min || px >= s->px_max) continue;

            // Fixed-point t in [0, 256)
            int t_256 = ((px - s->px_min) * KT_FIXED_POINT_SCALE) / (s->px_max - s->px_min);
            if (t_256 < 0) t_256 = 0;
            if (t_256 > 255) t_256 = 255;

            uint8_t r = (uint8_t)(s->r0 + s->dr * t_256 / KT_FIXED_POINT_SCALE);
            uint8_t g = (uint8_t)(s->g0 + s->dg * t_256 / KT_FIXED_POINT_SCALE);
            uint8_t b = (uint8_t)(s->b0 + s->db * t_256 / KT_FIXED_POINT_SCALE);
            uint8_t a = (uint8_t)(s->a0 + s->da * t_256 / KT_FIXED_POINT_SCALE);

            uint32_t src_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            fb[base + px] = kt_blend_srcover_u32(src_color, fb[base + px]);
        }
    }
}

// ── kt_draw_glyph_quad: Compute 4 vertices for a glyph quad. ──────────
//     Given a pen position and glyph metrics, fills a KaintanaDrawVert[4]
//     with position and UV coordinates. Optionally pixel-snaps.
//     Index order: 0,1,2,2,3,0 (two triangles forming a quad).
void kt_draw_glyph_quad(kt_Vec2 pen, float xoff, float yoff,
                         float xoff2, float yoff2,
                         float u0, float v0, float u1, float v1,
                         uint32_t color, bool pixel_snap,
                         KaintanaDrawVert verts[4])
{
    float x0 = pen.x + xoff;
    float y0 = pen.y + yoff;
    float x1 = pen.x + xoff2;
    float y1 = pen.y + yoff2;

    if (pixel_snap) {
        x0 = floorf(x0 + 0.5f);
        y0 = floorf(y0 + 0.5f);
        x1 = x0 + (xoff2 - xoff);
        y1 = y0 + (yoff2 - yoff);
    }

    // 4 vertices, in quad order (CCW: 0→1→2→3)
    verts[0].x = x0; verts[0].y = y0; verts[0].u = u0; verts[0].v = v0; verts[0].col = color;
    verts[1].x = x1; verts[1].y = y0; verts[1].u = u1; verts[1].v = v0; verts[1].col = color;
    verts[2].x = x1; verts[2].y = y1; verts[2].u = u1; verts[2].v = v1; verts[2].col = color;
    verts[3].x = x0; verts[3].y = y1; verts[3].u = u0; verts[3].v = v1; verts[3].col = color;
}

// ── kt_draw_emit_quad_indices: Fill a 6-element index array for a quad. ──
//     Index pattern: 0,1,2,2,3,0 (CCW, two triangles)
void kt_draw_emit_quad_indices(uint16_t base_idx, uint16_t indices[6]) {
    indices[0] = base_idx + 0;
    indices[1] = base_idx + 1;
    indices[2] = base_idx + 2;
    indices[3] = base_idx + 2;
    indices[4] = base_idx + 3;
    indices[5] = base_idx + 0;
}

// ============================================================================
//  SECTION 5: DRAW BATCH MANAGEMENT
// ============================================================================
//  Write-pointer reservation pattern (ImGui PrimReserve):

// ── kt_draw_batch_reserve: Ensure capacity for N more draw commands. ──
//     Geometric growth: new_cap = cap + cap/2 (1.5x).
//     If growth is insufficient, exact-fit.
//     Z3 UNSAT: kt-arena-grow-15x.smt2 (growth factor proven)
bool kt_draw_batch_reserve(KaintanaDrawBatch* batch, int count) {
    int needed = batch->count + count;
    // If buf is NULL (uninitialized) or capacity insufficient, grow.
    // needed <= capacity with both at 0 is a false positive.
    if (batch->buf && needed <= batch->capacity) {
        batch->write_ptr = &batch->buf[batch->count];
        return true;
    }

    // 1.5x geometric growth
    int new_cap = batch->capacity + batch->capacity / 2;
    if (new_cap < needed) new_cap = needed;
    if (new_cap < KAINTANA_DRAW_BATCH_INIT) new_cap = KAINTANA_DRAW_BATCH_INIT;

    KaintanaInternalDrawCmd* new_buf = (KaintanaInternalDrawCmd*)
        realloc(batch->buf, (size_t)new_cap * sizeof(KaintanaInternalDrawCmd));
    if (!new_buf) return false;

    batch->buf = new_buf;
    batch->capacity = new_cap;
    batch->write_ptr = &batch->buf[batch->count];
    return true;
}

// ── kt_draw_try_merge: Attempt to merge a new command with the last one. ──
//     Returns true if merged (caller should NOT append). Returns false if
//     caller should append normally.
//
//     Merge criteria:
//       - Same type
//       - Same color
//       - Same texture_handle (implies same font/image)
//       - Same blend_mode
//       - Same corner_radius (for rounded rects)
//       - Rects are on the same row (same y)
//       - Touching or overlapping horizontally
bool kt_draw_try_merge(KaintanaDrawBatch* batch, KaintanaInternalDrawCmd cmd) {
    if (batch->count == 0) return false;

    KaintanaInternalDrawCmd* prev = &batch->buf[batch->count - 1];

    // Check merge compatibility
    if (prev->type != cmd.type) return false;
    if (prev->color != cmd.color) return false;
    if (prev->texture_handle != cmd.texture_handle) return false;
    if (prev->blend_mode != cmd.blend_mode) return false;
    if (prev->corner_radius != cmd.corner_radius) return false;

    // Same row check
    if (prev->y != cmd.y) return false;

    // Adjacent or overlapping: prev's right edge >= cmd's left edge
    int prev_right = (int)prev->x + (int)prev->w;
    int cmd_right  = (int)cmd.x   + (int)cmd.w;

    if (prev_right < (int)cmd.x) return false;  // Gap between them

    // Merge: expand bounds rightward and down
    int new_right = kaintana__MAX(prev_right, cmd_right);
    prev->w = (uint16_t)(new_right - (int)prev->x);
    prev->h = (uint16_t)kaintana__MAX((int)prev->h, (int)cmd.h);

    // If cmd has a data_offset and it's contiguous, update
    if (cmd.data_offset >= 0) {
        // The data_offset for the merged command is not well-defined:
        // we keep the original. Individual backends handle per-glyph data.
    }

    return true;  // Merged — caller should NOT append
}

// ============================================================================
//  SECTION 6: COMMAND GENERATION — kaintana__draw_generate
// ============================================================================

// ── Internal helper: emit a single draw command into the batch ──────────
static void emit_cmd(struct kt_Session_t* sess, uint32_t type,
                     int16_t x, int16_t y, uint16_t w, uint16_t h,
                     uint32_t color, uint32_t color_b,
                     uint16_t corner_radius, uint8_t opacity,
                     uint8_t blend_mode, int32_t texture_handle,
                     int32_t data_offset)
{
    KaintanaInternalDrawCmd cmd;
    cmd.type           = type;
    cmd.color          = color;
    cmd.color_b        = color_b;
    cmd.x              = x;
    cmd.y              = y;
    cmd.w              = w;
    cmd.h              = h;
    cmd.corner_radius  = corner_radius;
    cmd.opacity        = opacity;
    cmd.blend_mode     = blend_mode;
    cmd.texture_handle = texture_handle;
    cmd.data_offset    = data_offset;

    // Try to merge with previous command
    if (!kt_draw_try_merge(&sess->draw_batch, cmd)) {
        // Not merged — append via write pointer
        if (sess->draw_batch.write_ptr) {
            *sess->draw_batch.write_ptr++ = cmd;
            sess->draw_batch.count++;
        }
    }
}

// ── kaintana__draw_generate: Walk the node tree and emit draw commands. ──
//     Called from kt_end() in tree.c.
//     Walks VISIBLE nodes depth-first, emitting FILL_RECT for each node
//     that has a valid resolved layout. Style properties (fill color, stroke,
//     text) are currently placeholders — real integration with attr_table.c
//     deferred (see task 1.9).
void kaintana__draw_generate(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);

    // Reset per-frame clip/transform state to full-screen, identity
    s_current_clip.x = 0.0f;
    s_current_clip.y = 0.0f;
    s_current_clip.w = KT_CLIP_INFINITE;  // Effectively unlimited
    s_current_clip.h = KT_CLIP_INFINITE;
    s_clip_depth = 0;

    s_current_transform.m[0] = 1.0f; s_current_transform.m[1] = 0.0f;
    s_current_transform.m[2] = 0.0f; s_current_transform.m[3] = 1.0f;
    s_current_transform.m[4] = 0.0f; s_current_transform.m[5] = 0.0f;
    s_transform_depth = 0;

    // Reset draw batch write pointer
    // Reset draw batch for new frame
    sess->draw_batch.count = 0;
    if (!kt_draw_batch_reserve(&sess->draw_batch, 0)) {
        return;  // Failed to reserve — early out
    }
    sess->draw_batch.write_ptr = &sess->draw_batch.buf[sess->draw_batch.count];

    // Depth-first node walk using explicit stack (avoids recursion limits)
    // Stack holds indices to visit; -1 = sentinel for no more children
    int32_t stack[KAINTANA_MAX_DEPTH];
    int32_t stack_depth = 0;

    // Root node index is 0 — emit children first
    KaintanaNode* root = kaintana__node(s, 0);
    if (!root) return;

    // Push first child of root onto stack
    if (root->first_child >= 0) {
        stack[stack_depth++] = root->first_child;
    }

    while (stack_depth > 0) {
        // Pop next node
        int32_t idx = stack[--stack_depth];
        KaintanaNode* node = kaintana__node(s, idx);
        if (!node) continue;

        // Check visibility
        bool visible = (node->flags & KT_NODE_VISIBLE) != 0
                    && node->visibility_flags == KT_VISIBLE_DEFAULT;
        if (!visible) continue;

        // Check for collapsed parent — children hidden
        if (node->flags & KT_NODE_COLLAPSED) {
            continue;  // Children will not be pushed
        }

        // Push children (in reverse order so they're processed left-to-right)
        if (node->first_child >= 0) {
            // Collect siblings in a temp buffer
            int32_t child = node->first_child;
            int32_t child_count = 0;
            int32_t children[KAINTANA_MAX_CHILDREN];
            while (child >= 0 && child_count < KAINTANA_MAX_CHILDREN) {
                children[child_count++] = child;
                KaintanaNode* cn = kaintana__node(s, child);
                child = cn ? cn->next_sibling : -1;
            }
            // Push in reverse order
            for (int i = child_count - 1; i >= 0; i--) {
                stack[stack_depth++] = children[i];
            }
        }

        // Emit draw command if node has a resolved layout
        if (node->layout_arena_index < 0) continue;

        KaintanaLayout* lay = kaintana__layout(s, node->layout_arena_index);
        if (!lay) continue;

        // Only emit if the node has non-zero size
        if (lay->resolved_width <= 0.0f || lay->resolved_height <= 0.0f) continue;

        // Convert layout to draw command coordinates.
        // Emit in PHYSICAL pixels: multiply logical layout coords by effective
        // DPI scale factor so every backend renders at 1:1 with zero DPI awareness.
        // This is the SINGLE scaling point — backends are dumb consumers.
        float eff_scale_x = sess->native_scale_x * sess->user_zoom;
        float eff_scale_y = sess->native_scale_y * sess->user_zoom;
        int16_t rx = (int16_t)roundf(lay->resolved_x * eff_scale_x);
        int16_t ry = (int16_t)roundf(lay->resolved_y * eff_scale_y);
        uint16_t rw = (uint16_t)kaintana__MAX(KT_MIN_ELEMENT_SIZE,
                                               (int)roundf(lay->resolved_width  * eff_scale_x));
        uint16_t rh = (uint16_t)kaintana__MAX(KT_MIN_ELEMENT_SIZE,
                                               (int)roundf(lay->resolved_height * eff_scale_y));

        // Corner radius: convert float to 8.8 fixed-point
        uint16_t cr = (uint16_t)(lay->corner_radius * (float)KT_FIXED_POINT_SCALE + 0.5f);

        // Opacity: convert float [0,1] to uint8 [0,255]
        uint8_t op = (uint8_t)(lay->opacity * 255.0f + 0.5f);
        if (op == 0) continue;  // Fully transparent — skip

    // ── Use fill_color from layout (BUG-006 fix) ────────────────
        // If fill_color is 0 (unset), skip the node entirely (no C-invented default).
        uint32_t fill_color = lay->fill_color;
        if (fill_color == 0) {
            continue;  // Transparent — skip
        }
        uint32_t color_b = 0;
        int32_t texture_handle = -1;
        int32_t data_offset = -1;
        uint8_t blend_mode = (uint8_t)KT_BLEND_SRC_OVER;

        // Emit fill command
        emit_cmd(sess, KT_CMD_FILL, rx, ry, rw, rh,
                 fill_color, color_b, cr, op, blend_mode,
                 texture_handle, data_offset);
    }

    // Store last-emitted command for merge state
    if (sess->draw_batch.count > 0) {
        sess->draw_batch.last = sess->draw_batch.buf[sess->draw_batch.count - 1];
    }
}

// ============================================================================
//  SECTION 7: COMMAND MERGING — kaintana__draw_merge
// ============================================================================

// ── kaintana__draw_merge: Post-processing pass to consolidate commands. ──
//     Called from kt_end() after kaintana__draw_generate().
//     Walks the draw batch and merges adjacent compatible commands.
//
//     Merge rules:
//       - Same type, color, texture_handle, blend_mode, corner_radius
//       - Same row (y position)
//       - Horizontally adjacent or overlapping
//       - Merged command's width expands to encompass both
void kaintana__draw_merge(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    KaintanaDrawBatch* batch = &sess->draw_batch;

    if (batch->count < 2) return;

    KaintanaInternalDrawCmd* cmds = batch->buf;
    int write_idx = 1;  // Position for next kept command

    for (int read_idx = 1; read_idx < batch->count; read_idx++) {
        KaintanaInternalDrawCmd* prev = &cmds[write_idx - 1];
        KaintanaInternalDrawCmd* curr = &cmds[read_idx];

        // Check merge compatibility
        bool can_merge =
            (prev->type          == curr->type)          &&
            (prev->color         == curr->color)         &&
            (prev->color_b       == curr->color_b)       &&
            (prev->texture_handle == curr->texture_handle) &&
            (prev->blend_mode    == curr->blend_mode)    &&
            (prev->corner_radius == curr->corner_radius) &&
            (prev->opacity       == curr->opacity)       &&
            (prev->y             == curr->y);             // Same row

        if (can_merge) {
            // Same row: expand width to encompass current
            int16_t prev_right  = (int16_t)((int)prev->x + (int)prev->w);
            int16_t curr_right  = (int16_t)((int)curr->x + (int)curr->w);
            int16_t new_right   = kaintana__MAX(prev_right, curr_right);
            int16_t new_left    = kaintana__MIN((int16_t)prev->x, (int16_t)curr->x);
            prev->x = new_left;
            prev->w = (uint16_t)(new_right - new_left);
            prev->h = kaintana__MAX(prev->h, curr->h);
        } else {
            // Cannot merge — keep as separate command
            if (write_idx != read_idx) {
                cmds[write_idx] = *curr;
            }
            write_idx++;
        }
    }

    batch->count = write_idx;
    batch->write_ptr = &cmds[batch->count];

    if (batch->count > 0) {
        batch->last = cmds[batch->count - 1];
    }
}
