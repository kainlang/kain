#include <array>
#include <cstdint>

constexpr std::array<std::int64_t, 8> X = {3, 13, 29, 43, 61, 79, 101, 113};
constexpr std::array<std::int64_t, 8> Y = {5, 17, 31, 47, 67, 83, 103, 127};
constexpr std::array<std::int64_t, 8> VX = {7, 19, 37, 53, 71, 89, 107, 131};
constexpr std::array<std::int64_t, 8> VY = {11, 23, 41, 59, 73, 97, 109, 137};
constexpr std::array<bool, 8> ALIVE = {true, false, true, false, true, false, true, false};
constexpr std::int64_t ITERATIONS = 500'000;
constexpr std::int64_t EXPECTED = -1'399'052'960;

int main() {
    std::int64_t acc = 0;
    std::int64_t round = 0;

    while (round < ITERATIONS) {
        std::size_t lane = 0;
        while (lane < X.size()) {
            if (ALIVE[lane]) {
                acc += (((X[lane] + round) % 97) * VX[lane]) + Y[lane] + static_cast<std::int64_t>(lane);
            } else {
                acc = acc - (((Y[lane] + round) % 89) * VY[lane]) + X[lane] - static_cast<std::int64_t>(lane);
            }
            ++lane;
        }
        ++round;
    }

    return acc == EXPECTED ? 0 : 1;
}
