// ============================================================================
//  fuzzer.c — Main fuzz test harness implementation
//  ============================================================================
//  Data-driven fuzz test framework for the Kain UI C substrate.
//  Reads fuzz_taxonomy.json (via Python orchestrator) for test parameters.
//  This file implements the C-side harness; run_fuzz.py drives it.
//
//  Part of the Kain UI substrate. Does not rely on any widget code.
//  ============================================================================

#include "fuzzer.h"

#include <signal.h>

jmp_buf            fuzz_crash_jmp;
volatile int       fuzz_crash_occurred = 0;

void fuzz_crash_handler(int sig) {
    (void)sig;
    fuzz_crash_occurred = 1;
    longjmp(fuzz_crash_jmp, 1);
}

void fuzz_install_crash_handler(void) {
    signal(SIGSEGV, fuzz_crash_handler);
    signal(SIGABRT, fuzz_crash_handler);
    signal(SIGFPE,  fuzz_crash_handler);
    signal(SIGILL,  fuzz_crash_handler);
}

// ── Fuzz state lifecycle ──────────────────────────────────────────

void fuzz_state_init(FuzzState* s, unsigned int seed, int fb_w, int fb_h) {
    memset(s, 0, sizeof(FuzzState));
    s->seed = seed;
    s->iter = 0;
    s->fb_w = fb_w;
    s->fb_h = fb_h;

    // Initialize basic state values
    s->last_rect  = kain_rect_make(0, 0, 0, 0);
    s->last_point = kain_point_make(0, 0);
    s->last_color = KAIN_COLOR_BLACK;
    s->last_matrix = kain_matrix_identity();

    // Allocate framebuffer for render tests
    s->fb = (uint32_t*)calloc((size_t)(fb_w * fb_h), sizeof(uint32_t));
    if (s->fb) {
        s->renderer = kain_renderer_create(fb_w, fb_h, s->fb);
    }

    // Create compositor
    s->compositor = kain_compositor_create(fb_w, fb_h);

    // Input pipeline (no real session — tests defensive behavior)
    s->input_pipeline = kain_input_pipeline_create(1);  // session_id = 1

    // Font session ID
    s->font_session_id = 1;  // matches session 1 used by input

    // Resolve native_ui surface for vtable tests
    s->surface = kain_component_surface_resolve("native_ui");
    // If no platform surface, we test the abi_ui_* API directly
    s->session_id = 0;

    // Surface
    s->test_surface = NULL;  // created on-demand

    // Telemetry init
    s->telemetry.total_tests = 0;
    s->telemetry.passed = 0;
    s->telemetry.failed = 0;
    s->telemetry.crashed = 0;
    s->telemetry.null_ptr_ok = 0;
    s->telemetry.boundary_hits = 0;
    s->telemetry.edge_violations = 0;
    s->telemetry.elapsed_ms = 0.0;
    s->telemetry.first_failure = NULL;
    s->telemetry.failure_count = 0;
}

void fuzz_state_destroy(FuzzState* s) {
    if (s->renderer) {
        kain_renderer_destroy(s->renderer);
        s->renderer = NULL;
    }
    if (s->fb) {
        free(s->fb);
        s->fb = NULL;
    }
    if (s->compositor) {
        kain_compositor_destroy(s->compositor);
        s->compositor = NULL;
    }
    if (s->input_pipeline) {
        kain_input_pipeline_destroy(s->input_pipeline);
        s->input_pipeline = NULL;
    }
    if (s->test_surface) {
        kain_surface_destroy(s->test_surface);
        s->test_surface = NULL;
    }
}

// ── Forward declarations for report helpers ───────────────────────
static void fuzz_store_result(FuzzTelemetry tel);
void fuzz_register_stub_surface(void);  // from fuzz_stubs.c

// ── Fuzz entry point (called from run_fuzz.py or standalone) ──────

