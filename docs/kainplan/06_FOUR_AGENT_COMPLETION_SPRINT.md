# KAIN Four-Agent Completion Sprint

> **Date:** February 20, 2026
> **Sprint Goal:** Push KAIN from ~65% to ~95% kainplan completion
> **Agents:** 4 parallel, fully independent assignments
> **Estimated Time:** 2-4 hours per agent
> **Priority Order:** All 4 agents can run simultaneously — zero dependencies between them

---

## Current State Summary

| Feature Area | Current % | Target % | Agent |
|---|---|---|---|
| Generics & Monomorphization | 90% | 100% | Agent 1 |
| Pattern Matching UE5 Codegen | 50% | 95% | Agent 2 |
| Traits & UE5 UInterface | 5% | 85% | Agent 3 |
| Stdlib Wiring + FluidFlow Oracle Fixes | 65% | 95% | Agent 4 |

---

## AGENT 1 — Generics Completion + Parser `>>` Fix

### Objective
Close the single remaining generics failure: the `>>` token ambiguity that breaks nested generic parsing (`Box<Box<Int>>`), and ensure `MonomorphizedProgram` flows natively through the UE5 backend instead of being cast back.

### Known State
- **1 failing test:** `test_nested_generic_types` in `crates/kain-core/tests/monomorphize_test.rs:355`
- **Error:** `Parser { message: "Expected Comma, got Colon" }` when parsing `Box<Box<Int>>`
- **Root cause:** The lexer tokenizes `>>` as a single right-shift token instead of two `>` closing angle brackets
- **Backend casting:** `crates/cli/src/lib.rs` casts `MonomorphizedProgram` back to `TypedProgram` before passing to UE5 backend (TODO comment exists)

### Files to Modify

#### 1. `crates/kain-core/src/lexer.rs`
Find where `>>` is tokenized as `RightShift`. Add context-aware heuristic: if we are inside a generic argument list (tracked by `<` depth counter), tokenize `>>` as TWO separate `>` tokens instead of one `>>`.

```
Search for: ">>" | "RightShift" | TokenKind::RightShift
Strategy: Add a `generic_depth: usize` counter to lexer state.
          Increment on `<` when parsing type context.
          When `>>` is seen and generic_depth >= 2, emit two `>` tokens.
```

#### 2. `crates/kain-core/src/parser.rs` — `parse_type()` function
When parsing type arguments inside `<...>`, track depth so the lexer hint is correct. Alternatively, handle the `>>` split at the parser level: when expecting `>` and encountering `>>`, consume only one `>` and leave a synthetic `>` for the next close.

Search for the generic type parsing: `fn parse_type` or wherever `<` and `>` are consumed for generics.

#### 3. `crates/cli/src/lib.rs` — Remove TypedProgram cast
Find this block (around line 54-70):
```rust
// Convert MonomorphizedProgram back to TypedProgram for now
// TODO: Update generate() to accept MonomorphizedProgram directly
let typed_for_codegen = TypedProgram { items: mono_ast.items };
```
Change UE5 backend call to pass `mono_ast` directly. Update `crates/ue5/src/codegen_ue5.rs::generate()` signature to accept `&MonomorphizedProgram`.

#### 4. `crates/ue5/src/codegen_ue5.rs` — `generate()` function (line ~45)
Update signature from `TypedProgram` to `MonomorphizedProgram`. The internal logic stays the same since `MonomorphizedProgram.items` is `Vec<TypedItem>` — same type.

### Acceptance Criteria
- [ ] `cargo test test_nested_generic_types` → PASS
- [ ] `cargo test` → 31/31 passing (was 30/31)
- [ ] `cargo build --release` → clean
- [ ] `kain build --ue5` on a `.kn` file with `Box<Box<Int>>` → no parser error

---

## AGENT 2 — Pattern Matching UE5 Backend Codegen

### Objective
Implement `match` expression code generation in `codegen_ue5.rs`. The parser already produces full `Expr::Match` / `TypedExpr::Match` AST nodes. The UE5 backend currently **does not handle them** — they silently fall through to an empty/default arm or are dropped entirely.

### Known State
- **Parser:** Complete — `parse_match`, `parse_pattern`, `Pattern::Variant(name, bindings)`, `Pattern::Ident`, `Pattern::Wildcard`, `Pattern::Literal` all exist
- **Runtime:** Complete — `crates/kain-core/src/runtime.rs` has full pattern matching
- **UE5 backend:** Zero match handling — search `crates/ue5/src/codegen_ue5.rs` for `Match` to confirm

