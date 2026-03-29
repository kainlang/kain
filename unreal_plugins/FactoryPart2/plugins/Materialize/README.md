# Materialize - KAIN Rebuild

**Version:** 2.0.0  
**Original Plugin:** Research/UEProj/Project_5.4/Plugins/Materialize  
**Status:** 🚧 In Development - Research Phase

## Overview

Materialize is a Substance Sampler alternative for UE5 that generates complete PBR materials from a single image. This is a complete rebuild in KAIN to leverage:

- **Material Asset Pipeline** - Binary .uasset generation for materials
- **Streamlined Shaders** - KAIN shader codegen for all 20+ .usf files
- **Graph Editor** - Node-based layer stack and filter system
- **Editor UI** - Slate viewport, details panels, toolbar integration
- **Compute Engine** - GPU-accelerated texture processing

## Original Plugin Features

### Core Systems
- **Layer Stack System** - Photoshop-style layer management with blend modes
- **Material Presets** - 30+ PBR presets (Metal, Wood, Concrete, Fabric, etc.)
- **Compute Shaders** - 20+ GPU shaders for filters, noise, PBR generation
- **Editor UI** - Custom Slate editor with viewport, layer panel, properties
- **Batch Processing** - Process multiple textures in parallel
- **Asset Generation** - Create materials, textures, and material functions

### Shader Categories
1. **PBR Generation** - Albedo, Normal, Roughness, Metallic, AO from single image
2. **Filters** - Blur, Sharpen, Edge Detect, Color Grading, HSV Adjust
3. **Noise Generators** - Perlin, Simplex, Voronoi, Cellular
4. **Blend Modes** - 15+ blend modes (Multiply, Screen, Overlay, etc.)
5. **Material Presets** - Glossy, Metal, Toon, Subsurface, Anisotropic

## KAIN Rebuild Goals

### Phase 1: Research & Analysis (Current)
- [ ] Analyze original C++ architecture
- [ ] Document shader pipeline and dependencies
- [ ] Map data structures and types
- [ ] Identify KAIN feature requirements

### Phase 2: Core Implementation
- [ ] Data structures and types
- [ ] Compute shader system
- [ ] Layer stack runtime
- [ ] Material preset registry
- [ ] Asset generation pipeline

### Phase 3: Editor UI
- [ ] Slate editor window
- [ ] Viewport with preview
- [ ] Layer stack widget
- [ ] Properties panel
- [ ] Toolbar integration

### Phase 4: Advanced Features
- [ ] Batch processing
- [ ] Graph editor for custom filters
- [ ] Material function generation
- [ ] Preset import/export

## Architecture

### Original C++ Structure (33 classes)
```
Core Systems:
- MaterializeEngine - Main engine coordinator
- MaterializeComputeEngine - GPU compute dispatch
- KLayerStack - Layer management
- KLayerEvaluator - Layer evaluation and blending

Editor:
- SMaterializeEditor - Main Slate editor window
- MaterializeEditorViewportClient - 3D preview viewport
- MaterializeToolbarExtension - Toolbar integration
- SMaterializeBatchWindow - Batch processing UI

Asset Pipeline:
- MaterializeAssetGenerator - Material/texture creation
- MaterializeMaterialLoader - Material loading
- MaterializePresetRegistry - Preset management
- MaterializeTransientGenerator - Runtime texture generation

Shaders:
- 20+ .usf compute shaders
- 5 .ush shared libraries
- Material functions for runtime blending
```

### KAIN Rebuild Structure (Target)
```
src/
├── types.kn                    # Core data structures, enums, presets
├── layer_system.kn             # Layer stack, evaluator, blend modes
├── compute_shaders.kn          # All GPU shaders in KAIN
├── material_pipeline.kn        # Material/texture generation
├── preset_registry.kn          # Preset management and loading
├── editor_main.kn              # Main Slate editor window
├── editor_viewport.kn          # Preview viewport
├── editor_widgets.kn           # Layer panel, properties, toolbar
├── batch_processor.kn          # Batch processing system
└── graph_editor.kn             # Optional: Node-based filter graph
```

## References

### Original Plugin
- **Location:** `Research/UEProj/Project_5.4/Plugins/Materialize`
- **Shaders:** 20+ .usf files in `Shaders/`
- **Source:** 33 C++ classes in `Source/Materialize/`
- **Content:** Material presets, functions, default textures

### Similar KAIN Plugins (FactoryPart2)
- **TextureForgePro** - Texture generation with graph editor
- **VoxelSculptPro** - GPU sculpting with viewport
- **MeshForge** - Procedural mesh with node editor

## Research Complete ✅

Three comprehensive analysis documents created:

1. **CORE_ARCHITECTURE.md** (50KB) — Complete type system, layer compositor, preset system
   - 9 enums (94 values), 10 structs (150+ fields)
   - Layer system with dirty tracking, blend modes, visibility
   - 33 presets across 7 categories
   - C++ → KAIN mapping for all types

2. **SHADER_ANALYSIS.md** (45KB) — GPU compute pipeline analysis
   - 22 shader files analyzed (PBR, blend, filter, noise, toon, glossy)
   - Multi-pass dispatch patterns (Gradient → Height → Final PBR)
   - Consolidation plan: 22 USF → 8 KAIN shaders (60% reduction)
   - Stdlib integration for common functions

3. **IMPLEMENTATION_PLAN.md** (20KB) — Complete rebuild roadmap
   - 8 implementation phases (11 weeks)
   - 9 KAIN files (3,500 lines total)
   - 4.3:1 compression ratio (15,000 C++ → 3,500 KAIN)
   - Testing strategy, risk assessment, success criteria

## Implementation Ready

**Next Steps:**
1. Phase 1: Core Types (types.kn - 800 lines)
2. Phase 2: Presets (presets.kn - 400 lines)
3. Phase 3: Engine API (engine.kn - 200 lines)
4. Phase 4: Layer System (layer_system.kn - 600 lines)
5. Phase 5: Compute Shaders (compute_shaders.kn - 800 lines)
6. Phase 6: Editor UI (editor_*.kn - 650 lines)
7. Phase 7: Batch Processing (batch_processor.kn - 50 lines)
8. Phase 8: Integration & Testing

## Build Commands

```bash
# After implementation
cd FactoryPart2/plugins/Materialize
kain build --ue5

# Test in UE5
# Copy to Research/UEProj/Project_5.4/Plugins/
```

---

**Status:** ✅ Research Complete — Ready for Phase 1 Implementation
