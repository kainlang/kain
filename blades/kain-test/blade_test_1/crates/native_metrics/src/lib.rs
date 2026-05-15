pub fn weighted_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let weight = ((index as i64) % 29) + 5;
        total = (total + value * weight + index as i64) % 9_999_991;
    }
    total
}

pub fn build_metric_label(byte_length: i64, checksum: i64, upstream_signature: String) -> String {
    format!(
        "native-metrics:{}:checksum={}:{}",
        byte_length, checksum, upstream_signature
    )
}
