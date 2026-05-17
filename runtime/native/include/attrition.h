#ifndef KAIN_ATTRITION_H
#define KAIN_ATTRITION_H

#include "actor.h"
#include "async.h"
#include "base.h"
#include "process_system.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_ATTRITION_SCHEMA_VERSION 2u
#define KAIN_ATTRITION_EVENT_RING_CAPACITY 1024u
#define KAIN_ATTRITION_QUARANTINE_CAPACITY_MAX 4096u
#define KAIN_ATTRITION_FRAGMENTATION_RING_CAPACITY 256u

typedef enum KainAttritionDeterminismTier {
    KAIN_ATTRITION_DETERMINISM_TIER_1 = 1,
    KAIN_ATTRITION_DETERMINISM_TIER_2 = 2,
    KAIN_ATTRITION_DETERMINISM_TIER_3 = 3
} KainAttritionDeterminismTier;

typedef enum KainAttritionEventKind {
    KAIN_ATTRITION_EVENT_CHECKPOINT = 1,
    KAIN_ATTRITION_EVENT_PROGRESS = 2,
    KAIN_ATTRITION_EVENT_RC_ALLOC = 10,
    KAIN_ATTRITION_EVENT_RC_FREE = 11,
    KAIN_ATTRITION_EVENT_RC_RETAIN = 12,
    KAIN_ATTRITION_EVENT_RC_RELEASE = 13,
    KAIN_ATTRITION_EVENT_RC_UNDERFLOW = 14,
    KAIN_ATTRITION_EVENT_RC_OVERFLOW = 15,
    KAIN_ATTRITION_EVENT_ACTOR_SPAWN = 20,
    KAIN_ATTRITION_EVENT_ACTOR_EXIT = 21,
    KAIN_ATTRITION_EVENT_ACTOR_STALE_REJECT = 22,
    KAIN_ATTRITION_EVENT_PROCESS_SPAWN = 30,
    KAIN_ATTRITION_EVENT_PROCESS_EXIT = 31,
    KAIN_ATTRITION_EVENT_PROCESS_STALE_REJECT = 32,
    KAIN_ATTRITION_EVENT_ASYNC_TASK_SPAWN = 40,
    KAIN_ATTRITION_EVENT_ASYNC_TASK_EXIT = 41,
    KAIN_ATTRITION_EVENT_ASYNC_TASK_STALE_REJECT = 42,
    KAIN_ATTRITION_EVENT_ASYNC_TIMER_SPAWN = 50,
    KAIN_ATTRITION_EVENT_ASYNC_TIMER_EXIT = 51,
    KAIN_ATTRITION_EVENT_ASYNC_TIMER_CANCEL = 52,
    KAIN_ATTRITION_EVENT_ASYNC_TIMER_STALE_REJECT = 53,
    KAIN_ATTRITION_EVENT_VIRTUAL_TIME_ADVANCE = 60,
    KAIN_ATTRITION_EVENT_RAW_CLOCK_FALLBACK = 61,
    KAIN_ATTRITION_EVENT_RAW_SLEEP_FALLBACK = 62
} KainAttritionEventKind;

typedef struct KainAttritionEvent {
    uint64_t event_index;
    uint32_t kind;
    uint32_t aux;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
} KainAttritionEvent;

typedef struct KainAttritionSessionConfig {
    uint64_t enabled;
    uint64_t seed;
    uint64_t virtual_time_enabled;
    uint64_t virtual_time_initial_ms;
    uint64_t virtual_time_step_ms;
    uint64_t poison_on_free;
    uint64_t quarantine_capacity;
    uint64_t fragmentation_noise_max_bytes;
    uint64_t allocation_fail_after;
    uint64_t determinism_tier;
} KainAttritionSessionConfig;

