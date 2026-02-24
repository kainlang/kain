# ue5-gas — Gameplay Ability System Support for KAIN

> **Codegen backend for Unreal Engine 5's Gameplay Ability System (GAS)**

![Status](https://img.shields.io/badge/Status-Phase%202%20Complete-brightgreen)
![Tests](https://img.shields.io/badge/Tests-38%20Passing-brightgreen)

---

## Overview

This crate provides KAIN language support for UE5's Gameplay Ability System, enabling:
- GameplayTags (native C++ + .ini generation)
- Attribute Sets (with replication, clamping, lifecycle hooks)
- Gameplay Abilities (with tag requirements, cost, cooldown)
- Gameplay Effects (with modifiers, stacking, tag requirements)

**Compression Ratio:** 1:10 average (1 line KAIN → 10 lines C++)

---

## Phase 1: GameplayTags (COMPLETE)

### Features

- **Tag namespace parser** — Parse hierarchical tag definitions
- **Tag IR** — Flatten hierarchy, generate parent tags, validate uniqueness
- **Native C++ codegen** — Generate `.h` and `.cpp` with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG_COMMENT`
- **INI file codegen** — Generate `DefaultGameplayTags.ini` for designer-friendly editing
- **Nested namespaces** — Automatic C++ namespace hierarchy generation

### Syntax

```kain
@gameplay_tags
namespace Ability:
    Attack:
        Melee:
            Sword
            Axe
        Ranged:
            Bow
            Gun
    Defend:
        Block
        Parry
```

### Generated Files

**GameplayTags.h:**
```cpp
#pragma once
#include "NativeGameplayTags.h"

namespace MyGameTags
{
    namespace Ability
    {
        namespace Attack
        {
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ranged);
        }
    }
}
```

**GameplayTags.cpp:**
```cpp
#include "GameplayTags.h"

namespace MyGameTags
{
    namespace Ability
    {
        namespace Attack
        {
            UE_DEFINE_GAMEPLAY_TAG(Melee, "Ability.Attack.Melee");
        }
    }
}
```

**DefaultGameplayTags.ini:**
```ini
[/Script/GameplayTags.GameplayTagsList]
; Ability Tags
GameplayTagList=(Tag="Ability.Attack.Melee")
GameplayTagList=(Tag="Ability.Attack.Ranged")
```

---

## Phase 2: Attribute Sets (COMPLETE)

### Features

- **Attribute set parser** — Parse `@attribute_set` structs with `@attribute` fields
- **Attribute IR** — Metadata for replication, rep_notify, hide_from_modifiers, meta attributes
- **UAttributeSet codegen** — Generate complete UAttributeSet subclasses
- **ATTRIBUTE_ACCESSORS** — Automatic accessor macro generation
- **Replication** — GetLifetimeReplicatedProps with DOREPLIFETIME_CONDITION_NOTIFY
- **RepNotify functions** — GAMEPLAYATTRIBUTE_REPNOTIFY macro usage
- **Lifecycle hooks** — PreGameplayEffectExecute, PostGameplayEffectExecute, PreAttributeChange, PostAttributeChange
- **Meta attributes** — Temporary calculation attributes (not replicated)
- **Constructor initialization** — Default values for all attributes

### Syntax

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
        # Handle meta attributes
        if incoming_damage > 0.0:
            health = health - incoming_damage
            incoming_damage = 0.0
```

### Generated Files

**HealthSet.h:**
```cpp
#pragma once
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HealthSet.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UHealthSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UHealthSet();

    ATTRIBUTE_ACCESSORS(UHealthSet, Health);
    ATTRIBUTE_ACCESSORS(UHealthSet, MaxHealth);
    ATTRIBUTE_ACCESSORS(UHealthSet, IncomingDamage);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health", Meta = (AllowPrivateAccess = true, HideFromModifiers))
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", Meta = (AllowPrivateAccess = true))
    FGameplayAttributeData MaxHealth;

    UPROPERTY(BlueprintReadOnly, Category = "Health", Meta = (AllowPrivateAccess = true))
    FGameplayAttributeData IncomingDamage;
};
```

**Compression Ratio:** 1:31.5 (2 attributes → 63 C++ lines)

---

## Architecture

### Crate Structure

```
ue5-gas/
├── src/
│   ├── lib.rs                      # Public API
│   ├── tags_ir.rs                  # Tag IR (flatten hierarchy)
│   ├── tags_codegen.rs             # Tag codegen (C++ + INI)
│   ├── attribute_set_ir.rs         # Attribute set IR
│   └── attribute_set_codegen.rs    # Attribute set codegen
└── tests/
    ├── tags_tests.rs               # Tag unit tests (16 tests)
    ├── attribute_set_integration_tests.rs  # Attribute set tests (11 tests)
    └── integration_test.rs         # Integration tests (2 tests)
```

### Dependencies

- `kain-core` — AST, types, parser
- `anyhow` — Error handling
- `thiserror` — Error types

---

## Testing

Run tests:
```bash
cargo test -p ue5-gas
```

**Test coverage:**
- Tag hierarchy flattening
- Parent tag extraction
- Duplicate detection
- C++ name generation
- Native C++ header generation
- Native C++ implementation generation
- INI file generation
- Leaf name extraction
- Complex hierarchies
- Multiple namespaces

---

## Integration

### Packager Integration

Add to `cli/src/packager/ue5_pipeline.rs`:

```rust
use ue5_gas::{GameplayTagsIR, tags_codegen};

match item {
    Item::GameplayTags(tags) => {
        let ir = GameplayTagsIR::from_ast(vec![tags.clone()])?;
        let output = tags_codegen::generate(&ir, &plugin_name)?;
        
        // Write files
        write_file("Source/Public/GameplayTags.h", output.header)?;
        write_file("Source/Private/GameplayTags.cpp", output.implementation)?;
        write_file("Config/Tags/DefaultGameplayTags.ini", output.ini_file)?;
    }
}
```

### Module Dependencies

Add to `Build.cs`:
```cpp
PublicDependencyModuleNames.AddRange(new string[] {
    "GameplayTags",
    "GameplayAbilities",
});
```

---

## Roadmap

### Phase 1: GameplayTags ✅
- [x] Tag namespace parser
- [x] Tag IR
- [x] Native C++ codegen
- [x] INI file codegen
- [x] Unit tests

### Phase 2: Attribute Sets ✅
- [x] Attribute set parser
- [x] Attribute set IR
- [x] Attribute set codegen
- [x] Replication support
- [x] Lifecycle hooks
- [x] 11 integration tests passing

### Phase 3: Gameplay Abilities
- [ ] Ability parser
- [ ] Ability IR
- [ ] Ability codegen
- [ ] Tag integration
- [ ] Cost/cooldown support

### Phase 4: Gameplay Effects
- [ ] Effect parser
- [ ] Effect IR
- [ ] Effect codegen
- [ ] Modifier support
- [ ] Stacking rules

---

## References

- [GAMEPLAY_TAGS_DEEP_DIVE.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/GAMEPLAY_TAGS_DEEP_DIVE.md)
- [TAG_EXAMPLES.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/TAG_EXAMPLES.md)
- [GAS_IMPLEMENTATION_PLAN.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/GAS_IMPLEMENTATION_PLAN.md)
- [UE5 GameplayTags Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-tags-in-unreal-engine)
- Lyra: `LyraGame/LyraGameplayTags.h/cpp`
- NinjaGAS: `NinjaGAS/Public/NinjaGASTags.h/cpp`

---

## License

Part of the KAIN compiler project.
