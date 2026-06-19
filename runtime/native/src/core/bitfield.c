/*
 */

#include "../../include/bitfield.h"
#include <string.h>

/* ============================================================================
 * Bitfield Operations
 * ============================================================================ */

/*
 * __kain_bitfield_get
 *
 * Extract bitfield value from struct.
 *
 * Algorithm:
 *   1. Load bitfield unit (8 bytes) from value at unit_offset
 *   2. Extract bits [bit_offset, bit_offset + width)
 *   3. If is_signed, sign-extend to promoted_bits
 *   4. Return as int64_t
 */
int64_t __kain_bitfield_get(
    const void* value,
    const char* field,
    int64_t unit_offset,
    int64_t bit_offset,
    int64_t width,
    int is_signed,
    int64_t promoted_bits
) {
    /* Field name is for diagnostics/debugging only */
    (void)field;
    (void)promoted_bits; /* Used for validation but not in core algorithm */

    /* Load the bitfield unit (8 bytes) from the struct */
    const char* base = (const char*)value;
    uint64_t unit;
    memcpy(&unit, base + unit_offset, sizeof(uint64_t));

    /* Create mask for the field width */
    uint64_t mask = (width == 64) ? ~0ULL : ((1ULL << width) - 1ULL);

    /* Shift and mask to extract the field */
    uint64_t shifted = unit >> bit_offset;
    uint64_t extracted = shifted & mask;

    /* Apply sign extension if needed */
    if (is_signed && width < 64) {
        /* Check if the sign bit is set */
        uint64_t sign_bit = 1ULL << (width - 1);
        if (extracted & sign_bit) {
            /* Sign extend by setting all bits above width to 1 */
            uint64_t extension_mask = ~mask;
            extracted |= extension_mask;
        }
    }

    /* Return as signed int64_t */
    return (int64_t)extracted;
}

/*
 * __kain_bitfield_set
 *
 * Write bitfield value to struct.
 *
 * Algorithm:
 *   1. Load bitfield unit (8 bytes) from value at unit_offset
 *   2. Clear bits [bit_offset, bit_offset + width)
 *   3. Insert new value (masked to width bits)
 *   4. Store unit back to value
 */
void __kain_bitfield_set(
    void* value,
    const char* field,
    int64_t unit_offset,
    int64_t bit_offset,
    int64_t width,
    int is_signed,
    int64_t promoted_bits,
    int64_t next
) {
    /* Field name and type info are for diagnostics/debugging only */
    (void)field;
    (void)is_signed;
    (void)promoted_bits;

    /* Load the bitfield unit (8 bytes) from the struct */
    char* base = (char*)value;
    uint64_t unit;
    memcpy(&unit, base + unit_offset, sizeof(uint64_t));

    /* Create mask for the field width */
    uint64_t mask = (width == 64) ? ~0ULL : ((1ULL << width) - 1ULL);

    /* Mask the new value to the field width */
    uint64_t encoded = ((uint64_t)next) & mask;

    /* Create mask for clearing the field in the unit */
    uint64_t shifted_mask = mask << bit_offset;

    /* Clear the field bits and insert the new value */
    unit = (unit & ~shifted_mask) | (encoded << bit_offset);

    /* Store the modified unit back to the struct */
    memcpy(base + unit_offset, &unit, sizeof(uint64_t));
}
