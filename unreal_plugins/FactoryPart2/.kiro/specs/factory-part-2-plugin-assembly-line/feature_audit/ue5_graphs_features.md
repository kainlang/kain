# ue5-graphs Features — Graph Editor & Runtime Codegen

> **Crate:** `Kain/crates/ue5-graphs/`  
> **Status:** Production — Both graph runtime (in-game logic graphs) and graph editor (UEdGraph-based editing) implemented  
> **Last Updated:** 2026-03-01

---

## Overview

The `ue5-graphs` crate generates two categories of graph-related UE5 C++ code:

1. **Graph Runtime** — `UEdGraph`-compatible runtime graphs with node data, instance execution, and asset management (for in-game use)
2. **Graph Editor** — `UEdGraphNode` subclasses for custom KAIN graph editors with pin types, schema, and factory registration

This crate enables visual node-based editors for dialogue systems, quest systems, state machines, behavior trees, material graphs, and any custom graph-based logic.

---

## Feature Categories

### 1. Graph Runtime System (`@graph_runtime`)
### 2. Graph Editor System (`@graph_editor`)
### 3. NodeData System (`@node_data`)
### 4. Pin Type System
### 5. Graph Instance System (`@instance`)
### 6. Binary Asset Serialization
### 7. Schema & Validation

---

## 1. Graph Runtime System (`@graph_runtime`)

### Purpose
Creates runtime graph execution systems for in-game logic (dialogue, quests, state machines, etc.)

### KAIN Syntax

```kain
@graph_runtime
graph DialogueSystem:
    @node_data
    node SpeakerNode:
        speaker_name: String = "NPC"
        @input_pin
        in_exec: Exec
        @output_pin
        next: Exec
```

### Generated C++ (Graph Runtime)

**UNodeData_SpeakerNode** — Node data class:
```cpp
UCLASS()
class NARRATIVEGRAPH_API UNodeData_SpeakerNode : public UNodeDataBase {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SpeakerName = TEXT("NPC");
    
    // Pin descriptions
    virtual TArray<FNodePinDesc> GetInputPins() const override;
    virtual TArray<FNodePinDesc> GetOutputPins() const override;
    
    // Execution
    virtual void ExecuteNode(UGraphInstance* Instance) override;
};
```

**UDialogueSystemInstance** — Runtime execution engine:
```cpp
UCLASS()
class NARRATIVEGRAPH_API UDialogueSystemInstance : public UGraphInstanceBase {
    GENERATED_BODY()
public:
    void ExecuteFrom(UNodeDataBase* StartNode);
    bool EvaluatePin(FName PinName);
    void SetLocalVar(FName Name, FInstanceValue Value);
    FInstanceValue GetLocalVar(FName Name);
};
```

**UDialogueSystemAsset** — Content browser asset:
```cpp
UCLASS(BlueprintType)
class NARRATIVEGRAPH_API UDialogueSystemAsset : public UGraphAssetBase {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Graph")
    UDialogueSystemGraphData* GraphData;
    
    UFUNCTION(BlueprintCallable, Category = "DialogueSystem|Asset")
    UDialogueSystemInstance* CreateInstance();
    
    UFUNCTION(BlueprintPure, Category = "DialogueSystem|Asset")
    bool ValidateGraph() const;
};
```

### Factory Part 1 Examples (Graph Runtime)

**NarrativeGraph** (`Factory/NarrativeGraph/narrative_graph.kn`):
```kain
@graph_runtime
struct DialogueGraph:
    @node_data
    struct NPCNode:
        @property
        speaker_name: String
        @property
        dialogue_text: String
        @property
        speaker_color: Vec3
        @input_pin
        in_exec: Exec
        @output_pin
        next: Exec
        @output_pin
        choice_1: Exec
    
    @instance
    struct DialogueInstance:
        current_node_id: Int
        dialogue_history: Array<Int>
        
        fn start_dialogue() -> Bool:
            current_node_id = 0
            dialogue_history = []
            return true
```

Generated files:
- `Factory/NarrativeGraph/NarrativeGraph/Source/NarrativeGraph/Public/DialogueGraphGraphAsset.h`
- `Factory/NarrativeGraph/NarrativeGraph/Source/NarrativeGraph/Public/NPCNodeNodeData.h`
- `Factory/NarrativeGraph/NarrativeGraph/Source/NarrativeGraph/Public/DialogueGraphPinData.h`

---

## 2. Graph Editor System (`@graph_editor`)

### Purpose
Creates UEdGraph-based visual editors for designing graphs in the UE5 editor

### KAIN Syntax

```kain
@graph_editor
graph DialogueGraphEditor:
    @node_type
    @category("Dialogue/NPC")
    @display_name("NPC Dialogue")
    @tooltip("An NPC speaking line with multiple choice outputs")
    node NPCNode:
        properties:
            SpeakerName: String = "NPC"
            DialogueText: String = "Hello!"
        inputs:
            InExec: Exec
        outputs:
            Next: Exec
            Choice1: Exec
```

