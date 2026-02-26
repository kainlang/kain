# ue5-gas Crate Reference

> **Gameplay Ability System (GAS) codegen backend for KAIN**

![Status](https://img.shields.io/badge/Phase%201-Complete-brightgreen)
![Tests](https://img.shields.io/badge/Tests-23%20Passing-brightgreen)
![Compression](https://img.shields.io/badge/Compression-1%3A10-blue)

---

## Overview

The `ue5-gas` crate provides KAIN language support for Unreal Engine 5's Gameplay Ability System. This is a **critical foundation** for multiplayer games, enabling:

- **GameplayTags** — Hierarchical string identifiers for state tracking, ability activation, effect application
- **Attribute Sets** — Replicated character stats (health, mana, stamina) with lifecycle hooks
- **Gameplay Abilities** — Player actions with tag requirements, costs, cooldowns
- **Gameplay Effects** — Stat modifications, buffs, debuffs, damage over time

**Why GAS Matters:**
- Every multiplayer game needs it
- Foundation for all gameplay systems (combat, abilities, status effects)
- Lyra (Epic's sample project) uses GAS extensively
- Massive market demand ($50-$300 plugins)

**Compression Ratio:** 1:10 average (1 line KAIN → 10 lines C++)

---

## Phase 1: GameplayTags (COMPLETE)

### What Are GameplayTags?

GameplayTags are **hierarchical string identifiers** that control every aspect of GAS:
- **Ability activation** — required tags, blocked tags
- **Effect application** — immunity, requirements, granted tags
- **Ability cancellation** — cancel tags, block tags
- **Cooldowns** — cooldown tags
- **State tracking** — status effects, buffs, debuffs
- **Gameplay cues** — visual/audio effects

**Format:** `"Parent.Child.Grandchild"` (dot-separated hierarchy)
**Storage:** Numeric IDs internally for fast comparison
**Matching:** Hierarchy-aware (`"Ability.Attack"` matches `"Ability.Attack.Melee"`)

### KAIN Syntax

```kain
@gameplay_tags
namespace Ability:
    Attack:
        Melee:
            Sword
            Axe
            Spear
        Ranged:
            Bow
            Gun
    Defend:
        Block
        Parry

@gameplay_tags
namespace Status:
    Alive
    Dead:
        Dying
        Dead
    CC:
        Stunned
        Rooted
        Silenced
```

### Generated Files

#### GameplayTags.h (Native C++ Declarations)

```cpp
#pragma once
#include "NativeGameplayTags.h"

namespace MyGameTags
{
    namespace Ability
    {
        namespace Attack
        {
            namespace Melee
            {
                MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sword);
                MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Axe);
            }
            namespace Ranged
            {
                MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bow);
            }
        }
    }
}
```

#### GameplayTags.cpp (Native C++ Definitions)

```cpp
#include "GameplayTags.h"

namespace MyGameTags
{
    namespace Ability
    {
        namespace Attack
        {
            namespace Melee
            {
                UE_DEFINE_GAMEPLAY_TAG(Sword, "Ability.Attack.Melee.Sword");
                UE_DEFINE_GAMEPLAY_TAG(Axe, "Ability.Attack.Melee.Axe");
            }
        }
    }
}
```

#### DefaultGameplayTags.ini (Designer-Friendly)

```ini
[/Script/GameplayTags.GameplayTagsList]
; Ability Tags
GameplayTagList=(Tag="Ability.Attack")
GameplayTagList=(Tag="Ability.Attack.Melee")
GameplayTagList=(Tag="Ability.Attack.Melee.Sword")
GameplayTagList=(Tag="Ability.Attack.Melee.Axe")

; Status Tags
GameplayTagList=(Tag="Status.Alive")
GameplayTagList=(Tag="Status.CC.Stunned")
```

### Usage in C++

```cpp
#include "GameplayTags.h"

void UMyAbility::ActivateAbility()
{
    // Use native tags for compile-time safety
    if (ASC->HasMatchingGameplayTag(MyGameTags::Status::CC::Stunned))
    {
        // Cannot activate while stunned
        return;
    }
    
    // Add activation owned tag
    ASC->AddLooseGameplayTag(MyGameTags::Status::Attacking);
}
```

---

## Architecture

### Crate Structure

```
ue5-gas/
├── src/
│   ├── lib.rs                  # Public API
│   ├── tags_ir.rs              # Tag IR (flatten hierarchy, validate)
│   └── tags_codegen.rs         # Tag codegen (C++ + INI)
├── tests/
│   ├── tags_tests.rs           # Unit tests (16 tests)
│   └── integration_test.rs     # Integration tests (2 tests)
├── examples/
│   └── test_tags.kn            # Example KAIN file
├── Cargo.toml
├── README.md
└── CRATE_REFERENCE.md          # This file
```

### Dependencies

```toml
[dependencies]
kain-core = { path = "../kain-core" }  # AST, parser, types
anyhow = "1.0"                          # Error handling
thiserror = "1.0"                       # Error types
```

### Data Flow

```
.kn source
    ↓
[Lexer] → Tokens
    ↓
[Parser] → AST (GameplayTagsNamespace)
    ↓
[tags_ir::from_ast] → GameplayTagsIR (flattened, validated)
    ↓
[tags_codegen::generate] → TagCodegenOutput
    ↓
    ├─→ GameplayTags.h (native declarations)
    ├─→ GameplayTags.cpp (native definitions)
    └─→ DefaultGameplayTags.ini (designer-friendly)
```

---

## API Reference

### tags_ir Module

#### `GameplayTagsIR`

Main IR structure containing all tag namespaces.

```rust
pub struct GameplayTagsIR {
    pub namespaces: Vec<TagNamespaceIR>,
}

impl GameplayTagsIR {
    /// Convert AST to IR
    pub fn from_ast(namespaces: Vec<GameplayTagsNamespace>) -> Result<Self>
    
    /// Get all tags as flat list
    pub fn all_tags(&self) -> Vec<&GameplayTagIR>
    
    /// Get tags for specific namespace
    pub fn get_namespace(&self, name: &str) -> Option<&TagNamespaceIR>
}
```

#### `GameplayTagIR`

Individual tag with metadata.

```rust
pub struct GameplayTagIR {
    pub tag: String,              // Full path: "Ability.Attack.Melee"
    pub comment: Option<String>,  // Optional comment
    pub parent: Option<String>,   // Parent tag: "Ability.Attack"
    pub cpp_name: String,         // C++ identifier: "Ability_Attack_Melee"
}

impl GameplayTagIR {
    /// Get namespace path components
    pub fn namespace_parts(&self) -> Vec<String>
    
    /// Get leaf name (last component)
    pub fn leaf_name(&self) -> String
}
```

### tags_codegen Module

#### `generate()`

Main codegen entry point.

```rust
pub fn generate(ir: &GameplayTagsIR, plugin_name: &str) -> Result<TagCodegenOutput>
```

**Parameters:**
- `ir` — Tag IR from `GameplayTagsIR::from_ast()`
- `plugin_name` — Plugin name for API macro (e.g., "MyGame" → "MYGAME_API")

**Returns:** `TagCodegenOutput` with header, implementation, and INI file content

#### `TagCodegenOutput`

```rust
pub struct TagCodegenOutput {
    pub header: String,          // GameplayTags.h content
    pub implementation: String,  // GameplayTags.cpp content
    pub ini_file: String,        // DefaultGameplayTags.ini content
}
```

---

## Testing

### Run Tests

```bash
# All tests
cargo test -p ue5-gas

# Specific test file
cargo test -p ue5-gas --test tags_tests

# With output
cargo test -p ue5-gas -- --nocapture
```

### Test Coverage

**Unit Tests (tags_tests.rs):** 16 tests
- Tag hierarchy flattening
- Parent tag extraction
- Duplicate detection (within namespace)
- Duplicate detection (across namespaces)
- C++ name generation
- Native C++ header generation
- Native C++ implementation generation
- INI file generation
- Leaf name extraction
- Namespace parts extraction
- Complex hierarchies
- Empty namespaces

**Codegen Tests (tags_codegen.rs):** 5 tests
- Simple tag hierarchy
- Nested namespaces
- Multiple namespaces
- INI file format
- Complex hierarchy

**Integration Tests (integration_test.rs):** 2 tests
- End-to-end parse and generate
- Complex hierarchy parsing

**Total: 23 tests, all passing**

---

## Integration with CLI

### Packager Integration

Add to `cli/src/packager/ue5_pipeline.rs`:

```rust
use ue5_gas::{GameplayTagsIR, tags_codegen};

// In generate_ue5_plugin()
for item in &program.items {
    match item {
        Item::GameplayTags(tags) => {
            // Collect all tag namespaces
            tag_namespaces.push(tags.clone());
        }
        // ... other items
    }
}

// After processing all items
if !tag_namespaces.is_empty() {
    let ir = GameplayTagsIR::from_ast(tag_namespaces)?;
    let output = tags_codegen::generate(&ir, &plugin_name)?;
    
    // Write files
    write_file(
        output_dir.join("Source/Public/GameplayTags.h"),
        output.header
    )?;
    write_file(
        output_dir.join("Source/Private/GameplayTags.cpp"),
        output.implementation
    )?;
    write_file(
        output_dir.join("Config/Tags/DefaultGameplayTags.ini"),
        output.ini_file
    )?;
}
```

### Module Dependencies

Add to generated `Build.cs`:

```cpp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameplayTags",      // CRITICAL for tags
    "GameplayAbilities", // CRITICAL for GAS
});
```

---

## Design Decisions

### Why Native Tags + INI?

**Native C++ tags:**
- Compile-time safety (typos caught at compile time)
- IDE autocomplete
- Refactoring support
- Fast access (no string lookup)
- Used in hot paths (ability activation, effect application)

**INI file tags:**
- Designer-friendly (no code changes)
- Hot-reload in editor
- Easy to organize by feature
- Version control friendly
- Used for content tags (weapon types, damage types)

**Both are generated** — developers get compile-time safety, designers get flexibility.

### Why Flatten Hierarchy in IR?

The AST preserves the nested structure from parsing, but the IR flattens it because:
1. **Validation** — Easier to check for duplicates across entire tag set
2. **Parent generation** — Automatically creates parent tags (`"Ability.Attack"` from `"Ability.Attack.Melee"`)
3. **Codegen** — Simpler to generate both flat (INI) and nested (C++) outputs from flat list

### Why BTreeMap for Namespace Grouping?

`BTreeMap` ensures **deterministic output order** (sorted by key). This is critical for:
- Consistent diffs in version control
- Predictable test assertions
- Readable generated code

---

## Examples

### Example 1: Simple Tags

**Input (KAIN):**
```kain
@gameplay_tags
namespace Status:
    Alive
    Dead
```

**Output (C++):**
```cpp
// GameplayTags.h
namespace MyGameTags
{
    namespace Status
    {
        MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alive);
        MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
    }
}

// GameplayTags.cpp
namespace MyGameTags
{
    namespace Status
    {
        UE_DEFINE_GAMEPLAY_TAG(Alive, "Status.Alive");
        UE_DEFINE_GAMEPLAY_TAG(Dead, "Status.Dead");
    }
}
```

### Example 2: Nested Hierarchy

**Input (KAIN):**
```kain
@gameplay_tags
namespace Damage:
    Physical:
        Slash
        Pierce
    Magical:
        Fire
        Ice
```

**Output (C++):**
```cpp
namespace MyGameTags
{
    namespace Damage
    {
        namespace Physical
        {
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slash);
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pierce);
        }
        namespace Magical
        {
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fire);
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ice);
        }
    }
}
```

### Example 3: Tags with Comments

**Input (KAIN):**
```kain
@gameplay_tags
namespace Status:
    CC:
        Stunned  # Character is stunned and cannot act
        Rooted   # Character is rooted in place
```

**Output (C++):**
```cpp
UE_DEFINE_GAMEPLAY_TAG_COMMENT(
    Stunned,
    "Status.CC.Stunned",
    "Character is stunned and cannot act"
);
```

**Note:** Comment parsing is not yet implemented in the parser. This is a future enhancement.

---

## Performance Characteristics

### Tag IR Construction
- **Time:** O(n) where n = total tags
- **Space:** O(n) for flattened list
- **Validation:** O(n) for duplicate detection (HashSet)

### Codegen
- **Time:** O(n * d) where d = average depth
- **Space:** O(n) for output strings
- **Grouping:** O(n log n) for BTreeMap sorting

### Generated Code Performance
- **Tag comparison:** O(1) (numeric ID comparison)
- **Container queries:** O(n) where n = container size
- **Memory:** 8 bytes per FGameplayTag, ~32 bytes per FGameplayTagContainer

---

## Best Practices

### Tag Naming Conventions

**Use clear hierarchies:**
```
✅ GOOD:
Ability.Attack.Melee.Sword
Status.CC.Stunned
Damage.Physical.Slash

❌ BAD:
Ability.SwordAttack
Status.Stun
Damage.Slash
```

**Use consistent prefixes:**
```
Ability.*       — Abilities
Status.*        — Character states
Effect.*        — Effect metadata
Damage.*        — Damage types
Cooldown.*      — Cooldown tags
Event.*         — Gameplay events
```

### Tag Organization

**Separate by feature:**
```kain
@gameplay_tags
namespace Ability:
    # Core abilities
    Attack:
        Melee
        Ranged
    Defend:
        Block

@gameplay_tags
namespace Status:
    # Character states
    Alive
    Dead
    CC:
        Stunned
```

**Use descriptive names:**
```
✅ GOOD:
Status.CC.Stunned
Status.Immune.Fire
Ability.ActivateFail.OnCooldown

❌ BAD:
Status.S
Status.IF
Ability.Fail
```

---

## Troubleshooting

### Duplicate Tag Error

**Error:** `Duplicate tag: Ability.Attack`

**Cause:** Same tag defined multiple times in hierarchy

**Fix:** Remove duplicate or rename one of the tags

### Parser Error: Expected 'namespace'

**Error:** `Expected 'namespace' keyword after @gameplay_tags`

**Cause:** Missing `namespace` keyword

**Fix:**
```kain
# ❌ BAD
@gameplay_tags
Ability:
    Attack

# ✅ GOOD
@gameplay_tags
namespace Ability:
    Attack
```

### Empty Namespace

**Warning:** Namespace has no tags

**Cause:** Empty namespace definition

**Fix:** Add at least one tag or remove the namespace

---

## Phase 2: Attribute Sets (COMPLETE)

### What Are Attribute Sets?

Attribute Sets are **replicated character stats** that define gameplay properties like health, mana, stamina, armor, etc. They are the foundation of GAS's stat system.

**Key Features:**
- Automatic replication with RepNotify
- Lifecycle hooks for validation and clamping
- Meta attributes for temporary calculations
- ATTRIBUTE_ACCESSORS macro generation
- Integration with Gameplay Effects

### KAIN Syntax

```kain
@attribute_set
struct HealthSet:
    @attribute(replicated: true, rep_notify: true, hide_from_modifiers: true)
    health: Float = 100.0
    
    @attribute(replicated: true, rep_notify: true)
    max_health: Float = 100.0
    
    @attribute(meta: true)
    incoming_damage: Float = 0.0
    
    fn post_gameplay_effect_execute():
        # Handle incoming damage meta attribute
        if incoming_damage > 0.0:
            health = health - incoming_damage
            incoming_damage = 0.0
```

### Generated Output

See [PHASE2_COMPLETE.md](PHASE2_COMPLETE.md) for full example output (63 lines C++ from 2 attributes).

### API Reference

#### `attribute_set_ir` Module

```rust
pub struct AttributeSetIR {
    pub name: String,
    pub attributes: Vec<AttributeIR>,
    pub lifecycle_hooks: LifecycleHooksIR,
    pub delegates: Vec<DelegateIR>,
}

pub struct AttributeIR {
    pub name: String,
    pub ty: Type,
    pub default_value: Option<String>,
    pub replicated: bool,
    pub rep_notify: bool,
    pub hide_from_modifiers: bool,
    pub is_meta: bool,
    pub clamp_min: Option<f32>,
    pub clamp_max: Option<f32>,
    pub category: String,
}
```

#### `attribute_set_codegen` Module

```rust
pub fn generate(ir: &AttributeSetIR, plugin_name: &str) -> Result<AttributeSetCodegenOutput>

pub struct AttributeSetCodegenOutput {
    pub header: String,          // .h file content
    pub implementation: String,  // .cpp file content
}
```

---

## Future Enhancements

### Phase 3: Gameplay Abilities (Next)
- `@ability` struct parsing
- Tag requirements (`@activation_required_tags`, `@activation_blocked_tags`)
- Cost/cooldown effects
- Instancing policies
- Network execution policies

### Phase 4: Gameplay Effects
- `@gameplay_effect` struct parsing
- Modifiers (Add, Multiply, Divide, Override)
- Duration types (Instant, HasDuration, Infinite)
- Stacking rules
- Tag requirements

### Phase 5: Advanced Features
- Ability tasks (WaitTargetData, WaitGameplayEvent)
- Gameplay cues (particles, sounds)
- Execution calculations (custom damage formulas)
- Ability sets (Lyra pattern)

---

## References

### Documentation
- [GAMEPLAY_TAGS_DEEP_DIVE.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/GAMEPLAY_TAGS_DEEP_DIVE.md) — Complete tag architecture
- [TAG_EXAMPLES.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/TAG_EXAMPLES.md) — Real-world patterns from Lyra/NinjaGAS
- [GAS_IMPLEMENTATION_PLAN.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/GAS_IMPLEMENTATION_PLAN.md) — Full roadmap

### UE5 Source
- `GameplayAbilities/Public/GameplayTagContainer.h` — FGameplayTag, FGameplayTagContainer
- `GameplayAbilities/Public/NativeGameplayTags.h` — UE_DECLARE/DEFINE macros
- `GameplayTags/Public/GameplayTagsManager.h` — Global tag registry

### Production Examples
- Lyra: `LyraGame/LyraGameplayTags.h/cpp`
- NinjaGAS: `NinjaGAS/Public/NinjaGASTags.h/cpp`
- ShooterCore: `Config/Tags/ShooterCoreTags.ini`

---

## Contributing

### Adding New Features

1. Add AST nodes to `kain-core/src/ast.rs`
2. Add parser logic to `kain-core/src/parser.rs`
3. Create IR module in `src/`
4. Create codegen module in `src/`
5. Add unit tests
6. Add integration tests
7. Update README and CRATE_REFERENCE

### Running Tests

```bash
# All tests
cargo test -p ue5-gas

# Specific module
cargo test -p ue5-gas tags_ir
cargo test -p ue5-gas tags_codegen

# Integration tests
cargo test -p ue5-gas --test integration_test

# With output
cargo test -p ue5-gas -- --nocapture
```

---

## License

Part of the KAIN compiler project.
