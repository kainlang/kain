# GAS Phases 5-7 Master Implementation Plan

**Status:** Ready to Execute  
**Total Estimated Time:** 7 days  
**Strategy:** Sequential implementation (one phase at a time)  
**Current Progress:** 4/7 phases complete (57%)

---

## Executive Summary

We're implementing the final 3 phases of KAIN's Gameplay Ability System support:
- **Phase 5:** Gameplay Cues (cosmetic events)
- **Phase 6:** Ability Tasks (async operations)
- **Phase 7:** Target Actors (targeting system)

These phases build on the solid foundation of Phases 1-4 (151 tests passing) and will complete the GAS feature set.

---

## Implementation Strategy

### Why Sequential?

**Dependency Chain:**
```
Phase 5 (Cues) ← depends on Phase 4 (Effects)
Phase 6 (Tasks) ← depends on Phase 3 (Abilities)
Phase 7 (Actors) ← depends on Phase 6 (Tasks)
```

**Risk Mitigation:**
- Avoid AST merge conflicts
- Test each phase thoroughly before moving on
- Maintain momentum from Phases 1-4
- Easier debugging and validation

**Timeline Benefits:**
- Sequential: 7 days total, low risk
- Parallel: 3 days total, high risk of integration issues
- **Recommendation:** Sequential (proven pattern from Phases 1-4)

---

## Phase 5: Gameplay Cues

**Duration:** 2 days  
**Complexity:** Low  
**Dependencies:** Phase 1 (Tags), Phase 4 (Effects)

### What Are Gameplay Cues?

Cosmetic events (VFX, SFX, camera shakes) triggered by gameplay effects and abilities.

### Key Features
- Static cues (UGameplayCueNotify_Static)
- Actor cues (AGameplayCueNotify_Actor)
- Lifecycle events (OnExecute, OnAdd, OnRemove, WhileActive)
- Tag-based identification (GameplayCue.*)
- Networked replication

### Syntax Example
```kain
@gameplay_cue
struct BurnCue:
    tag: "GameplayCue.Effect.Burn"
    
    on_execute:
        spawn_particle("P_Burn_Impact", location)
        play_sound("S_Burn_Impact", location)
    
    on_add:
        spawn_particle_attached("P_Burn_Loop", target, "spine_01")
    
    on_remove:
        spawn_particle("P_Burn_End", location)
```

### Implementation Tasks
1. AST structures (0.5h)
2. Parser implementation (2h)
3. IR implementation (1.5h)
4. Codegen implementation (3h)
5. Unit tests (2h)
6. Integration tests (2h)
7. CLI integration (1h)
8. Documentation (1h)

**Total:** 13 hours (2 days)

### Success Criteria
- ✅ 35 tests passing (20 unit + 15 integration)
- ✅ Static cues generate correctly
- ✅ Actor cues generate correctly
- ✅ Lifecycle methods work
- ✅ Tag validation works
- ✅ CLI integration functional
- ✅ Compression ratio: 1:6 to 1:8

### Files Created
- `Kain/crates/kain-core/src/ast.rs` (GameplayCueDef)
- `Kain/crates/ue5-gas/src/cue_ir.rs` (new)
- `Kain/crates/ue5-gas/src/cue_codegen.rs` (new)
- `Kain/crates/ue5-gas/tests/cue_ir_tests.rs` (new)
- `Kain/crates/ue5-gas/tests/cue_integration_tests.rs` (new)

### Output Structure
```
MyPlugin/
├── Source/
│   └── MyPlugin/
│       ├── Public/
│       │   └── Cues/
│       │       ├── BurnCue.h
│       │       └── ShieldCue.h
│       └── Private/
│           └── Cues/
│               ├── BurnCue.cpp
│               └── ShieldCue.cpp
```

---

## Phase 6: Ability Tasks

**Duration:** 3 days  
**Complexity:** High  
**Dependencies:** Phase 3 (Abilities), Phase 5 (Cues)

### What Are Ability Tasks?

Async operations that abilities can wait on (targeting, events, montages, delays).

### Key Features
- Built-in tasks (WaitTargetData, WaitGameplayEvent, PlayMontageAndWait, WaitDelay)
- Custom tasks with delegates
- Async/await syntax
- Cancellation support
- Networked execution

### Syntax Example
```kain
@ability
struct FireballAbility:
    fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
        # Wait for player to select target
        let target_data = await wait_target_data(
            confirmation: "UserConfirmed",
            target_actor_class: "LineTraceTargetActor",
            max_range: 1000.0
        )
        
        if target_data.is_valid():
            spawn_projectile("BP_Fireball", target_data.get_hit_location())
        
        end_ability(handle, actor_info, activation_info, true, false)
```

