use kain_core::ast::{
    Actor, Component, ConvergeDef, Enum, Function, Impl, Item, Param, Shader, Trait, Type,
    TypeAlias, Visibility,
};
use kain_core::diagnostics::SpanMapper;
use kain_core::module_resolution::canonical_stdlib_module_name;
use kain_core::{Lexer, Parser, Span};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

pub const DEFAULT_JSON_OUT: &str = "stdlib/stdlib.map.json";
pub const DEFAULT_LLM_OUT: &str = "stdlib/STDLIB_MAP.llm.md";
pub const SCHEMA_VERSION: u32 = 1;
const LLM_ROW_WIDTH: usize = 118;

#[derive(Debug, Clone)]
pub struct StdlibMapOptions {
    pub repo_root: PathBuf,
    pub stdlib_root: PathBuf,
    pub native_manifests: Vec<PathBuf>,
    pub json_out: PathBuf,
    pub llm_out: PathBuf,
}

#[derive(Debug, Clone)]
pub struct GeneratedStdlibMap {
    pub map: StdlibMap,
    pub json: String,
    pub llm: String,
    pub json_path: PathBuf,
    pub llm_path: PathBuf,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct StdlibMap {
    pub schema_version: u32,
    pub generated_by: String,
    pub source_roots: Vec<String>,
    pub summary: StdlibMapSummary,
    pub modules: Vec<StdlibModule>,
    pub builtins: Vec<BuiltinSymbol>,
    pub native_services: Vec<NativeService>,
    pub cookbook: Vec<CookbookEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Default)]