### Generated C++ (Graph Editor)

**UEdGraphNode Subclass**:
```cpp
UCLASS()
class UDialogueGraphEditorNPCNodeNode : public UEdGraphNode {
    GENERATED_BODY()
public:
    // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual void AllocateDefaultPins() override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const;
    virtual FLinearColor GetPinColor() const override;
};
```

**UEdGraphSchema Subclass**:
```cpp
UCLASS()
class UDialogueGraphEditorSchema : public UEdGraphSchema {
    GENERATED_BODY()
public:
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const override;
    virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
    virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
    virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
};
```

**UEdGraph Subclass**:
```cpp
UCLASS()
class UDialogueGraphEditor : public UEdGraph {
    GENERATED_BODY()
public:
    UDialogueGraphEditor();
};
```

### Node Attributes

| Attribute | Purpose | Example |
|-----------|---------|---------|
| `@node_type` | Marks a node type definition | `@node_type` |
| `@category("X")` | Sets node category in context menu | `@category("Dialogue/NPC")` |
| `@display_name("X")` | Sets display name in editor | `@display_name("NPC Dialogue")` |
| `@tooltip("X")` | Sets tooltip text | `@tooltip("An NPC speaking line")` |
| `@color(r, g, b, a)` | Sets node title bar color | `@color(0.8, 0.4, 0.2, 1.0)` |
| `@icon("X")` | Sets node icon | `@icon("Texture.Icon")` |
| `@execution_logic("X")` | Documents execution behavior | `@execution_logic("Execute custom HLSL code")` |

### Factory Part 1 Examples (Graph Editor)

**NarrativeGraph** (`Factory/NarrativeGraph/narrative_graph.kn`):
```kain
@graph_editor
graph DialogueGraphEditor:
    @node_type
    @category("Dialogue/Start")
    @display_name("Dialogue Root")
    @tooltip("The starting point of a dialogue graph")
    node RootNode:
        properties:
            GraphName: String = "New Dialogue"
        outputs:
            Start: Exec
    
    @node_type
    @category("Dialogue/Logic")
    @display_name("Condition Branch")
    @tooltip("Branch dialogue flow based on a condition tag")
    node BranchNode:
        properties:
            ConditionTag: String = "condition"
        inputs:
            InExec: Exec
        outputs:
            TrueBranch: Exec
            FalseBranch: Exec
    
    @schema
    schema:
        no_cycles: false
        max_depth: 100
```

Generated files:
- `Factory/NarrativeGraph/NarrativeGraph/Source/NarrativeGraphEditor/Public/DialogueGraphEditorFactory.h`
- Contains: `UDialogueGraphEditorRootNodeNode`, `UDialogueGraphEditorBranchNodeNode`, `UDialogueGraphEditorSchema`

**Example_Graph** (`Factory/Example_Graph/graph.kn`) — Complete feature showcase with 20+ node types demonstrating all pin types and attributes.

---

## 3. NodeData System (`@node_data`)

### Purpose
Defines runtime node implementations with properties, pins, and execution logic

### KAIN Syntax

```kain
@node_data
node ColorBlendData:
    category: "Material/Blend"
    
    properties:
        BlendMode: Enum = "BlendMode"
        @slider(0.0, 1.0)
        DefaultOpacity: Float = 1.0
    
    @input_pin
    Base: Vec3
    
    @input_pin
    Blend: Vec3
    
    @output_pin
    Result: Vec3
    
    execute:
        match BlendMode:
            "Multiply" => Result = Base * Blend
            "Add" => Result = Base + Blend
            _ => Result = lerp(Base, Blend, DefaultOpacity)
```

### Generated C++ (NodeData)

```cpp
UCLASS()
class UNodeData_ColorBlendData : public UNodeDataBase {
    GENERATED_BODY()
public:
    // Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material/Blend")
    FString BlendMode = TEXT("BlendMode");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material/Blend", meta = (UIMin = "0.0", UIMax = "1.0"))
    float DefaultOpacity = 1.0f;
    
    // Pin descriptions
    virtual TArray<FNodePinDesc> GetInputPins() const override {
        TArray<FNodePinDesc> Pins;
        Pins.Add(FNodePinDesc{TEXT("Base"), EPinType::Vec3});
        Pins.Add(FNodePinDesc{TEXT("Blend"), EPinType::Vec3});
        return Pins;
    }
    
    virtual TArray<FNodePinDesc> GetOutputPins() const override {
        TArray<FNodePinDesc> Pins;
        Pins.Add(FNodePinDesc{TEXT("Result"), EPinType::Vec3});
        return Pins;
    }
    
    // Execution logic
    virtual void ExecuteNode(UGraphInstance* Instance) override {
        FVector Base = Instance->GetPinValue<FVector>(TEXT("Base"));
        FVector Blend = Instance->GetPinValue<FVector>(TEXT("Blend"));
        FVector Result;
        
        if (BlendMode == TEXT("Multiply")) {
            Result = Base * Blend;
        } else if (BlendMode == TEXT("Add")) {
            Result = Base + Blend;
        } else {
            Result = FMath::Lerp(Base, Blend, DefaultOpacity);
        }
        
        Instance->SetPinValue(TEXT("Result"), Result);
    }
};
```

