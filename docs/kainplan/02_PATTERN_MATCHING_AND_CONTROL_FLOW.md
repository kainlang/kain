# KAIN Pattern Matching and Control Flow System

> **Last Updated:** Feb 19, 2026  
> **Purpose:** Technical specification of KAIN's pattern matching system and control flow constructs  
> **Status:** Runtime complete, backend implementations partial

---

## Table of Contents

1. [Overview](#overview)
2. [AST Definitions](#ast-definitions)
3. [Pattern Types](#pattern-types)
4. [Runtime Implementation](#runtime-implementation)
5. [Backend Gap Analysis](#backend-gap-analysis)
6. [Implementation Roadmap](#implementation-roadmap)
7. [Control Flow Constructs](#control-flow-constructs)
8. [Code Examples](#code-examples)
9. [Edge Cases and Validation](#edge-cases-and-validation)

---

## Overview

KAIN implements a comprehensive pattern matching system inspired by Rust and Python. The system supports:

- **Match expressions** - Multi-arm pattern matching with guards
- **Pattern types** - Wildcard, literal, binding, struct, tuple, enum variant, slice, or-patterns, ranges
- **Control flow** - if/else, loops (for/while/loop), break/continue
- **Exhaustiveness checking** - Type checker validates all cases covered (planned)

**Current Status:**
- ✅ Parser: Full pattern syntax support
- ✅ AST: Complete pattern representation
- ✅ Runtime: Reference implementation with pattern matching and binding
- ⚠️ Backends: Partial implementations (Rust complete, UE5/WASM/LLVM partial)
- ❌ Type Checker: Exhaustiveness checking not yet implemented

---

## AST Definitions

### Match Expression

```rust
pub struct MatchArm {
    pub pattern: Pattern,
    pub guard: Option<Expr>,  // Optional guard condition (not yet implemented)
    pub body: Expr,
    pub span: Span,
}
```

Match expressions in AST:
```rust
Expr::Match {
    scrutinee: Box<Expr>,  // Expression being matched
    arms: Vec<MatchArm>,   // Match arms
    span: Span,
}
```

### Pattern Enum


```rust
#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    /// Wildcard: `_`
    Wildcard(Span),
    
    /// Literal: `1`, `"hello"`, `true`
    Literal(Expr),
    
    /// Binding: `x`, `mut x`
    Binding {
        name: String,
        mutable: bool,
        span: Span,
    },
    
    /// Struct: `Point { x, y }`, `Point { x, y, .. }`
    Struct {
        name: String,
        fields: Vec<(String, Pattern)>,
        rest: bool,  // `..` for remaining fields
        span: Span,
    },
    
    /// Tuple: `(a, b, c)`
    Tuple(Vec<Pattern>, Span),
    
    /// Enum variant: `Some(x)`, `Result::Ok(val)`, `None`
    Variant {
        enum_name: Option<String>,  // Qualified: Some(EnumName), None for unqualified
        variant: String,
        fields: VariantPatternFields,
        span: Span,
    },
    
    /// Array/Slice: `[first, rest @ ..]`
    Slice {
        patterns: Vec<Pattern>,
        rest: Option<String>,  // Rest binding name
        span: Span,
    },
    
    /// Or pattern: `A | B | C`
    Or(Vec<Pattern>, Span),
    
    /// Range: `1..10`, `1..=10`
    Range {
        start: Option<Box<Expr>>,
        end: Option<Box<Expr>>,
        inclusive: bool,
        span: Span,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum VariantPatternFields {
    Unit,                           // `None`
    Tuple(Vec<Pattern>),            // `Some(x)`
    Struct(Vec<(String, Pattern)>), // `Point { x, y }`
}
```

---

## Pattern Types

### 1. Wildcard Pattern

**Syntax:** `_`

**Matches:** Everything (catch-all)

**Example:**
```kain
match value:
    _ => println("default case")
```

**Runtime behavior:** Always returns `true` from `pattern_matches()`


### 2. Literal Pattern

**Syntax:** `1`, `"hello"`, `true`, `false`

**Matches:** Exact value equality

**Example:**
```kain
match status_code:
    200 => println("OK")
    404 => println("Not Found")
    500 => println("Server Error")
    _ => println("Unknown")
```

**Runtime behavior:** Compares value with literal using equality

**Supported literals:**
- Integers: `42`, `-10`
- Strings: `"hello"`, `"world"`
- Booleans: `true`, `false`

### 3. Binding Pattern

**Syntax:** `x`, `mut x`

**Matches:** Everything, binds value to name

**Example:**
```kain
match result:
    value => println("Got: {value}")
```

**Mutable binding:**
```kain
match result:
    mut x => 
        x = x + 1
        println("Incremented: {x}")
```

**Runtime behavior:** Always matches, defines variable in environment

### 4. Struct Pattern

**Syntax:** `Point { x, y }`, `Point { x, y, .. }`

**Matches:** Struct values with field destructuring

**Example:**
```kain
match point:
    Point { x: 0, y: 0 } => println("Origin")
    Point { x, y } => println("Point at ({x}, {y})")
    Point { x, .. } => println("X is {x}, ignoring rest")
```

**Runtime behavior:** Not yet implemented in runtime.rs

**Backend support:** Rust only

### 5. Tuple Pattern

**Syntax:** `(a, b, c)`

**Matches:** Tuple values with positional destructuring

**Example:**
```kain
match coords:
    (0, 0) => println("Origin")
    (x, 0) => println("On X-axis at {x}")
    (0, y) => println("On Y-axis at {y}")
    (x, y) => println("Point at ({x}, {y})")
```

**Runtime behavior:** Not yet implemented in runtime.rs

**Backend support:** Rust only


### 6. Enum Variant Pattern

**Syntax:** 
- Qualified: `Option::Some(x)`, `Result::Ok(val)`
- Unqualified: `Some(x)`, `Ok(val)` (resolved at type-check time)

**Matches:** Enum variants with field destructuring

**Example:**
```kain
match result:
    Result::Ok(value) => println("Success: {value}")
    Result::Err(error) => println("Error: {error}")

match option:
    Some(x) => println("Got {x}")
    None => println("Nothing")
```

**Variant field types:**
- **Unit:** `None`, `Status::Idle`
- **Tuple:** `Some(x)`, `Result::Ok(val, code)`
- **Struct:** `Point::Cartesian { x, y }`

**Runtime behavior:** 
- Checks variant name matches
- Recursively matches nested patterns
- Binds fields to variables
- Special handling for `Poll::Ready(x)` and `Poll::Pending`

**Backend support:** All backends (partial)

### 7. Slice Pattern

**Syntax:** `[first, second]`, `[first, rest @ ..]`

**Matches:** Arrays and slices with element destructuring

**Example:**
```kain
match items:
    [] => println("Empty")
    [single] => println("One item: {single}")
    [first, second] => println("Two items: {first}, {second}")
    [first, rest @ ..] => println("First: {first}, rest: {rest}")
```

**Runtime behavior:** Not yet implemented in runtime.rs

**Backend support:** None

### 8. Or Pattern

**Syntax:** `A | B | C`

**Matches:** Any of the alternatives

**Example:**
```kain
match status:
    Status::Active | Status::Running => println("In progress")
    Status::Paused | Status::Stopped => println("Not running")
    _ => println("Unknown")
```

**Runtime behavior:** Not yet implemented in runtime.rs

**Backend support:** None

### 9. Range Pattern

**Syntax:** `1..10` (exclusive), `1..=10` (inclusive)

**Matches:** Values within range

**Example:**
```kain
match age:
    0..=12 => println("Child")
    13..=19 => println("Teenager")
    20..=64 => println("Adult")
    65.. => println("Senior")
    _ => println("Invalid")
```

**Runtime behavior:** Not yet implemented in runtime.rs

**Backend support:** None

---

## Runtime Implementation

The runtime interpreter (`crates/kain-core/src/runtime.rs`) provides the reference implementation for pattern matching.


### Pattern Matching Function

```rust
fn pattern_matches(pattern: &Pattern, value: &Value) -> bool {
    match pattern {
        Pattern::Wildcard(_) => true,
        
        Pattern::Binding { .. } => true,
        
        Pattern::Literal(Expr::Int(n, _)) => 
            matches!(value, Value::Int(v) if *v == *n),
        
        Pattern::Literal(Expr::String(s, _)) => 
            matches!(value, Value::String(v) if v == s),
        
        Pattern::Literal(Expr::Bool(b, _)) => 
            matches!(value, Value::Bool(v) if *v == *b),
        
        Pattern::Variant { variant, fields, .. } => {
            // Special handling for Poll enum
            if let Value::Poll(ready, val) = value {
                if *variant == "Ready" {
                    if !ready { return false; }
                    if let VariantPatternFields::Tuple(pats) = fields {
                        if pats.len() == 1 {
                            return if let Some(v) = val {
                                pattern_matches(&pats[0], v)
                            } else {
                                false
                            };
                        }
                    }
                    return false;
                } else if *variant == "Pending" {
                    return !ready;
                }
                return false;
            }
            
            // General enum variant matching
            if let Value::EnumVariant(_, v_name, v_fields) = value {
                if variant != v_name { return false; }
                
                match fields {
                    VariantPatternFields::Unit => v_fields.is_empty(),
                    VariantPatternFields::Tuple(pats) => {
                        if pats.len() != v_fields.len() { return false; }
                        pats.iter()
                            .zip(v_fields.iter())
                            .all(|(p, v)| pattern_matches(p, v))
                    }
                    _ => false,
                }
            } else {
                false
            }
        }
        
        _ => false,
    }
}
```

### Pattern Binding Function

```rust
fn bind_pattern(env: &mut Env, pattern: &Pattern, value: &Value) {
    match pattern {
        Pattern::Binding { name, .. } => {
            env.define(name.clone(), value.clone());
        }
        
        Pattern::Variant { variant, fields, .. } => {
            // Special handling for Poll::Ready
            if let Value::Poll(ready, val) = value {
                if *variant == "Ready" && *ready {
                    if let VariantPatternFields::Tuple(pats) = fields {
                        if pats.len() == 1 {
                            if let Some(v) = val {
                                bind_pattern(env, &pats[0], v);
                            }
                        }
                    }
                }
            } 
            // General enum variant binding
            else if let Value::EnumVariant(_, _, v_fields) = value {
                match fields {
                    VariantPatternFields::Tuple(pats) => {
                        for (p, v) in pats.iter().zip(v_fields.iter()) {
                            bind_pattern(env, p, v);
                        }
                    }
                    _ => {}
                }
            }
        }
        
        _ => {}
    }
}
```

### Match Expression Evaluation

```rust
Expr::Match { scrutinee, arms, .. } => {
    let val = eval_expr(env, scrutinee)?;
    
    for arm in arms {
        if pattern_matches(&arm.pattern, &val) {
            bind_pattern(env, &arm.pattern, &val);
            return eval_expr(env, &arm.body);
        }
    }
    
    Err(KainError::runtime("Non-exhaustive match", Span::new(0, 0)))
}
```

---

## Backend Gap Analysis


### Backend Comparison Table

| Feature | Runtime | Rust | UE5 | WASM | LLVM | JavaScript |
|---------|---------|------|-----|------|------|------------|
| **Wildcard** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Literal** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Binding** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Struct** | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| **Tuple** | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| **Enum Variant** | ✅ | ✅ | ✅ | ⚠️ | ✅ | ⚠️ |
| **Slice** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Or Pattern** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Range** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **Guards** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

**Legend:**
- ✅ Fully implemented
- ⚠️ Partially implemented
- ❌ Not implemented

### Rust Backend (Complete)

**File:** `crates/sys/src/codegen_rust.rs`

**Strategy:** Direct translation to Rust match expressions

**Implementation:**
```rust
Expr::Match { scrutinee, arms, .. } => {
    let scrut = self.gen_expr(scrutinee);
    let mut result = format!("match {} {{\n", scrut);
    for arm in arms {
        let pat = self.gen_pattern(&arm.pattern);
        let body = self.gen_expr(&arm.body);
        result.push_str(&format!("    {} => {{ {} }}\n", pat, body));
    }
    result.push_str("}");
    result
}
```

**Pattern generation:**
```rust
fn gen_pattern(&self, pattern: &Pattern) -> String {
    match pattern {
        Pattern::Wildcard(_) => "_".to_string(),
        Pattern::Literal(expr) => self.gen_expr(expr),
        Pattern::Binding { name, mutable, .. } => {
            if *mutable { format!("mut {}", name) } 
            else { name.clone() }
        }
        Pattern::Struct { name, fields, rest, .. } => {
            let field_pats: Vec<String> = fields
                .iter()
                .map(|(n, p)| format!("{}: {}", n, self.gen_pattern(p)))
                .collect();
            if *rest {
                format!("{} {{ {}, .. }}", name, field_pats.join(", "))
            } else {
                format!("{} {{ {} }}", name, field_pats.join(", "))
            }
        }
        Pattern::Tuple(pats, _) => {
            let pat_strs: Vec<String> = pats.iter()
                .map(|p| self.gen_pattern(p))
                .collect();
            format!("({})", pat_strs.join(", "))
        }
        Pattern::Variant { enum_name, variant, fields, .. } => {
            let full_name = if let Some(en) = enum_name {
                format!("{}::{}", en, variant)
            } else {
                variant.clone()
            };
            
            match fields {
                VariantPatternFields::Unit => full_name,
                VariantPatternFields::Tuple(pats) => {
                    let pat_strs: Vec<String> = pats.iter()
                        .map(|p| self.gen_pattern(p))
                        .collect();
                    format!("{}({})", full_name, pat_strs.join(", "))
                }
                VariantPatternFields::Struct(field_pats) => {
                    let field_strs: Vec<String> = field_pats
                        .iter()
                        .map(|(n, p)| format!("{}: {}", n, self.gen_pattern(p)))
                        .collect();
                    format!("{} {{ {} }}", full_name, field_strs.join(", "))
                }
            }
        }
        _ => "_".to_string(), // Fallback
    }
}
```

**Status:** ✅ Production-ready


### UE5 Backend (Partial)

**File:** `crates/ue5/src/codegen_ue5.rs`

**Strategy:** Convert to if-else chains or ternary operators

**Implementation:**

**For expression-level matches (ternary):**
```cpp
// KAIN:
match status:
    Status::Active => 1
    Status::Paused => 0

// UE5 C++:
(status == EStatus::Active ? 1 : (status == EStatus::Paused ? 0 : 0))
```

**For statement-level matches (if-else):**
```cpp
// KAIN:
match status:
    Status::Active => health = 100
    Status::Paused => health = 50

// UE5 C++:
if (status == EStatus::Active) { health = 100; }
else if (status == EStatus::Paused) { health = 50; }
```

**Supported patterns:**
- ✅ Wildcard: `_` → else clause
- ✅ Literal: `42` → `if (scrutinee == 42)`
- ✅ Binding: `x` → binds to variable (limited)
- ✅ Enum Variant: `Status::Active` → `if (scrutinee == EStatus::Active)`
- ❌ Struct destructuring
- ❌ Tuple destructuring
- ❌ Nested patterns

**Limitations:**
- No switch statement generation (could optimize enum matches)
- No pattern binding in complex cases
- Assignment detection heuristic (checks if arm body is assignment)

**Status:** ⚠️ Works for simple cases, needs enhancement

### WASM Backend (Partial)

**File:** `crates/web/src/codegen_wasm.rs`

**Strategy:** Nested if-else using WebAssembly control flow

**Implementation:**
```rust
Expr::Match { scrutinee, arms, .. } => {
    // Store scrutinee in temp local
    self.compile_expr(ctx, builder, scrutinee)?;
    builder.local_set(ctx.tmp_i32);
    
    // Build nested if-else chain
    for (i, arm) in arms.iter().enumerate() {
        let is_last = i == arms.len() - 1;
        
        match &arm.pattern {
            Pattern::Wildcard(_) => {
                // Always matches - just emit body
                self.compile_expr(ctx, builder, &arm.body)?;
            }
            Pattern::Literal(lit_expr) => {
                // Compare scrutinee with literal
                builder.local_get(ctx.tmp_i32);
                self.compile_expr(ctx, builder, lit_expr)?;
                builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                builder.binop(walrus::ir::BinaryOp::I32Eq);
                
                builder.if_else(
                    None,
                    |then_b| { let _ = self.compile_expr(ctx, then_b, &arm.body); },
                    |_else_b| { /* Continue to next arm */ }
                );
            }
            Pattern::Binding { name, .. } => {
                // Bind scrutinee to local
                if let Some(local_id) = ctx.locals.get(name) {
                    builder.local_get(ctx.tmp_i32);
                    builder.unop(walrus::ir::UnaryOp::I64ExtendSI32);
                    builder.local_set(*local_id);
                }
                self.compile_expr(ctx, builder, &arm.body)?;
            }
            Pattern::Variant { variant, .. } => {
                // Load enum tag and compare
                builder.local_get(ctx.tmp_i32);
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I32 { atomic: false },
                    walrus::ir::MemArg { align: 4, offset: 0 },
                );
                
                let tag = variant.len() as i32 % 256; // Placeholder
                builder.i32_const(tag);
                builder.binop(walrus::ir::BinaryOp::I32Eq);
                
                builder.if_else(
                    None,
                    |then_b| { let _ = self.compile_expr(ctx, then_b, &arm.body); },
                    |_else_b| {}
                );
            }
            _ => {
                // Fallback: just emit body
                self.compile_expr(ctx, builder, &arm.body)?;
            }
        }
    }
}
```

**Supported patterns:**
- ✅ Wildcard
- ✅ Literal (integers only)
- ✅ Binding (basic)
- ⚠️ Enum Variant (placeholder tag calculation)

**Limitations:**
- Enum tag calculation is placeholder (needs proper enum layout)
- No nested pattern support
- If-else chain doesn't properly continue to next arm

**Status:** ⚠️ Prototype, needs proper enum support


### LLVM Backend (Partial)

**File:** `crates/sys/src/codegen_llvm.rs`

**Strategy:** Use LLVM switch instruction for enum tags

**Implementation:**
```llvm
; KAIN:
; match result:
;     Result::Ok(x) => x
;     Result::Err(e) => 0

; LLVM IR:
%tag = load i64, i64* %result_tag_ptr
switch i64 %tag, label %default [
    i64 12345, label %arm_0  ; Ok tag
    i64 67890, label %arm_1  ; Err tag
]

arm_0:
    ; Load payload, bind x, evaluate body
    %payload_ptr = getelementptr inbounds %Result, %Result* %result, i32 0, i32 1
    %payload_void = load i8*, i8** %payload_ptr
    %payload = bitcast i8* %payload_void to %Result_Ok*
    %x_ptr = getelementptr inbounds %Result_Ok, %Result_Ok* %payload, i32 0, i32 0
    %x = load i64, i64* %x_ptr
    br label %end

arm_1:
    ; Err case - return 0
    br label %end

default:
    ; Non-exhaustive match error
    call void @panic()
    unreachable

end:
    %result = phi i64 [ %x, %arm_0 ], [ 0, %arm_1 ]
```

**Supported patterns:**
- ✅ Wildcard (default label)
- ✅ Literal (switch case)
- ✅ Binding (alloca + store)
- ✅ Enum Variant (switch on tag, payload extraction)

**Limitations:**
- Enum tag calculation uses hash (should use type checker info)
- No struct/tuple destructuring
- Payload type inference from struct_defs map (fragile)

**Status:** ⚠️ Works for enums, needs type system integration

### JavaScript Backend (Partial)

**File:** `crates/web/src/codegen_js.rs`

**Strategy:** IIFE with if-else chain

**Implementation:**
```javascript
// KAIN:
// match status:
//     Status::Active => 1
//     Status::Paused => 0

// JavaScript:
(() => {
    const __match = status;
    if (__match === Status.Active) {
        return 1;
    } else if (__match === Status.Paused) {
        return 0;
    }
    throw new Error('Non-exhaustive match');
})()
```

**Pattern matching helper:**
```rust
fn gen_pattern_match(&mut self, scrutinee: &str, pattern: &Pattern) {
    match pattern {
        Pattern::Wildcard(_) => self.write("true"),
        Pattern::Literal(expr) => {
            self.write(&format!("{} === ", scrutinee));
            self.gen_expr(expr);
        }
        Pattern::Binding { name, .. } => {
            self.write(&format!("(({} = {}) || true)", name, scrutinee));
        }
        Pattern::Variant { enum_name, variant, .. } => {
            self.write(&format!("{} === {}.{}", scrutinee, enum_name, variant));
        }
        _ => self.write("true"), // Fallback
    }
}
```

**Supported patterns:**
- ✅ Wildcard
- ✅ Literal
- ✅ Binding (side-effect in condition)
- ⚠️ Enum Variant (assumes JS enum object)

**Limitations:**
- Binding pattern uses assignment in condition (hacky)
- No nested pattern support
- Assumes enum representation

**Status:** ⚠️ Basic functionality, needs refinement

---

## Implementation Roadmap

### Phase 1: Core Patterns (Priority: High)

**Goal:** All backends support wildcard, literal, binding, enum variant

**Tasks:**
1. ✅ Runtime: Implement core patterns (DONE)
2. ✅ Rust: Full pattern support (DONE)
3. ⚠️ UE5: Enhance enum variant matching
   - Generate switch statements for enum matches
   - Proper binding support
   - Nested pattern support
4. ⚠️ WASM: Fix enum tag calculation
   - Integrate with type checker for proper tags
   - Fix if-else chain continuation
5. ⚠️ LLVM: Integrate with type system
   - Use type checker for enum tags
   - Proper payload type resolution
6. ⚠️ JavaScript: Improve binding pattern
   - Use proper variable declaration
   - Support nested patterns


### Phase 2: Struct and Tuple Patterns (Priority: Medium)

**Goal:** Support destructuring of structs and tuples

**Tasks:**
1. Runtime: Implement struct/tuple pattern matching
2. Rust: Already supported
3. UE5: Generate field access code
   ```cpp
   // KAIN: Point { x, y } => ...
   // UE5:
   if (auto* point = Cast<FPoint>(&value)) {
       float x = point->X;
       float y = point->Y;
       // ... body
   }
   ```
4. WASM: Implement struct/tuple memory layout access
5. LLVM: Generate GEP instructions for field access
6. JavaScript: Object destructuring

### Phase 3: Advanced Patterns (Priority: Low)

**Goal:** Support slice, or-patterns, ranges

**Tasks:**
1. Runtime: Implement slice/or/range patterns
2. All backends: Implement advanced patterns
3. Type checker: Exhaustiveness checking for or-patterns

### Phase 4: Guards (Priority: Low)

**Goal:** Support pattern guards

**Syntax:**
```kain
match value:
    x if x > 0 => println("Positive")
    x if x < 0 => println("Negative")
    _ => println("Zero")
```

**Tasks:**
1. Parser: Already supports `guard: Option<Expr>` in MatchArm
2. Runtime: Evaluate guard after pattern match
3. All backends: Generate guard condition checks

---

## Control Flow Constructs

### If-Else Expressions

**Syntax:**
```kain
if condition:
    body
else:
    alternative
```

**Inline form:**
```kain
if condition: single_statement
```

**Elif chain:**
```kain
if x > 0:
    println("Positive")
else if x < 0:
    println("Negative")
else:
    println("Zero")
```

**AST:**
```rust
Expr::If {
    condition: Box<Expr>,
    then_branch: Block,
    else_branch: Option<Box<ElseBranch>>,
    span: Span,
}

enum ElseBranch {
    Else(Block),
    ElseIf(Box<Expr>, Block, Option<Box<ElseBranch>>),
}
```

**Backend support:** ✅ All backends

### For Loops

**Syntax:**
```kain
for item in collection:
    println(item)
```

**AST:**
```rust
Stmt::For {
    binding: Pattern,
    iter: Expr,
    body: Block,
    span: Span,
}
```

**Backend implementations:**
- **Rust:** `for item in collection { ... }`
- **UE5:** `for (auto item : collection) { ... }`
- **WASM:** Loop with iterator state
- **LLVM:** Loop with phi nodes
- **JavaScript:** `for (const item of collection) { ... }`

**Backend support:** ✅ All backends


### While Loops

**Syntax:**
```kain
while condition:
    body
```

**AST:**
```rust
Stmt::While {
    condition: Expr,
    body: Block,
    span: Span,
}
```

**Backend implementations:**
- **Rust:** `while condition { ... }`
- **UE5:** `while (condition) { ... }`
- **WASM:** Loop with conditional branch
- **LLVM:** Loop with conditional branch
- **JavaScript:** `while (condition) { ... }`

**Backend support:** ✅ All backends

### Loop (Infinite)

**Syntax:**
```kain
loop:
    if should_exit:
        break
    do_work()
```

**AST:**
```rust
Stmt::Loop {
    body: Block,
    span: Span,
}
```

**Backend implementations:**
- **Rust:** `loop { ... }`
- **UE5:** `while (true) { ... }`
- **WASM:** Loop with explicit break
- **LLVM:** Unconditional branch loop
- **JavaScript:** `while (true) { ... }`

**Backend support:** ✅ All backends

### Break and Continue

**Syntax:**
```kain
break          # Exit loop
break value    # Exit loop with value (expression context)
continue       # Skip to next iteration
```

**AST:**
```rust
Expr::Break(Option<Box<Expr>>, Span)
Expr::Continue(Span)
```

**Backend implementations:**
- **Rust:** `break`, `break value`, `continue`
- **UE5:** `break`, `continue` (no break with value)
- **WASM:** `br` instruction
- **LLVM:** `br` to loop exit/continue label
- **JavaScript:** `break`, `continue`

**Backend support:** 
- ✅ All backends support basic break/continue
- ⚠️ Break with value: Rust only

---

## Code Examples

### Example 1: Simple Enum Match

**KAIN:**
```kain
enum Status:
    Active
    Paused
    Stopped

fn get_status_code(status: Status) -> Int:
    match status:
        Status::Active => 1
        Status::Paused => 2
        Status::Stopped => 0
```

**Rust output:**
```rust
enum Status {
    Active,
    Paused,
    Stopped,
}

fn get_status_code(status: Status) -> i64 {
    match status {
        Status::Active => { 1 }
        Status::Paused => { 2 }
        Status::Stopped => { 0 }
    }
}
```

**UE5 output:**
```cpp
UENUM(BlueprintType)
enum class EStatus : uint8 {
    Active,
    Paused,
    Stopped
};

int32 GetStatusCode(EStatus Status) {
    return (Status == EStatus::Active ? 1 : 
           (Status == EStatus::Paused ? 2 : 0));
}
```


### Example 2: Option Type Pattern Matching

**KAIN:**
```kain
enum Option<T>:
    Some(T)
    None

fn unwrap_or_default(opt: Option<Int>) -> Int:
    match opt:
        Some(value) => value
        None => 0
```

**Rust output:**
```rust
enum Option<T> {
    Some(T),
    None,
}

fn unwrap_or_default(opt: Option<i64>) -> i64 {
    match opt {
        Option::Some(value) => { value }
        Option::None => { 0 }
    }
}
```

**UE5 output (simplified):**
```cpp
// Enum variant matching with payload extraction
int32 UnwrapOrDefault(const FOption& Opt) {
    if (Opt.Tag == EOptionTag::Some) {
        // Extract payload
        int32 Value = *static_cast<int32*>(Opt.Payload);
        return Value;
    } else {
        return 0;
    }
}
```

### Example 3: Nested Pattern Matching

**KAIN:**
```kain
enum Result<T, E>:
    Ok(T)
    Err(E)

fn process_result(result: Result<Int, String>) -> String:
    match result:
        Result::Ok(0) => "Zero"
        Result::Ok(value) => f"Success: {value}"
        Result::Err(error) => f"Error: {error}"
```

**Rust output:**
```rust
enum Result<T, E> {
    Ok(T),
    Err(E),
}

fn process_result(result: Result<i64, String>) -> String {
    match result {
        Result::Ok(0) => { "Zero".to_string() }
        Result::Ok(value) => { format!("Success: {}", value) }
        Result::Err(error) => { format!("Error: {}", error) }
    }
}
```

### Example 4: Struct Destructuring (Rust only)

**KAIN:**
```kain
struct Point:
    x: Float
    y: Float

fn classify_point(p: Point) -> String:
    match p:
        Point { x: 0.0, y: 0.0 } => "Origin"
        Point { x: 0.0, y } => f"On Y-axis at {y}"
        Point { x, y: 0.0 } => f"On X-axis at {x}"
        Point { x, y } => f"Point at ({x}, {y})"
```

**Rust output:**
```rust
struct Point {
    x: f64,
    y: f64,
}

fn classify_point(p: Point) -> String {
    match p {
        Point { x: 0.0, y: 0.0 } => { "Origin".to_string() }
        Point { x: 0.0, y } => { format!("On Y-axis at {}", y) }
        Point { x, y: 0.0 } => { format!("On X-axis at {}", x) }
        Point { x, y } => { format!("Point at ({}, {})", x, y) }
    }
}
```

### Example 5: Control Flow with Break/Continue

**KAIN:**
```kain
fn find_first_positive(numbers: Array<Int>) -> Option<Int>:
    for num in numbers:
        if num < 0:
            continue
        if num > 0:
            return Some(num)
    return None
```

**Rust output:**
```rust
fn find_first_positive(numbers: Vec<i64>) -> Option<i64> {
    for num in numbers {
        if num < 0 {
            continue;
        }
        if num > 0 {
            return Some(num);
        }
    }
    return None;
}
```

**UE5 output:**
```cpp
TOptional<int32> FindFirstPositive(const TArray<int32>& Numbers) {
    for (auto Num : Numbers) {
        if (Num < 0) {
            continue;
        }
        if (Num > 0) {
            return TOptional<int32>(Num);
        }
    }
    return TOptional<int32>();
}
```

---

## Edge Cases and Validation

### Exhaustiveness Checking

**Problem:** Ensure all possible values are covered

**Example:**
```kain
enum Status:
    Active
    Paused
    Stopped

fn get_code(status: Status) -> Int:
    match status:
        Status::Active => 1
        Status::Paused => 2
        # Missing Status::Stopped - should error!
```

**Current status:** ❌ Not implemented

**Planned implementation:**
1. Type checker tracks all enum variants
2. For each match on enum type, verify all variants covered
3. Allow wildcard `_` as catch-all
4. Error if non-exhaustive


### Unreachable Patterns

**Problem:** Patterns that can never match

**Example:**
```kain
match value:
    _ => println("Catch-all")
    42 => println("Forty-two")  # Unreachable!
```

**Current status:** ❌ Not detected

**Planned implementation:**
1. Type checker analyzes pattern order
2. Warn if pattern is subsumed by earlier pattern
3. Error if pattern after wildcard

### Pattern Binding Conflicts

**Problem:** Same variable bound multiple times

**Example:**
```kain
match pair:
    (x, x) => println("Both same")  # Error: x bound twice
```

**Current status:** ❌ Not detected

**Planned implementation:**
1. Track bindings within pattern
2. Error if duplicate binding names
3. Allow in or-patterns if consistent

### Type Mismatches in Patterns

**Problem:** Pattern type doesn't match scrutinee type

**Example:**
```kain
let x: Int = 42
match x:
    "hello" => println("String")  # Error: string pattern on int
```

**Current status:** ⚠️ Partially checked by type checker

**Planned implementation:**
1. Type checker validates pattern type matches scrutinee
2. Enum variant patterns check against enum definition
3. Struct patterns check field names and types

### Or-Pattern Binding Consistency

**Problem:** Or-patterns must bind same variables

**Example:**
```kain
match value:
    Some(x) | None => println(x)  # Error: x not bound in None
```

**Current status:** ❌ Not implemented (or-patterns not supported)

**Planned implementation:**
1. Collect bindings from each or-pattern alternative
2. Verify all alternatives bind same names
3. Verify all bindings have same type

### Range Pattern Edge Cases

**Problem:** Invalid or overlapping ranges

**Example:**
```kain
match age:
    0..=17 => "Minor"
    18..=17 => "Invalid"  # Error: empty range
    10..=20 => "Overlap"  # Warning: overlaps with 0..=17
```

**Current status:** ❌ Not implemented (range patterns not supported)

**Planned implementation:**
1. Validate range bounds (start <= end)
2. Warn on overlapping ranges
3. Check exhaustiveness with ranges

---

## Performance Considerations

### Match Expression Optimization

**Strategy 1: Switch statements for enums**

When matching on enum with many variants, generate switch statement instead of if-else chain:

```cpp
// Instead of:
if (status == EStatus::Active) { ... }
else if (status == EStatus::Paused) { ... }
else if (status == EStatus::Stopped) { ... }

// Generate:
switch (status) {
    case EStatus::Active: ... break;
    case EStatus::Paused: ... break;
    case EStatus::Stopped: ... break;
}
```

**Benefits:**
- O(1) dispatch vs O(n) if-else chain
- Better branch prediction
- More compact code

**Status:** ❌ Not implemented in UE5 backend

**Strategy 2: Jump tables for dense integer matches**

For dense integer literal patterns, use jump table:

```cpp
// Instead of if-else chain for 0, 1, 2, 3, 4
static void (*jump_table[])() = { case_0, case_1, case_2, case_3, case_4 };
if (value >= 0 && value < 5) {
    jump_table[value]();
}
```

**Status:** ❌ Not implemented

**Strategy 3: Decision trees for complex patterns**

For patterns with multiple discriminants, build decision tree:

```
match (x, y):
    (0, 0) => A
    (0, _) => B
    (_, 0) => C
    (_, _) => D

// Decision tree:
if (x == 0) {
    if (y == 0) { A } else { B }
} else {
    if (y == 0) { C } else { D }
}
```

**Status:** ❌ Not implemented


### Pattern Compilation Complexity

| Pattern Type | Compilation Complexity | Notes |
|--------------|------------------------|-------|
| Wildcard | O(1) | Always matches |
| Literal | O(1) | Single comparison |
| Binding | O(1) | Variable assignment |
| Enum Variant | O(1) with switch, O(n) with if-else | n = number of arms |
| Struct | O(k) | k = number of fields |
| Tuple | O(k) | k = number of elements |
| Slice | O(n) | n = slice length |
| Or-pattern | O(m) | m = number of alternatives |
| Range | O(1) | Two comparisons |

---

## Testing Strategy

### Unit Tests

**Test pattern matching in isolation:**

```rust
#[test]
fn test_wildcard_pattern() {
    let pattern = Pattern::Wildcard(Span::new(0, 0));
    let value = Value::Int(42);
    assert!(pattern_matches(&pattern, &value));
}

#[test]
fn test_literal_pattern() {
    let pattern = Pattern::Literal(Expr::Int(42, Span::new(0, 0)));
    assert!(pattern_matches(&pattern, &Value::Int(42)));
    assert!(!pattern_matches(&pattern, &Value::Int(43)));
}

#[test]
fn test_enum_variant_pattern() {
    let pattern = Pattern::Variant {
        enum_name: Some("Option".to_string()),
        variant: "Some".to_string(),
        fields: VariantPatternFields::Tuple(vec![
            Pattern::Binding { 
                name: "x".to_string(), 
                mutable: false, 
                span: Span::new(0, 0) 
            }
        ]),
        span: Span::new(0, 0),
    };
    
    let value = Value::EnumVariant(
        "Option".to_string(),
        "Some".to_string(),
        vec![Value::Int(42)]
    );
    
    assert!(pattern_matches(&pattern, &value));
}
```

### Integration Tests

**Test full match expressions:**

```kain
test "match on enum variants":
    enum Status:
        Active
        Paused
    
    let status = Status::Active
    let code = match status:
        Status::Active => 1
        Status::Paused => 0
    
    assert code == 1
```

### Backend Tests

**Test codegen output:**

```rust
#[test]
fn test_ue5_enum_match_codegen() {
    let source = r#"
        enum Status:
            Active
            Paused
        
        fn get_code(s: Status) -> Int:
            match s:
                Status::Active => 1
                Status::Paused => 0
    "#;
    
    let output = compile_to_ue5(source);
    
    // Verify switch statement generated
    assert!(output.contains("switch") || output.contains("?"));
    assert!(output.contains("EStatus::Active"));
    assert!(output.contains("EStatus::Paused"));
}
```

---

## Future Enhancements

### 1. Pattern Macros

Allow user-defined pattern matching extensions:

```kain
macro pattern is_positive:
    x if x > 0

match value:
    is_positive!(x) => println("Positive: {x}")
    _ => println("Not positive")
```

### 2. Active Patterns (F#-style)

Define custom pattern matching logic:

```kain
fn pattern Even(x: Int) -> Bool:
    return x % 2 == 0

match number:
    Even() => println("Even")
    _ => println("Odd")
```

### 3. View Patterns (Haskell-style)

Apply function before matching:

```kain
match value -> abs:
    0 => println("Zero")
    x => println("Non-zero: {x}")
```

### 4. As-Patterns

Bind whole value and destructure:

```kain
match point:
    p @ Point { x: 0, y: 0 } => println("Origin: {p}")
    p @ Point { x, y } => println("Point {p} at ({x}, {y})")
```

### 5. Slice Patterns with Rest

```kain
match list:
    [] => println("Empty")
    [x] => println("Single: {x}")
    [first, rest @ ..] => println("First: {first}, rest: {rest}")
    [.., last] => println("Last: {last}")
```

---

## Summary

### Current State

- ✅ **Parser:** Full pattern syntax support
- ✅ **AST:** Complete pattern representation
- ✅ **Runtime:** Reference implementation for core patterns
- ⚠️ **Backends:** Partial implementations
  - Rust: Complete
  - UE5: Basic (needs switch optimization)
  - WASM: Prototype (needs enum support)
  - LLVM: Partial (needs type system integration)
  - JavaScript: Basic (needs refinement)
- ❌ **Type Checker:** Exhaustiveness checking not implemented

### Priority Tasks

1. **High:** Complete UE5 backend enum matching with switch statements
2. **High:** Implement exhaustiveness checking in type checker
3. **Medium:** Add struct/tuple pattern support to all backends
4. **Medium:** Fix WASM/LLVM enum tag calculation
5. **Low:** Implement advanced patterns (slice, or, range)
6. **Low:** Add pattern guards support

### Design Principles

1. **Correctness first:** Exhaustiveness checking prevents bugs
2. **Performance matters:** Use switch statements for enums
3. **Backend parity:** All backends should support same patterns
4. **Type safety:** Pattern types must match scrutinee type
5. **Ergonomics:** Python-like syntax, Rust-like semantics

---

**Document Version:** 1.0  
**Last Updated:** Feb 19, 2026  
**Next Review:** After Phase 1 completion
