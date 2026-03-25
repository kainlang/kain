# ue5-graphs Implementation Plan

## Status: Scaffolding Complete ✅

The crate structure is ready for implementation. All stub files compile successfully.

## What's Done

- ✅ Crate structure (`Cargo.toml`, `lib.rs`)
- ✅ Error types (`error.rs`)
- ✅ IR types (`graph_ir.rs`) - Complete with all data structures
- ✅ Node types (`node_types.rs`) - Built-in node helpers
- ✅ Schema builder (`schema_builder.rs`) - Schema construction API
- ✅ Reference patterns copied to `referencepatterns/`
- ✅ Crate compiles without errors

## What Needs Implementation

### 1. AST Converter (`src/ast_converter.rs`)

**Complexity:** Medium  
**Estimated Time:** 3-5 days  
**Dependencies:** Needs `GraphEditorDef` AST node in `kain-core`

**Tasks:**
- [ ] Parse `@graph_editor` attribute from AST
- [ ] Convert node type definitions to IR
- [ ] Convert pin definitions to IR
- [ ] Convert schema rules to IR
- [ ] Validate converted IR
- [ ] Add comprehensive tests

**Reference Files:**
- `kain/crates/ue5-materials/src/ast_converter.rs` (similar pattern)
- `kain/crates/kain-core/src/ast.rs` (AST definitions)

### 2. Factory Generator (`src/factory_generator.rs`)

**Complexity:** High  
**Estimated Time:** 5-7 days  
**Dependencies:** None (uses IR)

**Tasks:**
- [ ] Generate UEdGraphNode subclass headers
- [ ] Generate UEdGraphNode subclass implementations
- [ ] Generate UEdGraphSchema subclass
- [ ] Generate UEdGraph subclass
- [ ] Generate node registration code
- [ ] Generate schema validation code
- [ ] Add template system for code generation
- [ ] Add comprehensive tests

**Reference Files:**
- `referencepatterns/BaconCombatGraph/Source/ComboGraphEditor/Private/Graph/Node/*.cpp`
- `referencepatterns/BaconCombatGraph/Source/ComboGraphEditor/Private/Graph/Node/*.h`
- `kain/crates/ue5-editor/src/editor/slate.rs` (similar codegen patterns)

### 3. Binary Serializer (`src/binary_serializer.rs`)

**Complexity:** Very High  
**Estimated Time:** 7-10 days  
**Dependencies:** `unreal_asset` crate

**Tasks:**
- [ ] Create asset structure
- [ ] Add graph imports (UEdGraph, UEdGraphNode, etc.)
- [ ] Create graph export
- [ ] Add node type exports
- [ ] Add schema exports
- [ ] Serialize to binary bytes
- [ ] Test with UE5 (can it open the .uasset?)
- [ ] Add comprehensive tests

**Reference Files:**
- `kain/crates/ue5-materials/src/material_serializer.rs` (similar pattern)
- `kain/crates/unreal/unreal_asset/` (binary format library)

## Implementation Strategy

### Phase 1: AST Extensions (Prerequisite)

Before implementing the converter, we need to add AST nodes to `kain-core`:

```rust
// In kain-core/src/ast.rs
pub enum Item {
    // ... existing items ...
    GraphEditor(GraphEditorDef),
}

pub struct GraphEditorDef {
    pub name: String,
    pub attributes: Vec<Attribute>,
    pub node_types: Vec<NodeTypeDef>,
    pub schema: Option<GraphSchemaDef>,
    pub span: Span,
}

pub struct NodeTypeDef {
    pub name: String,
    pub category: Option<String>,
    pub inputs: Vec<PinDef>,
    pub outputs: Vec<PinDef>,
    pub properties: Vec<PropertyDef>,
    pub span: Span,
}

pub struct PinDef {
    pub name: String,
    pub ty: Type,
    pub is_array: bool,
    pub default: Option<Expr>,
    pub span: Span,
}
```

### Phase 2: Parser Extensions

Add parser logic for `@graph_editor`:

