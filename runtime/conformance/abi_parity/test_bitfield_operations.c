/*
 * KAIN Runtime ABI Parity Test: Bitfield Operations
 *
 * Tests the canonical low-level bitfield helper implementations:
 * - __kain_bitfield_get - Extract bitfield value from struct
 * - __kain_bitfield_set - Write bitfield value to struct
 *
 * Tests various bitfield widths, signedness, and packing scenarios.
 *
 * Requirements: 3.2, 3.3, 3.4, 13.1, 13.6
 */

#include "../../native/include/kain_runtime_memory.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name, ...) do { printf("  ❌ FAIL: " name "\n", ##__VA_ARGS__); return 0; } while(0)

/* Test struct with bitfields */
typedef struct {
    uint64_t storage; /* 8-byte bitfield storage unit */
} BitfieldStruct;

int test_bitfield_get_unsigned(void) {
    printf("\nTest 1: __kain_bitfield_get unsigned\n");
    
    BitfieldStruct bf;
    bf.storage = 0;
    
    /* Set bits [0:3) to value 5 (binary: 101) */
    bf.storage = 0x5; /* 0b00000101 */
    
    int64_t result = __kain_bitfield_get(
        bf,
        "field_a",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        0,  /* is_signed */
        32  /* promoted_bits */
    );
    
    if (result != 5) {
        TEST_FAIL("bitfield_get unsigned returned wrong value: %lld", (long long)result);
    }
    
    /* Test with different bit offset */
    bf.storage = 0x28; /* 0b00101000 - bits [3:6) = 5 */
    
    result = __kain_bitfield_get(
        bf,
        "field_b",
        0,  /* unit_offset */
        3,  /* bit_offset */
        3,  /* width */
        0,  /* is_signed */
        32  /* promoted_bits */
    );
    
    if (result != 5) {
        TEST_FAIL("bitfield_get with offset returned wrong value: %lld", (long long)result);
    }
    
    TEST_PASS("bitfield_get unsigned works correctly");
    return 1;
}

int test_bitfield_get_signed(void) {
    printf("\nTest 2: __kain_bitfield_get signed\n");
    
    BitfieldStruct bf;
    
    /* Test positive value: bits [0:3) = 3 (binary: 011) */
    bf.storage = 0x3;
    
    int64_t result = __kain_bitfield_get(
        bf,
        "signed_field",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        1,  /* is_signed */
        32  /* promoted_bits */
    );
    
    if (result != 3) {
        TEST_FAIL("bitfield_get signed positive returned wrong value: %lld", (long long)result);
    }
    
    /* Test negative value: bits [0:3) = 7 (binary: 111 = -1 in 3-bit signed) */
    bf.storage = 0x7;
    
    result = __kain_bitfield_get(
        bf,
        "signed_field",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        1,  /* is_signed */
        32  /* promoted_bits */
    );
    
    if (result != -1) {
        TEST_FAIL("bitfield_get signed negative returned wrong value: %lld", (long long)result);
    }
    
    /* Test -2 in 3-bit signed: binary 110 = 6 unsigned, -2 signed */
    bf.storage = 0x6;
    
    result = __kain_bitfield_get(
        bf,
        "signed_field",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        1,  /* is_signed */
        32  /* promoted_bits */
    );
    
    if (result != -2) {
        TEST_FAIL("bitfield_get signed -2 returned wrong value: %lld", (long long)result);
    }
    
    TEST_PASS("bitfield_get signed works correctly");
    return 1;
}

int test_bitfield_set_unsigned(void) {
    printf("\nTest 3: __kain_bitfield_set unsigned\n");
    
    BitfieldStruct bf;
    bf.storage = 0;
    
    /* Set bits [0:3) to value 5 */
    int64_t returned = __kain_bitfield_set(
        bf,
        "field_a",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        0,  /* is_signed */
        32, /* promoted_bits */
        5   /* next value */
    );
    
    if (returned != 5) {
        TEST_FAIL("bitfield_set did not return the set value: %lld", (long long)returned);
    }
    
    if ((bf.storage & 0x7) != 5) {
        TEST_FAIL("bitfield_set did not update storage: 0x%llX", 
                  (unsigned long long)bf.storage);
    }
    
    /* Set bits [3:6) to value 7 */
    __kain_bitfield_set(
        bf,
        "field_b",
        0,  /* unit_offset */
        3,  /* bit_offset */
        3,  /* width */
        0,  /* is_signed */
        32, /* promoted_bits */
        7   /* next value */
    );
    
    /* Verify both fields are set correctly */
    if ((bf.storage & 0x7) != 5) {
        TEST_FAIL("bitfield_set corrupted previous field");
    }
    
    if (((bf.storage >> 3) & 0x7) != 7) {
        TEST_FAIL("bitfield_set second field incorrect: 0x%llX", 
                  (unsigned long long)bf.storage);
    }
    
    TEST_PASS("bitfield_set unsigned works correctly");
    return 1;
}

