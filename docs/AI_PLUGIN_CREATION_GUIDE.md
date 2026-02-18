# KAIN Plugin Creation Guide - AI Edition
> Ultra-dense reference for LLM agents creating UE5 plugins with KAIN

## WHAT IS KAIN?

Python-like language → UE5 C++ plugin compiler. One `.kn` file = complete production plugin.

**Value prop:** 500 lines KAIN → 8000+ lines UE5 C++ (20+ files) that compiles first try.

**Binary:** `kain build --ue5` in plugin folder with `kain.toml`

---

## METADATA SYSTEM - THE BRAIN

### Engine Knowledge Database (94KB → 11GB scanned data)

Located: `unreal/metadata/engine_knowledge.json` + 7 version-specific scans

**What it contains:**
- 15,000+ UE5 classes with inheritance chains
- 50,000+ functions with signatures
- 10,000+ properties with specifiers
- Type aliases (Vec3→FVector, Actor→AActor)
- Include paths (UStaticMeshComponent→Components/StaticMeshComponent.h)
- Named colors ("sunset"→FLinearColor(1.0, 0.7, 0.3, 1.0))
- Constructor formats (vec3(x,y,z)→FVector(x,y,z))
- Property string formats (ImportText/ExportText)

**Queried at compile time for:**
- Type resolution: `StaticMeshComponent` → `UStaticMeshComponent*` with correct include
- Pointer semantics: UObject-derived types auto-get `*` suffix
- Name collision detection: Prevents `EHealthStatus` becoming `EEHealthStatus`
- Function templates: `GetActorLocation()` → `$0->GetActorLocation()`
- Module dependencies: Auto-adds to `.Build.cs`

**Additional metadata files:**
- `shader_knowledge.json` (3.7MB) - HLSL intrinsics, permutations, includes
- `uht_rules.json` (361KB) - UHT validation (UPROPERTY/UFUNCTION rules)
- `virtual_obligations.json` (4.3MB) - Virtual function overrides required
- `widget_registry.json` (1.2MB) - Slate widget hierarchy + slot types
- `editor_attributes.json` (10KB) - Details panel property decorators
- `codegen_rules.json` (15KB) - C++ generation patterns
- `module_graph.json` (1.7MB) - UE5 module dependency graph

**Data-driven = zero hardcoding.** All type mappings, includes, and conventions come from JSON.

---

## ATTRIBUTE SYSTEM - THE DECORATORS

### Runtime Attributes (Actors/Components/Structs)

```kain
@datatable              # Struct inherits FTableRowBase (CSV import)
@component              # Generates UActorComponent subclass
@uclass("Blueprintable", "BlueprintType")  # UCLASS specifiers
@replicated             # UPROPERTY(Replicated) + GetLifetimeReplicatedProps
@savegame               # UPROPERTY(SaveGame)
@transient              # UPROPERTY(Transient)
@editdefaults           # UPROPERTY(EditDefaultsOnly)
@visibleonly            # UPROPERTY(VisibleAnywhere)
@blueprint_callable     # UFUNCTION(BlueprintCallable)
@category("Name")       # UFUNCTION(Category="Name")
```

### Editor Attributes (Slate/Details/Viewports)

```kain
@slate                  # SCompoundWidget with SLATE_BEGIN_ARGS
@details                # IDetailCustomization subclass
@viewport               # SEditorViewport + FEditorViewportClient
@toolbar                # FToolBarBuilder extension
@asset_editor           # FAssetEditorToolkit subclass
@editor_module          # IModuleInterface with IMPLEMENT_MODULE

# Slate widget attributes
@argument               # SLATE_ARGUMENT (constructor param)
@attribute              # SLATE_ATTRIBUTE (reactive binding)
@event                  # SLATE_EVENT (delegate callback)

# Details panel decorators
@slider(min, max)       # Numeric slider widget
@color_picker           # FLinearColor picker
@button("Label")        # Clickable button in details
@dropdown               # Enum dropdown
@text_box               # String input field

# Viewport attributes
@scene_actor            # Actor spawned in preview scene
@camera                 # Editor camera controller
```

### Shader Attributes

```kain
uniform name: Type @N   # Binding slot (N = register index)
CFG_*                   # Permutation prefix (compile-time branch)
ENABLE_*                # Permutation prefix (feature toggle)
```

---

## SHADER SYSTEM - THE GPU PIPELINE

### Shader Stages

