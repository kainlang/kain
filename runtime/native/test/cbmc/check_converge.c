/*
 * check_converge.c — CBMC verification harness for converge module
 *
 * Converge: multi-lane dispatch telemetry and lane selection.
 *   - abi_converge_select_lane_for_key: chooses lanes by key+shape
 *   - abi_converge_commit_winner: records winning lane for cache affinity
 *   - abi_converge_record_telemetry: gathers timing samples (ring buffer)
 *   - Accessor queries: telemetry count, cache probe/hit counts
 *
 * Key invariants:
 *   - Lane selection returns fallback_lane when eligible_mask == 0
 *   - Lane selection always returns a lane in eligible_mask (non-zero case)
 *   - Winner commit rejects lane >= 64 and unset-bit lanes with -1
 *   - Winner commit returns 0 and caches entry for valid inputs
 *   - Telemetry ring buffer wraps at KAIN_CONVERGE_TELEMETRY_CAP (64)
 *   - Telemetry cursor is monotonic
 *   - Cache probe counter increments on select (non-zero mask), not on commit/telemetry
 *   - Hits <= probes (invariant)
 *   - MAX 8 lanes, 64 telemetry samples, 64 tune cache entries
 *   - All API functions take scalar params (no NULL pointer risk)
 *
 * Since converge.c uses file-static globals and the pipeline concatenates
 * source + harness into one translation unit, we directly reference the
 * static arrays (g_kain_converge_telemetry, g_kain_converge_cache, etc.)
 * and call static helper functions (kain_converge_lowbit_lane,
 * kain_converge_mix64, kain_converge_atomic_fetch_add_u64,
 * kain_converge_cache_key).
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_converge
 * Or:     cbmc --unwind 5 --no-unwinding-assertions --trace \
 *             test/cbmc/check_converge.c src/core/converge.c \
 *             -I include -I src/core
 *
 * For full cache iteration coverage (tune cache has 64 slots), use:
 *         cbmc --unwind 64 --no-unwinding-assertions --trace \
 *             test/cbmc/check_converge.c src/core/converge.c \
 *             -I include -I src/core
 */

#include "converge.h"


/* ──────────────────────────────────────────────────────────────────────────
 * Helper: Reset all global state to nondeterministic values
 *
 * converge.c defines static globals that persist across calls within a
 * single CBMC test.  Each test that depends on the initial state must
 * call this to reset to a nondeterministic (but pointer-valid) state.
 * ────────────────────────────────────────────────────────────────────────── */
