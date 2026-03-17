pub const CARGO_NODE_REVISION: i64 = 5;

pub fn signal_bands(count: i64, phase: i64) -> Vec<i64> {
    (0..count)
        .map(|index| 24 + ((index * 17 + phase * 3) % 70))
        .collect()
}

pub fn beacon_pairs(width: i64, height: i64, phase: i64) -> Vec<i64> {
    let inner_width = (width - 120).max(1);
    let inner_height = (height - 160).max(1);
    let mut points = Vec::new();
    for index in 0..14_i64 {
        let x = 60 + ((index * 73 + phase * 5) % inner_width);
        let y = 80 + ((index * 41 + phase * 7) % inner_height);
        points.push(x);
        points.push(y);
    }
    points
}

pub fn bar_energy(values: Vec<i64>) -> i64 {
    values.into_iter().sum()
}

pub fn workbench_signature(label: &str, phase: i64) -> String {
    format!("{label}:{phase}:cargo-node")
}

pub struct WorkbenchStamp;

impl WorkbenchStamp {
    pub fn orbit(seed: i64, stride: i64) -> i64 {
        seed * stride + 11
    }
}