int test_bitfield_set_signed(void) {
    printf("\nTest 4: __kain_bitfield_set signed\n");
    
    BitfieldStruct bf;
    bf.storage = 0;
    
    /* Set bits [0:3) to value -1 (should be stored as 0x7 in 3 bits) */
    __kain_bitfield_set(
        bf,
        "signed_field",
        0,  /* unit_offset */
        0,  /* bit_offset */
        3,  /* width */
        1,  /* is_signed */
        32, /* promoted_bits */
        -1  /* next value */
    );
    
    /* Verify the bit pattern is 0x7 (111 in binary) */
    if ((bf.storage & 0x7) != 0x7) {
        TEST_FAIL("bitfield_set signed -1 incorrect: 0x%llX", 
                  (unsigned long long)(bf.storage & 0x7));
    }
    
    /* Read it back to verify */
    int64_t result = __kain_bitfield_get(
        bf,
        "signed_field",
        0, 0, 3, 1, 32
    );
    
    if (result != -1) {
        TEST_FAIL("bitfield_get after set returned wrong value: %lld", (long long)result);
    }
    
    /* Set to -2 (should be stored as 0x6 in 3 bits) */
    bf.storage = 0;
    __kain_bitfield_set(
        bf,
        "signed_field",
        0, 0, 3, 1, 32, -2
    );
    
    if ((bf.storage & 0x7) != 0x6) {
        TEST_FAIL("bitfield_set signed -2 incorrect: 0x%llX", 
                  (unsigned long long)(bf.storage & 0x7));
    }
    
    TEST_PASS("bitfield_set signed works correctly");
    return 1;
}

int test_bitfield_width_variations(void) {
    printf("\nTest 5: Bitfield width variations\n");
    
    BitfieldStruct bf;
    
    /* Test 1-bit field */
    bf.storage = 0;
    __kain_bitfield_set(bf, "bit1", 0, 0, 1, 0, 32, 1);
    if ((bf.storage & 0x1) != 1) {
        TEST_FAIL("1-bit field set failed");
    }
    
    /* Test 8-bit field */
    bf.storage = 0;
    __kain_bitfield_set(bf, "byte", 0, 0, 8, 0, 32, 0xFF);
    if ((bf.storage & 0xFF) != 0xFF) {
        TEST_FAIL("8-bit field set failed");
    }
    
    /* Test 16-bit field */
    bf.storage = 0;
    __kain_bitfield_set(bf, "short", 0, 0, 16, 0, 32, 0xABCD);
    if ((bf.storage & 0xFFFF) != 0xABCD) {
        TEST_FAIL("16-bit field set failed");
    }
    
    /* Test 32-bit field */
    bf.storage = 0;
    __kain_bitfield_set(bf, "int", 0, 0, 32, 0, 32, 0x12345678);
    if ((bf.storage & 0xFFFFFFFF) != 0x12345678) {
        TEST_FAIL("32-bit field set failed");
    }
    
    TEST_PASS("Bitfield width variations work correctly");
    return 1;
}

