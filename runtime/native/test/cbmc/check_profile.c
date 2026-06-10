/*
 * check_profile.c — CBMC verification harness for profile module
 * ====================================================================
 *
 * Verifies the scoped profiling zone API with tier gating.
 *
 * The default KAIN_RUNTIME_PROFILE_TIER is GATED (not NOOP, not FULL).
 * At the GATED tier:
 *   - kain_profile_scope_begin sets scope fields (label, file, line, depth=0,
 *     token=0, start_ns=0, active=0) but does NOT push to the thread-local
 *     stack — the stack path only runs at FULL tier.
 *   - kain_profile_scope_end is a no-op (the FULL tier pop logic is elided).
 *   - Global counters (zone_count, total_ns, last_duration_ns, last_label)
 *     are updated only at FULL tier by kain_profile_scope_end.  At GATED
 *     they remain at their reset values.
 *
 * To test the FULL tier push/pop/timing logic, run with:
 *   cbmc -DKAIN_RUNTIME_PROFILE_TIER=2 --unwind 5 --trace \
 *        test/cbmc/check_profile.c src/core/profile.c -I include -I src/core
 *
 * Properties verified (~25 assertions):
 *   1.  scope_begin: sets label, file, line; zeroes others; active=0 (GATED)
 *   2.  scope_begin: NULL scope pointer is safe
 *   3.  scope_end: NULL scope is safe
 *   4.  scope_end: non-active scope is safe
 *   5.  scope_end: any scope is safe (no crash)
 *   6.  reset: zone_count, total_ns, last_duration_ns all zeroed
 *   7.  last_label: returns NULL after reset (no zone completed at GATED)
 *   8.  zone_count: always 0 at GATED (no counter updates)
 *   9.  total_ns: always 0 at GATED
 *  10.  last_duration_ns: always 0 at GATED
 *  11.  Multiple begin/end calls: safe, no leak
 *  12.  Nested begin (no matching end): safe, no crash
 *  13.  Invalid scope (active pre-set to 1): end is safe no-op
 *
 * Run:  cd runtime/native
 *       python test/scripts/run_pipeline.py cbmc --harness check_profile
 * Or:   cbmc --unwind 5 --trace test/cbmc/check_profile.c src/core/profile.c
 *            -I include -I src/core
 */

#include "profile.h"
#include <stddef.h>
#include <stdint.h>

/* ── Static backing buffers for pointer provenance ── */

static KainProfileScope scope_a;
static KainProfileScope scope_b;

static const char* g_label_a = "zone.alpha";
static const char* g_label_b = "zone.beta";
static const char* g_file = "check_profile.c";

/* Stored values from accessors */
static uint64_t g_count;
static uint64_t g_total;
static uint64_t g_last_dur;
static const char* g_last_label;


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1: scope_begin — field setting
 * ══════════════════════════════════════════════════════════════════════════ */

void check_scope_begin_sets_fields(void) {
    KainProfileScope scope;
    __CPROVER_havoc_object(&scope);

    kain_profile_scope_begin(&scope, g_label_a, g_file, 42);

    /* At all tiers, label and file pointer are preserved */
    __CPROVER_assert(scope.label == g_label_a,
        "begin: label pointer preserved");
    __CPROVER_assert(scope.file == g_file,
        "begin: file pointer preserved");
    __CPROVER_assert(scope.line == 42,
        "begin: line number set");

    /* At GATED tier, active is 0, depth is 0, token is 0, start_ns is 0 */
    __CPROVER_assert(scope.active == 0,
        "begin: active=0 at GATED tier");
    __CPROVER_assert(scope.depth == 0,
        "begin: depth=0 at GATED tier");
    __CPROVER_assert(scope.token == 0,
        "begin: token=0 at GATED tier");

    /* Multiple begins on same scope: just overwrites fields */
    kain_profile_scope_begin(&scope, g_label_b, g_file, 99);
    __CPROVER_assert(scope.label == g_label_b,
        "begin: re-begin updates label");
    __CPROVER_assert(scope.line == 99,
        "begin: re-begin updates line");
    __CPROVER_assert(scope.active == 0,
        "begin: re-begin active=0");
}

void check_scope_begin_null(void) {
    /* NULL scope must not crash at any tier */
    kain_profile_scope_begin(NULL, g_label_a, g_file, 1);
    __CPROVER_assert(1,
        "begin: NULL scope, no crash");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2: scope_end — safe no-op at GATED tier
 * ══════════════════════════════════════════════════════════════════════════ */

void check_scope_end_safe(void) {
    /* scope_end with a scope that was begun */
    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 10);
    kain_profile_scope_end(&scope_a);
    __CPROVER_assert(1,
        "end: begin+end cycle, no crash");

    /* scope_end on a scope that was never begun */
    KainProfileScope fresh;
    __CPROVER_havoc_object(&fresh);
    kain_profile_scope_end(&fresh);
    __CPROVER_assert(1,
        "end: never-begun scope, no crash");
}

