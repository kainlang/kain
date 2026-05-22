#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#ifndef _CRT_RAND_S
#define _CRT_RAND_S
#endif
#endif

#include "../../include/stdlib_abi.h"
#include "../../include/attrition.h"

#include "../../include/actor.h"
#include "../../include/async.h"
#include "../../include/base.h"
#include "../../include/diagnostics.h"
#include "../../include/entangle.h"
#include "../../include/fanout.h"

#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

static int abi_size_add_overflows(size_t left, size_t right) {
    return left > (SIZE_MAX - right);
}

void* kain_alloc_rc(size_t size, long long type_tag);

#define ABI_ATTRITION_TEXT_CAPACITY 256u
#define ABI_ATTRITION_PATH_CAPACITY 1024u

typedef struct KainAbiAttritionCaptureState {
    int enabled;
    int report_written;
    int result_set_seen;
    uint64_t ops;
    uint64_t expect_failure;
    int64_t checksum;
    int64_t run_status;
    char case_id[ABI_ATTRITION_TEXT_CAPACITY];
    char sabotage_mode[ABI_ATTRITION_TEXT_CAPACITY];
    char result_path[ABI_ATTRITION_PATH_CAPACITY];
    char run_failure[ABI_ATTRITION_TEXT_CAPACITY];
    KainAttritionSnapshot baseline_snapshot;
} KainAbiAttritionCaptureState;

static KainAbiAttritionCaptureState g_abi_attrition_capture_state;

#define ABI_TAG_OPTION_NONE 0
#define ABI_TAG_OPTION_SOME 1
#define ABI_TAG_RESULT_OK 2
#define ABI_TAG_RESULT_ERR 3
#define ABI_TAG_FUTURE 4

#ifndef KAIN_RUNTIME_INIT_NET_RESET
#define KAIN_RUNTIME_INIT_NET_RESET 1
#endif

#ifndef KAIN_RUNTIME_INIT_PROCESS_RESET
#define KAIN_RUNTIME_INIT_PROCESS_RESET 1
#endif

#ifndef KAIN_RUNTIME_INIT_ACTOR_SHUTDOWN
#define KAIN_RUNTIME_INIT_ACTOR_SHUTDOWN 1
#endif

typedef struct KainNativeTaggedValue {
    int64_t tag;
    int64_t payload_size;
    unsigned char payload[];
} KainNativeTaggedValue;

typedef struct KainNativeFutureValue {
    int64_t tag;
    int64_t payload_size;
    KainTaskId task_id;
    void* cached_result;
    unsigned char inline_payload[];
} KainNativeFutureValue;

static KainDiagnostic abi_diag(void) {
    KainDiagnostic diag;
    kain_diagnostic_init(&diag);
    return diag;
}

static void abi_copy_text(char* destination, size_t capacity, const char* source) {
    size_t index = 0u;
    if (destination == NULL || capacity == 0u) {
        return;
    }
    if (source != NULL) {
        while (source[index] != '\0' && index + 1u < capacity) {
            destination[index] = source[index];
            index += 1u;
        }
    }
    destination[index] = '\0';
}

static int abi_parse_env_u64(const char* key, uint64_t default_value, uint64_t* out_value) {
    const char* text;
    char* end = NULL;
    unsigned long long parsed;
    if (out_value == NULL) {
        return 0;
    }
    *out_value = default_value;
    if (key == NULL) {
        return 0;
    }
    text = getenv(key);
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || end == NULL || *end != '\0') {
        return -1;
    }
    *out_value = (uint64_t)parsed;
    return 1;
}

static int abi_attrition_capture_enabled(void) {
    return g_abi_attrition_capture_state.enabled != 0 &&
        g_abi_attrition_capture_state.result_path[0] != '\0';
}

static void abi_attrition_capture_reset_state(void) {
    memset(&g_abi_attrition_capture_state, 0, sizeof(g_abi_attrition_capture_state));
    abi_copy_text(g_abi_attrition_capture_state.case_id, sizeof(g_abi_attrition_capture_state.case_id), "unknown");
}

static void abi_attrition_json_write_escaped(FILE* stream, const char* text) {
    const unsigned char* cursor = (const unsigned char*)(text != NULL ? text : "");
    fputc('"', stream);
    while (*cursor != '\0') {
        unsigned char ch = *cursor++;
        switch (ch) {
            case '\\':
                fputs("\\\\", stream);
                break;
            case '"':
                fputs("\\\"", stream);
                break;
            case '\n':
                fputs("\\n", stream);
                break;
            case '\r':
                fputs("\\r", stream);
                break;
            case '\t':
                fputs("\\t", stream);
                break;
            default:
                if (ch < 0x20u) {
                    fprintf(stream, "\\u%04x", ch);
                } else {
                    fputc((int)ch, stream);
                }
                break;
        }
    }
    fputc('"', stream);
}

static void abi_attrition_json_write_snapshot(FILE* stream, const KainAttritionSnapshot* snapshot) {
    fprintf(
        stream,
        "{"
        "\"schema_version\":%llu,"
        "\"seed\":%llu,"
        "\"determinism_tier\":%llu,"
        "\"live_rc_objects\":%llu,"
        "\"peak_live_rc_objects\":%llu,"
        "\"live_runtime_bytes\":%llu,"
        "\"peak_runtime_bytes\":%llu,"
        "\"allocation_count\":%llu,"
        "\"free_count\":%llu,"
        "\"total_allocated_bytes\":%llu,"
        "\"total_freed_bytes\":%llu,"
        "\"allocation_fail_count\":%llu,"
        "\"retain_count\":%llu,"
        "\"release_count\":%llu,"
        "\"rc_underflow_count\":%llu,"
        "\"rc_overflow_count\":%llu,"
        "\"poison_free_count\":%llu,"
        "\"quarantine_live_entries\":%llu,"
        "\"quarantine_live_bytes\":%llu,"
        "\"quarantine_peak_entries\":%llu,"
        "\"quarantine_peak_bytes\":%llu,"
        "\"fragmentation_noise_live_bytes\":%llu,"
        "\"fragmentation_noise_peak_bytes\":%llu,"
        "\"fragmentation_noise_total_bytes\":%llu,"
        "\"fragmentation_injection_count\":%llu,"
        "\"actor_live_count\":%llu,"
        "\"actor_peak_count\":%llu,"
        "\"actor_spawn_count\":%llu,"
        "\"actor_exit_count\":%llu,"
        "\"actor_stale_reject_count\":%llu,"
        "\"reply_port_live_count\":%llu,"
        "\"reply_port_peak_count\":%llu,"
        "\"pending_mailbox_message_count\":%llu,"
        "\"pending_mailbox_cached_nodes\":%llu,"
        "\"actor_occupancy_low_word\":%llu,"
        "\"actor_occupancy_popcount\":%llu,"
        "\"actor_registry_live_entries\":%llu,"
        "\"actor_monitor_edge_count\":%llu,"
        "\"actor_link_edge_count\":%llu,"
        "\"actor_supervised_count\":%llu,"
        "\"actor_in_scheduler_turn_count\":%llu,"
        "\"actor_restart_attempt_count_total\":%llu,"
        "\"actor_supervision_limit_hit_count\":%llu,"
        "\"actor_strategy_shutdown_count_total\":%llu,"
        "\"actor_escalation_count_total\":%llu,"
        "\"actor_scheduler_queue_depth\":%llu,"
        "\"actor_scheduler_max_queue_depth\":%llu,"
        "\"actor_scheduler_total_enqueued\":%llu,"
        "\"actor_scheduler_total_dequeued\":%llu,"
        "\"actor_scheduler_worker_count\":%llu,"
        "\"actor_scheduler_active_workers\":%llu,"
        "\"actor_scheduler_busy_workers\":%llu,"
        "\"actor_scheduler_max_busy_workers\":%llu,"
        "\"actor_scheduler_overflow_thread_spawns\":%llu,"
        "\"actor_scheduler_shutdown\":%llu,"
        "\"process_live_count\":%llu,"
        "\"process_peak_count\":%llu,"
        "\"process_spawn_count\":%llu,"
        "\"process_exit_count\":%llu,"
        "\"process_stale_reject_count\":%llu,"
        "\"process_spec_live_count\":%llu,"
        "\"process_spec_occupancy_bits\":%llu,"
        "\"process_occupancy_bits\":%llu,"
        "\"process_pipe_handle_live_count\":%llu,"
        "\"process_os_handle_live_count\":%llu,"
        "\"process_pty_live_count\":%llu,"
        "\"process_capture_live_bytes\":%llu,"
        "\"async_task_live_count\":%llu,"
        "\"async_task_peak_count\":%llu,"
        "\"async_task_spawn_count\":%llu,"
        "\"async_task_exit_count\":%llu,"
        "\"async_task_stale_reject_count\":%llu,"
        "\"async_task_occupancy_low_word\":%llu,"
        "\"async_task_occupancy_popcount\":%llu,"
        "\"async_task_cancel_requested_count\":%llu,"
        "\"async_task_sleeping_count\":%llu,"
        "\"async_task_ready_count\":%llu,"
        "\"async_timer_live_count\":%llu,"
        "\"async_timer_peak_count\":%llu,"
        "\"async_timer_spawn_count\":%llu,"
        "\"async_timer_exit_count\":%llu,"
        "\"async_timer_cancel_count\":%llu,"
        "\"async_timer_stale_reject_count\":%llu,"
        "\"async_timer_occupancy_low_word\":%llu,"
        "\"async_timer_occupancy_popcount\":%llu,"
        "\"async_timer_cancelled_count\":%llu,"
        "\"async_timer_fired_count\":%llu,"
        "\"async_timer_started_count\":%llu,"
        "\"checkpoint_count\":%llu,"
        "\"last_checkpoint_label_hash\":%llu,"
        "\"last_checkpoint_subject_id\":%llu,"
        "\"progress_heartbeat_count\":%llu,"
        "\"last_progress_iteration\":%llu,"
        "\"last_progress_checksum\":%llu,"
        "\"event_count_total\":%llu,"
        "\"virtual_time_enabled\":%llu,"
        "\"virtual_time_now_ms\":%llu,"
        "\"virtual_time_step_ms\":%llu,"
        "\"virtual_time_advance_count\":%llu,"
        "\"virtual_time_advance_total_ms\":%llu,"
        "\"raw_clock_fallback_count\":%llu,"
        "\"raw_sleep_fallback_count\":%llu,"
        "\"raw_sleep_fallback_millis_total\":%llu"
        "}",
        (unsigned long long)snapshot->schema_version,
        (unsigned long long)snapshot->seed,
        (unsigned long long)snapshot->determinism_tier,
        (unsigned long long)snapshot->live_rc_objects,
        (unsigned long long)snapshot->peak_live_rc_objects,
        (unsigned long long)snapshot->live_runtime_bytes,
        (unsigned long long)snapshot->peak_runtime_bytes,
        (unsigned long long)snapshot->allocation_count,
        (unsigned long long)snapshot->free_count,
        (unsigned long long)snapshot->total_allocated_bytes,
        (unsigned long long)snapshot->total_freed_bytes,
        (unsigned long long)snapshot->allocation_fail_count,
        (unsigned long long)snapshot->retain_count,
        (unsigned long long)snapshot->release_count,
        (unsigned long long)snapshot->rc_underflow_count,
        (unsigned long long)snapshot->rc_overflow_count,
        (unsigned long long)snapshot->poison_free_count,
        (unsigned long long)snapshot->quarantine_live_entries,
        (unsigned long long)snapshot->quarantine_live_bytes,
        (unsigned long long)snapshot->quarantine_peak_entries,
        (unsigned long long)snapshot->quarantine_peak_bytes,
        (unsigned long long)snapshot->fragmentation_noise_live_bytes,
        (unsigned long long)snapshot->fragmentation_noise_peak_bytes,
        (unsigned long long)snapshot->fragmentation_noise_total_bytes,
        (unsigned long long)snapshot->fragmentation_injection_count,
        (unsigned long long)snapshot->actor_live_count,
        (unsigned long long)snapshot->actor_peak_count,
        (unsigned long long)snapshot->actor_spawn_count,
        (unsigned long long)snapshot->actor_exit_count,
        (unsigned long long)snapshot->actor_stale_reject_count,
        (unsigned long long)snapshot->reply_port_live_count,
        (unsigned long long)snapshot->reply_port_peak_count,
        (unsigned long long)snapshot->pending_mailbox_message_count,
        (unsigned long long)snapshot->pending_mailbox_cached_nodes,
        (unsigned long long)snapshot->actor_occupancy_low_word,
        (unsigned long long)snapshot->actor_occupancy_popcount,
        (unsigned long long)snapshot->actor_registry_live_entries,
        (unsigned long long)snapshot->actor_monitor_edge_count,
        (unsigned long long)snapshot->actor_link_edge_count,
        (unsigned long long)snapshot->actor_supervised_count,
        (unsigned long long)snapshot->actor_in_scheduler_turn_count,
        (unsigned long long)snapshot->actor_restart_attempt_count_total,
        (unsigned long long)snapshot->actor_supervision_limit_hit_count,
        (unsigned long long)snapshot->actor_strategy_shutdown_count_total,
        (unsigned long long)snapshot->actor_escalation_count_total,
        (unsigned long long)snapshot->actor_scheduler_queue_depth,
        (unsigned long long)snapshot->actor_scheduler_max_queue_depth,
        (unsigned long long)snapshot->actor_scheduler_total_enqueued,
        (unsigned long long)snapshot->actor_scheduler_total_dequeued,
        (unsigned long long)snapshot->actor_scheduler_worker_count,
        (unsigned long long)snapshot->actor_scheduler_active_workers,
        (unsigned long long)snapshot->actor_scheduler_busy_workers,
        (unsigned long long)snapshot->actor_scheduler_max_busy_workers,
        (unsigned long long)snapshot->actor_scheduler_overflow_thread_spawns,
        (unsigned long long)snapshot->actor_scheduler_shutdown,
        (unsigned long long)snapshot->process_live_count,
        (unsigned long long)snapshot->process_peak_count,
        (unsigned long long)snapshot->process_spawn_count,
        (unsigned long long)snapshot->process_exit_count,
        (unsigned long long)snapshot->process_stale_reject_count,
        (unsigned long long)snapshot->process_spec_live_count,
        (unsigned long long)snapshot->process_spec_occupancy_bits,
        (unsigned long long)snapshot->process_occupancy_bits,
        (unsigned long long)snapshot->process_pipe_handle_live_count,
        (unsigned long long)snapshot->process_os_handle_live_count,
        (unsigned long long)snapshot->process_pty_live_count,
        (unsigned long long)snapshot->process_capture_live_bytes,
        (unsigned long long)snapshot->async_task_live_count,
        (unsigned long long)snapshot->async_task_peak_count,
        (unsigned long long)snapshot->async_task_spawn_count,
        (unsigned long long)snapshot->async_task_exit_count,
        (unsigned long long)snapshot->async_task_stale_reject_count,
        (unsigned long long)snapshot->async_task_occupancy_low_word,
        (unsigned long long)snapshot->async_task_occupancy_popcount,
        (unsigned long long)snapshot->async_task_cancel_requested_count,
        (unsigned long long)snapshot->async_task_sleeping_count,
        (unsigned long long)snapshot->async_task_ready_count,
        (unsigned long long)snapshot->async_timer_live_count,
        (unsigned long long)snapshot->async_timer_peak_count,
        (unsigned long long)snapshot->async_timer_spawn_count,
        (unsigned long long)snapshot->async_timer_exit_count,
        (unsigned long long)snapshot->async_timer_cancel_count,
        (unsigned long long)snapshot->async_timer_stale_reject_count,
        (unsigned long long)snapshot->async_timer_occupancy_low_word,
        (unsigned long long)snapshot->async_timer_occupancy_popcount,
        (unsigned long long)snapshot->async_timer_cancelled_count,
        (unsigned long long)snapshot->async_timer_fired_count,
        (unsigned long long)snapshot->async_timer_started_count,
        (unsigned long long)snapshot->checkpoint_count,
        (unsigned long long)snapshot->last_checkpoint_label_hash,
        (unsigned long long)snapshot->last_checkpoint_subject_id,
        (unsigned long long)snapshot->progress_heartbeat_count,
        (unsigned long long)snapshot->last_progress_iteration,
        (unsigned long long)snapshot->last_progress_checksum,
        (unsigned long long)snapshot->event_count_total,
        (unsigned long long)snapshot->virtual_time_enabled,
        (unsigned long long)snapshot->virtual_time_now_ms,
        (unsigned long long)snapshot->virtual_time_step_ms,
        (unsigned long long)snapshot->virtual_time_advance_count,
        (unsigned long long)snapshot->virtual_time_advance_total_ms,
        (unsigned long long)snapshot->raw_clock_fallback_count,
        (unsigned long long)snapshot->raw_sleep_fallback_count,
        (unsigned long long)snapshot->raw_sleep_fallback_millis_total
    );
}

