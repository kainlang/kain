#include "../../include/attrition.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct KainAttritionQuarantineEntry {
    void* ptr;
    size_t bytes;
} KainAttritionQuarantineEntry;

typedef struct KainAttritionState {
    KainAttritionSessionConfig config;
    int initialized;

#ifdef _WIN32
    CRITICAL_SECTION lock;
    INIT_ONCE init_once;
#else
    pthread_mutex_t lock;
    pthread_once_t init_once;
#endif

    uint64_t event_write_count;
    size_t event_next_index;
    KainAttritionEvent events[KAIN_ATTRITION_EVENT_RING_CAPACITY];

    uint64_t live_rc_objects;
    uint64_t peak_live_rc_objects;
    uint64_t live_runtime_bytes;
    uint64_t peak_runtime_bytes;
    uint64_t allocation_count;
    uint64_t free_count;
    uint64_t total_allocated_bytes;
    uint64_t total_freed_bytes;
    uint64_t allocation_fail_count;
    uint64_t retain_count;
    uint64_t release_count;
    uint64_t rc_underflow_count;
    uint64_t rc_overflow_count;
    uint64_t poison_free_count;
    uint64_t quarantine_live_bytes;
    uint64_t quarantine_peak_entries;
    uint64_t quarantine_peak_bytes;
    uint64_t fragmentation_noise_live_bytes;
    uint64_t fragmentation_noise_peak_bytes;
    uint64_t fragmentation_noise_total_bytes;
    uint64_t fragmentation_injection_count;

    uint64_t checkpoint_count;
    uint64_t last_checkpoint_label_hash;
    uint64_t last_checkpoint_subject_id;
    uint64_t progress_heartbeat_count;
    uint64_t last_progress_iteration;
    uint64_t last_progress_checksum;

    uint64_t virtual_time_now_ms;
    uint64_t virtual_time_advance_count;
    uint64_t virtual_time_advance_total_ms;
    uint64_t raw_clock_fallback_count;
    uint64_t raw_sleep_fallback_count;
    uint64_t raw_sleep_fallback_millis_total;

    KainAttritionQuarantineEntry quarantine[KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX];
    size_t quarantine_head;
    size_t quarantine_count;

    KainAttritionQuarantineEntry fragmentation_ring[KAIN_ATTRITION_FRAGMENTATION_RING_CAPACITY];
    size_t fragmentation_head;
    size_t fragmentation_count;
} KainAttritionState;

static KainAttritionState g_kain_attrition_state = {
#ifdef _WIN32
    .init_once = INIT_ONCE_STATIC_INIT
#else
    .init_once = PTHREAD_ONCE_INIT
#endif
};

static void kain_attrition_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_kain_attrition_state.lock);
#else
    pthread_mutex_lock(&g_kain_attrition_state.lock);
#endif
}

static void kain_attrition_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_kain_attrition_state.lock);
#else
    pthread_mutex_unlock(&g_kain_attrition_state.lock);
#endif
}

static uint64_t kain_attrition_u64_saturating_add(uint64_t left, uint64_t right) {
    /*
     * Proof:
     * - runtime/native/src/core/z3/proofs-experimental/attrition-saturating-u64-add-monotonic.smt2
     *
     * The telemetry accumulators clamp to UINT64_MAX instead of wrapping
     * backward, and they stay identical to plain unsigned addition whenever
     * the add does not overflow.
     */
    uint64_t sum = left + right;
    if (sum < left) {
        return UINT64_MAX;
    }
    return sum;
}

static void kain_attrition_update_peak_u64(uint64_t* peak, uint64_t candidate) {
    if (peak != NULL && candidate > *peak) {
        *peak = candidate;
    }
}

