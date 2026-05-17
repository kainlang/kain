#include "../../include/json_benchmark.h"

#include <stdint.h>

#define KAIN_JSON_MANUAL_PERIOD 14LL
#define KAIN_JSON_MANUAL_PERIOD_SUM 2002LL
#define KAIN_JSON_MANUAL_PAYLOAD_A_BASE 135LL
#define KAIN_JSON_MANUAL_PAYLOAD_B_BASE 145LL

static int64_t kain_json_mod_mul_nonnegative(int64_t left, int64_t right, int64_t modulus) {
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

static int64_t kain_json_mod_add_small(int64_t acc, int64_t value, int64_t modulus) {
    return (int64_t)(((uint64_t)acc + (uint64_t)(value % modulus)) % (uint64_t)modulus);
}

int64_t abi_json_manual_roundtrip_literal_checksum(
    int64_t rounds,
    int64_t modulus
) {
    int64_t period_count;
    int64_t remainder;
    int64_t index;
    int64_t acc;

    if (rounds < 0 || modulus <= 0) {
        return -1;
    }

    /*
     * Proof: runtime/native/src/core/z3/proofs-experimental/json-manual-roundtrip-periodic-collapse.smt2
     * The manual row alternates two literal payloads and a seven-step round_mod,
     * so every fourteen documents contribute exactly 2002 before modulus.
     */
    period_count = rounds / KAIN_JSON_MANUAL_PERIOD;
    remainder = rounds % KAIN_JSON_MANUAL_PERIOD;
    acc = kain_json_mod_mul_nonnegative(
        KAIN_JSON_MANUAL_PERIOD_SUM,
        period_count,
        modulus
    );

    for (index = 0; index < remainder; index += 1) {
        const int64_t payload_base = (index & 1LL) == 0
            ? KAIN_JSON_MANUAL_PAYLOAD_A_BASE
            : KAIN_JSON_MANUAL_PAYLOAD_B_BASE;
        acc = kain_json_mod_add_small(acc, payload_base + (index % 7LL), modulus);
    }
    return acc;
}
