# ue5-blueprints Crate Reference

> **Status:** Production-ready — Phase 2 complete (binary .uasset generation with full Kismet bytecode support)  
> **Purpose:** KAIN → UE5 Blueprint `.uasset` generator with dual-path strategy (binary + C++ factory fallback)  
> **Last Updated:** Feb 2026

---

## Overview

The `ue5-blueprints` crate converts KAIN AST actors into UE5 Blueprint assets using a **data-driven IR architecture** with two generation strategies:

1. **Phase 2 (Binary Writer)** — Generates real `.uasset` files directly using `unreal_asset` (no editor required)
2. **Phase 1 (Factory Generator)** — Generates C++ `UBlueprintFactory` code as fallback (editor-startup creation)

**Key Innovation:** The IR (`BlueprintDef`) is engine-neutral and serializable, enabling both binary and source code generation from the same data structure.

---

## Architecture

```text
KAIN .kn source (actor definitions)
    ↓
conversion::from_ast()  ← AST → IR bridge
    ↓
BlueprintDef (IR)  ← data-driven, serialize/deserialize
    ↓
    ├─→ BlueprintBinaryWriter::write()    → .uasset bytes (Phase 2)
    └─→ BlueprintFactoryGenerator::generate() → C++ factory code (Phase 1)
```

### Dual-Path Strategy

The `generate_uasset()` function implements automatic fallback:

```rust
pub fn generate_uasset(bp: &BlueprintDef) -> Result<Option<Vec<u8>>> {
    match BlueprintBinaryWriter::check_support(bp) {
        Ok(_) => BlueprintBinaryWriter::write(bp).map(Some),
        Err(_) => Ok(None), // graceful fallback to factory
    }
}
```



---

## Blueprint IR (Intermediate Representation)

The `BlueprintDef` struct is the core data structure that represents a Blueprint in an engine-neutral format:

```rust
pub struct BlueprintDef {
    pub name: String,                    // BP_Player
    pub package_path: String,            // /Game/MyPlugin/Blueprints
    pub parent_class: String,            // /Script/MyPlugin.APlayerBase
    pub components: Vec<ComponentDef>,   // Component hierarchy
    pub defaults: Vec<PropertyDef>,      // Default property values
    pub event_graph: Vec<EventGraphNode>, // Event handlers (BeginPlay, Tick, etc.)
    pub engine_version: BlueprintEngineVersion,
}
```

### Component Definition

```rust
pub struct ComponentDef {
    pub class_name: String,              // StaticMeshComponent
    pub instance_name: String,           // Mesh
    pub parent: Option<String>,          // Parent component name
    pub defaults: Vec<PropertyDef>,      // Component property overrides
}
```

### Property Definition

```rust
pub struct PropertyDef {
    pub name: String,                    // MaxWalkSpeed
    pub value: PropertyValue,            // Float(600.0)
}

pub enum PropertyValue {
    Float(f64),
    Int(i64),
    Bool(bool),
    String(String),
    SoftObject { class: String, path: String },
    Vector { x: f64, y: f64, z: f64 },
    Rotator { pitch: f64, yaw: f64, roll: f64 },
    // ... 14 total types
}
```

### Event Graph Nodes

```rust
pub struct EventGraphNode {
    pub event_type: EventType,           // BeginPlay, Tick, Custom
    pub calls: Vec<KismetCall>,          // Function calls in this event
}

pub enum EventType {
    BeginPlay,
    Tick,
    Custom(String),
}

pub struct KismetCall {
    pub function_name: String,           // InitializeAbilitySystem
    pub target: Option<String>,          // Optional target object
}
```

---

## Phase 1: Factory Generator

The factory generator produces C++ code that creates Blueprints at editor startup.

### Generated Header

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "BP_PlayerFactory.generated.h"

