# Gameplay Ability System (GAS) — Complete Architecture Analysis

> **Comprehensive analysis of UE5's Gameplay Ability System for KAIN `ue5-gas` crate implementation**

![Status](https://img.shields.io/badge/Analysis-Complete-brightgreen)
![Source](https://img.shields.io/badge/Source-UE5%20%2B%20Lyra-blue)
![Target](https://img.shields.io/badge/Target-KAIN%20Compiler-orange)

---

## Table of Contents

1. [Overview](#overview)
2. [Core Components](#core-components)
3. [Attribute Sets](#attribute-sets)
4. [Gameplay Abilities](#gameplay-abilities)
5. [Gameplay Effects](#gameplay-effects)
6. [Gameplay Tags](#gameplay-tags)
7. [Ability System Component](#ability-system-component)
8. [Replication & Networking](#replication--networking)
9. [Pattern Extraction](#pattern-extraction)
10. [KAIN Syntax Proposals](#kain-syntax-proposals)
11. [Codegen Requirements](#codegen-requirements)
12. [Integration Strategy](#integration-strategy)
13. [Advanced Features](#advanced-features)
14. [Testing Strategy](#testing-strategy)

---

## Overview

### What is GAS?

The Gameplay Ability System (GAS) is UE5's official framework for implementing abilities, attributes, and effects in multiplayer games. It provides:

- **Attribute Management** — Health, mana, stamina, etc. with automatic replication
- **Ability Activation** — Input-driven or event-driven ability execution with prediction
- **Effect Application** — Instant, duration, or infinite modifications to attributes
- **Tag-Based Logic** — Gameplay tags control ability activation, blocking, and requirements
- **Network Prediction** — Client-side prediction with server reconciliation

### GAS Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    UAbilitySystemComponent                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Attribute    │  │ Active       │  │ Activatable  │          │
│  │ Sets         │  │ Gameplay     │  │ Abilities    │          │
│  │              │  │ Effects      │  │              │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         ▲                 ▲                  ▲                   │
│         │                 │                  │                   │
│         │                 │                  │                   │
└─────────┼─────────────────┼──────────────────┼───────────────────┘
          │                 │                  │
          │                 │                  │
    ┌─────▼─────┐     ┌─────▼─────┐     ┌─────▼─────┐
    │ UAttribute│     │ UGameplay │     │ UGameplay │
    │ Set       │     │ Effect    │     │ Ability   │
    │           │     │           │     │           │
    │ - Health  │     │ - Instant │     │ - Activate│
    │ - MaxHeal │     │ - Duration│     │ - Commit  │
    │ - Damage  │     │ - Infinite│     │ - End     │
    └───────────┘     └───────────┘     └───────────┘
          │                 │                  │
          │                 │                  │
          └─────────────────┴──────────────────┘
                            │
                    ┌───────▼────────┐
                    │ FGameplayTag   │
                    │ Container      │
                    │                │
                    │ - Ability.Jump │
                    │ - Status.Stun  │
                    │ - Damage.Fire  │
                    └────────────────┘
```

### Key Principles

1. **Data-Driven** — Abilities, effects, and attributes are assets (UObjects)
2. **Tag-Based** — Gameplay tags control all logic (activation, blocking, requirements)
3. **Replicated** — Automatic network replication with prediction support
4. **Modular** — Attribute sets, abilities, and effects are independent and composable
5. **Event-Driven** — Delegates for attribute changes, ability events, effect application

---

## Core Components

### The Big Four

| Component | Purpose | Replication | Instancing |
|-----------|---------|-------------|------------|
| **UAttributeSet** | Holds attributes (health, mana, etc.) | Per-attribute | Per-actor |
| **UGameplayAbility** | Defines ability logic | Activation only | Per-execution or per-actor |
| **UGameplayEffect** | Modifies attributes | Full spec | N/A (applied to container) |
| **UAbilitySystemComponent** | Orchestrates everything | Full | Per-actor |

---

### Gameplay Ability Core Structure (from GameplayAbility.h)

```cpp
UCLASS(Blueprintable, MinimalAPI)
class UGameplayAbility : public UObject, public IGameplayTaskOwnerInterface
{
    GENERATED_UCLASS_BODY()
    
public:
    // ===== ACTIVATION =====
    
    // Attempts to activate the ability
    virtual bool TryActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                     const FGameplayAbilityActivationInfo ActivationInfo, 
                                     const FGameplayEventData* TriggerEventData = nullptr);
    
    // Main ability logic - override this in child classes
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                  const FGameplayAbilityActivationInfo ActivationInfo, 
                                  const FGameplayEventData* TriggerEventData);
    
    // Commits resources (cost + cooldown)
    virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                               const FGameplayAbilityActivationInfo ActivationInfo, 
                               OUT FGameplayTagContainer* OptionalRelevantTags = nullptr);
    
    // Ends the ability
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                            const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled);
    
    // Cancels the ability
    virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility);
    
    // ===== VALIDATION =====
    
    // Returns true if this ability can be activated right now
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                     const FGameplayTagContainer* SourceTags = nullptr, 
                                     const FGameplayTagContainer* TargetTags = nullptr, 
                                     OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
    
    // Checks if ability satisfies tag requirements
    virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, 
                                                     const FGameplayTagContainer* SourceTags = nullptr, 
                                                     const FGameplayTagContainer* TargetTags = nullptr, 
                                                     OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
    
    // ===== COST & COOLDOWN =====
    
    // Returns the gameplay effect used to determine cooldown
    virtual UGameplayEffect* GetCooldownGameplayEffect() const;
    
    // Returns the gameplay effect used to apply cost
    virtual UGameplayEffect* GetCostGameplayEffect() const;
    
    // Checks cooldown
    virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                               OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
    
    // Applies cooldown
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                               const FGameplayAbilityActivationInfo ActivationInfo) const;
    
    // Checks cost
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                           OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
    
    // Applies cost
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                           const FGameplayAbilityActivationInfo ActivationInfo) const;
    
    // ===== TAGS =====
    
    // Tags this ability has
    UPROPERTY(EditDefaultsOnly, Category = Tags, DisplayName="AssetTags (Default AbilityTags)", meta=(Categories="AbilityTagCategory"))
    FGameplayTagContainer AbilityTags;
    
    // Tags to cancel when this ability activates
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer CancelAbilitiesWithTag;
    
    // Tags to block while this ability is active
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer BlockAbilitiesWithTag;
    
    // Tags required on the activating actor to use this ability
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationOwnedTags;
    
    // Tags that must be present on the activating actor to use this ability
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationRequiredTags;
    
    // Tags that must NOT be present on the activating actor to use this ability
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationBlockedTags;
    
    // Tags to apply to the activating actor while this ability is active
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FGameplayTagContainer ActivationOwnedTags;
    
    // ===== INSTANCING & REPLICATION =====
    
    // How the ability is instanced when executed
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    EGameplayAbilityInstancingPolicy::Type InstancingPolicy;
    
    // How the ability replicates state/events
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    EGameplayAbilityReplicationPolicy::Type ReplicationPolicy;
    
    // Where the ability executes on the network
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    EGameplayAbilityNetExecutionPolicy::Type NetExecutionPolicy;
    
    // Network security policy
    UPROPERTY(EditDefaultsOnly, Category = Advanced)
    EGameplayAbilityNetSecurityPolicy::Type NetSecurityPolicy;
    
    // ===== INPUT =====
    
    // If true, this ability will always replicate input press/release events to the server
    UPROPERTY(EditDefaultsOnly, Category = Input)
    bool bReplicateInputDirectly;
    
    // Input binding stub
    virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                              const FGameplayAbilityActivationInfo ActivationInfo) {};
    
    // Input binding stub
    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                               const FGameplayAbilityActivationInfo ActivationInfo) {};
};
```

### Instancing Policies

| Policy | Description | Use Case |
|--------|-------------|----------|
| **InstancedPerExecution** | New instance created each time ability activates | Most abilities (default) |
| **InstancedPerActor** | One instance per actor, reused | Passive abilities, toggles |
| **NonInstanced** | No instance, CDO used directly | Simple abilities with no state |

### Replication Policies

| Policy | Description | Use Case |
|--------|-------------|----------|
| **ReplicateNo** | No replication | Local-only abilities |
| **ReplicateYes** | Full replication | Multiplayer abilities |

### Net Execution Policies

| Policy | Description | Use Case |
|--------|-------------|----------|
| **LocalPredicted** | Client predicts, server confirms | Most multiplayer abilities |
| **LocalOnly** | Only executes on local client | Cosmetic abilities |
| **ServerInitiated** | Server only, no prediction | Server-authoritative abilities |
| **ServerOnly** | Server only, no client execution | Admin abilities |

### Ability Lifecycle

```
1. Input / Event Trigger
   ↓
2. TryActivateAbility()
   ↓
3. CanActivateAbility() — check tags, cooldown, cost
   ↓
4. ActivateAbility() — main ability logic
   ↓
5. CommitAbility() — spend resources, apply cooldown
   ↓
6. [Ability Logic Executes]
   ↓
7. EndAbility() — cleanup, remove tags
```

### Lyra Ability Example Pattern

```cpp
UCLASS()
class ULyraGameplayAbility_Jump : public ULyraGameplayAbility
{
    GENERATED_BODY()
    
public:
    ULyraGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
protected:
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                     const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, 
                                     FGameplayTagContainer* OptionalRelevantTags) const override;
    
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                  const FGameplayAbilityActivationInfo ActivationInfo, 
                                  const FGameplayEventData* TriggerEventData) override;
    
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                            const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    
    // Character jump logic
    UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
    void CharacterJumpStart();
    
    UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
    void CharacterJumpStop();
};
```

---

## Gameplay Effects

### What Are Gameplay Effects?

Gameplay effects are data assets that modify attributes. They:

- Subclass `UGameplayEffect`
- Define modifiers (which attributes to modify and by how much)
- Support duration types (instant, duration, infinite)
- Support stacking rules
- Support tag requirements (application, removal, ongoing)
- Support execution calculations (custom logic)
- Support conditional effects (apply other effects on success)

### Core Structure (from GameplayEffect.h)

```cpp
UCLASS(BlueprintType)
class UGameplayEffect : public UObject
{
    GENERATED_UCLASS_BODY()
    
public:
    // ===== DURATION =====
    
    // Policy for the duration of this effect
    UPROPERTY(EditDefaultsOnly, Category = Duration)
    EGameplayEffectDurationType DurationPolicy;
    
    // Duration magnitude (if HasDuration)
    UPROPERTY(EditDefaultsOnly, Category = Duration)
    FGameplayEffectModifierMagnitude DurationMagnitude;
    
    // Period for periodic effects (0 = not periodic)
    UPROPERTY(EditDefaultsOnly, Category = Period)
    FScalableFloat Period;
    
    // If true, effect executes on application (in addition to periodic)
    UPROPERTY(EditDefaultsOnly, Category = Period)
    bool bExecutePeriodicEffectOnApplication;
    
    // ===== MODIFIERS =====
    
    // Array of modifiers that will affect attributes
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
    TArray<FGameplayModifierInfo> Modifiers;
    
    // Executions that will run when this effect is applied
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Execution)
    TArray<FGameplayEffectExecutionDefinition> Executions;
    
    // ===== STACKING =====
    
    // How this effect stacks with other instances of itself
    UPROPERTY(EditDefaultsOnly, Category = Stacking)
    EGameplayEffectStackingType StackingType;
    
    // Maximum number of stacks
    UPROPERTY(EditDefaultsOnly, Category = Stacking)
    int32 StackLimitCount;
    
    // Policy for duration when stacking
    UPROPERTY(EditDefaultsOnly, Category = Stacking)
    EGameplayEffectStackingDurationPolicy StackDurationRefreshPolicy;
    
    // Policy for period when stacking
    UPROPERTY(EditDefaultsOnly, Category = Stacking)
    EGameplayEffectStackingPeriodPolicy StackPeriodResetPolicy;
    
    // Policy for what happens when stack expires
    UPROPERTY(EditDefaultsOnly, Category = Stacking)
    EGameplayEffectStackingExpirationPolicy StackExpirationPolicy;
    
    // ===== TAGS =====
    
    // Tags this effect has
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FInheritedTagContainer InheritableOwnedTagsContainer;
    
    // Tags to apply to the target while this effect is active
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FInheritedTagContainer InheritableGrantedTagsContainer;
    
    // Tags required on the target for this effect to apply
    UPROPERTY(EditDefaultsOnly, Category = "Application Tag Requirements")
    FGameplayTagRequirements ApplicationTagRequirements;
    
    // Tags required on the target for this effect to remain active
    UPROPERTY(EditDefaultsOnly, Category = "Ongoing Tag Requirements")
    FGameplayTagRequirements OngoingTagRequirements;
    
    // Tags that will remove this effect if applied to the target
    UPROPERTY(EditDefaultsOnly, Category = "Removal Tag Requirements")
    FGameplayTagRequirements RemovalTagRequirements;
    
    // Tags to remove from the target when this effect is applied
    UPROPERTY(EditDefaultsOnly, Category = Tags)
    FInheritedTagContainer RemoveGameplayEffectsWithTags;
    
    // ===== IMMUNITY =====
    
    // Grants immunity to effects with these tags
    UPROPERTY(EditDefaultsOnly, Category = Immunity)
    FGameplayTagRequirements GrantedApplicationImmunityTags;
    
    // Custom application requirements
    UPROPERTY(EditDefaultsOnly, Instanced, Category = Application)
    TArray<TObjectPtr<UGameplayEffectCustomApplicationRequirement>> ApplicationRequirements;
    
    // ===== CONDITIONAL EFFECTS =====
    
    // Effects to apply if this effect successfully applies
    UPROPERTY(EditDefaultsOnly, Category = "Conditional Gameplay Effects")
    TArray<FConditionalGameplayEffect> ConditionalGameplayEffects;
    
    // ===== OVERFLOW =====
    
    // Effects to apply if a modifier overflows (e.g., healing when at max health)
    UPROPERTY(EditDefaultsOnly, Category = Overflow)
    TArray<TSubclassOf<UGameplayEffect>> OverflowEffects;
    
    // ===== GAMEPLAY CUES =====
    
    // Gameplay cues to trigger when this effect is applied/removed/executed
    UPROPERTY(EditDefaultsOnly, Category = Display)
    TArray<FGameplayEffectCue> GameplayCues;
};
```

### Modifier Structure

```cpp
USTRUCT(BlueprintType)
struct FGameplayModifierInfo
{
    GENERATED_BODY()
    
    // The attribute to modify
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier, meta=(FilterMetaTag="HideFromModifiers"))
    FGameplayAttribute Attribute;
    
    // The operation (Add, Multiply, Divide, Override)
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
    TEnumAsByte<EGameplayModOp::Type> ModifierOp;
    
    // The magnitude of the modifier
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
    FGameplayEffectModifierMagnitude ModifierMagnitude;
    
    // Evaluation channel settings
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
    FGameplayModEvaluationChannelSettings EvaluationChannelSettings;
    
    // Source tag requirements
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
    FGameplayTagRequirements SourceTags;
    
    // Target tag requirements
    UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
    FGameplayTagRequirements TargetTags;
};
```

### Modifier Operations

| Operation | Formula | Use Case |
|-----------|---------|----------|
| **Add** | `BaseValue + Modifier` | Flat bonuses (e.g., +10 health) |
| **Multiply** | `BaseValue * Modifier` | Percentage bonuses (e.g., +50% damage) |
| **Divide** | `BaseValue / Modifier` | Percentage reductions (e.g., /2 for half speed) |
| **Override** | `Modifier` | Set to specific value (e.g., stun = 0 movement) |

### Magnitude Calculation Types

| Type | Description | Use Case |
|------|-------------|----------|
| **ScalableFloat** | Simple float value (can scale with level) | Fixed values |
| **AttributeBased** | Based on another attribute | Damage based on attack rating |
| **CustomCalculationClass** | Custom C++ calculation | Complex formulas |
| **SetByCaller** | Set at runtime by code | Dynamic values |

### Duration Types

| Type | Description | Use Case |
|------|-------------|----------|
| **Instant** | Applies once, immediately | Damage, healing |
| **Infinite** | Lasts forever (until removed) | Passive buffs |
| **HasDuration** | Lasts for specified duration | Temporary buffs/debuffs |

### Stacking Types

| Type | Description | Use Case |
|------|-------------|----------|
| **None** | No stacking, each instance is separate | Most effects |
| **AggregateBySource** | Stack per source | Bleed from multiple enemies |
| **AggregateByTarget** | Stack on target | Poison stacks |

---

## Gameplay Tags (CRITICAL!)

### What Are Gameplay Tags?

Gameplay tags are hierarchical string identifiers used throughout GAS for:

- **Ability Requirements** — "Ability.Jump" requires "Status.Grounded"
- **Ability Blocking** — "Status.Stunned" blocks "Ability.Attack"
- **Effect Application** — "Damage.Fire" effect requires "Weakness.Fire" tag
- **Event Triggering** — "Event.Death" triggers death abilities
- **Categorization** — "Item.Weapon.Sword" vs "Item.Weapon.Bow"

**WITHOUT GAMEPLAY TAGS, GAS DOES NOT WORK.** They are the glue that holds everything together.

### Core Structure

```cpp
// FGameplayTag — single tag
USTRUCT(BlueprintType)
struct FGameplayTag
{
    GENERATED_BODY()
    
    // The tag name (e.g., "Ability.Jump")
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GameplayTags)
    FName TagName;
    
    // Checks if this tag matches another tag (supports partial matching)
    bool MatchesTag(const FGameplayTag& TagToCheck) const;
    
    // Checks if this tag matches any tag in a container
    bool MatchesAny(const FGameplayTagContainer& ContainerToCheck) const;
    
    // Checks if this tag matches all tags in a container
    bool MatchesAll(const FGameplayTagContainer& ContainerToCheck) const;
};

// FGameplayTagContainer — collection of tags
USTRUCT(BlueprintType)
struct FGameplayTagContainer
{
    GENERATED_BODY()
    
    // Array of gameplay tags
    UPROPERTY(BlueprintReadWrite, Category = GameplayTags)
    TArray<FGameplayTag> GameplayTags;
    
    // Add a tag
    void AddTag(const FGameplayTag& TagToAdd);
    
    // Remove a tag
    void RemoveTag(const FGameplayTag& TagToRemove);
    
    // Check if container has tag
    bool HasTag(const FGameplayTag& TagToCheck) const;
    
    // Check if container has any tag from another container
    bool HasAny(const FGameplayTagContainer& ContainerToCheck) const;
    
    // Check if container has all tags from another container
    bool HasAll(const FGameplayTagContainer& ContainerToCheck) const;
};

// FGameplayTagRequirements — tag requirements for abilities/effects
USTRUCT(BlueprintType)
struct FGameplayTagRequirements
{
    GENERATED_BODY()
    
    // Tags that must be present
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayTags)
    FGameplayTagContainer RequireTags;
    
    // Tags that must NOT be present
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayTags)
    FGameplayTagContainer IgnoreTags;
    
    // Checks if requirements are met
    bool RequirementsMet(const FGameplayTagContainer& Container) const;
};
```

### Tag Hierarchy

Tags are hierarchical with `.` as separator:

```
Ability
├── Ability.Jump
├── Ability.Sprint
├── Ability.Attack
│   ├── Ability.Attack.Melee
│   └── Ability.Attack.Ranged
└── Ability.Skill
    ├── Ability.Skill.Fireball
    └── Ability.Skill.Heal

Status
├── Status.Stunned
├── Status.Rooted
├── Status.Silenced
└── Status.Invulnerable

Damage
├── Damage.Physical
├── Damage.Fire
├── Damage.Ice
└── Damage.Lightning
```

### Tag Matching

- **Exact Match** — "Ability.Jump" matches "Ability.Jump" only
- **Partial Match** — "Ability" matches "Ability.Jump", "Ability.Sprint", etc.
- **Parent Match** — "Ability.Attack" matches "Ability.Attack.Melee", "Ability.Attack.Ranged"

### Tag Registration

Tags must be registered in `DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Ability.Jump",DevComment="Jump ability")
+GameplayTagList=(Tag="Ability.Sprint",DevComment="Sprint ability")
+GameplayTagList=(Tag="Status.Stunned",DevComment="Stunned status")
+GameplayTagList=(Tag="Damage.Fire",DevComment="Fire damage type")
```

Or in C++ using macros:

```cpp
// In header
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Jump);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Stunned);

