# Stream FOXTROT: Typechecker + Monomorphizer

**Stream ID:** FOXTROT
**Role:** Implement the 4-pass typecheck pipeline, the complete `types_compatible()` decision tree for 20 ResolvedType variants, effect checking via `can_call()` lattice, generic monomorphization via `unify()`/`substitute_type()`, and the stub strategy for Layers 1-7 constructs
**Effort:** ~8 hours
**Depends On:** Stream DELTA (ast.kn AstNode struct + AST_* constants), Stream ALPHA (token.kn for Token reference in type env)
**Requirements Covered:** FR-TYPE.1–43
**Design Reference:** Research 02, Design §§TYPE

---

## Context

The typechecker is the compiler's semantic core. You implement a 4-pass pipeline (predeclare → register → re-register → check), the central `types_compatible()` function covering all 20 ResolvedType variants, an effect checking lattice (`Pure` bottom, `Unsafe` top, 4 rules), generic monomorphization with `unify()`/`substitute_type()`, and a stub strategy that treats Layer 1-7 constructs as simplified Layer 0 equivalents. The output is a `TypedProgram` that GOLF's codegen consumes.

**Critical dependency:** You MUST read DELTA's completed `ast.kn` for the AstNode struct and ALL AST_*, BINOP_*, UNOP_* tag constants. Also import ALPHA's `error.kn` for Diagnostic and `builtins.kn` from ECHO for primitive types.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\types.kn` | ResolvedType struct (20 variants), TypeEnv, 4-pass pipeline, types_compatible(), expression typecheck (~1500 lines) | GOLF reads |
| `X:\blades\kain\src\effects.kn` | Effect bitmask constants, can_call() lattice, check_effect_call() (~200 lines) | GOLF reads |
| `X:\blades\kain\src\monomorphize.kn` | unify(), substitute_type(), instantiate_generic(), MonomorphizedProgram (~400 lines) | GOLF reads |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\token.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\lexer.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\parser.kn` | Owned by DELTA (read-only) |
| `X:\blades\kain\src\ast.kn` | Owned by ALPHA+DELTA (read-only) |
| `X:\blades\kain\src\codegen.kn` | Owned by GOLF |
| `X:\blades\kain\src\llvm_ffi.kn` | Owned by ECHO+GOLF |

---

## Implementation Tasks

---

### FOXTROT-01: ResolvedType Struct + TypeEnv (`types.kn`, part 1)

**Effort:** 1.5h
**Objective:** Define the flat `ResolvedType` struct with 20 kind constants, the `TypeEnv` struct, and the `TypedProgram`/`TypedItem` output types.

**Implementation:**

Create `X:\blades\kain\src\types.kn`:

```kn
// types.kn — Type system core: ResolvedType, TypeEnv, 4-pass pipeline
// STREAM: FOXTROT
// Consumed by: GOLF (codegen)

// ── ResolvedType kind constants (20 variants) ──
pub const RT_UNIT:     Int = 0
pub const RT_BOOL:     Int = 1
pub const RT_INT:      Int = 2
pub const RT_FLOAT:    Int = 3
pub const RT_STRING:   Int = 4
pub const RT_CHAR:     Int = 5
pub const RT_ARRAY:    Int = 6
pub const RT_SLICE:    Int = 7
pub const RT_TUPLE:    Int = 8
pub const RT_REF:      Int = 9
pub const RT_PTR:      Int = 10
pub const RT_OPTION:   Int = 11
pub const RT_RESULT:   Int = 12
pub const RT_FUTURE:   Int = 13
pub const RT_STRUCT:   Int = 14
pub const RT_ENUM:     Int = 15
pub const RT_FUNCTION: Int = 16
pub const RT_GENERIC:  Int = 17
pub const RT_NEVER:    Int = 18
pub const RT_UNKNOWN:  Int = 19

// ── ResolvedType struct ──
// Flat representation using integer indices — NOT recursive
pub struct ResolvedType:
    kind: Int            // RT_* discriminant
    int_size: Int        // RT_INT: positive=signed, negative=unsigned; 1=I8, 2=I16, 4=I32, 8=I64
    float_size: Int      // RT_FLOAT: 4=F32, 8=F64
    name: Int            // RT_STRUCT, RT_ENUM, RT_GENERIC: string table index
    inner_type: Int      // RT_ARRAY, RT_SLICE, RT_OPTION, RT_FUTURE, RT_REF, RT_PTR: inner type index
    array_len: Int       // RT_ARRAY: compile-time length
    tuple_types: Int     // RT_TUPLE: index into TypeEnv's type array
    tuple_len: Int       // RT_TUPLE: number of elements
    result_ok: Int       // RT_RESULT: ok type index
    result_err: Int      // RT_RESULT: err type index
    fn_params: Int       // RT_FUNCTION: start index in TypeEnv
    fn_param_count: Int  // RT_FUNCTION: param count
    fn_ret: Int          // RT_FUNCTION: return type index
    fn_effects: Int      // RT_FUNCTION: effect bitmask
    ref_mutable: Bool    // RT_REF, RT_PTR: mutability

// ── Effect bitmask constants ──
pub const EFF_PURE:     Int = 0x00
pub const EFF_IO:       Int = 0x01
pub const EFF_GPU:      Int = 0x02
pub const EFF_ASYNC:    Int = 0x04
pub const EFF_REACTIVE: Int = 0x08
pub const EFF_UNSAFE:   Int = 0x10
pub const EFF_ALLOC:    Int = 0x20
pub const EFF_PANIC:    Int = 0x40

// ── Constructors for common types ──
pub fn type_unit() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_UNIT
    return t

pub fn type_bool() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_BOOL
    return t

pub fn type_i64() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_INT
    t.int_size = 8  // I64 (positive = signed)
    return t

pub fn type_f64() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_FLOAT
    t.float_size = 8
    return t

pub fn type_string() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_STRING
    return t

pub fn type_unknown() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_UNKNOWN
    return t

pub fn type_never() -> ResolvedType:
    let mut t: ResolvedType = zero_type()
    t.kind = RT_NEVER
    return t

pub fn zero_type() -> ResolvedType:
    return ResolvedType {
        kind: RT_UNKNOWN,
        int_size: 0, float_size: 0, name: -1,
        inner_type: -1, array_len: 0,
        tuple_types: -1, tuple_len: 0,
        result_ok: -1, result_err: -1,
        fn_params: -1, fn_param_count: 0, fn_ret: -1, fn_effects: 0,
        ref_mutable: false,
    }

// ── TypeEnv — the type environment ──
pub struct TypeEnv:
    types:           HashMap<String, ResolvedType>
    values:          HashMap<String, ResolvedType>
    scopes:          Array<Scope>
    all_types:       Array<ResolvedType>    // flat array for index-based references
    type_names:      Array<String>          // parallel to all_types
    enum_variants:   HashMap<String, HashMap<String, Int>>
    errors:          DiagnosticBag
    skip_2:          Array<Bool>
    skip_3:          Array<Bool>

pub struct Scope:
    bindings: HashMap<String, ResolvedType>

pub fn type_env_new() -> TypeEnv:
    let mut env: TypeEnv = TypeEnv {
        types: empty_map(),
        values: empty_map(),
        scopes: empty_array(),
        all_types: empty_array(),
        type_names: empty_array(),
        enum_variants: empty_map(),
        errors: diag_bag_new(),
        skip_2: empty_array(),
        skip_3: empty_array(),
    }

    // Pre-register primitive types
    env.types.insert("Int", type_i64())
    env.types.insert("Float", type_f64())
    env.types.insert("Bool", type_bool())
    env.types.insert("String", type_string())
    env.types.insert("Unit", type_unit())
    // ... register I8-I128, U8-U128, etc.

    // Push initial scope
    let global_scope: Scope = Scope { bindings: empty_map() }
    env.scopes.push(global_scope)

    return env

// ── TypeEnv index-based type storage ──
pub fn type_env_register(env: *mut TypeEnv, name: String, ty: ResolvedType) -> Int:
    let idx: Int = len(env.all_types)
    env.all_types.push(ty)
    env.type_names.push(name)
    return idx

pub fn type_env_get(env: TypeEnv, idx: Int) -> ResolvedType:
    if idx < 0 or idx >= len(env.all_types):
        return type_unknown()
    return env.all_types[idx]

// ── TypedProgram + TypedItem outputs ──
pub struct TypedProgram:
    items:   Array<TypedItem>
    env:     TypeEnv
    errors:  DiagnosticBag

pub struct TypedItem:
    kind:      Int
    name:      String
    resolved_type: ResolvedType
    ast_index: Int
    effects:   Int
```

