# Alpha Blending and Layer Ordering Implementation Notes

## Overview

This document describes the implementation of alpha blending and layer ordering in the Materialize layer compositor system, addressing Task 2.5 of the materialize-plugin-polish specification.

## Layer Ordering

### Bottom-to-Top Evaluation

Layers are stored and evaluated in **bottom-to-top order**:
- **Index 0**: Bottom layer (rendered first)
- **Index 1, 2, 3, ...**: Middle layers
- **Index N-1**: Top layer (rendered last)

This ordering is critical for correct alpha compositing, where each layer is blended onto the accumulated result from all layers below it.

### Implementation

The layer ordering is implemented in two key locations:

1. **FKLayerStack::GetVisibleLayerIndices()** (`KLayerStack.h`)
   - Returns layer indices in array order (0, 1, 2, ...)
   - Filters out disabled layers
   - Handles solo layer mode (only solo layers are visible)
   - Maintains bottom-to-top ordering

2. **UKLayerEvaluator::EvaluateStack()** (`KLayerEvaluator.cpp`)
   - Iterates through visible layer indices in order
   - Evaluates each layer and composites it onto the accumulated result
   - Ensures layers are processed bottom-to-top

### Code Example

```cpp
// Get visible layers (bottom-to-top order)
TArray<int32> VisibleIndices = Stack.GetVisibleLayerIndices();

// Evaluate each layer and composite (bottom-to-top)
for (int32 i = 0; i < VisibleIndices.Num(); ++i)
{
    int32 LayerIndex = VisibleIndices[i];
    FKLayer& Layer = Stack.Layers[LayerIndex];
    
    // Evaluate layer...
    // Blend onto accumulated result...
}
```

## Alpha Blending

### Mathematical Foundation

The alpha blending implementation uses the standard **"over" operator** for alpha compositing:

```
Given:
  base  = Accumulated result from layers below (RGBA)
  blend = Current layer being composited (RGBA)
  opacity = Layer opacity parameter (0.0-1.0)
  mask = Mask texture value (0.0-1.0, optional)

Process:
  1. effectiveOpacity = opacity * mask
  2. blended.rgb = ApplyBlendMode(base.rgb, blend.rgb)
  3. finalBlendAmount = effectiveOpacity * blend.a
  4. result.rgb = lerp(base.rgb, blended.rgb, finalBlendAmount)
  5. result.a = base.a + blend.a * effectiveOpacity * (1 - base.a)
```

### Key Improvements (Task 2.5)

#### 1. Separated Alpha from Effective Opacity

**Before:**
```hlsl
float effectiveOpacity = Opacity * maskValue * blend.a;
float3 result = lerp(base.rgb, blended, effectiveOpacity);
float resultAlpha = base.a + blend.a * effectiveOpacity * (1.0 - base.a);
```

**Problem:** The blend layer's alpha was being used twice - once in `effectiveOpacity` and again in the alpha calculation, leading to incorrect alpha accumulation.

**After:**
```hlsl
float effectiveOpacity = Opacity * maskValue;
float finalBlendAmount = effectiveOpacity * blend.a;
float3 result = lerp(base.rgb, blended, finalBlendAmount);
float resultAlpha = base.a + blend.a * effectiveOpacity * (1.0 - base.a);
```

**Fix:** The blend layer's alpha is now correctly separated:
- Used once in the RGB lerp (via `finalBlendAmount`)
- Used once in the alpha compositing formula
- This ensures proper alpha accumulation

#### 2. Added Saturation to Dodge/Burn Modes

**Before:**
```hlsl
float3 BlendColorDodge(float3 base, float3 blend)
{
    return base / max(1.0 - blend, 0.0001);
}
```

**Problem:** Color dodge and burn modes could produce values outside [0, 1] range.

**After:**
```hlsl
float3 BlendColorDodge(float3 base, float3 blend)
{
    return saturate(base / max(1.0 - blend, 0.0001));
}
```

**Fix:** Added `saturate()` to clamp results to valid [0, 1] range.

#### 3. Added Comprehensive Documentation

Added detailed comments explaining:
- The alpha blending mathematics
- The role of each parameter
- The compositing process
- Expected behavior

### Shader Implementation

The alpha blending is implemented in `Shaders/KStudioCore/LayerBlend.usf`:

```hlsl
[numthreads(8, 8, 1)]
void BlendCS(uint3 ThreadId : SV_DispatchThreadID)
{
    // Load base and blend textures
    float4 base = InBase.Load(int3(Pos, 0));
    float4 blend = InBlend.Load(int3(Pos, 0));
    
    // Get mask value (if present)
    float maskValue = 1.0;
    if (bHasMask > 0)
    {
        maskValue = InMask.Load(int3(Pos, 0));
        if (bInvertMask > 0) maskValue = 1.0 - maskValue;
    }
    
    // Calculate effective opacity
    float blendAlpha = blend.a;
    float effectiveOpacity = Opacity * maskValue;
    
    // Apply blend mode to RGB
    float3 blended = ApplyBlendMode(base.rgb, blend.rgb);
    
    // Combine with blend layer's alpha
    float finalBlendAmount = effectiveOpacity * blendAlpha;
    
    // Lerp RGB channels
    float3 result = lerp(base.rgb, blended, finalBlendAmount);
    
    // Composite alpha using "over" operator
    float resultAlpha = base.a + blendAlpha * effectiveOpacity * (1.0 - base.a);
    
    OutResult[Pos] = float4(result, resultAlpha);
}
```

## Blend Modes

The shader implements 20 Photoshop-compatible blend modes:

1. Normal
2. Multiply
3. Screen
4. Overlay
5. Soft Light
6. Hard Light
7. Add (Linear Dodge)
8. Subtract
9. Difference
10. Exclusion
11. Darken
12. Lighten
13. Color Dodge
14. Color Burn
15. Linear Dodge
16. Linear Burn
17. Vivid Light
18. Linear Light
19. Pin Light
20. Hard Mix

All blend modes:
- Operate only on RGB channels
- Preserve alpha channel for separate compositing
- Use saturate() where needed to clamp results
- Handle edge cases (division by zero, etc.)

## Validation

### Pre-Dispatch Validation

The `UKLayerEvaluator::ValidateLayerStack()` function validates:
- Layer stack is not empty
- Dimensions are valid (> 0, <= 8192)
- Each layer has valid opacity (0.0-1.0)
- Each layer has valid blend mode
- Image layers have valid textures
- Filter/adjustment parameters are in valid ranges
- Mask textures are valid (if present)

### Parameter Synchronization

The implementation includes validation checks to ensure CPU parameters are correctly synchronized to GPU:

```cpp
// Validate parameter synchronization (debug builds)
checkf(Params->BlendMode == static_cast<uint32>(BlendMode), 
    TEXT("BlendMode parameter mismatch: CPU=%d, GPU=%d"), 
    static_cast<uint32>(BlendMode), Params->BlendMode);
checkf(FMath::IsNearlyEqual(Params->Opacity, Opacity, 0.0001f),
    TEXT("Opacity parameter mismatch: CPU=%f, GPU=%f"), 
    Opacity, Params->Opacity);
```

## Testing

Unit tests have been created in `KLayerBlendTests.cpp` to verify:

1. **Layer Ordering**
   - Layers are returned in bottom-to-top order (0, 1, 2, ...)
   - Solo layers are correctly filtered
   - Disabled layers are excluded

2. **Blend Mode Validation**
   - All 20 blend modes are recognized as valid
   - Invalid blend modes are rejected

3. **Layer Stack Validation**
   - Empty stacks are rejected
   - Invalid dimensions are rejected
   - Excessive dimensions are rejected
   - Valid stacks pass validation

4. **Opacity Validation**
   - Opacity outside [0, 1] range is rejected
   - Valid opacity values pass validation

## Property Validation

This implementation validates **Property 4: Alpha Blending and Layer Order Correctness**:

> *For any* layer stack, compositing the layers should respect alpha blending mathematics and evaluate layers in bottom-to-top order, where each layer's contribution is determined by its opacity and blend mode.

The implementation ensures:
- ✅ Layers are evaluated in bottom-to-top order (index 0 = bottom)
- ✅ Alpha blending uses standard "over" operator mathematics
- ✅ Layer opacity correctly modulates blend strength
- ✅ Blend layer's alpha is correctly handled (not double-counted)
- ✅ Mask textures correctly modulate the blend amount
- ✅ All blend modes preserve alpha channel for separate compositing
- ✅ Edge cases (division by zero, out-of-range values) are handled

## Future Improvements

Potential enhancements for future iterations:

1. **GPU-Based Validation**: Move some validation checks to compute shaders for better performance
2. **Advanced Blend Modes**: Add HSL-based blend modes (Hue, Saturation, Color, Luminosity)
3. **Blend Mode Optimization**: Use shader permutations to reduce branching
4. **Alpha Premultiplication**: Support premultiplied alpha for better compositing quality
5. **Layer Groups**: Support nested layer groups with their own blend modes

## References

- [Porter-Duff Compositing Operators](https://en.wikipedia.org/wiki/Alpha_compositing)
- [Photoshop Blend Modes Specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/)
- [Unreal Engine RDG Documentation](https://docs.unrealengine.com/5.0/en-US/render-dependency-graph-in-unreal-engine/)
