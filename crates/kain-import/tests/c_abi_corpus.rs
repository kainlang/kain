use kain_core::ast::{Expr, Item};
use kain_core::diagnostics::SpanMapper;
use kain_core::low_level_memory_metadata::{
    attr_usize_arg, has_attr, C_PACK_ALIGN_ATTR, C_PACKED_ATTR, C_TYPE_ALIGN_ATTR,
};
use kain_core::types::{check, TypedItem, TypedProgram};
use kain_core::{lower_typed_program_memory_for_target, CompileTarget};
use kain_import::c::{import_c_file_with_options, CImportOptions};
use serde::Deserialize;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Deserialize)]
struct AbiCorpusManifest {
    cases: Vec<AbiCorpusCase>,
}

#[derive(Debug, Deserialize)]
struct AbiCorpusCase {
    name: String,
    source: String,
    targets: Vec<String>,
    #[serde(default)]
    struct_attrs: Vec<ExpectedStructAttr>,
    #[serde(default)]
    returns: Vec<ExpectedReturn>,
    #[serde(default)]
    bitfield_promotions: Vec<ExpectedBitfieldPromotion>,
    #[serde(default)]
    union_gets: Vec<ExpectedUnionGet>,
}

#[derive(Debug, Deserialize)]
struct ExpectedStructAttr {
    struct_name: String,
    #[serde(default)]
    packed: Option<bool>,
    #[serde(default)]
    pack_align_bits: Option<usize>,
    #[serde(default)]
    type_align_bits: Option<usize>,
}

#[derive(Debug, Deserialize)]
struct ExpectedReturn {
    function: String,
    value: i64,
}

#[derive(Debug, Deserialize)]
struct ExpectedBitfieldPromotion {
    function: String,
    widths: Vec<i64>,
}

#[derive(Debug, Deserialize)]
struct ExpectedUnionGet {
    function: String,
    field: String,
    type_name: String,
    stride: i64,
    layout_size: i64,
}

fn corpus_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests").join("abi_corpus")
}

fn load_manifest() -> AbiCorpusManifest {
    let path = corpus_root().join("manifest.json");
    let raw = fs::read_to_string(&path).expect("read ABI corpus manifest");
    serde_json::from_str(&raw).expect("parse ABI corpus manifest")
}

fn parse_target(name: &str) -> CompileTarget {
    match name.to_ascii_lowercase().as_str() {
        "ts" => CompileTarget::Ts,
        "js" => CompileTarget::Js,
        "wasm" => CompileTarget::Wasm,
        "cpp" => CompileTarget::Cpp,
        "rust" | "rs" => CompileTarget::Rust,
        "ue5" => CompileTarget::Ue5,
        other => panic!("unsupported ABI corpus target {other}"),
    }
}

fn import_and_lower(path: &Path, target: CompileTarget) -> (kain_core::ast::Program, TypedProgram) {
    let program = import_c_file_with_options(path, &CImportOptions::default()).expect("import");
    let mapper = SpanMapper::new("");
    let typed = check(&program, &mapper, path.to_string_lossy().as_ref()).expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, target).expect("lower");
    (program, lowered)
}

fn lowered_function<'a>(program: &'a TypedProgram, function_name: &str) -> &'a kain_core::types::TypedFunction {
    program
        .items
        .iter()
        .find_map(|item| match item {
            TypedItem::Function(function) if function.ast.name == function_name => Some(function),
            _ => None,
        })
        .unwrap_or_else(|| panic!("missing function {function_name}"))
}

fn lowered_return_int(program: &TypedProgram, function_name: &str) -> i64 {
    let function = lowered_function(program, function_name);
    let Some(kain_core::ast::Stmt::Return(Some(Expr::Int(value, _)), _)) = function.ast.body.stmts.last() else {
        panic!("expected lowered integer return for {function_name}");
    };
    *value
}

fn unwrap_cast(expr: &Expr) -> &Expr {
    match expr {
        Expr::Cast { value, .. } => value.as_ref(),
        other => other,
    }
}

