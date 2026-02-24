# Phase 5: Gameplay Cues — Implementation Plan

**Status:** Ready to Implement  
**Priority:** High  
**Estimated Effort:** 2 days  
**Dependencies:** Phase 1 (Tags), Phase 4 (Effects)

---

## Overview

Gameplay Cues are cosmetic events (VFX, SFX, camera shakes) triggered by gameplay effects, abilities, and other game events. They are purely visual/audio and do not affect gameplay logic.

### Key Characteristics
- **Cosmetic Only** — No gameplay impact
- **Tag-Based** — Identified by GameplayCue.* tags
- **Networked** — Replicated to all clients
- **Event-Driven** — Triggered by effects, abilities, or manual calls
- **Lifecycle** — OnExecute (instant), OnAdd/OnRemove (duration), WhileActive (looping)

---

## UE5 Reference

### Core Classes

**UGameplayCueNotify_Static** — Lightweight, stateless cues
```cpp
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class UGameplayCueNotify_Static : public UObject
{
    GENERATED_BODY()
    
    // Tag this cue responds to
    UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
    FGameplayTag GameplayCueTag;
    
    // Called when cue is executed (instant)
    virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    
    // Called when cue is added (duration start)
    virtual bool OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    
    // Called when cue is removed (duration end)
    virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    
    // Called every frame while active
    virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
};
```

**AGameplayCueNotify_Actor** — Stateful cues with spawned actors
```cpp
ACLASS(Blueprintable)
class AGameplayCueNotify_Actor : public AActor
{
    GENERATED_BODY()
    
    // Tag this cue responds to
    UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
    FGameplayTag GameplayCueTag;
    
    // Auto-destroy when removed
    UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
    bool bAutoDestroyOnRemove;
    
    // Lifecycle events
    virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    virtual bool OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
    virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters);
};
```

### Cue Parameters
```cpp
USTRUCT(BlueprintType)
struct FGameplayCueParameters
{
    GENERATED_BODY()
    
    // Normalized magnitude (0-1)
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    float NormalizedMagnitude;
    
    // Raw magnitude
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    float RawMagnitude;
    
    // Effect context
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FGameplayEffectContextHandle EffectContext;
    
    // Matched tag name
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FGameplayTag MatchedTagName;
    
    // Original tag
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FGameplayTag OriginalTag;
    
    // Aggregated source tags
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FGameplayTagContainer AggregatedSourceTags;
    
    // Aggregated target tags
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FGameplayTagContainer AggregatedTargetTags;
    
    // Location
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FVector_NetQuantize10 Location;
    
    // Normal
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    FVector_NetQuantizeNormal Normal;
    
    // Instigator
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    TWeakObjectPtr<AActor> Instigator;
    
    // Effect causer
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    TWeakObjectPtr<AActor> EffectCauser;
    
    // Source object
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    TWeakObjectPtr<UObject> SourceObject;
    
    // Physical material
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial;
    
    // Target attachment component
    UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
    TWeakObjectPtr<USceneComponent> TargetAttachComponent;
};
```

---

## KAIN Syntax Design

### Static Cue (Lightweight)
```kain
@gameplay_cue
struct BurnCue:
    tag: "GameplayCue.Effect.Burn"
    
    on_execute:
        spawn_particle("P_Burn_Impact", location)
        play_sound("S_Burn_Impact", location)
    
    on_add:
        spawn_particle_attached("P_Burn_Loop", target, "spine_01")
        play_sound_attached("S_Burn_Loop", target)
    
    on_remove:
        spawn_particle("P_Burn_End", location)
        play_sound("S_Burn_End", location)
```

### Actor Cue (Stateful)
```kain
@gameplay_cue(type: "Actor")
struct ShieldCue:
    tag: "GameplayCue.Effect.Shield"
    auto_destroy: true
    
    state shield_mesh: StaticMeshComponent
    state shield_material: MaterialInstanceDynamic
    
    on_add:
        shield_mesh = spawn_mesh("SM_Shield", target)
        shield_material = create_dynamic_material(shield_mesh, 0)
        shield_material.set_scalar_parameter("Opacity", 0.5)
    
    while_active(delta_time):
        let pulse = sin(get_world_time() * 2.0) * 0.5 + 0.5
        shield_material.set_scalar_parameter("Pulse", pulse)
    
    on_remove:
        spawn_particle("P_Shield_Break", shield_mesh.get_location())
        shield_mesh.destroy()
```

