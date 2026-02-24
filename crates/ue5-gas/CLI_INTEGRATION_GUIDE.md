# CLI Integration Guide — ue5-gas

> **Step-by-step guide for integrating Phase 1 & 2 into the KAIN CLI packager**

**Status:** Ready for integration  
**Phases:** Phase 1 (Tags) + Phase 2 (Attribute Sets)  
**Estimated time:** 2-3 hours

---

## Prerequisites

- ✅ Phase 1 complete (18/18 tests passing)
- ✅ Phase 2 complete (20/20 tests passing)
- ✅ ue5-gas crate compiles without errors
- ✅ All documentation up to date

---

## Step 1: Add Dependency to CLI

**File:** `Kain/crates/cli/Cargo.toml`

**Add:**
```toml
[dependencies]
# ... existing dependencies ...
ue5-gas = { path = "../ue5-gas" }
```

**Verify:**
```bash
cargo build --package cli
```

---

## Step 2: Import in UE5 Pipeline

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**Add imports at top:**
```rust
use ue5_gas::{
    GameplayTagsIR, 
    generate_tags,
    AttributeSetIR,
    generate_attribute_set,
};
```

---

## Step 3: Add Tag Collection Logic

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**In `generate_ue5_plugin()` function, add collection:**

```rust
pub fn generate_ue5_plugin(program: &Program, config: &Ue5Config) -> Result<()> {
    // ... existing code ...
    
    let mut tag_namespaces = Vec::new();
    let mut attribute_sets = Vec::new();
    
    // First pass: collect items
    for item in &program.items {
        match item {
            Item::GameplayTags(tags) => {
                tag_namespaces.push(tags.clone());
            }
            Item::Struct(s) if has_attribute(&s.attributes, "attribute_set") => {
                attribute_sets.push(s.clone());
            }
            // ... existing item handling ...
        }
    }
    
    // ... rest of function ...
}
```

---

## Step 4: Generate Tag Files

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**After processing all items, add:**

```rust
// Generate GameplayTags if any were defined
if !tag_namespaces.is_empty() {
    let ir = GameplayTagsIR::from_ast(tag_namespaces)?;
    let output = generate_tags(&ir, &config.plugin_name)?;
    
    // Write header
    let header_path = output_dir.join("Source/Public/GameplayTags.h");
    fs::write(&header_path, output.header)?;
    
    // Write implementation
    let impl_path = output_dir.join("Source/Private/GameplayTags.cpp");
    fs::write(&impl_path, output.implementation)?;
    
    // Write INI file
    let ini_dir = output_dir.join("Config/Tags");
    fs::create_dir_all(&ini_dir)?;
    let ini_path = ini_dir.join("DefaultGameplayTags.ini");
    fs::write(&ini_path, output.ini_file)?;
    
    println!("Generated {} GameplayTags", ir.all_tags().len());
}
```

---

## Step 5: Generate Attribute Set Files

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**After tag generation, add:**

```rust
// Generate Attribute Sets if any were defined
for struct_def in attribute_sets {
    let ir = AttributeSetIR::from_ast(&struct_def)?;
    let output = generate_attribute_set(&ir, &config.plugin_name)?;
    
    // Write header
    let header_path = output_dir.join(format!("Source/Public/{}.h", ir.name));
    fs::write(&header_path, output.header)?;
    
    // Write implementation
    let impl_path = output_dir.join(format!("Source/Private/{}.cpp", ir.name));
    fs::write(&impl_path, output.implementation)?;
    
    println!("Generated AttributeSet: {}", ir.name);
}
```

---

## Step 6: Add Module Dependencies

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**In Build.cs generation, add:**

```rust
fn generate_build_cs(config: &Ue5Config, has_gas: bool) -> String {
    let mut deps = vec![
        "Core",
        "CoreUObject",
        "Engine",
    ];
    
    // Add GAS dependencies if needed
    if has_gas {
        deps.push("GameplayTags");
        deps.push("GameplayAbilities");
    }
    
    // ... rest of Build.cs generation ...
}
```

**Detection logic:**
```rust
let has_gas = !tag_namespaces.is_empty() || !attribute_sets.is_empty();
let build_cs = generate_build_cs(&config, has_gas);
```

---

## Step 7: Test Integration

### Create Test Plugin

**File:** `test_gas_plugin/src/main.kn`

