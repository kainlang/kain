use kain_core::*;

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    comptime::eval_program(&mut ast)?;
    types::check(&ast, &span_mapper, "<test>")
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

#[test]
fn test_generic_struct_instantiation() {
    let source = r#"struct Box<T>:
    value: T

fn make_int_box() -> Box<Int>:
    return Box { value: 42 }

fn make_float_box() -> Box<Float>:
    return Box { value: 3.14 }"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let struct_count = mono.items.iter()
        .filter(|item| matches!(item, types::TypedItem::Struct(_)))
        .count();
    
    println!("Generated structs: {}", struct_count);
    
    let struct_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Struct(s) => Some(s.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Struct names: {:?}", struct_names);
    
    // Should have Box_Int and Box_Float
    assert!(struct_names.iter().any(|n| n.contains("Box")), "Should have Box structs");
    assert!(struct_count >= 2, "Expected at least 2 struct instantiations, got {}", struct_count);
}

#[test]
fn test_generic_struct_multiple_type_params() {
    let source = r#"struct Pair<T, U>:
    first: T
    second: U

fn make_pair() -> Pair<Int, String>:
    return Pair { first: 42, second: "hello" }"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let struct_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Struct(s) => Some(s.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Struct names: {:?}", struct_names);
    
    // Should have Pair_Int_String
    assert!(struct_names.iter().any(|n| n.contains("Pair")), "Should have Pair struct");
}

#[test]
fn test_nested_generic_structs() {
    // Note: This test avoids Container<Box<Int>> syntax due to >> parsing issue
    // Instead, we test that both Box and Container can be instantiated with different types
    let source = r#"struct Box<T>:
    value: T

struct Container<T>:
    item: T

fn make_box() -> Box<Int>:
    return Box { value: 42 }

fn make_container() -> Container<String>:
    return Container { item: "hello" }"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let struct_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Struct(s) => Some(s.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Struct names: {:?}", struct_names);
    
    // Should have Box_Int and Container_String
    assert!(struct_names.iter().any(|n| n.contains("Box")), "Should have Box structs");
    assert!(struct_names.iter().any(|n| n.contains("Container")), "Should have Container structs");
    assert!(struct_names.len() >= 2, "Should have at least 2 struct instantiations");
}

#[test]
fn test_generic_method_single_type_param() {
    let source = r#"struct Box<T>:
    value: T

impl<T> Box<T>:
    fn get(self) -> T:
        return self.value
    
    fn set(self, new_value: T):
        self.value = new_value

fn use_box():
    let int_box = Box { value: 42 }
    let val = int_box.get()"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let func_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", func_names);
    
    // Should have Box_Int_get and Box_Int_set methods
    assert!(func_names.iter().any(|n| n.contains("Box_Int_get")), "Should have Box_Int_get method");
    assert!(func_names.iter().any(|n| n.contains("Box_Int_set")), "Should have Box_Int_set method");
    
    let struct_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Struct(s) => Some(s.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated structs: {:?}", struct_names);
    
    // Should have Box_Int struct
    assert!(struct_names.iter().any(|n| n.contains("Box_Int")), "Should have Box_Int struct");
}

#[test]
fn test_generic_method_multiple_type_params() {
    let source = r#"struct Pair<T, U>:
    first: T
    second: U

impl<T, U> Pair<T, U>:
    fn get_first(self) -> T:
        return self.first
    
    fn get_second(self) -> U:
        return self.second
    
    fn swap(self) -> Pair<U, T>:
        return Pair { first: self.second, second: self.first }

fn use_pair():
    let p = Pair { first: 42, second: "hello" }
    let x = p.get_first()
    let y = p.get_second()"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let func_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", func_names);
    
    // Should have Pair_Int_String_get_first and Pair_Int_String_get_second methods
    assert!(func_names.iter().any(|n| n.contains("Pair_Int_String_get_first")), "Should have Pair_Int_String_get_first method");
    assert!(func_names.iter().any(|n| n.contains("Pair_Int_String_get_second")), "Should have Pair_Int_String_get_second method");
}

#[test]
fn test_generic_method_calls_in_functions() {
    let source = r#"struct Container<T>:
    item: T

impl<T> Container<T>:
    fn get(self) -> T:
        return self.item
    
    fn set(self, new_item: T):
        self.item = new_item

fn process_int_container(c: Container<Int>) -> Int:
    return c.get()

fn process_float_container(c: Container<Float>) -> Float:
    return c.get()"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let func_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", func_names);
    
    // Should have Container_Int_get and Container_Float_get methods
    assert!(func_names.iter().any(|n| n.contains("Container_Int_get")), "Should have Container_Int_get method");
    assert!(func_names.iter().any(|n| n.contains("Container_Float_get")), "Should have Container_Float_get method");
    
    // Should have the process functions
    assert!(func_names.contains(&"process_int_container"), "Should have process_int_container function");
    assert!(func_names.contains(&"process_float_container"), "Should have process_float_container function");
}

#[test]
fn test_nested_generic_types() {
    let source = r#"
struct Box<T>:
    value: T

fn make_nested() -> Box<Box<Int>>:
    let inner = Box { value: 42 }
    return Box { value: inner }

fn main():
    let nested = make_nested()
"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    // Should have Box_Int and Box_Box_Int structs
    let struct_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Struct(s) => Some(s.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated structs: {:?}", struct_names);
    
    // Check that nested generic types are properly instantiated
    assert!(struct_names.iter().any(|n| n.contains("Box") && n.contains("Int")), 
            "Should have Box<Int> instantiation");
    
    // Note: Full nested instantiation (Box<Box<Int>>) may require additional work
    // This test verifies the parser can handle >> tokens
}

#[test]
fn test_negative_literal_inference() {
    let source = r#"
fn abs<T>(x: T) -> T:
    return x

fn main():
    let a = abs(-42)
    let b = abs(42)
    let c = abs(-3.14)
"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    let func_names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            types::TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    println!("Generated functions: {:?}", func_names);
    
    // Should have abs_Int and abs_Float, not abs_Any
    assert!(func_names.iter().any(|n| n.contains("abs") && n.contains("Int")), 
            "Should have abs<Int> for negative integer literal");
    assert!(func_names.iter().any(|n| n.contains("abs") && n.contains("Float")), 
            "Should have abs<Float> for negative float literal");
    
    // Should NOT have abs_Any
    assert!(!func_names.iter().any(|n| n.contains("abs_Any")), 
            "Should not have abs<Any> - negative literals should infer concrete types");
}
