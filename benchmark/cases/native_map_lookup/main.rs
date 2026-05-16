use std::collections::HashMap;

const ITERATIONS: i64 = 1_200_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 351_450_000;
const KEYS: [&str; 16] = [
    "alpha",
    "beta",
    "gamma",
    "delta",
    "epsilon",
    "zeta",
    "eta",
    "theta",
    "iota",
    "kappa",
    "lambda",
    "mu",
    "nu",
    "xi",
    "omicron",
    "pi",
];
const VALUES: [i64; 16] = [11, 23, 37, 41, 53, 67, 79, 83, 97, 101, 113, 127, 131, 149, 157, 173];

fn main() {
    let mut metrics = HashMap::<String, i64>::new();
    let mut index = 0_usize;
    while index < KEYS.len() {
        metrics.insert(KEYS[index].to_string(), VALUES[index]);
        index += 1;
    }

    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        let slot = (i & 15) as usize;
        let value = *metrics.get(KEYS[slot]).unwrap();
        acc = (acc + (value * ((i % 5) + 1)) + ((slot as i64) * 3)) % MODULUS;
        i += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
