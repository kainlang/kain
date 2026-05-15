const ITERATIONS: i64 = 2_000_000;
const ADDEND: i64 = 17;
const OFFSET: i64 = ADDEND + 5;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 42_986_000;

fn main() {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        acc = (acc + i + OFFSET) % MODULUS;
        i += 1;
    }
    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
