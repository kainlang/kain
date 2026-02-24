// ============================================================================
// Gameplay Cue IR Tests
// ============================================================================
// Comprehensive tests for gameplay cue IR conversion and validation
// ============================================================================

use kain_core::ast::{GameplayCueDef, CueType, Attribute, Field, Function, Block};
use kain_core::span::Span;
use ue5_gas::cue_ir::{GameplayCueIR, CueTypeIR};

/// Helper to create a test cue with minimal fields
fn create_test_cue(name: &str) -> GameplayCueDef {
    GameplayCueDef {
        name: name.to_string(),
        attributes: vec![Attribute {
            name: "gameplay_cue".to_string(),
            args: Vec::new(),
            span: Span::default(),
        }],
        tag: "GameplayCue.Test".to_string(),
        cue_type: CueType::Static,
        auto_destroy: false,
        state_fields: vec![],
        on_execute: None,
        on_add: None,
        on_remove: None,
        while_active: None,
        span: Span::default(),
    }
}

// ============================================================================
// Basic Cue Type Tests
// ============================================================================

#[test]
fn test_minimal_static_cue() {
    let cue_def = create_test_cue("TestCue");
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.name, "TestCue");
    assert_eq!(cue_ir.tag, "GameplayCue.Test");
    assert_eq!(cue_ir.cue_type, CueTypeIR::Static);
    assert!(!cue_ir.auto_destroy);
}

#[test]
fn test_actor_cue_type() {
    let mut cue_def = create_test_cue("ActorCue");
    cue_def.cue_type = CueType::Actor;
    cue_def.auto_destroy = true;
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.cue_type, CueTypeIR::Actor);
    assert!(cue_ir.auto_destroy);
}

#[test]
fn test_static_cue_with_auto_destroy() {
    let mut cue_def = create_test_cue("StaticCue");
    cue_def.cue_type = CueType::Static;
    cue_def.auto_destroy = true;
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.cue_type, CueTypeIR::Static);
    // auto_destroy is ignored for Static cues
    assert!(cue_ir.auto_destroy);
}

// ============================================================================
// Tag Validation Tests
// ============================================================================

#[test]
fn test_tag_validation_valid_prefix() {
    let mut cue_def = create_test_cue("ValidCue");
    cue_def.tag = "GameplayCue.Effect.Burn".to_string();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_ok());
}

#[test]
fn test_tag_validation_missing_prefix() {
    let mut cue_def = create_test_cue("InvalidCue");
    cue_def.tag = "Invalid.Tag".to_string();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("must start with 'GameplayCue.'"));
}

#[test]
fn test_tag_validation_empty() {
    let mut cue_def = create_test_cue("InvalidCue");
    cue_def.tag = "".to_string();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Tag cannot be empty"));
}

#[test]
fn test_tag_validation_only_prefix() {
    let mut cue_def = create_test_cue("InvalidCue");
    cue_def.tag = "GameplayCue.".to_string();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_err());
}

#[test]
fn test_tag_validation_nested() {
    let mut cue_def = create_test_cue("NestedCue");
    cue_def.tag = "GameplayCue.Effect.Fire.Burn.Intense".to_string();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_ok());
}

// ============================================================================
// Attribute Validation Tests
// ============================================================================

#[test]
fn test_missing_gameplay_cue_attribute() {
    let mut cue_def = create_test_cue("InvalidCue");
    cue_def.attributes.clear();
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("must have @gameplay_cue attribute"));
}

#[test]
fn test_multiple_attributes() {
    let mut cue_def = create_test_cue("MultiAttrCue");
    cue_def.attributes.push(Attribute {
        name: "some_other_attr".to_string(),
        args: Vec::new(),
        span: Span::default(),
    });
    
    let result = GameplayCueIR::from_ast(&cue_def);
    
    // Should still work as long as @gameplay_cue is present
    assert!(result.is_ok());
}

// ============================================================================
// Lifecycle Method Tests
// ============================================================================

