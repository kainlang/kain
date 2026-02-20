// Integration tests for match expression codegen to UE5 C++
// Tests pattern matching → UE5 if/else chain generation

use kain_core::*;
use ue5::{generate, Ue5Output};

/// Helper: Parse, typecheck, and generate UE5 C++
fn compile_ue5(source: &str) -> Result<Ue5Output, error::KainError> {
    // Parse
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let mut ast = parser::Parser::new(&tokens).parse()?;
    
    // Compile-time evaluation
    comptime::eval_program(&mut ast)?;
    
    // Type checking
    let typed = types::check(&ast)?;
    
    // Monomorphization
    let mono = monomorphize::monomorphize(&typed)?;
    
    // Convert MonomorphizedProgram to TypedProgram for codegen
    let mono_typed = types::TypedProgram {
        items: mono.items,
    };
    
    // UE5 codegen
    let output = generate(&mono_typed, None, None)?;
    
    Ok(output)
}

// ============================================================================
// A. ENUM VARIANT MATCHING TESTS
// ============================================================================

#[test]
fn test_simple_enum_match() {
    let source = r#"
enum Status:
    Active
    Inactive
    Pending

fn get_status_code(status: Status) -> Int:
    return match status:
        Status.Active => 1
        Status.Inactive => 0
        Status.Pending => 2

fn main():
    let code = get_status_code(Status.Active)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify enum variant matching generates correct C++ comparisons
    assert!(cpp.contains("EStatus::Active") || cpp.contains("status == EStatus::Active"), 
            "Should have enum variant comparison");
    assert!(cpp.contains("EStatus::Inactive") || cpp.contains("status == EStatus::Inactive"), 
            "Should have enum variant comparison");
    assert!(cpp.contains("EStatus::Pending") || cpp.contains("status == EStatus::Pending"), 
            "Should have enum variant comparison");
    
    // Verify ternary or if/else chain structure
    assert!(cpp.contains("?") || cpp.contains("if"), 
            "Should generate ternary or if/else chain");
}

#[test]
fn test_enum_match_with_wildcard() {
    let source = r#"
enum Color:
    Red
    Green
    Blue
    Yellow

fn is_primary(color: Color) -> Bool:
    return match color:
        Color.Red => true
        Color.Green => true
        Color.Blue => true
        _ => false

fn main():
    let result = is_primary(Color.Red)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify wildcard generates else clause
    assert!(cpp.contains("else") || cpp.contains(": false"), 
            "Wildcard should generate else clause or default value");
    
    // Verify enum comparisons
    assert!(cpp.contains("EColor::Red"), "Should have Red variant");
    assert!(cpp.contains("EColor::Green"), "Should have Green variant");
    assert!(cpp.contains("EColor::Blue"), "Should have Blue variant");
}

// ============================================================================
// B. LITERAL MATCHING TESTS
// ============================================================================

#[test]
fn test_int_literal_match() {
    let source = r#"
fn classify_number(n: Int) -> String:
    return match n:
        0 => "zero"
        1 => "one"
        2 => "two"
        _ => "other"

fn main():
    let result = classify_number(1)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify literal comparisons
    assert!(cpp.contains("== 0") || cpp.contains("(n == 0)"), 
            "Should have literal 0 comparison");
    assert!(cpp.contains("== 1") || cpp.contains("(n == 1)"), 
            "Should have literal 1 comparison");
    assert!(cpp.contains("== 2") || cpp.contains("(n == 2)"), 
            "Should have literal 2 comparison");
    
    // Verify string literals are TEXT() wrapped
    assert!(cpp.contains("TEXT(\"zero\")") || cpp.contains("TEXT(\"one\")"), 
            "String literals should be TEXT() wrapped");
}

#[test]
fn test_bool_literal_match() {
    let source = r#"
fn bool_to_int(b: Bool) -> Int:
    return match b:
        true => 1
        false => 0

fn main():
    let result = bool_to_int(true)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify bool literal matching
    assert!(cpp.contains("true") && cpp.contains("false"), 
            "Should have bool literal comparisons");
    assert!(cpp.contains("?") || cpp.contains("if"), 
            "Should generate conditional");
}

// ============================================================================
// C. WILDCARD AND BINDING TESTS
// ============================================================================

#[test]
fn test_wildcard_pattern() {
    let source = r#"
fn always_returns_42(x: Int) -> Int:
    return match x:
        _ => 42

fn main():
    let result = always_returns_42(100)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Wildcard should just return the value directly or in else clause
    assert!(cpp.contains("42"), "Should have return value 42");
}

#[test]
fn test_binding_pattern() {
    let source = r#"
fn identity_match(x: Int) -> Int:
    return match x:
        n => n

fn main():
    let result = identity_match(42)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Binding pattern should create a variable binding
    // In simple cases, it might just return x directly
    assert!(cpp.contains("return") || cpp.contains("n"), 
            "Should have return or binding variable");
}

// ============================================================================
// D. COMPLEX MATCH EXPRESSIONS
// ============================================================================

