# GAS Quick Reference — KAIN Syntax Cheat Sheet

> **Fast lookup for GAS syntax in KAIN**

---

## GameplayTags

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

@gameplay_tags
namespace Status:
    Alive
    Dead
    CC:
        Stunned
        Rooted
```

**Generated:**
- Native C++ tags (UE_DEFINE_GAMEPLAY_TAG)
- DefaultGameplayTags.ini

---

## Attribute Sets

```kain
@attribute_set
struct HealthSet:
    @attribute(replicated: true, rep_notify: true, hide_from_modifiers: true)
    health: Float = 100.0
    
    @attribute(replicated: true, rep_notify: true)
    max_health: Float = 100.0
    
    @attribute(meta: true)
    damage: Float = 0.0
    
    @delegate
    on_health_changed: AttributeEvent
    
    fn pre_attribute_change(attribute: GameplayAttribute, new_value: Float):
        if attribute == get_health_attribute():
            new_value = clamp(new_value, 0.0, get_max_health())
    
    fn post_gameplay_effect_execute(data: GameplayEffectModCallbackData):
        if data.evaluated_data.attribute == get_damage_attribute():
            set_health(get_health() - get_damage())
            set_damage(0.0)
```

**Attribute Options:**
- `replicated: true` — Replicate to clients
- `rep_notify: true` — Generate OnRep callback
- `hide_from_modifiers: true` — Hide from modifier UI
- `meta: true` — Temporary calculation attribute (no replication)

---

## Gameplay Abilities

```kain
@ability
struct JumpAbility:
    @instancing(policy: "InstancedPerExecution")
    @replication(policy: "ReplicateYes")
    @net_execution(policy: "LocalPredicted")
    
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
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        
        get_avatar_actor().jump()
        end_ability(handle, actor_info, activation_info, true, false)
```

**Instancing Policies:**
- `InstancedPerExecution` — New instance each activation (default)
- `InstancedPerActor` — One instance per actor (passive, toggle)
- `NonInstanced` — No instance, use CDO (simple abilities)

**Net Execution Policies:**
- `LocalPredicted` — Client predicts, server confirms (most abilities)
- `ServerInitiated` — Server only, no prediction (authoritative)
- `ServerOnly` — Server only, no client execution (admin)
- `LocalOnly` — Client only, no server (cosmetic)

---

## Gameplay Effects

### Instant Effect

```kain
@gameplay_effect
struct DamageEffect:
    @duration(type: "Instant")
    
    @modifier(attribute: "Health", operation: "Add", magnitude_type: "SetByCaller")
    damage:
        set_by_caller: "SetByCaller.Damage.Amount"
    
    @owned_tags
    tags: ["Effect.Damage"]
    
    @application_tag_requirements
    require: ["Status.Alive"]
    ignore: ["Status.Immune.Damage"]
```

### Duration Effect (DOT)

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
    
    @owned_tags
    tags: ["Effect.Burn"]
    
    @granted_tags
    tags: ["Status.Burning"]
```

### Infinite Effect (Passive)

```kain
@gameplay_effect
struct PassiveRegenEffect:
    @duration(type: "Infinite")
    
    @period
    period: 1.0
    
    @modifier(attribute: "Health", operation: "Add")
    regen_per_second: 2.0
```

**Duration Types:**
- `Instant` — Apply once
- `HasDuration` — Lasts for specified time
- `Infinite` — Lasts forever until removed

**Modifier Operations:**
- `Add` — BaseValue + Modifier
- `Multiply` — BaseValue * Modifier
- `Divide` — BaseValue / Modifier
- `Override` — Set to Modifier

**Magnitude Types:**
- `ScalableFloat` — Simple float value
- `AttributeBased` — Based on another attribute
- `SetByCaller` — Set at runtime
- `CustomCalculation` — Custom C++ class

---

## Tag Queries