### Files to Modify

#### 1. `crates/ue5/src/codegen_ue5.rs` — Main expression generator
Find the large function that generates C++ from typed expressions (look for `TypedExpr::Call`, `TypedExpr::If`, `TypedExpr::Field` match arms). Add a new arm:

```rust
TypedExpr::Match { scrutinee, arms, .. } => {
    generate_match_expression(scrutinee, arms, ctx, indent)
}
```

#### 2. Create `fn generate_match_expression()` in `codegen_ue5.rs`

Map KAIN patterns to C++ as follows:

| KAIN Pattern | C++ Output Strategy |
|---|---|
| `Pattern::Wildcard(_)` | `else { ... }` / default case |
| `Pattern::Literal(val)` | `if (scrutinee == val)` |
| `Pattern::Ident(name)` | `auto name = scrutinee; if (true)` (binding) |
| `Pattern::Variant(name, [])` | `if (scrutinee == EEnumName::name)` |
| `Pattern::Variant(name, bindings)` | `if (scrutinee.IsA<FVariantName>()) { auto b0 = scrutinee.Get<FVariantName>().field0; ... }` |
| `Pattern::Struct { fields }` | Destructure: `auto field = scrutinee.field;` for each |

**C++ emit strategy — use if/else chains, NOT switch:**
```cpp
// KAIN: match solver_family:
//   SolverFamily.NavierStokes => use_ns()
//   SolverFamily.SPH => use_sph()
//   _ => use_default()

// Generated C++:
if (solver_family == ESolverFamily::NavierStokes) {
    use_ns();
} else if (solver_family == ESolverFamily::SPH) {
    use_sph();
} else {
    use_default();
}
```

**For enum variants with bindings:**
```cpp
// KAIN: match result:
//   Ok(value) => process(value)
//   Err(msg) => log(msg)

// Generated C++:
if (result.IsOk()) {
    auto value = result.GetOkValue();
    process(value);
} else if (result.IsErr()) {
    auto msg = result.GetErrValue();
    log(msg);
}
```

#### 3. `crates/kain-core/src/parser.rs` — `parse_impl()` trait_name fix
While you're in the parser, fix the `impl Trait for Type` parsing (line ~174):

```rust
// CURRENT (always None):
Ok(Item::Impl(Impl {
    trait_name: None,  // <-- WRONG
    ...
}))

// FIX: parse optional "TraitName for" before target_type:
// After consuming `impl` and generics, peek:
// If next token is an Ident followed by `for` keyword,
// consume trait_name and `for`, then parse target_type normally
```

Search for `fn parse_impl` at line 174 in `crates/kain-core/src/parser.rs`.

### Acceptance Criteria
- [ ] A `.kn` file with a `match` expression on an enum generates valid C++ if/else chain
- [ ] Wildcard `_` arm generates `else { }` block
- [ ] Enum variant match without bindings: `SolverFamily.SPH => ...` generates `if (x == ESolverFamily::SPH)`
- [ ] Enum variant with binding: `Ok(v) => ...` generates binding variable
- [ ] `cargo build --release` → clean
- [ ] Add at least 2 tests to `crates/kain-core/tests/` verifying generated C++ contains correct pattern output

---

## AGENT 3 — Trait System + UE5 UInterface Codegen

### Objective
Implement the full trait-to-UInterface pipeline. This is the most complex agent task. KAIN traits need to generate UE5's dual-class interface system (`ITraitName` + `UTraitName`).

### Known State
- `crates/ue5/src/ue5/traits.rs` — EXISTS but is a 18-line stub with `// Reserved for future`
- `crates/kain-core/src/ast.rs:304-332` — `Trait` and `TraitMethod` AST nodes are complete
- `crates/kain-core/src/parser.rs:174` — `parse_impl()` hardcodes `trait_name: None`
- `crates/ue5/src/codegen_ue5.rs` — No trait or UInterface generation anywhere

### Files to Modify

#### Step 1: Fix Parser — `crates/kain-core/src/parser.rs` line 174
Parse `impl TraitName for TypeName` syntax. After consuming `impl` and optional generics:

