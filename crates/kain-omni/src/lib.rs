use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};

use kain_asm;
use kain_core::ast::{Block, Expr, Function, Impl, Item, Pattern, Program, Stmt, Struct, Type};
use kain_core::error::KainError;
use kain_core::CompileTarget;
use kain_driver::{self as driver, GpuArtifactOutput, RustBundleOutput};
use kain_import::c::{import_c_file_with_options, CImportOptions};
use kain_import::rust::import_rust_file;
use kain_import::typescript::import_typescript_file;
use kain_sys_codegen::RustArtifactKind;
use serde::{Deserialize, Serialize};
use thiserror::Error;
#[derive(Debug, Error)]
pub enum OmniError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    #[error("TOML parse error: {0}")]
    Toml(#[from] toml::de::Error),
    #[error("TOML serialize error: {0}")]
    TomlSer(#[from] toml::ser::Error),
    #[error("Compiler error: {0}")]
    Compiler(String),
    #[error("Configuration error: {0}")]
    Config(String),
    #[error("Import resolution error: {0}")]
    ImportResolution(String),
    #[error("Source generation error: {0}")]
    SourceGeneration(String),
}

pub type OmniResult<T> = Result<T, OmniError>;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniManifest {
    pub project: OmniProject,
    #[serde(default)]
    pub imports: Vec<OmniImportSource>,
    #[serde(default)]
    pub targets: Vec<OmniTarget>,
    #[serde(default)]
    pub import_resolution: OmniImportResolution,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniProject {
    pub entry: PathBuf,
    #[serde(default = "default_build_dir")]
    pub build_dir: PathBuf,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct OmniImportResolution {
    #[serde(default)]
    pub search_roots: Vec<PathBuf>,
    #[serde(default = "default_inline_imports")]
    pub inline_kain_imports: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OmniSourceLanguage {
    Kain,
    Rust,
    TypeScript,
    C,
    Asm,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniImportSource {
    pub path: PathBuf,
    pub language: OmniSourceLanguage,
    #[serde(default)]
    pub output: Option<PathBuf>,
    #[serde(default)]
    pub flat: bool,
    #[serde(default)]
    pub recursive: bool,
    #[serde(default)]
    pub include_filters: Vec<String>,
    #[serde(default)]
    pub exclude_filters: Vec<String>,
    #[serde(default)]
    pub fail_fast: bool,
    #[serde(default)]
    pub include_paths: Vec<String>,
    #[serde(default)]
    pub defines: Vec<String>,
    #[serde(default)]
    pub asm_format: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OmniTargetKind {
    Rust,
    Js,
    Ts,
    Ks,
    Cpp,
    Hlsl,
    Usf,
    Spirv,
    GpuArtifacts,
    RustBundle,
    Ue5,
    Ue5Editor,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum OmniRustArtifact {
    Source,
    ShaderHost,
    ShaderReflection,
    Spirv,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniRustBundleConfig {
    #[serde(default)]
    pub output: Option<PathBuf>,
    #[serde(default = "default_rust_bundle_artifacts")]
    pub artifacts: Vec<OmniRustArtifact>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniTarget {
    pub kind: OmniTargetKind,
    pub output: PathBuf,
    #[serde(default)]
    pub rust_bundle: Option<OmniRustBundleConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OmniBuildResult {
    pub manifest_path: PathBuf,
    pub staged_imports: Vec<StagedImport>,
    pub resolved_entry: PathBuf,
    pub written_outputs: Vec<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StagedImport {
    pub source_path: PathBuf,
    pub language: OmniSourceLanguage,
    pub generated_kn_path: PathBuf,
}

impl Default for OmniManifest {
    fn default() -> Self {
        Self {
            project: OmniProject::default(),
            imports: Vec::new(),
            targets: vec![OmniTarget {
                kind: OmniTargetKind::Rust,
                output: PathBuf::from("omni_out/main.rs"),
                rust_bundle: None,
            }],
            import_resolution: OmniImportResolution::default(),
        }
    }
}

impl Default for OmniProject {
    fn default() -> Self {
        Self {
            entry: PathBuf::from("src/main.kn"),
            build_dir: default_build_dir(),
        }
    }
}

impl Default for OmniRustBundleConfig {
    fn default() -> Self {
        Self {
            output: None,
            artifacts: default_rust_bundle_artifacts(),
        }
    }
}

pub fn init_manifest(root: &Path) -> OmniResult<PathBuf> {
    fs::create_dir_all(root)?;
    let manifest_path = root.join("KAIN.omni.toml");
    let manifest = OmniManifest::default();
    fs::write(&manifest_path, toml::to_string_pretty(&manifest)?)?;
    Ok(manifest_path)
}

pub fn load_manifest(path: &Path) -> OmniResult<OmniManifest> {
    let content = fs::read_to_string(path)?;
    Ok(toml::from_str(&content)?)
}

pub fn build_manifest_path(path: &Path) -> OmniResult<OmniBuildResult> {
    let manifest = load_manifest(path)?;
    let manifest_root = path
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| PathBuf::from("."));
    build(&manifest_root, &manifest, Some(path))
}

pub fn build(
    project_root: &Path,
    manifest: &OmniManifest,
    manifest_path: Option<&Path>,
) -> OmniResult<OmniBuildResult> {
    let build_root = resolve_from_root(project_root, &manifest.project.build_dir);
    let staged_dir = build_root.join("staged_imports");
    let resolved_dir = build_root.join("resolved");
    fs::create_dir_all(&staged_dir)?;
    fs::create_dir_all(&resolved_dir)?;

    let staged_imports = stage_imports(project_root, &staged_dir, &manifest.imports)?;
    let entry_path = resolve_from_root(project_root, &manifest.project.entry);
    let resolved_source = if manifest.import_resolution.inline_kain_imports {
        resolve_kain_program(
            &entry_path,
            &build_search_roots(project_root, &staged_imports, &manifest.import_resolution),
        )?
    } else {
        fs::read_to_string(&entry_path)?
    };

    let resolved_entry = resolved_dir.join(
        entry_path
            .file_name()
            .unwrap_or_else(|| std::ffi::OsStr::new("entry.kn")),
    );
    fs::write(&resolved_entry, &resolved_source)?;

    let mut written_outputs = Vec::new();
    for target in &manifest.targets {
        let output = resolve_from_root(project_root, &target.output);
        if let Some(parent) = output.parent() {
            fs::create_dir_all(parent)?;
        }
        let mut outputs = build_target(&resolved_source, &resolved_entry, &output, target)?;
        written_outputs.append(&mut outputs);
    }

    Ok(OmniBuildResult {
        manifest_path: manifest_path
            .map(Path::to_path_buf)
            .unwrap_or_else(|| project_root.join("KAIN.omni.toml")),
        staged_imports,
        resolved_entry,
        written_outputs,
    })
}

fn default_build_dir() -> PathBuf {
    PathBuf::from("omni_out")
}

fn default_inline_imports() -> bool {
    true
}

fn default_rust_bundle_artifacts() -> Vec<OmniRustArtifact> {
    vec![
        OmniRustArtifact::Source,
        OmniRustArtifact::ShaderHost,
        OmniRustArtifact::ShaderReflection,
        OmniRustArtifact::Spirv,
    ]
}

fn stage_imports(
    project_root: &Path,
    staged_dir: &Path,
    imports: &[OmniImportSource],
) -> OmniResult<Vec<StagedImport>> {
    let mut staged = Vec::new();
    for import in imports {
        let source_path = resolve_from_root(project_root, &import.path);
        let generated_kn_path = import
            .output
            .as_ref()
            .map(|path| resolve_from_root(project_root, path))
            .unwrap_or_else(|| staged_dir.join(default_generated_name(&source_path)));
        if let Some(parent) = generated_kn_path.parent() {
            fs::create_dir_all(parent)?;
        }

        match import.language {
            OmniSourceLanguage::Kain => {
                fs::copy(&source_path, &generated_kn_path)?;
            }
            OmniSourceLanguage::Rust => {
                let program = import_rust_path(&source_path, import)?;
                fs::write(&generated_kn_path, render_program(&program)?)?;
            }
            OmniSourceLanguage::TypeScript => {
                let program = import_typescript_path(&source_path, import)?;
                fs::write(&generated_kn_path, render_program(&program)?)?;
            }
            OmniSourceLanguage::C => {
                let program = import_c_path(&source_path, import)?;
                fs::write(&generated_kn_path, render_program(&program)?)?;
            }
            OmniSourceLanguage::Asm => {
                kain_asm::import_asm(
                    &source_path,
                    import.asm_format.as_deref().unwrap_or("6502-furby"),
                    Some(generated_kn_path.as_path()),
                    false,
                )
                .map_err(|err| OmniError::Compiler(err.to_string()))?;
            }
        }

        staged.push(StagedImport {
            source_path,
            language: import.language.clone(),
            generated_kn_path,
        });
    }
    Ok(staged)
}

fn import_rust_path(path: &Path, config: &OmniImportSource) -> OmniResult<Program> {
    if path.is_file() {
        return import_rust_file(path).map_err(|err| OmniError::Compiler(err.to_string()));
    }
    let files = collect_language_files(path, config, &["rs"])?;
    import_many_as_program(&files, config.flat, |file| {
        import_rust_file(file).map_err(|err| OmniError::Compiler(err.to_string()))
    })
}

fn import_typescript_path(path: &Path, config: &OmniImportSource) -> OmniResult<Program> {
    if path.is_file() {
        return import_typescript_file(path).map_err(|err| OmniError::Compiler(err.to_string()));
    }
    let files = collect_language_files(path, config, &["ts", "tsx", "mts", "cts"])?;
    import_many_as_program(&files, config.flat, |file| {
        import_typescript_file(file).map_err(|err| OmniError::Compiler(err.to_string()))
    })
}

fn import_c_path(path: &Path, config: &OmniImportSource) -> OmniResult<Program> {
    let options = CImportOptions {
        include_paths: config.include_paths.clone(),
        defines: config.defines.clone(),
        cpp_options: Vec::new(),
        cpp_command: None,
    };
    if path.is_file() {
        return import_c_file_with_options(path, &options)
            .map_err(|err| OmniError::Compiler(err.to_string()));
    }
    let files = collect_language_files(path, config, &["c", "h"])?;
    import_many_as_program(&files, config.flat, |file| {
        import_c_file_with_options(file, &options)
            .map_err(|err| OmniError::Compiler(err.to_string()))
    })
}

fn collect_language_files(
    root: &Path,
    config: &OmniImportSource,
    allowed_extensions: &[&str],
) -> OmniResult<Vec<PathBuf>> {
    let mut files = Vec::new();
    collect_language_files_into(root, root, config, allowed_extensions, &mut files)?;
    files.sort();
    Ok(files)
}

fn collect_language_files_into(
    root: &Path,
    current: &Path,
    config: &OmniImportSource,
    allowed_extensions: &[&str],
    files: &mut Vec<PathBuf>,
) -> OmniResult<()> {
    for entry in fs::read_dir(current)? {
        let entry = entry?;
        let path = entry.path();
        if path.is_dir() {
            if config.recursive {
                collect_language_files_into(root, &path, config, allowed_extensions, files)?;
            }
            continue;
        }
        let Some(ext) = path.extension().and_then(|value| value.to_str()) else {
            continue;
        };
        if !allowed_extensions
            .iter()
            .any(|allowed| ext.eq_ignore_ascii_case(allowed))
        {
            continue;
        }
        let relative = path
            .strip_prefix(root)
            .unwrap_or(&path)
            .to_string_lossy()
            .replace('\\', "/")
            .to_ascii_lowercase();
        if !config.include_filters.is_empty()
            && !config
                .include_filters
                .iter()
                .any(|filter| relative.contains(&filter.to_ascii_lowercase()))
        {
            continue;
        }
        if config
            .exclude_filters
            .iter()
            .any(|filter| relative.contains(&filter.to_ascii_lowercase()))
        {
            continue;
        }
        files.push(path);
    }
    Ok(())
}

fn import_many_as_program<F>(files: &[PathBuf], flat: bool, mut importer: F) -> OmniResult<Program>
where
    F: FnMut(&Path) -> OmniResult<Program>,
{
    let span = kain_core::span::Span::default();
    let mut items = Vec::new();
    for file in files {
        let program = importer(file)?;
        if flat {
            items.extend(program.items);
        } else {
            items.push(Item::Mod(kain_core::ast::Mod {
                name: sanitize_module_name(file),
                inline: Some(program.items),
                visibility: kain_core::ast::Visibility::Public,
                span,
            }));
        }
    }
    Ok(Program { items, span })
}

fn sanitize_module_name(path: &Path) -> String {
    let raw = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("module");
    let mut name = raw
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() || ch == '_' {
                ch
            } else {
                '_'
            }
        })
        .collect::<String>();
    if name.is_empty() {
        name.push_str("module");
    }
    if name.chars().next().is_some_and(|ch| ch.is_ascii_digit()) {
        name.insert(0, 'm');
        name.insert(1, '_');
    }
    name
}

fn default_generated_name(path: &Path) -> PathBuf {
    let stem = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("imported");
    PathBuf::from(format!("{stem}.kn"))
}

fn render_program(program: &Program) -> OmniResult<String> {
    let mut output = String::new();
    for item in &program.items {
        write_item(&mut output, item, 0)?;
    }
    Ok(output)
}

fn write_item(output: &mut String, item: &Item, indent: usize) -> OmniResult<()> {
    use std::fmt::Write;

    match item {
        Item::Function(function) => write_function(output, function, indent),
        Item::Struct(value) => write_struct(output, value, indent),
        Item::Enum(value) => write_enum(output, value, indent),
        Item::Mod(value) => {
            write_line(output, indent, &format!("mod {}:", value.name))?;
            if let Some(children) = &value.inline {
                if children.is_empty() {
                    write_line(output, indent + 1, "pass")?;
                } else {
                    for child in children {
                        write_item(output, child, indent + 1)?;
                    }
                }
            } else {
                write_line(output, indent + 1, "pass")?;
            }
            writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
            Ok(())
        }
        Item::TypeAlias(value) => {
            write_line(
                output,
                indent,
                &format!("type {} = {}", value.name, type_to_string(&value.target)),
            )?;
            writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
            Ok(())
        }
        Item::Const(value) => {
            write_line(
                output,
                indent,
                &format!(
                    "const {}: {} = {}",
                    value.name,
                    type_to_string(&value.ty),
                    expr_to_string(&value.value)
                ),
            )?;
            writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
            Ok(())
        }
        Item::Impl(value) => write_impl(output, value, indent),
        _ => Ok(()),
    }
}

fn write_function(output: &mut String, function: &Function, indent: usize) -> OmniResult<()> {
    use std::fmt::Write;

    let mut signature = format!("fn {}(", function.name);
    for (index, param) in function.params.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
    }
    signature.push(')');
    if let Some(return_type) = &function.return_type {
        signature.push_str(&format!(" -> {}", type_to_string(return_type)));
    }
    signature.push(':');
    write_line(output, indent, &signature)?;
    write_block(output, &function.body, indent + 1)?;
    writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
    Ok(())
}

fn write_struct(output: &mut String, value: &Struct, indent: usize) -> OmniResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", value.name))?;
    if value.fields.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for field in &value.fields {
            let mut line = format!("{}: {}", field.name, type_to_string(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", expr_to_string(default)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
    Ok(())
}

fn write_enum(output: &mut String, value: &kain_core::ast::Enum, indent: usize) -> OmniResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", value.name))?;
    if value.variants.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for variant in &value.variants {
            let line = match &variant.fields {
                kain_core::ast::VariantFields::Unit => variant.name.clone(),
                kain_core::ast::VariantFields::Tuple(types) => {
                    let values = types
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{}({values})", variant.name)
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let values = fields
                        .iter()
                        .map(|field| format!("{}: {}", field.name, type_to_string(&field.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{ {values} }}", variant.name)
                }
            };
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
    Ok(())
}

fn write_impl(output: &mut String, value: &Impl, indent: usize) -> OmniResult<()> {
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
    writeln!(output).map_err(|err| OmniError::SourceGeneration(err.to_string()))?;
    Ok(())
}

fn write_block(output: &mut String, block: &Block, indent: usize) -> OmniResult<()> {
    if block.stmts.is_empty() {
        write_line(output, indent, "pass")?;
        return Ok(());
    }
    for stmt in &block.stmts {
        write_stmt(output, stmt, indent)?;
    }
    Ok(())
}

fn write_stmt(output: &mut String, stmt: &Stmt, indent: usize) -> OmniResult<()> {
    match stmt {
        Stmt::Let {
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
        Stmt::Expr(expr) => write_line(output, indent, &expr_to_string(expr)),
        Stmt::Return(value, _) => {
            if let Some(value) = value {
                write_line(output, indent, &format!("return {}", expr_to_string(value)))
            } else {
                write_line(output, indent, "return")
            }
        }
        _ => write_line(output, indent, "pass"),
    }
}

fn write_line(output: &mut String, indent: usize, line: &str) -> OmniResult<()> {
    use std::fmt::Write;

    writeln!(output, "{}{}", "    ".repeat(indent), line)
        .map_err(|err| OmniError::SourceGeneration(err.to_string()))
}

fn pattern_to_string(pattern: &Pattern) -> String {
    match pattern {
        Pattern::Wildcard(_) => "_".to_string(),
        Pattern::Binding { name, mutable, .. } => {
            if *mutable {
                format!("mut {name}")
            } else {
                name.clone()
            }
        }
        _ => "_".to_string(),
    }
}

fn expr_to_string(expr: &Expr) -> String {
    match expr {
        Expr::Int(value, _) => value.to_string(),
        Expr::Float(value, _) => value.to_string(),
        Expr::String(value, _) => format!("{:?}", value),
        Expr::Bool(value, _) => value.to_string(),
        Expr::None(_) => "none".to_string(),
        Expr::Ident(value, _) => value.clone(),
        _ => "none".to_string(),
    }
}

fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
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
        Type::Tuple(types, _) => format!(
            "({})",
            types
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Type::Array(inner, _, _) => format!("Array<{}>", type_to_string(inner)),
        Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        _ => "Any".to_string(),
    }
}

fn build_target(
    source: &str,
    resolved_entry: &Path,
    output: &Path,
    target: &OmniTarget,
) -> OmniResult<Vec<PathBuf>> {
    match target.kind {
        OmniTargetKind::GpuArtifacts => {
            let artifacts = compile_gpu_artifacts(source)?;
            write_gpu_artifacts_bundle(output, &artifacts)
        }
        OmniTargetKind::RustBundle => {
            let config = target.rust_bundle.clone().unwrap_or_default();
            let compiled = compile_rust_bundle(source, &config)?;
            let output_root = config
                .output
                .as_ref()
                .map(|value| {
                    if value.is_absolute() {
                        value.clone()
                    } else {
                        output
                            .parent()
                            .map(|parent| parent.join(value))
                            .unwrap_or_else(|| value.clone())
                    }
                })
                .unwrap_or_else(|| output.to_path_buf());
            let base_name = resolved_entry
                .file_stem()
                .and_then(|value| value.to_str())
                .unwrap_or("kain");
            write_rust_bundle_outputs(&output_root, base_name, &config, &compiled)
        }
        OmniTargetKind::Spirv => {
            let bytes = compile_spirv_binary(source)?;
            fs::write(output, bytes)?;
            Ok(vec![output.to_path_buf()])
        }
        _ => {
            let compile_target = compile_target_for_kind(&target.kind).ok_or_else(|| {
                OmniError::Config(format!("Unsupported target: {:?}", target.kind))
            })?;
            let compiled = compile_to_text(source, compile_target)?;
            let final_output = ensure_target_extension(output, compile_target);
            if let Some(parent) = final_output.parent() {
                fs::create_dir_all(parent)?;
            }
            fs::write(&final_output, compiled)?;
            Ok(vec![final_output])
        }
    }
}

fn compile_target_for_kind(kind: &OmniTargetKind) -> Option<CompileTarget> {
    match kind {
        OmniTargetKind::Rust => Some(CompileTarget::Rust),
        OmniTargetKind::Js => Some(CompileTarget::Js),
        OmniTargetKind::Ts => Some(CompileTarget::Ts),
        OmniTargetKind::Ks => Some(CompileTarget::Ks),
        OmniTargetKind::Cpp => Some(CompileTarget::Cpp),
        OmniTargetKind::Hlsl => Some(CompileTarget::Hlsl),
        OmniTargetKind::Usf => Some(CompileTarget::Usf),
        OmniTargetKind::Ue5 => Some(CompileTarget::Ue5),
        OmniTargetKind::Ue5Editor => Some(CompileTarget::Ue5Editor),
        OmniTargetKind::Spirv | OmniTargetKind::GpuArtifacts | OmniTargetKind::RustBundle => None,
    }
}

fn ensure_target_extension(path: &Path, target: CompileTarget) -> PathBuf {
    let expected = target_extension(target);
    match path.extension().and_then(|value| value.to_str()) {
        Some(ext) if ext.eq_ignore_ascii_case(expected) => path.to_path_buf(),
        _ => path.with_extension(expected),
    }
}

fn target_extension(target: CompileTarget) -> &'static str {
    driver::target_extension(target)
}

fn compile_to_text(source: &str, target: CompileTarget) -> OmniResult<String> {
    driver::compile(source, target).map_err(to_compiler_error)
}

fn compile_spirv_binary(source: &str) -> OmniResult<Vec<u8>> {
    driver::compile_spirv_binary(source).map_err(to_compiler_error)
}

fn compile_gpu_artifacts(source: &str) -> OmniResult<GpuArtifactOutput> {
    driver::compile_gpu_artifacts(source).map_err(to_compiler_error)
}

fn write_gpu_artifacts_bundle(
    output: &Path,
    artifacts: &GpuArtifactOutput,
) -> OmniResult<Vec<PathBuf>> {
    let spirv_path = with_file_name_suffix(output, "", "spv");
    let rust_path = with_file_name_suffix(output, ".gpu", "rs");
    let json_path = with_file_name_suffix(output, ".reflect", "json");
    let bundle_path = with_file_name_suffix(output, ".shader_bundle", "json");
    let hlsl_path = with_file_name_suffix(output, ".derived", "hlsl");
    for path in [
        &spirv_path,
        &rust_path,
        &json_path,
        &bundle_path,
        &hlsl_path,
    ] {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
    }
    fs::write(&spirv_path, &artifacts.spirv)?;
    fs::write(&rust_path, artifacts.rust_host.as_bytes())?;
    fs::write(&json_path, artifacts.reflection_json.as_bytes())?;
    fs::write(&bundle_path, artifacts.bundle_json.as_bytes())?;
    let mut written = vec![spirv_path, rust_path, json_path, bundle_path];
    if let Some(hlsl) = &artifacts.derived_hlsl {
        fs::write(&hlsl_path, hlsl.as_bytes())?;
        written.push(hlsl_path);
    }
    Ok(written)
}

fn compile_rust_bundle(
    source: &str,
    config: &OmniRustBundleConfig,
) -> OmniResult<RustBundleOutput> {
    driver::compile_rust_artifact_bundle(
        source,
        config.artifacts.contains(&OmniRustArtifact::Spirv),
    )
    .map_err(to_compiler_error)
}

fn write_rust_bundle_outputs(
    output_root: &Path,
    base_name: &str,
    config: &OmniRustBundleConfig,
    compiled: &RustBundleOutput,
) -> OmniResult<Vec<PathBuf>> {
    fs::create_dir_all(output_root)?;
    let mut written = Vec::new();
    if config.artifacts.contains(&OmniRustArtifact::Source) {
        let path = output_root.join(format!("{base_name}.rs"));
        fs::write(&path, compiled.bundle.primary.contents.as_bytes())?;
        written.push(path);
    }
    for artifact in &compiled.bundle.supplemental {
        let should_write = match artifact.kind {
            RustArtifactKind::PrimarySource => config.artifacts.contains(&OmniRustArtifact::Source),
            RustArtifactKind::ShaderHost => {
                config.artifacts.contains(&OmniRustArtifact::ShaderHost)
            }
            RustArtifactKind::ShaderReflection => config
                .artifacts
                .contains(&OmniRustArtifact::ShaderReflection),
        };
        if !should_write {
            continue;
        }
        let path = match artifact.kind {
            RustArtifactKind::PrimarySource => output_root.join(format!("{base_name}.rs")),
            RustArtifactKind::ShaderHost => output_root.join(format!("{base_name}.gpu.rs")),
            RustArtifactKind::ShaderReflection => {
                output_root.join(format!("{base_name}.reflect.json"))
            }
        };
        fs::write(&path, artifact.contents.as_bytes())?;
        written.push(path);
    }
    if config.artifacts.contains(&OmniRustArtifact::Spirv) {
        if let Some(spirv) = &compiled.spirv {
            let path = output_root.join(format!("{base_name}.spv"));
            fs::write(&path, spirv)?;
            written.push(path);
        }
    }
    Ok(written)
}

fn with_file_name_suffix(base: &Path, suffix: &str, extension: &str) -> PathBuf {
    let parent = base.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = base
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("artifact");
    parent.join(format!("{stem}{suffix}.{extension}"))
}

fn to_compiler_error(error: KainError) -> OmniError {
    OmniError::Compiler(error.to_string())
}

fn build_search_roots(
    project_root: &Path,
    staged_imports: &[StagedImport],
    config: &OmniImportResolution,
) -> Vec<PathBuf> {
    let mut roots = BTreeSet::new();
    roots.insert(project_root.to_path_buf());
    for root in &config.search_roots {
        roots.insert(resolve_from_root(project_root, root));
    }
    for staged in staged_imports {
        if let Some(parent) = staged.generated_kn_path.parent() {
            roots.insert(parent.to_path_buf());
        }
    }
    roots.into_iter().collect()
}

fn resolve_kain_program(entry: &Path, search_roots: &[PathBuf]) -> OmniResult<String> {
    let mut visited = BTreeSet::new();
    let mut ordered = Vec::new();
    resolve_kain_file(entry, search_roots, &mut visited, &mut ordered)?;
    let mut merged = String::new();
    for (index, content) in ordered.iter().enumerate() {
        if index > 0 {
            merged.push('\n');
        }
        merged.push_str(content);
        if !content.ends_with('\n') {
            merged.push('\n');
        }
    }
    Ok(merged)
}

fn resolve_kain_file(
    path: &Path,
    search_roots: &[PathBuf],
    visited: &mut BTreeSet<PathBuf>,
    ordered: &mut Vec<String>,
) -> OmniResult<()> {
    let canonical = fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    if !visited.insert(canonical.clone()) {
        return Ok(());
    }
    let source = fs::read_to_string(&canonical)?;
    let mut local_body = Vec::new();
    for line in source.lines() {
        if let Some(import_spec) = parse_quoted_import(line) {
            let resolved = resolve_quoted_import(&canonical, &import_spec, search_roots)?;
            resolve_kain_file(&resolved, search_roots, visited, ordered)?;
            continue;
        }
        local_body.push(line);
    }
    ordered.push(local_body.join("\n"));
    Ok(())
}

fn parse_quoted_import(line: &str) -> Option<String> {
    let trimmed = line.trim();
    if !trimmed.starts_with("import ") {
        return None;
    }
    let first_quote = trimmed.find('"')?;
    let rest = &trimmed[first_quote + 1..];
    let second_quote = rest.find('"')?;
    Some(rest[..second_quote].to_string())
}

fn resolve_quoted_import(
    current_file: &Path,
    import_spec: &str,
    search_roots: &[PathBuf],
) -> OmniResult<PathBuf> {
    let mut candidates = Vec::new();
    let import_path = PathBuf::from(import_spec);
    let dotted = PathBuf::from(import_spec.replace('.', "/"));
    if let Some(parent) = current_file.parent() {
        push_import_candidates(parent, &import_path, &dotted, &mut candidates);
    }
    for root in search_roots {
        push_import_candidates(root, &import_path, &dotted, &mut candidates);
    }
    for candidate in candidates {
        if candidate.exists() {
            return Ok(candidate);
        }
    }
    Err(OmniError::ImportResolution(format!(
        "Failed to resolve quoted import '{import_spec}' from {}",
        current_file.display()
    )))
}

fn push_import_candidates(base: &Path, import_path: &Path, dotted: &Path, out: &mut Vec<PathBuf>) {
    for path in [import_path, dotted] {
        out.push(base.join(path));
        out.push(base.join(path).with_extension("kn"));
        out.push(base.join(path).join("index.kn"));
    }
}

fn resolve_from_root(root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::Path;

    #[test]
    fn parses_quoted_import() {
        assert_eq!(
            parse_quoted_import("import \"Unreal.Core\""),
            Some("Unreal.Core".to_string())
        );
        assert_eq!(
            parse_quoted_import("  import \"paint/brush\"  "),
            Some("paint/brush".to_string())
        );
        assert_eq!(parse_quoted_import("use foo::bar"), None);
    }

    #[test]
    fn init_manifest_writes_loadable_default_manifest() {
        let dir = tempfile::tempdir().unwrap();

        let manifest_path = init_manifest(dir.path()).unwrap();
        let manifest = load_manifest(&manifest_path).unwrap();

        assert_eq!(manifest_path, dir.path().join("KAIN.omni.toml"));
        assert_eq!(manifest.project.entry, PathBuf::from("src/main.kn"));
        assert_eq!(manifest.project.build_dir, PathBuf::from("omni_out"));
        assert_eq!(manifest.targets.len(), 1);
        assert!(matches!(manifest.targets[0].kind, OmniTargetKind::Rust));
    }

    #[test]
    fn resolves_dotted_import_to_kn_file() {
        let dir = tempfile::tempdir().unwrap();
        let module_dir = dir.path().join("Unreal");
        fs::create_dir_all(&module_dir).unwrap();
        let module_path = module_dir.join("Core.kn");
        fs::write(&module_path, "fn core():\n    return 1\n").unwrap();
        let entry = dir.path().join("main.kn");
        fs::write(&entry, "import \"Unreal.Core\"\nfn main():\n    return 0\n").unwrap();

        let resolved =
            resolve_quoted_import(&entry, "Unreal.Core", &[dir.path().to_path_buf()]).unwrap();
        assert_eq!(resolved, module_path);
    }

    #[test]
    fn inlines_imported_files_before_entry_body() {
        let dir = tempfile::tempdir().unwrap();
        let shared = dir.path().join("shared.kn");
        let entry = dir.path().join("main.kn");
        fs::write(&shared, "fn helper():\n    return 7\n").unwrap();
        fs::write(
            &entry,
            "import \"shared.kn\"\nfn main():\n    return helper()\n",
        )
        .unwrap();

        let merged = resolve_kain_program(&entry, &[dir.path().to_path_buf()]).unwrap();
        assert!(merged.contains("fn helper():"));
        assert!(merged.contains("fn main():"));
        assert!(merged.find("fn helper():").unwrap() < merged.find("fn main():").unwrap());
    }

    #[test]
    fn build_manifest_path_resolves_relative_paths_from_manifest_directory() {
        let dir = tempfile::tempdir().unwrap();
        let project_root = dir.path().join("project");
        let src_dir = project_root.join("src");
        fs::create_dir_all(&src_dir).unwrap();

        fs::write(
            src_dir.join("main.kn"),
            "fn main() -> Int:\n    return 42\n",
        )
        .unwrap();

        let manifest = OmniManifest {
            project: OmniProject {
                entry: PathBuf::from("src/main.kn"),
                build_dir: PathBuf::from("build_artifacts"),
            },
            imports: Vec::new(),
            targets: vec![OmniTarget {
                kind: OmniTargetKind::Rust,
                output: PathBuf::from("dist/generated/app"),
                rust_bundle: None,
            }],
            import_resolution: OmniImportResolution::default(),
        };

        let manifest_path = project_root.join("KAIN.omni.toml");
        fs::write(&manifest_path, toml::to_string_pretty(&manifest).unwrap()).unwrap();

        let result = build_manifest_path(&manifest_path).unwrap();
        let resolved_entry = project_root.join(
            Path::new("build_artifacts")
                .join("resolved")
                .join("main.kn"),
        );
        let rust_output = project_root.join(Path::new("dist").join("generated").join("app.rs"));

        assert_eq!(result.resolved_entry, resolved_entry);
        assert!(resolved_entry.exists());
        assert!(rust_output.exists());
        assert!(result
            .written_outputs
            .iter()
            .any(|path| path == &rust_output));
    }

    #[test]
    fn build_emits_rust_from_import_aware_entry() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();
        let entry = root.join("main.kn");
        let shared = root.join("shared.kn");
        fs::write(&shared, "fn helper() -> Int:\n    return 7\n").unwrap();
        fs::write(
            &entry,
            "import \"shared.kn\"\nfn main() -> Int:\n    return helper()\n",
        )
        .unwrap();

        let manifest = OmniManifest {
            project: OmniProject {
                entry: PathBuf::from("main.kn"),
                build_dir: PathBuf::from("omni_out"),
            },
            imports: Vec::new(),
            targets: vec![OmniTarget {
                kind: OmniTargetKind::Rust,
                output: PathBuf::from("omni_out/generated/main"),
                rust_bundle: None,
            }],
            import_resolution: OmniImportResolution::default(),
        };

        let result = build(root, &manifest, None).unwrap();
        let rust_output = root.join("omni_out/generated/main.rs");
        assert!(rust_output.exists());
        assert!(result
            .written_outputs
            .iter()
            .any(|path| path == &rust_output));
    }
}