### Implementation Tasks
1. AST structures (1h)
2. Parser implementation (3h)
3. IR implementation (2h)
4. Codegen implementation (4h)
5. Built-in tasks (3h)
6. Unit tests (3h)
7. Integration tests (3h)
8. CLI integration (1.5h)
9. Documentation (1.5h)

**Total:** 22 hours (3 days)

### Success Criteria
- ✅ 45 tests passing (25 unit + 20 integration)
- ✅ Built-in tasks generate correctly
- ✅ Custom tasks generate correctly
- ✅ Await syntax works
- ✅ Delegates generate correctly
- ✅ CLI integration functional
- ✅ Compression ratio: 1:10 to 1:12

### Files Created
- `Kain/crates/kain-core/src/ast.rs` (AbilityTaskDef, Expr::Await)
- `Kain/crates/ue5-gas/src/task_ir.rs` (new)
- `Kain/crates/ue5-gas/src/task_codegen.rs` (new)
- `Kain/crates/ue5-gas/tests/task_ir_tests.rs` (new)
- `Kain/crates/ue5-gas/tests/task_integration_tests.rs` (new)

### Output Structure
```
MyPlugin/
├── Source/
│   └── MyPlugin/
│       ├── Public/
│       │   └── Tasks/
│       │       ├── WaitTargetDataTask.h
│       │       └── CustomTask.h
│       └── Private/
│           └── Tasks/
│               ├── WaitTargetDataTask.cpp
│               └── CustomTask.cpp
```

---

## Phase 7: Target Actors

**Duration:** 2 days  
**Complexity:** Medium  
**Dependencies:** Phase 3 (Abilities), Phase 6 (Tasks)

### What Are Target Actors?

Target selection and filtering system for abilities (traces, filters, reticles).

### Key Features
- Trace types (Line, Sphere, Cone, Box, Cylinder)
- Filters (tags, team, class, range)
- Custom filtering logic
- Reticle spawning
- Networked confirmation

### Syntax Example
```kain
@target_actor
struct LineTraceTarget:
    trace_type: "Line"
    max_range: 1000.0
    trace_channel: "Visibility"
    
    filter:
        self_filter: "Exclude"
        required_actor_class: "ACharacter"
        require_tags: ["Status.Alive"]
        ignore_tags: ["Status.Dead"]
    
    reticle_class: "BP_LineTraceReticle"
```

### Implementation Tasks
1. AST structures (1h)
2. Parser implementation (2.5h)
3. IR implementation (1.5h)
4. Codegen implementation (3.5h)
5. Unit tests (2h)
6. Integration tests (2h)
7. CLI integration (1h)
8. Documentation (1h)

**Total:** 14.5 hours (2 days)

### Success Criteria
- ✅ 35 tests passing (20 unit + 15 integration)
- ✅ All trace types generate correctly
- ✅ Filters generate correctly
- ✅ Custom logic generates correctly
- ✅ Reticle spawning works
- ✅ CLI integration functional
- ✅ Compression ratio: 1:8 to 1:10

### Files Created
- `Kain/crates/kain-core/src/ast.rs` (TargetActorDef)
- `Kain/crates/ue5-gas/src/target_ir.rs` (new)
- `Kain/crates/ue5-gas/src/target_codegen.rs` (new)
- `Kain/crates/ue5-gas/tests/target_ir_tests.rs` (new)
- `Kain/crates/ue5-gas/tests/target_integration_tests.rs` (new)

### Output Structure
```
MyPlugin/
├── Source/
│   └── MyPlugin/
│       ├── Public/
│       │   └── Targeting/
│       │       ├── LineTraceTarget.h
│       │       └── AOETarget.h
│       └── Private/
│           └── Targeting/
│               ├── LineTraceTarget.cpp
│               └── AOETarget.cpp
```

---

## Timeline

### Week 1: Phase 5 (Days 1-2)
- **Day 1:** AST, Parser, IR (morning), Codegen (afternoon)
- **Day 2:** Tests (morning), CLI integration + docs (afternoon)

### Week 2: Phase 6 (Days 3-5)
- **Day 3:** AST, Parser (morning), IR (afternoon)
- **Day 4:** Codegen + built-in tasks (full day)
- **Day 5:** Tests (morning), CLI integration + docs (afternoon)

