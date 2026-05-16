#include <cstdint>

constexpr std::int64_t ITERATIONS = 2'000'000;
constexpr std::int64_t ADDEND = 17;
constexpr std::int64_t OFFSET = ADDEND + 5;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 42'986'000;

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        acc = (acc + i + OFFSET) % MODULUS;
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
