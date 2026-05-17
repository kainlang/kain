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
    char* shared = string_new("attrition-hot-object");
    char* separator = string_new(":");
    uint64_t checksum = 0u;
    uint64_t index;
    if (shared == NULL || separator == NULL) {
        rc_release(shared);
        rc_release(separator);
        attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate hot-object string state");
        return -1;
    }

    for (index = 0u; index < options->ops; ++index) {
        char* digits = to_string((long long)(options->seed ^ (index * 17u + 3u)));
        char* joined;
        if (digits == NULL) {
            rc_release(separator);
            rc_release(shared);
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate decimal text");
            return -1;
        }
        joined = str_concat3(shared, separator, digits);
        if (joined == NULL) {
            rc_release(digits);
            rc_release(separator);
            rc_release(shared);
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate concatenated text");
            return -1;
        }
        rc_retain(shared);
        rc_release(shared);
        checksum ^= (uint64_t)len(joined) + (index << 1u);
        rc_release(digits);
        rc_release(joined);
        if (attrition_sabotage_is(options, "retain_without_release_every_1024") &&
            ((index & 1023u) == 0u)) {
            rc_retain(shared);
        }
        if ((index & 4095u) == 0u) {
            kain_attrition_runtime_note_progress(index, checksum);
        }
    }

    rc_release(separator);
    if (!attrition_sabotage_is(options, "skip_final_release")) {
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
    (void)options;
    if (attrition_validate_time_provenance(
            options,
            baseline,
            final_snapshot,
            out_failure,
            out_failure_capacity) != 0) {
        return -1;
    }
    return attrition_validate_rc_closure(baseline, final_snapshot, out_failure, out_failure_capacity);
}

int main(int argc, char** argv) {
    AttritionCaseOptions defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.case_id = "saturated_rc_hot_object";
    defaults.ops = 100000u;
    defaults.seed = 1u;
    defaults.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    return attrition_case_main(argc, argv, &defaults, run_lane, validate_lane);
}