```kain
shader fragment Name(uv: Vec2) -> Vec4:        # Pixel shader
shader compute Name(id: Vec3) -> Vec4:         # Compute shader
shader vertex Name(pos: Vec3) -> VertexOutput: # Vertex shader
shader surface Name(uv: Vec2) -> SurfaceOutput:# Material shader
```

### Uniform Bindings

```kain
uniform base_color: Vec3 @0           # Scalar parameter
uniform albedo_map: Sampler2D @1      # Texture input
uniform output_tex: RWTexture2D @0    # UAV output (compute)
```

### Permutations (Zero Runtime Cost)

```kain
uniform CFG_HIGH_QUALITY: Float @0    # Compile-time branch
uniform ENABLE_SHADOWS: Float @1      # Feature toggle

if CFG_HIGH_QUALITY:
    # Expensive path (only compiled if enabled)
    color = expensive_calculation()
elif ENABLE_SHADOWS:
    # Shadow path
    color = shadow_calculation()
```

**Generates:** Multiple shader variants, selected at material creation time.

### Type Mapping (KAIN → HLSL)

```
Float → float          Vec2 → float2         Vec3 → float3
Vec4 → float4          Int → int             Bool → bool
Sampler2D → Texture2D  Sampler3D → Texture3D
RWTexture2D → RWTexture2D<float4>
```

### Shader Output

**Per shader:**
- `.usf` file (HLSL code)
- `F{Name}Shader` C++ class (FGlobalShader subclass)
- `SHADER_PARAMETER_STRUCT` with bindings
- `IMPLEMENT_GLOBAL_SHADER` registration
- `AddPass_{Name}()` helper function for RDG

**Auto-wired:** Shader directory mapping in module StartupModule()

---

## ACTOR SYSTEM - NETWORKING & REPLICATION

### RPC Naming Convention (Auto-detected)

```kain
actor GameMode:
    on Server_Method():      # Server RPC (Reliable)
    on Client_Method():      # Client RPC (Reliable)
    on Multicast_Method():   # Multicast RPC (Reliable)
    on Tick(delta: Float):   # Standard override
    on BeginPlay():          # Standard override
```

**Generates:**
- `UFUNCTION(Server, Reliable)` + `_Implementation` + `_Validate`
- Automatic `GetLifetimeReplicatedProps()` for `@replicated` state
- Correct `DOREPLIFETIME()` macros

### State Management

```kain
actor Player:
    state health: Float = 100.0           # Local state
    
    @replicated
    state score: Int = 0                  # Replicated state
    
    @savegame
    state inventory: Array<Int> = []      # Persisted state
    
    @transient
    state is_jumping: Bool = false        # Never replicated/saved
```

---

## COMPONENT SYSTEM - MODULAR ARCHITECTURE

```kain
@component
struct HealthComponent:
    @replicated
    current: Float
    
    @replicated
    max: Float
    
    @transient
    regen_rate: Float
    
    @savegame
    is_invulnerable: Bool
```

**Generates:** `UHealthComponent : public UActorComponent` with full replication support.

---

## DATATABLE SYSTEM - CSV IMPORT

```kain
@datatable
struct ItemData:
    id: Int
    name: String
    description: String
    icon_path: String
    value: Int
    weight: Float
    rarity: ItemRarity
```

**Generates:** `FItemData : public FTableRowBase`

**Usage in UE5:** Create DataTable asset, import CSV, reference in Blueprints.

---

## SLATE SYSTEM - EDITOR UI

### Widget Composition

```kain
@slate
struct SMyWidget:
    @argument
    title: Text                    # Constructor param
    
    @attribute
    value: Float                   # Reactive binding
    
    @event
    on_clicked: OnButtonClicked    # Delegate callback
    
    fn Compose() -> Widget:
        let vbox = VerticalBox()
        
        let header = TextBlock()
        header.Text(title)
        vbox.Add(header)
        
        let slider = Slider()
        slider.Value(value)
        slider.OnValueChanged(on_value_changed)
        vbox.Add(slider)
        
        return vbox
```

**Available widgets:**
- Layout: `VerticalBox`, `HorizontalBox`, `Splitter`, `Border`, `ScrollBox`
- Input: `Button`, `Slider`, `CheckBox`, `TextBox`, `ComboBox`
- Display: `TextBlock`, `Image`, `ColorBlock`, `ProgressBar`
- Advanced: `TreeView`, `ListView`, `TileView`, `Graph`

**Generates:** `SCompoundWidget` with `SLATE_BEGIN_ARGS`, `SNew()` chains, proper slot management.

---

## DETAILS PANEL SYSTEM - PROPERTY CUSTOMIZATION