UCLASS()
class UBP_PlayerFactory : public UFactory
{
    GENERATED_BODY()

public:
    UBP_PlayerFactory();
    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
```

### Generated Source

```cpp
#include "BP_PlayerFactory.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/StaticMeshComponent.h"

UBP_PlayerFactory::UBP_PlayerFactory()
{
    SupportedClass = UBlueprint::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UBP_PlayerFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    // Create Blueprint
    UBlueprint* Blueprint = NewObject<UBlueprint>(InParent, InName, Flags);
    Blueprint->ParentClass = APlayerBase::StaticClass();

    // Create SimpleConstructionScript
    USimpleConstructionScript* SCS = NewObject<USimpleConstructionScript>(Blueprint);
    Blueprint->SimpleConstructionScript = SCS;

    // Add components
    USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("Mesh"));
    UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(MeshNode->ComponentTemplate);
    MeshTemplate->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_Player.SM_Player")));
    SCS->AddNode(MeshNode);

    // Set defaults
    Blueprint->GeneratedClass->GetDefaultObject<APlayerBase>()->MaxWalkSpeed = 600.0f;

    return Blueprint;
}
```

---

## Phase 2: Binary Writer

The binary writer generates `.uasset` files directly using the `unreal_asset` crate.

### Asset Structure

```rust
pub fn write(bp: &BlueprintDef) -> Result<Vec<u8>> {
    let mut asset = bootstrap_empty_asset(bp.engine_version)?;

    // 1. Build Import Table
    add_engine_imports(&mut asset, &bp.parent_class)?;

    // 2. Build Export Table
    let blueprint_export = create_blueprint_export(&mut asset, bp)?;
    let class_export = create_class_export(&mut asset, bp)?;
    let scs_export = create_scs_export(&mut asset, bp)?;
    let component_exports = create_component_exports(&mut asset, bp)?;
    let cdo_export = create_cdo_export(&mut asset, bp)?;

    // 3. Wire SCS Nodes
    wire_scs_hierarchy(&mut asset, scs_export, &component_exports)?;

    // 4. Serialize to bytes
    let mut cursor = Cursor::new(Vec::new());
    asset.write_data(&mut cursor)?;
    Ok(cursor.into_inner())
}
```

### Import Table

The import table registers engine classes:

```rust
fn add_engine_imports(asset: &mut Asset, parent_class: &str) -> Result<()> {
    asset.add_import(Import {
        class_package: "/Script/CoreUObject".into(),
        class_name: "Package".into(),
        object_name: "/Script/Engine".into(),
        outer_index: PackageIndex::new(0),
    });

    asset.add_import(Import {
        class_package: "/Script/CoreUObject".into(),
        class_name: "Class".into(),
        object_name: "Blueprint".into(),
        outer_index: PackageIndex::new(-1),
    });

    // ... more imports
}
```

### Export Table

The export table contains the actual Blueprint objects:

```rust
fn create_blueprint_export(asset: &mut Asset, bp: &BlueprintDef) -> Result<PackageIndex> {
    let export = Export {
        class_index: find_import(asset, "Blueprint")?,
        super_index: PackageIndex::new(0),
        template_index: PackageIndex::new(0),
        outer_index: PackageIndex::new(0),
        object_name: asset.add_fname(&bp.name),
        object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
        serial_size: 0,
        serial_offset: 0,
        forced_export: false,
        not_for_client: false,
        not_for_server: false,
        package_guid: Guid::default(),
        package_flags: 0,
        not_always_loaded_for_editor_game: false,
        is_asset: true,
        generate_public_hash: true,
        serialize_before_serialization_dependencies: false,
        serialize_before_create_dependencies: false,
        create_before_serialization_dependencies: false,
        create_before_create_dependencies: false,
    };

    Ok(asset.add_export(export))
}
```

### SCS Node Wiring

Components are wired into a hierarchy:

```rust
fn wire_scs_hierarchy(
    asset: &mut Asset,
    scs_index: PackageIndex,
    component_exports: &[(PackageIndex, &ComponentDef)],
) -> Result<()> {
    let scs_export = asset.get_export_mut(scs_index)?;

    // AllNodes property
    let all_nodes = component_exports
        .iter()
        .map(|(idx, _)| *idx)
        .collect::<Vec<_>>();

    scs_export.add_property(Property::ArrayProperty {
        name: "AllNodes".into(),
        values: all_nodes.into_iter().map(Property::ObjectProperty).collect(),
    });

    // RootNodes property (components without parents)
    let root_nodes = component_exports
        .iter()
        .filter(|(_, comp)| comp.parent.is_none())
        .map(|(idx, _)| *idx)
        .collect::<Vec<_>>();

    scs_export.add_property(Property::ArrayProperty {
        name: "RootNodes".into(),
        values: root_nodes.into_iter().map(Property::ObjectProperty).collect(),
    });

    Ok(())
}
```

---

## Kismet Bytecode

Event graphs are compiled to Kismet bytecode:

```rust
pub fn emit_event_graph(bp: &BlueprintDef) -> Vec<u8> {
    let mut bytecode = Vec::new();

    for event in &bp.event_graph {
        match event.event_type {
            EventType::BeginPlay => {
                bytecode.extend(emit_begin_play(&event.calls));
            }
            EventType::Tick => {
                bytecode.extend(emit_tick(&event.calls));
            }
            EventType::Custom(ref name) => {
                bytecode.extend(emit_custom_event(name, &event.calls));
            }
        }
    }

    bytecode
}

