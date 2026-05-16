#include <cstdint>
#include <memory>

constexpr std::int64_t ITERATIONS = 50'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 250'324'993;

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        auto cell = std::make_unique<std::int64_t>(i + 7);
        const volatile std::int64_t* value_ptr = cell.get();
        const std::int64_t value = *value_ptr;
        cell.reset();
        acc = (acc + value) % MODULUS;
        ++i;
    }
    return acc == EXPECTED ? 0 : 1;
}
