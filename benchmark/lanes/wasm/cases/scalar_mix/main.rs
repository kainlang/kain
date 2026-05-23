#![no_std]
#![no_main]

use core::panic::PanicInfo;

const WASM_SCALAR_ITERATIONS: i64 = 240_000;
const WASM_SCALAR_MODULUS: i64 = 1_000_000_007;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

fn scalar_mix_step(value: i64) -> i64 {
    value
        .wrapping_mul(1_664_525)
        .wrapping_add(1_013_904_223)
        % WASM_SCALAR_MODULUS
}

fn scalar_mix_checksum(iterations: i64, modulus: i64) -> i64 {
    let mut acc = 17_i64;
    let mut i = 0_i64;
    while i < iterations {
        acc = (scalar_mix_step(acc + i) + (i % 97)) % modulus;
        i += 1;
    }
    acc
}

#[no_mangle]
pub extern "C" fn main() -> i64 {
    scalar_mix_checksum(WASM_SCALAR_ITERATIONS, WASM_SCALAR_MODULUS)
}
