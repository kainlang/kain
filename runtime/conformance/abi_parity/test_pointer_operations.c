/*
 * KAIN Runtime ABI Parity Test: Pointer Operations
 *
 * Tests the canonical low-level pointer helper implementations:
 * - __kain_bind_local - Create pointer binding to local variable
 * - __kain_addr_of - Take address of value expression
 * - __kain_ptr_offset - Pointer arithmetic with explicit stride
 * - __kain_field_ptr - Compute pointer to struct field
 * - __kain_index_ptr - Compute pointer to array element
 *
 * Requirements: 3.2, 3.3, 3.4, 13.1, 13.6
 */

#include "../../native/include/memory.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name, ...) do { printf("  ❌ FAIL: " name "\n", ##__VA_ARGS__); return 0; } while(0)

/* Test struct for field pointer operations */
typedef struct {
    int32_t field_a;      /* offset 0 */
    int64_t field_b;      /* offset 8 (aligned) */
    int16_t field_c;      /* offset 16 */
    int8_t field_d;       /* offset 18 */
} TestStruct;

int test_bind_local(void) {
    printf("\nTest 1: __kain_bind_local\n");

    int32_t value = 42;
    void* ptr = __kain_bind_local(&value);

    if (ptr == NULL) {
        TEST_FAIL("bind_local returned NULL");
    }

    if (ptr != &value) {
        TEST_FAIL("bind_local returned different address than input");
    }

    /* Verify we can read through the pointer */
    int32_t read_value = *(int32_t*)ptr;
    if (read_value != 42) {
        TEST_FAIL("Read through bound pointer returned wrong value: %d", read_value);
    }

    /* Verify we can write through the pointer */
    *(int32_t*)ptr = 100;
    if (value != 100) {
        TEST_FAIL("Write through bound pointer did not update original value");
    }

    TEST_PASS("bind_local works correctly");
    return 1;
}

int test_addr_of(void) {
    printf("\nTest 2: __kain_addr_of\n");

    int64_t value = 0x123456789ABCDEF0LL;
    void* ptr = __kain_addr_of(&value, sizeof(value));

    if (ptr == NULL) {
        TEST_FAIL("addr_of returned NULL");
    }

    /* Verify we can read through the pointer */
    int64_t read_value = *(int64_t*)ptr;
    if (read_value != 0x123456789ABCDEF0LL) {
        TEST_FAIL("Read through addr_of pointer returned wrong value: 0x%llX",
                  (unsigned long long)read_value);
    }

    /* Test with different sizes */
    int8_t small_value = 42;
    void* small_ptr = __kain_addr_of(&small_value, sizeof(small_value));
    if (small_ptr == NULL) {
        TEST_FAIL("addr_of returned NULL for small value");
    }

    if (*(int8_t*)small_ptr != 42) {
        TEST_FAIL("addr_of failed for small value");
    }

    TEST_PASS("addr_of works correctly");
    return 1;
}

int test_ptr_offset(void) {
    printf("\nTest 3: __kain_ptr_offset\n");

    int32_t array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    void* base = array;

    /* Test positive offset */
    void* offset_ptr = __kain_ptr_offset(base, 5, sizeof(int32_t));
    if (offset_ptr == NULL) {
        TEST_FAIL("ptr_offset returned NULL for positive offset");
    }

    int32_t value = *(int32_t*)offset_ptr;
    if (value != 5) {
        TEST_FAIL("ptr_offset positive offset returned wrong value: %d", value);
    }

    /* Test negative offset */
    void* mid_ptr = __kain_ptr_offset(base, 5, sizeof(int32_t));
    void* back_ptr = __kain_ptr_offset(mid_ptr, -3, sizeof(int32_t));
    value = *(int32_t*)back_ptr;
    if (value != 2) {
        TEST_FAIL("ptr_offset negative offset returned wrong value: %d", value);
    }

    /* Test zero offset */
    void* zero_ptr = __kain_ptr_offset(base, 0, sizeof(int32_t));
    if (zero_ptr != base) {
        TEST_FAIL("ptr_offset with zero offset should return same pointer");
    }

    /* Test with different stride */
    int64_t large_array[5] = {100, 200, 300, 400, 500};
    void* large_base = large_array;
    void* large_offset = __kain_ptr_offset(large_base, 3, sizeof(int64_t));
    int64_t large_value = *(int64_t*)large_offset;
    if (large_value != 400) {
        TEST_FAIL("ptr_offset with large stride returned wrong value: %lld",
                  (long long)large_value);
    }

    TEST_PASS("ptr_offset works correctly");
    return 1;
}

