const STRING_TEXT: &str = "ka0in0be0nch";
const STRING_NEEDLE: &str = "in";
const STRING_TAIL: &str = "ch";

#[inline(never)]
fn starts_with_at(text: &str, index: usize, needle: &str) -> bool {
    if index + needle.len() > text.len() {
        return false;
    }
    let text = text.as_bytes();
    let needle = needle.as_bytes();
    let mut offset = 0usize;
    while offset < needle.len() {
        if text[index + offset] != needle[offset] {
            return false;
        }
        offset += 1;
    }
    true
}

#[inline(never)]
fn find_substring(text: &str, needle: &str, start: usize) -> usize {
    let needle_len = needle.len();
    if needle_len == 0 {
        return start;
    }
    let mut index = start;
    while index + needle_len <= text.len() {
        if starts_with_at(text, index, needle) {
            return index;
        }
        index += 1;
    }
    text.len()
}

fn main() {
    const ITERATIONS: i64 = 100_000;
    const EXPECTED: i64 = 2_050_000;
    let mut acc = 0i64;
    let mut i = 0i64;
    let mut use_needle = true;

    while i < ITERATIONS {
        if use_needle {
            acc = acc
                + STRING_TEXT.len() as i64
                + find_substring(STRING_TEXT, STRING_NEEDLE, 0) as i64
                + STRING_NEEDLE.len() as i64;
        } else {
            acc = acc
                + STRING_TEXT.len() as i64
                + find_substring(STRING_TEXT, STRING_TAIL, 0) as i64
                + STRING_TAIL.len() as i64;
        }
        use_needle = !use_needle;
        i += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
