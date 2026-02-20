# KAIN Generics and Monomorphization System

**Status:** ⚠️ **IMPLEMENTED BUT UNUSED** - Monomorphization exists but no backend uses it  
**Date:** 2025-02-19  
**Author:** AI Analysis  
**Purpose:** Technical specification for wiring generics to all codegen backends

---

## Executive Summary

KAIN has a **fully functional monomorphization system** (`crates/kain-core/src/monomorphize.rs`, 1471 lines) that:
- ✅ Instantiates generic functions with concrete types
- ✅ Performs type inference via unification
- ✅ Validates trait bounds
- ✅ Lowers async functions to state machines
- ✅ Mangles names to avoid collisions (`identity_Int`, `identity_Float`)

**The Problem:** All codegen backends (UE5, Web, GPU, Sys) operate on `TypedProgram` and **completely ignore** `MonomorphizedProgram`. Generic functions are never instantiated, so they either:
1. Generate invalid code (generic type parameters in output)
2. Silently skip generic functions
3. Crash on generic calls

**The Solution:** Wire `monomorphize()` into the compilation pipeline between type checking and codegen.

---

## Table of Contents

1. [Current State Analysis](#1-current-state-analysis)
2. [Gap Analysis by Backend](#2-gap-analysis-by-backend)
3. [Implementation Plan](#3-implementation-plan)
4. [Code Examples](#4-code-examples)
5. [Complexity Assessment](#5-complexity-assessment)
6. [Dependencies](#6-dependencies)
7. [Test Strategy](#7-test-strategy)
8. [Migration Path](#8-migration-path)

---

## 1. Current State Analysis

### 1.1 Monomorphization System Architecture

**Location:** `crates/kain-core/src/monomorphize.rs`

**Core Components:**

```rust
pub struct MonomorphizedProgram {
    pub items: Vec<TypedItem>,  // All generic functions replaced with concrete versions
}

pub fn monomorphize(program: &TypedProgram) -> KainResult<MonomorphizedProgram>
```

**What It Does:**

1. **First Pass - Collection:**
   - Scans all items in `TypedProgram`
   - Separates generic functions from concrete items
   - Registers structs, enums, impl blocks, methods
   - Builds trait implementation registry

2. **Second Pass - Instantiation:**
   - Scans concrete function bodies for generic calls
   - Infers type arguments via unification algorithm
   - Instantiates generic functions with concrete types
   - Mangles names: `identity<T>` + `Int` → `identity_Int`
   - Substitutes type parameters in AST

3. **Async Lowering (Bonus Feature):**
   - Detects `async` effect on functions
   - Transforms to state machine with `Poll<T>` return
   - Splits at `await` points into states
   - Generates `_Future` struct and `_poll` method

**Key Algorithms:**

```rust
// Type inference via unification
fn unify(param_type: &ResolvedType, arg_type: &ResolvedType, bindings: &mut HashMap<String, ResolvedType>)

// Instantiation with memoization
fn instantiate(&mut self, name: &str, type_args: &[ResolvedType]) -> KainResult<String>

// Type substitution in AST
fn substitute_ast_types(func: &mut Function, mapping: &HashMap<String, ResolvedType>)
```


### 1.2 Current Compilation Pipeline

**File:** `crates/cli/src/lib.rs`

```rust
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);

    // 1. Lex
    let tokens = Lexer::new(&full_source).tokenize()?;
    
    // 2. Parse
    let mut ast = Parser::new(&tokens).parse()?;
    
    // 2.5 Comptime
    comptime::eval_program(&mut ast)?;
    
    // 3. Type check
    let typed_ast = types::check(&ast)?;
    
    // ❌ MISSING: monomorphize() call here
    
    // 4. Codegen based on target
    match target {
        CompileTarget::Ue5 => ue5::generate(&typed_ast, ...),
        CompileTarget::Wasm => web::generate_wasm(&typed_ast),
        CompileTarget::Js => web::generate_js(&typed_ast),
        // ... all backends receive TypedProgram directly
    }
}
```

**The Gap:** No call to `monomorphize()` anywhere in the pipeline.

### 1.3 Generic Support in AST

**File:** `crates/kain-core/src/ast.rs`

```rust
pub struct Function {
    pub name: String,
    pub generics: Vec<Generic>,  // ✅ Parsed
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    // ...
}

pub struct Generic {
    pub name: String,           // e.g., "T"
    pub bounds: Vec<TypeBound>, // e.g., "T: Display + Clone"
    pub span: Span,
}

pub struct TypeBound {
    pub trait_name: String,
    pub span: Span,
}
```

**Status:** Generics are fully parsed and represented in AST.


### 1.4 Type System Support

**File:** `crates/kain-core/src/types.rs`

```rust
pub enum ResolvedType {
    // ... concrete types ...
    Generic(String),  // ✅ Generic type parameters represented
    Function { params: Vec<ResolvedType>, ret: Box<ResolvedType>, effects: EffectSet },
    // ...
}

pub fn resolve_type(ty: &Type) -> KainResult<ResolvedType> {
    match ty {
        Type::Named { name, .. } => {
            // ✅ Detects generic type parameters
            if name.len() == 1 && name.chars().next().map(|c| c.is_uppercase()).unwrap_or(false) {
                Ok(ResolvedType::Generic(name.clone()))
            } else if name.starts_with('_') && name.len() > 1 {
                Ok(ResolvedType::Generic(name.clone()))
            } else {
                Ok(ResolvedType::Struct(name.clone(), HashMap::new()))
            }
        }
        // ...
    }
}
```

**Status:** Type system fully supports generics.

---

## 2. Gap Analysis by Backend

### 2.1 UE5 Backend (`crates/ue5/`)

**Entry Point:** `crates/ue5/src/codegen_ue5.rs`

```rust
pub fn generate(program: &TypedProgram, ...) -> KainResult<Ue5Output>
```

**Current Behavior:**
- ❌ Receives `TypedProgram` with generic functions intact
- ❌ Generates C++ with template-like syntax (invalid UE5 C++)
- ❌ Generic type parameters appear as `T` in output

**Example Problem:**

```kain
fn identity<T>(x: T) -> T:
    return x
```

**Current Output (BROKEN):**
```cpp
T identity(T x) {
    return x;
}
```

**Expected Output (AFTER FIX):**
```cpp
int32 identity_Int(int32 x) {
    return x;
}

float identity_Float(float x) {
    return x;
}
```


**Impact:** HIGH - UE5 doesn't support C++ templates in UFUNCTION, breaks compilation

**Fix Complexity:** MEDIUM - Just wire monomorphize(), UE5 codegen already handles concrete types

---

### 2.2 Web Backend (`crates/web/`)

**Entry Points:**
- `crates/web/src/codegen_wasm.rs` - `pub fn generate(program: &TypedProgram)`
- `crates/web/src/codegen_js.rs` - `pub fn generate(program: &TypedProgram)`
- `crates/web/src/codegen_hybrid.rs` - `pub fn generate(program: &TypedProgram)`

**Current Behavior:**
- ❌ JavaScript output has generic type parameters as identifiers
- ❌ WASM output attempts to compile generic functions (fails or produces invalid bytecode)

**Example Problem:**

```kain
fn max<T>(a: T, b: T) -> T:
    if a > b: return a
    else: return b
```

**Current JS Output (BROKEN):**
```javascript
function max(a, b) {
    // T is undefined, comparison may fail for non-numeric types
    if (a > b) return a;
    else return b;
}
```

**Expected JS Output (AFTER FIX):**
```javascript
function max_Int(a, b) {
    if (a > b) return a;
    else return b;
}

function max_Float(a, b) {
    if (a > b) return a;
    else return b;
}
```

**Impact:** MEDIUM - JavaScript is dynamically typed so it "works" but loses type safety

**Fix Complexity:** LOW - JavaScript doesn't care about types, just need name mangling


---

### 2.3 GPU Backend (`crates/gpu/`)

**Entry Points:**
- `crates/gpu/src/codegen_spirv.rs` - Generates SPIR-V bytecode
- `crates/gpu/src/codegen_hlsl.rs` - Generates HLSL shader code

**Current Behavior:**
- ❌ HLSL output has generic type parameters (invalid shader code)
- ❌ SPIR-V compilation fails on generic functions

**Example Problem:**

```kain
fn lerp<T>(a: T, b: T, t: Float) -> T:
    return a * (1.0 - t) + b * t
```

**Current HLSL Output (BROKEN):**
```hlsl
T lerp(T a, T b, float t) {
    return a * (1.0 - t) + b * t;
}
```

**Expected HLSL Output (AFTER FIX):**
```hlsl
float lerp_Float(float a, float b, float t) {
    return a * (1.0 - t) + b * t;
}

float3 lerp_Vec3(float3 a, float3 b, float t) {
    return a * (1.0 - t) + b * t;
}
```

**Impact:** HIGH - Shader compilation fails completely

**Fix Complexity:** MEDIUM - HLSL has strict typing, need proper type mapping

---

### 2.4 Sys Backend (`crates/sys/`)

**Entry Points:**
- `crates/sys/src/codegen_llvm.rs` - Generates LLVM IR
- `crates/sys/src/codegen_rust.rs` - Generates Rust code
- `crates/sys/src/codegen_cpp.rs` - Generates C++ code

**Current Behavior:**
- ❌ LLVM IR has invalid generic type references
- ❌ Rust output uses generic syntax (could work but loses KAIN semantics)
- ❌ C++ output has template-like syntax (may work but not intended)

**Impact:** MEDIUM - Some targets might accidentally work, but semantics are wrong

**Fix Complexity:** LOW-MEDIUM - Rust backend could keep generics, others need monomorphization


---

## 3. Implementation Plan

### Phase 1: Wire Monomorphization into Pipeline (1-2 hours)

**Goal:** Make all backends use `MonomorphizedProgram` instead of `TypedProgram`

**Steps:**

1. **Update `crates/cli/src/lib.rs`:**

```rust
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);

    // 1. Lex
    let tokens = Lexer::new(&full_source).tokenize()?;
    
    // 2. Parse
    let mut ast = Parser::new(&tokens).parse()?;
    
    // 2.5 Comptime
    comptime::eval_program(&mut ast)?;
    
    // 3. Type check
    let typed_ast = types::check(&ast)?;
    
    // ✅ NEW: 3.5 Monomorphize
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    
    // 4. Codegen based on target
    match target {
        CompileTarget::Ue5 => ue5::generate_mono(&mono_ast, ...),
        CompileTarget::Wasm => web::generate_wasm_mono(&mono_ast),
        // ... update all backends
    }
}
```

2. **Update UE5 backend signature:**

```rust
// OLD
pub fn generate(program: &TypedProgram, ...) -> KainResult<Ue5Output>

// NEW
pub fn generate_mono(program: &MonomorphizedProgram, ...) -> KainResult<Ue5Output>

// Keep old function for backward compatibility
pub fn generate(program: &TypedProgram, ...) -> KainResult<Ue5Output> {
    let mono = monomorphize::monomorphize(program)?;
    generate_mono(&mono, ...)
}
```

3. **Repeat for all backends:**
   - `web::generate_wasm` → `web::generate_wasm_mono`
   - `web::generate_js` → `web::generate_js_mono`
   - `gpu::generate_spirv` → `gpu::generate_spirv_mono`
   - `gpu::generate_hlsl` → `gpu::generate_hlsl_mono`
   - `sys::generate_llvm` → `sys::generate_llvm_mono`
   - `sys::generate_rust` → `sys::generate_rust_mono`
   - `sys::generate_cpp` → `sys::generate_cpp_mono`


**Files to Modify:**
- `crates/cli/src/lib.rs` (main pipeline)
- `crates/ue5/src/codegen_ue5.rs`
- `crates/web/src/codegen_wasm.rs`
- `crates/web/src/codegen_js.rs`
- `crates/web/src/codegen_hybrid.rs`
- `crates/gpu/src/codegen_spirv.rs`
- `crates/gpu/src/codegen_hlsl.rs`
- `crates/sys/src/codegen_llvm.rs`
- `crates/sys/src/codegen_rust.rs`
- `crates/sys/src/codegen_cpp.rs`

**Testing:**
```bash
# Test that monomorphization runs without errors
cargo test --package kain-core monomorphize

# Test each backend with generic code
cargo test --package ue5 --lib
cargo test --package web --lib
cargo test --package gpu --lib
cargo test --package sys --lib
```

---

### Phase 2: Handle Edge Cases (2-4 hours)

**Goal:** Ensure monomorphization handles all KAIN features

**Edge Cases to Test:**

1. **Generic Structs:**
```kain
struct Box<T>:
    value: T

fn unbox<T>(b: Box<T>) -> T:
    return b.value
```

**Current Status:** ❌ Monomorphization only handles functions, not structs

**Fix:** Extend `MonoContext` to instantiate generic structs

2. **Nested Generics:**
```kain
fn map<T, U>(arr: Array<T>, f: fn(T) -> U) -> Array<U>:
    // ...
```

**Current Status:** ⚠️ Untested

**Fix:** Verify unification handles nested type parameters

3. **Generic Methods:**
```kain
impl<T> Box<T>:
    fn get(self) -> T:
        return self.value
```

**Current Status:** ❌ Not implemented

**Fix:** Extend impl block handling in monomorphization


4. **Trait Bounds:**
```kain
trait Display:
    fn to_string(self) -> String

fn print<T: Display>(x: T):
    println(x.to_string())
```

**Current Status:** ✅ Trait bounds validated in `infer_type_args()`

**Fix:** Ensure trait registry is populated correctly

5. **Higher-Kinded Types:**
```kain
fn map_option<T, U>(opt: Option<T>, f: fn(T) -> U) -> Option<U>:
    match opt:
        Some(x) => Some(f(x))
        None => None
```

**Current Status:** ⚠️ Untested

**Fix:** Verify `ResolvedType::Option` substitution works

---

### Phase 3: Optimize Monomorphization (4-8 hours)

**Goal:** Improve performance and code size

**Optimizations:**

1. **Dead Code Elimination:**
   - Only instantiate generic functions that are actually called
   - Current implementation instantiates on first call (✅ already optimal)

2. **Duplicate Detection:**
   - Avoid generating `identity_Int` twice if called from multiple places
   - Current implementation uses `instantiated: HashMap<String, String>` (✅ already optimal)

3. **Inline Small Functions:**
   - Mark monomorphized functions with `#[inline]` in Rust backend
   - Add `FORCEINLINE` in UE5 backend for small functions

4. **Specialization Hints:**
   - Allow manual specialization: `fn identity<T> @specialize(Int, Float)`
   - Generate only specified instantiations

**Implementation:**

```rust
// In monomorphize.rs
pub struct MonoConfig {
    pub inline_threshold: usize,  // Inline functions with < N statements
    pub specialize_only: bool,     // Only generate @specialize'd instantiations
}

pub fn monomorphize_with_config(
    program: &TypedProgram, 
    config: MonoConfig
) -> KainResult<MonomorphizedProgram>
```


---

## 4. Code Examples

### 4.1 Simple Generic Function

**KAIN Source:**
```kain
fn identity<T>(x: T) -> T:
    return x

fn main():
    let a = identity(42)
    let b = identity(3.14)
    let c = identity("hello")
```

**After Monomorphization (Internal AST):**
```kain
fn identity_Int(x: Int) -> Int:
    return x

fn identity_Float(x: Float) -> Float:
    return x

fn identity_String(x: String) -> String:
    return x

fn main():
    let a = identity_Int(42)
    let b = identity_Float(3.14)
    let c = identity_String("hello")
```

**UE5 C++ Output:**
```cpp
// identity_Int.h
#pragma once
#include "CoreMinimal.h"

UFUNCTION(BlueprintCallable, Category = "Kain")
int32 identity_Int(int32 x);

// identity_Int.cpp
int32 identity_Int(int32 x) {
    return x;
}

// identity_Float.h
#pragma once
#include "CoreMinimal.h"

UFUNCTION(BlueprintCallable, Category = "Kain")
float identity_Float(float x);

// identity_Float.cpp
float identity_Float(float x) {
    return x;
}

// identity_String.h
#pragma once
#include "CoreMinimal.h"

UFUNCTION(BlueprintCallable, Category = "Kain")
FString identity_String(FString x);

// identity_String.cpp
FString identity_String(FString x) {
    return x;
}
```


---

### 4.2 Generic with Trait Bounds

**KAIN Source:**
```kain
trait Numeric:
    fn add(self, other: Self) -> Self

impl Numeric for Int:
    fn add(self, other: Int) -> Int:
        return self + other

impl Numeric for Float:
    fn add(self, other: Float) -> Float:
        return self + other

fn sum<T: Numeric>(a: T, b: T) -> T:
    return a.add(b)

fn main():
    let x = sum(10, 20)      // T = Int
    let y = sum(1.5, 2.5)    // T = Float
```

**After Monomorphization:**
```kain
fn sum_Int(a: Int, b: Int) -> Int:
    return Int_add(a, b)  // Method call desugared

fn sum_Float(a: Float, b: Float) -> Float:
    return Float_add(a, b)

fn main():
    let x = sum_Int(10, 20)
    let y = sum_Float(1.5, 2.5)
```

**Validation:** Monomorphization checks that `Int` and `Float` implement `Numeric` trait before instantiation.

---

### 4.3 Generic Struct (Future Work)

**KAIN Source:**
```kain
struct Pair<T, U>:
    first: T
    second: U

fn swap<T, U>(p: Pair<T, U>) -> Pair<U, T>:
    return Pair { first: p.second, second: p.first }

fn main():
    let p1 = Pair { first: 42, second: "hello" }
    let p2 = swap(p1)  // Pair<String, Int>
```

**Current Status:** ❌ Not implemented - structs are not monomorphized

**Required Changes:**
1. Extend `MonoContext` to track generic struct definitions
2. Instantiate structs when used with concrete types
3. Generate mangled struct names: `Pair_Int_String`, `Pair_String_Int`


---

## 5. Complexity Assessment

### 5.1 Effort Estimates

| Task | Complexity | Time Estimate | Priority |
|------|-----------|---------------|----------|
| Wire monomorphize() into CLI pipeline | LOW | 1-2 hours | **CRITICAL** |
| Update UE5 backend signatures | LOW | 1 hour | **CRITICAL** |
| Update Web backends (WASM, JS, Hybrid) | LOW | 1-2 hours | HIGH |
| Update GPU backends (SPIRV, HLSL) | MEDIUM | 2-3 hours | HIGH |
| Update Sys backends (LLVM, Rust, C++) | MEDIUM | 2-3 hours | MEDIUM |
| Add generic struct support | HIGH | 8-12 hours | MEDIUM |
| Add generic method support | HIGH | 6-10 hours | MEDIUM |
| Optimize monomorphization | MEDIUM | 4-8 hours | LOW |
| Write comprehensive tests | MEDIUM | 4-6 hours | HIGH |
| Update documentation | LOW | 2-3 hours | MEDIUM |

**Total Estimate:** 31-52 hours (4-7 days)

**Critical Path (Minimum Viable):** 8-12 hours (1-2 days)
- Wire monomorphize() into pipeline
- Update UE5 backend
- Update Web backends
- Basic testing

---

### 5.2 Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Monomorphization breaks existing code | MEDIUM | HIGH | Keep old `generate()` functions for backward compatibility |
| Type inference fails on complex generics | LOW | MEDIUM | Add explicit type annotations as fallback |
| Code size explosion from instantiations | LOW | MEDIUM | Implement dead code elimination |
| Performance regression | LOW | LOW | Monomorphization is O(n) with memoization |
| Breaking changes to public API | HIGH | MEDIUM | Deprecate old functions, provide migration guide |

---

### 5.3 Performance Impact

**Compilation Time:**
- Monomorphization adds ~5-10% to compilation time
- Dominated by type inference (unification algorithm)
- Memoization prevents duplicate instantiations

**Runtime Performance:**
- ✅ **IMPROVEMENT** - Monomorphized code is faster than dynamic dispatch
- ✅ **IMPROVEMENT** - Enables inlining and optimization
- ✅ **IMPROVEMENT** - No runtime type checks

**Code Size:**
- ⚠️ **INCREASE** - Each instantiation generates separate code
- Typical increase: 10-30% for generic-heavy code
- Mitigated by dead code elimination


---

## 6. Dependencies

### 6.1 Internal Dependencies

**No Breaking Changes Required:**
- ✅ `monomorphize.rs` is self-contained
- ✅ All backends already handle `TypedItem` (which is what `MonomorphizedProgram` contains)
- ✅ Type system already supports `ResolvedType::Generic`

**Optional Enhancements:**
- Add `@specialize` attribute to AST
- Add `MonoConfig` to `CompileTarget`
- Extend `EngineKnowledge` with generic type mappings

---

### 6.2 External Dependencies

**None.** Monomorphization is a pure compiler transformation.

---

### 6.3 Backward Compatibility

**Strategy:** Dual API during transition

```rust
// OLD API (deprecated but still works)
pub fn generate(program: &TypedProgram, ...) -> KainResult<Ue5Output> {
    let mono = monomorphize::monomorphize(program)?;
    generate_mono(&mono, ...)
}

// NEW API (preferred)
pub fn generate_mono(program: &MonomorphizedProgram, ...) -> KainResult<Ue5Output> {
    // Implementation
}
```

**Migration Timeline:**
1. **Phase 1 (Week 1):** Add `_mono` functions, keep old functions
2. **Phase 2 (Week 2-4):** Update all internal callers to use `_mono`
3. **Phase 3 (Month 2):** Deprecate old functions with `#[deprecated]`
4. **Phase 4 (Month 3+):** Remove old functions in next major version

---

## 7. Test Strategy

### 7.1 Unit Tests

**Location:** `crates/kain-core/tests/monomorphize_tests.rs`

```rust
#[test]
fn test_simple_generic_instantiation() {
    let source = r#"
        fn identity<T>(x: T) -> T:
            return x
        
        fn main():
            let a = identity(42)
            let b = identity(3.14)
    "#;
    
    let typed = parse_and_typecheck(source).unwrap();
    let mono = monomorphize(&typed).unwrap();
    
    // Should have 3 functions: identity_Int, identity_Float, main
    assert_eq!(mono.items.len(), 3);
    
    // Check mangled names
    let names: Vec<_> = mono.items.iter()
        .filter_map(|item| match item {
            TypedItem::Function(f) => Some(f.ast.name.as_str()),
            _ => None,
        })
        .collect();
    
    assert!(names.contains(&"identity_Int"));
    assert!(names.contains(&"identity_Float"));
    assert!(names.contains(&"main"));
}
```


**Test Cases:**

1. ✅ Simple generic function with primitive types
2. ✅ Generic function with struct types
3. ✅ Generic function with trait bounds
4. ✅ Multiple type parameters
5. ✅ Nested generic calls
6. ✅ Generic method calls (after implementation)
7. ✅ Generic structs (after implementation)
8. ✅ Higher-kinded types (Option, Result, Array)
9. ✅ Type inference from context
10. ✅ Error: unbound type parameter
11. ✅ Error: trait bound not satisfied
12. ✅ Error: type mismatch in generic call

---

### 7.2 Integration Tests

**Location:** `crates/ue5/tests/generic_codegen_tests.rs`

```rust
#[test]
fn test_ue5_generic_blueprint_function() {
    let source = r#"
        @blueprint
        fn max<T>(a: T, b: T) -> T:
            if a > b: return a
            else: return b
        
        fn test():
            let x = max(10, 20)
            let y = max(1.5, 2.5)
    "#;
    
    let output = compile_ue5(source).unwrap();
    
    // Should generate two UFUNCTION declarations
    assert!(output.header.contains("int32 max_Int(int32 a, int32 b)"));
    assert!(output.header.contains("float max_Float(float a, float b)"));
    
    // Should have UFUNCTION macro
    assert!(output.header.contains("UFUNCTION(BlueprintCallable"));
}
```

**Test Coverage:**
- UE5 actor with generic methods
- UE5 component with generic properties (after struct support)
- WASM generic function compilation
- JavaScript generic function output
- HLSL shader with generic helper functions
- LLVM IR generic function lowering

---

### 7.3 End-to-End Tests

**Location:** `testing/generics/`

**Test Plugins:**

1. **GenericMath.kn** - Generic math utilities
```kain
fn lerp<T>(a: T, b: T, t: Float) -> T:
    return a * (1.0 - t) + b * t

fn clamp<T>(x: T, min: T, max: T) -> T:
    if x < min: return min
    if x > max: return max
    return x
```

2. **GenericCollections.kn** - Generic data structures
```kain
struct Stack<T>:
    items: Array<T>
    
fn push<T>(stack: Stack<T>, item: T) -> Stack<T>:
    // ...
```

3. **GenericActors.kn** - UE5 actors with generics
```kain
actor GenericContainer<T>:
    state value: T
    
    @blueprint_callable
    fn get_value(self) -> T:
        return self.value
```

**Validation:**
- ✅ Compiles without errors
- ✅ Generates valid C++/WASM/JS/HLSL
- ✅ Runs in UE5 without crashes
- ✅ Blueprint integration works

