use crate::tags_ir::{GameplayTagsIR, GameplayTagIR};
use anyhow::Result;
use std::collections::BTreeMap;

/// Output from tag codegen
#[derive(Debug, Clone)]
pub struct TagCodegenOutput {
    pub header: String,
    pub implementation: String,
    pub ini_file: String,
}

/// Generate all tag files from IR
pub fn generate(ir: &GameplayTagsIR, plugin_name: &str) -> Result<TagCodegenOutput> {
    let header = generate_header(ir, plugin_name)?;
    let implementation = generate_implementation(ir, plugin_name)?;
    let ini_file = generate_ini_file(ir)?;
    
    Ok(TagCodegenOutput {
        header,
        implementation,
        ini_file,
    })
}

/// Generate GameplayTags.h with native tag declarations
fn generate_header(ir: &GameplayTagsIR, plugin_name: &str) -> Result<String> {
    let mut output = String::new();
    
    // Header guard and includes
    output.push_str("#pragma once\n");
    output.push_str("#include \"NativeGameplayTags.h\"\n\n");
    
    // API macro
    let api_macro = format!("{}_API", plugin_name.to_uppercase());
    
    // Generate namespace hierarchy
    output.push_str(&format!("namespace {}Tags\n", plugin_name));
    output.push_str("{\n");
    
    // Build hierarchical namespace structure
    let all_tags: Vec<&GameplayTagIR> = ir.all_tags();
    generate_namespace_hierarchy(&mut output, &all_tags, &api_macro, 1, true)?;
    
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate GameplayTags.cpp with native tag definitions
fn generate_implementation(ir: &GameplayTagsIR, plugin_name: &str) -> Result<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str("#include \"GameplayTags.h\"\n\n");
    
    // Generate namespace hierarchy with definitions
    output.push_str(&format!("namespace {}Tags\n", plugin_name));
    output.push_str("{\n");
    
    let all_tags: Vec<&GameplayTagIR> = ir.all_tags();
    generate_namespace_hierarchy(&mut output, &all_tags, "", 1, false)?;
    
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate hierarchical namespace structure recursively
/// depth: current depth in the tag hierarchy (1-indexed)
/// is_header: true for declarations, false for definitions
fn generate_namespace_hierarchy(
    output: &mut String,
    tags: &[&GameplayTagIR],
    api_macro: &str,
    depth: usize,
    is_header: bool,
) -> Result<()> {
    let indent = "    ".repeat(depth);

    fn emitted_leaf_symbol_name(tag: &GameplayTagIR, has_same_named_child_namespace: bool) -> String {
        let leaf = tag.leaf_name();
        if has_same_named_child_namespace {
            format!("{}Tag", leaf)
        } else {
            leaf.to_string()
        }
    }
    
    // Group tags by namespace at current depth
    let mut groups: BTreeMap<String, Vec<&GameplayTagIR>> = BTreeMap::new();
    
    for tag in tags {
        let parts = tag.namespace_parts();
        
        if parts.len() == depth {
            // Leaf tag at this depth - add to special "" group
            groups.entry("".to_string()).or_insert_with(Vec::new).push(tag);
        } else if parts.len() > depth {
            // Belongs to a child namespace
            let ns_key = parts[depth - 1].clone();
            groups.entry(ns_key).or_insert_with(Vec::new).push(tag);
        }
    }

    let child_namespace_names: std::collections::BTreeSet<String> = groups
        .keys()
        .filter(|k| !k.is_empty())
        .cloned()
        .collect();
    
    // Process groups in sorted order (BTreeMap ensures this)
    for (ns_name, group_tags) in groups {
        if ns_name.is_empty() {
            // Leaf tags at this level
            for tag in group_tags {
                let symbol_name = emitted_leaf_symbol_name(tag, child_namespace_names.contains(&tag.leaf_name().to_string()));
                if is_header {
                    output.push_str(&format!("{}{} UE_DECLARE_GAMEPLAY_TAG_EXTERN({});\n", 
                        indent, api_macro, symbol_name));
                } else {
                    if let Some(comment) = &tag.comment {
                        output.push_str(&format!("{}UE_DEFINE_GAMEPLAY_TAG_COMMENT(\n", indent));
                        output.push_str(&format!("{}    {},\n", indent, symbol_name));
                        output.push_str(&format!("{}    \"{}\",\n", indent, tag.tag));
                        output.push_str(&format!("{}    \"{}\"\n", indent, comment));
                        output.push_str(&format!("{});\n", indent));
                    } else {
                        output.push_str(&format!("{}UE_DEFINE_GAMEPLAY_TAG({}, \"{}\");\n", 
                            indent, symbol_name, tag.tag));
                    }
                }
            }
        } else {
            // Child namespace
            output.push_str(&format!("{}namespace {}\n", indent, ns_name));
            output.push_str(&format!("{}{{\n", indent));
            
            generate_namespace_hierarchy(output, &group_tags, api_macro, depth + 1, is_header)?;
            
            output.push_str(&format!("{}}}\n", indent));
        }
    }
    
    Ok(())
}

/// Generate DefaultGameplayTags.ini file
fn generate_ini_file(ir: &GameplayTagsIR) -> Result<String> {
    let mut output = String::new();
    
    // INI header
    output.push_str("[/Script/GameplayTags.GameplayTagsList]\n");
    
    // Generate tag list entries
    for namespace in &ir.namespaces {
        output.push_str(&format!("; {} Tags\n", namespace.name));
        
        for tag in &namespace.tags {
            if let Some(comment) = &tag.comment {
                output.push_str(&format!("GameplayTagList=(Tag=\"{}\",DevComment=\"{}\")\n", 
                    tag.tag, comment));
            } else {
                output.push_str(&format!("GameplayTagList=(Tag=\"{}\")\n", tag.tag));
            }
        }
        
        output.push_str("\n");
    }
    
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{GameplayTagsNamespace, GameplayTagNode};
    use kain_core::span::Span;
    
    fn make_span() -> Span {
        Span { start: 0, end: 0 }
    }
    
    #[test]
    fn test_simple_tag_hierarchy() {
        let namespace = GameplayTagsNamespace {
            name: "Ability".to_string(),
            children: vec![
                GameplayTagNode {
                    name: "Attack".to_string(),
                    full_path: "Ability.Attack".to_string(),
                    comment: Some("Attack abilities".to_string()),
                    children: vec![
                        GameplayTagNode {
                            name: "Melee".to_string(),
                            full_path: "Ability.Attack.Melee".to_string(),
                            comment: None,
                            children: vec![],
                            span: make_span(),
                        },
                    ],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
        let output = generate(&ir, "MyGame").unwrap();
        
        // Verify header contains declarations
        assert!(output.header.contains("UE_DECLARE_GAMEPLAY_TAG_EXTERN"));
        assert!(output.header.contains("namespace MyGameTags"));
        assert!(output.header.contains("namespace Ability"));
        
        // Verify implementation contains definitions
        assert!(output.implementation.contains("UE_DEFINE_GAMEPLAY_TAG"));
        assert!(output.implementation.contains("Ability.Attack"));
        
        // Verify INI file
        assert!(output.ini_file.contains("[/Script/GameplayTags.GameplayTagsList]"));
        assert!(output.ini_file.contains("Tag=\"Ability.Attack\""));
    }
    
    #[test]
    fn test_nested_namespaces() {
        let namespace = GameplayTagsNamespace {
            name: "Status".to_string(),
            children: vec![
                GameplayTagNode {
                    name: "CC".to_string(),
                    full_path: "Status.CC".to_string(),
                    comment: None,
                    children: vec![
                        GameplayTagNode {
                            name: "Stunned".to_string(),
                            full_path: "Status.CC.Stunned".to_string(),
                            comment: Some("Character is stunned".to_string()),
                            children: vec![],
                            span: make_span(),
                        },
                        GameplayTagNode {
                            name: "Rooted".to_string(),
                            full_path: "Status.CC.Rooted".to_string(),
                            comment: Some("Character is rooted".to_string()),
                            children: vec![],
                            span: make_span(),
                        },
                    ],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
        let output = generate(&ir, "MyGame").unwrap();
        
        // Verify nested namespace structure
        assert!(output.header.contains("namespace MyGameTags"));
        assert!(output.header.contains("namespace Status"));
        assert!(output.header.contains("namespace CC"));
        assert!(output.header.contains("UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned)"));
        assert!(output.header.contains("UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted)"));
        
        // Verify definitions with comments
        assert!(output.implementation.contains("UE_DEFINE_GAMEPLAY_TAG_COMMENT"));
        assert!(output.implementation.contains("Status.CC.Stunned"));
        assert!(output.implementation.contains("Character is stunned"));
    }
    
    #[test]
    fn test_multiple_namespaces() {
        let ns1 = GameplayTagsNamespace {
            name: "Ability".to_string(),
            children: vec![
                GameplayTagNode {
                    name: "Attack".to_string(),
                    full_path: "Ability.Attack".to_string(),
                    comment: None,
                    children: vec![],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ns2 = GameplayTagsNamespace {
            name: "Status".to_string(),
            children: vec![
                GameplayTagNode {
                    name: "Alive".to_string(),
                    full_path: "Status.Alive".to_string(),
                    comment: None,
                    children: vec![],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ir = GameplayTagsIR::from_ast(vec![ns1, ns2]).unwrap();
        let output = generate(&ir, "MyGame").unwrap();
        
        // Verify both namespaces present
        assert!(output.header.contains("namespace Ability"));
        assert!(output.header.contains("namespace Status"));
        assert!(output.ini_file.contains("Ability.Attack"));
        assert!(output.ini_file.contains("Status.Alive"));
    }
    
    #[test]
    fn test_ini_file_generation() {
        let namespace = GameplayTagsNamespace {
            name: "Weapon".to_string(),
            children: vec![
                GameplayTagNode {
                    name: "Type".to_string(),
                    full_path: "Weapon.Type".to_string(),
                    comment: None,
                    children: vec![
                        GameplayTagNode {
                            name: "Rifle".to_string(),
                            full_path: "Weapon.Type.Rifle".to_string(),
                            comment: Some("Rifle weapon type".to_string()),
                            children: vec![],
                            span: make_span(),
                        },
                        GameplayTagNode {
                            name: "Pistol".to_string(),
                            full_path: "Weapon.Type.Pistol".to_string(),
                            comment: Some("Pistol weapon type".to_string()),
                            children: vec![],
                            span: make_span(),
                        },
                    ],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
        let output = generate(&ir, "MyGame").unwrap();
        
        // Verify INI format
        assert!(output.ini_file.contains("[/Script/GameplayTags.GameplayTagsList]"));
        assert!(output.ini_file.contains("; Weapon Tags"));
        assert!(output.ini_file.contains("GameplayTagList=(Tag=\"Weapon.Type\")"));
        assert!(output.ini_file.contains("GameplayTagList=(Tag=\"Weapon.Type.Rifle\",DevComment=\"Rifle weapon type\")"));
        assert!(output.ini_file.contains("GameplayTagList=(Tag=\"Weapon.Type.Pistol\",DevComment=\"Pistol weapon type\")"));
    }
    
    #[test]
    fn test_complex_hierarchy() {
        let namespace = GameplayTagsNamespace {
            name: "Ability".to_string(),
            children: vec![
                GameplayTagNode {
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
                            children: vec![
                                GameplayTagNode {
                                    name: "Bow".to_string(),
                                    full_path: "Ability.Attack.Ranged.Bow".to_string(),
                                    comment: None,
                                    children: vec![],
                                    span: make_span(),
                                },
                            ],
                            span: make_span(),
                        },
                    ],
                    span: make_span(),
                },
            ],
            span: make_span(),
        };
        
        let ir = GameplayTagsIR::from_ast(vec![namespace]).unwrap();
        let output = generate(&ir, "MyGame").unwrap();
        
        // Verify all tags present in INI
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
}
