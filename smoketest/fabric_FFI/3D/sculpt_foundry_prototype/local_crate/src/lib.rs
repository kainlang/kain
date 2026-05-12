pub const SCULPT_FOUNDRY_REVISION: i64 = 1;

pub fn sculpt_brush_curve(radius: i64, intensity: i64, hardness: i64, samples: i64) -> Vec<i64> {
    let samples = samples.max(2);
    let radius_scale = (radius as f64 / 96.0).clamp(0.35, 1.8);
    let intensity_scale = (intensity as f64 / 100.0).clamp(0.1, 1.5);
    let hardness_power = 1.1 + (hardness as f64 / 90.0);
    let mut values = Vec::new();
    for index in 0..samples {
        let t = index as f64 / (samples - 1) as f64;
        let falloff = (1.0 - t).max(0.0).powf(hardness_power);
        let shaped = (falloff * 100.0 * radius_scale * intensity_scale).round() as i64;
        values.push(shaped.max(0));
    }
    values
}

pub fn sculpt_mesh_forecast(base_polys: i64, dyn_topo: i64, detail_pct: i64, layers: i64) -> Vec<i64> {
    let detail = (detail_pct as f64 / 100.0).clamp(0.1, 1.8);
    let layer_factor = 1.0 + (layers.max(1) as f64 - 1.0) * 0.045;
    let dyn_factor = if dyn_topo != 0 { 1.22 + detail * 0.95 } else { 1.04 + detail * 0.35 };
    let target_polys = ((base_polys as f64) * dyn_factor * layer_factor).round() as i64;
    let memory_mb = ((target_polys as f64) * 76.0 / (1024.0 * 1024.0)).round() as i64;
    vec![base_polys, target_polys, memory_mb.max(1)]
}

pub fn sculpt_stroke_energy(radius: i64, intensity: i64, spacing: i64, accumulate: i64) -> i64 {
    let density = (radius.max(1) * intensity.max(1)) / spacing.max(1);
    let accumulate_factor = if accumulate != 0 { 14 } else { 7 };
    density + accumulate_factor
}

pub fn sculpt_status_signature(tool: String, layer: String, energy: i64, target_polys: i64) -> String {
    format!("{tool}:{layer}:e{energy}:p{target_polys}")
}
