const COUNT: usize = 48;
const STEPS: usize = 120;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 7_164_293;

fn absf(value: f64) -> f64 {
    if value < 0.0 { -value } else { value }
}

fn main() {
    let dt = 0.045_f64;
    let g = 0.0125_f64;
    let softening_sq = 0.35_f64 * 0.35_f64;
    let drag = 0.0015_f64;

    let mut x = vec![0.0_f64; COUNT];
    let mut y = vec![0.0_f64; COUNT];
    let mut z = vec![0.0_f64; COUNT];
    let mut vx = vec![0.0_f64; COUNT];
    let mut vy = vec![0.0_f64; COUNT];
    let mut vz = vec![0.0_f64; COUNT];
    let mut ax = vec![0.0_f64; COUNT];
    let mut ay = vec![0.0_f64; COUNT];
    let mut az = vec![0.0_f64; COUNT];
    let mut mass = vec![0.0_f64; COUNT];

    for i in 0..COUNT {
        x[i] = (((i * 37) % 29) as i64 - 14) as f64 * 0.73;
        y[i] = (((i * 19) % 31) as i64 - 15) as f64 * 0.61;
        z[i] = (((i * 23) % 27) as i64 - 13) as f64 * 0.67;
        vx[i] = (((i * 11) % 9) as i64 - 4) as f64 * 0.031;
        vy[i] = (((i * 7) % 11) as i64 - 5) as f64 * 0.027;
        vz[i] = (((i * 5) % 13) as i64 - 6) as f64 * 0.023;
        mass[i] = 0.8 + (i % 7) as f64 * 0.11;
    }

    for _ in 0..STEPS {
        for i in 0..COUNT {
            let xi = x[i];
            let yi = y[i];
            let zi = z[i];
            let vxi = vx[i];
            let vyi = vy[i];
            let vzi = vz[i];
            let mut accx = -xi * 0.0008 - vxi * drag;
            let mut accy = -yi * 0.0008 - vyi * drag;
            let mut accz = -zi * 0.0008 - vzi * drag;
            for j in 0..COUNT {
                if i == j {
                    continue;
                }
                let dx = x[j] - xi;
                let dy = y[j] - yi;
                let dz = z[j] - zi;
                let dist_sq = dx * dx + dy * dy + dz * dz + softening_sq;
                let inv_dist = dist_sq.sqrt().recip();
                let force_mag = g * mass[j] / dist_sq;
                let scale = force_mag * inv_dist;
                accx += dx * scale;
                accy += dy * scale;
                accz += dz * scale;
            }
            ax[i] = accx;
            ay[i] = accy;
            az[i] = accz;
        }
        for i in 0..COUNT {
            vx[i] += ax[i] * dt;
            vy[i] += ay[i] * dt;
            vz[i] += az[i] * dt;
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            z[i] += vz[i] * dt;
        }
    }

    let mut acc = 0_i64;
    for i in 0..COUNT {
        let bucket_x = ((x[i] + 64.0) * 256.0).floor() as i64;
        let bucket_y = ((y[i] + 64.0) * 256.0).floor() as i64;
        let bucket_z = ((z[i] + 64.0) * 256.0).floor() as i64;
        let bucket_v = ((absf(vx[i]) + absf(vy[i]) + absf(vz[i])) * 1024.0).floor() as i64;
        acc = (acc + bucket_x + bucket_y * 3 + bucket_z * 5 + bucket_v * 7 + i as i64 * 11) % MODULUS;
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
