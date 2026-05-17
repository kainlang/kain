#include "attrition_harness.h"

static int run_lane(
    const AttritionCaseOptions* options,
    uint64_t* out_checksum,
    char* out_failure,
    size_t out_failure_capacity
) {
    uint64_t checksum = 0u;
    uint64_t index;
    if (!options->virtual_time_enabled) {
        attrition_copy_failure(out_failure, out_failure_capacity, "virtual_time_async_timer requires virtual time");
        return -1;
    }

    for (index = 0u; index < options->ops; ++index) {
        KainDiagnostic diag;
        KainTaskId task_id = kain_async_sleep((index & 7u) + 1u, &diag);
        void* result = NULL;
        if (task_id == KAIN_TASK_ID_INVALID) {
            attrition_copy_failure(out_failure, out_failure_capacity, diag.message);
            return -1;
        }
        if (kain_task_await(task_id, &result, &diag) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, diag.message);
            return -1;
        }
        if (!attrition_sabotage_is(options, "skip_task_dispose") &&
            kain_attrition_async_dispose_task(task_id) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to dispose completed async task");
            return -1;
        }
        checksum ^= (uint64_t)task_id + (index << 3u);

        if ((index & 7u) == 0u) {
            KainTaskId cancelled_task_id = kain_async_sleep(32u + (index & 15u), &diag);
            if (cancelled_task_id == KAIN_TASK_ID_INVALID) {
                attrition_copy_failure(out_failure, out_failure_capacity, diag.message);
                return -1;
            }
            if (kain_task_cancel(cancelled_task_id, &diag) != 0) {
                attrition_copy_failure(out_failure, out_failure_capacity, diag.message);
                return -1;
            }
            if (!attrition_sabotage_is(options, "skip_task_dispose") &&
                kain_attrition_async_dispose_task(cancelled_task_id) != 0) {
                attrition_copy_failure(out_failure, out_failure_capacity, "failed to dispose cancelled async task");
                return -1;
            }
            checksum ^= (uint64_t)cancelled_task_id << 1u;
        }

        if ((index & 2047u) == 0u) {
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
    if (attrition_validate_time_provenance(
            options,
            baseline,
            final_snapshot,
            out_failure,
            out_failure_capacity) != 0) {
        return -1;
    }
    return attrition_validate_async_closure(baseline, final_snapshot, out_failure, out_failure_capacity);
}

int main(int argc, char** argv) {
    AttritionCaseOptions defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.case_id = "virtual_time_async_timer";
    defaults.ops = 32768u;
    defaults.seed = 2u;
    defaults.virtual_time_enabled = 1u;
    defaults.virtual_time_step_ms = 1u;
    defaults.time_provenance_required = 1u;
    defaults.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    return attrition_case_main(argc, argv, &defaults, run_lane, validate_lane);
}
