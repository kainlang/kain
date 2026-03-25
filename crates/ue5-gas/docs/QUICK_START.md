# GameplayTags Quick Start Guide

> **Get started with KAIN's GameplayTags system in 5 minutes**

---

## Installation

The `ue5-gas` crate is part of the KAIN workspace. No installation needed.

```bash
# Verify it compiles
cargo build -p ue5-gas

# Run tests
cargo test -p ue5-gas
```

---

## Basic Usage

### Step 1: Define Tags in KAIN

Create a `.kn` file with tag definitions:

```kain
@gameplay_tags
namespace Ability:
    Attack:
        Melee
        Ranged
    Defend:
        Block
        Parry

@gameplay_tags
namespace Status:
    Alive
    Dead
    CC:
        Stunned
        Rooted
```

### Step 2: Build Plugin

```bash
kain build --ue5
```

This will generate:
- `Source/Public/GameplayTags.h`
- `Source/Private/GameplayTags.cpp`
- `Config/Tags/DefaultGameplayTags.ini`

### Step 3: Use Tags in C++

```cpp
#include "GameplayTags.h"

void UMyAbility::ActivateAbility()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    
    // Check if stunned
    if (ASC->HasMatchingGameplayTag(MyGameTags::Status::CC::Stunned))
    {
        // Cannot activate while stunned
        return;
    }
    
    // Add attacking tag
    ASC->AddLooseGameplayTag(MyGameTags::Status::Attacking);
    
    // Execute ability logic
    DealDamage();
    
    // Remove attacking tag
    ASC->RemoveLooseGameplayTag(MyGameTags::Status::Attacking);
}
```

---

## Common Patterns

### Pattern 1: Ability Activation Requirements

```kain
@gameplay_tags
namespace Ability:
    Attack:
        Melee
    
@gameplay_tags
namespace Status:
    Alive
    CC:
        Stunned
        Silenced
```

**Usage:**
```cpp
bool CanActivate()
{
    // Must be alive
    if (!ASC->HasMatchingGameplayTag(MyGameTags::Status::Alive))
        return false;
    
    // Cannot be stunned or silenced
    FGameplayTagContainer BlockedTags;
    BlockedTags.AddTag(MyGameTags::Status::CC::Stunned);
    BlockedTags.AddTag(MyGameTags::Status::CC::Silenced);
    
    if (ASC->HasAnyMatchingGameplayTags(BlockedTags))
        return false;
    
    return true;
}
```

### Pattern 2: Status Effect Tracking

```kain
@gameplay_tags
namespace Status:
    Buff:
        Strength
        Speed
        Armor
    Debuff:
        Weakness
        Slow
        Vulnerable
```

**Usage:**
```cpp
void ApplyBuff()
{
    ASC->AddLooseGameplayTag(MyGameTags::Status::Buff::Strength);
    
    // Listen for buff removal
    ASC->RegisterGameplayTagEvent(
        MyGameTags::Status::Buff::Strength,
        EGameplayTagEventType::NewOrRemoved
    ).AddUObject(this, &UMyClass::OnStrengthBuffChanged);
}

void OnStrengthBuffChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Buff applied
        UpdateDamageMultiplier(1.5f);
    }
    else
    {
        // Buff removed
        UpdateDamageMultiplier(1.0f);
    }
}
```

### Pattern 3: Damage Type System

```kain
@gameplay_tags
namespace Damage:
    Physical:
        Slash
        Pierce
        Blunt
    Magical:
        Fire
        Ice
        Lightning
```

**Usage:**
```cpp
void ApplyDamage(float Amount, FGameplayTag DamageType)
{
    // Check immunity
    if (DamageType.MatchesTag(MyGameTags::Damage::Magical::Fire))
    {
        if (ASC->HasMatchingGameplayTag(MyGameTags::Status::Immune::Fire))
        {
            // Immune to fire damage
            return;
        }
    }
    
    // Apply damage
    // ...
}
```

---

## Tag Hierarchy Best Practices

### Use Clear Hierarchies

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

### Use Consistent Prefixes

```
Ability.*       — Abilities
Status.*        — Character states
Effect.*        — Effect metadata
Damage.*        — Damage types
Cooldown.*      — Cooldown tags
Event.*         — Gameplay events
```

### Keep Hierarchies Shallow

```
✅ GOOD (3 levels):
Ability.Attack.Melee

❌ BAD (5+ levels):
Ability.Combat.Attack.Physical.Melee.Weapon.Sword
```

**Recommendation:** 2-4 levels deep is optimal.

---

## Tag Matching

### Exact Match

```cpp
FGameplayTag Tag1 = MyGameTags::Ability::Attack;
FGameplayTag Tag2 = MyGameTags::Ability::Attack::Melee;

bool bExact = Tag1.MatchesTagExact(Tag2);  // FALSE
```

### Hierarchy Match

```cpp
FGameplayTag ParentTag = MyGameTags::Ability::Attack;
FGameplayTag ChildTag = MyGameTags::Ability::Attack::Melee;

bool bMatches = ParentTag.MatchesTag(ChildTag);  // TRUE (parent matches child)
bool bReverse = ChildTag.MatchesTag(ParentTag);  // FALSE (child doesn't match parent)
```

