#include "../include/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

static int expect_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "atomic-memory test failed: %s\n", label);
        return 0;
    }
    return 1;
}

static int expect_i64(int64_t actual, int64_t expected, const char* label) {
    if (actual != expected) {
        fprintf(
            stderr,
            "atomic-memory test failed: %s (expected %lld, got %lld)\n",
            label,
            (long long)expected,
            (long long)actual
        );
        return 0;
    }
    return 1;
}

static int test_invalid_store_orderings_are_canonicalized(void) {
    atomic_int_least64_t cell;
    atomic_init(&cell, 0);

    __kain_atomic_store_ordered((void*)&cell, 17, KAIN_MEMORY_ORDER_ACQUIRE);
    if (!expect_i64(
            __kain_atomic_load_seqcst((const void*)&cell),
            17,
            "acquire store ordering still commits through the ABI helper"
        )) {
        return 0;
    }

    __kain_atomic_store_ordered((void*)&cell, 29, KAIN_MEMORY_ORDER_ACQ_REL);
    if (!expect_i64(
            __kain_atomic_load_seqcst((const void*)&cell),
            29,
            "acq_rel store ordering still commits through the ABI helper"
        )) {
        return 0;
    }

    return 1;
}

static int test_compare_exchange_invalid_failure_orderings_do_not_break_runtime(void) {
    atomic_int_least64_t cell;
    atomic_init(&cell, 19);

    if (!expect_true(
            __kain_atomic_compare_exchange_ordered(
                (void*)&cell,
                0,
                23,
                KAIN_MEMORY_ORDER_RELAXED,
                KAIN_MEMORY_ORDER_SEQ_CST
            ) == 0,
            "stronger-than-success failure ordering returns false on mismatch"
        )) {
        return 0;
    }
    if (!expect_i64(
            __kain_atomic_load_seqcst((const void*)&cell),
            19,
            "failed compare_exchange leaves cell unchanged"
        )) {
        return 0;
    }

    if (!expect_true(
            __kain_atomic_compare_exchange_ordered(
                (void*)&cell,
                19,
                23,
                KAIN_MEMORY_ORDER_RELAXED,
                KAIN_MEMORY_ORDER_SEQ_CST
            ) == 1,
            "clamped compare_exchange still succeeds on a matching expected value"
        )) {
        return 0;
    }
    if (!expect_i64(
            __kain_atomic_load_seqcst((const void*)&cell),
            23,
            "successful compare_exchange publishes desired value"
        )) {
        return 0;
    }

    if (!expect_true(
            __kain_atomic_compare_exchange_ordered(
                (void*)&cell,
                99,
                31,
                KAIN_MEMORY_ORDER_SEQ_CST,
                KAIN_MEMORY_ORDER_RELEASE
            ) == 0,
            "release failure ordering is normalized for mismatch paths"
        )) {
        return 0;
    }
    if (!expect_i64(
            __kain_atomic_load_seqcst((const void*)&cell),
            23,
            "normalized failure ordering does not corrupt the cell"
        )) {
        return 0;
    }

    return 1;
}

int main(void) {
    int passed = 0;
    int total = 0;

    total++;
    passed += test_invalid_store_orderings_are_canonicalized();
    total++;
    passed += test_compare_exchange_invalid_failure_orderings_do_not_break_runtime();

    printf("\nAtomic memory ordering runtime tests: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
