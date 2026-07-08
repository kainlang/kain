// ============================================================================
//  null_host_fuzz.c — Standalone fuzz test for the Kaintana null backend
//
//  Bombs host_null.c's render path with randomly generated kt_DrawData to
//  catch crashes, buffer overflows, clip stack corruption, and edge cases.
//
//  Self-contained — only needs kaintana.h + host_null.c. No core runtime.
//
//  Compile:
//    gcc -std=c11 -Wall -O1 -I. -I../../include null_host_fuzz.c ../null/host_null.c -o null_host_fuzz.exe
//
//  Run:
//    null_host_fuzz.exe [--seed N] [--iters N] [--verbose]
// ============================================================================

#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "../../kaintana.h"

extern const KaintanaBackendVTable kaintana_null_backend;

// ============================================================================
//  RNG — SplitMix64
// ============================================================================
static uint64_t fuzz_state = 0;
static void fuzz_seed(uint64_t s) { fuzz_state = s; }
static uint64_t fuzz_rand64(void) {
    fuzz_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = fuzz_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static int fuzz_int(int lo, int hi) { return lo + (int)(fuzz_rand64() % (uint64_t)(hi-lo+1)); }
static float fuzz_float(float lo, float hi) { return lo + (float)((double)fuzz_rand64()/UINT64_MAX)*(hi-lo); }
static bool fuzz_bool(void) { return fuzz_rand64()&1; }
static uint32_t fuzz_color(void) { return (uint32_t)(fuzz_rand64()>>32); }
static kt_CmdType fuzz_cmdtype(void) { return (kt_CmdType)fuzz_int(0,5); }

// ============================================================================
//  COMMAND GENERATOR — Build a random kt_DrawData
// ============================================================================
#define MAX_FUZZ_CMDS 256
static kt_Cmd g_cmds[MAX_FUZZ_CMDS];

static kt_DrawData fuzz_generate_drawdata(int max_w, int max_h) {
    int n = fuzz_int(0, MAX_FUZZ_CMDS-1);

    // Occasionally: zero commands, max commands, or exactly 1
    if (fuzz_int(0,20)==0) n=0;
    if (fuzz_int(0,20)==0) n=MAX_FUZZ_CMDS-1;
    if (fuzz_int(0,20)==0) n=1;

    int clip_depth = 0;
    for (int i=0; i<n; i++) {
        kt_Cmd* c = &g_cmds[i];
        c->type = fuzz_cmdtype();

        // Force CLIP/UNCLIP balance sometimes
        if (clip_depth > 8 && c->type==KT_CMD_CLIP) c->type = KT_CMD_UNCLIP;
        if (i == n-1 && clip_depth > 0) c->type = KT_CMD_UNCLIP;

        // Random bounds — sometimes out of bounds, sometimes NaN, sometimes normal
        int style = fuzz_int(0, 9);
        switch (style) {
        case 0: // Normal
            c->bounds.x = fuzz_float(0, (float)max_w*0.8f);
            c->bounds.y = fuzz_float(0, (float)max_h*0.8f);
            c->bounds.w = fuzz_float(1, (float)max_w*0.5f);
            c->bounds.h = fuzz_float(1, (float)max_h*0.5f);
            break;
        case 1: // Negative positions
            c->bounds.x = fuzz_float(-500, -1);
            c->bounds.y = fuzz_float(-500, -1);
            c->bounds.w = fuzz_float(1, 200);
            c->bounds.h = fuzz_float(1, 200);
            break;
        case 2: // Off-screen
            c->bounds.x = fuzz_float((float)max_w, (float)max_w*10);
            c->bounds.y = fuzz_float((float)max_h, (float)max_h*10);
            c->bounds.w = fuzz_float(1, 200);
            c->bounds.h = fuzz_float(1, 200);
            break;
        case 3: // Negative size
            c->bounds.w = fuzz_float(-500, -1);
            c->bounds.h = fuzz_float(-500, -1);
            break;
        case 4: // Zero size
            c->bounds.w = 0; c->bounds.h = 0;
            break;
        case 5: // Huge
            c->bounds.x=0; c->bounds.y=0;
            c->bounds.w = fuzz_float(10000, 100000);
            c->bounds.h = fuzz_float(10000, 100000);
            break;
        case 6: // NaN
            c->bounds.x = NAN; c->bounds.y = NAN;
            c->bounds.w = NAN; c->bounds.h = NAN;
            break;
        case 7: // Infinity
            c->bounds.x = INFINITY; c->bounds.y = -INFINITY;
            c->bounds.w = INFINITY; c->bounds.h = INFINITY;
            break;
        case 8: // Tiny (1x1)
            c->bounds.x = fuzz_float(0,(float)max_w);
            c->bounds.y = fuzz_float(0,(float)max_h);
            c->bounds.w = 1; c->bounds.h = 1;
            break;
        case 9: // Full framebuffer
            c->bounds.x=0; c->bounds.y=0;
            c->bounds.w=(float)max_w; c->bounds.h=(float)max_h;
            break;
        }

        c->color     = fuzz_color();
        c->color_b   = fuzz_color();
        c->radius    = fuzz_float(-10, 500);
        c->thickness = fuzz_float(-10, 200);
        c->text_id   = fuzz_int(-100, 100);
        c->image_id  = fuzz_int(-100, 100);

        if (c->type == KT_CMD_CLIP) clip_depth++;
        if (c->type == KT_CMD_UNCLIP && clip_depth>0) clip_depth--;
    }

    kt_DrawData dd;
    dd.cmds = g_cmds;
    dd.cmd_count = n;
    return dd;
}

// ============================================================================
//  MAIN FUZZ LOOP
// ============================================================================
int main(int argc, char** argv) {
    uint64_t seed  = (uint64_t)time(NULL);
    int iters      = 10000;
    bool verbose   = false;

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--seed") && i+1<argc) seed=strtoull(argv[++i],NULL,10);
        else if (!strcmp(argv[i],"--iters") && i+1<argc) iters=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--verbose")||!strcmp(argv[i],"-v")) verbose=true;
    }
    if (seed==0) seed=1;

    printf("=== Null Backend Fuzz (standalone) ===\n");
    printf("  seed:  %llu\n", (unsigned long long)seed);
    printf("  iters: %d\n", iters);
    printf("======================================\n");
    fuzz_seed(seed);

    int failures = 0;

    for (int iter=0; iter<iters; iter++) {
        // Random framebuffer size each iteration
        int w = fuzz_int(1, 2048);
        int h = fuzz_int(1, 2048);
        if (fuzz_int(0,15)==0) { w=fuzz_int(1,8); h=fuzz_int(1,8); }     // Tiny
        if (fuzz_int(0,20)==0) { w=fuzz_int(1024,1920); h=fuzz_int(768,1080); } // Large
        if (fuzz_int(0,50)==0) { w=1; h=1; }                                // Minimum

        // ── Init ─────────────────────────────────────────────────────
        KaintanaBackendConfig cfg = { "fuzz", w, h, 0, NULL };
        int ok = kaintana_null_backend.init(&cfg);
        if (ok != 0) {
            fprintf(stderr, "  [FATAL] iter %d: null_init(%d,%d) returned %d\n", iter, w, h, ok);
            failures++;
            continue;
        }

        // ── Fuzz: random number of frames per session ────────────────
        int frames = fuzz_int(1, 20);
        for (int f=0; f<frames; f++) {
            kaintana_null_backend.new_frame();
            kt_DrawData dd = fuzz_generate_drawdata(w, h);
            kaintana_null_backend.render(&dd);

            // Also try NULL, empty, and zero-count edge cases
            if (fuzz_int(0,5)==0) kaintana_null_backend.render(NULL);
            if (fuzz_int(0,5)==0) {
                kt_DrawData empty = {g_cmds, 0};
                kaintana_null_backend.render(&empty);
            }
        }

        // ── Shutdown ─────────────────────────────────────────────────
        kaintana_null_backend.shutdown();

        if (verbose && iter%1000==0 && iter>0) {
            printf("  ... %d/%d, %d failures\n", iter, iters, failures);
        }
    }

    printf("\n======================================\n");
    printf("  FUZZ COMPLETE\n");
    printf("  seed:     %llu\n", (unsigned long long)seed);
    printf("  iters:    %d\n", iters);
    printf("  failures: %d\n", failures);
    printf("  result:   %s\n", failures==0?"PASS ✓":"FAIL ✗");
    printf("======================================\n");

    return failures>0 ? 1 : 0;
}
