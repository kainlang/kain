//! TypeScript language importer
//!
//! Transforms TypeScript source into KAIN AST using the `swc_ecma_parser` crate.
//!
//! ## The TypeScript Import Pipeline
//!
//! ```text
//! app.ts  →  kain import-ts  →  app.kn
//!                                  ↓
//!                         kain build -t ts
//!                                  ↓
//!                            app-generated.ts
//!                                  ↓
//!                    (round-trip complete = ✅ bidirectional)
//! ```
//!
//! ## What Maps Well (TypeScript → KAIN)
//!
//! - `interface` → KAIN `struct`
//! - `type` alias → KAIN type alias
//! - `enum` → KAIN `enum`
//! - `class` → KAIN `struct` + `impl`
//! - `function` → KAIN `fn`
//! - Arrow functions → KAIN lambdas
//! - `async function` → KAIN `fn` with Async effect
//! - `number` → KAIN `Float` or `Int` (context-dependent)
//! - `string` → KAIN `String`
//! - `boolean` → KAIN `Bool`
//! - `Array<T>` → KAIN `Array<T>`
//! - `T | U` → KAIN enum (union types)
//! - `T & U` → KAIN struct (intersection types)
//! - `Promise<T>` → KAIN async function
//! - Generics → KAIN generics

mod parser;
mod transformer;
mod types;

pub use transformer::TypeScriptTransformer;

use kain_core::ast::Program;
use std::path::Path;
use crate::{ImportError, Result};

// ── Public API ────────────────────────────────────────────────────────────────

/// Import a single TypeScript source file into KAIN AST.
///
/// # Example
/// ```bash
/// kain import-ts ./src/app.ts --output src/app.kn
/// ```
pub fn import_typescript_file(path: &Path) -> Result<Program> {
    let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
    import_typescript_source(&source, path)
}

/// Import TypeScript source from a string (useful for testing and REPL use).
pub fn import_typescript_source(source: &str, path: &Path) -> Result<Program> {
    let module = parser::parse_typescript(source, path)?;
    let mut tx = TypeScriptTransformer::new();
    tx.transform(module)
}

/// Import multiple TypeScript files into a single merged KAIN program (flat mode).
///
/// All symbols land in top-level scope.
pub fn import_typescript_project(paths: &[&Path]) -> Result<Program> {
    let mut all_items = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
        let module = parser::parse_typescript(&source, path)?;
        let mut tx = TypeScriptTransformer::new();
        let program = tx.transform(module)?;
        all_items.extend(program.items);
    }

    Ok(Program { items: all_items, span })
}

/// Recursively collect and import all `.ts` files under a directory.
///
/// `flat = true`  → all symbols merged into one top-level scope
/// `flat = false` → each file wrapped in a `mod <name>:` block
pub fn import_typescript_dir(dir: &Path, flat: bool) -> Result<Program> {
    let files = collect_typescript_files(dir)?;
    let paths: Vec<&Path> = files.iter().map(|p| p.as_path()).collect();

    if flat {
        import_typescript_project(&paths)
    } else {
        import_typescript_project_modular(&paths)
    }
}

/// Import with per-file module wrapping.
fn import_typescript_project_modular(paths: &[&Path]) -> Result<Program> {
    use kain_core::ast::{Item, Mod};
    let mut top_items = Vec::new();
    let span = kain_core::span::Span::default();

    for path in paths {
        let source = std::fs::read_to_string(path).map_err(ImportError::IoError)?;
        let module = parser::parse_typescript(&source, path)?;
        let mut tx = TypeScriptTransformer::new();
        let program = tx.transform(module)?;

        // Use file stem as module name (e.g., app.ts → mod app)
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

fn collect_typescript_files(dir: &Path) -> Result<Vec<std::path::PathBuf>> {
    let mut files = Vec::new();
    collect_typescript_files_into(dir, &mut files)?;
    files.sort(); // deterministic order
    Ok(files)
}

fn collect_typescript_files_into(
    dir: &Path,
    files: &mut Vec<std::path::PathBuf>,
) -> Result<()> {
    for entry in std::fs::read_dir(dir).map_err(ImportError::IoError)? {
        let entry = entry.map_err(ImportError::IoError)?;
        let path = entry.path();
        if path.is_dir() {
            collect_typescript_files_into(&path, files)?;
        } else if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
            if ext == "ts" || ext == "tsx" {
                files.push(path);
            }
        }
    }
    Ok(())
}
