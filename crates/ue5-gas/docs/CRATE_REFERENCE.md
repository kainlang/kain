# ue5-gas — Gameplay Ability System Reference

> **Last Updated:** 2026-03-01
> **Status:** Phases 1 & 2 production. Phases 3 & 4 have IR + codegen but not wired into CLI pipeline yet.

---

## Purpose

Gameplay Ability System (GAS) codegen for KAIN. Maps KAIN GAS constructs to UE5 `UGameplayAbility`, `UGameplayEffect`, `UAttributeSet`, `UGameplayCueNotify_*`, `UGameplayAbilityTask`, and `UGameplayTargetDataFilter_*` C++ classes.

---

## GAS Phases

| Phase | Construct | IR file | Codegen file | Status |
|---|---|---|---|---|
| 1 | `@gameplay_tags` | `tags_ir.rs` (4.6KB) | `tags_codegen.rs` (16KB) | ✅ Production |
| 2 | `@attribute_set struct` | `attribute_set_ir.rs` (9.7KB) | `attribute_set_codegen.rs` (15.5KB) | ✅ Production |
| 3a | `@ability struct` | `ability_ir.rs` (11.6KB) | `ability_codegen.rs` (17.6KB) | 🔶 IR + codegen complete, no CLI wiring |
| 3b | `@ability_task struct` | `task_ir.rs` (4.3KB) | `task_codegen.rs` (8.1KB) | 🔶 Same |
| 3c | `@target_actor struct` | `target_ir.rs` (3.2KB) | `target_codegen.rs` (4.4KB) | 🔶 Same |
| 4a | `@effect struct` | `effect_ir.rs` (11KB) | `effect_codegen.rs` (17.8KB) | 🔶 IR + codegen complete, no CLI wiring |
| 4b | `@gameplay_cue struct` | `cue_ir.rs` (4.3KB) | `cue_codegen.rs` (11.3KB) | 🔶 Same |

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `tags_ir.rs` | 4.6KB | `GameplayTagsNamespace` IR — tag hierarchy |
| `tags_codegen.rs` | 16KB | `UGameplayTagsManager` registration + `FGameplayTag` constants |
| `attribute_set_ir.rs` | 9.7KB | `AttributeSetDef` — attributes with min/max/clamping |
| `attribute_set_codegen.rs` | 15.5KB | `UAttributeSet` subclass with `OnRep_*` + `GetLifetimeReplicatedProps` |
| `ability_ir.rs` | 11.6KB | `GameplayAbilityDef` — cost, cooldown, tags, activation, effects |
| `ability_codegen.rs` | 17.6KB | `UGameplayAbility` subclass with all phase overrides |
| `task_ir.rs` | 4.3KB | `AbilityTaskDef` — delegates, lifecycle hooks |
| `task_codegen.rs` | 8.1KB | `UAbilityTask_*` subclass |
| `target_ir.rs` | 3.2KB | `TargetActorDef` — trace type, filters, reticle |
| `target_codegen.rs` | 4.4KB | `AGameplayAbilityTargetActor_*` |
| `effect_ir.rs` | 11KB | `GameplayEffectDef` — spec, magnitudes, periods |
| `effect_codegen.rs` | 17.8KB | `UGameplayEffect` subclass with modifier specs |
| `cue_ir.rs` | 4.3KB | `GameplayCueDef` — onActive/whileActive/onRemove |
| `cue_codegen.rs` | 11.3KB | `UGameplayCueNotify_Static` or `_Actor` |

---

## KAIN GAS Syntax

### Phase 1 — Gameplay Tags

```kain
@gameplay_tags namespace Combat:
    Attack:
        Melee
        Ranged
    Defense:
        Block
        Dodge
```

→ `GAMEPLAY_TAG_COMMENT` macro setup + `FGameplayTag` constants registered with `UGameplayTagsManager`.

### Phase 2 — Attribute Sets

```kain
@attribute_set struct CombatAttributes:
    @replicated(on_rep: true)
    health: Float = 100.0
    @min(0.0)
    @max(1000.0)
    max_health: Float = 100.0
    
    @replicated
    armor: Float = 0.0
```

→ `UCombatAttributesSet : public UAttributeSet` with:
- `UPROPERTY(ReplicatedUsing = OnRep_Health) FGameplayAttributeData Health;`
- `UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldHealth);`
- `GetLifetimeReplicatedProps` with `DOREPLIFETIME_CONDITION_NOTIFY`
- `PostGameplayEffectExecute` for damage/healing clamping

### Phase 3 — Abilities (IR complete)

```kain
@ability struct MeleeAttack:
    @cost(stamina: 20.0)
    @cooldown(duration: 0.5)
    @tags(required: ["Combat.State.Ready"], blocking: ["Combat.State.Stunned"])
    on activate():
        play_montage("MeleeSwing")
        apply_effect(target, DamageEffect)
```

→ `UMeleeAttackAbility : public UGameplayAbility` with `CanActivateAbility`, `ActivateAbility`, `EndAbility`, tag requirements, cost/cooldown `UGameplayEffect` classes.

### Phase 4 — Effects (IR complete)

```kain
@effect struct BurnEffect:
    @duration(5.0, period: 0.5)
    @modifier(attribute: "Health", op: "Add", magnitude: -10.0)
    @tag("Status.Burning")
```

→ `UBurnEffectGameplayEffect : public UGameplayEffect` with `FGameplayEffectSpec`, periodic execution, `FGameplayModifierInfo`.

---

## Known Gaps

| Gap | Impact |
|---|---|
| Phases 3 & 4 not wired into CLI | `kain build --ue5` does not process `@ability` / `@effect` / `@gameplay_cue` yet |
| No `UAbilitySystemComponent` auto-registration | Must manually add to actor |
| No data table integration for effect magnitudes | Magnitudes are hardcoded in generated class |
