#[link(name = "ffi_boundary_shared")]
unsafe extern "C" {
    fn ffi_boundary_mix(value: i64, salt: i64) -> i64;
}

const ITERATIONS: i64 = 5_000_000;
const EXPECTED: i64 = 374_126_489;

fn main() {
    let mut acc = 1_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        acc = unsafe { ffi_boundary_mix(acc + index, index) };
        index += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
