#[derive(Copy, Clone)]
struct Ray {
    origin_x: f64,
    origin_y: f64,
    origin_z: f64,
    direction_x: f64,
    direction_y: f64,
    direction_z: f64,
}

#[derive(Copy, Clone)]
struct Sphere {
    center_x: f64,
    center_y: f64,
    center_z: f64,
    radius: f64,
}

fn seeded_ray(index: i64) -> Ray {
    let origin_x = -4.0 + index as f64 * 0.31;
    let origin_y = -1.5 + (index % 4) as f64 * 0.45;
    let origin_z = -6.0 + (index % 3) as f64 * 0.55;
    let direction_x = 0.2 + (index % 5) as f64 * 0.07;
    let direction_y = -0.1 + (index % 3) as f64 * 0.08;
    let direction_z = 1.0 + (index % 4) as f64 * 0.05;
    let length = (direction_x * direction_x + direction_y * direction_y + direction_z * direction_z).sqrt();
    Ray {
        origin_x,
        origin_y,
        origin_z,
        direction_x: direction_x / length,
        direction_y: direction_y / length,
        direction_z: direction_z / length,
    }
}

fn seeded_sphere(index: i64) -> Sphere {
    Sphere {
        center_x: -1.8 + index as f64 * 0.63,
        center_y: -0.7 + (index % 3) as f64 * 0.58,
        center_z: 2.4 + index as f64 * 0.71,
        radius: 0.75 + (index % 4) as f64 * 0.17,
    }
}

fn hit_distance(ray: Ray, sphere: Sphere) -> f64 {
    let local_x = ray.origin_x - sphere.center_x;
    let local_y = ray.origin_y - sphere.center_y;
    let local_z = ray.origin_z - sphere.center_z;
    let a = ray.direction_x * ray.direction_x + ray.direction_y * ray.direction_y + ray.direction_z * ray.direction_z;
    let b = 2.0 * ((local_x * ray.direction_x) + (local_y * ray.direction_y) + (local_z * ray.direction_z));
    let c = (local_x * local_x) + (local_y * local_y) + (local_z * local_z) - (sphere.radius * sphere.radius);
    let discriminant = (b * b) - (4.0 * a * c);
    if discriminant < 0.0 {
        return -1.0;
    }
    let root = discriminant.sqrt();
    let near_hit = (-b - root) / (2.0 * a);
    if near_hit > 0.001 {
        return near_hit;
    }
    let far_hit = (-b + root) / (2.0 * a);
    if far_hit > 0.001 {
        return far_hit;
    }
    -1.0
}

const ITERATIONS: i64 = 150_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 48_999_657;

fn main() {
    let mut rays = [seeded_ray(0); 12];
    let mut spheres = [seeded_sphere(0); 8];
    let mut index = 0_i64;
    while index < 12 {
        rays[index as usize] = seeded_ray(index);
        index += 1;
    }
    index = 0;
    while index < 8 {
        spheres[index as usize] = seeded_sphere(index);
        index += 1;
    }

    let mut acc = 0_i64;
    let mut round = 0_i64;
    while round < ITERATIONS {
        let phase = round % 11;
        let mut ray_index = 0_usize;
        while ray_index < rays.len() {
            let mut sphere_index = 0_usize;
            while sphere_index < spheres.len() {
                let distance = hit_distance(rays[ray_index], spheres[sphere_index]);
                if distance > 0.0 {
                    let bucket = (distance * 128.0).floor() as i64;
                    acc = (acc + bucket + ray_index as i64 * 17 + sphere_index as i64 * 31 + phase) % MODULUS;
                } else {
                    acc = (acc + ray_index as i64 + sphere_index as i64 + 3) % MODULUS;
                }
                sphere_index += 1;
            }
            ray_index += 1;
        }
        round += 1;
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
