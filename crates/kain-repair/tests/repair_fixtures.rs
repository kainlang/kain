use kain_repair::{repair_text_with_input, RepairInput, RepairMode, RepairProfile};
use std::fs;
use std::path::PathBuf;

fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name);
    fs::read_to_string(&path).unwrap_or_else(|err| panic!("failed to read {}: {err}", path.display()))
}

#[test]
fn parser_fragment_fixture_reconstructs_block_and_trims_noise() {
    let source = fixture("kain_repair_parser_block.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result.repaired.contains("let renderer = SceneRenderer::new(state)\n    renderer.begin()"));
    assert!(result.repaired.ends_with('\n'));
}

#[test]
fn reserved_identifier_and_self_constructor_fixture_repairs_symbol_drift() {
    let source = fixture("kain_repair_reserved_self.kn");
    let result = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));

    assert!(result.changed);
    assert!(result.repaired.contains("fn Self_(value: Int) -> Self_"));
    assert!(result.repaired.contains("let type_ = value"));
    assert!(result.repaired.contains("Self::build(type_)"));
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

    let check = repair_text_with_input(&RepairInput::new(source.clone()).with_mode(RepairMode::Check));
    assert_eq!(check.original, source);
    assert_eq!(check.repaired, source);
    assert!(!check.changed);

    let safe = repair_text_with_input(&RepairInput::new(source).with_mode(RepairMode::ApplySafe));
    assert!(safe.changed);
    assert!(safe.repaired.contains("*/\n"));
}
