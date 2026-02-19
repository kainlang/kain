# KAIN Codebase Directory Guide

> **For LLMs:** This guide helps you understand the KAIN compiler architecture and navigate the codebase efficiently. Read this first before making changes.

**Last Updated:** Feb 19, 2026  
**Status:** Production-ready — 99 tests passing, data-driven pipeline operational

---

## Quick Start (2-Minute Overview)

**What is KAIN?**  
A Python-like language that compiles to production-ready UE5 C++ plugins. One `.kn` file generates 30+ C++ files (~8000 lines) with actors, components, Slate UI, materials, and shaders.

**Key Value Proposition:**  
- 10-30x faster than manual C++ development
- Compiler-verified (zero typos, no memory leaks)
- Data-driven (loads 21K types, 2.3K widgets, 7.2K shader functions from UE5 source)
- LLM-friendly (if it compiles, it's production-ready)

**Build Command:**
```bash
cd YourPlugin/
kain build --ue5
```

**Install:**
```bash
cargo install --path crates/cli --force
```

---

## Repository Structure

```
kain/
├── crates/                          # Rust compiler (monorepo)
│   ├── kain-core/                   # Parser, AST, type checker
│   ├── ue5/                         # Runtime codegen (actors, components, RPCs)
│   ├── ue5-editor/                  # Editor codegen (Slate, Details, Viewports)
│   ├── ue5-materials/               # Material graph system
│   ├── ue5-shaders/                 # Shader codegen (HLSL .usf files)
│   └── cli/                         # CLI binary + packager
├── unreal/
│   ├── metadata/                    # JSON databases (21K types, 2.3K widgets, 7.2K functions)
│   └── scripts/                     # Python extractors (corpus, shader, module graph)
├── testing/                         # Test plugins
│   ├── Phase3/SlateTest4/           # ACTIVE: "Ulta" comprehensive test
│   ├── Phase4/                      # Shader tests
│   └── BestExample/                 # Feature reference
├── docs/                            # Documentation (you are here)
├── kn_library/                      # KAIN code examples and corpus
└── python/                          # Post-processors and validators
```


---

## Core Concepts

### 1. The Compilation Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│  .kn Source Files                                                │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  PARSER (kain-core)                                              │
│  - Lexer tokenizes source                                        │
│  - Parser builds AST (actors, structs, enums, shaders, etc.)     │
│  - Handles @attributes (@datatable, @component, @slate, etc.)    │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  TYPE CHECKER (kain-core)                                        │
│  - Resolves types                                                │
│  - Validates expressions                                         │
│  - Monomorphizes generics                                        │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  ORACLE VALIDATOR (ue5)                                          │
│  - Semantic validation (UE5-specific rules)                      │
│  - Naming collision detection                                    │
│  - RPC naming validation                                         │
│  - Component/Actor state validation                              │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  PACKAGER (cli)                                                  │
│  - Reads kain.toml                                               │
│  - Merges multiple .kn files                                     │
│  - Splits runtime vs editor items                                │
│  - Dispatches to codegen crates                                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┬─────────────────┐
         ▼               ▼               ▼                 ▼
┌─────────────┐  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
│ UE5 RUNTIME │  │ UE5 EDITOR  │  │ UE5 MATERIALS│  │ UE5 SHADERS  │
│  (ue5)      │  │(ue5-editor) │  │(ue5-materials│  │(ue5-shaders) │
│             │  │             │  │              │  │              │
│ Actors      │  │ Slate       │  │ Material     │  │ .usf files   │
│ Components  │  │ Details     │  │ Graphs       │  │ Shader       │
│ Structs     │  │ Viewports   │  │ Factory      │  │ Bindings     │
│ Enums       │  │ Toolbars    │  │ Code         │  │ Permutations │
│ Delegates   │  │ Asset Eds   │  │              │  │              │
│ Blueprint   │  │ Modules     │  │              │  │              │
└─────────────┘  └─────────────┘  └──────────────┘  └──────────────┘
         │               │               │                 │
         └───────────────┴───────────────┴─────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  OUTPUT FILES                                                    │
│  - Source/Plugin/*.h, *.cpp                                      │
│  - Source/PluginEditor/*.h, *.cpp (if editor items exist)        │
│  - Shaders/*.usf                                                 │
│  - Plugin.uplugin                                                │
│  - Source/Plugin/Plugin.Build.cs                                 │
│  - Content/Materials/ (created at Editor startup)                │
└─────────────────────────────────────────────────────────────────┘
```


### 2. Key Systems

#### Parser & AST (kain-core)
- **Location:** `crates/kain-core/src/`
- **Key Files:** `lexer.rs`, `parser.rs`, `ast.rs`
- **Purpose:** Converts `.kn` source into Abstract Syntax Tree
- **Handles:** Functions, structs, enums, actors, shaders, material graphs, attributes

#### Type System (kain-core)
- **Location:** `crates/kain-core/src/types.rs`
- **Purpose:** Type inference, checking, and monomorphization
- **Features:** Generics, type aliases, trait resolution

#### UE5 Runtime Codegen (ue5)
- **Location:** `crates/ue5/src/`
- **Key File:** `codegen_ue5.rs` (~3200 lines)
- **Generates:** Actors, Components, Structs, Enums, Delegates, Blueprint Functions
- **Features:** Automatic prefixing (A/F/E/U), RPC generation, replication, Blueprint integration

#### UE5 Editor Codegen (ue5-editor)
- **Location:** `crates/ue5-editor/src/editor/`
- **Key Files:** `slate.rs`, `details.rs`, `viewport.rs`, `codegen.rs`
- **Generates:** Slate widgets, Details panels, Viewports, Toolbars, Asset Editors, Editor Modules
- **Features:** Widget tree → SNew() chains, property customization, viewport clients

#### Material System (ue5-materials)
- **Location:** `crates/ue5-materials/src/`
- **Key Files:** `material_graph.rs`, `material_factory.rs`, `ast_converter.rs`
- **Generates:** C++ factory code that creates materials at Editor startup
- **Features:** 16+ node types, auto-wiring, material properties

#### Shader System (ue5-shaders)
- **Location:** `crates/ue5-shaders/src/`
- **Key Files:** `codegen_usf.rs`, `shader_knowledge.rs`
- **Generates:** .usf HLSL files, C++ shader parameter structs, IMPLEMENT_GLOBAL_SHADER
- **Features:** Fragment/Compute/Vertex shaders, permutations, type-safe bindings

#### Packager & Build System (cli)
- **Location:** `crates/cli/src/packager/`
- **Key Files:** `ue5_pipeline.rs`, `build.rs`, `codegen.rs`
- **Purpose:** Orchestrates compilation, file generation, module splitting
- **Features:** Auto two-module split (Runtime + Editor), .uplugin generation, Build.cs generation

#### Data-Driven Intelligence
- **Location:** `unreal/metadata/*.json` + `crates/ue5/src/ue5/*.rs`
- **Components:**
  - **EngineKnowledge** (21,134 types) - Type resolution, includes, modules
  - **WidgetRegistry** (2,346 widgets) - Slate delegate resolution
  - **ShaderKnowledge** (7,271 functions) - Intrinsic return types, includes
  - **ModuleGraph** (711 modules) - Build.cs dependency resolution
  - **VirtualObligations** (3,541 classes) - Pure virtual method stubs
  - **UhtRules** (337 rules) - UPROPERTY/UFUNCTION validation


---

## Common Tasks

### Adding New KAIN Syntax

**Example: Add `@networked` attribute for actors**

1. **Update AST** (`crates/kain-core/src/ast.rs`):
```rust
pub struct ActorDef {
    pub name: String,
    pub attributes: Vec<Attribute>,  // Add @networked here
    pub methods: Vec<FunctionDef>,
    // ...
}
```

2. **Update Parser** (`crates/kain-core/src/parser.rs`):
```rust
fn parse_actor(&mut self) -> Result<ActorDef> {
    let attributes = self.parse_attributes()?;  // Parses @networked
    // ...
}
```

3. **Update Codegen** (`crates/ue5/src/codegen_ue5.rs`):
```rust
fn gen_actor(&self, actor: &ActorDef) -> String {
    if actor.attributes.iter().any(|a| a.name == "networked") {
        // Generate GetLifetimeReplicatedProps()
    }
    // ...
}
```

4. **Add Tests** (`crates/ue5/tests/`):
```rust
#[test]
fn test_networked_actor() {
    let source = r#"
        @networked
        actor Player:
            state health: Float = 100.0
    "#;
    // Assert generated code has replication
}
```

### Adding New UE5 Features

**Example: Add support for UAnimInstance**

1. **Add to EngineKnowledge** (`unreal/metadata/engine_knowledge.json`):
```json
{
  "UAnimInstance": {
    "type": "class",
    "parent": "UObject",
    "header": "Animation/AnimInstance.h",
    "module": "Engine"
  }
}
```

2. **Update Type Mapping** (`crates/ue5/src/ue5/types.rs`):
```rust
pub fn map_type(&self, ty: &str) -> String {
    match ty {
        "AnimInstance" => "UAnimInstance*".to_string(),
        // ...
    }
}
```

3. **Add Codegen Support** (`crates/ue5/src/codegen_ue5.rs`):
```rust
// AnimInstance-specific generation logic
```

4. **Test** (`crates/ue5/tests/`):
```rust
#[test]
fn test_anim_instance_generation() {
    // Test code
}
```


### Extending the Material System

**Example: Add new material node type**

1. **Add Node Type** (`crates/ue5-materials/src/material_graph.rs`):
```rust
pub enum MaterialNode {
    // Existing nodes...
    Noise {
        id: usize,
        scale: f32,
        position: (i32, i32),
    },
}
```

2. **Add Builder Method** (`crates/ue5-materials/src/material_nodes.rs`):
```rust
impl MaterialNodeBuilder {
    pub fn noise(&mut self, scale: f32, x: i32, y: i32) -> String {
        let id = self.next_id();
        self.nodes.push(MaterialNode::Noise { id, scale, position: (x, y) });
        format!("node_{}", id)
    }
}
```

3. **Add Codegen** (`crates/ue5-materials/src/material_factory.rs`):
```rust
fn generate_node_code(&self, node: &MaterialNode) -> String {
    match node {
        MaterialNode::Noise { id, scale, position } => {
            format!(r#"
    UMaterialExpressionNoise* node_{} = NewObject<UMaterialExpressionNoise>(Material);
    node_{}->Scale = {}f;
    node_{}->MaterialExpressionEditorX = {};
    node_{}->MaterialExpressionEditorY = {};
    Material->GetExpressionCollection().AddExpression(node_{});
"#, id, id, scale, id, position.0, id, position.1, id)
        }
        // ...
    }
}
```

4. **Test** (`crates/ue5-materials/tests/`):
```rust
#[test]
fn test_noise_node_generation() {
    let mut builder = MaterialNodeBuilder::new();
    let noise = builder.noise(5.0, 0, 0);
    // Assert generated code
}
```

### Adding New Codegen Targets

**Example: Add WebGPU shader target**

1. **Create New Crate** (`crates/webgpu/`):
```bash
cargo new --lib crates/webgpu
```

2. **Add to Workspace** (`Cargo.toml`):
```toml
[workspace]
members = [
    "crates/kain-core",
    "crates/ue5",
    "crates/webgpu",  # New
    # ...
]
```

3. **Implement Codegen** (`crates/webgpu/src/lib.rs`):
```rust
use kain_core::ast::ShaderDef;

pub fn generate_wgsl(shader: &ShaderDef) -> String {
    // Convert KAIN shader to WGSL
}
```

4. **Wire into CLI** (`crates/cli/src/main.rs`):
```rust
match target {
    "ue5" => ue5::generate(...),
    "usf" => ue5_shaders::generate(...),
    "wgsl" => webgpu::generate(...),  // New
    // ...
}
```


---

## File Reference

### Files You'll Touch Most

| File | Lines | Purpose | When to Modify |
|------|-------|---------|----------------|
| `crates/ue5/src/codegen_ue5.rs` | ~3200 | Actor/struct/enum C++ generation | Runtime codegen bugs, new UE5 features |
| `crates/ue5/src/ue5/engine_knowledge.rs` | ~800 | Engine type database (21K types) | Adding new engine types, type resolution |
| `crates/ue5/src/ue5/widget_registry.rs` | ~400 | Widget database (2.3K widgets) | Widget delegate resolution, Slate bugs |
| `crates/ue5-editor/src/editor/slate.rs` | ~1200 | Slate widget tree → SNew() | UI generation bugs, new widget types |
| `crates/ue5-editor/src/editor/details.rs` | ~600 | Details panel generation | Property panel bugs, new property types |
| `crates/ue5-editor/src/editor/codegen.rs` | ~1500 | Editor orchestrator | Asset editor bugs, module generation |
| `crates/ue5-shaders/src/codegen_usf.rs` | ~1970 | USF shader generation | Shader codegen bugs, new shader features |
| `crates/ue5-shaders/src/shader_knowledge.rs` | ~500 | Shader database (7.2K functions) | Intrinsic handling, return type inference |
| `crates/cli/src/packager/ue5_pipeline.rs` | ~800 | Build orchestration | File output bugs, module splitting |
| `crates/kain-core/src/parser.rs` | ~2500 | KAIN syntax parser | New syntax, parsing bugs |
| `crates/kain-core/src/ast.rs` | ~1000 | AST definitions | New language features |
| `unreal/metadata/*.json` | Various | Metadata databases | Re-extract when engine updates |

### Files You'll Rarely Touch

| File | Purpose | When to Modify |
|------|---------|----------------|
| `crates/kain-core/src/lexer.rs` | Tokenization | New keywords, operators |
| `crates/kain-core/src/types.rs` | Type system | Type inference bugs |
| `crates/ue5/src/ue5/naming.rs` | UE5 prefix rules | Naming convention changes |
| `crates/ue5/src/ue5/oracle.rs` | Semantic validation | New validation rules |
| `crates/cli/src/packager/uplugin_gen.rs` | .uplugin generation | Plugin metadata changes |
| `crates/cli/src/packager/build_cs_gen.rs` | Build.cs generation | Module dependency changes |
| `python/post_process.py` | C++ cleanup | Formatting issues |

### Configuration Files

| File | Purpose |
|------|---------|
| `kain.toml` | Per-plugin config (plugin name, version, modules) |
| `Cargo.toml` | Rust workspace config |
| `unreal/metadata/*.json` | Engine metadata databases |

### Test Files

| Location | Purpose |
|----------|---------|
| `crates/ue5/tests/` | Runtime codegen tests (28 tests) |
| `crates/ue5-editor/tests/` | Editor codegen tests (10 tests) |
| `crates/ue5-shaders/tests/` | Shader codegen tests (18 tests) |
| `crates/kain-core/tests/` | Parser tests (3 tests) |
| `testing/Phase3/SlateTest4/` | Comprehensive integration test |
| `testing/Phase4/` | Shader-specific tests |


---

## Architecture Deep Dives

### Parser & AST System

**Flow:**
```
Source Code → Lexer → Tokens → Parser → AST
```

**Key AST Types:**
- `Program` - Top-level container
- `Item` - Top-level declarations (functions, structs, actors, etc.)
- `FunctionDef` - Function definitions
- `StructDef` - Struct definitions
- `ActorDef` - Actor definitions (UE5-specific)
- `ShaderDef` - Shader definitions
- `MaterialGraphDef` - Material graph definitions
- `Expr` - Expressions (binary ops, calls, literals, etc.)
- `Stmt` - Statements (let, var, return, if, match, etc.)

**Attributes:**
- `@datatable` - Makes struct inherit from FTableRowBase
- `@component` - Generates UActorComponent
- `@slate` - Generates SCompoundWidget
- `@details` - Generates IDetailCustomization
- `@viewport` - Generates SEditorViewport
- `@material_graph` - Generates material factory
- `@blueprint` - Makes function Blueprint-callable
- `@replicated` - Adds replication
- `@savegame` - Adds SaveGame flag

**Parser Entry Points:**
- `parse_program()` - Main entry
- `parse_item()` - Top-level items
- `parse_function()` - Functions
- `parse_struct()` - Structs
- `parse_actor()` - Actors
- `parse_shader()` - Shaders
- `parse_material_graph()` - Material graphs

### Type System

**Type Inference:**
- Bottom-up inference from literals and function signatures
- Unification algorithm for constraint solving
- Monomorphization for generics

**Type Representations:**
- `Type::Int`, `Type::Float`, `Type::Bool`, `Type::String`
- `Type::Vec2`, `Type::Vec3`, `Type::Vec4`
- `Type::Array(Box<Type>)`
- `Type::Map(Box<Type>, Box<Type>)`
- `Type::Option(Box<Type>)`
- `Type::Custom(String)` - User-defined types

**UE5 Type Mapping:**
```rust
// In crates/ue5/src/ue5/types.rs
pub fn map_type(&self, ty: &str) -> String {
    match ty {
        "Int" => "int64",
        "Float" => "float",
        "Bool" => "bool",
        "String" => "FString",
        "Vec2" => "FVector2D",
        "Vec3" => "FVector",
        "Vec4" => "FVector4",
        "Array" => "TArray",
        // ... + 21,134 engine types from EngineKnowledge
    }
}
```


### UE5 Runtime Codegen

**Main Generator:** `crates/ue5/src/codegen_ue5.rs`

**Key Functions:**
- `gen_actor_with_shaders()` - Generates AActor subclass with RPCs
- `gen_component()` - Generates UActorComponent subclass
- `gen_struct()` - Generates USTRUCT
- `gen_enum()` - Generates UENUM
- `gen_delegate()` - Generates DECLARE_DYNAMIC_MULTICAST_DELEGATE
- `gen_blueprint_library()` - Generates UBlueprintFunctionLibrary
- `gen_expr()` - Converts KAIN expressions to C++
- `map_type()` - Maps KAIN types to UE5 types

**RPC Generation:**
- `Server_*` → `UFUNCTION(Server, Reliable, WithValidation)`
- `Client_*` → `UFUNCTION(Client, Reliable)`
- `Multicast_*` → `UFUNCTION(NetMulticast, Reliable)`

**Automatic Prefixing:**
- Actors: `Player` → `APlayer`
- Structs: `Point` → `FPoint`
- Enums: `State` → `EState`
- Components: `Health` → `UHealthComponent`

**Replication:**
```rust
// Detects @replicated attribute
if field.attributes.contains("replicated") {
    // Add to GetLifetimeReplicatedProps()
    // Add UPROPERTY(Replicated)
}
```

### UE5 Editor Codegen

**Main Generator:** `crates/ue5-editor/src/editor/codegen.rs`

**Slate Widget Generation:**
```
KAIN Widget Tree → SNew() Chains
```

**Example:**
```kn
@slate
struct MyWidget:
    fn construct() -> Widget:
        return VBox(
            Text("Hello"),
            Button("Click", on_click)
        )
```

**Generates:**
```cpp
SNew(SVerticalBox)
+ SVerticalBox::Slot()
[
    SNew(STextBlock)
    .Text(FText::FromString(TEXT("Hello")))
]
+ SVerticalBox::Slot()
[
    SNew(SButton)
    .Text(FText::FromString(TEXT("Click")))
    .OnClicked_Lambda([this]() { return on_click(); })
]
```

**Details Panel Generation:**
```kn
@details
struct MyDetails:
    @slider(min: 0.0, max: 100.0)
    value: Float
```

**Generates:**
```cpp
IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("MyDetails"));
IDetailPropertyRow& Row = Category.AddProperty(GET_MEMBER_NAME_CHECKED(UMyClass, value));
Row.CustomWidget()
    .NameContent()[/* Label */]
    .ValueContent()[
        SNew(SSpinBox<float>)
        .MinValue(0.0f)
        .MaxValue(100.0f)
        .Value(/* Getter */)
        .OnValueChanged(/* Setter */)
    ];
```


### Material System

**Pipeline:**
```
KAIN @material_graph → MaterialGraph IR → C++ Factory Code → UE5 Materials
```

**Material Graph IR:**
```rust
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,      // Parameters
    pub nodes: Vec<MaterialNode>,        // Node graph
    pub outputs: MaterialOutputs,        // Base color, roughness, etc.
    pub properties: MaterialProperties,  // Blend mode, shading model
}
```

**Node Types:**
- Parameters: ScalarParameter, VectorParameter, ColorParameter, TextureParameter
- Math: Multiply, Add, Subtract, Divide, Power, Clamp
- Interpolation: Lerp, Fresnel
- Vector: Dot, ComponentMask, Append
- Texture: TextureSample, TextureCoordinate
- Constants: ConstantFloat, ConstantVec3, ConstantVec4

**Factory Generation:**
```cpp
void FMyPluginMaterialFactory::Generate_MyMaterial()
{
    // Create package
    UPackage* Package = CreatePackage(TEXT("/MyPlugin/Materials/M_MyMaterial"));
    
    // Create material
    UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_MyMaterial"));
    
    // Create nodes
    UMaterialExpressionScalarParameter* Roughness = NewObject<...>(Material);
    Roughness->ParameterName = TEXT("Roughness");
    Roughness->DefaultValue = 0.5f;
    
    // Wire connections
    Material->GetEditorOnlyData()->Roughness.Expression = Roughness;
    
    // Compile and save
    Material->PostEditChange();
    UPackage::SavePackage(Package, Material, ...);
}
```

**Called from Module Startup:**
```cpp
void FMyPluginModule::StartupModule()
{
    if (GIsEditor && !IsRunningCommandlet())
    {
        FMyPluginMaterialFactory::GenerateMaterials();
    }
}
```

### Shader System

**Pipeline:**
```
KAIN shader → USF Code + C++ Bindings → UE5 Shader System
```

**Shader Types:**
- `shader fragment` → Fragment shader (pixel shader)
- `shader compute` → Compute shader
- `shader vertex` → Vertex shader
- `shader surface` → Material surface shader

**Permutations:**
```kn
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform CFG_HIGH_QUALITY: Float @0  // Permutation
    uniform ENABLE_SHADOWS: Float @1    // Permutation
    uniform color: Vec3 @2              // Regular uniform
    
    if CFG_HIGH_QUALITY:
        // High quality path (compile-time branch)
    else:
        // Low quality path
