#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kParticleCount = 72;
constexpr int kResolution = 16;
constexpr int kSteps = 220;
constexpr std::int64_t kModulus = 1000000007LL;
constexpr std::int64_t kExpected = 16741515LL;

double snap(double value) {
    return std::floor((value + 32.0) * 4096.0) / 4096.0 - 32.0;
}
}  // namespace

int main() {
    const double dt = 0.021;
    const double radius = 0.24;
    const double radius_sq = radius * radius;
    const double cell_size = 1.0 / static_cast<double>(kResolution);
    const double influence_radius = cell_size * 3.0;
    const double influence_radius_sq = influence_radius * influence_radius;
    const double inv_influence = 1.0 / influence_radius;

    std::vector<double> px(kParticleCount);
    std::vector<double> py(kParticleCount);
    std::vector<double> vx(kParticleCount);
    std::vector<double> vy(kParticleCount);

    for (int i = 0; i < kParticleCount; ++i) {
        px[i] = 0.1 + static_cast<double>((i * 37) % 71) / 71.0 * 0.8;
        py[i] = 0.1 + static_cast<double>((i * 19) % 67) / 67.0 * 0.8;
        vx[i] = (static_cast<double>((i * 13) % 9) - 4.0) * 0.018;
        vy[i] = (static_cast<double>((i * 11) % 11) - 5.0) * 0.016;
    }

    std::int64_t acc = 0;
    for (int step = 0; step < kSteps; ++step) {
        const double center_x = 0.5 + (static_cast<double>((step * 7) % 9) - 4.0) * 0.03;
        const double center_y = 0.5 + (static_cast<double>((step * 5) % 7) - 3.0) * 0.04;
        const double spin = 0.09 + static_cast<double>(step % 5) * 0.012;
        const double strength = 0.025 + static_cast<double>(step % 7) * 0.004;

        for (int i = 0; i < kParticleCount; ++i) {
            const double dx = center_x - px[i];
            const double dy = center_y - py[i];
            const double dist_sq = dx * dx + dy * dy;
            if (dist_sq < radius_sq && dist_sq > 0.0001) {
                const double dist = std::sqrt(dist_sq);
                const double falloff = 1.0 - (dist / radius);
                const double inv_dist = 1.0 / dist;
                const double grav = strength / (dist_sq + 0.01);
                const double tx = -dy * inv_dist;
                const double ty = dx * inv_dist;
                const double drag_force = spin / (dist + 0.1);
                vx[i] += (((dx * inv_dist) * grav) + (tx * drag_force)) * falloff;
                vy[i] += (((dy * inv_dist) * grav) + (ty * drag_force)) * falloff;
            }
            px[i] += vx[i] * dt;
            py[i] += vy[i] * dt;
            if (px[i] < 0.02) {
                px[i] = 0.02;
                vx[i] *= -0.65;
            } else if (px[i] > 0.98) {
                px[i] = 0.98;
                vx[i] *= -0.65;
            }
            if (py[i] < 0.02) {
                py[i] = 0.02;
                vy[i] *= -0.65;
            } else if (py[i] > 0.98) {
                py[i] = 0.98;
                vy[i] *= -0.65;
            }
            px[i] = snap(px[i]);
            py[i] = snap(py[i]);
            vx[i] = snap(vx[i]);
            vy[i] = snap(vy[i]);
        }

        for (int gy = 0; gy < kResolution; ++gy) {
            const double cell_y = (static_cast<double>(gy) + 0.5) * cell_size;
            for (int gx = 0; gx < kResolution; ++gx) {
                const double cell_x = (static_cast<double>(gx) + 0.5) * cell_size;
                double grid_vx = 0.0;
                double grid_vy = 0.0;
                for (int i = 0; i < kParticleCount; ++i) {
                    const double dx = px[i] - cell_x;
                    const double dy = py[i] - cell_y;
                    const double dist_sq = dx * dx + dy * dy;
                    if (dist_sq < influence_radius_sq) {
                        const double dist = std::sqrt(dist_sq);
                        const double weight = 1.0 - dist * inv_influence;
                        const double weight_sq = weight * weight;
                        grid_vx += vx[i] * weight_sq;
                        grid_vy += vy[i] * weight_sq;
                    }
                }
                if (((gx + gy + step) % 5) == 0) {
                    const std::int64_t bucket_x = static_cast<std::int64_t>(std::floor((grid_vx + 8.0) * 64.0));
                    const std::int64_t bucket_y = static_cast<std::int64_t>(std::floor((grid_vy + 8.0) * 64.0));
                    acc = (acc + bucket_x + bucket_y + static_cast<std::int64_t>(gx) * 7 +
                           static_cast<std::int64_t>(gy) * 11 + static_cast<std::int64_t>(step) * 3) %
                          kModulus;
                }
            }
        }
    }

    volatile std::int64_t observed = acc;
    return observed == kExpected ? 0 : 1;
}
