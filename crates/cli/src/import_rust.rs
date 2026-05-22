use crate::error::{KainError, KainResult};
use serde::Serialize;
use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct ImportRustBatchOptions {
    pub recursive: bool,
    pub flat: bool,
    pub include_filters: Vec<String>,
    pub exclude_filters: Vec<String>,
    pub fail_fast: bool,
    pub report_json: Option<PathBuf>,
}

impl Default for ImportRustBatchOptions {
    fn default() -> Self {
        Self {
            recursive: true,
            flat: false,
            include_filters: Vec::new(),
            exclude_filters: Vec::new(),
            fail_fast: false,
            report_json: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ImportRustCratesOptions {
    pub source_root: Option<PathBuf>,
    pub output: Option<PathBuf>,
    pub blades: bool,
    pub target: Option<String>,
    pub batch: ImportRustBatchOptions,
}

#[derive(Debug, Default)]
struct ImportRustSummary {
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<(PathBuf, String)>,
    diagnostics: Vec<(PathBuf, Vec<String>)>,
}

#[derive(Debug, Serialize)]
struct ImportRustFailureEntry {
    file: String,
    module_path: Option<String>,
    error: String,
    repair_hint: Option<String>,
}

#[derive(Debug, Serialize)]
struct ImportRustDiagnosticEntry {
    file: String,
    module_path: Option<String>,
    diagnostics: Vec<String>,
    diagnostic_classes: BTreeMap<String, usize>,
    repair_hint: Option<String>,
}

#[derive(Debug, Serialize)]
struct ImportRustFailureReport {
    input_path: String,
    output_path: Option<String>,
    target: Option<String>,
    recursive: bool,
    flat: bool,
    include_filters: Vec<String>,
    exclude_filters: Vec<String>,
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<ImportRustFailureEntry>,
    lossy_diagnostics: Vec<ImportRustDiagnosticEntry>,
    diagnostics_by_class: BTreeMap<String, usize>,
    generated_kain_path: Option<String>,
    compiled_output_path: Option<String>,
    generated_at_utc: String,
}

#[derive(Debug, Clone)]
struct ImportedRustFileProgram {
    relative_path: PathBuf,
    program: kain_core::ast::Program,
}

#[derive(Debug)]
struct ImportedRustDirectoryPrograms {
    imported_files: Vec<ImportedRustFileProgram>,
    summary: ImportRustSummary,
}

#[derive(Debug)]
struct ImportedRustCrateResult {
    crate_name: String,
    imported_files: Vec<ImportedRustFileProgram>,
}

/// Import a Rust file into KAIN AST and optionally write/compile
pub fn import_rust(input: &Path, output: Option<&Path>, target: Option<&str>) -> KainResult<()> {
    import_rust_with_batch(input, output, target, &ImportRustBatchOptions::default())
}

pub fn import_workspace_crates(
    workspace_root: &Path,
    options: &ImportRustCratesOptions,
) -> KainResult<()> {
    let source_root =
        resolve_workspace_rust_source_root(workspace_root, options.source_root.as_deref())?;
    let crate_roots = discover_workspace_crate_roots(&source_root)?;

    let discovered_crates = crate_roots.len();
    let mut imported_crates = Vec::new();
    let mut workspace_diagnostics = Vec::new();
    let mut workspace_failed_previews = Vec::new();
    let mut total_discovered_files = 0usize;
    let mut total_imported_files = 0usize;
    let mut total_skipped_files = 0usize;
    let mut total_failed_files = 0usize;

    for crate_root in crate_roots {
        let crate_name = crate_root
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("crate")
            .to_string();
        let result = import_rust_directory_programs(&crate_root, &options.batch)?;

        total_discovered_files += result.summary.discovered_files;
        total_imported_files += result.summary.imported_files;
        total_skipped_files += result.summary.skipped_files;
        total_failed_files += result.summary.failed_files.len();

        if workspace_failed_previews.len() < 5 {
            for (path, error) in &result.summary.failed_files {
                if workspace_failed_previews.len() >= 5 {
                    break;
                }
                let relative = path.strip_prefix(&crate_root).unwrap_or(path);
                workspace_failed_previews.push(format!(
                    "{}:{}: {}",
                    crate_name,
                    relative.display(),
                    error
                ));
            }
        }

        workspace_diagnostics.extend(result.summary.diagnostics.clone());

        if !result.imported_files.is_empty() {
            imported_crates.push(ImportedRustCrateResult {
                crate_name,
                imported_files: result.imported_files,
            });
        }
    }

    if imported_crates.is_empty() {
        let detail = if workspace_failed_previews.is_empty() {
            "No Rust source files matched include/exclude filters across discovered crates"
                .to_string()
        } else {
            format!(
                "All matching Rust files across discovered crates failed to import (e.g. {})",
                workspace_failed_previews.join(" | ")
            )
        };
        return Err(KainError::runtime(detail));
    }

    let imported_crate_count = imported_crates.len();
    let skipped_crate_count = discovered_crates.saturating_sub(imported_crate_count);

    if options.blades {
        let output_root = options
            .output
            .as_deref()
            .map(|path| resolve_workspace_relative_path(workspace_root, path))
            .unwrap_or_else(|| workspace_root.join("blades"));
        let mirrored_file_count = emit_imported_crates_as_blades(&output_root, &imported_crates)?;

        println!("✅ Import complete");
        println!("   Mode: mirrored blades workspace");
        println!("   Workspace root: {}", workspace_root.display());
        println!("   Source root: {}", source_root.display());
        println!("   Output root: {}", output_root.display());
        println!(
            "   Crates: discovered {}, imported {}, skipped {}",
            discovered_crates, imported_crate_count, skipped_crate_count
        );
        println!(
            "   Rust files: discovered {}, imported {}, skipped {}, failed {}",
            total_discovered_files, total_imported_files, total_skipped_files, total_failed_files
        );
        println!("   Mirrored KAIN files: {}", mirrored_file_count);
    } else {
        let output_path = options
            .output
            .as_deref()
            .map(|path| resolve_workspace_relative_path(workspace_root, path))
            .unwrap_or_else(|| source_root.with_extension("kn"));
        let program = merge_workspace_crate_programs(imported_crates, options.batch.flat);
        let kain_source = generate_kain_source(&program)?;
        write_generated_kain_file(&output_path, &kain_source, "workspace Rust import bundle")?;
        println!(
            "✓ Generated KAIN source: {} ({} bytes)",
            output_path.display(),
            kain_source.len()
        );

        let mut compiled_output_path: Option<PathBuf> = None;
        if let Some(target_str) = options.target.as_deref() {
            let compile_target = crate::parse_compile_target(target_str)
                .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;
            println!("🔨 Compiling to target: {}", target_str);
            let compiled = crate::compile(&kain_source, compile_target)
                .map_err(|e| KainError::runtime(format!("Compilation failed: {}", e)))?;
            let compiled_output =
                output_path.with_extension(crate::target_extension(compile_target));
            write_generated_kain_file(
                &compiled_output,
                &compiled,
                "workspace Rust import compiled output",
            )?;
            compiled_output_path = Some(compiled_output.clone());
            println!(
                "✓ Compiled output: {} ({} bytes)",
                compiled_output.display(),
                compiled.len()
            );
        }

        println!("✅ Import complete");
        println!("   Mode: single bundle");
        println!("   Workspace root: {}", workspace_root.display());
        println!("   Source root: {}", source_root.display());
        println!(
            "   Crates: discovered {}, imported {}, skipped {}",
            discovered_crates, imported_crate_count, skipped_crate_count
        );
        println!(
            "   Rust files: discovered {}, imported {}, skipped {}, failed {}",
            total_discovered_files, total_imported_files, total_skipped_files, total_failed_files
        );
        println!("   Functions: {}", count_functions(&program));
        println!("   Structs: {}", count_structs(&program));
        println!("   Enums: {}", count_enums(&program));
        println!("   Bundle output: {}", output_path.display());
        if let Some(compiled_output_path) = compiled_output_path {
            println!("   Compiled output: {}", compiled_output_path.display());
        }
    }

    if !workspace_failed_previews.is_empty() {
        println!("   Failed files:");
        for preview in &workspace_failed_previews {
            println!("     - {}", preview);
        }
        if total_failed_files > workspace_failed_previews.len() {
            println!(
                "     ... {} more",
                total_failed_files - workspace_failed_previews.len()
            );
        }
    }

    if !workspace_diagnostics.is_empty() {
        let total_diags: usize = workspace_diagnostics
            .iter()
            .map(|(_, diagnostics)| diagnostics.len())
            .sum();
        let class_counts = diagnostic_class_counts(&workspace_diagnostics);
        let external_mod_diags = *class_counts.get("external_mod_decl").unwrap_or(&0);
        let visible_diags = total_diags.saturating_sub(external_mod_diags);
        if visible_diags > 0 {
            println!(
                "   Lossy lowering: {} diagnostic(s) across {} file(s)",
                visible_diags,
                workspace_diagnostics.len()
            );
            if let Some((class, count)) = class_counts
                .iter()
                .find(|(class, _)| class.as_str() != "external_mod_decl")
            {
                println!(
                    "   Primary repair seam: class:{} ({} note(s))",
                    class, count
                );
            }
        }
        if external_mod_diags > 0 {
            println!(
                "   External module declarations: {} note(s)",
                external_mod_diags
            );
        }
    }

    Ok(())
}

/// Import a Rust file or directory into KAIN AST and optionally write/compile.
pub fn import_rust_with_batch(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportRustBatchOptions,
) -> KainResult<()> {
    // Directory imports always materialize a single .kn artifact by default.
    let resolved_output: Option<PathBuf> = match output {
        Some(path) => Some(path.to_path_buf()),
        None if input.is_dir() => Some(input.with_extension("kn")),
        None => None,
    };

    let (program, summary) = import_path_to_program(input, batch)?;

    if input.is_dir() && summary.imported_files == 0 {
        maybe_write_failure_report(
            input,
            resolved_output.as_deref(),
            target,
            batch,
            &summary,
            None,
            None,
        )?;
        return Err(KainError::runtime(build_no_import_detail(&summary)));
    }

    // Generate KAIN source code from AST
    let kain_source = generate_kain_source(&program)?;
    let mut generated_kain_path: Option<PathBuf> = None;

    if let Some(out_path) = resolved_output.as_deref() {
        fs::write(out_path, &kain_source)
            .map_err(|e| KainError::runtime(format!("Failed to write output: {}", e)))?;

        generated_kain_path = Some(out_path.to_path_buf());
        println!(
            "✓ Generated KAIN source: {} ({} bytes)",
            out_path.display(),
            kain_source.len()
        );
    }

    // If target specified, compile directly
    let mut compiled_output_path: Option<PathBuf> = None;
    if let Some(target_str) = target {
        let compile_target = crate::parse_compile_target(target_str)
            .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;

        println!("🔨 Compiling to target: {}", target_str);

        let compiled = crate::compile(&kain_source, compile_target)
            .map_err(|e| KainError::runtime(format!("Compilation failed: {}", e)))?;

        // Determine output path for compiled result
        let compiled_output = if let Some(out) = resolved_output.as_deref() {
            out.with_extension(crate::target_extension(compile_target))
        } else {
            input.with_extension(crate::target_extension(compile_target))
        };

        fs::write(&compiled_output, &compiled)
            .map_err(|e| KainError::runtime(format!("Failed to write compiled output: {}", e)))?;

        compiled_output_path = Some(compiled_output.clone());
        println!(
            "✓ Compiled output: {} ({} bytes)",
            compiled_output.display(),
            compiled.len()
        );
    }

    // Print summary
    println!("✅ Import complete");
    println!("   Functions: {}", count_functions(&program));
    println!("   Structs: {}", count_structs(&program));
    println!("   Enums: {}", count_enums(&program));
    if input.is_dir() {
        println!("   Directory structure: preserved as nested modules unless flat mode is enabled");
        println!(
            "   Rust files: discovered {}, imported {}, skipped {}, failed {}",
            summary.discovered_files,
            summary.imported_files,
            summary.skipped_files,
            summary.failed_files.len()
        );

        if !summary.failed_files.is_empty() {
            println!("   Failed files:");
            for (path, error) in summary.failed_files.iter().take(20) {
                println!("     - {}: {}", path.display(), error);
            }
            if summary.failed_files.len() > 20 {
                println!("     ... {} more", summary.failed_files.len() - 20);
            }
        }
        if !summary.diagnostics.is_empty() {
            let total_diags: usize = summary
                .diagnostics
                .iter()
                .map(|(_, diags)| diags.len())
                .sum();
            let class_counts = diagnostic_class_counts(&summary.diagnostics);
            let external_mod_diags = *class_counts.get("external_mod_decl").unwrap_or(&0);
            let visible_diags = total_diags.saturating_sub(external_mod_diags);
            if visible_diags > 0 {
                println!(
                    "   Lossy lowering: {} diagnostic(s) across {} file(s)",
                    visible_diags,
                    summary.diagnostics.len()
                );
                if let Some((class, count)) = class_counts
                    .iter()
                    .find(|(class, _)| class.as_str() != "external_mod_decl")
                {
                    println!(
                        "   Primary repair seam: class:{} ({} note(s))",
                        class, count
                    );
                }
            }
            if external_mod_diags > 0 {
                println!(
                    "   External module declarations: {} note(s) (directory structure preserved)",
                    external_mod_diags
                );
            }
        }
    } else if !summary.diagnostics.is_empty() {
        let total_diags: usize = summary
            .diagnostics
            .iter()
            .map(|(_, diags)| diags.len())
            .sum();
        let class_counts = diagnostic_class_counts(&summary.diagnostics);
        let external_mod_diags = *class_counts.get("external_mod_decl").unwrap_or(&0);
        let visible_diags = total_diags.saturating_sub(external_mod_diags);
        if visible_diags > 0 {
            println!("   Lossy lowering: {} diagnostic(s)", visible_diags);
            if let Some((class, count)) = class_counts
                .iter()
                .find(|(class, _)| class.as_str() != "external_mod_decl")
            {
                println!(
                    "   Primary repair seam: class:{} ({} note(s))",
                    class, count
                );
            }
        }
        if external_mod_diags > 0 {
            println!(
                "   External module declarations: {} note(s)",
                external_mod_diags
            );
        }
    }

    maybe_write_failure_report(
        input,
        resolved_output.as_deref(),
        target,
        batch,
        &summary,
        generated_kain_path.as_deref(),
        compiled_output_path.as_deref(),
    )?;

    Ok(())
}

fn import_path_to_program(
    input: &Path,
    batch: &ImportRustBatchOptions,
) -> KainResult<(kain_core::ast::Program, ImportRustSummary)> {
    if input.is_file() {
        let (program, diagnostics) = kain_import::import_rust_file_detailed(input)
            .map_err(|e| KainError::runtime(format!("Rust import failed: {}", e)))?;
        return Ok((
            program,
            ImportRustSummary {
                discovered_files: 1,
                imported_files: 1,
                diagnostics: if diagnostics.is_empty() {
                    Vec::new()
                } else {
                    vec![(input.to_path_buf(), diagnostics)]
                },
                ..ImportRustSummary::default()
            },
        ));
    }

    if !input.is_dir() {
        return Err(KainError::runtime(format!(
            "Input is neither file nor directory: {}",
            input.display()
        )));
    }

    let result = import_rust_directory_programs(input, batch)?;
    Ok((
        merge_imported_directory_programs(result.imported_files, batch.flat),
        result.summary,
    ))
}

fn build_no_import_detail(summary: &ImportRustSummary) -> String {
    if summary.failed_files.is_empty() {
        return "No Rust source files matched include/exclude filters".to_string();
    }

    let mut msg = String::from("All matching Rust files failed to import");
    let previews = summary
        .failed_files
        .iter()
        .take(5)
        .map(|(path, err)| format!("{}: {}", path.display(), err))
        .collect::<Vec<_>>();
    if !previews.is_empty() {
        msg.push_str(&format!(" (e.g. {})", previews.join(" | ")));
    }
    msg
}

fn maybe_write_failure_report(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportRustBatchOptions,
    summary: &ImportRustSummary,
    generated_kain_path: Option<&Path>,
    compiled_output_path: Option<&Path>,
) -> KainResult<()> {
    let Some(report_path) = resolve_report_path(input, output, batch, summary) else {
        return Ok(());
    };

    let report = ImportRustFailureReport {
        input_path: input.display().to_string(),
        output_path: output.map(|p| p.display().to_string()),
        target: target.map(str::to_string),
        recursive: batch.recursive,
        flat: batch.flat,
        include_filters: batch.include_filters.clone(),
        exclude_filters: batch.exclude_filters.clone(),
        discovered_files: summary.discovered_files,
        imported_files: summary.imported_files,
        skipped_files: summary.skipped_files,
        failed_files: summary
            .failed_files
            .iter()
            .map(|(path, error)| ImportRustFailureEntry {
                file: path.display().to_string(),
                module_path: module_path_string_for_report(input, path),
                error: error.clone(),
                repair_hint: Some("re-run the importer on this file with --report-json to isolate the failing seam".to_string()),
            })
            .collect(),
        lossy_diagnostics: summary
            .diagnostics
            .iter()
            .map(|(path, diagnostics)| ImportRustDiagnosticEntry {
                file: path.display().to_string(),
                module_path: module_path_string_for_report(input, path),
                diagnostic_classes: diagnostic_class_counts_single(diagnostics),
                diagnostics: diagnostics.clone(),
                repair_hint: Some("search these class markers in the generated .kn or import report; they point at the lowered seam".to_string()),
            })
            .collect(),
        diagnostics_by_class: diagnostic_class_counts(&summary.diagnostics),
        generated_kain_path: generated_kain_path.map(|p| p.display().to_string()),
        compiled_output_path: compiled_output_path.map(|p| p.display().to_string()),
        generated_at_utc: chrono::Utc::now().to_rfc3339(),
    };

    if let Some(parent) = report_path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent).map_err(|e| {
                KainError::runtime(format!(
                    "Failed to create report directory {}: {}",
                    parent.display(),
                    e
                ))
            })?;
        }
    }

