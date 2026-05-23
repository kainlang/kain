#![no_std]
#![no_main]

use core::panic::PanicInfo;

const WASM_BITWISE_ITERATIONS: i64 = 180_000;
const WASM_BITWISE_MODULUS: i64 = 1_000_000_007;
const WASM_U32_MASK: i64 = 4_294_967_295;
const WASM_AVALANCHE_A: i64 = 2_246_822_519;
const WASM_AVALANCHE_B: i64 = 3_266_489_917;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

fn wasm_rotl32(value: i64, bits: i64) -> i64 {
    let masked = value & WASM_U32_MASK;
    let left = masked.wrapping_shl(bits as u32) & WASM_U32_MASK;
    let right = masked >> (32 - bits);
    (left | right) & WASM_U32_MASK
}

fn wasm_avalanche32(value: i64) -> i64 {
    let mut x = value & WASM_U32_MASK;
    x = (x ^ (x >> 16)) & WASM_U32_MASK;
    x = x.wrapping_mul(WASM_AVALANCHE_A) & WASM_U32_MASK;
    x = (x ^ (x >> 13)) & WASM_U32_MASK;
    x = x.wrapping_mul(WASM_AVALANCHE_B) & WASM_U32_MASK;
    (x ^ (x >> 16)) & WASM_U32_MASK
}

fn bitwise_pack_checksum(iterations: i64, modulus: i64) -> i64 {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < iterations {
        let header = ((i & 1_048_575) << 12)
            | (((i * 3) & 15) << 8)
            | ((i & 15) << 4)
            | 1;
        let mixed = wasm_avalanche32(header + acc + 374_761_393);
        let rotated = wasm_rotl32(mixed, (i % 23) + 1);
        acc = (acc + rotated + (mixed & 4095)) & WASM_U32_MASK;
        i += 1;
    }
    acc % modulus
}

#[no_mangle]
pub extern "C" fn main() -> i64 {
    bitwise_pack_checksum(WASM_BITWISE_ITERATIONS, WASM_BITWISE_MODULUS)
}
