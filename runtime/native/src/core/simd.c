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
        products = (KainSimdI64x8)__builtin_ia32_pmuludq512(
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