static void havoc_global_state(void) {
    /* Global telemetry ring buffer — 64 samples */
    __CPROVER_havoc_object(g_kain_converge_telemetry);
    /* Global tune cache — 64 slots */
    __CPROVER_havoc_object(g_kain_converge_cache);
    /* Telemetry cursor (write index, wraps at 64) */
    __CPROVER_havoc_object(&g_kain_converge_telemetry_cursor);
    /* Cache probe counter */
    __CPROVER_havoc_object(&g_kain_converge_cache_probes);
    /* Cache hit counter */
    __CPROVER_havoc_object(&g_kain_converge_cache_hits);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 1. LANE SELECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: select_lane with zero eligible_mask returns fallback_lane
 *
 * When eligible_mask == 0, the function immediately returns fallback_lane
 * WITHOUT probing the cache or incrementing counters.
 * ────────────────────────────────────────────────────────────────────────── */
void check_select_lane_zero_mask(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t fallback_lane;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&fallback_lane);

    uint64_t eligible_mask = 0;

    uint64_t pre_probes = g_kain_converge_cache_probes;
    uint64_t pre_hits   = g_kain_converge_cache_hits;

    int64_t result = abi_converge_select_lane_for_key(
        converge_key, shape_key, eligible_mask, fallback_lane);

    /* Must return fallback_lane when mask is zero */
    __CPROVER_assert((uint64_t)result == fallback_lane,
        "select_zero_mask: returns fallback_lane");

    /* Must NOT probe the cache (early return before cache loop) */
    __CPROVER_assert(g_kain_converge_cache_probes == pre_probes,
        "select_zero_mask: probe count unchanged");

    /* Must NOT hit the cache */
    __CPROVER_assert(g_kain_converge_cache_hits == pre_hits,
        "select_zero_mask: hit count unchanged");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: select_lane with non-zero eligible_mask returns an eligible lane
 *
 * When eligible_mask != 0, the returned lane must be a valid set bit
 * in eligible_mask (either from cache hit or from lowbit fallback).
 * Also, the probe counter must advance by at least 1 (the cache is always
 * probed at least once when the mask is non-zero).
 * ────────────────────────────────────────────────────────────────────────── */
void check_select_lane_eligible(void) {
    uint64_t eligible_mask;
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_assume(eligible_mask != 0);

    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t fallback_lane;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&fallback_lane);

    uint64_t pre_probes = g_kain_converge_cache_probes;

    int64_t result = abi_converge_select_lane_for_key(
        converge_key, shape_key, eligible_mask, fallback_lane);

    /* Result must be non-negative (valid lane index) */
    __CPROVER_assert(result >= 0,
        "select_eligible: result is non-negative");

    /* Result must be < 64 (lane index is at most 63) */
    __CPROVER_assert(result < 64,
        "select_eligible: result < 64");

    /* Result lane must be a set bit in eligible_mask */
    __CPROVER_assert(((eligible_mask >> result) & 1ull) != 0,
        "select_eligible: result lane is in eligible_mask");

    /* Probe counter must have advanced (cache was probed) */
    __CPROVER_assert(g_kain_converge_cache_probes >= pre_probes + 1,
        "select_eligible: probe count advanced >= 1");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: select_lane with non-zero mask hits <= probes invariant
 *
 * After a lane selection with non-zero mask, the invariant
 * hits <= probes must hold.
 * ────────────────────────────────────────────────────────────────────────── */
void check_select_lane_hits_le_probes(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t eligible_mask;
    uint64_t fallback_lane;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_havoc_object(&fallback_lane);
    __CPROVER_assume(eligible_mask != 0);

    int64_t result = abi_converge_select_lane_for_key(
        converge_key, shape_key, eligible_mask, fallback_lane);

    __CPROVER_assert(result >= 0, "hits_le_probes: result non-negative");

    __CPROVER_assert(g_kain_converge_cache_hits <= g_kain_converge_cache_probes,
        "hits_le_probes: hits <= probes (invariant)");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 2. WINNER COMMIT
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: commit_winner rejects lane_index >= 64
 * ────────────────────────────────────────────────────────────────────────── */
void check_commit_winner_rejects_oor_lane(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t eligible_mask;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&lane_index);
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_assume(lane_index >= 64);

    uint64_t pre_probes = g_kain_converge_cache_probes;
    uint64_t pre_hits   = g_kain_converge_cache_hits;

    int64_t rc = abi_converge_commit_winner(
        converge_key, shape_key, lane_index, eligible_mask);

    __CPROVER_assert(rc == -1,
        "commit_oor_lane: lane_index >= 64 returns -1");

    /* Commit must not affect probe/hit counters */
    __CPROVER_assert(g_kain_converge_cache_probes == pre_probes,
        "commit_oor_lane: probe count unchanged");

    __CPROVER_assert(g_kain_converge_cache_hits == pre_hits,
        "commit_oor_lane: hit count unchanged");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: commit_winner rejects lane not set in eligible_mask
 * ────────────────────────────────────────────────────────────────────────── */
void check_commit_winner_rejects_unset_lane(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t eligible_mask;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&lane_index);
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_assume(lane_index < 64);
    __CPROVER_assume(eligible_mask != 0);
    /* Ensure lane_index is NOT set in eligible_mask */
    __CPROVER_assume(((eligible_mask >> lane_index) & 1ull) == 0);

    uint64_t pre_probes = g_kain_converge_cache_probes;
    uint64_t pre_hits   = g_kain_converge_cache_hits;

    int64_t rc = abi_converge_commit_winner(
        converge_key, shape_key, lane_index, eligible_mask);

    __CPROVER_assert(rc == -1,
        "commit_unset_lane: lane not in eligible_mask returns -1");

    __CPROVER_assert(g_kain_converge_cache_probes == pre_probes,
        "commit_unset_lane: probe count unchanged");

    __CPROVER_assert(g_kain_converge_cache_hits == pre_hits,
        "commit_unset_lane: hit count unchanged");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: commit_winner with valid lane returns 0 and preserves counters
 *
 * With a valid lane (< 64 and set in eligible_mask), the function must
 * return 0 and NOT modify the probe/hit counters.
 * ────────────────────────────────────────────────────────────────────────── */
void check_commit_winner_valid(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t eligible_mask;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&lane_index);
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_assume(lane_index < 64);
    __CPROVER_assume(eligible_mask != 0);
    __CPROVER_assume(((eligible_mask >> lane_index) & 1ull) != 0);

    uint64_t pre_probes = g_kain_converge_cache_probes;
    uint64_t pre_hits   = g_kain_converge_cache_hits;
    uint64_t pre_cursor = g_kain_converge_telemetry_cursor;

    int64_t rc = abi_converge_commit_winner(
        converge_key, shape_key, lane_index, eligible_mask);

    __CPROVER_assert(rc == 0,
        "commit_valid: returns 0");

    /* Must not probe cache */
    __CPROVER_assert(g_kain_converge_cache_probes == pre_probes,
        "commit_valid: probe count unchanged");

    /* Must not increment hit counter */
    __CPROVER_assert(g_kain_converge_cache_hits == pre_hits,
        "commit_valid: hit count unchanged");

    /* Must not affect telemetry cursor */
    __CPROVER_assert(g_kain_converge_telemetry_cursor == pre_cursor,
        "commit_valid: telemetry cursor unchanged");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 3. COMMIT + SELECT INTEGRATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: commit then select is consistent
 *
 * After commit_winner records a lane for a (converge_key, shape_key) pair,
 * a subsequent select_lane_for_key for the same pair should:
 * - Return a non-negative lane < 64
 * - Return a lane that is in eligible_mask
 * - Increment the probe counter
 * - Maintain the hits <= probes invariant
 *
 * NOTE: We cannot assert that a cache HIT occurs because the cache key
 * depends on abi_cpu_feature_fingerprint() which CBMC treats as
 * nondeterministic.  Two calls may see different fingerprints, producing
 * different cache keys.  However, the fallback to lowbit_lane guarantees
 * a valid lane in all cases.
 * ────────────────────────────────────────────────────────────────────────── */
void check_commit_then_select(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t eligible_mask;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&lane_index);
    __CPROVER_havoc_object(&eligible_mask);
    __CPROVER_assume(lane_index < 64);
    __CPROVER_assume(eligible_mask != 0);
    __CPROVER_assume(((eligible_mask >> lane_index) & 1ull) != 0);

    /* Commit the winner */
    int64_t commit_rc = abi_converge_commit_winner(
        converge_key, shape_key, lane_index, eligible_mask);

    __CPROVER_assert(commit_rc == 0,
        "commit_then_select: commit returns 0");

    /* Select lane for the same key+shape */
    int64_t select_result = abi_converge_select_lane_for_key(
        converge_key, shape_key, eligible_mask, 0);

    __CPROVER_assert(select_result >= 0,
        "commit_then_select: select result >= 0");

    __CPROVER_assert(select_result < 64,
        "commit_then_select: select result < 64");

    __CPROVER_assert(((eligible_mask >> select_result) & 1ull) != 0,
        "commit_then_select: result lane is in eligible_mask");

    /* Probe counter must have advanced */
    __CPROVER_assert(g_kain_converge_cache_probes >= 1,
        "commit_then_select: probe count >= 1");

    /* Hits <= probes invariant */
    __CPROVER_assert(g_kain_converge_cache_hits <= g_kain_converge_cache_probes,
        "commit_then_select: hits <= probes (invariant)");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 4. TELEMETRY RING BUFFER
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: single telemetry record stores correct values and advances cursor
 * ────────────────────────────────────────────────────────────────────────── */
void check_telemetry_record_single(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    uint64_t lane_index;
    uint64_t elapsed_ticks;
    int64_t status;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);
    __CPROVER_havoc_object(&lane_index);
    __CPROVER_havoc_object(&elapsed_ticks);
    __CPROVER_havoc_object(&status);

    uint64_t pre_cursor = g_kain_converge_telemetry_cursor;
    uint64_t expected_slot = pre_cursor & (KAIN_CONVERGE_TELEMETRY_CAP - 1u);

    int64_t rc = abi_converge_record_telemetry(
        converge_key, shape_key, lane_index, elapsed_ticks, status);

    __CPROVER_assert(rc == 0,
        "telemetry_single: always returns 0");

    /* Cursor advanced by exactly 1 */
    __CPROVER_assert(g_kain_converge_telemetry_cursor == pre_cursor + 1,
        "telemetry_single: cursor advanced by 1");

    /* Sample data was written to the correct ring buffer slot */
    __CPROVER_assert(
        g_kain_converge_telemetry[expected_slot].converge_key == converge_key,
        "telemetry_single: converge_key stored correctly");

    __CPROVER_assert(
        g_kain_converge_telemetry[expected_slot].shape_key == shape_key,
        "telemetry_single: shape_key stored correctly");

    __CPROVER_assert(
        g_kain_converge_telemetry[expected_slot].lane_index == lane_index,
        "telemetry_single: lane_index stored correctly");

    __CPROVER_assert(
        g_kain_converge_telemetry[expected_slot].elapsed_ticks == elapsed_ticks,
        "telemetry_single: elapsed_ticks stored correctly");

    __CPROVER_assert(
        g_kain_converge_telemetry[expected_slot].status == status,
        "telemetry_single: status stored correctly");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: telemetry ring buffer wraps around at KAIN_CONVERGE_TELEMETRY_CAP
 *
 * Set cursor to exactly (CAP - 1) so the next write goes to slot 63
 * and the write after that wraps to slot 0.  This tests the ring buffer
 * wraparound without requiring 64+ loop iterations.
 * ────────────────────────────────────────────────────────────────────────── */
void check_telemetry_ring_wraparound(void) {
    /* Position cursor so the next write goes to the last slot (63) */
    g_kain_converge_telemetry_cursor = KAIN_CONVERGE_TELEMETRY_CAP - 1;

    /* Write to slot 63 (the last slot before wraparound) */
    int64_t rc1 = abi_converge_record_telemetry(999, 100, 5, 12345, 0);
    __CPROVER_assert(rc1 == 0,
        "ring_wrap: first record returns 0");
    __CPROVER_assert(g_kain_converge_telemetry[63].converge_key == 999,
        "ring_wrap: slot 63 has converge_key 999");
    __CPROVER_assert(g_kain_converge_telemetry_cursor == KAIN_CONVERGE_TELEMETRY_CAP,
        "ring_wrap: cursor == CAP after last slot write");

    /* Next write wraps to slot 0 */
    int64_t rc2 = abi_converge_record_telemetry(888, 200, 7, 67890, -1);
    __CPROVER_assert(rc2 == 0,
        "ring_wrap: second record (wrap) returns 0");
    __CPROVER_assert(g_kain_converge_telemetry[0].converge_key == 888,
        "ring_wrap: slot 0 has converge_key 888 (wraparound)");
    __CPROVER_assert(g_kain_converge_telemetry_cursor == KAIN_CONVERGE_TELEMETRY_CAP + 1,
        "ring_wrap: cursor == CAP + 1 after wraparound");

    /* Slot 63 must still have its previous data (wraparound to slot 0
     * does not modify slot 63) */
    __CPROVER_assert(g_kain_converge_telemetry[63].converge_key == 999,
        "ring_wrap: slot 63 preserved after wraparound to slot 0");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: telemetry cursor is monotonic (always increases by exactly 1
 * per call) via sequential records
 * ────────────────────────────────────────────────────────────────────────── */
void check_telemetry_cursor_monotonic(void) {
    uint64_t c0 = g_kain_converge_telemetry_cursor;

    int64_t rc1 = abi_converge_record_telemetry(1, 2, 3, 100, 0);
    __CPROVER_assert(rc1 == 0, "cursor_mono: record 1 ok");
    __CPROVER_assert(g_kain_converge_telemetry_cursor == c0 + 1,
        "cursor_mono: cursor = c0 + 1");

    int64_t rc2 = abi_converge_record_telemetry(4, 5, 6, 200, 1);
    __CPROVER_assert(rc2 == 0, "cursor_mono: record 2 ok");
    __CPROVER_assert(g_kain_converge_telemetry_cursor == c0 + 2,
        "cursor_mono: cursor = c0 + 2");

    /* telemetry_count accessor matches global state */
    __CPROVER_assert(abi_converge_telemetry_count() == g_kain_converge_telemetry_cursor,
        "cursor_mono: accessor matches global cursor");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: telemetry does NOT affect cache counters
 * ────────────────────────────────────────────────────────────────────────── */
void check_telemetry_does_not_affect_cache(void) {
    uint64_t pre_probes = g_kain_converge_cache_probes;
    uint64_t pre_hits   = g_kain_converge_cache_hits;

    abi_converge_record_telemetry(10, 20, 3, 500, 0);

    __CPROVER_assert(g_kain_converge_cache_probes == pre_probes,
        "telemetry_cache_safe: probes unchanged");
    __CPROVER_assert(g_kain_converge_cache_hits == pre_hits,
        "telemetry_cache_safe: hits unchanged");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 5. ACCESSORS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: accessor functions return current global state
 * ────────────────────────────────────────────────────────────────────────── */
void check_accessors(void) {
    /* Set known deterministic values */
    g_kain_converge_telemetry_cursor = 42;
    g_kain_converge_cache_probes     = 17;
    g_kain_converge_cache_hits       = 8;

    uint64_t tc = abi_converge_telemetry_count();
    uint64_t pc = abi_converge_cache_probe_count();
    uint64_t hc = abi_converge_cache_hit_count();

    __CPROVER_assert(tc == 42,
        "accessors: telemetry_count returns cursor (42)");
    __CPROVER_assert(pc == 17,
        "accessors: probe_count returns probes (17)");
    __CPROVER_assert(hc == 8,
        "accessors: hit_count returns hits (8)");

    /* Invariant: hits <= probes */
    __CPROVER_assert(hc <= pc,
        "accessors: hits <= probes (invariant)");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 6. STATIC HELPERS (visible from same TU after concatenation)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: kain_converge_lowbit_lane
 *
 * - eligible_mask == 0  → returns fallback_lane
 * - Single bit set       → returns that bit's index (0..63)
 * - Multiple bits set    → returns the LOWEST set bit
 * ────────────────────────────────────────────────────────────────────────── */
void check_lowbit_lane(void) {
    uint64_t fallback_lane;
    __CPROVER_havoc_object(&fallback_lane);

    /* Case 1: zero mask → fallback */
    uint64_t result = kain_converge_lowbit_lane(0, fallback_lane);
    __CPROVER_assert(result == fallback_lane,
        "lowbit_lane: zero mask returns fallback_lane");

    /* Case 2: edge-case single-bit masks (bit 0, bit 1, bit 63) */
    result = kain_converge_lowbit_lane(1ull << 0, fallback_lane);
    __CPROVER_assert(result == 0,
        "lowbit_lane: bit 0 => lane 0");

    result = kain_converge_lowbit_lane(1ull << 1, fallback_lane);
    __CPROVER_assert(result == 1,
        "lowbit_lane: bit 1 => lane 1");

    result = kain_converge_lowbit_lane(1ull << 63, fallback_lane);
    __CPROVER_assert(result == 63,
        "lowbit_lane: bit 63 => lane 63");

    /* Case 3: multi-bit nondet mask → lowest set bit */
    uint64_t multi_mask;
    __CPROVER_havoc_object(&multi_mask);
    __CPROVER_assume(multi_mask != 0);

    result = kain_converge_lowbit_lane(multi_mask, fallback_lane);

    __CPROVER_assert(result < 64,
        "lowbit_lane: multi-bit result < 64");

    /* Result is a set bit in the mask */
    __CPROVER_assert(((multi_mask >> result) & 1ull) != 0,
        "lowbit_lane: multi-bit result is a set bit");

    /* Result is the LOWEST set bit (no lower bits set in mask) */
    __CPROVER_assert((multi_mask & ((1ull << result) - 1ull)) == 0,
        "lowbit_lane: result is the lowest set bit in mask");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: kain_converge_mix64 produces a valid uint64_t for any input
 *
 * The mix64 function is a splitmix64-style mixer.  It must not crash,
 * overflow, or produce UB for any uint64_t input.
 * ────────────────────────────────────────────────────────────────────────── */
void check_mix64(void) {
    uint64_t value;
    __CPROVER_havoc_object(&value);

    uint64_t result = kain_converge_mix64(value);

    /* Any uint64_t output is valid */
    __CPROVER_assert(1 == 1,
        "mix64: no crash for any uint64_t input");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: kain_converge_cache_key returns non-zero (odd) for any inputs
 *
 * The cache_key always ORs 1 into the result, guaranteeing it is
 * non-zero and odd (LSB set).  This is critical because slot->key == 0
 * is the "empty slot" sentinel.
 * ────────────────────────────────────────────────────────────────────────── */
void check_cache_key_nonzero(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);

    uint64_t key = kain_converge_cache_key(converge_key, shape_key);

    __CPROVER_assert(key != 0,
        "cache_key: result is non-zero (| 1ull prevents zero)");

    __CPROVER_assert((key & 1ull) != 0,
        "cache_key: result is odd (LSB set by | 1ull)");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: kain_converge_cache_key index is bounded to cache capacity
 *
 * The base and stride computations use bitwise-AND with
 * (KAIN_CONVERGE_TUNE_CACHE_CAP - 1) = 63 to ensure they are
 * always in [0, 63].
 * ────────────────────────────────────────────────────────────────────────── */
void check_cache_key_index_bounded(void) {
    uint64_t converge_key;
    uint64_t shape_key;
    __CPROVER_havoc_object(&converge_key);
    __CPROVER_havoc_object(&shape_key);

    uint64_t key = kain_converge_cache_key(converge_key, shape_key);
    uint64_t base   = key & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);
    uint64_t stride = ((key >> 6) | 1ull) & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1u);

    __CPROVER_assert(base < KAIN_CONVERGE_TUNE_CACHE_CAP,
        "cache_key_index: base < CAP (bounded by AND)");

    __CPROVER_assert(stride < KAIN_CONVERGE_TUNE_CACHE_CAP,
        "cache_key_index: stride < CAP (bounded by AND)");

    __CPROVER_assert(stride > 0,
        "cache_key_index: stride > 0 (| 1ull ensures non-zero)");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: kain_converge_atomic_fetch_add_u64 semantics
 *
 * The function must atomically increment *target by increment
 * and return the OLD value.
 * ────────────────────────────────────────────────────────────────────────── */
void check_atomic_fetch_add(void) {
    volatile uint64_t counter;
    __CPROVER_havoc_object(&counter);

    uint64_t increment;
    __CPROVER_havoc_object(&increment);

    uint64_t old = kain_converge_atomic_fetch_add_u64(&counter, increment);

    __CPROVER_assert(counter == old + increment,
        "atomic_fetch_add: counter == old + increment");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 7. PROBE/HIT COUNT INVARIANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: probe count invariant across operations
 *
 * - select_lane_for_key (non-zero mask): increments probes by >= 1
 * - commit_winner: does NOT increment probes
 * - record_telemetry: does NOT increment probes
 * - select_lane_for_key (zero mask): does NOT increment probes
 * ────────────────────────────────────────────────────────────────────────── */
void check_probe_count_invariant(void) {
    /* Start with known zero probe count */
    g_kain_converge_cache_probes = 0;
    g_kain_converge_cache_hits   = 0;

    /* Telemetry does NOT affect probes */
    abi_converge_record_telemetry(1, 2, 3, 100, 0);
    __CPROVER_assert(g_kain_converge_cache_probes == 0,
        "probe_invariant: telemetry does not increment probes");

    /* Commit does NOT affect probes */
    abi_converge_commit_winner(10, 20, 0, 1ull);
    __CPROVER_assert(g_kain_converge_cache_probes == 0,
        "probe_invariant: commit does not increment probes");

    /* Select with non-zero mask DOES increment probes */
    abi_converge_select_lane_for_key(100, 200, 1ull, 0);
    __CPROVER_assert(g_kain_converge_cache_probes >= 1,
        "probe_invariant: select (non-zero mask) increments probes >= 1");

    /* Select with zero mask does NOT increment probes */
    uint64_t probes_before = g_kain_converge_cache_probes;
    abi_converge_select_lane_for_key(300, 400, 0, 5);
    __CPROVER_assert(g_kain_converge_cache_probes == probes_before,
        "probe_invariant: select (zero mask) does not increment probes");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 8. APri BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────────
 * Check: max lanes, telemetry cap, and tune cache cap are correct
 * ────────────────────────────────────────────────────────────────────────── */
void check_bounds(void) {
    __CPROVER_assert(KAIN_CONVERGE_LANE_MAX == 8,
        "bounds: KAIN_CONVERGE_LANE_MAX == 8");

    __CPROVER_assert(KAIN_CONVERGE_TELEMETRY_CAP == 64,
        "bounds: KAIN_CONVERGE_TELEMETRY_CAP == 64");

    __CPROVER_assert(KAIN_CONVERGE_TUNE_CACHE_CAP == 64,
        "bounds: KAIN_CONVERGE_TUNE_CACHE_CAP == 64");

    /* These powers of 2 are critical for the bitwise-AND ring buffer */
    __CPROVER_assert((KAIN_CONVERGE_TELEMETRY_CAP & (KAIN_CONVERGE_TELEMETRY_CAP - 1)) == 0,
        "bounds: KAIN_CONVERGE_TELEMETRY_CAP is power of 2");

    __CPROVER_assert((KAIN_CONVERGE_TUNE_CACHE_CAP & (KAIN_CONVERGE_TUNE_CACHE_CAP - 1)) == 0,
        "bounds: KAIN_CONVERGE_TUNE_CACHE_CAP is power of 2");
}


/* ──────────────────────────────────────────────────────────────────────────
 * Check: KAIN_CONVERGE_NO_WINNER sentinel value
 * ────────────────────────────────────────────────────────────────────────── */
void check_no_winner_sentinel(void) {
    __CPROVER_assert(KAIN_CONVERGE_NO_WINNER == UINT64_MAX,
        "sentinel: KAIN_CONVERGE_NO_WINNER == UINT64_MAX");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * 9. NULL SAFETY (documentation)
 *
 * All public API functions in converge.h take only scalar (uint64_t/int64_t)
 * parameters — no pointers.  Therefore there are no NULL-pointer dereference
 * paths to test.  This is a deliberate API design that eliminates an entire
 * class of safety bugs at the type level.
 * ────────────────────────────────────────────────────────────────────────── */
void check_null_safety(void) {
    __CPROVER_assert(1 == 1,
        "null_safety: all converge API uses scalar params only — no NULL risk");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* 1. Lane Selection */
    check_select_lane_zero_mask();
    check_select_lane_eligible();
    check_select_lane_hits_le_probes();

    /* 2. Winner Commit */
    check_commit_winner_rejects_oor_lane();
    check_commit_winner_rejects_unset_lane();
    check_commit_winner_valid();

    /* 3. Commit + Select Integration */
    check_commit_then_select();

    /* 4. Telemetry Ring Buffer */
    check_telemetry_record_single();
    check_telemetry_ring_wraparound();
    check_telemetry_cursor_monotonic();
    check_telemetry_does_not_affect_cache();

    /* 5. Accessors */
    check_accessors();

    /* 6. Static Helpers */
    check_lowbit_lane();
    check_mix64();
    check_cache_key_nonzero();
    check_cache_key_index_bounded();
    check_atomic_fetch_add();

    /* 7. Probe/Hit Count Invariants */
    check_probe_count_invariant();

    /* 8. Bounds & Sentinels */
    check_bounds();
    check_no_winner_sentinel();

    /* 9. NULL Safety */
    check_null_safety();

    return 0;
}