### NodeData Features

- **Properties**: UPROPERTY fields with EditAnywhere, BlueprintReadWrite
- **Input Pins**: `@input_pin` → GetInputPins() override
- **Output Pins**: `@output_pin` → GetOutputPins() override
- **Execute Logic**: `execute:` block → ExecuteNode() implementation
- **Category**: Organizes nodes in editor context menu
- **Validation**: Optional ValidateNode() method

---

## 4. Pin Type System

### Supported Pin Types

| Pin Type | UE5 Type | Description | Example |
|----------|----------|-------------|---------|
| `Exec` | Execution flow | White execution pin | `@output_pin next: Exec` |
| `Bool` | `bool` | Boolean data | `@input_pin condition: Bool` |
| `Int` | `int32` | Integer data | `@input_pin count: Int` |
| `Float` | `float` | Floating point | `@input_pin alpha: Float` |
| `String` | `FString` | String data | `@input_pin text: String` |
| `Vec2` | `FVector2D` | 2D vector | `@input_pin uv: Vec2` |
| `Vec3` | `FVector` | 3D vector | `@input_pin color: Vec3` |
| `Object` | `UObject*` | Object reference | `@input_pin texture: Object` |
| `Struct` | Custom struct | Struct data | `@output_pin material: Struct` |
| `Enum` | Enum type | Enum value | `@input_pin mode: Enum` |
| `Wildcard` | `FWildcardProperty` | Any type | `@input_pin value: Wildcard` |
| `Array<T>` | `TArray<T>` | Array of type T | `@input_pin colors: Array<Vec3>` |

### Pin Type Examples

**Exec Pins** (execution flow):
```kain
@node_type
node BranchNode:
    inputs:
        InExec: Exec
    outputs:
        TrueBranch: Exec
        FalseBranch: Exec
```

**Data Pins** (typed data):
```kain
@node_type
node MathNode:
    inputs:
        A: Float = 0.0
        B: Float = 1.0
    outputs:
        Result: Float
```

**Object Pins** (UObject references):
```kain
@node_type
node TextureSampleNode:
    inputs:
        Texture: Object = "Texture2D"
        UV: Vec2 = (0.0, 0.0)
    outputs:
        RGB: Vec3
        Alpha: Float
```

**Array Pins** (collections):
```kain
@node_type
node ArrayBlendNode:
    inputs:
        Colors: Array<Vec3> = []
        Weights: Array<Float> = []
    outputs:
        Result: Vec3
```

**Wildcard Pins** (any type):
```kain
@node_type
node PassthroughNode:
    inputs:
        Input: Wildcard
    outputs:
        Output: Wildcard
```

---

## 5. Graph Instance System (`@instance`)

### Purpose
Defines runtime graph execution state and methods

### KAIN Syntax

```kain
@instance
struct DialogueInstance:
    @replicated
    current_node_id: Int
    
    @savegame
    dialogue_history: Array<Int>
    
    @transient
    debug_enabled: Bool = false
    
    @blueprint_callable
    fn start_dialogue() -> Bool:
        current_node_id = 0
        dialogue_history = []
        return true
    
    @blueprint_pure
    fn get_current_node() -> Int:
        return current_node_id
    
    @blueprint_event
    fn on_dialogue_complete():
        println("Dialogue complete!")
```

### Generated C++ (Graph Instance)

```cpp
UCLASS(BlueprintType)
class NARRATIVEGRAPH_API UDialogueInstance : public UGraphInstanceBase {
    GENERATED_BODY()
public:
    // State fields
    UPROPERTY(Replicated, BlueprintReadWrite, Category = "Dialogue")
    int32 CurrentNodeId;
    
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Dialogue")
    TArray<int32> DialogueHistory;
    
    UPROPERTY(Transient, BlueprintReadWrite, Category = "Dialogue")
    bool bDebugEnabled = false;
    
    // Replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Methods
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    bool StartDialogue();
    
    UFUNCTION(BlueprintPure, Category = "Dialogue")
    int32 GetCurrentNode() const;
    
    UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
    void OnDialogueComplete();
    virtual void OnDialogueComplete_Implementation();
    
    // Core execution methods
    void ExecuteFrom(UNodeDataBase* StartNode);
    bool EvaluatePin(FName PinName);
    void SetLocalVar(FName Name, FInstanceValue Value);
    FInstanceValue GetLocalVar(FName Name);
};
```

