# Phase 1 Quick Start: Core Types

**Goal:** Implement all 9 enums and 10 structs in `src/types.kn`

**Estimated Time:** 1 week  
**Estimated Lines:** 800 KAIN lines

---

## Task Checklist

### Enums (9 total)

- [ ] `MaterialCategory` (8 values) — Organic, Rubber, Ground, Fabric, Metal, Plastic, Paper, Custom
- [ ] `SeamlessMode` (4 values) — None, CrossBlend, MirrorBlend, Histogram
- [ ] `LayerBlendMode` (20 values) — Normal, Multiply, Screen, Overlay, etc.
- [ ] `LayerType` (8 values) — Base, Image, Procedural, Fill, Adjustment, Filter, Generator, Folder
- [ ] `LayerOutputChannel` (9 values, bitflags) — BaseColor, Normal, Roughness, Metallic, etc.
- [ ] `ProceduralNoiseType` (15 values) — Perlin, Simplex, Worley, FBM, etc.
- [ ] `FilterType` (13 values) — Blur, Sharpen, EdgeDetect, etc.
- [ ] `AdjustmentType` (9 values) — Levels, Curves, HSV, Brightness, etc.
- [ ] `GeneratorType` (8 values) — AmbientOcclusion, Curvature, Position, etc.

### Structs (10 total)

- [ ] `MaterializeParams` (40+ fields) — PBR generation parameters
- [ ] `MaterializePreset` (4 fields) — Preset descriptor
- [ ] `MaterializeResult` (10 fields) — PBR generation result
- [ ] `MasterPreset` (11 fields) — Master material preset
- [ ] `ProceduralParams` (9 fields) — Procedural generation parameters
- [ ] `FilterParams` (4 fields) — Filter parameters
- [ ] `AdjustmentParams` (11 fields) — Adjustment parameters
- [ ] `Layer` (25+ fields) — Single layer definition
- [ ] `LayerStack` (5 fields + methods) — Layer stack container
- [ ] `LayerEvalResult` (8 fields) — Layer evaluation result

---

## Implementation Template

### Enum Example

```kain
# From CORE_ARCHITECTURE.md section "Enums"
enum MaterialCategory:
    Organic
    Rubber
    Ground
    Fabric
    Metal
    Plastic
    Paper
    Custom
```

### Bitflags Enum Example

```kain
@bitflags
enum LayerOutputChannel:
    None = 0
    BaseColor = 1
    Normal = 2
    Roughness = 4
    Metallic = 8
    Height = 16
    AO = 32
    Emissive = 64
    Mask = 128
    All = 255
```

### Struct Example (Simple)

```kain
struct MaterializePreset:
    id: String
    display_name: String
    category: MaterialCategory
    params: MaterializeParams
```

### Struct Example (Complex with Attributes)

```kain
struct MaterializeParams:
    # Normal
    @editanywhere
    @category("Normal")
    @slider(0.0, 2.0)
    normal_strength: Float = 1.0
    
    # Roughness
    @editanywhere
    @category("Roughness")
    @slider(0.0, 1.0)
    roughness_base: Float = 0.7
    
    @editanywhere
    @category("Roughness")
    @slider(0.0, 3.0)
    roughness_contrast: Float = 1.0
    
    @editanywhere
    @category("Roughness")
    @slider(-128.0, 128.0)
    roughness_brightness: Float = 0.0
    
    @editanywhere
    @category("Roughness")
    roughness_invert: Bool = true
    
    # ... 35 more fields
```

### Struct with Methods Example

```kain
struct LayerStack:
    version: Int = 3
    layers: Array<Layer>
    width: Int = 1024
    height: Int = 1024
    selected_layer_index: Int = -1
    
    fn add_layer(layer: Layer) -> Int:
        push(layers, layer)
        return len(layers) - 1
    
    fn remove_layer(index: Int) -> Bool:
        if index < 0 or index >= len(layers):
            return false
        # Remove layer at index
        return true
    
    fn mark_dirty(index: Int):
        if index >= 0 and index < len(layers):
            layers[index].dirty = true
            # Mark all layers above as dirty
            for i in range(index + 1, len(layers)):
                layers[i].dirty = true
```

---

## Reference Documents

1. **CORE_ARCHITECTURE.md** — Complete type definitions with C++ equivalents
2. **Original C++ Headers:**
   - `Research/UEProj/Project_5.4/Plugins/Materialize/Source/Materialize/Public/MaterializeTypes.h`
   - `Research/UEProj/Project_5.4/Plugins/Materialize/Source/Materialize/Public/KLayerStack.h`

---

## Validation Steps

### 1. Compile Test

```bash
cd FactoryPart2/plugins/Materialize
kain build --ue5 --dry-run
```

Expected: No compilation errors

### 2. Enum Generation Test

Check generated C++ in `Source/Materialize/Public/`:
- `EMaterialCategory` with `UENUM(BlueprintType)`
- `UMETA(DisplayName=...)` for each value
- Correct E prefix

### 3. Struct Generation Test

Check generated C++ in `Source/Materialize/Public/`:
- `FMaterializeParams` with `USTRUCT(BlueprintType)`
- `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=...)` for each field
- Default values in constructor
- Correct F prefix

### 4. Bitflags Test

Check `ELayerOutputChannel`:
- `UENUM(BlueprintType, meta = (Bitflags))`
- `ENUM_CLASS_FLAGS(ELayerOutputChannel)` macro

### 5. Method Generation Test

Check `FLayerStack`:
- Member functions generated correctly
- Array operations work (push, len, range)

---

## Common Issues

### Issue 1: Missing @editanywhere

**Symptom:** Fields not visible in UE5 Details panel

**Fix:** Add `@editanywhere` attribute to all fields that should be editable

### Issue 2: Wrong Slider Range

**Symptom:** Slider allows values outside expected range

**Fix:** Use `@slider(min, max)` or `@meta("ClampMin=..., ClampMax=...")`

### Issue 3: Conditional Visibility Not Working

**Symptom:** Dependent fields always visible

**Fix:** Use `@meta("EditCondition=field_name")` on dependent field

### Issue 4: Bitflags Not Working

**Symptom:** Can't combine flags with `|` operator

**Fix:** Ensure `@bitflags` attribute on enum, check generated `ENUM_CLASS_FLAGS` macro

---

## Next Steps After Phase 1

Once `types.kn` is complete and validated:

1. **Phase 2:** Create `presets.kn` with 33 preset definitions
2. **Phase 3:** Create `engine.kn` with PBR generation API
3. **Phase 4:** Create `layer_system.kn` with layer stack logic

---

**Ready to Start:** Create `src/types.kn` and begin with enums!
