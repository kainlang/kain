//! Strict self-host bootstrap lane for Rust -> KAIN import.
//!
//! This mode is intentionally stricter than the general-purpose Rust importer:
//! diagnostics become hard failures unless explicitly allow-listed.

use super::{parser, RustTransformer};
use crate::common::language_schema::KainLanguageSchema;
use crate::{ImportError, Result};
use kain_core::ast::Program;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, HashSet};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone)]
pub struct RustSelfHostOptions {
    pub include_tests: bool,
    pub allow_external_mod_decls: bool,
    pub schema: KainLanguageSchema,
    pub allowlist: SelfHostAllowlist,
    pub module_map: Option<SelfHostModuleMap>,
}

impl Default for RustSelfHostOptions {
    fn default() -> Self {
        Self {
            include_tests: false,
            allow_external_mod_decls: true,
            schema: KainLanguageSchema::bootstrap_core_schema(),
            allowlist: SelfHostAllowlist::default(),
            module_map: None,
        }
    }
}

impl RustSelfHostOptions {
    pub fn from_inventory_dir(inventory_dir: &Path) -> Result<Self> {
        let allowlist_path = inventory_dir.join("selfhost_allowlist.json");
        let module_map_path = inventory_dir.join("module_map.json");
        let allowlist: SelfHostAllowlist = serde_json::from_str(
            &std::fs::read_to_string(&allowlist_path).map_err(ImportError::IoError)?,
        )
        .map_err(|e| ImportError::TransformError(format!("failed to parse {}: {e}", allowlist_path.display())))?;
        let module_map: SelfHostModuleMap = serde_json::from_str(
            &std::fs::read_to_string(&module_map_path).map_err(ImportError::IoError)?,
        )
        .map_err(|e| ImportError::TransformError(format!("failed to parse {}: {e}", module_map_path.display())))?;

        Ok(Self {
            allow_external_mod_decls: true,
            schema: KainLanguageSchema::bootstrap_core_schema(),
            allowlist,
            module_map: Some(module_map),
            ..Self::default()
        })
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
        if let Some(module_map) = &options.module_map {
            if let Some(crate_spec) = module_map.find_crate(crate_root) {
                collect_modules_from_spec(crate_root, crate_spec, options, &mut modules, &mut entry_points)?;
            } else {
                collect_modules(crate_root, crate_root, options, &mut modules, &mut entry_points)?;
            }
        } else {
            collect_modules(crate_root, crate_root, options, &mut modules, &mut entry_points)?;
        }
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
        let mut tx = RustTransformer::with_options(options.transform_options());
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

fn collect_modules_from_spec(
    crate_root: &Path,
    spec: &SelfHostCrateSpec,
    options: &RustSelfHostOptions,
    modules: &mut Vec<RustModuleNode>,
    entry_points: &mut BTreeMap<String, PathBuf>,
) -> Result<()> {
    let src_dir = crate_root.join("src");
    let root_path = crate_root.join(relative_path_within_crate(&spec.root));
    if root_path.exists() {
        entry_points.insert("crate".to_string(), root_path.clone());
        modules.push(RustModuleNode {
            module_name: "crate".to_string(),
            file_path: root_path,
        });
    }

    for module in &spec.root_modules {
        let direct = src_dir.join(format!("{module}.rs"));
        let nested = src_dir.join(module).join("mod.rs");
        let file_path = if direct.exists() { direct } else { nested };
        if file_path.exists() {
            modules.push(RustModuleNode {
                module_name: module.clone(),
                file_path,
            });
        }
    }

    for (owner, children) in &spec.nested_modules {
        let owner_dir = owner.trim_end_matches("mod.rs").trim_end_matches('/').trim_end_matches('\\');
        let owner_path = src_dir.join(normalize_rel_path(owner_dir));
        for child in children {
            let direct = owner_path.join(format!("{child}.rs"));
            let nested = owner_path.join(child).join("mod.rs");
            let file_path = if direct.exists() { direct } else { nested };
            if file_path.exists() {
                modules.push(RustModuleNode {
                    module_name: format!("{owner_dir}::{child}"),
                    file_path,
                });
            }
        }
    }

    if modules.is_empty() {
        collect_modules(crate_root, crate_root, options, modules, entry_points)?;
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

fn normalize_rel_path(path: &str) -> PathBuf {
    let trimmed = path.strip_prefix("crates/").unwrap_or(path);
    PathBuf::from(trimmed.replace('/', "\\"))
}

fn relative_path_within_crate(path: &str) -> PathBuf {
    let normalized = path.replace('\\', "/");
    if let Some(idx) = normalized.find("/src/") {
        return PathBuf::from(normalized[idx + 1..].replace('/', "\\"));
    }
    if normalized == "src/lib.rs" || normalized == "src/main.rs" {
        return PathBuf::from(normalized.replace('/', "\\"));
    }
    PathBuf::from(normalized.replace('/', "\\"))
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SelfHostAllowlist {
    pub initial_slice: Vec<String>,
    pub phase1_acceptable_diagnostics: Vec<String>,
    pub hard_fail_conditions: Vec<String>,
    pub macro_policy: SelfHostMacroPolicy,
    pub trait_object_usage_count: usize,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SelfHostMacroPolicy {
    pub lower_directly: Vec<String>,
    pub preserve: Vec<String>,
    pub reject: Vec<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SelfHostModuleMap {
    pub crates: BTreeMap<String, SelfHostCrateSpec>,
    pub initial_slice: Vec<String>,
}

impl SelfHostModuleMap {
    fn find_crate(&self, crate_root: &Path) -> Option<&SelfHostCrateSpec> {
        let crate_name = crate_root.file_name().and_then(|n| n.to_str())?;
        self.crates.get(crate_name)
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SelfHostCrateSpec {
    pub root: String,
    pub root_modules: Vec<String>,
    pub nested_modules: BTreeMap<String, Vec<String>>,
    pub initial_selfhost_candidate: bool,
}

impl RustSelfHostOptions {
    fn transform_options(&self) -> super::transformer::RustTransformOptions {
        super::transformer::RustTransformOptions {
            strict_selfhost: true,
            macro_policy: super::transformer::RustMacroPolicy {
                lower_directly: self.allowlist.macro_policy.lower_directly.iter().cloned().collect::<HashSet<_>>(),
                preserve: self.allowlist.macro_policy.preserve.iter().cloned().collect::<HashSet<_>>(),
                reject: self.allowlist.macro_policy.reject.iter().cloned().collect::<HashSet<_>>(),
            },
        }
    }
}