```kain
# Simple
has_tag("Status.Stunned")

# Any (OR)
has_any(["Status.Buffed", "Status.Empowered"])

# All (AND)
has_all(["Status.Alive", "Status.Conscious"])

# Not
not(has_tag("Status.Stunned"))

# Complex
any(["Status.Buffed", "Status.Empowered"]) 
and all(["Status.Alive"]) 
and not(any(["Status.Stunned", "Status.Silenced"]))
```

---

## Tag Events

```kain
actor Character:
    @on_tag_added("Status.CC.Stunned")
    fn on_stunned():
        cancel_all_abilities()
        play_animation("Stunned")
    
    @on_tag_removed("Status.CC.Stunned")
    fn on_unstunned():
        play_animation("Idle")
    
    @on_tag_count_changed("Status.Buff")
    fn on_buff_count_changed(tag: Tag, count: Int):
        update_buff_ui(count)
```

---

## ASC Operations

```kain
# Initialize
asc = create_ability_system_component()
asc.init_ability_actor_info(self, self)

# Grant attribute sets
health_set = asc.add_set(HealthSet)

# Grant abilities
asc.give_ability(JumpAbility, 1)
asc.give_ability_and_activate_once(PassiveAbility, 1)

# Activate abilities
asc.try_activate_abilities_by_tag(tag_container)

# Cancel abilities
asc.cancel_abilities(with_tags, without_tags)

# Apply effects
asc.apply_gameplay_effect_to_self(EffectClass, level)
asc.apply_gameplay_effect_to_target(target_asc, EffectClass, level)

# Remove effects
asc.remove_active_effects_with_tags(tag_container)
asc.remove_active_effects_with_granted_tags(tag_container)

# Tags
asc.add_loose_gameplay_tag(tag)
asc.remove_loose_gameplay_tag(tag)
asc.has_matching_gameplay_tag(tag)
asc.has_any_matching_gameplay_tags(tag_container)
asc.has_all_matching_gameplay_tags(tag_container)

# Attributes
value = asc.get_numeric_attribute(attribute)
asc.set_numeric_attribute_base(attribute, value)

# Cooldowns
remaining = asc.get_cooldown_time_remaining(tag)
is_on_cooldown = asc.has_matching_gameplay_tag(cooldown_tag)
```

---

## Common Patterns

### Apply Damage

```kain
fn apply_damage(target: Actor, damage: Float):
    let target_asc = get_asc_from_actor(target)
    
    if not target_asc.has_matching_gameplay_tag("Status.Alive"):
        return
    
    let effect_context = asc.make_effect_context()
    let effect_spec = asc.make_outgoing_spec(DamageEffect, 1, effect_context)
    effect_spec.set_set_by_caller_magnitude("SetByCaller.Damage.Amount", -damage)
    
    target_asc.apply_gameplay_effect_spec_to_self(effect_spec)
```

### Check Ability Activation

```kain
fn can_use_ability() -> Bool:
    let asc = get_ability_system_component()
    
    return asc.has_matching_gameplay_tag("Status.Alive") and
           not asc.has_any_matching_gameplay_tags(["Status.CC.Stunned", "Status.Dead"])
```

### Remove Debuffs

```kain
fn cleanse():
    let asc = get_ability_system_component()
    let debuff_tags = make_tag_container("Status.Debuff")
    asc.remove_active_effects_with_granted_tags(debuff_tags)
```

---

## Module Dependencies

**Build.cs:**
```cpp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameplayAbilities",  // REQUIRED
    "GameplayTags",       // REQUIRED
    "GameplayTasks",
    "NetCore",
});
```

---

## Compression Ratios

- **Tags:** 1:6
- **Attribute Sets:** 1:15
- **Abilities:** 1:8
- **Effects:** 1:7
- **Overall:** 1:10

---

**See FEATURE_REFERENCE.md for complete documentation**

### Ability Lifecycle Hooks

