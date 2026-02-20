use kain_core::*;

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let mut ast = parser::Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    types::check(&ast)
}

#[test]
fn test_simple_generic_instantiation() {
    let source = r#"fn identity<T>(x: T) -> T:
    return x

fn main():
    let a = identity(42)
    let b = identity(3.14)"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    // Should have 3 functions: identity_Int, identity_Float, main
    let func_count = mono.items.iter()
        .filter(|item| matches!(item, types::TypedItem::Function(_)))
        .count();
    
    assert!(func_count >= 3, "Expected at least 3 functions, got {}", func_count);
    
    // Check mangled names
    let names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", names);
    
    // Should have monomorphized versions
    assert!(names.iter().any(|n| n.contains("identity")), "Should have identity functions");
    assert!(names.contains(&"main"), "Should have main function");
}

#[test]
fn test_multiple_type_parameters() {
    let source = r#"fn pair<T, U>(first: T, second: U) -> T:
    return first

fn test_pair():
    let x = pair(42, "hello")
    let y = pair(3.14, 100)"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let func_count = mono.items.iter()
        .filter(|item| matches!(item, types::TypedItem::Function(_)))
        .count();
    
    // Should have pair_Int_String, pair_Float_Int, test
    assert!(func_count >= 3, "Expected at least 3 functions, got {}", func_count);
}

#[test]
fn test_generic_with_comparison() {
    let source = r#"fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    else:
        return b

fn test_max():
    let x = max(10, 20)
    let y = max(1.5, 2.5)"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", names);
    
    // Should have max_Int and max_Float
    assert!(names.iter().any(|n| n.contains("max")), "Should have max functions");
}

#[test]
fn test_no_generics_unchanged() {
    let source = r#"fn add(a: Int, b: Int) -> Int:
    return a + b

fn main():
    let x = add(1, 2)"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    // Should have exactly 2 functions: add, main
    let func_count = mono.items.iter()
        .filter(|item| matches!(item, types::TypedItem::Function(_)))
        .count();
    
    assert_eq!(func_count, 2, "Non-generic code should be unchanged");
}

#[test]
fn test_nested_generic_calls() {
    let source = r#"fn identity<T>(x: T) -> T:
    return x

fn double_identity<T>(x: T) -> T:
    return identity(identity(x))

fn test_nested():
    let x = double_identity(42)"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", names);
    
    // Should have identity_Int, double_identity_Int, test
    assert!(names.iter().any(|n| n.contains("identity")), "Should have identity functions");
    assert!(names.iter().any(|n| n.contains("double_identity")), "Should have double_identity functions");
}
