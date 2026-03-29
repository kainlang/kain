# KAIN Quick Reference - All Features

## New 2026 Features

### @subsystem - World Subsystems
```kain
@subsystem
struct MySubsystem:
    data: Int
    
    fn my_method():
        pass
```
**Generates:** `UMySubsystemSubsystem : public UWorldSubsystem`

### @tick - Tickable Objects
```kain
@subsystem
@tick
struct MySubsystem:
    fn on_tick(delta: Float):
        # Called every frame
        pass
```
**Generates:** `FTickableGameObject` interface

### @blueprint_event - Blueprint-Overridable Events
```kain
actor MyActor:
    @blueprint_event
    fn on_custom_event(value: Int):
        println("Default C++ behavior")
```
**Generates:** `UFUNCTION(BlueprintNativeEvent)` + `_Implementation`

### @graph_runtime - Runtime Graph System
```kain
@graph_runtime
struct MyGraph:
    @node_data
    struct MyNode:
        @property
        value: Float
        
        @input_pin
        in_exec: Exec
        
        @output_pin
        out_exec: Exec
    
    @instance
    struct MyInstance:
        current_node: Int
        
        fn start() -> Bool:
            return true
```
**Generates:** NodeData classes, GraphInstance, Asset classes

### @editor_module - Editor Extensions
```kain
@editor_module
struct MyModule:
    @menu_entry(path = "Tools/My", label = "Open Tool")
    fn on_open():
        println("Opening...")
    
    @toolbar_button(section = "Content", icon = "Icons.Tool")
    fn on_quick_action():
        println("Quick action...")
```
**Generates:** `IModuleInterface` with menu/toolbar registration

### @dispatch - Automatic Shader Dispatch
```kain
@dispatch("MyShader")
actor MyActor:
    on Tick(delta: Float):
        # Shader dispatch happens automatically
        pass
```
**Generates:** RDG boilerplate for GPU shader execution

## Core Features

### Enums
```kain
enum MyEnum:
    Variant1
    Variant2
    Variant3
```
**Generates:** `UENUM(BlueprintType) enum class EMyEnum`

### DataTables (CSV Import)
```kain
@datatable
struct MyData:
    id: Int
    name: String
    value: Float
```
**Generates:** `FMyData : public FTableRowBase`

### Components
```kain
@component
struct MyComponent:
    @replicated
    health: Float
    
    @savegame
    level: Int
    
    @transient
    temp_data: Bool
```
**Generates:** `UMyComponent : public UActorComponent`

### Actors
```kain
actor MyActor:
    @replicated
    state score: Int = 0
    
    on BeginPlay():
        println("Actor started")
    
    on Tick(delta: Float):
        score = score + 1
    
    on Server_DoAction():
        Multicast_Notify()
    
    on Multicast_Notify():
        println("Action done")
    
    @blueprint_callable
    fn GetScore() -> Int:
        return score
```
**Generates:** `AMyActor : public AActor` with RPCs

### Compute Shaders
```kain
shader compute MyShader(thread_id: Vec3) -> Unit:
    uniform CFG_HIGH_QUALITY: Float @0
    uniform data: RWBuffer<Float> @1
    uniform count: Int @2
    
    let idx = thread_id.x
    if idx >= count:
        return
    
    if CFG_HIGH_QUALITY:
        data[idx] = data[idx] * 2.0
    else:
        data[idx] = data[idx] * 1.5
```
**Generates:** `.usf` file + `FGlobalShader` + dispatch helpers

### Material Graphs
```kain
@material_graph(blend_mode = Opaque, shading_model = DefaultLit)
material MyMaterial:
    input base_color: Vec3 = vec3(1, 1, 1)
    input metallic: Float = 0.5
    input roughness: Float = 0.5
    
    output base_color = base_color
    output metallic = metallic
    output roughness = roughness
```
**Generates:** Material factory + binary .uasset

### Blueprint Functions
```kain
@blueprint
fn my_utility(a: Float, b: Float) -> Float:
    return a + b

@blueprint_pure
fn is_valid(value: Int) -> Bool:
    return value > 0

@blueprint_callable
fn do_action():
    println("Action!")
```
**Generates:** `UBlueprintFunctionLibrary` static methods