static void abi_attrition_json_write_events(FILE* stream, const KainAttritionEvent* events, size_t count) {
    size_t index;
    fputc('[', stream);
    for (index = 0u; index < count; ++index) {
        if (index != 0u) {
            fputc(',', stream);
        }
        fprintf(
            stream,
            "{"
            "\"event_index\":%llu,"
            "\"kind\":%u,"
            "\"aux\":%u,"
            "\"arg0\":%llu,"
            "\"arg1\":%llu,"
            "\"arg2\":%llu"
            "}",
            (unsigned long long)events[index].event_index,
            events[index].kind,
            events[index].aux,
            (unsigned long long)events[index].arg0,
            (unsigned long long)events[index].arg1,
            (unsigned long long)events[index].arg2
        );
    }
    fputc(']', stream);
}

static void abi_attrition_capture_write_report(void) {
    KainAttritionSnapshot final_snapshot;
    KainAttritionEvent events[KAIN_ATTRITION_EVENT_RING_CAPACITY];
    char audit_json[8192];
    size_t event_count;
    size_t audit_length;
    FILE* stream;
    if (!abi_attrition_capture_enabled() || g_abi_attrition_capture_state.report_written != 0) {
        return;
    }
    memset(&final_snapshot, 0, sizeof(final_snapshot));
    memset(events, 0, sizeof(events));
    memset(audit_json, 0, sizeof(audit_json));
    kain_attrition_runtime_snapshot(&final_snapshot);
    audit_length = kain_attrition_runtime_write_audit_json(audit_json, sizeof(audit_json));
    if (audit_length == 0u) {
        abi_copy_text(audit_json, sizeof(audit_json), "{}");
    }
    event_count = kain_attrition_runtime_copy_events(events, KAIN_ATTRITION_EVENT_RING_CAPACITY);
    stream = fopen(g_abi_attrition_capture_state.result_path, "wb");
    if (stream == NULL) {
        return;
    }
    fprintf(stream, "{");
    fprintf(stream, "\"schema_version\":1,");
    fprintf(stream, "\"report_kind\":\"attrition_runtime_capture\",");
    fprintf(stream, "\"case_id\":");
    abi_attrition_json_write_escaped(stream, g_abi_attrition_capture_state.case_id);
    fprintf(stream, ",\"sabotage_mode\":");
    abi_attrition_json_write_escaped(stream, g_abi_attrition_capture_state.sabotage_mode);
    fprintf(stream, ",\"ops\":%llu", (unsigned long long)g_abi_attrition_capture_state.ops);
    fprintf(stream, ",\"seed\":%llu", (unsigned long long)g_abi_attrition_capture_state.baseline_snapshot.seed);
    fprintf(stream, ",\"determinism_tier\":%llu", (unsigned long long)g_abi_attrition_capture_state.baseline_snapshot.determinism_tier);
    fprintf(stream, ",\"virtual_time_enabled\":%llu", (unsigned long long)final_snapshot.virtual_time_enabled);
    fprintf(stream, ",\"expect_failure\":%llu", (unsigned long long)g_abi_attrition_capture_state.expect_failure);
    fprintf(stream, ",\"checksum\":%lld", (long long)g_abi_attrition_capture_state.checksum);
    fprintf(stream, ",\"run_status\":%lld", (long long)g_abi_attrition_capture_state.run_status);
    fprintf(stream, ",\"run_failure\":");
    abi_attrition_json_write_escaped(stream, g_abi_attrition_capture_state.run_failure);
    fprintf(stream, ",\"baseline_snapshot\":");
    abi_attrition_json_write_snapshot(stream, &g_abi_attrition_capture_state.baseline_snapshot);
    fprintf(stream, ",\"final_snapshot\":");
    abi_attrition_json_write_snapshot(stream, &final_snapshot);
    fprintf(stream, ",\"audit\":%s", audit_json);
    fprintf(stream, ",\"events\":");
    abi_attrition_json_write_events(stream, events, event_count);
    fprintf(stream, "}\n");
    fclose(stream);
    g_abi_attrition_capture_state.report_written = 1;
}

static void abi_attrition_capture_configure_from_env(void) {
    KainAttritionSessionConfig session_config;
    const char* result_path = getenv("KAIN_ATTRITION_RESULT_PATH");
    const char* case_id = getenv("KAIN_ATTRITION_CASE_ID");
    const char* sabotage_mode = getenv("KAIN_ATTRITION_SABOTAGE");
    uint64_t enabled = 1u;
    abi_attrition_capture_reset_state();
    if (result_path == NULL || result_path[0] == '\0') {
        return;
    }
    if (abi_parse_env_u64("KAIN_ATTRITION_ENABLED", 1u, &enabled) < 0 || enabled == 0u) {
        return;
    }
    kain_attrition_session_config_init(&session_config);
    session_config.enabled = 1u;
    (void)abi_parse_env_u64("KAIN_ATTRITION_SEED", 1u, &session_config.seed);
    (void)abi_parse_env_u64("KAIN_ATTRITION_VIRTUAL_TIME_ENABLED", 0u, &session_config.virtual_time_enabled);
    (void)abi_parse_env_u64("KAIN_ATTRITION_VIRTUAL_TIME_INITIAL_MS", 0u, &session_config.virtual_time_initial_ms);
    (void)abi_parse_env_u64("KAIN_ATTRITION_VIRTUAL_TIME_STEP_MS", 1u, &session_config.virtual_time_step_ms);
    (void)abi_parse_env_u64("KAIN_ATTRITION_POISON_ON_FREE", 0u, &session_config.poison_on_free);
    (void)abi_parse_env_u64("KAIN_ATTRITION_QUARANTINE_CAPACITY", 0u, &session_config.quarantine_capacity);
    (void)abi_parse_env_u64(
        "KAIN_ATTRITION_FRAGMENTATION_NOISE_MAX_BYTES",
        0u,
        &session_config.fragmentation_noise_max_bytes
    );
    (void)abi_parse_env_u64("KAIN_ATTRITION_ALLOCATION_FAIL_AFTER", 0u, &session_config.allocation_fail_after);
    (void)abi_parse_env_u64(
        "KAIN_ATTRITION_DETERMINISM_TIER",
        (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1,
        &session_config.determinism_tier
    );
    (void)abi_parse_env_u64("KAIN_ATTRITION_OPS", 1u, &g_abi_attrition_capture_state.ops);
    (void)abi_parse_env_u64("KAIN_ATTRITION_EXPECT_FAILURE", 0u, &g_abi_attrition_capture_state.expect_failure);
    if (case_id != NULL && case_id[0] != '\0') {
        abi_copy_text(
            g_abi_attrition_capture_state.case_id,
            sizeof(g_abi_attrition_capture_state.case_id),
            case_id
        );
    }
    if (sabotage_mode != NULL && sabotage_mode[0] != '\0') {
        abi_copy_text(
            g_abi_attrition_capture_state.sabotage_mode,
            sizeof(g_abi_attrition_capture_state.sabotage_mode),
            sabotage_mode
        );
    }
    abi_copy_text(
        g_abi_attrition_capture_state.result_path,
        sizeof(g_abi_attrition_capture_state.result_path),
        result_path
    );
    g_abi_attrition_capture_state.enabled = 1;
    kain_attrition_runtime_configure(&session_config);
    kain_attrition_runtime_reset();
    kain_attrition_runtime_snapshot(&g_abi_attrition_capture_state.baseline_snapshot);
    kain_attrition_runtime_checkpoint("case-start", 0u);
}

int64_t abi_runtime_init(void) {
    /* Actors lazy-init on first spawn or registry touch so pure native
     * programs skip pooled scheduler startup during process bring-up. */
#if KAIN_RUNTIME_INIT_NET_RESET
    abi_net_reset();
#endif
#if KAIN_RUNTIME_INIT_PROCESS_RESET
    abi_process_reset();
#endif
    abi_attrition_capture_configure_from_env();
    return 0;
}

int64_t abi_runtime_shutdown(void) {
#if KAIN_RUNTIME_INIT_NET_RESET
    abi_net_reset();
#endif
#if KAIN_RUNTIME_INIT_PROCESS_RESET
    abi_process_reset();
#endif
#if KAIN_RUNTIME_INIT_ACTOR_SHUTDOWN
    kain_actor_runtime_shutdown();
#endif
    kain_fanout_runtime_shutdown();
    if (abi_attrition_capture_enabled()) {
        kain_attrition_runtime_checkpoint(
            "case-end",
            g_abi_attrition_capture_state.checksum >= 0
                ? (uint64_t)g_abi_attrition_capture_state.checksum
                : 0u
        );
        abi_attrition_capture_write_report();
    }
    return 0;
}

int64_t abi_attrition_checkpoint(const char* label, int64_t subject_id) {
    if (!abi_attrition_capture_enabled()) {
        return 0;
    }
    kain_attrition_runtime_checkpoint(label, subject_id >= 0 ? (uint64_t)subject_id : 0u);
    return 0;
}

int64_t abi_attrition_note_progress(int64_t iteration, int64_t checksum) {
    if (!abi_attrition_capture_enabled()) {
        return 0;
    }
    kain_attrition_runtime_note_progress(
        iteration >= 0 ? (uint64_t)iteration : 0u,
        checksum >= 0 ? (uint64_t)checksum : 0u
    );
    return 0;
}

int64_t abi_attrition_result_set(int64_t checksum, int64_t run_status, const char* run_failure) {
    if (!abi_attrition_capture_enabled()) {
        return 0;
    }
    g_abi_attrition_capture_state.result_set_seen = 1;
    g_abi_attrition_capture_state.checksum = checksum;
    g_abi_attrition_capture_state.run_status = run_status;
    abi_copy_text(
        g_abi_attrition_capture_state.run_failure,
        sizeof(g_abi_attrition_capture_state.run_failure),
        run_failure
    );
    return 0;
}

int64_t abi_runtime_heap_validate(void) {
#ifdef _WIN32
    HANDLE process_heap = GetProcessHeap();
    if (process_heap == NULL) {
        return 0;
    }
    return HeapValidate(process_heap, 0, NULL) ? 1 : 0;
#else
    return 1;
#endif
}

static void* abi_copy_payload_allocation(const void* payload, int64_t payload_size) {
    size_t allocation_size;
    KainNativeTaggedValue* value;

    if (payload_size < 0) {
        return 0;
    }

    allocation_size = sizeof(KainNativeTaggedValue) + (size_t)payload_size;
    value = (KainNativeTaggedValue*)kain_alloc_rc(allocation_size, 4);
    if (value == 0) {
        return 0;
    }
    value->tag = 0;
    value->payload_size = payload_size;
    if (payload != 0 && payload_size > 0) {
        memcpy(value->payload, payload, (size_t)payload_size);
    } else if (payload_size > 0) {
        memset(value->payload, 0, (size_t)payload_size);
    }
    return value;
}

static void* abi_tagged_new(int64_t tag, const void* payload, int64_t payload_size) {
    KainNativeTaggedValue* value = (KainNativeTaggedValue*)abi_copy_payload_allocation(payload, payload_size);
    if (value == 0) {
        return 0;
    }
    value->tag = tag;
    return value;
}

static const KainNativeTaggedValue* abi_as_tagged(const void* value) {
    if (value == 0) {
        return 0;
    }
    return (const KainNativeTaggedValue*)value;
}

void* abi_option_none(void) {
    return abi_tagged_new(ABI_TAG_OPTION_NONE, 0, 0);
}

void* abi_option_some(const void* payload, int64_t payload_size) {
    return abi_tagged_new(ABI_TAG_OPTION_SOME, payload, payload_size);
}

int64_t abi_option_is_some(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged != 0 && tagged->tag == ABI_TAG_OPTION_SOME;
}

int64_t abi_option_is_none(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged == 0 || tagged->tag == ABI_TAG_OPTION_NONE;
}

int64_t abi_tagged_matches(const void* value, int64_t tag) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged != 0 && tagged->tag == tag;
}

