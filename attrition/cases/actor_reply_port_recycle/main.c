#include "attrition_harness.h"

static int run_lane(
    const AttritionCaseOptions* options,
    uint64_t* out_checksum,
    char* out_failure,
    size_t out_failure_capacity
) {
    uint64_t checksum = 0u;
    uint64_t index;
    kain_actor_runtime_init();

    for (index = 0u; index < options->ops; ++index) {
        void* port = kain_actor_reply_port_new();
        KainActorRef stale_ref;
        KainActorRef live_ref;
        long long reply_value = (long long)(options->seed + index * 7u);
        long long received_value = 0;
        size_t received_size = 0u;
        if (port == NULL) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to allocate reply port");
            return -1;
        }

        kain_actor_reply_port_actor_ref(port, &stale_ref);
        if (kain_actor_reply_port_send_ref(&stale_ref, &reply_value, sizeof(reply_value)) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to send through live reply port");
            return -1;
        }
        if (kain_actor_reply_port_wait(
                port,
                1000,
                &received_value,
                sizeof(received_value),
                &received_size) != 0 || received_size != sizeof(received_value)) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to receive initial reply port message");
            return -1;
        }
        checksum ^= (uint64_t)received_value;
        kain_actor_reply_port_destroy(port);

        port = kain_actor_reply_port_new();
        if (port == NULL) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to rearm reply port");
            return -1;
        }
        kain_actor_reply_port_actor_ref(port, &live_ref);
        if (kain_actor_reply_port_send_ref(&stale_ref, &reply_value, sizeof(reply_value)) == 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "stale reply-port reference was accepted");
            return -1;
        }
        reply_value ^= (long long)(index << 5u);
        if (kain_actor_reply_port_send_ref(&live_ref, &reply_value, sizeof(reply_value)) != 0) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to send through rearmed reply port");
            return -1;
        }
        if (kain_actor_reply_port_wait(
                port,
                1000,
                &received_value,
                sizeof(received_value),
                &received_size) != 0 || received_size != sizeof(received_value)) {
            attrition_copy_failure(out_failure, out_failure_capacity, "failed to receive rearmed reply port message");
            return -1;
        }
        checksum ^= (uint64_t)received_value ^ (uint64_t)live_ref.generation;
        if (!attrition_sabotage_is(options, "skip_destroy")) {
            kain_actor_reply_port_destroy(port);
        }
        if ((index & 1023u) == 0u) {
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
    (void)baseline;
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
            "pending_mailbox_message_count",
            0u,
            final_snapshot->pending_mailbox_message_count) != 0) {
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
    if (final_snapshot->actor_stale_reject_count < options->ops) {
        attrition_copy_failure(out_failure, out_failure_capacity, "reply-port stale reject counter did not advance");
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    AttritionCaseOptions defaults;
    memset(&defaults, 0, sizeof(defaults));
    defaults.case_id = "actor_reply_port_recycle";
    defaults.ops = 4096u;
    defaults.seed = 3u;
    defaults.determinism_tier = (uint64_t)KAIN_ATTRITION_DETERMINISM_TIER_1;
    return attrition_case_main(argc, argv, &defaults, run_lane, validate_lane);
}
