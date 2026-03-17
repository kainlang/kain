pub const NATIVE_SCULPT_RUNTIME_REVISION: i64 = 1;

pub fn native_sculpt_curve(radius: i64, intensity: i64, hardness: i64, samples: i64) -> Vec<i64> {
    let samples = samples.max(4);
    let radius_scale = (radius as f64 / 96.0).clamp(0.35, 1.85);
    let intensity_scale = (intensity as f64 / 100.0).clamp(0.1, 1.6);
    let hardness_power = 1.0 + (hardness as f64 / 84.0);
    let mut values = Vec::with_capacity(samples as usize);
    for index in 0..samples {
        let t = index as f64 / (samples - 1) as f64;
        let falloff = (1.0 - t).max(0.0).powf(hardness_power);
        let shaped = (falloff * 100.0 * radius_scale * intensity_scale).round() as i64;
        values.push(shaped.max(0));
    }
    values
}

pub fn native_sculpt_forecast(
    base_polys: i64,
    dyn_topo: i64,
    detail_pct: i64,
    layers: i64,
) -> Vec<i64> {
    let detail = (detail_pct as f64 / 100.0).clamp(0.1, 1.8);
    let layer_factor = 1.0 + (layers.max(1) as f64 - 1.0) * 0.052;
    let dyn_factor = if dyn_topo != 0 {
        1.18 + detail * 0.98
    } else {
        1.03 + detail * 0.42
    };
    let target_polys = ((base_polys as f64) * dyn_factor * layer_factor).round() as i64;
    let memory_mb = ((target_polys as f64) * 84.0 / (1024.0 * 1024.0)).round() as i64;
    vec![base_polys, target_polys, memory_mb.max(1)]
}

pub fn native_sculpt_stroke_energy(radius: i64, intensity: i64, spacing: i64, accumulate: i64) -> i64 {
    let density = (radius.max(1) * intensity.max(1)) / spacing.max(1);
    let accumulate_factor = if accumulate != 0 { 22 } else { 11 };
    density + accumulate_factor
}

pub fn native_sculpt_runtime_signature(tool: String, energy: i64, target_polys: i64) -> String {
    format!("{tool}:e{energy}:p{target_polys}")
}