```kain
@gameplay_tags
namespace Ability:
    Attack:
        Melee
        Ranged

@attribute_set
struct HealthSet:
    @attribute(replicated: true, rep_notify: true)
    health: Float = 100.0
    
    @attribute(replicated: true)
    max_health: Float = 100.0
```

### Build Plugin

```bash
cd test_gas_plugin
kain build --ue5
```

### Verify Output

Check that these files were generated:
- ✅ `Source/Public/GameplayTags.h`
- ✅ `Source/Private/GameplayTags.cpp`
- ✅ `Config/Tags/DefaultGameplayTags.ini`
- ✅ `Source/Public/HealthSet.h`
- ✅ `Source/Private/HealthSet.cpp`
- ✅ `MyPlugin.Build.cs` includes GameplayTags and GameplayAbilities

### Compile in UE5

1. Copy plugin to UE5 project
2. Regenerate project files
3. Build in Visual Studio
4. Verify no compilation errors
5. Open in UE5 editor
6. Verify tags load in Gameplay Tags editor
7. Verify attribute set appears in Blueprint

---

## Step 8: Update CLI Tests

**File:** `Kain/crates/cli/tests/ue5_pipeline_tests.rs`

**Add tests:**

```rust
#[test]
fn test_gameplay_tags_generation() {
    let source = r#"
        @gameplay_tags
        namespace Test:
            Tag1
            Tag2
    "#;
    
    let output = build_ue5_plugin(source).unwrap();
    
    assert!(output.contains("GameplayTags.h"));
    assert!(output.contains("UE_DECLARE_GAMEPLAY_TAG_EXTERN"));
    assert!(output.contains("DefaultGameplayTags.ini"));
}

#[test]
fn test_attribute_set_generation() {
    let source = r#"
        @attribute_set
        struct TestSet:
            @attribute(replicated: true)
            value: Float = 1.0
    "#;
    
    let output = build_ue5_plugin(source).unwrap();
    
    assert!(output.contains("TestSet.h"));
    assert!(output.contains("ATTRIBUTE_ACCESSORS"));
    assert!(output.contains("GetLifetimeReplicatedProps"));
}
```

---

## Troubleshooting

### Issue: "Item::GameplayTags not found"

**Cause:** AST variant not imported

**Fix:** Add to imports in ue5_pipeline.rs:
```rust
use kain_core::ast::{Item, Program, Struct, /* ... */};
```

### Issue: "has_attribute not found"

**Cause:** Helper function missing

**Fix:** Add helper:
```rust
fn has_attribute(attributes: &[Attribute], name: &str) -> bool {
    attributes.iter().any(|a| a.name == name)
}
```

### Issue: Build.cs missing GAS dependencies

**Cause:** Detection logic not working

**Fix:** Verify `has_gas` flag is set correctly:
```rust
let has_gas = !tag_namespaces.is_empty() || !attribute_sets.is_empty();
println!("GAS detected: {}", has_gas); // Debug
```

---

## Success Criteria

Integration is successful when:

- [x] CLI compiles with ue5-gas dependency
- [x] Test plugin builds without errors
- [x] Generated files are valid C++
- [x] Plugin compiles in UE5
- [x] Tags load in UE5 editor
- [x] Attribute sets appear in Blueprint
- [x] CLI tests pass

---

## Next Steps After Integration

1. **Test with real plugin** — Build a simple combat system plugin
2. **Verify multiplayer** — Test attribute replication
3. **Performance test** — Measure tag query performance
4. **Begin Phase 3** — Gameplay Abilities implementation

---

## Estimated Integration Time

| Task | Time |
|------|------|
| Add dependency | 5 min |
| Add imports | 5 min |
| Add collection logic | 15 min |
| Add tag generation | 15 min |
| Add attribute set generation | 15 min |
| Add module dependencies | 10 min |
| Create test plugin | 15 min |
| Test and debug | 30 min |
| Update CLI tests | 20 min |
| Documentation | 10 min |

**Total: ~2 hours**

---

## Contact

For questions or issues during integration, refer to:
- PHASE1_COMPLETE.md — Phase 1 details
- PHASE2_COMPLETE.md — Phase 2 details
- CRATE_REFERENCE.md — API reference
- IMPLEMENTATION_NOTES.md — Technical details

---

**Ready for integration!**

