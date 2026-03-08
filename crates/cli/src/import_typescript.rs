use crate::error::{KainError, KainResult};
use serde::Serialize;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct ImportTypeScriptBatchOptions {
    pub recursive: bool,
    pub flat: bool,
    pub include_filters: Vec<String>,
    pub exclude_filters: Vec<String>,
    pub fail_fast: bool,
    pub report_json: Option<PathBuf>,
}

#[derive(Debug, Clone)]
struct ImportDiscoveryPolicy {
    ignored_dir_names: &'static [&'static str],
    ignored_file_name_fragments: &'static [&'static str],
    allowed_extensions: &'static [&'static str],
}

impl Default for ImportDiscoveryPolicy {
    fn default() -> Self {
        Self {
            ignored_dir_names: &["_out", "_single_out", "_batch_out", "dist", "build", "node_modules"],
            ignored_file_name_fragments: &[".generated."],
            allowed_extensions: &["ts", "tsx", "mts", "cts"],
        }
    }
}

impl Default for ImportTypeScriptBatchOptions {
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

impl ImportDiscoveryPolicy {
    fn should_descend(&self, path: &Path) -> bool {
        let Some(name) = path.file_name().and_then(|name| name.to_str()) else {
            return true;
        };
        !self
            .ignored_dir_names
            .iter()
            .any(|ignored| name.eq_ignore_ascii_case(ignored))
    }