static void kain_attrition_state_reset_locked(void) {
    size_t index;
    for (index = 0u; index < KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX; ++index) {
        if (g_kain_attrition_state.quarantine[index].ptr != NULL) {
            free(g_kain_attrition_state.quarantine[index].ptr);
            g_kain_attrition_state.quarantine[index].ptr = NULL;
            g_kain_attrition_state.quarantine[index].bytes = 0u;
        }
    }
    for (index = 0u; index < KAIN_ATTRITION_FRAGMENTATION_RING_CAPACITY; ++index) {
        if (g_kain_attrition_state.fragmentation_ring[index].ptr != NULL) {
            free(g_kain_attrition_state.fragmentation_ring[index].ptr);
            g_kain_attrition_state.fragmentation_ring[index].ptr = NULL;
            g_kain_attrition_state.fragmentation_ring[index].bytes = 0u;
        }
    }
    memset(g_kain_attrition_state.events, 0, sizeof(g_kain_attrition_state.events));
    g_kain_attrition_state.event_write_count = 0u;
    g_kain_attrition_state.event_next_index = 0u;
    g_kain_attrition_state.live_rc_objects = 0u;
    g_kain_attrition_state.peak_live_rc_objects = 0u;
    g_kain_attrition_state.live_runtime_bytes = 0u;
    g_kain_attrition_state.peak_runtime_bytes = 0u;
    g_kain_attrition_state.allocation_count = 0u;
    g_kain_attrition_state.free_count = 0u;
    g_kain_attrition_state.total_allocated_bytes = 0u;
    g_kain_attrition_state.total_freed_bytes = 0u;
    g_kain_attrition_state.allocation_fail_count = 0u;
    g_kain_attrition_state.retain_count = 0u;
    g_kain_attrition_state.release_count = 0u;
    g_kain_attrition_state.rc_underflow_count = 0u;
    g_kain_attrition_state.rc_overflow_count = 0u;
    g_kain_attrition_state.poison_free_count = 0u;
    g_kain_attrition_state.quarantine_live_bytes = 0u;
    g_kain_attrition_state.quarantine_peak_entries = 0u;
    g_kain_attrition_state.quarantine_peak_bytes = 0u;
    g_kain_attrition_state.fragmentation_noise_live_bytes = 0u;
    g_kain_attrition_state.fragmentation_noise_peak_bytes = 0u;
    g_kain_attrition_state.fragmentation_noise_total_bytes = 0u;
    g_kain_attrition_state.fragmentation_injection_count = 0u;
    g_kain_attrition_state.checkpoint_count = 0u;
    g_kain_attrition_state.last_checkpoint_label_hash = 0u;
    g_kain_attrition_state.last_checkpoint_subject_id = 0u;
    g_kain_attrition_state.progress_heartbeat_count = 0u;
    g_kain_attrition_state.last_progress_iteration = 0u;
    g_kain_attrition_state.last_progress_checksum = 0u;
    g_kain_attrition_state.quarantine_head = 0u;
    g_kain_attrition_state.quarantine_count = 0u;
    g_kain_attrition_state.fragmentation_head = 0u;
    g_kain_attrition_state.fragmentation_count = 0u;
    g_kain_attrition_state.virtual_time_now_ms = g_kain_attrition_state.config.virtual_time_initial_ms;
    g_kain_attrition_state.virtual_time_advance_count = 0u;
    g_kain_attrition_state.virtual_time_advance_total_ms = 0u;
    g_kain_attrition_state.raw_clock_fallback_count = 0u;
    g_kain_attrition_state.raw_sleep_fallback_count = 0u;
    g_kain_attrition_state.raw_sleep_fallback_millis_total = 0u;
}

static void kain_attrition_record_event_locked(uint32_t kind, uint32_t aux, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    KainAttritionEvent* event = &g_kain_attrition_state.events[g_kain_attrition_state.event_next_index];
    event->event_index = g_kain_attrition_state.event_write_count + 1u;
    event->kind = kind;
    event->aux = aux;
    event->arg0 = arg0;
    event->arg1 = arg1;
    event->arg2 = arg2;
    g_kain_attrition_state.event_next_index =
        (g_kain_attrition_state.event_next_index + 1u) % KAIN_ATTRITION_EVENT_RING_CAPACITY;
    g_kain_attrition_state.event_write_count += 1u;
}