int64_t abi_tagged_is_success(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged != 0 &&
        (tagged->tag == ABI_TAG_OPTION_SOME || tagged->tag == ABI_TAG_RESULT_OK);
}

int64_t abi_tagged_payload_copy(const void* value, void* out_payload, int64_t out_payload_size) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    if (tagged == 0 || out_payload == 0 || out_payload_size < 0) {
        return -1;
    }
    if (tagged->payload_size > out_payload_size) {
        return -2;
    }
    if (tagged->payload_size > 0) {
        memcpy(out_payload, tagged->payload, (size_t)tagged->payload_size);
    }
    if (out_payload_size > tagged->payload_size) {
        memset(
            (unsigned char*)out_payload + tagged->payload_size,
            0,
            (size_t)(out_payload_size - tagged->payload_size)
        );
    }
    return tagged->payload_size;
}

int64_t abi_option_payload_copy(const void* value, void* out_payload, int64_t out_payload_size) {
    if (!abi_option_is_some(value)) {
        return -1;
    }
    return abi_tagged_payload_copy(value, out_payload, out_payload_size);
}

void* abi_result_ok(const void* payload, int64_t payload_size) {
    return abi_tagged_new(ABI_TAG_RESULT_OK, payload, payload_size);
}

void* abi_result_err(const void* payload, int64_t payload_size) {
    return abi_tagged_new(ABI_TAG_RESULT_ERR, payload, payload_size);
}

int64_t abi_result_is_ok(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged != 0 && tagged->tag == ABI_TAG_RESULT_OK;
}

int64_t abi_result_is_err(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    return tagged != 0 && tagged->tag == ABI_TAG_RESULT_ERR;
}

int64_t abi_result_payload_copy(const void* value, void* out_payload, int64_t out_payload_size) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    if (tagged == 0 ||
        (tagged->tag != ABI_TAG_RESULT_OK && tagged->tag != ABI_TAG_RESULT_ERR)) {
        return -1;
    }
    return abi_tagged_payload_copy(value, out_payload, out_payload_size);
}

void* abi_result_ok_option(const void* value) {
    const KainNativeTaggedValue* tagged = abi_as_tagged(value);
    if (tagged == 0 || tagged->tag != ABI_TAG_RESULT_OK) {
        return abi_option_none();
    }
    return abi_option_some(tagged->payload, tagged->payload_size);
}

static int abi_future_allocation_size(
    int64_t payload_size,
    size_t* allocation_size
) {
    if (allocation_size == 0 || payload_size < 0) {
        return 0;
    }
    if (abi_size_add_overflows(sizeof(KainNativeFutureValue), (size_t)payload_size)) {
        return 0;
    }
    *allocation_size = sizeof(KainNativeFutureValue) + (size_t)payload_size;
    return 1;
}

static int abi_future_is_inline_ready(const KainNativeFutureValue* future) {
    /* task_id == 0 is the ready-inline sentinel. The payload tail stays within
     * the RC allocation; see z3/proofs-experimental/async-ready-future-inline-payload-bounds.smt2. */
    return future != 0 && future->task_id == KAIN_TASK_ID_INVALID;
}

void* abi_future_ready_from_value(const void* payload, int64_t payload_size) {
    KainNativeFutureValue* future_value;
    size_t allocation_size;

    if (!abi_future_allocation_size(payload_size, &allocation_size)) {
        return 0;
    }

    future_value = (KainNativeFutureValue*)kain_alloc_rc(allocation_size, 4);
    if (future_value == 0) {
        return 0;
    }

    future_value->tag = ABI_TAG_FUTURE;
    future_value->payload_size = payload_size;
    future_value->task_id = KAIN_TASK_ID_INVALID;
    future_value->cached_result = payload_size > 0 ? future_value->inline_payload : 0;
    if (payload != 0 && payload_size > 0) {
        memcpy(future_value->inline_payload, payload, (size_t)payload_size);
    } else if (payload_size > 0) {
        memset(future_value->inline_payload, 0, (size_t)payload_size);
    }
    return future_value;
}

static KainNativeFutureValue* abi_as_future(const void* future_value) {
    KainNativeFutureValue* future = (KainNativeFutureValue*)future_value;
    if (future == 0 || future->tag != ABI_TAG_FUTURE) {
        return 0;
    }
    return future;
}

int64_t abi_future_state(const void* future_value) {
    KainNativeFutureValue* future = abi_as_future(future_value);
    if (future == 0) {
        return -1;
    }
    if (abi_future_is_inline_ready(future)) {
        return (int64_t)KAIN_TASK_STATE_COMPLETED;
    }
    return (int64_t)kain_task_get_state(future->task_id);
}

int64_t abi_future_await_payload_copy(const void* future_value, void* out_payload, int64_t out_payload_size) {
    KainDiagnostic diag = abi_diag();
    KainNativeFutureValue* future = abi_as_future(future_value);
    void* result = 0;

    if (future == 0 || out_payload == 0 || out_payload_size < 0) {
        return -1;
    }
    if (future->payload_size > out_payload_size) {
        return -2;
    }

    if (abi_future_is_inline_ready(future)) {
        result = future->cached_result;
    } else if (future->cached_result != 0) {
        result = future->cached_result;
    } else if (kain_task_await(future->task_id, &result, &diag) != 0) {
        return -3;
    } else {
        future->cached_result = result;
    }

    if (future->payload_size > 0 && result == 0) {
        return -4;
    }
    if (future->payload_size > 0) {
        memcpy(out_payload, result, (size_t)future->payload_size);
    }
    if (out_payload_size > future->payload_size) {
        memset(
            (unsigned char*)out_payload + future->payload_size,
            0,
            (size_t)(out_payload_size - future->payload_size)
        );
    }
    return future->payload_size;
}

void* abi_async_sleep_future(int64_t milliseconds) {
    KainDiagnostic diag = abi_diag();
    KainNativeFutureValue* future_value;
    if (milliseconds < 0) {
        return 0;
    }
    future_value = (KainNativeFutureValue*)kain_alloc_rc(sizeof(KainNativeFutureValue), 4);
    if (future_value == 0) {
        return 0;
    }
    future_value->tag = ABI_TAG_FUTURE;
    future_value->payload_size = 0;
    future_value->cached_result = 0;
    future_value->task_id = kain_async_sleep((unsigned long long)milliseconds, &diag);
    if (future_value->task_id == KAIN_TASK_ID_INVALID) {
        return 0;
    }
    return future_value;
}

int64_t abi_actor_abi_version(void) {
    return (int64_t)KAIN_ACTOR_ABI_VERSION;
}

int64_t abi_actor_invalid_id(void) {
    return (int64_t)KAIN_ACTOR_ID_INVALID;
}

int64_t abi_actor_default_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_DEFAULT_CAPACITY;
}

int64_t abi_actor_unbounded_mailbox_capacity(void) {
    return (int64_t)KAIN_MAILBOX_UNBOUNDED_CAPACITY;
}

int64_t abi_actor_default_ask_timeout_ms(void) {
    return (int64_t)KAIN_ACTOR_DEFAULT_ASK_TIMEOUT_MS;
}

int64_t abi_actor_default_shutdown_grace_ms(void) {
    return (int64_t)KAIN_ACTOR_DEFAULT_SHUTDOWN_GRACE_MS;
}

int64_t abi_actor_supervision_max_restarts(void) {
    return (int64_t)KAIN_SUPERVISION_MAX_RESTARTS;
}

int64_t abi_actor_supervision_restart_window_millis(void) {
    return (int64_t)KAIN_SUPERVISION_RESTART_WINDOW_MILLIS;
}

static unsigned long long abi_hash_message_name(const char* value) {
    unsigned long long hash = 1469598103934665603ULL;
    if (value == 0) {
        return hash;
    }
    while (*value != '\0') {
        hash ^= (unsigned char)(*value);
        hash *= 1099511628211ULL;
        value++;
    }
    return hash;
}

static void abi_copy_actor_name(char* destination, size_t destination_size, const char* source) {
    size_t index = 0;
    if (destination == 0 || destination_size == 0) {
        return;
    }
    if (source != 0) {
        while (source[index] != '\0' && index + 1 < destination_size) {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static KainActorExitReason abi_actor_default_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)user_data;

    for (;;) {
        KainActorMessage message;
        KainDiagnostic diag = abi_diag();
        if (kain_actor_receive(mailbox, &message, &diag) != 0) {
            return KAIN_ACTOR_EXIT_NORMAL;
        }
        if (message.data != 0) {
            free(message.data);
        }
    }
}

int64_t abi_actor_spawn(const char* actor_name, const char* init_payload) {
    KainDiagnostic diag = abi_diag();
    KainActorSpawnConfig config;
    (void)init_payload;

    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = abi_actor_default_bootstrap;
    config.user_data = 0;
    config.mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    abi_copy_actor_name(config.name, sizeof(config.name), actor_name);

    return (int64_t)kain_actor_spawn(&config, &diag);
}

int64_t abi_actor_send(int64_t actor_id, const char* message_name, const char* data_payload) {
    KainDiagnostic diag = abi_diag();
    KainActorMessage message;
    message.type_tag = abi_hash_message_name(message_name);
    message.data = (void*)data_payload;
    message.data_size = data_payload == 0 ? 0 : strlen(data_payload) + 1;
    message.sender_id = KAIN_ACTOR_ID_INVALID;
    return (int64_t)kain_actor_send((KainActorId)actor_id, &message, &diag);
}

int abi_actor_state_invalid(int64_t actor_id) {
    return actor_id <= 0 || kain_actor_get_state((KainActorId)actor_id) == KAIN_ACTOR_STATE_UNINITIALIZED;
}

int64_t abi_actor_get_state(int64_t actor_id) {
    return (int64_t)kain_actor_get_state((KainActorId)actor_id);
}

int64_t abi_actor_shutdown(int64_t actor_id) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_shutdown((KainActorId)actor_id, &diag);
}

