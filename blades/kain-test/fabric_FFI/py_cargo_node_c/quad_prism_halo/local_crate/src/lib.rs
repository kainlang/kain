pub const QUAD_PRISM_REVISION: i64 = 1;

pub fn quad_prism_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let index = index as i64;
        let weight = (index % 31) + 7;
        total = (total + value * weight + (index % 97)) % 10_000_019;
    }
    total
}

pub fn quad_prism_bands(width: i64, count: i64) -> Vec<i64> {
    let count = count.max(1);
    let mut values = Vec::new();
    for index in 0..count {
        values.push(((index + 1) * width) / (count + 1));
    }
    values
}

pub fn quad_prism_signature(
    label: String,
    width: i64,
    height: i64,
    rust_checksum: i64,
    c_checksum: i64,
) -> String {
    format!("{label}:{width}x{height}:r{rust_checksum}:c{c_checksum}")
}

pub fn quad_prism_phase_stamp(width: i64, height: i64, phase: i64) -> String {
    format!("phase:{width}x{height}:{phase}")
}
