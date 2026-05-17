const NX: usize = 8;
const NY: usize = 6;
const NZ: usize = 5;
const CELL_COUNT: usize = NX * NY * NZ;
const VX_COUNT: usize = (NX + 1) * NY * NZ;
const VY_COUNT: usize = NX * (NY + 1) * NZ;
const VZ_COUNT: usize = NX * NY * (NZ + 1);
const STEPS: usize = 140;
const JACOBI_ITERS: usize = 8;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 56_427_256;

fn idx(x: usize, y: usize, z: usize) -> usize {
    z * NX * NY + y * NX + x
}

fn idx_u(x: usize, y: usize, z: usize) -> usize {
    z * (NX + 1) * NY + y * (NX + 1) + x
}

fn idx_v(x: usize, y: usize, z: usize) -> usize {
    z * NX * (NY + 1) + y * NX + x
}

fn idx_w(x: usize, y: usize, z: usize) -> usize {
    z * NX * NY + y * NX + x
}

fn main() {
    let dt = 0.035_f64;
    let cell_size = 0.125_f64;
    let gravity_y = -0.14_f64;
    let buoyancy = 0.32_f64;

    let mut velocity_x = vec![0.0_f64; VX_COUNT];
    let mut velocity_y = vec![0.0_f64; VY_COUNT];
    let mut velocity_z = vec![0.0_f64; VZ_COUNT];
    let mut pressure = vec![0.0_f64; CELL_COUNT];
    let mut pressure_old = vec![0.0_f64; CELL_COUNT];
    let mut divergence = vec![0.0_f64; CELL_COUNT];
    let mut temperature = vec![0.0_f64; CELL_COUNT];

    for z in 0..NZ {
        for y in 0..NY {
            for x in 0..NX {
                temperature[idx(x, y, z)] = ((x * 3 + y * 5 + z * 7) % 11) as f64 * 0.14;
            }
        }
    }

    for z in 0..NZ {
        for y in 0..NY {
            for x in 0..=NX {
                let slot = idx_u(x, y, z);
                velocity_x[slot] = ((slot * 7) % 13) as i64 as f64 * 0.03 - 6.0 * 0.03;
            }
        }
    }

    for z in 0..NZ {
        for y in 0..=NY {
            for x in 0..NX {
                let slot = idx_v(x, y, z);
                velocity_y[slot] = ((slot * 5) % 17) as i64 as f64 * 0.02 - 8.0 * 0.02;
            }
        }
    }

    for z in 0..=NZ {
        for y in 0..NY {
            for x in 0..NX {
                let slot = idx_w(x, y, z);
                velocity_z[slot] = ((slot * 11) % 19) as i64 as f64 * 0.025 - 9.0 * 0.025;
            }
        }
    }

    let mut acc = 0_i64;
    for step in 0..STEPS {
        for z in 0..NZ {
            for y in 0..=NY {
                for x in 0..NX {
                    let slot = idx_v(x, y, z);
                    velocity_y[slot] += gravity_y * dt;
                    if y < NY {
                        velocity_y[slot] += buoyancy * temperature[idx(x, y, z)] * dt;
                    }
                }
            }
        }

        for z in 0..NZ {
            for y in 0..NY {
                for x in 0..NX {
                    let cell = idx(x, y, z);
                    let u_right = velocity_x[idx_u(x + 1, y, z)];
                    let u_left = velocity_x[idx_u(x, y, z)];
                    let v_top = velocity_y[idx_v(x, y + 1, z)];
                    let v_bottom = velocity_y[idx_v(x, y, z)];
                    let w_front = velocity_z[idx_w(x, y, z + 1)];
                    let w_back = velocity_z[idx_w(x, y, z)];
                    divergence[cell] = ((u_right - u_left) + (v_top - v_bottom) + (w_front - w_back)) / cell_size;
                    pressure[cell] = 0.0;
                }
            }
        }

        for _ in 0..JACOBI_ITERS {
            pressure_old.copy_from_slice(&pressure);
            for z in 1..(NZ - 1) {
                for y in 1..(NY - 1) {
                    for x in 1..(NX - 1) {
                        let cell = idx(x, y, z);
                        let p_sum = pressure_old[idx(x + 1, y, z)]
                            + pressure_old[idx(x - 1, y, z)]
                            + pressure_old[idx(x, y + 1, z)]
                            + pressure_old[idx(x, y - 1, z)]
                            + pressure_old[idx(x, y, z + 1)]
                            + pressure_old[idx(x, y, z - 1)];
                        pressure[cell] = (p_sum - cell_size * cell_size * divergence[cell]) / 6.0;
                    }
                }
            }
        }

        for z in 1..(NZ - 1) {
            for y in 1..(NY - 1) {
                for x in 1..NX {
                    let slot = idx_u(x, y, z);
                    let p_right = pressure[idx(x, y, z)];
                    let p_left = pressure[idx(x - 1, y, z)];
                    velocity_x[slot] -= (p_right - p_left) / cell_size;
                }
            }
        }

        for z in 1..(NZ - 1) {
            for y in 1..NY {
                for x in 1..(NX - 1) {
                    let slot = idx_v(x, y, z);
                    let p_top = pressure[idx(x, y, z)];
                    let p_bottom = pressure[idx(x, y - 1, z)];
                    velocity_y[slot] -= (p_top - p_bottom) / cell_size;
                }
            }
        }

        for z in 1..NZ {
            for y in 1..(NY - 1) {
                for x in 1..(NX - 1) {
                    let slot = idx_w(x, y, z);
                    let p_front = pressure[idx(x, y, z)];
                    let p_back = pressure[idx(x, y, z - 1)];
                    velocity_z[slot] -= (p_front - p_back) / cell_size;
                }
            }
        }

        let sample = (step * 7) % CELL_COUNT;
        let pressure_bucket = ((pressure[sample] + 64.0) * 4096.0).floor() as i64;
        let divergence_bucket = ((divergence[sample] + 64.0) * 2048.0).floor() as i64;
        acc = (acc + pressure_bucket + divergence_bucket + step as i64 * 13) % MODULUS;
    }

    for sample in 0..CELL_COUNT {
        if (sample % 17) == 0 {
            let pressure_bucket = ((pressure[sample] + 64.0) * 1024.0).floor() as i64;
            let divergence_bucket = ((divergence[sample] + 64.0) * 512.0).floor() as i64;
            acc = (acc + pressure_bucket + divergence_bucket + sample as i64 * 5) % MODULUS;
        }
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