```

**Generates 4 variants:**
- `CFG_HIGH_QUALITY=0, ENABLE_SHADOWS=0`
- `CFG_HIGH_QUALITY=0, ENABLE_SHADOWS=1`
- `CFG_HIGH_QUALITY=1, ENABLE_SHADOWS=0`
- `CFG_HIGH_QUALITY=1, ENABLE_SHADOWS=1`

**C++ Bindings:**
```cpp
class FMyShaderShader : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FMyShaderShader);
    SHADER_USE_PARAMETER_STRUCT(FMyShaderShader, FGlobalShader);
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FVector3f, color)
    END_SHADER_PARAMETER_STRUCT()
    
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};

IMPLEMENT_GLOBAL_SHADER(FMyShaderShader, "/Plugin/MyShader.usf", "MainPS", SF_Pixel);
```


### Data-Driven Intelligence System

**The Big Win:** Compiler loads metadata extracted from actual UE5 engine source code.

**Three Databases:**

#### 1. EngineKnowledge (21,134 types)
- **Source:** `unreal/metadata/engine_knowledge_expanded.json` (6.6 MB)
- **Extracted by:** `unreal/scripts/corpus_extractor.py`
- **Contains:** 9,896 classes + 8,323 structs + 2,915 enums
- **Provides:**
  - Type resolution: `StaticMeshComponent` → `UStaticMeshComponent*`
  - Include mapping: `UStaticMeshComponent` → `"Components/StaticMeshComponent.h"`
  - Module mapping: `UStaticMeshComponent` → `"Engine"`
  - Class hierarchy: Knows inheritance chains
  - Constructor validation: Arg counts and types

#### 2. WidgetRegistry (2,346 widgets)
- **Source:** `unreal/metadata/widget_registry.json` (1.2 MB)
- **Extracted by:** `unreal/scripts/corpus_extractor.py`
- **Contains:** 2,346 widgets + 470 delegates + 3,839 properties
- **Provides:**
  - Delegate resolution: `("SSlider", "OnValueChanged")` → `"FOnFloatValueChanged"`
  - Property types: Widget property → C++ type
  - Slot detection: Which widgets have default/multi slots
  - Header mapping: Widget → include path

#### 3. ShaderKnowledge (7,271 functions)
- **Source:** `unreal/metadata/shader_knowledge.json` (3.7 MB)
- **Extracted by:** `unreal/scripts/shader_extractor.py`
- **Contains:** 7,271 intrinsics + 612 permutations + 97 material getters
- **Provides:**
  - Return type inference: `dot()` → `"float"`, `lerp()` → `"passthrough"`
  - Include resolution: `CalcSceneDepth()` → `"SceneTexturesCommon.ush"`
  - Permutation validation: Checks against corpus
  - Thread group defaults: `(8, 8, 1)` based on Epic's usage

**Loading:**
```rust
// In Ue5Context::new()
let knowledge = EngineKnowledge::load_metadata(&metadata_dir)?;
let widget_registry = WidgetRegistry::load(&metadata_dir)?;
let shader_knowledge = ShaderKnowledge::load(&metadata_dir)?;