static void kain_attrition_fragmentation_noise_locked(void) {
    uint64_t max_bytes = g_kain_attrition_state.config.fragmentation_noise_max_bytes;
    void* noise_block;
    size_t slot;
    size_t noise_bytes;
    if (max_bytes == 0u) {
        return;
    }
    noise_bytes = (size_t)(1u + ((g_kain_attrition_state.allocation_count ^ g_kain_attrition_state.config.seed) % max_bytes));
    noise_block = malloc(noise_bytes);
    if (noise_block == NULL) {
        return;
    }
    memset(noise_block, 0x3Cu, noise_bytes);
    slot = g_kain_attrition_state.fragmentation_head;
    if (g_kain_attrition_state.fragmentation_ring[slot].ptr != NULL) {
        if (g_kain_attrition_state.fragmentation_noise_live_bytes >=
            g_kain_attrition_state.fragmentation_ring[slot].bytes) {
            g_kain_attrition_state.fragmentation_noise_live_bytes -=
                g_kain_attrition_state.fragmentation_ring[slot].bytes;
        } else {
            g_kain_attrition_state.fragmentation_noise_live_bytes = 0u;
        }
        free(g_kain_attrition_state.fragmentation_ring[slot].ptr);
    } else if (g_kain_attrition_state.fragmentation_count < KAIN_ATTRITION_FRAGMENTATION_RING_CAPACITY) {
        g_kain_attrition_state.fragmentation_count += 1u;
    }
    g_kain_attrition_state.fragmentation_ring[slot].ptr = noise_block;
    g_kain_attrition_state.fragmentation_ring[slot].bytes = noise_bytes;
    g_kain_attrition_state.fragmentation_injection_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.fragmentation_injection_count, 1u);
    g_kain_attrition_state.fragmentation_noise_live_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.fragmentation_noise_live_bytes, (uint64_t)noise_bytes);
    g_kain_attrition_state.fragmentation_noise_total_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.fragmentation_noise_total_bytes, (uint64_t)noise_bytes);
    kain_attrition_update_peak_u64(
        &g_kain_attrition_state.fragmentation_noise_peak_bytes,
        g_kain_attrition_state.fragmentation_noise_live_bytes);
    g_kain_attrition_state.fragmentation_head =
        (g_kain_attrition_state.fragmentation_head + 1u) % KAIN_ATTRITION_FRAGMENTATION_RING_CAPACITY;
}

#ifdef _WIN32
static BOOL CALLBACK kain_attrition_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
    (void)init_once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_kain_attrition_state.lock);
    g_kain_attrition_state.initialized = 1;
    return TRUE;
}
#else
static void kain_attrition_init_once(void) {
    pthread_mutex_init(&g_kain_attrition_state.lock, NULL);
    g_kain_attrition_state.initialized = 1;
}
#endif

static void kain_attrition_ensure_initialized(void) {
#ifdef _WIN32
    InitOnceExecuteOnce(&g_kain_attrition_state.init_once, kain_attrition_init_once, NULL, NULL);
#else
    pthread_once(&g_kain_attrition_state.init_once, kain_attrition_init_once);
#endif
}

void kain_attrition_session_config_init(KainAttritionSessionConfig* config) {
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->enabled = 1u;
    config->determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    config->virtual_time_step_ms = 1u;
}

void kain_attrition_runtime_reset(void) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_state_reset_locked();
    kain_attrition_unlock();
    kain_attrition_actor_counters_reset();
    kain_attrition_process_counters_reset();
    kain_attrition_async_counters_reset();
}

void kain_attrition_runtime_configure(const KainAttritionSessionConfig* config) {
    KainAttritionSessionConfig local_config;
    kain_attrition_ensure_initialized();
    kain_attrition_session_config_init(&local_config);
    if (config != NULL) {
        local_config = *config;
    }
    if (local_config.quarantine_capacity > KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX) {
        local_config.quarantine_capacity = KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX;
    }
    if (local_config.determinism_tier == 0u) {
        local_config.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    }
    if (local_config.virtual_time_step_ms == 0u) {
        local_config.virtual_time_step_ms = 1u;
    }
    kain_attrition_lock();
    g_kain_attrition_state.config = local_config;
    g_kain_attrition_state.virtual_time_now_ms = local_config.virtual_time_initial_ms;
    kain_attrition_unlock();
}

static uint64_t kain_attrition_popcount_u64(uint64_t value) {
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) + ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (value * UINT64_C(0x0101010101010101)) >> 56u;
}

