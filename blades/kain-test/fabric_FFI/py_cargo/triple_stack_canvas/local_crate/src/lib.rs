pub const HYBRID_REVISION: i64 = 11;

pub struct HybridStamp {
    pub pulses: i64,
    pub label: String,
}

pub enum HybridMode {
    Ember,
    Tidal,
}

pub trait UnsupportedHybridSurface {
    fn surface_id(&self) -> String;
}

pub async fn hybrid_async_probe() -> i64 {
    1
}

pub fn generic_passthrough<T: Clone>(value: T) -> T {
    value
}

pub fn cargo_signature(label: &str, phase: i64) -> String {
    format!("{label}:{phase}:rust-native")
}

pub fn cargo_energy(values: Vec<i64>) -> i64 {
    values.into_iter().sum()
}

pub fn cargo_mask_row(width: i64, row: i64, phase: i64) -> Vec<i64> {
    let width = width.max(1) as usize;
    let mut row_values = Vec::with_capacity(width);
    for x in 0..width {
        let xi = x as i64;
        let ripple = ((xi * 13 + row * 11 + phase * 7) % 97).abs();
        let orbit = (((xi - row).abs() * 9) + phase * 5 + xi / 3) % 89;
        let band = ((xi * row) + phase * 17 + row * 5) % 71;
        let value = 40 + ((ripple + orbit + band) % 196);
        row_values.push(value);
    }
    row_values
}

pub fn cargo_beacon_points(width: i64, height: i64, phase: i64) -> Vec<i64> {
    let mut points = Vec::new();
    let width = width.max(8);
    let height = height.max(8);
    let offsets = [0, 1, 2, 3];
    for offset in offsets {
        let x = ((phase * 7 + offset * 41) % (width - 4)) + 2;
        let y = ((phase * 11 + offset * 29) % (height - 4)) + 2;
        points.push(x);
        points.push(y);
    }
    points
}

impl HybridStamp {
    pub fn orbit(seed: i64, lanes: i64) -> i64 {
        seed * lanes + 17
    }
}
