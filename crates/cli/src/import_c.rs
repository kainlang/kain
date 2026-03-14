use crate::error::{KainError, KainResult};
use serde::Serialize;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct ImportCBatchOptions {
    pub recursive: bool,
    pub flat: bool,
    pub include_filters: Vec<String>,
    pub exclude_filters: Vec<String>,
    pub fail_fast: bool,
    pub report_json: Option<PathBuf>,
}

impl Default for ImportCBatchOptions {
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
struct ImportCSummary {
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<(PathBuf, String)>,
}

#[derive(Debug, Serialize)]
struct ImportCFailureEntry {
    file: String,
    error: String,
}

#[derive(Debug, Serialize)]
struct ImportCFailureReport {
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
    failed_files: Vec<ImportCFailureEntry>,
    generated_kain_path: Option<String>,
    compiled_output_path: Option<String>,
    generated_at_utc: String,
}

/// Import a C file into KAIN AST and optionally write/compile
pub fn import_c(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    include_paths: &[String],
    defines: &[String],
) -> KainResult<()> {
    import_c_with_batch(
        input,
        output,
        target,
        include_paths,
        defines,
        &ImportCBatchOptions::default(),
    )
}

/// Import a C file or directory into KAIN AST and optionally write/compile.
pub fn import_c_with_batch(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    include_paths: &[String],
    defines: &[String],
    batch: &ImportCBatchOptions,
) -> KainResult<()> {
    let options = kain_import::c::CImportOptions {
        include_paths: include_paths.to_vec(),
        defines: defines.to_vec(),
        cpp_options: Vec::new(),
        cpp_command: None,
    };

    // Directory imports always materialize a single .kn artifact by default.
    let resolved_output: Option<PathBuf> = match output {
        Some(path) => Some(path.to_path_buf()),
        None if input.is_dir() => Some(input.with_extension("kn")),
        None => None,
    };

    let (program, summary) = import_path_to_program(input, &options, batch)?;

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
            " Generated KAIN source: {} ({} bytes)",
            out_path.display(),
            kain_source.len()
        );
    }

    // If target specified, compile directly
    let mut compiled_output_path: Option<PathBuf> = None;
    if let Some(target_str) = target {
        let compile_target = crate::parse_compile_target(target_str)
            .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;

        println!(" Compiling to target: {}", target_str);

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
            " Compiled output: {} ({} bytes)",
            compiled_output.display(),
            compiled.len()
        );
    }

    // Print summary
    println!(" Import complete");
    println!(" Functions: {}", count_functions(&program));
    println!(" Structs: {}", count_structs(&program));
    if input.is_dir() {
        println!(
            " C files: discovered {}, imported {}, skipped {}, failed {}",
            summary.discovered_files,
            summary.imported_files,
            summary.skipped_files,
            summary.failed_files.len()
        );

        if !summary.failed_files.is_empty() {
            println!(" Failed files:");
            for (path, error) in summary.failed_files.iter().take(20) {
                println!("   - {}: {}", path.display(), error);
            }
            if summary.failed_files.len() > 20 {
                println!("   ... {} more", summary.failed_files.len() - 20);
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
    options: &kain_import::c::CImportOptions,
    batch: &ImportCBatchOptions,
) -> KainResult<(kain_core::ast::Program, ImportCSummary)> {
    if input.is_file() {
        let program = kain_import::c::import_c_file_with_options(input, options)
            .map_err(|e| KainError::runtime(format!("C import failed: {}", e)))?;
        return Ok((
            program,
            ImportCSummary {
                discovered_files: 1,
                imported_files: 1,
                ..ImportCSummary::default()
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
    collect_c_files(input, batch.recursive, &mut candidates)?;
    candidates.sort();

    let mut summary = ImportCSummary {
        discovered_files: candidates.len(),
        ..ImportCSummary::default()
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

        match kain_import::c::import_c_file_with_options(&file, options) {
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
                        visibility: kain_core::ast::Visibility::Private,
                        span: kain_core::span::Span::default(),
                    }));
                }
            }
            Err(err) => {
                let compact = compact_error_message(&format!("{}", err));
                summary.failed_files.push((file.clone(), compact));
                if batch.fail_fast {
                    return Err(KainError::runtime(format!(
                        "C import failed: {}: {}",
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

fn build_no_import_detail(summary: &ImportCSummary) -> String {
    if summary.failed_files.is_empty() {
        return "No C source files matched include/exclude filters".to_string();
    }

    let mut msg = String::from("All matching C files failed to import");
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
    batch: &ImportCBatchOptions,
    summary: &ImportCSummary,
    generated_kain_path: Option<&Path>,
    compiled_output_path: Option<&Path>,
) -> KainResult<()> {
    let Some(report_path) = resolve_report_path(input, output, batch, summary) else {
        return Ok(());
    };

    let report = ImportCFailureReport {
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
            .map(|(path, error)| ImportCFailureEntry {
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

    println!(" Failure report JSON: {}", report_path.display());
    Ok(())
}

fn resolve_report_path(
    input: &Path,
    output: Option<&Path>,
    batch: &ImportCBatchOptions,
    summary: &ImportCSummary,
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

fn collect_c_files(root: &Path, recursive: bool, out: &mut Vec<PathBuf>) -> KainResult<()> {
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
                collect_c_files(&path, recursive, out)?;
            }
            continue;
        }

        if path
            .extension()
            .and_then(|ext| ext.to_str())
            .is_some_and(|ext| ext.eq_ignore_ascii_case("c"))
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
    writeln!(output, "# Generated from C source by kain import-c")
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

fn write_block(
    output: &mut String,
    block: &kain_core::ast::Block,
    indent: usize,
) -> KainResult<()> {
    if block.stmts.is_empty() {
        write_line(output, indent, "none")?;
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
            } else {
                line.push_str(" = none");
            }
            write_line(output, indent, &line)
        }
        kain_core::ast::Stmt::Expr(expr) => {
            if let Some(block) = desugar_sequence_stmt(expr) {
                return write_block(output, &block, indent);
            }
            if let kain_core::ast::Expr::Block(block, _) = expr {
                return write_block(output, block, indent);
            }
            if let kain_core::ast::Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } = expr
            {
                return write_if_expr_stmt(
                    output,
                    condition,
                    then_branch,
                    else_branch.as_deref(),
                    indent,
                );
            }
            write_line(output, indent, &expr_to_string(expr))
        }
        kain_core::ast::Stmt::Return(value, _) => {
            if let Some(expr) = value {
                write_line(output, indent, &format!("return {}", expr_to_string(expr)))
            } else {
                write_line(output, indent, "return")
            }
        }
        kain_core::ast::Stmt::Break(value, _) => {
            if let Some(expr) = value {
                write_line(output, indent, &format!("break {}", expr_to_string(expr)))
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
        } => {
            write_line(
                output,
                indent,
                &format!(
                    "for {} in {}:",
                    pattern_to_string(binding),
                    expr_to_string(iter)
                ),
            )?;
            write_block(output, body, indent + 1)
        }
        kain_core::ast::Stmt::While {
            condition, body, ..
        } => {
            write_line(
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
        // Nested items are not emitted by the C importer today.
        kain_core::ast::Stmt::Item(_) => write_line(output, indent, "none"),
    }
}

fn write_if_expr_stmt(
    output: &mut String,
    condition: &kain_core::ast::Expr,
    then_branch: &kain_core::ast::Block,
    else_branch: Option<&kain_core::ast::ElseBranch>,
    indent: usize,
) -> KainResult<()> {
    write_line(
        output,
        indent,
        &format!("if {}:", expr_to_string(condition)),
    )?;
    write_block(output, then_branch, indent + 1)?;
    write_else_branch(output, else_branch, indent)
}

fn write_else_branch(
    output: &mut String,
    else_branch: Option<&kain_core::ast::ElseBranch>,
    indent: usize,
) -> KainResult<()> {
    match else_branch {
        Some(kain_core::ast::ElseBranch::Else(block)) => {
            write_line(output, indent, "else:")?;
            write_block(output, block, indent + 1)
        }
        Some(kain_core::ast::ElseBranch::ElseIf(cond, block, next)) => {
            write_line(output, indent, &format!("elif {}:", expr_to_string(cond)))?;
            write_block(output, block, indent + 1)?;
            write_else_branch(output, next.as_deref(), indent)
        }
        None => Ok(()),
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
            let mut rendered = fields
                .iter()
                .map(|(field, pat)| format!("{field}: {}", pattern_to_string(pat)))
                .collect::<Vec<_>>();
            if *rest {
                rendered.push("..".to_string());
            }
            format!("{name} {{ {} }}", rendered.join(", "))
        }
        kain_core::ast::Pattern::Tuple(items, _) => {
            let body = items
                .iter()
                .map(pattern_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            if items.len() == 1 {
                format!("({body},)")
            } else {
                format!("({body})")
            }
        }
        kain_core::ast::Pattern::Variant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let head = match enum_name {
                Some(name) => format!("{name}::{variant}"),
                None => variant.clone(),
            };
            match fields {
                kain_core::ast::VariantPatternFields::Unit => head,
                kain_core::ast::VariantPatternFields::Tuple(items) => {
                    let body = items
                        .iter()
                        .map(pattern_to_string)
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{head}({body})")
                }
                kain_core::ast::VariantPatternFields::Struct(fields) => {
                    let body = fields
                        .iter()
                        .map(|(name, pat)| format!("{name}: {}", pattern_to_string(pat)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{head} {{ {body} }}")
                }
            }
        }
        kain_core::ast::Pattern::Slice { patterns, rest, .. } => {
            let mut parts = patterns.iter().map(pattern_to_string).collect::<Vec<_>>();
            if let Some(rest_name) = rest {
                parts.push(format!("{rest_name} @ .."));
            }
            format!("[{}]", parts.join(", "))
        }
        kain_core::ast::Pattern::Or(items, _) => items
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
            let left = start
                .as_ref()
                .map(|expr| expr_to_string(expr))
                .unwrap_or_default();
            let right = end
                .as_ref()
                .map(|expr| expr_to_string(expr))
                .unwrap_or_default();
            let op = if *inclusive { "..=" } else { ".." };
            format!("{left}{op}{right}")
        }
    }
}

fn expr_to_string(expr: &kain_core::ast::Expr) -> String {
    match expr {
        kain_core::ast::Expr::Int(value, _) => value.to_string(),
        kain_core::ast::Expr::Float(value, _) => format_float(*value),
        kain_core::ast::Expr::String(value, _) => format!("{value:?}"),
        kain_core::ast::Expr::FString(parts, _) => parts
            .iter()
            .map(expr_to_string)
            .collect::<Vec<_>>()
            .join(" + "),
        kain_core::ast::Expr::Bool(value, _) => value.to_string(),
        kain_core::ast::Expr::None(_) => "none".to_string(),
        kain_core::ast::Expr::Ident(name, _) => name.clone(),
        kain_core::ast::Expr::MacroCall { name, args, .. } => {
            let args = args
                .iter()
                .map(expr_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("{name}!({args})")
        }
        kain_core::ast::Expr::Binary {
            left, op, right, ..
        } => format!(
            "({} {} {})",
            expr_to_string(left),
            binary_op_to_string(*op),
            expr_to_string(right)
        ),
        kain_core::ast::Expr::Unary { op, operand, .. } => {
            format!("({}{})", unary_op_to_string(*op), expr_to_string(operand))
        }
        kain_core::ast::Expr::Call { callee, args, .. } => {
            let args = args
                .iter()
                .map(call_arg_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("{}({args})", expr_to_string(callee))
        }
        kain_core::ast::Expr::MethodCall {
            receiver,
            method,
            args,
            ..
        } => {
            let args = args
                .iter()
                .map(call_arg_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("{}.{}({args})", expr_to_string(receiver), method)
        }
        kain_core::ast::Expr::Field { object, field, .. } => {
            format!("{}.{}", expr_to_string(object), field)
        }
        kain_core::ast::Expr::Index { object, index, .. } => {
            format!("{}[{}]", expr_to_string(object), expr_to_string(index))
        }
        kain_core::ast::Expr::Assign { target, value, .. } => {
            format!("({} = {})", expr_to_string(target), expr_to_string(value))
        }
        // Struct literals are currently not supported by the parser; emit a constructor-shaped fallback.
        kain_core::ast::Expr::Struct { name, .. } => format!("{name}()"),
        kain_core::ast::Expr::AggregateInit {
            ty,
            fields,
            zero_fill_rest,
            ..
        } => {
            let mut args = vec![
                format!("\"{}\"", type_to_string(ty)),
                zero_fill_rest.to_string(),
            ];
            args.extend(
                fields
                    .iter()
                    .map(|(name, value)| format!("{name} = {}", expr_to_string(value))),
            );
            format!("aggregate_init({})", args.join(", "))
        }
        kain_core::ast::Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let head = format!("{enum_name}::{variant}");
            match fields {
                kain_core::ast::EnumVariantFields::Unit => head,
                kain_core::ast::EnumVariantFields::Tuple(items) => {
                    let args = items
                        .iter()
                        .map(expr_to_string)
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{head}({args})")
                }
                kain_core::ast::EnumVariantFields::Struct(fields) => {
                    let fields = fields
                        .iter()
                        .map(|(name, value)| format!("{name}: {}", expr_to_string(value)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{head} {{ {fields} }}")
                }
            }
        }
        kain_core::ast::Expr::Array(items, _) => {
            let body = items
                .iter()
                .map(expr_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("[{body}]")
        }
        kain_core::ast::Expr::Tuple(items, _) => {
            if items.is_empty() {
                "()".to_string()
            } else {
                let body = items
                    .iter()
                    .map(expr_to_string)
                    .collect::<Vec<_>>()
                    .join(", ");
                if items.len() == 1 {
                    format!("({body},)")
                } else {
                    format!("({body})")
                }
            }
        }
        kain_core::ast::Expr::Range {
            start,
            end,
            inclusive,
            ..
        } => {
            let left = start
                .as_ref()
                .map(|expr| expr_to_string(expr))
                .unwrap_or_default();
            let right = end
                .as_ref()
                .map(|expr| expr_to_string(expr))
                .unwrap_or_default();
            let op = if *inclusive { "..=" } else { ".." };
            format!("{left}{op}{right}")
        }
        kain_core::ast::Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => inline_if_expr(condition, then_branch, else_branch.as_deref()),
        kain_core::ast::Expr::Match { .. } => "none".to_string(),
        kain_core::ast::Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => {
            let params = params
                .iter()
                .map(|param| format!("{}: {}", param.name, type_to_string(&param.ty)))
                .collect::<Vec<_>>()
                .join(", ");
            let ret = return_type
                .as_ref()
                .map(|ty| format!(" -> {}", type_to_string(ty)))
                .unwrap_or_default();
            format!("(fn({params}){ret}: {})", expr_to_string(body))
        }
        kain_core::ast::Expr::Ref { mutable, value, .. } => {
            if *mutable {
                format!("(&mut {})", expr_to_string(value))
            } else {
                format!("(&{})", expr_to_string(value))
            }
        }
        kain_core::ast::Expr::AddrOf {
            value, pointee_ty, ..
        } => match pointee_ty {
            Some(ty) => format!(
                "addr_of({}, \"{}\")",
                expr_to_string(value),
                type_to_string(ty)
            ),
            None => format!("addr_of({})", expr_to_string(value)),
        },
        kain_core::ast::Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => {
            let base = format!(
                "ptr_offset({}, {})",
                expr_to_string(pointer),
                expr_to_string(offset)
            );
            match element_ty {
                Some(ty) => format!(
                    "ptr_offset({}, {}, \"{}\")",
                    expr_to_string(pointer),
                    expr_to_string(offset),
                    type_to_string(ty)
                ),
                None => base,
            }
        }
        kain_core::ast::Expr::MemLoad {
            pointer, load_ty, ..
        } => match load_ty {
            Some(ty) => format!(
                "mem_load({}, \"{}\")",
                expr_to_string(pointer),
                type_to_string(ty)
            ),
            None => format!("mem_load({})", expr_to_string(pointer)),
        },
        kain_core::ast::Expr::MemStore {
            pointer,
            value,
            store_ty,
            ..
        } => match store_ty {
            Some(ty) => format!(
                "mem_store({}, {}, \"{}\")",
                expr_to_string(pointer),
                expr_to_string(value),
                type_to_string(ty)
            ),
            None => format!(
                "mem_store({}, {})",
                expr_to_string(pointer),
                expr_to_string(value)
            ),
        },
        kain_core::ast::Expr::SizeOfType { target, .. } => {
            format!("sizeof_type(\"{}\")", type_to_string(target))
        }
        kain_core::ast::Expr::AlignOfType { target, .. } => {
            format!("alignof_type(\"{}\")", type_to_string(target))
        }
        kain_core::ast::Expr::Alloca { ty, .. } => {
            format!("alloca(\"{}\")", type_to_string(ty))
        }
        kain_core::ast::Expr::Uninit { ty, .. } => {
            format!("uninit(\"{}\")", type_to_string(ty))
        }
        kain_core::ast::Expr::Alloc {
            size, ty, zeroed, ..
        } => {
            let name = if *zeroed { "alloc_zeroed" } else { "alloc" };
            match ty {
                Some(ty) => format!(
                    "{}({}, \"{}\")",
                    name,
                    expr_to_string(size),
                    type_to_string(ty)
                ),
                None => format!("{}({})", name, expr_to_string(size)),
            }
        }
        kain_core::ast::Expr::Realloc {
            pointer, size, ty, ..
        } => match ty {
            Some(ty) => format!(
                "realloc_mem({}, {}, \"{}\")",
                expr_to_string(pointer),
                expr_to_string(size),
                type_to_string(ty)
            ),
            None => format!(
                "realloc_mem({}, {})",
                expr_to_string(pointer),
                expr_to_string(size)
            ),
        },
        kain_core::ast::Expr::Deref(value, _) => format!("(*{})", expr_to_string(value)),
        kain_core::ast::Expr::Cast { value, target, .. } => {
            format!("({} as {})", expr_to_string(value), type_to_string(target))
        }
        kain_core::ast::Expr::Try(value, _) => format!("({}?)", expr_to_string(value)),
        kain_core::ast::Expr::Await(value, _) => format!("(await {})", expr_to_string(value)),
        kain_core::ast::Expr::AsyncBlock(value, _) => {
            format!("(async {})", expr_to_string(value))
        }
        kain_core::ast::Expr::Spawn { actor, init, .. } => {
            let args = init
                .iter()
                .map(|(name, value)| format!("{name} = {}", expr_to_string(value)))
                .collect::<Vec<_>>()
                .join(", ");
            format!("spawn {actor}({args})")
        }
        kain_core::ast::Expr::SendMsg {
            target,
            message,
            data,
            ..
        } => {
            let args = data
                .iter()
                .map(|(name, value)| format!("{name} = {}", expr_to_string(value)))
                .collect::<Vec<_>>()
                .join(", ");
            format!("send {}.{}({args})", expr_to_string(target), message)
        }
        kain_core::ast::Expr::Comptime(_, _) => "none".to_string(),
        kain_core::ast::Expr::Block(block, _) => {
            if let Some(expr) = single_expr_from_block(block) {
                expr_to_string(expr)
            } else {
                "none".to_string()
            }
        }
        kain_core::ast::Expr::JSX(_, _) => "none".to_string(),
        kain_core::ast::Expr::Paren(value, _) => format!("({})", expr_to_string(value)),
        kain_core::ast::Expr::Return(value, _) => match value {
            Some(expr) => format!("return {}", expr_to_string(expr)),
            None => "return".to_string(),
        },
        kain_core::ast::Expr::Break(value, _) => match value {
            Some(expr) => format!("break {}", expr_to_string(expr)),
            None => "break".to_string(),
        },
        kain_core::ast::Expr::Continue(_) => "continue".to_string(),
    }
}

fn inline_if_expr(
    condition: &kain_core::ast::Expr,
    then_branch: &kain_core::ast::Block,
    else_branch: Option<&kain_core::ast::ElseBranch>,
) -> String {
    let then_expr = single_expr_from_block(then_branch)
        .map(expr_to_string)
        .unwrap_or_else(|| "none".to_string());
    let mut rendered = format!("if {}: {}", expr_to_string(condition), then_expr);

    if let Some(branch) = else_branch {
        rendered.push(' ');
        rendered.push_str(&inline_else_branch(branch));
    }

    rendered
}

fn inline_else_branch(else_branch: &kain_core::ast::ElseBranch) -> String {
    match else_branch {
        kain_core::ast::ElseBranch::Else(block) => {
            let value = single_expr_from_block(block)
                .map(expr_to_string)
                .unwrap_or_else(|| "none".to_string());
            format!("else: {value}")
        }
        kain_core::ast::ElseBranch::ElseIf(cond, block, next) => {
            let value = single_expr_from_block(block)
                .map(expr_to_string)
                .unwrap_or_else(|| "none".to_string());
            let mut rendered = format!("elif {}: {}", expr_to_string(cond), value);
            if let Some(next) = next.as_deref() {
                rendered.push(' ');
                rendered.push_str(&inline_else_branch(next));
            }
            rendered
        }
    }
}

fn single_expr_from_block(block: &kain_core::ast::Block) -> Option<&kain_core::ast::Expr> {
    if block.stmts.len() != 1 {
        return None;
    }
    match block.stmts.first() {
        Some(kain_core::ast::Stmt::Expr(expr)) => Some(expr),
        Some(kain_core::ast::Stmt::Return(Some(expr), _)) => Some(expr),
        _ => None,
    }
}

struct IncDecSequence<'a> {
    binding: &'a str,
    target: &'a kain_core::ast::Expr,
    op: kain_core::ast::BinaryOp,
    prefix: bool,
}

fn desugar_sequence_stmt(expr: &kain_core::ast::Expr) -> Option<kain_core::ast::Block> {
    let span = expr.span();

    if let Some(sequence) = decode_incdec_sequence(expr) {
        return Some(kain_core::ast::Block {
            stmts: vec![kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Assign {
                target: Box::new(sequence.target.clone()),
                value: Box::new(kain_core::ast::Expr::Binary {
                    left: Box::new(sequence.target.clone()),
                    op: sequence.op,
                    right: Box::new(kain_core::ast::Expr::Int(1, span)),
                    span,
                }),
                span,
            })],
            span,
        });
    }

    if let kain_core::ast::Expr::MemStore {
        pointer,
        value,
        store_ty,
        ..
    } = expr
    {
        let kain_core::ast::Expr::PtrOffset {
            pointer: base,
            offset,
            element_ty,
            ..
        } = &**pointer
        else {
            return None;
        };
        let sequence = decode_incdec_sequence(offset)?;

        let mut stmts = Vec::new();
        if !sequence.prefix {
            stmts.push(kain_core::ast::Stmt::Let {
                pattern: kain_core::ast::Pattern::Binding {
                    name: sequence.binding.to_string(),
                    mutable: true,
                    span,
                },
                ty: None,
                value: Some(sequence.target.clone()),
                span,
            });
        }

        stmts.push(kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Assign {
            target: Box::new(sequence.target.clone()),
            value: Box::new(kain_core::ast::Expr::Binary {
                left: Box::new(sequence.target.clone()),
                op: sequence.op,
                right: Box::new(kain_core::ast::Expr::Int(1, span)),
                span,
            }),
            span,
        }));

        let lowered_index = if sequence.prefix {
            sequence.target.clone()
        } else {
            kain_core::ast::Expr::Ident(sequence.binding.to_string(), span)
        };

        stmts.push(kain_core::ast::Stmt::Expr(kain_core::ast::Expr::MemStore {
            pointer: Box::new(kain_core::ast::Expr::PtrOffset {
                pointer: base.clone(),
                offset: Box::new(lowered_index),
                element_ty: element_ty.clone(),
                span,
            }),
            value: value.clone(),
            store_ty: store_ty.clone(),
            span,
        }));

        return Some(kain_core::ast::Block { stmts, span });
    }

    let kain_core::ast::Expr::Assign { target, value, .. } = expr else {
        return None;
    };
    let kain_core::ast::Expr::Index { object, index, .. } = &**target else {
        return None;
    };
    let sequence = decode_incdec_sequence(index)?;

    let mut stmts = Vec::new();
    if !sequence.prefix {
        stmts.push(kain_core::ast::Stmt::Let {
            pattern: kain_core::ast::Pattern::Binding {
                name: sequence.binding.to_string(),
                mutable: true,
                span,
            },
            ty: None,
            value: Some(sequence.target.clone()),
            span,
        });
    }

    stmts.push(kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Assign {
        target: Box::new(sequence.target.clone()),
        value: Box::new(kain_core::ast::Expr::Binary {
            left: Box::new(sequence.target.clone()),
            op: sequence.op,
            right: Box::new(kain_core::ast::Expr::Int(1, span)),
            span,
        }),
        span,
    }));

    let lowered_index = if sequence.prefix {
        sequence.target.clone()
    } else {
        kain_core::ast::Expr::Ident(sequence.binding.to_string(), span)
    };

    stmts.push(kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Assign {
        target: Box::new(kain_core::ast::Expr::Index {
            object: object.clone(),
            index: Box::new(lowered_index),
            span,
        }),
        value: value.clone(),
        span,
    }));

    Some(kain_core::ast::Block { stmts, span })
}

fn decode_incdec_sequence(expr: &kain_core::ast::Expr) -> Option<IncDecSequence<'_>> {
    let kain_core::ast::Expr::Match {
        scrutinee, arms, ..
    } = expr
    else {
        return None;
    };
    let [arm] = arms.as_slice() else {
        return None;
    };
    let kain_core::ast::Pattern::Binding { name, .. } = &arm.pattern else {
        return None;
    };
    let kain_core::ast::Expr::Index { object, index, .. } = &arm.body else {
        return None;
    };
    let kain_core::ast::Expr::Array(items, _) = &**object else {
        return None;
    };
    let [assign_expr, result_expr] = items.as_slice() else {
        return None;
    };
    let kain_core::ast::Expr::Int(1, _) = &**index else {
        return None;
    };
    let kain_core::ast::Expr::Assign { target, value, .. } = assign_expr else {
        return None;
    };
    if **target != **scrutinee {
        return None;
    }
    let kain_core::ast::Expr::Binary {
        left, op, right, ..
    } = &**value
    else {
        return None;
    };
    let kain_core::ast::Expr::Ident(left_name, _) = &**left else {
        return None;
    };
    if left_name != name {
        return None;
    }
    if !matches!(&**right, kain_core::ast::Expr::Int(1, _)) {
        return None;
    }

    let prefix = matches!(
        result_expr,
        kain_core::ast::Expr::Binary {
            left,
            op: result_op,
            right,
            ..
        } if matches!(&**left, kain_core::ast::Expr::Ident(result_name, _) if result_name == name)
            && result_op == op
            && matches!(&**right, kain_core::ast::Expr::Int(1, _))
    );
    let postfix =
        matches!(result_expr, kain_core::ast::Expr::Ident(result_name, _) if result_name == name);
    if !prefix && !postfix {
        return None;
    }

    Some(IncDecSequence {
        binding: name,
        target: scrutinee,
        op: *op,
        prefix,
    })
}

fn call_arg_to_string(arg: &kain_core::ast::CallArg) -> String {
    match &arg.name {
        Some(name) => format!("{name} = {}", expr_to_string(&arg.value)),
        None => expr_to_string(&arg.value),
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
        kain_core::ast::BinaryOp::And => "&&",
        kain_core::ast::BinaryOp::Or => "||",
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

fn format_float(value: f64) -> String {
    if !value.is_finite() {
        return "0.0".to_string();
    }

    let rendered = value.to_string();
    if rendered.contains('.') || rendered.contains('e') || rendered.contains('E') {
        rendered
    } else {
        format!("{rendered}.0")
    }
}

fn write_struct(output: &mut String, s: &kain_core::ast::Struct, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", s.name))?;

    for field in &s.fields {
        write_line(
            output,
            indent + 1,
            &format!("{}: {}", field.name, type_to_string(&field.ty)),
        )?;
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;

    Ok(())
}

fn write_enum(output: &mut String, e: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", e.name))?;

    for variant in &e.variants {
        write_line(output, indent + 1, &variant.name)?;
    }

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;

    Ok(())
}

fn type_to_string(ty: &kain_core::ast::Type) -> String {
    match ty {
        kain_core::ast::Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                let args = generics
                    .iter()
                    .map(type_to_string)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}<{}>", name, args)
            }
        }
        kain_core::ast::Type::Tuple(types, _) => {
            let members = types
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("({})", members)
        }
        kain_core::ast::Type::Array(inner, size, _) => {
            format!("[{}; {}]", type_to_string(inner), size)
        }
        kain_core::ast::Type::Slice(inner, _) => format!("[{}]", type_to_string(inner)),
        kain_core::ast::Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        kain_core::ast::Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", type_to_string(inner))
            } else {
                format!("ptr<{}>", type_to_string(inner))
            }
        }
        kain_core::ast::Type::Function {
            params,
            return_type,
            ..
        } => {
            let args = params
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ");
            format!("fn({}) -> {}", args, type_to_string(return_type))
        }
        kain_core::ast::Type::Option(inner, _) => format!("{}?", type_to_string(inner)),
        kain_core::ast::Type::Result(ok, err, _) => {
            format!("{}!{}", type_to_string(ok), type_to_string(err))
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
                let args = generics
                    .iter()
                    .map(type_to_string)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("impl {}<{}>", trait_name, args)
            }
        }
    }
}

fn count_functions(program: &kain_core::ast::Program) -> usize {
    count_functions_in_items(&program.items)
}

fn count_structs(program: &kain_core::ast::Program) -> usize {
    count_structs_in_items(&program.items)
}

fn count_functions_in_items(items: &[kain_core::ast::Item]) -> usize {
    let mut total = 0;
    for item in items {
        match item {
            kain_core::ast::Item::Function(_) => total += 1,
            kain_core::ast::Item::Mod(module) => {
                if let Some(children) = &module.inline {
                    total += count_functions_in_items(children);
                }
            }
            _ => {}
        }
    }
    total
}

fn count_structs_in_items(items: &[kain_core::ast::Item]) -> usize {
    let mut total = 0;
    for item in items {
        match item {
            kain_core::ast::Item::Struct(_) => total += 1,
            kain_core::ast::Item::Mod(module) => {
                if let Some(children) = &module.inline {
                    total += count_structs_in_items(children);
                }
            }
            _ => {}
        }
    }
    total
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    use tempfile::{Builder, NamedTempFile, TempDir};

    #[test]
    fn test_import_simple_c_file() {
        // Create a temporary C file
        let mut temp_file = NamedTempFile::new().unwrap();
        writeln!(temp_file, "int add(int a, int b) {{").unwrap();
        writeln!(temp_file, "    return a + b;").unwrap();
        writeln!(temp_file, "}}").unwrap();
        temp_file.flush().unwrap();

        // Import it
        let result = import_c(temp_file.path(), None, None, &[], &[]);

        // Should succeed
        assert!(result.is_ok());
    }

    #[test]
    fn test_import_with_output() {
        // Create a temporary C file with .c suffix so tooling paths treat it as C.
        let mut temp_c = Builder::new().suffix(".c").tempfile().unwrap();
        writeln!(temp_c, "int multiply(int x, int y) {{").unwrap();
        writeln!(temp_c, "    return x * y;").unwrap();
        writeln!(temp_c, "}}").unwrap();
        temp_c.flush().unwrap();

        // Create output path
        let temp_out = NamedTempFile::new().unwrap();
        let out_path = temp_out.path();

        // Import with output
        let result = import_c(temp_c.path(), Some(out_path), None, &[], &[]);

        // Should succeed and create output file
        assert!(result.is_ok());
        assert!(out_path.exists());

        // Output should contain KAIN code
        let content = fs::read_to_string(out_path).unwrap();
        assert!(content.contains("fn multiply"));
        assert!(content.contains("return (x * y)"));
    }

    #[test]
    fn test_import_with_target() {
        // Create a temporary C file
        let mut temp_c = NamedTempFile::new().unwrap();
        writeln!(temp_c, "int square(int n) {{").unwrap();
        writeln!(temp_c, "    return n * n;").unwrap();
        writeln!(temp_c, "}}").unwrap();
        temp_c.flush().unwrap();

        // Import with wasm target
        let result = import_c(temp_c.path(), None, Some("wasm"), &[], &[]);

        // Should succeed
        assert!(result.is_ok());
    }

    #[test]
    fn test_import_directory_to_single_file_with_modules() {
        let temp = TempDir::new().unwrap();
        let root = temp.path();
        fs::create_dir_all(root.join("sub")).unwrap();

        fs::write(root.join("alpha.c"), "int alpha(void) { return 1; }\n").unwrap();
        fs::write(
            root.join("sub").join("beta.c"),
            "int beta(void) { return 2; }\n",
        )
        .unwrap();

        let out_path = root.join("all.kn");
        let batch = ImportCBatchOptions::default();
        let result = import_c_with_batch(root, Some(&out_path), None, &[], &[], &batch);
        assert!(result.is_ok());

        let content = fs::read_to_string(&out_path).unwrap();
        assert!(content.contains("mod alpha:"));
        assert!(content.contains("mod sub_beta:"));
        assert!(content.contains("fn alpha"));
        assert!(content.contains("fn beta"));
    }

    #[test]
    fn test_import_empty_module_does_not_emit_none_item() {
        let temp = TempDir::new().unwrap();
        let root = temp.path();
        fs::write(root.join("empty.c"), "/* intentionally empty */\n").unwrap();

        let out_path = root.join("empty_out.kn");
        let batch = ImportCBatchOptions::default();
        let result = import_c_with_batch(root, Some(&out_path), None, &[], &[], &batch);
        assert!(result.is_ok());

        let content = fs::read_to_string(&out_path).unwrap();
        assert!(content.contains("mod empty:"));
        assert!(!content.contains("\n    none\n"));
    }

    #[test]
    fn test_import_directory_with_exclude_filter() {
        let temp = TempDir::new().unwrap();
        let root = temp.path();

        fs::write(root.join("keep.c"), "int keep(void) { return 1; }\n").unwrap();
        fs::write(
            root.join("skip_sound.c"),
            "int skip_sound(void) { return 0; }\n",
        )
        .unwrap();

        let out_path = root.join("filtered.kn");
        let mut batch = ImportCBatchOptions::default();
        batch.exclude_filters = vec!["sound".to_string()];
        let result = import_c_with_batch(root, Some(&out_path), None, &[], &[], &batch);
        assert!(result.is_ok());

        let content = fs::read_to_string(&out_path).unwrap();
        assert!(content.contains("fn keep"));
        assert!(!content.contains("skip_sound"));
    }

    #[test]
    fn test_import_directory_writes_failure_report_json() {
        let temp = TempDir::new().unwrap();
        let root = temp.path();

        fs::write(root.join("ok.c"), "int ok(void) { return 1; }\n").unwrap();
        fs::write(root.join("bad.c"), vec![0xFF, 0xFE, 0x00]).unwrap();

        let out_path = root.join("mixed.kn");
        let report_path = root.join("mixed_failures.json");
        let mut batch = ImportCBatchOptions::default();
        batch.report_json = Some(report_path.clone());

        let result = import_c_with_batch(root, Some(&out_path), None, &[], &[], &batch);
        assert!(result.is_ok());
        assert!(report_path.exists());

        let report = fs::read_to_string(&report_path).unwrap();
        let json: serde_json::Value = serde_json::from_str(&report).unwrap();
        assert_eq!(json["imported_files"].as_u64(), Some(1));
        assert_eq!(json["failed_files"].as_array().map(|v| v.len()), Some(1));
    }

    #[test]
    fn test_self_hosting_runtime_import_compiles_to_ts() {
        let workspace_root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .ancestors()
            .nth(3)
            .expect("workspace root");
        let input = workspace_root
            .join("Other")
            .join("kainselfhosting")
            .join("runtime")
            .join("kain_runtime_clean.c");
        assert!(
            input.exists(),
            "missing self-hosting runtime sample: {}",
            input.display()
        );

        let temp = TempDir::new().unwrap();
        let out_path = temp.path().join("clean.kn");

        import_c(input.as_path(), Some(&out_path), None, &[], &[]).unwrap();

        let kain_source = fs::read_to_string(&out_path).unwrap();
        assert!(kain_source.contains("fn kain_str_new"));

        let ts_target = crate::parse_compile_target("ts").expect("ts target");
        let compiled = crate::compile(&kain_source, ts_target).unwrap();
        assert!(!compiled.trim().is_empty());
    }
}