    let json = serde_json::to_string_pretty(&report)
        .map_err(|e| KainError::runtime(format!("Failed to serialize import report: {}", e)))?;
    fs::write(&report_path, json)
        .map_err(|e| KainError::runtime(format!("Failed to write import report: {}", e)))?;

    println!("📄 Failure report JSON: {}", report_path.display());
    Ok(())
}

fn resolve_report_path(
    input: &Path,
    output: Option<&Path>,
    batch: &ImportRustBatchOptions,
    summary: &ImportRustSummary,
) -> Option<PathBuf> {
    if let Some(path) = &batch.report_json {
        return Some(path.clone());
    }

    if input.is_dir() && (!summary.failed_files.is_empty() || !summary.diagnostics.is_empty()) {
        return Some(
            output
                .map(|out| out.with_extension("import_report.json"))
                .unwrap_or_else(|| input.with_extension("import_report.json")),
        );
    }

    if input.is_file() && !summary.diagnostics.is_empty() {
        return Some(
            output
                .map(|out| out.with_extension("import_report.json"))
                .unwrap_or_else(|| input.with_extension("import_report.json")),
        );
    }

    None
}

fn compact_error_message(raw: &str) -> String {
    let flattened = raw
        .lines()
        .take(2)
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .collect::<Vec<_>>()
        .join(" | ");

    let mut compact = if flattened.is_empty() {
        raw.trim().to_string()
    } else {
        flattened
    };

    const LIMIT: usize = 320;
    if compact.len() > LIMIT {
        compact.truncate(LIMIT);
        compact.push_str("...");
    }
    compact
}

