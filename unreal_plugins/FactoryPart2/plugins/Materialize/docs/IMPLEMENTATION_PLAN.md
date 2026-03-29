# Materialize KAIN Implementation Plan

**Version:** 2.0.0 (KAIN Rebuild)  
**Original Plugin:** Research/UEProj/Project_5.4/Plugins/Materialize  
**Target:** FactoryPart2/plugins/Materialize

---

## Executive Summary

Rebuild Materialize (Substance Sampler alternative) in KAIN to leverage:
- **Material Asset Pipeline** — Binary .uasset generation
- **Streamlined Shaders** — 22 USF files → 8 KAIN shaders (60% reduction)
- **Graph Editor** — Node-based layer stack
- **Editor UI** — Slate viewport, details panels, toolbar
- **Compute Engine** — GPU-accelerated texture processing

**Estimated Compression:** 15,000 C++ lines → 3,000-4,000 KAIN lines (4:1 ratio)

---

## KAIN File Structure

```
FactoryPart2/plugins/Materialize/src/
├── types.kn                    # Core data structures (9 enums, 10 structs)
├── presets.kn                  # 33 preset definitions
├── engine.kn                   # PBR generation API
├── layer_system.kn             # Layer stack, evaluator, blend modes
├── compute_shaders.kn          # GPU shader consolidation
├── editor_main.kn              # Main Slate editor window
├── editor_viewport.kn          # Preview viewport
├── editor_widgets.kn           # Layer panel, properties, toolbar
└── batch_processor.kn          # Batch processing system
```

**Total Estimated Lines:** 3,500 KAIN lines

| File | Lines | Purpose |
|------|-------|---------|
| `types.kn` | 800 | All enums and structs |
| `presets.kn` | 400 | 33 preset definitions |
| `engine.kn` | 200 | PBR generation API |
| `layer_system.kn` | 600 | Layer stack + evaluator |
| `compute_shaders.kn` | 800 | 8 consolidated shaders |
| `editor_main.kn` | 300 | Main editor window |
| `editor_viewport.kn` | 200 | Preview viewport |
| `editor_widgets.kn` | 150 | Layer panel + properties |
| `batch_processor.kn` | 50 | Batch processing |

---

## Implementation Phases

### Phase 1: Core Types & Data Structures (Week 1)

**Goal:** Establish the complete type system

**Tasks:**
1. Create `src/types.kn` with all 9 enums
2. Create all 10 structs with 150+ fields
3. Add KAIN attributes (`@editanywhere`, `@category`, `@slider`)
4. Test enum generation (UENUM with DisplayName)
5. Test struct generation (USTRUCT with defaults)

**Deliverables:**
- `types.kn` (800 lines)
- Unit tests for type validation
- Verify C++ codegen matches original

**Dependencies:**
- KAIN bitflags enum support (`@bitflags`)
- Conditional property visibility (`@meta("EditCondition=...")`)
- Slider range metadata (`@slider(min, max)`)

---

### Phase 2: Preset System (Week 1)

**Goal:** Define all 33 material presets

**Tasks:**
1. Create `src/presets.kn` with preset data
2. Implement preset registry functions
3. Add category filtering
4. Test preset lookup by ID

**Deliverables:**
- `presets.kn` (400 lines)
- 33 presets across 7 categories
- Blueprint functions for preset access

**Example:**
```kain
let skin_basic = MaterializePreset:
    id: "skin_basic"
    display_name: "Basic Skin"
    category: MaterialCategory.Organic
    params: MaterializeParams:
        normal_strength: 0.02
        roughness_contrast: 1.2
        roughness_brightness: 20.0
        roughness_invert: true
        metallic_contrast: 0.0
        metallic_bias: -100.0
        ao_intensity: 0.8
        bio_detail: 0.1

@blueprint
fn get_all_materialize_presets() -> Array<MaterializePreset>:
    return [skin_basic, leather_worn, alien_bio, ...]
```

---

### Phase 3: PBR Engine API (Week 2)

**Goal:** Blueprint functions for PBR generation

**Tasks:**
1. Create `src/engine.kn` with API functions
2. Implement `generate_pbr_maps()` wrapper
3. Implement `generate_and_save_pbr_maps()` wrapper
4. Test against original C++ output

**Deliverables:**
- `engine.kn` (200 lines)
- 2 blueprint functions
- Integration tests

**Example:**
```kain
@blueprint
fn generate_pbr_maps(source: Texture2D, params: MaterializeParams) -> MaterializeResult?:
    # Calls UMaterializeEngine::GeneratePBRMaps() under the hood
    # KAIN compiler generates C++ wrapper

@blueprint
fn generate_and_save_pbr_maps(
    source: Texture2D,
    params: MaterializeParams,
    output_path: String,
    base_name: String
) -> MaterializeResult?:
    # Calls UMaterializeEngine::GenerateAndSavePBRMaps()
```

