use kain_core::ast::{GameplayTagNode, GameplayTagsNamespace};
use kain_core::span::Span;
use ue5_gas::tags_codegen;
use ue5_gas::tags_ir::GameplayTagsIR;

fn make_span() -> Span {
    Span { start: 0, end: 0 }
}

#[test]
fn test_tag_hierarchy_flattening() {
    let namespace = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "Melee".to_string(),
                full_path: "Ability.Attack.Melee".to_string(),
                comment: None,
                children: vec![GameplayTagNode {
                    name: "Sword".to_string(),
                    full_path: "Ability.Attack.Melee.Sword".to_string(),
                    comment: None,
                    children: vec![],
                    span: make_span(),
                }],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();

    // Should have 3 tags: Ability.Attack, Ability.Attack.Melee, Ability.Attack.Melee.Sword
    assert_eq!(ir.namespaces.len(), 1);
    assert_eq!(ir.namespaces[0].tags.len(), 3);

    let tags: Vec<String> = ir.namespaces[0]
        .tags
        .iter()
        .map(|t| t.tag.clone())
        .collect();

    assert!(tags.contains(&"Ability.Attack".to_string()));
    assert!(tags.contains(&"Ability.Attack.Melee".to_string()));
    assert!(tags.contains(&"Ability.Attack.Melee.Sword".to_string()));
}