fn import_rust_directory_programs(
    input: &Path,
    batch: &ImportRustBatchOptions,
) -> KainResult<ImportedRustDirectoryPrograms> {
    let mut candidates = Vec::new();
    collect_rust_files(input, batch.recursive, &mut candidates)?;
    candidates.sort();

    let mut summary = ImportRustSummary {
        discovered_files: candidates.len(),
        ..ImportRustSummary::default()
    };
    let mut imported_files = Vec::new();
    let normalized_includes = normalize_filters(&batch.include_filters);
    let normalized_excludes = normalize_filters(&batch.exclude_filters);

    for file in candidates {
        let relative_path = file
            .strip_prefix(input)
            .unwrap_or(file.as_path())
            .to_path_buf();
        if !path_matches_filters(&relative_path, &normalized_includes, &normalized_excludes) {
            summary.skipped_files += 1;
            continue;
        }

        match kain_import::import_rust_file_detailed(&file) {
            Ok((program, diagnostics)) => {
                summary.imported_files += 1;
                if !diagnostics.is_empty() {
                    summary.diagnostics.push((file.clone(), diagnostics));
                }
                imported_files.push(ImportedRustFileProgram {
                    relative_path,
                    program,
                });
            }
            Err(err) => {
                let compact = compact_error_message(&format!("{}", err));
                summary.failed_files.push((file.clone(), compact));
                if batch.fail_fast {
                    return Err(KainError::runtime(format!(
                        "Rust import failed: {}: {}",
                        file.display(),
                        err
                    )));
                }
            }
        }
    }

    Ok(ImportedRustDirectoryPrograms {
        imported_files,
        summary,
    })
}

fn collect_rust_files(root: &Path, recursive: bool, out: &mut Vec<PathBuf>) -> KainResult<()> {
    let entries = fs::read_dir(root).map_err(|e| {
        KainError::runtime(format!(
            "Failed to read directory {}: {}",
            root.display(),
            e
        ))
    })?;

    for entry in entries {
        let entry = entry
            .map_err(|e| KainError::runtime(format!("Failed to read directory entry: {}", e)))?;
        let path = entry.path();
        if path.is_dir() {
            if recursive {
                collect_rust_files(&path, recursive, out)?;
            }
            continue;
        }

        if path
            .extension()
            .and_then(|ext| ext.to_str())
            .is_some_and(|ext| ext.eq_ignore_ascii_case("rs"))
        {
            out.push(path);
        }
    }

    Ok(())
}

fn normalize_filters(filters: &[String]) -> Vec<String> {
    filters
        .iter()
        .map(|f| f.trim().replace('\\', "/").to_ascii_lowercase())
        .filter(|f| !f.is_empty())
        .collect()
}

fn path_matches_filters(path: &Path, includes: &[String], excludes: &[String]) -> bool {
    let normalized = path
        .to_string_lossy()
        .replace('\\', "/")
        .to_ascii_lowercase();

    if !includes.is_empty() && !includes.iter().any(|inc| normalized.contains(inc)) {
        return false;
    }

    !excludes.iter().any(|exc| normalized.contains(exc))
}

fn module_path_for_relative_file(path: &Path) -> Vec<String> {
    let mut parts = Vec::new();
    let file_name = path.file_name().and_then(|s| s.to_str()).unwrap_or("");
    let preserve_root_file = matches!(file_name, "lib.rs" | "main.rs");
    let preserve_mod_file = file_name == "mod.rs";

    if preserve_root_file {
        return Vec::new();
    }

    if let Some(parent) = path.parent() {
        for component in parent.components() {
            let part = sanitize_module_component(&component.as_os_str().to_string_lossy());
            if !part.is_empty() {
                parts.push(part);
            }
        }
    }

    let file_stem = path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("module");
    let leaf = if preserve_mod_file && !parts.is_empty() {
        None
    } else {
        Some(sanitize_module_component(file_stem))
    };
    if let Some(leaf) = leaf.filter(|leaf| !leaf.is_empty()) {
        parts.push(leaf);
    }

    if parts.is_empty() {
        parts.push("module".to_string());
    }

    parts
}

fn sanitize_module_component(raw: &str) -> String {
    let mut name = raw
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_lowercase()
            } else {
                '_'
            }
        })
        .collect::<String>();

    while name.contains("__") {
        name = name.replace("__", "_");
    }
    name = name.trim_matches('_').to_string();

    if name.is_empty() {
        return "module".to_string();
    }
    if name.chars().next().is_some_and(|c| c.is_ascii_digit()) {
        return format!("m_{name}");
    }
    name
}

fn build_nested_module(path: &[String], items: Vec<kain_core::ast::Item>) -> kain_core::ast::Item {
    let mut current = items;
    for name in path.iter().rev() {
        current = vec![kain_core::ast::Item::Mod(kain_core::ast::Mod {
            name: name.clone(),
            inline: Some(current),
            visibility: kain_core::ast::Visibility::Public,
            span: kain_core::span::Span::default(),
        })];
    }

    current.into_iter().next().expect("nested module wrapper")
}

fn merge_imported_directory_programs(
    imported_files: Vec<ImportedRustFileProgram>,
    flat: bool,
) -> kain_core::ast::Program {
    let mut merged_items = Vec::new();
    let mut module_name_counts: HashMap<String, usize> = HashMap::new();

    for imported_file in imported_files {
        let ImportedRustFileProgram {
            relative_path,
            program,
            ..
        } = imported_file;

        if flat {
            merged_items.extend(program.items);
            continue;
        }

        let module_path = module_path_for_relative_file(&relative_path);
        if module_path.is_empty() {
            merged_items.extend(program.items);
            continue;
        }

        let entry = module_name_counts
            .entry(module_path.join("::"))
            .or_insert(0);
        *entry += 1;
        let resolved_module_path = if *entry == 1 {
            module_path
        } else {
            let mut adjusted = module_path;
            if let Some(last) = adjusted.last_mut() {
                *last = format!("{}_{}", last, *entry);
            }
            adjusted
        };

        merged_items.push(build_nested_module(&resolved_module_path, program.items));
    }

    kain_core::ast::Program {
        items: merged_items,
        span: kain_core::span::Span::default(),
    }
}

