use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

use kain_amalgamate::{
    default_materialize_root, inspect_capsule, materialize_capsule, maybe_capsule_metadata,
    pack_capsule, unpack_capsule, CapsuleCompression, CapsuleHeaderStyle, CapsuleIndexMode,
    CapsuleStorage, InspectReport, MaterializedCapsule, PackOptions,
};
use kain_commands::kain::AmalgamateCommand;

pub fn run(
    command: Option<AmalgamateCommand>,
    input: Option<PathBuf>,
    output: Option<PathBuf>,
    name: Option<String>,
    version: Option<String>,
    authors: Vec<String>,
    notes: Vec<String>,
    tags: Vec<String>,
    meta: Vec<String>,
    archive: bool,
    header: String,
    preview_symbols: usize,
    compression: String,
    api_index: String,
    module_index: String,
) -> Result<(), String> {
    match command {
        Some(AmalgamateCommand::Inspect { input, json }) => inspect(&input, json),
        Some(AmalgamateCommand::Unpack { input, output }) => unpack(&input, output.as_deref()),
        None => {
            let input = input.ok_or_else(|| {
                "amalgamate requires an input path when no subcommand is selected".to_string()
            })?;
            let output = output.ok_or_else(|| {
                "amalgamate requires -o/--output when packing a capsule".to_string()
            })?;
            let mut options = PackOptions::new(input, output);
            options.name = name;
            options.version = version;
            options.authors = authors;
            options.notes = notes;
            options.tags = tags;
            options.meta = parse_meta_items(meta)?;
            options.storage = if archive {
                CapsuleStorage::Archive
            } else {
                CapsuleStorage::Editable
            };
            options.header_style = parse_header_style(&header)?;
            options.preview_symbol_limit = preview_symbols;
            options.compression = parse_compression(&compression)?;
            options.api_index = parse_index_mode(&api_index)?;
            options.module_index = parse_index_mode(&module_index)?;
            let report = pack_capsule(&options).map_err(|err| err.to_string())?;
            println!(" Packed capsule: {}", report.output_path.display());
            println!("  kind: {}", report.kind);
            println!("  name: {}", report.name);
            println!("  digest: {}", report.digest);
            println!(
                "  structure: {} files | {} modules",
                report.file_count, report.module_count
            );
            Ok(())
        }
    }
}

pub fn maybe_materialize_input(path: &Path) -> Result<Option<MaterializedCapsule>, String> {
    let Some(_) = maybe_capsule_metadata(path).map_err(|err| err.to_string())? else {
        return Ok(None);
    };
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let cache_root = default_materialize_root(&cwd);
    materialize_capsule(path, &cache_root)
        .map(Some)
        .map_err(|err| err.to_string())
}

fn inspect(path: &Path, json: bool) -> Result<(), String> {
    let report = inspect_capsule(path).map_err(|err| err.to_string())?;
    if json {
        let text = serde_json::to_string_pretty(&report)
            .map_err(|err| format!("failed to encode capsule inspect JSON: {err}"))?;
        println!("{text}");
        return Ok(());
    }
    print_inspect_report(&report);
    Ok(())
}

fn unpack(path: &Path, output: Option<&Path>) -> Result<(), String> {
    let destination = output
        .map(Path::to_path_buf)
        .unwrap_or_else(|| default_unpack_directory(path));
    let report = unpack_capsule(path, &destination).map_err(|err| err.to_string())?;
    println!(" Unpacked capsule to {}", report.output_root.display());
    println!("  files: {}", report.file_count);
    Ok(())
}

fn print_inspect_report(report: &InspectReport) {
    println!("KAIN capsule");
    if let Some(name) = report.metadata.name.as_deref() {
        println!("  name: {}", name);
    }
    if let Some(version) = report.metadata.version.as_deref() {
        println!("  version: {}", version);
    }
    println!("  kind: {}", report.metadata.display_kind());
    println!("  digest: {}", report.metadata.digest);
    println!("  storage: {}", report.metadata.storage);
    if report.metadata.storage == CapsuleStorage::Archive {
        println!("  compression: {}", report.metadata.compression);
    }
    if let Some(entry) = report.metadata.entry.as_deref() {
        println!("  entry: {}", entry);
    }
    if let Some(manifest) = report.metadata.manifest.as_deref() {
        println!("  manifest: {}", manifest);
    }
    println!(
        "  structure: {} files | {} modules",
        report.metadata.file_count, report.metadata.module_count
    );
    if !report.metadata.authors.is_empty() {
        println!("  authors: {}", report.metadata.authors.join(", "));
    }
    if !report.metadata.tags.is_empty() {
        println!("  tags: {}", report.metadata.tags.join(", "));
    }
    for note in &report.metadata.notes {
        println!("  note: {}", note);
    }
    for (key, value) in &report.metadata.meta {
        println!("  meta.{key}: {value}");
    }
    if !report.preview.sections.is_empty() {
        println!();
        println!("PUBLIC INTERFACE DIRECTORY");
        for section in &report.preview.sections {
            println!("[{}]", section.title);
            for row in symbol_rows(&section.symbols) {
                println!("  {}", row);
            }
        }
    }
    println!();
    println!("FILES");
    for file in &report.files {
        println!("  {} ({} bytes)", file.path, file.size_bytes);
    }
}

fn symbol_rows(symbols: &[String]) -> Vec<String> {
    let mut rows = Vec::new();
    let mut current = String::new();
    for symbol in symbols {
        let candidate = if current.is_empty() {
            symbol.clone()
        } else {
            format!("{current}, {symbol}")
        };
        if !current.is_empty() && candidate.len() > 92 {
            rows.push(current);
            current = symbol.clone();
        } else {
            current = candidate;
        }
    }
    if !current.is_empty() {
        rows.push(current);
    }
    rows
}

fn default_unpack_directory(path: &Path) -> PathBuf {
    let stem = path
        .file_stem()
        .map(|value| value.to_string_lossy().into_owned())
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| "capsule".to_string());
    path.with_file_name(format!("{stem}.unpacked"))
}

fn parse_meta_items(items: Vec<String>) -> Result<BTreeMap<String, String>, String> {
    let mut meta = BTreeMap::new();
    for item in items {
        let Some((key, value)) = item.split_once('=') else {
            return Err(format!(
                "invalid --meta value '{}'; expected key=value",
                item
            ));
        };
        let key = key.trim();
        let value = value.trim();
        if key.is_empty() {
            return Err("meta keys cannot be empty".to_string());
        }
        meta.insert(key.to_string(), value.to_string());
    }
    Ok(meta)
}

fn parse_header_style(value: &str) -> Result<CapsuleHeaderStyle, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "minimal" => Ok(CapsuleHeaderStyle::Minimal),
        "rich" => Ok(CapsuleHeaderStyle::Rich),
        "off" => Ok(CapsuleHeaderStyle::Off),
        other => Err(format!(
            "unknown capsule header style '{}'; expected minimal, rich, or off",
            other
        )),
    }
}

fn parse_compression(value: &str) -> Result<CapsuleCompression, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "zstd" => Ok(CapsuleCompression::Zstd),
        "none" => Ok(CapsuleCompression::None),
        other => Err(format!(
            "unknown capsule compression '{}'; expected zstd or none",
            other
        )),
    }
}

fn parse_index_mode(value: &str) -> Result<CapsuleIndexMode, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Ok(CapsuleIndexMode::Auto),
        "off" => Ok(CapsuleIndexMode::Off),
        other => Err(format!(
            "unknown capsule index mode '{}'; expected auto or off",
            other
        )),
    }
}
