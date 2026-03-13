use crate::error::{KainError, KainResult};
use serde::Serialize;
use std::collections::HashMap;
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

#[derive(Debug, Default)]
struct ImportRustSummary {
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<(PathBuf, String)>,
}

#[derive(Debug, Serialize)]
struct ImportRustFailureEntry {
    file: String,
    error: String,
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
    generated_kain_path: Option<String>,
    compiled_output_path: Option<String>,
    generated_at_utc: String,
}

/// Import a Rust file into KAIN AST and optionally write/compile
pub fn import_rust(input: &Path, output: Option<&Path>, target: Option<&str>) -> KainResult<()> {
    import_rust_with_batch(input, output, target, &ImportRustBatchOptions::default())
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
        let program = kain_import::import_rust(input)
            .map_err(|e| KainError::runtime(format!("Rust import failed: {}", e)))?;
        return Ok((
            program,
            ImportRustSummary {
                discovered_files: 1,
                imported_files: 1,
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

    let mut candidates = Vec::new();
    collect_rust_files(input, batch.recursive, &mut candidates)?;
    candidates.sort();

    let mut summary = ImportRustSummary {
        discovered_files: candidates.len(),
        ..ImportRustSummary::default()
    };

    let mut merged_items = Vec::new();
    let mut module_name_counts: HashMap<String, usize> = HashMap::new();
    let normalized_includes = normalize_filters(&batch.include_filters);
    let normalized_excludes = normalize_filters(&batch.exclude_filters);

    for file in candidates {
        let rel = file.strip_prefix(input).unwrap_or(file.as_path());
        if !path_matches_filters(rel, &normalized_includes, &normalized_excludes) {
            summary.skipped_files += 1;
            continue;
        }

        match kain_import::import_rust(&file) {
            Ok(program) => {
                summary.imported_files += 1;

                if batch.flat {
                    merged_items.extend(program.items);
                } else {
                    let base_name = sanitize_module_name(rel);
                    let entry = module_name_counts.entry(base_name.clone()).or_insert(0);
                    *entry += 1;
                    let module_name = if *entry == 1 {
                        base_name
                    } else {
                        format!("{base_name}_{}", *entry)
                    };

                    merged_items.push(kain_core::ast::Item::Mod(kain_core::ast::Mod {
                        name: module_name,
                        inline: Some(program.items),
                        visibility: kain_core::ast::Visibility::Public,
                        span: kain_core::span::Span::default(),
                    }));
                }
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

    Ok((
        kain_core::ast::Program {
            items: merged_items,
            span: kain_core::span::Span::default(),
        },
        summary,
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
                error: error.clone(),
            })
            .collect(),
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

    if input.is_dir() && !summary.failed_files.is_empty() {
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

fn sanitize_module_name(path: &Path) -> String {
    let mut parts = Vec::new();
    let mut saw_file_name = false;

    for component in path.components() {
        let raw = component.as_os_str().to_string_lossy();
        let mut part = raw.to_string();

        if !saw_file_name && part.contains('.') {
            part = path
                .file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("module")
                .to_string();
            saw_file_name = true;
        } else if path.file_name().is_some_and(|f| f == component.as_os_str()) {
            part = path
                .file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("module")
                .to_string();
            saw_file_name = true;
        }

        let sanitized = part
            .chars()
            .map(|ch| {
                if ch.is_ascii_alphanumeric() {
                    ch.to_ascii_lowercase()
                } else {
                    '_'
                }
            })
            .collect::<String>();

        if !sanitized.is_empty() {
            parts.push(sanitized);
        }
    }

    let mut name = parts.join("_");
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
        write_line(output, indent + 1, "pass")?;
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
        write_line(output, indent + 1, "pass")?;
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

    write_line(output, indent, &format!("trait {}:", value.name))?;
    if value.methods.is_empty() {
        write_line(output, indent + 1, "pass")?;
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
                write_line(output, indent + 2, "pass")?;
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
        write_line(output, indent + 1, "pass")?;
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
        write_line(output, indent, "pass")?;
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
            write_line(output, indent, &line)
        }
        kain_core::ast::Stmt::Expr(expr) => write_line(output, indent, &expr_to_string(expr)),
        kain_core::ast::Stmt::Return(value, _) => {
            if let Some(expr) = value {
                write_line(output, indent, &format!("return {}", expr_to_string(expr)))
            } else {
                write_line(output, indent, "return")
            }
        }
        _ => write_line(output, indent, "# <stmt>"),
    }
}

fn write_line(output: &mut String, indent: usize, line: &str) -> KainResult<()> {
    use std::fmt::Write;
    writeln!(output, "{}{}", "    ".repeat(indent), line)
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))
}

fn pattern_to_string(pattern: &kain_core::ast::Pattern) -> String {
    match pattern {
        kain_core::ast::Pattern::Wildcard(_) => "_".to_string(),
        kain_core::ast::Pattern::Binding { name, mutable, .. } => {
            if *mutable {
                format!("mut {}", name)
            } else {
                name.clone()
            }
        }
        _ => "_".to_string(),
    }
}

fn expr_to_string(expr: &kain_core::ast::Expr) -> String {
    match expr {
        kain_core::ast::Expr::Int(value, _) => value.to_string(),
        kain_core::ast::Expr::Float(value, _) => value.to_string(),
        kain_core::ast::Expr::String(value, _) => format!("{:?}", value),
        kain_core::ast::Expr::Bool(value, _) => value.to_string(),
        kain_core::ast::Expr::None(_) => "none".to_string(),
        kain_core::ast::Expr::Ident(name, _) => name.clone(),
        _ => "# <expr>".to_string(),
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
