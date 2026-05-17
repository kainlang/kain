const ROUNDS: i64 = 220_000;
const MASK: i64 = 2_147_483_647;
const EXPECTED: i64 = 1_528_465_470;
const KEYS: [i64; 8] = [1_267_611, 2_386_093, 1_059_128, 5_596_791, 9_022_413, 3_227_993, 2_562_088, 4_342_338];

fn rotl31(value: i64, shift: u32) -> i64 {
    (((value << shift) & MASK) | (value >> (31 - shift))) & MASK
}

fn main() {
    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ROUNDS {
        let mut left = ((index * 1_103_515) + 12_345) & MASK;
        let mut right = ((index * 2_654_435) + 54_321) & MASK;
        for round_key in KEYS {
            let mixed = (rotl31((left + round_key + 13) & MASK, 5) ^ right) & MASK;
            let next_right = (mixed + ((right & 255) * 17) + round_key) & MASK;
            left = right;
            right = next_right;
        }
        acc = (acc + left + right + (left ^ right)) & MASK;
        index += 1;
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