#[test]
fn test_on_execute_method() {
    let mut cue_def = create_test_cue("ExecuteCue");
    cue_def.on_execute = Some(Function {
        name: "on_execute".to_string(),
        params: vec![],
        return_type: None,
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert!(cue_ir.on_execute_body.is_some());
}

#[test]
fn test_on_add_method() {
    let mut cue_def = create_test_cue("AddCue");
    cue_def.cue_type = CueType::Actor;
    cue_def.on_add = Some(Function {
        name: "on_add".to_string(),
        params: vec![],
        return_type: None,
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert!(cue_ir.on_add_body.is_some());
}

#[test]
fn test_on_remove_method() {
    let mut cue_def = create_test_cue("RemoveCue");
    cue_def.cue_type = CueType::Actor;
    cue_def.on_remove = Some(Function {
        name: "on_remove".to_string(),
        params: vec![],
        return_type: None,
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert!(cue_ir.on_remove_body.is_some());
}

#[test]
fn test_while_active_method() {
    let mut cue_def = create_test_cue("ActiveCue");
    cue_def.cue_type = CueType::Actor;
    cue_def.while_active = Some(Function {
        name: "while_active".to_string(),
        params: vec![],
        return_type: None,
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert!(cue_ir.while_active_body.is_some());
}

#[test]
fn test_all_lifecycle_methods() {
    let mut cue_def = create_test_cue("CompleteCue");
    cue_def.cue_type = CueType::Actor;
    
    let empty_func = |name: &str| Function {
        name: name.to_string(),
        params: vec![],
        return_type: None,
        body: Block { stmts: vec![], span: Span::default() },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    };
    
    cue_def.on_execute = Some(empty_func("on_execute"));
    cue_def.on_add = Some(empty_func("on_add"));
    cue_def.on_remove = Some(empty_func("on_remove"));
    cue_def.while_active = Some(empty_func("while_active"));
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert!(cue_ir.on_execute_body.is_some());
    assert!(cue_ir.on_add_body.is_some());
    assert!(cue_ir.on_remove_body.is_some());
    assert!(cue_ir.while_active_body.is_some());
}

// ============================================================================
// State Field Tests
// ============================================================================

#[test]
fn test_state_fields() {
    let mut cue_def = create_test_cue("StatefulCue");
    cue_def.cue_type = CueType::Actor;
    cue_def.state_fields.push(Field {
        name: "particle_system".to_string(),
        ty: kain_core::ast::Type::Named {
            name: "ParticleSystemComponent".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default: None,
        attributes: vec![],
        visibility: kain_core::ast::Visibility::Public,
        weak: false,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.state_fields.len(), 1);
    assert_eq!(cue_ir.state_fields[0].name, "particle_system");
}

#[test]
fn test_multiple_state_fields() {
    let mut cue_def = create_test_cue("MultiStateCue");
    cue_def.cue_type = CueType::Actor;
    
    cue_def.state_fields.push(Field {
        name: "particle_system".to_string(),
        ty: kain_core::ast::Type::Named {
            name: "ParticleSystemComponent".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default: None,
        attributes: vec![],
        visibility: kain_core::ast::Visibility::Public,
        weak: false,
        span: Span::default(),
    });
    
    cue_def.state_fields.push(Field {
        name: "audio_component".to_string(),
        ty: kain_core::ast::Type::Named {
            name: "AudioComponent".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default: None,
        attributes: vec![],
        visibility: kain_core::ast::Visibility::Public,
        weak: false,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.state_fields.len(), 2);
}

// ============================================================================
// Complete Cue Tests
// ============================================================================

#[test]
fn test_complete_static_cue() {
    let mut cue_def = create_test_cue("BurnCue");
    cue_def.tag = "GameplayCue.Effect.Burn".to_string();
    cue_def.cue_type = CueType::Static;
    cue_def.on_execute = Some(Function {
        name: "on_execute".to_string(),
        params: vec![],
        return_type: None,
        body: Block { stmts: vec![], span: Span::default() },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    });
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.name, "BurnCue");
    assert_eq!(cue_ir.tag, "GameplayCue.Effect.Burn");
    assert_eq!(cue_ir.cue_type, CueTypeIR::Static);
    assert!(cue_ir.on_execute_body.is_some());
}

#[test]
fn test_complete_actor_cue() {
    let mut cue_def = create_test_cue("HealCue");
    cue_def.tag = "GameplayCue.Effect.Heal".to_string();
    cue_def.cue_type = CueType::Actor;
    cue_def.auto_destroy = true;
    
    cue_def.state_fields.push(Field {
        name: "particle_system".to_string(),
        ty: kain_core::ast::Type::Named {
            name: "ParticleSystemComponent".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default: None,
        attributes: vec![],
        visibility: kain_core::ast::Visibility::Public,
        weak: false,
        span: Span::default(),
    });
    
    let empty_func = |name: &str| Function {
        name: name.to_string(),
        params: vec![],
        return_type: None,
        body: Block { stmts: vec![], span: Span::default() },
        attributes: vec![],
        effects: vec![],
        generics: vec![],
        visibility: kain_core::ast::Visibility::Public,
        span: Span::default(),
    };
    
    cue_def.on_add = Some(empty_func("on_add"));
    cue_def.on_remove = Some(empty_func("on_remove"));
    
    let cue_ir = GameplayCueIR::from_ast(&cue_def).unwrap();
    
    assert_eq!(cue_ir.name, "HealCue");
    assert_eq!(cue_ir.tag, "GameplayCue.Effect.Heal");
    assert_eq!(cue_ir.cue_type, CueTypeIR::Actor);
    assert!(cue_ir.auto_destroy);
    assert_eq!(cue_ir.state_fields.len(), 1);
    assert!(cue_ir.on_add_body.is_some());
    assert!(cue_ir.on_remove_body.is_some());
}