int64_t abi_actor_kill(int64_t actor_id) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_kill((KainActorId)actor_id, &diag);
}

int64_t abi_actor_registry_lookup(const char* name) {
    if (name == 0 || name[0] == '\0') {
        return (int64_t)KAIN_ACTOR_ID_INVALID;
    }
    return (int64_t)kain_actor_registry_lookup(name);
}

int64_t abi_actor_registry_register(const char* name, int64_t actor_id) {
    KainDiagnostic diag = abi_diag();
    if (name == 0 || name[0] == '\0' || actor_id <= 0) {
        return -1;
    }
    return (int64_t)kain_actor_registry_register(name, (KainActorId)actor_id, &diag);
}

int64_t abi_actor_registry_unregister(const char* name) {
    KainDiagnostic diag = abi_diag();
    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    return (int64_t)kain_actor_registry_unregister(name, &diag);
}

int64_t abi_actor_monitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_monitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t abi_actor_demonitor(int64_t monitor_id, int64_t monitored_id) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_demonitor((KainActorId)monitor_id, (KainActorId)monitored_id, &diag);
}

int64_t abi_actor_link(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_link((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

int64_t abi_actor_unlink(int64_t actor_a, int64_t actor_b) {
    KainDiagnostic diag = abi_diag();
    return (int64_t)kain_actor_unlink((KainActorId)actor_a, (KainActorId)actor_b, &diag);
}

static int abi_actor_supervision_snapshot(
    int64_t actor_id,
    KainActorSupervisionSnapshot* snapshot
) {
    KainDiagnostic diag = abi_diag();
    if (snapshot == 0) {
        return -1;
    }
    return kain_actor_get_supervision_snapshot((KainActorId)actor_id, snapshot, &diag);
}

int64_t abi_actor_supervision_observed_child_exit_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (abi_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.observed_child_exit_count;
}

int64_t abi_actor_supervision_restart_attempt_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (abi_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.restart_attempt_count;
}

int64_t abi_actor_supervision_escalation_count(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (abi_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return -1;
    }
    return (int64_t)snapshot.escalation_count;
}

int abi_actor_supervision_limit_hit(int64_t actor_id) {
    KainActorSupervisionSnapshot snapshot;
    if (abi_actor_supervision_snapshot(actor_id, &snapshot) != 0) {
        return 0;
    }
    return snapshot.restart_limit_hit != 0 || snapshot.supervision_limit_hits != 0;
}

static KainActorSchedulerSnapshot abi_actor_scheduler_snapshot(void) {
    KainActorSchedulerSnapshot snapshot;
    kain_actor_scheduler_snapshot(&snapshot);
    return snapshot;
}

int64_t abi_actor_scheduler_queue_depth(void) {
    return (int64_t)abi_actor_scheduler_snapshot().queue_depth;
}

int64_t abi_actor_scheduler_max_queue_depth(void) {
    return (int64_t)abi_actor_scheduler_snapshot().max_queue_depth;
}

int64_t abi_actor_scheduler_total_enqueued(void) {
    return (int64_t)abi_actor_scheduler_snapshot().total_enqueued;
}

int64_t abi_actor_scheduler_total_dequeued(void) {
    return (int64_t)abi_actor_scheduler_snapshot().total_dequeued;
}

int64_t abi_actor_scheduler_worker_count(void) {
    return (int64_t)abi_actor_scheduler_snapshot().worker_count;
}

int64_t abi_actor_scheduler_active_workers(void) {
    return (int64_t)abi_actor_scheduler_snapshot().active_workers;
}

int64_t abi_actor_scheduler_busy_workers(void) {
    return (int64_t)abi_actor_scheduler_snapshot().busy_workers;
}

int64_t abi_actor_scheduler_overflow_thread_spawns(void) {
    return (int64_t)abi_actor_scheduler_snapshot().overflow_thread_spawns;
}

int64_t abi_entangle_reset(void) {
    entangle_registry_reset();
    return 0;
}

int64_t abi_entangle_registered_count(void) {
    return (int64_t)entangle_registry_count();
}

int64_t abi_entangle_register(
    const char* authority,
    const char* mirror,
    const char* policy,
    const char* type_name
) {
    return (int64_t)entangle_registry_register(authority, mirror, policy, type_name);
}

static const KainRuntimeEntangleBinding* abi_entangle_binding_at(int64_t index) {
    static KainRuntimeEntangleBinding binding;
    if (index < 0) {
        return 0;
    }
    if (entangle_registry_get((size_t)index, &binding) != 0) {
        return 0;
    }
    return &binding;
}

const char* abi_entangle_get_authority(int64_t index) {
    const KainRuntimeEntangleBinding* binding = abi_entangle_binding_at(index);
    return binding ? binding->authority : "";
}

const char* abi_entangle_get_mirror(int64_t index) {
    const KainRuntimeEntangleBinding* binding = abi_entangle_binding_at(index);
    return binding ? binding->mirror : "";
}

const char* abi_entangle_get_policy(int64_t index) {
    const KainRuntimeEntangleBinding* binding = abi_entangle_binding_at(index);
    return binding ? binding->policy : "";
}

const char* abi_entangle_get_type_name(int64_t index) {
    const KainRuntimeEntangleBinding* binding = abi_entangle_binding_at(index);
    return binding ? binding->type_name : "";
}

#define ABI_PATCH_JOURNAL_MAX 256

typedef struct KainNativePatchJournalEntry {
    char patch_name[128];
    char path[256];
    int64_t old_value;
    int64_t new_value;
    int active;
    int committed;
    int undone;
} KainNativePatchJournalEntry;

static KainNativePatchJournalEntry g_kain_native_patch_journal[ABI_PATCH_JOURNAL_MAX];
static int64_t g_kain_native_patch_journal_count = 0;
static char g_kain_native_active_patch[128] = "";
static int64_t g_kain_native_entangle_propagation_count = 0;
static char g_kain_native_entangle_last_authority[256] = "";
static char g_kain_native_entangle_last_mirror[256] = "";
static int64_t g_kain_native_converge_mismatch_count = 0;
static int64_t g_kain_native_orchestrate_stage_count = 0;

int64_t abi_patch_begin(const char* patch_name) {
    abi_copy_text(g_kain_native_active_patch, sizeof(g_kain_native_active_patch), patch_name);
    return 0;
}

int64_t abi_patch_record_i64(const char* patch_name, const char* path, int64_t old_value, int64_t new_value) {
    KainNativePatchJournalEntry* entry;
    if (g_kain_native_patch_journal_count >= ABI_PATCH_JOURNAL_MAX) {
        return -3;
    }
    entry = &g_kain_native_patch_journal[g_kain_native_patch_journal_count];
    memset(entry, 0, sizeof(*entry));
    abi_copy_text(entry->patch_name, sizeof(entry->patch_name), patch_name);
    abi_copy_text(entry->path, sizeof(entry->path), path);
    entry->old_value = old_value;
    entry->new_value = new_value;
    entry->active = 1;
    entry->committed = 0;
    entry->undone = 0;
    g_kain_native_patch_journal_count += 1;
    return 0;
}

int64_t abi_patch_commit(const char* patch_name) {
    int64_t committed = 0;
    int64_t index;
    for (index = 0; index < g_kain_native_patch_journal_count; index++) {
        KainNativePatchJournalEntry* entry = &g_kain_native_patch_journal[index];
        if (entry->active && !entry->committed &&
            (patch_name == 0 || strcmp(entry->patch_name, patch_name) == 0)) {
            entry->committed = 1;
            committed += 1;
        }
    }
    g_kain_native_active_patch[0] = '\0';
    return committed;
}

int64_t abi_patch_undo_last(void) {
    int64_t index;
    for (index = g_kain_native_patch_journal_count - 1; index >= 0; index--) {
        KainNativePatchJournalEntry* entry = &g_kain_native_patch_journal[index];
        if (entry->committed && !entry->undone) {
            entry->undone = 1;
            return entry->old_value;
        }
    }
    return 0;
}

int64_t abi_patch_journal_count(void) {
    return g_kain_native_patch_journal_count;
}

const char* abi_patch_last_path(void) {
    if (g_kain_native_patch_journal_count <= 0) {
        return string_new("");
    }
    return string_new(g_kain_native_patch_journal[g_kain_native_patch_journal_count - 1].path);
}

int64_t abi_entangle_record_i64(const char* authority, const char* mirror, int64_t value) {
    (void)value;
    abi_copy_text(
        g_kain_native_entangle_last_authority,
        sizeof(g_kain_native_entangle_last_authority),
        authority
    );
    abi_copy_text(
        g_kain_native_entangle_last_mirror,
        sizeof(g_kain_native_entangle_last_mirror),
        mirror
    );
    g_kain_native_entangle_propagation_count += 1;
    return g_kain_native_entangle_propagation_count;
}

int64_t abi_entangle_propagation_count(void) {
    return g_kain_native_entangle_propagation_count;
}

const char* abi_entangle_last_authority(void) {
    return string_new(g_kain_native_entangle_last_authority);
}

const char* abi_entangle_last_mirror(void) {
    return string_new(g_kain_native_entangle_last_mirror);
}

int64_t abi_converge_record_i64(const char* converge_name, const char* lane_name, int64_t spec_value, int64_t fast_value) {
    (void)converge_name;
    (void)lane_name;
    if (spec_value != fast_value) {
        g_kain_native_converge_mismatch_count += 1;
        return 1;
    }
    return 0;
}

int64_t abi_converge_record_bool(const char* converge_name, const char* lane_name, int fast_matches) {
    (void)converge_name;
    (void)lane_name;
    if (!fast_matches) {
        g_kain_native_converge_mismatch_count += 1;
        return 1;
    }
    return 0;
}

int64_t abi_converge_mismatch_count(void) {
    return g_kain_native_converge_mismatch_count;
}

int64_t abi_orchestrate_stage_begin(const char* runtime_name, const char* function_name) {
    (void)runtime_name;
    (void)function_name;
    g_kain_native_orchestrate_stage_count += 1;
    return g_kain_native_orchestrate_stage_count;
}

int64_t abi_orchestrate_stage_end_i64(const char* runtime_name, const char* function_name, int64_t status) {
    (void)runtime_name;
    (void)function_name;
    return status;
}

int64_t abi_orchestrate_stage_count(void) {
    return g_kain_native_orchestrate_stage_count;
}

int64_t abi_now_millis(void) {
    return (int64_t)kain_attrition_now_millis();
}

int64_t abi_sleep_millis(int64_t milliseconds) {
    if (milliseconds < 0) {
        return -1;
    }
    kain_attrition_sleep_for_millis((unsigned long long)milliseconds);
    return 0;
}

static int64_t g_kain_native_fs_last_status = 0;
static char g_kain_native_fs_last_error_kind[64] = "ok";
static char g_kain_native_fs_last_error_message[512] = "";

static char* abi_fs_string_with_len(const char* source, size_t length) {
    char* result = (char*)kain_alloc_rc(length + 1, 1);
    if (result == 0) {
        return 0;
    }
    if (source != 0 && length > 0) {
        memcpy(result, source, length);
    }
    result[length] = '\0';
    if (source != 0) {
        kain_rc_set_string_length(result, kain_bounded_text_length(source, length));
    }
    return result;
}

static void abi_fs_copy_message(char* destination, size_t destination_size, const char* source) {
    if (destination == 0 || destination_size == 0) {
        return;
    }
    if (source == 0) {
        source = "";
    }
#ifdef _WIN32
    strncpy_s(destination, destination_size, source, _TRUNCATE);
#else
    strncpy(destination, source, destination_size - 1);
    destination[destination_size - 1] = '\0';
#endif
}

static const char* abi_fs_errno_kind(int error_code) {
    switch (error_code) {
        case 0:
            return "ok";
        case ENOENT:
            return "not_found";
        case EACCES:
            return "access_denied";
        case EEXIST:
            return "already_exists";
        case EINVAL:
            return "invalid_input";
#ifdef ENOTDIR
        case ENOTDIR:
            return "not_a_directory";
#endif
#ifdef EISDIR
        case EISDIR:
            return "is_directory";
#endif
#ifdef ENOTEMPTY
        case ENOTEMPTY:
            return "directory_not_empty";
#endif
#ifdef EXDEV
        case EXDEV:
            return "cross_device";
#endif
        default:
            return "other";
    }
}

static int64_t abi_fs_fail(const char* operation, const char* path) {
    int error_code = errno;
    char message[512];
    const char* kind = abi_fs_errno_kind(error_code);
    g_kain_native_fs_last_status = error_code == 0 ? -1 : -(int64_t)error_code;
    abi_fs_copy_message(g_kain_native_fs_last_error_kind, sizeof(g_kain_native_fs_last_error_kind), kind);
#ifdef _WIN32
    strerror_s(message, sizeof(message), error_code);
#else
    abi_fs_copy_message(message, sizeof(message), strerror(error_code));
#endif
    if (operation == 0) {
        operation = "fs";
    }
    if (path == 0) {
        path = "";
    }
    {
        char detail[512];
#ifdef _WIN32
        _snprintf_s(detail, sizeof(detail), _TRUNCATE, "%s failed for '%s': %s", operation, path, message);
#else
        snprintf(detail, sizeof(detail), "%s failed for '%s': %s", operation, path, message);
#endif
        abi_fs_copy_message(g_kain_native_fs_last_error_message, sizeof(g_kain_native_fs_last_error_message), detail);
    }
    return g_kain_native_fs_last_status;
}

static int64_t abi_fs_ok(void) {
    g_kain_native_fs_last_status = 0;
    abi_fs_copy_message(g_kain_native_fs_last_error_kind, sizeof(g_kain_native_fs_last_error_kind), "ok");
    abi_fs_copy_message(g_kain_native_fs_last_error_message, sizeof(g_kain_native_fs_last_error_message), "");
    return 0;
}

typedef struct KainNativeFsTextBuilder {
    char* data;
    size_t length;
    size_t capacity;
} KainNativeFsTextBuilder;

static int abi_fs_builder_init(KainNativeFsTextBuilder* builder) {
    builder->capacity = 1024;
    builder->length = 0;
    builder->data = (char*)malloc(builder->capacity);
    if (builder->data == 0) {
        errno = ENOMEM;
        return -1;
    }
    builder->data[0] = '\0';
    return 0;
}

static void abi_fs_builder_free(KainNativeFsTextBuilder* builder) {
    if (builder->data != 0) {
        free(builder->data);
    }
    builder->data = 0;
    builder->length = 0;
    builder->capacity = 0;
}

#ifdef _WIN32
static long long abi_fs_filetime_to_unix_seconds(FILETIME value) {
    ULARGE_INTEGER ticks;
    const unsigned long long windows_epoch_delta_seconds = 11644473600ULL;

    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    if (ticks.QuadPart == 0ULL) {
        return 0;
    }
    return (long long)((ticks.QuadPart / 10000000ULL) - windows_epoch_delta_seconds);
}
#endif

static int abi_fs_builder_reserve(KainNativeFsTextBuilder* builder, size_t additional) {
    size_t required_length;
    size_t needed;
    size_t next_capacity;
    char* grown;
    if (builder == 0) {
        errno = EINVAL;
        return -1;
    }
    if (abi_size_add_overflows(builder->length, additional)) {
        errno = EOVERFLOW;
        return -1;
    }
    required_length = builder->length + additional;
    if (abi_size_add_overflows(required_length, 1u)) {
        errno = EOVERFLOW;
        return -1;
    }
    needed = required_length + 1u;
    if (needed <= builder->capacity) {
        return 0;
    }
    next_capacity = builder->capacity ? builder->capacity : 1024u;
    while (next_capacity < needed) {
        if (next_capacity > (SIZE_MAX / 2u)) {
            next_capacity = needed;
            break;
        }
        next_capacity *= 2u;
    }
    grown = (char*)realloc(builder->data, next_capacity);
    if (grown == 0) {
        errno = ENOMEM;
        return -1;
    }
    builder->data = grown;
    builder->capacity = next_capacity;
    return 0;
}

static int abi_fs_builder_append(KainNativeFsTextBuilder* builder, const char* text) {
    size_t length;
    if (text == 0) {
        text = "";
    }
    length = strlen(text);
    if (abi_fs_builder_reserve(builder, length) != 0) {
        return -1;
    }
    memcpy(builder->data + builder->length, text, length + 1);
    builder->length += length;
    return 0;
}

static int abi_fs_builder_appendf(KainNativeFsTextBuilder* builder, const char* format, ...) {
    char stack_buffer[1024];
    va_list args;
    int written;
    va_start(args, format);
#ifdef _WIN32
    written = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args);
#else
    written = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args);
#endif
    va_end(args);
    if (written < 0) {
        errno = EINVAL;
        return -1;
    }
    if ((size_t)written < sizeof(stack_buffer)) {
        return abi_fs_builder_append(builder, stack_buffer);
    }
    if (abi_fs_builder_reserve(builder, (size_t)written) != 0) {
        return -1;
    }
    va_start(args, format);
    vsnprintf(builder->data + builder->length, (size_t)written + 1, format, args);
    va_end(args);
    builder->length += (size_t)written;
    return 0;
}

static const char* abi_fs_builder_finish(KainNativeFsTextBuilder* builder) {
    const char* result = abi_fs_string_with_len(builder->data, builder->length);
    abi_fs_builder_free(builder);
    return result != 0 ? result : string_new("");
}

static int abi_fs_path_is_absolute(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    return strlen(path) > 2 && path[1] == ':';
}

static int64_t abi_fs_create_one_dir(const char* path) {
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    errno = EACCES;
    return -1;
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
#endif
}

static int64_t abi_fs_create_parent_dirs(const char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(buffer, path, length + 1);
    for (index = 1; index < length; index++) {
        if (buffer[index] == '/' || buffer[index] == '\\') {
            char saved = buffer[index];
            if (index == 2 && buffer[1] == ':') {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && abi_fs_create_one_dir(buffer) != 0) {
                buffer[index] = saved;
                return -1;
            }
            buffer[index] = saved;
        }
    }
    return 0;
}

static int abi_fs_open_write_retry_parent_dirs(const char* path, const char* mode, FILE** out_file) {
    if (out_file == 0) {
        errno = EINVAL;
        return -1;
    }
    *out_file = 0;
#ifdef _WIN32
    if (fopen_s(out_file, path, mode) == 0 && *out_file != 0) {
        return 0;
    }
#else
    *out_file = fopen(path, mode);
    if (*out_file != 0) {
        return 0;
    }
#endif
    if (abi_fs_create_parent_dirs(path) != 0) {
        return -1;
    }
#ifdef _WIN32
    if (fopen_s(out_file, path, mode) == 0 && *out_file != 0) {
        return 0;
    }
#else
    *out_file = fopen(path, mode);
    if (*out_file != 0) {
        return 0;
    }
#endif
    return -1;
}

static int64_t abi_fs_write_mode_len(const char* path, const char* content, size_t length, const char* mode) {
    FILE* file = 0;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return abi_fs_fail("write_text", path);
    }
    if (abi_fs_open_write_retry_parent_dirs(path, mode, &file) != 0 || file == 0) {
        return abi_fs_fail("write_text", path);
    }
    if (content == 0) {
        content = "";
        length = 0;
    }
    if (length > 0 && fwrite(content, 1, length, file) != length) {
        fclose(file);
        return abi_fs_fail("write_text", path);
    }
    fclose(file);
    return abi_fs_ok();
}

