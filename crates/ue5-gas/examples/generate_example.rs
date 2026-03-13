use kain_core::{diagnostics::SpanMapper, lexer::Lexer, parser::Parser};
use ue5_gas::{tags_codegen, GameplayTagsIR};

fn main() {
    let source = r#"
@gameplay_tags
namespace Ability:
    Attack:
        Melee:
            Sword
            Axe
        Ranged:
            Bow
            Gun
    Defend:
        Block
        Parry

@gameplay_tags
namespace Status:
    Alive
    Dead
    CC:
        Stunned
        Rooted
        Silenced
"#;

    println!("=== KAIN SOURCE ===\n{}\n", source);

    // Lex
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().expect("Lexer failed");

    // Parse
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "example.kn");
    let program = parser.parse().expect("Parser failed");

    // Extract GameplayTags items
    let mut tag_namespaces = Vec::new();
    for item in &program.items {
        if let kain_core::ast::Item::GameplayTags(tags) = item {
            tag_namespaces.push(tags.clone());
        }
    }

    println!("Found {} tag namespaces\n", tag_namespaces.len());

    // Convert to IR
    let ir = GameplayTagsIR::from_ast(tag_namespaces).expect("IR conversion failed");

    println!("=== IR SUMMARY ===");
    for namespace in &ir.namespaces {
        println!("Namespace: {}", namespace.name);
        println!("  Tags: {}", namespace.tags.len());
        for tag in &namespace.tags {
            println!("    - {} (parent: {:?})", tag.tag, tag.parent);
        }
    }
    println!();

    // Generate code
    let output = tags_codegen::generate(&ir, "MyGame").expect("Codegen failed");

    println!("=== GENERATED GameplayTags.h ===\n{}\n", output.header);
    println!(
        "=== GENERATED GameplayTags.cpp ===\n{}\n",
        output.implementation
    );
    println!(
        "=== GENERATED DefaultGameplayTags.ini ===\n{}\n",
        output.ini_file
    );
}
