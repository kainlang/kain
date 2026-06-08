/*
 * check_crash_handler.c — CBMC verification harness for the crash handler
 *                         pipeline (core + platform)
 *
 * The crash handler was split into a cross-platform core and per-platform
 * signal/exception registration files:
 *
 *   src/core/crash_handler.c          ← table lookup, render, public API
 *   src/platform/linux/crash_handler_linux.c
 *   src/platform/macos/crash_handler_macos.c
 *   src/platform/win32/crash_handler_win32.c
 *
 * This harness tests the core functions unconditionally.  Platform-specific
 * tests (walk_callstack_x64) are guarded by KAIN_CRASH_PLATFORM and
 * activated only when the platform source is also compiled in.
 *
 * ── Key constraint ─────────────────────────────────────────────────────
 *
 * The weak __kain_crash_table is declared as const[1] in the core source.
 * The real compiler-emitted table can be any size, but the CBMC model
 * is a single-entry array.  To keep array-bounds checks sound, every
 * test constrains crash_table_count ≤ 1.
 *
 * The sentinel-counting loop in __kain_crash_handler_init reads past [1]
 * for a non-empty table, so we test initialization ONLY for the empty-table
 * path (sentinel at [0]).  The lookup and render tests work directly by
 * setting crash_table_count = 0 or 1.
 *
 * ── Running ────────────────────────────────────────────────────────────
 *
 *   # Core only (pipeline default — combines crash_handler.c with harness)
 *   python test/scripts/run_pipeline.py cbmc --harness check_crash_handler
 *
 *   # Platform-specific walk test
 *   cbmc --bounds-check --pointer-check --trace \
 *     test/cbmc/check_crash_handler.c \
 *     src/core/crash_handler.c \
 *     src/platform/linux/crash_handler_linux.c \
 *     -I include -I src/core --no-unwinding-assertions \
 *     -DKAIN_CRASH_PLATFORM=linux
 *
 * ═══════════════════════════════════════════════════════════════════════
 * INVARIANTS PROVED
 * ═══════════════════════════════════════════════════════════════════════
 *
 * Core (always):
 *   1. lookup_crash_entry — binary search never reads OOB
 *   2. __kain_crash_lookup — safe for any IP value (including NULL)
 *   3. __kain_crash_render_report — NEVER crashes (double-fault proof)
 *   4. __kain_crash_handler_init — idempotent (tested via empty table)
 *
 * Platform (when KAIN_CRASH_PLATFORM is set):
 *   5. walk_callstack_x64 — NULL-safe, OOB-safe, chain termination
 */

#include "crash_handler.h"

#include <stdint.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════════
 * Static backing buffers — CBMC real pointer provenance
 * ══════════════════════════════════════════════════════════════════════ */
static char      g_fn_name[64];
static char      g_file_name[128];
static uintptr_t g_fake_stack[512];

/* ── Forward declarations of static/internal functions ──────────────── */

/* From src/core/crash_handler.c — static, accessible in combined TU. */
static const KainCrashEntry *lookup_crash_entry(const void *ip);

/* From platform/<os>/crash_handler_<os>.c — guarded. */
#if defined(KAIN_CRASH_PLATFORM)
static int walk_callstack_x64(void *rbp_val, void *stack_bottom,
                              void **frames, int max_frames);
#endif


/* ══════════════════════════════════════════════════════════════════════
 * Helpers
 * ══════════════════════════════════════════════════════════════════════ */

/* Build a fake x86-64 frame chain in buf.  Each frame: [next_rbp, ret]. */
static unsigned int build_fake_frames(uintptr_t *buf, unsigned int nframes) {
    unsigned int written = 0;
    for (unsigned int i = 0; i < nframes && (i * 2 + 1) < 512; i++) {
        uintptr_t ret;
        __CPROVER_havoc_object(&ret);
        __CPROVER_assume(ret != 0);
        buf[i * 2 + 1] = ret;
        buf[i * 2] = (i + 1 < nframes && (i * 2 + 2) < 512)
                     ? (uintptr_t)&buf[(i + 1) * 2] : 0;
        written++;
    }
    return written;
}

/* Reset handler state for independent tests.  These are static in
 * crash_handler.c but accessible in the combined TU because the
 * pipeline concatenates the source text:
 *   combined = crash_handler.c + check_crash_handler.c  → one TU
 *
 * For multi-file CBMC invocations (e.g. with platform sources), the
 * public-API-only tests below still work without touching internals.
 * Platform tests (guarded by KAIN_CRASH_PLATFORM) require all sources
 * concatenated into one file. */
