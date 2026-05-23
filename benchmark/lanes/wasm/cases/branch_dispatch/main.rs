#![no_std]
#![no_main]

use core::panic::PanicInfo;

const WASM_BRANCH_ITERATIONS: i64 = 320_000;
const WASM_BRANCH_MODULUS: i64 = 1_000_000_007;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

fn wasm_branch_classify(value: i64) -> i64 {
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

fn branch_dispatch_checksum(iterations: i64, modulus: i64) -> i64 {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < iterations {
        acc = (acc + wasm_branch_classify(i)) % modulus;
        i += 1;
    }
    acc
}

#[no_mangle]
pub extern "C" fn main() -> i64 {
    branch_dispatch_checksum(WASM_BRANCH_ITERATIONS, WASM_BRANCH_MODULUS)
}