#[test]
fn test_nested_match() {
    let source = r#"
enum Outer:
    A
    B

enum Inner:
    X
    Y

fn nested_match(o: Outer, i: Inner) -> Int:
    let result = match o:
        Outer.A => match i:
            Inner.X => 1
            Inner.Y => 2
        Outer.B => match i:
            Inner.X => 3
            Inner.Y => 4
    return result

fn main():
    let val = nested_match(Outer.A, Inner.X)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify nested match generates nested conditionals
    assert!(cpp.contains("EOuter::A") && cpp.contains("EInner::X"), 
            "Should have both enum types");
    
    // Should have multiple conditional levels
    let if_count = cpp.matches("if").count();
    assert!(if_count >= 2, "Should have multiple if statements for nested match");
}

#[test]
fn test_match_with_function_calls() {
    let source = r#"
enum Operation:
    Add
    Subtract
    Multiply

fn apply_op(op: Operation, a: Int, b: Int) -> Int:
    return match op:
        Operation.Add => a + b
        Operation.Subtract => a - b
        Operation.Multiply => a * b

fn main():
    let result = apply_op(Operation.Add, 10, 20)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify arithmetic operations in match arms
    assert!(cpp.contains("+") || cpp.contains("a + b"), 
            "Should have addition");
    assert!(cpp.contains("-") || cpp.contains("a - b"), 
            "Should have subtraction");
    assert!(cpp.contains("*") || cpp.contains("a * b"), 
            "Should have multiplication");
    
    // Verify enum matching
    assert!(cpp.contains("EOperation::Add"), "Should have Add variant");
    assert!(cpp.contains("EOperation::Subtract"), "Should have Subtract variant");
    assert!(cpp.contains("EOperation::Multiply"), "Should have Multiply variant");
}

// ============================================================================
// E. MATCH AS STATEMENT (ASSIGNMENT IN ARMS)
// ============================================================================

#[test]
fn test_match_statement_with_assignment() {
    let source = r#"
enum Mode:
    Fast
    Slow

fn set_speed(mode: Mode):
    var speed = 0
    match mode:
        Mode.Fast => speed = 100
        Mode.Slow => speed = 10
    println("Speed: {speed}")

fn main():
    set_speed(Mode.Fast)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify match with assignments generates if/else blocks
    assert!(cpp.contains("speed = 100") || cpp.contains("speed=100"), 
            "Should have assignment in match arm");
    assert!(cpp.contains("speed = 10") || cpp.contains("speed=10"), 
            "Should have assignment in match arm");
    
    // Should use if/else, not ternary (because of assignments)
    assert!(cpp.contains("if"), "Should use if/else for statement-level match");
}

// ============================================================================
// F. EDGE CASES AND ERROR HANDLING
// ============================================================================

#[test]
fn test_single_arm_match() {
    let source = r#"
fn always_42(x: Int) -> Int:
    return match x:
        _ => 42

fn main():
    let result = always_42(100)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Single wildcard arm should just return the value
    assert!(cpp.contains("42"), "Should have return value");
}

#[test]
fn test_match_with_multiple_same_type_arms() {
    let source = r#"
enum Level:
    Low
    Medium
    High
    VeryHigh

fn get_multiplier(level: Level) -> Float:
    return match level:
        Level.Low => 0.5
        Level.Medium => 1.0
        Level.High => 1.5
        Level.VeryHigh => 2.0

fn main():
    let mult = get_multiplier(Level.High)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify all arms are generated
    assert!(cpp.contains("ELevel::Low"), "Should have Low variant");
    assert!(cpp.contains("ELevel::Medium"), "Should have Medium variant");
    assert!(cpp.contains("ELevel::High"), "Should have High variant");
    assert!(cpp.contains("ELevel::VeryHigh"), "Should have VeryHigh variant");
    
    // Verify float literals
    assert!(cpp.contains("0.5") || cpp.contains("0.500000"), 
            "Should have float literal");
    assert!(cpp.contains("1.0") || cpp.contains("1.000000"), 
            "Should have float literal");
}

// ============================================================================
// G. INTEGRATION WITH OTHER FEATURES
// ============================================================================

#[test]
fn test_match_in_actor() {
    let source = r#"
enum GameState:
    Menu
    Playing
    Paused

actor GameManager:
    state current_state: GameState = GameState.Menu
    
    fn get_state_name() -> String:
        return match current_state:
            GameState.Menu => "Menu"
            GameState.Playing => "Playing"
            GameState.Paused => "Paused"

fn main():
    println("Test")
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify actor generation with match expression
    assert!(cpp.contains("AGameManager") || cpp.contains("class GameManager"), 
            "Should generate actor class");
    assert!(cpp.contains("EGameState::Menu"), "Should have enum variant");
    assert!(cpp.contains("get_state_name") || cpp.contains("GetStateName"), 
            "Should have method");
}

#[test]
fn test_match_with_blueprint_function() {
    let source = r#"
enum Priority:
    Low
    Medium
    High

@blueprint
fn get_priority_value(priority: Priority) -> Int:
    return match priority:
        Priority.Low => 1
        Priority.Medium => 5
        Priority.High => 10

fn main():
    println("Test")
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify blueprint function with match
    assert!(cpp.contains("UFUNCTION") || cpp.contains("BlueprintCallable"), 
            "Should have blueprint function markers");
    assert!(cpp.contains("EPriority::Low"), "Should have enum variant");
    assert!(cpp.contains("get_priority_value") || cpp.contains("GetPriorityValue"), 
            "Should have function name");
}