### Instance Features

| Feature | Attribute | Generated Code |
|---------|-----------|----------------|
| Replication | `@replicated` | `UPROPERTY(Replicated)` + `GetLifetimeReplicatedProps()` |
| Save/Load | `@savegame` | `UPROPERTY(SaveGame)` |
| Transient | `@transient` | `UPROPERTY(Transient)` |
| Blueprint Callable | `@blueprint_callable` | `UFUNCTION(BlueprintCallable)` |
| Blueprint Pure | `@blueprint_pure` | `UFUNCTION(BlueprintPure)` + `const` |
| Blueprint Event | `@blueprint_event` | `UFUNCTION(BlueprintNativeEvent)` + `_Implementation()` |

---

## 6. Binary Asset Serialization

### Purpose
Writes graph assets as binary `.uasset` files for UE5 content browser

### Features

- **Graph Topology Serialization**: Node list, connection list, pin connections
- **Node Property Values**: Serialized property data for each node
- **Engine Version Support**: UE 4.27 / 5.0+ compatibility
- **Asset Registry Format**: `AddedDependencyFlags` format support
- **Compression**: Optional compression for large graphs

### Implementation

**File**: `Kain/crates/ue5-graphs/src/binary_serializer.rs` (23KB)

**Test Coverage**: 6 test fixtures validating binary format correctness

---

## 7. Schema & Validation

### Purpose
Defines connection rules, validation rules, and context menu actions for graph editors

### KAIN Syntax

```kain
@graph_editor
graph MaterialGraph:
    @schema
    schema MaterialGraphSchema:
        connection_rules:
            rule ExecToExec:
                from: Exec
                to: Exec
                allowed: true
            
            rule ExecToNonExec:
                from: Exec
                to: Float
                allowed: false
                error: "Cannot connect execution pin to data pin"
        
        validation_rules:
            rule RequireOutputNode:
                condition: "graph.nodes.any(n => n.type == 'MaterialOutputNode')"
                message: "Material graph must have at least one output node"
            
            rule NoCycles:
                condition: "!graph.has_cycles()"
                message: "Material graph cannot contain cycles"
        
        context_actions:
            action CreateTextureNode:
                category: "Material/Texture"
                label: "Add Texture Sample"
                tooltip: "Create a new texture sample node"
```

### Generated C++ (Schema)

```cpp
UCLASS()
class UMaterialGraphSchema : public UEdGraphSchema {
    GENERATED_BODY()
public:
    // Connection validation
    virtual const FPinConnectionResponse CanCreateConnection(
        const UEdGraphPin* PinA, 
        const UEdGraphPin* PinB
    ) const override {
        // Exec to Exec allowed
        if (PinA->PinType.PinCategory == TEXT("exec") && 
            PinB->PinType.PinCategory == TEXT("exec")) {
            return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
        }
        
        // Exec to non-Exec disallowed
        if (PinA->PinType.PinCategory == TEXT("exec") && 
            PinB->PinType.PinCategory != TEXT("exec")) {
            return FPinConnectionResponse(
                CONNECT_RESPONSE_DISALLOW, 
                TEXT("Cannot connect execution pin to data pin")
            );
        }
        
        return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
    }
    
    // Context menu actions
    virtual void GetGraphContextActions(
        FGraphContextMenuBuilder& ContextMenuBuilder
    ) const override {
        // Add "Add Texture Sample" action
        TSharedPtr<FEdGraphSchemaAction_NewNode> NewNodeAction(
            new FEdGraphSchemaAction_NewNode(
                FText::FromString(TEXT("Material/Texture")),
                FText::FromString(TEXT("Add Texture Sample")),
                FText::FromString(TEXT("Create a new texture sample node")),
                0
            )
        );
        ContextMenuBuilder.AddAction(NewNodeAction);
    }
};
```

---

## Complete Feature Matrix

### Graph Runtime Features

| Feature | KAIN Syntax | Generated C++ | Factory Example |
|---------|-------------|---------------|-----------------|
| Graph definition | `@graph_runtime graph X:` | `UXInstance`, `UXAsset`, `UXGraphData` | NarrativeGraph |
| Node data | `@node_data node Y:` | `UNodeData_Y : public UNodeDataBase` | NarrativeGraph |
| Input pins | `@input_pin field: Type` | `GetInputPins()` override | NarrativeGraph |
| Output pins | `@output_pin field: Type` | `GetOutputPins()` override | NarrativeGraph |
| Execute logic | `execute: ...` | `ExecuteNode()` implementation | Example_Graph |
| Instance state | `@instance struct Z:` | `UZInstance : public UGraphInstanceBase` | NarrativeGraph |
| Replication | `@replicated field: Type` | `UPROPERTY(Replicated)` + GetLifetimeReplicatedProps | NarrativeGraph |
| Save/Load | `@savegame field: Type` | `UPROPERTY(SaveGame)` | NarrativeGraph |
| Transient state | `@transient field: Type` | `UPROPERTY(Transient)` | Example_Graph |
| Blueprint methods | `@blueprint_callable fn` | `UFUNCTION(BlueprintCallable)` | NarrativeGraph |
| Pure methods | `@blueprint_pure fn` | `UFUNCTION(BlueprintPure)` + const | NarrativeGraph |
| Event methods | `@blueprint_event fn` | `UFUNCTION(BlueprintNativeEvent)` | NarrativeGraph |

