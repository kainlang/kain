# ue5-graphs — Graph Editor & Runtime Codegen Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — both graph runtime (for in-game logic graphs) and graph editor (for UEdGraph-based editing) implemented.

---

## Purpose

Generates two categories of graph-related UE5 C++ code:

1. **Graph Runtime** — `UEdGraph`-compatible runtime graphs with node data, instance execution, and asset management (for in-game use)
2. **Graph Editor** — `UEdGraphNode` subclasses for custom KAIN graph editors with pin types, schema, and factory registration

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `runtime_codegen/node_data_gen.rs` | 35KB | `UNodeData_*` C++ classes — runtime node implementations |
| `runtime_codegen/instance_gen.rs` | 23KB | `UGraphInstance` + `UGraphAsset` (asset container) |
| `runtime_codegen/asset_gen.rs` | 18KB | Asset registration + content browser integration |
| `runtime_codegen/graph_data_gen.rs` | 8.6KB | `UGraphData` — graph topology serialization |
| `factory_generator.rs` | 27KB | `UEdGraphNode` subclasses for editor-facing graph |
| `binary_serializer.rs` | 23KB | Binary `.uasset` graph asset serialization |
| `ast_converter.rs` | 16KB | KAIN `@graph_runtime` / `@graph_editor` AST → IR |
| `runtime_converter.rs` | 19KB | Runtime IR refinement and validation |
| `runtime_ir.rs` | 11KB | `RuntimeGraph` IR structs |
| `graph_ir.rs` | 7KB | `GraphIR` — shared graph topology IR |
| `node_types.rs` | 3KB | Pin type enum + metadata |
| `schema_builder.rs` | 2KB | `UEdGraphSchema` subclass generation |

---

## KAIN Syntax

### Graph Runtime (`@graph_runtime`)

For in-game logic graphs:

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
    
    @node_data
    node ChoiceNode:
        choices: Array<String> = []
        @input_pin
        in_exec: Exec
        @output_pin
        choice_selected: Exec
```

Generates:
- `UNodeData_SpeakerNode : public UNodeDataBase` with `ExecuteNode(UGraphInstance*)` override
- `UNodeData_ChoiceNode : public UNodeDataBase` with execution logic
- `UDialogueSystemInstance : public UGraphInstanceBase` — runtime execution engine
- `UDialogueSystemAsset : public UGraphAssetBase` — content browser asset with `CreateInstance()` / `ValidateGraph()`

### Graph Editor (`@graph_editor`)

For custom UEdGraph-based editors:

```kain
@graph_editor
graph DialogueGraph:
    @node_type
    node NPCNode:
        properties:
            SpeakerName: String = "NPC"
        inputs:
            InExec: Exec
        outputs:
            Next: Exec
```

Generates:
- `UDialogueGraph_NPCNode : public UEdGraphNode` with:
  - `AllocateDefaultPins()` — creates InExec (input exec pin) and Next (output exec pin)
  - `GetNodeTitle(ENodeTitleType::Type)` — returns `"NPC Node"`
  - `GetMenuCategory()` — returns `"Dialogue"`
- `UDialogueGraphSchema : public UEdGraphSchema` — connection rules, node spawn menu
- `FDialogueGraphFactory` — `FGraphPanelNodeFactory` for rendering

---

## Pin Types (`node_types.rs`)

| Pin type | UE5 type |
|---|---|
| `Exec` | Execution flow pin |
| `Bool` | `bool` data pin |
| `Int` | `int32` data pin |
| `Float` | `float` data pin |
| `String` | `FString` data pin |
| `Object` | `UObject*` reference pin |
| `Struct` | Struct data pin |
| `Enum` | Enum data pin |
| `Wildcard` | `FWildcardProperty` — accepts any type |
| `Array` | `TArray<T>` data pin |

---

## Binary Serializer (`binary_serializer.rs`, 23KB)

Writes graph assets as binary `.uasset` files:
- Graph topology serialization (node list, connection list)
- Node property values
- Pin connection records
- Engine version parameterization (UE5 4.27 / 5.0+)

`AddedDependencyFlags` format support for UE 4.27 and 5.0+ asset registries. Tested with 6 test fixtures.

---

## Runtime Node Data (`runtime_codegen/node_data_gen.rs`, 35KB)

Each `@node_data node X` in a `@graph_runtime` generates:

```cpp
UCLASS()
class UNodeData_X : public UNodeDataBase {
    GENERATED_BODY()
public:
    // Properties from KAIN node fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SpeakerName = TEXT("NPC");
    
    // Pin descriptions
    virtual TArray<FNodePinDesc> GetInputPins() const override;
    virtual TArray<FNodePinDesc> GetOutputPins() const override;
    
    // Execution
    virtual void ExecuteNode(UGraphInstance* Instance) override;
};
```

---

## Graph Instance (`runtime_codegen/instance_gen.rs`, 23KB)

`UGraphInstance` runtime execution engine:
- `void ExecuteFrom(UNodeDataBase* StartNode)` — drives execution forward
- `bool EvaluatePin(FName PinName)` — conditional pin evaluation
- `void SetLocalVar(FName Name, FInstanceValue Value)` — local variable store
- `FInstanceValue GetLocalVar(FName Name)` — read local variable

`UGraphAsset`:
- `UGraphInstance* CreateInstance(UObject* Outer)` — factory method
- `bool ValidateGraph(TArray<FString>& Errors)` — graph validity check
- `FGraphTopology Topology` — serialized node/connection graph