int main(int argc, char** argv) {
    // Default configuration
    FuzzConfig cfg;
    cfg.iteration_count = 50000;
    cfg.seed = 42;
    cfg.fb_width = DEFAULT_FB_WIDTH;
    cfg.fb_height = DEFAULT_FB_HEIGHT;
    cfg.crash_on_error = false;
    cfg.max_log_samples = 50;
    cfg.report_path = "fuzz_report.json";

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            cfg.iteration_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            cfg.seed = (unsigned int)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg.fb_width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg.fb_height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            cfg.report_path = argv[++i];
        } else if (strcmp(argv[i], "--quick") == 0) {
            cfg.iteration_count = 10000;
        } else if (strcmp(argv[i], "--stress") == 0) {
            cfg.iteration_count = 500000;
        }
    }

    // Install crash handler
    fuzz_install_crash_handler();

    // Register stub native_ui surface for vtable tests
    fuzz_register_stub_surface();

    // Initialize state
    FuzzState state;
    fuzz_state_init(&state, cfg.seed, cfg.fb_width, cfg.fb_height);

    printf("+------------------------------------------------------------------+\n");
    printf("|  Kain UI C Substrate Fuzz Suite v1.0                             |\n");
    printf("+------------------------------------------------------------------+\n");
    printf("|  Iterations: %-10d  Seed: %-10u  FB: %dx%d          |\n",
           cfg.iteration_count, cfg.seed, cfg.fb_width, cfg.fb_height);
    printf("+------------------------------------------------------------------+\n\n");

    FuzzTelemetry results[7];
    int result_count = 0;

    // ── Run fuzz test domains ────────────────────────────────────
    clock_t t_start = clock();

    // ── Each domain is crash-guarded ──────────────────────────────
    // If a domain crashes (SIGSEGV/SIGABRT/etc), we catch it via setjmp/longjmp
    // and record the crash in telemetry. This lets the fuzzer continue testing
    // other domains even when one has a bug.

    // Pre-reserve slots (one per domain) so crash handling doesn't corrupt indices
    FuzzTelemetry zero_tel = {0};
    for (int di = 0; di < 7; di++) results[di] = zero_tel;
    result_count = 0;

    for (int di = 0; di < 7; di++) {
        fuzz_crash_occurred = 0;
        if (setjmp(fuzz_crash_jmp) == 0) {
            FuzzTelemetry tel;
            switch (di) {
                case 0:
                    printf("> Fuzzing geometry...\n");
                    tel = fuzz_geometry(&state, cfg.iteration_count);
                    break;
                case 1:
                    printf("> Fuzzing render primitives...\n");
                    tel = fuzz_render(&state, cfg.iteration_count);
                    break;
                case 2:
                    printf("> Fuzzing compositor...\n");
                    tel = fuzz_compositor(&state, cfg.iteration_count);
                    break;
                case 3:
                    printf("> Fuzzing input pipeline...\n");
                    tel = fuzz_input(&state, cfg.iteration_count);
                    break;
                case 4:
                    printf("> Fuzzing font subsystem...\n");
                    tel = fuzz_font(&state, cfg.iteration_count);
                    break;
                case 5:
                    printf("> Fuzzing surface abstraction...\n");
                    tel = fuzz_surface(&state, cfg.iteration_count);
                    break;
                case 6:
                    printf("> Fuzzing vtable surface...\n");
                    tel = fuzz_vtable(&state, cfg.iteration_count);
                    break;
                default:
                    tel = zero_tel;
                    break;
            }
            fuzz_store_result(tel);
            results[result_count++] = tel;
        } else {
            // Crash caught — fill telemetry with crash info
            FuzzTelemetry crash_tel;
            memset(&crash_tel, 0, sizeof(crash_tel));
            crash_tel.domain_name = "(crashed)";
            crash_tel.total_tests = 0;
            crash_tel.crashed = 1;
            crash_tel.first_failure = "Crashed with SIGSEGV/SIGABRT";
            results[result_count++] = crash_tel;
            printf("  !! Domain %d crashed (caught by signal handler)\n", di);
        }
    }

    clock_t t_end = clock();
    double total_ms = 1000.0 * (double)(t_end - t_start) / (double)CLOCKS_PER_SEC;

    // ── Aggregate results ────────────────────────────────────────
    int grand_total = 0, grand_passed = 0, grand_failed = 0, grand_crashed = 0;
    for (int i = 0; i < result_count; i++) {
        grand_total  += results[i].total_tests;
        grand_passed += results[i].passed;
        grand_failed += results[i].failed;
        grand_crashed += results[i].crashed;
    }

    printf("\n═══ FUZZ RESULTS ════════════════════════════════════════════\n");
    printf("  %-20s %8s %8s %8s %8s %8s\n",
           "Domain", "Total", "Passed", "Failed", "Crashed", "Time(ms)");
    printf("  %s\n", "─────────────────────────────────────────────────────────────");
    for (int i = 0; i < result_count; i++) {
        printf("  %-20s %8d %8d %8d %8d %8.0f\n",
               results[i].domain_name,
               results[i].total_tests,
               results[i].passed,
               results[i].failed,
               results[i].crashed,
               results[i].elapsed_ms);
    }
    printf("  %s\n", "─────────────────────────────────────────────────────────────");
    printf("  %-20s %8d %8d %8d %8d %8.0f\n",
           "TOTAL", grand_total, grand_passed, grand_failed, grand_crashed, total_ms);
    printf("  %s\n", "═══════════════════════════════════════════════════════════════");

    // ── Write report ─────────────────────────────────────────────
    fuzz_write_report(&state, cfg.report_path);
    printf("\nReport written to: %s\n", cfg.report_path);

    // Report failures
    int total_issues = grand_failed + grand_crashed;
    if (total_issues > 0) {
        printf("\n⚠ WARNING: %d test operations reported issues. See report for details.\n",
               total_issues);
    } else {
        printf("\nOK All fuzz domains passed without issues.\n");
    }

    // Cleanup
    fuzz_state_destroy(&state);
    return total_issues > 0 ? 1 : 0;
}

// ── Report writer ─────────────────────────────────────────────────

// Global results storage for JSON report
static FuzzTelemetry g_all_results[8];
static int g_all_results_count = 0;

void fuzz_store_result(FuzzTelemetry tel) {
    if (g_all_results_count < 8) {
        g_all_results[g_all_results_count++] = tel;
    }
}

void fuzz_write_report(FuzzState* s, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "ERROR: Could not open report path: %s\n", path);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"fuzz_version\": \"1.0.0\",\n");
    fprintf(f, "  \"seed\": %u,\n", s->seed);
    fprintf(f, "  \"framebuffer\": \"%dx%d\",\n", s->fb_w, s->fb_h);
    fprintf(f, "  \"domains\": [\n");

    int first = 1;
    for (int i = 0; i < g_all_results_count; i++) {
        FuzzTelemetry* t = &g_all_results[i];
        if (!first) fprintf(f, ",\n");
        first = 0;
        fprintf(f, "    {\"name\":\"%s\",\"total\":%d,\"passed\":%d,\"failed\":%d,\"crashed\":%d,\"null_ok\":%d,\"boundary\":%d,\"edge\":%d,\"time_ms\":%.0f}",
                t->domain_name ? t->domain_name : "unknown",
                t->total_tests, t->passed, t->failed, t->crashed,
                t->null_ptr_ok, t->boundary_hits, t->edge_violations,
                t->elapsed_ms);
    }

    fprintf(f, "\n  ]\n");
    fprintf(f, "}\n");
    fclose(f);

    printf("\nJSON report written to: %s\n", path);
}