pub struct StdlibMapSummary {
    pub module_count: usize,
    pub symbol_count: usize,
    pub public_symbol_count: usize,
    pub builtin_count: usize,
    pub native_service_count: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct StdlibModule {
    pub name: String,
    pub import_path: String,
    pub source_path: String,
    pub parse_status: ParseStatus,
    pub docs: Vec<String>,
    pub symbols: Vec<StdlibSymbol>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(tag = "status", rename_all = "snake_case")]
pub enum ParseStatus {
    Parsed,
    FallbackScanned { error: String },
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct StdlibSymbol {
    pub name: String,
    pub qualified_name: String,
    pub kind: String,
    pub visibility: String,
    pub signature: String,
    pub source_path: String,
    pub line: usize,
    pub attributes: Vec<String>,
    pub docs: Vec<String>,
    pub target_notes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BuiltinSymbol {
    pub name: String,
    pub signature: String,
    pub params: Vec<BuiltinParam>,
    pub return_type: String,
    pub doc: String,
    pub origin: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BuiltinParam {
    pub name: String,
    pub ty: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct NativeService {
    pub key: String,
    pub name: String,
    pub provider: String,
    pub requirement: String,
    pub status: String,
    pub platforms: Vec<String>,
    pub description: String,
    pub manifest_path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CookbookEntry {
    pub need: String,
    pub import_path: String,
    pub symbols: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct RuntimeManifest {
    #[serde(default)]
    services: Vec<RuntimeManifestService>,
}

#[derive(Debug, Deserialize)]
struct RuntimeManifestService {
    key: String,
    name: String,
    provider: String,
    requirement: String,
    status: String,
    #[serde(default)]
    platforms: Vec<String>,
    #[serde(default)]
    description: String,
}

impl StdlibMapOptions {
    pub fn from_repo_root(repo_root: PathBuf) -> Self {
        let stdlib_root = repo_root.join("stdlib");
        Self {
            json_out: repo_root.join(DEFAULT_JSON_OUT),
            llm_out: repo_root.join(DEFAULT_LLM_OUT),
            native_manifests: vec![
                repo_root.join("runtime/native_core_runtime.toml"),
                repo_root.join("runtime/native_runtime.toml"),
            ],
            repo_root,
            stdlib_root,
        }
    }

    pub fn with_stdlib_root(mut self, stdlib_root: Option<PathBuf>) -> Self {
        if let Some(path) = stdlib_root {
            self.stdlib_root = self.resolve_input_path(path);
        }
        self
    }

    pub fn with_native_manifests(mut self, manifests: Vec<PathBuf>) -> Self {
        if !manifests.is_empty() {
            self.native_manifests = manifests
                .into_iter()
                .map(|path| self.resolve_input_path(path))
                .collect();
        }
        self
    }

    pub fn with_json_out(mut self, json_out: Option<PathBuf>) -> Self {
        if let Some(path) = json_out {
            self.json_out = self.resolve_input_path(path);
        }
        self
    }

    pub fn with_llm_out(mut self, llm_out: Option<PathBuf>) -> Self {
        if let Some(path) = llm_out {
            self.llm_out = self.resolve_input_path(path);
        }
        self
    }

    fn resolve_input_path(&self, path: PathBuf) -> PathBuf {
        if path.is_absolute() {
            path
        } else {
            self.repo_root.join(path)
        }
    }
}

pub fn discover_repo_root(start: PathBuf) -> Result<PathBuf> {
    for candidate in start.ancestors() {
        if candidate.join("Cargo.toml").is_file() && candidate.join("stdlib").is_dir() {
            return Ok(candidate.to_path_buf());
        }
    }
    Err(format!("could not discover Kain repo root from {}", start.display()).into())
}

pub fn generate_stdlib_map(options: &StdlibMapOptions) -> Result<StdlibMap> {
    let mut modules = Vec::new();
    for path in collect_native_stdlib_files(&options.stdlib_root)? {
        modules.push(extract_module(options, &path)?);
    }
    modules.sort_by(|left, right| left.import_path.cmp(&right.import_path));

    let builtins = extract_builtins();
    let native_services = extract_native_services(options)?;
    let cookbook = build_cookbook(&modules);

    let symbol_count = modules.iter().map(|module| module.symbols.len()).sum();
    let public_symbol_count = modules
        .iter()
        .flat_map(|module| module.symbols.iter())
        .filter(|symbol| symbol.visibility == "public")
        .count();

    Ok(StdlibMap {
        schema_version: SCHEMA_VERSION,
        generated_by: "kain-stdlib-map".to_string(),
        source_roots: vec![
            format!("{}/*.kn", relative_path(options, &options.stdlib_root)),
            "crates/kain-core/src/stdlib.rs".to_string(),
            "runtime/native_core_runtime.toml".to_string(),
            "runtime/native_runtime.toml".to_string(),
        ],
        summary: StdlibMapSummary {
            module_count: modules.len(),
            symbol_count,
            public_symbol_count,
            builtin_count: builtins.len(),
            native_service_count: native_services.len(),
        },
        modules,
        builtins,
        native_services,
        cookbook,
    })
}

pub fn render_llm_markdown(map: &StdlibMap) -> String {
    let mut out = String::new();
    out.push_str("# Kain Native Stdlib Map\n\n");
    out.push_str("Generated by `kain stdlib-map`. Do not edit by hand.\n");
    out.push_str("Scope: LLVM/native root profile only. Indexed source is top-level `stdlib/*.kn`; target/vendor overlays such as `stdlib/ue5`, `stdlib/python`, `stdlib/javascript`, and `stdlib/c` are intentionally excluded.\n\n");
    out.push_str(&format!(
        "summary: modules={} | public_symbols={} | total_symbols={} | rust_builtins={} | native_services={}\n\n",
        map.summary.module_count,
        map.summary.public_symbol_count,
        map.summary.symbol_count,
        map.summary.builtin_count,
        map.summary.native_service_count
    ));

    out.push_str("## Use First\n\n");
    out.push_str("- Import root domains directly: `use std::fs`, `use std::net`, `use std::math`, `use std::ui`.\n");
    out.push_str("- Prefer public symbols below. Private symbols are kept in `stdlib.map.json` for stdlib-internal maintenance, not normal authoring.\n");
    out.push_str("- `@extern` means native/runtime-backed ABI surface; wrap it unless you are editing the runtime substrate.\n\n");

    out.push_str("## Imports\n\n");
    for row in render_symbol_rows(
        &map.modules
            .iter()
            .map(|module| format!("use {}", module.import_path))
            .collect::<Vec<_>>(),
        usize::MAX,
    ) {
        out.push_str(&format!("- {}\n", row.0));
    }
    out.push('\n');

    out.push_str("## Cook By Need\n\n");
    for entry in &map.cookbook {
        out.push_str(&format!(
            "- {}: `{}` -> `{}`\n",
            entry.need,
            entry.import_path,
            entry.symbols.join("`, `")
        ));
    }
    out.push('\n');

    out.push_str("## Modules\n\n");
    for module in &map.modules {
        let public_symbols = module
            .symbols
            .iter()
            .filter(|symbol| symbol.visibility == "public")
            .collect::<Vec<_>>();
        let private_count = module.symbols.len().saturating_sub(public_symbols.len());
        out.push_str(&format!(
            "### `{}` ({}, public={}, private={})\n\n",
            module.import_path,
            module.source_path,
            public_symbols.len(),
            private_count
        ));
        match &module.parse_status {
            ParseStatus::Parsed => {}
            ParseStatus::FallbackScanned { error } => {
                let first_line = error.lines().next().unwrap_or(error);
                out.push_str(&format!("- fallback-scanned: {}\n", first_line))
            }
        }
        if public_symbols.is_empty() {
            out.push_str("_No indexed symbols._\n\n");
            continue;
        }
        for (title, symbols) in module_public_sections(&public_symbols) {
            out.push_str(&format!("- {}:\n", title));
            for row in render_symbol_rows(&symbols, usize::MAX) {
                out.push_str(&format!("  - {}\n", row.0));
            }
        }
        out.push('\n');
    }

    out.push_str("## Rust Builtins\n\n");
    let builtin_rows = map
        .builtins
        .iter()
        .map(|builtin| builtin.signature.clone())
        .collect::<Vec<_>>();
    for row in render_symbol_rows(&builtin_rows, usize::MAX) {
        out.push_str(&format!("- {}\n", row.0));
    }

    out.push_str("\n## Native Runtime Services\n\n");
    for service in &map.native_services {
        let platforms = if service.platforms.is_empty() {
            "all declared platforms".to_string()
        } else {
            service.platforms.join(", ")
        };
        out.push_str(&format!(
            "- `{}`: {} {} on {}",
            service.key, service.status, service.requirement, platforms
        ));
        if !service.description.is_empty() {
            out.push_str(&format!(" - {}", service.description));
        }
        out.push('\n');
    }
    out
}

fn module_public_sections(symbols: &[&StdlibSymbol]) -> Vec<(&'static str, Vec<String>)> {
    let mut sections = ordered_symbol_buckets();
    for symbol in symbols {
        if let Some(bucket) = symbol_bucket(&symbol.kind) {
            push_unique(
                &mut sections.entry(bucket).or_default(),
                symbol_label(symbol),
            );
        }
    }
    [
        "constants",
        "types",
        "traits",
        "components",
        "actors",
        "worlds",
        "functions",
        "methods",
        "patches",
        "laws",
        "converges",
        "orchestrates",
        "shaders",
    ]
    .into_iter()
    .filter_map(|bucket| {
        sections
            .remove(bucket)
            .filter(|symbols| !symbols.is_empty())
            .map(|symbols| (bucket, symbols))
    })
    .collect()
}

fn ordered_symbol_buckets() -> BTreeMap<&'static str, Vec<String>> {
    let mut buckets = BTreeMap::new();
    for bucket in [
        "constants",
        "types",
        "traits",
        "components",
        "actors",
        "worlds",
        "functions",
        "methods",
        "patches",
        "laws",
        "converges",
        "orchestrates",
        "shaders",
    ] {
        buckets.insert(bucket, Vec::new());
    }
    buckets
}

fn symbol_bucket(kind: &str) -> Option<&'static str> {
    match kind {
        "const" => Some("constants"),
        "struct" | "enum" | "type_alias" => Some("types"),
        "trait" => Some("traits"),
        "component" => Some("components"),
        "actor" => Some("actors"),
        "world" => Some("worlds"),
        "function" => Some("functions"),
        "method" => Some("methods"),
        "patch" => Some("patches"),
        "law" => Some("laws"),
        "converge" => Some("converges"),
        "orchestrate" => Some("orchestrates"),
        "shader" => Some("shaders"),
        _ => None,
    }
}

fn symbol_label(symbol: &StdlibSymbol) -> String {
    let mut label = format!("{} @{}", symbol.signature, symbol.line);
    if symbol.attributes.iter().any(|attr| attr == "extern") {
        label.push_str(" @extern");
    }
    label
}

fn render_symbol_rows(symbols: &[String], remaining_symbols: usize) -> Vec<(String, usize)> {
    let mut rows = Vec::new();
    let mut current = String::new();
    let mut current_count = 0usize;
    for symbol in symbols.iter().take(remaining_symbols) {
        let candidate = if current.is_empty() {
            symbol.clone()
        } else {
            format!("{current}, {symbol}")
        };
        if !current.is_empty() && candidate.len() > LLM_ROW_WIDTH {
            rows.push((current, current_count));
            current = symbol.clone();
            current_count = 1;
            continue;
        }
        current = candidate;
        current_count += 1;
    }
    if !current.is_empty() {
        rows.push((current, current_count));
    }
    rows
}

fn push_unique(symbols: &mut Vec<String>, symbol: String) {
    if !symbols.iter().any(|existing| existing == &symbol) {
        symbols.push(symbol);
    }
}

pub fn write_generated_files(options: &StdlibMapOptions) -> Result<GeneratedStdlibMap> {
    let report = render_generated(options)?;
    if let Some(parent) = report.json_path.parent() {
        fs::create_dir_all(parent)?;
    }
    if let Some(parent) = report.llm_path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&report.json_path, &report.json)?;
    fs::write(&report.llm_path, &report.llm)?;
    Ok(report)
}

pub fn check_generated_files(options: &StdlibMapOptions) -> Result<()> {
    let report = render_generated(options)?;
    check_one(&report.json_path, &report.json)?;
    check_one(&report.llm_path, &report.llm)?;
    Ok(())
}

fn render_generated(options: &StdlibMapOptions) -> Result<GeneratedStdlibMap> {
    let map = generate_stdlib_map(options)?;
    let json = serde_json::to_string(&map)? + "\n";
    let llm = render_llm_markdown(&map);
    Ok(GeneratedStdlibMap {
        map,
        json,
        llm,
        json_path: options.json_out.clone(),
        llm_path: options.llm_out.clone(),
    })
}

fn check_one(path: &Path, expected: &str) -> Result<()> {
    let actual = fs::read_to_string(path).map_err(|err| {
        format!(
            "failed to read checked generated file {}: {err}",
            path.display()
        )
    })?;
    if actual != expected {
        return Err(format!("{} is stale; run `kain stdlib-map --write`", path.display()).into());
    }
    Ok(())
}

fn collect_native_stdlib_files(root: &Path) -> Result<Vec<PathBuf>> {
    let mut out = Vec::new();
    for entry in fs::read_dir(root)? {
        let entry = entry?;
        let path = entry.path();
        let file_type = entry.file_type()?;
        if file_type.is_file() && path.extension().and_then(|ext| ext.to_str()) == Some("kn") {
            out.push(path);
        }
    }
    out.sort();
    Ok(out)
}

fn extract_module(options: &StdlibMapOptions, path: &Path) -> Result<StdlibModule> {
    let source = fs::read_to_string(path)?;
    let relative = relative_path(options, path);
    let module_name = module_name(options, path);
    let import_path = format!("std::{}", module_name.replace('/', "::"));
    let docs = leading_comments(&source);
    let parsed = parse_module(&relative, &source);
    let (parse_status, mut symbols) = match parsed {
        Ok(items) => (
            ParseStatus::Parsed,
            extract_ast_symbols(options, path, &source, &module_name, &items),
        ),
        Err(err) => (
            ParseStatus::FallbackScanned {
                error: err.to_string(),
            },
            fallback_scan_symbols(options, path, &source, &module_name),
        ),
    };
    symbols.sort_by(|left, right| {
        left.line
            .cmp(&right.line)
            .then_with(|| left.qualified_name.cmp(&right.qualified_name))
    });
    Ok(StdlibModule {
        name: module_name,
        import_path,
        source_path: relative,
        parse_status,
        docs,
        symbols,
    })
}

fn parse_module(filename: &str, source: &str) -> Result<Vec<Item>> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &span_mapper, filename).parse()?;
    Ok(program.items)
}

fn extract_ast_symbols(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    items: &[Item],
) -> Vec<StdlibSymbol> {
    let mut symbols = Vec::new();
    for item in items {
        match item {
            Item::Function(function) => symbols.push(symbol_for_function(
                options, path, source, module, function, "function", None,
            )),
            Item::Converge(converge) => {
                symbols.push(symbol_for_converge(options, path, source, module, converge))
            }
            Item::Struct(value) => {
                symbols.push(symbol_for_named_type(
                    options,
                    path,
                    source,
                    module,
                    &value.name,
                    "struct",
                    value.visibility,
                    value.span,
                    &value.attributes,
                    format!("struct {}", value.name),
                ));
                for method in &value.methods {
                    symbols.push(symbol_for_function(
                        options,
                        path,
                        source,
                        module,
                        method,
                        "method",
                        Some(&value.name),
                    ));
                }
            }
            Item::Enum(value) => {
                symbols.push(symbol_for_enum(options, path, source, module, value))
            }
            Item::Trait(value) => {
                symbols.push(symbol_for_trait(options, path, source, module, value))
            }
            Item::Impl(value) => {
                symbols.extend(symbols_for_impl(options, path, source, module, value))
            }
            Item::TypeAlias(value) => {
                symbols.push(symbol_for_type_alias(options, path, source, module, value))
            }
            Item::Const(value) => symbols.push(StdlibSymbol {
                name: value.name.clone(),
                qualified_name: qualify(module, &value.name),
                kind: "const".to_string(),
                visibility: visibility_name(value.visibility),
                signature: format!("const {}: {}", value.name, type_to_string(&value.ty)),
                source_path: relative_path(options, path),
                line: line_for_span(source, value.span),
                attributes: Vec::new(),
                docs: docs_before_span(source, value.span),
                target_notes: Vec::new(),
            }),
            Item::Actor(value) => {
                symbols.push(symbol_for_actor(options, path, source, module, value))
            }
            Item::Component(value) => {
                symbols.push(symbol_for_component(options, path, source, module, value))
            }
            Item::Shader(value) => {
                symbols.push(symbol_for_shader(options, path, source, module, value))
            }
            Item::World(value) => symbols.push(StdlibSymbol {
                name: value.name.clone(),
                qualified_name: qualify(module, &value.name),
                kind: "world".to_string(),
                visibility: visibility_name(value.visibility),
                signature: format!("world {}", value.name),
                source_path: relative_path(options, path),
                line: line_for_span(source, value.span),
                attributes: attr_names(&value.attributes),
                docs: docs_before_span(source, value.span),
                target_notes: Vec::new(),
            }),
            _ => {}
        }
    }
    symbols
}

fn fallback_scan_symbols(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
) -> Vec<StdlibSymbol> {
    let mut symbols = Vec::new();
    let mut pending_attrs = Vec::new();
    for (index, raw_line) in source.lines().enumerate() {
        let line = raw_line.trim();
        if line.starts_with('@') {
            pending_attrs.push(line.trim_start_matches('@').to_string());
            continue;
        }
        let Some((kind, rest)) = declaration_kind_and_rest(line) else {
            if !line.is_empty() && !line.starts_with("//") {
                pending_attrs.clear();
            }
            continue;
        };
        let name = rest
            .split(|ch: char| !(ch.is_alphanumeric() || ch == '_'))
            .next()
            .unwrap_or("")
            .to_string();
        if name.is_empty() {
            continue;
        }
        let visibility = if line.starts_with("pub ") {
            "public"
        } else {
            "private"
        };
        let attributes = std::mem::take(&mut pending_attrs);
        symbols.push(StdlibSymbol {
            name: name.clone(),
            qualified_name: qualify(module, &name),
            kind: kind.to_string(),
            visibility: visibility.to_string(),
            signature: line.trim_end_matches(':').to_string(),
            source_path: relative_path(options, path),
            line: index + 1,
            target_notes: target_notes_for_attrs(&attributes),
            attributes,
            docs: docs_before_line(source, index + 1),
        });
    }
    symbols
}

fn declaration_kind_and_rest(line: &str) -> Option<(&'static str, &str)> {
    let line = line.strip_prefix("pub ").unwrap_or(line);
    for kind in [
        "fn",
        "struct",
        "enum",
        "trait",
        "const",
        "actor",
        "world",
        "component",
        "shader",
        "converge",
        "law",
        "patch",
        "pulse",
        "axiom",
        "type",
    ] {
        if let Some(rest) = line
            .strip_prefix(kind)
            .and_then(|rest| rest.strip_prefix(' '))
        {
            return Some((kind, rest));
        }
    }
    None
}

fn symbol_for_function(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    function: &Function,
    kind: &str,
    owner: Option<&str>,
) -> StdlibSymbol {
    let name = match owner {
        Some(owner) => format!("{owner}.{}", function.name),
        None => function.name.clone(),
    };
    let attrs = attr_names(&function.attributes);
    StdlibSymbol {
        name: function.name.clone(),
        qualified_name: qualify(module, &name),
        kind: if attrs.iter().any(|attr| attr == "extern") {
            "extern_function".to_string()
        } else {
            kind.to_string()
        },
        visibility: visibility_name(function.visibility),
        signature: function_signature(
            &function.name,
            &function.params,
            function.return_type.as_ref(),
        ),
        source_path: relative_path(options, path),
        line: line_for_span(source, function.span),
        target_notes: target_notes_for_attrs(&attrs),
        attributes: attrs,
        docs: docs_before_span(source, function.span),
    }
}

fn symbol_for_converge(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    converge: &ConvergeDef,
) -> StdlibSymbol {
    StdlibSymbol {
        name: converge.name.clone(),
        qualified_name: qualify(module, &converge.name),
        kind: "converge".to_string(),
        visibility: visibility_name(converge.visibility),
        signature: format!(
            "converge {}",
            function_signature(
                &converge.name,
                &converge.params,
                converge.return_type.as_ref()
            )
            .trim_start_matches("fn ")
        ),
        source_path: relative_path(options, path),
        line: line_for_span(source, converge.span),
        attributes: attr_names(&converge.attributes),
        docs: docs_before_span(source, converge.span),
        target_notes: vec!["semantic selector with spec/fast lanes".to_string()],
    }
}

fn symbol_for_named_type(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    name: &str,
    kind: &str,
    visibility: Visibility,
    span: Span,
    attributes: &[kain_core::ast::Attribute],
    signature: String,
) -> StdlibSymbol {
    StdlibSymbol {
        name: name.to_string(),
        qualified_name: qualify(module, name),
        kind: kind.to_string(),
        visibility: visibility_name(visibility),
        signature,
        source_path: relative_path(options, path),
        line: line_for_span(source, span),
        attributes: attr_names(attributes),
        docs: docs_before_span(source, span),
        target_notes: Vec::new(),
    }
}

fn symbol_for_enum(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Enum,
) -> StdlibSymbol {
    symbol_for_named_type(
        options,
        path,
        source,
        module,
        &value.name,
        "enum",
        value.visibility,
        value.span,
        &[],
        format!("enum {}", value.name),
    )
}

fn symbol_for_trait(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Trait,
) -> StdlibSymbol {
    symbol_for_named_type(
        options,
        path,
        source,
        module,
        &value.name,
        "trait",
        value.visibility,
        value.span,
        &[],
        format!("trait {}", value.name),
    )
}

fn symbols_for_impl(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Impl,
) -> Vec<StdlibSymbol> {
    let owner = type_to_string(&value.target_type);
    value
        .methods
        .iter()
        .map(|method| {
            symbol_for_function(
                options,
                path,
                source,
                module,
                method,
                "impl_method",
                Some(&owner),
            )
        })
        .collect()
}

fn symbol_for_type_alias(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &TypeAlias,
) -> StdlibSymbol {
    StdlibSymbol {
        name: value.name.clone(),
        qualified_name: qualify(module, &value.name),
        kind: "type_alias".to_string(),
        visibility: visibility_name(value.visibility),
        signature: format!("type {} = {}", value.name, type_to_string(&value.target)),
        source_path: relative_path(options, path),
        line: line_for_span(source, value.span),
        attributes: Vec::new(),
        docs: docs_before_span(source, value.span),
        target_notes: Vec::new(),
    }
}

fn symbol_for_actor(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Actor,
) -> StdlibSymbol {
    StdlibSymbol {
        name: value.name.clone(),
        qualified_name: qualify(module, &value.name),
        kind: "actor".to_string(),
        visibility: "public".to_string(),
        signature: format!("actor {}", value.name),
        source_path: relative_path(options, path),
        line: line_for_span(source, value.span),
        attributes: attr_names(&value.attributes),
        docs: docs_before_span(source, value.span),
        target_notes: vec!["actor runtime semantics".to_string()],
    }
}

fn symbol_for_component(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Component,
) -> StdlibSymbol {
    StdlibSymbol {
        name: value.name.clone(),
        qualified_name: qualify(module, &value.name),
        kind: "component".to_string(),
        visibility: visibility_name(value.visibility),
        signature: function_signature(&value.name, &value.props, None).replace("fn ", "component "),
        source_path: relative_path(options, path),
        line: line_for_span(source, value.span),
        attributes: attr_names(&value.attributes),
        docs: docs_before_span(source, value.span),
        target_notes: vec!["UI/component authoring surface".to_string()],
    }
}

fn symbol_for_shader(
    options: &StdlibMapOptions,
    path: &Path,
    source: &str,
    module: &str,
    value: &Shader,
) -> StdlibSymbol {
    StdlibSymbol {
        name: value.name.clone(),
        qualified_name: qualify(module, &value.name),
        kind: "shader".to_string(),
        visibility: "public".to_string(),
        signature: function_signature(&value.name, &value.inputs, Some(&value.outputs))
            .replace("fn ", &format!("shader {:?} ", value.stage).to_lowercase()),
        source_path: relative_path(options, path),
        line: line_for_span(source, value.span),
        attributes: Vec::new(),
        docs: docs_before_span(source, value.span),
        target_notes: vec!["GPU shader surface".to_string()],
    }
}

fn extract_builtins() -> Vec<BuiltinSymbol> {
    let lib = kain_core::stdlib::StdLib::new();
    let mut builtins: Vec<_> = lib
        .functions
        .values()
        .map(|function| {
            let params: Vec<_> = function
                .params
                .iter()
                .map(|(name, ty)| BuiltinParam {
                    name: (*name).to_string(),
                    ty: (*ty).to_string(),
                })
                .collect();
            let param_text = params
                .iter()
                .map(|param| format!("{}: {}", param.name, param.ty))
                .collect::<Vec<_>>()
                .join(", ");
            BuiltinSymbol {
                name: function.name.to_string(),
                signature: format!(
                    "fn {}({}) -> {}",
                    function.name, param_text, function.return_type
                ),
                params,
                return_type: function.return_type.to_string(),
                doc: function.doc.to_string(),
                origin: "crates/kain-core/src/stdlib.rs".to_string(),
            }
        })
        .collect();
    builtins.sort_by(|left, right| left.name.cmp(&right.name));
    builtins
}

fn extract_native_services(options: &StdlibMapOptions) -> Result<Vec<NativeService>> {
    let mut services_by_key = BTreeMap::<String, NativeService>::new();
    for manifest_path in &options.native_manifests {
        if !manifest_path.is_file() {
            continue;
        }
        let text = fs::read_to_string(manifest_path)?;
        let manifest: RuntimeManifest = toml::from_str(&text)?;
        for service in manifest.services {
            services_by_key
                .entry(service.key.clone())
                .or_insert(NativeService {
                    key: service.key,
                    name: service.name,
                    provider: service.provider,
                    requirement: service.requirement,
                    status: service.status,
                    platforms: service.platforms,
                    description: service.description,
                    manifest_path: relative_path(options, manifest_path),
                });
        }
    }
    Ok(services_by_key.into_values().collect())
}

fn build_cookbook(modules: &[StdlibModule]) -> Vec<CookbookEntry> {
    let mut by_module = BTreeMap::<String, BTreeSet<String>>::new();
    for module in modules {
        let public_symbols = module
            .symbols
            .iter()
            .filter(|symbol| symbol.visibility == "public")
            .map(|symbol| symbol.name.clone())
            .take(8)
            .collect::<BTreeSet<_>>();
        by_module.insert(module.import_path.clone(), public_symbols);
    }

    let seeds = [
        ("Read/write files and paths", "std::fs"),
        ("Spawn processes and capture output", "std::process"),
        ("Open network/http/tls lanes", "std::net"),
        ("Build deterministic hashes and fingerprints", "std::hash"),
        ("Use actors and mailbox helpers", "std::actor"),
        ("Create native graphics sessions", "std::graphics"),
        ("Use GPU shared-residency contracts", "std::gpu"),
        ("Author graphics-shared bindings", "std::graphics::shared"),
        ("Author UI handles and component helpers", "std::ui"),
        (
            "Use math vectors, matrices, quaternions, and colors",
            "std::math",
        ),
    ];
    seeds
        .iter()
        .filter_map(|(need, import_path)| {
            by_module.get(*import_path).map(|symbols| CookbookEntry {
                need: (*need).to_string(),
                import_path: (*import_path).to_string(),
                symbols: symbols.iter().cloned().collect(),
            })
        })
        .collect()
}

fn function_signature(name: &str, params: &[Param], return_type: Option<&Type>) -> String {
    let params = params
        .iter()
        .map(|param| format!("{}: {}", param.name, type_to_string(&param.ty)))
        .collect::<Vec<_>>()
        .join(", ");
    match return_type {
        Some(ty) => format!("fn {name}({params}) -> {}", type_to_string(ty)),
        None => format!("fn {name}({params})"),
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
        Type::Array(inner, size, _) => format!("[{}; {}]", type_to_string(inner), size),
        Type::Slice(inner, _) => format!("[{}]", type_to_string(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", type_to_string(inner))
            } else {
                format!("ptr<{}>", type_to_string(inner))
            }
        }
        Type::Function {
            params,
            return_type,
            ..
        } => format!(
            "fn({}) -> {}",
            params
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", "),
            type_to_string(return_type)
        ),
        Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        Type::Result(ok, err, _) => {
            format!("Result<{}, {}>", type_to_string(ok), type_to_string(err))
        }
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "!".to_string(),
        Type::Unit(_) => "()".to_string(),
        Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {trait_name}")
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

fn attr_names(attributes: &[kain_core::ast::Attribute]) -> Vec<String> {
    attributes.iter().map(|attr| attr.name.clone()).collect()
}

fn target_notes_for_attrs(attributes: &[String]) -> Vec<String> {
    if attributes.iter().any(|attr| attr == "extern") {
        vec!["native/runtime/import-backed declaration".to_string()]
    } else {
        Vec::new()
    }
}

fn visibility_name(visibility: Visibility) -> String {
    match visibility {
        Visibility::Private => "private",
        Visibility::Public => "public",
        Visibility::Crate => "crate",
        Visibility::Super => "super",
    }
    .to_string()
}

fn module_name(options: &StdlibMapOptions, path: &Path) -> String {
    let relative = path.strip_prefix(&options.stdlib_root).unwrap_or(path);
    let without_ext = relative.with_extension("");
    canonical_stdlib_module_name(&without_ext.to_string_lossy().replace('\\', "/"))
}

fn qualify(module: &str, name: &str) -> String {
    format!("std::{}::{}", module.replace('/', "::"), name)
}

fn relative_path(options: &StdlibMapOptions, path: &Path) -> String {
    path.strip_prefix(&options.repo_root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

fn line_for_span(source: &str, span: Span) -> usize {
    source[..span.start.min(source.len())]
        .bytes()
        .filter(|byte| *byte == b'\n')
        .count()
        + 1
}

fn docs_before_span(source: &str, span: Span) -> Vec<String> {
    docs_before_line(source, line_for_span(source, span))
}

fn docs_before_line(source: &str, line: usize) -> Vec<String> {
    let lines: Vec<_> = source.lines().collect();
    if line <= 1 || line - 1 > lines.len() {
        return Vec::new();
    }
    let mut docs = Vec::new();
    let mut index = line - 1;
    while index > 0 {
        index -= 1;
        let trimmed = lines[index].trim();
        if let Some(doc) = trimmed.strip_prefix("///") {
            docs.push(doc.trim().to_string());
        } else if let Some(doc) = trimmed.strip_prefix("//!") {
            docs.push(doc.trim().to_string());
        } else if let Some(doc) = trimmed.strip_prefix("//") {
            docs.push(doc.trim().to_string());
        } else if trimmed.is_empty() && docs.is_empty() {
            continue;
        } else {
            break;
        }
    }
    docs.reverse();
    docs
}

fn leading_comments(source: &str) -> Vec<String> {
    let mut docs = Vec::new();
    for line in source.lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            if docs.is_empty() {
                continue;
            }
            break;
        }
        if let Some(doc) = trimmed.strip_prefix("//!") {
            docs.push(doc.trim().to_string());
        } else if let Some(doc) = trimmed.strip_prefix("//") {
            docs.push(doc.trim().to_string());
        } else {
            break;
        }
    }
    docs
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn type_rendering_handles_nested_named_types() {
        let span = Span::new(0, 0);
        let ty = Type::Named {
            name: "Result".to_string(),
            generics: vec![
                Type::Named {
                    name: "Int".to_string(),
                    generics: Vec::new(),
                    span,
                },
                Type::Named {
                    name: "String".to_string(),
                    generics: Vec::new(),
                    span,
                },
            ],
            span,
        };
        assert_eq!(type_to_string(&ty), "Result<Int, String>");
    }

    #[test]
    fn native_stdlib_collection_ignores_target_subdirectories() {
        let root =
            std::env::temp_dir().join(format!("kain-stdlib-map-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("ue5")).unwrap();
        fs::write(
            root.join("fs.kn"),
            "pub fn fs_ping() -> Int:\n    return 1\n",
        )
        .unwrap();
        fs::write(
            root.join("ue5").join("shaders.kn"),
            "pub fn should_not_index() -> Int:\n    return 0\n",
        )
        .unwrap();

        let files = collect_native_stdlib_files(&root).unwrap();
        let names = files
            .iter()
            .map(|path| path.file_name().unwrap().to_string_lossy().to_string())
            .collect::<Vec<_>>();
        assert_eq!(names, vec!["fs.kn"]);
        fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn llm_markdown_is_public_compact_and_native_scoped() {
        let map = StdlibMap {
            schema_version: SCHEMA_VERSION,
            generated_by: "test".to_string(),
            source_roots: vec!["stdlib/*.kn".to_string()],
            summary: StdlibMapSummary {
                module_count: 1,
                symbol_count: 2,
                public_symbol_count: 1,
                builtin_count: 0,
                native_service_count: 0,
            },
            modules: vec![StdlibModule {
                name: "fs".to_string(),
                import_path: "std::fs".to_string(),
                source_path: "stdlib/fs.kn".to_string(),
                parse_status: ParseStatus::Parsed,
                docs: Vec::new(),
                symbols: vec![
                    StdlibSymbol {
                        name: "fs_read_text".to_string(),
                        qualified_name: "std::fs::fs_read_text".to_string(),
                        kind: "function".to_string(),
                        visibility: "public".to_string(),
                        signature: "fn fs_read_text(path: String) -> String".to_string(),
                        source_path: "stdlib/fs.kn".to_string(),
                        line: 7,
                        attributes: Vec::new(),
                        docs: Vec::new(),
                        target_notes: Vec::new(),
                    },
                    StdlibSymbol {
                        name: "fs_secret".to_string(),
                        qualified_name: "std::fs::fs_secret".to_string(),
                        kind: "function".to_string(),
                        visibility: "private".to_string(),
                        signature: "fn fs_secret() -> Int".to_string(),
                        source_path: "stdlib/fs.kn".to_string(),
                        line: 99,
                        attributes: Vec::new(),
                        docs: Vec::new(),
                        target_notes: Vec::new(),
                    },
                ],
            }],
            builtins: Vec::new(),
            native_services: Vec::new(),
            cookbook: Vec::new(),
        };
        let rendered = render_llm_markdown(&map);
        assert!(rendered.contains("LLVM/native root profile only"));
        assert!(rendered.contains("fn fs_read_text(path: String) -> String @7"));
        assert!(!rendered.contains("fs_secret"));
    }
}