### Slate Widgets
```kain
@slate
struct MyWidget:
    @property
    text: String
    
    @property
    count: Int
    
    fn construct() -> Widget:
        let vbox = VerticalBox()
        
        let label = TextBlock()
        label.Text(text)
        vbox.Add(label)
        
        let count_label = TextBlock()
        count_label.Text("Count: {count}")
        vbox.Add(count_label)
        
        return vbox
```
**Generates:** `SMyWidget : public SCompoundWidget`

### Details Panels
```kain
@details
struct MyDetails:
    @category("Settings")
    @slider(min = 0.0, max = 100.0)
    value: Float
    
    @category("Visual")
    @color_picker
    color: Vec3
    
    @category("Actions")
    @button(label = "Do Action")
    fn on_action():
        println("Action!")
```
**Generates:** `FMyDetailsCustomization : public IDetailCustomization`

### Viewports
```kain
@viewport
struct MyViewport:
    @scene_actor
    mesh: StaticMeshComponent
    
    @camera
    camera: CameraComponent
    
    fn on_viewport_tick(delta: Float):
        mesh.AddLocalRotation(vec3(0, delta * 45, 0))
```
**Generates:** `SMyViewport` + `FMyViewportClient`

### Toolbars
```kain
@toolbar
struct MyToolbar:
    @button(icon = "Icons.Play", tooltip = "Play")
    fn on_play():
        println("Playing...")
    
    @toggle(label = "Debug", default = true)
    fn on_toggle_debug(enabled: Bool):
        println("Debug: {enabled}")
    
    @separator
    
    @dropdown(label = "Mode", options = ["A", "B", "C"])
    fn on_mode_changed(value: String):
        println("Mode: {value}")
```
**Generates:** `FToolBarBuilder` extension

### Asset Editors
```kain
@asset_editor
struct MyEditor:
    @viewport
    preview: MyViewport
    
    @details
    properties: MyDetails
    
    @toolbar
    tools: MyToolbar
    
    fn on_asset_opened(asset: MyAsset):
        println("Asset opened")
    
    fn on_asset_saved():
        println("Asset saved")
```
**Generates:** `FMyEditorToolkit : public FAssetEditorToolkit`

## Attributes Reference

### Struct Attributes
- `@datatable` - CSV-importable struct (FTableRowBase)
- `@component` - Actor component (UActorComponent)
- `@subsystem` - World subsystem (UWorldSubsystem)
- `@tick` - Tickable object (FTickableGameObject)
- `@slate` - Slate widget (SCompoundWidget)
- `@details` - Details customization (IDetailCustomization)
- `@viewport` - Viewport widget (SEditorViewport)
- `@toolbar` - Toolbar builder (FToolBarBuilder)
- `@asset_editor` - Asset editor (FAssetEditorToolkit)
- `@editor_module` - Editor module (IModuleInterface)
- `@graph_runtime` - Runtime graph system
- `@async_task` - Async task (FRunnable) [IR complete]

### Field Attributes
- `@replicated` - Replicated property (UPROPERTY(Replicated))
- `@savegame` - Saved property (UPROPERTY(SaveGame))
- `@transient` - Transient property (UPROPERTY(Transient))
- `@editdefaults` - Editable in defaults (UPROPERTY(EditDefaultsOnly))
- `@visibleonly` - Visible only (UPROPERTY(VisibleAnywhere))
- `@property` - Slate widget property
- `@scene_actor` - Viewport scene actor
- `@camera` - Viewport camera
- `@input_pin` - Graph input pin
- `@output_pin` - Graph output pin
- `@node_data` - Graph node data
- `@instance` - Graph instance

### Function Attributes
- `@blueprint` - Blueprint-callable function
- `@blueprint_pure` - Blueprint pure function (no side effects)
- `@blueprint_callable` - Blueprint-callable method
- `@blueprint_event` - Blueprint-overridable event (NEW 2026)
- `@dispatch` - Automatic shader dispatch (NEW 2026)
- `@menu_entry` - Editor menu entry
- `@toolbar_button` - Editor toolbar button
- `@button` - Details panel button
- `@slider` - Details panel slider
- `@color_picker` - Details panel color picker
- `@category` - Details panel category

### Shader Attributes
- `@material_graph` - Material graph definition
- Permutation uniforms: `CFG_*` or `ENABLE_*` prefix

## Type Mapping

