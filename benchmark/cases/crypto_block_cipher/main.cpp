#include <cstdint>

namespace {
constexpr std::int64_t kRounds = 220000;
constexpr std::int64_t kMask = 2147483647;
constexpr std::int64_t kExpected = 1528465470;
constexpr std::int64_t kKeys[8] = {1267611, 2386093, 1059128, 5596791, 9022413, 3227993, 2562088, 4342338};
} // namespace

std::int64_t rotl31(std::int64_t value, int shift) {
    return (((value << shift) & kMask) | (value >> (31 - shift))) & kMask;
}

int main() {
    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < kRounds; ++index) {
        std::int64_t left = ((index * 1103515) + 12345) & kMask;
        std::int64_t right = ((index * 2654435) + 54321) & kMask;
        for (std::int64_t round_key : kKeys) {
            const std::int64_t mixed = (rotl31((left + round_key + 13) & kMask, 5) ^ right) & kMask;
            const std::int64_t next_right = (mixed + ((right & 255) * 17) + round_key) & kMask;
            left = right;
            right = next_right;
        }
        acc = (acc + left + right + (left ^ right)) & kMask;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == kExpected ? 0 : 1;
}
