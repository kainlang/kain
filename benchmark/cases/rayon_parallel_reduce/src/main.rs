use rayon::prelude::*;

const ITERATIONS: u64 = 4_000_000;
const MODULUS: u64 = 1_000_000_007;
const EXPECTED: u64 = 987_976_414;

fn lane_value(i: u64) -> u64 {
    ((i * 31) + (i / 8)) % 1_000_003
}

fn main() {
    let acc = (0..ITERATIONS)
        .into_par_iter()
        .map(lane_value)
        .reduce(|| 0_u64, |left, right| (left + right) % MODULUS);

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
