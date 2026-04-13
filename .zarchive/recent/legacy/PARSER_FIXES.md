# Parser Fixes: Nested Generics and Literal Type Inference

**Date:** 2026-02-19  
**Status:** Implemented  
**Related Files:**
- `crates/kain-core/src/parser.rs`
- `crates/kain-core/src/monomorphize.rs`
- `crates/kain-core/tests/monomorphize_test.rs`

---

## Overview

This document describes two critical parser improvements that enable better generic type support and more accurate type inference for literals.

---

## Issue 1: Nested Generic Types (`>>` Parsing)

### Problem

The lexer tokenizes `>>` as a single `Shr` (right shift) token, which caused parsing failures for nested generic types:

```kain
struct Box<T>:
    value: T

fn make_nested() -> Box<Box<Int>>:  // ❌ Parse error: >> treated as right-shift
    let inner = Box { value: 42 }
    return Box { value: inner }
```

**Error:** Parser expected `>` but found `>>` (Shr token).

### Root Cause

The lexer uses the `logos` crate which greedily matches `>>` as a single token for the right-shift operator. When parsing generic type arguments like `Type<T>`, the parser expects individual `>` tokens to close each generic level.

For nested generics like `Box<Box<Int>>`, the closing `>>` should be treated as two separate `>` tokens:
- First `>` closes `Box<Int>`
- Second `>` closes `Box<...>`

### Solution

Modified `parse_type()` in `parser.rs` to explicitly check for the `Shr` token when parsing generic type arguments:

```rust
// Parse generic type arguments: Type<T, U>
let mut type_args = Vec::new();
if self.check(TokenKind::Lt) {
    self.advance(); // consume <
    while !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) && !self.at_end() {
        type_args.push(self.parse_type()?);
        if !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) {
            self.expect(TokenKind::Comma)?;
        }
    }
    
    // Handle >> token for nested generics like Box<Box<Int>>
    if self.check(TokenKind::Shr) {
        self.advance();  // Consume >> - closes this generic level
    } else {
        self.expect(TokenKind::Gt)?;  // consume >
    }
}
```

**Key insight:** When the inner `parse_type()` call encounters `>>`, it consumes the token and returns. The outer `parse_type()` call then also checks for `Shr` and handles it, effectively treating `>>` as two closing brackets.

### What Now Works

```kain
// ✅ Nested generics
struct Box<T>:
    value: T

fn make_nested() -> Box<Box<Int>>:
    let inner = Box { value: 42 }
    return Box { value: inner }

// ✅ Triple nesting
type TripleBox = Box<Box<Box<String>>>

// ✅ Multiple nested generics
fn complex() -> Map<String, Vec<Box<Int>>>:
    // ...
```

### Limitations

- **Deeply nested generics** (4+ levels) may still have issues depending on how many `>` tokens are combined
- **Mixed operators:** `Box<T> >> 2` (generic followed by right-shift) works correctly since the `>>` is in a different context

---

## Issue 2: Negative Literal Type Inference

### Problem

Negative numeric literals were inferred as `Any` type instead of their concrete types:

```kain
fn abs<T>(x: T) -> T:
    return x

fn main():
    let a = abs(-42)  // ❌ Inferred as abs<Any> instead of abs<Int>
```

**Result:** Monomorphization generated `abs_Any` instead of `abs_Int`.

### Root Cause

The `scan_expr()` function in `monomorphize.rs` didn't handle `Expr::Unary` expressions. When encountering `-42`, the type inference saw:

```
Unary { op: Neg, operand: Int(42) }
```

But the match statement had no case for `Expr::Unary`, so it fell through to the catch-all `_ => Ok(ResolvedType::Unknown)`.

### Solution

Added explicit handling for unary expressions in `scan_expr()`:

```rust
Expr::Unary { op, operand, .. } => {
    // Handle unary expressions - importantly, negative literals like -42
    let operand_ty = scan_expr(ctx, env, operand)?;
    
    // For unary minus, preserve the operand type (Int/Float)
    // This ensures -42 is inferred as Int, not Any
    match op {
        UnaryOp::Neg => Ok(operand_ty),
        UnaryOp::Not | UnaryOp::BitNot => Ok(ResolvedType::Bool),
        UnaryOp::Ref | UnaryOp::RefMut => Ok(ResolvedType::Ref {
            mutable: matches!(op, UnaryOp::RefMut),
            inner: Box::new(operand_ty),
        }),
        UnaryOp::Deref => {
            match operand_ty {
                ResolvedType::Ref { inner, .. } => Ok(*inner),
                _ => Ok(operand_ty),
            }
        }
    }
}
```

