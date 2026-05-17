#ifndef KAIN_ATTRITION_HARNESS_H
#define KAIN_ATTRITION_HARNESS_H

#include "actor.h"
#include "async.h"
#include "attrition.h"
#include "base.h"
#include "process_system.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AttritionCaseOptions {
    const char* case_id;
    const char* sabotage_mode;
    uint64_t ops;
    uint64_t seed;
    uint64_t virtual_time_enabled;
    uint64_t virtual_time_initial_ms;
    uint64_t virtual_time_step_ms;
    uint64_t poison_on_free;
    uint64_t quarantine_capacity;
    uint64_t fragmentation_noise_max_bytes;
    uint64_t allocation_fail_after;
    uint64_t determinism_tier;
    uint64_t expect_failure;
    uint64_t time_provenance_required;
} AttritionCaseOptions;

typedef int (*AttritionLaneRunFn)(
    const AttritionCaseOptions* options,
    uint64_t* out_checksum,
    char* out_failure,
    size_t out_failure_capacity
);

typedef int (*AttritionLaneValidateFn)(
    const AttritionCaseOptions* options,
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
);

static void attrition_case_options_init(
    AttritionCaseOptions* options,
    const AttritionCaseOptions* defaults
) {
    memset(options, 0, sizeof(*options));
    if (defaults != NULL) {
        *options = *defaults;
    }
    if (options->case_id == NULL) {
        options->case_id = "unknown";
    }
    if (options->sabotage_mode == NULL) {
        options->sabotage_mode = "";
    }
    if (options->ops == 0u) {
        options->ops = 1u;
    }
    if (options->virtual_time_step_ms == 0u) {
        options->virtual_time_step_ms = 1u;
    }
    if (options->determinism_tier == 0u) {
        options->determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    }
}

static int attrition_parse_u64_arg(const char* text, uint64_t* out_value) {
    char* end = NULL;
    unsigned long long parsed;
    if (text == NULL || out_value == NULL) {
        return -1;
    }
    parsed = strtoull(text, &end, 10);
    if (end == NULL || *end != '\0') {
        return -1;
    }
    *out_value = (uint64_t)parsed;
    return 0;
}

static int attrition_parse_cli(int argc, char** argv, AttritionCaseOptions* options) {
    int index;
    for (index = 1; index < argc; ++index) {
        const char* arg = argv[index];
        const char* value = (index + 1 < argc) ? argv[index + 1] : NULL;
        if (strcmp(arg, "--case-id") == 0 && value != NULL) {
            options->case_id = value;
            index += 1;
        } else if (strcmp(arg, "--sabotage") == 0 && value != NULL) {
            options->sabotage_mode = value;
            index += 1;
        } else if (strcmp(arg, "--ops") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->ops) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--seed") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->seed) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--virtual-time-enabled") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->virtual_time_enabled) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--virtual-time-initial-ms") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->virtual_time_initial_ms) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--virtual-time-step-ms") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->virtual_time_step_ms) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--poison-on-free") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->poison_on_free) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--quarantine-capacity") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->quarantine_capacity) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--fragmentation-noise-max-bytes") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->fragmentation_noise_max_bytes) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--allocation-fail-after") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->allocation_fail_after) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--determinism-tier") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->determinism_tier) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--expect-failure") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->expect_failure) != 0) {
                return -1;
            }
            index += 1;
        } else if (strcmp(arg, "--time-provenance-required") == 0 && value != NULL) {
            if (attrition_parse_u64_arg(value, &options->time_provenance_required) != 0) {
                return -1;
            }
            index += 1;
        } else {
            return -1;
        }
    }
    return 0;
}

static int attrition_sabotage_is(const AttritionCaseOptions* options, const char* sabotage_mode) {
    const char* active_mode = options != NULL && options->sabotage_mode != NULL
        ? options->sabotage_mode
        : "";
    return strcmp(active_mode, sabotage_mode) == 0;
}

static void attrition_copy_failure(
    char* out_failure,
    size_t out_failure_capacity,
    const char* message
) {
    if (out_failure == NULL || out_failure_capacity == 0u) {
        return;
    }
    if (message == NULL) {
        out_failure[0] = '\0';
        return;
    }
    snprintf(out_failure, out_failure_capacity, "%s", message);
}

