pub const WORKFLOW_CITY_REVISION: i64 = 1;

const GROUP_LABELS: [&str; 6] = ["MODEL", "UV", "SURFACE", "ANIM", "RENDER", "SIM"];
const GROUP_COLORS: [&str; 6] = [
    "#4da3ff",
    "#3dd9c4",
    "#ff6f91",
    "#b388ff",
    "#74f299",
    "#38d8ff",
];
const GROUP_ANCHORS: [(f64, f64); 6] = [
    (-9.5, -5.6),
    (0.0, -7.8),
    (9.6, -5.0),
    (-7.4, 5.2),
    (0.0, 8.2),
    (8.8, 5.8),
];
const GROUP_SPANS: [f64; 6] = [7.4, 3.8, 7.8, 5.4, 3.6, 7.2];
const MODULES: [(&str, usize, f64, f64); 13] = [
    ("K-SCULPT", 0, -2.2, -0.2),
    ("K-GREEBLE", 0, 0.0, 0.3),
    ("K-SCATTER", 0, 2.2, -0.1),
    ("K-ATLAS", 1, 0.0, 0.0),
    ("K-GRAPHOS", 2, -2.4, -0.2),
    ("K-SAMPLE", 2, 0.0, 0.4),
    ("K-PAINTER", 2, 2.4, -0.1),
    ("K-RIG", 3, -1.3, 0.1),
    ("K-CLONER", 3, 1.3, -0.2),
    ("K-INSPECT", 4, 0.0, 0.0),
    ("K-TECTON", 5, -2.2, -0.1),
    ("K-CHRONOS", 5, 0.0, 0.3),
    ("K-QUANTUM", 5, 2.2, -0.2),
];

fn module_height_at(index: usize) -> f64 {
    let (name, group_index, local_x, local_z) = MODULES[index];
    let base = 1.7 + (group_index as f64) * 0.22;
    let glyph = (name.len() % 5) as f64 * 0.35;
    let local = local_x.abs() * 0.16 + local_z.abs() * 0.2;
    let sim_bonus = if group_index == 5 { 0.9 } else { 0.0 };
    base + glyph + local + sim_bonus
}

fn module_energy_at(index: usize) -> f64 {
    let (_, group_index, local_x, local_z) = MODULES[index];
    let base = 0.68 + (index as f64) * 0.09;
    let category = (group_index as f64) * 0.11;
    base + category + local_x.abs() * 0.04 + local_z.abs() * 0.06
}

pub fn workflow_group_labels() -> Vec<String> {
    GROUP_LABELS.iter().map(|value| (*value).to_string()).collect()
}

pub fn workflow_group_colors() -> Vec<String> {
    GROUP_COLORS.iter().map(|value| (*value).to_string()).collect()
}

pub fn workflow_group_anchor_xs() -> Vec<f64> {
    GROUP_ANCHORS.iter().map(|value| value.0).collect()
}

pub fn workflow_group_anchor_zs() -> Vec<f64> {
    GROUP_ANCHORS.iter().map(|value| value.1).collect()
}

pub fn workflow_group_spans() -> Vec<f64> {
    GROUP_SPANS.to_vec()
}

pub fn workflow_group_module_counts() -> Vec<i64> {
    let mut counts = vec![0i64; GROUP_LABELS.len()];
    for (_, group_index, _, _) in MODULES {
        counts[group_index] += 1;
    }
    counts
}

pub fn workflow_module_names() -> Vec<String> {
    MODULES.iter().map(|value| value.0.to_string()).collect()
}

pub fn workflow_module_group_indices() -> Vec<i64> {
    MODULES.iter().map(|value| value.1 as i64).collect()
}

pub fn workflow_module_xs() -> Vec<f64> {
    MODULES
        .iter()
        .map(|value| GROUP_ANCHORS[value.1].0 + value.2)
        .collect()
}

pub fn workflow_module_zs() -> Vec<f64> {
    MODULES
        .iter()
        .map(|value| GROUP_ANCHORS[value.1].1 + value.3)
        .collect()
}

pub fn workflow_module_heights() -> Vec<f64> {
    (0..MODULES.len()).map(module_height_at).collect()
}

pub fn workflow_module_energies() -> Vec<f64> {
    (0..MODULES.len()).map(module_energy_at).collect()
}

pub fn workflow_total_energy() -> f64 {
    (0..MODULES.len()).map(module_energy_at).sum()
}

pub fn workflow_city_signature(seed: i64) -> String {
    let energy = (workflow_total_energy() * 100.0) as i64;
    let height = (workflow_module_heights().into_iter().sum::<f64>() * 100.0) as i64;
    format!(
        "workflow-city:r{}:g{}:m{}:s{}:e{}:h{}",
        WORKFLOW_CITY_REVISION,
        GROUP_LABELS.len(),
        MODULES.len(),
        seed,
        energy + seed * 17,
        height
    )
}
