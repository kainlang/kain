# GameplayTags Deep Dive — Foundation of GAS

> **Complete analysis of Unreal Engine's GameplayTag system and design for KAIN integration**

---

## Table of Contents

1. [Overview](#overview)
2. [Core Concepts](#core-concepts)
3. [Tag Architecture](#tag-architecture)
4. [Tag Registration](#tag-registration)
5. [Tag Matching & Queries](#tag-matching--queries)
6. [Tag Usage in GAS](#tag-usage-in-gas)
7. [Replication](#replication)
8. [Performance Considerations](#performance-considerations)
9. [KAIN Tag System Design](#kain-tag-system-design)
10. [Codegen Strategy](#codegen-strategy)
11. [Integration with Abilities/Effects](#integration-with-abilitieseffects)
12. [Editor Support](#editor-support)
13. [Best Practices](#best-practices)

---

## Overview

**GameplayTags are the foundation of the Gameplay Ability System.** Without proper tag support, nothing else in GAS works correctly.

### What Are GameplayTags?

GameplayTags are **hierarchical string identifiers** represented by the `FGameplayTag` type:
- Format: `"Parent.Child.Grandchild"` (dot-separated hierarchy)
- Stored as **numeric IDs** internally for fast comparison
- **Global dictionary** — all tags exist in one game-wide pool
- **Implicit hierarchy** — creating `"Player.Weapon.Shotgun"` automatically creates `"Player.Weapon"` and `"Player"`

### Why Tags Matter in GAS

Tags control **every aspect** of the ability system:
- **Ability activation** — required tags, blocked tags
- **Effect application** — immunity, requirements, granted tags
- **Ability cancellation** — cancel tags, block tags
- **Cooldowns** — cooldown tags
- **Gameplay cues** — visual/audio effects triggered by tags
- **State tracking** — status effects, buffs, debuffs

**Without tags, you cannot:**
- Prevent abilities from activating
- Apply conditional effects
- Track character state
- Implement cooldowns
- Cancel abilities
- Trigger gameplay cues

---

## Core Concepts

### FGameplayTag

**Single tag** — represents one node in the hierarchy.

```cpp
FGameplayTag AbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Melee"));
```

**Key properties:**
- Lightweight (stored as FName internally)
- Fast comparison (numeric ID comparison)
- Supports hierarchy matching
- Immutable once created

**Common operations:**
```cpp
// Exact match
bool bMatches = Tag1.MatchesTagExact(Tag2);

// Partial match (hierarchy-aware)
bool bMatchesPartial = Tag1.MatchesTag(Tag2);  // "Ability.Attack" matches "Ability.Attack.Melee"

// Get parent tag
FGameplayTag ParentTag = Tag.RequestDirectParent();

// Check validity
bool bValid = Tag.IsValid();
```

### FGameplayTagContainer

**Collection of tags** — optimized for fast queries.

```cpp
FGameplayTagContainer TagContainer;
TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned")));
TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Rooted")));
```

**Key operations:**
```cpp
// Has exact tag
bool bHasTag = TagContainer.HasTagExact(Tag);

// Has tag (hierarchy-aware)
bool bHasTag = TagContainer.HasTag(Tag);

// Has any tags from another container
bool bHasAny = TagContainer.HasAny(OtherContainer);

// Has all tags from another container
bool bHasAll = TagContainer.HasAll(OtherContainer);

// Filter tags
FGameplayTagContainer Filtered = TagContainer.Filter(FilterContainer);

// Append tags
TagContainer.AppendTags(OtherContainer);

// Remove tags
TagContainer.RemoveTags(OtherContainer);
```

### FGameplayTagQuery

**Complex logical queries** — arbitrarily recursive expressions.

```cpp
// Query: (HasAny["Status.Buffed", "Status.Empowered"]) AND (NOT "Status.Stunned")
FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
    .And(FGameplayTagQuery::MakeQuery_MatchNoTags(StunnedTags));

bool bMatches = Query.Matches(TargetContainer);
```

**Query types:**
- `MatchAnyTags` — OR operation
- `MatchAllTags` — AND operation
- `MatchNoTags` — NOT operation
- Recursive combinations — `(A OR B) AND NOT (C OR D)`

---

## Tag Architecture

### Hierarchy Structure

Tags form a **tree structure**:

```
Ability
├── Attack
│   ├── Melee
│   │   ├── Sword
│   │   └── Axe
│   └── Ranged
│       ├── Bow
│       └── Gun
├── Defend
│   ├── Block
│   └── Parry
└── Utility
    ├── Heal
    └── Buff

Status
├── Stunned
├── Rooted
├── Silenced
└── Death
    ├── Dying
    └── Dead
```

**Hierarchy matching:**
- `"Ability.Attack"` matches `"Ability.Attack.Melee.Sword"` (parent matches child)
- `"Ability.Attack.Melee.Sword"` does NOT match `"Ability.Attack"` (child doesn't match parent)
- Use `MatchesTagExact()` for exact matching
- Use `MatchesTag()` for hierarchy-aware matching

### Global Tag Dictionary

All tags exist in **one global pool** managed by `UGameplayTagsManager`:
- Tags are registered at startup
- Each tag gets a unique numeric ID
- Fast comparison via ID lookup
- Thread-safe access
- Editor integration for tag picking

---

## Tag Registration

### Method 1: Native C++ Tags (Recommended for Core Tags)

**Header file:**
```cpp
// MyGameplayTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace MyGameplayTags
{
    // Declare tags
    MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Melee);
    MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Ranged);
    MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Stunned);
    MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Rooted);
}
```

**Implementation file:**
```cpp
// MyGameplayTags.cpp
#include "MyGameplayTags.h"

namespace MyGameplayTags
{
    // Define tags with full path and comment
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_Attack_Melee, 
        "Ability.Attack.Melee", 
        "Melee attack ability"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_Attack_Ranged, 
        "Ability.Attack.Ranged", 
        "Ranged attack ability"
    );
    
    UE_DEFINE_GAMEPLAY_TAG(Status_Stunned, "Status.Stunned");
    UE_DEFINE_GAMEPLAY_TAG(Status_Rooted, "Status.Rooted");
}
```

**Usage:**
```cpp
#include "MyGameplayTags.h"

void UMyAbility::ActivateAbility()
{
    // Use the tag directly
    if (ASC->HasMatchingGameplayTag(MyGameplayTags::Status_Stunned))
    {
        // Cannot activate while stunned
        return;
    }
}
```

**Advantages:**
- Compile-time safety (typos caught at compile time)
- Autocomplete in IDE
- Refactoring support
- Fast access (no string lookup)
- Guaranteed to exist

**Disadvantages:**
- Requires recompile to add/change tags
- Not designer-friendly


### Method 2: GameplayTags.ini Files (Recommended for Designer Tags)

**Config/Tags/GameplayTags.ini:**
```ini
[/Script/GameplayTags.GameplayTagsList]
GameplayTagList=(Tag="Ability.ActivateFail.MagazineFull",DevComment="Cannot reload with full magazine")
GameplayTagList=(Tag="Ability.ActivateFail.NoSpareAmmo",DevComment="No ammo to reload")
GameplayTagList=(Tag="Event.Movement.ADS",DevComment="Aim down sights event")
GameplayTagList=(Tag="Event.Movement.Dash",DevComment="Dash movement event")
GameplayTagList=(Tag="Weapon.Type.Rifle",DevComment="Rifle weapon type")
GameplayTagList=(Tag="Weapon.Type.Pistol",DevComment="Pistol weapon type")
```

**Advantages:**
- Designer-friendly (no code changes)
- Hot-reload in editor
- Easy to organize by feature/plugin
- Version control friendly

**Disadvantages:**
- No compile-time safety
- Typos only caught at runtime
- Must use `RequestGameplayTag()` to access

**Best practice:** Use separate .ini files per feature/plugin:
```
Config/Tags/
├── CoreTags.ini          # Core game tags
├── AbilityTags.ini       # Ability-specific tags
├── WeaponTags.ini        # Weapon system tags
└── StatusTags.ini        # Status effect tags
```

### Method 3: Data Tables (Recommended for External Tools)

**Create a DataTable with `GameplayTagTableRow` type:**

CSV file (`WeaponTags.csv`):
```csv
RowName,Tag,DevComment
Rifle,Weapon.Type.Rifle,Rifle weapon type
Pistol,Weapon.Type.Pistol,Pistol weapon type
Shotgun,Weapon.Type.Shotgun,Shotgun weapon type
```

**Import into Unreal:**
1. Right-click in Content Browser → Import
2. Select CSV file
3. Choose `GameplayTagTableRow` as row structure
4. Add DataTable to Project Settings → GameplayTags → Gameplay Tag Table List

**Advantages:**
- External tool integration
- Bulk import/export
- Spreadsheet editing
- Reimport on change

**Disadvantages:**
- Extra asset management
- Must be added to project settings
- No compile-time safety

### Method 4: Runtime Registration (Rare)

```cpp
UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
Manager.AddNativeGameplayTag(
    FName("Dynamic.Tag.Example"),
    FString("Dynamically created tag")
);
```

**⚠️ Warning:** Can only be called during engine initialization. The tag table must be locked before replication starts.

---

## Tag Matching & Queries

### Exact vs Hierarchy Matching

```cpp
FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack"));
FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Melee"));

// Exact match
bool bExact = ParentTag.MatchesTagExact(ChildTag);  // FALSE

// Hierarchy match
bool bHierarchy = ParentTag.MatchesTag(ChildTag);   // TRUE (parent matches child)
bool bReverse = ChildTag.MatchesTag(ParentTag);     // FALSE (child doesn't match parent)
```

### Container Matching

```cpp
FGameplayTagContainer OwnerTags;
OwnerTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned")));
OwnerTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Rooted")));

FGameplayTagContainer RequiredTags;
RequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned")));

// Has exact tag
bool bHasExact = OwnerTags.HasTagExact(StunnedTag);  // TRUE

// Has tag (hierarchy-aware)
bool bHasTag = OwnerTags.HasTag(StatusTag);  // TRUE if "Status" parent exists

// Has any tags
bool bHasAny = OwnerTags.HasAny(RequiredTags);  // TRUE (has at least one)

// Has all tags
bool bHasAll = OwnerTags.HasAll(RequiredTags);  // TRUE (has all required)
```

### Complex Queries

**Query structure:**
```cpp
// (HasAny[A, B]) AND (HasAll[C, D]) AND (HasNone[E, F])
FGameplayTagQuery Query;
Query.BuildQuery(
    FGameplayTagQueryExpression()
        .AllTagsMatch()
        .AddTag(TagA)
        .AddTag(TagB)
    .AnyTagsMatch()
        .AddTag(TagC)
        .AddTag(TagD)
    .NoTagsMatch()
        .AddTag(TagE)
        .AddTag(TagF)
);

bool bMatches = Query.Matches(TargetContainer);
```

**Common query patterns:**

**1. Ability can activate if buffed OR empowered, but NOT stunned:**
```cpp
FGameplayTagContainer BuffTags;
BuffTags.AddTag(StatusBuffed);
BuffTags.AddTag(StatusEmpowered);

FGameplayTagContainer BlockedTags;
BlockedTags.AddTag(StatusStunned);

FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
    .And(FGameplayTagQuery::MakeQuery_MatchNoTags(BlockedTags));
```

**2. Effect applies if target has ALL damage vulnerability tags:**
```cpp
FGameplayTagContainer VulnerabilityTags;
VulnerabilityTags.AddTag(VulnerabilityFire);
VulnerabilityTags.AddTag(VulnerabilityPhysical);

FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchAllTags(VulnerabilityTags);
```

**3. Complex nested query:**
```cpp
// ((HasAny[Buffed, Empowered]) AND (HasAll[Alive, Conscious])) AND (NOT (HasAny[Stunned, Silenced]))
FGameplayTagQuery Query = 
    FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
        .And(FGameplayTagQuery::MakeQuery_MatchAllTags(StateTags))
        .And(FGameplayTagQuery::MakeQuery_MatchNoTags(CCTags));
```

---

## Tag Usage in GAS

### Abilities (UGameplayAbility)

**Tag properties:**
```cpp
UCLASS()
class UMyGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
    // Tags that describe this ability (what it IS)
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer AbilityTags;  // "Ability.Attack.Melee"
    
    // Tags granted to owner while ability is active
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationOwnedTags;  // "Status.Attacking"
    
    // Owner must have ALL these tags to activate
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationRequiredTags;  // "Status.Alive"
    
    // Owner cannot have ANY of these tags to activate
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationBlockedTags;  // "Status.Stunned", "Status.Silenced"
    
    // Source must have ALL these tags
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer SourceRequiredTags;
    
    // Source cannot have ANY of these tags
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer SourceBlockedTags;
    
    // Target must have ALL these tags
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer TargetRequiredTags;
    
    // Target cannot have ANY of these tags
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer TargetBlockedTags;
};
```

**Activation flow:**
1. Check `ActivationBlockedTags` — if owner has ANY, fail
2. Check `ActivationRequiredTags` — if owner missing ANY, fail
3. Check `SourceBlockedTags` — if source has ANY, fail
4. Check `SourceRequiredTags` — if source missing ANY, fail
5. Check cooldown tags
6. Check cost
7. If all pass → activate ability
8. Apply `ActivationOwnedTags` to owner


### GameplayEffects (UGameplayEffect)

**Tag properties in effect components:**

**1. AssetTagsGameplayEffectComponent** — tags the effect HAS (metadata):
```cpp
// Tags that describe this effect
FGameplayTagContainer AssetTags;  // "Effect.Damage.Fire", "Effect.Type.DOT"
```

**2. TargetTagsGameplayEffectComponent** — tags granted to target:
```cpp
// Tags granted to target while effect is active
FGameplayTagContainer GrantedTags;  // "Status.Burning", "Status.DOT"

// Tags applied to target (for queries, not granted)
FGameplayTagContainer AppliedTags;
```

**3. TargetTagRequirementsGameplayEffectComponent** — application requirements:
```cpp
// Target must have ALL these tags
FGameplayTagContainer ApplicationRequiredTags;  // "Status.Alive"

// Target cannot have ANY of these tags
FGameplayTagContainer ApplicationBlockedTags;  // "Status.Immune.Fire"

// Effect only active while target has these tags
FGameplayTagContainer OngoingRequiredTags;

// Effect removed if target gets any of these tags
FGameplayTagContainer RemovalRequiredTags;
```

**4. BlockAbilityTagsGameplayEffectComponent** — blocks abilities:
```cpp
// Blocks abilities with these tags
FGameplayTagContainer BlockedAbilityTags;  // "Ability.Attack", "Ability.Cast"
```

**5. CancelAbilityTagsGameplayEffectComponent** — cancels abilities:
```cpp
// Cancels abilities with these tags when applied
FGameplayTagContainer CancelAbilityTags;  // "Ability.Channeled"
```

**6. ImmunityGameplayEffectComponent** — grants immunity:
```cpp
// Immune to effects with these tags
FGameplayTagContainer ImmunityTags;  // "Effect.Damage.Fire", "Effect.CC.Stun"
```

### AbilitySystemComponent

**Tag tracking:**
```cpp
// Owned tags (from effects, abilities, loose tags)
FGameplayTagCountContainer GameplayTagCountContainer;

// Blocked ability tags (from effects)
FGameplayTagCountContainer BlockedAbilityTags;
```

**Tag operations:**
```cpp
// Check if has tag
bool bHasTag = ASC->HasMatchingGameplayTag(Tag);
bool bHasAll = ASC->HasAllMatchingGameplayTags(TagContainer);
bool bHasAny = ASC->HasAnyMatchingGameplayTags(TagContainer);

// Get owned tags
FGameplayTagContainer OwnedTags;
ASC->GetOwnedGameplayTags(OwnedTags);

// Add/remove loose tags (not from effects)
ASC->AddLooseGameplayTag(Tag);
ASC->AddLooseGameplayTags(TagContainer);
ASC->RemoveLooseGameplayTag(Tag);
ASC->RemoveLooseGameplayTags(TagContainer);

// Query active effects by tags
TArray<FActiveGameplayEffectHandle> Effects = ASC->GetActiveEffectsWithAllTags(TagContainer);

// Remove effects by tags
int32 Removed = ASC->RemoveActiveEffectsWithTags(TagContainer);
int32 Removed = ASC->RemoveActiveEffectsWithGrantedTags(TagContainer);
```

**Tag events:**
```cpp
// Listen for tag changes
ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
    .AddUObject(this, &UMyClass::OnTagChanged);

// Listen for tag count changes
ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
    .AddUObject(this, &UMyClass::OnTagCountChanged);
```

---

## Replication

### Tag Replication Modes

**1. Full replication (default):**
- All tags replicated to all clients
- Highest bandwidth cost
- Most accurate

**2. Minimal replication:**
- Only specific tags replicated
- Lower bandwidth
- Use for tags that need to be visible to all clients

**3. Owner-only replication:**
- Tags only replicated to owning client
- Lowest bandwidth
- Use for private information

### Replication Configuration

**In AbilitySystemGlobals:**
```cpp
// Project Settings → GameplayAbilities
bool bReplicateActivationOwnedTags = true;  // Replicate ActivationOwnedTags from abilities
```

**Per-tag replication:**
```cpp
// Add tag with replication state
ASC->AddLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::Full);
ASC->AddLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::TagOnly);
ASC->AddLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::None);
```

### Replication Performance

**Tag count container replication:**
- Uses delta compression
- Only sends changed tags
- Efficient for large tag sets

**Minimal replication proxy:**
- Separate replication channel for minimal tags
- Lower priority than full tags
- Used for cosmetic tags

---

## Performance Considerations

### Tag Comparison Performance

**Fast operations (O(1)):**
- Exact tag comparison (numeric ID comparison)
- Tag validity check
- Tag hash lookup

**Medium operations (O(n)):**
- Container `HasTag()` — iterates container
- Container `HasAny()` — iterates until match found
- Container `HasAll()` — iterates checking all

**Slow operations (O(n*m)):**
- Container intersection
- Complex queries with multiple sub-expressions

### Optimization Tips

**1. Cache tags:**
```cpp
// BAD: Lookup every frame
if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned"))))
{
    // ...
}

// GOOD: Cache tag
static const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName("Status.Stunned"));
if (ASC->HasMatchingGameplayTag(StunnedTag))
{
    // ...
}
```

**2. Use native tags for hot paths:**
```cpp
// BEST: Native tag (compile-time constant)
#include "MyGameplayTags.h"
if (ASC->HasMatchingGameplayTag(MyGameplayTags::Status_Stunned))
{
    // ...
}
```

**3. Minimize tag count:**
- Don't add tags unnecessarily
- Remove tags when no longer needed
- Use tag queries instead of multiple individual tags

**4. Use tag events instead of polling:**
```cpp
// BAD: Check every frame
void Tick(float DeltaTime)
{
    if (ASC->HasMatchingGameplayTag(StunnedTag))
    {
        // Handle stunned
    }
}

// GOOD: Event-driven
void BeginPlay()
{
    ASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UMyClass::OnStunnedChanged);
}

void OnStunnedChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Became stunned
    }
    else
    {
        // No longer stunned
    }
}
```

**5. Batch tag operations:**
```cpp
// BAD: Multiple individual operations
ASC->AddLooseGameplayTag(Tag1);
ASC->AddLooseGameplayTag(Tag2);
ASC->AddLooseGameplayTag(Tag3);

// GOOD: Batch operation
FGameplayTagContainer Tags;
Tags.AddTag(Tag1);
Tags.AddTag(Tag2);
Tags.AddTag(Tag3);
ASC->AddLooseGameplayTags(Tags);
```

### Memory Considerations

**Tag storage:**
- `FGameplayTag` — 8 bytes (FName)
- `FGameplayTagContainer` — ~32 bytes + tag array
- `FGameplayTagCountContainer` — ~48 bytes + tag map

**Replication bandwidth:**
- Full tag replication: ~4 bytes per tag
- Minimal replication: ~2 bytes per tag
- Delta compression reduces bandwidth significantly

---

## KAIN Tag System Design

### Design Goals

1. **Type-safe** — compile-time tag validation
2. **Hierarchy-aware** — automatic parent tag creation
3. **Designer-friendly** — .ini file generation
4. **Performance** — native tag generation for hot paths
5. **Integration** — seamless GAS integration
6. **Editor support** — tag picker metadata

### Proposed KAIN Syntax

**Tag namespace definition:**
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
            Magic
    Defend:
        Block
        Parry
        Dodge
    Utility:
        Heal
        Buff
        Debuff

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
        Feared
    Buff:
        Strength
        Speed
        Armor
    Debuff:
        Weakness
        Slow
        Vulnerable

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
        Poison
```


**Tag usage in abilities:**
```kain
@ability
struct FireballAbility:
    @tags
    ability_tags: ["Ability.Attack.Ranged", "Ability.Magic.Fire"]
    
    @activation_required
    required_tags: ["Status.Alive", "Status.CanCast"]
    
    @activation_blocked
    blocked_tags: ["Status.CC.Stunned", "Status.CC.Silenced", "Status.Dead"]
    
    @activation_owned
    owned_tags: ["Status.Casting", "Status.Busy"]
    
    @cooldown_tags
    cooldown: ["Cooldown.Ability.Fireball"]
    
    fn activate():
        // Ability logic
        apply_damage(target, 100.0)
```

**Tag usage in effects:**
```kain
@effect
struct BurnEffect:
    duration: 5.0
    period: 1.0
    
    @asset_tags
    tags: ["Effect.Damage.Fire", "Effect.Type.DOT"]
    
    @granted_tags
    granted: ["Status.Debuff.Burning"]
    
    @application_required
    required: ["Status.Alive"]
    
    @application_blocked
    blocked: ["Status.Immune.Fire"]
    
    @modifiers
    damage_per_tick: -10.0 to Health
```

**Tag queries:**
```kain
@ability
struct ConditionalAbility:
    @tag_query
    can_activate: any(["Status.Buff.Strength", "Status.Buff.Empowered"]) 
                  and all(["Status.Alive", "Status.Conscious"])
                  and not(any(["Status.CC.Stunned", "Status.CC.Silenced"]))
    
    fn check_activation() -> Bool:
        return evaluate_query(can_activate, owner_tags)
```

**Inline tag operations:**
```kain
actor Player:
    state tags: TagContainer = []
    
    fn on_hit(damage: Float):
        if has_tag("Status.Immune.Physical"):
            return
        
        if has_any(["Status.Buff.Armor", "Status.Buff.Shield"]):
            damage = damage * 0.5
        
        if has_all(["Status.Vulnerable.Physical", "Status.Debuff.Weakness"]):
            damage = damage * 2.0
        
        apply_damage(damage)
        add_tag("Status.Hit")
```

**Tag events:**
```kain
actor Character:
    @on_tag_added("Status.CC.Stunned")
    fn on_stunned():
        cancel_abilities()
        play_animation("Stunned")
    
    @on_tag_removed("Status.CC.Stunned")
    fn on_unstunned():
        play_animation("Idle")
    
    @on_tag_count_changed("Status.Buff")
    fn on_buff_count_changed(tag: Tag, count: Int):
        update_buff_ui(count)
```

### Alternative Syntax (More Explicit)

**Tag definition with metadata:**
```kain
@gameplay_tags
tags:
    Ability.Attack.Melee:
        comment: "Melee attack abilities"
        category: "Ability"
        
    Ability.Attack.Ranged:
        comment: "Ranged attack abilities"
        category: "Ability"
        
    Status.CC.Stunned:
        comment: "Character is stunned and cannot act"
        category: "Status"
        replicate: true
        
    Status.Immune.Fire:
        comment: "Immune to fire damage"
        category: "Status"
        replicate: true
```

**Tag container fields:**
```kain
@ability
struct MeleeAbility:
    tags:
        ability: ["Ability.Attack.Melee"]
        activation_required: ["Status.Alive"]
        activation_blocked: ["Status.CC.Stunned", "Status.Dead"]
        activation_owned: ["Status.Attacking"]
        cooldown: ["Cooldown.Ability.Melee"]
```

---

## Codegen Strategy

### Native Tag Generation

**For each `@gameplay_tags` namespace, generate:**

**Header file (`MyGameTags.h`):**
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
        namespace Defend
        {
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block);
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Parry);
        }
    }
    
    namespace Status
    {
        MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alive);
        MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
        
        namespace CC
        {
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
            MYGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted);
        }
    }
}
```

**Implementation file (`MyGameTags.cpp`):**
```cpp
#include "MyGameTags.h"

namespace MyGameTags
{
    namespace Ability
    {
        namespace Attack
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(
                Melee, 
                "Ability.Attack.Melee", 
                "Melee attack abilities"
            );
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(
                Ranged, 
                "Ability.Attack.Ranged", 
                "Ranged attack abilities"
            );
        }
    }
    
    namespace Status
    {
        UE_DEFINE_GAMEPLAY_TAG(Alive, "Status.Alive");
        UE_DEFINE_GAMEPLAY_TAG(Dead, "Status.Dead");
        
        namespace CC
        {
            UE_DEFINE_GAMEPLAY_TAG(Stunned, "Status.CC.Stunned");
            UE_DEFINE_GAMEPLAY_TAG(Rooted, "Status.CC.Rooted");
        }
    }
}
```

### GameplayTags.ini Generation

**For designer-friendly tags, generate .ini file:**

**Config/Tags/GameplayTags.ini:**
```ini
[/Script/GameplayTags.GameplayTagsList]
; Ability Tags
GameplayTagList=(Tag="Ability.Attack.Melee",DevComment="Melee attack abilities")
GameplayTagList=(Tag="Ability.Attack.Ranged",DevComment="Ranged attack abilities")
GameplayTagList=(Tag="Ability.Defend.Block",DevComment="Block ability")
GameplayTagList=(Tag="Ability.Defend.Parry",DevComment="Parry ability")

; Status Tags
GameplayTagList=(Tag="Status.Alive",DevComment="Character is alive")
GameplayTagList=(Tag="Status.Dead",DevComment="Character is dead")
GameplayTagList=(Tag="Status.CC.Stunned",DevComment="Character is stunned")
GameplayTagList=(Tag="Status.CC.Rooted",DevComment="Character is rooted")

; Damage Tags
GameplayTagList=(Tag="Damage.Physical.Slash",DevComment="Slashing physical damage")
GameplayTagList=(Tag="Damage.Magical.Fire",DevComment="Fire magical damage")
```

### Ability Tag Integration

**For abilities with tag attributes:**

```cpp
UCLASS()
class UFireballAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UFireballAbility()
    {
        // Set ability tags
        AbilityTags.AddTag(MyGameTags::Ability::Attack::Ranged);
        AbilityTags.AddTag(MyGameTags::Ability::Magic::Fire);
        
        // Set activation requirements
        ActivationRequiredTags.AddTag(MyGameTags::Status::Alive);
        ActivationRequiredTags.AddTag(MyGameTags::Status::CanCast);
        
        // Set activation blocks
        ActivationBlockedTags.AddTag(MyGameTags::Status::CC::Stunned);
        ActivationBlockedTags.AddTag(MyGameTags::Status::CC::Silenced);
        
        // Set activation owned tags
        ActivationOwnedTags.AddTag(MyGameTags::Status::Casting);
    }
};
```

### Effect Tag Integration

**For effects with tag components:**

```cpp
UCLASS()
class UBurnEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UBurnEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        DurationMagnitude = FScalableFloat(5.0f);
        Period = 1.0f;
        
        // Add AssetTags component
        UAssetTagsGameplayEffectComponent* AssetTagsComp = 
            CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
        AssetTagsComp->InheritableAssetTags.AddTag(MyGameTags::Effect::Damage::Fire);
        AssetTagsComp->InheritableAssetTags.AddTag(MyGameTags::Effect::Type::DOT);
        
        // Add TargetTags component
        UTargetTagsGameplayEffectComponent* TargetTagsComp = 
            CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
        TargetTagsComp->InheritableGrantedTagsContainer.AddTag(MyGameTags::Status::Debuff::Burning);
        
        // Add TargetTagRequirements component
        UTargetTagRequirementsGameplayEffectComponent* RequirementsComp = 
            CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("Requirements"));
        RequirementsComp->ApplicationTagRequirements.RequireTags.AddTag(MyGameTags::Status::Alive);
        RequirementsComp->ApplicationTagRequirements.IgnoreTags.AddTag(MyGameTags::Status::Immune::Fire);
    }
};
```

### Tag Query Codegen

**For complex tag queries:**

```cpp
// KAIN: can_activate: any(["Status.Buffed", "Status.Empowered"]) and not("Status.Stunned")

FGameplayTagContainer BuffTags;
BuffTags.AddTag(MyGameTags::Status::Buff::Strength);
BuffTags.AddTag(MyGameTags::Status::Buff::Empowered);

FGameplayTagContainer BlockedTags;
BlockedTags.AddTag(MyGameTags::Status::CC::Stunned);

FGameplayTagQuery ActivationQuery = 
    FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
        .And(FGameplayTagQuery::MakeQuery_MatchNoTags(BlockedTags));

bool bCanActivate = ActivationQuery.Matches(OwnerTags);
```


### Tag Event Codegen

**For tag event handlers:**

```cpp
// KAIN: @on_tag_added("Status.CC.Stunned")

void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->RegisterGameplayTagEvent(
            MyGameTags::Status::CC::Stunned,
            EGameplayTagEventType::NewOrRemoved
        ).AddUObject(this, &AMyCharacter::OnStunnedTagChanged);
    }
}

void AMyCharacter::OnStunnedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Tag added
        OnStunned();
    }
    else
    {
        // Tag removed
        OnUnstunned();
    }
}
```

---

## Integration with Abilities/Effects

### Ability Activation Flow with Tags

**Complete activation check:**

```cpp
bool UGameplayAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // 1. Check if blocked by tags
    if (ActorInfo->AbilitySystemComponent->AreAbilityTagsBlocked(AbilityTags))
    {
        if (OptionalRelevantTags)
        {
            ActorInfo->AbilitySystemComponent->GetBlockedAbilityTags(*OptionalRelevantTags);
        }
        return false;
    }
    
    // 2. Check activation blocked tags
    if (ActivationBlockedTags.Num() > 0)
    {
        if (ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(ActivationBlockedTags))
        {
            if (OptionalRelevantTags)
            {
                ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(*OptionalRelevantTags);
            }
            return false;
        }
    }
    
    // 3. Check activation required tags
    if (ActivationRequiredTags.Num() > 0)
    {
        if (!ActorInfo->AbilitySystemComponent->HasAllMatchingGameplayTags(ActivationRequiredTags))
        {
            if (OptionalRelevantTags)
            {
                ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(*OptionalRelevantTags);
            }
            return false;
        }
    }
    
    // 4. Check source tags
    if (SourceTags)
    {
        if (SourceBlockedTags.Num() > 0 && SourceTags->HasAny(SourceBlockedTags))
        {
            return false;
        }
        
        if (SourceRequiredTags.Num() > 0 && !SourceTags->HasAll(SourceRequiredTags))
        {
            return false;
        }
    }
    
    // 5. Check target tags
    if (TargetTags)
    {
        if (TargetBlockedTags.Num() > 0 && TargetTags->HasAny(TargetBlockedTags))
        {
            return false;
        }
        
        if (TargetRequiredTags.Num() > 0 && !TargetTags->HasAll(TargetRequiredTags))
        {
            return false;
        }
    }
    
    // 6. Check cooldown
    if (!CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }
    
    // 7. Check cost
    if (!CheckCost(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }
    
    return true;
}
```

### Effect Application Flow with Tags

**Complete application check:**

```cpp
bool UAbilitySystemComponent::CanApplyGameplayEffect(
    const UGameplayEffect* GameplayEffect,
    const FGameplayEffectSpec& Spec) const
{
    // 1. Check immunity
    if (UImmunityGameplayEffectComponent* ImmunityComp = 
        GameplayEffect->FindComponent<UImmunityGameplayEffectComponent>())
    {
        const FGameplayTagContainer& EffectTags = Spec.Def->GetAssetTags();
        if (HasAnyMatchingGameplayTags(ImmunityComp->ImmunityTags))
        {
            // Target is immune to this effect
            return false;
        }
    }
    
    // 2. Check application requirements
    if (UTargetTagRequirementsGameplayEffectComponent* RequirementsComp = 
        GameplayEffect->FindComponent<UTargetTagRequirementsGameplayEffectComponent>())
    {
        const FGameplayTagRequirements& AppReqs = RequirementsComp->ApplicationTagRequirements;
        
        // Check required tags
        if (AppReqs.RequireTags.Num() > 0)
        {
            if (!HasAllMatchingGameplayTags(AppReqs.RequireTags))
            {
                return false;
            }
        }
        
        // Check ignored tags
        if (AppReqs.IgnoreTags.Num() > 0)
        {
            if (HasAnyMatchingGameplayTags(AppReqs.IgnoreTags))
            {
                return false;
            }
        }
    }
    
    // 3. Check custom application requirements
    if (UCustomCanApplyGameplayEffectComponent* CustomComp = 
        GameplayEffect->FindComponent<UCustomCanApplyGameplayEffectComponent>())
    {
        if (!CustomComp->CanApplyGameplayEffect(this, Spec))
        {
            return false;
        }
    }
    
    return true;
}
```

### Tag-Based Ability Cancellation

**Cancel abilities by tags:**

```cpp
void UAbilitySystemComponent::CancelAbilities(
    const FGameplayTagContainer* WithTags,
    const FGameplayTagContainer* WithoutTags,
    UGameplayAbility* Ignore)
{
    for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
    {
        if (!Spec.IsActive())
        {
            continue;
        }
        
        UGameplayAbility* Ability = Spec.Ability;
        if (Ability == Ignore)
        {
            continue;
        }
        
        // Check WithTags
        if (WithTags && WithTags->Num() > 0)
        {
            if (!Ability->AbilityTags.HasAny(*WithTags))
            {
                continue;
            }
        }
        
        // Check WithoutTags
        if (WithoutTags && WithoutTags->Num() > 0)
        {
            if (Ability->AbilityTags.HasAny(*WithoutTags))
            {
                continue;
            }
        }
        
        // Cancel the ability
        Ability->CancelAbility(Spec.Handle, ActorInfo.Get(), 
            Spec.ActivationInfo, true);
    }
}
```

---

## Editor Support

### Tag Picker Metadata

**Generate metadata for tag picker:**

```cpp
// In KAIN.toml or tag definition
[gameplay_tags]
categories = [
    { name = "Ability", color = "#FF0000" },
    { name = "Status", color = "#00FF00" },
    { name = "Damage", color = "#0000FF" },
]

filters = [
    { name = "AbilityTagCategory", includes = ["Ability.*"] },
    { name = "StatusTagCategory", includes = ["Status.*"] },
    { name = "DamageTagCategory", includes = ["Damage.*"] },
]
```

**Generated UPROPERTY meta:**

```cpp
// Ability tags only
UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="AbilityTagCategory"))
FGameplayTagContainer AbilityTags;

// Status tags only
UPROPERTY(EditDefaultsOnly, Category = Tags, meta=(Categories="StatusTagCategory"))
FGameplayTagContainer StatusTags;

// All tags
UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer AllTags;
```

### Tag Validation

**Compile-time validation:**

```kain
@ability
struct InvalidAbility:
    @tags
    ability_tags: ["Ability.Attack.Melee"]
    
    @activation_blocked
    blocked_tags: ["Ability.Attack.Melee"]  // ERROR: Cannot block own ability tags!
```

**Runtime validation:**

```cpp
// Validate tag exists
if (!UGameplayTagsManager::Get().IsValidGameplayTagString(TagString))
{
    UE_LOG(LogTemp, Error, TEXT("Invalid tag: %s"), *TagString);
}

// Validate tag hierarchy
FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack"));
FGameplayTag ChildTag = FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Melee"));
check(ChildTag.MatchesTag(ParentTag));  // Child should match parent
```

---

## Best Practices

### Tag Naming Conventions

**1. Use clear hierarchy:**
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

**2. Use consistent prefixes:**
```
✅ GOOD:
Ability.*       — Abilities
Status.*        — Character states
Effect.*        — Effect metadata
Damage.*        — Damage types
Cooldown.*      — Cooldown tags
Event.*         — Gameplay events

❌ BAD:
Mixed prefixes, no clear organization
```

**3. Use descriptive names:**
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

### Tag Organization

**1. Separate by feature:**
```
Config/Tags/
├── CoreTags.ini          # Core game tags
├── AbilityTags.ini       # Ability system tags
├── CombatTags.ini        # Combat-specific tags
├── StatusTags.ini        # Status effects
└── WeaponTags.ini        # Weapon system tags
```

**2. Use native tags for hot paths:**
```cpp
// Hot path: checked every frame
if (ASC->HasMatchingGameplayTag(MyGameTags::Status::Alive))
{
    // Use native tag for performance
}

// Cold path: checked rarely
FGameplayTag RareTag = FGameplayTag::RequestGameplayTag(FName("Event.Rare.Occurrence"));
```

**3. Document tag purpose:**
```ini
GameplayTagList=(Tag="Status.CC.Stunned",DevComment="Character cannot move or act. Duration-based.")
GameplayTagList=(Tag="Status.Immune.Fire",DevComment="Immune to all fire damage and effects.")
```

### Tag Usage Patterns

**1. Use tag events instead of polling:**
```cpp
// ✅ GOOD: Event-driven
ASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved)
    .AddUObject(this, &UMyClass::OnStunnedChanged);

// ❌ BAD: Polling every frame
void Tick(float DeltaTime)
{
    if (ASC->HasMatchingGameplayTag(StunnedTag))
    {
        // ...
    }
}
```

**2. Cache tag containers:**
```cpp
// ✅ GOOD: Cache container
static FGameplayTagContainer CCTags;
if (CCTags.IsEmpty())
{
    CCTags.AddTag(MyGameTags::Status::CC::Stunned);
    CCTags.AddTag(MyGameTags::Status::CC::Rooted);
    CCTags.AddTag(MyGameTags::Status::CC::Silenced);
}
bool bHasCC = ASC->HasAnyMatchingGameplayTags(CCTags);

// ❌ BAD: Create container every time
FGameplayTagContainer CCTags;
CCTags.AddTag(StunnedTag);
CCTags.AddTag(RootedTag);
bool bHasCC = ASC->HasAnyMatchingGameplayTags(CCTags);
```

**3. Use queries for complex logic:**
```cpp
// ✅ GOOD: Query for complex conditions
FGameplayTagQuery CanActivateQuery = 
    FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
        .And(FGameplayTagQuery::MakeQuery_MatchAllTags(RequiredTags))
        .And(FGameplayTagQuery::MakeQuery_MatchNoTags(BlockedTags));

// ❌ BAD: Manual checks
bool bCanActivate = (ASC->HasAnyMatchingGameplayTags(BuffTags) ||
                     ASC->HasMatchingGameplayTag(EmpoweredTag)) &&
                    ASC->HasAllMatchingGameplayTags(RequiredTags) &&
                    !ASC->HasAnyMatchingGameplayTags(BlockedTags);
```

### Common Pitfalls

**1. Don't use tags for data:**
```cpp
// ❌ BAD: Using tags to store values
FGameplayTag Health_100 = FGameplayTag::RequestGameplayTag(FName("Health.100"));
FGameplayTag Health_50 = FGameplayTag::RequestGameplayTag(FName("Health.50"));

// ✅ GOOD: Use attributes for data
float Health = ASC->GetNumericAttribute(HealthAttribute);
```

**2. Don't create too many tags:**
```cpp
// ❌ BAD: Tag explosion
Weapon.Rifle.AK47.Level1
Weapon.Rifle.AK47.Level2
Weapon.Rifle.AK47.Level3
// ... 100 more weapon tags

// ✅ GOOD: Use data assets
Weapon.Type.Rifle
// Store weapon data in UDataAsset
```

**3. Don't forget replication:**
```cpp
// ❌ BAD: Tags not replicated
ASC->AddLooseGameplayTag(ImportantTag);  // Not replicated!

// ✅ GOOD: Specify replication
ASC->AddLooseGameplayTag(ImportantTag, 1, EGameplayTagReplicationState::Full);
```

---

## Summary

**GameplayTags are the foundation of GAS:**
- Hierarchical string identifiers stored as numeric IDs
- Fast comparison, flexible matching
- Used for ability activation, effect application, state tracking
- Three registration methods: native C++, .ini files, data tables
- Support complex queries with AND/OR/NOT logic
- Replicated efficiently with delta compression
- Critical for performance — cache tags, use events, minimize polling

**For KAIN:**
- Generate native tags for compile-time safety
- Generate .ini files for designer-friendly editing
- Integrate seamlessly with ability/effect syntax
- Support tag queries with readable syntax
- Provide tag event decorators
- Generate editor metadata for tag picker

**Next steps:**
1. Implement tag namespace parser
2. Generate native tag headers/implementations
3. Generate GameplayTags.ini files
4. Integrate with ability/effect codegen
5. Implement tag query parser and codegen
6. Add tag event decorator support
7. Generate editor metadata

---

**References:**
- [Lyra GameplayTags](https://www.unrealcode.net/LyraGASGameplayTags/)
- [UE5 FGameplayTag API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GameplayTags/FGameplayTag)
- [UE5 FGameplayTagQuery API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GameplayTags/FGameplayTagQuery)
- SourceGAS: `GameplayAbilities/Public/Abilities/GameplayAbility.h`
- SourceGAS: `GameplayAbilities/Public/GameplayEffect.h`
- LyraGAS: `LyraGame/LyraGameplayTags.h`
- NinjaGAS: `NinjaGAS/Public/NinjaGASTags.h`