int test_bitfield_multiple_fields(void) {
    printf("\nTest 6: Multiple bitfields in same unit\n");
    
    BitfieldStruct bf;
    bf.storage = 0;
    
    /* Field A: bits [0:4) = 15 (4 bits) */
    __kain_bitfield_set(bf, "field_a", 0, 0, 4, 0, 32, 15);
    
    /* Field B: bits [4:8) = 10 (4 bits) */
    __kain_bitfield_set(bf, "field_b", 0, 4, 4, 0, 32, 10);
    
    /* Field C: bits [8:16) = 200 (8 bits) */
    __kain_bitfield_set(bf, "field_c", 0, 8, 8, 0, 32, 200);
    
    /* Verify all fields */
    int64_t val_a = __kain_bitfield_get(bf, "field_a", 0, 0, 4, 0, 32);
    int64_t val_b = __kain_bitfield_get(bf, "field_b", 0, 4, 4, 0, 32);
    int64_t val_c = __kain_bitfield_get(bf, "field_c", 0, 8, 8, 0, 32);
    
    if (val_a != 15) {
        TEST_FAIL("Multiple fields: field_a incorrect: %lld", (long long)val_a);
    }
    
    if (val_b != 10) {
        TEST_FAIL("Multiple fields: field_b incorrect: %lld", (long long)val_b);
    }
    
    if (val_c != 200) {
        TEST_FAIL("Multiple fields: field_c incorrect: %lld", (long long)val_c);
    }
    
    TEST_PASS("Multiple bitfields in same unit work correctly");
    return 1;
}

int test_bitfield_boundary_values(void) {
    printf("\nTest 7: Bitfield boundary values\n");
    
    BitfieldStruct bf;
    
    /* Test maximum value for 3-bit unsigned: 7 */
    bf.storage = 0;
    __kain_bitfield_set(bf, "max3", 0, 0, 3, 0, 32, 7);
    int64_t result = __kain_bitfield_get(bf, "max3", 0, 0, 3, 0, 32);
    if (result != 7) {
        TEST_FAIL("3-bit max value incorrect: %lld", (long long)result);
    }
    
    /* Test overflow: setting 8 in 3-bit field should truncate to 0 */
    bf.storage = 0;
    __kain_bitfield_set(bf, "overflow", 0, 0, 3, 0, 32, 8);
    result = __kain_bitfield_get(bf, "overflow", 0, 0, 3, 0, 32);
    if (result != 0) {
        TEST_FAIL("3-bit overflow should truncate to 0: %lld", (long long)result);
    }
    
    /* Test maximum positive for 3-bit signed: 3 */
    bf.storage = 0;
    __kain_bitfield_set(bf, "max_pos", 0, 0, 3, 1, 32, 3);
    result = __kain_bitfield_get(bf, "max_pos", 0, 0, 3, 1, 32);
    if (result != 3) {
        TEST_FAIL("3-bit signed max positive incorrect: %lld", (long long)result);
    }
    
    /* Test minimum negative for 3-bit signed: -4 */
    bf.storage = 0;
    __kain_bitfield_set(bf, "min_neg", 0, 0, 3, 1, 32, -4);
    result = __kain_bitfield_get(bf, "min_neg", 0, 0, 3, 1, 32);
    if (result != -4) {
        TEST_FAIL("3-bit signed min negative incorrect: %lld", (long long)result);
    }
    
    TEST_PASS("Bitfield boundary values work correctly");
    return 1;
}

int test_bitfield_preservation(void) {
    printf("\nTest 8: Bitfield preservation of other fields\n");
    
    BitfieldStruct bf;
    bf.storage = 0xFFFFFFFFFFFFFFFFULL; /* All bits set */
    
    /* Clear bits [8:16) and set to 0 */
    __kain_bitfield_set(bf, "middle", 0, 8, 8, 0, 32, 0);
    
    /* Verify bits [0:8) are still set */
    if ((bf.storage & 0xFF) != 0xFF) {
        TEST_FAIL("bitfield_set corrupted lower bits");
    }
    
    /* Verify bits [16:64) are still set */
    if ((bf.storage >> 16) != 0xFFFFFFFFFFFFULL) {
        TEST_FAIL("bitfield_set corrupted upper bits");
    }
    
    /* Verify bits [8:16) are cleared */
    if (((bf.storage >> 8) & 0xFF) != 0) {
        TEST_FAIL("bitfield_set did not clear middle bits");
    }
    
    TEST_PASS("Bitfield preservation of other fields works correctly");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 8;
    
    printf("=== KAIN Runtime ABI Parity Test: Bitfield Operations ===\n");
    
    if (test_bitfield_get_unsigned()) passed++;
    if (test_bitfield_get_signed()) passed++;
    if (test_bitfield_set_unsigned()) passed++;
    if (test_bitfield_set_signed()) passed++;
    if (test_bitfield_width_variations()) passed++;
    if (test_bitfield_multiple_fields()) passed++;
    if (test_bitfield_boundary_values()) passed++;
    if (test_bitfield_preservation()) passed++;
    
    printf("\n=== Test Results: %d/%d Passed ===\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}
