# Additional Systems Features

**Category**: Compiler Infrastructure / Quality Assurance  
**Status**: Implemented and Production-Validated

## Overview

KAIN's additional systems provide the infrastructure for data-driven validation, metadata management, post-processing, extension support, multi-module plugins, and binary asset generation. These systems ensure code quality, extensibility, and production-readiness.

---

## Feature 1: Data-Driven Validation (Oracle System)

### Description
The Oracle system provides compile-time validation of KAIN code using data-driven rules defined in `validation_rules.json`. Rules can be added/modified without recompiling the compiler.

### Built-in Validation Rules

#### Replication Validation
- Validates `@replicated` fields have correct types
- Ensures `GetLifetimeReplicatedProps()` is generated
- Checks for `DOREPLIFETIME` macros

#### RPC Naming Validation
- Validates `Server_*` functions have `_Validate()` methods
- Ensures `Client_*` functions are marked `UFUNCTION(Client, Reliable)`
- Checks `Multicast_*` functions are marked `UFUNCTION(NetMulticast, Reliable)`

#### DataTable Field Validation
- Validates `@datatable` structs inherit from `FTableRowBase`
- Ensures all fields are CSV-compatible types
- Checks for required `id` field

#### Component Validation
- Validates `@component` structs inherit from `UActorComponent`
- Ensures `SetIsReplicatedByDefault(true)` for replicated components
- Checks for lifecycle methods (`BeginPlay`, `TickComponent`)

#### Name Collision Detection
- Detects collisions with engine types (UObject, AActor, etc.)
- Detects collisions with C++ keywords (class, struct, enum, etc.)
- Detects collisions with UE5 macros (UCLASS, UPROPERTY, etc.)

#### Circular Dependency Detection
- Detects circular dependencies between modules
- Detects circular dependencies between actors/components
- Ensures dependency graph is acyclic

#### Blueprint Event Rules
- Validates `@blueprint_event` functions have correct signatures
- Ensures `_Implementation()` methods are generated
- Checks for Blueprint-compatible parameter types

### Custom Validation Rules (validation_rules.json)

#### Rule Categories (7 types)
1. **Naming**: Enforce naming conventions
2. **TypeCompatibility**: Ensure type compatibility
3. **AttributeCombination**: Validate attribute combinations
4. **Replication**: Validate replication setup
5. **Blueprint**: Validate Blueprint integration
6. **Shader**: Validate shader code
7. **Editor**: Validate editor UI code

#### Condition Types (7 types)
1. **TypeCollision**: Detect type name collisions
2. **IncompatibleAttributes**: Detect incompatible attribute combinations
3. **InvalidRpcNaming**: Detect invalid RPC naming
4. **NestedContainer**: Detect nested containers (Array<Array<T>>)
5. **InvalidNaming**: Detect invalid naming conventions
6. **MissingAttribute**: Detect missing required attributes
7. **ForbiddenType**: Detect forbidden types in specific contexts

### validation_rules.json Schema
```json
{
  "rules": [
    {
      "id": "no_nested_arrays",
      "category": "TypeCompatibility",
      "condition": "NestedContainer",
      "message": "Nested arrays are not supported in UE5",
      "suggestion": "Use a struct with an array field instead",
      "severity": "Error",
      "enabled": true
    },
    {
      "id": "rpc_naming_convention",
      "category": "Replication",
      "condition": "InvalidRpcNaming",
      "message": "RPC functions must start with Server_, Client_, or Multicast_",
      "suggestion": "Rename function to follow RPC naming convention",
      "severity": "Error",
      "enabled": true
    }
  ]
}
```

### Usage Example
```kain
# Invalid: Nested array
actor Player:
    state inventory: Array<Array<ItemStack>>  # ERROR: Nested arrays not supported

# Valid: Struct with array
struct Inventory:
    slots: Array<ItemStack>

actor Player:
    state inventory: Inventory  # OK
```

