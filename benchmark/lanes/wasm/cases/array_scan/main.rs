#![no_std]
#![no_main]

use core::panic::PanicInfo;

const WASM_ARRAY_ITERATIONS: i64 = 90_000;
const WASM_ARRAY_MODULUS: i64 = 1_000_000_007;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

fn array_scan_checksum(iterations: i64, modulus: i64) -> i64 {
    let values: [i64; 8] = [3, 1, 4, 1, 5, 9, 2, 6];
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < iterations {
        let mut inner = 0_i64;
        let mut index = 0_usize;
        while index < values.len() {
            inner = (inner + values[index] * ((index as i64) + 1)) % modulus;
            index += 1;
        }
        acc = (acc + inner + (i % 31)) % modulus;
        i += 1;
    }
    acc
}

#[no_mangle]
pub extern "C" fn main() -> i64 {
    array_scan_checksum(WASM_ARRAY_ITERATIONS, WASM_ARRAY_MODULUS)
}
