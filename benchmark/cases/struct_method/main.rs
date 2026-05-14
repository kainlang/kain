const ITERATIONS: i64 = 1_000_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 393_996_945;

struct BenchPair {
    x: i64,
    y: i64,
}

#[inline(never)]
fn make_pair(seed: i64) -> BenchPair {
    BenchPair {
        x: seed % 97,
        y: (seed * 7) % 101,
    }
}

#[inline(never)]
fn score_pair(pair: BenchPair) -> i64 {
    (pair.x * 3) + (pair.y * 5)
}

fn main() {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        let pair = make_pair(i);
        acc = (acc + score_pair(pair)) % MODULUS;
        i += 1;
    }
    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
