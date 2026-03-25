# Phase 7: Target Actors — Implementation Plan

**Status:** Ready to Implement (After Phase 6)  
**Priority:** Medium  
**Estimated Effort:** 2 days  
**Dependencies:** Phase 3 (Abilities), Phase 6 (Tasks)

---

## Overview

Target Actors handle target selection and filtering for abilities. They perform traces, apply filters, and return target data to abilities via WaitTargetData tasks.

### Key Characteristics
- **Trace-Based** — Line, sphere, cone, box traces
- **Filter-Based** — Tag requirements, team checks, range checks
- **Networked** — Client predicts, server confirms
- **Reusable** — Can be used by multiple abilities
- **Configurable** — Range, radius, filters all configurable

---

## UE5 Reference

### Core Class

**AGameplayAbilityTargetActor** — Base class for all target actors
```cpp
ACLASS(Blueprintable, notplaceable)
class AGameplayAbilityTargetActor : public AActor
{
    GENERATED_BODY()
    
public:
    // Start targeting
    virtual void StartTargeting(UGameplayAbility* Ability);
    
    // Confirm targeting
    virtual void ConfirmTargeting();
    
    // Cancel targeting
    virtual void CancelTargeting();
    
    // Called every frame to update targeting
    virtual void Tick(float DeltaSeconds) override;
    
    // Filter for valid targets
    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
    FGameplayTargetDataFilterHandle Filter;
    
    // Reticle class to spawn
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Targeting)
    TSubclassOf<AGameplayAbilityWorldReticle> ReticleClass;
    
    // Reticle instance
    UPROPERTY()
    TObjectPtr<AGameplayAbilityWorldReticle> ReticleActor;
    
    // Owning ability
    UPROPERTY()
    TObjectPtr<UGameplayAbility> OwningAbility;
    
    // Should destroy on confirmation
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Targeting)
    bool bDestroyOnConfirmation;
    
protected:
    // Generate target data
    virtual FGameplayAbilityTargetDataHandle MakeTargetData() const;
};
```

### Line Trace Target Actor
```cpp
ACLASS(Blueprintable)
class AGameplayAbilityTargetActor_Trace : public AGameplayAbilityTargetActor
{
    GENERATED_BODY()
    
public:
    // Max range of trace
    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
    float MaxRange;
    
    // Trace profile name
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Trace)
    FCollisionProfileName TraceProfile;
    
    // Should trace complex collision
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Trace)
    bool bTraceComplex;
    
    // Should trace from player view point
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Trace)
    bool bTraceFromPlayerViewPoint;
    
protected:
    virtual FHitResult PerformTrace(AActor* InSourceActor);
};
```

### Target Data Filter
```cpp
USTRUCT(BlueprintType)
struct FGameplayTargetDataFilter
{
    GENERATED_BODY()
    
    // Actor to filter out (usually self)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Filter)
    TObjectPtr<AActor> SelfActor;
    
    // Required actor class
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Filter)
    TSubclassOf<AActor> RequiredActorClass;
    
    // Self filter type
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Filter)
    ETargetDataFilterSelf SelfFilter;
    
    // Reverses the filter (exclude instead of include)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Filter)
    bool bReverseFilter;
    
    // Filter function
    bool FilterPassesForActor(const AActor* ActorToBeFiltered) const;
};
```

---

## KAIN Syntax Design

### Line Trace Target Actor
```kain
@target_actor
struct LineTraceTarget:
    trace_type: "Line"
    max_range: 1000.0
    trace_channel: "Visibility"
    trace_complex: false
    trace_from_player_view: true
    
    filter:
        self_filter: "Exclude"
        required_actor_class: "ACharacter"
        require_tags: ["Status.Alive"]
        ignore_tags: ["Status.Dead", "Status.Invulnerable"]
    
    reticle_class: "BP_LineTraceReticle"
    destroy_on_confirmation: true
```

### Sphere Trace Target Actor
```kain
@target_actor
struct AOETarget:
    trace_type: "Sphere"
    max_range: 500.0
    radius: 200.0
    trace_channel: "Pawn"
    
    filter:
        self_filter: "Exclude"
        team_filter: "Enemy"
        max_targets: 5
        require_tags: ["Status.Alive"]
    
    reticle_class: "BP_SphereReticle"
    show_range_indicator: true
```

### Cone Trace Target Actor
```kain
@target_actor
struct ConeTarget:
    trace_type: "Cone"
    max_range: 800.0
    cone_angle: 45.0
    trace_channel: "Pawn"
    
    filter:
        self_filter: "Exclude"
        team_filter: "Enemy"
        sort_by: "Distance"  # Distance, Health, Threat
        require_tags: ["Status.Alive"]
        ignore_tags: ["Status.Stealth"]
    
    reticle_class: "BP_ConeReticle"
```