### Container Queries

```cpp
FGameplayTagContainer OwnerTags;
OwnerTags.AddTag(MyGameTags::Status::CC::Stunned);
OwnerTags.AddTag(MyGameTags::Status::Buff::Strength);

// Has exact tag
bool bHasStun = OwnerTags.HasTagExact(MyGameTags::Status::CC::Stunned);  // TRUE

// Has any CC tag
bool bHasCC = OwnerTags.HasTag(MyGameTags::Status::CC);  // TRUE (hierarchy match)

// Has any from container
FGameplayTagContainer CCTags;
CCTags.AddTag(MyGameTags::Status::CC::Stunned);
CCTags.AddTag(MyGameTags::Status::CC::Rooted);

bool bHasAnyCC = OwnerTags.HasAny(CCTags);  // TRUE
```

---

## Performance Tips

### 1. Cache Tags

```cpp
// ❌ BAD: Lookup every frame
if (ASC->HasMatchingGameplayTag(
    FGameplayTag::RequestGameplayTag(FName("Status.Stunned"))))
{
    // ...
}

// ✅ GOOD: Use native tag (compile-time constant)
if (ASC->HasMatchingGameplayTag(MyGameTags::Status::CC::Stunned))
{
    // ...
}
```

### 2. Use Tag Events

```cpp
// ❌ BAD: Poll every frame
void Tick(float DeltaTime)
{
    if (ASC->HasMatchingGameplayTag(StunnedTag))
    {
        // Handle stunned
    }
}

// ✅ GOOD: Event-driven
void BeginPlay()
{
    ASC->RegisterGameplayTagEvent(
        MyGameTags::Status::CC::Stunned,
        EGameplayTagEventType::NewOrRemoved
    ).AddUObject(this, &UMyClass::OnStunnedChanged);
}
```

### 3. Batch Operations

```cpp
// ❌ BAD: Multiple individual operations
ASC->AddLooseGameplayTag(Tag1);
ASC->AddLooseGameplayTag(Tag2);
ASC->AddLooseGameplayTag(Tag3);

// ✅ GOOD: Batch operation
FGameplayTagContainer Tags;
Tags.AddTag(Tag1);
Tags.AddTag(Tag2);
Tags.AddTag(Tag3);
ASC->AddLooseGameplayTags(Tags);
```

---

## Troubleshooting

### Error: Duplicate tag

**Problem:** Same tag defined multiple times

**Solution:** Remove duplicate or rename

### Error: Expected 'namespace'

**Problem:** Missing `namespace` keyword after `@gameplay_tags`

**Solution:**
```kain
# ✅ GOOD
@gameplay_tags
namespace Ability:
    Attack
```

### Warning: Empty namespace

**Problem:** Namespace has no tags

**Solution:** Add tags or remove namespace

---

## Examples

### Example 1: Combat System

```kain
@gameplay_tags
namespace Combat:
    Weapon:
        Melee:
            Sword
            Axe
            Spear
        Ranged:
            Bow
            Crossbow
            Gun
    Damage:
        Physical
        Magical
    State:
        Attacking
        Defending
        Dodging
```

### Example 2: Status Effects

```kain
@gameplay_tags
namespace Status:
    Health:
        Alive
        Injured
        Critical
        Dead
    CC:
        Stunned
        Rooted
        Silenced
        Feared
        Charmed
    Buff:
        Strength
        Speed
        Armor
        Regeneration
    Debuff:
        Weakness
        Slow
        Vulnerable
        Bleeding
```

### Example 3: Ability System

```kain
@gameplay_tags
namespace Ability:
    Type:
        Active
        Passive
        Channeled
    Category:
        Attack
        Defend
        Utility
        Movement
    ActivateFail:
        OnCooldown
        InsufficientMana
        TagsBlocked
        TagsMissing
        IsDead
```

---

## Resources

### Documentation
- [README.md](README.md) — Overview
- [CRATE_REFERENCE.md](CRATE_REFERENCE.md) — Complete API reference
- [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md) — Technical details

### Examples
- `examples/test_tags.kn` — Example KAIN file
- `examples/generate_example.rs` — Runnable example

### Research
- [GAMEPLAY_TAGS_DEEP_DIVE.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/GAMEPLAY_TAGS_DEEP_DIVE.md)
- [TAG_EXAMPLES.md](../../../Research/ReferenceCode/GameplayAbilities_GAS/TAG_EXAMPLES.md)

### UE5 Documentation
- [GameplayTags Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-tags-in-unreal-engine)
- [Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine)

---

## Support

For issues or questions:
1. Check [CRATE_REFERENCE.md](CRATE_REFERENCE.md) for API details
2. Check [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md) for technical details
3. Run tests: `cargo test -p ue5-gas`
4. Check examples: `cargo run -p ue5-gas --example generate_example`

---

**Phase 1: COMPLETE ✅**