### Generated Validation Error
```
Error: Nested arrays are not supported in UE5
  --> player.kn:2:5
   |
2  |     state inventory: Array<Array<ItemStack>>
   |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
Suggestion: Use a struct with an array field instead
```

### Features
- **No Recompilation**: Add/modify rules without recompiling compiler
- **Custom Messages**: Define custom error messages and suggestions
- **Severity Levels**: Error, Warning, Info
- **Conflict Detection**: Detect conflicting rules
- **Rule Disabling**: Disable rules without removing them

### Factory Part 1 Examples
- **All Plugins**: Validated by Oracle system
- **VoxelForgePro**: RPC naming validation, replication validation
- **NarrativeGraph**: Component validation, circular dependency detection

---

## Feature 2: Metadata-First Architecture

### Description
14 JSON metadata files drive the compiler, providing a queryable database of UE5 engine types, widgets, shaders, modules, and validation rules.

### Metadata Files (14 total)

#### 1. engine_knowledge.json (10MB, 500+ types)
```json
{
  "classes": [
    {
      "name": "AActor",
      "base": "UObject",
      "module": "Engine",
      "header": "GameFramework/Actor.h",
      "properties": [...],
      "functions": [...]
    }
  ],
  "structs": [...],
  "enums": [...]
}
```

**Purpose**: UE5 type database (UCLASS, USTRUCT, UENUM)

#### 2. widget_registry.json (1.2MB)
```json
{
  "widgets": [
    {
      "name": "SButton",
      "base": "SCompoundWidget",
      "slots": ["Content"],
      "arguments": ["OnClicked", "Text", "ToolTipText"]
    }
  ]
}
```

**Purpose**: All Slate widget types with slot info

#### 3. shader_knowledge.json (500KB)
```json
{
  "types": ["float", "float2", "float3", "float4", "Texture2D", "SamplerState"],
  "builtins": ["dot", "cross", "normalize", "length", "distance"],
  "semantics": ["SV_Position", "SV_Target", "SV_DispatchThreadID"]
}
```

**Purpose**: HLSL types, built-ins, semantics

#### 4. module_graph.json (1.4MB)
```json
{
  "modules": [
    {
      "name": "Engine",
      "dependencies": ["Core", "CoreUObject"],
      "type": "Runtime"
    }
  ]
}
```

**Purpose**: Full transitive dependency resolution

#### 5. validation_rules.json (100KB)
**Purpose**: Custom validation rules (see Feature 1)

#### 6. virtual_obligations.json (4.3MB)
```json
{
  "classes": [
    {
      "name": "AActor",
      "virtual_methods": [
        {
          "name": "BeginPlay",
          "signature": "virtual void BeginPlay()",
          "required": false
        }
      ]
    }
  ]
}
```

**Purpose**: Virtual method override requirements

#### 7. uht_rules.json (50KB)
```json
{
  "uclass_specifiers": ["Blueprintable", "Abstract", "NotBlueprintable"],
  "uproperty_specifiers": ["EditAnywhere", "BlueprintReadWrite", "Replicated"],
  "ufunction_specifiers": ["BlueprintCallable", "Server", "Client", "NetMulticast"]
}
```

**Purpose**: Unreal Header Tool rules

#### 8-14. Additional Metadata Files
- `blueprint_node_types.json` - Blueprint node types
- `animation_types.json` - Animation types
- `physics_types.json` - Physics types
- `ai_types.json` - AI types
- `audio_types.json` - Audio types
- `rendering_types.json` - Rendering types
- `networking_types.json` - Networking types

### Features
- **Multi-UE5-Version Support**: 5.4-5.7 metadata
- **Multi-Drive Installation**: Auto-detect UE5 installation
- **Schema Validation**: Validate metadata on load
- **Hot-Reload**: Reload metadata without restarting compiler

### Usage Example
```kain
# Compiler queries engine_knowledge.json
actor Player:  # Compiler knows AActor exists, inherits from UObject
    state health: Float  # Compiler knows Float is valid type
```

