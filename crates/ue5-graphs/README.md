# ue5-graphs

UE5 graph editor codegen for KAIN compiler.

## Overview

This crate generates UE5 graph editors (UEdGraph, UEdGraphNode, UEdGraphSchema) from KAIN source code. It follows the same pattern as `ue5-materials`:

```
KAIN AST → Graph IR → Binary .uasset + C++ Factory
```

## Architecture

```
src/
├── lib.rs                  # Public API
├── error.rs                # Error types
├── graph_ir.rs             # Intermediate representation (COMPLETE)
├── ast_converter.rs        # AST → IR conversion (TODO)
├── factory_generator.rs    # IR → C++ code (TODO)
├── binary_serializer.rs    # IR → .uasset (TODO)
├── node_types.rs           # Built-in node types (COMPLETE)
└── schema_builder.rs       # Schema builder (COMPLETE)
```

## Status

- ✅ **Scaffolding Complete** - Crate structure, IR types, error handling
- ⏳ **AST Converter** - Needs implementation
- ⏳ **Factory Generator** - Needs implementation
- ⏳ **Binary Serializer** - Needs implementation

## Reference Patterns

The `referencepatterns/` folder contains extracted C++ code from production graph editors:

- **BaconCombatGraph** - Combat behavior graph (best reference)
- **LogicNodeGraph** - State machine graph
- **VoxelPluginPro** - Voxel graph
- **PaperZD** - 2D animation graph
- **NarrativeNodeGraph** - Dialogue/quest graphs

## Implementation Guide

### Phase 1: AST Converter (Week 1)

**Goal:** Convert KAIN AST to Graph IR

**Files to implement:**
- `src/ast_converter.rs`

**Reference:**
- `kain/crates/ue5-materials/src/ast_converter.rs` (similar pattern)
- `kain/crates/kain-core/src/ast.rs` (AST definitions)

**Tasks:**
1. Parse `@graph_editor` attribute
2. Convert node type definitions
3. Convert pin definitions
4. Convert schema rules
5. Validate IR

### Phase 2: Factory Generator (Week 2)

**Goal:** Generate C++ .h/.cpp files

**Files to implement:**
- `src/factory_generator.rs`

**Reference:**
- `referencepatterns/BaconCombatGraph/` (production code)
- `kain/crates/ue5-editor/src/editor/slate.rs` (similar codegen)

**Tasks:**
1. Generate UEdGraphNode subclasses
2. Generate UEdGraphSchema subclass
3. Generate UEdGraph subclass
4. Generate node registration code
5. Generate schema validation code

### Phase 3: Binary Serializer (Week 3)

**Goal:** Generate binary .uasset files

**Files to implement:**
- `src/binary_serializer.rs`

**Reference:**
- `kain/crates/ue5-materials/src/material_serializer.rs` (similar pattern)
- `kain/crates/unreal/unreal_asset/` (binary format library)

**Tasks:**
1. Create asset structure
2. Add graph imports
3. Create graph export
4. Add node type exports
5. Add schema exports
6. Serialize to bytes

## Usage Example

```rust
use ue5_graphs::generate_graph_editor;

// Generate from KAIN AST
let output = generate_graph_editor(&graph_def, "MyPlugin")?;

// Write outputs
std::fs::write("MyGraph.uasset", &output.uasset)?;
std::fs::write("MyGraph.h", &output.header)?;
std::fs::write("MyGraph.cpp", &output.source)?;
```

## Testing

```bash
# Run tests
cargo test --package ue5-graphs

# Run with output
cargo test --package ue5-graphs -- --nocapture
```

## Dependencies

- `kain-core` - AST and type definitions
- `unreal_asset` - Binary .uasset format
- `serde` - Serialization
- `thiserror` - Error handling

## Next Steps

1. Implement AST Converter
2. Implement Factory Generator
3. Implement Binary Serializer
4. Add integration tests
5. Test with real UE5 project
