pub const TRINITY_STACK_REVISION: i64 = 9;

pub fn cargo_spokes(width: i64, count: i64, phase: i64) -> Vec<i64> {
    let span = (width - 140).max(1);
    (0..count)
        .map(|index| 70 + ((index * 61 + phase * 5) % span))
        .collect()
}

pub fn cargo_markers(width: i64, height: i64, phase: i64) -> Vec<i64> {
    let inner_width = (width - 180).max(1);
    let inner_height = (height - 200).max(1);
    let mut points = Vec::new();
    for index in 0..10_i64 {
        let x = 90 + ((index * 89 + phase * 7) % inner_width);
        let y = 100 + ((index * 47 + phase * 11) % inner_height);
        points.push(x);
        points.push(y);
    }
    points
}

pub fn trinity_signature(label: &str, phase: i64) -> String {
    format!("{label}:{phase}:py-cargo-node")
}

pub struct TrinityStamp;

impl TrinityStamp {
    pub fn orbit(seed: i64, stride: i64) -> i64 {
        seed * stride + 17
    }
}
