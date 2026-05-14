const ITERATIONS: i64 = 50_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 250_324_993;

fn main() {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        let cell = Box::new(i + 7);
        let value = unsafe { std::ptr::read_volatile(&*cell) };
        drop(cell);
        acc = (acc + value) % MODULUS;
        i += 1;
    }
    if acc != EXPECTED {
        std::process::exit(1);
    }
}
