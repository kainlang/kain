use crate::error::{KainError, KainResult};
use serde::Serialize;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct ImportUsfBatchOptions {
    pub recursive: bool,
    pub flat: bool,
    pub include_filters: Vec<String>,
    pub exclude_filters: Vec<String>,
    pub fail_fast: bool,
    pub report_json: Option<PathBuf>,
    pub research_mode: bool,
    pub preserve_comments: bool,
    pub llm_annotations: bool,
    pub flatten_includes: bool,
    pub engine_shaders_path: Option<PathBuf>,
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
            allowed_extensions: &["usf", "ush"],
        }
    }
}

impl Default for ImportUsfBatchOptions {
    fn default() -> Self {
        Self {
            recursive: true,
            flat: false,
            include_filters: Vec::new(),
            exclude_filters: Vec::new(),
            fail_fast: false,
            report_json: None,
            research_mode: false,
            preserve_comments: true,
            llm_annotations: false,
            flatten_includes: false,
            engine_shaders_path: None,
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
struct ImportUsfSummary {
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<(PathBuf, String)>,
}

#[derive(Debug, Serialize)]
struct ImportUsfFailureEntry {
    file: String,
    error: String,
}

#[derive(Debug, Serialize)]
struct ImportUsfFailureReport {
    input_path: String,
    output_path: Option<String>,
    target: Option<String>,
    recursive: bool,
    flat: bool,
    research_mode: bool,
    include_filters: Vec<String>,
    exclude_filters: Vec<String>,
    discovered_files: usize,
    imported_files: usize,
    skipped_files: usize,
    failed_files: Vec<ImportUsfFailureEntry>,
    generated_kain_path: Option<String>,
    compiled_output_path: Option<String>,
    generated_at_utc: String,
}

pub fn import_usf(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
) -> KainResult<()> {
    import_usf_with_batch(
        input,
        output,
        target,
        &ImportUsfBatchOptions::default(),
    )
}

pub fn import_usf_with_batch(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportUsfBatchOptions,
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
        println!("✅ Generated KAIN source: {} ({} bytes)", out_path.display(), kain_source.len());
    }

    let mut compiled_output_path = None;
    if let Some(target_str) = target {
        let compile_target = crate::parse_compile_target(target_str)
            .ok_or_else(|| KainError::runtime(format!("Unknown target: {}", target_str)))?;

        println!("🔨 Compiling to target: {}", target_str);

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
        println!("✅ Compiled output: {} ({} bytes)", compiled_output.display(), compiled.len());
    }

    println!("✅ Import complete");
    println!("  Functions: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Function(_))));
    println!("  Structs: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Struct(_))));
    println!("  Shaders: {}", count_items(&program.items, |item| matches!(item, kain_core::ast::Item::Shader(_))));

    if input.is_dir() {
        println!(
            "  USF files: discovered {}, imported {}, skipped {}, failed {}",
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
    batch: &ImportUsfBatchOptions,
) -> KainResult<(kain_core::ast::Program, ImportUsfSummary)> {
    if input.is_file() {
        let config = kain_import::usf::UsfImportConfig {
            research_mode: batch.research_mode,
            preserve_comments: batch.preserve_comments,
            add_attribution: true,
            llm_annotations: batch.llm_annotations,
            flatten_includes: batch.flatten_includes,
            engine_shaders_path: batch.engine_shaders_path.clone(),
        };

        let program = kain_import::usf::import_usf_file(input, config)
            .map_err(|e| KainError::runtime(format!("USF import failed: {}", e)))?;
        return Ok((
            program,
            ImportUsfSummary {
                discovered_files: 1,
                imported_files: 1,
                ..ImportUsfSummary::default()
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
    collect_usf_files(input, batch.recursive, &discovery_policy, &mut candidates)?;
    candidates.sort();

    let mut summary = ImportUsfSummary {
        discovered_files: candidates.len(),
        ..ImportUsfSummary::default()
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

        let config = kain_import::usf::UsfImportConfig {
            research_mode: batch.research_mode,
            preserve_comments: batch.preserve_comments,
            add_attribution: true,
            llm_annotations: batch.llm_annotations,
            flatten_includes: batch.flatten_includes,
            engine_shaders_path: batch.engine_shaders_path.clone(),
        };

        match kain_import::usf::import_usf_file(&file, config) {
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
                        "USF import failed: {}: {}",
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

fn build_no_import_detail(summary: &ImportUsfSummary) -> String {
    if summary.failed_files.is_empty() {
        return "No USF shader files matched include/exclude filters".to_string();
    }

    let previews = summary
        .failed_files
        .iter()
        .take(5)
        .map(|(path, err)| format!("{}: {}", path.display(), err))
        .collect::<Vec<_>>();

    format!(
        "All matching USF files failed to import (e.g. {})",
        previews.join(" | ")
    )
}

fn maybe_write_failure_report(
    input: &Path,
    output: Option<&Path>,
    target: Option<&str>,
    batch: &ImportUsfBatchOptions,
    summary: &ImportUsfSummary,
    generated_kain_path: Option<&Path>,
    compiled_output_path: Option<&Path>,
) -> KainResult<()> {
    let Some(report_path) = resolve_report_path(input, output, batch, summary) else {
        return Ok(());
    };

    let report = ImportUsfFailureReport {
        input_path: input.display().to_string(),
        output_path: output.map(|p| p.display().to_string()),
        target: target.map(str::to_string),
        recursive: batch.recursive,
        flat: batch.flat,
        research_mode: batch.research_mode,
        include_filters: batch.include_filters.clone(),
        exclude_filters: batch.exclude_filters.clone(),
        discovered_files: summary.discovered_files,
        imported_files: summary.imported_files,
        skipped_files: summary.skipped_files,
        failed_files: summary
            .failed_files
            .iter()
            .map(|(path, error)| ImportUsfFailureEntry {
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
    batch: &ImportUsfBatchOptions,
    summary: &ImportUsfSummary,
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

fn collect_usf_files(
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
                collect_usf_files(&path, recursive, discovery_policy, out)?;
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
    writeln!(output, "# Generated from USF shader source by kain import-usf")
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
        kain_core::ast::Item::Struct(st) => write_struct(output, st, indent),
        kain_core::ast::Item::Shader(shader) => write_shader(output, shader, indent),
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

fn write_struct(output: &mut String, st: &kain_core::ast::Struct, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", st.name))?;
    if st.fields.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for field in &st.fields {
            let mut line = format!("{}: {}", field.name, type_to_string(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", expr_to_string(default)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write struct: {}", e)))?;
    Ok(())
}

fn write_shader(output: &mut String, shader: &kain_core::ast::Shader, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let stage = match shader.stage {
        kain_core::ast::ShaderStage::Vertex => "vertex",
        kain_core::ast::ShaderStage::Fragment => "fragment",
        kain_core::ast::ShaderStage::Compute => "compute",
        kain_core::ast::ShaderStage::Surface => "surface",
    };

    write_line(output, indent, &format!("shader {} {}:", stage, shader.name))?;
    write_block(output, &shader.body, indent + 1)?;
    writeln!(output).map_err(|e| KainError::runtime(format!("Failed to write shader: {}", e)))?;
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
                line.push_str(&expr_to_string(value));
            }
            write_line(output, indent, &line)
        }
        kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Block(block, _)) => {
            write_block(output, block, indent)
        }
        kain_core::ast::Stmt::Expr(expr) => write_line(output, indent, &expr_to_string(expr)),
        kain_core::ast::Stmt::Return(Some(expr), _) => {
            write_line(output, indent, &format!("return {}", expr_to_string(expr)))
        }
        kain_core::ast::Stmt::Return(None, _) => write_line(output, indent, "return"),
        _ => write_line(output, indent, "pass"),
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
    match expr {
        kain_core::ast::Expr::Int(value, _) => value.to_string(),
        kain_core::ast::Expr::Float(value, _) => value.to_string(),
        kain_core::ast::Expr::String(value, _) => format!("{:?}", value),
        kain_core::ast::Expr::Bool(value, _) => value.to_string(),
        kain_core::ast::Expr::None(_) => "none".to_string(),
        kain_core::ast::Expr::Ident(name, _) => name.clone(),
        kain_core::ast::Expr::Binary { left, op, right, .. } => {
            format!("({} {} {})", expr_to_string(left), binary_op_to_string(*op), expr_to_string(right))
        }
        kain_core::ast::Expr::AsyncBlock(value, _) => format!("async {}", expr_to_string(value)),
        kain_core::ast::Expr::Call { callee, args, .. } => {
            let args = args.iter().map(|arg| call_arg_to_string(arg)).collect::<Vec<_>>().join(", ");
            format!("{}({})", expr_to_string(callee), args)
        }
        _ => "unknown".to_string(),
    }
}

fn type_to_string(ty: &kain_core::ast::Type) -> String {
    match ty {
        kain_core::ast::Type::Named(name) => name.clone(),
        kain_core::ast::Type::Array(inner, size) => {
            if let Some(size) = size {
                format!("[{}; {}]", type_to_string(inner), size)
            } else {
                format!("[{}]", type_to_string(inner))
            }
        }
        _ => "unknown".to_string(),
    }
}

fn binary_op_to_string(op: kain_core::ast::BinaryOp) -> &'static str {
    match op {
        kain_core::ast::BinaryOp::Add => "+",
        kain_core::ast::BinaryOp::Sub => "-",
        kain_core::ast::BinaryOp::Mul => "*",
        kain_core::ast::BinaryOp::Div => "/",
        kain_core::ast::BinaryOp::Mod => "%",
        kain_core::ast::BinaryOp::Eq => "==",
        kain_core::ast::BinaryOp::Ne => "!=",
        kain_core::ast::BinaryOp::Lt => "<",
        kain_core::ast::BinaryOp::Le => "<=",
        kain_core::ast::BinaryOp::Gt => ">",
        kain_core::ast::BinaryOp::Ge => ">=",
        kain_core::ast::BinaryOp::And => "&&",
        kain_core::ast::BinaryOp::Or => "||",
        _ => "?",
    }
}

fn call_arg_to_string(arg: &kain_core::ast::CallArg) -> String {
    match arg {
        kain_core::ast::CallArg::Positional(expr) => expr_to_string(expr),
        kain_core::ast::CallArg::Named { name, value } => format!("{} = {}", name, expr_to_string(value)),
    }
}

fn count_items<F>(items: &[kain_core::ast::Item], predicate: F) -> usize
where
    F: Fn(&kain_core::ast::Item) -> bool,
{
    items.iter().filter(|item| predicate(item)).count()
}
