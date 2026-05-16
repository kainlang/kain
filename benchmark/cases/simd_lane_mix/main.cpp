#include <cstdint>
#include <vector>

constexpr std::int64_t CELLS = 32'768;
constexpr std::int64_t PASSES = 256;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 194'810'730;

[[gnu::noinline]] std::int64_t dot_vectorizable(const std::vector<std::int32_t>& left, const std::vector<std::int32_t>& right, std::int32_t lane_bias) {
    std::int64_t total = 0;
    for (std::int64_t index = 0; index < CELLS; ++index) {
        total += static_cast<std::int64_t>(left[static_cast<std::size_t>(index)] + lane_bias) *
                 static_cast<std::int64_t>(right[static_cast<std::size_t>(index)]);
    }
    return total;
}

int main() {
    std::vector<std::int32_t> left(static_cast<std::size_t>(CELLS));
    std::vector<std::int32_t> right(static_cast<std::size_t>(CELLS));
    for (std::int64_t index = 0; index < CELLS; ++index) {
        left[static_cast<std::size_t>(index)] = static_cast<std::int32_t>(((index * 31) + 7) % 1024);
        right[static_cast<std::size_t>(index)] = static_cast<std::int32_t>(((index * 17) + 3) % 512);
    }

    std::int64_t acc = 0;
    for (std::int64_t phase = 0; phase < PASSES; ++phase) {
        const std::int64_t inner = dot_vectorizable(left, right, static_cast<std::int32_t>(phase % 13)) % MODULUS;
        acc = (acc + inner + (phase % 29)) % MODULUS;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