void kain_attrition_runtime_snapshot(KainAttritionSnapshot* out_snapshot) {
    if (out_snapshot == NULL) {
        return;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->schema_version = KAIN_ATTRITION_SCHEMA_VERSION;
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    out_snapshot->seed = g_kain_attrition_state.config.seed;
    out_snapshot->determinism_tier = g_kain_attrition_state.config.determinism_tier;
    out_snapshot->live_rc_objects = g_kain_attrition_state.live_rc_objects;
    out_snapshot->peak_live_rc_objects = g_kain_attrition_state.peak_live_rc_objects;
    out_snapshot->live_runtime_bytes = g_kain_attrition_state.live_runtime_bytes;
    out_snapshot->peak_runtime_bytes = g_kain_attrition_state.peak_runtime_bytes;
    out_snapshot->allocation_count = g_kain_attrition_state.allocation_count;
    out_snapshot->free_count = g_kain_attrition_state.free_count;
    out_snapshot->total_allocated_bytes = g_kain_attrition_state.total_allocated_bytes;
    out_snapshot->total_freed_bytes = g_kain_attrition_state.total_freed_bytes;
    out_snapshot->allocation_fail_count = g_kain_attrition_state.allocation_fail_count;
    out_snapshot->retain_count = g_kain_attrition_state.retain_count;
    out_snapshot->release_count = g_kain_attrition_state.release_count;
    out_snapshot->rc_underflow_count = g_kain_attrition_state.rc_underflow_count;
    out_snapshot->rc_overflow_count = g_kain_attrition_state.rc_overflow_count;
    out_snapshot->poison_free_count = g_kain_attrition_state.poison_free_count;
    out_snapshot->quarantine_live_entries = g_kain_attrition_state.quarantine_count;
    out_snapshot->quarantine_live_bytes = g_kain_attrition_state.quarantine_live_bytes;
    out_snapshot->quarantine_peak_entries = g_kain_attrition_state.quarantine_peak_entries;
    out_snapshot->quarantine_peak_bytes = g_kain_attrition_state.quarantine_peak_bytes;
    out_snapshot->fragmentation_noise_live_bytes = g_kain_attrition_state.fragmentation_noise_live_bytes;
    out_snapshot->fragmentation_noise_peak_bytes = g_kain_attrition_state.fragmentation_noise_peak_bytes;
    out_snapshot->fragmentation_noise_total_bytes = g_kain_attrition_state.fragmentation_noise_total_bytes;
    out_snapshot->fragmentation_injection_count = g_kain_attrition_state.fragmentation_injection_count;
    out_snapshot->checkpoint_count = g_kain_attrition_state.checkpoint_count;
    out_snapshot->last_checkpoint_label_hash = g_kain_attrition_state.last_checkpoint_label_hash;
    out_snapshot->last_checkpoint_subject_id = g_kain_attrition_state.last_checkpoint_subject_id;
    out_snapshot->progress_heartbeat_count = g_kain_attrition_state.progress_heartbeat_count;
    out_snapshot->last_progress_iteration = g_kain_attrition_state.last_progress_iteration;
    out_snapshot->last_progress_checksum = g_kain_attrition_state.last_progress_checksum;
    out_snapshot->event_count_total = g_kain_attrition_state.event_write_count;
    out_snapshot->virtual_time_enabled = g_kain_attrition_state.config.virtual_time_enabled;
    out_snapshot->virtual_time_now_ms = g_kain_attrition_state.virtual_time_now_ms;
    out_snapshot->virtual_time_step_ms = g_kain_attrition_state.config.virtual_time_step_ms;
    out_snapshot->virtual_time_advance_count = g_kain_attrition_state.virtual_time_advance_count;
    out_snapshot->virtual_time_advance_total_ms = g_kain_attrition_state.virtual_time_advance_total_ms;
    out_snapshot->raw_clock_fallback_count = g_kain_attrition_state.raw_clock_fallback_count;
    out_snapshot->raw_sleep_fallback_count = g_kain_attrition_state.raw_sleep_fallback_count;
    out_snapshot->raw_sleep_fallback_millis_total = g_kain_attrition_state.raw_sleep_fallback_millis_total;
    kain_attrition_unlock();
    kain_attrition_actor_fill_snapshot(out_snapshot);
    kain_attrition_process_fill_snapshot(out_snapshot);
    kain_attrition_async_fill_snapshot(out_snapshot);
}

size_t kain_attrition_runtime_copy_events(KainAttritionEvent* out_events, size_t max_events) {
    size_t available;
    size_t count;
    size_t start_index;
    size_t i;
    /*
     * Proof:
     * - runtime/native/src/core/z3/proofs-experimental/attrition-event-ring-copy-window-bounds.smt2
     *
     * The flight-recorder ring copies the newest min(available, max_events)
     * entries. The solver owns the window math so count, start_index, and every
     * copied candidate slot stay inside the 1024-entry ring.
     */
    kain_attrition_ensure_initialized();
    if (out_events == NULL || max_events == 0u) {
        return 0u;
    }
    kain_attrition_lock();
    available = (size_t)(
        g_kain_attrition_state.event_write_count < KAIN_ATTRITION_EVENT_RING_CAPACITY
            ? g_kain_attrition_state.event_write_count
            : KAIN_ATTRITION_EVENT_RING_CAPACITY
    );
    count = available < max_events ? available : max_events;
    start_index = (g_kain_attrition_state.event_next_index + KAIN_ATTRITION_EVENT_RING_CAPACITY - count)
        % KAIN_ATTRITION_EVENT_RING_CAPACITY;
    for (i = 0u; i < count; ++i) {
        out_events[i] = g_kain_attrition_state.events[
            (start_index + i) % KAIN_ATTRITION_EVENT_RING_CAPACITY
        ];
    }
    kain_attrition_unlock();
    return count;
}

size_t kain_attrition_runtime_write_audit_json(char* out_text, size_t capacity) {
    KainAttritionSnapshot snapshot;
    int written;
    /*
     * Proof:
     * - runtime/native/src/core/z3/proofs-experimental/attrition-audit-json-length-range.smt2
     *
     * For every capacity > 0, the returned length is always strictly less than
     * capacity across the error, truncation, and exact-fit branches.
     */
    if (out_text == NULL || capacity == 0u) {
        return 0u;
    }
    kain_attrition_runtime_snapshot(&snapshot);
    written = snprintf(
        out_text,
        capacity,
        "{"
        "\"schema_version\":%llu,"
        "\"live_rc_objects\":%llu,"
        "\"peak_live_rc_objects\":%llu,"
        "\"live_runtime_bytes\":%llu,"
        "\"peak_runtime_bytes\":%llu,"
        "\"total_allocated_bytes\":%llu,"
        "\"total_freed_bytes\":%llu,"
        "\"allocation_fail_count\":%llu,"
        "\"actor_live_count\":%llu,"
        "\"reply_port_live_count\":%llu,"
        "\"pending_mailbox_message_count\":%llu,"
        "\"actor_registry_live_entries\":%llu,"
        "\"actor_scheduler_max_queue_depth\":%llu,"
        "\"process_live_count\":%llu,"
        "\"process_spec_live_count\":%llu,"
        "\"process_pipe_handle_live_count\":%llu,"
        "\"async_task_live_count\":%llu,"
        "\"async_task_sleeping_count\":%llu,"
        "\"async_timer_live_count\":%llu,"
        "\"async_timer_started_count\":%llu,"
        "\"checkpoint_count\":%llu,"
        "\"virtual_time_advance_count\":%llu,"
        "\"virtual_time_advance_total_ms\":%llu,"
        "\"actor_occupancy_low_word\":%llu,"
        "\"process_occupancy_bits\":%llu,"
        "\"async_task_occupancy_low_word\":%llu,"
        "\"async_timer_occupancy_low_word\":%llu,"
        "\"raw_sleep_fallback_millis_total\":%llu"
        "}",
        (unsigned long long)snapshot.schema_version,
        (unsigned long long)snapshot.live_rc_objects,
        (unsigned long long)snapshot.peak_live_rc_objects,
        (unsigned long long)snapshot.live_runtime_bytes,
        (unsigned long long)snapshot.peak_runtime_bytes,
        (unsigned long long)snapshot.total_allocated_bytes,
        (unsigned long long)snapshot.total_freed_bytes,
        (unsigned long long)snapshot.allocation_fail_count,
        (unsigned long long)snapshot.actor_live_count,
        (unsigned long long)snapshot.reply_port_live_count,
        (unsigned long long)snapshot.pending_mailbox_message_count,
        (unsigned long long)snapshot.actor_registry_live_entries,
        (unsigned long long)snapshot.actor_scheduler_max_queue_depth,
        (unsigned long long)snapshot.process_live_count,
        (unsigned long long)snapshot.process_spec_live_count,
        (unsigned long long)snapshot.process_pipe_handle_live_count,
        (unsigned long long)snapshot.async_task_live_count,
        (unsigned long long)snapshot.async_task_sleeping_count,
        (unsigned long long)snapshot.async_timer_live_count,
        (unsigned long long)snapshot.async_timer_started_count,
        (unsigned long long)snapshot.checkpoint_count,
        (unsigned long long)snapshot.virtual_time_advance_count,
        (unsigned long long)snapshot.virtual_time_advance_total_ms,
        (unsigned long long)snapshot.actor_occupancy_low_word,
        (unsigned long long)snapshot.process_occupancy_bits,
        (unsigned long long)snapshot.async_task_occupancy_low_word,
        (unsigned long long)snapshot.async_timer_occupancy_low_word,
        (unsigned long long)snapshot.raw_sleep_fallback_millis_total
    );
    if (written < 0) {
        out_text[0] = '\0';
        return 0u;
    }
    if ((size_t)written >= capacity) {
        out_text[capacity - 1u] = '\0';
        return capacity - 1u;
    }
    return (size_t)written;
}

void kain_attrition_runtime_checkpoint(const char* label, uint64_t subject_id) {
    uint64_t label_hash = 1469598103934665603ULL;
    const unsigned char* cursor = (const unsigned char*)(label != NULL ? label : "");
    kain_attrition_ensure_initialized();
    while (*cursor != '\0') {
        label_hash ^= (uint64_t)(*cursor++);
        label_hash *= 1099511628211ULL;
    }
    kain_attrition_lock();
    g_kain_attrition_state.checkpoint_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.checkpoint_count, 1u);
    g_kain_attrition_state.last_checkpoint_label_hash = label_hash;
    g_kain_attrition_state.last_checkpoint_subject_id = subject_id;
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_CHECKPOINT,
        0u,
        label_hash,
        subject_id,
        g_kain_attrition_state.progress_heartbeat_count
    );
    kain_attrition_unlock();
}

