#include "../../include/simd.h"

#include "../../include/cpu.h"

#include <stddef.h>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define KAIN_SIMD_X86 1
#else
#define KAIN_SIMD_X86 0
#endif

#if KAIN_SIMD_X86 && (defined(__clang__) || defined(__GNUC__))
#define KAIN_SIMD_X86_INTRINSICS 1
#else
#define KAIN_SIMD_X86_INTRINSICS 0
#endif

#if KAIN_SIMD_X86_INTRINSICS && !defined(__clang__)
#include <immintrin.h>
#endif

#if KAIN_SIMD_X86_INTRINSICS && defined(__GNUC__) && !defined(__clang__)
// GCC needs the standard intrinsic; Clang uses the Z3-discovered builtin
#define KAIN_SIMD_PMULUDQ512(a, b) _mm512_mul_epu32((__m512i)(a), (__m512i)(b))
#else
#define KAIN_SIMD_PMULUDQ512(a, b) __builtin_ia32_pmuludq512(a, b)
#endif

#if defined(__clang__) || defined(__GNUC__)
#define KAIN_SIMD_TARGET_AVX2 __attribute__((target("avx2")))
#define KAIN_SIMD_TARGET_AVX512F __attribute__((target("avx512f")))
#else
#define KAIN_SIMD_TARGET_AVX2
#define KAIN_SIMD_TARGET_AVX512F
#endif

#if KAIN_SIMD_X86_INTRINSICS
typedef long long KainSimdI64x4 __attribute__((vector_size(32)));
typedef int KainSimdI32x8 __attribute__((vector_size(32)));
typedef long long KainSimdI64x8 __attribute__((vector_size(64)));
typedef int KainSimdI32x16 __attribute__((vector_size(64)));
#endif

typedef struct KainSimdAffineStats {
    int64_t base_dot;
    int64_t sum_right;
} KainSimdAffineStats;

static int64_t kain_simd_dot_scalar_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias
) {
    int64_t total = 0;
    int64_t index = 0;
    while (index < cells) {
        total += (left[index] + lane_bias) * right[index];
        index += 1;
    }
    return total;
}

static KainSimdAffineStats kain_simd_affine_stats_scalar_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells
) {
    KainSimdAffineStats stats = { 0, 0 };
    int64_t index = 0;
    while (index < cells) {
        const int64_t right_value = right[index];
        stats.base_dot += left[index] * right_value;
        stats.sum_right += right_value;
        index += 1;
    }
    return stats;
}

static int64_t kain_simd_affine_fold_mod(
    KainSimdAffineStats stats,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
) {
    /* Replaces phase % bias_mod and phase % phase_mod with running counters
     * that increment and wrap. Each IDIV (20-80 cycles) becomes
     * INC + CMP + CMOV (<5 cycles).
     *
     * Proof: runtime/native/src/core/z3/proofs-experimental/
     *   simd-affine-fold-wrapping-counter-equivalence.smt2
     * Domain: bias_mod > 0, phase_mod > 0 (validated before call) */
    int64_t acc = 0;
    int64_t bias = 0;
    int64_t phase_rem = 0;
    int64_t phase = 0;
    while (phase < passes) {
        const int64_t inner = (stats.base_dot + (bias * stats.sum_right)) % modulus;
        acc = (acc + inner + phase_rem) % modulus;
        /* Advance running counters — compilers emit CMOVcc here */
        bias = (bias + 1 < bias_mod) ? bias + 1 : 0;
        phase_rem = (phase_rem + 1 < phase_mod) ? phase_rem + 1 : 0;
        phase += 1;
    }
    return acc;
}

