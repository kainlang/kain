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
mod transformer;
mod types;

pub use selfhost::{
    import_rust_selfhost_dir,
    import_rust_selfhost_dir_detailed,
    RustCrateGraph,
    RustModuleNode,
    RustSelfHostImportResult,
    RustSelfHostOptions,
};
pub use transformer::RustTransformer;

use kain_core::ast::Program;
use std::path::Path;
use crate::{ImportError, Result};

// ── Public API ────────────────────────────────────────────────────────────────

/// Import a single Rust source file into KAIN AST.
///
/// # Example
/// ```bash
/// kain import-rust ./crates/kain-core/src/lib.rs --output kain-core.kn
/// ```
pub fn import_rust_file(path: &Path) -> Result<Program> {
    let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
    import_rust_source(&source, path)
}

/// Import Rust source from a string (useful for testing and REPL use).
pub fn import_rust_source(source: &str, path: &Path) -> Result<Program> {
    let file = parser::parse_rust(source, path)?;
    let mut tx = RustTransformer::new();
    tx.transform(file)
}

/// Import multiple Rust files into a single merged KAIN program (flat mode).
///
/// All symbols land in top-level scope. Use this for self-hosting imports
/// where you want the entire compiler in one `.kn` file.
pub fn import_rust_project(paths: &[&Path]) -> Result<Program> {
    let mut all_items = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
        let file = parser::parse_rust(&source, path)?;
        let mut tx = RustTransformer::new();
        let program = tx.transform(file)?;
        all_items.extend(program.items);
    }

    Ok(Program { items: all_items, span })
}

/// Recursively collect and import all `.rs` files under a directory.
///
/// `flat = true`  → all symbols merged into one top-level scope (for self-hosting)
/// `flat = false` → each file wrapped in a `mod <name>:` block
pub fn import_rust_dir(dir: &Path, flat: bool) -> Result<Program> {
    let files = collect_rust_files(dir)?;
    let paths: Vec<&Path> = files.iter().map(|p| p.as_path()).collect();

    if flat {
        import_rust_project(&paths)
    } else {
        import_rust_project_modular(&paths)
    }
}

/// Import with per-file module wrapping (mirrors C importer directory mode).
fn import_rust_project_modular(paths: &[&Path]) -> Result<Program> {
    use kain_core::ast::{Item, Mod};
    let mut top_items = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
        let file = parser::parse_rust(&source, path)?;
        let mut tx = RustTransformer::new();
        let program = tx.transform(file)?;

        // Use file stem as module name (e.g., parser.rs → mod parser)
        let mod_name = path
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("unknown")
            .to_string();

        top_items.push(Item::Mod(Mod {
            name: mod_name,
            inline: Some(program.items),
            visibility: kain_core::ast::Visibility::Public,
            span,
        }));
    }

    Ok(Program { items: top_items, span })
}

fn collect_rust_files(dir: &Path) -> Result<Vec<std::path::PathBuf>> {
    let mut files = Vec::new();
    collect_rust_files_into(dir, &mut files)?;
    files.sort(); // deterministic order
    Ok(files)
}

fn collect_rust_files_into(
    dir: &Path,
    files: &mut Vec<std::path::PathBuf>,
) -> Result<()> {
    for entry in std::fs::read_dir(dir).map_err(ImportError::IoError)? {
        let entry = entry.map_err(ImportError::IoError)?;
        let path = entry.path();
        if path.is_dir() {
            collect_rust_files_into(&path, files)?;
        } else if path.extension().and_then(|e| e.to_str()) == Some("rs") {
            files.push(path);
        }
    }
    Ok(())
}
