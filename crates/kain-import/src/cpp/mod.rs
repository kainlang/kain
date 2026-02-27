//! C++ language importer (PLANNED)
//!
//! Will use `tree-sitter-cpp` to parse C++ source code and transform to KAIN AST.
//!
//! ## Future Features
//!
//! - Classes and inheritance
//! - Templates
//! - Namespaces
//! - Operator overloading
//! - RAII patterns
//!
//! ## Example (Future)
//!
//! ```rust,ignore
//! use kain_import::cpp;
//! use std::path::Path;
//!
//! let program = cpp::import_cpp_file(Path::new("game.cpp"))?;
//! ```

// TODO: Implement C++ importer using tree-sitter-cpp
// Dependencies needed:
// - tree-sitter = "0.20"
// - tree-sitter-cpp = "0.20"