typedef struct KainAttritionSnapshot {
    uint64_t schema_version;
    uint64_t seed;
    uint64_t determinism_tier;

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
    uint64_t quarantine_live_entries;
    uint64_t quarantine_live_bytes;
    uint64_t quarantine_peak_entries;
    uint64_t quarantine_peak_bytes;
    uint64_t fragmentation_noise_live_bytes;
    uint64_t fragmentation_noise_peak_bytes;
    uint64_t fragmentation_noise_total_bytes;
    uint64_t fragmentation_injection_count;

    uint64_t actor_live_count;
    uint64_t actor_peak_count;
    uint64_t actor_spawn_count;
    uint64_t actor_exit_count;
    uint64_t actor_stale_reject_count;
    uint64_t reply_port_live_count;
    uint64_t reply_port_peak_count;
    uint64_t pending_mailbox_message_count;
    uint64_t pending_mailbox_cached_nodes;
    uint64_t actor_occupancy_low_word;
    uint64_t actor_occupancy_popcount;
    uint64_t actor_registry_live_entries;
    uint64_t actor_monitor_edge_count;
    uint64_t actor_link_edge_count;
    uint64_t actor_supervised_count;
    uint64_t actor_in_scheduler_turn_count;
    uint64_t actor_restart_attempt_count_total;
    uint64_t actor_supervision_limit_hit_count;
    uint64_t actor_strategy_shutdown_count_total;
    uint64_t actor_escalation_count_total;
    uint64_t actor_scheduler_queue_depth;
    uint64_t actor_scheduler_max_queue_depth;
    uint64_t actor_scheduler_total_enqueued;
    uint64_t actor_scheduler_total_dequeued;
    uint64_t actor_scheduler_worker_count;
    uint64_t actor_scheduler_active_workers;
    uint64_t actor_scheduler_busy_workers;
    uint64_t actor_scheduler_max_busy_workers;
    uint64_t actor_scheduler_overflow_thread_spawns;
    uint64_t actor_scheduler_shutdown;

    uint64_t process_live_count;
    uint64_t process_peak_count;
    uint64_t process_spawn_count;
    uint64_t process_exit_count;
    uint64_t process_stale_reject_count;
    uint64_t process_spec_live_count;
    uint64_t process_spec_occupancy_bits;
    uint64_t process_occupancy_bits;
    uint64_t process_pipe_handle_live_count;
    uint64_t process_os_handle_live_count;
    uint64_t process_pty_live_count;
    uint64_t process_capture_live_bytes;

    uint64_t async_task_live_count;
    uint64_t async_task_peak_count;
    uint64_t async_task_spawn_count;
    uint64_t async_task_exit_count;
    uint64_t async_task_stale_reject_count;
    uint64_t async_task_occupancy_low_word;
    uint64_t async_task_occupancy_popcount;
    uint64_t async_task_cancel_requested_count;
    uint64_t async_task_sleeping_count;
    uint64_t async_task_ready_count;

    uint64_t async_timer_live_count;
    uint64_t async_timer_peak_count;
    uint64_t async_timer_spawn_count;
    uint64_t async_timer_exit_count;
    uint64_t async_timer_cancel_count;
    uint64_t async_timer_stale_reject_count;
    uint64_t async_timer_occupancy_low_word;
    uint64_t async_timer_occupancy_popcount;
    uint64_t async_timer_cancelled_count;
    uint64_t async_timer_fired_count;
    uint64_t async_timer_started_count;

    uint64_t checkpoint_count;
    uint64_t last_checkpoint_label_hash;
    uint64_t last_checkpoint_subject_id;
    uint64_t progress_heartbeat_count;
    uint64_t last_progress_iteration;
    uint64_t last_progress_checksum;
    uint64_t event_count_total;

    uint64_t virtual_time_enabled;
    uint64_t virtual_time_now_ms;
    uint64_t virtual_time_step_ms;
    uint64_t virtual_time_advance_count;
    uint64_t virtual_time_advance_total_ms;
    uint64_t raw_clock_fallback_count;
    uint64_t raw_sleep_fallback_count;
    uint64_t raw_sleep_fallback_millis_total;
} KainAttritionSnapshot;

void kain_attrition_session_config_init(KainAttritionSessionConfig* config);
void kain_attrition_runtime_reset(void);
void kain_attrition_runtime_configure(const KainAttritionSessionConfig* config);
void kain_attrition_runtime_snapshot(KainAttritionSnapshot* out_snapshot);
size_t kain_attrition_runtime_copy_events(KainAttritionEvent* out_events, size_t max_events);
size_t kain_attrition_runtime_write_audit_json(char* out_text, size_t capacity);
void kain_attrition_runtime_checkpoint(const char* label, uint64_t subject_id);
void kain_attrition_runtime_note_progress(uint64_t iteration, uint64_t checksum);

void* kain_attrition_heap_alloc(size_t total_bytes);
int kain_attrition_heap_release(void* raw_header, size_t total_bytes);
unsigned long long kain_attrition_now_millis(void);
long long kain_attrition_clock_ticks(void);
void kain_attrition_sleep_for_millis(unsigned long long milliseconds);
void kain_attrition_note_raw_clock_fallback(void);
void kain_attrition_note_raw_sleep_fallback(unsigned long long milliseconds);

void kain_attrition_note_rc_alloc(size_t total_bytes);
void kain_attrition_note_rc_free(size_t total_bytes);
void kain_attrition_note_rc_retain(void);
void kain_attrition_note_rc_release(void);
void kain_attrition_note_rc_underflow(void);
void kain_attrition_note_rc_overflow(void);

void kain_attrition_note_actor_spawn(uint64_t actor_id, int synthetic_reply_port);
void kain_attrition_note_actor_exit(uint64_t actor_id, int synthetic_reply_port);
void kain_attrition_note_actor_stale_reject(uint64_t actor_id, uint64_t generation);
void kain_attrition_actor_counters_reset(void);
void kain_attrition_actor_fill_snapshot(KainAttritionSnapshot* snapshot);

void kain_attrition_note_process_spawn(uint64_t process_id);
void kain_attrition_note_process_exit(uint64_t process_id);
void kain_attrition_note_process_stale_reject(uint64_t subject_id, int64_t status);
void kain_attrition_process_counters_reset(void);
void kain_attrition_process_fill_snapshot(KainAttritionSnapshot* snapshot);

void kain_attrition_note_async_task_spawn(uint64_t task_id);
void kain_attrition_note_async_task_exit(uint64_t task_id);
void kain_attrition_note_async_task_stale_reject(uint64_t task_id);
void kain_attrition_note_async_timer_spawn(uint64_t timer_id);
void kain_attrition_note_async_timer_exit(uint64_t timer_id);
void kain_attrition_note_async_timer_cancel(uint64_t timer_id);
void kain_attrition_note_async_timer_stale_reject(uint64_t timer_id);
void kain_attrition_async_counters_reset(void);
void kain_attrition_async_fill_snapshot(KainAttritionSnapshot* snapshot);
int kain_attrition_async_dispose_task(KainTaskId task_id);

#ifdef __cplusplus
}
#endif

#endif