```kain
@details
struct MyActorDetails:
    @category("Rendering")
    @slider(0.0, 1.0)
    roughness: Float
    
    @category("Rendering")
    @color_picker
    base_color: Vec3
    
    @category("Actions")
    @button("Reset All")
    fn ResetAll():
        roughness = 0.5
        base_color = vec3(1.0, 1.0, 1.0)
```

**Generates:** `IDetailCustomization` subclass with `CustomizeDetails()` override.

**Supported decorators:**
- `@slider(min, max)` - Numeric slider
- `@color_picker` - FLinearColor picker
- `@button("Label")` - Action button
- `@dropdown` - Enum dropdown
- `@text_box` - String input
- `@checkbox` - Boolean toggle
- `@vector_input` - FVector editor
- `@asset_picker(Type)` - Asset reference picker

---

## VIEWPORT SYSTEM - 3D PREVIEW

```kain
@viewport
struct MyViewport:
    @scene_actor
    preview_actor: MyActor
    
    @camera
    camera: EditorCamera
    
    fn SetPreviewMaterial(mat: Material):
        preview_actor.SetMaterial(mat)
```

**Generates:**
- `SMyViewport : public SEditorViewport`
- `FMyViewportClient : public FEditorViewportClient`
- Preview scene setup with actor spawning
- Camera controller integration

---

## TOOLBAR SYSTEM - EDITOR ACTIONS

```kain
@toolbar
struct MyToolbar:
    @button("Run", icon="Play", shortcut="F5")
    fn OnRun():
        println("Running...")
    
    @toggle("Auto-Save", icon="Save")
    auto_save: Bool
    
    @separator
    
    @button("Export", icon="Export", shortcut="Ctrl+E")
    fn OnExport():
        println("Exporting...")
```

**Generates:** `FToolBarBuilder` extension with icon/shortcut registration.

---

## ASSET EDITOR SYSTEM - COMPLETE EDITOR WINDOW

```kain
@asset_editor
struct MyAssetEditor:
    @asset
    my_asset: MyAssetType
    
    @viewport
    viewport: MyViewport
    
    @details
    properties: MyDetails
    
    @toolbar
    toolbar: MyToolbar
    
    @slate
    custom_panel: MyCustomWidget
    
    fn OnAssetOpened():
        # Initialize editor state
        properties.LoadFromAsset(my_asset)
    
    fn OnPropertyChanged():
        # Sync changes to viewport
        viewport.UpdatePreview(properties)
```

**Generates:** `FAssetEditorToolkit` subclass with tab management, asset saving, undo/redo.

---

## EDITOR MODULE SYSTEM - MENU/TOOLBAR INTEGRATION

```kain
@editor_module
struct MyModule:
    module_name: String
    version: String
    
    @menu_entry("Tools/My Tool")
    fn OpenTool():
        SpawnTab("MyTool")
    
    @toolbar_button("My Tool", icon="Tool")
    fn ToolbarOpen():
        OpenTool()
```

**Generates:**
- `IModuleInterface` subclass
- `IMPLEMENT_MODULE(FMyModule, MyModule)`
- Menu/toolbar registration in `StartupModule()`
- Tab spawner registration

---

## NAMING CONVENTIONS (Auto-Applied)

### Prefixes (Compiler adds automatically)

```
Actor → A prefix      (Player → APlayer)
Struct → F prefix     (Transform → FTransform)
Enum → E prefix       (Direction → EDirection)
Component → U prefix  (Health → UHealthComponent)
Interface → I prefix  (Damageable → IDamageable)
```

**CRITICAL:** Never manually prefix in KAIN source. Compiler detects existing prefixes and doesn't double-prefix.

### Pointer Semantics (Auto-detected)

```
UObject-derived → * suffix    (AActor → AActor*)
Structs → value type          (FVector → FVector)
Primitives → value type       (int32, float, bool)
```

---

## BUILD PIPELINE - HOW IT WORKS

### 1. Parse Phase (Per-File)

```
file1.kn → Lexer → Parser → AST₁ ✓
file2.kn → Lexer → Parser → AST₂ ✓
file3.kn → Lexer → Parser → AST₃ ✓
```

**Errors:** `actors.kn:11:51: Expected initializer`

### 2. Merge Phase (AST Combining)

```
AST₁ + AST₂ + AST₃ → Merged AST
```

**Validates:** No duplicate definitions, all types resolve.

### 3. Type Check Phase

```
Merged AST → Type Checker → Typed Program
```

**Validates:** Type correctness, function signatures, expression types.

### 4. Oracle Phase (UE5 Semantic Validation)