    fn should_include_file(&self, path: &Path) -> bool {
        let Some(file_name) = path.file_name().and_then(|name| name.to_str()) else {
            return false;
        };
        if self
            .ignored_file_name_fragments
            .iter()
            .any(|fragment| file_name.to_ascii_lowercase().contains(fragment))
        {
            return false;
        }

        let Some(ext) = path.extension().and_then(|ext| ext.to_str()) else {
            return false;
        };
        self.allowed_extensions
            .iter()
            .any(|allowed| ext.eq_ignore_ascii_case(allowed))
    }
}

#[derive(Debug, Default)]
struct ImportTypeScriptSummary {
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<(PathBuf, String)>,
}

#[derive(Debug, Serialize)]
struct ImportTypeScriptFailureEntry {
    file: String,
    error: String,
}

#[derive(Debug, Serialize)]
struct ImportTypeScriptFailureReport {
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
    failed_files: Vec<ImportTypeScriptFailureEntry>,
    generated_kain_path: Option<String>,
    compiled_output_path: Option<String>,
    generated_at_utc: String,
}

pub fn import_typescript(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
) -> KainResult<()> {
    import_typescript_with_batch(
        input,
        output,
        target,
        &ImportTypeScriptBatchOptions::default(),
    )
}

pub fn import_typescript_with_batch(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportTypeScriptBatchOptions,
) -> KainResult<()> {
    let resolved_output = match output {
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

    let kain_source = generate_kain_source(&program)?;
    let mut generated_kain_path = None;

    if let Some(out_path) = resolved_output.as_deref() {
        fs::write(out_path, &kain_source)
            .map_err(|e| KainError::runtime(format!("Failed to write output: {}", e)))?;
        generated_kain_path = Some(out_path.to_path_buf());
        println!("Generated KAIN source: {} ({} bytes)", out_path.display(), kain_source.len());
    }

    let mut compiled_output_path = None;
    if let Some(target_str) = target {
        let compile_target = crate::parse_compile_target(target_str)
            .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;

        println!("Compiling to target: {}", target_str);

        let compiled = crate::compile(&kain_source, compile_target)
            .map_err(|e| KainError::runtime(format!("Compilation failed: {}", e)))?;

        let compiled_output = if let Some(out) = resolved_output.as_deref() {
            out.with_extension(crate::target_extension(compile_target))
        } else {
            input.with_extension(crate::target_extension(compile_target))
        };

        fs::write(&compiled_output, &compiled)
            .map_err(|e| KainError::runtime(format!("Failed to write compiled output: {}", e)))?;
        compiled_output_path = Some(compiled_output.clone());
        println!("Compiled output: {} ({} bytes)", compiled_output.display(), compiled.len());
    }

    println!("Import complete");
    println!("  Functions: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Function(_))));
    println!("  Structs: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Struct(_))));
    println!("  Enums: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Enum(_))));
    println!("  Impls: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Impl(_))));
    println!("  Type aliases: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::TypeAlias(_))));

    if input.is_dir() {
        println!(
            "  TypeScript files: discovered {}, imported {}, skipped {}, failed {}",
            summary.discovered_files,
            summary.imported_files,
            summary.skipped_files,
            summary.failed_files.len()
        );

        if !summary.failed_files.is_empty() {
            println!("  Failed files:");
            for (path, error) in summary.failed_files.iter().take(20) {
                println!("    - {}: {}", path.display(), error);
            }
            if summary.failed_files.len() > 20 {
                println!("    ... {} more", summary.failed_files.len() - 20);
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
    batch: &ImportTypeScriptBatchOptions,
) -> KainResult<(kain_core::ast::Program, ImportTypeScriptSummary)> {
    if input.is_file() {
        let program = kain_import::import_typescript(input)
            .map_err(|e| KainError::runtime(format!("TypeScript import failed: {}", e)))?;
        return Ok((
            program,
            ImportTypeScriptSummary {
                discovered_files: 1,
                imported_files: 1,
                ..ImportTypeScriptSummary::default()
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
    let discovery_policy = ImportDiscoveryPolicy::default();
    collect_typescript_files(input, batch.recursive, &discovery_policy, &mut candidates)?;
    candidates.sort();

    let mut summary = ImportTypeScriptSummary {
        discovered_files: candidates.len(),
        ..ImportTypeScriptSummary::default()
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

        match kain_import::import_typescript(&file) {
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
                summary.failed_files.push((file.clone(), compact_error_message(&err.to_string())));
                if batch.fail_fast {
                    return Err(KainError::runtime(format!(
                        "TypeScript import failed: {}: {}",
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

fn build_no_import_detail(summary: &ImportTypeScriptSummary) -> String {
    if summary.failed_files.is_empty() {
        return "No TypeScript source files matched include/exclude filters".to_string();
    }

    let previews = summary
        .failed_files
        .iter()
        .take(5)
        .map(|(path, err)| format!("{}: {}", path.display(), err))
        .collect::<Vec<_>>();

    format!(
        "All matching TypeScript files failed to import (e.g. {})",
        previews.join(" | ")
    )
}

fn maybe_write_failure_report(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportTypeScriptBatchOptions,
    summary: &ImportTypeScriptSummary,
    generated_kain_path: Option<&Path>,
    compiled_output_path: Option<&Path>,
) -> KainResult<()> {
    let Some(report_path) = resolve_report_path(input, output, batch, summary) else {
        return Ok(());
    };

    let report = ImportTypeScriptFailureReport {
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
            .map(|(path, error)| ImportTypeScriptFailureEntry {
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

    println!("Failure report JSON: {}", report_path.display());
    Ok(())
}

fn resolve_report_path(
    input: &Path,
    output: Option<&Path>,
    batch: &ImportTypeScriptBatchOptions,
    summary: &ImportTypeScriptSummary,
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

fn collect_typescript_files(
    root: &Path,
    recursive: bool,
    discovery_policy: &ImportDiscoveryPolicy,
    out: &mut Vec<PathBuf>,
) -> KainResult<()> {
    let entries = fs::read_dir(root)
        .map_err(|e| KainError::runtime(format!("Failed to read directory {}: {}", root.display(), e)))?;

    for entry in entries {
        let entry = entry.map_err(|e| KainError::runtime(format!("Failed to read directory entry: {}", e)))?;
        let path = entry.path();

        if path.is_dir() {
            if recursive && discovery_policy.should_descend(&path) {
                collect_typescript_files(&path, recursive, discovery_policy, out)?;
            }
            continue;
        }

        if discovery_policy.should_include_file(&path) {
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
    let normalized = path.to_string_lossy().replace('\\', "/").to_ascii_lowercase();

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
            .map(|ch| if ch.is_ascii_alphanumeric() { ch.to_ascii_lowercase() } else { '_' })
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

fn generate_kain_source(program: &kain_core::ast::Program) -> KainResult<String> {
    use std::fmt::Write;

    let mut output = String::new();
    writeln!(output, "# Generated from TypeScript source by kain import-ts")
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    writeln!(output)
        .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;

    for item in &program.items {
        write_item(&mut output, item, 0)?;
    }

    Ok(output)
}

fn write_item(output: &mut String, item: &kain_core::ast::Item, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    match item {
        kain_core::ast::Item::Function(func) => write_function(output, func, indent),
        kain_core::ast::Item::Component(component) => write_component(output, component, indent),
        kain_core::ast::Item::Struct(st) => write_struct(output, st, indent),
        kain_core::ast::Item::Enum(en) => write_enum(output, en, indent),
        kain_core::ast::Item::TypeAlias(alias) => {
            write_line(output, indent, &format!("type {} = {}", alias.name, type_to_string(&alias.target)))?;
            writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write type alias: {}", e)))
        }
        kain_core::ast::Item::Const(item) => {
            write_line(
                output,
                indent,
                &format!("const {}: {} = {}", item.name, type_to_string(&item.ty), expr_to_string(&item.value)),
            )?;
            writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write const: {}", e)))
        }
        kain_core::ast::Item::Impl(imp) => write_impl(output, imp, indent),
        kain_core::ast::Item::Mod(module) => {
            write_line(output, indent, &format!("mod {}:", module.name))?;
            if let Some(children) = &module.inline {
                for child in children {
                    write_item(output, child, indent + 1)?;
                }
            }
            writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write module: {}", e)))
        }
        _ => Ok(()),
    }
}

fn write_function(output: &mut String, func: &kain_core::ast::Function, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let mut signature = format!("fn {}(", func.name);
    for (index, param) in func.params.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
    }
    signature.push(')');

    if let Some(ret_ty) = &func.return_type {
        signature.push_str(&format!(" -> {}", type_to_string(ret_ty)));
    }

    signature.push(':');
    write_line(output, indent, &signature)?;
    write_block(output, &func.body, indent + 1)?;
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write function: {}", e)))?;
    Ok(())
}

fn write_component(
    output: &mut String,
    component: &kain_core::ast::Component,
    indent: usize,
) -> KainResult<()> {
    use std::fmt::Write;

    let mut signature = format!("component {}(", component.name);
    for (index, prop) in component.props.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", prop.name, type_to_string(&prop.ty)));
    }
    signature.push(')');
    if !component.effects.is_empty() {
        signature.push_str(&format!(" with {}", effects_to_string(&component.effects)));
    }
    signature.push(':');

    write_line(output, indent, &signature)?;

    for state in &component.state {
        let weak_prefix = if state.weak { "weak " } else { "" };
        write_line(
            output,
            indent + 1,
            &format!(
                "{}state {}: {} = {}",
                weak_prefix,
                state.name,
                type_to_string(&state.ty),
                expr_to_string_with_indent(&state.initial, indent + 1)
            ),
        )?;
    }

    for method in &component.methods {
        write_function(output, method, indent + 1)?;
    }

    write_line(
        output,
        indent + 1,
        &format!("render {}", jsx_to_string(&component.body)),
    )?;

    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write component: {}", e)))?;
    Ok(())
}

fn write_struct(output: &mut String, st: &kain_core::ast::Struct, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", st.name))?;
    if st.fields.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for field in &st.fields {
            let mut line = format!("{}: {}", field.name, type_to_string(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", expr_to_string_with_indent(default, indent + 1)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;
    Ok(())
}

fn write_enum(output: &mut String, en: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", en.name))?;
    if en.variants.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for variant in &en.variants {
            let line = match &variant.fields {
                kain_core::ast::VariantFields::Unit => variant.name.clone(),
                kain_core::ast::VariantFields::Tuple(types) => {
                    let fields = types.iter().map(type_to_string).collect::<Vec<_>>().join(", ");
                    format!("{}({})", variant.name, fields)
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let fields = fields
                        .iter()
                        .map(|field| format!("{}: {}", field.name, type_to_string(&field.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{ {} }}", variant.name, fields)
                }
            };
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write enum: {}", e)))?;
    Ok(())
}

fn write_impl(output: &mut String, imp: &kain_core::ast::Impl, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let header = match &imp.trait_name {
        Some(trait_name) => format!("impl {} for {}:", trait_name, type_to_string(&imp.target_type)),
        None => format!("impl {}:", type_to_string(&imp.target_type)),
    };

    write_line(output, indent, &header)?;
    if imp.methods.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for method in &imp.methods {
            write_function(output, method, indent + 1)?;
        }
    }
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write impl: {}", e)))?;
    Ok(())
}

fn write_block(output: &mut String, block: &kain_core::ast::Block, indent: usize) -> KainResult<()> {
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
        kain_core::ast::Stmt::Let { pattern, ty, value, .. } => {
            let mut line = format!("let {}", pattern_to_string(pattern));
            if let Some(ty) = ty {
                line.push_str(&format!(": {}", type_to_string(ty)));
            }
            if let Some(value) = value {
                line.push_str(" = ");
                line.push_str(&expr_to_string_with_indent(value, indent));
            }
            write_line(output, indent, &line)
        }
        kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Block(block, _)) => {
            write_block(output, block, indent)
        }
        kain_core::ast::Stmt::Expr(expr) => write_line(output, indent, &expr_to_string_with_indent(expr, indent)),
        kain_core::ast::Stmt::Return(Some(expr), _) => {
            write_line(
                output,
                indent,
                &format!("return {}", expr_to_string_with_indent(expr, indent)),
            )
        }
        kain_core::ast::Stmt::Return(None, _) => write_line(output, indent, "return"),
        kain_core::ast::Stmt::Break(Some(expr), _) => {
            write_line(
                output,
                indent,
                &format!("break {}", expr_to_string_with_indent(expr, indent)),
            )
        }
        kain_core::ast::Stmt::Break(None, _) => write_line(output, indent, "break"),
        kain_core::ast::Stmt::Continue(_) => write_line(output, indent, "continue"),
        kain_core::ast::Stmt::While { condition, body, .. } => {
            write_line(
                output,
                indent,
                &format!("while {}:", expr_to_string_with_indent(condition, indent)),
            )?;
            write_block(output, body, indent + 1)
        }
        kain_core::ast::Stmt::For {
            binding, iter, body, ..
        } => {
            write_line(
                output,
                indent,
                &format!(
                    "for {} in {}:",
                    pattern_to_string(binding),
                    expr_to_string_with_indent(iter, indent)
                ),
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

    let mut lines = line.lines();
    if let Some(first) = lines.next() {
        writeln!(output, "{}{}", "    ".repeat(indent), first)
            .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    } else {
        writeln!(output).map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
        return Ok(());
    }

    for rest in lines {
        writeln!(output, "{rest}")
            .map_err(|e| KainError::runtime(format!("Failed to generate source: {}", e)))?;
    }

    Ok(())
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
        kain_core::ast::Pattern::Tuple(patterns, _) => {
            let items = patterns.iter().map(pattern_to_string).collect::<Vec<_>>().join(", ");
            format!("({})", items)
        }
        kain_core::ast::Pattern::Literal(expr) => expr_to_string(expr),
        _ => "_".to_string(),
    }
}

fn expr_to_string(expr: &kain_core::ast::Expr) -> String {
    expr_to_string_with_indent(expr, 0)
}

fn expr_to_string_with_indent(expr: &kain_core::ast::Expr, indent: usize) -> String {
    match expr {
        kain_core::ast::Expr::Int(value, _) => value.to_string(),
        kain_core::ast::Expr::Float(value, _) => value.to_string(),
        kain_core::ast::Expr::String(value, _) => format!("{:?}", value),
        kain_core::ast::Expr::FString(parts, _) => parts
            .iter()
            .map(|part| expr_to_string_with_indent(part, indent))
            .collect::<Vec<_>>()
            .join(" + "),
        kain_core::ast::Expr::Bool(value, _) => value.to_string(),
        kain_core::ast::Expr::None(_) => "none".to_string(),
        kain_core::ast::Expr::Ident(name, _) => name.clone(),
        kain_core::ast::Expr::Binary { left, op, right, .. } => {
            format!(
                "({} {} {})",
                expr_to_string_with_indent(left, indent),
                binary_op_to_string(*op),
                expr_to_string_with_indent(right, indent)
            )
        }
        kain_core::ast::Expr::Unary { op, operand, .. } => {
            format!("({}{})", unary_op_to_string(*op), expr_to_string_with_indent(operand, indent))
        }
        kain_core::ast::Expr::Call { callee, args, .. } => {
            let callee = expr_to_string_with_indent(callee, indent);
            let args = args
                .iter()
                .map(|arg| call_arg_to_string_with_indent(arg, indent + 1))
                .collect::<Vec<_>>();
            render_call_like(&callee, &args, indent)
        }
        kain_core::ast::Expr::MethodCall { receiver, method, args, .. } => {
            let callee = format!("{}.{}", expr_to_string_with_indent(receiver, indent), method);
            let args = args
                .iter()
                .map(|arg| call_arg_to_string_with_indent(arg, indent + 1))
                .collect::<Vec<_>>();
            render_call_like(&callee, &args, indent)
        }
        kain_core::ast::Expr::Field { object, field, .. } => {
            format!("{}.{}", expr_to_string_with_indent(object, indent), field)
        }
        kain_core::ast::Expr::Index { object, index, .. } => {
            format!(
                "{}[{}]",
                expr_to_string_with_indent(object, indent),
                expr_to_string_with_indent(index, indent)
            )
        }
        kain_core::ast::Expr::Assign { target, value, .. } => {
            format!(
                "{} = {}",
                expr_to_string_with_indent(target, indent),
                expr_to_string_with_indent(value, indent)
            )
        }
        kain_core::ast::Expr::Array(items, _) => {
            let items = items
                .iter()
                .map(|item| expr_to_string_with_indent(item, indent))
                .collect::<Vec<_>>()
                .join(", ");
            format!("[{}]", items)
        }
        kain_core::ast::Expr::Tuple(items, _) => {
            let items = items
                .iter()
                .map(|item| expr_to_string_with_indent(item, indent))
                .collect::<Vec<_>>()
                .join(", ");
            format!("({})", items)
        }
        kain_core::ast::Expr::Struct { name, fields, .. } => {
            let fields = fields
                .iter()
                .map(|(name, value)| format!("{name}: {}", expr_to_string_with_indent(value, indent)))
                .collect::<Vec<_>>()
                .join(", ");
            format!("{name} {{ {fields} }}")
        }
        kain_core::ast::Expr::AggregateInit { ty, fields, .. } => {
            let _ = (ty, fields, indent);
            "Any()".to_string()
        }
        kain_core::ast::Expr::Cast { value, target, .. } => {
            format!(
                "({} as {})",
                expr_to_string_with_indent(value, indent),
                type_to_string(target)
            )
        }
        kain_core::ast::Expr::Await(value, _) => {
            format!("(await {})", expr_to_string_with_indent(value, indent))
        }
        kain_core::ast::Expr::Try(value, _) => format!("({}?)", expr_to_string_with_indent(value, indent)),
        kain_core::ast::Expr::Paren(value, _) => format!("({})", expr_to_string_with_indent(value, indent)),
        kain_core::ast::Expr::JSX(node, _) => jsx_to_string(node),
        kain_core::ast::Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => if_expr_to_string(condition, then_branch, else_branch.as_deref(), indent),
        kain_core::ast::Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => lambda_to_string(params, return_type.as_ref(), body, indent),
        kain_core::ast::Expr::Block(block, _) => block_expr_to_string(block, indent),
        kain_core::ast::Expr::Return(Some(value), _) => {
            format!("return {}", expr_to_string_with_indent(value, indent))
        }
        kain_core::ast::Expr::Return(None, _) => "return".to_string(),
        kain_core::ast::Expr::Break(Some(value), _) => {
            format!("break {}", expr_to_string_with_indent(value, indent))
        }
        kain_core::ast::Expr::Break(None, _) => "break".to_string(),
        kain_core::ast::Expr::Continue(_) => "continue".to_string(),
        _ => "none".to_string(),
    }
}

fn jsx_to_string(node: &kain_core::ast::JSXNode) -> String {
    match node {
        kain_core::ast::JSXNode::Element {
            tag,
            attributes,
            children,
            ..
        } => {
            let attrs = jsx_attrs_to_string(attributes);
            if children.is_empty() {
                format!("<{}{} />", tag, attrs)
            } else {
                let children = children.iter().map(jsx_to_string).collect::<Vec<_>>().join("");
                format!("<{}{}>{}</{}>", tag, attrs, children, tag)
            }
        }
        kain_core::ast::JSXNode::Text(text, _) => text.clone(),
        kain_core::ast::JSXNode::Expression(expr) => format!("{{{}}}", expr_to_string(expr)),
        kain_core::ast::JSXNode::ComponentCall {
            name,
            props,
            children,
            ..
        } => {
            let attrs = jsx_attrs_to_string(props);
            if children.is_empty() {
                format!("<{}{} />", name, attrs)
            } else {
                let children = children.iter().map(jsx_to_string).collect::<Vec<_>>().join("");
                format!("<{}{}>{}</{}>", name, attrs, children, name)
            }
        }
        kain_core::ast::JSXNode::For {
            binding, iter, body, ..
        } => format!("{{for {} in {}: {}}}", binding, expr_to_string(iter), jsx_to_string(body)),
        kain_core::ast::JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let else_suffix = else_branch
                .as_ref()
                .map(|node| format!(" else: {}", jsx_to_string(node)))
                .unwrap_or_default();
            format!(
                "{{if {}: {}{}}}",
                expr_to_string(condition),
                jsx_to_string(then_branch),
                else_suffix
            )
        }
        kain_core::ast::JSXNode::Fragment(children, _) => {
            let children = children.iter().map(jsx_to_string).collect::<Vec<_>>().join("");
            format!("<Fragment>{}</Fragment>", children)
        }
    }
}

fn jsx_attrs_to_string(attrs: &[kain_core::ast::JSXAttribute]) -> String {
    if attrs.is_empty() {
        return String::new();
    }

    let rendered = attrs
        .iter()
        .map(|attr| match &attr.value {
            kain_core::ast::JSXAttrValue::String(value) => {
                format!(r#"{}={:?}"#, attr.name, value)
            }
            kain_core::ast::JSXAttrValue::Expr(expr) => {
                let rendered = match expr {
                    kain_core::ast::Expr::Lambda { .. } => "none".to_string(),
                    _ => expr_to_string(expr),
                };
                format!("{}={{{}}}", attr.name, rendered)
            }
            kain_core::ast::JSXAttrValue::Bool(value) => {
                format!("{}={{{}}}", attr.name, value)
            }
        })
        .collect::<Vec<_>>()
        .join(" ");
    format!(" {}", rendered)
}

fn effects_to_string(effects: &[kain_core::effects::Effect]) -> String {
    effects
        .iter()
        .map(|effect| match effect {
            kain_core::effects::Effect::Pure => "Pure",
            kain_core::effects::Effect::IO => "IO",
            kain_core::effects::Effect::Async => "Async",
            kain_core::effects::Effect::GPU => "GPU",
            kain_core::effects::Effect::Reactive => "Reactive",
            kain_core::effects::Effect::Unsafe => "Unsafe",
            kain_core::effects::Effect::Alloc => "Alloc",
            kain_core::effects::Effect::Panic => "Panic",
        })
        .collect::<Vec<_>>()
        .join(", ")
}

fn call_arg_to_string(arg: &kain_core::ast::CallArg) -> String {
    call_arg_to_string_with_indent(arg, 0)
}

fn call_arg_to_string_with_indent(arg: &kain_core::ast::CallArg, indent: usize) -> String {
    match &arg.name {
        Some(name) => format!("{name} = {}", expr_to_string_with_indent(&arg.value, indent)),
        None => expr_to_string_with_indent(&arg.value, indent),
    }
}

fn render_call_like(callee: &str, args: &[String], indent: usize) -> String {
    if args.is_empty() {
        return format!("{callee}()");
    }

    if args.iter().all(|arg| !arg.contains('\n')) {
        return format!("{callee}({})", args.join(", "));
    }

    let body_indent = "    ".repeat(indent + 1);
    let closing_indent = "    ".repeat(indent);
    let rendered_args = args
        .iter()
        .map(|arg| indent_multiline(arg, &body_indent))
        .collect::<Vec<_>>()
        .join(",\n");
    format!("{callee}(\n{rendered_args}\n{closing_indent})")
}

fn indent_multiline(text: &str, prefix: &str) -> String {
    text.lines()
        .map(|line| format!("{prefix}{line}"))
        .collect::<Vec<_>>()
        .join("\n")
}

fn block_to_string(block: &kain_core::ast::Block, indent: usize) -> String {
    if block.stmts.is_empty() {
        return format!("{}pass", "    ".repeat(indent));
    }

    block
        .stmts
        .iter()
        .map(|stmt| stmt_to_string(stmt, indent))
        .collect::<Vec<_>>()
        .join("\n")
}

fn stmt_to_string(stmt: &kain_core::ast::Stmt, indent: usize) -> String {
    let prefix = "    ".repeat(indent);
    match stmt {
        kain_core::ast::Stmt::Let { pattern, ty, value, .. } => {
            let mut line = format!("{prefix}let {}", pattern_to_string(pattern));
            if let Some(ty) = ty {
                line.push_str(&format!(": {}", type_to_string(ty)));
            }
            if let Some(value) = value {
                line.push_str(" = ");
                line.push_str(&expr_to_string_with_indent(value, indent));
            }
            line
        }
        kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Block(block, _)) => block_to_string(block, indent),
        kain_core::ast::Stmt::Expr(expr) => format!("{prefix}{}", expr_to_string_with_indent(expr, indent)),
        kain_core::ast::Stmt::Return(Some(expr), _) => {
            format!("{prefix}return {}", expr_to_string_with_indent(expr, indent))
        }
        kain_core::ast::Stmt::Return(None, _) => format!("{prefix}return"),
        kain_core::ast::Stmt::Break(Some(expr), _) => {
            format!("{prefix}break {}", expr_to_string_with_indent(expr, indent))
        }
        kain_core::ast::Stmt::Break(None, _) => format!("{prefix}break"),
        kain_core::ast::Stmt::Continue(_) => format!("{prefix}continue"),
        kain_core::ast::Stmt::While { condition, body, .. } => format!(
            "{prefix}while {}:\n{}",
            expr_to_string_with_indent(condition, indent),
            block_to_string(body, indent + 1)
        ),
        kain_core::ast::Stmt::For { binding, iter, body, .. } => format!(
            "{prefix}for {} in {}:\n{}",
            pattern_to_string(binding),
            expr_to_string_with_indent(iter, indent),
            block_to_string(body, indent + 1)
        ),
        kain_core::ast::Stmt::Loop { body, .. } => {
            format!("{prefix}loop:\n{}", block_to_string(body, indent + 1))
        }
        kain_core::ast::Stmt::Item(item) => item_to_string(item, indent),
    }
}

fn item_to_string(item: &kain_core::ast::Item, indent: usize) -> String {
    let mut output = String::new();
    let _ = write_item(&mut output, item, indent);
    output.trim_end().to_string()
}

fn block_expr_to_string(block: &kain_core::ast::Block, indent: usize) -> String {
    let _ = (block, indent);
    "none".to_string()
}

fn lambda_to_string(
    params: &[kain_core::ast::Param],
    return_type: Option<&kain_core::ast::Type>,
    body: &kain_core::ast::Expr,
    indent: usize,
) -> String {
    let params = params
        .iter()
        .map(|param| format!("{}: {}", param.name, type_to_string(&param.ty)))
        .collect::<Vec<_>>()
        .join(", ");
    let ret = return_type
        .map(|ty| format!(" -> {}", type_to_string(ty)))
        .unwrap_or_default();

    match body {
        kain_core::ast::Expr::Block(block, _) => format!(
            "fn({params}){ret}:\n{}",
            block_to_string(block, indent + 1)
        ),
        _ => format!("fn({params}){ret}: {}", expr_to_string_with_indent(body, indent)),
    }
}

fn if_expr_to_string(
    condition: &kain_core::ast::Expr,
    then_branch: &kain_core::ast::Block,
    else_branch: Option<&kain_core::ast::ElseBranch>,
    indent: usize,
) -> String {
    let prefix = "    ".repeat(indent);
    let mut out = format!(
        "if {}:\n{}",
        expr_to_string_with_indent(condition, indent),
        block_to_string(then_branch, indent + 1)
    );
    if let Some(else_branch) = else_branch {
        out.push('\n');
        out.push_str(&else_branch_to_string(else_branch, indent, &prefix));
    }
    out
}

fn else_branch_to_string(
    else_branch: &kain_core::ast::ElseBranch,
    indent: usize,
    prefix: &str,
) -> String {
    match else_branch {
        kain_core::ast::ElseBranch::Else(block) => {
            format!("{prefix}else:\n{}", block_to_string(block, indent + 1))
        }
        kain_core::ast::ElseBranch::ElseIf(condition, block, nested) => {
            let mut out = format!(
                "{prefix}else if {}:\n{}",
                expr_to_string_with_indent(condition, indent),
                block_to_string(block, indent + 1)
            );
            if let Some(nested) = nested.as_deref() {
                out.push('\n');
                out.push_str(&else_branch_to_string(nested, indent, prefix));
            }
            out
        }
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

fn type_to_string(ty: &kain_core::ast::Type) -> String {
    match ty {
        kain_core::ast::Type::Named { name, generics, .. } => {
            let name = sanitize_type_name(name);
            if generics.is_empty() {
                name
            } else {
                let args = generics.iter().map(type_to_string).collect::<Vec<_>>().join(", " );
                format!("{name}<{args}>")
            }
        }
        kain_core::ast::Type::Tuple(types, _) => {
            let types = types.iter().map(type_to_string).collect::<Vec<_>>().join(", " );
            format!("({})", types)
        }
        kain_core::ast::Type::Array(inner, size, _) => format!("[{}; {}]", type_to_string(inner), size),
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
        // The current KAIN parser does not reliably accept emitted function types in
        // struct field positions from imported TS signatures. Prefer a parse-stable
        // fallback here so generated .kn files remain buildable.
        kain_core::ast::Type::Function { .. } => "Any".to_string(),
        kain_core::ast::Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        kain_core::ast::Type::Result(ok, err, _) => format!("{}!{}", type_to_string(ok), type_to_string(err)),
        kain_core::ast::Type::Infer(_) => "Any".to_string(),
        kain_core::ast::Type::Never(_) => "!".to_string(),
        kain_core::ast::Type::Unit(_) => "()".to_string(),
        kain_core::ast::Type::Impl { trait_name, generics, .. } => {
            let trait_name = sanitize_type_name(trait_name);
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                let args = generics.iter().map(type_to_string).collect::<Vec<_>>().join(", " );
                format!("impl {}<{}>", trait_name, args)
            }
        }
    }
}

fn sanitize_type_name(name: &str) -> String {
    let sanitized = name
        .chars()
        .map(|ch| if ch.is_ascii_alphanumeric() || ch == '_' { ch } else { '_' })
        .collect::<String>();

    if sanitized.is_empty() || sanitized == "_" {
        "Any".to_string()
    } else {
        sanitized
    }
}

fn count_items<F>(items: &[kain_core::ast::Item], predicate: F) -> usize
where
    F: Copy + Fn(&kain_core::ast::Item) -> bool,
{
    let mut total = 0;
    for item in items {
        if predicate(item) {
            total += 1;
        }
        if let kain_core::ast::Item::Mod(module) = item {
            if let Some(children) = &module.inline {
                total += count_items(children, predicate);
            }
        }
    }
    total
}
