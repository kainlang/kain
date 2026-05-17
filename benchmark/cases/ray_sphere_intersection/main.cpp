#include <array>
#include <cmath>
#include <cstdint>

struct Ray {
    double origin_x;
    double origin_y;
    double origin_z;
    double direction_x;
    double direction_y;
    double direction_z;
};

struct Sphere {
    double center_x;
    double center_y;
    double center_z;
    double radius;
};

Ray seeded_ray(std::int64_t index) {
    const double origin_x = -4.0 + static_cast<double>(index) * 0.31;
    const double origin_y = -1.5 + static_cast<double>(index % 4) * 0.45;
    const double origin_z = -6.0 + static_cast<double>(index % 3) * 0.55;
    const double direction_x = 0.2 + static_cast<double>(index % 5) * 0.07;
    const double direction_y = -0.1 + static_cast<double>(index % 3) * 0.08;
    const double direction_z = 1.0 + static_cast<double>(index % 4) * 0.05;
    const double length = std::sqrt(direction_x * direction_x + direction_y * direction_y + direction_z * direction_z);
    return Ray{
        origin_x,
        origin_y,
        origin_z,
        direction_x / length,
        direction_y / length,
        direction_z / length,
    };
}

Sphere seeded_sphere(std::int64_t index) {
    return Sphere{
        -1.8 + static_cast<double>(index) * 0.63,
        -0.7 + static_cast<double>(index % 3) * 0.58,
        2.4 + static_cast<double>(index) * 0.71,
        0.75 + static_cast<double>(index % 4) * 0.17,
    };
}

double hit_distance(const Ray& ray, const Sphere& sphere) {
    const double local_x = ray.origin_x - sphere.center_x;
    const double local_y = ray.origin_y - sphere.center_y;
    const double local_z = ray.origin_z - sphere.center_z;
    const double a = ray.direction_x * ray.direction_x + ray.direction_y * ray.direction_y + ray.direction_z * ray.direction_z;
    const double b = 2.0 * ((local_x * ray.direction_x) + (local_y * ray.direction_y) + (local_z * ray.direction_z));
    const double c = (local_x * local_x) + (local_y * local_y) + (local_z * local_z) - (sphere.radius * sphere.radius);
    const double discriminant = (b * b) - (4.0 * a * c);
    if (discriminant < 0.0) {
        return -1.0;
    }
    const double root = std::sqrt(discriminant);
    const double near_hit = (-b - root) / (2.0 * a);
    if (near_hit > 0.001) {
        return near_hit;
    }
    const double far_hit = (-b + root) / (2.0 * a);
    if (far_hit > 0.001) {
        return far_hit;
    }
    return -1.0;
}

int main() {
    constexpr std::int64_t kIterations = 150000;
    constexpr std::int64_t kModulus = 1000000007;
    constexpr std::int64_t kExpected = 48999657;
    std::array<Ray, 12> rays{};
    std::array<Sphere, 8> spheres{};
    for (std::int64_t index = 0; index < 12; ++index) {
        rays[static_cast<std::size_t>(index)] = seeded_ray(index);
    }
    for (std::int64_t index = 0; index < 8; ++index) {
        spheres[static_cast<std::size_t>(index)] = seeded_sphere(index);
    }

    std::int64_t acc = 0;
    for (std::int64_t round = 0; round < kIterations; ++round) {
        const std::int64_t phase = round % 11;
        for (std::size_t ray_index = 0; ray_index < rays.size(); ++ray_index) {
            for (std::size_t sphere_index = 0; sphere_index < spheres.size(); ++sphere_index) {
                const double distance = hit_distance(rays[ray_index], spheres[sphere_index]);
                if (distance > 0.0) {
                    const std::int64_t bucket = static_cast<std::int64_t>(std::floor(distance * 128.0));
                    acc = (acc + bucket + static_cast<std::int64_t>(ray_index) * 17 +
                           static_cast<std::int64_t>(sphere_index) * 31 + phase) %
                          kModulus;
                } else {
                    acc = (acc + static_cast<std::int64_t>(ray_index) + static_cast<std::int64_t>(sphere_index) + 3) % kModulus;
                }
            }
        }
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == kExpected ? 0 : 1;
}