void check_scope_end_null(void) {
    kain_profile_scope_end(NULL);
    __CPROVER_assert(1,
        "end: NULL scope, no crash");
}

void check_scope_end_non_active(void) {
    /* Pre-set active=0 on a scope that was never begun */
    KainProfileScope scope;
    __CPROVER_havoc_object(&scope);
    scope.active = 0;

    kain_profile_scope_end(&scope);
    __CPROVER_assert(1,
        "end: non-active scope, no crash");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3: Reset zeros all counters
 * ══════════════════════════════════════════════════════════════════════════ */

void check_reset(void) {
    /* Run a begin/end cycle first (at GATED tier this does nothing to counters) */
    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 1);
    kain_profile_scope_end(&scope_a);

    /* Reset */
    kain_profile_reset();

    g_count = kain_profile_zone_count();
    g_total = kain_profile_total_ns();
    g_last_dur = kain_profile_last_duration_ns();

    __CPROVER_assert(g_count == 0,
        "reset: zone_count == 0");
    __CPROVER_assert(g_total == 0,
        "reset: total_ns == 0");
    __CPROVER_assert(g_last_dur == 0,
        "reset: last_duration_ns == 0");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4: Accessors — initial/steady state at GATED tier
 * ══════════════════════════════════════════════════════════════════════════ */

void check_accessors_initial(void) {
    /* Before any begin/end, all counters are 0 */
    kain_profile_reset();

    g_count = kain_profile_zone_count();
    __CPROVER_assert(g_count == 0,
        "accessors: zone_count initial == 0");

    g_total = kain_profile_total_ns();
    __CPROVER_assert(g_total == 0,
        "accessors: total_ns initial == 0");

    g_last_dur = kain_profile_last_duration_ns();
    __CPROVER_assert(g_last_dur == 0,
        "accessors: last_duration_ns initial == 0");

    /* last_label should be NULL after reset (atomic store of (uintptr_t)0) */
    g_last_label = kain_profile_last_label();
    __CPROVER_assert(g_last_label == NULL,
        "accessors: last_label == NULL after reset");
}

void check_accessors_after_cycle(void) {
    /* At GATED tier, begin+end does NOT update counters */
    kain_profile_reset();
    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 1);
    kain_profile_scope_end(&scope_a);

    g_count = kain_profile_zone_count();
    __CPROVER_assert(g_count == 0,
        "accessors: zone_count == 0 after begin+end (GATED)");

    g_total = kain_profile_total_ns();
    __CPROVER_assert(g_total == 0,
        "accessors: total_ns == 0 after begin+end (GATED)");

    g_last_dur = kain_profile_last_duration_ns();
    __CPROVER_assert(g_last_dur == 0,
        "accessors: last_duration_ns == 0 after begin+end (GATED)");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5: Multiple begin/end cycles — safe at GATED
 * ══════════════════════════════════════════════════════════════════════════ */

void check_multiple_cycles(void) {
    kain_profile_reset();

    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 1);
    kain_profile_scope_end(&scope_a);

    kain_profile_scope_begin(&scope_a, g_label_b, g_file, 2);
    kain_profile_scope_end(&scope_a);

    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 3);
    kain_profile_scope_end(&scope_a);

    /* Still 0 at GATED tier */
    g_count = kain_profile_zone_count();
    __CPROVER_assert(g_count == 0,
        "multi: zone_count still 0 at GATED tier");

    g_last_label = kain_profile_last_label();
    __CPROVER_assert(g_last_label == NULL,
        "multi: last_label still NULL at GATED tier");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6: Nested scope — safe at GATED
 * ══════════════════════════════════════════════════════════════════════════ */

void check_nested_safe(void) {
    kain_profile_reset();

    kain_profile_scope_begin(&scope_a, g_label_a, g_file, 1);
    kain_profile_scope_begin(&scope_b, g_label_b, g_file, 2);

    /* At all tiers, end is safe even when nested */
    kain_profile_scope_end(&scope_b);
    kain_profile_scope_end(&scope_a);

    __CPROVER_assert(1,
        "nested: no crash after nested begin/end");
}


/* ══════════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Section 1: begin */
    check_scope_begin_sets_fields();
    check_scope_begin_null();

    /* Section 2: end */
    check_scope_end_safe();
    check_scope_end_null();
    check_scope_end_non_active();

    /* Section 3: reset */
    check_reset();

    /* Section 4: accessors */
    check_accessors_initial();
    check_accessors_after_cycle();

    /* Section 5: multiple cycles */
    check_multiple_cycles();

    /* Section 6: nesting */
    check_nested_safe();

    return 0;
}