// In cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Jump, "Ability.Jump");
UE_DEFINE_GAMEPLAY_TAG(TAG_Status_Stunned, "Status.Stunned");
```

### Lyra Tag Usage Example

```cpp
// LyraGameplayTags.h
namespace LyraGameplayTags
{
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_IsDead);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cooldown);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Cost);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsBlocked);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_TagsMissing);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Networking);
    
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);
    
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
    LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);
}

// LyraGameplayTags.cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown");
// ... etc
```

### Tag Usage in Abilities

```cpp
// In ability class
UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer AbilityTags;  // Tags this ability has

UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer ActivationRequiredTags;  // Must have these to activate

UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer ActivationBlockedTags;  // Cannot have these to activate

UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer CancelAbilitiesWithTag;  // Cancel abilities with these tags

UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer BlockAbilitiesWithTag;  // Block abilities with these tags while active

UPROPERTY(EditDefaultsOnly, Category = Tags)
FGameplayTagContainer ActivationOwnedTags;  // Apply these tags while active
```

### Tag Usage in Effects

```cpp
// In gameplay effect
UPROPERTY(EditDefaultsOnly, Category = Tags)
FInheritedTagContainer InheritableOwnedTagsContainer;  // Tags this effect has

UPROPERTY(EditDefaultsOnly, Category = Tags)
FInheritedTagContainer InheritableGrantedTagsContainer;  // Tags to apply to target