**Acceptance Criteria:**
- [ ] All 20 RT_* constants defined with correct values (0–19)
- [ ] `ResolvedType` struct has all 14 fields
- [ ] Constructor functions for common types (Unit, Bool, I64, F64, String)
- [ ] `TypeEnv` with pre-registered primitive types
- [ ] `TypedProgram` and `TypedItem` structs defined

---

### FOXTROT-02: types_compatible() — Complete Decision Tree (`types.kn`, part 2)

**Effort:** 1.5h
**Objective:** Implement the complete pairwise type compatibility function covering all 20 ResolvedType variants.

**Implementation (append to `types.kn`):**

```kn
// ── Type Compatibility ──
pub fn types_compatible(expected: ResolvedType, actual: ResolvedType) -> Bool:
    // Escape valves
    if expected.kind == RT_UNKNOWN or actual.kind == RT_UNKNOWN:
        return true
    if expected.kind == RT_NEVER or actual.kind == RT_NEVER:
        return true
    if expected.kind == RT_GENERIC or actual.kind == RT_GENERIC:
        return true

    // Primitives
    if expected.kind == RT_UNIT and actual.kind == RT_UNIT:
        return true
    if expected.kind == RT_BOOL and actual.kind == RT_BOOL:
        return true
    if expected.kind == RT_STRING and actual.kind == RT_STRING:
        return true
    if expected.kind == RT_CHAR and actual.kind == RT_CHAR:
        return true

    // Integer — any sizes cross-compatible
    if expected.kind == RT_INT and actual.kind == RT_INT:
        return true

    // Float — any sizes cross-compatible
    if expected.kind == RT_FLOAT and actual.kind == RT_FLOAT:
        return true

    // Numeric promotion: Int ↔ Float
    if expected.kind == RT_INT and actual.kind == RT_FLOAT:
        return true
    if expected.kind == RT_FLOAT and actual.kind == RT_INT:
        return true

    // Array: same length (or 0 = unknown), compatible element types
    if expected.kind == RT_ARRAY and actual.kind == RT_ARRAY:
        if expected.array_len != 0 and actual.array_len != 0:
            if expected.array_len != actual.array_len:
                return false
        let elem_expected: ResolvedType = type_env_get(/* env */, expected.inner_type)
        let elem_actual: ResolvedType = type_env_get(/* env */, actual.inner_type)
        return types_compatible(elem_expected, elem_actual)

    // Slice
    if expected.kind == RT_SLICE and actual.kind == RT_SLICE:
        return types_compatible(type_env_get(/* env */, expected.inner_type),
                                type_env_get(/* env */, actual.inner_type))

    // Slice from Array
    if expected.kind == RT_SLICE and actual.kind == RT_ARRAY:
        return types_compatible(type_env_get(/* env */, expected.inner_type),
                                type_env_get(/* env */, actual.inner_type))

    // Tuple: structural matching
    if expected.kind == RT_TUPLE and actual.kind == RT_TUPLE:
        if expected.tuple_len != actual.tuple_len:
            return false
        var i: Int = 0
        while i < expected.tuple_len:
            // compare element by element...
            i = i + 1
        return true

    // Option<T>
    if expected.kind == RT_OPTION and actual.kind == RT_OPTION:
        return types_compatible(type_env_get(/* env */, expected.inner_type),
                                type_env_get(/* env */, actual.inner_type))

    // Result<T, E>
    if expected.kind == RT_RESULT and actual.kind == RT_RESULT:
        return types_compatible(type_env_get(/* env */, expected.result_ok),
                                type_env_get(/* env */, actual.result_ok)) and
               types_compatible(type_env_get(/* env */, expected.result_err),
                                type_env_get(/* env */, actual.result_err))

    // Future<T>
    if expected.kind == RT_FUTURE and actual.kind == RT_FUTURE:
        return types_compatible(type_env_get(/* env */, expected.inner_type),
                                type_env_get(/* env */, actual.inner_type))

    // Reference auto-deref (immutable refs only)
    if expected.kind == RT_REF and !expected.ref_mutable:
        return types_compatible(type_env_get(/* env */, expected.inner_type), actual)
    if actual.kind == RT_REF and !actual.ref_mutable:
        return types_compatible(expected, type_env_get(/* env */, actual.inner_type))

    // Pointer — exact match only
    if expected.kind == RT_PTR and actual.kind == RT_PTR:
        return true  // opaque pointers

    // Function — structural matching
    if expected.kind == RT_FUNCTION and actual.kind == RT_FUNCTION:
        if expected.fn_param_count != actual.fn_param_count:
            return false
        if !types_compatible(type_env_get(/* env */, expected.fn_ret),
                             type_env_get(/* env */, actual.fn_ret)):
            return false
        var i: Int = 0
        while i < expected.fn_param_count:
            // compare params
            i = i + 1
        return true

    // Named types — nominal matching (name equality only)
    if expected.kind == RT_STRUCT and actual.kind == RT_STRUCT:
        return expected.name == actual.name
    if expected.kind == RT_ENUM and actual.kind == RT_ENUM:
        return expected.name == actual.name

    return false
```

