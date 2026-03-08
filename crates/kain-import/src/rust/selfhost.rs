//! Strict self-host bootstrap lane for Rust -> KAIN import.
//!
//! This mode is intentionally stricter than the general-purpose Rust importer:
//! diagnostics become hard failures unless explicitly allow-listed.

use super::{parser, RustTransformer};
use crate::common::language_schema::KainLanguageSchema;
use crate::{ImportError, Result};
use kain_core::ast::Program;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct RustSelfHostOptions {
    pub include_tests: bool,
    pub allow_external_mod_decls: bool,
    pub schema: KainLanguageSchema,
}

impl Default for RustSelfHostOptions {
    fn default() -> Self {
        Self {
            include_tests: false,
            allow_external_mod_decls: true,
            schema: KainLanguageSchema::bootstrap_core_schema(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct RustModuleNode {
    pub module_name: String,
    pub file_path: PathBuf,
}

#[derive(Debug, Clone)]
pub struct RustCrateGraph {
    pub crate_root: PathBuf,
    pub modules: Vec<RustModuleNode>,
    pub entry_points: BTreeMap<String, PathBuf>,
}

impl RustCrateGraph {
    pub fn discover(crate_root: &Path, options: &RustSelfHostOptions) -> Result<Self> {
        let mut modules = Vec::new();
        let mut entry_points = BTreeMap::new();
        collect_modules(crate_root, crate_root, options, &mut modules, &mut entry_points)?;
        modules.sort_by(|a, b| a.file_path.cmp(&b.file_path));
        Ok(Self {
            crate_root: crate_root.to_path_buf(),
            modules,
            entry_points,
        })
    }
}

pub fn import_rust_selfhost_dir(crate_root: &Path, options: &RustSelfHostOptions) -> Result<Program> {
    let graph = RustCrateGraph::discover(crate_root, options)?;
    let mut all_items = Vec::new();
    let mut diagnostics = Vec::new();

    for module in &graph.modules {
        let source = std::fs::read_to_string(&module.file_path).map_err(ImportError::IoError)?;
        let file = parser::parse_rust(&source, &module.file_path)?;
        let mut tx = RustTransformer::new_selfhost();
        let program = tx.transform(file)?;
        diagnostics.extend(
            tx.diagnostics
                .into_iter()
                .filter(|diag| !is_allowed_diagnostic(diag, options)),
        );
        all_items.extend(program.items);
    }

    if !diagnostics.is_empty() {
        return Err(ImportError::UnsupportedFeature(format!(
            "self-host import rejected {} diagnostic(s):\n{}",
            diagnostics.len(),
            diagnostics.join("\n")
        )));
    }

    Ok(Program {
        items: all_items,
        span: kain_core::span::Span::default(),
    })
}

fn collect_modules(
    root: &Path,
    dir: &Path,
    options: &RustSelfHostOptions,
    modules: &mut Vec<RustModuleNode>,
    entry_points: &mut BTreeMap<String, PathBuf>,
) -> Result<()> {
    for entry in std::fs::read_dir(dir).map_err(ImportError::IoError)? {
        let entry = entry.map_err(ImportError::IoError)?;
        let path = entry.path();
        if path.is_dir() {
            let name = path.file_name().and_then(|n| n.to_str()).unwrap_or_default();
            if !options.include_tests && matches!(name, "tests" | "benches" | "examples") {
                continue;
            }
            collect_modules(root, &path, options, modules, entry_points)?;
            continue;
        }

        if path.extension().and_then(|e| e.to_str()) != Some("rs") {
            continue;
        }

        if !options.include_tests && is_test_like_file(&path) {
            continue;
        }

        let module_name = module_name_for(root, &path);
        if matches!(path.file_name().and_then(|n| n.to_str()), Some("lib.rs" | "main.rs" | "mod.rs")) {
            entry_points.insert(module_name.clone(), path.clone());
        }
        modules.push(RustModuleNode { module_name, file_path: path });
    }
    Ok(())
}

fn module_name_for(root: &Path, file: &Path) -> String {
    let relative = file.strip_prefix(root).unwrap_or(file);
    let mut parts = relative
        .iter()
        .filter_map(|part| part.to_str())
        .map(str::to_string)
        .collect::<Vec<_>>();

    if let Some(last) = parts.last_mut() {
        if last == "mod.rs" {
            *last = "mod".to_string();
        } else if last.ends_with(".rs") {
            *last = last.trim_end_matches(".rs").to_string();
        }
    }

    parts.join("::")
}

fn is_test_like_file(path: &Path) -> bool {
    path.file_name()
        .and_then(|n| n.to_str())
        .is_some_and(|name| name.ends_with("_test.rs") || name.ends_with("_tests.rs"))
}

fn is_allowed_diagnostic(diag: &str, options: &RustSelfHostOptions) -> bool {
    if options.allow_external_mod_decls && diag.contains("external file") {
        return true;
    }
    false
}