UPROPERTY(EditDefaultsOnly, Category = "Application Tag Requirements")
FGameplayTagRequirements ApplicationTagRequirements;  // Required for application

UPROPERTY(EditDefaultsOnly, Category = "Ongoing Tag Requirements")
FGameplayTagRequirements OngoingTagRequirements;  // Required to stay active

UPROPERTY(EditDefaultsOnly, Category = "Removal Tag Requirements")
FGameplayTagRequirements RemovalTagRequirements;  // Tags that remove this effect
```

---

## Ability System Component

### What is the Ability System Component?

`UAbilitySystemComponent` is the orchestrator that ties everything together. It:

- **Manages Attribute Sets** — Holds and replicates attribute sets
- **Manages Active Effects** — Tracks active gameplay effects
- **Manages Abilities** — Grants, activates, and tracks abilities
- **Handles Replication** — Replicates attributes, effects, and ability activation
- **Handles Prediction** — Client-side prediction with server reconciliation
- **Handles Tags** — Aggregates tags from abilities, effects, and attributes

### Core Structure (from AbilitySystemComponent.h)

```cpp
UCLASS(ClassGroup=AbilitySystem, hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming), 
       editinlinenew, meta=(BlueprintSpawnableComponent), MinimalAPI)
class UAbilitySystemComponent : public UGameplayTasksComponent, public IGameplayTagAssetInterface
{
    GENERATED_UCLASS_BODY()
    
public:
    // ===== ATTRIBUTE SETS =====
    
    // Get an attribute set by class
    template <class T>
    const T* GetSet() const;
    
    // Add a new attribute set
    template <class T>
    const T* AddSet();
    
    // Check if component has attribute
    bool HasAttributeSetForAttribute(FGameplayAttribute Attribute) const;
    
    // Get attribute value
    float GetNumericAttribute(const FGameplayAttribute &Attribute) const;
    
    // Set attribute base value
    void SetNumericAttributeBase(const FGameplayAttribute &Attribute, float NewBaseValue);
    
    // Apply instant mod to attribute (bypasses gameplay effects)
    void ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude);
    
    // ===== GAMEPLAY EFFECTS =====
    
    // Apply gameplay effect spec to target
    virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpec& GameplayEffect, 
                                                                          UAbilitySystemComponent *Target, 
                                                                          FPredictionKey PredictionKey=FPredictionKey());
    
    // Apply gameplay effect spec to self
    virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec& GameplayEffect, 
                                                                        FPredictionKey PredictionKey = FPredictionKey());
    
    // Make outgoing gameplay effect spec
    virtual FGameplayEffectSpecHandle MakeOutgoingSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, 
                                                         float Level, 
                                                         FGameplayEffectContextHandle Context) const;
    
    // Remove active gameplay effect
    virtual bool RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1);
    
    // Get gameplay effect count
    int32 GetGameplayEffectCount(TSubclassOf<UGameplayEffect> SourceGameplayEffect, 
                                  UAbilitySystemComponent* OptionalInstigatorFilterComponent, 
                                  bool bEnforceOnGoingCheck = true) const;
    
    // Get gameplay effect duration
    float GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const;
    
    // ===== ABILITIES =====
    
    // Give ability to actor
    FGameplayAbilitySpecHandle GiveAbility(const FGameplayAbilitySpec& Spec);
    
    // Give ability and activate immediately
    FGameplayAbilitySpecHandle GiveAbilityAndActivateOnce(const FGameplayAbilitySpec& Spec);
    
    // Clear ability by handle
    void ClearAbility(const FGameplayAbilitySpecHandle& Handle);
    
    // Try activate ability by class
    bool TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> InAbilityToActivate, bool bAllowRemoteActivation = true);
    
    // Try activate ability by tag
    bool TryActivateAbilitiesByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = true);
    
    // Cancel ability by handle
    void CancelAbilityHandle(const FGameplayAbilitySpecHandle& AbilityHandle);
    
    // Cancel abilities with tag
    void CancelAbilities(const FGameplayTagContainer* WithTags=nullptr, const FGameplayTagContainer* WithoutTags=nullptr, 
                         UGameplayAbility* Ignore=nullptr);
    
    // ===== TAGS =====
    
    // Get all owned tags (from abilities, effects, etc.)
    void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const;
    
    // Check if has tag
    bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const;
    
    // Check if has any tag
    bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const;
    
    // Check if has all tags
    bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const;
    
    // Add loose gameplay tag (not from ability/effect)
    void AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count=1);
    
    // Remove loose gameplay tag
    void RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count=1);
    
    // ===== REPLICATION =====
    
    // Set replication mode
    virtual void SetReplicationMode(EGameplayEffectReplicationMode NewReplicationMode);
    
    // Replication mode
    EGameplayEffectReplicationMode ReplicationMode;
    
    // ===== DELEGATES =====
    
    // Called when gameplay effect is applied to self
    FOnGameplayEffectAppliedDelegate OnGameplayEffectAppliedDelegateToSelf;
    
    // Called when gameplay effect is applied to target
    FOnGameplayEffectAppliedDelegate OnGameplayEffectAppliedDelegateToTarget;
    
    // Called when active gameplay effect is added
    FOnGameplayEffectAppliedDelegate OnActiveGameplayEffectAddedDelegateToSelf;
    
    // Called when attribute changes
    FOnGameplayAttributeValueChange& GetGameplayAttributeValueChangeDelegate(FGameplayAttribute Attribute);
    
protected:
    // Spawned attribute sets
    UPROPERTY(Replicated)
    TArray<TObjectPtr<UAttributeSet>> SpawnedAttributes;
    
    // Active gameplay effects container
    UPROPERTY(Replicated)
    FActiveGameplayEffectsContainer ActiveGameplayEffects;
    
    // Activatable abilities
    UPROPERTY(Replicated)
    FGameplayAbilitySpecContainer ActivatableAbilities;
};
```

### Lyra Ability System Component Example

```cpp
UCLASS()
class ULyraAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()
    
public:
    // Initialize ability actor info
    virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
    
    // Ability input binding
    void AbilityInputTagPressed(const FGameplayTag& InputTag);
    void AbilityInputTagReleased(const FGameplayTag& InputTag);
    
    // Process ability input
    void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
    
    // Clear ability input
    void ClearAbilityInput();
    
    // Ability failed to activate
    void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);
    