### Custom Target Actor with Logic
```kain
@target_actor
struct SmartTarget:
    trace_type: "Line"
    max_range: 1500.0
    trace_channel: "Visibility"
    
    filter:
        self_filter: "Exclude"
        custom_filter: true
    
    fn custom_filter_check(actor: Actor) -> Bool:
        # Custom filtering logic
        if not actor.has_tag("Status.Alive"):
            return false
        
        # Check if actor is in front of player
        let forward = get_owner_forward_vector()
        let to_actor = normalize(actor.get_location() - get_owner_location())
        let dot = dot_product(forward, to_actor)
        
        if dot < 0.5:  # 60 degree cone
            return false
        
        # Check if actor is visible (not behind cover)
        if not line_of_sight_to(actor):
            return false
        
        return true
    
    fn on_targeting_update(delta_time: Float):
        # Update reticle color based on target validity
        if has_valid_target():
            set_reticle_color(Color.Green)
        else:
            set_reticle_color(Color.Red)
```

---

## Implementation Tasks

### Task 7.1: AST Structures

**File:** `Kain/crates/kain-core/src/ast.rs`

```rust
#[derive(Debug, Clone)]
pub struct TargetActorDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub trace_type: TraceType,
    pub max_range: f32,
    pub radius: Option<f32>,  // For sphere/cylinder
    pub cone_angle: Option<f32>,  // For cone
    pub trace_channel: String,
    pub trace_complex: bool,
    pub trace_from_player_view: bool,
    pub filter: TargetFilterDef,
    pub reticle_class: Option<String>,
    pub destroy_on_confirmation: bool,
    pub custom_filter_method: Option<FunctionDef>,
    pub on_targeting_update: Option<FunctionDef>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TraceType {
    Line,
    Sphere,
    Box,
    Cone,
    Cylinder,
}

#[derive(Debug, Clone)]
pub struct TargetFilterDef {
    pub self_filter: SelfFilterType,
    pub team_filter: Option<TeamFilterType>,
    pub required_actor_class: Option<String>,
    pub require_tags: Vec<String>,
    pub ignore_tags: Vec<String>,
    pub max_targets: Option<i32>,
    pub sort_by: Option<SortType>,
    pub custom_filter: bool,
}

#[derive(Debug, Clone, PartialEq)]
pub enum SelfFilterType {
    Include,
    Exclude,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TeamFilterType {
    Friendly,
    Enemy,
    Neutral,
    All,
}

#[derive(Debug, Clone, PartialEq)]
pub enum SortType {
    Distance,
    Health,
    Threat,
}

// Add to Item enum
pub enum Item {
    // ... existing variants
    TargetActor(TargetActorDef),
}
```

**Estimated Time:** 1 hour

---

### Task 7.2: Parser Implementation

**File:** `Kain/crates/kain-core/src/parser.rs`

```rust
fn parse_target_actor(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
    self.expect(TokenKind::Struct)?;
    let name = self.parse_ident()?;
    self.expect(TokenKind::Colon)?;
    
    self.skip_newlines();
    self.expect(TokenKind::Indent)?;
    
    // Parse target actor fields
    let mut trace_type = None;
    let mut max_range = 1000.0;
    let mut radius = None;
    let mut cone_angle = None;
    // ... etc
    
    while !self.check(TokenKind::Dedent) && !self.at_end() {
        // Parse fields
    }
    
    // Build TargetActorDef
}
```

**Estimated Time:** 2.5 hours

---

### Task 7.3: IR Implementation

**File:** `Kain/crates/ue5-gas/src/target_ir.rs` (new file)

```rust
#[derive(Debug, Clone)]
pub struct TargetActorIR {
    pub name: String,
    pub trace_type: TraceTypeIR,
    pub max_range: f32,
    pub radius: Option<f32>,
    pub cone_angle: Option<f32>,
    pub trace_channel: String,
    pub filter: TargetFilterIR,
    pub reticle_class: Option<String>,
    pub custom_filter_body: Option<String>,
    pub on_targeting_update_body: Option<String>,
}

impl TargetActorIR {
    pub fn from_ast(target: &TargetActorDef) -> KainResult<Self> {
        // Validate and convert
    }
}
```

**Estimated Time:** 1.5 hours

---

### Task 7.4: Codegen Implementation

**File:** `Kain/crates/ue5-gas/src/target_codegen.rs` (new file)

Generate AGameplayAbilityTargetActor subclasses with:
- Trace configuration (range, radius, angle)
- Filter setup (tags, team, class)
- PerformTrace() override
- FilterPassesForActor() override
- Tick() override for custom logic
- Reticle spawning

**Estimated Time:** 3.5 hours

---

## Testing Strategy

### Unit Tests (20 tests)
- Trace type validation
- Filter parsing
- Range/radius validation
- Custom filter parsing

### Integration Tests (15 tests)
- Line trace target
- Sphere trace target
- Cone trace target
- Target with filters
- Target with custom logic

**Total Tests:** 35

---

## Success Criteria

- ✅ 35 tests passing
- ✅ All trace types generate correctly
- ✅ Filters generate correctly
- ✅ Custom logic generates correctly
- ✅ Reticle spawning works
- ✅ CLI integration functional
- ✅ Compression ratio: 1:8 to 1:10

---

**Phase 7 Ready After Phase 6!**