### Graph Editor Features

| Feature | KAIN Syntax | Generated C++ | Factory Example |
|---------|-------------|---------------|-----------------|
| Graph editor | `@graph_editor graph X:` | `UXSchema`, `UXEditor`, node classes | NarrativeGraph |
| Node type | `@node_type node Y:` | `UXEditorYNode : public UEdGraphNode` | NarrativeGraph |
| Category | `@category("X/Y")` | `GetMenuCategory()` returns "X\|Y" | Example_Graph |
| Display name | `@display_name("X")` | `GetNodeTitle()` returns "X" | NarrativeGraph |
| Tooltip | `@tooltip("X")` | `GetTooltipText()` returns "X" | Example_Graph |
| Color | `@color(r, g, b, a)` | `GetNodeTitleColor()` returns color | Example_Graph |
| Icon | `@icon("X")` | Node icon path | Example_Graph |
| Properties | `properties: { X: Type }` | Node property fields | NarrativeGraph |
| Input pins | `inputs: { X: Type }` | `AllocateDefaultPins()` creates inputs | NarrativeGraph |
| Output pins | `outputs: { X: Type }` | `AllocateDefaultPins()` creates outputs | NarrativeGraph |
| Schema | `@schema schema X:` | `UXSchema : public UEdGraphSchema` | Example_Graph |
| Connection rules | `connection_rules: ...` | `CanCreateConnection()` logic | Example_Graph |
| Validation rules | `validation_rules: ...` | Graph validation logic | Example_Graph |
| Context actions | `context_actions: ...` | `GetGraphContextActions()` menu items | Example_Graph |

### Pin Type Support

| Pin Type | KAIN | UE5 Type | Example Usage |
|----------|------|----------|---------------|
| Execution | `Exec` | Execution pin | `@output_pin next: Exec` |
| Boolean | `Bool` | `bool` | `@input_pin enabled: Bool = true` |
| Integer | `Int` | `int32` | `@input_pin count: Int = 0` |
| Float | `Float` | `float` | `@input_pin alpha: Float = 0.5` |
| String | `String` | `FString` | `@input_pin text: String = "Hello"` |
| Vector2D | `Vec2` | `FVector2D` | `@input_pin uv: Vec2 = (0.0, 0.0)` |
| Vector3D | `Vec3` | `FVector` | `@input_pin color: Vec3 = (1.0, 1.0, 1.0)` |
| Object | `Object` | `UObject*` | `@input_pin texture: Object = "Texture2D"` |
| Struct | `Struct` | Custom struct | `@output_pin data: Struct = "MaterialData"` |
| Enum | `Enum` | Enum type | `@input_pin mode: Enum = "BlendMode"` |
| Wildcard | `Wildcard` | Any type | `@input_pin value: Wildcard` |
| Array | `Array<T>` | `TArray<T>` | `@input_pin items: Array<Vec3> = []` |

---

## Source Files Reference

### Codegen Implementation

| File | Size | Purpose |
|------|------|---------|
| `runtime_codegen/node_data_gen.rs` | 35KB | UNodeData_* C++ classes with ExecuteNode() |
| `runtime_codegen/instance_gen.rs` | 23KB | UGraphInstance + UGraphAsset generation |
| `runtime_codegen/asset_gen.rs` | 18KB | Asset registration + content browser integration |
| `runtime_codegen/graph_data_gen.rs` | 8.6KB | UGraphData topology serialization |
| `factory_generator.rs` | 27KB | UEdGraphNode subclasses for editor |
| `binary_serializer.rs` | 23KB | Binary .uasset graph asset serialization |
| `ast_converter.rs` | 16KB | KAIN AST → IR conversion |
| `runtime_converter.rs` | 19KB | Runtime IR refinement and validation |
| `runtime_ir.rs` | 11KB | RuntimeGraph IR structs |
| `graph_ir.rs` | 7KB | GraphIR shared topology |
| `node_types.rs` | 3KB | Pin type enum + metadata |
| `schema_builder.rs` | 2KB | UEdGraphSchema subclass generation |

**Total**: ~192KB of specialized graph codegen