protected:
    // Input pressed abilities
    TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
    
    // Input released abilities
    TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
    
    // Input held abilities
    TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
```

---

## KAIN Syntax Proposals

### Attribute Sets

**Proposed KAIN Syntax:**

```kain
@attribute_set
struct HealthSet:
    @attribute(replicated: true, rep_notify: true, hide_from_modifiers: true)
    health: Float = 100.0
    
    @attribute(replicated: true, rep_notify: true)
    max_health: Float = 100.0
    
    @attribute(meta: true)
    healing: Float = 0.0
    
    @attribute(meta: true, hide_from_modifiers: true)
    damage: Float = 0.0
    
    @delegate
    on_health_changed: AttributeEvent
    
    @delegate
    on_max_health_changed: AttributeEvent
    
    @delegate
    on_out_of_health: AttributeEvent
    
    fn pre_gameplay_effect_execute(data: GameplayEffectModCallbackData) -> Bool:
        if data.evaluated_data.attribute == get_damage_attribute():
            if data.evaluated_data.magnitude > 0.0:
                if data.target.has_matching_gameplay_tag("Gameplay.DamageImmunity"):
                    data.evaluated_data.magnitude = 0.0
                    return false
        return true
    
    fn post_gameplay_effect_execute(data: GameplayEffectModCallbackData):
        if data.evaluated_data.attribute == get_damage_attribute():
            set_health(clamp(get_health() - get_damage(), 0.0, get_max_health()))
            set_damage(0.0)
        elif data.evaluated_data.attribute == get_healing_attribute():
            set_health(clamp(get_health() + get_healing(), 0.0, get_max_health()))
            set_healing(0.0)
        
        if get_health() != health_before_change:
            on_health_changed.broadcast(instigator, causer, data.effect_spec, data.evaluated_data.magnitude, health_before_change, get_health())
        
        if get_health() <= 0.0 and not out_of_health:
            on_out_of_health.broadcast(instigator, causer, data.effect_spec, data.evaluated_data.magnitude, health_before_change, get_health())
    
    fn pre_attribute_change(attribute: GameplayAttribute, new_value: Float):
        clamp_attribute(attribute, new_value)
    
    fn clamp_attribute(attribute: GameplayAttribute, new_value: Float):
        if attribute == get_health_attribute():
            new_value = clamp(new_value, 0.0, get_max_health())
        elif attribute == get_max_health_attribute():
            new_value = max(new_value, 1.0)
```

**Generated C++ (HealthSet.h):**

```cpp
UCLASS(MinimalAPI, BlueprintType)
class UHealthSet : public UAttributeSet
{
    GENERATED_BODY()
    
public:
    UHealthSet();
    
    ATTRIBUTE_ACCESSORS(UHealthSet, Health);
    ATTRIBUTE_ACCESSORS(UHealthSet, MaxHealth);
    ATTRIBUTE_ACCESSORS(UHealthSet, Healing);
    ATTRIBUTE_ACCESSORS(UHealthSet, Damage);
    
    mutable FAttributeEvent OnHealthChanged;
    mutable FAttributeEvent OnMaxHealthChanged;
    mutable FAttributeEvent OnOutOfHealth;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);
    
    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
    
    virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    
    void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
    
private:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health", 
              Meta = (HideFromModifiers, AllowPrivateAccess = true))
    FGameplayAttributeData Health;
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", 
              Meta = (AllowPrivateAccess = true))
    FGameplayAttributeData MaxHealth;
    
    UPROPERTY(BlueprintReadOnly, Category="Health", Meta=(AllowPrivateAccess=true))
    FGameplayAttributeData Healing;
    
    UPROPERTY(BlueprintReadOnly, Category="Health", Meta=(HideFromModifiers, AllowPrivateAccess=true))
    FGameplayAttributeData Damage;
    
    bool bOutOfHealth;
    float HealthBeforeAttributeChange;
    float MaxHealthBeforeAttributeChange;
};
```

### Attribute Set Attribute Options

| KAIN Attribute | UE5 Mapping | Purpose |
|----------------|-------------|---------|
| `replicated: true` | `DOREPLIFETIME_CONDITION_NOTIFY` | Replicate to clients |
| `rep_notify: true` | `ReplicatedUsing = OnRep_X` | Generate RepNotify function |
| `hide_from_modifiers: true` | `Meta = (HideFromModifiers)` | Hide from modifier UI |
| `meta: true` | No replication | Temporary calculation attribute |
| `clamped: true` | Auto-generate clamping logic | Clamp in PreAttributeChange |
| `min: 0.0` | Clamp minimum | Minimum value |
| `max: 100.0` | Clamp maximum | Maximum value |

---

## Gameplay Abilities — KAIN Syntax

**Proposed KAIN Syntax:**

```kain
@ability
struct JumpAbility:
    @instancing(policy: "InstancedPerExecution")
    @replication(policy: "ReplicateYes")
    @net_execution(policy: "LocalPredicted")
    @net_security(policy: "ClientOrServer")
    
    @ability_tags
    tags: ["Ability.Jump"]
    
    @activation_required_tags
    required: ["Status.Grounded"]
    
    @activation_blocked_tags
    blocked: ["Status.Stunned", "Status.Rooted"]
    
    @activation_owned_tags
    owned: ["Status.Jumping"]
    
    @block_abilities_with_tag
    block: ["Ability.Sprint"]
    
    @cost
    effect: StaminaCostEffect
    
    @cooldown
    effect: JumpCooldownEffect
    
    fn can_activate_ability(handle: AbilitySpecHandle, actor_info: AbilityActorInfo, 
                            source_tags: TagContainer, target_tags: TagContainer) -> Bool:
        if not has_stamina(actor_info, 10.0):
            return false
        return true
    
    fn activate_ability(handle: AbilitySpecHandle, actor_info: AbilityActorInfo, 
                        activation_info: AbilityActivationInfo, trigger_event_data: GameplayEventData):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        
        let character = get_avatar_actor_from_actor_info()
        character.jump()
        
        end_ability(handle, actor_info, activation_info, true, false)
    
    fn end_ability(handle: AbilitySpecHandle, actor_info: AbilityActorInfo, 
                   activation_info: AbilityActivationInfo, replicate_end: Bool, was_cancelled: Bool):
        println("Jump ability ended")
```

**Generated C++ (JumpAbility.h):**

```cpp
UCLASS()
class UJumpAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UJumpAbility();
    
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                     const FGameplayTagContainer* SourceTags = nullptr, 
                                     const FGameplayTagContainer* TargetTags = nullptr, 
                                     OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                  const FGameplayAbilityActivationInfo ActivationInfo, 
                                  const FGameplayEventData* TriggerEventData) override;
    
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                            const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};

// Constructor
UJumpAbility::UJumpAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
    
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Jump")));
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Grounded")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Rooted")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Jumping")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Sprint")));
    
    CostGameplayEffectClass = UStaminaCostEffect::StaticClass();
    CooldownGameplayEffectClass = UJumpCooldownEffect::StaticClass();
}
```

### Ability Attribute Options

| KAIN Attribute | UE5 Mapping | Purpose |
|----------------|-------------|---------|
| `@instancing(policy: "X")` | `InstancingPolicy` | How ability is instanced |
| `@replication(policy: "X")` | `ReplicationPolicy` | How ability replicates |
| `@net_execution(policy: "X")` | `NetExecutionPolicy` | Where ability executes |
| `@net_security(policy: "X")` | `NetSecurityPolicy` | Security restrictions |
| `@ability_tags` | `AbilityTags` | Tags this ability has |
| `@activation_required_tags` | `ActivationRequiredTags` | Must have these tags |
| `@activation_blocked_tags` | `ActivationBlockedTags` | Cannot have these tags |
| `@activation_owned_tags` | `ActivationOwnedTags` | Apply these tags while active |
| `@block_abilities_with_tag` | `BlockAbilitiesWithTag` | Block abilities with these tags |
| `@cancel_abilities_with_tag` | `CancelAbilitiesWithTag` | Cancel abilities with these tags |
| `@cost` | `CostGameplayEffectClass` | Cost effect to apply |
| `@cooldown` | `CooldownGameplayEffectClass` | Cooldown effect to apply |

---

## Gameplay Effects — KAIN Syntax

**Proposed KAIN Syntax:**

```kain
@gameplay_effect
struct BurnEffect:
    @duration(type: "HasDuration")
    duration: 5.0
    
    @period
    period: 1.0
    execute_on_application: true
    
    @modifier(attribute: "Health", operation: "Add")
    damage_per_tick: -10.0
    
    @stacking
    type: "AggregateBySource"
    limit: 5
    duration_policy: "RefreshOnSuccessfulApplication"
    period_policy: "ResetOnSuccessfulApplication"
    expiration_policy: "RemoveSingleStackAndRefreshDuration"
    
    @owned_tags
    tags: ["Effect.Burn"]
    
    @granted_tags
    tags: ["Status.Burning"]
    
    @application_tag_requirements
    require: ["Weakness.Fire"]
    ignore: ["Immunity.Fire"]
    
    @ongoing_tag_requirements
    require: []
    ignore: ["Immunity.Fire"]
    
    @removal_tag_requirements
    require: ["Cleanse.Fire"]
    
    @gameplay_cues
    cues: ["GameplayCue.Burn.Start", "GameplayCue.Burn.Loop", "GameplayCue.Burn.End"]