static int attrition_expect_u64_eq(
    char* out_failure,
    size_t out_failure_capacity,
    const char* name,
    uint64_t expected,
    uint64_t actual
) {
    if (expected == actual) {
        return 0;
    }
    snprintf(
        out_failure,
        out_failure_capacity,
        "%s expected %" PRIu64 " but observed %" PRIu64,
        name,
        expected,
        actual
    );
    return -1;
}

static int attrition_validate_time_provenance(
    const AttritionCaseOptions* options,
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (options == NULL || baseline == NULL || final_snapshot == NULL) {
        return -1;
    }
    if (!options->time_provenance_required || !options->virtual_time_enabled) {
        return 0;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "raw_clock_fallback_count",
            baseline->raw_clock_fallback_count,
            final_snapshot->raw_clock_fallback_count) != 0) {
        return -1;
    }
    return attrition_expect_u64_eq(
        out_failure,
        out_failure_capacity,
        "raw_sleep_fallback_count",
        baseline->raw_sleep_fallback_count,
        final_snapshot->raw_sleep_fallback_count);
}

static int attrition_validate_rc_closure(
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "live_rc_objects",
            baseline->live_rc_objects,
            final_snapshot->live_rc_objects) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "live_runtime_bytes",
            baseline->live_runtime_bytes,
            final_snapshot->live_runtime_bytes) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "allocation_minus_free_delta",
            baseline->allocation_count - baseline->free_count,
            final_snapshot->allocation_count - final_snapshot->free_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "rc_underflow_count",
            baseline->rc_underflow_count,
            final_snapshot->rc_underflow_count) != 0) {
        return -1;
    }
    return attrition_expect_u64_eq(
        out_failure,
        out_failure_capacity,
        "rc_overflow_count",
        baseline->rc_overflow_count,
        final_snapshot->rc_overflow_count);
}

static int attrition_validate_actor_closure(
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "actor_live_count",
            baseline->actor_live_count,
            final_snapshot->actor_live_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "reply_port_live_count",
            baseline->reply_port_live_count,
            final_snapshot->reply_port_live_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "pending_mailbox_message_count",
            baseline->pending_mailbox_message_count,
            final_snapshot->pending_mailbox_message_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "pending_mailbox_cached_nodes",
            baseline->pending_mailbox_cached_nodes,
            final_snapshot->pending_mailbox_cached_nodes) != 0) {
        return -1;
    }
    return attrition_expect_u64_eq(
        out_failure,
        out_failure_capacity,
        "actor_occupancy_low_word",
        baseline->actor_occupancy_low_word,
        final_snapshot->actor_occupancy_low_word);
}

static int attrition_validate_process_closure(
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "process_live_count",
            baseline->process_live_count,
            final_snapshot->process_live_count) != 0) {
        return -1;
    }
    return attrition_expect_u64_eq(
        out_failure,
        out_failure_capacity,
        "process_occupancy_bits",
        baseline->process_occupancy_bits,
        final_snapshot->process_occupancy_bits);
}

static int attrition_validate_async_closure(
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "async_task_live_count",
            baseline->async_task_live_count,
            final_snapshot->async_task_live_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "async_task_occupancy_low_word",
            baseline->async_task_occupancy_low_word,
            final_snapshot->async_task_occupancy_low_word) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "async_timer_live_count",
            baseline->async_timer_live_count,
            final_snapshot->async_timer_live_count) != 0) {
        return -1;
    }
    return attrition_expect_u64_eq(
        out_failure,
        out_failure_capacity,
        "async_timer_occupancy_low_word",
        baseline->async_timer_occupancy_low_word,
        final_snapshot->async_timer_occupancy_low_word);
}

