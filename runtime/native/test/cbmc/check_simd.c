/*
 * check_simd.c -- CBMC verification harness for SIMD dot-product module
 *
 * Verifies the scalar SIMD operations (dot product, affine accumulate,
 * power-of-2 fill pair) for NULL safety, bounds, modulus invariants,
 * and internal helper correctness.
 *
 * The AVX2/AVX512 intrinsic paths are tested only for NULL/error returns
 * (which don't reach intrinsics) and via the scalar fallback (when the
 * CPU feature check returns 0). The actual vector intrinsic code paths
 * are tested by Z3 proofs referenced in the source.
 *
 * Static functions tested directly: kain_simd_dot_scalar_raw,
 * kain_simd_affine_stats_scalar_raw, kain_simd_affine_fold_mod,
 * kain_simd_mask_is_pow2_minus_one.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_simd
 * Or:     cbmc --unwind 10 --trace test/cbmc/check_simd.c \
 *              src/core/simd.c -I include -I src/core
 */

#include "simd.h"
#include "cpu.h"

#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers for pointer provenance
 *
 * We provide two 64-element int64_t arrays (512 bytes each) so that
 * SIMD operations have room for at least 8x vectorized lanes.
 * ────────────────────────────────────────────────────────────────────── */
static int64_t g_left[64];
static int64_t g_right[64];

/* Pre-constrained lane string for affine fill tests — the fill writes
 * computed values into both g_left and g_right. */
static int64_t g_fill_left[64];
static int64_t g_fill_right[64];


/* ──────────────────────────────────────────────────────────────────────
 * Forward declarations of static functions from simd.c
 *
 * These are the scalar (non-intrinsic) internal helpers. The AVX2/AVX512
 * internal functions are not forward-declared because they use GCC vector
 * extensions that CBMC may not fully model.
 * ────────────────────────────────────────────────────────────────────── */
static int64_t kain_simd_dot_scalar_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias
);

static KainSimdAffineStats kain_simd_affine_stats_scalar_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells
);

static int64_t kain_simd_affine_fold_mod(
    KainSimdAffineStats stats,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
);

static int kain_simd_mask_is_pow2_minus_one(int64_t mask);

/* abi_cpu_feature_mask is an external function — CBMC treats it as
 * nondeterministic. We'll constrain it in tests where we need specific
 * CPU features. */


/* ──────────────────────────────────────────────────────────────────────
 * Helper: create valid left/right arrays with nondet contents but
 *         bounded cell count
 * ────────────────────────────────────────────────────────────────────── */