```rust
fn parse_impl(&mut self) -> KainResult<Item> {
    // ... parse generics ...
    
    // NEW: check for "TraitName for" pattern
    let trait_name = if self.peek_is_ident() && self.peek_ahead_is_keyword("for") {
        let name = self.expect_ident()?;
        self.expect_keyword("for")?;
        Some(name)
    } else {
        None
    };
    
    let target_type = self.parse_type()?;
    // ... rest unchanged ...
    Ok(Item::Impl(Impl { trait_name, target_type, ... }))
}
```

#### Step 2: Implement `crates/ue5/src/ue5/traits.rs` — Replace stub

Implement `generate_trait_header(trait_def: &Trait, module_name: &str) -> String`:

```cpp
// KAIN: trait Damageable:
//     fn take_damage(self, amount: Float) -> Bool

// Generated MyPlugin.h:
UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface {
    GENERATED_BODY()
};

class MYPLUGIN_API IDamageable {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damageable")
    bool TakeDamage(float amount);
    virtual bool TakeDamage_Implementation(float amount) { return false; }
};
```

Implement `generate_trait_impl_methods(impl_block: &Impl, trait_def: &Trait) -> String`:
- For each method in the impl block, generate `bool ClassName::TakeDamage_Implementation(float amount) { ... }`

Implement `generate_class_interface_list(impls: &[Impl], all_traits: &HashMap<String, Trait>) -> String`:
- Returns `", public IDamageable, public ISimulatable"` etc. for the UCLASS inheritance line

#### Step 3: Wire into `crates/ue5/src/codegen_ue5.rs`

In `generate_filtered()` (line ~118), after generating structs/actors, add a pass:

```rust
// Generate trait interface headers
for item in &program.items {
    if let TypedItem::Trait(trait_def) = item {
        let trait_header = traits::generate_trait_header(trait_def, module_name);
        output.push_header_file(format!("I{}.h", trait_def.ast.name), trait_header);
    }
}

// Wire trait impls into class declarations
for item in &program.items {
    if let TypedItem::Impl(impl_block) = item {
        if let Some(ref trait_name) = impl_block.trait_name {
            // Add interface to target class's inheritance list
            ctx.register_trait_impl(&impl_block.target_type_name, trait_name);
        }
    }
}
```

In actor/struct header generation, use `ctx.get_trait_impls(class_name)` to add interface inheritance.

#### Step 4: Add to `Ue5Context` — `crates/ue5/src/ue5/context.rs`

Add `trait_impls: HashMap<String, Vec<String>>` — maps class name to list of implemented trait names.

```rust
pub fn register_trait_impl(&mut self, class_name: &str, trait_name: &str) {
    self.trait_impls
        .entry(class_name.to_string())
        .or_default()
        .push(trait_name.to_string());
}

pub fn get_interface_list(&self, class_name: &str) -> String {
    match self.trait_impls.get(class_name) {
        Some(traits) => traits.iter()
            .map(|t| format!(", public I{}", t))
            .collect::<Vec<_>>()
            .join(""),
        None => String::new(),
    }
}
```

### Example KAIN → UE5 Mapping

```kain
trait Simulatable:
    fn step(self, dt: Float)
    fn reset(self)

impl Simulatable for HyperFluidSimulationCore:
    fn step(self, dt: Float):
        self.advance(dt)
    fn reset(self):
        self.clear_all()
```

**Generated `ISimulatable.h`:**
```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class USimulatable : public UInterface { GENERATED_BODY() };

class FLUIDFLOW_API ISimulatable {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void Step(float dt);
    virtual void Step_Implementation(float dt) {}

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void Reset();
    virtual void Reset_Implementation() {}
};
```

**Generated `UHyperFluidSimulationCore` class header (modified):**
```cpp
UCLASS()
class FLUIDFLOW_API UHyperFluidSimulationCore : public UActorComponent, public ISimulatable {
    // ...
    virtual void Step_Implementation(float dt) override;
    virtual void Reset_Implementation() override;
};
```

### Acceptance Criteria
- [ ] `parse_impl` correctly populates `trait_name: Some("TraitName")` for `impl T for S`
- [ ] A `.kn` file with a `trait` definition generates `ITraitName.h` and `UTraitName` headers
- [ ] A `.kn` file with `impl Trait for Type` generates `_Implementation` method stubs in `.cpp`
- [ ] The UCLASS inheriting the trait includes `, public ITraitName` in its header
- [ ] `cargo build --release` → clean
- [ ] Add 3 tests verifying trait header generation output