---

## Factory Part 1 Plugin Examples

### NarrativeGraph (464 LOC → 2,321 C++ LOC)

**Location**: `Factory/NarrativeGraph/`

**Features Used**:
- `@graph_runtime` with DialogueGraph and QuestGraph
- `@graph_editor` with visual node editors
- `@node_data` for 10+ node types (Root, NPC, Player, Branch, End, Start, Objective, Success, Failure)
- `@instance` with state management and Blueprint integration
- `@asset_editor` with viewport, details, and toolbar
- All 12 pin types demonstrated
- Replication (`@replicated`)
- Save/Load (`@savegame`)
- Blueprint integration (`@blueprint_callable`, `@blueprint_event`)

**Generated Files**:
```
NarrativeGraph/Source/NarrativeGraph/Public/
├── DialogueGraphGraphAsset.h          # UDialogueGraphGraphAsset
├── DialogueGraphGraphData.h           # Graph topology
├── DialogueGraphInstance.h            # Runtime instance
├── DialogueGraphPinData.h             # Pin connection data
├── RootNodeNodeData.h                 # Node data classes
├── NPCNodeNodeData.h
├── PlayerNodeNodeData.h
├── BranchNodeNodeData.h
└── EndNodeNodeData.h

NarrativeGraph/Source/NarrativeGraphEditor/Public/
├── DialogueGraphEditorFactory.h       # UEdGraphNode subclasses
├── QuestGraphEditorFactory.h          # UEdGraphNode subclasses
└── (Schema and editor integration)
```

**Compression Ratio**: 1:5 (464 KAIN → 2,321 C++)

### TitanGraph (1,692 LOC → 10,000+ C++ LOC)

**Location**: `Factory/TitanGraph/`

**Features Used**:
- Complex quest and dialogue system
- 8+ node types (Quest Start, Objective, Dialogue, Condition, Action, Branch, Delay, Parallel)
- DataTable integration (`@datatable`)
- Component system (`@component`)
- Actor system with RPCs (Server_, Client_, Multicast_)
- Replication and networking
- Slate UI for graph canvas and inspector
- Details panels for node properties
- Blueprint function libraries

**Key Patterns**:
```kain
# Quest tracking component
@component
struct QuestTrackerComponent:
    active_quests: Array<ActiveQuest>
    @savegame
    completed_quest_ids: Array<Int>
    @transient
    is_in_dialogue: Bool

# Quest manager actor with RPCs
actor QuestManager:
    @replicated
    state total_quests_completed: Int = 0
    
    on Server_StartQuest(quest_id: Int):
        # Quest start logic
        Multicast_AnnounceQuestStart(quest_id)
    
    on Multicast_AnnounceQuestStart(quest_id: Int):
        println("Quest {quest_id} started!")
```

**Compression Ratio**: 1:6 (1,692 KAIN → 10,000+ C++)

### Example_Graph (Complete Feature Showcase)

**Location**: `Factory/Example_Graph/graph.kn`

**Features Used**:
- 20+ node types demonstrating ALL pin types
- Material graph theme (texture sampling, blending, PBR, post-processing)
- All node attributes (`@category`, `@color`, `@icon`, `@tooltip`, `@execution_logic`)
- Schema with connection rules, validation rules, context actions
- Graph properties (`@allow_cycles`, `@grid_snap`)
- Advanced nodes (Custom HLSL, Noise Generator, Gradient)
- Array processing nodes
- Wildcard pin nodes
- Conditional branching nodes

**Node Type Examples**:
```kain
# Texture sampler with Object pin
@node_type
@category("Material/Texture")
@color(0.8, 0.4, 0.2, 1.0)
@tooltip("Sample a 2D texture at UV coordinates")
node TextureSampleNode:
    inputs:
        Execute: Exec
        Texture: Object = "Texture2D"
        UV: Vec2 = (0.0, 0.0)
    outputs:
        Execute: Exec
        RGB: Vec3
        Alpha: Float

# Array blend with array pins
@node_type
@category("Material/Array")
node ArrayBlendNode:
    inputs:
        Execute: Exec
        Colors: Array<Vec3> = []
        Weights: Array<Float> = []
    outputs:
        Execute: Exec
        Result: Vec3

# Wildcard passthrough
@node_type
@category("Material/Utility")
node PassthroughNode:
    inputs:
        Execute: Exec
        Input: Wildcard
    outputs:
        Execute: Exec
        Output: Wildcard
```

---

## Advanced Patterns

### Pattern 1: State Machine Graph

```kain
@graph_runtime
graph StateMachine:
    @node_data
    node StateNode:
        @property
        state_name: String
        @property
        duration: Float
        @input_pin
        enter: Exec
        @output_pin
        exit: Exec
        @output_pin
        on_complete: Exec
        
        execute:
            # State logic here
            wait(duration)
            return on_complete
    
    @instance
    struct StateMachineInstance:
        @replicated
        current_state: String
        @transient
        state_timer: Float
        
        @blueprint_callable
        fn transition_to(state_name: String) -> Bool:
            current_state = state_name
            state_timer = 0.0
            return true
```