static void attrition_json_write_escaped(FILE* stream, const char* text) {
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

static void attrition_json_write_snapshot(FILE* stream, const KainAttritionSnapshot* snapshot) {
    fprintf(
        stream,
        "{"
        "\"schema_version\":%" PRIu64 ","
        "\"seed\":%" PRIu64 ","
        "\"determinism_tier\":%" PRIu64 ","
        "\"live_rc_objects\":%" PRIu64 ","
        "\"peak_live_rc_objects\":%" PRIu64 ","
        "\"live_runtime_bytes\":%" PRIu64 ","
        "\"peak_runtime_bytes\":%" PRIu64 ","
        "\"allocation_count\":%" PRIu64 ","
        "\"free_count\":%" PRIu64 ","
        "\"total_allocated_bytes\":%" PRIu64 ","
        "\"total_freed_bytes\":%" PRIu64 ","
        "\"allocation_fail_count\":%" PRIu64 ","
        "\"retain_count\":%" PRIu64 ","
        "\"release_count\":%" PRIu64 ","
        "\"rc_underflow_count\":%" PRIu64 ","
        "\"rc_overflow_count\":%" PRIu64 ","
        "\"poison_free_count\":%" PRIu64 ","
        "\"quarantine_live_entries\":%" PRIu64 ","
        "\"quarantine_live_bytes\":%" PRIu64 ","
        "\"quarantine_peak_entries\":%" PRIu64 ","
        "\"quarantine_peak_bytes\":%" PRIu64 ","
        "\"fragmentation_noise_live_bytes\":%" PRIu64 ","
        "\"fragmentation_noise_peak_bytes\":%" PRIu64 ","
        "\"fragmentation_noise_total_bytes\":%" PRIu64 ","
        "\"fragmentation_injection_count\":%" PRIu64 ","
        "\"actor_live_count\":%" PRIu64 ","
        "\"actor_peak_count\":%" PRIu64 ","
        "\"actor_spawn_count\":%" PRIu64 ","
        "\"actor_exit_count\":%" PRIu64 ","
        "\"actor_stale_reject_count\":%" PRIu64 ","
        "\"reply_port_live_count\":%" PRIu64 ","
        "\"reply_port_peak_count\":%" PRIu64 ","
        "\"pending_mailbox_message_count\":%" PRIu64 ","
        "\"pending_mailbox_cached_nodes\":%" PRIu64 ","
        "\"actor_occupancy_low_word\":%" PRIu64 ","
        "\"actor_occupancy_popcount\":%" PRIu64 ","
        "\"actor_registry_live_entries\":%" PRIu64 ","
        "\"actor_monitor_edge_count\":%" PRIu64 ","
        "\"actor_link_edge_count\":%" PRIu64 ","
        "\"actor_supervised_count\":%" PRIu64 ","
        "\"actor_in_scheduler_turn_count\":%" PRIu64 ","
        "\"actor_restart_attempt_count_total\":%" PRIu64 ","
        "\"actor_supervision_limit_hit_count\":%" PRIu64 ","
        "\"actor_strategy_shutdown_count_total\":%" PRIu64 ","
        "\"actor_escalation_count_total\":%" PRIu64 ","
        "\"actor_scheduler_queue_depth\":%" PRIu64 ","
        "\"actor_scheduler_max_queue_depth\":%" PRIu64 ","
        "\"actor_scheduler_total_enqueued\":%" PRIu64 ","
        "\"actor_scheduler_total_dequeued\":%" PRIu64 ","
        "\"actor_scheduler_worker_count\":%" PRIu64 ","
        "\"actor_scheduler_active_workers\":%" PRIu64 ","
        "\"actor_scheduler_busy_workers\":%" PRIu64 ","
        "\"actor_scheduler_max_busy_workers\":%" PRIu64 ","
        "\"actor_scheduler_overflow_thread_spawns\":%" PRIu64 ","
        "\"actor_scheduler_shutdown\":%" PRIu64 ","
        "\"process_live_count\":%" PRIu64 ","
        "\"process_peak_count\":%" PRIu64 ","
        "\"process_spawn_count\":%" PRIu64 ","
        "\"process_exit_count\":%" PRIu64 ","
        "\"process_stale_reject_count\":%" PRIu64 ","
        "\"process_spec_live_count\":%" PRIu64 ","
        "\"process_spec_occupancy_bits\":%" PRIu64 ","
        "\"process_occupancy_bits\":%" PRIu64 ","
        "\"process_pipe_handle_live_count\":%" PRIu64 ","
        "\"process_os_handle_live_count\":%" PRIu64 ","
        "\"process_pty_live_count\":%" PRIu64 ","
        "\"process_capture_live_bytes\":%" PRIu64 ","
        "\"async_task_live_count\":%" PRIu64 ","
        "\"async_task_peak_count\":%" PRIu64 ","
        "\"async_task_spawn_count\":%" PRIu64 ","
        "\"async_task_exit_count\":%" PRIu64 ","
        "\"async_task_stale_reject_count\":%" PRIu64 ","
        "\"async_task_occupancy_low_word\":%" PRIu64 ","
        "\"async_task_occupancy_popcount\":%" PRIu64 ","
        "\"async_task_cancel_requested_count\":%" PRIu64 ","
        "\"async_task_sleeping_count\":%" PRIu64 ","
        "\"async_task_ready_count\":%" PRIu64 ","
        "\"async_timer_live_count\":%" PRIu64 ","
        "\"async_timer_peak_count\":%" PRIu64 ","
        "\"async_timer_spawn_count\":%" PRIu64 ","
        "\"async_timer_exit_count\":%" PRIu64 ","
        "\"async_timer_cancel_count\":%" PRIu64 ","
        "\"async_timer_stale_reject_count\":%" PRIu64 ","
        "\"async_timer_occupancy_low_word\":%" PRIu64 ","
        "\"async_timer_occupancy_popcount\":%" PRIu64 ","
        "\"async_timer_cancelled_count\":%" PRIu64 ","
        "\"async_timer_fired_count\":%" PRIu64 ","
        "\"async_timer_started_count\":%" PRIu64 ","
        "\"checkpoint_count\":%" PRIu64 ","
        "\"last_checkpoint_label_hash\":%" PRIu64 ","
        "\"last_checkpoint_subject_id\":%" PRIu64 ","
        "\"progress_heartbeat_count\":%" PRIu64 ","
        "\"last_progress_iteration\":%" PRIu64 ","
        "\"last_progress_checksum\":%" PRIu64 ","
        "\"event_count_total\":%" PRIu64 ","
        "\"virtual_time_enabled\":%" PRIu64 ","
        "\"virtual_time_now_ms\":%" PRIu64 ","
        "\"virtual_time_step_ms\":%" PRIu64 ","
        "\"virtual_time_advance_count\":%" PRIu64 ","
        "\"virtual_time_advance_total_ms\":%" PRIu64 ","
        "\"raw_clock_fallback_count\":%" PRIu64 ","
        "\"raw_sleep_fallback_count\":%" PRIu64 ","
        "\"raw_sleep_fallback_millis_total\":%" PRIu64
        "}",
        snapshot->schema_version,
        snapshot->seed,
        snapshot->determinism_tier,
        snapshot->live_rc_objects,
        snapshot->peak_live_rc_objects,
        snapshot->live_runtime_bytes,
        snapshot->peak_runtime_bytes,
        snapshot->allocation_count,
        snapshot->free_count,
        snapshot->total_allocated_bytes,
        snapshot->total_freed_bytes,
        snapshot->allocation_fail_count,
        snapshot->retain_count,
        snapshot->release_count,
        snapshot->rc_underflow_count,
        snapshot->rc_overflow_count,
        snapshot->poison_free_count,
        snapshot->quarantine_live_entries,
        snapshot->quarantine_live_bytes,
        snapshot->quarantine_peak_entries,
        snapshot->quarantine_peak_bytes,
        snapshot->fragmentation_noise_live_bytes,
        snapshot->fragmentation_noise_peak_bytes,
        snapshot->fragmentation_noise_total_bytes,
        snapshot->fragmentation_injection_count,
        snapshot->actor_live_count,
        snapshot->actor_peak_count,
        snapshot->actor_spawn_count,
        snapshot->actor_exit_count,
        snapshot->actor_stale_reject_count,
        snapshot->reply_port_live_count,
        snapshot->reply_port_peak_count,
        snapshot->pending_mailbox_message_count,
        snapshot->pending_mailbox_cached_nodes,
        snapshot->actor_occupancy_low_word,
        snapshot->actor_occupancy_popcount,
        snapshot->actor_registry_live_entries,
        snapshot->actor_monitor_edge_count,
        snapshot->actor_link_edge_count,
        snapshot->actor_supervised_count,
        snapshot->actor_in_scheduler_turn_count,
        snapshot->actor_restart_attempt_count_total,
        snapshot->actor_supervision_limit_hit_count,
        snapshot->actor_strategy_shutdown_count_total,
        snapshot->actor_escalation_count_total,
        snapshot->actor_scheduler_queue_depth,
        snapshot->actor_scheduler_max_queue_depth,
        snapshot->actor_scheduler_total_enqueued,
        snapshot->actor_scheduler_total_dequeued,
        snapshot->actor_scheduler_worker_count,
        snapshot->actor_scheduler_active_workers,
        snapshot->actor_scheduler_busy_workers,
        snapshot->actor_scheduler_max_busy_workers,
        snapshot->actor_scheduler_overflow_thread_spawns,
        snapshot->actor_scheduler_shutdown,
        snapshot->process_live_count,
        snapshot->process_peak_count,
        snapshot->process_spawn_count,
        snapshot->process_exit_count,
        snapshot->process_stale_reject_count,
        snapshot->process_spec_live_count,
        snapshot->process_spec_occupancy_bits,
        snapshot->process_occupancy_bits,
        snapshot->process_pipe_handle_live_count,
        snapshot->process_os_handle_live_count,
        snapshot->process_pty_live_count,
        snapshot->process_capture_live_bytes,
        snapshot->async_task_live_count,
        snapshot->async_task_peak_count,
        snapshot->async_task_spawn_count,
        snapshot->async_task_exit_count,
        snapshot->async_task_stale_reject_count,
        snapshot->async_task_occupancy_low_word,
        snapshot->async_task_occupancy_popcount,
        snapshot->async_task_cancel_requested_count,
        snapshot->async_task_sleeping_count,
        snapshot->async_task_ready_count,
        snapshot->async_timer_live_count,
        snapshot->async_timer_peak_count,
        snapshot->async_timer_spawn_count,
        snapshot->async_timer_exit_count,
        snapshot->async_timer_cancel_count,
        snapshot->async_timer_stale_reject_count,
        snapshot->async_timer_occupancy_low_word,
        snapshot->async_timer_occupancy_popcount,
        snapshot->async_timer_cancelled_count,
        snapshot->async_timer_fired_count,
        snapshot->async_timer_started_count,
        snapshot->checkpoint_count,
        snapshot->last_checkpoint_label_hash,
        snapshot->last_checkpoint_subject_id,
        snapshot->progress_heartbeat_count,
        snapshot->last_progress_iteration,
        snapshot->last_progress_checksum,
        snapshot->event_count_total,
        snapshot->virtual_time_enabled,
        snapshot->virtual_time_now_ms,
        snapshot->virtual_time_step_ms,
        snapshot->virtual_time_advance_count,
        snapshot->virtual_time_advance_total_ms,
        snapshot->raw_clock_fallback_count,
        snapshot->raw_sleep_fallback_count,
        snapshot->raw_sleep_fallback_millis_total
    );
}

static void attrition_json_write_events(FILE* stream, const KainAttritionEvent* events, size_t count) {
    size_t index;
    fputc('[', stream);
    for (index = 0u; index < count; ++index) {
        if (index != 0u) {
            fputc(',', stream);
        }
        fprintf(
            stream,
            "{"
            "\"event_index\":%" PRIu64 ","
            "\"kind\":%u,"
            "\"aux\":%u,"
            "\"arg0\":%" PRIu64 ","
            "\"arg1\":%" PRIu64 ","
            "\"arg2\":%" PRIu64
            "}",
            events[index].event_index,
            events[index].kind,
            events[index].aux,
            events[index].arg0,
            events[index].arg1,
            events[index].arg2
        );
    }
    fputc(']', stream);
}

static int attrition_case_main(
    int argc,
    char** argv,
    const AttritionCaseOptions* defaults,
    AttritionLaneRunFn run_fn,
    AttritionLaneValidateFn validate_fn
) {
    AttritionCaseOptions options;
    KainAttritionSessionConfig session_config;
    KainAttritionSnapshot baseline_snapshot;
    KainAttritionSnapshot final_snapshot;
    KainAttritionEvent events[KAIN_ATTRITION_EVENT_RING_CAPACITY];
    char audit_json[8192];
    char run_failure[512];
    char validate_failure[512];
    size_t event_count;
    size_t audit_length;
    uint64_t checksum = 0u;
    int run_status;
    int validate_status = 0;
    int overall_status;

    attrition_case_options_init(&options, defaults);
    if (attrition_parse_cli(argc, argv, &options) != 0) {
        fprintf(stderr, "invalid attrition case arguments\n");
        return 2;
    }

    memset(&baseline_snapshot, 0, sizeof(baseline_snapshot));
    memset(&final_snapshot, 0, sizeof(final_snapshot));
    memset(events, 0, sizeof(events));
    memset(audit_json, 0, sizeof(audit_json));
    memset(run_failure, 0, sizeof(run_failure));
    memset(validate_failure, 0, sizeof(validate_failure));

    kain_attrition_session_config_init(&session_config);
    session_config.enabled = 1u;
    session_config.seed = options.seed;
    session_config.virtual_time_enabled = options.virtual_time_enabled;
    session_config.virtual_time_initial_ms = options.virtual_time_initial_ms;
    session_config.virtual_time_step_ms = options.virtual_time_step_ms;
    session_config.poison_on_free = options.poison_on_free;
    session_config.quarantine_capacity = options.quarantine_capacity;
    session_config.fragmentation_noise_max_bytes = options.fragmentation_noise_max_bytes;
    session_config.allocation_fail_after = options.allocation_fail_after;
    session_config.determinism_tier = options.determinism_tier;

    kain_attrition_runtime_configure(&session_config);
    kain_attrition_runtime_reset();
    kain_attrition_runtime_snapshot(&baseline_snapshot);
    kain_attrition_runtime_checkpoint("case-start", 0u);

    run_status = run_fn(&options, &checksum, run_failure, sizeof(run_failure));
    kain_attrition_runtime_checkpoint("case-end", checksum);
    kain_attrition_runtime_snapshot(&final_snapshot);
    audit_length = kain_attrition_runtime_write_audit_json(audit_json, sizeof(audit_json));
    if (audit_length == 0u) {
        snprintf(audit_json, sizeof(audit_json), "{}");
    }
    event_count = kain_attrition_runtime_copy_events(events, KAIN_ATTRITION_EVENT_RING_CAPACITY);

    if (run_status == 0 && validate_fn != NULL) {
        validate_status = validate_fn(
            &options,
            &baseline_snapshot,
            &final_snapshot,
            validate_failure,
            sizeof(validate_failure)
        );
    }

    overall_status = run_status != 0 ? run_status : validate_status;

    fprintf(stdout, "{");
    fprintf(stdout, "\"schema_version\":1,");
    fprintf(stdout, "\"report_kind\":\"attrition_case_result\",");
    fprintf(stdout, "\"case_id\":");
    attrition_json_write_escaped(stdout, options.case_id);
    fprintf(stdout, ",\"sabotage_mode\":");
    attrition_json_write_escaped(stdout, options.sabotage_mode);
    fprintf(stdout, ",\"ops\":%" PRIu64, options.ops);
    fprintf(stdout, ",\"seed\":%" PRIu64, options.seed);
    fprintf(stdout, ",\"determinism_tier\":%" PRIu64, options.determinism_tier);
    fprintf(stdout, ",\"virtual_time_enabled\":%" PRIu64, options.virtual_time_enabled);
    fprintf(stdout, ",\"expect_failure\":%" PRIu64, options.expect_failure);
    fprintf(stdout, ",\"checksum\":%" PRIu64, checksum);
    fprintf(stdout, ",\"run_status\":%d", run_status);
    fprintf(stdout, ",\"validate_status\":%d", validate_status);
    fprintf(stdout, ",\"overall_status\":%d", overall_status);
    fprintf(stdout, ",\"passed\":%s", overall_status == 0 ? "true" : "false");
    fprintf(stdout, ",\"expected_failure_matched\":%s", options.expect_failure != 0u
        ? (overall_status != 0 ? "true" : "false")
        : (overall_status == 0 ? "true" : "false"));
    fprintf(stdout, ",\"run_failure\":");
    attrition_json_write_escaped(stdout, run_failure);
    fprintf(stdout, ",\"validate_failure\":");
    attrition_json_write_escaped(stdout, validate_failure);
    fprintf(stdout, ",\"baseline_snapshot\":");
    attrition_json_write_snapshot(stdout, &baseline_snapshot);
    fprintf(stdout, ",\"final_snapshot\":");
    attrition_json_write_snapshot(stdout, &final_snapshot);
    fprintf(stdout, ",\"audit\":%s", audit_json);
    fprintf(stdout, ",\"events\":");
    attrition_json_write_events(stdout, events, event_count);
    fprintf(stdout, "}\n");
    fflush(stdout);
    return overall_status == 0 ? 0 : 1;
}

#endif