@gameplay_effect
struct HealOverTimeEffect:
    @duration(type: "HasDuration")
    duration: 10.0
    
    @period
    period: 1.0
    execute_on_application: false
    
    @modifier(attribute: "Health", operation: "Add", magnitude_type: "ScalableFloat")
    healing_per_tick: 5.0
    
    @owned_tags
    tags: ["Effect.HealOverTime"]
    
    @granted_tags
    tags: ["Status.Regenerating"]

@gameplay_effect
struct DamageEffect:
    @duration(type: "Instant")
    
    @modifier(attribute: "Health", operation: "Add", magnitude_type: "SetByCaller")
    damage: 
        set_by_caller: "Damage.Amount"
    
    @owned_tags
    tags: ["Effect.Damage"]

@gameplay_effect
struct AttackBuffEffect:
    @duration(type: "Infinite")
    
    @modifier(attribute: "AttackPower", operation: "Multiply", magnitude_type: "AttributeBased")
    attack_multiplier:
        coefficient: 1.5
        backing_attribute: "Level"
        calculation_type: "AttributeMagnitude"
    
    @owned_tags
    tags: ["Effect.AttackBuff"]
    
    @granted_tags
    tags: ["Status.Buffed"]

@gameplay_effect
struct StunEffect:
    @duration(type: "HasDuration")
    duration: 3.0
    
    @modifier(attribute: "MovementSpeed", operation: "Override")
    movement_speed: 0.0
    
    @owned_tags
    tags: ["Effect.Stun"]
    
    @granted_tags
    tags: ["Status.Stunned"]
    
    @block_abilities_with_tag
    block: ["Ability.Attack", "Ability.Jump", "Ability.Sprint"]
```

**Generated C++ (BurnEffect.h):**

```cpp
UCLASS()
class UBurnEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UBurnEffect();
};

// Constructor (BurnEffect.cpp)
UBurnEffect::UBurnEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));
    
    Period = FScalableFloat(1.0f);
    bExecutePeriodicEffectOnApplication = true;
    
    // Add modifier
    FGameplayModifierInfo ModifierInfo;
    ModifierInfo.Attribute = UHealthSet::GetHealthAttribute();
    ModifierInfo.ModifierOp = EGameplayModOp::Additive;
    ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-10.0f));
    Modifiers.Add(ModifierInfo);
    
    // Stacking
    StackingType = EGameplayEffectStackingType::AggregateBySource;
    StackLimitCount = 5;
    StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
    StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
    StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
    
    // Tags
    InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Burn")));
    InheritableGrantedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Burning")));
    
    ApplicationTagRequirements.RequireTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Weakness.Fire")));
    ApplicationTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Immunity.Fire")));
    
    OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Immunity.Fire")));
    
    RemovalTagRequirements.RequireTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cleanse.Fire")));
    
    // Gameplay cues
    FGameplayEffectCue BurnCue;
    BurnCue.GameplayCueTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Burn.Start")));
    GameplayCues.Add(BurnCue);
}
```

### Gameplay Effect Attribute Options

| KAIN Attribute | UE5 Mapping | Purpose |
|----------------|-------------|---------|
| `@duration(type: "X")` | `DurationPolicy` | Instant, HasDuration, Infinite |
| `@period` | `Period`, `bExecutePeriodicEffectOnApplication` | Periodic execution |
| `@modifier` | `Modifiers` array | Attribute modifications |
| `@stacking` | Stacking properties | How effect stacks |
| `@owned_tags` | `InheritableOwnedTagsContainer` | Tags this effect has |
| `@granted_tags` | `InheritableGrantedTagsContainer` | Tags to apply to target |
| `@application_tag_requirements` | `ApplicationTagRequirements` | Required for application |
| `@ongoing_tag_requirements` | `OngoingTagRequirements` | Required to stay active |
| `@removal_tag_requirements` | `RemovalTagRequirements` | Tags that remove effect |
| `@gameplay_cues` | `GameplayCues` | Visual/audio cues |
| `@conditional_effects` | `ConditionalGameplayEffects` | Effects to apply on success |
| `@overflow_effects` | `OverflowEffects` | Effects to apply on overflow |

### Modifier Magnitude Types

```kain
# ScalableFloat
@modifier(attribute: "Health", operation: "Add", magnitude_type: "ScalableFloat")
damage: -10.0

# AttributeBased
@modifier(attribute: "Damage", operation: "Multiply", magnitude_type: "AttributeBased")
damage_multiplier:
    coefficient: 1.5
    backing_attribute: "AttackPower"
    calculation_type: "AttributeMagnitude"

# CustomCalculationClass
@modifier(attribute: "Health", operation: "Add", magnitude_type: "CustomCalculationClass")
damage:
    calculation_class: DamageCalculation

# SetByCaller
@modifier(attribute: "Health", operation: "Add", magnitude_type: "SetByCaller")
damage:
    set_by_caller: "Damage.Amount"
```

---

## Gameplay Tags — KAIN Syntax

**Proposed KAIN Syntax:**

```kain
@gameplay_tags
tags:
    # Abilities
    - "Ability.Jump"
    - "Ability.Sprint"
    - "Ability.Attack"
    - "Ability.Attack.Melee"
    - "Ability.Attack.Ranged"
    - "Ability.Skill.Fireball"
    - "Ability.Skill.Heal"
    
    # Status
    - "Status.Grounded"
    - "Status.Jumping"
    - "Status.Stunned"
    - "Status.Rooted"
    - "Status.Silenced"
    - "Status.Invulnerable"
    - "Status.Burning"
    - "Status.Frozen"
    - "Status.Poisoned"
    
    # Damage Types
    - "Damage.Physical"
    - "Damage.Fire"
    - "Damage.Ice"
    - "Damage.Lightning"
    - "Damage.Poison"
    
    # Immunity
    - "Immunity.Fire"
    - "Immunity.Ice"
    - "Immunity.Stun"
    - "Immunity.Damage"
    
    # Weakness
    - "Weakness.Fire"
    - "Weakness.Ice"
    
    # Effects
    - "Effect.Burn"
    - "Effect.Freeze"
    - "Effect.Poison"
    - "Effect.HealOverTime"
    
    # Events
    - "Event.Death"
    - "Event.LevelUp"
    - "Event.Respawn"
    
    # Input
    - "InputTag.Jump"
    - "InputTag.Sprint"
    - "InputTag.Attack"
    - "InputTag.Skill1"
    - "InputTag.Skill2"
    
    # Gameplay Cues
    - "GameplayCue.Burn.Start"
    - "GameplayCue.Burn.Loop"
    - "GameplayCue.Burn.End"
    - "GameplayCue.Impact.Physical"
    - "GameplayCue.Impact.Fire"
```

**Generated Files:**

**DefaultGameplayTags.ini:**
```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Ability.Jump",DevComment="Jump ability")
+GameplayTagList=(Tag="Ability.Sprint",DevComment="Sprint ability")
+GameplayTagList=(Tag="Ability.Attack",DevComment="Attack ability")
+GameplayTagList=(Tag="Ability.Attack.Melee",DevComment="Melee attack")
+GameplayTagList=(Tag="Ability.Attack.Ranged",DevComment="Ranged attack")
+GameplayTagList=(Tag="Status.Stunned",DevComment="Stunned status")
+GameplayTagList=(Tag="Damage.Fire",DevComment="Fire damage type")
# ... etc
```

**GameplayTags.h:**
```cpp
namespace GameplayTags
{
    GAMEPLAYABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Jump);
    GAMEPLAYABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
    GAMEPLAYABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Stunned);
    GAMEPLAYABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
    // ... etc
}
```

**GameplayTags.cpp:**
```cpp
namespace GameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(Ability_Jump, "Ability.Jump");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
    UE_DEFINE_GAMEPLAY_TAG(Status_Stunned, "Status.Stunned");
    UE_DEFINE_GAMEPLAY_TAG(Damage_Fire, "Damage.Fire");
    // ... etc
}
```

---

## Replication & Networking

### Replication Architecture

GAS has sophisticated replication with three main modes:

| Mode | Description | Use Case |
|------|-------------|----------|
| **Minimal** | Only replicate minimal info (tags, cues) | Simulated proxies (NPCs) |
| **Mixed** | Minimal for simulated, full for owner/autonomous | Player characters |
| **Full** | Replicate everything to everyone | Spectators, debugging |

### What Gets Replicated

| Component | What Replicates | How |
|-----------|----------------|-----|
| **Attribute Sets** | Attribute values | Per-attribute UPROPERTY replication |
| **Active Effects** | Effect specs, duration, stacks | FActiveGameplayEffectsContainer replication |
| **Abilities** | Activation, input, montages | Ability spec replication + RPCs |
| **Tags** | Owned tags, blocked tags | Tag container replication |

### Attribute Replication Pattern

```cpp
// In AttributeSet.h
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
FGameplayAttributeData Health;

// In AttributeSet.cpp
void UHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UHealthSet, Health, COND_None, REPNOTIFY_Always);
}

void UHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthSet, Health, OldValue);
    // Custom logic here
}
```

### Ability Activation Replication

```
CLIENT                          SERVER
  |                               |
  | TryActivateAbility()          |
  |------------------------------>|
  |                               | CanActivateAbility()
  |                               | ActivateAbility()
  |                               | CommitAbility()
  |                               |
  |<------------------------------|
  | ServerActivateAbility RPC     |
  |                               |
  | ActivateAbility() (predicted) |
  |                               |