### Cue with Parameters
```kain
@gameplay_cue
struct DamageCue:
    tag: "GameplayCue.Damage"
    
    on_execute:
        let damage_amount = parameters.raw_magnitude
        let is_critical = parameters.aggregated_source_tags.has_tag("Damage.Critical")
        
        if is_critical:
            spawn_particle("P_Damage_Critical", location)
            play_sound("S_Damage_Critical", location)
            spawn_damage_number(damage_amount * 2.0, location, "Critical")
        else:
            spawn_particle("P_Damage_Normal", location)
            play_sound("S_Damage_Hit", location)
            spawn_damage_number(damage_amount, location, "Normal")
```

---

## Implementation Tasks

### Task 5.1: AST Structures

**File:** `Kain/crates/kain-core/src/ast.rs`

Add after `GameplayEffectDef`:
```rust
#[derive(Debug, Clone)]
pub struct GameplayCueDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub tag: String,
    pub cue_type: CueType,  // Static or Actor
    pub auto_destroy: bool,
    pub state_fields: Vec<StructField>,
    pub on_execute: Option<FunctionDef>,
    pub on_add: Option<FunctionDef>,
    pub on_remove: Option<FunctionDef>,
    pub while_active: Option<FunctionDef>,
    pub span: Span,
}

#[derive(Debug, Clone, PartialEq)]
pub enum CueType {
    Static,
    Actor,
}

// Add to Item enum
pub enum Item {
    // ... existing variants
    GameplayCue(GameplayCueDef),
}
```

**Estimated Time:** 0.5 hours

---

### Task 5.2: Parser Implementation

**File:** `Kain/crates/kain-core/src/parser.rs`

Add `parse_gameplay_cue()` function:
```rust
fn parse_gameplay_cue(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
    // Expect 'struct' keyword
    self.expect(TokenKind::Struct)?;
    
    // Parse cue name
    let name = self.parse_ident()?;
    
    // Expect ':'
    self.expect(TokenKind::Colon)?;
    
    // Parse cue body
    self.skip_newlines();
    self.expect(TokenKind::Indent)?;
    
    let mut tag: Option<String> = None;
    let mut cue_type = CueType::Static;
    let mut auto_destroy = false;
    let mut state_fields = Vec::new();
    let mut on_execute = None;
    let mut on_add = None;
    let mut on_remove = None;
    let mut while_active = None;
    
    while !self.check(TokenKind::Dedent) && !self.at_end() {
        self.skip_newlines();
        if self.check(TokenKind::Dedent) { break; }
        
        // Parse field or lifecycle method
        let field_name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        
        match field_name.as_str() {
            "tag" => {
                tag = Some(self.parse_string()?);
            }
            "type" => {
                let type_str = self.parse_string()?;
                cue_type = match type_str.as_str() {
                    "Static" => CueType::Static,
                    "Actor" => CueType::Actor,
                    _ => return Err(self.parser_error("Invalid cue type", self.current_span())),
                };
            }
            "auto_destroy" => {
                auto_destroy = self.parse_bool()?;
            }
            "on_execute" | "on_add" | "on_remove" | "while_active" => {
                // Parse lifecycle method
                let method = self.parse_function_body()?;
                match field_name.as_str() {
                    "on_execute" => on_execute = Some(method),
                    "on_add" => on_add = Some(method),
                    "on_remove" => on_remove = Some(method),
                    "while_active" => while_active = Some(method),
                    _ => {}
                }
            }
            "state" => {
                // Parse state field
                let field = self.parse_struct_field()?;
                state_fields.push(field);
            }
            _ => {
                return Err(self.parser_error(
                    format!("Unknown cue field: {}", field_name),
                    self.current_span()
                ));
            }
        }
        
        self.skip_newlines();
    }
    
    self.expect(TokenKind::Dedent)?;
    
    // Validate required fields
    let tag = tag.ok_or_else(|| {
        self.parser_error("Gameplay cue must have 'tag' field", start)
    })?;
    
    Ok(Item::GameplayCue(GameplayCueDef {
        name,
        attributes,
        tag,
        cue_type,
        auto_destroy,
        state_fields,
        on_execute,
        on_add,
        on_remove,
        while_active,
        span: start,
    }))
}
```

Add to `parse_item()`:
```rust
"gameplay_cue" => self.parse_gameplay_cue(attributes),
```

**Estimated Time:** 2 hours

---

### Task 5.3: IR Implementation

**File:** `Kain/crates/ue5-gas/src/cue_ir.rs` (new file)