```kain
fn can_activate_ability(handle, actor_info, source_tags, target_tags) -> Bool:
    # Custom validation logic
    return true

fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
    # Main ability logic
    if not commit_ability(handle, actor_info, activation_info):
        end_ability(handle, actor_info, activation_info, true, true)
        return
    
    # Execute ability
    end_ability(handle, actor_info, activation_info, true, false)

fn end_ability(handle, actor_info, activation_info, replicate_end, was_cancelled):
    # Cleanup logic

fn input_pressed(handle, actor_info, activation_info):
    # Handle input press

fn input_released(handle, actor_info, activation_info):
    # Handle input release
```

---

## Gameplay Effects

### Duration Types

```kain
@duration(type: "Instant")           # Apply once
@duration(type: "HasDuration")       # Lasts for duration
@duration(type: "Infinite")          # Lasts forever
```

### Modifiers

```kain
@modifier(attribute: "Health", operation: "Add")
damage: -10.0

@modifier(attribute: "AttackPower", operation: "Multiply")
multiplier: 1.5

@modifier(attribute: "MovementSpeed", operation: "Override")
speed: 0.0
```

### Magnitude Types

```kain
# ScalableFloat
@modifier(attribute: "Health", operation: "Add")
damage: -10.0

# AttributeBased
@modifier(attribute: "Damage", operation: "Multiply", magnitude_type: "AttributeBased")
damage_multiplier:
    coefficient: 1.5
    backing_attribute: "AttackPower"
    calculation_type: "AttributeMagnitude"

# SetByCaller
@modifier(attribute: "Health", operation: "Add", magnitude_type: "SetByCaller")
damage:
    set_by_caller: "SetByCaller.Damage.Amount"
```

### Stacking

```kain
@stacking
type: "AggregateBySource"           # Stack per source
limit: 5                            # Max 5 stacks
duration_policy: "RefreshOnSuccessfulApplication"
period_policy: "ResetOnSuccessfulApplication"
expiration_policy: "RemoveSingleStackAndRefreshDuration"
```

**Stacking Types:**
- `None` — No stacking
- `AggregateBySource` — Stack per source
- `AggregateByTarget` — Stack on target

### Tag Requirements

```kain
@application_tag_requirements
require: ["Status.Alive"]           # Must have these
ignore: ["Status.Immune.Fire"]      # Cannot have these

@ongoing_tag_requirements
require: []                         # Must have to stay active
ignore: ["Status.Immune.Fire"]      # Removed if target gets these

@removal_tag_requirements
require: ["Cleanse.Fire"]           # Remove if target gets these
```

### Effect Components

```kain
@owned_tags
tags: ["Effect.Burn"]               # Tags this effect HAS

@granted_tags
tags: ["Status.Burning"]            # Tags applied to target

@block_abilities_with_tag
block: ["Ability.Attack"]           # Block abilities

@cancel_abilities_with_tag
cancel: ["Ability.Channeled"]       # Cancel abilities

@immunity
immune_to: ["Effect.Damage.Fire"]   # Grant immunity

@remove_effects_with_tags
remove: ["Effect.Burn"]             # Remove effects on application

@conditional_effects
on_damage_dealt: ["LifestealEffect"]  # Apply on condition

@overflow_effects
overflow: ["ShieldEffect"]          # Apply on overflow

@gameplay_cues
cues: ["GameplayCue.Burn.Start"]    # Visual/audio cues
```

---

## Tag Queries

```kain
@ability
struct ConditionalAbility:
    @tag_query
    can_activate: any(["Status.Buffed", "Status.Empowered"]) 
                  and all(["Status.Alive"])
                  and not(any(["Status.Stunned"]))
    
    fn can_activate_ability(handle, actor_info, source_tags, target_tags) -> Bool:
        let asc = get_ability_system_component()
        let owner_tags = asc.get_owned_gameplay_tags()
        return evaluate_query(can_activate, owner_tags)
```

---

## Tag Events