```

### Prediction Keys

Prediction keys are used to match client predictions with server confirmations:

```cpp
struct FPredictionKey
{
    int16 Current;      // Current prediction key
    int16 Base;         // Base prediction key from server
    bool bIsStale;      // Is this key stale?
    bool bIsServerInitiated;  // Was this initiated by server?
};
```

**Prediction Flow:**

1. Client generates prediction key
2. Client executes ability with prediction key
3. Client sends RPC to server with prediction key
4. Server executes ability
5. Server sends confirmation with prediction key
6. Client matches prediction key and reconciles

### Gameplay Effect Replication

```cpp
// FActiveGameplayEffectsContainer replicates via:
UPROPERTY(Replicated)
FActiveGameplayEffectsContainer ActiveGameplayEffects;

// Each active effect has:
struct FActiveGameplayEffect
{
    FGameplayEffectSpec Spec;                    // Full effect spec
    FPredictionKey PredictionKey;                // Prediction key
    float StartServerWorldTime;                  // When effect started
    float CachedStartServerWorldTime;            // Cached start time
    float StartWorldTime;                        // Local start time
    bool bIsInhibited;                           // Is effect inhibited?
    // ... etc
};
```

### Replication Modes Comparison

**Minimal Mode:**
- Replicates: Tags, gameplay cues
- Does NOT replicate: Effect specs, attribute values (except via RepNotify)
- Use for: NPCs, simulated proxies

**Mixed Mode:**
- Replicates to owner/autonomous: Full effect specs, attribute values
- Replicates to simulated: Minimal (tags, cues)
- Use for: Player characters

**Full Mode:**
- Replicates everything to everyone
- Use for: Spectators, debugging, small player counts

### Network Optimization

**Attribute Replication:**
- Use `COND_OwnerOnly` for private attributes (mana, stamina)
- Use `COND_None` for public attributes (health)
- Use `REPNOTIFY_Always` for critical attributes

**Effect Replication:**
- Instant effects don't replicate (executed immediately)
- Duration effects replicate full spec
- Periodic effects replicate execution

**Ability Replication:**
- Non-instanced abilities: Limited replication (activation only)
- Instanced abilities: Full replication (state, RPCs, properties)

---

## Pattern Extraction

### Pattern 1: Attribute Set with Clamping

**Pattern:**
- Replicated attributes with RepNotify
- Meta attributes for calculations (Damage, Healing)
- PreAttributeChange for clamping
- PostGameplayEffectExecute for meta attribute conversion
- Delegates for event broadcasting

**Boilerplate:**
```cpp
// Header
ATTRIBUTE_ACCESSORS(UHealthSet, Health);
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
FGameplayAttributeData Health;

UFUNCTION()
void OnRep_Health(const FGameplayAttributeData& OldValue);

// CPP
void UHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UHealthSet, Health, COND_None, REPNOTIFY_Always);
}

void UHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthSet, Health, OldValue);
}

void UHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
}
```

**KAIN Compression:** 5 lines → 30+ lines C++

### Pattern 2: Ability with Cost and Cooldown

**Pattern:**
- Cost gameplay effect (instant, removes resources)
- Cooldown gameplay effect (duration, blocks activation)
- Tag-based activation requirements
- CommitAbility() to apply cost/cooldown
- Input binding support

**Boilerplate:**
```cpp
// Ability constructor
UJumpAbility::UJumpAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Jump")));
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Grounded")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Stunned")));
    
    CostGameplayEffectClass = UStaminaCostEffect::StaticClass();
    CooldownGameplayEffectClass = UJumpCooldownEffect::StaticClass();
}

// Ability activation
void UJumpAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
                                    const FGameplayAbilityActivationInfo ActivationInfo, 
                                    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // Ability logic here
    
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

**KAIN Compression:** 10 lines → 40+ lines C++

### Pattern 3: Gameplay Effect with Modifiers

**Pattern:**
- Duration policy (instant, duration, infinite)
- Modifiers array with attribute + operation + magnitude
- Tag requirements (application, ongoing, removal)
- Stacking rules
- Gameplay cues

**Boilerplate:**
```cpp
UBurnEffect::UBurnEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));
    Period = FScalableFloat(1.0f);
    bExecutePeriodicEffectOnApplication = true;
    
    FGameplayModifierInfo ModifierInfo;
    ModifierInfo.Attribute = UHealthSet::GetHealthAttribute();
    ModifierInfo.ModifierOp = EGameplayModOp::Additive;
    ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-10.0f));
    Modifiers.Add(ModifierInfo);
    
    InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Burn")));
    InheritableGrantedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Burning")));
}
```

**KAIN Compression:** 8 lines → 25+ lines C++

### Pattern 4: Ability System Component Setup

**Pattern:**
- Add attribute sets
- Grant abilities
- Apply initial effects
- Set replication mode
- Initialize ability actor info

**Boilerplate:**
```cpp
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (AbilitySystemComponent)
    {
        // Add attribute sets
        AbilitySystemComponent->AddSet<UHealthSet>();
        AbilitySystemComponent->AddSet<UCombatSet>();
        
        // Initialize ability actor info
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        
        // Grant abilities
        for (TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
        }
        
        // Apply initial effects
        for (TSubclassOf<UGameplayEffect>& Effect : DefaultEffects)
        {
            FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
            EffectContext.AddSourceObject(this);
            
            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(Effect, 1, EffectContext);
            if (SpecHandle.IsValid())
            {
                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
        
        // Set replication mode
        AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    }
}
```

**KAIN Compression:** 15 lines → 35+ lines C++

### Pattern 5: Ability Set (Lyra Pattern)

**Pattern:**
- Data asset that bundles abilities, effects, and attribute sets
- Grant all at once
- Track granted handles for removal
- Used for equipment, character classes, etc.

**Boilerplate:**
```cpp
// Ability set structure
USTRUCT(BlueprintType)
struct FAbilitySet_GameplayAbility
{
    GENERATED_BODY()
    
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayAbility> Ability;
    
    UPROPERTY(EditDefaultsOnly)
    int32 AbilityLevel = 1;
    
    UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
    FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FAbilitySet_GameplayEffect
{
    GENERATED_BODY()
    
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayEffect> GameplayEffect;
    
    UPROPERTY(EditDefaultsOnly)
    float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct FAbilitySet_AttributeSet
{
    GENERATED_BODY()
    
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UAttributeSet> AttributeSet;
};

UCLASS(BlueprintType, Const)
class UAbilitySet : public UPrimaryDataAsset
{
    GENERATED_BODY()
    
public:
    void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;
    
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities")
    TArray<FAbilitySet_GameplayAbility> GrantedGameplayAbilities;
    
    UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
    TArray<FAbilitySet_GameplayEffect> GrantedGameplayEffects;
    
    UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets")
    TArray<FAbilitySet_AttributeSet> GrantedAttributes;
};
```

**KAIN Compression:** 20 lines → 60+ lines C++

---

## Codegen Requirements

### IR (Intermediate Representation) Structures

The `ue5-gas` crate will need IR structures for:

**1. AttributeSetIR**
```rust
pub struct AttributeSetIR {
    pub name: String,
    pub attributes: Vec<AttributeIR>,
    pub delegates: Vec<DelegateIR>,
    pub lifecycle_hooks: LifecycleHooksIR,
}

pub struct AttributeIR {
    pub name: String,
    pub ty: Type,
    pub default_value: Option<f32>,
    pub replicated: bool,
    pub rep_notify: bool,
    pub hide_from_modifiers: bool,
    pub is_meta: bool,
    pub clamp_min: Option<f32>,
    pub clamp_max: Option<f32>,
}

pub struct LifecycleHooksIR {
    pub pre_gameplay_effect_execute: Option<FunctionIR>,
    pub post_gameplay_effect_execute: Option<FunctionIR>,
    pub pre_attribute_change: Option<FunctionIR>,
    pub post_attribute_change: Option<FunctionIR>,
    pub pre_attribute_base_change: Option<FunctionIR>,
    pub post_attribute_base_change: Option<FunctionIR>,
}
```

**2. GameplayAbilityIR**
```rust
pub struct GameplayAbilityIR {
    pub name: String,
    pub instancing_policy: InstancingPolicy,
    pub replication_policy: ReplicationPolicy,
    pub net_execution_policy: NetExecutionPolicy,
    pub net_security_policy: NetSecurityPolicy,
    pub ability_tags: Vec<String>,
    pub activation_required_tags: Vec<String>,
    pub activation_blocked_tags: Vec<String>,
    pub activation_owned_tags: Vec<String>,
    pub cancel_abilities_with_tag: Vec<String>,
    pub block_abilities_with_tag: Vec<String>,
    pub cost_effect: Option<String>,
    pub cooldown_effect: Option<String>,
    pub lifecycle_hooks: AbilityLifecycleHooksIR,
}

pub struct AbilityLifecycleHooksIR {
    pub can_activate_ability: Option<FunctionIR>,
    pub activate_ability: Option<FunctionIR>,
    pub end_ability: Option<FunctionIR>,
    pub cancel_ability: Option<FunctionIR>,
    pub input_pressed: Option<FunctionIR>,
    pub input_released: Option<FunctionIR>,
}

pub enum InstancingPolicy {
    InstancedPerExecution,
    InstancedPerActor,
    NonInstanced,
}

pub enum ReplicationPolicy {
    ReplicateNo,
    ReplicateYes,
}

pub enum NetExecutionPolicy {
    LocalPredicted,
    LocalOnly,
    ServerInitiated,
    ServerOnly,
}
```

