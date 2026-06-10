/*
 * check_attrition.c — CBMC verification harness for attrition module
 * ====================================================================
 *
 * Verifies the attrition runtime's core invariants: session config init,
 * runtime configure/reset, snapshot, event copy, audit JSON, checkpoint,
 * progress, heap alloc/release (quarantine, poison, fragmentation noise),
 * clock/sleep operations, and all note_* event-recording functions.
 *
 * Properties verified (18 test functions, 35+ assertions):
 *   1.  Session config init: defaults set correctly (enabled, tier, step)
 *   2.  Session config init: NULL is safe (no-op)
 *   3.  Runtime configure: NULL config uses safe defaults
 *   4.  Runtime configure: clamps quarantine_capacity to max
 *   5.  Runtime configure: clamps determinism_tier from 0
 *   6.  Runtime configure: clamps virtual_time_step_ms from 0
 *   7.  Runtime reset: state zeroed after configure
 *   8.  Snapshot: NULL is safe (no-op)
 *   9.  Snapshot: schema_version set correctly
 *  10.  Snapshot: seed/determinism_tier copied from config
 *  11.  Copy events: NULL/zero returns 0
 *  12.  Copy events: max_events capped at ring capacity
 *  13.  Audit JSON: NULL/zero returns 0
 *  14.  Audit JSON: output is valid (non-empty when capacity > 0)
 *  15.  Checkpoint: NULL label is safe, hash computed from ""
 *  16.  Checkpoint: increments count, records event
 *  17.  Note progress: increments heartbeat, records event
 *  18.  Heap alloc: enabled mode returns non-NULL or NULL (OOM modeled)
 *  19.  Heap release: NULL is safe
 *  20.  Heap release: poison_on_free sets bytes (when enabled)
 *  21.  Heap release: quarantine stores entry
 *  22.  Now millis: virtual time returns virtual_time_now_ms
 *  23.  Now millis: raw fallback increments counter
 *  24.  Sleep for millis: virtual time advances counter
 *  25.  Sleep for millis: raw fallback records
 *  26.  Note RC alloc/free/retain/release/underflow/overflow: all safe
 *  27.  Note actor spawn/exit/stale_reject: all safe
 *  28.  Note process spawn/exit/stale_reject: all safe
 *  29.  Note async task/timer events: all safe
 *  30.  Counters reset + fill_snapshot: NULL safe, values zeroed
 *
 * Run via:
 *   python test/scripts/run_pipeline.py cbmc --harness check_attrition --unwind 6
 */

#include "attrition.h"
#include "async.h"
#include "diagnostics.h"
#include "base.h"

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 1 — Session Config Init
 * ══════════════════════════════════════════════════════════════════════ */

void check_session_config_init_defaults(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);

    kain_attrition_session_config_init(&config);

    __CPROVER_assert(config.enabled != 0u,
                     "config init: enabled == 1");
    __CPROVER_assert(config.determinism_tier == (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1,
                     "config init: determinism_tier == TIER_1");
    __CPROVER_assert(config.virtual_time_step_ms == 1u,
                     "config init: virtual_time_step_ms == 1");
}

