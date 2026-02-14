# UHT Pre-Validation System — "The Oracle"

> **Date:** February 13, 2026  
> **Status:** Fully implemented, tested (69/69), integrated into build pipeline  
> **Impact:** Catches UHT errors in ~10ms instead of waiting 2-5 minutes for UE5 compilation

---

## Overview

The Oracle is KAIN's semantic validation layer that runs **before** C++ code generation. It enforces Unreal Header Tool (UHT) rules at KAIN compile time, preventing errors that would otherwise only surface during the UE5 build process.

The system operates in two phases:

1. **Phase 1 — Hardcoded Rules:** Hand-written validation for function specifiers, RPC naming, actor/struct/enum naming conventions, replication constraints, and engine name collisions.
2. **Phase 2 — Data-Driven UHT Rules:** 337 validation rules extracted directly from Epic's `EpicGames.UHT` C# source code, loaded from `uht_rules.json` at build time.

```
.kn source
    │
    ▼
  Parser → AST → Type Checker
    │
    ▼
  Oracle Phase 1: Hardcoded rules (naming, RPCs, specifier conflicts)
    │
    ▼
  Oracle Phase 2: Data-driven UHT rules (uht_rules.json)
    │                                      ▲
    │                          ┌────────────┘
    │                          │
    │              unreal/metadata/uht_rules.json
    │              (337 rules, 154 specifiers, 41 prop types, 25 combos)
    │                          ▲
    │                          │
    │              unreal/scripts/uht_extractor.py
    │                          ▲
    │                          │
    │              D:\Unreal\UE_5.7\Engine\Source\Programs\
    │                Shared\EpicGames.UHT\ (133 .cs files)
    │
    ▼
  C++ Codegen (errors already caught — UHT finds nothing to complain about)
```

---

## Architecture

### Source Files

| File | Purpose |
|------|---------|
| `crates/ue5/src/ue5/oracle.rs` | Main validation module — Phase 1 + Phase 2 orchestration |
| `crates/ue5/src/ue5/uht_rules.rs` | UHT rules schema, JSON loader, and query API |
| `crates/ue5/src/ue5/context.rs` | `Ue5Context` — loads `uht_rules.json` alongside other metadata |
| `crates/cli/src/packager/ue5_pipeline.rs` | Build pipeline — calls `validate_program_full()` with UHT rules |
| `unreal/scripts/uht_extractor.py` | Python extractor that scans Epic's UHT C# source |
| `unreal/metadata/uht_rules.json` | Extracted rules (361 KB) |

### Entry Points

```rust
// Simple validation (loads EngineKnowledge + empty UhtRules)
oracle::validate_program(&typed_program)

// With explicit EngineKnowledge (loads empty UhtRules)
oracle::validate_program_with_knowledge(&typed_program, &kb)

// Full validation with all data sources (used by ue5_pipeline.rs)
oracle::validate_program_full(&typed_program, &kb, &uht)
```

---

## Phase 1: Hardcoded Rules

These rules are hand-written in `oracle.rs` based on known UHT behavior and past compilation failures.

### Function Rules (UFUNCTION)

| Rule | Source | What It Catches |
|------|--------|-----------------|
| BlueprintImplementableEvent + Replicated | `UhtFunctionSpecifiers.cs` | `Server_`/`Client_`/`Multicast_` functions with `@blueprint_implementable_event` |
| BlueprintNativeEvent + Replicated | `UhtFunctionSpecifiers.cs` | Same, with `@blueprint_native_event` |
| BlueprintImplementable + BlueprintNative | `UhtFunctionSpecifiers.cs` | Both on same function |
| Exec + Replicated | `UhtFunctionSpecifiers.cs` | `@exec` functions with RPC naming |
| Private + BlueprintEvent | `UhtFunctionSpecifiers.cs` | Private visibility on blueprint events |
| BlueprintEvent + BlueprintGetter | `UhtFunctionSpecifiers.cs` | Conflicting function roles |
| RigVM + Parameters | UE 5.2+ restriction | `@rigvm_method` functions with params |
| Replicated + Delegate params | `UhtDelegateProperty.cs:158` | Delegate types in RPC parameters |

### Actor Rules

| Rule | What It Catches |
|------|-----------------|
| Naming prefix (A) | Empty or numeric actor names |
| BlueprintImplementable + BlueprintNative | Both on same actor |
| Handler RPC + BlueprintEvent | `on Server_X()` handlers with blueprint event attributes |
| Method-level specifier conflicts | All function rules applied to actor methods |

### Struct Rules

| Rule | What It Catches |
|------|-----------------|
| Naming prefix (F) | Empty or too-short struct names |
| Engine name collision | Names that collide with UE5 engine types (via EngineKnowledge) |
| Field validation | All property rules applied to struct fields |

### Enum Rules

| Rule | What It Catches |
|------|-----------------|
| Naming prefix (E) | Empty or too-short enum names |
| `true`/`false` variants | Case-insensitive — UE5 rejects these |
| Missing `_MAX` variant | Warning — Blueprint metadata stability |
| Engine name collision | Names that collide with UE5 engine types |