NOTE: In the actual implementation, `types_compatible()` takes a third parameter `env: TypeEnv` for resolving type indices. The above is the logic skeleton — fill in the env parameter for all recursive calls.

**Acceptance Criteria:**
- [ ] All 20 ResolvedType variants handled in the decision tree
- [ ] Escape valves work: Unknown, Never, Generic always compatible
- [ ] Integer cross-compatibility: any Int size matches any Int size
- [ ] Numeric promotion: Int↔Float compatible
- [ ] Array length matching (0 = unknown)
- [ ] Struct/Enum nominal matching (name equality only)
- [ ] Ref auto-deref for immutable refs
- [ ] Fallthrough → false

---

### FOXTROT-03: 4-Pass Typecheck Pipeline (`types.kn`, part 3)

**Effort:** 1.5h
**Objective:** Implement pass1_predeclare(), pass2_register(), pass3_re_register(), and pass4_check().

**Implementation (append to `types.kn`):**

```kn
// ── 4-Pass Typecheck Pipeline ──

pub fn typecheck(env: *mut TypeEnv, program: AstProgram) -> TypedProgram:
    let nodes: Array<AstNode> = program.nodes
    let n: Int = len(nodes)

    // Initialize skip vectors
    var i: Int = 0
    while i < n:
        env.skip_2.push(true)  // true = item PASSED pass 2
        env.skip_3.push(true)
        i = i + 1

    // PASS 1: Predeclare type names
    pass1_predeclare(env, nodes)

    // PASS 2: Register field/method types
    pass2_register(env, nodes)

    // PASS 3: Re-register for forward references
    pass3_re_register(env, nodes)

    // PASS 4: Full expression typecheck
    let typed_items: Array<TypedItem> = pass4_check(env, nodes)

    return TypedProgram {
        items: typed_items,
        env: env,
        errors: env.errors,
    }

pub fn pass1_predeclare(env: *mut TypeEnv, nodes: Array<AstNode>):
    // Iterate nodes, find struct/enum/trait/world/actor/component items
    // Register them as empty type shells
    var i: Int = 0
    while i < len(nodes):
        let node: AstNode = nodes[i]
        let kind: Int = node.kind

        if kind == AST_ITEM_STRUCT:
            // Register struct shell
            let name_idx: Int = ast_data_get(node, 0)
            let name: String = ""  // resolve from string table
            let shell: ResolvedType = zero_type()
            shell.kind = RT_STRUCT
            shell.name = name_idx
            env.types.insert(name, shell)

        elif kind == AST_ITEM_ENUM:
            // Register enum shell
            // ...

        elif kind == AST_ITEM_TRAIT:
            // Register trait metadata
            // ...

        // Stub: world/actor/component → Struct shells
        elif kind == AST_ITEM_WORLD or kind == AST_ITEM_ACTOR or kind == AST_ITEM_COMPONENT:
            // Register as struct shell (stub strategy)
            pass

        i = i + 1

pub fn pass2_register(env: *mut TypeEnv, nodes: Array<AstNode>):
    // Resolve field types, variant payload types, method signatures
    // Mark items that fail as skip_2[i] = false
    var i: Int = 0
    while i < len(nodes):
        let node: AstNode = nodes[i]
        let kind: Int = node.kind

        if kind == AST_ITEM_STRUCT:
            // Resolve field types
            // If any field type not found → skip_2[i] = false; emit error
            pass
        elif kind == AST_ITEM_IMPL:
            // Register method signatures
            pass
        elif kind == AST_ITEM_ENUM:
            // Resolve variant payload types
            pass

        i = i + 1

pub fn pass3_re_register(env: *mut TypeEnv, nodes: Array<AstNode>):
    // Single retry for items that failed Pass 2
    var i: Int = 0
    while i < len(nodes):
        if !env.skip_2[i]:
            // Try registering again (types may have been registered later in Pass 2)
            // If still fails → skip_3[i] = false
            env.skip_3[i] = false
        i = i + 1

pub fn pass4_check(env: *mut TypeEnv, nodes: Array<AstNode>) -> Array<TypedItem>:
    // Full expression typecheck for items that passed Pass 2 and Pass 3
    let mut items: Array<TypedItem> = empty_array()

    var i: Int = 0
    while i < len(nodes):
        if env.skip_2[i] and env.skip_3[i]:
            let node: AstNode = nodes[i]
            let typed: TypedItem = check_item(env, node, i)
            items.push(typed)
        i = i + 1

    return items
```