```rust
// In kain-core/src/parser.rs
fn parse_item(&mut self) -> Result<Item> {
    let attributes = self.parse_attributes()?;
    
    if attributes.iter().any(|a| a.name == "graph_editor") {
        return self.parse_graph_editor(attributes);
    }
    
    // ... existing logic ...
}
```

### Phase 3: Implement Modules

1. **Start with AST Converter** - Foundation for everything else
2. **Then Factory Generator** - Can test C++ output immediately
3. **Finally Binary Serializer** - Most complex, test last

### Phase 4: Integration

Add to packager dispatch:

```rust
// In cli/src/packager/codegen.rs
match item {
    TypedItem::GraphEditor(graph_def) => {
        let output = ue5_graphs::generate_graph_editor(graph_def, plugin_name)?;
        // Write files...
    }
    // ... existing cases ...
}
```

## Delegation Strategy

### Option A: Single Agent (Sequential)

One agent implements all three modules sequentially:
- Week 1: AST Converter
- Week 2: Factory Generator  
- Week 3: Binary Serializer

**Pros:** Consistent code style, single context  
**Cons:** Slower, single point of failure

### Option B: Three Agents (Parallel)

Three specialized agents work in parallel:
- Agent 1: AST Converter (3-5 days)
- Agent 2: Factory Generator (5-7 days)
- Agent 3: Binary Serializer (7-10 days)

**Pros:** Faster, specialized expertise  
**Cons:** Need coordination, potential style inconsistencies

### Recommended: Hybrid Approach

1. **Agent 1** implements AST Converter + AST extensions (Week 1)
2. **Agent 2 & 3** work in parallel on Factory Generator and Binary Serializer (Week 2-3)
3. **Integration Agent** ties everything together and tests (Week 4)

## Testing Strategy

### Unit Tests

Each module should have comprehensive unit tests:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_convert_simple_graph() {
        // Test AST → IR conversion
    }

    #[test]
    fn test_generate_node_class() {
        // Test C++ generation
    }

    #[test]
    fn test_serialize_graph() {
        // Test binary serialization
    }
}
```

### Integration Tests

Create `tests/` folder with end-to-end tests:

```rust
// tests/integration_test.rs
#[test]
fn test_full_pipeline() {
    // KAIN source → AST → IR → C++ + Binary
    // Verify all outputs
}
```

### UE5 Validation

Final test: Can UE5 open the generated .uasset?

1. Generate graph editor
2. Copy to UE5 project
3. Open in UE5 editor
4. Verify nodes appear in context menu
5. Verify connections work

## Success Criteria

### Minimum Viable Product (MVP)

- [ ] Can parse `@graph_editor` from KAIN source
- [ ] Can convert to IR
- [ ] Can generate C++ factory code
- [ ] Generated C++ compiles in UE5
- [ ] Can generate binary .uasset
- [ ] UE5 can open the .uasset
- [ ] Nodes appear in context menu
- [ ] Basic connections work

### Full Feature Set

- [ ] Supports all pin types (Exec, Bool, Int, Float, String, Object, Struct)
- [ ] Supports array pins
- [ ] Supports default values
- [ ] Supports node categories
- [ ] Supports node colors/icons
- [ ] Supports schema validation rules
- [ ] Supports context menu actions
- [ ] Has comprehensive tests
- [ ] Has documentation

## Next Steps

1. ✅ Scaffolding complete
2. ⏭️ Add AST nodes to `kain-core`
3. ⏭️ Add parser logic for `@graph_editor`
4. ⏭️ Implement AST Converter
5. ⏭️ Implement Factory Generator
6. ⏭️ Implement Binary Serializer
7. ⏭️ Integration testing
8. ⏭️ UE5 validation

## Agent Delegation

Ready to delegate implementation to specialized agents. Each agent will have:

- ✅ Complete scaffolding
- ✅ IR types defined
- ✅ Error handling ready
- ✅ Reference patterns available
- ✅ Clear implementation tasks
- ✅ Test structure ready

**Agents can start immediately on their assigned modules.**