static int64_t abi_fs_write_mode(const char* path, const char* content, const char* mode) {
    if (content == 0) {
        return abi_fs_write_mode_len(path, "", 0u, mode);
    }
    return abi_fs_write_mode_len(path, content, strlen(content), mode);
}

const char* abi_fs_read_text(const char* path) {
    FILE* file = 0;
    long size;
    char* buffer;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        abi_fs_fail("read_text", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) {
        file = 0;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        abi_fs_fail("read_text", path);
        return string_new("");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        abi_fs_fail("read_text", path);
        return string_new("");
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        abi_fs_fail("read_text", path);
        return string_new("");
    }
    rewind(file);
    buffer = abi_fs_string_with_len(0, (size_t)size);
    if (buffer == 0) {
        fclose(file);
        errno = ENOMEM;
        abi_fs_fail("read_text", path);
        return string_new("");
    }
    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        abi_fs_fail("read_text", path);
        return string_new("");
    }
    kain_rc_set_string_length(buffer, (size_t)size);
    fclose(file);
    abi_fs_ok();
    return buffer;
}

const char* abi_fs_read_text_range(const char* path, int64_t offset, int64_t length) {
    FILE* file = 0;
    char* buffer;
    size_t read_count;
    if (path == 0 || path[0] == '\0' || offset < 0 || length < 0) {
        errno = EINVAL;
        abi_fs_fail("read_text_range", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        abi_fs_fail("read_text_range", path);
        return string_new("");
    }
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        fclose(file);
        abi_fs_fail("read_text_range", path);
        return string_new("");
    }
    buffer = abi_fs_string_with_len(0, (size_t)length);
    if (buffer == 0) {
        fclose(file);
        errno = ENOMEM;
        abi_fs_fail("read_text_range", path);
        return string_new("");
    }
    read_count = fread(buffer, 1, (size_t)length, file);
    buffer[read_count] = '\0';
    kain_rc_set_string_length(buffer, read_count);
    fclose(file);
    abi_fs_ok();
    return buffer;
}

int64_t abi_fs_write_text(const char* path, const char* content) {
    return abi_fs_write_mode(path, content, "wb");
}

int64_t abi_fs_write_text_len(const char* path, const char* content, int64_t content_length) {
    if (content_length < 0) {
        errno = EINVAL;
        return abi_fs_fail("write_text", path);
    }
    return abi_fs_write_mode_len(path, content, (size_t)content_length, "wb");
}

int64_t abi_fs_append_text(const char* path, const char* content) {
    return abi_fs_write_mode(path, content, "ab");
}

int64_t abi_fs_append_text_len(const char* path, const char* content, int64_t content_length) {
    if (content_length < 0) {
        errno = EINVAL;
        return abi_fs_fail("append_text", path);
    }
    return abi_fs_write_mode_len(path, content, (size_t)content_length, "ab");
}

static int abi_fs_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

const char* abi_fs_read_byte_range_hex(const char* path, int64_t offset, int64_t length) {
    FILE* file = 0;
    unsigned char buffer[4096];
    KainNativeFsTextBuilder builder;
    int64_t remaining = length;
    static const char* digits = "0123456789abcdef";
    if (path == 0 || path[0] == '\0' || offset < 0 || length < 0) {
        errno = EINVAL;
        abi_fs_fail("read_byte_range_hex", path);
        return string_new("");
    }
    if (abi_fs_builder_init(&builder) != 0) {
        abi_fs_fail("read_byte_range_hex", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        abi_fs_builder_free(&builder);
        abi_fs_fail("read_byte_range_hex", path);
        return string_new("");
    }
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        fclose(file);
        abi_fs_builder_free(&builder);
        abi_fs_fail("read_byte_range_hex", path);
        return string_new("");
    }
    while (remaining > 0) {
        size_t want = remaining > (int64_t)sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        size_t read_count = fread(buffer, 1, want, file);
        size_t index;
        if (read_count == 0) {
            break;
        }
        if (abi_fs_builder_reserve(&builder, read_count * 2) != 0) {
            fclose(file);
            abi_fs_builder_free(&builder);
            abi_fs_fail("read_byte_range_hex", path);
            return string_new("");
        }
        for (index = 0; index < read_count; index++) {
            builder.data[builder.length++] = digits[(buffer[index] >> 4) & 0xF];
            builder.data[builder.length++] = digits[buffer[index] & 0xF];
        }
        builder.data[builder.length] = '\0';
        remaining -= (int64_t)read_count;
    }
    fclose(file);
    abi_fs_ok();
    return abi_fs_builder_finish(&builder);
}

const char* abi_fs_read_bytes_hex(const char* path) {
    long size;
    FILE* file = 0;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        abi_fs_fail("read_bytes_hex", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        abi_fs_fail("read_bytes_hex", path);
        return string_new("");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        abi_fs_fail("read_bytes_hex", path);
        return string_new("");
    }
    size = ftell(file);
    fclose(file);
    if (size < 0) {
        abi_fs_fail("read_bytes_hex", path);
        return string_new("");
    }
    return abi_fs_read_byte_range_hex(path, 0, (int64_t)size);
}

int64_t abi_fs_write_bytes_hex(const char* path, const char* hex) {
    FILE* file = 0;
    size_t length;
    size_t index;
    if (path == 0 || path[0] == '\0' || hex == 0) {
        errno = EINVAL;
        return abi_fs_fail("write_bytes_hex", path);
    }
    length = strlen(hex);
    if ((length % 2) != 0 || abi_fs_create_parent_dirs(path) != 0) {
        errno = EINVAL;
        return abi_fs_fail("write_bytes_hex", path);
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "wb") != 0) file = 0;
#else
    file = fopen(path, "wb");
