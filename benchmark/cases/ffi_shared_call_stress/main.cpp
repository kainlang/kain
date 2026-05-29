#include <cstdint>

#include "../../lanes/ffi_boundary/native/ffi_boundary.h"

constexpr std::int64_t ITERATIONS = 5'000'000;
constexpr std::int64_t EXPECTED = 374'126'489;

int main() {
    std::int64_t acc = 1;
    for (std::int64_t index = 0; index < ITERATIONS; ++index) {
        acc = ffi_boundary_mix(acc + index, index);
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
