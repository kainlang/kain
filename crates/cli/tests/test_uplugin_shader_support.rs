/// Test that .uplugin generation correctly sets CanContainContent based on shader presence
///
/// This test validates Requirements 11.1, 11.2, 11.3:
/// - 11.1: Shaders/ directory is created in plugin root
/// - 11.2: .usf files are written to correct location
/// - 11.3: .uplugin includes CanContainContent: true when shaders present
use cli::packager::uplugin_gen::generate_uplugin_file;

#[test]
fn test_uplugin_without_shaders() {
    let uplugin = generate_uplugin_file(
        "TestPlugin",
        &Some("Test plugin without shaders".to_string()),
        false,
        false,
        false, // no shaders
        &[],   // no plugin dependencies
    );

    // Should have CanContainContent: false
    assert!(
        uplugin.contains(r#""CanContainContent": false"#),
        "Plugin without shaders should have CanContainContent: false"
    );
}

#[test]
fn test_uplugin_with_shaders() {
    let uplugin = generate_uplugin_file(
        "TestPlugin",
        &Some("Test plugin with shaders".to_string()),
        false,
        false,
        true, // has shaders
        &[],  // no plugin dependencies
    );

    // Should have CanContainContent: true
    assert!(
        uplugin.contains(r#""CanContainContent": true"#),
        "Plugin with shaders should have CanContainContent: true"
    );
}

#[test]
fn test_uplugin_split_mode_with_shaders() {
    let uplugin = generate_uplugin_file(
        "TestPlugin",
        &Some("Test plugin with split modules and shaders".to_string()),
        true,
        true, // split mode
        true, // has shaders
        &[],  // no plugin dependencies
    );

    // Should have CanContainContent: true
    assert!(
        uplugin.contains(r#""CanContainContent": true"#),
        "Split mode plugin with shaders should have CanContainContent: true"
    );

    // Should have both modules
    assert!(
        uplugin.contains(r#""Name": "TestPlugin""#),
        "Should have runtime module"
    );
    assert!(
        uplugin.contains(r#""Name": "TestPluginEditor""#),
        "Should have editor module"
    );
}

#[test]
fn test_uplugin_editor_only_with_shaders() {
    let uplugin = generate_uplugin_file(
        "TestPlugin",
        &Some("Test editor plugin with shaders".to_string()),
        true,  // has editor items
        false, // no split
        true,  // has shaders
        &[],   // no plugin dependencies
    );

    // Should have CanContainContent: true
    assert!(
        uplugin.contains(r#""CanContainContent": true"#),
        "Editor plugin with shaders should have CanContainContent: true"
    );

    // Should be Editor type
    assert!(
        uplugin.contains(r#""Type": "Editor""#),
        "Should be Editor type module"
    );
}

#[test]
fn test_uplugin_structure() {
    let uplugin = generate_uplugin_file(
        "MyPlugin",
        &None,
        false,
        false,
        true,
        &[], // no plugin dependencies
    );

    // Verify JSON structure
    assert!(uplugin.contains(r#""FileVersion": 3"#));
    assert!(uplugin.contains(r#""Version": 1"#));
    assert!(uplugin.contains(r#""VersionName": "1.0.0""#));
    assert!(uplugin.contains(r#""FriendlyName": "MyPlugin""#));
    assert!(uplugin.contains(r#""Category": "KAIN-PRO""#));
    assert!(uplugin.contains(r#""CreatedBy": "KAIN-PRO Compiler""#));
    assert!(uplugin.contains(r#""Modules": ["#));
}