### Pattern 2: Behavior Tree Graph

```kain
@graph_runtime
graph BehaviorTree:
    @node_data
    node SelectorNode:
        @input_pin
        in_exec: Exec
        @output_pin
        child_1: Exec
        @output_pin
        child_2: Exec
        @output_pin
        child_3: Exec
        
        execute:
            # Try children in order until one succeeds
            if evaluate(child_1):
                return child_1
            if evaluate(child_2):
                return child_2
            return child_3
    
    @node_data
    node SequenceNode:
        @input_pin
        in_exec: Exec
        @output_pin
        child_1: Exec
        @output_pin
        child_2: Exec
        
        execute:
            # Execute all children in sequence
            execute(child_1)
            execute(child_2)
            return null
```

### Pattern 3: Visual Scripting Graph

```kain
@graph_editor
graph VisualScript:
    @node_type
    @category("Variables")
    node GetVariableNode:
        properties:
            VariableName: String = "MyVar"
        outputs:
            Value: Wildcard
    
    @node_type
    @category("Variables")
    node SetVariableNode:
        properties:
            VariableName: String = "MyVar"
        inputs:
            Execute: Exec
            Value: Wildcard
        outputs:
            Execute: Exec
    
    @node_type
    @category("Flow")
    node ForLoopNode:
        inputs:
            Execute: Exec
            StartIndex: Int = 0
            EndIndex: Int = 10
        outputs:
            LoopBody: Exec
            Index: Int
            Completed: Exec
```

---

## Integration with Other Systems

### With Actor System

```kain
actor GraphExecutor:
    @component
    state graph_instance: DialogueGraphInstance = DialogueGraphInstance{}
    
    on Server_ExecuteGraph(graph_id: Int):
        graph_instance.execute_graph()
        Multicast_NotifyGraphComplete(graph_id)
    
    on Multicast_NotifyGraphComplete(graph_id: Int):
        println("Graph {graph_id} completed")
```

### With Subsystem

```kain
@subsystem
@tick
struct GraphManagerSubsystem:
    active_graphs: Array<GraphInstance>
    
    fn on_tick(delta: Float):
        # Update all active graph instances
        var i: Int = 0
        while i < active_graphs.length():
            active_graphs[i].tick(delta)
            i = i + 1
    
    @blueprint_callable
    fn register_graph(instance: GraphInstance):
        active_graphs.push(instance)
```

### With Blueprint Integration

```kain
@blueprint
fn execute_dialogue_graph(graph_asset: DialogueGraphAsset, starting_node: Int) -> Bool:
    let instance = graph_asset.CreateInstance()
    if instance != null:
        instance.start_dialogue()
        return true
    return false

@blueprint
fn get_graph_output_value(instance: GraphInstance, pin_name: String) -> Wildcard:
    return instance.GetPinValue(pin_name)
```

### With Save/Load System

```kain
@graph_runtime
graph SaveableGraph:
    @instance
    struct SaveableInstance:
        @savegame
        current_node_id: Int
        
        @savegame
        visited_nodes: Array<Int>
        
        @savegame
        local_variables: Array<String>
        
        @blueprint_callable
        fn save_state() -> Bool:
            # State automatically saved via @savegame
            return true
        
        @blueprint_callable
        fn load_state() -> Bool:
            # State automatically loaded via @savegame
            return true
```

---

## Best Practices

### 1. Node Design

