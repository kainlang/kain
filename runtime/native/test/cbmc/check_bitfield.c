/*
 * check_bitfield.c — CBMC verification harness for bitfield module
 * ====================================================================
 *
 * Verifies __kain_bitfield_get and __kain_bitfield_set with valid memory
 * via a static uint64_t buf[4] that gives CBMC real pointer provenance.
 *
 * Properties verified:
 *   1. Unsigned round-trip — set(v) then get() == (v & mask)
 *   2. Signed round-trip   — set(v) then get() == sign_extend(v & mask)
 *   3. Multi-field isolation — adjacent fields in same unit don't corrupt
 *   4. Single-bit (width=1)   — stores 0 or 1 correctly
 *   5. Full word  (width=64)  — stores full 64-bit value round-trip
 *   6. Cross-unit independence — buf[0] and buf[1] don't interfere
 *   7. NULL field-name safety  — field is (void)'d, so NULL is OK
 *   8. Signed negative value   — explicit negative preserves sign
 *
 * CBMC explores ALL paths on ALL possible inputs (within unwind bound).
 * Since bitfield.c has no loops, --unwind 5 is more than sufficient.
 *
 * Run:  cd runtime/native
 *       python test/scripts/run_pipeline.py cbmc --harness check_bitfield --unwind 5
 */

#include "bitfield.h"

/* =========================================================================
 * Static buffer for pointer provenance
 *
 * CBMC needs objects at known addresses so pointer arithmetic has a
 * concrete provenance root.  buf[4] = 32 bytes, enough for four 8-byte
 * bitfield units (the natural unit size).
 * ========================================================================= */
static uint64_t buf[4];
#define BUF_SIZE  (sizeof(buf))   /* 32 bytes */
#define UNIT_SIZE 8               /* uint64_t */

/* =========================================================================
 * Helper: bitmask matching the implementation in bitfield.c
 *
 *   width 1..63  -> (1ULL << width) - 1
 *   width 64     -> ~0ULL
 * ========================================================================= */
static uint64_t bitmask(int64_t width) {
    if (width == 64) return ~0ULL;
    return ((uint64_t)1 << width) - 1;
}

/* =========================================================================
 * Helper: constrain nondeterministic parameters to valid bitfield ops
 *
 * A valid operation satisfies:
 *   - unit_offset >= 0, aligned to 8, and unit_offset + 8 <= BUF_SIZE
 *   - bit_offset in [0, 63]
 *   - width in [1, 64]
 *   - bit_offset + width <= 64  (field stays within one unit)
 * ========================================================================= */
static void constrain_params(int64_t *unit_offset,
                              int64_t *bit_offset,
                              int64_t *width) {
    __CPROVER_havoc_object(unit_offset);
    __CPROVER_havoc_object(bit_offset);
    __CPROVER_havoc_object(width);

    /* Use subtraction instead of addition to avoid signed overflow
     * checks on the expression itself.  CBMC checks overflow before
     * the solver sees the assume constraint. */
    __CPROVER_assume(*unit_offset >= 0);
    __CPROVER_assume(*unit_offset <= (int64_t)(BUF_SIZE - UNIT_SIZE));
    __CPROVER_assume(*unit_offset % UNIT_SIZE == 0);

    __CPROVER_assume(*bit_offset >= 0);
    __CPROVER_assume(*bit_offset <= 63);
    __CPROVER_assume(*width >= 1);
    __CPROVER_assume(*width <= 64);
    __CPROVER_assume(*bit_offset <= 64 - *width);
}

/* =========================================================================
 * 1.  Unsigned round-trip
 *
 * For any valid bitfield parameters and any int64_t value, the result of
 * a set followed by get must equal (value & mask).  Bits beyond the field
 * width are masked out by the setter; the getter for `is_signed=0` returns
 * the masked value as a positive int64_t.
 * ========================================================================= */
void check_unsigned_roundtrip(void) {
    __CPROVER_havoc_object(buf);

    int64_t unit_offset, bit_offset, width, value;
    constrain_params(&unit_offset, &bit_offset, &width);
    __CPROVER_havoc_object(&value);

    __kain_bitfield_set(buf, "f", unit_offset, bit_offset, width, 0, 64, value);
    int64_t result = __kain_bitfield_get(buf, "f", unit_offset, bit_offset, width, 0, 64);

    uint64_t m = bitmask(width);
    int64_t expected = (int64_t)((uint64_t)value & m);

    __CPROVER_assert(result == expected,
                     "unsigned-roundtrip: result == (value & mask)");
}

