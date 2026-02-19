/// Test that the expanded widget_registry.json loads correctly
/// and contains the new composition rules and property constraints.

use std::fs;
use std::path::PathBuf;
use ue5::ue5::widget_registry::WidgetRegistry;

#[test]
fn test_expanded_widget_registry_loads() {
    // Find the widget_registry.json file
    let mut registry_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    registry_path.push("../../unreal/metadata/widget_registry.json");
    
    // Load the file
    let json_data = fs::read_to_string(&registry_path)
        .expect("Failed to read widget_registry.json");
    
    // Parse as WidgetRegistry
    let mut registry = WidgetRegistry::new();
    registry.load(&json_data)
        .expect("Failed to load widget_registry.json");
    
    // Verify basic stats
    let (widget_count, event_count, delegate_count) = registry.stats();
    println!("Loaded {} widgets, {} events, {} delegates", widget_count, event_count, delegate_count);
    
    assert!(widget_count > 2300, "Expected at least 2300 widgets, got {}", widget_count);
    assert!(delegate_count > 400, "Expected at least 400 delegates, got {}", delegate_count);
    
    // Verify composition rules are present
    assert!(registry.composition_rules.is_some(), "Composition rules should be present");
    
    let comp_rules = registry.composition_rules.as_ref().unwrap();
    
    // Check slot requirements
    assert!(!comp_rules.slot_requirements.is_empty(), "Slot requirements should not be empty");
    assert!(comp_rules.slot_requirements.contains_key("SButton"), "SButton should have slot requirements");
    assert!(comp_rules.slot_requirements.contains_key("SVerticalBox"), "SVerticalBox should have slot requirements");
    
    // Check property dependencies
    assert!(!comp_rules.property_dependencies.is_empty(), "Property dependencies should not be empty");
    assert!(comp_rules.property_dependencies.contains_key("SSlider"), "SSlider should have property dependencies");
    
    // Check property exclusions
    assert!(!comp_rules.property_exclusions.is_empty(), "Property exclusions should not be empty");
    assert!(comp_rules.property_exclusions.contains_key("SButton"), "SButton should have property exclusions");
    
    // Check parent-child compatibility
    assert!(!comp_rules.parent_child_compatibility.is_empty(), "Parent-child compatibility should not be empty");
    assert!(comp_rules.parent_child_compatibility.contains_key("SPanel"), "SPanel should have compatibility rules");
    
    // Verify property constraints are present
    assert!(!registry.property_constraints.is_empty(), "Property constraints should not be empty");
    assert!(registry.property_constraints.contains_key("float"), "float constraint should be present");
    assert!(registry.property_constraints.contains_key("FLinearColor"), "FLinearColor constraint should be present");
    assert!(registry.property_constraints.contains_key("EHorizontalAlignment"), "EHorizontalAlignment constraint should be present");
    
    println!("✓ Widget registry expansion validated successfully");
}

#[test]
fn test_composition_rule_queries() {
    // Load registry
    let mut registry_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    registry_path.push("../../unreal/metadata/widget_registry.json");
    
    let json_data = fs::read_to_string(&registry_path)
        .expect("Failed to read widget_registry.json");
    
    let mut registry = WidgetRegistry::new();
    registry.load(&json_data)
        .expect("Failed to load widget_registry.json");
    
    // Test slot requirements query
    let button_slots = registry.get_slot_requirements("SButton");
    assert!(button_slots.is_some(), "SButton should have slot requirements");
    assert_eq!(button_slots.unwrap().max_children, 1, "SButton should allow 1 child");
    
    // Test property dependencies query
    let slider_deps = registry.get_property_dependencies("SSlider", "Value");
    assert!(slider_deps.is_some(), "SSlider Value should have dependencies");
    let deps = slider_deps.unwrap();
    assert!(deps.requires.contains(&"MinValue".to_string()), "Value should require MinValue");
    assert!(deps.requires.contains(&"MaxValue".to_string()), "Value should require MaxValue");
    
    // Test property exclusions query
    let button_exclusions = registry.get_property_exclusions("SButton");
    assert!(button_exclusions.is_some(), "SButton should have property exclusions");
    
    // Test can_contain_children
    assert!(registry.can_contain_children("SButton"), "SButton should be able to contain children");
    assert!(registry.can_contain_children("SVerticalBox"), "SVerticalBox should be able to contain children");
    
    // Test property constraint query
    let float_constraint = registry.get_property_constraint("float");
    assert!(float_constraint.is_some(), "float should have constraints");
    assert_eq!(float_constraint.unwrap().validation, "is_finite", "float should validate as finite");
    
    println!("✓ Composition rule queries work correctly");
}

#[test]
fn test_missing_widgets_added() {
    // Load registry
    let mut registry_path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    registry_path.push("../../unreal/metadata/widget_registry.json");
    
    let json_data = fs::read_to_string(&registry_path)
        .expect("Failed to read widget_registry.json");
    
    let mut registry = WidgetRegistry::new();
    registry.load(&json_data)
        .expect("Failed to load widget_registry.json");
    
    // Verify the missing widgets were added
    assert!(registry.is_known_widget("SEditorViewport"), "SEditorViewport should be present");
    assert!(registry.is_known_widget("SColorPicker"), "SColorPicker should be present");
    assert!(registry.is_known_widget("STableRow"), "STableRow should be present");
    
    println!("✓ Missing widgets were added successfully");
}
