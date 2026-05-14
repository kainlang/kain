const ITERATIONS: i64 = 3_000_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 632_706_747;

#[inline(never)]
fn classify(value: i64) -> i64 {
    let tag = value % 8;
    if tag == 0 {
        return value + 1;
    }
    if tag == 1 {
        return (value * 3) + 7;
    }
    if tag == 2 {
        return value - 5;
    }
    if tag == 3 {
        return (value * value) + 11;
    }
    if tag == 4 {
        return value + 17;
    }
    if tag == 5 {
        return (value * 5) - 13;
    }
    if tag == 6 {
        return value + 23;
    }
    value - 11
}

fn main() {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        acc = (acc + classify(i)) % MODULUS;
        i += 1;
    }
    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
