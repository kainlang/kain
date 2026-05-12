pub const CARGO_SMOKE_VERSION: i64 = 7;

pub struct PulseProfile {
    pub seed: i64,
    pub label: String,
}

pub enum PulseMode {
    Calm,
    Charged,
}

pub trait UnsupportedPulseSurface {
    fn surface_name(&self) -> String;
}

pub async fn warm_async_path() -> i64 {
    1
}

pub fn generic_echo<T: Clone>(value: T) -> T {
    value
}

pub fn add(lhs: i64, rhs: i64) -> i64 {
    lhs + rhs
}

pub fn amplify_series(values: Vec<i64>, gain: i64) -> Vec<i64> {
    values.into_iter().map(|value| value * gain).collect()
}

pub fn sum_series(values: Vec<i64>) -> i64 {
    values.into_iter().sum()
}

pub fn every_above(values: Vec<i64>, floor: i64) -> bool {
    values.into_iter().all(|value| value > floor)
}

pub fn compose_badge(label: &str, index: i64) -> String {
    format!("{label}-{index}:crate-ffi")
}

pub fn wave_row(width: i64, phase: i64) -> String {
    let width = width.max(1) as usize;
    let mut row = String::with_capacity(width);
    for x in 0..width {
        let sample = ((x as i64 * 5) + phase * 7 + (x as i64 / 3)) % 19;
        let glyph = match sample {
            0..=2 => ' ',
            3..=5 => '.',
            6..=8 => ':',
            9..=11 => '*',
            12..=14 => 'o',
            15..=16 => 'O',
            _ => '@',
        };
        row.push(glyph);
    }
    row
}

impl PulseProfile {
    pub fn stamp(seed: i64, lanes: i64) -> i64 {
        seed * lanes
    }
}
