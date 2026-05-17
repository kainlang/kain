#include <array>
#include <cstdint>

namespace {
constexpr std::int64_t kEntityCount = 32;
constexpr std::int64_t kIterations = 350000;
constexpr std::int64_t kModulus = 1000000007;
constexpr std::int64_t kExpected = 886666628;
} // namespace

int main() {
    std::array<std::int64_t, kEntityCount> position_x{};
    std::array<std::int64_t, kEntityCount> position_y{};
    std::array<std::int64_t, kEntityCount> velocity_x{};
    std::array<std::int64_t, kEntityCount> velocity_y{};
    std::array<std::int64_t, kEntityCount> health{};
    std::array<std::int64_t, kEntityCount> team{};
    std::array<bool, kEntityCount> active{};

    for (std::int64_t index = 0; index < kEntityCount; ++index) {
        position_x[static_cast<std::size_t>(index)] = ((index * 17) % 97) + 3;
        position_y[static_cast<std::size_t>(index)] = ((index * 29) % 89) + 5;
        velocity_x[static_cast<std::size_t>(index)] = ((index * 7) % 11) + 1;
        velocity_y[static_cast<std::size_t>(index)] = ((index * 5) % 13) + 2;
        health[static_cast<std::size_t>(index)] = ((index * 19) % 41) + 9;
        team[static_cast<std::size_t>(index)] = index % 4;
        active[static_cast<std::size_t>(index)] = (index % 3) != 1;
    }

    std::int64_t acc = 0;
    for (std::int64_t round = 0; round < kIterations; ++round) {
        const std::int64_t round_phase = round % 5;
        const std::int64_t round_bias = round % 7;
        for (std::int64_t lane = 0; lane < kEntityCount; ++lane) {
            const std::size_t slot = static_cast<std::size_t>(lane);
            if (active[slot] && health[slot] > ((round + lane) % 11)) {
                const std::int64_t motion = position_x[slot] + velocity_x[slot] * (round_phase + 1);
                const std::int64_t support = position_y[slot] + velocity_y[slot] * ((round_bias % 3) + 2);
                if (((team[slot] + round + lane) % 3) == 0) {
                    acc = (acc + motion + support + health[slot] + lane) % kModulus;
                } else {
                    acc = (acc + motion + (support * 2) + team[slot] + 17) % kModulus;
                }
            } else {
                acc = (acc + team[slot] + lane + 23) % kModulus;
            }
        }
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == kExpected ? 0 : 1;
}
