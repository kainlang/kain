#ifndef KAIN_NATIVE_RAY_SPHERE_BENCHMARK_H
#define KAIN_NATIVE_RAY_SPHERE_BENCHMARK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_ray_sphere_intersection_checksum(
    int64_t iterations,
    int64_t ray_count,
    int64_t sphere_count,
    int64_t modulus
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_RAY_SPHERE_BENCHMARK_H */
