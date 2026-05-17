#include "attrition_harness.h"

static int run_lane(
    const AttritionCaseOptions* options,
    uint64_t* out_checksum,
    char* out_failure,
    size_t out_failure_capacity
) {
    uint64_t checksum = 0u;
    uint64_t index;
    if (!abi_process_platform_available()) {
        attrition_copy_failure(out_failure, out_failure_capacity, "process attrition lane requires the Windows native process substrate");
        return -1;
    }

    abi_process_reset();
    for (index = 0u; index < options->ops; ++index) {
        int64_t spec_id = abi_process_spec_create("cmd.exe");
        int64_t process_id;
        int64_t wait_status;
        int64_t exit_code;
        if (spec_id < 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }
        if (abi_process_spec_add_arg(spec_id, "/c") != 0 ||
            abi_process_spec_add_arg(spec_id, "exit") != 0 ||
            abi_process_spec_add_arg(spec_id, "/b") != 0 ||
            abi_process_spec_add_arg(spec_id, "0") != 0 ||
            abi_process_spec_set_stdout_mode(spec_id, "null") != 0 ||
            abi_process_spec_set_stderr_mode(spec_id, "null") != 0 ||
            abi_process_spec_set_stdin_mode(spec_id, "null") != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }

        process_id = abi_process_spawn(spec_id);
        if (process_id < 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }
        wait_status = abi_process_wait(process_id, 5000);
        if (wait_status < 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }
        if (wait_status == 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "spawned child did not exit before timeout");
            return -1;
        }
        exit_code = abi_process_exit_code(process_id);
        if (exit_code != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "spawned child exited with non-zero code");
            return -1;
        }
        if (!attrition_sabotage_is(options, "skip_close") && abi_process_close(process_id) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }
        if (!attrition_sabotage_is(options, "skip_close") && abi_process_close(process_id) == 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "stale process close was unexpectedly accepted");
            return -1;
        }
        if (abi_process_spec_destroy(spec_id) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, abi_process_last_error_message());
            return -1;
        }
        if (abi_process_spec_destroy(spec_id) == 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "stale process spec destroy was unexpectedly accepted");
            return -1;
        }
        checksum ^= (uint64_t)process_id + (uint64_t)exit_code + index;
        if ((index & 255u) == 0u) {
            kain_attrition_runtime_note_progress(index, checksum);
        }
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
    (void)options;
    if (attrition_validate_process_closure(baseline, final_snapshot, out_failure, out_failure_capacity) != 0) {
        return -1;
    }
    if (final_snapshot->process_stale_reject_count < options->ops * 2u) {
        attrition_copy_failure(out_failure, out_failure_capacity, "process stale reject counter did not advance enough");
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    AttritionCaseOptions defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.case_id = "process_slot_recycle";
    defaults.ops = 128u;
    defaults.seed = 4u;
    defaults.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    return attrition_case_main(argc, argv, &defaults, run_lane, validate_lane);
}
