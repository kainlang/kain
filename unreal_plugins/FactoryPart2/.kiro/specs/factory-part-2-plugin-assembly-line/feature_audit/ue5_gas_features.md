# ue5-gas Features Audit

> **Crate:** `Kain/crates/ue5-gas` (NOT YET IMPLEMENTED)
> **Status:** ⚠️ Planned - GAS integration is listed as "In Progress" in TECH.md
> **Last Updated:** 2026-03-02

---

## Overview

The ue5-gas crate would generate UE5 Gameplay Ability System (GAS) integration from KAIN constructs. This crate **does not currently exist** but is planned for future implementation.

**Expected Output:**
- `UAbilitySystemComponent` subclasses
- `UGameplayAbility` subclasses
- `UAttributeSet` subclasses
- `UGameplayEffect` definitions
- Gameplay tag definitions
- Gameplay cue handlers

---

## Current Status

### From TECH.md:
> **In Progress:** Pattern database export, final regression suite across all 25 Factory plugins, **GAS integration**, Timeline Sequencer, Mesh Manipulation, AI Integration.

### From Requirements:
> 8. THE Feature_Audit_System SHALL document all capabilities from ue5-gas crate (Gameplay Ability System integration)

### From Design:
> **ue5-gas crate**:
> - Gameplay Ability System integration
> - Ability definitions
> - Attribute sets
> - Gameplay effects
> - Gameplay tags
> - Gameplay cues

---

## Planned Feature Categories

### 1. Ability System Component

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@ability_system_component
struct CharacterAbilitySystem:
    default_abilities: Array<GameplayAbility>
    default_effects: Array<GameplayEffect>
    
    fn GrantAbility(ability: GameplayAbility):
        # Grant ability to character
        pass
    
    fn RemoveAbility(ability: GameplayAbility):
        # Remove ability from character
        pass
```

**Expected Generated Output:**
```cpp
UCLASS()
class UCharacterAbilitySystemComponent : public UAbilitySystemComponent {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;
    
    UFUNCTION(BlueprintCallable)
    void GrantAbility(TSubclassOf<UGameplayAbility> Ability);
    
    UFUNCTION(BlueprintCallable)
    void RemoveAbility(TSubclassOf<UGameplayAbility> Ability);
};
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS** (plugin name suggests GAS usage, but implementation details unknown)
- No confirmed GAS implementations in Factory Part 1

---

### 2. Gameplay Abilities

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@gameplay_ability
struct FireWeaponAbility:
    cost_stamina: Float = 10.0
    cooldown: Float = 0.5
    
    @ability_tags("Ability.Weapon.Fire")
    @cancel_tags("Ability.Weapon.Reload")
    
    fn ActivateAbility():
        # Fire weapon logic
        if has_ammo():
            fire_projectile()
            consume_ammo()
            apply_cooldown()
    
    fn CanActivateAbility() -> Bool:
        return has_ammo() and not is_reloading()
```

**Expected Generated Output:**
```cpp
UCLASS()
class UFireWeaponAbility : public UGameplayAbility {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CostStamina = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Cooldown = 0.5f;
    
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;
    
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags
    ) const override;
};
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS**: Likely has tactical abilities (suppression, breach, extraction)
- **RPGCorePro** (planned): Would use abilities for skills
- **CombatSystemPro** (planned): Would use abilities for combat moves
- **LootGeneratorPro** (planned): Would use abilities for loot effects

---

### 3. Attribute Sets

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@attribute_set
struct CharacterAttributes:
    @replicated
    health: Float = 100.0
    
    @replicated
    max_health: Float = 100.0
    
    @replicated
    stamina: Float = 100.0
    
    @replicated
    max_stamina: Float = 100.0
    
    @replicated
    armor: Float = 0.0
    
    fn PreAttributeChange(attribute: String, new_value: Float):
        # Clamp values
        if attribute == "health":
            return clamp(new_value, 0.0, max_health)
        return new_value
```

**Expected Generated Output:**
```cpp
UCLASS()
class UCharacterAttributeSet : public UAttributeSet {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Health)
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxHealth)
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina)
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Stamina)
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxStamina)
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor)
    FGameplayAttributeData Armor;
    ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Armor)
    
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldHealth);
    
    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    
    // ... other OnRep functions
};
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS**: Likely has tactical attributes (suppression level, threat level)
- **RPGCorePro** (planned): Would have RPG attributes (strength, dexterity, intelligence)
- **CombatSystemPro** (planned): Would have combat attributes (damage, defense, crit chance)