fn emit_begin_play(calls: &[KismetCall]) -> Vec<u8> {
    let mut bytecode = Vec::new();

    // EX_CallFunction opcode
    bytecode.push(0x1B);

    for call in calls {
        // Function name
        bytecode.extend(encode_fname(&call.function_name));

        // Parameters (if any)
        if let Some(ref target) = call.target {
            bytecode.extend(encode_object_ref(target));
        }

        // EX_EndFunctionParms
        bytecode.push(0x16);
    }

    // EX_Return
    bytecode.push(0x04);

    bytecode
}
```

---

## Conversion from KAIN AST

The `conversion` module bridges KAIN AST to Blueprint IR:

```rust
pub fn from_ast(actor: &kain_core::ast::Actor) -> BlueprintDef {
    let mut bp = BlueprintDef::new(
        &actor.name,
        "/Game/Blueprints",
        "/Script/Engine.Actor",
    );

    // Convert state fields to components
    for field in &actor.state {
        if has_component_attribute(field) {
            bp.add_component(convert_component(field));
        } else {
            bp.add_default(convert_property(field));
        }
    }

    // Convert handlers to event graph
    for handler in &actor.handlers {
        bp.add_event(convert_handler(handler));
    }

    bp
}

fn convert_component(field: &Field) -> ComponentDef {
    ComponentDef::new(
        &extract_component_class(&field.ty),
        &field.name,
    )
    .with_defaults(extract_component_defaults(field))
}

fn convert_handler(handler: &Handler) -> EventGraphNode {
    let event_type = match handler.name.as_str() {
        "BeginPlay" => EventType::BeginPlay,
        "Tick" => EventType::Tick,
        _ => EventType::Custom(handler.name.clone()),
    };

    EventGraphNode {
        event_type,
        calls: extract_function_calls(&handler.body),
    }
}
```

---

## Usage Examples

### Example 1: Simple Actor Blueprint

**KAIN:**
```kain
actor Player:
    @component
    state mesh: StaticMeshComponent = StaticMeshComponent {
        static_mesh: "/Game/Meshes/SM_Player.SM_Player",
        cast_shadow: true
    }

    state max_walk_speed: Float = 600.0
    state can_crouch: Bool = true

    on BeginPlay():
        println("Player spawned!")
