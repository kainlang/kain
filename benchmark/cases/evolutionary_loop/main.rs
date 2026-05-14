const ITERATIONS: i64 = 2_000_000;
const EXPECTED: i64 = 403_591_996;
const MODULUS: i64 = 1_000_000_007;

#[inline(never)]
fn scalar_lane(value: i64) -> i64 {
    ((value * 31) + 7) % MODULUS
}

#[inline(never)]
fn wide_lane(value: i64) -> i64 {
    ((value * 31) + 7) % MODULUS
}

#[inline(never)]
fn choose(value: i64) -> i64 {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    {
        if std::is_x86_feature_detected!("avx2") {
            return wide_lane(value);
        }
    }
    scalar_lane(value)
}

#[inline(never)]
fn mix(value: i64) -> i64 {
    ((value * 17) + 11) % MODULUS
}

#[inline(never)]
fn pipeline(value: i64) -> i64 {
    mix(choose(value))
}

fn main() {
    let mut acc = 1_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        acc = pipeline(acc + i);
        i += 1;
    }
    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
