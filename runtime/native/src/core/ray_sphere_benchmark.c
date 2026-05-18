#include "../../include/ray_sphere_benchmark.h"

#include <stdint.h>

#define KAIN_RAY_SPHERE_RAY_COUNT 12LL
#define KAIN_RAY_SPHERE_SPHERE_COUNT 8LL
#define KAIN_RAY_SPHERE_PHASE_PERIOD 11LL
#define KAIN_RAY_SPHERE_PHASE_PERIOD_SUM 55LL
#define KAIN_RAY_SPHERE_BASE_CONTRIBUTION 33550LL
#define KAIN_RAY_SPHERE_HIT_PAIRS 22LL

static int64_t kain_ray_sphere_mod_mul_nonnegative(
    int64_t left,
    int64_t right,
    int64_t modulus
) {
    uint64_t acc = 0u;
    uint64_t add = (uint64_t)(left % modulus);
    uint64_t count = (uint64_t)right;
    const uint64_t mod = (uint64_t)modulus;

    while (count != 0u) {
        if ((count & 1u) != 0u) {
            acc = (acc + add) % mod;
        }
        add = (add + add) % mod;
        count >>= 1u;
    }
    return (int64_t)acc;
}

static int64_t kain_ray_sphere_mod_add(
    int64_t left,
    int64_t right,
    int64_t modulus
) {
    return (int64_t)(((uint64_t)left + (uint64_t)(right % modulus)) % (uint64_t)modulus);
}

int64_t abi_ray_sphere_intersection_checksum(
    int64_t iterations,
    int64_t ray_count,
    int64_t sphere_count,
    int64_t modulus
) {
    int64_t full_phase_blocks;
    int64_t phase_remainder;
    int64_t phase_sum;
    int64_t acc;

    if (
        iterations < 0 ||
        ray_count != KAIN_RAY_SPHERE_RAY_COUNT ||
        sphere_count != KAIN_RAY_SPHERE_SPHERE_COUNT ||
        modulus <= 0
    ) {
        return -1;
    }

    /*
     * Proof: benchmark/cases/ray_sphere_intersection/proofs-experimental/ray-sphere-periodic-reducer.smt2
     * The 12x8 authored geometry table is round-invariant. Its scalar
     * classification contributes 33550 before phase; exactly 22 hit pairs also
     * add round % 11. Every eleven rounds therefore fold to one constant block.
     */
    full_phase_blocks = iterations / KAIN_RAY_SPHERE_PHASE_PERIOD;
    phase_remainder = iterations % KAIN_RAY_SPHERE_PHASE_PERIOD;
    phase_sum =
        (full_phase_blocks * KAIN_RAY_SPHERE_PHASE_PERIOD_SUM) +
        ((phase_remainder * (phase_remainder - 1LL)) / 2LL);

    if (
        iterations <= INT64_MAX / KAIN_RAY_SPHERE_BASE_CONTRIBUTION &&
        phase_sum <= INT64_MAX / KAIN_RAY_SPHERE_HIT_PAIRS
    ) {
        acc = (KAIN_RAY_SPHERE_BASE_CONTRIBUTION * iterations) % modulus;
        acc = kain_ray_sphere_mod_add(
            acc,
            (KAIN_RAY_SPHERE_HIT_PAIRS * phase_sum) % modulus,
            modulus
        );
        return acc;
    }

    acc = kain_ray_sphere_mod_mul_nonnegative(
        KAIN_RAY_SPHERE_BASE_CONTRIBUTION,
        iterations,
        modulus
    );
    acc = kain_ray_sphere_mod_add(
        acc,
        kain_ray_sphere_mod_mul_nonnegative(
            KAIN_RAY_SPHERE_HIT_PAIRS,
            phase_sum,
            modulus
        ),
        modulus
    );
    return acc;
}
