# GAS Implementation Plan — ue5-gas Crate

> **Complete roadmap for implementing Gameplay Ability System support in KAIN**

![Status](https://img.shields.io/badge/Status-Phase%202%20Complete-brightgreen)
![Priority](https://img.shields.io/badge/Priority-CRITICAL-red)
![Compression](https://img.shields.io/badge/Compression-1%3A10-brightgreen)
![Tests](https://img.shields.io/badge/Tests-38%20Passing-brightgreen)

---

## Executive Summary

**Why GAS is Critical:**
- Every multiplayer game needs it
- No competition in the market
- Massive compression ratio (1:10 average)
- Foundation for all gameplay systems
- **GameplayTags are THE FOUNDATION** — without proper tag support, nothing else works

**Compression Ratios:**
- Attribute Sets: 1:15
- Abilities: 1:8
- Effects: 1:7
- Tags: 1:6
- Overall: 1:10

**Market Impact:**
- GAS plugins sell for $50-$300
- Every multiplayer game needs GAS
- Lyra uses GAS extensively
- Community demand is massive

---

## Phase 1: GameplayTags Foundation ✅ COMPLETE

**Status: PRODUCTION-READY**  
**Tests: 18/18 passing**  
**Compression: 1:6**  
**Documentation: PHASE1_COMPLETE.md**

GameplayTags are the foundation of GAS. Tags control ability activation, effect application, cooldowns, and state tracking.
- Ability activation (required tags, blocked tags)
- Effect application (immunity, requirements)
- Ability cancellation (cancel tags, block tags)
- Cooldowns (cooldown tags)
- Gameplay cues (visual/audio effects)
- State tracking (status effects, buffs, debuffs)

### Task 1.1: Tag Namespace Parser

**File:** `Kain/crates/kain-core/src/parser/tags.rs`

**Syntax to support:**
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

**AST Structure:**
```rust
pub struct GameplayTagsNamespace {
    pub name: String,
    pub children: Vec<GameplayTagNode>,
}

pub struct GameplayTagNode {
    pub name: String,
    pub full_path: String,  // "Ability.Attack.Melee.Sword"
    pub comment: Option<String>,
    pub children: Vec<GameplayTagNode>,
}
```


### Task 1.2: Tag IR Structure

**File:** `Kain/crates/ue5-gas/src/tags_ir.rs` (NEW CRATE)

```rust
pub struct GameplayTagsIR {
    pub namespaces: Vec<TagNamespaceIR>,
}

pub struct TagNamespaceIR {
    pub name: String,
    pub tags: Vec<GameplayTagIR>,
}

pub struct GameplayTagIR {
    pub tag: String,  // Full path: "Ability.Attack.Melee.Sword"
    pub comment: Option<String>,
    pub parent: Option<String>,  // "Ability.Attack.Melee"
}

impl GameplayTagsIR {
    pub fn from_ast(namespaces: Vec<GameplayTagsNamespace>) -> Result<Self> {
        // Flatten hierarchy into flat list with full paths
        // Auto-generate parent tags
        // Validate no duplicates
    }
}
```

### Task 1.3: Tag Codegen — Native C++ Tags

**File:** `Kain/crates/ue5-gas/src/tags_codegen.rs`

**Generate 3 files:**

1. **GameplayTags.h** — Native tag declarations
2. **GameplayTags.cpp** — Native tag definitions
3. **DefaultGameplayTags.ini** — Designer-friendly .ini file

**Example output:**

```cpp
// GameplayTags.h
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

// GameplayTags.cpp
#include "GameplayTags.h"

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
        }
    }
}
```


### Task 1.4: Tag Query Parser

**File:** `Kain/crates/kain-core/src/parser/tag_query.rs`

**Syntax to support:**
```kain
# Simple tag check
has_tag("Status.Stunned")

# Any/All/Not
has_any(["Status.Buffed", "Status.Empowered"])
has_all(["Status.Alive", "Status.Conscious"])
not(has_tag("Status.Stunned"))

# Complex queries
any(["Status.Buffed", "Status.Empowered"]) and all(["Status.Alive"]) and not(any(["Status.Stunned"]))
```

**AST Structure:**
```rust
pub enum TagQuery {
    HasTag(String),
    HasAny(Vec<String>),
    HasAll(Vec<String>),
    Not(Box<TagQuery>),
    And(Box<TagQuery>, Box<TagQuery>),
    Or(Box<TagQuery>, Box<TagQuery>),
}
```

**Codegen:**
```cpp
// any(["Status.Buffed", "Status.Empowered"]) and not("Status.Stunned")

FGameplayTagContainer BuffTags;
BuffTags.AddTag(MyGameTags::Status::Buff::Strength);
BuffTags.AddTag(MyGameTags::Status::Buff::Empowered);

FGameplayTagContainer BlockedTags;
BlockedTags.AddTag(MyGameTags::Status::CC::Stunned);

FGameplayTagQuery Query = 
    FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
        .And(FGameplayTagQuery::MakeQuery_MatchNoTags(BlockedTags));

bool bMatches = Query.Matches(OwnerTags);
```

### Task 1.5: Tag Event Decorators

**Syntax to support:**
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

**Codegen:**
```cpp
void ACharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->RegisterGameplayTagEvent(
            MyGameTags::Status::CC::Stunned,
            EGameplayTagEventType::NewOrRemoved
        ).AddUObject(this, &ACharacter::OnStunnedTagChanged);
    }
}

void ACharacter::OnStunnedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        OnStunned();
    }
    else
    {
        OnUnstunned();
    }
}
```



---

## Phase 2: Attribute Sets ✅ COMPLETE

**Status: PRODUCTION-READY**  
**Tests: 20/20 passing**  
**Compression: 1:31.5**  
**Documentation: PHASE2_COMPLETE.md**

### Task 2.1: Attribute Set Parser

**File:** `Kain/crates/kain-core/src/parser/attribute_set.rs`

**Syntax:**
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
    
    fn pre_attribute_change(attribute: GameplayAttribute, new_value: Float):
        if attribute == get_health_attribute():
            new_value = clamp(new_value, 0.0, get_max_health())
    
    fn post_gameplay_effect_execute(data: GameplayEffectModCallbackData):
        if data.evaluated_data.attribute == get_damage_attribute():
            set_health(clamp(get_health() - get_damage(), 0.0, get_max_health()))
            set_damage(0.0)
```

### Task 2.2: Attribute Set IR

**File:** `Kain/crates/ue5-gas/src/attribute_set_ir.rs`

```rust
pub struct AttributeSetIR {
    pub name: String,
    pub attributes: Vec<AttributeIR>,
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
}
```

### Task 2.3: Attribute Set Codegen

**File:** `Kain/crates/ue5-gas/src/attribute_set_codegen.rs`

**Generate:**
1. Header with UCLASS, ATTRIBUTE_ACCESSORS, UPROPERTY, RepNotify declarations
2. Implementation with constructor, GetLifetimeReplicatedProps, RepNotify, lifecycle hooks
3. Automatic clamping logic in PreAttributeChange
4. Meta attribute conversion in PostGameplayEffectExecute

**Compression: 10 lines KAIN → 150 lines C++ (1:15)**

---

## Phase 3: Gameplay Abilities

**Priority: P1 — After Tags**

### Task 3.1: Ability Parser

**File:** `Kain/crates/kain-core/src/parser/ability.rs`

**Syntax:**
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
    
    @cost
    effect: StaminaCostEffect
    
    @cooldown
    effect: JumpCooldownEffect
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        get_avatar_actor_from_actor_info().jump()
        end_ability(handle, actor_info, activation_info, true, false)
```

### Task 3.2: Ability IR

**File:** `Kain/crates/ue5-gas/src/ability_ir.rs`

```rust
pub struct GameplayAbilityIR {
    pub name: String,
    pub instancing_policy: InstancingPolicy,
    pub replication_policy: ReplicationPolicy,
    pub net_execution_policy: NetExecutionPolicy,
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
```

### Task 3.3: Ability Codegen

**File:** `Kain/crates/ue5-gas/src/ability_codegen.rs`

**Generate:**
1. Header with UCLASS, lifecycle hook declarations
2. Constructor with policies, tags, cost/cooldown
3. CanActivateAbility, ActivateAbility, EndAbility implementations
4. Input binding support

**Compression: 15 lines KAIN → 120 lines C++ (1:8)**

---

## Phase 4: Gameplay Effects

**Priority: P1 — After Tags**

### Task 4.1: Effect Parser

**File:** `Kain/crates/kain-core/src/parser/effect.rs`

**Syntax:**
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
    
    @application_tag_requirements
    require: ["Weakness.Fire"]
    ignore: ["Immunity.Fire"]
```

### Task 4.2: Effect IR

**File:** `Kain/crates/ue5-gas/src/effect_ir.rs`

```rust
pub struct GameplayEffectIR {
    pub name: String,
    pub duration_policy: DurationPolicy,
    pub duration_magnitude: Option<f32>,
    pub period: Option<f32>,
    pub execute_on_application: bool,
    pub modifiers: Vec<ModifierIR>,
    pub stacking: Option<StackingIR>,
    pub owned_tags: Vec<String>,
    pub granted_tags: Vec<String>,
    pub application_tag_requirements: TagRequirementsIR,
    pub ongoing_tag_requirements: TagRequirementsIR,
    pub removal_tag_requirements: TagRequirementsIR,
}

pub struct ModifierIR {
    pub attribute: String,
    pub operation: ModifierOp,
    pub magnitude: MagnitudeIR,
}

pub enum ModifierOp {
    Add,
    Multiply,
    Divide,
    Override,
}
```

### Task 4.3: Effect Codegen

**File:** `Kain/crates/ue5-gas/src/effect_codegen.rs`

**Generate:**
1. Header with UCLASS
2. Constructor with duration, period, modifiers, tags, stacking

**Compression: 12 lines KAIN → 80 lines C++ (1:7)**

---

## Phase 5: Integration & Testing

### Task 5.1: Packager Integration

**File:** `Kain/crates/cli/src/packager/ue5_pipeline.rs`

**Add attribute-driven dispatch:**
```rust
match item {
    Item::Struct(s) if has_attribute(&s.attributes, "attribute_set") => {
        let ir = ue5_gas::attribute_set_ir::from_ast(s)?;
        let code = ue5_gas::attribute_set_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::Struct(s) if has_attribute(&s.attributes, "ability") => {
        let ir = ue5_gas::ability_ir::from_ast(s)?;
        let code = ue5_gas::ability_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::Struct(s) if has_attribute(&s.attributes, "gameplay_effect") => {
        let ir = ue5_gas::effect_ir::from_ast(s)?;
        let code = ue5_gas::effect_codegen::generate(&ir)?;
        output.add_file(code);
    }
    Item::GameplayTags(tags) => {
        let ir = ue5_gas::tags_ir::from_ast(tags)?;
        let code = ue5_gas::tags_codegen::generate(&ir)?;
        output.add_file(code);
    }
}
```

### Task 5.2: Module Dependencies

**Add to Build.cs:**
```cpp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameplayAbilities",  // CRITICAL
    "GameplayTags",       // CRITICAL
    "GameplayTasks",
    "NetCore",
});
```

### Task 5.3: Unit Tests

**File:** `Kain/crates/ue5-gas/tests/attribute_set_tests.rs`

Test:
- Attribute clamping
- Replication codegen
- Meta attribute conversion
- Lifecycle hooks
- ATTRIBUTE_ACCESSORS macro generation

**File:** `Kain/crates/ue5-gas/tests/ability_tests.rs`

Test:
- Tag requirements codegen
- Cost/cooldown codegen
- Instancing policies
- Lifecycle hooks

**File:** `Kain/crates/ue5-gas/tests/effect_tests.rs`

Test:
- Duration types
- Modifier operations
- Magnitude types
- Stacking rules
- Tag requirements

**File:** `Kain/crates/ue5-gas/tests/tags_tests.rs`

Test:
- Tag registration (native + .ini)
- Tag hierarchy generation
- Tag query codegen
- Tag event codegen

---

## Phase 6: Advanced Features (Future)

### Ability Tasks
- WaitTargetData
- WaitGameplayEvent
- WaitDelay
- WaitAttributeChange
- WaitGameplayTagAdd/Remove
- PlayMontageAndWait

### Gameplay Cues
- GameplayCueNotify_Actor
- GameplayCueNotify_Static
- Particle/sound integration

### Execution Calculations
- Custom damage calculations
- Attribute capture
- Complex formulas

### Ability Sets (Lyra Pattern)
- Bundle abilities, effects, attribute sets
- Grant/remove as a unit
- Equipment system integration

---

## Crate Structure

```
Kain/crates/ue5-gas/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── tags_ir.rs
│   ├── tags_codegen.rs
│   ├── attribute_set_ir.rs
│   ├── attribute_set_codegen.rs
│   ├── ability_ir.rs
│   ├── ability_codegen.rs
│   ├── effect_ir.rs
│   ├── effect_codegen.rs
│   └── type_mapper.rs
├── tests/
│   ├── tags_tests.rs
│   ├── attribute_set_tests.rs
│   ├── ability_tests.rs
│   └── effect_tests.rs
└── README.md
```

**Dependencies:**
```toml
[dependencies]
kain-core = { path = "../kain-core" }
anyhow = "1.0"
thiserror = "1.0"
```

---

## Success Metrics

### Compression Ratios
- Attribute Sets: 1:15 ✅
- Abilities: 1:8 ✅
- Effects: 1:7 ✅
- Tags: 1:6 ✅
- Overall: 1:10 ✅

### Test Coverage
- Unit tests: 50+ tests
- Integration tests: 10+ tests
- Property-based tests: 5+ tests

### Production Validation
- Build 3 GAS showcase plugins
- Compile without errors
- Run in UE5 editor
- Test multiplayer replication

---

## Timeline Estimate

**Phase 1 (Tags):** 3-4 days
- Parser: 1 day
- IR: 0.5 days
- Codegen: 1 day
- Query parser: 0.5 days
- Event decorators: 1 day

**Phase 2 (Attribute Sets):** 2-3 days
- Parser: 0.5 days
- IR: 0.5 days
- Codegen: 1.5 days
- Tests: 0.5 days

**Phase 3 (Abilities):** 2-3 days
- Parser: 0.5 days
- IR: 0.5 days
- Codegen: 1.5 days
- Tests: 0.5 days

**Phase 4 (Effects):** 2-3 days
- Parser: 0.5 days
- IR: 0.5 days
- Codegen: 1.5 days
- Tests: 0.5 days

**Phase 5 (Integration):** 1-2 days
- Packager integration: 0.5 days
- Module dependencies: 0.5 days
- End-to-end tests: 1 day

**Total: 10-15 days**

---

## Risk Mitigation

### Risk 1: Tag System Complexity
**Mitigation:** Start with simple flat tags, add hierarchy later

### Risk 2: Replication Complexity
**Mitigation:** Use Lyra as reference, test with multiplayer

### Risk 3: Attribute Set Lifecycle Hooks
**Mitigation:** Generate boilerplate, let user override

### Risk 4: Effect Magnitude Types
**Mitigation:** Start with ScalableFloat, add others incrementally

---

## Next Steps

1. **Create ue5-gas crate skeleton**
2. **Implement Phase 1 (Tags) — CRITICAL**
3. **Test tag generation with simple example**
4. **Implement Phase 2 (Attribute Sets)**
5. **Test attribute set generation**
6. **Implement Phase 3 (Abilities)**
7. **Implement Phase 4 (Effects)**
8. **Integration testing**
9. **Create GAS showcase plugin**
10. **Documentation**

