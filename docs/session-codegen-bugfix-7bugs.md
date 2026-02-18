# Codegen Bug-Fix Session — 7 ToonShaderz Bugs

**Date:** 2026-02-18  
**Branch:** kain-private/kain  
**Scope:** Compiler codegen fixes for `ToonShaderz` UE5 plugin  

---

## Bugs Fixed

| ID | Description | Files Changed |
|----|-------------|---------------|
| BUG-001 | `shader vertex` emitted `SF_Pixel` in `IMPLEMENT_GLOBAL_SHADER` | `crates/ue5-shaders/src/codegen_usf.rs` |
| BUG-002 | `shader vertex` generated single PS class instead of `F{Name}VS` | `crates/ue5-shaders/src/codegen_usf.rs` |
| BUG-003 | Vertex USF return missing `mul(..., View.WorldToClip)` | `crates/ue5-shaders/src/codegen_usf.rs` |
| BUG-005 | `@replicated` on actor state not generating `GetLifetimeReplicatedProps` | `ast.rs`, `parser.rs`, `codegen_ue5.rs` |
| BUG-006 | `@category` on actor state ignored in `UPROPERTY` | `ast.rs`, `parser.rs`, `codegen_ue5.rs` |
| BUG-007 | `YourPlugin` placeholder in generated `.h` comment | `crates/ue5-shaders/src/codegen_usf.rs` |
| BUG-008 | `@blueprint` fn calls emitted as bare free functions in actor methods | `crates/ue5/src/codegen_ue5.rs` |

---

## Root Cause Summary

**BUG-001/002/007:** `codegen_usf.rs` had a single `is_compute: bool` flag with no `is_vertex` path. All non-compute shaders fell through to pixel shader defaults for class naming, `IMPLEMENT_GLOBAL_SHADER` frequency/entry suffix, and Exec() generation.

**BUG-003:** The vertex return emission at `Stmt::Return` in the USF emitter wrote `Output.Position = EXPR` directly with no clip-space transformation. Vertex shader outputs require `mul(position, View.WorldToClip)` to be in NDC.

**BUG-005/006 (two-layer):**  
- Layer 1: `StateDecl` in `ast.rs` had no `attributes` field, causing `@replicated`/`@category` decorators to be silently discarded by the parser.  
- Layer 2: The actor state emitter in `codegen_ue5.rs` hardcoded `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Settings")` regardless of attributes, and never checked for `@replicated` to generate `GetLifetimeReplicatedProps`.

**BUG-008:** `Ue5Gen` had no record of which functions carried `@blueprint`. The `gen_expr` `Expr::Call` arm had no qualification logic, so all calls to blueprint utility functions emitted as unqualified bare calls instead of `U{Plugin}FunctionLibrary::fn(args)`.

---

## Key Changes

### `crates/kain-core/src/ast.rs`
```rust
pub struct StateDecl {
    pub name: String,
    pub ty: Type,
    pub initial: Expr,
    pub weak: bool,
    pub attributes: Vec<Attribute>,  // ADDED
    pub span: Span,
}
```

### `crates/kain-core/src/parser.rs`
- 3 actor `StateDecl` construction sites: pass `method_attributes`
- 3 React-component `StateDecl` construction sites: pass `vec![]`

### `crates/ue5-shaders/src/codegen_usf.rs`
- `is_compute` → `(is_compute, is_vertex)` tuple in both `generate_cpp_header_cached` and `generate_cpp_implementation_cached`
- Added `plugin_name: &str` param to `generate_cpp_header_cached`, threaded from `compile_shader_artifacts`
- `Output.Position = EXPR` → `Output.Position = mul(EXPR, View.WorldToClip)`

### `crates/ue5/src/codegen_ue5.rs`
- Added `blueprint_fn_names: HashSet<String>` + `module_name: String` to `Ue5Gen`
- Pre-pass: collect `@blueprint` fn names from `TypedItem::Function`
- Actor state loop: inline attribute-aware `UPROPERTY` generation + `has_replicated_state` gate for `GetLifetimeReplicatedProps` header decl + source impl with `DOREPLIFETIME`
- `gen_expr` `Expr::Call`: qualify blueprint fn calls before fallthrough

---

## Validation Proof

### `cargo test` (all crates)
```
test result: ok. 66 passed; 0 failed  (kain-core)
test result: ok. 26 passed; 0 failed  (ue5)
test result: ok. 10 passed; 0 failed  (ue5-shaders)
test result: ok.  3 passed; 0 failed  (cli)
test result: ok.  2 passed; 0 failed  (kain-runtime)
TOTAL: 107 passed, 0 failed
```

### `kain build --ue5` — ToonShaderz
```
✅ Plugin build complete!
📍 Location: M:\UnrealEngine\Plugins_Kn\ToonShaderz
⚡ Total shaders: 12
```

### Generated Output Spot-Checks

**ToonHullOutline.h (BUG-001/002):**
```cpp
class FToonHullOutlineVS : public FGlobalShader
// IMPLEMENT_GLOBAL_SHADER(FToonHullOutlineVS, "/Plugin/ToonShaderz/ToonHullOutline.usf", "ToonHullOutlineVS", SF_Vertex);
```

**ToonHullOutline.usf (BUG-003):**
```hlsl
Output.Position = mul(float4(extruded, 1.000000), View.WorldToClip);
```

**AToonDirector.h (BUG-005/006):**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Simulation Settings")
EToonStyle active_style;
virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
```

**AToonDirector.cpp (BUG-005/008):**
```cpp
void AToonDirector::GetLifetimeReplicatedProps(...) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AToonDirector, active_style);
    // ... 34 more fields
}
void AToonDirector::SetQuality(const EQualityTier tier) {
    shadow_steps = UToonShaderzFunctionLibrary::ToonShadowSteps(tier);
    outline_thickness = UToonShaderzFunctionLibrary::ToonOutlineThickness(tier);
}
```