void kain_attrition_runtime_note_progress(uint64_t iteration, uint64_t checksum) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.progress_heartbeat_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.progress_heartbeat_count, 1u);
    g_kain_attrition_state.last_progress_iteration = iteration;
    g_kain_attrition_state.last_progress_checksum = checksum;
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_PROGRESS,
        0u,
        iteration,
        checksum,
        g_kain_attrition_state.progress_heartbeat_count
    );
    kain_attrition_unlock();
}

void* kain_attrition_heap_alloc(size_t total_bytes) {
    void* allocation = NULL;
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    if (g_kain_attrition_state.config.allocation_fail_after != 0u &&
        g_kain_attrition_state.allocation_count >= g_kain_attrition_state.config.allocation_fail_after) {
        g_kain_attrition_state.allocation_fail_count =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.allocation_fail_count, 1u);
        kain_attrition_unlock();
        return NULL;
    }
    kain_attrition_fragmentation_noise_locked();
    kain_attrition_unlock();
    allocation = malloc(total_bytes);
    if (allocation == NULL) {
        kain_attrition_lock();
        g_kain_attrition_state.allocation_fail_count =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.allocation_fail_count, 1u);
        kain_attrition_unlock();
    }
    return allocation;
}

int kain_attrition_heap_release(void* raw_header, size_t total_bytes) {
    size_t slot;
    KainAttritionQuarantineEntry* entry;
    if (raw_header == NULL) {
        return 1;
    }
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    if (g_kain_attrition_state.config.poison_on_free != 0u) {
        memset(raw_header, 0xA5, total_bytes);
        g_kain_attrition_state.poison_free_count =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.poison_free_count, 1u);
    }
    if (g_kain_attrition_state.config.quarantine_capacity == 0u) {
        kain_attrition_unlock();
        return 0;
    }
    slot = g_kain_attrition_state.quarantine_head;
    entry = &g_kain_attrition_state.quarantine[slot];
    if (entry->ptr != NULL) {
        if (g_kain_attrition_state.quarantine_live_bytes >= entry->bytes) {
            g_kain_attrition_state.quarantine_live_bytes -= entry->bytes;
        } else {
            g_kain_attrition_state.quarantine_live_bytes = 0u;
        }
        free(entry->ptr);
    } else if (g_kain_attrition_state.quarantine_count < g_kain_attrition_state.config.quarantine_capacity) {
        g_kain_attrition_state.quarantine_count += 1u;
    }
    entry->ptr = raw_header;
    entry->bytes = total_bytes;
    g_kain_attrition_state.quarantine_live_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.quarantine_live_bytes, (uint64_t)total_bytes);
    kain_attrition_update_peak_u64(
        &g_kain_attrition_state.quarantine_peak_entries,
        (uint64_t)g_kain_attrition_state.quarantine_count);
    kain_attrition_update_peak_u64(
        &g_kain_attrition_state.quarantine_peak_bytes,
        g_kain_attrition_state.quarantine_live_bytes);
    g_kain_attrition_state.quarantine_head =
        (g_kain_attrition_state.quarantine_head + 1u) % g_kain_attrition_state.config.quarantine_capacity;
    kain_attrition_unlock();
    return 1;
}

