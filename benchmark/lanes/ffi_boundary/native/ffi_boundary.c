#include "ffi_boundary.h"

int64_t ffi_boundary_mix(int64_t value, int64_t salt) {
    const int64_t modulus = 1000000007LL;
    const int64_t lane_a =
        ((value * 1103515245LL) + 12345LL + (salt * 97LL)) % modulus;
    const int64_t lane_b =
        ((value / 7LL) + (salt * 31LL) + 17LL) % modulus;
    return (lane_a + lane_b + 19LL) % modulus;
}
