use kain_core::lexer::Lexer;
use kain_core::parser::Parser;

fn main() {
    let source = std::fs::read_to_string("test_material_graph.kn")
        .expect("Failed to read test file");
    
    println!("Source code:");
    println!("{}", source);
    println!("\n--- Parsing ---\n");
    
    let tokens = match Lexer::new(&source).tokenize() {
        Ok(t) => t,
        Err(e) => {
            eprintln!("Lexer error: {:?}", e);
            return;
        }
    };
    
    println!("Tokens: {} tokens", tokens.len());
    
    let mut parser = Parser::new(&tokens);
    match parser.parse() {
        Ok(program) => {
            println!("✓ Parsing successful!");
            println!("\nProgram has {} items", program.items.len());
            
            for (i, item) in program.items.iter().enumerate() {
                println!("\nItem {}: {:?}", i, item);
            }
        }
        Err(e) => {
            eprintln!("✗ Parser error: {:?}", e);
        }
    }
}