unsigned long long kain_attrition_now_millis(void) {
    unsigned long long result;
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    if (g_kain_attrition_state.config.virtual_time_enabled != 0u) {
        result = g_kain_attrition_state.virtual_time_now_ms;
        kain_attrition_unlock();
        return result;
    }
    g_kain_attrition_state.raw_clock_fallback_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.raw_clock_fallback_count, 1u);
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_RAW_CLOCK_FALLBACK,
        0u,
        0u,
        0u,
        0u
    );
    kain_attrition_unlock();
    return (unsigned long long)((clock() * 1000ULL) / CLOCKS_PER_SEC);
}

long long kain_attrition_clock_ticks(void) {
    return (long long)kain_attrition_now_millis();
}

void kain_attrition_sleep_for_millis(unsigned long long milliseconds) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    if (g_kain_attrition_state.config.virtual_time_enabled != 0u) {
        uint64_t advance = milliseconds != 0u ? milliseconds : g_kain_attrition_state.config.virtual_time_step_ms;
        g_kain_attrition_state.virtual_time_now_ms =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.virtual_time_now_ms, advance);
        g_kain_attrition_state.virtual_time_advance_count =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.virtual_time_advance_count, 1u);
        g_kain_attrition_state.virtual_time_advance_total_ms =
            kain_attrition_u64_saturating_add(g_kain_attrition_state.virtual_time_advance_total_ms, advance);
        kain_attrition_record_event_locked(
            KAIN_ATTRITION_EVENT_VIRTUAL_TIME_ADVANCE,
            0u,
            advance,
            g_kain_attrition_state.virtual_time_now_ms,
            milliseconds
        );
        kain_attrition_unlock();
        return;
    }
    g_kain_attrition_state.raw_sleep_fallback_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.raw_sleep_fallback_count, 1u);
    g_kain_attrition_state.raw_sleep_fallback_millis_total =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.raw_sleep_fallback_millis_total, milliseconds);
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_RAW_SLEEP_FALLBACK,
        0u,
        milliseconds,
        0u,
        0u
    );
    kain_attrition_unlock();