---

### 4. Gameplay Effects

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@gameplay_effect
struct BurnEffect:
    duration: Float = 5.0
    period: Float = 1.0
    
    @modifier(attribute: "Health", operation: "Add")
    damage_per_tick: Float = -10.0
    
    @granted_tags("Effect.Burn")
    @ongoing_tags("State.Burning")
    
    fn OnApplied(target: Actor):
        # Visual effects
        spawn_burn_particles(target)
    
    fn OnRemoved(target: Actor):
        # Cleanup
        remove_burn_particles(target)
```

**Expected Generated Output:**
```cpp
UCLASS()
class UBurnEffect : public UGameplayEffect {
    GENERATED_BODY()
public:
    UBurnEffect() {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        DurationMagnitude = FScalableFloat(5.0f);
        Period = 1.0f;
        
        // Add modifier
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UCharacterAttributeSet::GetHealthAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Additive;
        ModifierInfo.ModifierMagnitude = FScalableFloat(-10.0f);
        Modifiers.Add(ModifierInfo);
        
        // Add tags
        InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag("Effect.Burn"));
        InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag("State.Burning"));
    }
};
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS**: Likely has tactical effects (suppression, flashbang, smoke)
- **RPGCorePro** (planned): Would have buff/debuff effects
- **CombatSystemPro** (planned): Would have combat effects (stun, slow, bleed)
- **LootGeneratorPro** (planned): Would have loot effects (stat boosts)

---

### 5. Gameplay Tags

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@gameplay_tags
enum AbilityTags:
    Ability_Weapon_Fire = "Ability.Weapon.Fire"
    Ability_Weapon_Reload = "Ability.Weapon.Reload"
    Ability_Movement_Sprint = "Ability.Movement.Sprint"
    Ability_Movement_Crouch = "Ability.Movement.Crouch"

@gameplay_tags
enum StateTags:
    State_Burning = "State.Burning"
    State_Stunned = "State.Stunned"
    State_Invulnerable = "State.Invulnerable"
```

**Expected Generated Output:**
```ini
; Config/Tags/GameplayTags.ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="Ability.Weapon.Fire",DevComment="Fire weapon ability")
+GameplayTagList=(Tag="Ability.Weapon.Reload",DevComment="Reload weapon ability")
+GameplayTagList=(Tag="Ability.Movement.Sprint",DevComment="Sprint ability")
+GameplayTagList=(Tag="Ability.Movement.Crouch",DevComment="Crouch ability")
+GameplayTagList=(Tag="State.Burning",DevComment="Burning state")
+GameplayTagList=(Tag="State.Stunned",DevComment="Stunned state")
+GameplayTagList=(Tag="State.Invulnerable",DevComment="Invulnerable state")
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS**: Likely has tactical tags (Tactical.Suppression, Tactical.Breach)
- No confirmed tag definitions in Factory Part 1

---

### 6. Gameplay Cues

**Status:** ❌ Not Implemented

**Expected KAIN Syntax:**
```kain
@gameplay_cue
struct BurnCue:
    @cue_tag("GameplayCue.Burn")
    
    fn OnActive(target: Actor, parameters: GameplayCueParameters):
        # Spawn burn particles
        spawn_particle_system(target, "BurnParticles")
        play_sound(target, "BurnSound")
    
    fn OnRemove(target: Actor, parameters: GameplayCueParameters):
        # Remove burn particles
        stop_particle_system(target, "BurnParticles")
```

**Expected Generated Output:**
```cpp
UCLASS()
class ABurnCue : public AGameplayCueNotify_Actor {
    GENERATED_BODY()
public:
    virtual bool OnActive_Implementation(
        AActor* Target,
        const FGameplayCueParameters& Parameters
    ) override;
    
    virtual bool OnRemove_Implementation(
        AActor* Target,
        const FGameplayCueParameters& Parameters
    ) override;
};
```

**Factory Part 1 Examples:**
- **TacticalRaidGAS**: Likely has tactical cues (suppression effects, breach effects)
- No confirmed cue implementations in Factory Part 1

---

## Feature Coverage Summary

