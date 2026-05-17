#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kNx = 8;
constexpr int kNy = 6;
constexpr int kNz = 5;
constexpr int kCellCount = kNx * kNy * kNz;
constexpr int kVxCount = (kNx + 1) * kNy * kNz;
constexpr int kVyCount = kNx * (kNy + 1) * kNz;
constexpr int kVzCount = kNx * kNy * (kNz + 1);
constexpr int kSteps = 140;
constexpr int kJacobiIters = 8;
constexpr std::int64_t kModulus = 1000000007LL;
constexpr std::int64_t kExpected = 56427256LL;

int idx(int x, int y, int z) {
    return z * kNx * kNy + y * kNx + x;
}

int idx_u(int x, int y, int z) {
    return z * (kNx + 1) * kNy + y * (kNx + 1) + x;
}

int idx_v(int x, int y, int z) {
    return z * kNx * (kNy + 1) + y * kNx + x;
}

int idx_w(int x, int y, int z) {
    return z * kNx * kNy + y * kNx + x;
}
}  // namespace

int main() {
    const double dt = 0.035;
    const double cell_size = 0.125;
    const double gravity_y = -0.14;
    const double buoyancy = 0.32;

    std::vector<double> velocity_x(kVxCount, 0.0);
    std::vector<double> velocity_y(kVyCount, 0.0);
    std::vector<double> velocity_z(kVzCount, 0.0);
    std::vector<double> pressure(kCellCount, 0.0);
    std::vector<double> pressure_old(kCellCount, 0.0);
    std::vector<double> divergence(kCellCount, 0.0);
    std::vector<double> temperature(kCellCount, 0.0);

    for (int z = 0; z < kNz; ++z) {
        for (int y = 0; y < kNy; ++y) {
            for (int x = 0; x < kNx; ++x) {
                temperature[idx(x, y, z)] = static_cast<double>((x * 3 + y * 5 + z * 7) % 11) * 0.14;
            }
        }
    }

    for (int z = 0; z < kNz; ++z) {
        for (int y = 0; y < kNy; ++y) {
            for (int x = 0; x <= kNx; ++x) {
                const int slot = idx_u(x, y, z);
                velocity_x[slot] = (static_cast<double>((slot * 7) % 13) - 6.0) * 0.03;
            }
        }
    }

    for (int z = 0; z < kNz; ++z) {
        for (int y = 0; y <= kNy; ++y) {
            for (int x = 0; x < kNx; ++x) {
                const int slot = idx_v(x, y, z);
                velocity_y[slot] = (static_cast<double>((slot * 5) % 17) - 8.0) * 0.02;
            }
        }
    }

    for (int z = 0; z <= kNz; ++z) {
        for (int y = 0; y < kNy; ++y) {
            for (int x = 0; x < kNx; ++x) {
                const int slot = idx_w(x, y, z);
                velocity_z[slot] = (static_cast<double>((slot * 11) % 19) - 9.0) * 0.025;
            }
        }
    }

    std::int64_t acc = 0;
    for (int step = 0; step < kSteps; ++step) {
        for (int z = 0; z < kNz; ++z) {
            for (int y = 0; y <= kNy; ++y) {
                for (int x = 0; x < kNx; ++x) {
                    const int slot = idx_v(x, y, z);
                    velocity_y[slot] += gravity_y * dt;
                    if (y < kNy) {
                        velocity_y[slot] += buoyancy * temperature[idx(x, y, z)] * dt;
                    }
                }
            }
        }

        for (int z = 0; z < kNz; ++z) {
            for (int y = 0; y < kNy; ++y) {
                for (int x = 0; x < kNx; ++x) {
                    const int cell = idx(x, y, z);
                    const double u_right = velocity_x[idx_u(x + 1, y, z)];
                    const double u_left = velocity_x[idx_u(x, y, z)];
                    const double v_top = velocity_y[idx_v(x, y + 1, z)];
                    const double v_bottom = velocity_y[idx_v(x, y, z)];
                    const double w_front = velocity_z[idx_w(x, y, z + 1)];
                    const double w_back = velocity_z[idx_w(x, y, z)];
                    divergence[cell] = ((u_right - u_left) + (v_top - v_bottom) + (w_front - w_back)) / cell_size;
                    pressure[cell] = 0.0;
                }
            }
        }

        for (int iter = 0; iter < kJacobiIters; ++iter) {
            pressure_old = pressure;
            for (int z = 1; z < kNz - 1; ++z) {
                for (int y = 1; y < kNy - 1; ++y) {
                    for (int x = 1; x < kNx - 1; ++x) {
                        const int cell = idx(x, y, z);
                        const double p_sum = pressure_old[idx(x + 1, y, z)] + pressure_old[idx(x - 1, y, z)] +
                                             pressure_old[idx(x, y + 1, z)] + pressure_old[idx(x, y - 1, z)] +
                                             pressure_old[idx(x, y, z + 1)] + pressure_old[idx(x, y, z - 1)];
                        pressure[cell] = (p_sum - cell_size * cell_size * divergence[cell]) / 6.0;
                    }
                }
            }
        }

        for (int z = 1; z < kNz - 1; ++z) {
            for (int y = 1; y < kNy - 1; ++y) {
                for (int x = 1; x < kNx; ++x) {
                    const int slot = idx_u(x, y, z);
                    const double p_right = pressure[idx(x, y, z)];
                    const double p_left = pressure[idx(x - 1, y, z)];
                    velocity_x[slot] -= (p_right - p_left) / cell_size;
                }
            }
        }

        for (int z = 1; z < kNz - 1; ++z) {
            for (int y = 1; y < kNy; ++y) {
                for (int x = 1; x < kNx - 1; ++x) {
                    const int slot = idx_v(x, y, z);
                    const double p_top = pressure[idx(x, y, z)];
                    const double p_bottom = pressure[idx(x, y - 1, z)];
                    velocity_y[slot] -= (p_top - p_bottom) / cell_size;
                }
            }
        }

        for (int z = 1; z < kNz; ++z) {
            for (int y = 1; y < kNy - 1; ++y) {
                for (int x = 1; x < kNx - 1; ++x) {
                    const int slot = idx_w(x, y, z);
                    const double p_front = pressure[idx(x, y, z)];
                    const double p_back = pressure[idx(x, y, z - 1)];
                    velocity_z[slot] -= (p_front - p_back) / cell_size;
                }
            }
        }

        const int sample = (step * 7) % kCellCount;
        const std::int64_t pressure_bucket = static_cast<std::int64_t>(std::floor((pressure[sample] + 64.0) * 4096.0));
        const std::int64_t divergence_bucket =
            static_cast<std::int64_t>(std::floor((divergence[sample] + 64.0) * 2048.0));
        acc = (acc + pressure_bucket + divergence_bucket + static_cast<std::int64_t>(step) * 13) % kModulus;
    }

    for (int sample = 0; sample < kCellCount; ++sample) {
        if ((sample % 17) == 0) {
            const std::int64_t pressure_bucket =
                static_cast<std::int64_t>(std::floor((pressure[sample] + 64.0) * 1024.0));
            const std::int64_t divergence_bucket =
                static_cast<std::int64_t>(std::floor((divergence[sample] + 64.0) * 512.0));
            acc = (acc + pressure_bucket + divergence_bucket + static_cast<std::int64_t>(sample) * 5) % kModulus;
        }
    }

    volatile std::int64_t observed = acc;
    return observed == kExpected ? 0 : 1;
}
