const ITERATIONS: i64 = 300_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 143_207_783;

#[inline(never)]
fn maybe_value(value: i64) -> Option<i64> {
    if value % 5 == 0 {
        None
    } else {
        Some(value + 3)
    }
}

#[inline(never)]
fn parse_value(value: i64) -> Result<i64, &'static str> {
    if value % 7 == 0 {
        Err("skip")
    } else {
        Ok(value * 2)
    }
}

fn main() {
    let mut acc = 0_i64;
    let mut i = 0_i64;
    while i < ITERATIONS {
        let maybe_component = maybe_value(i).unwrap_or(1);
        let parsed = parse_value(i);
        let parsed_component = if parsed.is_err() {
            2
        } else {
            parsed.unwrap()
        };
        acc = (acc + maybe_component + parsed_component) % MODULUS;
        i += 1;
    }
    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
