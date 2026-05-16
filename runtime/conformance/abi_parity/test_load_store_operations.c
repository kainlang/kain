/*
 * KAIN Runtime ABI Parity Test: Load/Store Operations
 *
 * Tests the canonical low-level memory load/store helpers:
 * - __kain_mem_load - Load value from pointer (raw memory read)
 * - __kain_mem_store - Store value to pointer (raw memory write)
 *
 * Tests various data types, sizes, and alignment scenarios.
 *
 * Requirements: 3.2, 3.3, 3.4, 13.1, 13.6
 */

#include "../../native/include/memory.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name, ...) do { printf("  ❌ FAIL: " name "\n", ##__VA_ARGS__); return 0; } while(0)

int test_load_store_int8(void) {
    printf("\nTest 1: Load/Store int8_t\n");

    int8_t source = -42;
    int8_t dest = 0;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(int8_t));
    if (dest != -42) {
        TEST_FAIL("mem_load int8_t returned wrong value: %d", dest);
    }

    /* Test store */
    int8_t new_value = 100;
    __kain_mem_store(&dest, &new_value, sizeof(int8_t));
    if (dest != 100) {
        TEST_FAIL("mem_store int8_t did not update value: %d", dest);
    }

    TEST_PASS("Load/store int8_t works correctly");
    return 1;
}

int test_load_store_int16(void) {
    printf("\nTest 2: Load/Store int16_t\n");

    int16_t source = -12345;
    int16_t dest = 0;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(int16_t));
    if (dest != -12345) {
        TEST_FAIL("mem_load int16_t returned wrong value: %d", dest);
    }

    /* Test store */
    int16_t new_value = 30000;
    __kain_mem_store(&dest, &new_value, sizeof(int16_t));
    if (dest != 30000) {
        TEST_FAIL("mem_store int16_t did not update value: %d", dest);
    }

    TEST_PASS("Load/store int16_t works correctly");
    return 1;
}

int test_load_store_int32(void) {
    printf("\nTest 3: Load/Store int32_t\n");

    int32_t source = -123456789;
    int32_t dest = 0;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(int32_t));
    if (dest != -123456789) {
        TEST_FAIL("mem_load int32_t returned wrong value: %d", dest);
    }

    /* Test store */
    int32_t new_value = 2000000000;
    __kain_mem_store(&dest, &new_value, sizeof(int32_t));
    if (dest != 2000000000) {
        TEST_FAIL("mem_store int32_t did not update value: %d", dest);
    }

    TEST_PASS("Load/store int32_t works correctly");
    return 1;
}

int test_load_store_int64(void) {
    printf("\nTest 4: Load/Store int64_t\n");

    int64_t source = -9223372036854775807LL;
    int64_t dest = 0;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(int64_t));
    if (dest != -9223372036854775807LL) {
        TEST_FAIL("mem_load int64_t returned wrong value: %lld", (long long)dest);
    }

    /* Test store */
    int64_t new_value = 9223372036854775806LL;
    __kain_mem_store(&dest, &new_value, sizeof(int64_t));
    if (dest != 9223372036854775806LL) {
        TEST_FAIL("mem_store int64_t did not update value: %lld", (long long)dest);
    }

    TEST_PASS("Load/store int64_t works correctly");
    return 1;
}

int test_load_store_float(void) {
    printf("\nTest 5: Load/Store float\n");

    float source = 3.14159f;
    float dest = 0.0f;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(float));
    if (fabsf(dest - 3.14159f) > 0.00001f) {
        TEST_FAIL("mem_load float returned wrong value: %f", dest);
    }

    /* Test store */
    float new_value = -2.71828f;
    __kain_mem_store(&dest, &new_value, sizeof(float));
    if (fabsf(dest - (-2.71828f)) > 0.00001f) {
        TEST_FAIL("mem_store float did not update value: %f", dest);
    }

    TEST_PASS("Load/store float works correctly");
    return 1;
}

int test_load_store_double(void) {
    printf("\nTest 6: Load/Store double\n");

    double source = 3.141592653589793;
    double dest = 0.0;

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(double));
    if (fabs(dest - 3.141592653589793) > 0.0000000001) {
        TEST_FAIL("mem_load double returned wrong value: %f", dest);
    }

    /* Test store */
    double new_value = -2.718281828459045;
    __kain_mem_store(&dest, &new_value, sizeof(double));
    if (fabs(dest - (-2.718281828459045)) > 0.0000000001) {
        TEST_FAIL("mem_store double did not update value: %f", dest);
    }

    TEST_PASS("Load/store double works correctly");
    return 1;
}

int test_load_store_struct(void) {
    printf("\nTest 7: Load/Store struct\n");

    typedef struct {
        int32_t a;
        int64_t b;
        float c;
        int16_t d;
    } TestStruct;

    TestStruct source = {42, 1000000000LL, 3.14f, 999};
    TestStruct dest;
    memset(&dest, 0, sizeof(TestStruct));

    /* Test load */
    __kain_mem_load(&source, &dest, sizeof(TestStruct));
    if (dest.a != 42 || dest.b != 1000000000LL ||
        fabsf(dest.c - 3.14f) > 0.001f || dest.d != 999) {
        TEST_FAIL("mem_load struct did not copy all fields correctly");
    }

    /* Test store */
    TestStruct new_value = {100, 2000000000LL, 2.71f, 500};
    __kain_mem_store(&dest, &new_value, sizeof(TestStruct));
    if (dest.a != 100 || dest.b != 2000000000LL ||
        fabsf(dest.c - 2.71f) > 0.001f || dest.d != 500) {
        TEST_FAIL("mem_store struct did not update all fields correctly");
    }

    TEST_PASS("Load/store struct works correctly");
    return 1;
}

