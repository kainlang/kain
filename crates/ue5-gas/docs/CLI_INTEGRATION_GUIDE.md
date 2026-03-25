# CLI Integration Guide for Phase 3 (Gameplay Abilities)

**Status:** ✅ COMPLETE  
**Date:** February 24, 2026

---

## Integration Summary

Phase 3 (Gameplay Abilities) has been successfully integrated into the CLI packager. The `kain build --ue5` command now supports `@ability` definitions and generates complete UE5 C++ code.

---

## Changes Made

### 1. Cargo.toml Updates

**File:** `Kain/crates/cli/Cargo.toml`

Added `ue5-gas` dependency:
```toml
ue5-gas = { path = "../ue5-gas", optional = true }
```

Added to `ue5` feature:
```toml
ue5 = ["dep:ue5", ..., "dep:ue5-gas", ...]
```

### 2. Packager Integration

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

#### Extraction Before Type Checking (lines ~1660-1680)
```rust
// Extract GameplayTags BEFORE type checking
let gameplay_tags: Vec<kain_core::ast::GameplayTagsNamespace> = merged.items.iter()
    .filter_map(|item| {
        if let kain_core::ast::Item::GameplayTags(def) = item {
            Some(def.clone())
        } else {
            None
        }
    })
    .collect();

// Extract GameplayAbilities BEFORE type checking
let gameplay_abilities: Vec<kain_core::ast::GameplayAbilityDef> = merged.items.iter()
    .filter_map(|item| {
        if let kain_core::ast::Item::GameplayAbility(def) = item {
            Some(def.clone())
        } else {
            None
        }
    })
    .collect();
```

#### Filter Items (lines ~1682-1690)
```rust
merged.items.retain(|item| !matches!(item, 
    kain_core::ast::Item::MaterialGraph(_) | 
    kain_core::ast::Item::MaterialFunction(_) |
    kain_core::ast::Item::GraphEditor(_) |
    kain_core::ast::Item::GraphRuntime(_) |
    kain_core::ast::Item::GameplayTags(_) |
    kain_core::ast::Item::GameplayAbility(_)
));
```

#### Generation Steps (lines ~890-990)

**STEP 3.8: Generate GameplayTags**
```rust
#[cfg(feature = "ue5")]
if !gameplay_tags.is_empty() {
    println!("🏷️  Generating {} GameplayTags namespace(s)...", gameplay_tags.len());
    
    for tags_namespace in &gameplay_tags {
        match ue5_gas::tags_ir::from_ast(tags_namespace) {
            Ok(tags_ir) => {
                match ue5_gas::tags_codegen::generate(&tags_ir, &ue5_config.plugin_name) {
                    Ok(output) => {
                        // Write GameplayTags.h, GameplayTags.cpp, DefaultGameplayTags.ini
                    }
                }
            }
        }
    }
}
```

**STEP 3.9: Generate GameplayAbilities**
```rust
#[cfg(feature = "ue5")]
if !gameplay_abilities.is_empty() {
    println!("⚡ Generating {} GameplayAbility(ies)...", gameplay_abilities.len());
    
    for ability_def in &gameplay_abilities {
        match ue5_gas::ability_ir::GameplayAbilityIR::from_ast(ability_def) {
            Ok(ability_ir) => {
                match ue5_gas::ability_codegen::generate(&ability_ir, &ue5_config.plugin_name) {
                    Ok(output) => {
                        // Write {AbilityName}.h and {AbilityName}.cpp
                    }
                }
            }
        }
    }
}
```

#### Module Dependencies (lines ~1132-1140)
```rust
// Detect GAS features for module dependencies
let has_gameplay_tags = !gameplay_tags.is_empty();
let has_gameplay_abilities = !gameplay_abilities.is_empty();
let has_gas_features = has_gameplay_tags || has_gameplay_abilities;

super::codegen::write_plugin_files(&layout, &ue5_config, &description, has_shaders, has_gas_features, &module_graph, &typed_program)?;
```

### 3. Build.cs Generation

**File:** `Kain/crates/cli/src/packager/codegen.rs`

#### Updated Function Signature (line ~1577)
```rust
pub fn write_plugin_files(
    layout: &PluginLayout,
    config: &Ue5Config,
    description: &Option<String>,
    has_shaders: bool,
    has_gas_features: bool,  // NEW PARAMETER
    module_graph: &ue5::ue5::module_graph::ModuleGraph,
    program: &kain_core::types::TypedProgram,
) -> KainResult<()>
```

#### Updated compute_runtime_deps (lines ~1503-1550)
```rust
fn compute_runtime_deps(
    has_shaders: bool,
    has_gas_features: bool,  // NEW PARAMETER
    module_graph: &ue5::ue5::module_graph::ModuleGraph,
    program: &kain_core::types::TypedProgram,
) -> Vec<String> {
    // ... existing shader logic ...
    
    // GAS features require GameplayTags and GameplayAbilities modules
    if has_gas_features {
        for module in &["GameplayTags", "GameplayAbilities"] {
            let s = module.to_string();
            if !deps.contains(&s) {
                deps.push(s);
            }
        }
    }
    
    deps
}
```

