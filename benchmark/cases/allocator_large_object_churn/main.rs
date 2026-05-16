const ITERATIONS: i64 = 2_500;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 41_587_426;
const SIZES: [usize; 6] = [512, 1_024, 2_048, 4_096, 8_192, 16_384];

fn main() {
    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        let cells = SIZES[(index as usize) % SIZES.len()];
        let mut buffer = vec![0_i64; cells];
        buffer[0] = index + 1;
        buffer[cells / 2] = (index * 3) + 7;
        buffer[cells - 1] = (index * 5) + 11;
        let observed = buffer[0] + buffer[cells / 2] + buffer[cells - 1];
        acc = (acc + observed + cells as i64) % MODULUS;
        index += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
