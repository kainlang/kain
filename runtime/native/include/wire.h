#ifndef KAIN_NATIVE_WIRE_H
#define KAIN_NATIVE_WIRE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_wire_zero_copy_binary_checksum(
    int64_t iterations,
    int64_t packet_count,
    int64_t words_per_packet,
    int64_t modulus
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_WIRE_H */