fn resolve_workspace_rust_source_root(
    workspace_root: &Path,
    explicit_source_root: Option<&Path>,
) -> KainResult<PathBuf> {
    if let Some(source_root) = explicit_source_root {
        let resolved = resolve_workspace_relative_path(workspace_root, source_root);
        if !resolved.is_dir() {
            return Err(KainError::runtime(format!(
                "Rust source root is not a directory: {}",
                resolved.display()
            )));
        }
        return Ok(resolved);
    }

    for candidate in [
        workspace_root.join("crates"),
        workspace_root.join("rust"),
        workspace_root.join("src").join("rust"),
    ] {
        if candidate.is_dir() {
            return Ok(candidate);
        }
    }

    Err(KainError::runtime(format!(
        "No Rust crate source root found under {} (checked ./crates, ./rust, and ./src/rust)",
        workspace_root.display()
    )))
}

fn discover_workspace_crate_roots(source_root: &Path) -> KainResult<Vec<PathBuf>> {
    let mut crate_roots = Vec::new();
    let entries = fs::read_dir(source_root).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read Rust source root {}: {}",
            source_root.display(),
            err
        ))
    })?;

    for entry in entries {
        let entry = entry.map_err(|err| {
            KainError::runtime(format!("Failed to read directory entry: {}", err))
        })?;
        let path = entry.path();
        if path.is_dir() && path.join("Cargo.toml").is_file() {
            crate_roots.push(path);
        }
    }

    crate_roots.sort();
    crate_roots.dedup();

    if crate_roots.is_empty() && source_root.join("Cargo.toml").is_file() {
        crate_roots.push(source_root.to_path_buf());
    }

    if crate_roots.is_empty() {
        return Err(KainError::runtime(format!(
            "No Rust crates with Cargo.toml were discovered under {}",
            source_root.display()
        )));
    }

    Ok(crate_roots)
}

fn resolve_workspace_relative_path(workspace_root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        workspace_root.join(path)
    }
}

fn merge_workspace_crate_programs(
    crate_results: Vec<ImportedRustCrateResult>,
    flat: bool,
) -> kain_core::ast::Program {
    let mut merged_items = Vec::new();
    let mut crate_name_counts: HashMap<String, usize> = HashMap::new();

    for crate_result in crate_results {
        let program = merge_imported_directory_programs(crate_result.imported_files, flat);
        if flat {
            merged_items.extend(program.items);
            continue;
        }
        if program.items.is_empty() {
            continue;
        }

        let base_name = sanitize_module_component(&crate_result.crate_name);
        let entry = crate_name_counts.entry(base_name.clone()).or_insert(0);
        *entry += 1;
        let crate_module_name = if *entry == 1 {
            base_name
        } else {
            format!("{}_{}", base_name, *entry)
        };
        merged_items.push(build_nested_module(&[crate_module_name], program.items));
    }

    kain_core::ast::Program {
        items: merged_items,
        span: kain_core::span::Span::default(),
    }
}

fn emit_imported_crates_as_blades(
    output_root: &Path,
    crate_results: &[ImportedRustCrateResult],
) -> KainResult<usize> {
    let mut mirrored_file_count = 0usize;

    for crate_result in crate_results {
        for imported_file in &crate_result.imported_files {
            let rendered = generate_kain_source(&imported_file.program)?;
            let output_path = output_root.join(&crate_result.crate_name).join(
                kain_output_relative_path_for_source_file(&imported_file.relative_path),
            );
            write_generated_kain_file(&output_path, &rendered, "workspace blades mirror")?;
            mirrored_file_count += 1;
        }
    }

    Ok(mirrored_file_count)
}

fn kain_output_relative_path_for_source_file(relative_path: &Path) -> PathBuf {
    let mut output_path = relative_path.to_path_buf();
    output_path.set_extension("kn");
    output_path
}

fn write_generated_kain_file(path: &Path, content: &str, label: &str) -> KainResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create {} parent directory {}: {}",
                label,
                parent.display(),
                err
            ))
        })?;
    }
    fs::write(path, content).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write {} {}: {}",
            label,
            path.display(),
            err
        ))
    })
}

/// Generate KAIN source code from AST
fn generate_kain_source(program: &kain_core::ast::Program) -> KainResult<String> {
    use std::fmt::Write;

    let mut output = String::new();

    // Header comment
    writeln!(output, "# Generated from Rust source by kain import-rust")
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    writeln!(output, "# Project Ouroboros — Rust → KAIN → Rust")
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;

    // Generate code for each item
    for item in &program.items {
        write_item(&mut output, item, 0)?;
    }

    Ok(output)
}

fn write_item(output: &mut String, item: &kain_core::ast::Item, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    match item {
        kain_core::ast::Item::Function(func) => write_function(output, func, indent),
        kain_core::ast::Item::Struct(s) => write_struct(output, s, indent),
        kain_core::ast::Item::Enum(e) => write_enum(output, e, indent),
        kain_core::ast::Item::Trait(value) => write_trait(output, value, indent),
        kain_core::ast::Item::Impl(value) => write_impl(output, value, indent),
        kain_core::ast::Item::Mod(m) => {
            write_line(output, indent, &format!("mod {}:", m.name))?;
            if let Some(children) = &m.inline {
                if !children.is_empty() {
                    for child in children {
                        write_item(output, child, indent + 1)?;
                    }
                }
            }
            writeln!(output)
                .map_err(|e| KainError::runtime(format!("Failed to write module: {}", e)))
        }
        _ => Ok(()),
    }
}

fn write_function(
    output: &mut String,
    func: &kain_core::ast::Function,
    indent: usize,
) -> KainResult<()> {
    use std::fmt::Write;

    // Function signature
    let mut signature = format!("fn {}(", func.name);

    // Parameters
    for (i, param) in func.params.iter().enumerate() {
        if i > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
    }

    signature.push(')');

    // Return type
    if let Some(ret_ty) = &func.return_type {
        signature.push_str(&format!(" -> {}", type_to_string(ret_ty)));
    }

    signature.push(':');
    write_line(output, indent, &signature)?;

    write_block(output, &func.body, indent + 1)?;
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;

    Ok(())
}