```rust
use kain_core::ast::GameplayCueDef;
use kain_core::error::{KainError, KainResult};

#[derive(Debug, Clone)]
pub struct GameplayCueIR {
    pub name: String,
    pub tag: String,
    pub cue_type: CueTypeIR,
    pub auto_destroy: bool,
    pub state_fields: Vec<StateFieldIR>,
    pub on_execute: Option<String>,  // KAIN code as string
    pub on_add: Option<String>,
    pub on_remove: Option<String>,
    pub while_active: Option<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum CueTypeIR {
    Static,
    Actor,
}

#[derive(Debug, Clone)]
pub struct StateFieldIR {
    pub name: String,
    pub field_type: String,
}

impl GameplayCueIR {
    pub fn from_ast(cue: &GameplayCueDef) -> KainResult<Self> {
        // Validate @gameplay_cue attribute
        if !cue.attributes.iter().any(|a| a.name == "gameplay_cue") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @gameplay_cue attribute", cue.name),
                cue.span,
            ));
        }
        
        // Validate tag format (must start with "GameplayCue.")
        if !cue.tag.starts_with("GameplayCue.") {
            return Err(KainError::codegen(
                format!("Cue tag '{}' must start with 'GameplayCue.'", cue.tag),
                cue.span,
            ));
        }
        
        // Convert cue type
        let cue_type = match cue.cue_type {
            kain_core::ast::CueType::Static => CueTypeIR::Static,
            kain_core::ast::CueType::Actor => CueTypeIR::Actor,
        };
        
        // Convert state fields
        let state_fields = cue.state_fields.iter()
            .map(|f| StateFieldIR {
                name: f.name.clone(),
                field_type: format!("{:?}", f.field_type),  // TODO: proper type conversion
            })
            .collect();
        
        Ok(GameplayCueIR {
            name: cue.name.clone(),
            tag: cue.tag.clone(),
            cue_type,
            auto_destroy: cue.auto_destroy,
            state_fields,
            on_execute: cue.on_execute.as_ref().map(|_| "// TODO: codegen".to_string()),
            on_add: cue.on_add.as_ref().map(|_| "// TODO: codegen".to_string()),
            on_remove: cue.on_remove.as_ref().map(|_| "// TODO: codegen".to_string()),
            while_active: cue.while_active.as_ref().map(|_| "// TODO: codegen".to_string()),
        })
    }
}
```

**Estimated Time:** 1.5 hours

---

### Task 5.4: Codegen Implementation

**File:** `Kain/crates/ue5-gas/src/cue_codegen.rs` (new file)

See next file for full implementation...

**Estimated Time:** 3 hours

---

## Testing Strategy

### Unit Tests (20 tests)
- Tag validation (must start with "GameplayCue.")
- Cue type validation (Static vs Actor)
- State field parsing
- Lifecycle method parsing

### Integration Tests (15 tests)
- Static cue generation
- Actor cue generation
- Cue with all lifecycle methods
- Cue with state fields
- Cue with parameters

**Total Tests:** 35

---

## CLI Integration

### Extraction (ue5_pipeline.rs)
```rust
let gameplay_cues: Vec<kain_core::ast::GameplayCueDef> = merged.items.iter()
    .filter_map(|item| {
        if let kain_core::ast::Item::GameplayCue(def) = item {
            Some(def.clone())
        } else {
            None
        }
    })
    .collect();

merged.items.retain(|item| !matches!(item, 
    kain_core::ast::Item::GameplayCue(_)
));
```

### Generation Step
```rust
// STEP 3.11: Generate GameplayCues
#[cfg(feature = "ue5")]
if !gameplay_cues.is_empty() {
    println!("🎬 Generating {} GameplayCue(s)...", gameplay_cues.len());
    
    let cues_public_dir = layout.public_dir.join("Cues");
    let cues_private_dir = layout.private_dir.join("Cues");
    fs::create_dir_all(&cues_public_dir)?;
    fs::create_dir_all(&cues_private_dir)?;
    
    for cue_def in &gameplay_cues {
        match ue5_gas::cue_ir::GameplayCueIR::from_ast(cue_def) {
            Ok(cue_ir) => {
                match ue5_gas::cue_codegen::generate(&cue_ir, &ue5_config.plugin_name) {
                    Ok(output) => {
                        // Write files...
                    }
                }
            }
        }
    }
}
```

---

## Success Criteria

- ✅ 35 tests passing
- ✅ Static cues generate UGameplayCueNotify_Static
- ✅ Actor cues generate AGameplayCueNotify_Actor
- ✅ Lifecycle methods generate correctly
- ✅ Tag validation works
- ✅ CLI integration functional
- ✅ Compression ratio: 1:6 to 1:8

---

**Phase 5 Ready to Implement!**
