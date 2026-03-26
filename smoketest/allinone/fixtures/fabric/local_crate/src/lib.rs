pub fn buffer_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let weight = ((index as i64) % 19) + 3;
        total = (total + value * weight + index as i64) % 1_000_003;
    }
    total
}

pub fn analysis_label(
    snapshot_byte_length: i64,
    snapshot_checksum: i64,
    gpu_checksum: i64,
    upstream_report: String
) -> String {
    format!(
        "rust-analysis:{}:snapshot={}:gpu={}:{}",
        snapshot_byte_length,
        snapshot_checksum,
        gpu_checksum,
        upstream_report
    )
}
