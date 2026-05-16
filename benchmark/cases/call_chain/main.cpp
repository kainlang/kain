#include <cstdint>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 1'500'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 61'920'954;

BENCH_NOINLINE std::int64_t step_a(std::int64_t value) {
    return ((value * 3) + 1) % MODULUS;
}

BENCH_NOINLINE std::int64_t step_b(std::int64_t value) {
    return ((step_a(value) + 5) * 7) % MODULUS;
}

BENCH_NOINLINE std::int64_t step_c(std::int64_t value) {
    return (step_b(value) + step_a(value + 11) + 13) % MODULUS;
}

BENCH_NOINLINE std::int64_t step_d(std::int64_t value) {
    return ((step_c(value) * 3) + step_b(value + 17) + 19) % MODULUS;
}

int main() {
    std::int64_t acc = 1;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        acc = step_d(acc + i);
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
