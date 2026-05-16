#include <cstdint>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 1'000'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 393'996'945;

struct BenchPair {
    std::int64_t x;
    std::int64_t y;
};

BENCH_NOINLINE BenchPair make_pair(std::int64_t seed) {
    return BenchPair{
        seed % 97,
        (seed * 7) % 101,
    };
}

BENCH_NOINLINE std::int64_t score_pair(BenchPair pair) {
    return (pair.x * 3) + (pair.y * 5);
}

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        const BenchPair pair = make_pair(i);
        acc = (acc + score_pair(pair)) % MODULUS;
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