fn expect_call<'a>(expr: &'a Expr, name: &str) -> &'a Vec<kain_core::ast::CallArg> {
    let expr = unwrap_cast(expr);
    let Expr::Call { callee, args, .. } = expr else {
        panic!("expected call expression, got {expr:?}");
    };
    assert!(matches!(callee.as_ref(), Expr::Ident(found, _) if found == name));
    args
}

#[test]
fn abi_corpus_fixtures_survive_import_and_lowering_targets() {
    let manifest = load_manifest();
    let root = corpus_root();

    for case in &manifest.cases {
        let path = root.join(&case.source);
        assert!(path.exists(), "missing ABI corpus source {}", path.display());

        let mut imported_program = None;
        for target_name in &case.targets {
            let target = parse_target(target_name);
            let (program, lowered) = import_and_lower(&path, target);
            if imported_program.is_none() {
                imported_program = Some(program);
            }

            for expected in &case.returns {
                assert_eq!(
                    lowered_return_int(&lowered, &expected.function),
                    expected.value,
                    "case {} target {} function {}",
                    case.name,
                    target_name,
                    expected.function
                );
            }

            for expected in &case.bitfield_promotions {
                let function = lowered_function(&lowered, &expected.function);
                let kain_core::ast::Stmt::Return(Some(Expr::Binary { left, right, .. }), _) =
                    &function.ast.body.stmts[0]
                else {
                    panic!("expected binary return for {} in {}", expected.function, case.name);
                };
                let left_args = expect_call(left.as_ref(), "__kain_bitfield_get");
                let right_args = expect_call(right.as_ref(), "__kain_bitfield_get");
                let observed = vec![
                    match &left_args[6].value {
                        Expr::Int(value, _) => *value,
                        other => panic!("expected left promotion width int, got {other:?}"),
                    },
                    match &right_args[6].value {
                        Expr::Int(value, _) => *value,
                        other => panic!("expected right promotion width int, got {other:?}"),
                    },
                ];
                assert_eq!(
                    observed, expected.widths,
                    "case {} target {} function {}",
                    case.name, target_name, expected.function
                );
            }

            for expected in &case.union_gets {
                let function = lowered_function(&lowered, &expected.function);
                let kain_core::ast::Stmt::Return(Some(expr), _) = &function.ast.body.stmts[0] else {
                    panic!("expected return for {} in {}", expected.function, case.name);
                };
                let args = expect_call(expr, "__kain_union_get");
                assert!(matches!(&args[1].value, Expr::String(value, _) if value == &expected.field));
                assert!(matches!(&args[2].value, Expr::String(value, _) if value == &expected.type_name));
                assert!(matches!(&args[3].value, Expr::Int(value, _) if *value == expected.stride));
                assert!(matches!(&args[4].value, Expr::Int(value, _) if *value == expected.layout_size));
            }
        }

        let program = imported_program.expect("imported program");
        for expected in &case.struct_attrs {
            let st = program
                .items
                .iter()
                .find_map(|item| match item {
                    Item::Struct(st) if st.name == expected.struct_name => Some(st),
                    _ => None,
                })
                .unwrap_or_else(|| panic!("missing struct {} in {}", expected.struct_name, case.name));

            if let Some(packed) = expected.packed {
                assert_eq!(
                    has_attr(&st.attributes, C_PACKED_ATTR),
                    packed,
                    "case {} struct {} packed mismatch",
                    case.name,
                    expected.struct_name
                );
            }
            assert_eq!(
                attr_usize_arg(&st.attributes, C_PACK_ALIGN_ATTR),
                expected.pack_align_bits,
                "case {} struct {} pack-align mismatch",
                case.name,
                expected.struct_name
            );
            assert_eq!(
                attr_usize_arg(&st.attributes, C_TYPE_ALIGN_ATTR),
                expected.type_align_bits,
                "case {} struct {} type-align mismatch",
                case.name,
                expected.struct_name
            );
        }
    }
}
