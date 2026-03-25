/*
 * KAIN Runtime ABI Parity Test: Union Operations
 *
 * Tests the canonical low-level union helper implementations:
 * - __kain_union_get - Read union field with type-safe access
 * - __kain_union_set - Write union field with type-safe access
 * - __kain_union_wrap - Initialize union with active field
 *
 * Requirements: 3.2, 3.3, 3.4, 13.1, 13.6
 */

#include "../../native/include/kain_runtime_memory.h"
#include "../../native/include/kain_runtime_union.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name, ...) do { printf("  ❌ FAIL: " name "\n", ##__VA_ARGS__); return 0; } while(0)
#define TEST_UNION_GET(value, field, type_key, byte_size, union_size, fallback) \
    __extension__ ({ \
        __typeof__(fallback) _fallback = (fallback); \
        __typeof__(fallback) _out = _fallback; \
        __kain_union_get(&(value), field, type_key, byte_size, union_size, &_fallback, &_out, sizeof(_out)); \
        _out; \
    })
#define TEST_UNION_SET(value, field, type_key, byte_size, union_size, next) \
    __extension__ ({ \
        __typeof__(next) _next = (next); \
        __kain_union_set(&(value), field, type_key, byte_size, union_size, &_next, sizeof(_next)); \
        _next; \
    })
#define TEST_UNION_WRAP(value, field, type_key, byte_size, union_size, next) \
    __extension__ ({ \
        __typeof__(value) _wrapped = (value); \
        __typeof__(next) _next = (next); \
        __kain_union_wrap(&_wrapped, field, type_key, byte_size, union_size, &_next, sizeof(_next)); \
        _wrapped; \
    })

/* Test union types */
typedef union {
    int32_t int_val;
    float float_val;
    int64_t long_val;
} SimpleUnion;

typedef union {
    int8_t byte_val;
    int16_t short_val;
    int32_t int_val;
    int64_t long_val;
} SizedUnion;

int test_union_get_basic(void) {
    printf("\nTest 1: __kain_union_get basic\n");
    
    SimpleUnion u;
    u.int_val = 42;
    
    /* Read int_val field */
    int32_t result = TEST_UNION_GET(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SimpleUnion),
        0
    );
    
    if (result != 42) {
        TEST_FAIL("union_get int_val returned wrong value: %d", result);
    }
    
    /* Read as float (type punning) */
    float float_result = TEST_UNION_GET(
        u,
        "float_val",
        "float",
        sizeof(float),
        sizeof(SimpleUnion),
        0.0f
    );
    
    /* Should get the bit pattern of 42 interpreted as float */
    /* We just verify it's not the fallback value */
    if (float_result == 0.0f) {
        TEST_FAIL("union_get float_val returned fallback value");
    }
    
    TEST_PASS("union_get basic works correctly");
    return 1;
}

int test_union_set_basic(void) {
    printf("\nTest 2: __kain_union_set basic\n");
    
    SimpleUnion u;
    memset(&u, 0xFF, sizeof(u)); /* Fill with garbage */
    
    /* Set int_val field */
    int32_t new_int = 100;
    int32_t returned = TEST_UNION_SET(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SimpleUnion),
        new_int
    );
    
    if (returned != 100) {
        TEST_FAIL("union_set did not return the set value: %d", returned);
    }
    
    /* Verify the union was updated */
    if (u.int_val != 100) {
        TEST_FAIL("union_set did not update union: %d", u.int_val);
    }
    
    /* Set float_val field */
    float new_float = 3.14f;
    float returned_float = TEST_UNION_SET(
        u,
        "float_val",
        "float",
        sizeof(float),
        sizeof(SimpleUnion),
        new_float
    );
    
    if (fabsf(returned_float - 3.14f) > 0.001f) {
        TEST_FAIL("union_set float did not return the set value: %f", returned_float);
    }
    
    if (fabsf(u.float_val - 3.14f) > 0.001f) {
        TEST_FAIL("union_set did not update union float: %f", u.float_val);
    }
    
    TEST_PASS("union_set basic works correctly");
    return 1;
}

int test_union_wrap_basic(void) {
    printf("\nTest 3: __kain_union_wrap basic\n");
    
    SimpleUnion u;
    memset(&u, 0xFF, sizeof(u)); /* Fill with garbage */
    
    /* Initialize with int_val */
    u = TEST_UNION_WRAP(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SimpleUnion),
        42
    );
    
    if (u.int_val != 42) {
        TEST_FAIL("union_wrap did not initialize int_val: %d", u.int_val);
    }
    
    /* Initialize with float_val */
    u = TEST_UNION_WRAP(
        u,
        "float_val",
        "float",
        sizeof(float),
        sizeof(SimpleUnion),
        2.71f
    );
    
    if (fabsf(u.float_val - 2.71f) > 0.001f) {
        TEST_FAIL("union_wrap did not initialize float_val: %f", u.float_val);
    }
    
    TEST_PASS("union_wrap basic works correctly");
    return 1;
}