#ifdef _WIN32
    Sleep((DWORD)(milliseconds > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : milliseconds));
#else
    {
        struct timespec delay;
        delay.tv_sec = (time_t)(milliseconds / 1000ULL);
        delay.tv_nsec = (long)((milliseconds % 1000ULL) * 1000000ULL);
        while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
        }
    }
#endif
}

void kain_attrition_note_raw_clock_fallback(void) {
    (void)kain_attrition_now_millis();
}

void kain_attrition_note_raw_sleep_fallback(unsigned long long milliseconds) {
    kain_attrition_sleep_for_millis(milliseconds);
}

void kain_attrition_note_rc_alloc(size_t total_bytes) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.allocation_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.allocation_count, 1u);
    g_kain_attrition_state.live_rc_objects =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.live_rc_objects, 1u);
    g_kain_attrition_state.total_allocated_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.total_allocated_bytes, (uint64_t)total_bytes);
    g_kain_attrition_state.live_runtime_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.live_runtime_bytes, (uint64_t)total_bytes);
    kain_attrition_update_peak_u64(
        &g_kain_attrition_state.peak_live_rc_objects,
        g_kain_attrition_state.live_rc_objects);
    kain_attrition_update_peak_u64(
        &g_kain_attrition_state.peak_runtime_bytes,
        g_kain_attrition_state.live_runtime_bytes);
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_RC_ALLOC,
        0u,
        total_bytes,
        g_kain_attrition_state.live_runtime_bytes,
        g_kain_attrition_state.total_allocated_bytes);
    kain_attrition_unlock();
}

