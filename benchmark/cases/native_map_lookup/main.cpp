#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

constexpr std::int64_t ITERATIONS = 1'200'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 351'450'000;
constexpr std::array<const char*, 16> KEYS = {
    "alpha",
    "beta",
    "gamma",
    "delta",
    "epsilon",
    "zeta",
    "eta",
    "theta",
    "iota",
    "kappa",
    "lambda",
    "mu",
    "nu",
    "xi",
    "omicron",
    "pi",
};
constexpr std::array<std::int64_t, 16> VALUES = {11, 23, 37, 41, 53, 67, 79, 83, 97, 101, 113, 127, 131, 149, 157, 173};

int main() {
    std::unordered_map<std::string, std::int64_t> metrics;
    for (std::size_t index = 0; index < KEYS.size(); ++index) {
        metrics.emplace(KEYS[index], VALUES[index]);
    }

    std::int64_t acc = 0;
    for (std::int64_t i = 0; i < ITERATIONS; ++i) {
        const std::size_t slot = static_cast<std::size_t>(i & 15);
        const std::int64_t value = metrics.at(KEYS[slot]);
        acc = (acc + (value * ((i % 5) + 1)) + (static_cast<std::int64_t>(slot) * 3)) % MODULUS;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