| Feature | Status | Factory Part 1 Usage |
|---------|--------|---------------------|
| Ability System Component | ❌ Not Implemented | Unknown |
| Gameplay Abilities | ❌ Not Implemented | Unknown |
| Attribute Sets | ❌ Not Implemented | Unknown |
| Gameplay Effects | ❌ Not Implemented | Unknown |
| Gameplay Tags | ❌ Not Implemented | Unknown |
| Gameplay Cues | ❌ Not Implemented | Unknown |

---

## Planned Factory Part 2 Plugins Using GAS

From the design document, the following plugins are planned to use GAS integration:

### Narrative Systems Domain
1. **DialogueForge** - Graph editor, graph runtime, subsystems, blueprint integration, **GAS integration**

### RPG/Gameplay Systems Domain
1. **RPGCorePro** - Complete RPG system with stats, attributes, leveling, equipment
   - Features: **GAS integration**, networking, subsystems, blueprint integration, UI widgets
   - LOC: 12000-15000
   - Unique: Network-replicated RPG system, **GAS integration**, modular design

2. **CombatSystemPro** - Advanced combat system with combos, hitboxes, damage calculation
   - Features: **GAS integration**, animation state machines, networking, blueprint integration
   - LOC: 10000-13000
   - Unique: Combo system, hitbox visualization, network prediction

### Game-Inspired Clones Domain
1. **LootGeneratorPro** - Borderlands-style procedural loot with rarity, stats, prefixes/suffixes
   - Features: Procedural generation, **GAS integration**, networking, material graphs, blueprint integration
   - LOC: 10000-13000
   - Unique: Procedural weapon generation, stat system, visual effects

---

## Implementation Priority

Based on the design document feature coverage targets:

| Feature | Current | Target | Status |
|---------|---------|--------|--------|
| GAS Integration | 2 | 4 | 🔴 |

**Priority:** HIGH - GAS integration is needed for 4+ planned plugins

---

## Reference Plugins for GAS Patterns

From Research/_docs/REFERENCE_ANALYSIS_PLAN.md:

### NinjaGAS Plugin
- Gameplay Ability System details
- Complex property types
- Inline editing

**Location:** Unknown (marketplace plugin reference)

---

## Implementation Roadmap

### Phase 1: Core GAS Support
1. Ability System Component generation
2. Basic Gameplay Ability generation
3. Attribute Set generation with replication

### Phase 2: Effects & Tags
1. Gameplay Effect generation
2. Gameplay Tag definition system
3. Tag container management

### Phase 3: Advanced Features
1. Gameplay Cue generation
2. Ability task support
3. Target data generation
4. Prediction support

### Phase 4: Integration
1. Blueprint integration for abilities
2. Networking support for GAS
3. Debugging tools
4. Performance optimization

---

## Estimated Implementation Effort

From Research/Reports/Scouting/CURRENT_STATE_SUMMARY.md:

> ### Phase 2: Add TIER 3 Missing Patterns (4-6 weeks)
> - **GAS Integration (40-60h)**
> - Timeline Sequencer (60-80h)
> - **Target**: 16/29 patterns (55%)

**Estimated Effort:** 40-60 hours

---

## Known Challenges

1. **Complex GAS API** - UE5's GAS API is extensive and complex
2. **Replication** - GAS has intricate replication requirements
3. **Prediction** - Client-side prediction is challenging
4. **Performance** - GAS can be performance-intensive
5. **Debugging** - GAS debugging is notoriously difficult

---

## Crate Files (Expected)

| File | Expected Size | Purpose |
|------|---------------|---------|
| `ability_system_component.rs` | ~40KB | ASC generation |
| `gameplay_ability.rs` | ~50KB | Ability generation |
| `attribute_set.rs` | ~40KB | Attribute set generation |
| `gameplay_effect.rs` | ~60KB | Effect generation |
| `gameplay_tags.rs` | ~30KB | Tag system |
| `gameplay_cue.rs` | ~40KB | Cue generation |
| `validation.rs` | ~30KB | GAS validation |

**Expected Total:** ~290KB

---

## Conclusion

The ue5-gas crate **does not currently exist** but is a high-priority feature for Factory Part 2. It is listed as "In Progress" in TECH.md and is required for 4+ planned plugins (RPGCorePro, CombatSystemPro, LootGeneratorPro, DialogueForge).

**Recommendation:** Prioritize GAS integration implementation before starting Factory Part 2 plugin generation, or exclude GAS-dependent plugins from the initial batch.