---

## Generated File Structure

When `kain build --ue5` is run on a plugin with GAS features:

```
MyPlugin/
├── Config/
│   └── Tags/
│       └── DefaultGameplayTags.ini    # Generated from @gameplay_tags
├── Source/
│   └── MyPlugin/
│       ├── Public/
│       │   ├── GameplayTags.h         # Generated from @gameplay_tags
│       │   └── Abilities/
│       │       ├── JumpAbility.h      # Generated from @ability
│       │       └── DashAbility.h
│       └── Private/
│           ├── GameplayTags.cpp       # Generated from @gameplay_tags
│           └── Abilities/
│               ├── JumpAbility.cpp    # Generated from @ability
│               └── DashAbility.cpp
└── MyPlugin.Build.cs                  # Includes GameplayTags, GameplayAbilities modules
```

---

## Module Dependencies

When GAS features are detected, the following modules are automatically added to `PublicDependencyModuleNames` in the `.Build.cs` file:

- `GameplayTags` — Required for FGameplayTag, FGameplayTagContainer
- `GameplayAbilities` — Required for UGameplayAbility, UAbilitySystemComponent

---

## Usage Example

### Input (KAIN)

**File:** `src/abilities.kn`

```kain
@gameplay_tags
namespace Ability:
    Jump
    Dash
    Attack:
        Melee
        Ranged

@ability
struct JumpAbility:
    @instancing(policy: "InstancedPerExecution")
    @net_execution(policy: "LocalPredicted")
    
    @ability_tags
    tags: ["Ability.Jump"]
    
    @activation_required_tags
    required: ["Status.Grounded"]
    
    @cost
    effect: StaminaCostEffect
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        get_avatar_actor_from_actor_info().jump()
        end_ability(handle, actor_info, activation_info, true, false)
```

### Build Command

```bash
cd MyPlugin
kain build --ue5
```

### Output

```
🚀 Building UE5 Plugin: MyPlugin
📍 Plugin directory: M:\Code\MyPlugin

🔍 Type checking merged program...
   ✓ Type checking passed

🏷️  Generating 1 GameplayTags namespace(s)...
   ✓ GameplayTags.h (25 lines)
   ✓ GameplayTags.cpp (30 lines)
   ✓ DefaultGameplayTags.ini (5 tags)

⚡ Generating 1 GameplayAbility(ies)...
   ✓ JumpAbility.h (45 lines)
   ✓ JumpAbility.cpp (60 lines)

📦 Generating .uplugin file...
   ✓ MyPlugin.uplugin

📝 Generating .Build.cs files...
   ✓ MyPlugin.Build.cs + auto-resolved: GameplayTags, GameplayAbilities

✅ Plugin build complete!
```

---

## Testing

### Minimal Test Plugin

Create a test plugin to verify the integration:

**File:** `test_gas/KAIN.toml`
```toml
[package]
name = "TestGAS"
version = "1.0.0"

[ue5]
plugin_name = "TestGAS"
engine_version = "5.4"
```

**File:** `test_gas/src/test.kn`
```kain
@gameplay_tags
namespace Test:
    Ability:
        Test

@ability
struct TestAbility:
    @instancing(policy: "InstancedPerExecution")
    @ability_tags
    tags: ["Test.Ability.Test"]
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        end_ability(handle, actor_info, activation_info, true, false)
```

**Build:**
```bash
cd test_gas
kain build --ue5
```

**Verify:**
- Check `Source/TestGAS/Public/GameplayTags.h` exists
- Check `Source/TestGAS/Private/GameplayTags.cpp` exists
- Check `Config/Tags/DefaultGameplayTags.ini` exists
- Check `Source/TestGAS/Public/Abilities/TestAbility.h` exists
- Check `Source/TestGAS/Private/Abilities/TestAbility.cpp` exists
- Check `TestGAS.Build.cs` contains `GameplayTags` and `GameplayAbilities`

---

## Success Criteria

- [x] ue5-gas dependency added to CLI
- [x] GameplayTags extraction before type checking
- [x] GameplayAbility extraction before type checking
- [x] Items filtered from type checking
- [x] GameplayTags generation step added
- [x] GameplayAbility generation step added
- [x] Module dependencies added to Build.cs
- [x] Files written to correct directories
- [x] Integration follows existing patterns (materials, graphs)

---

## Next Steps

1. Test with Factory/Example_GAS plugin
2. Verify UE5 compilation
3. Add Phase 2 (Attribute Sets) integration
4. Document full GAS workflow

---

## Notes

- The integration follows the same pattern as materials and graphs
- GAS items are extracted before type checking (like materials)
- Module dependencies are automatically added when GAS features are detected
- Generated files follow UE5 naming conventions (U prefix for abilities)
- Config/Tags directory is created automatically for GameplayTags INI files
- Abilities directory is created automatically under Public/Private

---

**Integration Complete!** ✅