### Property Rules (UPROPERTY)

| Rule | What It Catches |
|------|-----------------|
| Struct member + Replicated | `@replicated` on struct fields (only valid on class members) |
| BlueprintReadOnly + BlueprintSetter | Conflicting access specifiers |

### Component Rules

| Rule | What It Catches |
|------|-----------------|
| Naming prefix (U) | Empty or too-short component names |
| State field validation | All property rules applied to component state |

### Engine Name Collision Detection

Uses `EngineKnowledge` (21,134 types) to check all prefixed variants:

```rust
kb.is_known_type(name)              // "Player"
kb.is_known_type(&format!("A{}", name))  // "APlayer"
kb.is_known_type(&format!("U{}", name))  // "UPlayer"
kb.is_known_type(&format!("F{}", name))  // "FPlayer"
kb.is_known_type(&format!("E{}", name))  // "EPlayer"
kb.resolve_type_alias(name)              // "Vec3" → "FVector"
```

---

## Phase 2: Data-Driven UHT Rules

### Extraction Pipeline

The Python extractor (`unreal/scripts/uht_extractor.py`) performs 5 passes over Epic's UHT C# source:

1. **LogError/LogWarning extraction** — Captures all validation rule messages with context (file, line, category)
2. **[UhtSpecifier] attribute extraction** — Parses specifier definitions with their `applies_to` type
3. **Property type extraction** — Identifies property types and their constraints
4. **Incompatible combo extraction** — Finds specifier pairs that UHT rejects together
5. **KAIN categorization** — Maps UHT rules to KAIN construct types

### Extracted Data (`uht_rules.json`)

| Category | Count | Description |
|----------|-------|-------------|
| Validation rules | 337 (321 errors, 16 warnings) | All `LogError`/`LogWarning` calls from UHT |
| Specifier definitions | 154 | 47 class, 22 function, 44 property, 9 struct, 3 enum, etc. |
| Property type definitions | 41 | With container flags and constraints |
| Incompatible combinations | 25 | Specifier pairs that UHT rejects |

### UHT Source Location

```
D:\Unreal\UE_5.7\Engine\Source\Programs\Shared\EpicGames.UHT\
├── Specifiers/
│   ├── UhtClassSpecifiers.cs        (47 class specifiers)
│   ├── UhtFunctionSpecifiers.cs     (22 function specifiers)
│   ├── UhtPropertyMemberSpecifiers.cs (44 property specifiers)
│   ├── UhtStructSpecifiers.cs       (9 struct specifiers)
│   └── UhtEnumSpecifiers.cs         (3 enum specifiers)
├── Types/
│   ├── UhtClass.cs
│   ├── UhtFunction.cs
│   ├── UhtProperty.cs
│   ├── UhtStruct.cs
│   └── UhtEnum.cs
└── ... (133 files total, 47,849 lines)
```

### Re-Extraction

When upgrading to a new UE version (e.g., 5.8), re-run the extractor:

```bash
cd kain
python unreal/scripts/uht_extractor.py "D:\Unreal\UE_5.8\Engine\Source\Programs\Shared\EpicGames.UHT"
```

This regenerates `unreal/metadata/uht_rules.json`. **Zero Rust code changes needed.**

---

## UhtRules Query API

The `UhtRules` struct in `uht_rules.rs` provides the following query methods:

### Specifier Validation

```rust
// Check if a specifier is valid for a type
uht.is_valid_specifier("EditAnywhere", "property")  // true
uht.is_valid_specifier("EditAnywhere", "class")      // false

// Check if a specifier exists at all
uht.is_known_specifier("BlueprintReadWrite")  // true
uht.is_known_specifier("MadeUpThing")         // false

// Get all valid specifiers for a type
uht.specifiers_for("class")  // ["NoExport", "Intrinsic", "Abstract", ...]
```

### Incompatible Combination Detection

```rust
// Check if two specifiers conflict
uht.are_incompatible("BlueprintReadOnly", "BlueprintReadWrite")
// → Some("Cannot specify a property as being both BlueprintReadOnly and BlueprintReadWrite")

// Get all incompatible partners for a specifier
uht.incompatible_with("BlueprintReadOnly")
// → [("BlueprintReadWrite", "Cannot specify..."), ...]
```

### Container Type Detection

```rust
// Check if a type is a container (for nested container validation)
uht.is_container_type("Array")     // true
uht.is_container_type("Map")       // true
uht.is_container_type("FString")   // false
```

### Property Constraints

```rust
// Get constraints for a property type
uht.property_type_constraints("Map")
// → ["Nested containers not supported", ...]
```

### Rule Search

```rust
// Full-text search across all rules
uht.search_rules("replicated")
// → [UhtValidationRule { message: "Struct members cannot be replicated", ... }, ...]

// Get rules for a specific KAIN construct
uht.rules_for_kain_construct("actor")
// → [UhtValidationRule { ... }, ...]
```

