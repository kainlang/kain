/*
 * Test: Ownership Memory Runtime
 *
 * Validates the native collapse/observe/decay guard surface for helper-owned
 * heap allocations and imported/FFI pointers.
 */

#include "../include/kain_runtime_memory.h"
#include "../include/kain_runtime_ownership.h"
#include <stdio.h>

static int expect_status(const char* label, int actual, int expected) {
    if (actual != expected) {
        printf("FAIL: %s expected %d, got %d\n", label, expected, actual);
        return 0;
    }
    return 1;
}

static int expect_ptr(const char* label, const void* ptr) {
    if (ptr == NULL) {
        printf("FAIL: %s returned NULL\n", label);
        return 0;
    }
    return 1;
}

static int test_heap_region_transitions(void) {
    printf("\n=== Test 1: heap allocation ownership transitions ===\n");

    int* value = (int*)__kain_alloc(1, sizeof(int), 1);
    if (!expect_ptr("__kain_alloc", value)) {
        return 0;
    }
    *value = 41;

    if (!expect_status(
            "allocated heap starts idle",
            __kain_ownership_state(value),
            KAIN_OWNERSHIP_STATE_IDLE
        )) {
        return 0;
    }
    if (!expect_status(
            "begin observe",
            __kain_ownership_begin_observe(value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "collapse rejected while observed",
            __kain_ownership_begin_collapse(value),
            KAIN_OWNERSHIP_ERR_OBSERVED
        )) {
        return 0;
    }
    if (!expect_status(
            "end observe",
            __kain_ownership_end_observe(value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "begin collapse",
            __kain_ownership_begin_collapse(value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "observe rejected while collapsed",
            __kain_ownership_begin_observe(value),
            KAIN_OWNERSHIP_ERR_COLLAPSED
        )) {
        return 0;
    }
    if (__kain_realloc(value, 2, sizeof(int), 1) != NULL) {
        printf("FAIL: realloc succeeded while collapsed\n");
        return 0;
    }
    if (!expect_status(
            "end collapse",
            __kain_ownership_end_collapse(value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }

    int* grown = (int*)__kain_realloc(value, 2, sizeof(int), 1);
    if (!expect_ptr("__kain_realloc after collapse end", grown)) {
        return 0;
    }
    value = grown;
    value[1] = 42;

    if (!expect_status("decay heap", __kain_ownership_decay(value), KAIN_OWNERSHIP_OK)) {
        return 0;
    }
    if (!expect_status(
            "heap is terminal after decay",
            __kain_ownership_state(value),
            KAIN_OWNERSHIP_STATE_DECAYED
        )) {
        return 0;
    }
    if (!expect_status(
            "second decay rejected",
            __kain_ownership_decay(value),
            KAIN_OWNERSHIP_ERR_DECAYED
        )) {
        return 0;
    }

    printf("PASS: heap allocation ownership transitions are guarded\n");
    return 1;
}

static int test_imported_region_transitions(void) {
    printf("\n=== Test 2: imported pointer ownership transitions ===\n");

    int local = 7;
    if (!expect_status(
            "register imported",
            __kain_ownership_register_imported(&local, sizeof(local)),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "imported observe",
            __kain_ownership_begin_observe(&local),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "imported end observe",
            __kain_ownership_end_observe(&local),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "imported collapse",
            __kain_ownership_begin_collapse(&local),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "imported end collapse",
            __kain_ownership_end_collapse(&local),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status("imported decay", __kain_ownership_decay(&local), KAIN_OWNERSHIP_OK)) {
        return 0;
    }
    if (!expect_status(
            "imported state after decay",
            __kain_ownership_state(&local),
            KAIN_OWNERSHIP_STATE_DECAYED
        )) {
        return 0;
    }
    if (local != 7) {
        printf("FAIL: imported decay mutated local storage\n");
        return 0;
    }

    printf("PASS: imported pointer ownership transitions preserve foreign storage\n");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 0;

    total++;
    passed += test_heap_region_transitions();
    total++;
    passed += test_imported_region_transitions();

    printf("\nOwnership memory runtime tests: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