/* =========================================================================
 * 2.  Signed round-trip
 *
 * Same as above but with is_signed=1.  The setter ignores is_signed and
 * always masks to width bits; the getter sign-extends when width < 64.
 *
 * Expected result for signed fields:
 *   trunc  = value & mask
 *   result = trunc  if trunc's sign-bit is clear
 *          = trunc | ~mask  if sign-bit is set  (sign extension)
 * ========================================================================= */
void check_signed_roundtrip(void) {
    __CPROVER_havoc_object(buf);

    int64_t unit_offset, bit_offset, width, value;
    constrain_params(&unit_offset, &bit_offset, &width);
    __CPROVER_havoc_object(&value);

    __kain_bitfield_set(buf, "f", unit_offset, bit_offset, width, 1, 32, value);
    int64_t result = __kain_bitfield_get(buf, "f", unit_offset, bit_offset, width, 1, 32);

    /* Compute expected: mask then sign-extend */
    uint64_t m = bitmask(width);
    int64_t truncated = (int64_t)((uint64_t)value & m);

    int64_t expected = truncated;
    if (width < 64) {
        uint64_t sign_bit = (uint64_t)1 << (width - 1);
        if ((uint64_t)truncated & sign_bit) {
            expected = (int64_t)((uint64_t)truncated | ~m);
        }
    }

    __CPROVER_assert(result == expected,
                     "signed-roundtrip: result == sign-extend(value & mask)");
}

/* =========================================================================
 * 3.  Multi-field isolation
 *
 * Two fields in the same uint64_t unit at non-overlapping bit ranges.
 * Setting field B must not corrupt field A, and vice versa.
 *
 *   Field A: bits [0, 4)   — width=4
 *   Field B: bits [4, 10)  — width=6
 * ========================================================================= */
void check_multi_field_isolation(void) {
    __CPROVER_havoc_object(buf);

    int64_t unit_offset = 0;

    int64_t val_a, val_b;
    __CPROVER_havoc_object(&val_a);
    __CPROVER_havoc_object(&val_b);
    __CPROVER_assume(val_a >= 0 && val_a < 16);   /* 4 bits */
    __CPROVER_assume(val_b >= 0 && val_b < 64);   /* 6 bits */

    /* Set field A, then field B */
    __kain_bitfield_set(buf, "a", unit_offset, 0, 4, 0, 32, val_a);
    __kain_bitfield_set(buf, "b", unit_offset, 4, 6, 0, 32, val_b);

    int64_t got_a = __kain_bitfield_get(buf, "a", unit_offset, 0, 4, 0, 32);
    int64_t got_b = __kain_bitfield_get(buf, "b", unit_offset, 4, 6, 0, 32);

    __CPROVER_assert(got_a == val_a,
                     "multi-field: field A preserved after setting B");
    __CPROVER_assert(got_b == val_b,
                     "multi-field: field B correct");
}

/* =========================================================================
 * 4.  Single-bit field (width=1)
 *
 * A 1-bit field stores either 0 or 1.  This exercises the mask = 1 case
 * and verifies that bit_offset can be any valid position in the unit.
 * ========================================================================= */
void check_width_one(void) {
    __CPROVER_havoc_object(buf);

    int64_t unit_offset, bit_offset, val;
    __CPROVER_havoc_object(&unit_offset);
    __CPROVER_havoc_object(&bit_offset);
    __CPROVER_havoc_object(&val);

    __CPROVER_assume(unit_offset >= 0);
    __CPROVER_assume(unit_offset <= (int64_t)(BUF_SIZE - UNIT_SIZE));
    __CPROVER_assume(unit_offset % 8 == 0);
    __CPROVER_assume(bit_offset >= 0 && bit_offset < 64);
    __CPROVER_assume(val == 0 || val == 1);

    __kain_bitfield_set(buf, "bit", unit_offset, bit_offset, 1, 0, 32, val);
    int64_t result = __kain_bitfield_get(buf, "bit", unit_offset, bit_offset, 1, 0, 32);

    __CPROVER_assert(result == val,
                     "width=1: single bit stores 0 or 1 correctly");
}

