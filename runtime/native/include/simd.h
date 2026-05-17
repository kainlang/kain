#ifndef SIMD_H
#define SIMD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_simd_i64_dot_i32_domain_scalar_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
);

int64_t abi_simd_i64_dot_i32_domain_avx2_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
);

int64_t abi_simd_i64_dot_i32_domain_avx512_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t lane_bias,
    int64_t modulus
);

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_scalar_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
);

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_avx2_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
);

int64_t abi_simd_i64_dot_i32_domain_affine_accumulate_avx512_mod(
    const int64_t* left,
    const int64_t* right,
    int64_t cells,
    int64_t passes,
    int64_t bias_mod,
    int64_t phase_mod,
    int64_t modulus
);

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
);

#ifdef __cplusplus
}
#endif

#endif /* SIMD_H */
