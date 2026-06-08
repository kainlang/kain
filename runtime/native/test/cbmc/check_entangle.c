/*
 * check_entangle.c — CBMC verification harness for world entangle registry
 *
 * Tests authority↔mirror binding registry: register, get, reset, max
 * capacity, duplicate registration, and edge cases.
 *
 * Key invariants verified:
 *   - entangle_registry_register succeeds with valid strings and increments count
 *   - entangle_registry_get with valid index returns exact binding data
 *   - NULL authority/mirror/policy/type_name rejected with -1; count unchanged
 *   - Empty-string authority rejected with -1; count unchanged
 *   - entangle_registry_reset clears all bindings (count = 0, get fails)
 *   - entangle_registry_get before any registration returns -1
 *   - entangle_registry_get with out-of-bounds index returns -1
 *   - entangle_registry_get with NULL out_binding returns -1
 *   - Full-capacity registration returns -3 (overload protection)
 *   - Duplicate registrations create distinct entries at sequential indices
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_entangle --unwind 8
 * Or:     cbmc --unwind 8 --no-unwinding-assertions --trace \
 *             test/cbmc/check_entangle.c src/core/entangle.c \
 *             -I include -I src/core
 */

#include "entangle.h"

/* ── Static backing buffers for string pointer provenance ──
 * CBMC needs real objects behind every pointer so that pointer-
 * dereference and memcpy have valid target addresses.  These
 * static arrays fill that role for all nondet-string tests.
 */
static char g_auth_buf[16];
static char g_mir_buf[16];
static char g_policy_buf[8];
static char g_type_buf[16];
static KainRuntimeEntangleBinding g_get_binding;


/* ═══════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * make_valid_nondet_strings
 *
 * Fill the four static buffers with nondet content, then constrain
 * each to be a valid C string: non-empty and null-terminated within
 * the buffer's capacity.  The null-byte position bounds strlen()
 * so that runtime_copy_entangle_text will always succeed (the length
 * is guaranteed < the destination field capacity).
 *
 * After this call, g_auth_buf, g_mir_buf, g_policy_buf, g_type_buf
 * are safe to pass as the four arguments to entangle_registry_register.
 * ────────────────────────────────────────────────────────────────────── */
static void make_valid_nondet_strings(void) {
    __CPROVER_havoc_object(g_auth_buf);
    __CPROVER_havoc_object(g_mir_buf);
    __CPROVER_havoc_object(g_policy_buf);
    __CPROVER_havoc_object(g_type_buf);

    /* Non-empty */
    __CPROVER_assume(g_auth_buf[0]   != '\0');
    __CPROVER_assume(g_mir_buf[0]    != '\0');
    __CPROVER_assume(g_policy_buf[0] != '\0');
    __CPROVER_assume(g_type_buf[0]   != '\0');

    /* Null-terminated within bounds — guarantees strlen < field capacity
     * so runtime_copy_entangle_text never returns -2 for these buffers.
     * Fields capacities: authority=256, mirror=256, policy=64, type=128.
     * Our buffers (16/16/8/16) are well within those limits. */
    __CPROVER_assume(g_auth_buf[15]   == '\0');
    __CPROVER_assume(g_mir_buf[15]    == '\0');
    __CPROVER_assume(g_policy_buf[7]  == '\0');
    __CPROVER_assume(g_type_buf[15]   == '\0');
}


/* ──────────────────────────────────────────────────────────────────────
 * check_register_null_all
 *
 * Internal helper — check that passing NULL for ANY of the four
 * register arguments produces -1 and leaves the registry empty.
 * We test each position independently so CBMC sees every null-
 * branch.  Valid strings come from the nondet buffers (already
 * set up by the caller via make_valid_nondet_strings).
 * ────────────────────────────────────────────────────────────────────── */
