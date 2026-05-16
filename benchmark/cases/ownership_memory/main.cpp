#include <cstdint>
#include <memory>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 750'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 758'650'175;

BENCH_NOINLINE std::int64_t run() {
    auto cell = std::make_unique<std::int64_t>(0);
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        const std::int64_t current = *cell;
        *cell = ((current * 33) + i + 7) % MODULUS;
        ++i;
    }
    const volatile std::int64_t* observed_ptr = cell.get();
    return *observed_ptr;
}

int main() {
    return run() == EXPECTED ? 0 : 1;
}
