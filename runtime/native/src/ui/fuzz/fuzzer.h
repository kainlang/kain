// ============================================================================
//  fuzzer.h — Fuzz test harness for the Kain UI C substrate
//  ============================================================================
//  Data-driven fuzz test framework that exercises all kain_* APIs:
//    - kain_geometry.h   — rect, point, color, matrix operations
//    - kain_render_software.h — 16 draw primitives + clip/transform stacks
//    - kain_compositor.h — damage region tracking
//    - kain_input.h      — event pipeline (poll, push, hit-test)
//    - kain_font.h       — TTF loading, glyph access, text measurement
//    - kain_host.h       — host vtable dispatch (Win32 GDI backend)
//    - kain_surface.h    — surface create/destroy/resize
//    - KainComponentSurface vtable — all 24 slots
//
//  The fuzz taxonomy lives in fuzz_taxonomy.json (the data-driven truth).
//  This header defines the C-side interfaces; the Python orchestrator
//  (run_fuzz.py) reads JSON, builds, runs, and generates reports.
//
//  Part of the Kain UI substrate (KUIF Phase 1 / P1-C-015).
//  ============================================================================

#ifndef KAIN_UI_FUZZER_H
#define KAIN_UI_FUZZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <setjmp.h>

// ── Include all kain_* APIs ───────────────────────────────────────
//  These are required because FuzzState references types from all domains.
#include "kain_geometry.h"
#include "kain_render_software.h"
#include "kain_compositor.h"
#include "kain_input.h"
#include "kain_font.h"
#include "kain_host.h"
#include "kain_surface.h"
#include "../../include/component_surface.h"

// ══════════════════════════════════════════════════════════════════════════
//  Fuzzer configuration
// ══════════════════════════════════════════════════════════════════════════

typedef struct FuzzConfig {
    int          iteration_count;       // Total fuzz iterations
    unsigned int seed;                  // RNG seed
    int          fb_width;              // Default framebuffer width
    int          fb_height;             // Default framebuffer height
    bool         crash_on_error;        // If true, abort on assertion failure
    int          max_log_samples;       // Max failures to log per domain
    const char*  report_path;           // Path to write JSON telemetry
} FuzzConfig;

#define DEFAULT_FB_WIDTH  800
#define DEFAULT_FB_HEIGHT 600
#define MAX_CRASH_LOG     100

// ══════════════════════════════════════════════════════════════════════════
//  Fuzz telemetry — collected per-domain, written to JSON report
// ══════════════════════════════════════════════════════════════════════════

typedef struct FuzzTelemetry {
    const char* domain_name;     // e.g. "geometry", "render", "compositor"
    int         total_tests;     // Total test operations executed
    int         passed;          // Operations completed without error
    int         failed;          // Operations that hit unexpected errors
    int         crashed;         // Operations that caused a crash (SEGV, abort)
    int         null_ptr_ok;     // Operations that tolerated NULL input gracefully
    int         boundary_hits;   // Number of boundary cases tested
    int         edge_violations; // Boundary cases that caused state corruption
    double      elapsed_ms;      // Time spent on this domain
    const char* first_failure;   // Description of first failure (if any)
    int         failure_count;   // Total distinct failure signatures
} FuzzTelemetry;

// ══════════════════════════════════════════════════════════════════════════
//  Fuzz state — per-domain mutable state for sequences
// ══════════════════════════════════════════════════════════════════════════

typedef struct FuzzState {
    // RNG
    unsigned int seed;
    unsigned int iter;

    // Geometry domain
    kainRect   last_rect;
    kainPoint  last_point;
    kainColor  last_color;
    kainMatrix last_matrix;

    // Render domain
    KainSoftwareRenderer* renderer;
    uint32_t*             fb;          // 800x600 framebuffer
    int                   fb_w, fb_h;
    int                   clip_push_count;
    int                   xf_push_count;

    // Compositor domain
    KainCompositor* compositor;

    // Input domain (no real session — we test with mock/null session IDs)
    KainInputPipeline* input_pipeline;

    // Font domain (no real session — tests focus on defensive behavior)
    int64_t font_session_id;

    // Vtable domain
    const KainComponentSurface* surface;
    int64_t                     session_id;

    // Surface domain
    kainSurface* test_surface;

    // Telemetry
    FuzzTelemetry telemetry;
} FuzzState;

// ══════════════════════════════════════════════════════════════════════════
//  Fuzz entry points — one per domain
// ══════════════════════════════════════════════════════════════════════════

FuzzTelemetry fuzz_geometry(FuzzState* state, int iterations);
FuzzTelemetry fuzz_render(FuzzState* state, int iterations);
FuzzTelemetry fuzz_compositor(FuzzState* state, int iterations);
FuzzTelemetry fuzz_input(FuzzState* state, int iterations);
FuzzTelemetry fuzz_font(FuzzState* state, int iterations);
FuzzTelemetry fuzz_surface(FuzzState* state, int iterations);
FuzzTelemetry fuzz_vtable(FuzzState* state, int iterations);

// ══════════════════════════════════════════════════════════════════════════
//  Utility functions
// ══════════════════════════════════════════════════════════════════════════