**DO**:
- Keep nodes focused on single responsibility
- Use descriptive names for pins and properties
- Provide default values for all inputs
- Add tooltips and categories for discoverability
- Use appropriate pin types (don't overuse Wildcard)

**DON'T**:
- Create nodes with too many pins (>8 inputs/outputs)
- Use generic names like "Node1", "Input", "Output"
- Forget to validate node data in ExecuteNode()
- Mix execution flow and data flow in confusing ways

### 2. Graph Structure

**DO**:
- Start with a clear root/entry node
- Have explicit terminal/exit nodes
- Use categories to organize node types
- Implement validation rules to catch errors early
- Document expected graph topology in schema

**DON'T**:
- Allow cycles unless explicitly needed (state machines)
- Create graphs with no clear entry/exit points
- Mix different graph types (dialogue + quest in same graph)
- Forget to validate graph integrity before execution

### 3. Instance Management

**DO**:
- Use `@replicated` for multiplayer-critical state
- Use `@savegame` for persistent state
- Use `@transient` for runtime-only state
- Provide Blueprint-callable methods for common operations
- Implement proper cleanup in instance destructors

**DON'T**:
- Replicate everything (bandwidth cost)
- Store large data structures in instance state
- Forget to reset state when reusing instances
- Expose internal implementation details to Blueprint

### 4. Performance

**DO**:
- Cache frequently accessed node data
- Use execution flow pins to control evaluation order
- Implement early-exit conditions in complex nodes
- Profile graph execution in shipping builds
- Consider parallel execution for independent branches

**DON'T**:
- Execute entire graph every frame
- Perform expensive operations in ExecuteNode() without caching
- Create deep recursion in graph execution
- Allocate memory in hot paths

---

## Limitations & Known Issues

### Current Limitations

1. **No Nested Graphs**: Graphs cannot contain sub-graphs (workaround: use separate graph assets)
2. **No Dynamic Pin Creation**: Pin count must be known at compile time
3. **Limited Type Inference**: Wildcard pins require explicit type casting
4. **No Visual Debugging**: No built-in graph execution visualization (must implement custom)
5. **Single-Threaded Execution**: Graph execution is sequential (parallel execution planned)

### Workarounds

**Nested Graphs**:
```kain
@node_data
node SubGraphNode:
    @property
    sub_graph_asset: Object = "GraphAsset"
    
    execute:
        let instance = sub_graph_asset.CreateInstance()
        instance.execute_graph()
```

**Dynamic Pins** (use arrays):
```kain
@node_type
node DynamicInputNode:
    inputs:
        Inputs: Array<Wildcard> = []
    outputs:
        Result: Wildcard
```

**Visual Debugging** (custom implementation):
```kain
@instance
struct DebugGraphInstance:
    @transient
    execution_log: Array<String>
    
    fn log_node_execution(node_name: String):
        execution_log.push(node_name)
```

---

## Testing & Validation

### Unit Tests

**Location**: `Kain/crates/ue5-graphs/tests/`

**Coverage**:
- Node data generation (35KB test file)
- Graph instance generation (23KB test file)
- Pin type validation (node_types.rs tests)
- Binary serialization (6 test fixtures)
- Schema validation (schema_builder.rs tests)

### Integration Tests

**Factory Plugins**:
- NarrativeGraph: 464 LOC → 2,321 C++ (compiles successfully)
- TitanGraph: 1,692 LOC → 10,000+ C++ (compiles successfully)
- Example_Graph: Complete feature showcase (compiles successfully)

### Validation Checklist

- [ ] All node types have valid pin configurations
- [ ] Graph has at least one entry node
- [ ] Graph has at least one exit node (if required)
- [ ] No disconnected nodes (unless intentional)
- [ ] No cycles (unless explicitly allowed)
- [ ] All required properties have default values
- [ ] Pin types are compatible for connections
- [ ] Schema validation rules pass
- [ ] Binary asset serialization succeeds
- [ ] Generated C++ compiles without errors

---

## Future Enhancements

### Planned Features

1. **Parallel Execution**: Execute independent graph branches in parallel
2. **Visual Debugging**: Built-in graph execution visualization
3. **Hot Reload**: Reload graph assets without restarting editor
4. **Graph Diffing**: Compare graph versions for source control
5. **Performance Profiling**: Built-in profiling for graph execution
6. **Nested Graphs**: Support for sub-graphs and graph composition
7. **Dynamic Pins**: Runtime pin creation/removal
8. **Type Inference**: Automatic type inference for Wildcard pins
9. **Graph Templates**: Reusable graph patterns
10. **Visual Scripting**: Full visual scripting system like Blueprint

### Roadmap

- **Q2 2026**: Parallel execution, visual debugging
- **Q3 2026**: Hot reload, graph diffing
- **Q4 2026**: Nested graphs, dynamic pins
- **Q1 2027**: Full visual scripting system

---

## Summary

The `ue5-graphs` crate provides a complete graph editor and runtime system for UE5, enabling:

- **Visual Node Editors**: UEdGraph-based editors with custom nodes, pins, and schemas
- **Runtime Execution**: Graph instance execution with state management and Blueprint integration
- **12 Pin Types**: Exec, Bool, Int, Float, String, Vec2, Vec3, Object, Struct, Enum, Wildcard, Array
- **Binary Assets**: Direct `.uasset` serialization for content browser integration
- **Replication & Save/Load**: Full networking and persistence support
- **Blueprint Integration**: Callable methods, pure functions, and events

**Proven Results**:
- NarrativeGraph: 464 LOC → 2,321 C++ (1:5 compression)
- TitanGraph: 1,692 LOC → 10,000+ C++ (1:6 compression)
- Example_Graph: Complete feature showcase with 20+ node types

**Use Cases**:
- Dialogue systems
- Quest systems
- State machines
- Behavior trees
- Material graphs
- Visual scripting
- Custom game logic editors

The system is production-ready and battle-tested across multiple Factory Part 1 plugins.
