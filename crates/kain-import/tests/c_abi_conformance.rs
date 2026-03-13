use kain_core::ast::{Expr, Item};
use kain_core::diagnostics::SpanMapper;
use kain_core::low_level_memory_metadata::{
    attr_usize_arg, has_attr, C_PACKED_ATTR, C_PACK_ALIGN_ATTR,
};
use kain_core::types::{check, TypedItem, TypedProgram};
use kain_core::{lower_typed_program_memory_for_target, CompileTarget};
use kain_import::c::{import_c_file_with_options, CImportOptions};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

fn temp_c_path(stem: &str) -> PathBuf {
    let unique = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock")
        .as_nanos();
    std::env::temp_dir().join(format!("kain_{stem}_{unique}.c"))
}

fn write_source(path: &Path, source: &str) {
    std::fs::write(path, source).expect("write temp source");
}

fn import_and_lower(path: &Path, target: CompileTarget) -> (kain_core::ast::Program, TypedProgram) {
    let program = import_c_file_with_options(path, &CImportOptions::default()).expect("import");
    let mapper = SpanMapper::new("");
    let typed = check(&program, &mapper, path.to_string_lossy().as_ref()).expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, target).expect("lower");
    (program, lowered)
}

fn lowered_return_int(program: &TypedProgram, function_name: &str) -> i64 {
    let function = program
        .items
        .iter()
        .find_map(|item| match item {
            TypedItem::Function(function) if function.ast.name == function_name => Some(function),
            _ => None,
        })
        .unwrap_or_else(|| panic!("missing function {function_name}"));

    let Some(kain_core::ast::Stmt::Return(Some(Expr::Int(value, _)), _)) =
        function.ast.body.stmts.last()
    else {
        panic!("expected lowered integer return for {function_name}");
    };
    *value
}

#[test]
fn pragma_pack_conformance_survives_import_and_lowering_targets() {
    let path = temp_c_path("pragma_pack");
    let source = r#"
        #pragma pack(push, 1)
        struct Packet {
            char tag;
            int value;
        };
        #pragma pack(pop)

        int packet_size() { return sizeof(struct Packet); }
        int packet_align() { return _Alignof(struct Packet); }
    "#;
    write_source(&path, source);

    let (program, lowered_ts) = import_and_lower(&path, CompileTarget::Ts);
    let (_, lowered_wasm) = import_and_lower(&path, CompileTarget::Wasm);
    let (_, lowered_cpp) = import_and_lower(&path, CompileTarget::Cpp);
    let _ = std::fs::remove_file(&path);

    let packet = program
        .items
        .iter()
        .find_map(|item| match item {
            Item::Struct(st) if st.name == "Packet" => Some(st),
            _ => None,
        })
        .expect("missing Packet struct");

    assert!(has_attr(&packet.attributes, C_PACKED_ATTR));
    assert_eq!(
        attr_usize_arg(&packet.attributes, C_PACK_ALIGN_ATTR),
        Some(8)
    );

    for lowered in [&lowered_ts, &lowered_wasm, &lowered_cpp] {
        assert_eq!(lowered_return_int(lowered, "packet_size"), 5);
        assert_eq!(lowered_return_int(lowered, "packet_align"), 1);
    }
}

#[test]
fn explicit_aligned_attribute_conformance_survives_import_and_lowering_targets() {
    let path = temp_c_path("aligned_attr");
    let source = r#"
        struct Wide {
            char tag;
            int value;
        } __attribute__((aligned(16)));

        int wide_size() { return sizeof(struct Wide); }
        int wide_align() { return _Alignof(struct Wide); }
    "#;
    write_source(&path, source);

    let (program, lowered_ts) = import_and_lower(&path, CompileTarget::Ts);
    let (_, lowered_wasm) = import_and_lower(&path, CompileTarget::Wasm);
    let (_, lowered_rust) = import_and_lower(&path, CompileTarget::Rust);
    let _ = std::fs::remove_file(&path);

    let wide = program
        .items
        .iter()
        .find_map(|item| match item {
            Item::Struct(st) if st.name == "Wide" => Some(st),
            _ => None,
        })
        .expect("missing Wide struct");

    assert_eq!(
        kain_core::low_level_memory_metadata::attr_usize_arg(
            &wide.attributes,
            kain_core::low_level_memory_metadata::C_TYPE_ALIGN_ATTR,
        ),
        Some(128)
    );

    for lowered in [&lowered_ts, &lowered_wasm, &lowered_rust] {
        assert_eq!(lowered_return_int(lowered, "wide_size"), 16);
        assert_eq!(lowered_return_int(lowered, "wide_align"), 16);
    }
}

#[test]
fn named_pragma_pack_stack_conformance_survives_import_and_lowering_targets() {
    let path = temp_c_path("named_pack");
    let source = r#"
        #pragma pack(push, outer, 4)
        struct Outer {
            char tag;
            int value;
        };
        #pragma pack(push, inner, 1)
        struct Inner {
            char tag;
            int value;
        };
        #pragma pack(pop, inner)
        struct AfterInner {
            char tag;
            int value;
        };
        #pragma pack(pop, outer)
        struct Natural {
            char tag;
            int value;
        };

        int inner_size() { return sizeof(struct Inner); }
        int after_inner_align() { return _Alignof(struct AfterInner); }
        int natural_align() { return _Alignof(struct Natural); }
    "#;
    write_source(&path, source);

    let (program, lowered_ts) = import_and_lower(&path, CompileTarget::Ts);
    let (_, lowered_wasm) = import_and_lower(&path, CompileTarget::Wasm);
    let (_, lowered_cpp) = import_and_lower(&path, CompileTarget::Cpp);
    let _ = std::fs::remove_file(&path);

    let mut pack_attrs = std::collections::HashMap::new();
    for item in &program.items {
        if let Item::Struct(st) = item {
            pack_attrs.insert(
                st.name.as_str(),
                attr_usize_arg(&st.attributes, C_PACK_ALIGN_ATTR),
            );
        }
    }

    assert_eq!(pack_attrs.get("Outer").copied().flatten(), Some(32));
    assert_eq!(pack_attrs.get("Inner").copied().flatten(), Some(8));
    assert_eq!(pack_attrs.get("AfterInner").copied().flatten(), Some(32));
    assert_eq!(pack_attrs.get("Natural").copied().flatten(), None);

    for lowered in [&lowered_ts, &lowered_wasm, &lowered_cpp] {
        assert_eq!(lowered_return_int(lowered, "inner_size"), 5);
        assert_eq!(lowered_return_int(lowered, "after_inner_align"), 4);
        assert_eq!(lowered_return_int(lowered, "natural_align"), 4);
    }
}
