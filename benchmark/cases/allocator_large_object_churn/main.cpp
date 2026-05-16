#include <array>
#include <cstdint>
#include <vector>

constexpr std::int64_t ITERATIONS = 2'500;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 41'587'426;
constexpr std::array<std::size_t, 6> SIZES = {512, 1'024, 2'048, 4'096, 8'192, 16'384};

int main() {
    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < ITERATIONS; ++index) {
        const std::size_t cells = SIZES[static_cast<std::size_t>(index) % SIZES.size()];
        std::vector<std::int64_t> buffer(cells, 0);
        buffer[0] = index + 1;
        buffer[cells / 2] = (index * 3) + 7;
        buffer[cells - 1] = (index * 5) + 11;
        const std::int64_t observed = buffer[0] + buffer[cells / 2] + buffer[cells - 1];
        acc = (acc + observed + static_cast<std::int64_t>(cells)) % MODULUS;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
