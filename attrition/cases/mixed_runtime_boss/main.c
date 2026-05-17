#include "attrition_harness.h"

char* to_string(long long value);
char* str_concat3(char* s1, char* s2, char* s3);
long long len(void* value);

static int run_lane(
    const AttritionCaseOptions* options,
    uint64_t* out_checksum,
    char* out_failure,
    size_t out_failure_capacity
) {
    char* shared = string_new("mixed-runtime-boss");
    char* separator = string_new("#");
    uint64_t checksum = 0u;
    uint64_t index;
    if (shared == NULL || separator == NULL) {
        rc_release(shared);
        rc_release(separator);
        attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate mixed shared string state");
        return -1;
    }
    kain_actor_runtime_init();

    for (index = 0u; index < options->ops; ++index) {
        char* digits = to_string((long long)(index ^ options->seed));
        char* joined;
        if (digits == NULL) {
            rc_release(separator);
            rc_release(shared);
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate mixed decimal text");
            return -1;
        }
        joined = str_concat3(shared, separator, digits);
        if (joined == NULL) {
            rc_release(digits);
            rc_release(separator);
            rc_release(shared);
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate mixed concatenated text");
            return -1;
        }
        checksum ^= (uint64_t)len(joined);
        rc_retain(shared);
        rc_release(shared);
        rc_release(digits);
        rc_release(joined);

        if ((index & 3u) == 0u) {
            void* port = kain_actor_reply_port_new();
            KainActorRef stale_ref;
            KainActorRef live_ref;
            long long reply_value = (long long)(index * 13u + 5u);
            long long received_value = 0;
            size_t received_size = 0u;
            if (port == NULL) {
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate mixed reply port");
                return -1;
            }
            kain_actor_reply_port_actor_ref(port, &stale_ref);
            if (kain_actor_reply_port_send_ref(&stale_ref, &reply_value, sizeof(reply_value)) != 0 ||
                kain_actor_reply_port_wait(port, 1000, &received_value, sizeof(received_value), &received_size) != 0) {
                kain_actor_reply_port_destroy(port);
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to roundtrip mixed reply port");
                return -1;
            }
            checksum ^= (uint64_t)received_value;
            kain_actor_reply_port_destroy(port);
            port = kain_actor_reply_port_new();
            if (port == NULL) {
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to rearm mixed reply port");
                return -1;
            }
            kain_actor_reply_port_actor_ref(port, &live_ref);
            if (kain_actor_reply_port_send_ref(&stale_ref, &reply_value, sizeof(reply_value)) == 0) {
                kain_actor_reply_port_destroy(port);
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "mixed lane accepted a stale reply port");
                return -1;
            }
            reply_value ^= (long long)(index << 2u);
            if (kain_actor_reply_port_send_ref(&live_ref, &reply_value, sizeof(reply_value)) != 0 ||
                kain_actor_reply_port_wait(port, 1000, &received_value, sizeof(received_value), &received_size) != 0) {
                kain_actor_reply_port_destroy(port);
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to receive mixed live reply port");
                return -1;
            }
            checksum ^= (uint64_t)live_ref.generation;
            if (!attrition_sabotage_is(options, "skip_mixed_cleanup")) {
                kain_actor_reply_port_destroy(port);
            }
        }

        if ((index & 7u) == 0u) {
            KainDiagnostic diag;
            KainTaskId task_id = kain_async_sleep((index & 15u) + 1u, &diag);
            void* result = NULL;
            if (task_id == KAIN_TASK_ID_INVALID ||
                kain_task_await(task_id, &result, &diag) != 0) {
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, diag.message);
                return -1;
            }
            checksum ^= (uint64_t)task_id << 2u;
            if (!attrition_sabotage_is(options, "skip_mixed_cleanup") &&
                kain_attrition_async_dispose_task(task_id) != 0) {
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to dispose mixed async task");
                return -1;
            }
        }

        if ((index & 31u) == 0u) {
            if (!abi_process_platform_available()) {
                rc_release(separator);
                rc_release(shared);
                attrition_copy_failure(out_failure, out_failure_capacity, "mixed lane requires the Windows native process substrate");
                return -1;
            }
            {
                int64_t spec_id = abi_process_spec_create("cmd.exe");
                int64_t process_id;
                int64_t wait_status = -1;
                if (spec_id < 0 ||
                    abi_process_spec_add_arg(spec_id, "/c") != 0 ||
                    abi_process_spec_add_arg(spec_id, "exit") != 0 ||
                    abi_process_spec_add_arg(spec_id, "/b") != 0 ||
                    abi_process_spec_add_arg(spec_id, "0") != 0 ||
                    abi_process_spec_set_stdout_mode(spec_id, "null") != 0 ||
                    abi_process_spec_set_stderr_mode(spec_id, "null") != 0 ||
                    abi_process_spec_set_stdin_mode(spec_id, "null") != 0) {
                    if (spec_id >= 0) {
                        abi_process_spec_destroy(spec_id);
                    }
                    rc_release(separator);
                    rc_release(shared);
                    attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
                    return -1;
                }
                process_id = abi_process_spawn(spec_id);
                if (process_id >= 0) {
                    wait_status = abi_process_wait(process_id, 5000);
                }
                if (process_id < 0 ||
                    wait_status < 0 ||
                    wait_status == 0 ||
                    (!attrition_sabotage_is(options, "skip_mixed_cleanup") && abi_process_close(process_id) != 0) ||
                    abi_process_spec_destroy(spec_id) != 0) {
                    if (process_id >= 0 && !attrition_sabotage_is(options, "skip_mixed_cleanup")) {
                        abi_process_close(process_id);
                    }
                    if (spec_id >= 0) {
                        abi_process_spec_destroy(spec_id);
                    }
                    rc_release(separator);
                    rc_release(shared);
                    if (wait_status == 0) {
                        attrition_copy_failure(out_failure, out_failure_capacity, "mixed lane child process did not exit before timeout");
                    } else {
                        attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
                    }
                    return -1;
                }
                checksum ^= (uint64_t)process_id;
            }
        }

        if ((index & 1023u) == 0u) {
            kain_attrition_runtime_note_progress(index, checksum);
        }
    }

    rc_release(separator);
    if (!attrition_sabotage_is(options, "skip_mixed_cleanup")) {
        rc_release(shared);
    }
    *out_checksum = checksum;
    return 0;
}