```
Typed Program + EngineKnowledge → Oracle → Validated Program
```

**Validates:**
- Name collisions with engine types
- Virtual function overrides
- UPROPERTY/UFUNCTION rules
- Module dependencies

### 5. Codegen Phase (Parallel Dispatch)

```
Validated Program → ue5 crate      → Actors/Structs/Enums .h/.cpp
                  → ue5-editor crate → Slate/Details/Viewport .h/.cpp
                  → ue5-shaders crate → .usf + shader bindings
```

### 6. Packager Phase (File Assembly)

```
All codegen outputs → Packager → Plugin structure:
    Source/
        {Plugin}/
            Public/
                {Plugin}.h
                {Type1}.h
                {Type2}.h
            Private/
                {Plugin}.cpp
                {Type1}.cpp
                {Type2}.cpp
        {Plugin}Editor/
            Public/
                {Widget}.h
                {Details}.h
            Private/
                {Widget}.cpp
                {Details}.cpp
    Shaders/
        {Shader1}.usf
        {Shader2}.usf
    {Plugin}.uplugin
    {Plugin}.Build.cs
```

---

## PLUGIN STRUCTURE TEMPLATE

### Minimal Plugin (Runtime Only)

```kain
// types.kn
enum ItemRarity: Common, Rare, Epic

@datatable
struct ItemData:
    id: Int
    name: String
    rarity: ItemRarity

// components.kn
@component
struct InventoryComponent:
    @replicated
    items: Array<ItemData>
    capacity: Int

// actors.kn
actor ItemPickup:
    state item_id: Int = 0
    
    on Server_Pickup(player: Actor):
        println("Item picked up")

// utilities.kn
@blueprint
fn calculate_value(base: Int, rarity: ItemRarity) -> Int:
    match rarity:
        ItemRarity::Common => base
        ItemRarity::Rare => base * 2
        ItemRarity::Epic => base * 5
        _ => base
```

**Output:** 15+ C++ files, compiles in UE5, Blueprint-ready.

### Full Plugin (Runtime + Editor)

Add to above:

```kain
// editor.kn
@slate
struct SInventoryPanel:
    @argument
    title: Text
    
    fn Compose() -> Widget:
        let vbox = VerticalBox()
        let header = TextBlock()
        header.Text(title)
        vbox.Add(header)
        return vbox

@details
struct ItemDataDetails:
    @category("Item")
    @slider(0, 1000)
    value: Int
    
    @category("Item")
    @color_picker
    rarity_color: Vec3

@viewport
struct ItemPreviewViewport:
    @scene_actor
    preview_item: ItemPickup
    
    @camera
    camera: EditorCamera

@asset_editor
struct ItemEditor:
    @asset
    item_data: ItemData
    
    @viewport
    viewport: ItemPreviewViewport
    
    @details
    properties: ItemDataDetails
    
    fn OnAssetOpened():
        properties.LoadFromAsset(item_data)

@editor_module
struct InventoryEditorModule:
    @menu_entry("Tools/Inventory Editor")
    fn OpenEditor():
        SpawnTab("InventoryEditor")
```

**Output:** 30+ C++ files, full editor integration, custom asset type.

---

## KAIN.TOML CONFIGURATION

```toml
[package]
name = "MyPlugin"
version = "1.0.0"
author = "Your Name"
description = "Plugin description"

[ue5]
engine_version = "5.4"
modules = ["Core", "CoreUObject", "Engine", "Slate", "SlateCore"]

[build]
output_dir = "Source"
shader_dir = "Shaders"

[features]
networking = true
editor = true
shaders = true
```

---

## COMMON PATTERNS

### Inventory System

```kain
enum ItemType: Weapon, Armor, Consumable

@datatable
struct ItemDefinition:
    id: Int
    name: String
    type: ItemType
    max_stack: Int

@component
struct InventoryComponent:
    @replicated
    items: Array<ItemStack>
    capacity: Int

struct ItemStack:
    item_id: Int
    quantity: Int

@blueprint
fn can_add_item(inv: InventoryComponent, item_id: Int, qty: Int) -> Bool:
    return true  # Logic here
```

### Combat System

```kain
@component
struct CombatComponent:
    @replicated
    health: Float
    
    @replicated
    max_health: Float
    
    @replicated
    armor: Float

actor Weapon:
    state damage: Float = 10.0
    state attack_speed: Float = 1.0
    
    on Server_Attack(target: Actor):
        # Apply damage
        Multicast_PlayEffect()
    
    on Multicast_PlayEffect():
        println("Attack effect")

@blueprint
fn calculate_damage(base: Float, armor: Float, crit: Bool) -> Float:
    var dmg = base * (1.0 - armor / 100.0)
    if crit:
        dmg = dmg * 2.0
    return dmg
```

