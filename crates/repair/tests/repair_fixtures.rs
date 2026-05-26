use kain_repair::{repair_text_with_input, RepairInput, RepairMode, RepairProfile};
use std::fs;
use std::path::PathBuf;

fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name);
    fs::read_to_string(&path)
        .unwrap_or_else(|err| panic!("failed to read {}: {err}", path.display()))
}

#[test]
fn parser_fragment_fixture_reconstructs_block_and_trims_noise() {
    let source = fixture("kain_repair_parser_block.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result
        .repaired
        .contains("let renderer = SceneRenderer::new(state)\n    renderer.begin()"));
    assert!(result.repaired.ends_with('\n'));
}

#[test]
fn reserved_identifier_and_self_constructor_fixture_repairs_symbol_drift() {
    let source = fixture("kain_repair_reserved_self.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result.repaired.contains("fn Self(value: Int) -> Self"));
    assert!(result.repaired.contains("let type_ = value"));
    assert!(result.repaired.contains("Self::build(type_)"));
    assert!(result
        .repaired
        .contains("fn build_pair(left: Self, right: Self) -> Result<Self, Self>"));
    assert!(result.repaired.contains("Result::ok(Self(left, right))"));
}

#[test]
fn repair_report_exposes_fix_counts_and_risk_classes() {
    let source = fixture("kain_repair_reserved_self.kn");
    let report = kain_repair::repair_source_with_profile(
        &source,
        RepairProfile::default(),
        RepairMode::ApplyAggressive,
    );

    assert!(report.changed());
    assert!(report.fixes_applied > 0);
    assert_eq!(report.fixes_applied, report.fixes.len());
    assert_eq!(
        report.safety_class,
        kain_repair::RepairSafetyClass::Aggressive
    );
    assert_eq!(
        report.remaining_unknown_risk,
        kain_repair::RepairRiskLevel::Elevated
    );
    assert_eq!(report.parser_proof_status(), None);
}

#[test]
fn declaration_header_fixture_normalizes_reserved_suffix_headers() {
    let source = fixture("kain_repair_declaration_headers.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result.repaired.contains("enum AssetType:"));
    assert!(result.repaired.contains("struct AnimationSourceKind:"));
    assert!(result.repaired.contains("trait AssetCodec:"));
    assert!(result.repaired.contains("impl AssetCodec for AssetType:"));
    assert!(result.repaired.contains("mod library:"));
    assert!(result.repaired.contains("mod material:"));
    assert!(result.repaired.contains("mod preset:"));
    assert!(result.repaired.contains("mod texture:"));
}

#[test]
fn nested_declaration_fixture_flattens_nested_blocks_to_top_level() {
    let source = fixture("kain_repair_nested_declarations.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result.repaired.contains("enum AssetKind:"));
    assert!(result.repaired.contains("struct AssetRecord:"));
    assert!(result.repaired.contains("impl AssetRecord:"));
    assert!(result.repaired.contains("enum AssetState:"));
    assert!(result.repaired.contains("fn after_assets():"));
    assert!(result
        .repaired
        .lines()
        .any(|line| line == "struct AssetRecord:"));
    assert!(result
        .repaired
        .lines()
        .any(|line| line == "impl AssetRecord:"));
    assert!(result
        .repaired
        .lines()
        .any(|line| line == "enum AssetState:"));
}

#[test]
fn impl_type_token_fixture_normalizes_parameter_and_type_positions() {
    let source = fixture("kain_repair_impl_type_tokens.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result
        .repaired
        .contains("fn with_name(plugin_name: impl Into<String>) -> Self:"));
    assert!(result
        .repaired
        .contains("let name: impl Display = plugin_name"));
    assert!(result.repaired.contains("return Self::new(name)"));
}

#[test]
fn namespace_path_fixture_normalizes_slashes_without_touching_profile_defaults() {
    let source = fixture("kain_repair_namespace_path.kn");
    let profile = RepairProfile {
        normalize_namespace_paths: true,
        reconstruct_parser_safe_blocks: false,
        ..RepairProfile::default()
    };
    let result = repair_text_with_input(&RepairInput::new(source).with_profile(profile));

    assert!(result.changed);
    assert!(result.repaired.contains("use kain::runtime::native"));
    assert!(result.repaired.contains("host::bridge::load()"));
}

#[test]
fn unterminated_comment_fixture_stays_open_in_check_mode_but_closes_in_safe_mode() {
    let source = fixture("kain_repair_unterminated_comment.kn");

    let check =
        repair_text_with_input(&RepairInput::new(source.clone()).with_mode(RepairMode::Check));
    assert_eq!(check.original, source);
    assert_eq!(check.repaired, source);
    assert!(!check.changed);

    let safe = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));
    assert!(safe.changed);
    assert!(safe.repaired.contains("*/\n"));
}