static void check_register_null_all(void) {
    /* --- NULL authority --- */
    entangle_registry_reset();
    int rc = entangle_registry_register(NULL, g_mir_buf, g_policy_buf, g_type_buf);
    __CPROVER_assert(rc == -1,
                     "null_authority: register returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "null_authority: count unchanged");

    /* --- NULL mirror --- */
    entangle_registry_reset();
    rc = entangle_registry_register(g_auth_buf, NULL, g_policy_buf, g_type_buf);
    __CPROVER_assert(rc == -1,
                     "null_mirror: register returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "null_mirror: count unchanged");

    /* --- NULL policy --- */
    entangle_registry_reset();
    rc = entangle_registry_register(g_auth_buf, g_mir_buf, NULL, g_type_buf);
    __CPROVER_assert(rc == -1,
                     "null_policy: register returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "null_policy: count unchanged");

    /* --- NULL type_name --- */
    entangle_registry_reset();
    rc = entangle_registry_register(g_auth_buf, g_mir_buf, g_policy_buf, NULL);
    __CPROVER_assert(rc == -1,
                     "null_type_name: register returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "null_type_name: count unchanged");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Registration tests
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_register_valid
 *
 * Register a binding with all four fields as valid nondet strings.
 * Expected: returns 0, count increments to 1.
 * ────────────────────────────────────────────────────────────────────── */
void check_register_valid(void) {
    entangle_registry_reset();
    make_valid_nondet_strings();

    size_t before = entangle_registry_count();

    int rc = entangle_registry_register(
        g_auth_buf, g_mir_buf, g_policy_buf, g_type_buf);

    __CPROVER_assert(rc == 0,
                     "register_valid: returns 0");
    __CPROVER_assert(entangle_registry_count() == before + 1,
                     "register_valid: count incremented");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_register_null_args
 *
 * Each of the four register arguments, when NULL, must cause the
 * function to return -1 with zero state change.
 * ────────────────────────────────────────────────────────────────────── */
void check_register_null_args(void) {
    make_valid_nondet_strings();
    check_register_null_all();
}


/* ──────────────────────────────────────────────────────────────────────
 * check_register_empty_authority
 *
 * An empty-string authority (src[0] == '\0') triggers the
 * runtime_copy_entangle_text rejection path and returns -1.
 * The other three fields are valid nondet strings.
 * ────────────────────────────────────────────────────────────────────── */
void check_register_empty_authority(void) {
    entangle_registry_reset();
    make_valid_nondet_strings();

    int rc = entangle_registry_register("", g_mir_buf, g_policy_buf, g_type_buf);
    __CPROVER_assert(rc == -1,
                     "empty_authority: returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "empty_authority: count unchanged");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_register_empty_mirror
 *
 * Mirror too must be non-empty; empty-string mirror is rejected.
 * ────────────────────────────────────────────────────────────────────── */
void check_register_empty_mirror(void) {
    entangle_registry_reset();
    make_valid_nondet_strings();

    int rc = entangle_registry_register(g_auth_buf, "", g_policy_buf, g_type_buf);
    __CPROVER_assert(rc == -1,
                     "empty_mirror: returns -1");
    __CPROVER_assert(entangle_registry_count() == 0,
                     "empty_mirror: count unchanged");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Get tests
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_register_get_roundtrip
 *
 * Register a binding with known fixed strings, then retrieve it via
 * entangle_registry_get(0, ...).  Verify that every field was copied
 * faithfully by checking exact byte content.
 *
 * Using fixed strings (rather than nondet) lets us assert exact
 * content matches — this proves the struct-copy chain works end-to-end.
 * ────────────────────────────────────────────────────────────────────── */
void check_register_get_roundtrip(void) {
    entangle_registry_reset();

    int rc = entangle_registry_register("kain_auth", "kain_mir",
                                        "policy01", "t_entangle");
    __CPROVER_assert(rc == 0,
                     "get_roundtrip: register succeeded");
    __CPROVER_assert(entangle_registry_count() == 1,
                     "get_roundtrip: count == 1");

    /* Retrieve */
    __CPROVER_havoc_object(&g_get_binding);
    rc = entangle_registry_get(0, &g_get_binding);
    __CPROVER_assert(rc == 0,
                     "get_roundtrip: get(0) succeeded");

    /* --- authority: "kain_auth" --- */
    __CPROVER_assert(g_get_binding.authority[0] == 'k',
                     "roundtrip: auth[0] == 'k'");
    __CPROVER_assert(g_get_binding.authority[4] == '_',
                     "roundtrip: auth[4] == '_'");
    __CPROVER_assert(g_get_binding.authority[8] == 'h',
                     "roundtrip: auth[8] == 'h'");
    __CPROVER_assert(g_get_binding.authority[9] == '\0',
                     "roundtrip: auth[9] == '\\0'");

    /* --- mirror: "kain_mir" --- */
    __CPROVER_assert(g_get_binding.mirror[0] == 'k',
                     "roundtrip: mir[0] == 'k'");
    __CPROVER_assert(g_get_binding.mirror[5] == '_',
                     "roundtrip: mir[5] == '_'");
    __CPROVER_assert(g_get_binding.mirror[8] == '\0',
                     "roundtrip: mir[8] == '\\0'");

    /* --- policy: "policy01" --- */
    __CPROVER_assert(g_get_binding.policy[0] == 'p',
                     "roundtrip: pol[0] == 'p'");
    __CPROVER_assert(g_get_binding.policy[6] == '0',
                     "roundtrip: pol[6] == '0'");
    __CPROVER_assert(g_get_binding.policy[8] == '\0',
                     "roundtrip: pol[8] == '\\0'");

    /* --- type_name: "t_entangle" --- */
    __CPROVER_assert(g_get_binding.type_name[0] == 't',
                     "roundtrip: type[0] == 't'");
    __CPROVER_assert(g_get_binding.type_name[9] == 'e',
                     "roundtrip: type[9] == 'e'");
    __CPROVER_assert(g_get_binding.type_name[10] == '\0',
                     "roundtrip: type[10] == '\\0'");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_get_before_register
 *
 * entangle_registry_get when the registry is empty must return -1.
 * ────────────────────────────────────────────────────────────────────── */
void check_get_before_register(void) {
    entangle_registry_reset();

    __CPROVER_havoc_object(&g_get_binding);
    int rc = entangle_registry_get(0, &g_get_binding);
    __CPROVER_assert(rc == -1,
                     "get_before: index 0 with empty registry returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_get_out_of_bounds
 *
 * entangle_registry_get with an index at or past the live count must
 * return -1.  After registering one binding, index 1 is OOB.
 * A very large index (9999) is also OOB regardless of state.
 * ────────────────────────────────────────────────────────────────────── */
void check_get_out_of_bounds(void) {
    entangle_registry_reset();

    /* Register one binding */
    int rc = entangle_registry_register("test_a", "test_m", "test_p", "test_t");
    __CPROVER_assert(rc == 0, "get_oob: register succeeded");
    __CPROVER_assert(entangle_registry_count() == 1,
                     "get_oob: count == 1");

    /* Index equal to count → OOB */
    __CPROVER_havoc_object(&g_get_binding);
    rc = entangle_registry_get(1, &g_get_binding);
    __CPROVER_assert(rc == -1,
                     "get_oob: index 1 with count 1 returns -1");

    /* Large index → OOB */
    __CPROVER_havoc_object(&g_get_binding);
    rc = entangle_registry_get(9999, &g_get_binding);
    __CPROVER_assert(rc == -1,
                     "get_oob: large index returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_get_null_out
 *
 * entangle_registry_get with a NULL out_binding pointer must return -1
 * without crashing.
 * ────────────────────────────────────────────────────────────────────── */
void check_get_null_out(void) {
    entangle_registry_reset();

    entangle_registry_register("a", "m", "p", "t");

    int rc = entangle_registry_get(0, NULL);
    __CPROVER_assert(rc == -1,
                     "get_null_out: NULL out_binding returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Reset tests
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_reset
 *
 * 1. Register a valid binding → count == 1.
 * 2. entangle_registry_reset → count == 0.
 * 3. Any get call now fails (all bindings cleared).
 * ────────────────────────────────────────────────────────────────────── */
void check_reset(void) {
    entangle_registry_reset();
    make_valid_nondet_strings();

    /* Register */
    int rc = entangle_registry_register(
        g_auth_buf, g_mir_buf, g_policy_buf, g_type_buf);
    __CPROVER_assert(rc == 0,
                     "reset: register succeeded");
    __CPROVER_assert(entangle_registry_count() == 1,
                     "reset: count == 1 before reset");

    /* Reset */
    entangle_registry_reset();
    __CPROVER_assert(entangle_registry_count() == 0,
                     "reset: count == 0 after reset");

    /* Get at any index must fail */
    __CPROVER_havoc_object(&g_get_binding);
    rc = entangle_registry_get(0, &g_get_binding);
    __CPROVER_assert(rc == -1,
                     "reset: get(0) fails after reset");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Max capacity test
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_max_capacity
 *
 * Prove that entangle_registry_register correctly rejects registrations
 * when the binding count reaches ENTANGLE_MAX_BINDINGS (128).
 *
 * Approach: set the internal static count to a nondet value in
 * [0, ENTANGLE_MAX_BINDINGS], then call register.  CBMC explores
 * ALL possible starting counts in one go:
 *
 *   - count < 128 → register succeeds (returns 0), count incremented
 *   - count == 128 → register returns -3, count unchanged
 *
 * This is far more efficient than registering 128 times in a loop,
 * and it proves the boundary condition for every reachable state.
 *
 * The internal static variable g_kain_entangle_binding_count is
 * accessible because the harness is concatenated with entangle.c
 * into one translation unit (same TU → same file scope).
 * ────────────────────────────────────────────────────────────────────── */
void check_max_capacity(void) {
    entangle_registry_reset();

    /* At this point count == 0.  Set it to a nondet value spanning
     * the full range [0, ENTANGLE_MAX_BINDINGS] so CBMC explores
     * every possible starting state. */
    size_t starting_count;
    __CPROVER_havoc_object(&starting_count);
    __CPROVER_assume(starting_count <= ENTANGLE_MAX_BINDINGS);

    /* Direct access to the internal static counter — same TU. */
    g_kain_entangle_binding_count = starting_count;

    /* Attempt to register one more binding. */
    int rc = entangle_registry_register("a", "b", "c", "d");

    if (starting_count < ENTANGLE_MAX_BINDINGS) {
        /* Room available — registration succeeds and count advances. */
        __CPROVER_assert(rc == 0,
                         "max_capacity: register OK when count < max");
        __CPROVER_assert(g_kain_entangle_binding_count == starting_count + 1,
                         "max_capacity: count incremented");
    } else {
        /* Registry full — registration rejected with -3, count stable. */
        __CPROVER_assert(rc == -3,
                         "max_capacity: register returns -3 when full");
        __CPROVER_assert(g_kain_entangle_binding_count == starting_count,
                         "max_capacity: count unchanged after overflow");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * Duplicate registration test
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_duplicate
 *
 * Register the same binding twice.  The entangle registry does NOT
 * deduplicate — it stores each registration as a separate entry.
 * After two identical registrations:
 *   - count == 2
 *   - Both entries are independently retrievable
 *   - Both entries have identical content
 *   - Each entry occupies a distinct index
 * ────────────────────────────────────────────────────────────────────── */
void check_duplicate(void) {
    entangle_registry_reset();

    /* First registration */
    int rc1 = entangle_registry_register("dup_auth", "dup_mir",
                                         "dup_pol", "dup_type");
    __CPROVER_assert(rc1 == 0,
                     "duplicate: first register returns 0");
    __CPROVER_assert(entangle_registry_count() == 1,
                     "duplicate: count == 1 after first");

    /* Second registration (identical values) */
    int rc2 = entangle_registry_register("dup_auth", "dup_mir",
                                         "dup_pol", "dup_type");
    __CPROVER_assert(rc2 == 0,
                     "duplicate: second register returns 0");
    __CPROVER_assert(entangle_registry_count() == 2,
                     "duplicate: count == 2 after second");

    /* Retrieve both entries */
    KainRuntimeEntangleBinding out0, out1;
    __CPROVER_havoc_object(&out0);
    __CPROVER_havoc_object(&out1);

    int rg0 = entangle_registry_get(0, &out0);
    __CPROVER_assert(rg0 == 0,
                     "duplicate: get(0) succeeds");
    int rg1 = entangle_registry_get(1, &out1);
    __CPROVER_assert(rg1 == 0,
                     "duplicate: get(1) succeeds");

    /* Both entries have identical first-byte content */
    __CPROVER_assert(out0.authority[0] == 'd',
                     "duplicate: entry0 authority[0] == 'd'");
    __CPROVER_assert(out1.authority[0] == 'd',
                     "duplicate: entry1 authority[0] == 'd'");
    __CPROVER_assert(out0.mirror[0] == 'd',
                     "duplicate: entry0 mirror[0] == 'd'");
    __CPROVER_assert(out1.mirror[0] == 'd',
                     "duplicate: entry1 mirror[0] == 'd'");
    __CPROVER_assert(out0.policy[0] == 'd',
                     "duplicate: entry0 policy[0] == 'd'");
    __CPROVER_assert(out1.policy[0] == 'd',
                     "duplicate: entry1 policy[0] == 'd'");
    __CPROVER_assert(out0.type_name[0] == 'd',
                     "duplicate: entry0 type[0] == 'd'");
    __CPROVER_assert(out1.type_name[0] == 'd',
                     "duplicate: entry1 type[0] == 'd'");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Multiple bindings test
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_multiple_registrations
 *
 * Register three bindings with distinct values, then verify that each
 * is independently retrievable at its expected index and that
 * get(i) for i > last fails.
 * ────────────────────────────────────────────────────────────────────── */
void check_multiple_registrations(void) {
    entangle_registry_reset();

    /* Register three distinct bindings */
    int r1 = entangle_registry_register("sys.net",  "core/gate",  "strict",   "tcp_endpoint");
    int r2 = entangle_registry_register("sys.fs",   "virt/blk",   "isolated", "block_device");
    int r3 = entangle_registry_register("sys.gpu",  "dev/vk",     "unified",  "vulkan_context");

    __CPROVER_assert(r1 == 0, "multi: first register OK");
    __CPROVER_assert(r2 == 0, "multi: second register OK");
    __CPROVER_assert(r3 == 0, "multi: third register OK");
    __CPROVER_assert(entangle_registry_count() == 3,
                     "multi: count == 3");

    /* Retrieve and spot-check each */
    KainRuntimeEntangleBinding b0, b1, b2;
    __CPROVER_havoc_object(&b0);
    __CPROVER_havoc_object(&b1);
    __CPROVER_havoc_object(&b2);

    __CPROVER_assert(entangle_registry_get(0, &b0) == 0,
                     "multi: get(0) OK");
    __CPROVER_assert(entangle_registry_get(1, &b1) == 0,
                     "multi: get(1) OK");
    __CPROVER_assert(entangle_registry_get(2, &b2) == 0,
                     "multi: get(2) OK");

    /* Distinct first characters — each entry has unique authority prefix */
    __CPROVER_assert(b0.authority[0] == 's',
                     "multi: b0 auth starts with 's'");
    __CPROVER_assert(b1.authority[4] == 'f',
                     "multi: b1 auth[4] == 'f' (sys.fs)");
    __CPROVER_assert(b2.authority[4] == 'g',
                     "multi: b2 auth[4] == 'g' (sys.gpu)");

    /* Index out of range */
    KainRuntimeEntangleBinding out;
    __CPROVER_havoc_object(&out);
    int rc = entangle_registry_get(3, &out);
    __CPROVER_assert(rc == -1,
                     "multi: get(3) with count 3 returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Multiple resets test
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_reset_idempotent
 *
 * Calling entangle_registry_reset on an already-empty registry is a
 * no-op (idempotent).  After double reset, count stays 0, get still
 * fails, and a fresh register succeeds.
 * ────────────────────────────────────────────────────────────────────── */
void check_reset_idempotent(void) {
    entangle_registry_reset();
    entangle_registry_reset();  /* second reset on empty state */

    __CPROVER_assert(entangle_registry_count() == 0,
                     "reset_idempotent: count == 0 after double reset");

    /* Get still fails */
    __CPROVER_havoc_object(&g_get_binding);
    __CPROVER_assert(entangle_registry_get(0, &g_get_binding) == -1,
                     "reset_idempotent: get fails after double reset");

    /* Fresh register still works */
    int rc = entangle_registry_register("fresh", "new", "def", "reset_test");
    __CPROVER_assert(rc == 0,
                     "reset_idempotent: fresh register succeeds after reset");
    __CPROVER_assert(entangle_registry_count() == 1,
                     "reset_idempotent: count == 1 after fresh register");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Registration */
    check_register_valid();
    check_register_null_args();
    check_register_empty_authority();
    check_register_empty_mirror();

    /* Get */
    check_register_get_roundtrip();
    check_get_before_register();
    check_get_out_of_bounds();
    check_get_null_out();

    /* Reset */
    check_reset();
    check_reset_idempotent();

    /* Capacity */
    check_max_capacity();

    /* Multiple / duplicate */
    check_duplicate();
    check_multiple_registrations();

    return 0;
}