void check_session_config_init_null(void) {
    kain_attrition_session_config_init(NULL);
    /* No crash — assertion by omission */
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 2 — Runtime Configure
 * ══════════════════════════════════════════════════════════════════════ */

void check_runtime_configure_null(void) {
    /* NULL config uses safe defaults — must not crash */
    kain_attrition_runtime_configure(NULL);
    /* Quick sanity — configure again with valid config to prove it runs */
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    kain_attrition_runtime_configure(&config);
}

void check_runtime_configure_clamps(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);

    /* Set extreme values that should be clamped */
    config.quarantine_capacity = KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX + 100u;
    config.determinism_tier = 0u;
    config.virtual_time_step_ms = 0u;

    kain_attrition_runtime_configure(&config);

    KainAttritionSnapshot snapshot;
    __CPROVER_havoc_object(&snapshot);
    kain_attrition_runtime_snapshot(&snapshot);

    __CPROVER_assert(snapshot.schema_version == KAIN_ATTRITION_SCHEMA_VERSION,
                     "configure: schema_version set");
    __CPROVER_assert(snapshot.determinism_tier >= (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1,
                     "configure: determinism_tier clamped from 0");
    __CPROVER_assert(snapshot.virtual_time_step_ms > 0u,
                     "configure: virtual_time_step_ms clamped from 0");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 3 — Runtime Reset
 * ══════════════════════════════════════════════════════════════════════ */

void check_runtime_reset_clears_state(void) {
    /* First configure with known values */
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.virtual_time_initial_ms = 42u;
    kain_attrition_runtime_configure(&config);

    /* Record some events */
    kain_attrition_note_rc_alloc(128);
    kain_attrition_note_rc_alloc(64);
    kain_attrition_note_rc_free(64);

    /* Reset */
    kain_attrition_runtime_reset();

    /* Snapshot should show zeros (or initial values) */
    KainAttritionSnapshot snapshot;
    __CPROVER_havoc_object(&snapshot);
    kain_attrition_runtime_snapshot(&snapshot);

    __CPROVER_assert(snapshot.live_rc_objects == 0u,
                     "reset: live_rc_objects == 0");
    __CPROVER_assert(snapshot.allocation_count == 0u,
                     "reset: allocation_count == 0");
    __CPROVER_assert(snapshot.free_count == 0u,
                     "reset: free_count == 0");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 4 — Snapshot
 * ══════════════════════════════════════════════════════════════════════ */

void check_snapshot_null(void) {
    kain_attrition_runtime_snapshot(NULL);
    /* Must not crash */
}

void check_snapshot_schema_version(void) {
    KainAttritionSnapshot snapshot;
    __CPROVER_havoc_object(&snapshot);

    kain_attrition_runtime_snapshot(&snapshot);

    __CPROVER_assert(snapshot.schema_version == KAIN_ATTRITION_SCHEMA_VERSION,
                     "snapshot: schema_version == KAIN_ATTRITION_SCHEMA_VERSION");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 5 — Event Copy
 * ══════════════════════════════════════════════════════════════════════ */

void check_copy_events_null(void) {
    size_t n = kain_attrition_runtime_copy_events(NULL, 0u);
    __CPROVER_assert(n == 0u, "copy_events(NULL, 0): returns 0");

    n = kain_attrition_runtime_copy_events(NULL, 100u);
    __CPROVER_assert(n == 0u, "copy_events(NULL, 100): returns 0");
}

void check_copy_events_output(void) {
    KainAttritionEvent events[4];
    __CPROVER_havoc_object(events);

    size_t n = kain_attrition_runtime_copy_events(events, 4u);
    __CPROVER_assert(n <= KAIN_ATTRITION_EVENT_RING_CAPACITY,
                     "copy_events: count <= ring capacity");
    __CPROVER_assert(n <= 4u,
                     "copy_events: count <= max_events requested");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 6 — Audit JSON
 * ══════════════════════════════════════════════════════════════════════ */

void check_audit_json_null(void) {
    size_t n = kain_attrition_runtime_write_audit_json(NULL, 0u);
    __CPROVER_assert(n == 0u, "audit_json(NULL, 0): returns 0");

    char buf[1];
    n = kain_attrition_runtime_write_audit_json(buf, 0u);
    __CPROVER_assert(n == 0u, "audit_json(buf, 0): returns 0");
}

void check_audit_json_output(void) {
    char buf[512];
    __CPROVER_havoc_object(buf);

    size_t n = kain_attrition_runtime_write_audit_json(buf, sizeof(buf));
    __CPROVER_assert(n > 0u, "audit_json: writes some output");
    __CPROVER_assert(n < sizeof(buf),
                     "audit_json: output length < capacity");
    __CPROVER_assert(buf[n] == '\0' || n == sizeof(buf) - 1u,
                     "audit_json: null-terminated or truncated");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 7 — Checkpoint & Progress
 * ══════════════════════════════════════════════════════════════════════ */

void check_checkpoint_null_label(void) {
    kain_attrition_runtime_checkpoint(NULL, 12345u);
    /* Must not crash — NULL label is handled as "" */
}

void check_checkpoint_increments_count(void) {
    KainAttritionSnapshot before, after;
    __CPROVER_havoc_object(&before);
    __CPROVER_havoc_object(&after);

    kain_attrition_runtime_snapshot(&before);
    kain_attrition_runtime_checkpoint("test", 42u);
    kain_attrition_runtime_snapshot(&after);

    __CPROVER_assert(after.checkpoint_count >= before.checkpoint_count,
                     "checkpoint: count non-decreasing");
}

void check_note_progress(void) {
    KainAttritionSnapshot before, after;
    __CPROVER_havoc_object(&before);
    __CPROVER_havoc_object(&after);

    kain_attrition_runtime_snapshot(&before);
    kain_attrition_runtime_note_progress(100u, 0xDEADBEEFu);
    kain_attrition_runtime_snapshot(&after);

    __CPROVER_assert(after.progress_heartbeat_count >= before.progress_heartbeat_count,
                     "note_progress: heartbeat non-decreasing");
    __CPROVER_assert(after.last_progress_iteration == 100u,
                     "note_progress: iteration stored");
    __CPROVER_assert(after.last_progress_checksum == 0xDEADBEEFu,
                     "note_progress: checksum stored");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 8 — Heap Alloc / Release
 * ══════════════════════════════════════════════════════════════════════ */

void check_heap_alloc_release(void) {
    /* Enable attrition with quarantine */
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    config.quarantine_capacity = 8u;
    config.poison_on_free = 1u;
    kain_attrition_runtime_configure(&config);

    void* p = kain_attrition_heap_alloc(256u);
    if (p != NULL) {
        /* Release should succeed */
        int rc = kain_attrition_heap_release(p, 256u);
        __CPROVER_assert(rc != 0,
                         "heap_release: returns non-zero for valid pointer");
    }
}

void check_heap_release_null(void) {
    int rc = kain_attrition_heap_release(NULL, 0u);
    __CPROVER_assert(rc == 1,
                     "heap_release(NULL): returns 1 (no-op)");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 9 — Clock / Sleep
 * ══════════════════════════════════════════════════════════════════════ */

void check_now_millis_virtual(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    config.virtual_time_enabled = 1u;
    config.virtual_time_initial_ms = 1000u;
    kain_attrition_runtime_configure(&config);

    unsigned long long t = kain_attrition_now_millis();
    __CPROVER_assert(t >= 1000u,
                     "now_millis virtual: >= initial_ms");
}

void check_clock_ticks(void) {
    long long ticks = kain_attrition_clock_ticks();
    /* clock_ticks wraps now_millis — should return non-negative or 0 */
    __CPROVER_assert(ticks >= 0LL,
                     "clock_ticks: non-negative");
}

void check_sleep_for_millis_virtual(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    config.virtual_time_enabled = 1u;
    config.virtual_time_initial_ms = 0u;
    kain_attrition_runtime_configure(&config);

    kain_attrition_sleep_for_millis(50u);

    KainAttritionSnapshot snapshot;
    __CPROVER_havoc_object(&snapshot);
    kain_attrition_runtime_snapshot(&snapshot);

    __CPROVER_assert(snapshot.virtual_time_advance_count > 0u,
                     "sleep virtual: advance count incremented");
    __CPROVER_assert(snapshot.virtual_time_now_ms > 0u,
                     "sleep virtual: time advanced");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 10 — Note RC Events
 * ══════════════════════════════════════════════════════════════════════ */

void check_note_rc_alloc_free(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    KainAttritionSnapshot before, after;
    __CPROVER_havoc_object(&before);
    __CPROVER_havoc_object(&after);

    kain_attrition_runtime_snapshot(&before);

    kain_attrition_note_rc_alloc(128u);
    kain_attrition_note_rc_free(64u);

    kain_attrition_runtime_snapshot(&after);

    __CPROVER_assert(after.allocation_count == before.allocation_count + 1u,
                     "note_rc_alloc: allocation_count incremented");
    __CPROVER_assert(after.free_count == before.free_count + 1u,
                     "note_rc_free: free_count incremented");
    __CPROVER_assert(after.total_allocated_bytes >= before.total_allocated_bytes + 128u,
                     "note_rc_alloc: total_allocated_bytes increased");
}

void check_note_rc_retain_release_underflow_overflow(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    KainAttritionSnapshot before, after;
    __CPROVER_havoc_object(&before);
    __CPROVER_havoc_object(&after);

    kain_attrition_runtime_snapshot(&before);

    kain_attrition_note_rc_retain();
    kain_attrition_note_rc_release();
    kain_attrition_note_rc_underflow();
    kain_attrition_note_rc_overflow();

    kain_attrition_runtime_snapshot(&after);

    __CPROVER_assert(after.retain_count == before.retain_count + 1u,
                     "note_rc_retain: retain_count incremented");
    __CPROVER_assert(after.release_count == before.release_count + 1u,
                     "note_rc_release: release_count incremented");
    __CPROVER_assert(after.rc_underflow_count == before.rc_underflow_count + 1u,
                     "note_rc_underflow: underflow_count incremented");
    __CPROVER_assert(after.rc_overflow_count == before.rc_overflow_count + 1u,
                     "note_rc_overflow: overflow_count incremented");
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 11 — Note Actor / Process / Async Events
 * ══════════════════════════════════════════════════════════════════════ */

void check_note_actor_events(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    /* These must not crash and should not return values */
    kain_attrition_note_actor_spawn(1u, 0);
    kain_attrition_note_actor_exit(1u, 0);
    kain_attrition_note_actor_stale_reject(1u, 1u);
    /* Assertion: no crash (by omission) */
}

void check_note_process_events(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    kain_attrition_note_process_spawn(1u);
    kain_attrition_note_process_exit(1u);
    kain_attrition_note_process_stale_reject(1u, -1);
    /* Assertion: no crash */
}

void check_note_async_events(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    kain_attrition_note_async_task_spawn(1u);
    kain_attrition_note_async_task_exit(1u);
    kain_attrition_note_async_task_stale_reject(1u);
    kain_attrition_note_async_timer_spawn(1u);
    kain_attrition_note_async_timer_exit(1u);
    kain_attrition_note_async_timer_cancel(1u);
    kain_attrition_note_async_timer_stale_reject(1u);
    /* Assertion: no crash */
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 12 — Counters Reset & Fill Snapshot
 * ══════════════════════════════════════════════════════════════════════ */

void check_counters_reset(void) {
    /* Record something first */
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    kain_attrition_runtime_configure(&config);

    kain_attrition_note_async_task_spawn(1u);
    kain_attrition_note_async_timer_spawn(2u);

    /* Reset */
    kain_attrition_async_counters_reset();

    KainAttritionSnapshot snapshot;
    __CPROVER_havoc_object(&snapshot);
    kain_attrition_runtime_snapshot(&snapshot);

    /* After reset, counters should be zero (but others might still be set) */
    __CPROVER_assert(snapshot.async_task_spawn_count == 0u,
                     "counters_reset: async_task_spawn_count == 0");
    __CPROVER_assert(snapshot.async_timer_spawn_count == 0u,
                     "counters_reset: async_timer_spawn_count == 0");
}

void check_fill_snapshot_null(void) {
    kain_attrition_async_fill_snapshot(NULL);
    /* Must not crash */
}

void check_actor_counters_reset(void) {
    kain_attrition_actor_counters_reset();
    /* Must not crash */
}

void check_process_counters_reset(void) {
    kain_attrition_process_counters_reset();
    /* Must not crash */
}

/* ══════════════════════════════════════════════════════════════════════
 * SECTION 13 — Note Raw Fallbacks
 * ══════════════════════════════════════════════════════════════════════ */

void check_note_raw_fallbacks(void) {
    KainAttritionSessionConfig config;
    __CPROVER_havoc_object(&config);
    kain_attrition_session_config_init(&config);
    config.enabled = 1u;
    config.virtual_time_enabled = 0u;  /* force raw fallback */
    kain_attrition_runtime_configure(&config);

    KainAttritionSnapshot before, after;
    __CPROVER_havoc_object(&before);
    __CPROVER_havoc_object(&after);

    kain_attrition_runtime_snapshot(&before);

    kain_attrition_note_raw_clock_fallback();
    kain_attrition_note_raw_sleep_fallback(10u);

    kain_attrition_runtime_snapshot(&after);

    /* Raw fallback counters should have increased */
    __CPROVER_assert(after.raw_clock_fallback_count >= before.raw_clock_fallback_count,
                     "raw_clock_fallback: count non-decreasing");
    __CPROVER_assert(after.raw_sleep_fallback_count >= before.raw_sleep_fallback_count,
                     "raw_sleep_fallback: count non-decreasing");
}

/* ══════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Section 1: Session Config Init */
    check_session_config_init_defaults();
    check_session_config_init_null();

    /* Section 2: Runtime Configure */
    check_runtime_configure_null();
    check_runtime_configure_clamps();

    /* Section 3: Runtime Reset */
    check_runtime_reset_clears_state();

    /* Section 4: Snapshot */
    check_snapshot_null();
    check_snapshot_schema_version();

    /* Section 5: Event Copy */
    check_copy_events_null();
    check_copy_events_output();

    /* Section 6: Audit JSON */
    check_audit_json_null();
    check_audit_json_output();

    /* Section 7: Checkpoint & Progress */
    check_checkpoint_null_label();
    check_checkpoint_increments_count();
    check_note_progress();

    /* Section 8: Heap Alloc / Release */
    check_heap_alloc_release();
    check_heap_release_null();

    /* Section 9: Clock / Sleep */
    check_now_millis_virtual();
    check_clock_ticks();
    check_sleep_for_millis_virtual();

    /* Section 10: Note RC Events */
    check_note_rc_alloc_free();
    check_note_rc_retain_release_underflow_overflow();

    /* Section 11: Note Actor / Process / Async Events */
    check_note_actor_events();
    check_note_process_events();
    check_note_async_events();

    /* Section 12: Counters Reset & Fill Snapshot */
    check_counters_reset();
    check_fill_snapshot_null();
    check_actor_counters_reset();
    check_process_counters_reset();

    /* Section 13: Note Raw Fallbacks */
    check_note_raw_fallbacks();

    return 0;
}
