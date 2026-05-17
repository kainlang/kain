#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kCount = 48;
constexpr int kSteps = 120;
constexpr std::int64_t kModulus = 1000000007LL;
constexpr std::int64_t kExpected = 7164293LL;

double absf(double value) {
    return value < 0.0 ? -value : value;
}
}  // namespace

int main() {
    const double dt = 0.045;
    const double g = 0.0125;
    const double softening_sq = 0.35 * 0.35;
    const double drag = 0.0015;

    std::vector<double> x(kCount);
    std::vector<double> y(kCount);
    std::vector<double> z(kCount);
    std::vector<double> vx(kCount);
    std::vector<double> vy(kCount);
    std::vector<double> vz(kCount);
    std::vector<double> ax(kCount);
    std::vector<double> ay(kCount);
    std::vector<double> az(kCount);
    std::vector<double> mass(kCount);

    for (int i = 0; i < kCount; ++i) {
        x[i] = (static_cast<double>((i * 37) % 29) - 14.0) * 0.73;
        y[i] = (static_cast<double>((i * 19) % 31) - 15.0) * 0.61;
        z[i] = (static_cast<double>((i * 23) % 27) - 13.0) * 0.67;
        vx[i] = (static_cast<double>((i * 11) % 9) - 4.0) * 0.031;
        vy[i] = (static_cast<double>((i * 7) % 11) - 5.0) * 0.027;
        vz[i] = (static_cast<double>((i * 5) % 13) - 6.0) * 0.023;
        mass[i] = 0.8 + static_cast<double>(i % 7) * 0.11;
    }

    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kCount; ++i) {
            const double xi = x[i];
            const double yi = y[i];
            const double zi = z[i];
            const double vxi = vx[i];
            const double vyi = vy[i];
            const double vzi = vz[i];
            double accx = -xi * 0.0008 - vxi * drag;
            double accy = -yi * 0.0008 - vyi * drag;
            double accz = -zi * 0.0008 - vzi * drag;
            for (int j = 0; j < kCount; ++j) {
                if (i == j) {
                    continue;
                }
                const double dx = x[j] - xi;
                const double dy = y[j] - yi;
                const double dz = z[j] - zi;
                const double dist_sq = dx * dx + dy * dy + dz * dz + softening_sq;
                const double inv_dist = 1.0 / std::sqrt(dist_sq);
                const double force_mag = g * mass[j] / dist_sq;
                const double scale = force_mag * inv_dist;
                accx += dx * scale;
                accy += dy * scale;
                accz += dz * scale;
            }
            ax[i] = accx;
            ay[i] = accy;
            az[i] = accz;
        }
        for (int i = 0; i < kCount; ++i) {
            vx[i] += ax[i] * dt;
            vy[i] += ay[i] * dt;
            vz[i] += az[i] * dt;
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            z[i] += vz[i] * dt;
        }
    }

    std::int64_t acc = 0;
    for (int i = 0; i < kCount; ++i) {
        const std::int64_t bucket_x = static_cast<std::int64_t>(std::floor((x[i] + 64.0) * 256.0));
        const std::int64_t bucket_y = static_cast<std::int64_t>(std::floor((y[i] + 64.0) * 256.0));
        const std::int64_t bucket_z = static_cast<std::int64_t>(std::floor((z[i] + 64.0) * 256.0));
        const std::int64_t bucket_v =
            static_cast<std::int64_t>(std::floor((absf(vx[i]) + absf(vy[i]) + absf(vz[i])) * 1024.0));
        acc = (acc + bucket_x + bucket_y * 3 + bucket_z * 5 + bucket_v * 7 + static_cast<std::int64_t>(i) * 11) %
              kModulus;
    }

    volatile std::int64_t observed = acc;
    return observed == kExpected ? 0 : 1;
}