fn write_struct(output: &mut String, s: &kain_core::ast::Struct, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", s.name))?;

    if s.fields.is_empty() {
        write_line(
            output,
            indent + 1,
            &lossy_marker(
                "empty_struct_lowering",
                "empty Rust struct lowered to pass",
                None,
            ),
        )?;
    } else {
        for field in &s.fields {
            write_line(
                output,
                indent + 1,
                &format!("{}: {}", field.name, type_to_string(&field.ty)),
            )?;
        }
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;

    Ok(())
}

fn write_enum(output: &mut String, e: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", e.name))?;

    if e.variants.is_empty() {
        write_line(
            output,
            indent + 1,
            &lossy_marker(
                "empty_enum_lowering",
                "empty Rust enum lowered to pass",
                None,
            ),
        )?;
    } else {
        for variant in &e.variants {
            let variant_str = match &variant.fields {
                kain_core::ast::VariantFields::Unit => variant.name.clone(),
                kain_core::ast::VariantFields::Tuple(types) => {
                    let types_str = types
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{}({})", variant.name, types_str)
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let fields_str = fields
                        .iter()
                        .map(|field| format!("{}: {}", field.name, type_to_string(&field.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{ {} }}", variant.name, fields_str)
                }
            };
            write_line(output, indent + 1, &variant_str)?;
        }
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;

    Ok(())
}

fn write_trait(
    output: &mut String,
    value: &kain_core::ast::Trait,
    indent: usize,
) -> KainResult<()> {
    use std::fmt::Write;

    let trait_header = if value.supertraits.is_empty() {
        format!("trait {}:", value.name)
    } else {
        let supertraits = value
            .supertraits
            .iter()
            .map(type_to_string)
            .collect::<Vec<_>>()
            .join(" + ");
        format!("trait {} <: {}:", value.name, supertraits)
    };
    write_line(output, indent, &trait_header)?;
    if value.methods.is_empty() {
        write_line(
            output,
            indent + 1,
            &lossy_marker(
                "empty_trait_lowering",
                "empty Rust trait lowered to pass",
                None,
            ),
        )?;
    } else {
        for method in &value.methods {
            let mut signature = format!("fn {}(", method.name);
            for (index, param) in method.params.iter().enumerate() {
                if index > 0 {
                    signature.push_str(", ");
                }
                signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
            }
            signature.push(')');
            if let Some(return_type) = &method.return_type {
                signature.push_str(&format!(" -> {}", type_to_string(return_type)));
            }
            signature.push(':');
            write_line(output, indent + 1, &signature)?;
            if let Some(default_impl) = &method.default_impl {
                write_block(output, default_impl, indent + 2)?;
            } else {
                write_line(
                    output,
                    indent + 2,
                    &lossy_marker(
                        "missing_trait_default_body",
                        "missing Rust trait default body lowered to pass",
                        Some("restore the original Rust body or add a concrete KAIN lowering"),
                    ),
                )?;
            }
        }
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write trait: {}", e)))?;

    Ok(())
}

fn write_impl(output: &mut String, value: &kain_core::ast::Impl, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let header = match &value.trait_name {
        Some(trait_name) => format!(
            "impl {} for {}:",
            trait_name,
            type_to_string(&value.target_type)
        ),
        None => format!("impl {}:", type_to_string(&value.target_type)),
    };
    write_line(output, indent, &header)?;

    if value.methods.is_empty() {
        write_line(
            output,
            indent + 1,
            &lossy_marker(
                "empty_impl_lowering",
                "empty Rust impl lowered to pass",
                None,
            ),
        )?;
    } else {
        for method in &value.methods {
            write_function(output, method, indent + 1)?;
        }
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write impl: {}", e)))?;

    Ok(())
}

fn write_block(
    output: &mut String,
    block: &kain_core::ast::Block,
    indent: usize,
) -> KainResult<()> {
    if block.stmts.is_empty() {
        write_line(output, indent, "()")?;
        return Ok(());
    }

    for stmt in &block.stmts {
        write_stmt(output, stmt, indent)?;
    }

    Ok(())
}

fn write_stmt(output: &mut String, stmt: &kain_core::ast::Stmt, indent: usize) -> KainResult<()> {
    match stmt {
        kain_core::ast::Stmt::Let {
            pattern, ty, value, ..
        } => {
            let mut line = format!("let {}", pattern_to_string(pattern));
            if let Some(ty) = ty {
                line.push_str(&format!(": {}", type_to_string(ty)));
            }
            if let Some(value) = value {
                line.push_str(&format!(" = {}", expr_to_string(value)));
            }
            write_multiline(output, indent, &line)
        }
        kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Block(block, _)) => {
            write_block(output, block, indent)
        }
        kain_core::ast::Stmt::Expr(expr) => write_multiline(output, indent, &expr_to_string(expr)),
        kain_core::ast::Stmt::Return(value, _) => {
            if let Some(expr) = value {
                write_multiline(output, indent, &format!("return {}", expr_to_string(expr)))
            } else {
                write_line(output, indent, "return")
            }
        }
        kain_core::ast::Stmt::Break(value, _) => {
            if let Some(expr) = value {
                write_multiline(output, indent, &format!("break {}", expr_to_string(expr)))
            } else {
                write_line(output, indent, "break")
            }
        }
        kain_core::ast::Stmt::Continue(_) => write_line(output, indent, "continue"),
        kain_core::ast::Stmt::For {
            binding,
            iter,
            body,
            ..
        }
        | kain_core::ast::Stmt::Fanout {
            binding,
            iter,
            body,
            ..
        } => {
            let keyword = if matches!(stmt, kain_core::ast::Stmt::Fanout { .. }) {
                "fanout"
            } else {
                "for"
            };
            write_multiline(
                output,
                indent,
                &format!(
                    "{keyword} {} in {}:",
                    pattern_to_string(binding),
                    expr_to_string(iter)
                ),
            )?;
            write_block(output, body, indent + 1)
        }
        kain_core::ast::Stmt::While {
            condition, body, ..
        } => {
            write_multiline(
                output,
                indent,
                &format!("while {}:", expr_to_string(condition)),
            )?;
            write_block(output, body, indent + 1)
        }
        kain_core::ast::Stmt::Loop { body, .. } => {
            write_line(output, indent, "loop:")?;
            write_block(output, body, indent + 1)
        }
        kain_core::ast::Stmt::Item(item) => write_item(output, item, indent),
    }
}

fn write_line(output: &mut String, indent: usize, line: &str) -> KainResult<()> {
    use std::fmt::Write;
    writeln!(output, "{}{}", "    ".repeat(indent), line)
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))
}

fn write_multiline(output: &mut String, indent: usize, text: &str) -> KainResult<()> {
    for line in text.lines() {
        write_line(output, indent, line)?;
    }
    if text.is_empty() {
        write_line(output, indent, "")?;
    }
    Ok(())
}

fn lossy_marker(class: &str, message: &str, repair_hint: Option<&str>) -> String {
    match repair_hint {
        Some(hint) => format!("# LOSSY LOWERING [class:{class}]: {message} | repair: {hint}",),
        None => format!("# LOSSY LOWERING [class:{class}]: {message}"),
    }
}

fn module_path_string_for_report(root: &Path, path: &Path) -> Option<String> {
    let rel = path.strip_prefix(root).unwrap_or(path);
    let module_path = module_path_for_relative_file(rel);
    if module_path.is_empty() {
        None
    } else {
        Some(module_path.join("::"))
    }
}

fn diagnostic_class_counts_single(diagnostics: &[String]) -> BTreeMap<String, usize> {
    let mut counts = BTreeMap::new();
    for diag in diagnostics {
        if let Some(class) = diagnostic_class(diag) {
            *counts.entry(class.to_string()).or_insert(0) += 1;
        }
    }
    counts
}

fn diagnostic_class_counts(diagnostics: &[(PathBuf, Vec<String>)]) -> BTreeMap<String, usize> {
    let mut counts = BTreeMap::new();
    for (_, file_diags) in diagnostics {
        for diag in file_diags {
            if let Some(class) = diagnostic_class(diag) {
                *counts.entry(class.to_string()).or_insert(0) += 1;
            }
        }
    }
    counts
}

fn diagnostic_class(diag: &str) -> Option<&str> {
    diag.split("[class:")
        .nth(1)?
        .split(']')
        .next()
        .map(str::trim)
        .filter(|s| !s.is_empty())
}

fn pattern_to_string(pattern: &kain_core::ast::Pattern) -> String {
    match pattern {
        kain_core::ast::Pattern::Wildcard(_) => "_".to_string(),
        kain_core::ast::Pattern::Literal(expr) => expr_to_string(expr),
        kain_core::ast::Pattern::Binding { name, mutable, .. } => {
            if *mutable {
                format!("mut {}", name)
            } else {
                name.clone()
            }
        }
        kain_core::ast::Pattern::Struct {
            name, fields, rest, ..
        } => {
            let mut parts = fields
                .iter()
                .map(|(field, pattern)| format!("{field}: {}", pattern_to_string(pattern)))
                .collect::<Vec<_>>();
            if *rest {
                parts.push("..".to_string());
            }
            format!("{name} {{ {} }}", parts.join(", "))
        }
        kain_core::ast::Pattern::Tuple(patterns, _) => format!(
            "({})",
            patterns
                .iter()
                .map(pattern_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        kain_core::ast::Pattern::Variant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let head = enum_name
                .as_ref()
                .map(|name| format!("{name}::{variant}"))
                .unwrap_or_else(|| variant.clone());
            variant_pattern_to_string(&head, fields)
        }
        kain_core::ast::Pattern::Slice { patterns, rest, .. } => {
            let mut parts = patterns.iter().map(pattern_to_string).collect::<Vec<_>>();
            if let Some(rest) = rest {
                parts.push(format!("{rest} @ .."));
            }
            format!("[{}]", parts.join(", "))
        }
        kain_core::ast::Pattern::Or(patterns, _) => patterns
            .iter()
            .map(pattern_to_string)
            .collect::<Vec<_>>()
            .join(" | "),
        kain_core::ast::Pattern::Range {
            start,
            end,
            inclusive,
            ..
        } => {
            let marker = if *inclusive { "..=" } else { ".." };
            format!(
                "{}{}{}",
                start
                    .as_ref()
                    .map(|expr| expr_to_string(expr))
                    .unwrap_or_default(),
                marker,
                end.as_ref()
                    .map(|expr| expr_to_string(expr))
                    .unwrap_or_default()
            )
        }
    }
}

fn expr_to_string(expr: &kain_core::ast::Expr) -> String {
    expr_to_string_prec(expr, 0)
}

fn expr_to_string_prec(expr: &kain_core::ast::Expr, parent_prec: u8) -> String {
    use kain_core::ast::Expr;

    let current_prec = expr_precedence(expr);
    let mut rendered = match expr {
        Expr::Int(value, _) => value.to_string(),
        Expr::Float(value, _) => format!("{value:?}"),
        Expr::String(value, _) => format!("{:?}", value),
        Expr::FString(parts, _) => format_f_string(parts),
        Expr::Bool(value, _) => value.to_string(),
        Expr::None(_) => "none".to_string(),
        Expr::Ident(name, _) => name.clone(),
        Expr::MacroCall { name, args, .. } => {
            format!(
                "{}!({})",
                name,
                args.iter()
                    .map(expr_to_string)
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        }
        Expr::Binary {
            left, op, right, ..
        } => {
            let prec = binary_precedence(*op);
            format!(
                "{} {} {}",
                expr_to_string_prec(left, prec),
                binary_op_to_string(*op),
                expr_to_string_prec(right, prec + 1)
            )
        }
        Expr::Unary { op, operand, .. } => {
            format!(
                "{}{}",
                unary_op_to_string(*op),
                expr_to_string_prec(operand, 13)
            )
        }
        Expr::Call { callee, args, .. } => render_call_like(
            &expr_to_string_prec(callee, 14),
            &call_args_to_strings(args),
        ),
        Expr::StageCall {
            runtime,
            function,
            args,
            ..
        } => render_call_like(
            &format!("{} {}", runtime.as_str(), function),
            &call_args_to_strings(args),
        ),
        Expr::MethodCall {
            receiver,
            method,
            args,
            ..
        } => render_call_like(
            &format!("{}.{}", expr_to_string_prec(receiver, 14), method),
            &call_args_to_strings(args),
        ),
        Expr::Field { object, field, .. } => {
            format!("{}.{}", expr_to_string_prec(object, 14), field)
        }
        Expr::Index { object, index, .. } => {
            format!(
                "{}[{}]",
                expr_to_string_prec(object, 14),
                expr_to_string(index)
            )
        }
        Expr::Assign { target, value, .. } => {
            format!("{} = {}", expr_to_string(target), expr_to_string(value))
        }
        Expr::Struct {
            name, fields, rest, ..
        } => {
            let mut entries = fields
                .iter()
                .map(|(field, value)| format!("{field}: {}", expr_to_string(value)))
                .collect::<Vec<_>>();
            if let Some(rest) = rest {
                entries.push(format!("..{}", expr_to_string(rest)));
            }
            format!("{name} {{ {} }}", entries.join(", "))
        }
        Expr::AggregateInit {
            ty,
            fields,
            zero_fill_rest,
            ..
        } => {
            let mut args = vec![format!("{:?}", type_to_string(ty))];
            args.extend(
                fields
                    .iter()
                    .map(|(field, value)| format!("{field} = {}", expr_to_string(value))),
            );
            if !zero_fill_rest {
                args.push("zero_fill_rest = false".to_string());
            }
            render_call_like("aggregate_init", &args)
        }
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            ..
        } => enum_variant_to_string(&format!("{enum_name}::{variant}"), fields),
        Expr::Array(items, _) => format!(
            "[{}]",
            items
                .iter()
                .map(expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Tuple(items, _) => format!(
            "({})",
            items
                .iter()
                .map(expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Range {
            start,
            end,
            inclusive,
            ..
        } => {
            let marker = if *inclusive { "..=" } else { ".." };
            format!(
                "{}{}{}",
                start
                    .as_ref()
                    .map(|expr| expr_to_string(expr))
                    .unwrap_or_default(),
                marker,
                end.as_ref()
                    .map(|expr| expr_to_string(expr))
                    .unwrap_or_default()
            )
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let mut output = format!(
                "if {}:\n{}",
                expr_to_string(condition),
                block_to_string(then_branch, 1)
            );
            if let Some(branch) = else_branch {
                output.push('\n');
                output.push_str(&else_branch_to_string(branch));
            }
            output
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            let mut lines = vec![format!("match {}:", expr_to_string(scrutinee))];
            lines.extend(
                arms.iter()
                    .map(match_arm_to_string)
                    .map(|arm| indent_text(&arm, 1)),
            );
            lines.join("\n")
        }
        Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => lambda_to_string(params, return_type.as_ref(), body),
        Expr::Ref { mutable, value, .. } => {
            if *mutable {
                format!("&mut {}", expr_to_string_prec(value, 13))
            } else {
                format!("&{}", expr_to_string_prec(value, 13))
            }
        }
        Expr::AddrOf { value, .. } => format!("addr_of({})", expr_to_string(value)),
        Expr::Deref(value, _) => format!("*{}", expr_to_string_prec(value, 13)),
        Expr::PtrOffset {
            pointer, offset, ..
        } => format!(
            "ptr_offset({}, {})",
            expr_to_string(pointer),
            expr_to_string(offset)
        ),
        Expr::MemLoad { pointer, .. } => format!("mem_load({})", expr_to_string(pointer)),
        Expr::VolatileLoad { pointer, .. } => format!("volatile_load({})", expr_to_string(pointer)),
        Expr::MemStore { pointer, value, .. } => {
            format!(
                "mem_store({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::VolatileStore { pointer, value, .. } => {
            format!(
                "volatile_store({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicLoad { pointer, .. } => format!("atomic_load({})", expr_to_string(pointer)),
        Expr::AtomicStore { pointer, value, .. } => {
            format!(
                "atomic_store({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicAdd { pointer, value, .. } => {
            format!(
                "atomic_add({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicSub { pointer, value, .. } => {
            format!(
                "atomic_sub({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicAnd { pointer, value, .. } => {
            format!(
                "atomic_and({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicOr { pointer, value, .. } => {
            format!(
                "atomic_or({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicXor { pointer, value, .. } => {
            format!(
                "atomic_xor({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicExchange { pointer, value, .. } => {
            format!(
                "atomic_exchange({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            )
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => format!(
            "atomic_compare_exchange({}, {}, {})",
            expr_to_string(pointer),
            expr_to_string(expected),
            expr_to_string(desired)
        ),
        Expr::AtomicFence { .. } => "atomic_fence()".to_string(),
        Expr::SizeOfType { target, .. } => {
            format!("sizeof_type({:?})", type_to_string(target))
        }
        Expr::AlignOfType { target, .. } => {
            format!("alignof_type({:?})", type_to_string(target))
        }
        Expr::Alloca { ty, .. } => format!("alloca({:?})", type_to_string(ty)),
        Expr::Uninit { ty, .. } => format!("uninit({:?})", type_to_string(ty)),
        Expr::Alloc { size, zeroed, .. } => {
            format!(
                "{}({})",
                if *zeroed { "alloc_zeroed" } else { "alloc" },
                expr_to_string(size)
            )
        }
        Expr::Realloc { pointer, size, .. } => {
            format!(
                "realloc_mem({}, {})",
                expr_to_string(pointer),
                expr_to_string(size)
            )
        }
        Expr::Observe { target, body, .. } => match body.as_ref() {
            Expr::Block(block, _) => {
                format!(
                    "observe {}:\n{}",
                    expr_to_string(target),
                    block_to_string(block, 1)
                )
            }
            other => format!(
                "observe {}: {}",
                expr_to_string(target),
                expr_to_string(other)
            ),
        },
        Expr::Collapse { target, body, .. } => match body.as_ref() {
            Expr::Block(block, _) => {
                format!(
                    "collapse {}:\n{}",
                    expr_to_string(target),
                    block_to_string(block, 1)
                )
            }
            other => format!(
                "collapse {}: {}",
                expr_to_string(target),
                expr_to_string(other)
            ),
        },
        Expr::Share { target, body, .. } => match body.as_ref() {
            Expr::Block(block, _) => {
                format!(
                    "share {}:\n{}",
                    expr_to_string(target),
                    block_to_string(block, 1)
                )
            }
            other => format!(
                "share {}: {}",
                expr_to_string(target),
                expr_to_string(other)
            ),
        },
        Expr::Decay { target, .. } => format!("decay {}", expr_to_string_prec(target, 13)),
        Expr::Teleport {
            value,
            source_world,
            target_world,
            channel,
            ..
        } => {
            let mut rendered = format!(
                "teleport {} from {} to {}",
                expr_to_string_prec(value, 13),
                source_world,
                target_world
            );
            if let Some(channel) = channel {
                rendered.push_str(&format!(" via {channel}"));
            }
            rendered
        }
        Expr::Cast { value, target, .. } => {
            format!(
                "{} as {}",
                expr_to_string_prec(value, 12),
                type_to_string(target)
            )
        }
        Expr::Try(value, _) => format!("{}?", expr_to_string_prec(value, 14)),
        Expr::Await(value, _) => format!("await {}", expr_to_string_prec(value, 13)),
        Expr::AsyncBlock(value, _) => match value.as_ref() {
            Expr::Block(block, _) => format!("async:\n{}", block_to_string(block, 1)),
            other => format!("async {}", expr_to_string_prec(other, 13)),
        },
        Expr::Spawn { actor, init, .. } => render_call_like(
            &format!("spawn {actor}"),
            &init
                .iter()
                .map(|(name, value)| format!("{name} = {}", expr_to_string(value)))
                .collect::<Vec<_>>(),
        ),
        Expr::SendMsg {
            target,
            message,
            data,
            ..
        } => render_call_like(
            &format!("send {}.{}", expr_to_string(target), message),
            &data
                .iter()
                .map(|(name, value)| format!("{name} = {}", expr_to_string(value)))
                .collect::<Vec<_>>(),
        ),
        Expr::Comptime(value, _) => match value.as_ref() {
            Expr::Block(block, _) => format!("comptime:\n{}", block_to_string(block, 1)),
            other => format!("comptime {}", expr_to_string(other)),
        },
        Expr::Block(block, _) => block_expr_to_string(block),
        Expr::JSX(_, _) => lossy_marker(
            "jsx_expr_printing",
            "JSX expression preserved in AST but import-rust CLI printer cannot emit JSX yet",
            Some("use the AST formatter or add JSX emission here"),
        ),
        Expr::Paren(value, _) => format!("({})", expr_to_string(value)),
        Expr::Return(Some(value), _) => format!("return {}", expr_to_string(value)),
        Expr::Return(None, _) => "return".to_string(),
        Expr::Break(Some(value), _) => format!("break {}", expr_to_string(value)),
        Expr::Break(None, _) => "break".to_string(),
        Expr::Continue(_) => "continue".to_string(),
    };

    if current_prec != 0 && current_prec < parent_prec {
        rendered = format!("({rendered})");
    }
    rendered
}

fn call_args_to_strings(args: &[kain_core::ast::CallArg]) -> Vec<String> {
    args.iter()
        .map(|arg| match &arg.name {
            Some(name) => format!("{name} = {}", expr_to_string(&arg.value)),
            None => expr_to_string(&arg.value),
        })
        .collect()
}

fn render_call_like(callee: &str, args: &[String]) -> String {
    if args.is_empty() {
        return format!("{callee}()");
    }
    if args.iter().all(|arg| !arg.contains('\n')) {
        return format!("{callee}({})", args.join(", "));
    }

    let body = args
        .iter()
        .map(|arg| indent_text(arg, 1))
        .collect::<Vec<_>>()
        .join(",\n");
    format!("{callee}(\n{body}\n)")
}

fn enum_variant_to_string(head: &str, fields: &kain_core::ast::EnumVariantFields) -> String {
    match fields {
        kain_core::ast::EnumVariantFields::Unit => head.to_string(),
        kain_core::ast::EnumVariantFields::Tuple(values) => format!(
            "{}({})",
            head,
            values
                .iter()
                .map(expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        kain_core::ast::EnumVariantFields::Struct(values) => format!(
            "{} {{ {} }}",
            head,
            values
                .iter()
                .map(|(name, value)| format!("{name}: {}", expr_to_string(value)))
                .collect::<Vec<_>>()
                .join(", ")
        ),
    }
}

fn variant_pattern_to_string(head: &str, fields: &kain_core::ast::VariantPatternFields) -> String {
    match fields {
        kain_core::ast::VariantPatternFields::Unit => head.to_string(),
        kain_core::ast::VariantPatternFields::Tuple(patterns) => format!(
            "{}({})",
            head,
            patterns
                .iter()
                .map(pattern_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        kain_core::ast::VariantPatternFields::Struct(patterns) => format!(
            "{} {{ {} }}",
            head,
            patterns
                .iter()
                .map(|(field, pattern)| format!("{field}: {}", pattern_to_string(pattern)))
                .collect::<Vec<_>>()
                .join(", ")
        ),
    }
}

fn block_to_string(block: &kain_core::ast::Block, indent: usize) -> String {
    let mut output = String::new();
    match write_block(&mut output, block, indent) {
        Ok(()) => output.trim_end().to_string(),
        Err(err) => lossy_marker(
            "block_printing",
            &format!("block could not be printed: {err}"),
            Some("inspect the source block and add a concrete import-rust printer case"),
        ),
    }
}

fn block_expr_to_string(block: &kain_core::ast::Block) -> String {
    if block.stmts.is_empty() {
        return "()".to_string();
    }
    if block.stmts.len() == 1 {
        match &block.stmts[0] {
            kain_core::ast::Stmt::Expr(expr) => return expr_to_string(expr),
            kain_core::ast::Stmt::Return(Some(expr), _) => return expr_to_string(expr),
            other => {
                return block_to_string(
                    &kain_core::ast::Block {
                        stmts: vec![other.clone()],
                        span: block.span,
                    },
                    0,
                )
            }
        }
    }
    format!("block:\n{}", block_to_string(block, 1))
}

fn else_branch_to_string(branch: &kain_core::ast::ElseBranch) -> String {
    match branch {
        kain_core::ast::ElseBranch::Else(block) => {
            format!("else:\n{}", block_to_string(block, 1))
        }
        kain_core::ast::ElseBranch::ElseIf(condition, block, next) => {
            let mut output = format!(
                "else if {}:\n{}",
                expr_to_string(condition),
                block_to_string(block, 1)
            );
            if let Some(next) = next {
                output.push('\n');
                output.push_str(&else_branch_to_string(next));
            }
            output
        }
    }
}

fn match_arm_to_string(arm: &kain_core::ast::MatchArm) -> String {
    let mut head = pattern_to_string(&arm.pattern);
    if let Some(guard) = &arm.guard {
        head.push_str(" if ");
        head.push_str(&expr_to_string(guard));
    }
    match &arm.body {
        kain_core::ast::Expr::Block(block, _) => {
            format!("{head} =>\n{}", block_to_string(block, 1))
        }
        kain_core::ast::Expr::If { .. } | kain_core::ast::Expr::Match { .. } => {
            format!("{head} =>\n{}", indent_text(&expr_to_string(&arm.body), 1))
        }
        _ => format!("{head} => {}", expr_to_string(&arm.body)),
    }
}

fn lambda_to_string(
    params: &[kain_core::ast::Param],
    return_type: Option<&kain_core::ast::Type>,
    body: &kain_core::ast::Expr,
) -> String {
    let can_use_pipe = return_type.is_none()
        && params
            .iter()
            .all(|param| matches!(param.ty, kain_core::ast::Type::Infer(_)) && !param.mutable);
    if can_use_pipe {
        return format!(
            "|{}| {}",
            params
                .iter()
                .map(|param| param.name.clone())
                .collect::<Vec<_>>()
                .join(", "),
            expr_to_string(body)
        );
    }

    let mut head = format!(
        "fn({})",
        params
            .iter()
            .map(|param| {
                let mut rendered = String::new();
                if param.mutable {
                    rendered.push_str("mut ");
                }
                rendered.push_str(&param.name);
                rendered.push_str(": ");
                rendered.push_str(&type_to_string(&param.ty));
                rendered
            })
            .collect::<Vec<_>>()
            .join(", ")
    );
    if let Some(return_type) = return_type {
        head.push_str(" -> ");
        head.push_str(&type_to_string(return_type));
    }
    match body {
        kain_core::ast::Expr::Block(block, _) => {
            format!("{head}:\n{}", block_to_string(block, 1))
        }
        _ => format!("{head}: {}", expr_to_string(body)),
    }
}

fn indent_text(text: &str, levels: usize) -> String {
    let prefix = "    ".repeat(levels);
    text.lines()
        .map(|line| format!("{prefix}{line}"))
        .collect::<Vec<_>>()
        .join("\n")
}

fn format_f_string(parts: &[kain_core::ast::Expr]) -> String {
    let mut output = String::from("f\"");
    for part in parts {
        match part {
            kain_core::ast::Expr::String(value, _) => output.push_str(&escape_f_string_text(value)),
            other => {
                output.push('{');
                output.push_str(&expr_to_string(other));
                output.push('}');
            }
        }
    }
    output.push('"');
    output
}

fn escape_f_string_text(value: &str) -> String {
    let mut output = String::new();
    for ch in value.chars() {
        match ch {
            '\\' => output.push_str("\\\\"),
            '"' => output.push_str("\\\""),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            '{' => output.push_str("{{"),
            '}' => output.push_str("}}"),
            other => output.push(other),
        }
    }
    output
}

fn expr_precedence(expr: &kain_core::ast::Expr) -> u8 {
    match expr {
        kain_core::ast::Expr::Assign { .. } => 1,
        kain_core::ast::Expr::Binary { op, .. } => binary_precedence(*op),
        kain_core::ast::Expr::Cast { .. } => 12,
        kain_core::ast::Expr::Unary { .. }
        | kain_core::ast::Expr::Ref { .. }
        | kain_core::ast::Expr::Deref(..)
        | kain_core::ast::Expr::Await(..)
        | kain_core::ast::Expr::Try(..)
        | kain_core::ast::Expr::Teleport { .. } => 13,
        kain_core::ast::Expr::Call { .. }
        | kain_core::ast::Expr::StageCall { .. }
        | kain_core::ast::Expr::MethodCall { .. }
        | kain_core::ast::Expr::Field { .. }
        | kain_core::ast::Expr::Index { .. } => 14,
        _ => 0,
    }
}

fn binary_precedence(op: kain_core::ast::BinaryOp) -> u8 {
    match op {
        kain_core::ast::BinaryOp::Or => 1,
        kain_core::ast::BinaryOp::And => 2,
        kain_core::ast::BinaryOp::BitOr => 3,
        kain_core::ast::BinaryOp::BitXor => 4,
        kain_core::ast::BinaryOp::BitAnd => 5,
        kain_core::ast::BinaryOp::Eq | kain_core::ast::BinaryOp::Ne => 6,
        kain_core::ast::BinaryOp::Lt
        | kain_core::ast::BinaryOp::Gt
        | kain_core::ast::BinaryOp::Le
        | kain_core::ast::BinaryOp::Ge => 7,
        kain_core::ast::BinaryOp::Shl | kain_core::ast::BinaryOp::Shr => 8,
        kain_core::ast::BinaryOp::Add | kain_core::ast::BinaryOp::Sub => 9,
        kain_core::ast::BinaryOp::Mul
        | kain_core::ast::BinaryOp::Div
        | kain_core::ast::BinaryOp::Mod => 10,
        kain_core::ast::BinaryOp::Pow => 11,
        kain_core::ast::BinaryOp::Assign
        | kain_core::ast::BinaryOp::AddAssign
        | kain_core::ast::BinaryOp::SubAssign
        | kain_core::ast::BinaryOp::MulAssign
        | kain_core::ast::BinaryOp::DivAssign
        | kain_core::ast::BinaryOp::Range
        | kain_core::ast::BinaryOp::RangeInclusive => 1,
    }
}

fn binary_op_to_string(op: kain_core::ast::BinaryOp) -> &'static str {
    match op {
        kain_core::ast::BinaryOp::Add => "+",
        kain_core::ast::BinaryOp::Sub => "-",
        kain_core::ast::BinaryOp::Mul => "*",
        kain_core::ast::BinaryOp::Div => "/",
        kain_core::ast::BinaryOp::Mod => "%",
        kain_core::ast::BinaryOp::Pow => "**",
        kain_core::ast::BinaryOp::Eq => "==",
        kain_core::ast::BinaryOp::Ne => "!=",
        kain_core::ast::BinaryOp::Lt => "<",
        kain_core::ast::BinaryOp::Gt => ">",
        kain_core::ast::BinaryOp::Le => "<=",
        kain_core::ast::BinaryOp::Ge => ">=",
        kain_core::ast::BinaryOp::And => "and",
        kain_core::ast::BinaryOp::Or => "or",
        kain_core::ast::BinaryOp::BitAnd => "&",
        kain_core::ast::BinaryOp::BitOr => "|",
        kain_core::ast::BinaryOp::BitXor => "^",
        kain_core::ast::BinaryOp::Shl => "<<",
        kain_core::ast::BinaryOp::Shr => ">>",
        kain_core::ast::BinaryOp::Assign => "=",
        kain_core::ast::BinaryOp::AddAssign => "+=",
        kain_core::ast::BinaryOp::SubAssign => "-=",
        kain_core::ast::BinaryOp::MulAssign => "*=",
        kain_core::ast::BinaryOp::DivAssign => "/=",
        kain_core::ast::BinaryOp::Range => "..",
        kain_core::ast::BinaryOp::RangeInclusive => "..=",
    }
}

fn unary_op_to_string(op: kain_core::ast::UnaryOp) -> &'static str {
    match op {
        kain_core::ast::UnaryOp::Neg => "-",
        kain_core::ast::UnaryOp::Not => "!",
        kain_core::ast::UnaryOp::BitNot => "~",
        kain_core::ast::UnaryOp::Ref => "&",
        kain_core::ast::UnaryOp::RefMut => "&mut ",
        kain_core::ast::UnaryOp::Deref => "*",
    }
}

fn type_to_string(ty: &kain_core::ast::Type) -> String {
    match ty {
        kain_core::ast::Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                format!(
                    "{}<{}>",
                    name,
                    generics
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
        kain_core::ast::Type::Tuple(types, _) => {
            let types_str = types
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("({})", types_str)
        }
        kain_core::ast::Type::Array(inner, _, _) => {
            format!("Array<{}>", type_to_string(inner))
        }
        kain_core::ast::Type::Slice(inner, _) => {
            format!("Slice<{}>", type_to_string(inner))
        }
        kain_core::ast::Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        kain_core::ast::Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("PtrMut<{}>", type_to_string(inner))
            } else {
                format!("Ptr<{}>", type_to_string(inner))
            }
        }
        kain_core::ast::Type::Function {
            params,
            return_type,
            ..
        } => {
            format!(
                "fn({}) -> {}",
                params
                    .iter()
                    .map(type_to_string)
                    .collect::<Vec<_>>()
                    .join(", "),
                type_to_string(return_type)
            )
        }
        kain_core::ast::Type::Option(inner, _) => {
            format!("Option<{}>", type_to_string(inner))
        }
        kain_core::ast::Type::Result(ok, err, _) => {
            format!("Result<{}, {}>", type_to_string(ok), type_to_string(err))
        }
        kain_core::ast::Type::Infer(_) => "_".to_string(),
        kain_core::ast::Type::Never(_) => "!".to_string(),
        kain_core::ast::Type::Unit(_) => "()".to_string(),
        kain_core::ast::Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                format!(
                    "impl {}<{}>",
                    trait_name,
                    generics
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
    }
}

fn count_functions(program: &kain_core::ast::Program) -> usize {
    program
        .items
        .iter()
        .filter(|item| matches!(item, kain_core::ast::Item::Function(_)))
        .count()
}

fn count_structs(program: &kain_core::ast::Program) -> usize {
    program
        .items
        .iter()
        .filter(|item| matches!(item, kain_core::ast::Item::Struct(_)))
        .count()
}

fn count_enums(program: &kain_core::ast::Program) -> usize {
    program
        .items
        .iter()
        .filter(|item| matches!(item, kain_core::ast::Item::Enum(_)))
        .count()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        fs,
        path::{Path, PathBuf},
    };

    #[test]
    fn rust_import_printer_preserves_tauri_preview_expression_bodies() {
        let source = r#"
            pub async fn fs_read_preview_bytes_impl(
                native_task_graph: &NativeTaskGraphManager,
                preview_streaming: &PreviewStreamingManager,
                path: String,
                max_bytes: Option<u64>,
            ) -> Result<tauri::transport::BinaryResponse, String> {
                let target = PathBuf::from(&path);
                let policy = preview_streaming.policy().clone();
                let bytes = run_native_blocking_task(
                    &native_task_graph,
                    NativeTaskRequest::new(
                        NativeTaskLane::PreviewRead,
                        NativeTaskPriority::Visible,
                        "read preview bytes",
                    )
                    .with_work_key(NativeTaskWorkKey::new(format!("preview-bytes:{path}"))),
                    move |token| read_local_preview_bytes(&target, max_bytes, &policy, &token),
                )
                .await?;
                Ok(tauri::transport::BinaryResponse::new(bytes))
            }

            pub async fn fs_get_home_dir() -> Result<String, String> {
                dirs::home_dir()
                    .map(|p| p.to_string_lossy().to_string())
                    .ok_or_else(|| "Could not determine home directory".to_string())
            }
        "#;
        let (program, diagnostics) =
            kain_import::rust::import_rust_source_detailed(source, Path::new("fs_commands.rs"))
                .expect("rust source should import");
        let generated = generate_kain_source(&program).expect("Kain source should generate");

        assert!(
            diagnostics.is_empty(),
            "unexpected import diagnostics: {diagnostics:?}"
        );
        assert!(
            !generated.contains("unsupported_expr_lowering"),
            "printer should not emit unsupported expression placeholders:\n{generated}"
        );
        assert!(
            !generated.contains("LOSSY LOWERING"),
            "printer should not emit lossy placeholders:\n{generated}"
        );
        assert!(generated.contains("let target = PathBuf__from(&path)"));
        assert!(generated.contains("let policy = preview_streaming.policy().clone()"));
        assert!(generated.contains("await run_native_blocking_task"));
        assert!(generated.contains("BinaryResponse__new_(bytes)"));
        assert!(generated.contains("dirs__home_dir().map"));
    }

    #[test]
    fn blades_output_paths_preserve_relative_rust_layout() {
        assert_eq!(
            kain_output_relative_path_for_source_file(Path::new("src/lib.rs")),
            PathBuf::from("src/lib.kn")
        );
        assert_eq!(
            kain_output_relative_path_for_source_file(Path::new("build.rs")),
            PathBuf::from("build.kn")
        );
        assert_eq!(
            kain_output_relative_path_for_source_file(Path::new("tests/basic.rs")),
            PathBuf::from("tests/basic.kn")
        );
    }

    #[test]
    fn workspace_source_root_detection_prefers_crates_then_rust() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let workspace_root = temp_dir.path();
        fs::create_dir_all(workspace_root.join("rust")).expect("rust dir");
        fs::create_dir_all(workspace_root.join("crates")).expect("crates dir");

        let resolved = resolve_workspace_rust_source_root(workspace_root, None)
            .expect("source root should resolve");
        assert_eq!(resolved, workspace_root.join("crates"));
    }
}
