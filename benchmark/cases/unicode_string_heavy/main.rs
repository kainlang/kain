const TEXT_A: &str = "orbit-世界-кисть-مرحبا-🙂-flux";
const NEEDLE_A1: &str = "世界";
const NEEDLE_A2: &str = "🙂";
const TEXT_B: &str = "lattice-猫-данные-سلام-🚀-field";
const NEEDLE_B1: &str = "данные";
const NEEDLE_B2: &str = "🚀";
const ITERATIONS: i64 = 150_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 15_524_994;

fn starts_with_at(text: &[u8], index: usize, needle: &[u8]) -> bool {
    if index + needle.len() > text.len() {
        return false;
    }
    let mut offset = 0_usize;
    while offset < needle.len() {
        if text[index + offset] != needle[offset] {
            return false;
        }
        offset += 1;
    }
    true
}

fn find_substring(text: &[u8], needle: &[u8], start: usize) -> i64 {
    if needle.is_empty() {
        return start as i64;
    }
    let mut index = start;
    while index + needle.len() <= text.len() {
        if starts_with_at(text, index, needle) {
            return index as i64;
        }
        index += 1;
    }
    -1
}

fn score_text(text: &[u8], needle_a: &[u8], needle_b: &[u8]) -> i64 {
    text.len() as i64
        + find_substring(text, needle_a, 0)
        + find_substring(text, needle_b, 0)
        + needle_a.len() as i64
        + needle_b.len() as i64
}

fn main() {
    let score_a = score_text(TEXT_A.as_bytes(), NEEDLE_A1.as_bytes(), NEEDLE_A2.as_bytes());
    let score_b = score_text(TEXT_B.as_bytes(), NEEDLE_B1.as_bytes(), NEEDLE_B2.as_bytes());
    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        let score = if index & 1 == 0 { score_a } else { score_b };
        acc = (acc + score + (index % 7)) % MODULUS;
        index += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
