# Phase 6: Ability Tasks — Implementation Plan

**Status:** Ready to Implement (After Phase 5)  
**Priority:** High  
**Estimated Effort:** 3 days  
**Dependencies:** Phase 3 (Abilities), Phase 5 (Cues)

---

## Overview

Ability Tasks are async operations that abilities can wait on. They enable complex ability behaviors like targeting, waiting for events, playing montages, and more.

### Key Characteristics
- **Async Operations** — Non-blocking waits
- **Delegate-Based** — Callbacks for completion/cancellation
- **Networked** — Can replicate to server
- **Composable** — Tasks can chain together
- **Cancellable** — Can be cancelled mid-execution

---

## UE5 Reference

### Core Class

**UAbilityTask** — Base class for all ability tasks
```cpp
UCLASS(Abstract)
class UAbilityTask : public UGameplayTask
{
    GENERATED_BODY()
    
public:
    // The ability that owns this task
    UPROPERTY()
    TObjectPtr<UGameplayAbility> Ability;
    
    // The ability system component
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
    
    // Called when task is activated
    virtual void Activate() override;
    
    // Called when task ends
    virtual void OnDestroy(bool bInOwnerFinished) override;
    
    // Called when ability ends
    virtual void OnAbilityEnded();
    
    // Helper to get ability
    UGameplayAbility* GetAbility() const { return Ability; }
    
    // Helper to get ASC
    UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }
};
```

### Common Task: WaitTargetData
```cpp
UCLASS()
class UAbilityTask_WaitTargetData : public UAbilityTask
{
    GENERATED_BODY()
    
public:
    // Delegate for when target data is ready
    UPROPERTY(BlueprintAssignable)
    FWaitTargetDataDelegate ValidData;
    
    // Delegate for when cancelled
    UPROPERTY(BlueprintAssignable)
    FWaitTargetDataDelegate Cancelled;
    
    // Create task
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
    static UAbilityTask_WaitTargetData* WaitTargetData(
        UGameplayAbility* OwningAbility,
        FName TaskInstanceName,
        TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType,
        TSubclassOf<AGameplayAbilityTargetActor> Class
    );
    
    virtual void Activate() override;
    
protected:
    void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data);
    void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);
};
```

### Common Task: WaitGameplayEvent
```cpp
UCLASS()
class UAbilityTask_WaitGameplayEvent : public UAbilityTask
{
    GENERATED_BODY()
    
public:
    // Delegate for when event is received
    UPROPERTY(BlueprintAssignable)
    FWaitGameplayEventDelegate EventReceived;
    
    // Create task
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
    static UAbilityTask_WaitGameplayEvent* WaitGameplayEvent(
        UGameplayAbility* OwningAbility,
        FGameplayTag EventTag,
        AActor* OptionalExternalTarget = nullptr,
        bool OnlyTriggerOnce = false,
        bool OnlyMatchExact = true
    );
    
    virtual void Activate() override;
    
protected:
    void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);
};
```

---

## KAIN Syntax Design

### WaitTargetData Task
```kain
@ability
struct FireballAbility:
    @instancing(policy: "InstancedPerExecution")
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        # Wait for player to select target
        let target_data = await wait_target_data(
            confirmation: "UserConfirmed",
            target_actor_class: "LineTraceTargetActor",
            max_range: 1000.0
        )
        
        if target_data.is_valid():
            spawn_projectile("BP_Fireball", target_data.get_hit_location())
            apply_damage_at_location(target_data.get_hit_location(), 50.0)
        
        end_ability(handle, actor_info, activation_info, true, false)
```

### WaitGameplayEvent Task
```kain
@ability
struct ComboAttackAbility:
    @instancing(policy: "InstancedPerActor")
    
    state combo_count: Int = 0
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        combo_count = combo_count + 1
        
        play_montage("Attack_Combo_" + str(combo_count))
        
        # Wait for next input within 1 second
        let event_data = await wait_gameplay_event(
            tag: "Input.Attack",
            timeout: 1.0,
            only_trigger_once: true
        )
        
        if event_data.is_valid() and combo_count < 3:
            # Continue combo
            activate_ability(handle, actor_info, activation_info, event_data)
        else:
            # End combo
            combo_count = 0
            end_ability(handle, actor_info, activation_info, true, false)
```

### PlayMontageAndWait Task
```kain
@ability
struct HeavyAttackAbility:
    @instancing(policy: "InstancedPerExecution")
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        
        # Play montage and wait for it to finish
        let montage_result = await play_montage_and_wait(
            montage: "Attack_Heavy",
            rate: 1.0,
            section: "Default"
        )
        
        if montage_result == "Completed":
            apply_damage_to_target(100.0, "Damage.Physical.Crush")
        elif montage_result == "Interrupted":
            println("Attack interrupted!")
        
        end_ability(handle, actor_info, activation_info, true, false)
```