### Shader Material

```kain
shader fragment PBRMaterial(uv: Vec2) -> Vec4:
    uniform roughness: Float @0
    uniform metallic: Float @1
    uniform albedo_map: Sampler2D @2
    uniform normal_map: Sampler2D @3
    
    let albedo = sample(albedo_map, uv).rgb
    let normal = sample(normal_map, uv).rgb
    
    # PBR lighting here
    return vec4(albedo, 1.0)
```

---

## ERROR HANDLING - WHAT GOOD ERRORS LOOK LIKE

### ❌ BAD (Old System)
```
error: Expected Eq, got Newline
  --> position 512
```

### ✅ GOOD (Current System)
```
❌ Parse error in actors.kn:11:51

   11 |     state inventory_component: InventoryComponent
      |                                                   ^
      |
   Expected initializer. Actor state must have a default value.
   
   Help: Add an initializer:
         state inventory_component: InventoryComponent = ...
   
   Note: Components should be created in BeginPlay(), not as state.
```

**LLM can fix immediately from error message alone.**

---

## PRODUCTION QUALITY GUARANTEES

If `kain build --ue5` succeeds:

✅ Compiles in UE5 (no C++ errors)
✅ No memory leaks (KAIN is memory-safe)
✅ No typos (compiler-verified names)
✅ Correct UE5 macros (auto-generated)
✅ Proper networking (RPCs auto-configured)
✅ Blueprint integration (works out of box)
✅ Shader registration (auto-wired)
✅ Marketplace-ready (follows UE5 conventions)

**Zero manual fixes required.**

---

## MARKETPLACE DOMINATION MATH

**Traditional:** 80-120 hours per plugin, 15-30 plugins/year
**KAIN:** 7.5-18 hours per plugin, 150-300 plugins/year

**10x volume + better quality = unassailable market position**

---

## QUICK REFERENCE CHEAT SHEET

```kain
# Enums
enum Type: Variant1, Variant2

# DataTable
@datatable
struct Data: ...

# Component
@component
struct Comp: ...

# Actor with RPCs
actor Name:
    on Server_Method(): ...
    on Client_Method(): ...
    on Multicast_Method(): ...

# Blueprint function
@blueprint
fn utility(...) -> Type: ...

# Shader
shader fragment Name(uv: Vec2) -> Vec4:
    uniform param: Type @N
    return vec4(...)

# Slate widget
@slate
struct SWidget:
    fn Compose() -> Widget: ...

# Details panel
@details
struct Details:
    @slider(min, max)
    value: Float

# Viewport
@viewport
struct Viewport:
    @scene_actor
    actor: MyActor

# Asset editor
@asset_editor
struct Editor:
    @asset
    asset: MyAsset
    @viewport
    viewport: MyViewport
    @details
    details: MyDetails

# Editor module
@editor_module
struct Module:
    @menu_entry("Tools/Name")
    fn Open(): ...
```

---

## TESTING CHECKLIST

- [ ] `kain build --ue5` succeeds
- [ ] Generated C++ compiles in UE5
- [ ] Blueprint integration works
- [ ] Networking replicates correctly
- [ ] Save/load persists data
- [ ] Shaders render correctly
- [ ] Editor tools open without crash
- [ ] No runtime errors in UE5
- [ ] Performance is acceptable

---

## KEY FILES TO REFERENCE

- `testing/Phase3/SlateTest4/ultimate.kn` - Comprehensive test plugin
- `kn_library/shaders/KainFlowGod.kn` - Advanced shader example
- `kn_library/editor/animation_editor_suite.kn` - Editor example
- `docs/AGENT_HANDOFF.md` - Architecture deep dive
- `unreal/metadata/engine_knowledge.json` - Type database

---

## FINAL NOTES FOR AI AGENTS

1. **Never hardcode** - All type mappings come from metadata
2. **Trust the compiler** - If it builds, it works
3. **Use attributes liberally** - They're free and powerful
4. **Organize by feature** - types.kn, components.kn, actors.kn, etc.
5. **Test incrementally** - Build after each major addition
6. **Read error messages** - They're designed for LLM comprehension
7. **Reference examples** - kn_library has 100+ working patterns
8. **Leverage metadata** - 11GB of UE5 knowledge at your fingertips

**The system is designed for you. Use it.**