#[test]
fn test_parent_tag_extraction() {
    let namespace = GameplayTagsNamespace {
        name: "Status".to_string(),
        children: vec![GameplayTagNode {
            name: "CC".to_string(),
            full_path: "Status.CC".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "Stunned".to_string(),
                full_path: "Status.CC.Stunned".to_string(),
                comment: None,
                children: vec![],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();

    // Find the Stunned tag
    let stunned_tag = ir.namespaces[0]
        .tags
        .iter()
        .find(|t| t.tag == "Status.CC.Stunned")
        .unwrap();

    assert_eq!(stunned_tag.parent, Some("Status.CC".to_string()));

    // Find the CC tag
    let cc_tag = ir.namespaces[0]
        .tags
        .iter()
        .find(|t| t.tag == "Status.CC")
        .unwrap();

    assert_eq!(cc_tag.parent, Some("Status".to_string()));
}

#[test]
fn test_duplicate_detection() {
    let namespace = GameplayTagsNamespace {
        name: "Test".to_string(),
        children: vec![
            GameplayTagNode {
                name: "Tag1".to_string(),
                full_path: "Test.Tag1".to_string(),
                comment: None,
                children: vec![],
                span: make_span(),
            },
            GameplayTagNode {
                name: "Tag1".to_string(),
                full_path: "Test.Tag1".to_string(),
                comment: None,
                children: vec![],
                span: make_span(),
            },
        ],
        span: make_span(),
    };

    let result = GameplayTagsIR::from_ast(vec![namespace]);
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Duplicate tag"));
}

#[test]
fn test_cpp_name_generation() {
    let namespace = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "Melee".to_string(),
                full_path: "Ability.Attack.Melee".to_string(),
                comment: None,
                children: vec![],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();

    let melee_tag = ir.namespaces[0]
        .tags
        .iter()
        .find(|t| t.tag == "Ability.Attack.Melee")
        .unwrap();

    assert_eq!(melee_tag.cpp_name, "Ability_Attack_Melee");
}

#[test]
fn test_native_cpp_header_generation() {
    let namespace = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: Some("Attack abilities".to_string()),
            children: vec![GameplayTagNode {
                name: "Melee".to_string(),
                full_path: "Ability.Attack.Melee".to_string(),
                comment: None,
                children: vec![],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify header structure
    assert!(output.header.contains("#pragma once"));
    assert!(output.header.contains("#include \"NativeGameplayTags.h\""));
    assert!(output.header.contains("namespace MyGameTags"));
    assert!(output.header.contains("namespace Ability"));
    assert!(output.header.contains("namespace Attack"));
    assert!(output
        .header
        .contains("MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee)"));
}

#[test]
fn test_native_cpp_implementation_generation() {
    let namespace = GameplayTagsNamespace {
        name: "Status".to_string(),
        children: vec![GameplayTagNode {
            name: "Alive".to_string(),
            full_path: "Status.Alive".to_string(),
            comment: Some("Character is alive".to_string()),
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify implementation structure
    assert!(output
        .implementation
        .contains("#include \"GameplayTags.h\""));
    assert!(output.implementation.contains("namespace MyGameTags"));
    assert!(output.implementation.contains("namespace Status"));
    assert!(output
        .implementation
        .contains("UE_DEFINE_GAMEPLAY_TAG_COMMENT"));
    assert!(output.implementation.contains("Alive"));
    assert!(output.implementation.contains("\"Status.Alive\""));
    assert!(output.implementation.contains("\"Character is alive\""));
}

#[test]
fn test_ini_file_generation() {
    let namespace = GameplayTagsNamespace {
        name: "Weapon".to_string(),
        children: vec![GameplayTagNode {
            name: "Type".to_string(),
            full_path: "Weapon.Type".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "Rifle".to_string(),
                full_path: "Weapon.Type.Rifle".to_string(),
                comment: Some("Rifle weapon type".to_string()),
                children: vec![],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify INI format
    assert!(output
        .ini_file
        .contains("[/Script/GameplayTags.GameplayTagsList]"));
    assert!(output.ini_file.contains("; Weapon Tags"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Weapon.Type\")"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Weapon.Type.Rifle\",DevComment=\"Rifle weapon type\")"));
}

#[test]
fn test_leaf_name_extraction() {
    let namespace = GameplayTagsNamespace {
        name: "Test".to_string(),
        children: vec![GameplayTagNode {
            name: "A".to_string(),
            full_path: "Test.A".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "B".to_string(),
                full_path: "Test.A.B".to_string(),
                comment: None,
                children: vec![GameplayTagNode {
                    name: "C".to_string(),
                    full_path: "Test.A.B.C".to_string(),
                    comment: None,
                    children: vec![],
                    span: make_span(),
                }],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();

    let tag_c = ir.namespaces[0]
        .tags
        .iter()
        .find(|t| t.tag == "Test.A.B.C")
        .unwrap();

    assert_eq!(tag_c.leaf_name(), "C");
    assert_eq!(tag_c.namespace_parts(), vec!["Test", "A", "B", "C"]);
}

#[test]
fn test_complex_hierarchy() {
    let namespace = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: None,
            children: vec![
                GameplayTagNode {
                    name: "Melee".to_string(),
                    full_path: "Ability.Attack.Melee".to_string(),
                    comment: None,
                    children: vec![
                        GameplayTagNode {
                            name: "Sword".to_string(),
                            full_path: "Ability.Attack.Melee.Sword".to_string(),
                            comment: None,
                            children: vec![],
                            span: make_span(),
                        },
                        GameplayTagNode {
                            name: "Axe".to_string(),
                            full_path: "Ability.Attack.Melee.Axe".to_string(),
                            comment: None,
                            children: vec![],
                            span: make_span(),
                        },
                    ],
                    span: make_span(),
                },
                GameplayTagNode {
                    name: "Ranged".to_string(),
                    full_path: "Ability.Attack.Ranged".to_string(),
                    comment: None,
                    children: vec![GameplayTagNode {
                        name: "Bow".to_string(),
                        full_path: "Ability.Attack.Ranged.Bow".to_string(),
                        comment: None,
                        children: vec![],
                        span: make_span(),
                    }],
                    span: make_span(),
                },
            ],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify all tags present
    assert!(output.ini_file.contains("Ability.Attack"));
    assert!(output.ini_file.contains("Ability.Attack.Melee"));
    assert!(output.ini_file.contains("Ability.Attack.Melee.Sword"));
    assert!(output.ini_file.contains("Ability.Attack.Melee.Axe"));
    assert!(output.ini_file.contains("Ability.Attack.Ranged"));
    assert!(output.ini_file.contains("Ability.Attack.Ranged.Bow"));

    // Verify nested namespace structure in header
    assert!(output.header.contains("namespace Ability"));
    assert!(output.header.contains("namespace Attack"));
    assert!(output.header.contains("namespace Melee"));
    assert!(output.header.contains("namespace Ranged"));
}

#[test]
fn test_duplicate_detection_across_namespaces() {
    let ns1 = GameplayTagsNamespace {
        name: "NS1".to_string(),
        children: vec![GameplayTagNode {
            name: "Tag".to_string(),
            full_path: "NS1.Tag".to_string(),
            comment: None,
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ns2 = GameplayTagsNamespace {
        name: "NS2".to_string(),
        children: vec![GameplayTagNode {
            name: "Tag".to_string(),
            full_path: "NS1.Tag".to_string(), // Same full path - should error
            comment: None,
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let result = GameplayTagsIR::from_ast(vec![ns1, ns2]);
    assert!(result.is_err());
}

#[test]
fn test_all_tags_accessor() {
    let ns1 = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: None,
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ns2 = GameplayTagsNamespace {
        name: "Status".to_string(),
        children: vec![GameplayTagNode {
            name: "Alive".to_string(),
            full_path: "Status.Alive".to_string(),
            comment: None,
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![ns1, ns2]).unwrap();
    let all_tags = ir.all_tags();

    assert_eq!(all_tags.len(), 2);

    let tag_strings: Vec<String> = all_tags.iter().map(|t| t.tag.clone()).collect();

    assert!(tag_strings.contains(&"Ability.Attack".to_string()));
    assert!(tag_strings.contains(&"Status.Alive".to_string()));
}

#[test]
fn test_get_namespace() {
    let ns1 = GameplayTagsNamespace {
        name: "Ability".to_string(),
        children: vec![GameplayTagNode {
            name: "Attack".to_string(),
            full_path: "Ability.Attack".to_string(),
            comment: None,
            children: vec![],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![ns1]).unwrap();

    let ability_ns = ir.get_namespace("Ability");
    assert!(ability_ns.is_some());
    assert_eq!(ability_ns.unwrap().name, "Ability");

    let missing_ns = ir.get_namespace("Status");
    assert!(missing_ns.is_none());
}

#[test]
fn test_namespace_parts() {
    let namespace = GameplayTagsNamespace {
        name: "Test".to_string(),
        children: vec![GameplayTagNode {
            name: "A".to_string(),
            full_path: "Test.A".to_string(),
            comment: None,
            children: vec![GameplayTagNode {
                name: "B".to_string(),
                full_path: "Test.A.B".to_string(),
                comment: None,
                children: vec![GameplayTagNode {
                    name: "C".to_string(),
                    full_path: "Test.A.B.C".to_string(),
                    comment: None,
                    children: vec![],
                    span: make_span(),
                }],
                span: make_span(),
            }],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();

    let tag_c = ir.namespaces[0]
        .tags
        .iter()
        .find(|t| t.tag == "Test.A.B.C")
        .unwrap();

    let parts = tag_c.namespace_parts();
    assert_eq!(parts, vec!["Test", "A", "B", "C"]);
}

#[test]
fn test_codegen_compiles_valid_cpp() {
    let namespace = GameplayTagsNamespace {
        name: "Status".to_string(),
        children: vec![GameplayTagNode {
            name: "CC".to_string(),
            full_path: "Status.CC".to_string(),
            comment: None,
            children: vec![
                GameplayTagNode {
                    name: "Stunned".to_string(),
                    full_path: "Status.CC.Stunned".to_string(),
                    comment: Some("Character is stunned and cannot act".to_string()),
                    children: vec![],
                    span: make_span(),
                },
                GameplayTagNode {
                    name: "Rooted".to_string(),
                    full_path: "Status.CC.Rooted".to_string(),
                    comment: Some("Character is rooted in place".to_string()),
                    children: vec![],
                    span: make_span(),
                },
            ],
            span: make_span(),
        }],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify header has proper structure
    assert!(output.header.contains("namespace MyGameTags"));
    assert!(output.header.contains("namespace Status"));
    assert!(output.header.contains("namespace CC"));
    assert!(output
        .header
        .contains("MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned)"));
    assert!(output
        .header
        .contains("MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted)"));

    // Verify implementation has proper definitions
    assert!(output
        .implementation
        .contains("UE_DEFINE_GAMEPLAY_TAG_COMMENT"));
    assert!(output.implementation.contains("Stunned"));
    assert!(output.implementation.contains("\"Status.CC.Stunned\""));
    assert!(output
        .implementation
        .contains("\"Character is stunned and cannot act\""));
    assert!(output.implementation.contains("Rooted"));
    assert!(output.implementation.contains("\"Status.CC.Rooted\""));
    assert!(output
        .implementation
        .contains("\"Character is rooted in place\""));
}

#[test]
fn test_ini_file_format() {
    let namespace = GameplayTagsNamespace {
        name: "Damage".to_string(),
        children: vec![
            GameplayTagNode {
                name: "Physical".to_string(),
                full_path: "Damage.Physical".to_string(),
                comment: Some("Physical damage type".to_string()),
                children: vec![],
                span: make_span(),
            },
            GameplayTagNode {
                name: "Magical".to_string(),
                full_path: "Damage.Magical".to_string(),
                comment: Some("Magical damage type".to_string()),
                children: vec![],
                span: make_span(),
            },
        ],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify INI structure
    assert!(output
        .ini_file
        .contains("[/Script/GameplayTags.GameplayTagsList]"));
    assert!(output.ini_file.contains("; Damage Tags"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Damage.Physical\",DevComment=\"Physical damage type\")"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Damage.Magical\",DevComment=\"Magical damage type\")"));
}

#[test]
fn test_empty_namespace() {
    let namespace = GameplayTagsNamespace {
        name: "Empty".to_string(),
        children: vec![],
        span: make_span(),
    };

    let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
    assert_eq!(ir.namespaces[0].tags.len(), 0);

    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Should still generate valid files
    assert!(output.header.contains("namespace MyGameTags"));
    assert!(output
        .ini_file
        .contains("[/Script/GameplayTags.GameplayTagsList]"));
}