static int validate_lane(
    const AttritionCaseOptions* options,
    const KainAttritionSnapshot* baseline,
    const KainAttritionSnapshot* final_snapshot,
    char* out_failure,
    size_t out_failure_capacity
) {
    if (attrition_validate_time_provenance(
            options,
            baseline,
            final_snapshot,
            out_failure,
            out_failure_capacity) != 0) {
        return -1;
    }
    if (attrition_validate_rc_closure(baseline, final_snapshot, out_failure, out_failure_capacity) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "actor_live_count",
            0u,
            final_snapshot->actor_live_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "reply_port_live_count",
            0u,
            final_snapshot->reply_port_live_count) != 0) {
        return -1;
    }
    if (attrition_expect_u64_eq(
            out_failure,
            out_failure_capacity,
            "actor_occupancy_low_word",
            1u,
            final_snapshot->actor_occupancy_low_word) != 0) {
        return -1;
    }
    if (attrition_validate_async_closure(baseline, final_snapshot, out_failure, out_failure_capacity) != 0) {
        return -1;
    }
    return attrition_validate_process_closure(baseline, final_snapshot, out_failure, out_failure_capacity);
}

int main(int argc, char** argv) {
    AttritionCaseOptions defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.case_id = "mixed_runtime_boss";
    defaults.ops = 2048u;
    defaults.seed = 5u;
    defaults.virtual_time_enabled = 1u;
    defaults.virtual_time_step_ms = 1u;
    defaults.time_provenance_required = 1u;
    defaults.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    return attrition_case_main(argc, argv, &defaults, run_lane, validate_lane);
}
