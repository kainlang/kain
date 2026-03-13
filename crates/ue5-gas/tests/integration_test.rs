use kain_core::{diagnostics::SpanMapper, lexer::Lexer, parser::Parser};
use ue5_gas::{tags_codegen, GameplayTagsIR};

#[test]
fn test_parse_and_generate_tags() {
    let source = r#"
@gameplay_tags
namespace Ability:
    Attack:
        Melee:
            Sword
            Axe
        Ranged:
            Bow
    Defend:
        Block

@gameplay_tags
namespace Status:
    Alive
    CC:
        Stunned
        Rooted
"#;

    // Lex
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();

    // Parse
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "test.kn");
    let program = parser.parse().unwrap();

    // Extract GameplayTags items
    let mut tag_namespaces = Vec::new();
    for item in &program.items {
        if let kain_core::ast::Item::GameplayTags(tags) = item {
            tag_namespaces.push(tags.clone());
        }
    }

    assert_eq!(tag_namespaces.len(), 2);

    // Convert to IR
    let ir = GameplayTagsIR::from_ast(tag_namespaces).unwrap();

    // Verify IR structure
    assert_eq!(ir.namespaces.len(), 2);

    let ability_ns = ir.get_namespace("Ability").unwrap();
    assert_eq!(ability_ns.name, "Ability");
    assert!(ability_ns.tags.iter().any(|t| t.tag == "Ability.Attack"));
    assert!(ability_ns
        .tags
        .iter()
        .any(|t| t.tag == "Ability.Attack.Melee"));
    assert!(ability_ns
        .tags
        .iter()
        .any(|t| t.tag == "Ability.Attack.Melee.Sword"));

    let status_ns = ir.get_namespace("Status").unwrap();
    assert_eq!(status_ns.name, "Status");
    assert!(status_ns.tags.iter().any(|t| t.tag == "Status.Alive"));
    assert!(status_ns.tags.iter().any(|t| t.tag == "Status.CC"));
    assert!(status_ns.tags.iter().any(|t| t.tag == "Status.CC.Stunned"));

    // Generate code
    let output = tags_codegen::generate(&ir, "TestGame").unwrap();

    // Verify header
    assert!(output.header.contains("#pragma once"));
    assert!(output.header.contains("#include \"NativeGameplayTags.h\""));
    assert!(output.header.contains("namespace TestGameTags"));
    assert!(output
        .header
        .contains("TESTGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN"));

    // Verify implementation
    assert!(output
        .implementation
        .contains("#include \"GameplayTags.h\""));
    assert!(output.implementation.contains("UE_DEFINE_GAMEPLAY_TAG"));
    assert!(output
        .implementation
        .contains("\"Ability.Attack.Melee.Sword\""));
    assert!(output.implementation.contains("\"Status.CC.Stunned\""));

    // Verify INI
    assert!(output
        .ini_file
        .contains("[/Script/GameplayTags.GameplayTagsList]"));
    assert!(output.ini_file.contains("; Ability Tags"));
    assert!(output.ini_file.contains("; Status Tags"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Ability.Attack\")"));
    assert!(output
        .ini_file
        .contains("GameplayTagList=(Tag=\"Status.Alive\")"));
}

#[test]
fn test_end_to_end_complex_hierarchy() {
    let source = r#"
@gameplay_tags
namespace Damage:
    Physical:
        Slash
        Pierce
        Blunt
    Magical:
        Fire
        Ice
        Lightning
"#;

    // Lex and parse
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "test.kn");
    let program = parser.parse().unwrap();

    // Extract tags
    let mut tag_namespaces = Vec::new();
    for item in &program.items {
        if let kain_core::ast::Item::GameplayTags(tags) = item {
            tag_namespaces.push(tags.clone());
        }
    }

    // Convert to IR and generate
    let ir = GameplayTagsIR::from_ast(tag_namespaces).unwrap();
    let output = tags_codegen::generate(&ir, "MyGame").unwrap();

    // Verify all damage types present
    assert!(output.ini_file.contains("Damage.Physical"));
    assert!(output.ini_file.contains("Damage.Physical.Slash"));
    assert!(output.ini_file.contains("Damage.Physical.Pierce"));
    assert!(output.ini_file.contains("Damage.Physical.Blunt"));
    assert!(output.ini_file.contains("Damage.Magical"));
    assert!(output.ini_file.contains("Damage.Magical.Fire"));
    assert!(output.ini_file.contains("Damage.Magical.Ice"));
    assert!(output.ini_file.contains("Damage.Magical.Lightning"));

    // Verify namespace structure
    assert!(output.header.contains("namespace Damage"));
    assert!(output.header.contains("namespace Physical"));
    assert!(output.header.contains("namespace Magical"));
}
