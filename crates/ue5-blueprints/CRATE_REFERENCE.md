# ue5-blueprints — Blueprint Binary Asset Reference

> **Last Updated:** 2026-03-01
> **Status:** Phase 2 complete for simple Blueprints (UDataAsset, simple event graphs). Complex Kismet bytecode for arbitrary logic still falls back to C++ factory generation.

---

## Purpose

Generates UE5 Blueprint assets from KAIN Blueprint constructs. Produces binary `.uasset` Blueprint files and C++ `UK2Node` custom node subclasses.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `writer.rs` | 35.5KB | `BlueprintBinaryWriter` — binary `.uasset` Blueprint serialization |
| `factory.rs` | 15.4KB | `UK2Node` C++ codegen for custom Blueprint nodes |
| `conversion.rs` | 10.8KB | KAIN Blueprint node IR → UE5 Blueprint pin/connection conversion |
| `kismet.rs` | 13.6KB | Kismet bytecode instruction emitter |
| `ir.rs` | 7.4KB | Blueprint node IR structs |

---

## Public API (`lib.rs`, 6.1KB)

```rust
pub fn generate_blueprint(program: &TypedProgram) -> KainResult<BlueprintOutput>
pub fn generate_k2node(program: &TypedProgram) -> KainResult<String>  // C++ UK2Node

pub struct BlueprintOutput {
    pub assets: Vec<(String, Vec<u8>)>,   // (filename, binary .uasset bytes)
    pub cpp_files: Vec<(String, String)>, // C++ factory fallback files
}
```

---

## Blueprint Binary Writer (`writer.rs`, 35.5KB)

Writes UE5 Blueprint `.uasset` binary format:

### Phase 1 — Simple Blueprints (complete)
- `UDataAsset` subclasses
- Expose Blueprint-callable functions
- Simple property set (bool, int, float, string, enum, object)
- Supported property types: Bool, Int, Float, String, Name, Text, Enum, Object, Struct, SoftObject, SoftClass, Array

### Phase 2 — Event Graphs (complete for simple graphs)
- `BeginPlay` / `EndPlay` event nodes
- Function call nodes with direct output wiring
- Variable get/set nodes
- Branch (if/else) nodes

### Kismet Bytecode (`kismet.rs`, 13.6KB)

For event graphs, generates Kismet VM bytecode instructions:

| Instruction | KAIN origin |
|---|---|
| `EX_CallMath` | Math function call |
| `EX_LocalVariable` | Variable read |
| `EX_InstanceVariable` | Field access |
| `EX_LocalOutVariable` | Output from function |
| `EX_True` / `EX_False` | Boolean constants |
| `EX_IntConst` / `EX_FloatConst` / `EX_StringConst` | Literal values |
| `EX_Jump` / `EX_JumpIfNot` | Conditional flow |
| `EX_Return` | Function return |
| `EX_EndOfScript` | Block terminator |

**Limitation:** Complex event graphs (arbitrary branching, loops, async nodes) still fall back to C++ factory generation rather than Kismet bytecode.

---

## `UK2Node` C++ Codegen (`factory.rs`, 15.4KB)

Generates `UK2Node` subclasses for custom Blueprint nodes:

```cpp
UCLASS()
class UMyCustomNode : public UK2Node {
    GENERATED_BODY()
public:
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetMenuCategory() const override;
    virtual void ExpandNode(FKismetCompilerContext& CompilerContext, ...) override;
};
```

Supports async nodes via `UK2Node_AsyncAction` base class when `@async` is annotated on the KAIN function.

---

## KAIN Blueprint Syntax

```kain
@blueprint
fn calculate_damage(base: Float, multiplier: Float, armor: Float) -> Float:
    let raw = base * multiplier
    return max(raw * (1.0 - armor / 100.0), 0.0)
```
→ `static UFUNCTION(BlueprintCallable)` in `UBlueprintFunctionLibrary`.

```kain
actor GameMode:
    @blueprint_event
    fn on_player_joined(player: Actor):
        println("Player joined!")
```
→ `UFUNCTION(BlueprintNativeEvent)` + `on_player_joined_Implementation()`.

---

## Known Gaps

| Gap | Impact |
|---|---|
| Complex event graph Kismet codegen | Arbitrary Blueprint logic falls back to C++ factory |
| No async Blueprint Task UI | `UK2Node_AsyncAction` generated but no progress pin |
| No Blueprint interface codegen | `UInterface` assets not generated |