#endif
    if (file == 0) {
        return abi_fs_fail("write_bytes_hex", path);
    }
    for (index = 0; index < length; index += 2) {
        int hi = abi_fs_hex_value(hex[index]);
        int lo = abi_fs_hex_value(hex[index + 1]);
        unsigned char byte;
        if (hi < 0 || lo < 0) {
            fclose(file);
            errno = EINVAL;
            return abi_fs_fail("write_bytes_hex", path);
        }
        byte = (unsigned char)((hi << 4) | lo);
        if (fwrite(&byte, 1, 1, file) != 1) {
            fclose(file);
            return abi_fs_fail("write_bytes_hex", path);
        }
    }
    fclose(file);
    return abi_fs_ok();
}

int abi_fs_exists(const char* path) {
    if (path == 0 || path[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

int abi_fs_is_file(const char* path) {
#ifdef _WIN32
    DWORD attrs;
    if (path == 0 || path[0] == '\0') return 0;
    attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    if (path == 0 || stat(path, &info) != 0) return 0;
    return S_ISREG(info.st_mode);
#endif
}

int abi_fs_is_dir(const char* path) {
#ifdef _WIN32
    DWORD attrs;
    if (path == 0 || path[0] == '\0') return 0;
    attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    if (path == 0 || stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
#endif
}

#ifdef _WIN32
static const char* abi_fs_file_type_text(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return "directory";
    }
    return "file";
}
#else
static const char* abi_fs_file_type_text(const char* path, const struct stat* info) {
    (void)path;
    if (S_ISDIR(info->st_mode)) return "directory";
    if (S_ISREG(info->st_mode)) return "file";
#ifdef S_ISLNK
    if (S_ISLNK(info->st_mode)) return "symlink";
#endif
    return "other";
}
#endif

const char* abi_fs_metadata_text(const char* path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA file_info;
    ULARGE_INTEGER file_size;
    long long modified_seconds;
#else
    struct stat info;
#endif
    KainNativeFsTextBuilder builder;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        abi_fs_fail("metadata_text", path);
        return string_new("");
    }
#ifdef _WIN32
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &file_info) == 0) {
        abi_fs_fail("metadata_text", path);
        return string_new("");
    }
    file_size.LowPart = file_info.nFileSizeLow;
    file_size.HighPart = file_info.nFileSizeHigh;
    modified_seconds =
        abi_fs_filetime_to_unix_seconds(file_info.ftLastWriteTime);
#else
    if (stat(path, &info) != 0) {
        abi_fs_fail("metadata_text", path);
        return string_new("");
    }
#endif
    if (abi_fs_builder_init(&builder) != 0) {
        abi_fs_fail("metadata_text", path);
        return string_new("");
    }
#ifdef _WIN32
    if (abi_fs_builder_appendf(&builder, "file_type=%s\n", abi_fs_file_type_text(path)) != 0
        || abi_fs_builder_appendf(&builder, "len=%lld\n", (long long)file_size.QuadPart) != 0
        || abi_fs_builder_appendf(&builder, "readonly=%d\n", (file_info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 1 : 0) != 0
        || abi_fs_builder_appendf(&builder, "modified_seconds=%lld\n", modified_seconds) != 0) {
#else
    if (abi_fs_builder_appendf(&builder, "file_type=%s\n", abi_fs_file_type_text(path, &info)) != 0
        || abi_fs_builder_appendf(&builder, "len=%lld\n", (long long)info.st_size) != 0
        || abi_fs_builder_appendf(&builder, "readonly=%d\n", (info.st_mode & S_IWUSR) ? 0 : 1) != 0
        || abi_fs_builder_appendf(&builder, "modified_seconds=%lld\n", (long long)info.st_mtime) != 0) {
#endif
        abi_fs_builder_free(&builder);
        abi_fs_fail("metadata_text", path);
        return string_new("");
    }
    abi_fs_ok();
    return abi_fs_builder_finish(&builder);
}

#ifdef _WIN32
static int abi_fs_read_dir_append(KainNativeFsTextBuilder* builder, const char* path) {
    char pattern[4096];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (_snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*", path) < 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = ENOENT;
        return -1;
    }
    do {
        char child[4096];
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        if (_snprintf_s(child, sizeof(child), _TRUNCATE, "%s\\%s", path, data.cFileName) < 0) {
            FindClose(handle);
            errno = ENAMETOOLONG;
            return -1;
        }
        if (abi_fs_builder_append(builder, child) != 0 || abi_fs_builder_append(builder, "\n") != 0) {
            FindClose(handle);
            return -1;
        }
    } while (FindNextFileA(handle, &data) != 0);
    FindClose(handle);
    return 0;
}

static int abi_fs_walk_append(KainNativeFsTextBuilder* builder, const char* path) {
    char pattern[4096];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (_snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*", path) < 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = ENOENT;
        return -1;
    }
    do {
        char child[4096];
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
            continue;
        }
        if (_snprintf_s(child, sizeof(child), _TRUNCATE, "%s\\%s", path, data.cFileName) < 0) {
            FindClose(handle);
            errno = ENAMETOOLONG;
            return -1;
        }
        if (abi_fs_builder_append(builder, child) != 0 || abi_fs_builder_append(builder, "\n") != 0) {
            FindClose(handle);
            return -1;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (abi_fs_walk_append(builder, child) != 0) {
                FindClose(handle);
                return -1;
            }
        }
    } while (FindNextFileA(handle, &data) != 0);
    FindClose(handle);
    return 0;
}
#else
static int abi_fs_read_dir_append(KainNativeFsTextBuilder* builder, const char* path) {
    DIR* dir = opendir(path);
    struct dirent* entry;
    if (dir == 0) {
        return -1;
    }
    while ((entry = readdir(dir)) != 0) {
        char child[4096];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0) {
            closedir(dir);
            errno = ENAMETOOLONG;
            return -1;
        }
        if (abi_fs_builder_append(builder, child) != 0 || abi_fs_builder_append(builder, "\n") != 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return 0;
}

static int abi_fs_walk_append(KainNativeFsTextBuilder* builder, const char* path) {
    DIR* dir = opendir(path);
    struct dirent* entry;
    if (dir == 0) {
        return -1;
    }
    while ((entry = readdir(dir)) != 0) {
        char child[4096];
        struct stat child_info;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0) {
            closedir(dir);
            errno = ENAMETOOLONG;
            return -1;
        }
        if (abi_fs_builder_append(builder, child) != 0 || abi_fs_builder_append(builder, "\n") != 0) {
            closedir(dir);
            return -1;
        }
        if (stat(child, &child_info) == 0 && S_ISDIR(child_info.st_mode)) {
            if (abi_fs_walk_append(builder, child) != 0) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return 0;
}
#endif

const char* abi_fs_read_dir_paths_text(const char* path) {
    KainNativeFsTextBuilder builder;
    if (abi_fs_builder_init(&builder) != 0) {
        abi_fs_fail("read_dir_paths_text", path);
        return string_new("");
    }
    if (path == 0 || path[0] == '\0' || abi_fs_read_dir_append(&builder, path) != 0) {
        abi_fs_builder_free(&builder);
        abi_fs_fail("read_dir_paths_text", path);
        return string_new("");
    }
    abi_fs_ok();
    return abi_fs_builder_finish(&builder);
}

const char* abi_fs_walk_paths_text(const char* path) {
    KainNativeFsTextBuilder builder;
    if (abi_fs_builder_init(&builder) != 0) {
        abi_fs_fail("walk_paths_text", path);
        return string_new("");
    }
    if (path == 0 || path[0] == '\0' || abi_fs_walk_append(&builder, path) != 0) {
        abi_fs_builder_free(&builder);
        abi_fs_fail("walk_paths_text", path);
        return string_new("");
    }
    abi_fs_ok();
    return abi_fs_builder_finish(&builder);
}

int64_t abi_fs_create_dir_all(const char* path) {
    char buffer[4096];
    size_t length;
    size_t index;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return abi_fs_fail("create_dir_all", path);
    }
    length = strlen(path);
    if (length >= sizeof(buffer)) {
        errno = ENAMETOOLONG;
        return abi_fs_fail("create_dir_all", path);
    }
    memcpy(buffer, path, length + 1);
    for (index = 1; index <= length; index++) {
        if (buffer[index] == '/' || buffer[index] == '\\' || buffer[index] == '\0') {
            char saved = buffer[index];
            if (index == 2 && buffer[1] == ':') {
                continue;
            }
            buffer[index] = '\0';
            if (buffer[0] != '\0' && !abi_fs_path_is_absolute(buffer)) {
                if (abi_fs_create_one_dir(buffer) != 0) {
                    buffer[index] = saved;
                    return abi_fs_fail("create_dir_all", buffer);
                }
            } else if (strlen(buffer) > 3) {
                if (abi_fs_create_one_dir(buffer) != 0) {
                    buffer[index] = saved;
                    return abi_fs_fail("create_dir_all", buffer);
                }
            }
            buffer[index] = saved;
        }
    }
    return abi_fs_ok();
}

int64_t abi_fs_copy_file(const char* src, const char* dest) {
    FILE* input = 0;
    FILE* output = 0;
    char buffer[65536];
    size_t read_count;
    if (src == 0 || dest == 0) {
        errno = EINVAL;
        return abi_fs_fail("copy_file", src ? src : dest);
    }
#ifdef _WIN32
    if (fopen_s(&input, src, "rb") != 0) input = 0;
#else
    input = fopen(src, "rb");
#endif
    if (input == 0) {
        return abi_fs_fail("copy_file", src);
    }
    if (abi_fs_open_write_retry_parent_dirs(dest, "wb", &output) != 0 || output == 0) {
        fclose(input);
        return abi_fs_fail("copy_file", dest);
    }
    while ((read_count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, read_count, output) != read_count) {
            fclose(input);
            fclose(output);
            return abi_fs_fail("copy_file", dest);
        }
    }
    fclose(input);
    fclose(output);
    return abi_fs_ok();
}

int64_t abi_fs_copy_file_streaming(const char* src, const char* dest, int64_t chunk_size) {
    FILE* input = 0;
    FILE* output = 0;
    char stack_buffer[4096];
    char* buffer = stack_buffer;
    char* heap_buffer = 0;
    size_t buffer_size;
    size_t read_count;
    int64_t copied = 0;
    if (src == 0 || dest == 0 || chunk_size < 1) {
        errno = EINVAL;
        return abi_fs_fail("copy_file_streaming", src ? src : dest);
    }
    buffer_size = (size_t)chunk_size;
    if (buffer_size > 1024 * 1024) {
        buffer_size = 1024 * 1024;
    }
    if (buffer_size > sizeof(stack_buffer)) {
        heap_buffer = (char*)malloc(buffer_size);
        if (heap_buffer == 0) {
            errno = ENOMEM;
            return abi_fs_fail("copy_file_streaming", dest);
        }
        buffer = heap_buffer;
    }
#ifdef _WIN32
    if (fopen_s(&input, src, "rb") != 0) input = 0;
#else
    input = fopen(src, "rb");
#endif
    if (input == 0) {
        free(heap_buffer);
        return abi_fs_fail("copy_file_streaming", src);
    }
    if (abi_fs_open_write_retry_parent_dirs(dest, "wb", &output) != 0 || output == 0) {
        fclose(input);
        free(heap_buffer);
        return abi_fs_fail("copy_file_streaming", dest);
    }
    while ((read_count = fread(buffer, 1, buffer_size, input)) > 0) {
        if (fwrite(buffer, 1, read_count, output) != read_count) {
            fclose(input);
            fclose(output);
            free(heap_buffer);
            return abi_fs_fail("copy_file_streaming", dest);
        }
        copied += (int64_t)read_count;
    }
    fclose(input);
    fclose(output);
    free(heap_buffer);
    abi_fs_ok();
    return copied;
}

int64_t abi_fs_move_path(const char* src, const char* dest) {
    if (src == 0 || dest == 0) {
        errno = EINVAL;
        return abi_fs_fail("move_path", src ? src : dest);
    }
    if (abi_fs_create_parent_dirs(dest) != 0) {
        return abi_fs_fail("move_path", dest);
    }
    if (rename(src, dest) != 0) {
        return abi_fs_fail("move_path", src);
    }
    return abi_fs_ok();
}

int64_t abi_fs_remove_file(const char* path) {
    if (path == 0 || remove(path) != 0) {
        return abi_fs_fail("remove_file", path);
    }
    return abi_fs_ok();
}

#ifdef _WIN32
static int64_t abi_fs_remove_dir_all_inner(const char* path) {
    char pattern[4096];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) < 0) {
        errno = EINVAL;
        return -1;
    }
    handle = FindFirstFileA(pattern, &data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            char child[4096];
            if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
                continue;
            }
            if (snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName) < 0) {
                FindClose(handle);
                errno = EINVAL;
                return -1;
            }
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (abi_fs_remove_dir_all_inner(child) != 0) {
                    FindClose(handle);
                    return -1;
                }
            } else if (DeleteFileA(child) == 0) {
                FindClose(handle);
                errno = EACCES;
                return -1;
            }
        } while (FindNextFileA(handle, &data) != 0);
        FindClose(handle);
    }
    if (RemoveDirectoryA(path) == 0) {
        errno = EACCES;
        return -1;
    }
    return 0;
}
#else
static int64_t abi_fs_remove_dir_all_inner(const char* path) {
    DIR* dir = opendir(path);
    struct dirent* entry;
    if (dir == 0) {
        return -1;
    }
    while ((entry = readdir(dir)) != 0) {
        char child[4096];
        struct stat info;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0) {
            closedir(dir);
            errno = EINVAL;
            return -1;
        }
        if (stat(child, &info) != 0) {
            closedir(dir);
            return -1;
        }
        if (S_ISDIR(info.st_mode)) {
            if (abi_fs_remove_dir_all_inner(child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (unlink(child) != 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return rmdir(path);
}
#endif

int64_t abi_fs_remove_dir_all(const char* path) {
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return abi_fs_fail("remove_dir_all", path);
    }
    if (abi_fs_remove_dir_all_inner(path) != 0) {
        return abi_fs_fail("remove_dir_all", path);
    }
    return abi_fs_ok();
}

static void abi_fs_temp_base(char* buffer, size_t buffer_size) {
    const char* temp = getenv("TMPDIR");
    if (temp == 0 || temp[0] == '\0') temp = getenv("TEMP");
    if (temp == 0 || temp[0] == '\0') temp = getenv("TMP");
    if (temp == 0 || temp[0] == '\0') temp = ".";
    abi_fs_copy_message(buffer, buffer_size, temp);
}

static void abi_fs_temp_path(char* buffer, size_t buffer_size, const char* prefix, int attempt) {
    char base[1024];
    if (prefix == 0 || prefix[0] == '\0') {
        prefix = "kain";
    }
    abi_fs_temp_base(base, sizeof(base));
#ifdef _WIN32
    _snprintf_s(buffer, buffer_size, _TRUNCATE, "%s\\%s-%lu-%lld-%d", base, prefix, (unsigned long)GetCurrentProcessId(), (long long)time(NULL), attempt);
#else
    snprintf(buffer, buffer_size, "%s/%s-%lu-%lld-%d", base, prefix, (unsigned long)getpid(), (long long)time(NULL), attempt);
#endif
}

const char* abi_fs_temp_file(const char* prefix) {
    int attempt;
    for (attempt = 0; attempt < 128; attempt++) {
        char path[4096];
        FILE* file = 0;
        abi_fs_temp_path(path, sizeof(path), prefix, attempt);
#ifdef _WIN32
        if (fopen_s(&file, path, "wx") == 0 && file != 0) {
#else
        file = fopen(path, "wx");
        if (file != 0) {
#endif
            fclose(file);
            abi_fs_ok();
            return string_new(path);
        }
    }
    errno = EEXIST;
    abi_fs_fail("temp_file", prefix);
    return string_new("");
}

const char* abi_fs_temp_dir(const char* prefix) {
    int attempt;
    for (attempt = 0; attempt < 128; attempt++) {
        char path[4096];
        abi_fs_temp_path(path, sizeof(path), prefix, attempt);
        if (abi_fs_create_one_dir(path) == 0 && abi_fs_is_dir(path)) {
            abi_fs_ok();
            return string_new(path);
        }
    }
    errno = EEXIST;
    abi_fs_fail("temp_dir", prefix);
    return string_new("");
}

int64_t abi_fs_atomic_write_text(const char* path, const char* content) {
    if (content == 0) {
        return abi_fs_atomic_write_text_len(path, "", 0);
    }
    return abi_fs_atomic_write_text_len(path, content, (int64_t)strlen(content));
}

int64_t abi_fs_atomic_write_text_len(const char* path, const char* content, int64_t content_length) {
    char temp_path[4096];
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        return abi_fs_fail("atomic_write_text", path);
    }
    if (content_length < 0) {
        errno = EINVAL;
        return abi_fs_fail("atomic_write_text", path);
    }
#ifdef _WIN32
    _snprintf_s(temp_path, sizeof(temp_path), _TRUNCATE, "%s.%lld.tmp", path, (long long)time(NULL));
#else
    snprintf(temp_path, sizeof(temp_path), "%s.%lld.tmp", path, (long long)time(NULL));
#endif
    if (abi_fs_write_text_len(temp_path, content, content_length) != 0) {
        return g_kain_native_fs_last_status;
    }
#ifdef _WIN32
    DeleteFileA(path);
#endif
    if (rename(temp_path, path) != 0) {
        remove(temp_path);
        return abi_fs_fail("atomic_write_text", path);
    }
    return abi_fs_ok();
}

typedef struct KainNativeSha256 {
    uint32_t state[8];
    uint64_t bit_len;
    unsigned char data[64];
    size_t data_len;
} KainNativeSha256;

static const uint32_t ABI_SHA256_K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t abi_sha256_rotr(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

static void abi_sha256_transform(KainNativeSha256* ctx, const unsigned char block[64]) {
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t index;

    for (index = 0; index < 16; index++) {
        size_t base = index * 4;
        words[index] =
            ((uint32_t)block[base] << 24) |
            ((uint32_t)block[base + 1] << 16) |
            ((uint32_t)block[base + 2] << 8) |
            ((uint32_t)block[base + 3]);
    }
    for (index = 16; index < 64; index++) {
        uint32_t s0 =
            abi_sha256_rotr(words[index - 15], 7) ^
            abi_sha256_rotr(words[index - 15], 18) ^
            (words[index - 15] >> 3);
        uint32_t s1 =
            abi_sha256_rotr(words[index - 2], 17) ^
            abi_sha256_rotr(words[index - 2], 19) ^
            (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (index = 0; index < 64; index++) {
        uint32_t s1 = abi_sha256_rotr(e, 6) ^
            abi_sha256_rotr(e, 11) ^
            abi_sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + ABI_SHA256_K[index] + words[index];
        uint32_t s0 = abi_sha256_rotr(a, 2) ^
            abi_sha256_rotr(a, 13) ^
            abi_sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void abi_sha256_init(KainNativeSha256* ctx) {
    ctx->data_len = 0;
    ctx->bit_len = 0;
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void abi_sha256_update(KainNativeSha256* ctx, const unsigned char* data, size_t length) {
    size_t index;
    for (index = 0; index < length; index++) {
        ctx->data[ctx->data_len] = data[index];
        ctx->data_len++;
        if (ctx->data_len == 64) {
            abi_sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void abi_sha256_final(KainNativeSha256* ctx, unsigned char digest[32]) {
    size_t index = ctx->data_len;
    size_t state_index;

    ctx->data[index++] = 0x80;
    if (index > 56) {
        while (index < 64) {
            ctx->data[index++] = 0;
        }
        abi_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    } else {
        while (index < 56) {
            ctx->data[index++] = 0;
        }
    }

    ctx->bit_len += (uint64_t)ctx->data_len * 8U;
    ctx->data[56] = (unsigned char)(ctx->bit_len >> 56);
    ctx->data[57] = (unsigned char)(ctx->bit_len >> 48);
    ctx->data[58] = (unsigned char)(ctx->bit_len >> 40);
    ctx->data[59] = (unsigned char)(ctx->bit_len >> 32);
    ctx->data[60] = (unsigned char)(ctx->bit_len >> 24);
    ctx->data[61] = (unsigned char)(ctx->bit_len >> 16);
    ctx->data[62] = (unsigned char)(ctx->bit_len >> 8);
    ctx->data[63] = (unsigned char)(ctx->bit_len);
    abi_sha256_transform(ctx, ctx->data);

    for (state_index = 0; state_index < 8; state_index++) {
        digest[state_index * 4] = (unsigned char)(ctx->state[state_index] >> 24);
        digest[state_index * 4 + 1] = (unsigned char)(ctx->state[state_index] >> 16);
        digest[state_index * 4 + 2] = (unsigned char)(ctx->state[state_index] >> 8);
        digest[state_index * 4 + 3] = (unsigned char)(ctx->state[state_index]);
    }
}

static const char ABI_CRYPTO_HEX[] = "0123456789abcdef";

static void abi_crypto_sha256_bytes(const unsigned char* data, size_t length, unsigned char digest[32]) {
    KainNativeSha256 sha;
    abi_sha256_init(&sha);
    if (data != 0 && length > 0) {
        abi_sha256_update(&sha, data, length);
    }
    abi_sha256_final(&sha, digest);
}

static const char* abi_crypto_hex_string_from_bytes(const unsigned char* bytes, size_t length) {
    char* output;
    const char* result;
    size_t index;
    if (bytes == 0 && length > 0) {
        return string_new("");
    }
    if (length > ((SIZE_MAX - 1u) / 2u)) {
        return string_new("");
    }
    output = (char*)malloc((length * 2u) + 1u);
    if (output == 0) {
        return string_new("");
    }
    for (index = 0; index < length; index++) {
        output[index * 2u] = ABI_CRYPTO_HEX[bytes[index] >> 4u];
        output[index * 2u + 1u] = ABI_CRYPTO_HEX[bytes[index] & 0x0fu];
    }
    output[length * 2u] = '\0';
    result = string_new(output);
    free(output);
    return result;
}

static int abi_crypto_fill_random(unsigned char* buffer, size_t length) {
    size_t offset = 0;
    if (buffer == 0 && length > 0) {
        return -1;
    }
#ifdef _WIN32
    while (offset < length) {
        unsigned int word = 0;
        size_t remaining = length - offset;
        size_t take = remaining < sizeof(word) ? remaining : sizeof(word);
        if (rand_s(&word) != 0) {
            return -1;
        }
        memcpy(buffer + offset, &word, take);
        offset += take;
    }
    return 0;
#else
    FILE* random_file = fopen("/dev/urandom", "rb");
    if (random_file == 0) {
        return -1;
    }
    while (offset < length) {
        size_t read_count = fread(buffer + offset, 1, length - offset, random_file);
        if (read_count == 0) {
            int failed = ferror(random_file);
            fclose(random_file);
            return failed ? -1 : -1;
        }
        offset += read_count;
    }
    fclose(random_file);
    return 0;
#endif
}

const char* abi_crypto_random_bytes_hex(int64_t length) {
    unsigned char* buffer;
    const char* result;
    if (length < 0 || length > (int64_t)(16 * 1024 * 1024)) {
        return string_new("");
    }
    if (length == 0) {
        return string_new("");
    }
    buffer = (unsigned char*)malloc((size_t)length);
    if (buffer == 0) {
        return string_new("");
    }
    if (abi_crypto_fill_random(buffer, (size_t)length) != 0) {
        free(buffer);
        return string_new("");
    }
    result = abi_crypto_hex_string_from_bytes(buffer, (size_t)length);
    free(buffer);
    return result;
}

const char* abi_crypto_sha256_text(const char* text, int64_t text_length) {
    unsigned char digest[32];
    size_t length;
    if (text_length < 0) {
        return string_new("");
    }
    length = (size_t)text_length;
    if (text == 0 && length > 0) {
        return string_new("");
    }
    abi_crypto_sha256_bytes((const unsigned char*)text, length, digest);
    return abi_crypto_hex_string_from_bytes(digest, sizeof(digest));
}

const char* abi_crypto_hmac_sha256_text(const char* key, int64_t key_length, const char* message, int64_t message_length) {
    unsigned char key_block[64];
    unsigned char inner_pad[64];
    unsigned char outer_pad[64];
    unsigned char inner_digest[32];
    unsigned char digest[32];
    size_t key_len;
    size_t message_len;
    size_t index;
    KainNativeSha256 sha;
    if (key_length < 0 || message_length < 0) {
        return string_new("");
    }
    key_len = (size_t)key_length;
    message_len = (size_t)message_length;
    if ((key == 0 && key_len > 0) || (message == 0 && message_len > 0)) {
        return string_new("");
    }
    memset(key_block, 0, sizeof(key_block));
    if (key_len > sizeof(key_block)) {
        abi_crypto_sha256_bytes((const unsigned char*)key, key_len, key_block);
        key_len = 32;
    } else if (key_len > 0) {
        memcpy(key_block, key, key_len);
    }
    for (index = 0; index < sizeof(key_block); index++) {
        inner_pad[index] = (unsigned char)(key_block[index] ^ 0x36u);
        outer_pad[index] = (unsigned char)(key_block[index] ^ 0x5cu);
    }
    abi_sha256_init(&sha);
    abi_sha256_update(&sha, inner_pad, sizeof(inner_pad));
    if (message_len > 0) {
        abi_sha256_update(&sha, (const unsigned char*)message, message_len);
    }
    abi_sha256_final(&sha, inner_digest);

    abi_sha256_init(&sha);
    abi_sha256_update(&sha, outer_pad, sizeof(outer_pad));
    abi_sha256_update(&sha, inner_digest, sizeof(inner_digest));
    abi_sha256_final(&sha, digest);
    return abi_crypto_hex_string_from_bytes(digest, sizeof(digest));
}

#define ABI_BLAKE3_OUT_LEN 32u
#define ABI_BLAKE3_BLOCK_LEN 64u
#define ABI_BLAKE3_CHUNK_LEN 1024u
#define ABI_BLAKE3_CHUNK_START 1u
#define ABI_BLAKE3_CHUNK_END 2u
#define ABI_BLAKE3_PARENT 4u
#define ABI_BLAKE3_ROOT 8u

typedef struct KainNativeBlake3Output {
    uint32_t input_cv[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} KainNativeBlake3Output;

static const uint32_t ABI_BLAKE3_IV[8] = {
    0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
    0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u,
};

static const uint8_t ABI_BLAKE3_MSG_PERMUTATION[16] = {
    2u, 6u, 3u, 10u, 7u, 0u, 4u, 13u,
    1u, 11u, 12u, 5u, 9u, 14u, 15u, 8u,
};

static uint32_t abi_blake3_rotr32(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32u - count));
}

static uint32_t abi_blake3_load32_le(const unsigned char* bytes) {
    return ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static void abi_blake3_store32_le(unsigned char* bytes, uint32_t word) {
    bytes[0] = (unsigned char)word;
    bytes[1] = (unsigned char)(word >> 8u);
    bytes[2] = (unsigned char)(word >> 16u);
    bytes[3] = (unsigned char)(word >> 24u);
}

static void abi_blake3_block_words(const unsigned char* block, size_t block_len, uint32_t words[16]) {
    unsigned char padded[ABI_BLAKE3_BLOCK_LEN];
    size_t index;
    memset(padded, 0, sizeof(padded));
    if (block != 0 && block_len > 0) {
        memcpy(padded, block, block_len);
    }
    for (index = 0; index < 16; index++) {
        words[index] = abi_blake3_load32_le(padded + (index * 4u));
    }
}

static void abi_blake3_g(uint32_t state[16], size_t a, size_t b, size_t c, size_t d, uint32_t mx, uint32_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = abi_blake3_rotr32(state[d] ^ state[a], 16u);
    state[c] = state[c] + state[d];
    state[b] = abi_blake3_rotr32(state[b] ^ state[c], 12u);
    state[a] = state[a] + state[b] + my;
    state[d] = abi_blake3_rotr32(state[d] ^ state[a], 8u);
    state[c] = state[c] + state[d];
    state[b] = abi_blake3_rotr32(state[b] ^ state[c], 7u);
}

static void abi_blake3_round(uint32_t state[16], const uint32_t msg[16]) {
    abi_blake3_g(state, 0, 4, 8, 12, msg[0], msg[1]);
    abi_blake3_g(state, 1, 5, 9, 13, msg[2], msg[3]);
    abi_blake3_g(state, 2, 6, 10, 14, msg[4], msg[5]);
    abi_blake3_g(state, 3, 7, 11, 15, msg[6], msg[7]);
    abi_blake3_g(state, 0, 5, 10, 15, msg[8], msg[9]);
    abi_blake3_g(state, 1, 6, 11, 12, msg[10], msg[11]);
    abi_blake3_g(state, 2, 7, 8, 13, msg[12], msg[13]);
    abi_blake3_g(state, 3, 4, 9, 14, msg[14], msg[15]);
}

static void abi_blake3_permute(uint32_t msg[16]) {
    uint32_t permuted[16];
    size_t index;
    for (index = 0; index < 16; index++) {
        permuted[index] = msg[ABI_BLAKE3_MSG_PERMUTATION[index]];
    }
    memcpy(msg, permuted, sizeof(permuted));
}

static void abi_blake3_compress_words(
    const uint32_t cv[8],
    const uint32_t block_words[16],
    uint64_t counter,
    uint32_t block_len,
    uint32_t flags,
    uint32_t out[16]
) {
    uint32_t state[16];
    uint32_t msg[16];
    size_t index;
    memcpy(state, cv, 8u * sizeof(uint32_t));
    memcpy(state + 8, ABI_BLAKE3_IV, 4u * sizeof(uint32_t));
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32u);
    state[14] = block_len;
    state[15] = flags;
    memcpy(msg, block_words, 16u * sizeof(uint32_t));
    for (index = 0; index < 7; index++) {
        abi_blake3_round(state, msg);
        abi_blake3_permute(msg);
    }
    for (index = 0; index < 8; index++) {
        out[index] = state[index] ^ state[index + 8u];
        out[index + 8u] = state[index + 8u] ^ cv[index];
    }
}

static void abi_blake3_output_chaining_value(const KainNativeBlake3Output* output, uint32_t cv[8]) {
    uint32_t words[16];
    abi_blake3_compress_words(
        output->input_cv,
        output->block_words,
        output->counter,
        output->block_len,
        output->flags,
        words
    );
    memcpy(cv, words, 8u * sizeof(uint32_t));
}

static void abi_blake3_output_bytes(const KainNativeBlake3Output* output, unsigned char digest[ABI_BLAKE3_OUT_LEN]) {
    uint32_t words[16];
    size_t index;
    abi_blake3_compress_words(
        output->input_cv,
        output->block_words,
        output->counter,
        output->block_len,
        output->flags | ABI_BLAKE3_ROOT,
        words
    );
    for (index = 0; index < 8; index++) {
        abi_blake3_store32_le(digest + (index * 4u), words[index]);
    }
}

static void abi_blake3_chunk_output(
    const unsigned char* input,
    size_t length,
    uint64_t chunk_counter,
    KainNativeBlake3Output* output
) {
    uint32_t cv[8];
    uint32_t words[16];
    uint32_t compressed[16];
    size_t offset = 0;
    size_t blocks_compressed = 0;
    memcpy(cv, ABI_BLAKE3_IV, sizeof(cv));
    while ((length - offset) > ABI_BLAKE3_BLOCK_LEN) {
        uint32_t flags = blocks_compressed == 0 ? ABI_BLAKE3_CHUNK_START : 0u;
        abi_blake3_block_words(input + offset, ABI_BLAKE3_BLOCK_LEN, words);
        abi_blake3_compress_words(cv, words, chunk_counter, ABI_BLAKE3_BLOCK_LEN, flags, compressed);
        memcpy(cv, compressed, 8u * sizeof(uint32_t));
        offset += ABI_BLAKE3_BLOCK_LEN;
        blocks_compressed++;
    }
    memcpy(output->input_cv, cv, sizeof(output->input_cv));
    abi_blake3_block_words(input == 0 ? 0 : input + offset, length - offset, output->block_words);
    output->counter = chunk_counter;
    output->block_len = (uint32_t)(length - offset);
    output->flags = ABI_BLAKE3_CHUNK_END | (blocks_compressed == 0 ? ABI_BLAKE3_CHUNK_START : 0u);
}

static void abi_blake3_parent_output(
    const uint32_t left_cv[8],
    const uint32_t right_cv[8],
    KainNativeBlake3Output* output
) {
    memcpy(output->input_cv, ABI_BLAKE3_IV, sizeof(output->input_cv));
    memcpy(output->block_words, left_cv, 8u * sizeof(uint32_t));
    memcpy(output->block_words + 8, right_cv, 8u * sizeof(uint32_t));
    output->counter = 0;
    output->block_len = ABI_BLAKE3_BLOCK_LEN;
    output->flags = ABI_BLAKE3_PARENT;
}

static void abi_blake3_parent_cv(const uint32_t left_cv[8], const uint32_t right_cv[8], uint32_t out_cv[8]) {
    KainNativeBlake3Output output;
    abi_blake3_parent_output(left_cv, right_cv, &output);
    abi_blake3_output_chaining_value(&output, out_cv);
}

static void abi_crypto_blake3_bytes(const unsigned char* input, size_t length, unsigned char digest[ABI_BLAKE3_OUT_LEN]) {
    size_t chunk_count = length == 0 ? 1u : ((length + ABI_BLAKE3_CHUNK_LEN - 1u) / ABI_BLAKE3_CHUNK_LEN);
    size_t chunk_index;
    uint32_t* cvs;
    KainNativeBlake3Output single_output;
    if (chunk_count == 1u) {
        abi_blake3_chunk_output(input, length, 0, &single_output);
        abi_blake3_output_bytes(&single_output, digest);
        return;
    }
    if (chunk_count > (SIZE_MAX / (8u * sizeof(uint32_t)))) {
        memset(digest, 0, ABI_BLAKE3_OUT_LEN);
        return;
    }
    cvs = (uint32_t*)malloc(chunk_count * 8u * sizeof(uint32_t));
    if (cvs == 0) {
        memset(digest, 0, ABI_BLAKE3_OUT_LEN);
        return;
    }
    for (chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
        KainNativeBlake3Output output;
        size_t offset = chunk_index * ABI_BLAKE3_CHUNK_LEN;
        size_t remaining = length - offset;
        size_t chunk_len = remaining > ABI_BLAKE3_CHUNK_LEN ? ABI_BLAKE3_CHUNK_LEN : remaining;
        abi_blake3_chunk_output(input + offset, chunk_len, (uint64_t)chunk_index, &output);
        abi_blake3_output_chaining_value(&output, cvs + (chunk_index * 8u));
    }
    while (chunk_count > 2u) {
        size_t read_index = 0;
        size_t write_index = 0;
        while ((read_index + 1u) < chunk_count) {
            abi_blake3_parent_cv(cvs + (read_index * 8u), cvs + ((read_index + 1u) * 8u), cvs + (write_index * 8u));
            read_index += 2u;
            write_index++;
        }
        if (read_index < chunk_count) {
            memmove(cvs + (write_index * 8u), cvs + (read_index * 8u), 8u * sizeof(uint32_t));
            write_index++;
        }
        chunk_count = write_index;
    }
    {
        KainNativeBlake3Output root_output;
        abi_blake3_parent_output(cvs, cvs + 8u, &root_output);
        abi_blake3_output_bytes(&root_output, digest);
    }
    free(cvs);
}

const char* abi_crypto_blake3_text(const char* text, int64_t text_length) {
    unsigned char digest[ABI_BLAKE3_OUT_LEN];
    size_t length;
    if (text_length < 0) {
        return string_new("");
    }
    length = (size_t)text_length;
    if (text == 0 && length > 0) {
        return string_new("");
    }
    abi_crypto_blake3_bytes((const unsigned char*)text, length, digest);
    return abi_crypto_hex_string_from_bytes(digest, ABI_BLAKE3_OUT_LEN);
}

int64_t abi_map_release(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    rc_release((void*)(intptr_t)handle);
    return 0;
}

const char* abi_fs_hash_file(const char* path) {
    FILE* file = 0;
    KainNativeSha256 sha;
    unsigned char digest[32];
    unsigned char buffer[65536];
    size_t read_count;
    char output[65];
    static const char hex[] = "0123456789abcdef";
    size_t index;
    if (path == 0 || path[0] == '\0') {
        errno = EINVAL;
        abi_fs_fail("hash_file", path);
        return string_new("");
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) file = 0;
#else
    file = fopen(path, "rb");
#endif
    if (file == 0) {
        abi_fs_fail("hash_file", path);
        return string_new("");
    }
    abi_sha256_init(&sha);
    while ((read_count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        abi_sha256_update(&sha, buffer, read_count);
    }
    if (ferror(file) != 0) {
        fclose(file);
        abi_fs_fail("hash_file", path);
        return string_new("");
    }
    fclose(file);
    abi_sha256_final(&sha, digest);
    for (index = 0; index < 32; index++) {
        output[index * 2] = hex[digest[index] >> 4];
        output[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    output[64] = '\0';
    abi_fs_ok();
    return string_new(output);
}

const char* abi_fs_path_join(const char* base, const char* child) {
    char joined[4096];
    size_t base_len;
    if (child == 0) child = "";
    if (base == 0 || base[0] == '\0' || abi_fs_path_is_absolute(child)) {
        return string_new((char*)child);
    }
    base_len = strlen(base);
    if (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
#ifdef _WIN32
        _snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s%s", base, child);
#else
        snprintf(joined, sizeof(joined), "%s%s", base, child);
#endif
    } else {
#ifdef _WIN32
        _snprintf_s(joined, sizeof(joined), _TRUNCATE, "%s\\%s", base, child);
#else
        snprintf(joined, sizeof(joined), "%s/%s", base, child);
#endif
    }
    return string_new(joined);
}

int64_t abi_fs_last_status(void) {
    return g_kain_native_fs_last_status;
}

const char* abi_fs_last_error_kind(void) {
    return string_new(g_kain_native_fs_last_error_kind);
}

const char* abi_fs_last_error_message(void) {
    return string_new(g_kain_native_fs_last_error_message);
}