---

## AGENT 4 — Stdlib Wiring + Oracle Fixes + FluidFlow Source Fixes

### Objective
Three tasks: (1) Wire `StdLibResolver` into the actual expression generation path so stdlib calls emit correct UE5 code, (2) Fix the Oracle's false-positive errors (Vec3 and enum RPC serialization), (3) Fix the FluidFlow `.kn` source architectural violations so `kain build --ue5` passes.

### Task A: Wire StdLibResolver into Codegen

#### Current State
`crates/ue5/src/ue5/stdlib_resolver.rs` exists with 47+ mapped functions. BUT it is never called during expression codegen. The resolver exists as a standalone struct but the main `codegen_ue5.rs` expression generator does NOT consult it when it sees a function call.

#### Fix — `crates/ue5/src/codegen_ue5.rs`

Find the expression generator where `TypedExpr::Call { func, args }` is handled. Currently it likely emits the function name directly. Change it to:

```rust
TypedExpr::Call { func, args, .. } => {
    let arg_strings: Vec<String> = args.iter()
        .map(|a| generate_typed_expr(a, ctx))
        .collect();
    
    // Try stdlib resolver first
    if let Some(resolved) = ctx.stdlib_resolver.resolve(&func_name, &arg_strings) {
        resolved
    } else {
        // Fall back to direct call
        format!("{}({})", func_name, arg_strings.join(", "))
    }
}
```

Add `stdlib_resolver: StdLibResolver` to `Ue5Context` in `crates/ue5/src/ue5/context.rs` and initialize it in `Ue5Context::new()`.

#### Task B: Fix Oracle False Positives

**File:** Find the Oracle serialization checker — run:
```
rg -rn "cannot be used in RPC" crates/
```

**Fix 1: Vec3 → serializable**
In the Oracle's `is_rpc_serializable()` or `is_datatable_serializable()` function, add `Vec3` to the allowed types whitelist since `Vec3` maps to `FVector` which IS serializable in UE5:

```rust
fn is_serializable_type(ty: &Type) -> bool {
    match type_name.as_str() {
        // Primitive types
        "Int" | "Float" | "Bool" | "String" => true,
        // KAIN stdlib types that map to UE5 serializable types
        "Vec2" | "Vec3" | "Vec4" => true,  // → FVector2D, FVector, FVector4
        // ... existing entries ...
    }
}
```

**Fix 2: User-defined enums → RPC serializable**
User-defined enums (like `VisualizationMode`, `QualityTier`) ARE serializable in UE5 RPCs. The Oracle needs to check if the type is a known enum from the program's type registry, and if so, allow it:

```rust
// In Oracle RPC validation:
if ctx.is_known_enum(&param_type) {
    continue; // Enums are serializable
}
```

Add `is_known_enum(&str) -> bool` to `Ue5Context` that checks if a type name exists in the registered enums map.

#### Task C: Fix FluidFlow `.kn` Source

**File:** `unreal/plugins/FluidFlow/HyperFluidDynamics_EXPANDED.kn`

**Fix 1: Remove `HyperFluidSimulationCore` from RPC parameters (lines 1261, 1265, 1284, 1291, 1295, 1299)**

`HyperFluidEmitter` and `HyperFluidProbe` need a `state world` field. Then remove `world` from their RPC signatures. The actors already know about each other through the world context.

```kain
# BEFORE (broken - can't pass UObject in RPC):
actor HyperFluidEmitter:
    on Server_Emit(world: HyperFluidSimulationCore, count: Int):
        SmoothedParticleHydrodynamics.Emit(world.particles, ...)

# AFTER (correct - world is actor state):
actor HyperFluidEmitter:
    state world: HyperFluidSimulationCore = null
    on Server_Emit(count: Int):
        SmoothedParticleHydrodynamics.Emit(self.world.particles, ...)
```

Apply same pattern to `HyperFluidProbe` (lines 1284, 1291, 1295, 1299).

**Fix 2: FluidPreset DataTable — pointer fields (lines 596, 599)**

`Array<CouplingField>` and `AdaptiveStrategy` are flagged as pointer/non-flat types in a `@datatable` struct. `FluidPreset` is already missing its `@datatable` attribute — check if it actually needs to be a DataTable row. If not, remove `@datatable`. If yes, convert:

```kain
# BEFORE:
struct FluidPreset:
    coupling: Array<CouplingField>   # ❌ dynamic array in DataTable
    adaptive: AdaptiveStrategy       # ❌ check if this is a pointer

# AFTER option A — remove @datatable if it's not needed as DataTable
struct FluidPreset:   # (no @datatable)
    ...

# AFTER option B — flatten for DataTable compatibility
@datatable
struct FluidPreset:
    coupling_flags: Int    # bitmask instead of array
    adaptive: AdaptiveStrategy  # if this is an enum, it's fine
```

Run `rg "@datatable" HyperFluidDynamics_EXPANDED.kn` to verify which structs have the attribute.

### Acceptance Criteria
- [ ] `kain build --ue5` in `unreal/plugins/FluidFlow/` produces 0 Oracle errors (down from 25)
- [ ] A `.kn` file calling `abs(-1.0)` generates `FMath::Abs(-1.0f)` in C++
- [ ] A `.kn` file calling `sqrt(x)` generates `FMath::Sqrt(x)` in C++
- [ ] A `.kn` file calling `len(arr)` generates `arr.Num()` in C++
- [ ] `VisualizationMode` and `QualityTier` no longer flagged as non-serializable in RPCs
- [ ] `Vec3` fields in `@datatable` structs no longer flagged
- [ ] `cargo build --release` → clean

---

## Integration Checklist (Run After All 4 Agents Complete)

```bash
# 1. Full build — must be clean
cargo build --release

# 2. All tests — must be 31/31
cargo test

# 3. FluidFlow build — must be 0 Oracle errors
cd unreal/plugins/FluidFlow
kain build --ue5

# 4. Verify traits in generated output
grep -r "ISimulatable\|UINTERFACE" unreal/plugins/FluidFlow/Source/

# 5. Verify stdlib in generated output
grep -r "FMath::" unreal/plugins/FluidFlow/Source/FluidFlow/Private/

# 6. Verify match expressions in generated output
grep -r "else if\|switch" unreal/plugins/FluidFlow/Source/FluidFlow/Private/
```

---

## File Ownership Map (No Conflicts Between Agents)

| Agent | Primary Files | No Touch |
|---|---|---|
| Agent 1 | `lexer.rs`, `parser.rs` (>>), `cli/src/lib.rs`, `codegen_ue5.rs::generate()` sig | Never touch `traits.rs`, `stdlib_resolver.rs` |
| Agent 2 | `codegen_ue5.rs` (match emit), `parser.rs` (impl trait_name only) | Never touch `lexer.rs`, `stdlib_resolver.rs` |
| Agent 3 | `traits.rs`, `context.rs`, `codegen_ue5.rs` (trait wiring), `parser.rs` (parse_impl) | Never touch `lexer.rs`, `stdlib_resolver.rs` |
| Agent 4 | `stdlib_resolver.rs`, `oracle.rs`/`oracle_enhanced.rs`, `HyperFluidDynamics_EXPANDED.kn`, `context.rs` (stdlib_resolver field) | Never touch `lexer.rs`, `traits.rs` |

> ⚠️ **Agents 1, 2, and 3 all touch `parser.rs`** — each must edit a different function:
> - Agent 1: `parse_type()` or lexer `>>` handling
> - Agent 2: No `parser.rs` changes needed (match parsing already works)
> - Agent 3: `parse_impl()` only — the `trait_name: None` line

---

## Expected Impact After Sprint

| Metric | Before | After |
|---|---|---|
| Kainplan completion | ~65% | ~95% |
| Tests passing | 30/31 | 31/31 |
| FluidFlow Oracle errors | 25 | 0 |
| Stdlib calls generating correct UE5 | 0% | 100% |
| Trait polymorphism in generated code | 0% | 85% |
| Match expressions in generated code | 0% | 90% |
| `>>` nested generics | Broken | Fixed |

**After this sprint, a single `.kn` source file generates a UE5 plugin with:**
- ✅ Correct polymorphic interfaces (traits → UInterface)
- ✅ Game logic state machines (match → if/else chains)
- ✅ Working stdlib calls (sqrt, abs, len, push, etc. → FMath/TArray)
- ✅ Zero Oracle errors on FluidFlow
- ✅ Nested generic types (`Box<Box<Int>>`)
