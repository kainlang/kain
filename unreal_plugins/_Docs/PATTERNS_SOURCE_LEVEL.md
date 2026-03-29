# Source-Level Fix Patterns — Cross-Plugin Database

**Last Updated:** Phase 6 Documentation  
**Source:** Materialize, VoxelForgePro, Cinema4DMograph, TemporalBlueprint, MetaFitter

---

## Pattern 1: Struct Literal Initializers

**ID:** `SRC-001`  
**Category:** Syntax  
**Priority:** HIGH

### Problem
Struct literal syntax `TypeName { field: val }` or `TypeName(field: val)` is not valid KAIN syntax.

### Error Message
```
Parse error: Expected statement, got '{'
```

### Root Cause
Conversion scripts or manual code used Rust-style or function-call-style struct initialization.

### Fix Strategy
**Manual** - Replace with field-by-field assignment

### Code Example
```kain
# Invalid (Rust-style)
let p = Particle { position: vec3(0,0,0), age: 0.0 }

# Invalid (function-call style)
let p = Particle(position: vec3(0,0,0), age: 0.0)

# Valid KAIN
let p = Particle
p.position = vec3(0, 0, 0)
p.age = 0.0
```

### Frequency
- **Materialize:** 50+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
Medium - Could detect pattern and auto-convert, but requires understanding field names

---

## Pattern 2: `var` Keyword

**ID:** `SRC-002`  
**Category:** Syntax  
**Priority:** HIGH

### Problem
`var` keyword used for variable declarations. KAIN uses `let` for all declarations.

### Error Message
```
Parse error: Unexpected token 'var'
```

### Root Cause
Conversion from JavaScript/Rust-style code

### Fix Strategy
**Automated** - Simple find/replace

### Code Example
```kain
# Invalid
var count = 0
var name = "test"

# Valid KAIN
let count = 0
let name = "test"
```

### Frequency
- **Materialize:** 20+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Simple regex replacement: `\bvar\b` → `let`

---

## Pattern 3: `not` Keyword

**ID:** `SRC-003`  
**Category:** Syntax  
**Priority:** MEDIUM

### Problem
`not expr` used for boolean negation. KAIN uses `== false`.

### Error Message
```
Parse error: Unexpected token 'not'
```

### Root Cause
Python-style boolean negation

### Fix Strategy
**Automated** - Find/replace with context

### Code Example
```kain
# Invalid
if not is_valid:
    return

# Valid KAIN
if is_valid == false:
    return
```

### Frequency
- **Materialize:** 15+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Regex replacement: `\bnot\s+(\w+)` → `$1 == false`

---

## Pattern 4: Boolean Operators (`&&`, `||`)

**ID:** `SRC-004`  
**Category:** Syntax  
**Priority:** HIGH

### Problem
`&&` and `||` used for boolean logic. KAIN uses `and` and `or`.

### Error Message
```
Parse error: Unexpected token '&&'
```

### Root Cause
C-style boolean operators

### Fix Strategy
**Automated** - Simple find/replace

### Code Example
```kain
# Invalid
if x > 0 && y < 10:
    process()

if a == 1 || b == 2:
    handle()

# Valid KAIN
if x > 0 and y < 10:
    process()

if a == 1 or b == 2:
    handle()
```

### Frequency
- **Materialize:** 30+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Simple replacement: `&&` → `and`, `||` → `or`

---

## Pattern 5: `for..in` Range Loops

**ID:** `SRC-005`  
**Category:** Syntax  
**Priority:** HIGH

### Problem
`for i in start..end:` syntax not supported. KAIN requires `while` loops.

### Error Message
```
Parse error: Unexpected 'for' statement
```

### Root Cause
Rust/Python-style range loops

### Fix Strategy
**Manual** - Convert to while loop with explicit counter

### Code Example
```kain
# Invalid
for i in 0..count:
    process(i)

# Valid KAIN
let i = 0
while i < count:
    process(i)
    i = i + 1
```

### Frequency
- **Materialize:** 25+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
Medium - Pattern is regular but requires code generation

---

## Pattern 6: Struct Field Access with `::`

**ID:** `SRC-006`  
**Category:** Semantic  
**Priority:** HIGH

### Problem
`::` used for struct field access. KAIN uses `::` only for enum variants, `.` for struct fields.

### Error Message
```
Type error: Cannot use '::' on non-enum type
```

### Root Cause
Confusion between enum variant syntax and struct field syntax

### Fix Strategy
**Automated** - Replace `::` with `.` on struct types

### Code Example
```kain
# Invalid
let age = particle::age
let vel = particle::velocity

# Valid KAIN
let age = particle.age
let vel = particle.velocity
```

### Frequency
- **Materialize:** 40+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Requires type information but pattern is clear

---

## Pattern 7: Match Arm Braces

**ID:** `SRC-007`  
**Category:** Syntax  
**Priority:** MEDIUM

### Problem
Match arms use `{ }` block syntax. KAIN uses indented blocks.

### Error Message
```
Parse error: Unexpected '{' in match arm
```

### Root Cause
Rust-style match syntax

### Fix Strategy
**Manual** - Remove braces, use indentation

### Code Example
```kain
# Invalid
match value:
    0 => { return "zero" }
    1 => { return "one" }

# Valid KAIN
match value:
    0 => return "zero"
    1 => return "one"
```

### Frequency
- **Materialize:** 10+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
Medium - Pattern is regular but requires indentation handling

---

## Pattern 8: Actor/Subsystem Field Declarations with `let`

**ID:** `SRC-008`  
**Category:** Semantic  
**Priority:** HIGH

### Problem
Fields inside `actor` or `@subsystem` blocks use `let` keyword. KAIN uses bare declarations with `state` for actor state.