int test_load_store_array(void) {
    printf("\nTest 8: Load/Store array\n");

    int32_t source[5] = {10, 20, 30, 40, 50};
    int32_t dest[5] = {0};

    /* Test load */
    __kain_mem_load(source, dest, sizeof(source));
    for (int i = 0; i < 5; i++) {
        if (dest[i] != source[i]) {
            TEST_FAIL("mem_load array element %d incorrect: %d", i, dest[i]);
        }
    }

    /* Test store */
    int32_t new_array[5] = {100, 200, 300, 400, 500};
    __kain_mem_store(dest, new_array, sizeof(new_array));
    for (int i = 0; i < 5; i++) {
        if (dest[i] != new_array[i]) {
            TEST_FAIL("mem_store array element %d incorrect: %d", i, dest[i]);
        }
    }

    TEST_PASS("Load/store array works correctly");
    return 1;
}

int test_load_store_partial(void) {
    printf("\nTest 9: Load/Store partial data\n");

    int64_t source = 0x123456789ABCDEF0LL;
    int32_t dest = 0;

    /* Load only first 4 bytes */
    __kain_mem_load(&source, &dest, sizeof(int32_t));

    /* On little-endian, should get lower 32 bits */
    /* On big-endian, should get upper 32 bits */
    /* We just verify that something was loaded */
    if (dest == 0) {
        TEST_FAIL("mem_load partial did not load any data");
    }

    /* Store partial data */
    int64_t large_dest = 0xFFFFFFFFFFFFFFFFLL;
    int32_t partial_value = 0x12345678;
    __kain_mem_store(&large_dest, &partial_value, sizeof(int32_t));

    /* Verify that at least the first 4 bytes changed */
    if (large_dest == 0xFFFFFFFFFFFFFFFFLL) {
        TEST_FAIL("mem_store partial did not modify any data");
    }

    TEST_PASS("Load/store partial data works");
    return 1;
}

int test_load_store_zero_size(void) {
    printf("\nTest 10: Load/Store zero size\n");

    int32_t source = 42;
    int32_t dest = 100;

    /* Load zero bytes should not change dest */
    __kain_mem_load(&source, &dest, 0);
    if (dest != 100) {
        TEST_FAIL("mem_load with size 0 modified destination");
    }

    /* Store zero bytes should not change dest */
    int32_t new_value = 999;
    __kain_mem_store(&dest, &new_value, 0);
    if (dest != 100) {
        TEST_FAIL("mem_store with size 0 modified destination");
    }

    TEST_PASS("Load/store with zero size works correctly");
    return 1;
}

int test_load_store_bit_pattern_preservation(void) {
    printf("\nTest 11: Bit Pattern Preservation\n");

    /* Test that exact bit patterns are preserved (important for unions/bitfields) */
    uint64_t source = 0xDEADBEEFCAFEBABEULL;
    uint64_t dest = 0;

    __kain_mem_load(&source, &dest, sizeof(uint64_t));
    if (dest != 0xDEADBEEFCAFEBABEULL) {
        TEST_FAIL("mem_load did not preserve bit pattern: 0x%llX",
                  (unsigned long long)dest);
    }

    /* Test with NaN float (special bit pattern) */
    uint32_t nan_bits = 0x7FC00000; /* Quiet NaN */
    float nan_float;
    __kain_mem_load(&nan_bits, &nan_float, sizeof(float));

    uint32_t loaded_bits;
    __kain_mem_load(&nan_float, &loaded_bits, sizeof(uint32_t));

    if (loaded_bits != nan_bits) {
        TEST_FAIL("mem_load/store did not preserve NaN bit pattern: 0x%X", loaded_bits);
    }

    TEST_PASS("Bit pattern preservation works correctly");
    return 1;
}

int test_load_store_overlapping_safe(void) {
    printf("\nTest 12: Load/Store with same source/dest\n");

    /* Test that load/store can handle source == dest safely */
    int32_t value = 42;

    /* Load from value to itself (should be no-op or safe) */
    __kain_mem_load(&value, &value, sizeof(int32_t));
    if (value != 42) {
        TEST_FAIL("mem_load with same src/dest corrupted value");
    }

    /* Store from value to itself (should be no-op or safe) */
    __kain_mem_store(&value, &value, sizeof(int32_t));
    if (value != 42) {
        TEST_FAIL("mem_store with same src/dest corrupted value");
    }

    TEST_PASS("Load/store with same source/dest is safe");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 12;

    printf("=== KAIN Runtime ABI Parity Test: Load/Store Operations ===\n");

    if (test_load_store_int8()) passed++;
    if (test_load_store_int16()) passed++;
    if (test_load_store_int32()) passed++;
    if (test_load_store_int64()) passed++;
    if (test_load_store_float()) passed++;
    if (test_load_store_double()) passed++;
    if (test_load_store_struct()) passed++;
    if (test_load_store_array()) passed++;
    if (test_load_store_partial()) passed++;
    if (test_load_store_zero_size()) passed++;
    if (test_load_store_bit_pattern_preservation()) passed++;
    if (test_load_store_overlapping_safe()) passed++;

    printf("\n=== Test Results: %d/%d Passed ===\n", passed, total);

    return (passed == total) ? 0 : 1;
}
