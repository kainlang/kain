const PARTICLE_COUNT: usize = 72;
const RESOLUTION: usize = 16;
const STEPS: usize = 220;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 16_741_515;

fn snap(value: f64) -> f64 {
    (((value + 32.0) * 4096.0).floor() / 4096.0) - 32.0
}

fn main() {
    let dt = 0.021_f64;
    let radius = 0.24_f64;
    let radius_sq = radius * radius;
    let cell_size = 1.0_f64 / RESOLUTION as f64;
    let influence_radius = cell_size * 3.0;
    let influence_radius_sq = influence_radius * influence_radius;
    let inv_influence = 1.0 / influence_radius;

    let mut px = vec![0.0_f64; PARTICLE_COUNT];
    let mut py = vec![0.0_f64; PARTICLE_COUNT];
    let mut vx = vec![0.0_f64; PARTICLE_COUNT];
    let mut vy = vec![0.0_f64; PARTICLE_COUNT];

    for i in 0..PARTICLE_COUNT {
        px[i] = 0.1 + ((i * 37) % 71) as f64 / 71.0 * 0.8;
        py[i] = 0.1 + ((i * 19) % 67) as f64 / 67.0 * 0.8;
        vx[i] = (((i * 13) % 9) as i64 - 4) as f64 * 0.018;
        vy[i] = (((i * 11) % 11) as i64 - 5) as f64 * 0.016;
    }

    let mut acc = 0_i64;
    for step in 0..STEPS {
        let center_x = 0.5 + (((step * 7) % 9) as i64 - 4) as f64 * 0.03;
        let center_y = 0.5 + (((step * 5) % 7) as i64 - 3) as f64 * 0.04;
        let spin = 0.09 + (step % 5) as f64 * 0.012;
        let strength = 0.025 + (step % 7) as f64 * 0.004;

        for i in 0..PARTICLE_COUNT {
            let dx = center_x - px[i];
            let dy = center_y - py[i];
            let dist_sq = dx * dx + dy * dy;
            if dist_sq < radius_sq && dist_sq > 0.0001 {
                let dist = dist_sq.sqrt();
                let falloff = 1.0 - dist / radius;
                let inv_dist = 1.0 / dist;
                let grav = strength / (dist_sq + 0.01);
                let tx = -dy * inv_dist;
                let ty = dx * inv_dist;
                let drag_force = spin / (dist + 0.1);
                vx[i] += (((dx * inv_dist) * grav) + (tx * drag_force)) * falloff;
                vy[i] += (((dy * inv_dist) * grav) + (ty * drag_force)) * falloff;
            }
            px[i] += vx[i] * dt;
            py[i] += vy[i] * dt;
            if px[i] < 0.02 {
                px[i] = 0.02;
                vx[i] *= -0.65;
            } else if px[i] > 0.98 {
                px[i] = 0.98;
                vx[i] *= -0.65;
            }
            if py[i] < 0.02 {
                py[i] = 0.02;
                vy[i] *= -0.65;
            } else if py[i] > 0.98 {
                py[i] = 0.98;
                vy[i] *= -0.65;
            }
            px[i] = snap(px[i]);
            py[i] = snap(py[i]);
            vx[i] = snap(vx[i]);
            vy[i] = snap(vy[i]);
        }

        for gy in 0..RESOLUTION {
            let cell_y = (gy as f64 + 0.5) * cell_size;
            for gx in 0..RESOLUTION {
                let cell_x = (gx as f64 + 0.5) * cell_size;
                let mut grid_vx = 0.0_f64;
                let mut grid_vy = 0.0_f64;
                for i in 0..PARTICLE_COUNT {
                    let dx = px[i] - cell_x;
                    let dy = py[i] - cell_y;
                    let dist_sq = dx * dx + dy * dy;
                    if dist_sq < influence_radius_sq {
                        let dist = dist_sq.sqrt();
                        let weight = 1.0 - dist * inv_influence;
                        let weight_sq = weight * weight;
                        grid_vx += vx[i] * weight_sq;
                        grid_vy += vy[i] * weight_sq;
                    }
                }
                if ((gx + gy + step) % 5) == 0 {
                    let bucket_x = ((grid_vx + 8.0) * 64.0).floor() as i64;
                    let bucket_y = ((grid_vy + 8.0) * 64.0).floor() as i64;
                    acc = (acc + bucket_x + bucket_y + gx as i64 * 7 + gy as i64 * 11 + step as i64 * 3) % MODULUS;
                }
            }
        }
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