### Diagnostics

```rust
// Check if data was loaded
uht.is_loaded()  // true/false

// Get counts
uht.stats()  // (337, 154) — (total_rules, total_specifiers)
```

---

## Phase 2 Validation Rules

These rules are enforced when `uht_rules.json` is loaded:

### Nested Container Detection

UHT rejects nested containers like `TMap<FString, TArray<int>>`. The oracle catches this at KAIN compile time:

```kn
# ❌ This will be caught by the oracle
struct BadData:
    items: Map<String, Array<Int>>
    # Error: Nested containers are not supported by UHT.
    # 'Map<..., Array<...>>' will fail UE5 compilation. Use a wrapper struct instead.

# ✅ Fix: Use a wrapper struct
struct ItemList:
    values: Array<Int>

struct GoodData:
    items: Map<String, ItemList>
```

### Specifier Compatibility

The oracle checks all attribute pairs against the 25 extracted incompatible combinations:

```kn
# ❌ Caught by oracle
struct PlayerData:
    @blueprint_read_only
    @blueprint_read_write
    health: Float
    # Error: Cannot specify a property as being both BlueprintReadOnly and BlueprintReadWrite (UHT rule)
```

### Struct Member Restrictions

```kn
# ❌ Caught by oracle
struct ItemData:
    @blueprint_setter
    name: String
    # Error: Cannot specify BlueprintSetter for a struct member.
    # This is only valid on class (actor/component) members. (UHT rule)
```

### Editor-Only + Blueprint Exposed

```kn
# ❌ Caught by oracle
struct ConfigData:
    @blueprint_read_write
    @editor_only
    debug_value: Float
    # Error: Blueprint exposed struct members cannot be editor only. (UHT rule)
```

---

## Pipeline Integration

### Build Pipeline (`ue5_pipeline.rs`)

```rust
// 1. Load EngineKnowledge (21,134 types)
let kb = EngineKnowledge::new();

// 2. Load UHT rules (337 rules from uht_rules.json)
let mut uht = UhtRules::new();
if let Ok(data) = std::fs::read_to_string("unreal/metadata/uht_rules.json") {
    let _ = uht.load(&data);
}

// 3. Run full validation (Phase 1 + Phase 2)
oracle::validate_program_full(&typed_program, &kb, &uht)?;

// 4. If we get here, code is clean — proceed to C++ codegen
```

### Ue5Context Integration

`UhtRules` is also loaded into `Ue5Context` for access during codegen:

```rust
pub struct Ue5Context {
    pub knowledge: EngineKnowledge,    // 21,134 engine types
    pub resolver: StdLibResolver,       // Type resolution
    pub uht_rules: UhtRules,           // 337 UHT validation rules
    // ...
}
```

---

## Known UE5 Compilation Errors (Historical)

These errors were encountered during development and informed the oracle's rule set:

| # | Error | Cause | Oracle Coverage |
|---|-------|-------|-----------------|
| 1 | `C2079: uses undefined class 'XXX_API_API'` | Double API macro suffix | Codegen fix (not oracle) |
| 2 | `C2572: default argument redefinition` | Default values in both .h and .cpp | Codegen fix |
| 3 | `Undeclared identifier 'PrimaryActorTick'` | Cascading from #1 | Fixed by #1 |
| 4 | `C2440: cannot convert FVector3f to FVector` | Float vs double vectors | Codegen fix (forced double) |
| 5 | `C2079: 'GraphBuilder' uses undefined class` | Missing RDG includes | Codegen fix (auto-includes) |
| 6 | `C2440: FSceneRenderTargetItem conversion` | UE5.4 API change | Codegen fix |
| 7 | `C2660: argument mismatch` | UE5.4 signature change | Codegen fix |

---

## Test Coverage

### Unit Tests (8 tests in `uht_rules.rs`)

- `test_uht_rules_default` — Empty rules struct
- `test_uht_rules_load_minimal` — Minimal JSON loading
- `test_specifier_lookup` — Specifier validation queries
- `test_incompatible_combos` — Specifier conflict detection
- `test_container_types` — Container type identification
- `test_property_constraints` — Property constraint lookup
- `test_kain_rules` — KAIN construct rule mapping
- `test_search_rules` — Full-text rule search

### Integration Tests (in `oracle.rs`)

- `test_validation_context` — Error/warning tracking
- Full build verification on SlateTest4 and CorpusTest plugins

### Total: 69/69 tests passing

---

## Future Work

- **Shader compiler metadata extraction** from `D:\Unreal\UE_5.7\Engine\Source\Developer` — contains the entire shader compiler, high-value target for shader validation rules
- **Specifier auto-suggestion** — When an invalid specifier is used, suggest the closest valid one from the 154 known specifiers
- **Per-field UHT error codes** — Map oracle errors to specific UHT error codes for cross-referencing with Epic's documentation
- **LSP integration** — Surface oracle errors as real-time diagnostics in the editor via the KAIN Language Server