```kain
actor Character:
    @on_tag_added("Status.CC.Stunned")
    fn on_stunned():
        is_stunned = true
        cancel_all_abilities()
    
    @on_tag_removed("Status.CC.Stunned")
    fn on_unstunned():
        is_stunned = false
    
    @on_tag_count_changed("Status.Buff")
    fn on_buff_count_changed(tag: Tag, count: Int):
        buff_count = count
```

---

## ASC Integration

```kain
actor GASPlayer:
    state asc: AbilitySystemComponent
    state health_set: HealthSet
    
    on BeginPlay():
        # Initialize
        asc = create_ability_system_component()
        asc.init_ability_actor_info(self, self)
        
        # Grant attribute sets
        health_set = asc.add_set(HealthSet)
        
        # Grant abilities
        asc.give_ability(JumpAbility, 1)
        
        # Apply effects
        asc.apply_gameplay_effect_to_self(PassiveRegenEffect, 1)
```

---

## Multiplayer

```kain
actor MultiplayerCharacter:
    fn setup_replication():
        if is_locally_controlled():
            asc.set_replication_mode("Mixed")
        elif is_simulated_proxy():
            asc.set_replication_mode("Minimal")
        else:
            asc.set_replication_mode("Full")
    
    on Server_ApplyDamage(target: Actor, damage: Float):
        # Server-authoritative damage
        target_asc.apply_gameplay_effect_spec_to_self(damage_spec)
        Multicast_ShowDamageEffect(target, damage)
    
    on Multicast_ShowDamageEffect(target: Actor, damage: Float):
        # Visual feedback on all clients
        play_damage_effect(target)
```

---

## Gameplay Cues

```kain
# Execute cue
execute_gameplay_cue("GameplayCue.Impact.Fire")

# Execute at location
execute_gameplay_cue_at_location("GameplayCue.Impact.Fire", location)

# Execute on actor
execute_gameplay_cue_on_actor("GameplayCue.Effect.Heal", target)

# Looping cues
start_looping_cue("GameplayCue.Effect.Burn.Loop")
stop_looping_cue("GameplayCue.Effect.Burn.Loop")
```

---

## Common Mistakes

### ❌ Don't use tags for data

```kain
# BAD
FGameplayTag Health_100
FGameplayTag Health_50

# GOOD
Float health = asc.get_numeric_attribute(HealthAttribute)
```

### ❌ Don't forget replication

```kain
# BAD
@attribute
health: Float  # Not replicated!

# GOOD
@attribute(replicated: true, rep_notify: true)
health: Float
```

### ❌ Don't poll tags every frame

```kain
# BAD
on Tick(delta: Float):
    if asc.has_matching_gameplay_tag("Status.Stunned"):
        # ...

# GOOD
@on_tag_added("Status.Stunned")
fn on_stunned():
    # Event-driven
```

### ❌ Don't forget to commit abilities

```kain
# BAD
fn activate_ability(...):
    execute_ability()  # No cost/cooldown applied!

# GOOD
fn activate_ability(...):
    if not commit_ability(...):
        end_ability(..., true, true)
        return
    execute_ability()
```

---

## Performance Tips

1. **Cache tags** — Don't call `request_gameplay_tag()` every frame
2. **Use native tags** — Compile-time constants are faster
3. **Use tag events** — Don't poll tags in Tick
4. **Batch operations** — Use containers for multiple tags
5. **Minimize tag count** — Remove tags when not needed

---

## See Also

- **FEATURE_REFERENCE.md** — Complete feature documentation
- **README.md** — Showcase overview
- **gas_showcase.kn** — Full source code (1200+ lines)
- **GAS_ARCHITECTURE_ANALYSIS.md** — GAS deep-dive
- **GAMEPLAY_TAGS_DEEP_DIVE.md** — Tag system analysis

---

**Quick Reference Version:** 1.0  
**Last Updated:** 2026-02-19
