# UE5 Materials Crate - Pre-Existing Compilation Errors

## Status

The `ue5-materials` crate has pre-existing compilation errors that are **unrelated to the error message quality improvements**. These errors existed before the error improvements were implemented and need to be fixed separately.

## Errors Found

### 1. AST Type Enum Mismatches (`ast_converter.rs`)

**Location:** `Kain/crates/ue5-materials/src/ast_converter.rs` lines 35-39

**Problem:** The code uses old field names that don't match the current AST definition.

**Current AST Structure:**
```rust
pub enum Type {
    Array(Box<Type>, usize, Span),           // Not Array { element, .. }
    Option(Box<Type>, Span),                 // Not Option { inner, .. }
    Result(Box<Type>, Box<Type>, Span),      // Not Result { ok, err, .. }
    Tuple(Vec<Type>, Span),                  // Not Tuple { elements, .. }
    Function {                               // return_type, not ret
        params: Vec<Type>,
        return_type: Box<Type>,              // ← Correct field name
        effects: Vec<Effect>,
        span: Span,
    },
}
```

**Fixes Needed:**
```rust
// Line 35 - BEFORE:
Type::Array { element, .. } => format!("Array<{}>", ...)
// AFTER:
Type::Array(element, _, _) => format!("Array<{}>", ...)

// Line 36 - BEFORE:
Type::Option { inner, .. } => format!("Option<{}>", ...)
// AFTER:
Type::Option(inner, _) => format!("Option<{}>", ...)

// Line 37 - BEFORE:
Type::Result { ok, err, .. } => format!("Result<{}, {}>", ...)
// AFTER:
Type::Result(ok, err, _) => format!("Result<{}, {}>", ...)

// Line 38 - BEFORE:
Type::Tuple { elements, .. } => format!("({})", ...)
// AFTER:
Type::Tuple(elements, _) => format!("({})", ...)

// Line 39 - BEFORE:
Type::Function { params, ret, .. } => format!("fn({}) -> {}", ...)
// AFTER:
Type::Function { params, return_type, .. } => format!("fn({}) -> {}", ...)
```

### 2. BinaryOp Enum Variant Name Mismatches (`ast_converter.rs`)

**Location:** `Kain/crates/ue5-materials/src/ast_converter.rs` lines 187, 190-191, 195

**Problem:** The code uses old variant names that don't exist in the current AST.

**Current AST Variants:**
```rust
pub enum BinaryOp {
    Ne,    // Not NotEq
    Le,    // Not LtEq
    Ge,    // Not GtEq
    Pow,   // Not Power
}
```

**Fixes Needed:**
```rust
// Line 187 - BEFORE:
BinaryOp::NotEq => "!=",
// AFTER:
BinaryOp::Ne => "!=",

// Line 190 - BEFORE:
BinaryOp::LtEq => "<=",
// AFTER:
BinaryOp::Le => "<=",

// Line 191 - BEFORE:
BinaryOp::GtEq => ">=",
// AFTER:
BinaryOp::Ge => ">=",

// Line 195 - BEFORE:
BinaryOp::Power => "**",
// AFTER:
BinaryOp::Pow => "**",
```

### 3. MaterialInputType Missing Variant (`material_graph.rs`)

**Location:** `Kain/crates/ue5-materials/src/material_graph.rs` line 312

**Problem:** Code references `MaterialInputType::TextureCube` which doesn't exist in the enum.

**Fix Needed:** Either add the `TextureCube` variant to the enum or remove the reference.

### 4. MaterialNodeType Missing Variant (`material_function_builder.rs`)

**Location:** `Kain/crates/ue5-materials/src/material_function_builder.rs` line 804

**Problem:** Code references `MaterialNodeType::CallShader` which doesn't exist in the enum.

**Fix Needed:** Either add the `CallShader` variant to the enum or remove the reference.

## Impact on Error Message Improvements

**NONE** - These errors are completely separate from the error message quality improvements implemented in:
- `Kain/crates/kain-core/src/effects.rs`
- `Kain/crates/ue5-shaders/src/codegen_usf.rs`
- `Kain/crates/ue5-shaders/src/validation.rs`

The error improvements compile successfully when building just those crates:
```bash
cargo build --release -p ue5-shaders -p kain-core  # ✅ SUCCESS
```

## Recommended Action

Fix these pre-existing bugs in the `ue5-materials` crate separately from the error message improvements. The materials crate appears to have fallen out of sync with AST changes.

## Quick Fix Commands

```bash
# Fix ast_converter.rs Type enum mismatches
# Fix ast_converter.rs BinaryOp variant names
# Fix material_graph.rs TextureCube reference
# Fix material_function_builder.rs CallShader reference
```

These are straightforward find-replace fixes that should take ~5 minutes to implement.