---

### Phase 4: Layer System (Week 2-3)

**Goal:** Layer stack, evaluator, and compositor

**Tasks:**
1. Create `src/layer_system.kn` with LayerStack struct
2. Implement layer management methods (add, remove, move, duplicate)
3. Implement dirty tracking and visibility filtering
4. Implement layer evaluator API
5. Test layer stack operations

**Deliverables:**
- `layer_system.kn` (600 lines)
- LayerStack with 10+ methods
- 9 blueprint functions for layer operations
- Unit tests for dirty tracking

**Example:**
```kain
struct LayerStack:
    layers: Array<Layer>
    width: Int = 1024
    height: Int = 1024
    selected_layer_index: Int = -1
    
    fn add_layer(layer: Layer) -> Int:
        push(layers, layer)
        return len(layers) - 1
    
    fn mark_dirty(index: Int):
        if index >= 0 and index < len(layers):
            layers[index].dirty = true
            # Propagate upward
            for i in range(index + 1, len(layers)):
                layers[i].dirty = true

@blueprint
fn evaluate_stack(stack: LayerStack) -> LayerEvalResult?

@blueprint
fn blend_textures(
    base: Texture2D,
    blend: Texture2D,
    blend_mode: LayerBlendMode,
    opacity: Float,
    mask: Texture2D?,
    invert_mask: Bool
) -> Texture2D?
```

---

### Phase 5: Compute Shaders (Week 3-4)

**Goal:** Consolidate 22 USF files into 8 KAIN shaders

**Tasks:**
1. Port `PBRGenerator.usf` → `pbr_generator.kn` (4 kernels)
2. Port blend modes → `layer_blend.kn` (20 modes)
3. Port filters → `image_filters.kn` (13 types)
4. Port noise → `noise_generator.kn` (16 types)
5. Port seamless + packing → `seamless_packing.kn`
6. Keep specialized shaders (toon, glossy) as-is
7. Extract common functions to stdlib

**Deliverables:**
- `compute_shaders.kn` (800 lines) or separate files
- 8 consolidated shaders
- Stdlib functions (hash, linearize_srgb, blend modes)
- Pixel-perfect validation against USF output

**Consolidation Plan:**

| Original USF | KAIN Shader | Kernels | Lines |
|--------------|-------------|---------|-------|
| PBRGenerator.usf | pbr_generator.kn | 4 | 200 |
| MaterializeBlend.usf + LayerBlend.usf | layer_blend.kn | 1 | 100 |
| MaterializeFilters.usf + LayerFilter.usf | image_filters.kn | 1 | 150 |
| MaterializeNoiseGenerator.usf + ProceduralNoise.usf | noise_generator.kn | 1 | 150 |
| SeamlessAndPacking.usf | seamless_packing.kn | 2 | 50 |
| Toon*.usf (5 files) | toon_shading.kn | 5 | 100 |
| Glossy*.usf (3 files) | glossy_shading.kn | 3 | 50 |

**Example:**
```kain
shader compute PBRGradient(thread_id: Vec3):
    uniform source_texture: Sampler2D @0
    uniform normal_strength: Float @1
    uniform texture_dimensions: Vec2 @2
    uniform advanced_normal: Bool @3
    uniform normal_octaves: Int @4
    buffer out_gradient: RWBuffer<Vec2> @5
    
    let pos = thread_id.xy
    if pos.x >= texture_dimensions.x or pos.y >= texture_dimensions.y:
        return
    
    var grad = vec2(0.0, 0.0)
    
    if advanced_normal:
        for k in 0..normal_octaves:
            let sigma = 1.0 * pow(2.0, k as Float)
            grad = grad + compute_gradient_at_scale(pos, sigma)
    else:
        grad = sobel_gradient(pos)
    
    out_gradient[pos] = grad * normal_strength * 0.25
```

---

### Phase 6: Editor UI (Week 5-6)

**Goal:** Slate editor window with viewport and layer panel

**Tasks:**
1. Create `src/editor_main.kn` with main editor window
2. Create `src/editor_viewport.kn` with preview viewport
3. Create `src/editor_widgets.kn` with layer panel + properties
4. Implement toolbar integration
5. Test UI layout and interaction

**Deliverables:**
- `editor_main.kn` (300 lines)
- `editor_viewport.kn` (200 lines)
- `editor_widgets.kn` (150 lines)
- Functional editor UI

