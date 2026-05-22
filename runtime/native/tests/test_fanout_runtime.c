#include "../include/fanout.h"
#include "../include/memory.h"
#include "../include/ownership.h"

#include <stdint.h>
#include <stdio.h>

typedef struct FanoutCounterCtx {
    int64_t* counter;
} FanoutCounterCtx;

static void increment_counter(void* raw_ctx, int64_t index) {
    FanoutCounterCtx* ctx = (FanoutCounterCtx*)raw_ctx;
    (void)index;
    __kain_atomic_add_seqcst(ctx->counter, 1);
}

int main(void) {
    const int64_t worker_count = 100;
    const int64_t rounds = 8;
    int64_t* counter = (int64_t*)__kain_alloc(1, sizeof(int64_t), 1);
    if (counter == NULL) {
        printf("FAIL: __kain_alloc returned NULL\n");
        return 1;
    }

    FanoutCounterCtx ctx = {counter};
    for (int64_t round = 0; round < rounds; ++round) {
        if (__kain_ownership_begin_share_helper(counter) != KAIN_OWNERSHIP_OK) {
            printf("FAIL: begin share helper rejected counter on round %lld\n", (long long)round);
            return 1;
        }
        if (__kain_fanout_i64(0, worker_count, &ctx, increment_counter) != 0) {
            printf("FAIL: __kain_fanout_i64 returned non-zero status on round %lld\n", (long long)round);
            return 1;
        }
        if (__kain_ownership_end_share_helper(counter) != KAIN_OWNERSHIP_OK) {
            printf("FAIL: end share helper rejected counter on round %lld\n", (long long)round);
            return 1;
        }

        int64_t final_value = __kain_atomic_load_seqcst(counter);
        int64_t expected = (round + 1) * worker_count;
        if (final_value != expected) {
            printf(
                "FAIL: expected %lld after round %lld, got %lld\n",
                (long long)expected,
                (long long)round,
                (long long)final_value
            );
            return 1;
        }
    }

    if (__kain_ownership_decay_helper(counter) != KAIN_OWNERSHIP_OK) {
        printf("FAIL: decay helper rejected counter\n");
        return 1;
    }

    kain_fanout_runtime_shutdown();
    printf(
        "PASS: native fanout runtime reused worker helpers across %lld rounds of %lld slices\n",
        (long long)rounds,
        (long long)worker_count
    );
    return 0;
}
