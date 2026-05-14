const ITERATIONS: i64 = 1_500_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 61_920_954;

#[inline(never)]
fn step_a(value: i64) -> i64 {
    ((value * 3) + 1) % MODULUS
}

#[inline(never)]
fn step_b(value: i64) -> i64 {
    ((step_a(value) + 5) * 7) % MODULUS
}

#[inline(never)]
fn step_c(value: i64) -> i64 {
    (step_b(value) + step_a(value + 11) + 13) % MODULUS
}

#[inline(never)]
fn step_d(value: i64) -> i64 {
    ((step_c(value) * 3) + step_b(value + 17) + 19) % MODULUS
}

fn main() {
    let mut acc = 1_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        acc = step_d(acc + i);
        i += 1;
    }
    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