**Reference Plugins:**
- `FactoryPart2/plugins/TextureForgePro` — Similar texture editor
- `FactoryPart2/plugins/VoxelSculptPro` — Viewport patterns
- `Factory/Cinema4DMograph/Kain/editor.kn` — Editor UI patterns

**Example:**
```kain
@slate
struct MaterializeEditor:
    @viewport
    preview_viewport: MaterializeViewport
    
    @details
    properties_panel: MaterializeProperties
    
    @toolbar
    toolbar: MaterializeToolbar
    
    layer_stack: LayerStack
    current_result: MaterializeResult?

@viewport
struct MaterializeViewport:
    @scene_actor
    preview_mesh: StaticMesh
    
    @camera
    camera_position: Vec3 = vec3(0.0, -200.0, 0.0)
    camera_rotation: Vec3 = vec3(0.0, 0.0, 0.0)
```

---

### Phase 7: Batch Processing (Week 6)

**Goal:** Batch process multiple textures

**Tasks:**
1. Create `src/batch_processor.kn`
2. Implement batch queue system
3. Add progress tracking
4. Test batch operations

**Deliverables:**
- `batch_processor.kn` (50 lines)
- Batch processing UI
- Progress bar integration

---

### Phase 8: Integration & Testing (Week 7)

**Goal:** Full plugin integration and validation

**Tasks:**
1. Update `KAIN.toml` with sources array
2. Run `kain build --ue5`
3. Test in UE5 project
4. Validate against original plugin
5. Performance benchmarking
6. Bug fixes

**Deliverables:**
- Working UE5 plugin
- Test suite (unit + integration)
- Performance report
- Documentation

---

## KAIN.toml Configuration

```toml
[package]
name = "Materialize"
version = "2.0.0"
authors = ["K-Studio - KAIN Rebuild"]
description = "Substance Sampler for UE5. Generate complete PBR materials from a single image with 30+ material presets. Rebuilt in KAIN with full material asset pipeline, streamlined shaders, and graph editor."

[build]
entry = "src/types.kn"
target = "ue5"

[ue5]
plugin_name = "Materialize"
plugin_dir = "."
engine_version = "5.4"
modular_output = true

sources = [
    "src/types.kn",
    "src/presets.kn",
    "src/engine.kn",
    "src/layer_system.kn",
    "src/compute_shaders.kn",
    "src/editor_main.kn",
    "src/editor_viewport.kn",
    "src/editor_widgets.kn",
    "src/batch_processor.kn",
]

[[ue5.modules]]
name = "Materialize"
type = "Editor"
loading_phase = "PostConfigInit"
```

---

## Feature Comparison

### Original C++ Plugin

| Component | Files | Lines | Complexity |
|-----------|-------|-------|------------|
| Types | 1 header | 500 | High (manual UPROPERTY) |
| Layer System | 2 headers + 2 cpp | 2,000 | High (manual methods) |
| Engine | 2 headers + 2 cpp | 3,000 | High (GPU dispatch) |
| Shaders | 22 .usf files | 5,000 | High (duplicate code) |
| Editor UI | 10 headers + 10 cpp | 4,000 | High (Slate boilerplate) |
| Presets | 2 headers + 2 cpp | 500 | Medium (data arrays) |
| **Total** | **51 files** | **15,000** | **Very High** |

### KAIN Rebuild

| Component | Files | Lines | Complexity |
|-----------|-------|-------|------------|
| Types | 1 .kn | 800 | Low (auto UPROPERTY) |
| Layer System | 1 .kn | 600 | Low (auto methods) |
| Engine | 1 .kn | 200 | Low (wrapper functions) |
| Shaders | 8 .kn | 800 | Medium (consolidated) |
| Editor UI | 3 .kn | 650 | Low (auto Slate) |
| Presets | 1 .kn | 400 | Low (data only) |
| **Total** | **15 files** | **3,450** | **Low** |

**Compression Ratio:** 4.3:1 (15,000 → 3,450 lines)

---

## Dependencies

### KAIN Features Required

| Feature | Status | Priority |
|---------|--------|----------|
| Bitflags enums (`@bitflags`) | ✅ Supported | Critical |
| Conditional visibility (`@meta("EditCondition=...")`) | ✅ Supported | High |
| Slider ranges (`@slider(min, max)`) | ✅ Supported | High |
| Struct methods | ✅ Supported | Critical |
| Nullable textures (`Texture2D?`) | ✅ Supported | Critical |
| Blueprint functions (`@blueprint`) | ✅ Supported | Critical |
| Compute shaders (`shader compute`) | ✅ Supported | Critical |
| Slate widgets (`@slate`) | ✅ Supported | High |
| Details panels (`@details`) | ✅ Supported | High |
| Viewports (`@viewport`) | ✅ Supported | High |
| Material asset generation | ✅ Supported | High |