static int kain_simd_mask_is_pow2_minus_one(int64_t mask) {
    /* Branchless: (mask >= 0) AND ((uint64_t)mask & (uint64_t)(mask+1)) == 0
     * Replaces two comparisons + && branch with bit arithmetic:
     *   sign_ok = ~(x >> 63)   — all-1s if mask >= 0
     *   pow2_ok = ~(x & (x+1) | -(x & (x+1)))  — all-1s if x & (x+1) == 0
     *   result = (sign_ok & pow2_ok) >> 63
     * Proof: runtime/native/src/core/z3/proofs-experimental/
     *   simd-mask-pow2-minus-one-branchless.smt2 */
    const uint64_t x = (uint64_t)mask;
    const uint64_t t = x & (x + 1u);
    const uint64_t ok = (~(x >> 63)) & ~(t | -t);
    return (int)(ok >> 63);
}

int64_t abi_simd_i64_dot_i32_domain_scalar_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || modulus <= 0) {
        return -1;
    }
    return kain_simd_dot_scalar_raw(left, right, cells, lane_bias) % modulus;
}

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || passes < 0 || bias_mod <= 0 || phase_mod <= 0 || modulus <= 0) {
        return -1;
    }
    return kain_simd_affine_fold_mod(
        kain_simd_affine_stats_scalar_raw(left, right, cells),
        passes,
        bias_mod,
        phase_mod,
        modulus
    );
}

int64_t abi_simd_i64_affine_pow2_fill_pair_accumulate_mod(
    int64_t* left,
    int64_t* right,
    int64_t cells,
    int64_t left_mul,
    int64_t left_add,
    int64_t left_mask,
    int64_t right_mul,
    int64_t right_add,
    int64_t right_mask,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
) {
    KainSimdAffineStats stats = { 0, 0 };
    int64_t index = 0;
    if (
        left == 0 ||
        right == 0 ||
        cells < 0 ||
        passes < 0 ||
        bias_mod <= 0 ||
        phase_mod <= 0 ||
        modulus <= 0 ||
        !kain_simd_mask_is_pow2_minus_one(left_mask) ||
        !kain_simd_mask_is_pow2_minus_one(right_mask)
    ) {
        return -1;
    }

    while (index < cells) {
        const int64_t left_value = ((index * left_mul) + left_add) & left_mask;
        const int64_t right_value = ((index * right_mul) + right_add) & right_mask;
        left[index] = left_value;
        right[index] = right_value;
        stats.base_dot += left_value * right_value;
        stats.sum_right += right_value;
        index += 1;
    }
    return kain_simd_affine_fold_mod(stats, passes, bias_mod, phase_mod, modulus);
}

#if KAIN_SIMD_X86_INTRINSICS
static KAIN_SIMD_TARGET_AVX2 int64_t kain_simd_dot_avx2_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias
) {
    /* Proof: runtime/native/src/core/z3/proofs-experimental/simd-i32-domain-even-dword-mul-equivalence.smt2 */
    const KainSimdI64x4 bias = { lane_bias, lane_bias, lane_bias, lane_bias };
    KainSimdI64x4 acc = { 0, 0, 0, 0 };
    int64_t lanes[4];
    int64_t total;
    int64_t index = 0;

    while (index + 4 <= cells) {
        KainSimdI64x4 left_values;
        KainSimdI64x4 right_values;
        KainSimdI64x4 biased_left;
        KainSimdI64x4 products;
        __builtin_memcpy(&left_values, left + index, sizeof(left_values));
        __builtin_memcpy(&right_values, right + index, sizeof(right_values));
        biased_left = left_values + bias;
        products = (KainSimdI64x4)__builtin_ia32_pmuludq256(
            (KainSimdI32x8)biased_left,
            (KainSimdI32x8)right_values
        );
        acc += products;
        index += 4;
    }

    __builtin_memcpy(lanes, &acc, sizeof(lanes));
    total = lanes[0] + lanes[1] + lanes[2] + lanes[3];
    while (index < cells) {
        total += (left[index] + lane_bias) * right[index];
        index += 1;
    }
    return total;
}

