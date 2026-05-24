//! Rust language importer — Project Ouroboros (Import Path)
//!
//! Transforms Rust source into KAIN AST using the `syn` crate.
//!
//! ## The Reflexive Import Bootstrap Pipeline
//!
//! Instead of manually rewriting the KAIN compiler in KAIN (traditional bootstrap),
//! this importer enables a reverse pipeline:
//!
//! ```text
//! kain-core/*.rs  →  kain import-rust --flat  →  kain-core.kn
//!                                                      ↓
//!                                            kain build -t rust
//!                                                      ↓
//!                                            kain-core-generated.rs
//!                                                      ↓
//!                                         (compiles + tests pass = ✅ self-hosted)
//! ```
//!
//! The translation ring: Rust → KAIN → Rust closes without a single manual port.
//!
//! ## What Maps Well (Rust → KAIN is ~1:1)
//!
//! - `struct` → KAIN `struct`
//! - `enum` with tuple/struct/unit variants → KAIN `enum`
//! - `fn` → KAIN `fn` (with effects: `unsafe fn` → `with Unsafe`)
//! - `impl` blocks → KAIN `impl`
//! - `const` / `static` → KAIN `const`
//! - `type` alias → KAIN type alias
//! - `Box<T>` → transparent (unwrapped to T)
//! - `Arc<T>` / `Rc<T>` → transparent
//! - `Vec<T>` → `Array<T>`
//! - `Option<T>` → KAIN `Option<T>`
//! - `Result<T, E>` → KAIN `Result<T, E>`
//! - `&T` / `&mut T` → KAIN refs
//! - `*const T` / `*mut T` → KAIN `Ptr<T>` (low-level memory layer)
//! - Pattern matching → KAIN match
//! - Closures → KAIN lambdas
//! - Lifetimes → erased (KAIN manages memory differently)
//! - Traits → noted as comments (KAIN uses structural typing / impl blocks)

mod parser;
mod selfhost;
mod semantic_map;
mod transformer;
mod types;

pub use selfhost::{
    import_rust_selfhost_dir, import_rust_selfhost_dir_detailed, RustCrateGraph, RustModuleNode,
    RustSelfHostImportResult, RustSelfHostModuleProgram, RustSelfHostOptions,
};
pub use transformer::RustTransformer;

use crate::Result;
use kain_core::ast::Program;
use kain_fs as kfs;
use std::path::Path;

// ── Public API ────────────────────────────────────────────────────────────────

/// Import a single Rust source file into KAIN AST.
///
/// # Example
/// ```bash
/// kain import-rust ./crates/kain-core/src/lib.rs --output kain-core.kn
/// ```
pub fn import_rust_file(path: &Path) -> Result<Program> {
    let source = kfs::read_text(path)?;
    import_rust_source(&source, path)
}

/// Import a single Rust source file and return diagnostics.
pub fn import_rust_file_detailed(path: &Path) -> Result<(Program, Vec<String>)> {
    let source = kfs::read_text(path)?;
    import_rust_source_detailed(&source, path)
}

/// Import Rust source from a string (useful for testing and REPL use).
pub fn import_rust_source(source: &str, path: &Path) -> Result<Program> {
    import_rust_source_detailed(source, path).map(|(program, _)| program)
}

/// Import Rust source from a string and return diagnostics.
pub fn import_rust_source_detailed(source: &str, path: &Path) -> Result<(Program, Vec<String>)> {
    let file = parser::parse_rust(source, path)?;
    let mut tx = RustTransformer::new();
    let program = tx.transform(file)?;
    Ok((program, tx.diagnostics))
}

/// Import multiple Rust files into a single merged KAIN program (flat mode).
///
/// All symbols land in top-level scope. Use this for self-hosting imports
/// where you want the entire compiler in one `.kn` file.
pub fn import_rust_project(paths: &[&Path]) -> Result<Program> {
    import_rust_project_detailed(paths).map(|(program, _)| program)
}

/// Import multiple Rust files into a single merged KAIN program and return diagnostics.
pub fn import_rust_project_detailed(paths: &[&Path]) -> Result<(Program, Vec<String>)> {
    let mut all_items = Vec::new();
    let mut diagnostics = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = kfs::read_text(path)?;
        let file = parser::parse_rust(&source, path)?;
        let mut tx = RustTransformer::new();
        let program = tx.transform(file)?;
        all_items.extend(program.items);
        diagnostics.extend(tx.diagnostics);
    }

    Ok((
        Program {
            items: all_items,
            span,
        },
        diagnostics,
    ))
}

/// Recursively collect and import all `.rs` files under a directory.
///
/// `flat = true`  → all symbols merged into one top-level scope (for self-hosting)
/// `flat = false` → each file wrapped in a `mod <name>:` block
pub fn import_rust_dir(dir: &Path, flat: bool) -> Result<Program> {
    import_rust_dir_detailed(dir, flat).map(|(program, _)| program)
}

/// Import a Rust directory and return diagnostics.
pub fn import_rust_dir_detailed(dir: &Path, flat: bool) -> Result<(Program, Vec<String>)> {
    let files = collect_rust_files(dir)?;
    let paths: Vec<&Path> = files.iter().map(|p| p.as_path()).collect();

    if flat {
        import_rust_project_detailed(&paths)
    } else {
        import_rust_project_modular_detailed(&paths)
    }
}

/// Import with per-file module wrapping (mirrors Rust's directory/module layout).
fn import_rust_project_modular_detailed(paths: &[&Path]) -> Result<(Program, Vec<String>)> {
    let mut top_items = Vec::new();
    let mut diagnostics = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = kfs::read_text(path)?;
        let file = parser::parse_rust(&source, path)?;
        let mut tx = RustTransformer::new();
        let program = tx.transform(file)?;
        diagnostics.extend(tx.diagnostics);

        let module_path = module_path_for_file(path);
        if module_path.is_empty() {
            top_items.extend(program.items);
        } else {
            top_items.push(build_nested_module(&module_path, program.items));
        }
    }

    Ok((
        Program {
            items: top_items,
            span,
        },
        diagnostics,
    ))
}

fn module_path_for_file(path: &Path) -> Vec<String> {
    let mut parts = Vec::new();
    let file_name = path.file_name().and_then(|s| s.to_str()).unwrap_or("");

    if matches!(file_name, "lib.rs" | "main.rs") {
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
    if !(file_name == "mod.rs" && !parts.is_empty()) {
        let leaf = sanitize_module_component(file_stem);
        if !leaf.is_empty() {
            parts.push(leaf);
        }
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

fn collect_rust_files(dir: &Path) -> Result<Vec<std::path::PathBuf>> {
    let mut files = Vec::new();
    collect_rust_files_into(dir, &mut files)?;
    files.sort(); // deterministic order
    Ok(files)
}

fn collect_rust_files_into(dir: &Path, files: &mut Vec<std::path::PathBuf>) -> Result<()> {
    for entry in kfs::read_dir_entries(dir)? {
        let path = entry.path;
        if path.is_dir() {
            collect_rust_files_into(&path, files)?;
        } else if path.extension().and_then(|e| e.to_str()) == Some("rs") {
            files.push(path);
        }
    }
    Ok(())
}