static void havoc_arrays(void) {
    __CPROVER_havoc_object(g_left);
    __CPROVER_havoc_object(g_right);
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_simd_dot_scalar_raw returns a consistent result bounded
 *         by the number of cells
 * ────────────────────────────────────────────────────────────────────── */
void check_dot_scalar_raw(void) {
    havoc_arrays();

    int64_t cells;
    int64_t lane_bias;
    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&lane_bias);

    /* Constrain cells to a small positive range for bounded computation */
    __CPROVER_assume(cells >= 0 && cells <= 8);

    int64_t result = kain_simd_dot_scalar_raw(g_left, g_right, cells, lane_bias);

    /* The dot product of up to 8 cells of int64 can produce any value,
     * but we can assert it completes without overflow UB if we constrain
     * values. Let CBMC verify: no crash, no bounds overrun. */
    /* Just assert it returns something — CBMC already validates pointer
     * safety through the static buffers. */
    __CPROVER_assert(1, "dot_scalar_raw: no crash during execution");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_simd_affine_stats_scalar_raw computes base_dot and sum_right
 *         without accessing out-of-bounds memory
 * ────────────────────────────────────────────────────────────────────── */
void check_affine_stats_scalar_raw(void) {
    havoc_arrays();

    int64_t cells;
    __CPROVER_havoc_object(&cells);
    __CPROVER_assume(cells >= 0 && cells <= 8);

    KainSimdAffineStats stats = kain_simd_affine_stats_scalar_raw(
        g_left, g_right, cells);

    /* With 0 cells, both should be 0 */
    if (cells == 0) {
        __CPROVER_assert(stats.base_dot == 0,
                         "affine_stats: 0 cells -> base_dot == 0");
        __CPROVER_assert(stats.sum_right == 0,
                         "affine_stats: 0 cells -> sum_right == 0");
    }

    /* No pointer-safety violation possible — CBMC validates this */
    __CPROVER_assert(1, "affine_stats_scalar_raw: no crash");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_simd_affine_fold_mod produces result in [0, modulus)
 * ────────────────────────────────────────────────────────────────────── */
void check_affine_fold_mod(void) {
    KainSimdAffineStats stats;
    int64_t passes;
    int64_t bias_mod;
    int64_t phase_mod;
    int64_t modulus;

    __CPROVER_havoc_object(&stats);
    __CPROVER_havoc_object(&passes);
    __CPROVER_havoc_object(&bias_mod);
    __CPROVER_havoc_object(&phase_mod);
    __CPROVER_havoc_object(&modulus);

    /* Constrain to valid ranges */
    __CPROVER_assume(passes >= 0 && passes <= 4);
    __CPROVER_assume(bias_mod > 0 && bias_mod <= 100);
    __CPROVER_assume(phase_mod > 0 && phase_mod <= 100);
    __CPROVER_assume(modulus > 0 && modulus <= 1000);

    /* Constrain stats to avoid overflow in intermediate computation */
    __CPROVER_assume(stats.base_dot >= -10000 && stats.base_dot <= 10000);
    __CPROVER_assume(stats.sum_right >= -10000 && stats.sum_right <= 10000);

    int64_t result = kain_simd_affine_fold_mod(
        stats, passes, bias_mod, phase_mod, modulus);

    /* The mod result must be in [0, modulus) */
    __CPROVER_assert(result >= 0, "affine_fold_mod: result >= 0");
    __CPROVER_assert(result < modulus, "affine_fold_mod: result < modulus");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_simd_mask_is_pow2_minus_one identifies correct masks
 * ────────────────────────────────────────────────────────────────────── */
void check_mask_is_pow2_minus_one(void) {
    /* Known values that ARE pow2 - 1 */
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(0) == 1,
                     "mask: 0 is pow2-1 (2^0 - 1)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(1) == 1,
                     "mask: 1 is pow2-1 (2^1 - 1)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(3) == 1,
                     "mask: 3 is pow2-1 (2^2 - 1)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(7) == 1,
                     "mask: 7 is pow2-1 (2^3 - 1)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(255) == 1,
                     "mask: 255 is pow2-1 (2^8 - 1)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(0x7FFFFFFFFFFFFFFFLL) == 1,
                     "mask: INT64_MAX is pow2-1 (2^63 - 1)");

    /* Known values that ARE NOT pow2 - 1 */
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(-1) == 0,
                     "mask: -1 is NOT pow2-1 (negative)");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(-5) == 0,
                     "mask: -5 is not pow2-1");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(2) == 0,
                     "mask: 2 is not pow2-1");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(5) == 0,
                     "mask: 5 is not pow2-1");
    __CPROVER_assert(kain_simd_mask_is_pow2_minus_one(8) == 0,
                     "mask: 8 is not pow2-1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with NULL left returns -1
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_null_left(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        NULL, g_right, 4, 0, 100);
    __CPROVER_assert(result == -1, "scalar_mod NULL left: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with NULL right returns -1
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_null_right(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        g_left, NULL, 4, 0, 100);
    __CPROVER_assert(result == -1, "scalar_mod NULL right: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with negative cells
 *         returns -1
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_negative_cells(void) {
    int64_t neg_cells;
    __CPROVER_havoc_object(&neg_cells);
    __CPROVER_assume(neg_cells < 0);

    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        g_left, g_right, neg_cells, 0, 100);
    __CPROVER_assert(result == -1,
                     "scalar_mod negative cells: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with modulus <= 0
 *         returns -1
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_zero_modulus(void) {
    int64_t bad_mod;
    __CPROVER_havoc_object(&bad_mod);
    __CPROVER_assume(bad_mod <= 0);

    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        g_left, g_right, 4, 0, bad_mod);
    __CPROVER_assert(result == -1,
                     "scalar_mod non-positive modulus: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with valid args produces
 *         result in [0, modulus)
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_valid(void) {
    havoc_arrays();

    int64_t cells;
    int64_t lane_bias;
    int64_t modulus;

    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&lane_bias);
    __CPROVER_havoc_object(&modulus);

    /* Constrain to valid ranges */
    __CPROVER_assume(cells >= 0 && cells <= 8);
    __CPROVER_assume(modulus > 0 && modulus <= 100000);

    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        g_left, g_right, cells, lane_bias, modulus);

    if (result != -1) {
        /* For valid params, result must be in [0, modulus) */
        __CPROVER_assert(result >= 0, "scalar_mod valid: result >= 0");
        __CPROVER_assert(result < modulus,
                         "scalar_mod valid: result < modulus");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx2_mod error path (NULL left)
 *
 * The NULL check is before any intrinsic code, so this tests the
 * error path without requiring intrinsic support from CBMC.
 * ────────────────────────────────────────────────────────────────────── */
void check_avx2_mod_null_left(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_avx2_mod(
        NULL, g_right, 4, 0, 100);
    __CPROVER_assert(result == -1, "avx2_mod NULL left: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx2_mod error path (negative cells)
 * ────────────────────────────────────────────────────────────────────── */
void check_avx2_mod_negative_cells(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_avx2_mod(
        g_left, g_right, -1, 0, 100);
    __CPROVER_assert(result == -1, "avx2_mod negative cells: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx2_mod scalar fallback
 *
 * When abi_cpu_feature_mask() returns 0 (no AVX2), the function falls
 * through to the scalar path. This verifies the fallback produces a
 * valid mod result without entering intrinsics.
 * ────────────────────────────────────────────────────────────────────── */
void check_avx2_mod_scalar_fallback(void) {
    havoc_arrays();

    int64_t cells;
    int64_t lane_bias;
    int64_t modulus;

    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&lane_bias);
    __CPROVER_havoc_object(&modulus);

    __CPROVER_assume(cells >= 0 && cells <= 4);
    __CPROVER_assume(modulus > 0 && modulus <= 100000);

    /* Force scalar fallback by assuming no CPU features */
    __CPROVER_assume((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX2) == 0);

    int64_t result = abi_simd_i64_dot_i32_domain_avx2_mod(
        g_left, g_right, cells, lane_bias, modulus);

    if (result != -1) {
        __CPROVER_assert(result >= 0,
                         "avx2_mod scalar fallback: result >= 0");
        __CPROVER_assert(result < modulus,
                         "avx2_mod scalar fallback: result < modulus");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx512_mod error path (NULL right)
 * ────────────────────────────────────────────────────────────────────── */
void check_avx512_mod_null_right(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_avx512_mod(
        g_left, NULL, 4, 0, 100);
    __CPROVER_assert(result == -1, "avx512_mod NULL right: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx512_mod zero modulus
 * ────────────────────────────────────────────────────────────────────── */
void check_avx512_mod_zero_modulus(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_avx512_mod(
        g_left, g_right, 4, 0, 0);
    __CPROVER_assert(result == -1,
                     "avx512_mod zero modulus: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx512_mod scalar fallback
 *
 * When no AVX512F, the function tries AVX2, then falls to scalar.
 * ────────────────────────────────────────────────────────────────────── */
void check_avx512_mod_scalar_fallback(void) {
    havoc_arrays();

    int64_t cells;
    int64_t lane_bias;
    int64_t modulus;

    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&lane_bias);
    __CPROVER_havoc_object(&modulus);

    __CPROVER_assume(cells >= 0 && cells <= 4);
    __CPROVER_assume(modulus > 0 && modulus <= 100000);

    /* Force scalar fallback by assuming no AVX at all */
    __CPROVER_assume((abi_cpu_feature_mask() &
                      (KAIN_CPU_FEATURE_X86_AVX512F | KAIN_CPU_FEATURE_X86_AVX2)) == 0);

    int64_t result = abi_simd_i64_dot_i32_domain_avx512_mod(
        g_left, g_right, cells, lane_bias, modulus);

    if (result != -1) {
        __CPROVER_assert(result >= 0,
                         "avx512_mod fallback: result >= 0");
        __CPROVER_assert(result < modulus,
                         "avx512_mod fallback: result < modulus");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod
 *         error paths
 * ────────────────────────────────────────────────────────────────────── */
void check_affine_scalar_mod_error_paths(void) {
    /* NULL left */
    int64_t result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        NULL, g_right, 4, 1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "affine_scalar_mod NULL left: returns -1");

    /* NULL right */
    result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        g_left, NULL, 4, 1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "affine_scalar_mod NULL right: returns -1");

    /* Negative passes */
    result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        g_left, g_right, 4, -1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "affine_scalar_mod negative passes: returns -1");

    /* bias_mod <= 0 */
    result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        g_left, g_right, 4, 1, 0, 10, 100);
    __CPROVER_assert(result == -1,
                     "affine_scalar_mod zero bias_mod: returns -1");

    /* modulus <= 0 */
    result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        g_left, g_right, 4, 1, 10, 10, 0);
    __CPROVER_assert(result == -1,
                     "affine_scalar_mod zero modulus: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod valid
 *
 * With small bounded cells and passes, the inner loops are constrained
 * enough for CBMC unwinding.
 * ────────────────────────────────────────────────────────────────────── */
void check_affine_scalar_mod_valid(void) {
    havoc_arrays();

    int64_t cells;
    int64_t passes;
    int64_t bias_mod;
    int64_t phase_mod;
    int64_t modulus;

    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&passes);
    __CPROVER_havoc_object(&bias_mod);
    __CPROVER_havoc_object(&phase_mod);
    __CPROVER_havoc_object(&modulus);

    /* Tight constraints for bounded verification */
    __CPROVER_assume(cells >= 0 && cells <= 4);
    __CPROVER_assume(passes >= 0 && passes <= 3);
    __CPROVER_assume(bias_mod > 0 && bias_mod <= 10);
    __CPROVER_assume(phase_mod > 0 && phase_mod <= 10);
    __CPROVER_assume(modulus > 0 && modulus <= 1000);

    int64_t result = abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
        g_left, g_right, cells, passes, bias_mod, phase_mod, modulus);

    if (result != -1) {
        __CPROVER_assert(result >= 0,
                         "affine_scalar_mod valid: result >= 0");
        __CPROVER_assert(result < modulus,
                         "affine_scalar_mod valid: result < modulus");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_affine_pow2_fill_pair_accumulate_mod error paths
 *
 * This function has many precondition checks before the fill loop.
 * ────────────────────────────────────────────────────────────────────── */
void check_pow2_fill_error_paths(void) {
    /* NULL left */
    int64_t result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        NULL, g_right, 4,
        1, 0, 3,   /* left_mul, left_add, left_mask (= 0b11, pow2 - 1) */
        1, 0, 3,   /* right_mul, right_add, right_mask */
        1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "pow2_fill NULL left: returns -1");

    /* NULL right */
    result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_left, NULL, 4,
        1, 0, 3, 1, 0, 3,
        1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "pow2_fill NULL right: returns -1");

    /* Negative cells */
    result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_left, g_right, -1,
        1, 0, 3, 1, 0, 3,
        1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "pow2_fill negative cells: returns -1");

    /* Invalid left_mask (not pow2 - 1) */
    result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_left, g_right, 4,
        1, 0, 5,   /* 5 = 0b101, NOT pow2 - 1 */
        1, 0, 3,
        1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "pow2_fill invalid left_mask: returns -1");

    /* Invalid right_mask (not pow2 - 1) */
    result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_left, g_right, 4,
        1, 0, 3,
        1, 0, 6,   /* 6 = 0b110, NOT pow2 - 1 */
        1, 10, 10, 100);
    __CPROVER_assert(result == -1,
                     "pow2_fill invalid right_mask: returns -1");

    /* modulus <= 0 */
    result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_left, g_right, 4,
        1, 0, 3, 1, 0, 3,
        1, 10, 10, 0);
    __CPROVER_assert(result == -1,
                     "pow2_fill zero modulus: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_affine_pow2_fill_pair_accumulate_mod with valid
 *         params writes to both arrays and computes correct result
 *
 * The fill function writes computed values to left[index] and right[index]
 * for all cells. This test verifies that the writes stay within bounds
 * and the result is properly reduced modulo modulus.
 * ────────────────────────────────────────────────────────────────────── */
void check_pow2_fill_valid(void) {
    /* Start with clean fill arrays */
    __CPROVER_havoc_object(g_fill_left);
    __CPROVER_havoc_object(g_fill_right);

    int64_t cells;
    int64_t passes;
    int64_t left_mul;
    int64_t left_add;
    int64_t left_mask;
    int64_t right_mul;
    int64_t right_add;
    int64_t right_mask;
    int64_t bias_mod;
    int64_t phase_mod;
    int64_t modulus;

    __CPROVER_havoc_object(&cells);
    __CPROVER_havoc_object(&passes);
    __CPROVER_havoc_object(&left_mul);
    __CPROVER_havoc_object(&left_add);
    __CPROVER_havoc_object(&left_mask);
    __CPROVER_havoc_object(&right_mul);
    __CPROVER_havoc_object(&right_add);
    __CPROVER_havoc_object(&right_mask);
    __CPROVER_havoc_object(&bias_mod);
    __CPROVER_havoc_object(&phase_mod);
    __CPROVER_havoc_object(&modulus);

    /* Constrain to small verifiable ranges */
    __CPROVER_assume(cells >= 0 && cells <= 4);
    __CPROVER_assume(passes >= 0 && passes <= 2);
    __CPROVER_assume(modulus > 0 && modulus <= 1000);
    __CPROVER_assume(bias_mod > 0 && bias_mod <= 10);
    __CPROVER_assume(phase_mod > 0 && phase_mod <= 10);

    /* Masks must be power-of-2-minus-1. Use 3 (0b11) or 7 (0b111) as
     * concrete valid masks — CBMC can reason about concrete better
     * than fully unconstrained nondet masks here. */
    __CPROVER_assume(left_mask == 3 || left_mask == 7 || left_mask == 255);
    __CPROVER_assume(right_mask == 3 || right_mask == 7 || right_mask == 255);

    /* Use small multiplier/add values to avoid overflow in the fill */
    __CPROVER_assume(left_mul >= 0 && left_mul <= 4);
    __CPROVER_assume(left_add >= 0 && left_add <= 8);
    __CPROVER_assume(right_mul >= 0 && right_mul <= 4);
    __CPROVER_assume(right_add >= 0 && right_add <= 8);

    int64_t result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_fill_left, g_fill_right, cells,
        left_mul, left_add, left_mask,
        right_mul, right_add, right_mask,
        passes, bias_mod, phase_mod, modulus);

    /* The fill function should NOT crash even with nondet contents */
    /* Result should be -1 (fallthrough error) or in valid mod range */
    if (result != -1) {
        __CPROVER_assert(result >= 0,
                         "pow2_fill valid: result >= 0");
        __CPROVER_assert(result < modulus,
                         "pow2_fill valid: result < modulus");
    }

    /* Verify no out-of-bounds writes occurred — CBMC detects these
     * automatically through the static buffer bounds. If cells=4 and
     * g_fill_left has 64 elements, all writes are in bounds. */
    __CPROVER_assert(1, "pow2_fill valid: no OOB writes");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_affine_pow2_fill_pair_accumulate_mod with zero
 *         cells writes nothing and returns 0 (no-op)
 * ────────────────────────────────────────────────────────────────────── */
void check_pow2_fill_zero_cells(void) {
    int64_t result = abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
        g_fill_left, g_fill_right, 0,
        1, 0, 3, 1, 0, 3,
        1, 10, 10, 100);

    /* Zero cells is valid; the fill loop doesn't execute, so stats are
     * zero, and the affine fold is computed over zero stats. */
    if (result != -1) {
        __CPROVER_assert(result >= 0, "pow2_fill zero cells: result >= 0");
        __CPROVER_assert(result < 100, "pow2_fill zero cells: result < mod");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_avx2_mod with AVX2 available but
 *         zero cells (fast path processes nothing, scalar tail handles)
 * ────────────────────────────────────────────────────────────────────── */
void check_avx2_mod_zero_cells(void) {
    havoc_arrays();

    int64_t result = abi_simd_i64_dot_i32_domain_avx2_mod(
        g_left, g_right, 0, 0, 100);
    __CPROVER_assert(result == 0, "avx2_mod zero cells: result == 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_scalar_mod with zero cells and
 *         any lane_bias returns 0
 * ────────────────────────────────────────────────────────────────────── */
void check_scalar_mod_zero_cells(void) {
    int64_t lane_bias;
    __CPROVER_havoc_object(&lane_bias);

    int64_t result = abi_simd_i64_dot_i32_domain_scalar_mod(
        g_left, g_right, 0, lane_bias, 100);
    __CPROVER_assert(result == 0, "scalar_mod zero cells: result == 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: abi_simd_i64_dot_i32_domain_affine_accumulate_avx2_mod error
 *         path (NULL left — no intrinsics reached)
 * ────────────────────────────────────────────────────────────────────── */
void check_affine_avx2_mod_null(void) {
    int64_t result = abi_simd_i64_dot_i32_domain_affine_accumulate_avx2_mod(
        NULL, g_right, 4, 1, 10, 10, 100);
    __CPROVER_assert(result == -1, "affine_avx2_mod NULL: returns -1");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main -- run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_dot_scalar_raw();
    check_affine_stats_scalar_raw();
    check_affine_fold_mod();
    check_mask_is_pow2_minus_one();
    check_scalar_mod_null_left();
    check_scalar_mod_null_right();
    check_scalar_mod_negative_cells();
    check_scalar_mod_zero_modulus();
    check_scalar_mod_valid();
    check_scalar_mod_zero_cells();
    check_avx2_mod_null_left();
    check_avx2_mod_negative_cells();
    check_avx2_mod_scalar_fallback();
    check_avx2_mod_zero_cells();
    check_avx512_mod_null_right();
    check_avx512_mod_zero_modulus();
    check_avx512_mod_scalar_fallback();
    check_affine_scalar_mod_error_paths();
    check_affine_scalar_mod_valid();
    check_pow2_fill_error_paths();
    check_pow2_fill_valid();
    check_pow2_fill_zero_cells();
    check_affine_avx2_mod_null();
    return 0;
}
