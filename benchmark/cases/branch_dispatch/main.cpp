#include <cstdint>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 3'000'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 632'706'747;

BENCH_NOINLINE std::int64_t classify(std::int64_t value) {
    const std::int64_t tag = value % 8;
    if (tag == 0) {
        return value + 1;
    }
    if (tag == 1) {
        return (value * 3) + 7;
    }
    if (tag == 2) {
        return value - 5;
    }
    if (tag == 3) {
        return (value * value) + 11;
    }
    if (tag == 4) {
        return value + 17;
    }
    if (tag == 5) {
        return (value * 5) - 13;
    }
    if (tag == 6) {
        return value + 23;
    }
    return value - 11;
}

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        acc = (acc + classify(i)) % MODULUS;
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
