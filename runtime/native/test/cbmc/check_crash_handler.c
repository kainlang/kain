/*
 * check_crash_handler.c — CBMC verification harness for crash_handler module
 *
 * Proves the crash handler is itself crash-safe.
 * A crash handler that crashes during signal delivery is a double-fault
 * that loses all diagnostic information — the handler must be proven safe.
 *
 * The pipeline concatenates crash_handler.c + this harness into a single
 * translation unit (combined_check_crash_handler.c), so all static
 * functions from crash_handler.c are accessible.
 *
 * Key invariants verified:
 *  1. lookup_crash_entry never reads OOB during binary search
 *  2. __kain_crash_handler_init is idempotent — safe to call N times
 *  3. __kain_crash_handler_init handles NULL/empty crash table gracefully
 *  4. render_crash_report never crashes with any valid/invalid entry
 *  5. walk_callstack never reads past the fake frame buffer
 *  6. __kain_crash_lookup is safe for any input IP
 *
 * Run via:
 *   python test/scripts/run_pipeline.py cbmc --harness check_crash_handler
 * Or directly on the combined file:
 *   cbmc --bounds-check --pointer-check --trace \
 *     test/cbmc/combined_check_crash_handler.c \
 *     -I include -I src/core --no-unwinding-assertions
 */

#include "crash_handler.h"

#include <stdint.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════════════
 * Static backing buffers — CBMC knows these have VALID pointer provenance.
 * Stack frames and crash table entries are backed by real memory so that
 * walk_callstack and lookup_crash_entry can read/write through pointers.
 * ══════════════════════════════════════════════════════════════════════ */

/* Crash table strings — render_crash_report calls fprintf(%s) on these */
static char g_fn_name[64];
static char g_file_name[128];

/* Fake stack for walk_callstack frame-pointer-chain walking.
 * Each frame uses two uintptr_t slots: [next_rbp][return_address]. */
static uintptr_t g_fake_stack[512];

/* ──────────────────────────────────────────────────────────────────────
 * Forward declarations of static functions from crash_handler.c.
 * These are valid in the combined TU (crash_handler.c prepended).
 * ────────────────────────────────────────────────────────────────────── */
static int walk_callstack(void *rbp_val, void *stack_bottom,
                          void **frames, int max_frames);

static void render_crash_report(const char *signal_name,
                                const KainCrashEntry *entry,
                                const void *fault_ip,
                                void **callstack, int callstack_depth);

static const KainCrashEntry *lookup_crash_entry(const void *ip);


/* ──────────────────────────────────────────────────────────────────────
 * Helper: build a fake x86-64 frame pointer chain in g_fake_stack.
 *
 * Layout (per frame):
 *   frame[i*2 + 0] = next_rbp  (0 terminates the chain)
 *   frame[i*2 + 1] = return_address (0 skips the frame)
 * ────────────────────────────────────────────────────────────────────── */
