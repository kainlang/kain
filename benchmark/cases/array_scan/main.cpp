#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::array<std::int64_t, 8> VALUES = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr std::int64_t ITERATIONS = 500'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 103'499'994;

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;

    while (i < ITERATIONS) {
        std::int64_t inner = 0;
        std::size_t index = 0;
        while (index < VALUES.size()) {
            inner = (inner + (VALUES[index] * static_cast<std::int64_t>(index + 1))) % MODULUS;
            ++index;
        }
        acc = (acc + inner + (i % 7)) % MODULUS;
        ++i;
    }

    return acc == EXPECTED ? 0 : 1;
}