int test_field_ptr(void) {
    printf("\nTest 4: __kain_field_ptr\n");

    TestStruct obj;
    obj.field_a = 10;
    obj.field_b = 20;
    obj.field_c = 30;
    obj.field_d = 40;

    void* base = &obj;

    /* Test field_a (offset 0) */
    void* field_a_ptr = __kain_field_ptr(base, "field_a", 0);
    if (field_a_ptr == NULL) {
        TEST_FAIL("field_ptr returned NULL for field_a");
    }

    int32_t field_a_value = *(int32_t*)field_a_ptr;
    if (field_a_value != 10) {
        TEST_FAIL("field_ptr for field_a returned wrong value: %d", field_a_value);
    }

    /* Test field_b (offset 8) */
    void* field_b_ptr = __kain_field_ptr(base, "field_b", 8);
    if (field_b_ptr == NULL) {
        TEST_FAIL("field_ptr returned NULL for field_b");
    }

    int64_t field_b_value = *(int64_t*)field_b_ptr;
    if (field_b_value != 20) {
        TEST_FAIL("field_ptr for field_b returned wrong value: %lld",
                  (long long)field_b_value);
    }

    /* Test field_c (offset 16) */
    void* field_c_ptr = __kain_field_ptr(base, "field_c", 16);
    int16_t field_c_value = *(int16_t*)field_c_ptr;
    if (field_c_value != 30) {
        TEST_FAIL("field_ptr for field_c returned wrong value: %d", field_c_value);
    }

    /* Test field_d (offset 18) */
    void* field_d_ptr = __kain_field_ptr(base, "field_d", 18);
    int8_t field_d_value = *(int8_t*)field_d_ptr;
    if (field_d_value != 40) {
        TEST_FAIL("field_ptr for field_d returned wrong value: %d", field_d_value);
    }

    /* Verify we can write through field pointers */
    *(int32_t*)field_a_ptr = 100;
    if (obj.field_a != 100) {
        TEST_FAIL("Write through field_a pointer did not update struct");
    }

    TEST_PASS("field_ptr works correctly");
    return 1;
}

int test_index_ptr(void) {
    printf("\nTest 5: __kain_index_ptr\n");

    int32_t array[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    void* base = array;

    /* Test various indices */
    for (int i = 0; i < 10; i++) {
        void* elem_ptr = __kain_index_ptr(base, i, sizeof(int32_t));
        if (elem_ptr == NULL) {
            TEST_FAIL("index_ptr returned NULL for index %d", i);
        }

        int32_t value = *(int32_t*)elem_ptr;
        int32_t expected = (i + 1) * 10;
        if (value != expected) {
            TEST_FAIL("index_ptr for index %d returned wrong value: %d (expected %d)",
                      i, value, expected);
        }
    }

    /* Test negative index (pointer arithmetic) */
    void* mid_ptr = __kain_index_ptr(base, 5, sizeof(int32_t));
    void* back_ptr = __kain_index_ptr(mid_ptr, -2, sizeof(int32_t));
    int32_t back_value = *(int32_t*)back_ptr;
    if (back_value != 40) {
        TEST_FAIL("index_ptr with negative index returned wrong value: %d", back_value);
    }

    /* Test with different element size */
    int64_t large_array[5] = {1000, 2000, 3000, 4000, 5000};
    void* large_base = large_array;
    void* large_elem = __kain_index_ptr(large_base, 3, sizeof(int64_t));
    int64_t large_value = *(int64_t*)large_elem;
    if (large_value != 4000) {
        TEST_FAIL("index_ptr with large elements returned wrong value: %lld",
                  (long long)large_value);
    }

    /* Verify we can write through index pointers */
    void* write_ptr = __kain_index_ptr(base, 7, sizeof(int32_t));
    *(int32_t*)write_ptr = 999;
    if (array[7] != 999) {
        TEST_FAIL("Write through index pointer did not update array");
    }

    TEST_PASS("index_ptr works correctly");
    return 1;
}

int test_pointer_arithmetic_consistency(void) {
    printf("\nTest 6: Pointer Arithmetic Consistency\n");

    int32_t array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    void* base = array;

    /* ptr_offset and index_ptr should produce identical results */
    for (int i = 0; i < 10; i++) {
        void* offset_ptr = __kain_ptr_offset(base, i, sizeof(int32_t));
        void* index_ptr = __kain_index_ptr(base, i, sizeof(int32_t));

        if (offset_ptr != index_ptr) {
            TEST_FAIL("ptr_offset and index_ptr produced different pointers for index %d", i);
        }

        int32_t offset_value = *(int32_t*)offset_ptr;
        int32_t index_value = *(int32_t*)index_ptr;

        if (offset_value != index_value || offset_value != i) {
            TEST_FAIL("ptr_offset and index_ptr produced different values for index %d", i);
        }
    }

    TEST_PASS("ptr_offset and index_ptr are consistent");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 6;

    printf("=== KAIN Runtime ABI Parity Test: Pointer Operations ===\n");

    if (test_bind_local()) passed++;
    if (test_addr_of()) passed++;
    if (test_ptr_offset()) passed++;
    if (test_field_ptr()) passed++;
    if (test_index_ptr()) passed++;
    if (test_pointer_arithmetic_consistency()) passed++;

    printf("\n=== Test Results: %d/%d Passed ===\n", passed, total);

    return (passed == total) ? 0 : 1;
}