Ue5Context {
    knowledge,
    widget_registry,
    shader_knowledge,
    // ...
}
```

**Usage Example:**
```rust
// Type resolution
let cpp_type = ctx.knowledge.resolve_type("StaticMeshComponent");
// Returns: "UStaticMeshComponent*"

// Widget delegate
let delegate = ctx.widget_registry.get_event_delegate("SSlider", "OnValueChanged");
// Returns: Some("FOnFloatValueChanged")

// Shader intrinsic
let return_type = ctx.shader_knowledge.infer_return_type("dot");
// Returns: "float"
```

**Re-extraction:**
```bash
# When UE5 version changes
python unreal/scripts/corpus_extractor.py "M:\Utility\Unreal-Corpus" --output unreal/metadata
python unreal/scripts/shader_extractor.py "D:\Unreal\UE_5.7\Engine\Shaders" --output unreal/metadata
cargo build --release  # Compiler auto-loads new JSON
```


---

## Testing Patterns

### Unit Tests

**Location:** `crates/*/tests/`

**Pattern:**
```rust
#[test]
fn test_actor_generation() {
    let source = r#"
        actor Player:
            state health: Float = 100.0
            
            on Server_TakeDamage(amount: Float):
                health = health - amount
    "#;
    
    let ast = parse(source).unwrap();
    let cpp = generate_ue5(&ast).unwrap();
    
    assert!(cpp.contains("class APlayer : public AActor"));
    assert!(cpp.contains("UFUNCTION(Server, Reliable"));
    assert!(cpp.contains("void Server_TakeDamage"));
}
```

**Run Tests:**
```bash
# All tests
cargo test

# Specific crate
cargo test --package ue5
cargo test --package ue5-editor
cargo test --package ue5-shaders

# Specific test
cargo test --package ue5 test_actor_generation
```

### Integration Tests

**Location:** `testing/Phase3/SlateTest4/`

**Purpose:** Comprehensive system test with all features

**Contents:**
- `ultimate.kn` - 544-line self-validating dashboard plugin
- Exercises: 3 enums, 5 delegates, 2 structs, 1 shader, 1 actor, 5 Slate widgets, 1 details panel, 1 viewport, 1 toolbar, 1 asset editor, 1 editor module

**Build Test:**
```bash
cd testing/Phase3/SlateTest4
kain build --ue5
# Should generate 32 files with no errors
```

**Validation:**
```bash
# Check generated files exist
ls Source/Ulta/*.h
ls Source/Ulta/*.cpp
ls Source/UltaEditor/*.h
ls Source/UltaEditor/*.cpp
ls Shaders/*.usf

# Check for common issues
grep -r "TODO" Source/
grep -r "FIXME" Source/
```

### Shader Tests

**Location:** `testing/Phase4/`

**Pattern:**
```kn
shader fragment TestShader(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
```

**Build:**
```bash
cd testing/Phase4
kain build --ue5
```

**Verify:**
- `.usf` file generated
- C++ bindings generated
- `IMPLEMENT_GLOBAL_SHADER` present
- Shader parameters correct


---

## Debugging Tips

### Common Issues

#### 1. Parser Errors
**Symptom:** "Unexpected token" or "Expected X, found Y"

**Debug:**
```bash
# Enable parser debug output
RUST_LOG=kain_core=debug kain build --ue5
```

**Common Causes:**
- Missing colons after function/struct/actor names
- Incorrect indentation (KAIN is indent-sensitive)
- Missing return type on functions
- Unclosed parentheses/brackets

#### 2. Type Errors
**Symptom:** "Type mismatch" or "Cannot infer type"

**Debug:**
```bash
# Enable type checker debug output
RUST_LOG=kain_core::types=debug kain build --ue5
```

**Common Causes:**
- Missing type annotations on function parameters
- Incompatible types in binary operations
- Wrong return type

#### 3. Codegen Errors
**Symptom:** Generated C++ doesn't compile

**Debug:**
```bash
# Check generated files
cat Source/MyPlugin/Generated/*.cpp

# Look for common issues
grep "TODO" Source/MyPlugin/Generated/*.cpp
grep "FIXME" Source/MyPlugin/Generated/*.cpp
```

**Common Causes:**
- Missing includes (check EngineKnowledge)
- Wrong type mapping (check types.rs)
- Incorrect pointer usage (`.` vs `->`)
- Missing UE5 prefixes (A/F/E/U)

#### 4. Metadata Loading Errors
**Symptom:** "Failed to load metadata" or "Unknown type"

**Debug:**
```bash
# Check metadata files exist
ls unreal/metadata/*.json

# Validate JSON
python -m json.tool unreal/metadata/engine_knowledge.json > /dev/null
```

**Fix:**
```bash
# Re-extract metadata
python unreal/scripts/corpus_extractor.py "M:\Utility\Unreal-Corpus" --output unreal/metadata
```

### Debugging Workflow

1. **Enable Logging:**
```bash
export RUST_LOG=debug
kain build --ue5
```

2. **Check AST:**
```bash
# Add debug print in parser
println!("{:#?}", ast);
```

3. **Check Generated Code:**
```bash
# Look at generated files
cat Source/MyPlugin/Generated/*.h
cat Source/MyPlugin/Generated/*.cpp
```

4. **Run Tests:**
```bash
# Run specific test
cargo test --package ue5 test_actor_generation -- --nocapture
```

5. **Compare with Reference:**
```bash
# Compare with working example
diff Source/MyPlugin/Generated/MyActor.h testing/Phase3/SlateTest4/Source/Ulta/Generated/MyActor.h
```


---

## Performance Considerations

### Compilation Speed

**Current Performance:**
- Small plugin (< 500 lines): < 1 second
- Medium plugin (500-2000 lines): 1-3 seconds
- Large plugin (2000+ lines): 3-10 seconds

**Bottlenecks:**
1. **Metadata Loading** - 6.6 MB + 1.2 MB + 3.7 MB JSON parsing
   - **Optimization:** Cache parsed metadata in memory
   - **Future:** Embed metadata in binary via `include_bytes!()`

2. **Type Checking** - Monomorphization can be slow for heavy generic usage
   - **Optimization:** Memoize monomorphized types

3. **Codegen** - String concatenation for large files
   - **Optimization:** Use `String::with_capacity()` with size hints

### Runtime Performance

**Generated Code Quality:**
- Zero overhead abstractions (all KAIN constructs compile to native C++)
- No runtime type checking (all types resolved at compile time)
- Optimal UE5 patterns (uses engine best practices)

**Shader Performance:**
- Permutations compiled at shader compile time (zero runtime cost)
- Type-safe bindings prevent GPU stalls
- Optimal HLSL patterns (uses Epic's intrinsics)

### Memory Usage

**Compiler Memory:**
- Typical: 50-100 MB
- Large plugins: 200-300 MB
- Metadata: ~12 MB in memory

**Optimization Tips:**
- Use `Box<T>` for large AST nodes
- Use `Rc<T>` for shared data
- Use `String` interning for repeated strings

---

## LLM-Specific Guidance

### For LLMs Working on This Codebase

#### 1. Understanding the Architecture
- **Start here:** Read `docs/AGENT_HANDOFF.md` first
- **Then:** Read this file (DIRECTORY.md)
- **Finally:** Read specific system docs (`docs/DATA_DRIVEN_PIPELINE.md`, etc.)

#### 2. Making Changes
- **Always run tests** after changes: `cargo test`
- **Check generated output** in `testing/Phase3/SlateTest4/Source/`
- **Validate against UE5** if possible (compile in actual UE5 project)

#### 3. Common Patterns

**Adding New Syntax:**
```
1. Update AST (ast.rs)
2. Update Parser (parser.rs)
3. Update Codegen (codegen_ue5.rs or editor/codegen.rs)
4. Add Tests
5. Update Documentation
```

**Adding New UE5 Type:**
```
1. Add to engine_knowledge.json
2. Update type mapping (types.rs)
3. Add codegen support if needed
4. Add test
```

**Fixing Codegen Bug:**
```
1. Identify which crate (ue5, ue5-editor, ue5-shaders, ue5-materials)
2. Find the generator function
3. Fix the bug
4. Add regression test
5. Verify with integration test
```

#### 4. Token Efficiency Tips
- **Use tables** instead of prose for reference information
- **Use code examples** instead of explanations
- **Reference existing files** instead of duplicating content
- **Use ASCII diagrams** for architecture (more compact than text)

#### 5. Quality Checklist
- [ ] Tests pass (`cargo test`)
- [ ] Integration test builds (`cd testing/Phase3/SlateTest4 && kain build --ue5`)
- [ ] Generated C++ compiles (if UE5 available)
- [ ] Documentation updated
- [ ] No TODOs or FIXMEs in generated code


#### 6. Understanding the Data Flow

**Compilation Data Flow:**
```
User writes .kn file
    ↓
Lexer tokenizes (lexer.rs)
    ↓
Parser builds AST (parser.rs)
    ↓
Type checker resolves types (types.rs)
    ↓
Oracle validates semantics (oracle.rs)
    ↓
Packager orchestrates (packager/ue5_pipeline.rs)
    ↓
Codegen generates C++ (codegen_ue5.rs, editor/codegen.rs, etc.)
    ↓
Post-processor cleans up (python/post_process.py)
    ↓
Output files written to Source/
```

**Metadata Data Flow:**
```
UE5 Engine Source Code
    ↓
Python extractors (corpus_extractor.py, shader_extractor.py, etc.)
    ↓
JSON metadata files (unreal/metadata/*.json)
    ↓
Rust loaders (engine_knowledge.rs, widget_registry.rs, shader_knowledge.rs)
    ↓
Ue5Context (context.rs)
    ↓
Available to all codegen crates
```

#### 7. Key Invariants

**Must Always Be True:**
- All UE5 types have correct prefixes (A/F/E/U)
- All pointers use `->`, all values use `.`
- All UPROPERTY/UFUNCTION macros are correct
- All includes are present
- All module dependencies are in Build.cs
- All RPCs follow naming convention (Server_/Client_/Multicast_)
- All replicated properties have GetLifetimeReplicatedProps()
- All shaders have IMPLEMENT_GLOBAL_SHADER
- All materials have factory generation code

**Validation:**
- Oracle checks semantic rules
- Tests check generated code
- UE5 compilation is final validation

#### 8. When to Ask for Help

**Ask User When:**
- Unclear requirements (new syntax, new feature)
- UE5 version-specific behavior (API changes between versions)
- Marketplace requirements (specific plugin standards)
- Performance targets (optimization priorities)

**Don't Ask When:**
- Standard codegen patterns (follow existing examples)
- Bug fixes (fix and test)
- Documentation updates (just do it)
- Test additions (always add tests)

---

## Quick Reference

### Build Commands
```bash
# Build compiler
cargo build --release

# Install globally
cargo install --path crates/cli --force

# Build plugin
cd YourPlugin/
kain build --ue5

# Run tests
cargo test
cargo test --package ue5
cargo test --package ue5-editor
cargo test --package ue5-shaders
```

### File Patterns
```
Source/Plugin/                    # Runtime module
Source/PluginEditor/              # Editor module (if editor items exist)
Shaders/                          # .usf shader files
Content/Materials/                # Materials (created at Editor startup)
Plugin.uplugin                    # Plugin descriptor
Source/Plugin/Plugin.Build.cs     # Build configuration
```

### KAIN Syntax Quick Reference
```kn
// Struct
struct Point:
    x: Float
    y: Float

// Enum
enum State:
    Idle
    Running

// Actor
actor Player:
    state health: Float = 100.0
    on Server_TakeDamage(amount: Float):
        health = health - amount

// Component
@component
struct HealthComponent:
    @replicated
    current: Float

// Blueprint function
@blueprint
fn calculate_damage(base: Float) -> Float:
    return base * 2.0

// Shader
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)

// Material
@material_graph
material MyMaterial:
    input roughness: Float = 0.5
    output base_color = vec3(1, 1, 1)
    output roughness = roughness
```

### Type Mappings
| KAIN | UE5 |
|------|-----|
| `Int` | `int64` |
| `Float` | `float` |
| `Bool` | `bool` |
| `String` | `FString` |
| `Vec2` | `FVector2D` |
| `Vec3` | `FVector` |
| `Vec4` | `FVector4` |
| `Array<T>` | `TArray<T>` |

### Attribute Reference
| Attribute | Effect |
|-----------|--------|
| `@datatable` | Struct inherits FTableRowBase |
| `@component` | Generates UActorComponent |
| `@slate` | Generates SCompoundWidget |
| `@details` | Generates IDetailCustomization |
| `@viewport` | Generates SEditorViewport |
| `@material_graph` | Generates material factory |
| `@blueprint` | Makes function Blueprint-callable |
| `@replicated` | Adds replication |
| `@savegame` | Adds SaveGame flag |

---

## Additional Resources

### Documentation
- `docs/AGENT_HANDOFF.md` - Start here for new agents
- `docs/DATA_DRIVEN_PIPELINE.md` - Metadata system deep dive
- `docs/MATERIAL_SYSTEM_PHASE2_COMPLETE.md` - Material system overview
- `docs/PARSER_AST_GUIDE.md` - Parser and AST details
- `docs/UE5_GODMODE_GUIDE.md` - LLM agent guide

### Examples
- `testing/Phase3/SlateTest4/ultimate.kn` - Comprehensive example
- `testing/BestExample/ULTIMATE_DEMO.kn` - Feature showcase
- `kn_library/` - KAIN code corpus

### Tools
- `unreal/scripts/corpus_extractor.py` - Extract engine metadata
- `unreal/scripts/shader_extractor.py` - Extract shader metadata
- `python/post_process.py` - C++ cleanup
- `python/ue5_validator.py` - Validate generated code

---

**Last Updated:** Feb 19, 2026  
**Maintainer:** KAIN Development Team  
**Status:** Production-ready, actively maintained