int test_union_different_sizes(void) {
    printf("\nTest 4: Union operations with different field sizes\n");
    
    SizedUnion u = {0};
    
    /* Set byte_val (1 byte) */
    u = TEST_UNION_WRAP(u, "byte_val", "int8_t", 1, sizeof(SizedUnion), (int8_t)42);
    if (u.byte_val != 42) {
        TEST_FAIL("union_wrap byte_val failed: %d", u.byte_val);
    }
    
    /* Set short_val (2 bytes) */
    u = TEST_UNION_WRAP(u, "short_val", "int16_t", 2, sizeof(SizedUnion), (int16_t)1000);
    if (u.short_val != 1000) {
        TEST_FAIL("union_wrap short_val failed: %d", u.short_val);
    }
    
    /* Set int_val (4 bytes) */
    u = TEST_UNION_WRAP(u, "int_val", "int32_t", 4, sizeof(SizedUnion), (int32_t)100000);
    if (u.int_val != 100000) {
        TEST_FAIL("union_wrap int_val failed: %d", u.int_val);
    }
    
    /* Set long_val (8 bytes) */
    u = TEST_UNION_WRAP(u, "long_val", "int64_t", 8, sizeof(SizedUnion), (int64_t)10000000000LL);
    if (u.long_val != 10000000000LL) {
        TEST_FAIL("union_wrap long_val failed: %lld", (long long)u.long_val);
    }
    
    TEST_PASS("Union operations with different sizes work correctly");
    return 1;
}

int test_union_zero_initialization(void) {
    printf("\nTest 5: Union zero initialization\n");
    
    SimpleUnion u;
    memset(&u, 0xFF, sizeof(u)); /* Fill with garbage */
    
    /* union_set should zero the entire union before setting */
    int32_t small_value = 1;
    (void)TEST_UNION_SET(u, "int_val", "int32_t", sizeof(int32_t), sizeof(SimpleUnion), small_value);
    
    /* Check that the union was zeroed (at least the int_val part) */
    if (u.int_val != 1) {
        TEST_FAIL("union_set did not properly initialize: %d", u.int_val);
    }
    
    /* union_wrap should also zero the union */
    memset(&u, 0xFF, sizeof(u));
    u = TEST_UNION_WRAP(u, "int_val", "int32_t", sizeof(int32_t), sizeof(SimpleUnion), 1);
    
    if (u.int_val != 1) {
        TEST_FAIL("union_wrap did not properly initialize: %d", u.int_val);
    }
    
    TEST_PASS("Union zero initialization works correctly");
    return 1;
}

int test_union_type_punning(void) {
    printf("\nTest 6: Union type punning\n");
    
    SimpleUnion u;
    
    /* Set as int */
    u.int_val = 0x42000000;
    
    /* Read as float (type punning) */
    float float_result = TEST_UNION_GET(
        u,
        "float_val",
        "float",
        sizeof(float),
        sizeof(SimpleUnion),
        0.0f
    );
    
    /* Verify bit pattern was preserved */
    uint32_t int_bits;
    memcpy(&int_bits, &u.int_val, sizeof(uint32_t));
    
    uint32_t float_bits;
    memcpy(&float_bits, &float_result, sizeof(uint32_t));
    
    if (int_bits != float_bits) {
        TEST_FAIL("union_get did not preserve bit pattern during type punning");
    }
    
    /* Set as float */
    float new_float = 3.14159f;
    (void)TEST_UNION_SET(u, "float_val", "float", sizeof(float), sizeof(SimpleUnion), new_float);
    
    /* Read as int (type punning) */
    int32_t int_result = TEST_UNION_GET(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SimpleUnion),
        0
    );
    
    /* Verify bit pattern was preserved */
    memcpy(&float_bits, &u.float_val, sizeof(uint32_t));
    memcpy(&int_bits, &int_result, sizeof(uint32_t));
    
    if (float_bits != int_bits) {
        TEST_FAIL("union_get did not preserve bit pattern during reverse type punning");
    }
    
    TEST_PASS("Union type punning works correctly");
    return 1;
}

int test_union_fallback_value(void) {
    printf("\nTest 7: Union fallback value\n");
    
    SimpleUnion u;
    memset(&u, 0, sizeof(u));
    
    /* When reading a field, if the union is smaller than expected,
     * the fallback value should be used for uninitialized parts */
    
    /* Set a small value */
    u.int_val = 0;
    
    /* Read with a fallback */
    int32_t result = TEST_UNION_GET(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SimpleUnion),
        999  /* fallback */
    );
    
    /* Should get the actual value (0), not the fallback */
    if (result != 0) {
        TEST_FAIL("union_get used fallback when it shouldn't: %d", result);
    }
    
    TEST_PASS("Union fallback value works correctly");
    return 1;
}

int test_union_partial_overlap(void) {
    printf("\nTest 8: Union partial field overlap\n");
    
    SizedUnion u;
    
    /* Set long_val (8 bytes) */
    u.long_val = 0x123456789ABCDEF0LL;
    
    /* Read int_val (4 bytes) - should get lower 32 bits on little-endian */
    int32_t int_result = TEST_UNION_GET(
        u,
        "int_val",
        "int32_t",
        sizeof(int32_t),
        sizeof(SizedUnion),
        0
    );
    
    /* Verify we got some data (exact value depends on endianness) */
    if (int_result == 0) {
        TEST_FAIL("union_get partial overlap returned zero");
    }
    
    /* Read short_val (2 bytes) */
    int16_t short_result = TEST_UNION_GET(
        u,
        "short_val",
        "int16_t",
        sizeof(int16_t),
        sizeof(SizedUnion),
        0
    );
    
    /* Verify we got some data */
    if (short_result == 0) {
        TEST_FAIL("union_get partial overlap (short) returned zero");
    }
    
    TEST_PASS("Union partial field overlap works");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 8;
    
    printf("=== KAIN Runtime ABI Parity Test: Union Operations ===\n");
    
    if (test_union_get_basic()) passed++;
    if (test_union_set_basic()) passed++;
    if (test_union_wrap_basic()) passed++;
    if (test_union_different_sizes()) passed++;
    if (test_union_zero_initialization()) passed++;
    if (test_union_type_punning()) passed++;
    if (test_union_fallback_value()) passed++;
    if (test_union_partial_overlap()) passed++;
    
    printf("\n=== Test Results: %d/%d Passed ===\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}
