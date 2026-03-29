# Materialize — Quick Reference

## File Structure

```
src/
├── types.kn                (300 lines)  - Data structures
├── shaders.kn              (1200 lines) - 15 compute shaders [EXISTS]
├── layer_evaluator.kn      (400 lines)  - Layer compositor
├── graph_runtime.kn        (500 lines)  - Graph executor
├── graph_editor.kn         (400 lines)  - Graph UI
├── editor_viewport.kn      (200 lines)  - 3D preview
├── editor_main.kn          (800 lines)  - Main editor window
├── editor_batch.kn         (400 lines)  - Batch processor
├── asset_generator.kn      (300 lines)  - Material generation
├── material_loader.kn      (200 lines)  - Material loading
├── notifications.kn        (150 lines)  - Toast system
└── toolbar.kn              (100 lines)  - Toolbar integration

Total: 5,050 lines (3,750 new + 1,300 existing shaders)
```

## Key Components

### Editor UI
- **SMaterializeEditor** — Main window with dual workflow (Layer/Graph)
- **SMaterializeEditorViewport** — 3D preview with environment maps
- **SMaterializeBatchWindow** — Batch processing queue
- **SMaterializeNodePalette** — Graph node library

### Workflow Systems
- **UKLayerEvaluator** — GPU layer compositor (6 types, 12 blend modes)
- **FMaterializeGraphExecutor** — Node graph runner (cycle detection, caching)
- **UMaterializeBatchProcessor** — Multi-texture batch operations

### Asset Pipeline
- **FMaterializeAssetGenerator** — Creates 4 materials + 8 functions on startup
- **FMaterializeMaterialLoader** — Loads materials with fallback chain
- **FMaterializeTransientGenerator** — Runtime material creation

## Implementation Order

1. types.kn (1 day)
2. asset_generator.kn + material_loader.kn (3 days)
3. layer_evaluator.kn (3 days)
4. graph_runtime.kn + graph_editor.kn (5 days)
5. editor_viewport.kn (2 days)
6. editor_main.kn (5 days)
7. editor_batch.kn (3 days)
8. notifications.kn + toolbar.kn (1 day)
9. Integration testing (2 days)

**Total: 25 days**

## Critical Patterns

### Texture Validation
```kain
if has_flag(texture, RF_NeedLoad) or has_flag(texture, RF_NeedPostLoad):
    return false
texture.wait_for_streaming()
flush_rendering_commands()
```

### Debounced Preview
```kain
preview_debounce_requested = true
register_active_timer(0.15, lambda() -> TimerReturnType:
    if preview_debounce_requested:
        preview_debounce_requested = false
        return TimerReturnType::Continue
    on_generate_preview()
    return TimerReturnType::Stop
)
```

### Base Layer Auto-Creation
```kain
if not has_base_layer(stack):
    let base = Layer(type: LayerType::Base, name: "Base Pass")
    stack.layers.insert(0, base)
```

### Cycle Detection
```kain
fn detect_cycles_dfs(node, visited, recursion_stack) -> Bool:
    visited.add(node.id)
    recursion_stack.add(node.id)
    for connected_node in get_connected_nodes(node):
        if recursion_stack.contains(connected_node.id):
            return true  # Cycle!
    recursion_stack.remove(node.id)
    return false
```

## Build Commands

```bash
# Build plugin
kain build --ue5

# Dry run
kain build --ue5 --dry-run

# Analyze shaders
kain build src/shaders.kn --target usf --analyze

# Verbose output
kain build --ue5 --verbose
```
