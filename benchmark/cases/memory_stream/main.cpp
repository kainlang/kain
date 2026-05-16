#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::size_t CELLS = 262'144;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 149'653'729;

int main() {
    std::vector<std::int64_t> buffer(CELLS, 0);
    std::size_t i = 0;
    while (i < CELLS) {
        buffer[i] = ((static_cast<std::int64_t>(i) * 31) + 7) % MODULUS;
        ++i;
    }

    std::int64_t checksum = 0;
    std::size_t j = 0;
    while (j < CELLS) {
        const volatile std::int64_t* cell = &buffer[j];
        checksum = (checksum + *cell) % MODULUS;
        ++j;
    }

    return checksum == EXPECTED ? 0 : 1;
}
