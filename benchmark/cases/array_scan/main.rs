const VALUES: [i64; 8] = [1, 2, 3, 4, 5, 6, 7, 8];

fn main() {
    const ITERATIONS: i64 = 500_000;
    const MODULUS: i64 = 1_000_000_007;
    const EXPECTED: i64 = 103_499_994;
    let mut acc = 0i64;
    let mut i = 0i64;

    while i < ITERATIONS {
        let mut inner = 0i64;
        let mut index = 0usize;
        while index < VALUES.len() {
            inner = (inner + VALUES[index] * (index as i64 + 1)) % MODULUS;
            index += 1;
        }
        acc = (acc + inner + (i % 7)) % MODULUS;
        i += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