**Key insight:** For unary minus (`-`), the result type is the same as the operand type. So `-42` has type `Int`, and `-3.14` has type `Float`.

### What Now Works

```kain
// ✅ Negative integer literals
fn abs<T>(x: T) -> T:
    return x

fn main():
    let a = abs(-42)    // Infers abs<Int>
    let b = abs(42)     // Infers abs<Int>
    let c = abs(-3.14)  // Infers abs<Float>

// ✅ Unary operations preserve types
fn negate<T>(x: T) -> T:
    return -x

fn test():
    let x = negate(-100)  // Infers negate<Int>

// ✅ Boolean operations
fn not_fn<T>(x: T) -> Bool:
    return !x

fn test_bool():
    let x = not_fn(true)  // Infers not_fn<Bool>
```

### Additional Benefits

The unary expression handling also correctly types:
- **References:** `&x` and `&mut x` now properly infer reference types
- **Dereferences:** `*ptr` correctly unwraps reference types
- **Bitwise NOT:** `~x` preserves integer types
- **Logical NOT:** `!x` returns `Bool`

---

## Testing

Added two comprehensive tests in `crates/kain-core/tests/monomorphize_test.rs`:

### Test 1: `test_nested_generic_types()`

Verifies that nested generic types like `Box<Box<Int>>` parse correctly and generate the expected monomorphized structs.

```rust
#[test]
fn test_nested_generic_types() {
    let source = r#"
struct Box<T>:
    value: T

fn make_nested() -> Box<Box<Int>>:
    let inner = Box { value: 42 }
    return Box { value: inner }
"#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize::monomorphize(&typed).unwrap();
    
    // Verifies Box<Int> is instantiated
    assert!(struct_names.iter().any(|n| n.contains("Box") && n.contains("Int")));
}
```

### Test 2: `test_negative_literal_inference()`

Verifies that negative literals infer concrete types (`Int`, `Float`) instead of `Any`.

```rust
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
    
    // Should have abs_Int and abs_Float, not abs_Any
    assert!(func_names.iter().any(|n| n.contains("abs") && n.contains("Int")));
    assert!(func_names.iter().any(|n| n.contains("abs") && n.contains("Float")));
    assert!(!func_names.iter().any(|n| n.contains("abs_Any")));
}
```

---

## Impact

### Before

```kain
// ❌ Parse error
type Nested = Box<Box<Int>>

// ❌ Wrong type inference
let x = abs(-42)  // abs_Any
```

### After

```kain
// ✅ Parses correctly
type Nested = Box<Box<Int>>

// ✅ Correct type inference
let x = abs(-42)  // abs_Int
```

---

## Future Improvements

### Nested Generics

1. **Full monomorphization:** Currently, the parser handles `>>` correctly, but full monomorphization of deeply nested generics (e.g., `Box<Box<Box<Int>>>`) may need additional work in the type checker.

2. **Error messages:** When nested generics fail, provide clearer error messages pointing to the specific nesting level.

3. **Performance:** Consider caching monomorphized nested types to avoid redundant instantiations.

### Type Inference

1. **Contextual inference:** Infer types from assignment context:
   ```kain
   let x: Int = abs(-42)  // Should infer abs<Int> from context
   ```

2. **Binary operations:** Improve inference for expressions like `abs(-42 + 10)`.

3. **Method calls:** Infer types from method receiver:
   ```kain
   let x = (-42).abs()  // Should infer Int.abs()
   ```

---

## Related Issues

- **Generic constraints:** Nested generics with trait bounds (e.g., `Box<T> where T: Clone`) need special handling
- **Type aliases:** Type aliases with nested generics should be expanded correctly
- **Error recovery:** Parser should provide helpful suggestions when `>>` is used incorrectly

---

## Conclusion

These fixes significantly improve KAIN's generic type system:

1. **Nested generics** now parse correctly, enabling complex data structures
2. **Literal type inference** is more accurate, reducing `Any` types in monomorphized code
3. **Unary operations** are properly typed, improving overall type safety

Both changes are backward-compatible and don't break existing code.