/* =========================================================================
 * 5.  Full-word field (width=64)
 *
 * When width == 64, mask = ~0ULL so every bit of the value is stored.
 * The getter returns the full uint64_t as int64_t without sign extension
 * (the `width < 64` guard prevents it).
 * ========================================================================= */
void check_width_sixtyfour(void) {
    __CPROVER_havoc_object(buf);

    int64_t unit_offset, val;
    __CPROVER_havoc_object(&unit_offset);
    __CPROVER_havoc_object(&val);

    __CPROVER_assume(unit_offset >= 0);
    __CPROVER_assume(unit_offset <= (int64_t)(BUF_SIZE - UNIT_SIZE));
    __CPROVER_assume(unit_offset % 8 == 0);

    __kain_bitfield_set(buf, "full", unit_offset, 0, 64, 0, 64, val);
    int64_t result = __kain_bitfield_get(buf, "full", unit_offset, 0, 64, 0, 64);

    __CPROVER_assert(result == val,
                     "width=64: full word round-trip");
}

/* =========================================================================
 * 6.  Cross-unit independence
 *
 * Operations on buf[0] (offset 0) and buf[1] (offset 8) must not interfere
 * because they access distinct uint64_t memory locations.
 * ========================================================================= */
void check_cross_unit_independence(void) {
    __CPROVER_havoc_object(buf);

    int64_t val0, val1;
    __CPROVER_havoc_object(&val0);
    __CPROVER_havoc_object(&val1);
    __CPROVER_assume(val0 >= 0 && val0 < 256);    /* 8 bits */
    __CPROVER_assume(val1 >= 0 && val1 < 256);

    __kain_bitfield_set(buf, "u0", 0, 0, 8, 0, 32, val0);
    __kain_bitfield_set(buf, "u1", 8, 0, 8, 0, 32, val1);

    int64_t got0 = __kain_bitfield_get(buf, "u0", 0, 0, 8, 0, 32);
    int64_t got1 = __kain_bitfield_get(buf, "u1", 8, 0, 8, 0, 32);

    __CPROVER_assert(got0 == val0,
                     "cross-unit: unit 0 correct");
    __CPROVER_assert(got1 == val1,
                     "cross-unit: unit 1 correct");
}

/* =========================================================================
 * 7.  NULL field-name safety
 *
 * The field parameter is documented as "for diagnostics/debugging only" and
 * is immediately cast to void.  Passing NULL must be safe and must not
 * affect the bitfield operation result.
 * ========================================================================= */
void check_null_field_is_safe(void) {
    __CPROVER_havoc_object(buf);

    int64_t val;
    __CPROVER_havoc_object(&val);
    __CPROVER_assume(val >= 0 && val < 256);

    /* field = NULL — the implementation does (void)field, so this is safe */
    __kain_bitfield_set(buf, NULL, 0, 0, 8, 0, 32, val);
    int64_t result = __kain_bitfield_get(buf, NULL, 0, 0, 8, 0, 32);

    __CPROVER_assert(result == val,
                     "NULL-field: result matches value");
}

/* =========================================================================
 * 8.  Signed negative value
 *
 * A signed 5-bit field has range [-16, 15].  Negative values in two's
 * complement are stored as their bit pattern, and the getter sign-extends
 * to int64_t.  This test verifies the full range.
 * ========================================================================= */
void check_signed_negative(void) {
    __CPROVER_havoc_object(buf);

    int64_t val;
    __CPROVER_havoc_object(&val);
    __CPROVER_assume(val >= -16 && val <= 15);

    /* width=5 signed, unit_offset=0, bit_offset=0 */
    __kain_bitfield_set(buf, "sn", 0, 0, 5, 1, 32, val);
    int64_t result = __kain_bitfield_get(buf, "sn", 0, 0, 5, 1, 32);

    __CPROVER_assert(result == val,
                     "signed-negative: negative value round-trips");
}

/* =========================================================================
 * Main — run all checks
 * ========================================================================= */
int main(void) {
    check_unsigned_roundtrip();
    check_signed_roundtrip();
    check_multi_field_isolation();
    check_width_one();
    check_width_sixtyfour();
    check_cross_unit_independence();
    check_null_field_is_safe();
    check_signed_negative();
    return 0;
}