void kain_attrition_note_rc_free(size_t total_bytes) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.free_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.free_count, 1u);
    if (g_kain_attrition_state.live_rc_objects > 0u) {
        g_kain_attrition_state.live_rc_objects -= 1u;
    }
    if (g_kain_attrition_state.live_runtime_bytes >= total_bytes) {
        g_kain_attrition_state.live_runtime_bytes -= total_bytes;
    } else {
        g_kain_attrition_state.live_runtime_bytes = 0u;
    }
    g_kain_attrition_state.total_freed_bytes =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.total_freed_bytes, (uint64_t)total_bytes);
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_RC_FREE,
        0u,
        total_bytes,
        g_kain_attrition_state.live_runtime_bytes,
        g_kain_attrition_state.total_freed_bytes);
    kain_attrition_unlock();
}

void kain_attrition_note_rc_retain(void) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.retain_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.retain_count, 1u);
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_RC_RETAIN, 0u, g_kain_attrition_state.retain_count, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_rc_release(void) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.release_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.release_count, 1u);
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_RC_RELEASE, 0u, g_kain_attrition_state.release_count, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_rc_underflow(void) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.rc_underflow_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.rc_underflow_count, 1u);
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_RC_UNDERFLOW, 0u, g_kain_attrition_state.rc_underflow_count, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_rc_overflow(void) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    g_kain_attrition_state.rc_overflow_count =
        kain_attrition_u64_saturating_add(g_kain_attrition_state.rc_overflow_count, 1u);
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_RC_OVERFLOW, 0u, g_kain_attrition_state.rc_overflow_count, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_actor_spawn(uint64_t actor_id, int synthetic_reply_port) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_ACTOR_SPAWN,
        synthetic_reply_port ? 1u : 0u,
        actor_id,
        0u,
        0u
    );
    kain_attrition_unlock();
}

void kain_attrition_note_actor_exit(uint64_t actor_id, int synthetic_reply_port) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(
        KAIN_ATTRITION_EVENT_ACTOR_EXIT,
        synthetic_reply_port ? 1u : 0u,
        actor_id,
        0u,
        0u
    );
    kain_attrition_unlock();
}

void kain_attrition_note_actor_stale_reject(uint64_t actor_id, uint64_t generation) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ACTOR_STALE_REJECT, 0u, actor_id, generation, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_process_spawn(uint64_t process_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_PROCESS_SPAWN, 0u, process_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_process_exit(uint64_t process_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_PROCESS_EXIT, 0u, process_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_process_stale_reject(uint64_t subject_id, int64_t status) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_PROCESS_STALE_REJECT, (uint32_t)(status & 0xffffffffu), subject_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_task_spawn(uint64_t task_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TASK_SPAWN, 0u, task_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_task_exit(uint64_t task_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TASK_EXIT, 0u, task_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_task_stale_reject(uint64_t task_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TASK_STALE_REJECT, 0u, task_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_timer_spawn(uint64_t timer_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TIMER_SPAWN, 0u, timer_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_timer_exit(uint64_t timer_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TIMER_EXIT, 0u, timer_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_timer_cancel(uint64_t timer_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TIMER_CANCEL, 0u, timer_id, 0u, 0u);
    kain_attrition_unlock();
}

void kain_attrition_note_async_timer_stale_reject(uint64_t timer_id) {
    kain_attrition_ensure_initialized();
    kain_attrition_lock();
    kain_attrition_record_event_locked(KAIN_ATTRITION_EVENT_ASYNC_TIMER_STALE_REJECT, 0u, timer_id, 0u, 0u);
    kain_attrition_unlock();
}