// Fast xorshift random in [0, UINT32_MAX]
static inline uint32_t fuzz_rand(FuzzState* s) {
    uint32_t x = s->seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->seed = x;
    return x;
}

// Random float in [lo, hi]
static inline float fuzz_float(FuzzState* s, float lo, float hi) {
    uint32_t r = fuzz_rand(s);
    float t = (float)r / (float)UINT32_MAX;
    return lo + t * (hi - lo);
}

// Random double in [lo, hi]
static inline double fuzz_double(FuzzState* s, double lo, double hi) {
    uint32_t r = fuzz_rand(s);
    double t = (double)r / (double)UINT32_MAX;
    return lo + t * (hi - lo);
}

// Random int64_t in [lo, hi] inclusive
static inline int64_t fuzz_i64_range(FuzzState* s, int64_t lo, int64_t hi) {
    if (lo >= hi) return lo;
    uint64_t range = (uint64_t)(hi - lo);
    uint64_t r = (uint64_t)fuzz_rand(s);
    return lo + (int64_t)(r % (range + 1));
}

// Random int in [lo, hi] inclusive
static inline int fuzz_int(FuzzState* s, int lo, int hi) {
    return (int)fuzz_i64_range(s, (int64_t)lo, (int64_t)hi);
}

// Random boolean
static inline bool fuzz_bool(FuzzState* s) {
    return (fuzz_rand(s) & 1) != 0;
}

// Random kainRect
static inline kainRect fuzz_rand_rect(FuzzState* s) {
    kainRect r;
    r.x = fuzz_float(s, -1000.0f, 2000.0f);
    r.y = fuzz_float(s, -1000.0f, 2000.0f);
    r.w = fuzz_float(s, -100.0f, 2000.0f);
    r.h = fuzz_float(s, -100.0f, 2000.0f);
    return r;
}

// Random kainPoint
static inline kainPoint fuzz_rand_point(FuzzState* s) {
    kainPoint p;
    p.x = fuzz_float(s, -10000.0f, 10000.0f);
    p.y = fuzz_float(s, -10000.0f, 10000.0f);
    return p;
}

// Random kainColor
static inline kainColor fuzz_rand_color(FuzzState* s) {
    kainColor c;
    c.r = fuzz_float(s, -2.0f, 2.0f);   // intentionally includes out-of-range
    c.g = fuzz_float(s, -2.0f, 2.0f);
    c.b = fuzz_float(s, -2.0f, 2.0f);
    c.a = fuzz_float(s, -2.0f, 2.0f);
    return c;
}

// Random kainMatrix (2D affine)
static inline kainMatrix fuzz_rand_matrix(FuzzState* s) {
    kainMatrix m;
    m.m[0] = fuzz_float(s, -10.0f, 10.0f);
    m.m[1] = fuzz_float(s, -10.0f, 10.0f);
    m.m[2] = fuzz_float(s, -1000.0f, 1000.0f);
    m.m[3] = fuzz_float(s, -10.0f, 10.0f);
    m.m[4] = fuzz_float(s, -10.0f, 10.0f);
    m.m[5] = fuzz_float(s, -1000.0f, 1000.0f);
    return m;
}

// Random text (up to 32 chars)
static inline void fuzz_rand_text(FuzzState* s, char* out, int max_len) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "!@#$%^&*()_+-=[]{}|;':\",./<>?`~ \t\n\r"
        "\x00\x01\x02\x7F\xFF";
    int len = fuzz_int(s, 0, max_len - 1);
    for (int i = 0; i < len; i++) {
        out[i] = chars[fuzz_int(s, 0, (int)(sizeof(chars) - 2))];
    }
    out[len] = '\0';
}

// Initialize fuzz state
void fuzz_state_init(FuzzState* s, unsigned int seed, int fb_w, int fb_h);

// Destroy fuzz state (free all resources)
void fuzz_state_destroy(FuzzState* s);

// ── Crash-safe execution ──────────────────────────────────────────
// Platforms with signal handling (POSIX) can use SIGSEGV/SIGABRT.
// On Win32, we use __try/__except (SEH) or setjmp/longjmp fallback.
// This is a best-effort crash catcher — some crashes are unrecoverable.

#if defined(_WIN32) && defined(_MSC_VER)
#define FUZZ_TRY       __try
#define FUZZ_EXCEPT(block) __except(EXCEPTION_EXECUTE_HANDLER) { block; }
#elif defined(__GNUC__) || defined(__clang__)
// Use signal handling installed at startup
#include <signal.h>
extern jmp_buf fuzz_crash_jmp;
extern volatile int fuzz_crash_occurred;
extern void fuzz_crash_handler(int sig);
#define FUZZ_TRY       if (1)
#define FUZZ_EXCEPT(block) ((void)0)
// Call fuzz_install_crash_handler() at startup
#else
#define FUZZ_TRY       if (1)
#define FUZZ_EXCEPT(block) ((void)0)
#endif

// Install platform crash handler
void fuzz_install_crash_handler(void);

// ══════════════════════════════════════════════════════════════════════════
//  Report generation
// ══════════════════════════════════════════════════════════════════════════

// Write aggregated telemetry as JSON (read by run_fuzz.py for markdown output)
void fuzz_write_report(FuzzState* s, const char* path);

#endif // KAIN_UI_FUZZER_H
