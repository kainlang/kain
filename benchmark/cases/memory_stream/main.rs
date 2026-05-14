const CELLS: usize = 262_144;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 149_653_729;

fn main() {
    let mut buffer = vec![0_i64; CELLS];
    let mut i = 0_usize;
    while i < CELLS {
        buffer[i] = (((i as i64) * 31) + 7) % MODULUS;
        i += 1;
    }

    let mut checksum = 0_i64;
    let mut j = 0_usize;
    while j < CELLS {
        checksum = (checksum + unsafe { std::ptr::read_volatile(&buffer[j]) }) % MODULUS;
        j += 1;
    }

    if checksum != EXPECTED {
        std::process::exit(1);
    }
}
