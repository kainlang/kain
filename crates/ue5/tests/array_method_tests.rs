// Integration tests for KAIN array method translation to UE5 TArray methods
// Tests that KAIN array methods (.len(), .push(), .pop(), .clear()) correctly
// map to UE5 TArray equivalents (.Num(), .Add(), .Pop(), .Empty())

use kain_core::*;
use ue5::{generate, Ue5Output};

/// Helper: Parse, typecheck, monomorphize, and generate UE5 C++
fn compile_ue5(source: &str) -> Result<Ue5Output, error::KainError> {
    // Parse
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = kain_core::diagnostics::SpanMapper::new(source);
    let mut ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    
    // Compile-time evaluation
    comptime::eval_program(&mut ast)?;
    
    // Type checking
    let typed = types::check(&ast, &span_mapper, "<test>")?;
    
    // Monomorphization
    let mono = monomorphize::monomorphize(&typed)?;
    
    // UE5 codegen
    let output = generate(&mono, None, None)?;
    
    Ok(output)
}

// ============================================================================
// A. BASIC ARRAY METHOD TRANSLATION TESTS
// ============================================================================

#[test]
fn test_array_len_method() {
    let source = r#"
fn get_array_size(items: Array<Int>) -> Int:
    return items.len()

fn main():
    let arr: Array<Int> = []
    let size = get_array_size(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .len() → .Num()
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num()");
    assert!(!cpp.contains(".len()"), "Should not contain KAIN .len() in output");
}

#[test]
fn test_array_push_method() {
    let source = r#"
fn add_item(items: Array<Int>, value: Int):
    items.push(value)

fn main():
    let arr: Array<Int> = []
    add_item(arr, 42)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .push() → .Add()
    assert!(cpp.contains(".Add("), "Should translate .push() to .Add()");
    assert!(!cpp.contains(".push("), "Should not contain KAIN .push() in output");
}

#[test]
fn test_array_pop_method() {
    let source = r#"
fn remove_last(items: Array<Int>) -> Int:
    return items.pop()

fn main():
    let arr: Array<Int> = [1, 2, 3]
    let last = remove_last(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .pop() → .Pop()
    assert!(cpp.contains(".Pop("), "Should translate .pop() to .Pop()");
    assert!(!cpp.contains(".pop("), "Should not contain KAIN .pop() in output");
}

#[test]
fn test_array_clear_method() {
    let source = r#"
fn clear_array(items: Array<Int>):
    items.clear()

fn main():
    let arr: Array<Int> = [1, 2, 3]
    clear_array(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .clear() → .Empty()
    assert!(cpp.contains(".Empty("), "Should translate .clear() to .Empty()");
    assert!(!cpp.contains(".clear("), "Should not contain KAIN .clear() in output");
}

// ============================================================================
// B. ALTERNATIVE METHOD NAME TESTS
// ============================================================================

#[test]
fn test_array_length_alias() {
    let source = r#"
fn get_length(items: Array<Float>) -> Int:
    return items.length()

fn main():
    let arr: Array<Float> = [1.0, 2.0]
    let len = get_length(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .length() → .Num()
    assert!(cpp.contains(".Num()"), "Should translate .length() to .Num()");
    assert!(!cpp.contains(".length()"), "Should not contain KAIN .length() in output");
}

#[test]
fn test_array_count_alias() {
    let source = r#"
fn count_items(items: Array<String>) -> Int:
    return items.count()

fn main():
    let arr: Array<String> = ["a", "b", "c"]
    let cnt = count_items(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .count() → .Num()
    assert!(cpp.contains(".Num()"), "Should translate .count() to .Num()");
    assert!(!cpp.contains(".count()"), "Should not contain KAIN .count() in output");
}

#[test]
fn test_array_size_alias() {
    let source = r#"
fn get_size(items: Array<Bool>) -> Int:
    return items.size()

fn main():
    let arr: Array<Bool> = [true, false]
    let sz = get_size(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .size() → .Num()
    assert!(cpp.contains(".Num()"), "Should translate .size() to .Num()");
    assert!(!cpp.contains(".size()"), "Should not contain KAIN .size() in output");
}

#[test]
fn test_array_append_alias() {
    let source = r#"
fn append_item(items: Array<Int>, value: Int):
    items.append(value)

fn main():
    let arr: Array<Int> = []
    append_item(arr, 10)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .append() → .Add()
    assert!(cpp.contains(".Add("), "Should translate .append() to .Add()");
    assert!(!cpp.contains(".append("), "Should not contain KAIN .append() in output");
}

#[test]
fn test_array_add_alias() {
    let source = r#"
fn add_value(items: Array<Float>, value: Float):
    items.add(value)

fn main():
    let arr: Array<Float> = []
    add_value(arr, 3.14)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .add() → .Add()
    assert!(cpp.contains(".Add("), "Should translate .add() to .Add()");
    assert!(!cpp.contains(".add("), "Should not contain KAIN .add() in output");
}

#[test]
fn test_array_empty_alias() {
    let source = r#"
fn empty_array(items: Array<String>):
    items.empty()

fn main():
    let arr: Array<String> = ["test"]
    empty_array(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .empty() → .Empty()
    assert!(cpp.contains(".Empty("), "Should translate .empty() to .Empty()");
    assert!(!cpp.contains(".empty("), "Should not contain KAIN .empty() in output");
}

// ============================================================================
// C. ADDITIONAL ARRAY METHOD TESTS
// ============================================================================

#[test]
fn test_array_remove_method() {
    let source = r#"
fn remove_at_index(items: Array<Int>, index: Int):
    items.remove(index)

fn main():
    let arr: Array<Int> = [1, 2, 3]
    remove_at_index(arr, 1)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .remove() → .RemoveAt()
    assert!(cpp.contains(".RemoveAt("), "Should translate .remove() to .RemoveAt()");
    assert!(!cpp.contains(".remove("), "Should not contain KAIN .remove() in output");
}

#[test]
fn test_array_contains_method() {
    let source = r#"
fn has_value(items: Array<Int>, value: Int) -> Bool:
    return items.contains(value)

fn main():
    let arr: Array<Int> = [1, 2, 3]
    let found = has_value(arr, 2)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .contains() → .Contains()
    assert!(cpp.contains(".Contains("), "Should translate .contains() to .Contains()");
    assert!(!cpp.contains(".contains("), "Should not contain KAIN .contains() in output");
}

#[test]
fn test_array_find_method() {
    let source = r#"
fn find_index(items: Array<String>, value: String) -> Int:
    return items.find(value)

fn main():
    let arr: Array<String> = ["a", "b", "c"]
    let idx = find_index(arr, "b")
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .find() → .Find()
    assert!(cpp.contains(".Find("), "Should translate .find() to .Find()");
    assert!(!cpp.contains(".find("), "Should not contain KAIN .find() in output");
}

#[test]
fn test_array_insert_method() {
    let source = r#"
fn insert_at(items: Array<Int>, index: Int, value: Int):
    items.insert(index, value)

fn main():
    let arr: Array<Int> = [1, 3]
    insert_at(arr, 1, 2)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .insert() → .Insert()
    assert!(cpp.contains(".Insert("), "Should translate .insert() to .Insert()");
    assert!(!cpp.contains(".insert("), "Should not contain KAIN .insert() in output");
}

#[test]
fn test_array_sort_method() {
    let source = r#"
fn sort_array(items: Array<Int>):
    items.sort()

fn main():
    let arr: Array<Int> = [3, 1, 2]
    sort_array(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .sort() → .Sort()
    assert!(cpp.contains(".Sort("), "Should translate .sort() to .Sort()");
    assert!(!cpp.contains(".sort("), "Should not contain KAIN .sort() in output");
}

// ============================================================================
// D. COMPLEX USAGE TESTS
// ============================================================================

#[test]
fn test_multiple_array_methods_in_function() {
    let source = r#"
fn process_array(items: Array<Int>, value: Int) -> Int:
    items.push(value)
    let size = items.len()
    items.clear()
    return size

fn main():
    let arr: Array<Int> = [1, 2]
    let result = process_array(arr, 3)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify all methods are translated
    assert!(cpp.contains(".Add("), "Should translate .push() to .Add()");
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num()");
    assert!(cpp.contains(".Empty("), "Should translate .clear() to .Empty()");
    
    // Verify no KAIN methods remain
    assert!(!cpp.contains(".push("), "Should not contain KAIN .push()");
    assert!(!cpp.contains(".len()"), "Should not contain KAIN .len()");
    assert!(!cpp.contains(".clear("), "Should not contain KAIN .clear()");
}

#[test]
fn test_array_methods_in_actor() {
    let source = r#"
actor InventoryManager:
    state items: Array<Int> = []
    
    fn add_item(self, item_id: Int):
        items.push(item_id)
    
    fn get_count(self) -> Int:
        return items.len()
    
    fn clear_inventory(self):
        items.clear()

fn main():
    let manager = InventoryManager()
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify all methods are translated in actor context
    assert!(cpp.contains(".Add("), "Should translate .push() to .Add() in actor");
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num() in actor");
    assert!(cpp.contains(".Empty("), "Should translate .clear() to .Empty() in actor");
}

#[test]
fn test_array_methods_in_component() {
    let source = r#"
@component
struct DataCollector:
    samples: Array<Float>

fn main():
    let collector = DataCollector()
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify component is generated correctly
    assert!(cpp.contains("UDataCollector"), "Should generate UDataCollector component");
    // Note: Methods would need to be in an impl block to be generated
    // This test verifies the component structure is correct
}

#[test]
fn test_chained_array_operations() {
    let source = r#"
fn process(items: Array<Int>) -> Int:
    items.push(1)
    items.push(2)
    items.push(3)
    let count = items.len()
    items.pop()
    return count

fn main():
    let arr: Array<Int> = []
    let result = process(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify all operations are translated
    assert!(cpp.matches(".Add(").count() >= 3, "Should have multiple .Add() calls");
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num()");
    assert!(cpp.contains(".Pop("), "Should translate .pop() to .Pop()");
}

#[test]
fn test_array_method_with_generic_type() {
    let source = r#"
fn get_array_length<T>(items: Array<T>) -> Int:
    return items.len()

fn main():
    let int_arr: Array<Int> = [1, 2, 3]
    let float_arr: Array<Float> = [1.0, 2.0]
    let int_len = get_array_length(int_arr)
    let float_len = get_array_length(float_arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .len() → .Num() works with generic arrays
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num() for generic arrays");
    // Note: Generic functions may be monomorphized to Any type due to type inference limitations
    // This is acceptable as long as the method translation works
}

// ============================================================================
// E. PROPERTY-STYLE LENGTH ACCESS TESTS
// ============================================================================

#[test]
fn test_array_length_property_access() {
    let source = r#"
fn get_size(items: Array<Int>) -> Int:
    return items.length

fn main():
    let arr: Array<Int> = [1, 2, 3]
    let size = get_size(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify property-style .length → .Num()
    assert!(cpp.contains(".Num()"), "Should translate property .length to .Num()");
    assert!(!cpp.contains(".length"), "Should not contain KAIN .length property in output");
}

#[test]
fn test_array_len_property_access() {
    let source = r#"
fn check_empty(items: Array<String>) -> Bool:
    return items.len == 0

fn main():
    let arr: Array<String> = []
    let is_empty = check_empty(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify property-style .len → .Num()
    assert!(cpp.contains(".Num()"), "Should translate property .len to .Num()");
    assert!(!cpp.contains(".len"), "Should not contain KAIN .len property in output");
}

// ============================================================================
// F. EDGE CASE TESTS
// ============================================================================

#[test]
fn test_array_method_on_nested_arrays() {
    let source = r#"
fn get_nested_size(grid: Array<Array<Int>>) -> Int:
    return grid.len()

fn main():
    let nested: Array<Array<Int>> = [[1, 2], [3, 4]]
    let size = get_nested_size(nested)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .len() → .Num() works with nested arrays
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num() for nested arrays");
    assert!(cpp.contains("TArray<TArray<int64>>") || cpp.contains("TArray<TArray<int32>>"),
            "Should have nested TArray type");
}

#[test]
fn test_array_method_in_conditional() {
    let source = r#"
fn is_empty(items: Array<Int>) -> Bool:
    if items.len() == 0:
        return true
    return false

fn main():
    let arr: Array<Int> = []
    let empty = is_empty(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .len() → .Num() in conditional
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num() in conditional");
    assert!(cpp.contains("== 0"), "Should have comparison with 0");
}

#[test]
fn test_array_method_in_loop() {
    let source = r#"
fn sum_array(items: Array<Int>) -> Int:
    let total = 0
    let i = 0
    while i < items.len():
        total = total + items[i]
        i = i + 1
    return total

fn main():
    let arr: Array<Int> = [1, 2, 3]
    let sum = sum_array(arr)
"#;
    
    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;
    
    println!("Generated C++:\n{}", cpp);
    
    // Verify .len() → .Num() in loop condition
    assert!(cpp.contains(".Num()"), "Should translate .len() to .Num() in loop");
    assert!(cpp.contains("while"), "Should have while loop");
}