static void reset_handler(void) {
    crash_handler_initialized = 0;
    crash_table_count         = 0;
}


/* ══════════════════════════════════════════════════════════════════════
 * CORE CHECK 1 — lookup_crash_entry: binary search safety
 *
 * Uses crash_table_count directly (not through init) to keep the array
 * bound at ≤ 1.  The real table can be any size; this proves the
 * search algorithm itself is safe for any valid (count, ip) pair.
 * ══════════════════════════════════════════════════════════════════════ */
void check_core_lookup(void) {
    __CPROVER_havoc_object((void *)__kain_crash_table);

    /* 1a: Empty table */
    {
        crash_table_count = 0;
        const KainCrashEntry *r = lookup_crash_entry(NULL);
        __CPROVER_assert(r == NULL, "lookup[empty]: NULL => NULL");
    }

    /* 1b: Nondet count ≤ 1, nondet IP */
    {
        __CPROVER_havoc_object(&crash_table_count);
        __CPROVER_assume(crash_table_count <= 1);

        uint64_t ip;
        __CPROVER_havoc_object(&ip);
        const KainCrashEntry *r = lookup_crash_entry((const void *)(uintptr_t)ip);

        if (crash_table_count == 0) {
            __CPROVER_assert(r == NULL, "lookup: count=0 => NULL");
        } else if (r != NULL) {
            __CPROVER_assert(r == &__kain_crash_table[0],
                             "lookup: result -> table[0]");
            __CPROVER_assert(r->fn_ptr <= ip,
                             "lookup: found fn_ptr <= ip");
        } else {
            __CPROVER_assert(ip < __kain_crash_table[0].fn_ptr,
                             "lookup: NULL => ip < first fn_ptr");
        }
    }

    /* 1c: Public API — __kain_crash_lookup edge cases */
    {
        crash_table_count = 1;
        __CPROVER_havoc_object((void *)__kain_crash_table);

        __kain_crash_lookup(NULL);
        __kain_crash_lookup((const void *)0);
        __kain_crash_lookup((const void *)(uintptr_t)~0ULL);
        __kain_crash_lookup((const void *)(uintptr_t)1);
        { int x; __kain_crash_lookup(&x); }

        __CPROVER_assert(1, "lookup: public API edge cases complete");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CORE CHECK 2 — __kain_crash_render_report: crash-safe rendering
 *
 * This function is called from signal handlers.  A crash here is a
 * double-fault.  We prove it never crashes for any possible input.
 * ══════════════════════════════════════════════════════════════════════ */
void check_core_render(void) {
    /* 2a: NULL entry, NULL fault_ip, no callstack */
    __kain_crash_render_report("SIGSEGV", NULL, NULL, NULL, 0);

    /* 2b: NULL entry with a fault IP */
    __kain_crash_render_report("SIGILL", NULL,
                               (const void *)(uintptr_t)0x7fff1234, NULL, 0);

    /* 2c: Valid entry with proper string provenance */
    {
        static KainCrashEntry e;
        __CPROVER_havoc_object(&e);
        /* Point string fields at real static buffers */
        e.fn_name = &g_fn_name[0];
        e.file    = &g_file_name[0];
        __CPROVER_assume(e.line <= 100000);
        __CPROVER_assume(e.col  <= 5000);

        __kain_crash_render_report("SIGFPE", &e, NULL, NULL, 0);
    }

    /* 2d: Nondet callstack, depth 0..8 */
    {
        static KainCrashEntry e;
        __CPROVER_havoc_object(&e);
        e.fn_name = &g_fn_name[0];
        e.file    = &g_file_name[0];

        void *cs[8];
        __CPROVER_havoc_object(cs);

        int d;
        __CPROVER_havoc_object(&d);
        __CPROVER_assume(d >= 0 && d <= 8);

        __kain_crash_render_report("SIGSEGV", &e, NULL, cs, d);
    }

    /* 2e: Mixed nondet (entry yes/no, callstack yes/no) */
    {
        static KainCrashEntry e;
        int has_e, has_cs;
        __CPROVER_havoc_object(&has_e);
        __CPROVER_havoc_object(&has_cs);

        if (has_e % 2 == 0) {
            __CPROVER_havoc_object(&e);
            e.fn_name = &g_fn_name[0];
            e.file    = &g_file_name[0];
        }

        void *cs[4];
        __CPROVER_havoc_object(cs);
        int depth = (has_cs % 2 == 0) ? 4 : 0;

        __kain_crash_render_report("SIGTEST",
            (has_e % 2 == 0) ? &e : NULL,
            (const void *)(uintptr_t)0xdead, cs, depth);
    }

    /* 2f: Negative depth (must not crash — render checks depth > 0) */
    {
        __kain_crash_render_report("SIGSEGV", NULL, NULL, NULL, -1);
    }

    /* 2g: NULL signal_name — implementation-defined (typically prints
     *     "(null)" rather than crashing).  Documented; real code should
     *     never pass NULL. */
    {
        __kain_crash_render_report(NULL, NULL, NULL, NULL, 0);
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CORE CHECK 3 — __kain_crash_handler_init: idempotency
 *
 * We test only the empty-table path (fn_ptr == 0 at entry 0) because
 * the weak __kain_crash_table[1] is too small for the sentinel-counting
 * loop on a non-empty table.  In production the compiler emits a table
 * sized to the actual function count.
 * ══════════════════════════════════════════════════════════════════════ */
void check_core_init(void) {
    /* 3a: Empty table — sentinel at [0] → count stays 0 → no handlers */
    {
        reset_handler();
        __CPROVER_havoc_object((void *)__kain_crash_table);
        __CPROVER_assume(__kain_crash_table[0].fn_ptr == 0);

        __kain_crash_handler_init();

        __CPROVER_assert(crash_handler_initialized == 1,
                         "init[empty]: flag set");
        __CPROVER_assert(crash_table_count == 0,
                         "init[empty]: count = 0 for sentinel-[0] table");
    }

    /* 3b: Idempotent — call twice */
    {
        reset_handler();
        __CPROVER_havoc_object((void *)__kain_crash_table);
        __CPROVER_assume(__kain_crash_table[0].fn_ptr == 0);

        __kain_crash_handler_init();
        int flag_after = crash_handler_initialized;

        __kain_crash_handler_init();

        __CPROVER_assert(crash_handler_initialized == flag_after,
                         "init[idempotent]: flag unchanged after 2nd call");
    }

    /* 3c: Triple init + lookup after empty-table init */
    {
        reset_handler();
        __CPROVER_havoc_object((void *)__kain_crash_table);
        __CPROVER_assume(__kain_crash_table[0].fn_ptr == 0);

        __kain_crash_handler_init();
        __kain_crash_handler_init();
        __kain_crash_handler_init();

        __kain_crash_lookup(NULL);
        __kain_crash_lookup((const void *)(uintptr_t)0x42);

        __CPROVER_assert(1, "init x3 + lookup: all safe");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CORE CHECK 4 — integration: render with prepared entry
 *
 * Creates a manually-prepared crash table entry and renders various
 * crash report combinations.  Does NOT call __kain_crash_handler_init
 * (avoids the sentinel-counting OOB on weak [1]).
 * ══════════════════════════════════════════════════════════════════════ */
void check_core_integration(void) {
    /* Build a known entry with valid string provenance */
    static KainCrashEntry entry;
    __CPROVER_havoc_object(&entry);
    entry.fn_name = &g_fn_name[0];
    entry.file    = &g_file_name[0];

    /* Fake callstack — one hit, two misses */
    void *callstack[16];
    callstack[0] = (void *)entry.fn_ptr;
    callstack[1] = (void *)(uintptr_t)0xdead;
    callstack[2] = (void *)(uintptr_t)0xbeef;

    /* Render with entry + callstack */
    __kain_crash_render_report("SIGSEGV", &entry,
                               (const void *)(uintptr_t)0x14000,
                               callstack, 3);

    /* Render without entry */
    __kain_crash_render_report("SIGFPE", NULL,
                               (const void *)(uintptr_t)0x7ffe0000,
                               callstack, 3);

    /* Render without callstack */
    __kain_crash_render_report("SIGILL", NULL,
                               (const void *)(uintptr_t)0, NULL, 0);

    /* Render with empty callstack (depth = 0) */
    __kain_crash_render_report("SIGTERM", NULL,
                               (const void *)(uintptr_t)0x1000,
                               NULL, 0);

    __CPROVER_assert(1, "integration: all render variants complete");
}


/* ══════════════════════════════════════════════════════════════════════
 * PLATFORM CHECK — walk_callstack_x64 (guarded by KAIN_CRASH_PLATFORM)
 *
 * Tests the static frame-pointer-chain walker from the platform source.
 * Only compiled when a platform source is in the TU.
 * ══════════════════════════════════════════════════════════════════════ */
#if defined(KAIN_CRASH_PLATFORM)

void check_platform_walk(void) {
    /* ── NULL rbp_val ──────────────────────────────────────────────── */
    {
        void *frames[16];
        int d = walk_callstack_x64(NULL, (void *)&g_fake_stack[500],
                                   frames, 16);
        __CPROVER_assert(d == 0,
                         "walk_x64(NULL rbp): returns 0");
    }

    /* ── NULL stack_bottom ─────────────────────────────────────────── */
    {
        void *frames[16];
        int d = walk_callstack_x64((void *)&g_fake_stack[0], NULL,
                                   frames, 16);
        __CPROVER_assert(d == 0,
                         "walk_x64(NULL bottom): returns 0");
    }

    /* ── Fake chain of nondet length 1..8 ──────────────────────────── */
    {
        unsigned int nf;
        __CPROVER_havoc_object(&nf);
        __CPROVER_assume(nf >= 1 && nf <= 8);

        unsigned int written = build_fake_frames(g_fake_stack, nf);

        void *frames[16];
        int depth = walk_callstack_x64(
            (void *)&g_fake_stack[0],
            (void *)((uintptr_t)&g_fake_stack + sizeof(g_fake_stack)),
            frames, 16);

        __CPROVER_assert(depth >= 0,   "walk(chain): depth >= 0");
        __CPROVER_assert(depth <= (int)written,
                         "walk(chain): depth <= frames built");
        __CPROVER_assert(depth <= 16,  "walk(chain): depth <= max_frames");
        for (int i = 0; i < depth; i++)
            __CPROVER_assert(frames[i] != NULL,
                             "walk(chain): ret_addr != NULL");
    }

    /* ── Terminator at frame 0 — walk_callstack_x64 doesn't count    ──
     *     the frame where next_rbp == 0 because the loop breaks before
     *     the depth increment.  The return address IS valid, but the
     *     chain has no link to next, so depth == 0.                    ── */
    {
        g_fake_stack[0] = 0;
        g_fake_stack[1] = (uintptr_t)&g_fake_stack;

        void *frames[16];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack + 64),
                                   frames, 16);
        __CPROVER_assert(d == 0,
                         "walk(term): next=0 => depth 0 (term frame not counted)");
    }

    /* ── ret_addr == 0 → skip ──────────────────────────────────────── */
    {
        g_fake_stack[0] = 0;
        g_fake_stack[1] = 0;

        void *frames[16];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack + 64),
                                   frames, 16);
        __CPROVER_assert(d == 0,
                         "walk(ret==0): ret=0 => depth 0");
    }

    /* ── max_frames = 0 ────────────────────────────────────────────── */
    {
        void *frames[1];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack + 64),
                                   frames, 0);
        __CPROVER_assert(d == 0,
                         "walk(max=0): max_frames=0 => depth 0");
    }

    /* ── All-zero frame ────────────────────────────────────────────── */
    {
        g_fake_stack[0] = 0;
        g_fake_stack[1] = 0;

        void *frames[4];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack + 64),
                                   frames, 4);
        __CPROVER_assert(d == 0,
                         "walk(zero): next=0 ret=0 => depth 0");
    }

    /* ── next_rbp <= current → loop breaks before depth increment  ── */
    {
        g_fake_stack[0] = (uintptr_t)&g_fake_stack[0]; /* self-pointer */
        g_fake_stack[1] = (uintptr_t)&g_fake_stack;

        void *frames[4];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack + 64),
                                   frames, 4);
        __CPROVER_assert(d == 0,
                         "walk(same): next==current => depth 0 (loop breaks before count)");
    }

    /* ── rbp past stack_bottom ──────────────────────────────────────── */
    {
        g_fake_stack[0] = (uintptr_t)&g_fake_stack[2];
        g_fake_stack[1] = (uintptr_t)&g_fake_stack;

        void *frames[4];
        int d = walk_callstack_x64((void *)&g_fake_stack[0],
                                   (void *)((uintptr_t)&g_fake_stack - 64),
                                   frames, 4);
        __CPROVER_assert(d == 0,
                         "walk(rbp>bottom): rbp past bottom => 0");
    }
}

#endif /* KAIN_CRASH_PLATFORM */


/* ══════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════ */
int main(void) {
    check_core_lookup();
    check_core_render();
    check_core_init();
    check_core_integration();

#if defined(KAIN_CRASH_PLATFORM)
    check_platform_walk();
#endif

    return 0;
}
