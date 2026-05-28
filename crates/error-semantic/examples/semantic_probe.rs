use kain_error::{CompilerPhase, DiagnosticCode, DiagnosticSemanticPacket};
use kain_error_semantic::SemanticCoprocessor;

fn print_case(name: &str, packet: DiagnosticSemanticPacket) {
    let coprocessor = SemanticCoprocessor::new();
    let report = coprocessor.analyze(&packet);

    println!("=== {name} ===");
    println!("packet_code={}", packet.code);
    println!("primary_text={}", packet.primary_text);
    println!("result={}", serde_json::to_string_pretty(&report).unwrap());
    println!();
}

fn main() {
    let typo_packet = DiagnosticSemanticPacket::new(
        DiagnosticCode::TypeUnknownIdentifier,
        CompilerPhase::TypeChecking,
        "prntln",
    )
    .visible_symbols(vec!["println".into(), "print".into(), "format".into()])
    .add_scope_match("println", 1)
    .add_repair("rename_symbol", "Rename to println", "println")
    .add_downstream(DiagnosticCode::TypeGeneric);

    let missing_import_packet = DiagnosticSemanticPacket::new(
        DiagnosticCode::TypeUnknownIdentifier,
        CompilerPhase::TypeChecking,
        "fs_read_text",
    )
    .source_window("fn main() -> Int:\n    let content = fs_read_text('demo.txt')\n    return 0")
    .add_downstream(DiagnosticCode::TypeGeneric)
    .add_downstream(DiagnosticCode::TypeDuplicateSymbol);

    let missing_surface_packet = DiagnosticSemanticPacket::new(
        DiagnosticCode::TypeWorldMissingSurface,
        CompilerPhase::TypeChecking,
        "Demo",
    )
    .source_window("world Demo:\n    state hp: Int = 3")
    .add_repair(
        "insert_surface_native_ui",
        "Add a native_ui surface projection",
        "surface native_ui => DemoPanel",
    )
    .add_downstream(DiagnosticCode::TypeGeneric);

    let parser_packet = DiagnosticSemanticPacket::new(
        DiagnosticCode::ParseMissingDelimiterBeforeNewline,
        CompilerPhase::Parser,
        "Int",
    )
    .source_window("fn main() -> Int\n    return 0")
    .add_repair("insert_colon", "Insert missing ':'", ":")
    .add_downstream(DiagnosticCode::TypeGeneric)
    .add_downstream(DiagnosticCode::TypeUnknownIdentifier)
    .add_downstream(DiagnosticCode::TypeDuplicateSymbol);

    let converge_packet = DiagnosticSemanticPacket::new(
        DiagnosticCode::EffectViolation,
        CompilerPhase::TypeChecking,
        "fast",
    )
    .flag("in_converge_block", true)
    .source_window("converge checksum:\n    spec ...\n    fast ...")
    .add_repair(
        "repair_fast_lane",
        "Make the fast lane match the spec lane outputs",
        "fast => ...",
    );

    print_case("typo", typo_packet);
    print_case("missing_import", missing_import_packet);
    print_case("missing_surface", missing_surface_packet);
    print_case("parser_delimiter", parser_packet);
    print_case("converge_effect", converge_packet);
}