**Acceptance Criteria:**
- [ ] Pass 1 registers struct/enum/trait/world/actor/component as empty shells
- [ ] Pass 2 resolves field/variant types and marks failures in skip_2
- [ ] Pass 3 retries once for forward references (no fixpoint)
- [ ] Pass 4 typechecks all expressions for passed items
- [ ] Forward reference pattern works: struct A { b: B }; struct B { a: A } — Pass 2 fails for one, Pass 3 resolves

---

### FOXTROT-04: Expression Typecheck (`check_expr`, `check_item`)

**Effort:** 1.5h
**Objective:** Implement type inference for all expression and statement kinds the self-host compiler uses.

Key functions:
- `check_expr(env, node, idx) -> ResolvedType` — dispatch on node.kind
- `check_item(env, node, idx) -> TypedItem` — typecheck a top-level item

Coverage: AST_EXPR_INT→I64, AST_EXPR_FLOAT→F64, AST_EXPR_BOOL→Bool, AST_EXPR_STRING→String, AST_EXPR_NONE→Option(Unknown), AST_EXPR_BINARY→arithmetic/comparison/logic, AST_EXPR_IF→branch unification, AST_EXPR_MATCH→arm unification, AST_EXPR_CALL→param/return matching, AST_EXPR_BLOCK→last expr type, AST_EXPR_IDENT→scope lookup, AST_EXPR_ASSIGN→target/expr compatibility, AST_EXPR_STRUCT_LIT→field-by-field, AST_EXPR_FIELD→field type lookup, AST_EXPR_REF/DEREF, AST_EXPR_CAST, AST_EXPR_LAMBDA, AST_EXPR_ARRAY/TUPLE

---

### FOXTROT-05: Effect Checking (`effects.kn`)

**Effort:** 1h
**Objective:** Implement the 4-rule `can_call()` lattice and effect violation checking.

**Implementation:**

Create `X:\blades\kain\src\effects.kn`:

```kn
// effects.kn — Effect checking lattice
// STREAM: FOXTROT

use src::types::{EFF_PURE, EFF_IO, EFF_GPU, EFF_ASYNC, EFF_REACTIVE, EFF_UNSAFE, EFF_ALLOC, EFF_PANIC}

pub fn effect_from_str(name: String) -> Int:
    if name == "Pure":     return EFF_PURE
    if name == "IO":       return EFF_IO
    if name == "GPU":      return EFF_GPU
    if name == "Async":    return EFF_ASYNC
    if name == "Reactive": return EFF_REACTIVE
    if name == "Unsafe":   return EFF_UNSAFE
    if name == "Alloc":    return EFF_ALLOC
    if name == "Panic":    return EFF_PANIC
    return EFF_PURE

// ── can_call(caller_effects, callee_effects) ──
// Lattice: Pure (bottom) < IO|GPU|Async|Reactive|Alloc|Panic < Unsafe (top)
// Rule 1: Pure callee → anyone can call
// Rule 2: Pure caller → can only call Pure
// Rule 3: Unsafe caller → can call anything
// Rule 4: callee effects ⊆ caller effects (subset check)
pub fn can_call(caller_effects: Int, callee_effects: Int) -> Bool:
    // Rule 1: Pure callee — always callable
    if callee_effects == EFF_PURE:
        return true

    // Rule 3: Unsafe caller — can call anything
    if (caller_effects and EFF_UNSAFE) != 0:
        return true

    // Rule 2: Pure caller — can only call Pure
    if caller_effects == EFF_PURE:
        return false

    // Rule 4: callee ⊆ caller (the bits of callee must be a subset of caller's bits)
    let intersection: Int = caller_effects and callee_effects
    return intersection == callee_effects

// ── Check effect at a call site ──
pub fn check_effect_call(caller: Int, callee: Int, caller_name: String,
                          callee_name: String, span_start: Int) -> Bool:
    if can_call(caller, callee):
        return true
    // Effect violation
    // Emit ERR_TYPE_EFFECT_VIOLATION diagnostic
    return false

// ── Auto-emit effects for pulse/resonate bodies ──
pub fn pulse_body_effects() -> Int:
    return EFF_PURE or EFF_IO or EFF_GPU or EFF_ASYNC or EFF_REACTIVE or EFF_UNSAFE or EFF_ALLOC or EFF_PANIC
```

**Acceptance Criteria:**
- [ ] `can_call()` implements all 4 rules correctly
- [ ] Pure callee always callable
- [ ] Pure caller rejects non-Pure callees
- [ ] Unsafe caller accepts everything
- [ ] Subset check for intermediate effects (IO can call IO; IO cannot call GPU)

---

### FOXTROT-06: Stub Strategy for Layers 1-7 (`types.kn`, part 4)

**Effort:** 0.5h
**Objective:** Implement the stub strategy that treats Layer 1-7 constructs as simplified Layer 0 equivalents.

Key stubs:
- `world` → Struct(name, state_fields)
- `actor` → Struct(name, state_fields); `on` handlers → fn signatures
- `component` → Struct(name, prop_fields); skip JSX/render validation
- `patch` → typecheck body as fn; any return type
- `law` → typecheck body as fn; enforce return type Bool
- `converge` → typecheck lanes as fn; skip selector/match/verify
- `orchestrate` → typecheck stage bodies as expressions; skip graph validation
- `pulse/resonate` → typecheck body as block expr
- `axiom/shatter/teleport` → parse and store; skip all semantic validation

---

### FOXTROT-07: Generic Monomorphization (`monomorphize.kn`)

**Effort:** 1h
**Objective:** Implement `unify()`, `substitute_type()`, and `instantiate_generic()`.

**Implementation:**

Create `X:\blades\kain\src\monomorphize.kn`:

```kn
// monomorphize.kn — Generic monomorphization
// STREAM: FOXTROT

use src::types::{ResolvedType, RT_GENERIC, RT_UNKNOWN, type_env_get, zero_type}

// ── unify(param_type, arg_type, bindings) ──
// Bind generic type parameter names to concrete types
pub fn unify(param_type: ResolvedType, arg_type: ResolvedType,
             bindings: *mut HashMap<String, ResolvedType>) -> Bool:
    if param_type.kind == RT_GENERIC:
        let gen_name: String = ""  // resolve from string table via param_type.name
        if bindings.has(gen_name):
            // Already bound — check compatibility
            let existing: ResolvedType = bindings.get(gen_name)
            return types_compatible(existing, arg_type)
        else:
            // Bind generic to concrete type
            bindings.insert(gen_name, arg_type)
            return true
    return types_compatible(param_type, arg_type)

// ── substitute_type(ty, bindings) ──
// Replace generic type references with concrete types
pub fn substitute_type(ty: ResolvedType, bindings: HashMap<String, ResolvedType>) -> ResolvedType:
    if ty.kind == RT_GENERIC:
        let gen_name: String = ""  // resolve from string table
        if bindings.has(gen_name):
            return bindings.get(gen_name)
    // Compound types: recursively substitute inner types
    let mut result: ResolvedType = ty
    if ty.kind == RT_OPTION or ty.kind == RT_ARRAY or ty.kind == RT_SLICE:
        let inner: ResolvedType = type_env_get(/* env */, ty.inner_type)
        let substituted: ResolvedType = substitute_type(inner, bindings)
        result.inner_type = /* index of substituted */
    // ... similar for other compound types
    return result

// ── Monomorphized Program ──
pub struct MonomorphizedProgram:
    items: Array<TypedItem>

pub fn monomorphize(env: *mut TypeEnv, typed: TypedProgram) -> MonomorphizedProgram:
    // Find all generic function calls
    // For each call with concrete types:
    //   1. unify(param_type, arg_type) → bindings
    //   2. substitute_type() for each instantiation
    //   3. Create monomorphized copy with mangled name
    let mut items: Array<TypedItem> = typed.items
    return MonomorphizedProgram { items: items }
```

**Acceptance Criteria:**
- [ ] `unify()` binds generic names to concrete types
- [ ] Conflicting bindings produce error
- [ ] `substitute_type()` replaces generics with concrete types
- [ ] Compound types (Option, Array, Result) recursively substituted
- [ ] `monomorphize()` produces MonomorphizedProgram output

---

### FOXTROT-08: Test Specification (`spec/typechecker_spec.md`)

**Effort:** 0.5h
**Objective:** Write test cases for the typechecker covering all compatibility rules.

Create `X:\blades\kain\spec\typechecker_spec.md` with test cases for:
- types_compatible() pairwise for all 20 variants
- 4-pass pipeline (forward reference resolution)
- Effect checking (4 rules + violations)
- Generic unify/substitute
- Stub strategy verification

---

## Stream Conventions

- **Language:** Pure Kain Layer 0 (fn, struct, enum, let, while, if, match, return)
- **Naming:** snake_case; `type_*` prefix for ResolvedType constructors; `check_*` for typecheck functions
- **Imports:** Import AST_* constants from `ast.kn`; import error constants from `error.kn`; import builtin lists from `builtins.kn`
- **Error handling:** Accumulate in `DiagnosticBag` via `type_error(env, msg, kind, span)`. Skip propagation: items failing early passes are excluded from later passes.
- **Testing:** Write test cases BEFORE or alongside implementation

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT implement codegen — that's GOLF's job
- ❌ Do NOT modify `ast.kn`, `token.kn`, `error.kn` — those are read-only imports
- ❌ Do NOT use recursive types for AST or types — use flat arrays with integer indices
- ❌ Do NOT full-implement Layer 1-7 semantics — stub them only

---

## Verification (After This Stream)

```bash
# Check individual files
kain check X:\blades\kain\src\types.kn
kain check X:\blades\kain\src\effects.kn
kain check X:\blades\kain\src\monomorphize.kn

# Typecheck together
kain check X:\blades\kain\src\types.kn X:\blades\kain\src\effects.kn X:\blades\kain\src\monomorphize.kn

# Run typechecker tests
kain test X:\blades\kain\spec\typechecker_spec.md
```

**Self-check:**
- [ ] All 3 files created
- [ ] 20 ResolvedType variants defined
- [ ] types_compatible() handles all 20 variants
- [ ] 4-pass pipeline with skip vectors
- [ ] can_call() 4-rule lattice correct
- [ ] Stub strategy for all 8 Layer 1-7 constructs
- [ ] unify() and substitute_type() working
- [ ] Forward references resolve via Pass 3 re-register

---

## Completion Report

When done, report:
- Files created: types.kn, effects.kn, monomorphize.kn — with line counts
- ResolvedType variants: 20
- Pipeline passes: 4 (predeclare, register, re-register, check)
- Effect rules: 4
- Stub constructs: 8
- Any issues encountered
- Whether GOLF can safely start codegen