### WaitDelay Task
```kain
@ability
struct DelayedHealAbility:
    @instancing(policy: "InstancedPerExecution")
    
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        if not commit_ability(handle, actor_info, activation_info):
            end_ability(handle, actor_info, activation_info, true, true)
            return
        
        play_animation("Cast_Heal")
        
        # Wait 2 seconds before healing
        await wait_delay(2.0)
        
        apply_heal_effect(get_avatar_actor(), 50.0)
        
        end_ability(handle, actor_info, activation_info, true, false)
```

### Custom Task
```kain
@ability_task
struct WaitAttributeChange:
    @delegate
    on_attribute_changed: AttributeChangeDelegate
    
    @delegate
    on_cancelled: TaskCancelledDelegate
    
    state attribute: GameplayAttribute
    state threshold: Float
    state comparison: ComparisonType  # GreaterThan, LessThan, Equal
    
    fn activate():
        # Register attribute change callback
        let asc = get_ability_system_component()
        asc.register_attribute_change_callback(attribute, on_attribute_value_changed)
    
    fn on_attribute_value_changed(new_value: Float):
        let meets_condition = false
        
        match comparison:
            ComparisonType.GreaterThan => meets_condition = new_value > threshold
            ComparisonType.LessThan => meets_condition = new_value < threshold
            ComparisonType.Equal => meets_condition = new_value == threshold
        
        if meets_condition:
            on_attribute_changed.broadcast(new_value)
            end_task()
    
    fn on_destroy():
        # Unregister callback
        let asc = get_ability_system_component()
        asc.unregister_attribute_change_callback(attribute, on_attribute_value_changed)
```

---

## Implementation Tasks

### Task 6.1: AST Structures

**File:** `Kain/crates/kain-core/src/ast.rs`

```rust
#[derive(Debug, Clone)]
pub struct AbilityTaskDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub delegates: Vec<DelegateDef>,
    pub state_fields: Vec<StructField>,
    pub activate_method: Option<FunctionDef>,
    pub on_destroy_method: Option<FunctionDef>,
    pub custom_methods: Vec<FunctionDef>,
    pub span: Span,
}

#[derive(Debug, Clone)]
pub struct DelegateDef {
    pub name: String,
    pub delegate_type: String,
    pub span: Span,
}

// Add to Item enum
pub enum Item {
    // ... existing variants
    AbilityTask(AbilityTaskDef),
}

// Add await expression to Expr enum
pub enum Expr {
    // ... existing variants
    Await(Box<Expr>, Span),
}
```

**Estimated Time:** 1 hour

---

### Task 6.2: Parser Implementation

**File:** `Kain/crates/kain-core/src/parser.rs`

Add `parse_ability_task()` and `parse_await_expr()`:
```rust
fn parse_ability_task(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
    // Similar to parse_struct but with task-specific fields
    // Parse delegates, state, activate, on_destroy methods
}

fn parse_await_expr(&mut self) -> KainResult<Expr> {
    self.expect(TokenKind::Await)?;
    let expr = self.parse_primary()?;
    Ok(Expr::Await(Box::new(expr), self.current_span()))
}
```

**Estimated Time:** 3 hours

---

### Task 6.3: IR Implementation

**File:** `Kain/crates/ue5-gas/src/task_ir.rs` (new file)

```rust
#[derive(Debug, Clone)]
pub struct AbilityTaskIR {
    pub name: String,
    pub delegates: Vec<DelegateIR>,
    pub state_fields: Vec<StateFieldIR>,
    pub activate_body: Option<String>,
    pub on_destroy_body: Option<String>,
    pub custom_methods: Vec<MethodIR>,
}

#[derive(Debug, Clone)]
pub struct DelegateIR {
    pub name: String,
    pub delegate_type: DelegateTypeIR,
}

#[derive(Debug, Clone, PartialEq)]
pub enum DelegateTypeIR {
    AttributeChange,
    TaskCancelled,
    TargetDataReady,
    GameplayEvent,
    Custom(String),
}
```

**Estimated Time:** 2 hours

---

### Task 6.4: Codegen Implementation

**File:** `Kain/crates/ue5-gas/src/task_codegen.rs` (new file)

Generate UAbilityTask subclasses with:
- Delegate declarations (UPROPERTY(BlueprintAssignable))
- State fields
- Static factory method
- Activate() override
- OnDestroy() override
- Custom methods

**Estimated Time:** 4 hours

---

## Testing Strategy

### Unit Tests (25 tests)
- Delegate parsing
- State field parsing
- Await expression parsing
- Task lifecycle methods

### Integration Tests (20 tests)
- WaitTargetData task
- WaitGameplayEvent task
- PlayMontageAndWait task
- WaitDelay task
- Custom task with delegates

**Total Tests:** 45

---

## Success Criteria

- ✅ 45 tests passing
- ✅ Built-in tasks generate correctly
- ✅ Custom tasks generate correctly
- ✅ Await syntax works
- ✅ Delegates generate correctly
- ✅ CLI integration functional
- ✅ Compression ratio: 1:10 to 1:12

---

**Phase 6 Ready After Phase 5!**
