#include "../include/machine_stones.h"

#include <stdint.h>
#include <stdio.h>

static int expect_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "machine-stones test failed: %s\n", label);
        return 1;
    }
    return 0;
}

int main(void) {
    uint64_t tick = 999u;
    int64_t dt_ms = -1;
    uint64_t missed = 999u;
    uint64_t before_count;
    uint64_t after_count;
    uint64_t* lane_ptr;
    void* shatter;
    int payload = 7;
    void* teleported;

    if (expect_true(kain_machine_now_ns() > 0u, "monotonic clock must produce ns")) {
        return 1;
    }
    if (expect_true(kain_machine_axiom_accept("llvm", "", KAIN_MACHINE_CAP_ATOMIC_BITMASK) == 1,
                    "llvm atomic axiom accepted")) {
        return 2;
    }
    if (expect_true(kain_machine_axiom_accept("web", "x86_64", KAIN_MACHINE_CAP_ATOMIC_BITMASK) == 0,
                    "non-native target axiom rejected")) {
        return 3;
    }

    kain_machine_pulse_snapshot(0x1234u, 1u, 0u, &tick, &dt_ms, &missed);
    if (expect_true(dt_ms >= 0, "pulse dt is nonnegative")) {
        return 4;
    }
    if (expect_true(missed == 0u, "first pulse does not report missed beats")) {
        return 5;
    }

    shatter = kain_machine_shatter_alloc(3u, 2u);
    if (expect_true(shatter != 0, "shatter allocation succeeds")) {
        return 6;
    }
    lane_ptr = (uint64_t*)kain_machine_shatter_lane_ptr(shatter, 1u, 1u);
    if (expect_true(lane_ptr != 0, "shatter lane pointer succeeds")) {
        return 7;
    }
    *lane_ptr = 42u;
    if (expect_true(*(uint64_t*)kain_machine_shatter_lane_ptr(shatter, 1u, 1u) == 42u,
                    "shatter lane stores isolate field/index slot")) {
        return 8;
    }
    if (expect_true(kain_machine_shatter_lane_ptr(shatter, 3u, 0u) == 0,
                    "shatter rejects out-of-bounds lane")) {
        return 9;
    }
    kain_machine_shatter_free(shatter);

    before_count = kain_machine_teleport_count();
    teleported = kain_machine_teleport_ptr(&payload, "NativeWorld", "GpuWorld", "gpu_upload");
    after_count = kain_machine_teleport_count();
    if (expect_true(teleported == &payload, "teleport preserves pointer identity")) {
        return 10;
    }
    if (expect_true(after_count == before_count + 1u, "teleport increments handoff telemetry")) {
        return 11;
    }
    if (expect_true(kain_machine_teleport_last_token() != 0u, "teleport records nonzero token")) {
        return 12;
    }
    return 0;
}