### UE5 Modules Required

- Core, CoreUObject, Engine
- RenderCore, RHI (GPU compute)
- Slate, SlateCore (Editor UI)
- UnrealEd, AssetTools (Asset generation)
- PropertyEditor (Details panels)

---

## Testing Strategy

### Unit Tests

**Type System:**
- Enum value ranges
- Struct default values
- Bitflags operations

**Layer Stack:**
- Add/Insert/Remove/Move/Duplicate
- Dirty tracking propagation
- Visibility filtering (enabled/solo/locked)

**Presets:**
- All 33 presets load correctly
- Category filtering
- Preset lookup by ID

### Integration Tests

**PBR Generation:**
- Generate from 1024x1024 source
- All 7 maps produced
- ORM packing correct
- Generation time < 500ms

**Layer Evaluation:**
- 10-layer stack evaluates correctly
- Blend modes produce expected results
- Masking works correctly
- Dirty tracking avoids redundant work

**Editor UI:**
- Window opens and renders
- Viewport displays preview
- Layer panel shows stack
- Properties update on selection

### Performance Tests

**Benchmarks:**
- 1024x1024 single layer: < 5ms
- 2048x2048 single layer: < 20ms
- 10-layer stack (1024x1024): < 50ms
- Full PBR generation (2048x2048): < 500ms

**Memory:**
- 10-layer stack: < 5KB
- Cached textures: Width × Height × 4 bytes per layer
- Total memory: < 100MB for typical workflow

---

## Risk Assessment

### High Risk

1. **Shader Consolidation** — 22 USF files → 8 KAIN shaders
   - Mitigation: Incremental migration, pixel-perfect validation
   - Fallback: Keep original USF files, use KAIN for new shaders

2. **Multi-Pass GPU Pipeline** — Gradient → Height → Final PBR
   - Mitigation: Test each pass independently
   - Fallback: Single-pass mode (fast preview)

### Medium Risk

3. **Editor UI Complexity** — Slate viewport + layer panel + properties
   - Mitigation: Reference TextureForgePro, VoxelSculptPro patterns
   - Fallback: Simplified UI (viewport only)

4. **Layer Stack Dirty Tracking** — Upward propagation logic
   - Mitigation: Unit tests for all edge cases
   - Fallback: Mark all dirty on any change

### Low Risk

5. **Type System** — 9 enums, 10 structs
   - Mitigation: Direct 1:1 mapping from C++
   - Fallback: None needed (straightforward)

6. **Preset System** — 33 preset definitions
   - Mitigation: Pure data, no logic
   - Fallback: None needed (straightforward)

---

## Success Criteria

### Functional Requirements

- [ ] All 33 presets generate correct PBR maps
- [ ] Layer stack supports 8 layer types
- [ ] 20 blend modes produce pixel-perfect results
- [ ] 13 filters work correctly
- [ ] 16 noise types generate expected patterns
- [ ] Editor UI opens and renders
- [ ] Viewport displays preview mesh
- [ ] Layer panel shows stack
- [ ] Properties panel updates on selection
- [ ] Batch processing works

### Performance Requirements

- [ ] PBR generation (2048x2048): < 500ms
- [ ] Layer evaluation (10 layers): < 50ms
- [ ] UI responsiveness: < 16ms per frame
- [ ] Memory usage: < 100MB

### Quality Requirements

- [ ] Zero compilation errors
- [ ] Zero runtime crashes
- [ ] Pixel-perfect match with original plugin
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Performance benchmarks met

---

## Timeline

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| Phase 1: Core Types | 1 week | types.kn (800 lines) |
| Phase 2: Presets | 1 week | presets.kn (400 lines) |
| Phase 3: Engine API | 1 week | engine.kn (200 lines) |
| Phase 4: Layer System | 2 weeks | layer_system.kn (600 lines) |
| Phase 5: Compute Shaders | 2 weeks | compute_shaders.kn (800 lines) |
| Phase 6: Editor UI | 2 weeks | editor_*.kn (650 lines) |
| Phase 7: Batch Processing | 1 week | batch_processor.kn (50 lines) |
| Phase 8: Integration & Testing | 1 week | Full plugin |
| **Total** | **11 weeks** | **3,500 KAIN lines** |

---

## Next Steps

1. **Scaffold Project** — Create `src/` directory and `KAIN.toml`
2. **Phase 1 Start** — Begin with `types.kn` (9 enums, 10 structs)
3. **Parallel Research** — Study TextureForgePro and VoxelSculptPro for editor patterns
4. **Shader Prototyping** — Test single-pass PBR shader first
5. **Weekly Reviews** — Validate progress against timeline

---

**Status:** Ready to begin Phase 1 (Core Types)