### Error Message
```
Parse error: Unexpected 'let' in actor body
```

### Root Cause
Variable declaration syntax used in class-like context

### Fix Strategy
**Manual** - Remove `let`, add `state` where appropriate

### Code Example
```kain
# Invalid
actor Player:
    let health: Float = 100.0
    let max_health: Float = 100.0

# Valid KAIN
actor Player:
    state health: Float = 100.0
    state max_health: Float = 100.0
```

### Frequency
- **Materialize:** 15+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Pattern is regular within actor/subsystem blocks

---

## Pattern 9: Cast Expressions in Shaders (`as Type`)

**ID:** `SRC-009`  
**Category:** Codegen Limitation  
**Priority:** HIGH

### Problem
`expr as Type` cast syntax not supported in USF shader codegen.

### Error Message
```
Warning: Unsupported expression in shader
```

### Root Cause
USF codegen backend doesn't implement cast expression node

### Fix Strategy
**Backend Fix** or **Manual Workaround** - Remove casts or implement in codegen

### Code Example
```kain
# Invalid in USF
let id = thread_id.x as UInt

# Workaround (remove cast, rely on implicit conversion)
let id = thread_id.x

# Or use constructor syntax
let id = uint(thread_id.x)
```

### Frequency
- **Materialize:** 38 occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Backend Fix Required
Yes - Implement cast expression in `codegen_usf.rs`

### Automation Potential
High - Once backend fix applied, no source changes needed

---

## Pattern 10: Array Literals in Shaders

**ID:** `SRC-010`  
**Category:** Codegen Limitation  
**Priority:** HIGH

### Problem
Array literals `[a, b, c]` not supported in USF shader codegen.

### Error Message
```
Warning: Unsupported expression in shader
```

### Root Cause
USF codegen backend doesn't implement array literal node

### Fix Strategy
**Backend Fix** or **Manual Workaround** - Replace with if/else chains

### Code Example
```kain
# Invalid in USF
let dirs = [vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0)]
let d = dirs[i]

# Workaround
let dir_x = if i == 0: 1.0 else: if i == 1: -1.0 else: 0.0
let dir_y = if i == 0: 0.0 else: if i == 1: 0.0 else: 1.0
```

### Frequency
- **Materialize:** 20+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Backend Fix Required
Yes - Implement array literal in `codegen_usf.rs`

### Automation Potential
High - Once backend fix applied, no source changes needed

---

## Pattern 11: Reserved Word as Parameter Name

**ID:** `SRC-011`  
**Category:** Semantic  
**Priority:** MEDIUM

### Problem
Reserved keyword `state` used as parameter name.

### Error Message
```
Parse error: Expected identifier, got 'state'
```

### Root Cause
Keyword collision

### Fix Strategy
**Manual** - Rename parameter

### Code Example
```kain
# Invalid
fn process(state: GameState):
    update(state)

# Valid KAIN
fn process(game_state: GameState):
    update(game_state)
```

### Frequency
- **Materialize:** 5+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
Low - Requires semantic understanding of context

---

## Pattern 12: `let mut` Declarations

**ID:** `SRC-012`  
**Category:** Syntax  
**Priority:** MEDIUM

### Problem
`let mut` used for mutable declarations. KAIN uses `let` for all declarations.

### Error Message
```
Parse error: Unexpected 'mut'
```

### Root Cause
Rust-style mutability syntax

### Fix Strategy
**Automated** - Remove `mut` keyword

### Code Example
```kain
# Invalid
let mut count = 0
let mut name = "test"

# Valid KAIN
let count = 0
let name = "test"
```

### Frequency
- **Materialize:** 10+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Simple regex: `let\s+mut\s+` → `let `

---

## Pattern 13: Vec3 Struct Literal Constructor

**ID:** `SRC-013`  
**Category:** Syntax  
**Priority:** MEDIUM

### Problem
`Vec3i { x: 0, y: 0, z: 0 }` struct literal syntax not supported.

### Error Message
```
Parse error: Expected statement, got '{'
```

### Root Cause
Rust-style struct literal

### Fix Strategy
**Automated** - Replace with constructor function

### Code Example
```kain
# Invalid
let pos = Vec3i { x: 0, y: 0, z: 0 }

# Valid KAIN
let pos = vec3i(0, 0, 0)
```

### Frequency
- **Materialize:** 15+ occurrences
- **VoxelForgePro:** TBD
- **Cinema4DMograph:** TBD
- **TemporalBlueprint:** TBD
- **MetaFitter:** TBD

### Automation Potential
High - Pattern is very regular

---

## Summary Statistics

| Pattern ID | Name | Priority | Automation | Backend Fix |
|------------|------|----------|------------|-------------|
| SRC-001 | Struct Literal Initializers | HIGH | Medium | No |
| SRC-002 | var Keyword | HIGH | High | No |
| SRC-003 | not Keyword | MEDIUM | High | No |
| SRC-004 | Boolean Operators | HIGH | High | No |
| SRC-005 | for..in Loops | HIGH | Medium | No |
| SRC-006 | Struct Field :: | HIGH | High | No |
| SRC-007 | Match Arm Braces | MEDIUM | Medium | No |
| SRC-008 | Actor Field let | HIGH | High | No |
| SRC-009 | Cast in Shaders | HIGH | High | **YES** |
| SRC-010 | Array in Shaders | HIGH | High | **YES** |
| SRC-011 | Reserved Word Param | MEDIUM | Low | No |
| SRC-012 | let mut | MEDIUM | High | No |
| SRC-013 | Vec3 Struct Literal | MEDIUM | High | No |

**Total Patterns:** 13  
**Require Backend Fix:** 2  
**High Automation Potential:** 9  
**High Priority:** 7
