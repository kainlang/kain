const ITERATIONS: i64 = 5000;
const DEPTH: i64 = 128;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 41_280_000;

#[inline(never)]
fn recursive_sum(value: i64) -> i64 {
    if value <= 0 {
        return 0;
    }
    value + recursive_sum(value - 1)
}

fn main() {
    let mut acc = 0i64;
    let mut i = 0i64;
    while i < ITERATIONS {
        acc = (acc + recursive_sum(DEPTH)) % MODULUS;
        i += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