**3. GameplayEffectIR**
```rust
pub struct GameplayEffectIR {
    pub name: String,
    pub duration_policy: DurationPolicy,
    pub duration_magnitude: Option<MagnitudeIR>,
    pub period: Option<f32>,
    pub execute_on_application: bool,
    pub modifiers: Vec<ModifierIR>,
    pub executions: Vec<ExecutionIR>,
    pub stacking: Option<StackingIR>,
    pub owned_tags: Vec<String>,
    pub granted_tags: Vec<String>,
    pub application_tag_requirements: TagRequirementsIR,
    pub ongoing_tag_requirements: TagRequirementsIR,
    pub removal_tag_requirements: TagRequirementsIR,
    pub gameplay_cues: Vec<String>,
    pub conditional_effects: Vec<String>,
}

pub struct ModifierIR {
    pub attribute: String,
    pub operation: ModifierOp,
    pub magnitude: MagnitudeIR,
    pub source_tags: TagRequirementsIR,
    pub target_tags: TagRequirementsIR,
}

pub enum ModifierOp {
    Add,
    Multiply,
    Divide,
    Override,
}

pub enum MagnitudeType {
    ScalableFloat(f32),
    AttributeBased {
        coefficient: f32,
        backing_attribute: String,
        calculation_type: AttributeCalculationType,
    },
    CustomCalculationClass(String),
    SetByCaller(String),
}

pub enum DurationPolicy {
    Instant,
    Infinite,
    HasDuration,
}

pub struct StackingIR {
    pub stacking_type: StackingType,
    pub limit: i32,
    pub duration_policy: StackingDurationPolicy,
    pub period_policy: StackingPeriodPolicy,
    pub expiration_policy: StackingExpirationPolicy,
}
```

**4. GameplayTagsIR**
```rust
pub struct GameplayTagsIR {
    pub tags: Vec<GameplayTagIR>,
}

pub struct GameplayTagIR {
    pub tag: String,
    pub comment: Option<String>,
}
```

---

## Codegen Strategy

### File Generation

**For Attribute Sets:**

1. **Header (.h):**
   - `UCLASS(DefaultToInstanced, Blueprintable, MinimalAPI)`
   - `ATTRIBUTE_ACCESSORS` macros for each attribute
   - `UPROPERTY` declarations with replication
   - RepNotify function declarations
   - Lifecycle hook declarations
   - Delegate declarations

2. **Implementation (.cpp):**
   - Constructor with default values
   - `GetLifetimeReplicatedProps()` with `DOREPLIFETIME_CONDITION_NOTIFY`
   - RepNotify implementations with `GAMEPLAYATTRIBUTE_REPNOTIFY`
   - Lifecycle hook implementations
   - Clamping logic

**For Gameplay Abilities:**

1. **Header (.h):**
   - `UCLASS()`
   - Lifecycle hook declarations
   - Input binding declarations

2. **Implementation (.cpp):**
   - Constructor with policies, tags, cost/cooldown
   - `CanActivateAbility()` implementation
   - `ActivateAbility()` implementation
   - `EndAbility()` implementation
   - Input binding implementations

**For Gameplay Effects:**

1. **Header (.h):**
   - `UCLASS()`
   - No additional declarations needed (data-only)

2. **Implementation (.cpp):**
   - Constructor with duration, modifiers, tags, stacking

**For Gameplay Tags:**

1. **DefaultGameplayTags.ini:**
   - Tag list with comments

2. **GameplayTags.h:**
   - `UE_DECLARE_GAMEPLAY_TAG_EXTERN` for each tag

3. **GameplayTags.cpp:**
   - `UE_DEFINE_GAMEPLAY_TAG` for each tag

### Module Dependencies

The `ue5-gas` crate will require these UE5 modules:

```rust
pub fn get_required_modules() -> Vec<&'static str> {
    vec![
        "Core",
        "CoreUObject",
        "Engine",
        "GameplayAbilities",  // CRITICAL
        "GameplayTags",       // CRITICAL
        "GameplayTasks",
        "NetCore",
    ]
}
```

### Include Dependencies

**Attribute Sets:**
```cpp
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
```

**Gameplay Abilities:**
```cpp
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
```

**Gameplay Effects:**
```cpp
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
```

### Macro Generation

**ATTRIBUTE_ACCESSORS Macro:**
```cpp
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
```

Generates:
- `static FGameplayAttribute GetHealthAttribute()`
- `float GetHealth() const`
- `void SetHealth(float NewVal)`
- `void InitHealth(float NewVal)`

**GAMEPLAYATTRIBUTE_REPNOTIFY Macro:**
```cpp
#define GAMEPLAYATTRIBUTE_REPNOTIFY(ClassName, PropertyName, OldValue) \
{ \
    static FProperty* ThisProperty = FindFieldChecked<FProperty>(ClassName::StaticClass(), GET_MEMBER_NAME_CHECKED(ClassName, PropertyName)); \
    GetOwningAbilitySystemComponentChecked()->SetBaseAttributeValueFromReplication(FGameplayAttribute(ThisProperty), PropertyName, OldValue); \
}
```

### Type Mapping

| KAIN Type | UE5 Type | Notes |
|-----------|----------|-------|
| `Float` | `float` | Attribute values |
| `Int` | `int32` | Ability level, stack count |
| `Bool` | `bool` | Flags |
| `String` | `FString` | Names, descriptions |
| `GameplayAttribute` | `FGameplayAttribute` | Attribute reference |
| `GameplayTag` | `FGameplayTag` | Single tag |
| `TagContainer` | `FGameplayTagContainer` | Tag collection |
| `AbilitySpecHandle` | `FGameplayAbilitySpecHandle` | Ability reference |
| `AbilityActorInfo` | `FGameplayAbilityActorInfo` | Actor info |
| `AbilityActivationInfo` | `FGameplayAbilityActivationInfo` | Activation info |
| `GameplayEffectModCallbackData` | `FGameplayEffectModCallbackData` | Effect callback data |
| `GameplayEventData` | `FGameplayEventData` | Event data |

---

## Integration with Existing `ue5` Crate

### Attribute-Driven Dispatch

The packager will route GAS items to the `ue5-gas` crate:

```rust
// In cli/src/packager/ue5_pipeline.rs
match item {
    Item::Struct(s) if has_attribute(&s.attributes, "attribute_set") => {
        // Route to ue5-gas crate
        let ir = ue5_gas::attribute_set_ir::from_ast(s)?;
        let code = ue5_gas::attribute_set_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::Struct(s) if has_attribute(&s.attributes, "ability") => {
        // Route to ue5-gas crate
        let ir = ue5_gas::ability_ir::from_ast(s)?;
        let code = ue5_gas::ability_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::Struct(s) if has_attribute(&s.attributes, "gameplay_effect") => {
        // Route to ue5-gas crate
        let ir = ue5_gas::effect_ir::from_ast(s)?;
        let code = ue5_gas::effect_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::GameplayTags(tags) => {
        // Route to ue5-gas crate
        let ir = ue5_gas::tags_ir::from_ast(tags)?;
        let code = ue5_gas::tags_codegen::generate(&ir)?;
        output.add_file(code);
    }
    // ... existing routes
}
```

### Shared Context

The `ue5-gas` crate will share context with the `ue5` crate:

```rust
pub struct Ue5Context {
    pub plugin_name: String,
    pub module_name: String,
    pub user_types: HashMap<String, UserType>,
    pub attribute_sets: HashMap<String, AttributeSetIR>,  // NEW
    pub gameplay_abilities: HashMap<String, GameplayAbilityIR>,  // NEW
    pub gameplay_effects: HashMap<String, GameplayEffectIR>,  // NEW
    pub gameplay_tags: Vec<GameplayTagIR>,  // NEW
}
```

### Dependency Resolution

**Attribute Set Dependencies:**
- Needs to know other attribute sets for cross-attribute references
- Needs to know gameplay tags for tag checks
- Needs to know gameplay effects for effect application

**Gameplay Ability Dependencies:**
- Needs to know attribute sets for attribute access
- Needs to know gameplay effects for cost/cooldown
- Needs to know gameplay tags for tag requirements

**Gameplay Effect Dependencies:**
- Needs to know attribute sets for modifiers
- Needs to know gameplay tags for tag requirements
- Needs to know other gameplay effects for conditional effects

### Forward Declaration Strategy

```cpp
// Forward declarations
class UHealthSet;
class UCombatSet;
class UJumpAbility;
class UBurnEffect;

// Includes
#include "AttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
```

### Module Registration

```cpp
// In module startup
void FMyPluginModule::StartupModule()
{
    // Register gameplay tags
    UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Ability.Jump"),
        FString("Jump ability")
    );
    
    // ... etc
}
```

---

## Compression Ratio Estimates

### Attribute Set Compression

**KAIN Input (10 lines):**
```kain
@attribute_set
struct HealthSet:
    @attribute(replicated: true, rep_notify: true)
    health: Float = 100.0
    
    @attribute(replicated: true, rep_notify: true)
    max_health: Float = 100.0
    
    fn pre_attribute_change(attribute: GameplayAttribute, new_value: Float):
        if attribute == get_health_attribute():
            new_value = clamp(new_value, 0.0, get_max_health())
```

**Generated C++ (150+ lines):**
- Header: 60 lines (UCLASS, ATTRIBUTE_ACCESSORS, UPROPERTY, RepNotify declarations)
- Implementation: 90 lines (constructor, GetLifetimeReplicatedProps, RepNotify, PreAttributeChange)

