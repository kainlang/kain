//! DWARF debug metadata correctness tests for LLVM IR code generation.
//!
//! Each test targets one invariant.  Failures are easy to isolate.
//!
//! Related: llvm_codegen_test.rs (general IR correctness)

use kain_core::diagnostics::SpanMapper;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::types;
use kain_sys_codegen::generate_with_debug;

/// Parse + typecheck + debug-codegen from raw source.
fn generate(source: &str, filename: &str) -> String {
    let tokens = Lexer::new(source)
        .tokenize()
        .expect("lex");
    let mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &mapper, filename)
        .parse()
        .expect("parse");
    let typed = types::check(&ast, &mapper, filename).expect("typecheck");
    String::from_utf8(
        generate_with_debug(&typed, source, filename).expect("generate_with_debug"),
    )
    .expect("utf8")
}

// ---------------------------------------------------------------------------
// Column clamp — DWARF limits column to u16 (max 65535)
// ---------------------------------------------------------------------------

/// A source file with an extremely long line (giant string literal, generated
/// code, minified blob) produces column values that exceed the DWARF u16
/// maximum.  The emitter must clamp to 65535.
///
/// Regression for: `!DILocation(line: …, column: 66474, …)` rejected by LLVM.
#[test]
fn column_clamped_to_u16_max() {
    let pad = " ".repeat(70_000);
    let source = format!(
        "fn long_line() -> Int:\n    // start of long line\n    let x = {pad}42\n    return x\n"
    );

    let llvm = generate(&source, "long.kn");

    for line in llvm.lines() {
        if !line.contains("!DILocation(line:") {
            continue;
        }
        let Some(col_start) = line.find("column: ") else {
            continue;
        };
        let rest = &line[col_start + "column: ".len()..];
        let col_end = rest
            .find(|c: char| !c.is_ascii_digit())
            .unwrap_or(rest.len());
        let col: usize = rest[..col_end]
            .parse()
            .unwrap_or_else(|_| panic!("bad column in: {line}"));

        assert!(
            col <= 65535,
            "DILocation column {col} exceeds DWARF u16 max (65535)\n{line}"
        );
    }
}

// ---------------------------------------------------------------------------
// Baseline – debug info present
// ---------------------------------------------------------------------------

/// When --debug is on we must emit a compile unit, file, and module flags.
#[test]
fn emits_compile_unit_and_file() {
    let llvm = generate("fn main() -> Int:\n    return 0\n", "test.kn");
    assert!(llvm.contains("!DICompileUnit("), "missing DICompileUnit");
    assert!(llvm.contains("!DIFile("), "missing DIFile");
    assert!(llvm.contains("!llvm.module.flags"), "missing module flags");
    assert!(llvm.contains("Dwarf Version"), "missing Dwarf Version flag");
}

// ---------------------------------------------------------------------------
// Crash table – linkage and sentinel
// ---------------------------------------------------------------------------

/// @__kain_crash_table must use `global` (or `dso_local global`) linkage
/// so the C runtime's `extern` weak symbol resolves.
#[test]
fn crash_table_has_global_linkage() {
    let llvm = generate("fn main() -> Int:\n    return 0\n", "test.kn");

    let table_line = llvm
        .lines()
        .find(|l| l.contains("@__kain_crash_table"))
        .expect("@__kain_crash_table not found");

    assert!(
        table_line.contains("global"),
        "crash table lacks global linkage:\n{table_line}"
    );
}

/// The table must end with a sentinel (fn_ptr == 0) so the runtime counting
/// loop terminates.
#[test]
fn crash_table_ends_with_sentinel() {
    let source = r#"
fn add(a: Int, b: Int) -> Int:
    return a + b

fn main() -> Int:
    return add(3, 4)
"#;
    let llvm = generate(source, "test.kn");

    // Collect @__kain_crash_table body up to the next top-level definition.
    let table_lines: Vec<&str> = llvm
        .lines()
        .skip_while(|l| !l.contains("@__kain_crash_table"))
        .take(100)
        .take_while(|l| {
            !l.starts_with('@') || l.contains("@__kain_crash_table")
        })
        .collect();

    let text = table_lines.join("\n");
    assert!(
        text.contains("i64 0") || text.contains("i64 null"),
        "crash table missing sentinel (zero fn_ptr entry)"
    );
}