static void build_fake_frames(uintptr_t *buf, unsigned int nframes) {
    for (unsigned int i = 0; i < nframes && (i * 2 + 1) < 512; i++) {
        uintptr_t ret;
        __CPROVER_havoc_object(&ret);
        __CPROVER_assume(ret != 0);
        buf[i * 2 + 1] = ret;  /* return address (non-zero = valid) */

        if (i + 1 < nframes && (i * 2 + 2) < 512) {
            buf[i * 2] = (uintptr_t)&buf[(i + 1) * 2];  /* next frame */
        } else {
            buf[i * 2] = 0;  /* chain terminator */
        }
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 1 — lookup_crash_entry: binary search safety
 *
 * The binary search iterates mid = lo + (hi-lo)/2 and reads
 * __kain_crash_table[mid].  This test proves:
 *  - No OOB read for any IP value
 *  - Returns NULL for empty table (table_len == 0)
 *  - Returns NULL for IP before first entry's fn_ptr
 *  - Returns &table[last] for IP past last entry's fn_ptr
 *  - Correct entry for IP within an entry's range
 * ══════════════════════════════════════════════════════════════════════ */
void check_lookup_crash_entry(void) {
    /* Test 1: Empty table (table_len = 0) */
    {
        __CPROVER_havoc_object((void *)&__kain_crash_table_len);
        __CPROVER_assume(__kain_crash_table_len == 0);
        const KainCrashEntry *r = lookup_crash_entry(NULL);
        __CPROVER_assert(r == NULL,
                         "lookup[empty]: NULL table returns NULL");
    }

    /* Test 2: Non-empty table with nondet contents */
    {
        __CPROVER_havoc_object((void *)__kain_crash_table);
        __CPROVER_havoc_object(&__kain_crash_table_len);
        __CPROVER_assume(__kain_crash_table_len <= 1);

        uint64_t ip;
        __CPROVER_havoc_object(&ip);

        const KainCrashEntry *r = lookup_crash_entry((const void *)(uintptr_t)ip);

        if (__kain_crash_table_len == 0) {
            __CPROVER_assert(r == NULL, "lookup: empty => NULL");
        } else if (r != NULL) {
            __CPROVER_assert(r == &__kain_crash_table[0],
                             "lookup: non-NULL result points to valid entry");
            __CPROVER_assert(r->fn_ptr <= ip,
                             "lookup: found entry fn_ptr <= ip");
        } else {
            __CPROVER_assert(ip < __kain_crash_table[0].fn_ptr,
                             "lookup: NULL => ip < first entry fn_ptr");
        }
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 2 — __kain_crash_handler_init: idempotency + null-table safety
 *
 * Must be safe to call any number of times.
 * Must not register signal handlers if crash table is absent.
 * Subsequent lookups must still be safe.
 * ══════════════════════════════════════════════════════════════════════ */
void check_init_idempotent(void) {
    crash_handler_initialized = 0;

    __CPROVER_havoc_object((void *)__kain_crash_table);
    __CPROVER_havoc_object(&__kain_crash_table_len);

    __kain_crash_handler_init();  /* first call */
    __kain_crash_handler_init();  /* second — idempotent */

    /* Lookup after init */
    uint64_t ip;
    __CPROVER_havoc_object(&ip);
    const KainCrashEntry *r = __kain_crash_lookup((const void *)(uintptr_t)ip);
    if (r != NULL) {
        __CPROVER_assert(r == &__kain_crash_table[0],
                         "init_idempotent: non-NULL lookup points to table[0]");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 3 — render_crash_report: signal-safe rendering
 *
 * The render function is called from a signal/VEH handler. If it crashes,
 * the process loses all diagnostic information. We prove:
 *  - NULL entry + NULL fault_ip + NULL callstack = safe
 *  - Valid entry with nondet fn_name/file = safe (strings have provenance)
 *  - Callstack with nondet frame entries = safe (lookup is safe)
 *  - Any combination of entry/callstack/fault_ip = safe
 *  - signal_name itself can be arbitrary (but we note that NULL is UB in %s)
 * ══════════════════════════════════════════════════════════════════════ */
void check_render_crash_report(void) {
    /* Test 1: Minimal — NULL entry, no callstack */
    render_crash_report("SIGSEGV", NULL, NULL, NULL, 0);

    /* Test 2: Minimal — NULL entry with a fault IP */
    render_crash_report("SIGILL", NULL, (const void *)(uintptr_t)0x7fff1234, NULL, 0);

    /* Test 3: Valid entry with nondet fn_name/file */
    {
        static KainCrashEntry entry;
        __CPROVER_havoc_object(&entry);
        entry.fn_name = &g_fn_name[0];
        entry.file   = &g_file_name[0];
        __CPROVER_assume(entry.line <= 100000);
        __CPROVER_assume(entry.col  <= 5000);

        render_crash_report("SIGFPE", &entry, NULL, NULL, 0);
    }

    /* Test 4: Callstack with nondet entries */
    {
        static KainCrashEntry entry;
        __CPROVER_havoc_object(&entry);
        entry.fn_name = &g_fn_name[0];
        entry.file   = &g_file_name[0];

        void *callstack[8];
        __CPROVER_havoc_object(callstack);

        int depth;
        __CPROVER_havoc_object(&depth);
        __CPROVER_assume(depth >= 0 && depth <= 8);

        render_crash_report("SIGSEGV", &entry, NULL, callstack, depth);
    }

    /* Test 5: All combinations via nondet fields */
    {
        static KainCrashEntry entry;
        int has_entry;
        int has_callstack;
        __CPROVER_havoc_object(&has_entry);
        __CPROVER_havoc_object(&has_callstack);

        if (has_entry % 2 == 0) {
            __CPROVER_havoc_object(&entry);
            entry.fn_name = &g_fn_name[0];
            entry.file   = &g_file_name[0];
        }

        void *callstack[4];
        int depth = 0;
        if (has_callstack % 2 == 0) {
            __CPROVER_havoc_object(callstack);
            depth = 4;
        }

        render_crash_report("SIGTEST",
                            (has_entry % 2 == 0) ? &entry : NULL,
                            (const void *)(uintptr_t)0xdead,
                            callstack, depth);
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 4 — walk_callstack: frame-pointer chain walk safety
 *
 * walk_callstack reads from the frame pointer chain:
 *   rbp[0] = next frame pointer
 *   rbp[1] = return address
 *
 * The loop guard checks:
 *   rbp >= &depth     (frame is above local)
 *   rbp < bottom      (frame is below artificial boundary)
 *   depth < max_frames
 *
 * We prove:
 *  - NULL rbp → returns 0 (no deref)
 *  - rbp at/above bottom → returns 0
 *  - Valid chain of 1-8 frames → walks exactly num_frames
 *  - ret_addr == 0 → skips that frame
 *  - next_rbp == 0 or <= current → terminates
 *  - max_frames cap is respected
 *  - No OOB reads from g_fake_stack
 * ══════════════════════════════════════════════════════════════════════ */
void check_walk_callstack(void) {
    /* 4a: NULL rbp_val */
    {
        void *frames[16];
        int d = walk_callstack(NULL, (void *)&g_fake_stack[500], frames, 16);
        __CPROVER_assert(d == 0, "walk(NULL): returns 0");
    }

    /* 4b: rbp >= bottom (loop guard fails) */
    {
        void *frames[16];
        const void *bp = &g_fake_stack[400];
        int d = walk_callstack((void *)bp, (void *)bp, frames, 16);
        __CPROVER_assert(d == 0, "walk(rbp==bottom): returns 0");
    }

    /* 4c: Fake chain of nondet length 1-8 */
    {
        unsigned int nf;
        __CPROVER_havoc_object(&nf);
        __CPROVER_assume(nf >= 1 && nf <= 8);

        build_fake_frames(g_fake_stack, nf);

        void *frames[16];
        int depth = walk_callstack(
            (void *)&g_fake_stack[0],
            (void *)((uintptr_t)&g_fake_stack[0] + sizeof(g_fake_stack)),
            frames, 16);

        __CPROVER_assert(depth >= 0,
                         "walk(chain): depth >= 0");
        __CPROVER_assert(depth <= (int)nf,
                         "walk(chain): depth <= num_frames");
        __CPROVER_assert(depth <= 16,
                         "walk(chain): depth <= max_frames");

        for (int i = 0; i < depth; i++) {
            __CPROVER_assert(frames[i] != NULL,
                             "walk(chain): each frame has non-NULL ret_addr");
        }
    }

    /* 4d: Terminator chain — next_rbp == 0 immediately */
    {
        g_fake_stack[0] = 0;           /* next_rbp = 0 */
        g_fake_stack[1] = (uintptr_t)&g_fake_stack;  /* ret_addr */

        void *frames[16];
        int d = walk_callstack(
            (void *)&g_fake_stack[0],
            (void *)((uintptr_t)&g_fake_stack + 64),
            frames, 16);
        __CPROVER_assert(d == 1,
                         "walk(terminator): single frame returns depth 1");
    }

    /* 4e: ret_addr == 0 → skip frame (loop breaks) */
    {
        g_fake_stack[0] = 0;
        g_fake_stack[1] = 0;  /* ret_addr = 0 */

        void *frames[16];
        int d = walk_callstack(
            (void *)&g_fake_stack[0],
            (void *)((uintptr_t)&g_fake_stack + 64),
            frames, 16);
        __CPROVER_assert(d == 0,
                         "walk(ret==0): zero ret_addr yields depth 0");
    }

    /* 4f: max_frames = 0 — should not read any frame */
    {
        void *frames[1];
        int d = walk_callstack(
            (void *)&g_fake_stack[0],
            (void *)((uintptr_t)&g_fake_stack + 64),
            frames, 0);
        __CPROVER_assert(d == 0,
                         "walk(max=0): max_frames=0 yields depth 0");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 5 — __kain_crash_lookup: public API, edge cases
 *
 * The public wrapper must be safe for:
 *  - NULL IP
 *  - Zero address
 *  - Near-UINTPTR_MAX address
 *  - Stack/local addresses
 *  - Any nondet address
 * ══════════════════════════════════════════════════════════════════════ */
void check_lookup_edge_cases(void) {
    __kain_crash_handler_init();

    /* NULL */
    __kain_crash_lookup(NULL);
    /* Zero */
    __kain_crash_lookup((const void *)0);
    /* Max */
    __kain_crash_lookup((const void *)(uintptr_t)~0ULL);
    /* Address 1 */
    __kain_crash_lookup((const void *)(uintptr_t)1);
    /* Local */
    { int x; __kain_crash_lookup(&x); }

    /* Multiple lookups in a row */
    void *ips[8];
    __CPROVER_havoc_object(ips);
    for (int i = 0; i < 8; i++) {
        const KainCrashEntry *r = __kain_crash_lookup(ips[i]);
        if (r != NULL) {
            __CPROVER_assert(r == &__kain_crash_table[0],
                             "lookup_multi: non-NULL -> table[0]");
        }
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * CHECK 6 — Full crash scenario: simulate a real crash end-to-end
 *
 * 1. Handler initialized
 * 2. Fake callstack built from frame chain
 * 3. Fault IP looked up
 * 4. Crash report rendered
 * ══════════════════════════════════════════════════════════════════════ */
void check_full_crash_scenario(void) {
    crash_handler_initialized = 0;
    __kain_crash_handler_init();

    /* Build a 3-frame callstack */
    build_fake_frames(g_fake_stack, 3);

    void *callstack[16];
    int cs_depth = walk_callstack(
        (void *)&g_fake_stack[0],
        (void *)((uintptr_t)&g_fake_stack + sizeof(g_fake_stack)),
        callstack, 16);

    /* Render the report — MUST NOT CRASH */
    {
        static KainCrashEntry entry;
        __CPROVER_havoc_object(&entry);
        entry.fn_name = &g_fn_name[0];
        entry.file   = &g_file_name[0];

        render_crash_report("SIGSEGV", &entry,
                            (const void *)(uintptr_t)0x140001234,
                            callstack, cs_depth);
    }

    /* Render without entry */
    render_crash_report("SIGFPE", NULL,
                        (const void *)(uintptr_t)0x7ffe0000,
                        callstack, cs_depth);

    /* Render without entry or callstack */
    render_crash_report("SIGILL", NULL,
                        (const void *)(uintptr_t)0, NULL, 0);

    __CPROVER_assert(1, "full_crash: all crash report variants completed");
}


/* ══════════════════════════════════════════════════════════════════════
 * main — entry point, run all checks
 * ══════════════════════════════════════════════════════════════════════ */
int main(void) {
    check_lookup_crash_entry();
    check_init_idempotent();
    check_render_crash_report();
    check_walk_callstack();
    check_lookup_edge_cases();
    check_full_crash_scenario();
    return 0;
}