static KAIN_SIMD_TARGET_AVX2 KainSimdAffineStats kain_simd_affine_stats_avx2_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells
) {
    /* Proof: runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-dot-factorization.smt2 */
    KainSimdI64x4 base_acc = { 0, 0, 0, 0 };
    KainSimdI64x4 sum_acc = { 0, 0, 0, 0 };
    int64_t base_lanes[4];
    int64_t sum_lanes[4];
    KainSimdAffineStats stats;
    int64_t index = 0;

    while (index + 4 <= cells) {
        KainSimdI64x4 left_values;
        KainSimdI64x4 right_values;
        KainSimdI64x4 products;
        __builtin_memcpy(&left_values, left + index, sizeof(left_values));
        __builtin_memcpy(&right_values, right + index, sizeof(right_values));
        products = (KainSimdI64x4)__builtin_ia32_pmuludq256(
            (KainSimdI32x8)left_values,
            (KainSimdI32x8)right_values
        );
        base_acc += products;
        sum_acc += right_values;
        index += 4;
    }

    __builtin_memcpy(base_lanes, &base_acc, sizeof(base_lanes));
    __builtin_memcpy(sum_lanes, &sum_acc, sizeof(sum_lanes));
    stats.base_dot = base_lanes[0] + base_lanes[1] + base_lanes[2] + base_lanes[3];
    stats.sum_right = sum_lanes[0] + sum_lanes[1] + sum_lanes[2] + sum_lanes[3];
    while (index < cells) {
        const int64_t right_value = right[index];
        stats.base_dot += left[index] * right_value;
        stats.sum_right += right_value;
        index += 1;
    }
    return stats;
}

static KAIN_SIMD_TARGET_AVX512F int64_t kain_simd_dot_avx512_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias
) {
    /* Proof: runtime/native/src/core/z3/proofs-experimental/simd-i32-domain-even-dword-mul-equivalence.smt2 */
    const KainSimdI64x8 bias = {
        lane_bias, lane_bias, lane_bias, lane_bias,
        lane_bias, lane_bias, lane_bias, lane_bias
    };
    KainSimdI64x8 acc = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int64_t lanes[8];
    int64_t total;
    int64_t index = 0;

    while (index + 8 <= cells) {
        KainSimdI64x8 left_values;
        KainSimdI64x8 right_values;
        KainSimdI64x8 biased_left;
        KainSimdI64x8 products;
        __builtin_memcpy(&left_values, left + index, sizeof(left_values));
        __builtin_memcpy(&right_values, right + index, sizeof(right_values));
        biased_left = left_values + bias;
        products = (KainSimdI64x8)KAIN_SIMD_PMULUDQ512(
            (KainSimdI32x16)biased_left,
            (KainSimdI32x16)right_values
        );
        acc += products;
        index += 8;
    }

    __builtin_memcpy(lanes, &acc, sizeof(lanes));
    total = lanes[0] + lanes[1] + lanes[2] + lanes[3] + lanes[4] + lanes[5] + lanes[6] + lanes[7];
    while (index < cells) {
        total += (left[index] + lane_bias) * right[index];
        index += 1;
    }
    return total;
}