### Factory Part 1 Examples
- **All Plugins**: Use metadata for type validation
- **Compiler**: `Kain/unreal/metadata/` directory

---

## Feature 3: Post-Processing Pipeline

### Description
Five post-processing fixes ensure generated C++ code is production-ready with correct replication, shader initialization, forward declarations, include ordering, and formatting.

### Fix 1: ReplicationFix

#### Purpose
Injects `GetLifetimeReplicatedProps()` and `DOREPLIFETIME` macros for replicated actors/components.

#### Input (Generated C++)
```cpp
UCLASS()
class APlayer : public AActor {
    GENERATED_BODY()
    
    UPROPERTY(Replicated)
    float Health;
};
```

#### Output (After ReplicationFix)
```cpp
UCLASS()
class APlayer : public AActor {
    GENERATED_BODY()
    
    UPROPERTY(Replicated)
    float Health;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

// In .cpp
void APlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APlayer, Health);
}
```

### Fix 2: ShaderInitFix

#### Purpose
Injects shader initialization in `BeginPlay()` for actors with shaders.

#### Input (Generated C++)
```cpp
UCLASS()
class AVoxelGenerator : public AActor {
    GENERATED_BODY()
    
    UTextureRenderTarget2D* PositionRT_A;
    UTextureRenderTarget2D* PositionRT_B;
};
```

#### Output (After ShaderInitFix)
```cpp
UCLASS()
class AVoxelGenerator : public AActor {
    GENERATED_BODY()
    
    UTextureRenderTarget2D* PositionRT_A;
    UTextureRenderTarget2D* PositionRT_B;
    
    virtual void BeginPlay() override;
};

// In .cpp
void AVoxelGenerator::BeginPlay() {
    Super::BeginPlay();
    
    // Initialize render targets
    PositionRT_A = NewObject<UTextureRenderTarget2D>();
    PositionRT_A->InitAutoFormat(512, 512);
    
    PositionRT_B = NewObject<UTextureRenderTarget2D>();
    PositionRT_B->InitAutoFormat(512, 512);
}
```

### Fix 3: ForwardDeclFix

#### Purpose
Injects missing forward declarations in correct order.

#### Input (Generated C++)
```cpp
#include "Player.h"

UCLASS()
class AGameMode : public AGameModeBase {
    GENERATED_BODY()
    
    APlayer* CurrentPlayer;  // ERROR: APlayer not declared
};
```

#### Output (After ForwardDeclFix)
```cpp
#include "Player.h"

class APlayer;  // Forward declaration

UCLASS()
class AGameMode : public AGameModeBase {
    GENERATED_BODY()
    
    APlayer* CurrentPlayer;  // OK
};
```

### Fix 4: IncludeOrderFix

#### Purpose
Ensures correct include ordering: CoreMinimal → Engine → Project.

#### Input (Generated C++)
```cpp
#include "Player.h"
#include "Engine/World.h"
#include "CoreMinimal.h"  // WRONG ORDER
```

#### Output (After IncludeOrderFix)
```cpp
#include "CoreMinimal.h"  // Core first
#include "Engine/World.h"  // Engine second
#include "Player.h"  // Project third
```

### Fix 5: FormattingFix

#### Purpose
Ensures consistent formatting: tabs, single blank lines, LF line endings.

#### Input (Generated C++)
```cpp
UCLASS()
class APlayer : public AActor {


    GENERATED_BODY()
    
        float Health;
};
```

#### Output (After FormattingFix)
```cpp
UCLASS()
class APlayer : public AActor {
	GENERATED_BODY()
	
	float Health;
};
```

### Factory Part 1 Examples
- **All Plugins**: Post-processing applied to all generated C++
- **VoxelForgePro**: ReplicationFix, ShaderInitFix
- **NarrativeGraph**: ForwardDeclFix, IncludeOrderFix

---

## Feature 4: Extension System

### Description
The extension system allows adding support for third-party UE5 plugins (MetaHuman, Niagara, PCG) without modifying core compiler code.

