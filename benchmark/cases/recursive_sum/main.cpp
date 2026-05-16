#include <cstdint>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 5'000;
constexpr std::int64_t DEPTH = 128;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 41'280'000;

BENCH_NOINLINE std::int64_t recursive_sum(std::int64_t value) {
    if (value <= 0) {
        return 0;
    }
    return value + recursive_sum(value - 1);
}

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        acc = (acc + recursive_sum(DEPTH)) % MODULUS;
        ++i;
    }

    return acc == EXPECTED ? 0 : 1;
}
