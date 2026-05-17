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

#ifdef __cplusplus
}
#endif

#endif /* SIMD_H */