### Week 2: Phase 7 (Days 6-7)
- **Day 6:** AST, Parser, IR (morning), Codegen (afternoon)
- **Day 7:** Tests (morning), CLI integration + docs (afternoon)

**Total:** 7 working days

---

## Test Coverage Summary

| Phase | Unit Tests | Integration Tests | Total |
|-------|-----------|-------------------|-------|
| Phase 5 | 20 | 15 | 35 |
| Phase 6 | 25 | 20 | 45 |
| Phase 7 | 20 | 15 | 35 |
| **Total** | **65** | **50** | **115** |

**Grand Total (All Phases):** 151 (current) + 115 (new) = **266 tests**

---

## Compression Ratios

| Phase | KAIN Lines | C++ Lines | Ratio | Example |
|-------|-----------|-----------|-------|---------|
| Phase 5 | 10 | 60-80 | 1:6-1:8 | Cue with lifecycle |
| Phase 6 | 15 | 150-180 | 1:10-1:12 | Task with delegates |
| Phase 7 | 12 | 96-120 | 1:8-1:10 | Target with filters |

**Average:** 1:8 to 1:10 across all new phases

---

## CLI Integration Pattern

Each phase follows the same pattern established in Phases 1-4:

### 1. Extraction (before type checking)
```rust
let gameplay_cues = extract_gameplay_cues(&merged);
merged.items.retain(|item| !matches!(item, Item::GameplayCue(_)));
```

### 2. Generation Step
```rust
// STEP 3.11: Generate GameplayCues
if !gameplay_cues.is_empty() {
    println!("🎬 Generating {} GameplayCue(s)...", gameplay_cues.len());
    // Generate files...
}
```

### 3. Module Dependencies
Automatically added when features detected:
- Phase 5: No new modules (uses existing GameplayAbilities)
- Phase 6: No new modules (uses existing GameplayAbilities)
- Phase 7: No new modules (uses existing GameplayAbilities)

---

## Risk Assessment

### Low Risk
- ✅ Pattern established from Phases 1-4
- ✅ Sequential approach avoids conflicts
- ✅ Each phase independently testable
- ✅ Clear UE5 reference implementations

### Medium Risk
- ⚠️ Phase 6 async/await syntax (new language feature)
- ⚠️ Phase 7 custom filtering logic (complex codegen)

### Mitigation
- Start with simple cases, add complexity incrementally
- Test each feature thoroughly before moving on
- Use subagents for parallel work within each phase (not across phases)

---

## Success Metrics

### Phase 5 Complete When:
- [ ] 35/35 tests passing
- [ ] Static and actor cues generate correctly
- [ ] CLI integration works end-to-end
- [ ] Example_GAS updated with cue examples
- [ ] Documentation complete

### Phase 6 Complete When:
- [ ] 45/45 tests passing
- [ ] Built-in and custom tasks generate correctly
- [ ] Await syntax works in abilities
- [ ] CLI integration works end-to-end
- [ ] Example_GAS updated with task examples
- [ ] Documentation complete

### Phase 7 Complete When:
- [ ] 35/35 tests passing
- [ ] All trace types generate correctly
- [ ] Filters and custom logic work
- [ ] CLI integration works end-to-end
- [ ] Example_GAS updated with targeting examples
- [ ] Documentation complete

### All Phases Complete When:
- [ ] 266/266 tests passing (151 existing + 115 new)
- [ ] 7/7 GAS phases complete (100%)
- [ ] Full GAS feature parity with UE5
- [ ] Example_GAS demonstrates all features
- [ ] Documentation complete for all phases

---

## Next Steps

1. **Review this plan** — Confirm approach and timeline
2. **Start Phase 5** — Begin with gameplay cues implementation
3. **Test thoroughly** — Ensure Phase 5 works before Phase 6
4. **Iterate** — Phase 6, then Phase 7
5. **Celebrate** — Complete GAS implementation! 🎉

---

## Documentation Structure

```
Kain/crates/ue5-gas/
├── PHASE5_IMPLEMENTATION_PLAN.md ✅
├── PHASE6_IMPLEMENTATION_PLAN.md ✅
├── PHASE7_IMPLEMENTATION_PLAN.md ✅
├── PHASES_5_6_7_MASTER_PLAN.md ✅ (this file)
├── GAS_PHASE_STATUS.md (update after each phase)
├── GAS_ARCHITECTURE_ANALYSIS.md (reference)
└── PHASE[5-7]_COMPLETE.md (create after each phase)
```

---

**Ready to begin Phase 5!** 🚀

Let's complete the GAS implementation and achieve 100% feature parity with UE5.
