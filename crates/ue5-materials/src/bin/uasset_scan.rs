use std::collections::{BTreeMap, BTreeSet};
use std::env;
use std::fs;
use std::fs::File;
use std::path::{Path, PathBuf};

use unreal_asset::base::engine_version::EngineVersion;
use unreal_asset::base::reader::ArchiveTrait;
use unreal_asset::exports::{ExportBaseTrait, ExportNormalTrait};
use unreal_asset::Asset;
use unreal_asset_properties::Property;

#[derive(Default)]
struct ScanReport {
    files_scanned: usize,
    parse_failures: usize,
    material_exports: usize,

    enum_values: BTreeMap<String, BTreeSet<String>>,
    expression_extras: BTreeMap<String, BTreeSet<usize>>,
    parse_error_samples: Vec<String>,
}

fn collect_uasset_files(root: &Path, out: &mut Vec<PathBuf>) {
    let entries = match fs::read_dir(root) {
        Ok(e) => e,
        Err(_) => return,
    };

    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_uasset_files(&path, out);
        } else if path
            .extension()
            .and_then(|e| e.to_str())
            .map(|e| e.eq_ignore_ascii_case("uasset"))
            .unwrap_or(false)
        {
            out.push(path);
        }
    }
}

fn add_enum(report: &mut ScanReport, key: &str, value: &str) {
    report
        .enum_values
        .entry(key.to_string())
        .or_default()
        .insert(value.to_string());
}

fn engine_versions_to_try() -> &'static [EngineVersion] {
    &[
        EngineVersion::VER_UE5_7,
        EngineVersion::VER_UE5_6,
        EngineVersion::VER_UE5_5,
        EngineVersion::VER_UE5_4,
        EngineVersion::VER_UE5_2,
    ]
}

fn try_parse_asset(path: &Path) -> Result<Asset<File>, String> {
    let mut errors = Vec::new();
    let uexp_path = path.with_extension("uexp");
    let has_uexp = uexp_path.exists();

    for &engine_version in engine_versions_to_try() {
        // Try with split export file first when present.
        if has_uexp {
            let file = File::open(path).map_err(|e| format!("open uasset failed: {}", e))?;
            let uexp_file =
                File::open(&uexp_path).map_err(|e| format!("open uexp failed: {}", e))?;
            match Asset::new(file, Some(uexp_file), engine_version, None) {
                Ok(asset) => return Ok(asset),
                Err(e) => errors.push(format!("{:?} with .uexp: {}", engine_version, e)),
            }
        }

        // Try without .uexp as fallback.
        let file = File::open(path).map_err(|e| format!("open uasset failed: {}", e))?;
        match Asset::new(file, None, engine_version, None) {
            Ok(asset) => return Ok(asset),
            Err(e) => errors.push(format!("{:?} no .uexp: {}", engine_version, e)),
        }
    }

    Err(errors.join(" | "))
}

fn scan_file(path: &Path, report: &mut ScanReport) {
    report.files_scanned += 1;

    let asset = match try_parse_asset(path) {
        Ok(a) => a,
        Err(e) => {
            report.parse_failures += 1;
            if report.parse_error_samples.len() < 8 {
                report
                    .parse_error_samples
                    .push(format!("{} => {}", path.display(), e));
            }
            return;
        }
    };

    for export in &asset.asset_data.exports {
        let base = export.get_base_export();
        let class_name = asset
            .get_object_name(base.class_index)
            .map(|f| f.get_owned_content())
            .unwrap_or_default();

        if class_name.contains("Material") {
            report.material_exports += 1;
        }

        if let Some(normal) = export.get_normal_export() {
            if class_name.starts_with("MaterialExpression") {
                report
                    .expression_extras
                    .entry(class_name.clone())
                    .or_default()
                    .insert(normal.extras.len());
            }

            for prop in &normal.properties {
                if let Property::EnumProperty(ep) = prop {
                    let prop_name = ep.name.get_owned_content();
                    let enum_type = ep
                        .enum_type
                        .as_ref()
                        .map(|e| e.get_owned_content())
                        .unwrap_or_else(|| "<none>".to_string());
                    let enum_value = ep
                        .value
                        .as_ref()
                        .map(|e| e.get_owned_content())
                        .unwrap_or_else(|| "<none>".to_string());

                    let key = format!("{}::{}", class_name, prop_name);
                    add_enum(
                        report,
                        &format!("{} (type={})", key, enum_type),
                        &enum_value,
                    );
                }
            }
        }
    }
}

fn print_report(title: &str, report: &ScanReport) {
    println!("\n=== {} ===", title);
    println!("files_scanned     : {}", report.files_scanned);
    println!("parse_failures    : {}", report.parse_failures);
    println!("material_exports  : {}", report.material_exports);

    if !report.parse_error_samples.is_empty() {
        println!("\n-- Parse error samples --");
        for sample in &report.parse_error_samples {
            println!("{}", sample);
        }
    }

    println!("\n-- Enum values --");
    if report.enum_values.is_empty() {
        println!("(none found)");
    } else {
        for (k, vals) in &report.enum_values {
            let joined = vals.iter().cloned().collect::<Vec<_>>().join(", ");
            println!("{} => [{}]", k, joined);
        }
    }

    println!("\n-- MaterialExpression extras lengths --");
    if report.expression_extras.is_empty() {
        println!("(none found)");
    } else {
        for (k, vals) in &report.expression_extras {
            let joined = vals
                .iter()
                .map(|v| v.to_string())
                .collect::<Vec<_>>()
                .join(", ");
            println!("{} => [{}]", k, joined);
        }
    }
}

fn main() {
    let args = env::args().skip(1).collect::<Vec<_>>();
    if args.is_empty() {
        eprintln!(
            "Usage: cargo run -p ue5-materials --bin uasset_scan -- <ROOT_DIR> [ROOT_DIR ...]"
        );
        std::process::exit(2);
    }

    for root in args {
        let root_path = PathBuf::from(&root);
        let mut files = Vec::new();
        collect_uasset_files(&root_path, &mut files);

        let mut report = ScanReport::default();
        for file in files {
            scan_file(&file, &mut report);
        }

        print_report(&root, &report);
    }
}
