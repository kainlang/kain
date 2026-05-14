const ITERATIONS: i64 = 750_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 758_650_175;

#[inline(never)]
fn run() -> i64 {
    let mut cell = Box::new(0_i64);
    let mut i = 0_i64;
    while i < ITERATIONS {
        let current = *cell;
        *cell = ((current * 33) + i + 7) % MODULUS;
        i += 1;
    }
    let result = unsafe { std::ptr::read_volatile(&*cell) };
    drop(cell);
    result
}

fn main() {
    if run() != EXPECTED {
        std::process::exit(1);
    }
}
