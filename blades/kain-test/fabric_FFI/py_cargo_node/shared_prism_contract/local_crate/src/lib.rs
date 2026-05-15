pub const SHARED_PRISM_REVISION: i64 = 4;

pub fn shared_prism_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let weight = ((index as i64) % 19) + 3;
        total = (total + value * weight) % 1_000_003;
    }
    total
}

pub fn shared_prism_bands(width: i64, count: i64) -> Vec<i64> {
    let count = count.max(1);
    let mut values = Vec::new();
    for index in 0..count {
        values.push(((index + 1) * width) / (count + 1));
    }
    values
}

pub fn shared_prism_signature(label: String, width: i64, height: i64, checksum: i64) -> String {
    format!("{label}:{width}x{height}:{checksum}")
}