### How It Works

#### Step 1: Create Extension JSON
```bash
python Kain/unreal/scripts/extension_scanner.py <plugin_path> --name <name>
```

#### Step 2: Extension Auto-Loads
When you run `kain build --ue5`, the compiler:
1. Loads `engine_knowledge.json` (core UE5 types)
2. Loads all `extensions/*.json` files
3. Merges everything into `EngineKnowledge`

### Available Extensions

#### 1. metahuman.json (256 classes, 176 structs, 99 enums)
```json
{
  "classes": [
    {
      "name": "UMetaHumanComponent",
      "base": "UActorComponent",
      "module": "MetaHuman"
    }
  ]
}
```

**Purpose**: MetaHuman plugin support

#### 2. niagara.json
```json
{
  "classes": [
    {
      "name": "UNiagaraComponent",
      "base": "UFXSystemComponent",
      "module": "Niagara"
    }
  ]
}
```

**Purpose**: Niagara VFX plugin support

#### 3. pcg.json
```json
{
  "classes": [
    {
      "name": "UPCGComponent",
      "base": "UActorComponent",
      "module": "PCG"
    }
  ]
}
```

**Purpose**: Procedural Content Generation plugin support

### Usage Example
```kain
# Use MetaHuman types (auto-loaded from metahuman.json)
@component
struct MetaHumanController:
    metahuman: UMetaHumanComponent
    
    fn update_expression(expression: String):
        metahuman.SetExpression(expression)
```

### Features
- **Zero Core Modifications**: No need to edit `engine_knowledge.json`
- **No Rust Code Changes**: Just drop a JSON file in `extensions/`
- **Auto-Discovery**: Extensions auto-loaded on compilation
- **Custom Extensions**: Create your own with `extension_scanner.py`

### Factory Part 1 Examples
- **MetaHuman Integration**: `Kain/unreal/metadata/extensions/metahuman.json`
- **Niagara Integration**: `Kain/unreal/metadata/extensions/niagara.json`
- **PCG Integration**: `Kain/unreal/metadata/extensions/pcg.json`

---

## Feature 5: Multi-Module Plugin System

### Description
Data-driven multi-module plugin system allows creating plugins with separate Runtime, Editor, Developer, and UncookedOnly modules.

### KAIN.toml Configuration
```toml
[package]
name = "MyPlugin"
version = "1.0.0"

[ue5]
plugin_name = "MyPlugin"
engine_version = "5.4"

[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"
source_globs = ["src/runtime/**"]
loading_phase = "Default"

[[ue5.modules]]
name = "MyPluginEditor"
type = "Editor"
depends_on = ["MyPlugin"]
source_globs = ["src/editor/**"]
loading_phase = "PostEngineInit"

[[ue5.modules]]
name = "MyPluginDeveloper"
type = "Developer"
depends_on = ["MyPlugin"]
source_globs = ["src/developer/**"]
```

### Module Types
- **Runtime**: Loaded in game and editor
- **Editor**: Loaded only in editor
- **Developer**: Loaded only in development builds
- **UncookedOnly**: Loaded only with uncooked content

### Validation
- **Duplicate Detection**: Detect duplicate module names
- **Unknown Dependencies**: Detect dependencies on non-existent modules
- **Circular Dependencies**: Detect circular module dependencies

### Generated .uplugin
```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "MyPlugin",
  "Modules": [
    {
      "Name": "MyPlugin",
      "Type": "Runtime",
      "LoadingPhase": "Default"
    },
    {
      "Name": "MyPluginEditor",
      "Type": "Editor",
      "LoadingPhase": "PostEngineInit"
    }
  ]
}
```

### Generated Build.cs (per module)
```csharp
// MyPlugin.Build.cs
public class MyPlugin : ModuleRules {
    public MyPlugin(ReadOnlyTargetRules Target) : base(Target) {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine"
        });
    }
}

// MyPluginEditor.Build.cs
public class MyPluginEditor : ModuleRules {
    public MyPluginEditor(ReadOnlyTargetRules Target) : base(Target) {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "MyPlugin"  // Depends on Runtime module
        });
    }
}
```

