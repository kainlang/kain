#ifndef KAIN_NATIVE_JSON_BENCHMARK_H
#define KAIN_NATIVE_JSON_BENCHMARK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_json_manual_roundtrip_literal_checksum(
    int64_t rounds,
    int64_t modulus
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_JSON_BENCHMARK_H */