static KAIN_SIMD_TARGET_AVX512F KainSimdAffineStats kain_simd_affine_stats_avx512_raw(
    const int64_t* left,
    const int64_t* right,
    int64_t cells
) {
    /* Proof: runtime/native/src/core/z3/proofs-experimental/simd-affine-bias-dot-factorization.smt2 */
    KainSimdI64x8 base_acc = { 0, 0, 0, 0, 0, 0, 0, 0 };
    KainSimdI64x8 sum_acc = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int64_t base_lanes[8];
    int64_t sum_lanes[8];
    KainSimdAffineStats stats;
    int64_t index = 0;

    while (index + 8 <= cells) {
        KainSimdI64x8 left_values;
        KainSimdI64x8 right_values;
        KainSimdI64x8 products;
        __builtin_memcpy(&left_values, left + index, sizeof(left_values));
        __builtin_memcpy(&right_values, right + index, sizeof(right_values));
        products = (KainSimdI64x8)KAIN_SIMD_PMULUDQ512(
            (KainSimdI32x16)left_values,
            (KainSimdI32x16)right_values
        );
        base_acc += products;
        sum_acc += right_values;
        index += 8;
    }

    __builtin_memcpy(base_lanes, &base_acc, sizeof(base_lanes));
    __builtin_memcpy(sum_lanes, &sum_acc, sizeof(sum_lanes));
    stats.base_dot = base_lanes[0] + base_lanes[1] + base_lanes[2] + base_lanes[3] +
        base_lanes[4] + base_lanes[5] + base_lanes[6] + base_lanes[7];
    stats.sum_right = sum_lanes[0] + sum_lanes[1] + sum_lanes[2] + sum_lanes[3] +
        sum_lanes[4] + sum_lanes[5] + sum_lanes[6] + sum_lanes[7];
    while (index < cells) {
        const int64_t right_value = right[index];
        stats.base_dot += left[index] * right_value;
        stats.sum_right += right_value;
        index += 1;
    }
    return stats;
}
#endif

int64_t abi_simd_i64_dot_i32_domain_avx2_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || modulus <= 0) {
        return -1;
    }
#if KAIN_SIMD_X86_INTRINSICS && (defined(__clang__) || defined(__GNUC__))
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX2) != 0u) {
        return kain_simd_dot_avx2_raw(left, right, cells, lane_bias) % modulus;
    }
#endif
    return kain_simd_dot_scalar_raw(left, right, cells, lane_bias) % modulus;
}

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_avx2_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || passes < 0 || bias_mod <= 0 || phase_mod <= 0 || modulus <= 0) {
        return -1;
    }
#if KAIN_SIMD_X86_INTRINSICS && (defined(__clang__) || defined(__GNUC__))
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX2) != 0u) {
        return kain_simd_affine_fold_mod(
            kain_simd_affine_stats_avx2_raw(left, right, cells),
            passes,
            bias_mod,
            phase_mod,
            modulus
        );
    }
#endif
    return kain_simd_affine_fold_mod(
        kain_simd_affine_stats_scalar_raw(left, right, cells),
        passes,
        bias_mod,
        phase_mod,
        modulus
    );
}

int64_t abi_simd_i64_dot_i32_domain_avx512_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || modulus <= 0) {
        return -1;
    }
#if KAIN_SIMD_X86_INTRINSICS && (defined(__clang__) || defined(__GNUC__))
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX512F) != 0u) {
        return kain_simd_dot_avx512_raw(left, right, cells, lane_bias) % modulus;
    }
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX2) != 0u) {
        return kain_simd_dot_avx2_raw(left, right, cells, lane_bias) % modulus;
    }
#endif
    return kain_simd_dot_scalar_raw(left, right, cells, lane_bias) % modulus;
}

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_avx512_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
) {
    if (left == 0 || right == 0 || cells < 0 || passes < 0 || bias_mod <= 0 || phase_mod <= 0 || modulus <= 0) {
        return -1;
    }
#if KAIN_SIMD_X86_INTRINSICS && (defined(__clang__) || defined(__GNUC__))
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX512F) != 0u) {
        return kain_simd_affine_fold_mod(
            kain_simd_affine_stats_avx512_raw(left, right, cells),
            passes,
            bias_mod,
            phase_mod,
            modulus
        );
    }
    if ((abi_cpu_feature_mask() & KAIN_CPU_FEATURE_X86_AVX2) != 0u) {
        return kain_simd_affine_fold_mod(
            kain_simd_affine_stats_avx2_raw(left, right, cells),
            passes,
            bias_mod,
            phase_mod,
            modulus
        );
    }
#endif
    return kain_simd_affine_fold_mod(
        kain_simd_affine_stats_scalar_raw(left, right, cells),
        passes,
        bias_mod,
        phase_mod,
        modulus
    );
}
