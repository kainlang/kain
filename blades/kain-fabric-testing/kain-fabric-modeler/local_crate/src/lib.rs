pub fn mesh_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let weight = ((index as i64) % 23) + 5;
        total = (total + value * weight + (index as i64 * 3)) % 1_000_003;
    }
    total
}

pub fn topology_report(
    byte_length: i64,
    checksum: i64,
    upstream_report: String,
    native_signature: String,
) -> String {
    format!(
        "topology-report:{}:checksum={}:{}:{}",
        byte_length, checksum, upstream_report, native_signature
    )
}