**Compression Ratio: 1:15**

### Gameplay Ability Compression

**KAIN Input (15 lines):**
```kain
@ability
struct JumpAbility:
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

**Generated C++ (120+ lines):**
- Header: 40 lines (UCLASS, function declarations)
- Implementation: 80 lines (constructor with tags/policies, CanActivateAbility, ActivateAbility, EndAbility)

**Compression Ratio: 1:8**

### Gameplay Effect Compression

**KAIN Input (12 lines):**
```kain
@gameplay_effect
struct BurnEffect:
    @duration(type: "HasDuration")
    duration: 5.0
    
    @period
    period: 1.0
    
    @modifier(attribute: "Health", operation: "Add")
    damage_per_tick: -10.0
    
    @owned_tags
    tags: ["Effect.Burn"]
```

**Generated C++ (80+ lines):**
- Header: 20 lines (UCLASS)
- Implementation: 60 lines (constructor with duration, period, modifiers, tags)

**Compression Ratio: 1:7**

### Gameplay Tags Compression

**KAIN Input (5 lines):**
```kain
@gameplay_tags
tags:
    - "Ability.Jump"
    - "Status.Stunned"
    - "Damage.Fire"
```

**Generated Files (30+ lines):**
- DefaultGameplayTags.ini: 3 lines
- GameplayTags.h: 9 lines (includes, namespace, declarations)
- GameplayTags.cpp: 18 lines (includes, namespace, definitions)

**Compression Ratio: 1:6**

### Overall GAS Compression

**Average Compression Ratio: 1:10**

A typical GAS setup with:
- 3 attribute sets (30 lines KAIN)
- 10 abilities (150 lines KAIN)
- 15 gameplay effects (180 lines KAIN)
- 50 gameplay tags (10 lines KAIN)

**Total: 370 lines KAIN → 3,700+ lines C++**

---

## Advanced Features

### Ability Tasks

Ability tasks are async operations that abilities can wait on:

```cpp
// Example: Wait for target data
UCLASS()
class UAbilityTask_WaitTargetData : public UAbilityTask
{
    GENERATED_BODY()
    
public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitTargetDataDelegate, const FGameplayAbilityTargetDataHandle&, Data);
    
    UPROPERTY(BlueprintAssignable)
    FWaitTargetDataDelegate ValidData;
    
    UPROPERTY(BlueprintAssignable)
    FWaitTargetDataDelegate Cancelled;
    
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
    static UAbilityTask_WaitTargetData* WaitTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, 
                                                         TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType, 
                                                         TSubclassOf<AGameplayAbilityTargetActor> Class);
    
    virtual void Activate() override;
};
```

**Common Ability Tasks:**
- `WaitTargetData` — Wait for targeting
- `WaitGameplayEvent` — Wait for gameplay event
- `WaitDelay` — Wait for time
- `WaitAttributeChange` — Wait for attribute change
- `WaitGameplayTagAdd` — Wait for tag to be added
- `WaitGameplayTagRemove` — Wait for tag to be removed
- `PlayMontageAndWait` — Play animation and wait for completion

**KAIN Syntax Proposal:**
```kain
@ability
struct FireballAbility:
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        
        # Wait for target data
        let target_data = await wait_target_data(GroundTraceTargetActor)
        
        if target_data.is_valid():
            # Apply damage effect to target
            let effect_spec = make_outgoing_gameplay_effect_spec(FireballDamageEffect, 1.0)
            apply_gameplay_effect_spec_to_target(effect_spec, target_data.get_target())
        
        end_ability(handle, actor_info, activation_info, true, false)
```

### Gameplay Cues

Gameplay cues are cosmetic events (VFX, SFX) triggered by gameplay effects:

```cpp
// Gameplay cue notify
UCLASS()
class AGameplayCueNotify_BurnEffect : public AGameplayCueNotify_Actor
{
    GENERATED_BODY()
    
public:
    virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
    virtual bool OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
    virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
    
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Burn")
    UParticleSystem* BurnParticles;
    
    UPROPERTY(EditDefaultsOnly, Category = "Burn")
    USoundBase* BurnSound;
};
```

**KAIN Syntax Proposal:**
```kain
@gameplay_cue
struct BurnCue:
    @tag
    tag: "GameplayCue.Burn"
    
    @particles
    burn_particles: ParticleSystem = "P_Burn"
    
    @sound
    burn_sound: Sound = "S_Burn"
    
    fn on_execute(target: Actor, parameters: GameplayCueParameters):
        spawn_emitter_at_location(target.get_location(), burn_particles)
        play_sound_at_location(target.get_location(), burn_sound)
    
    fn on_active(target: Actor, parameters: GameplayCueParameters):
        attach_emitter_to_actor(target, burn_particles)
    
    fn on_remove(target: Actor, parameters: GameplayCueParameters):
        detach_emitter_from_actor(target)
```

### Execution Calculations

Custom calculations for complex gameplay effect logic:

```cpp
UCLASS()
class UDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()
    
public:
    UDamageExecutionCalculation();
    
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
                                         FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

void UDamageExecutionCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, 
                                                          FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    // Capture attributes
    float AttackPower = 0.0f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(AttackPowerDef, EvaluateParameters, AttackPower);
    
    float Defense = 0.0f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DefenseDef, EvaluateParameters, Defense);
    
    // Calculate damage
    float Damage = AttackPower * (1.0f - Defense / 100.0f);
    
    // Output
    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(HealthProperty, EGameplayModOp::Additive, -Damage));
}
```

**KAIN Syntax Proposal:**
```kain
@execution_calculation
struct DamageCalculation:
    @capture(source: true)
    attack_power: Float
    
    @capture(target: true)
    defense: Float
    
    fn execute(params: ExecutionParameters) -> ExecutionOutput:
        let attack = params.get_captured_attribute(attack_power)
        let def = params.get_captured_attribute(defense)
        
        let damage = attack * (1.0 - def / 100.0)
        
        return output_modifier("Health", "Add", -damage)
```

### Ability Sets (Lyra Pattern)

```kain
@ability_set
struct WarriorAbilitySet:
    @abilities
    abilities:
        - ability: JumpAbility
          level: 1
          input_tag: "InputTag.Jump"
        - ability: AttackAbility
          level: 1
          input_tag: "InputTag.Attack"
        - ability: BlockAbility
          level: 1
          input_tag: "InputTag.Block"
    
    @effects
    effects:
        - effect: BaseHealthEffect
          level: 1.0
        - effect: BaseStaminaEffect
          level: 1.0
    
    @attribute_sets
    attribute_sets:
        - HealthSet
        - CombatSet
        - MovementSet
```

**Generated C++ (AbilitySet.h):**
```cpp
UCLASS(BlueprintType, Const)
class UWarriorAbilitySet : public UPrimaryDataAsset
{
    GENERATED_BODY()
    
public:
    void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;
    
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities")
    TArray<FAbilitySet_GameplayAbility> GrantedGameplayAbilities;
    
    UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
    TArray<FAbilitySet_GameplayEffect> GrantedGameplayEffects;
    
    UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets")
    TArray<FAbilitySet_AttributeSet> GrantedAttributes;
};
```

---

## Testing Strategy

### Unit Tests

**Attribute Set Tests:**
- Attribute clamping (health can't go negative)
- Replication (RepNotify called correctly)
- Meta attribute conversion (Damage → -Health)
- Delegate broadcasting (OnHealthChanged)
- Lifecycle hooks (PreAttributeChange, PostGameplayEffectExecute)

**Gameplay Ability Tests:**
- Activation validation (CanActivateAbility)
- Tag requirements (required, blocked, owned)
- Cost/cooldown application
- Instancing policies
- Replication policies
- Input binding

**Gameplay Effect Tests:**
- Duration types (instant, duration, infinite)
- Modifier operations (add, multiply, divide, override)
- Magnitude types (scalable, attribute-based, custom, set-by-caller)
- Stacking rules
- Tag requirements
- Conditional effects

**Gameplay Tag Tests:**
- Tag registration
- Tag matching (exact, partial, parent)
- Tag requirements (require, ignore)
- Tag containers (add, remove, has)

### Integration Tests

**Full GAS Pipeline:**
1. Create attribute set with health
2. Create damage effect
3. Apply effect to attribute set
4. Verify health decreased
5. Verify RepNotify called
6. Verify delegate broadcast

**Ability Activation:**
1. Create ability with cost/cooldown
2. Grant ability to ASC
3. Activate ability
4. Verify cost applied
5. Verify cooldown applied
6. Verify ability cannot activate again until cooldown expires

**Effect Stacking:**
1. Create stacking effect
2. Apply effect multiple times
3. Verify stack count increases
4. Verify stack limit enforced
5. Verify duration refresh on stack

### Property-Based Tests

**Attribute Clamping:**
```rust
#[test]
fn test_attribute_clamping() {
    proptest!(|(health in -1000.0f32..1000.0f32, max_health in 1.0f32..1000.0f32)| {
        let clamped = clamp_health(health, max_health);
        assert!(clamped >= 0.0);
        assert!(clamped <= max_health);
    });
}
```

**Tag Matching:**
```rust
#[test]
fn test_tag_matching() {
    proptest!(|(tag: String, container: Vec<String>)| {
        let matches = tag_matches_any(&tag, &container);
        // Verify partial matching works correctly
    });
}
```

