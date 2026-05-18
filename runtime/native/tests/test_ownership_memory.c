/*
 * Test: Ownership Memory Runtime
 *
 * Validates the native collapse/observe/decay guard surface for helper-owned
 * heap allocations and imported/FFI pointers.
 */

#include "../include/memory.h"
#include "../include/ownership.h"
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

typedef struct SpoofedHelperPrefixCell {
    uint64_t fake_magic_and_slot;
    size_t fake_payload_size;
    int value;
} SpoofedHelperPrefixCell;

static int test_imported_path_ignores_spoofed_helper_header(void) {
    printf("\n=== Test 3: imported path ignores spoofed helper header bytes ===\n");

    SpoofedHelperPrefixCell spoofed = {0};
    spoofed.fake_magic_and_slot = __kain_alloc_header_magic_with_slot(1u);
    spoofed.fake_payload_size = sizeof(int);
    spoofed.value = 19;

    if (!expect_status(
            "ensure imported for spoofed pointer",
            __kain_ownership_ensure_imported(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "helper fast path rejects spoofed imported pointer",
            __kain_ownership_begin_observe_helper(&spoofed.value),
            KAIN_OWNERSHIP_ERR_NOT_FOUND
        )) {
        return 0;
    }
    if (!expect_status(
            "generic observe uses imported registry path",
            __kain_ownership_begin_observe(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "generic end observe uses imported registry path",
            __kain_ownership_end_observe(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "generic collapse uses imported registry path",
            __kain_ownership_begin_collapse(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "generic end collapse uses imported registry path",
            __kain_ownership_end_collapse(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "generic decay uses imported registry path",
            __kain_ownership_decay(&spoofed.value),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }
    if (!expect_status(
            "spoofed imported pointer reaches decayed state",
            __kain_ownership_state(&spoofed.value),
            KAIN_OWNERSHIP_STATE_DECAYED
        )) {
        return 0;
    }
    if (spoofed.value != 19) {
        printf("FAIL: spoofed imported decay mutated local storage\n");
        return 0;
    }

    printf("PASS: imported registry path ignores spoofed helper-looking prefixes\n");
    return 1;
}

static int test_helper_decay_reclaims_registry_slots(void) {
    printf("\n=== Test 4: helper decay reclaims registry slots ===\n");

    uint16_t first_slot_token = 0u;
    int saw_reuse = 0;
    for (int i = 0; i < 5000; ++i) {
        int* value = (int*)__kain_alloc(8, sizeof(int), 1);
        if (!expect_ptr("helper reclaim allocation", value)) {
            return 0;
        }
        KainAllocHeader* header = __kain_alloc_header_from_payload(value);
        uint16_t slot_token = __kain_alloc_header_slot_token(header);
        if (slot_token == 0u) {
            printf("FAIL: helper allocation did not receive a slot token\n");
            return 0;
        }
        if (i == 0) {
            first_slot_token = slot_token;
        } else if (slot_token == first_slot_token) {
            saw_reuse = 1;
        }
        value[0] = i;
        if (!expect_status(
                "helper decay",
                __kain_ownership_decay_helper(value),
                KAIN_OWNERSHIP_OK
            )) {
            return 0;
        }
        if (!expect_status(
                "decayed helper token no longer resolves",
                __kain_ownership_helper_allocation_state(value, slot_token),
                KAIN_OWNERSHIP_ERR_NOT_FOUND
            )) {
            return 0;
        }
    }

    if (!saw_reuse) {
        printf("FAIL: helper registry slots were not observed being reused\n");
        return 0;
    }

    printf("PASS: helper decay reclaims helper-owned registry slots\n");
    return 1;
}

static int test_large_zeroed_alloc_reuses_clean_cached_block(void) {
    printf("\n=== Test 5: large zeroed helper allocation cache preserves zero-fill ===\n");

    int* first = (int*)__kain_alloc(512, sizeof(int), 1);
    if (!expect_ptr("first large zeroed allocation", first)) {
        return 0;
    }
    first[0] = 111;
    first[256] = 222;
    first[511] = 333;
    if (!expect_status(
            "decay first cached candidate",
            __kain_ownership_decay_helper(first),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }

    int* second = (int*)__kain_alloc(512, sizeof(int), 1);
    if (!expect_ptr("second large zeroed allocation", second)) {
        return 0;
    }
    if (second != first) {
        printf("FAIL: large allocation cache did not reuse exact-size block\n");
        return 0;
    }
    if (second[0] != 0 || second[256] != 0 || second[511] != 0) {
        printf("FAIL: cached zeroed allocation leaked prior contents\n");
        return 0;
    }
    if (!expect_status(
            "decay reused cached block",
            __kain_ownership_decay_helper(second),
            KAIN_OWNERSHIP_OK
        )) {
        return 0;
    }

    printf("PASS: large cached helper allocations keep alloc_zeroed semantics\n");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 0;

    total++;
    passed += test_heap_region_transitions();
    total++;
    passed += test_imported_region_transitions();
    total++;
    passed += test_imported_path_ignores_spoofed_helper_header();
    total++;
    passed += test_helper_decay_reclaims_registry_slots();
    total++;
    passed += test_large_zeroed_alloc_reuses_clean_cached_block();

    printf("\nOwnership memory runtime tests: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