### Features
- **Data-Driven**: Modules defined in KAIN.toml
- **Validation**: Duplicate/unknown/circular dependency detection
- **Auto .uplugin**: Automatic .uplugin generation
- **Per-Module Build.cs**: Separate Build.cs for each module
- **Back-Compatible**: Works with legacy single/split mode

### Factory Part 1 Examples
- **NarrativeGraph**: Multi-module (Runtime + Editor)
- **TitanGraph**: Multi-module (Runtime + Editor)
- **VoxelForgePro**: Single module (Runtime only)

---

## Feature 6: Binary Asset Pipeline

### Description
Direct binary .uasset generation for materials, blueprints, and data assets without UE5 editor.

### Material .uasset Generation

#### MaterialAssetBuilder
- **30+ node types**: Texture sampling, math ops, UV manipulation
- **Direct binary serialization**: No UE5 editor required
- **Engine version parameterization**: UE 5.0→5.4+

#### Example
```kain
material PBRGround:
    input albedo: Texture2D
    input roughness_value: Float = 0.5
    base_color = texture_sample(albedo).rgb
    roughness = roughness_value
```

**Generated**: `Content/Materials/PBRGround.uasset` (binary file)

### Blueprint .uasset Generation

#### BlueprintBinaryWriter
- **14 property types**: Int, Float, Bool, String, Object, Struct, Enum, Array, etc.
- **Kismet bytecode**: Full bytecode generation for event graphs
- **UK2Node support**: Custom blueprint nodes

#### Example
```kain
@blueprint
fn calculate_damage(base: Float, multiplier: Float) -> Float:
    return base * multiplier
```

**Generated**: `Content/Blueprints/CalculateDamage.uasset` (binary file)

### UDataAsset Writer

#### Features
- **Engine version parameterization**: UE 5.0→5.4+
- **26 tests**: Comprehensive test coverage
- **Schema validation**: Validate asset structure

### Asset Registry Writer

#### Features
- **AddedDependencyFlags format**: UE 4.27/5.0+
- **6 tests**: Comprehensive test coverage
- **Dependency tracking**: Track asset dependencies

### Factory Part 1 Examples
- **All Material Plugins**: Binary .uasset generation
- **Blueprint Plugins**: Binary .uasset generation
- **Data Asset Plugins**: UDataAsset writer

---

## Summary

KAIN's additional systems provide the infrastructure for production-ready plugin development with data-driven validation, metadata management, post-processing, extension support, multi-module plugins, and binary asset generation.

**Key Capabilities**:
1. **Oracle System**: Data-driven validation with custom rules
2. **Metadata Architecture**: 14 JSON files drive the compiler
3. **Post-Processing**: 5 fixes ensure production-ready C++
4. **Extension System**: Add third-party plugin support without core changes
5. **Multi-Module Plugins**: Data-driven module system with validation
6. **Binary Assets**: Direct .uasset generation without UE5 editor

**Proven Results**:
- 50+ plugins validated by Oracle system
- 14 metadata files (16.5MB total)
- 5 post-processing fixes applied to all generated C++
- 3 extensions (MetaHuman, Niagara, PCG)
- 10+ multi-module plugins
- 100+ binary assets generated

**Factory Part 1 Examples**:
- **All Plugins**: Oracle validation, metadata, post-processing
- **NarrativeGraph**: Multi-module plugin
- **VoxelForgePro**: Binary material assets
- **Extension System**: MetaHuman, Niagara, PCG

---

**Total Features Documented**: 6  
**Factory Part 1 Examples**: 10+ (All plugins use these systems)  
**Metadata Files**: 14 (16.5MB total)  
**Post-Processing Fixes**: 5  
**Extensions**: 3 (MetaHuman, Niagara, PCG)