| KAIN Type | UE5 Type | Pointer? |
|-----------|----------|----------|
| `Int` | `int64` | No |
| `Float` | `float` | No |
| `Bool` | `bool` | No |
| `String` | `FString` | No |
| `Vec2` | `FVector2D` | No |
| `Vec3` | `FVector` | No |
| `Vec4` | `FLinearColor` | No |
| `Array<T>` | `TArray<T>` | No |
| `Map<K,V>` | `TMap<K,V>` | No |
| `Actor` | `AActor*` | Yes |
| `Component` | `UActorComponent*` | Yes |
| Custom Actor | `AMyActor*` | Yes |
| Custom Component | `UMyComponent*` | Yes |
| Custom Struct | `FMyStruct` | No |
| Custom Enum | `EMyEnum` | No |

## Naming Conventions

| KAIN | UE5 Prefix | Example |
|------|------------|---------|
| `actor Name` | `A` | `APlayer` |
| `struct Name` | `F` | `FTransform` |
| `enum Name` | `E` | `EDirection` |
| `@component Name` | `U` | `UHealthComponent` |
| `@subsystem Name` | `U` | `UMySubsystemSubsystem` |

**Note:** If KAIN source already has prefix (e.g., `EHealthStatus`), compiler detects it and doesn't double-prefix.

## RPC Naming

| Prefix | Type | Reliability |
|--------|------|-------------|
| `Server_*` | Server RPC | Reliable |
| `Client_*` | Client RPC | Reliable |
| `Multicast_*` | Multicast RPC | Reliable |

Example:
```kain
actor MyActor:
    on Server_DoAction():
        # Runs on server
        Multicast_Notify()
    
    on Multicast_Notify():
        # Runs on all clients
        println("Notified!")
```

## Build Commands

```bash
# Build plugin
kain build --ue5

# Build with verbose output
kain build --ue5 --verbose

# Build specific file
kain build --ue5 --file my_plugin.kn

# Check syntax only
kain check my_plugin.kn
```

## Common Patterns

### Inventory System
```kain
@datatable
struct ItemData:
    id: Int
    name: String
    value: Int

@component
struct InventoryComponent:
    @replicated
    items: Array<Int>
    capacity: Int

actor Player:
    state inventory: InventoryComponent = InventoryComponent()
    
    on Server_AddItem(item_id: Int):
        inventory.items.push(item_id)
```

### Combat System
```kain
@component
struct HealthComponent:
    @replicated
    current: Float
    max: Float

actor Enemy:
    state health: HealthComponent = HealthComponent()
    
    on Server_TakeDamage(amount: Float):
        health.current = health.current - amount
        if health.current <= 0.0:
            Multicast_Die()
    
    on Multicast_Die():
        println("Enemy died!")
```

### Quest System
```kain
@datatable
struct QuestData:
    id: Int
    name: String
    objectives: Array<String>

@subsystem
struct QuestSubsystem:
    active_quests: Array<Int>
    
    fn start_quest(quest_id: Int) -> Bool:
        active_quests.push(quest_id)
        return true
```

### Dialogue System
```kain
@graph_runtime
struct DialogueGraph:
    @node_data
    struct DialogueNode:
        @property
        text: String
        
        @input_pin
        in_exec: Exec
        
        @output_pin
        next: Exec
    
    @instance
    struct DialogueInstance:
        current_node: Int
        
        fn start() -> Bool:
            return true
```

## Error Messages

KAIN provides clear, actionable error messages:

```
❌ Parse error in my_plugin.kn:42:15

   42 |     state health: HealthComponent
      |                                  ^
      |
   Expected initializer. Actor state must have a default value.
   
   Help: Add an initializer:
         state health: HealthComponent = HealthComponent()
```

## Performance Tips

1. Use `@transient` for data that doesn't need replication/saving
2. Use permutations (`CFG_*`) for quality levels (zero runtime cost)
3. Use `@tick` only when needed (adds overhead)
4. Batch operations in Blueprint functions (fewer calls)
5. Use `@dispatch` for GPU-accelerated processing

## Testing

```bash
# Run all tests
cargo test --package ue5 --package ue5-editor --package ue5-graphs --package ue5-shaders

# Run specific test
cargo test --package ue5 test_subsystem_basic_generation

# Run with output
cargo test -- --nocapture
```

## Documentation

- Full guide: `Factory/Example/README.md`
- Pattern reference: `.kiro/steering/kain-patterns.md`
- Architecture: `.windsurf/rules/AGENT_HANDOFF.md`
- LLM guide: `.windsurf/rules/llm-first-development.md`

---

**Last Updated:** February 22, 2026