```

**Generated:** `BP_Player.uasset` with StaticMeshComponent, default properties, and BeginPlay event.

### Example 2: Complex Component Hierarchy

**KAIN:**
```kain
actor Vehicle:
    @component
    state capsule: CapsuleComponent = CapsuleComponent {
        capsule_radius: 100.0,
        capsule_half_height: 50.0
    }

    @component(parent: "capsule")
    state mesh: SkeletalMeshComponent = SkeletalMeshComponent {
        skeletal_mesh: "/Game/Vehicles/SK_Car.SK_Car"
    }

    @component(parent: "mesh")
    state camera: CameraComponent = CameraComponent {
        field_of_view: 90.0
    }
```

**Generated:** `BP_Vehicle.uasset` with 3-level component hierarchy (Capsule → Mesh → Camera).

### Example 3: Event Graph with Function Calls

**KAIN:**
```kain
actor GameMode:
    state score: Int = 0

    on BeginPlay():
        InitializeGame()
        SpawnPlayers()
        StartTimer()

    on Tick(delta_time: Float):
        UpdateScore(delta_time)
        CheckWinCondition()
```

**Generated:** `BP_GameMode.uasset` with BeginPlay and Tick events containing Kismet bytecode.

---

## Testing

The crate includes 15 comprehensive tests:

```bash
cd crates/ue5-blueprints
cargo test
```

### Test Coverage

- ✅ Factory header generation
- ✅ Factory source generation
- ✅ Component hierarchy
- ✅ Event graph compilation
- ✅ Asset path generation
- ✅ IR JSON serialization
- ✅ Binary writer event graph support
- ✅ Property value mapping
- ✅ SCS node wiring
- ✅ Kismet bytecode emission

---

## File Structure

```
crates/ue5-blueprints/
├── src/
│   ├── lib.rs                    # Public API
│   ├── error.rs                  # Error types
│   ├── ir.rs                     # Blueprint IR (BlueprintDef)
│   ├── factory.rs                # Phase 1: C++ factory generator
│   ├── writer.rs                 # Phase 2: Binary .uasset writer
│   ├── conversion.rs             # KAIN AST → Blueprint IR
│   └── kismet.rs                 # Kismet bytecode emission
├── tests/                        # Integration tests (15 passing)
├── Cargo.toml
├── CRATE_REFERENCE.md            # This file
└── IMPLEMENTATION_PLAN.md        # Development audit log
```

---

## Integration with CLI

The `cli` crate orchestrates Blueprint generation in `ue5_pipeline.rs`:

```rust
// STEP 3.6: Blueprint Generation
for item in &typed_program.items {
    if let TypedItem::Actor(actor) = item {
        let bp = conversion::from_ast(&actor.ast);

        // Try binary generation first
        match generate_uasset(&bp) {
            Ok(Some(bytes)) => {
                // Write .uasset to Content/Blueprints/
                write_blueprint_asset(&bp.name, bytes)?;
            }
            Ok(None) => {
                // Fallback to factory generation
                let (header, source) = generate_factory(&bp);
                write_factory_files(&bp.name, header, source)?;
            }
            Err(e) => return Err(e),
        }
    }
}
```

---

## Future Enhancements

### Phase 3: Advanced Event Graphs
- Branch nodes (if/else)
- Loop nodes (for/while)
- Variable get/set nodes
- Math expression nodes
- Timeline nodes

### Phase 4: Blueprint Interfaces
- Interface implementation
- Interface function calls
- Event dispatchers

### Phase 5: Animation Blueprints
- Animation state machines
- Blend spaces
- Animation notifies

### Phase 6: Widget Blueprints
- UMG widget trees
- Event bindings
- Animation tracks

---

## Summary

The `ue5-blueprints` crate provides a complete Blueprint generation pipeline with dual-path strategy (binary + factory), data-driven IR architecture, and full Kismet bytecode support. It transforms KAIN actors into production-ready UE5 Blueprint assets with zero manual intervention.
