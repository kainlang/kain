# Task 3 Checkpoint: Layer System Stability Validation

## Overview

This document provides a comprehensive validation checklist for Task 3 of the materialize-plugin-polish spec. The checkpoint ensures that all layer system improvements from tasks 2.1-2.9 are working correctly before proceeding to toolbar integration.

## Automated Test Coverage

### Test Files Created

1. **KLayerBlendTests.cpp** - Alpha blending and layer ordering
2. **KLayerParameterSyncTests.cpp** - CPU-GPU parameter synchronization  
3. **KLayerEvaluatorValidationTests.cpp** - Layer stack validation
4. **KSampleResourceManagementTests.cpp** - Resource management and cleanup

### Test Categories

#### 1. Layer Ordering Tests (Task 2.5)
- ✅ `FKLayerAlphaBlendingTest::RunTest`
  - Verifies bottom-to-top layer ordering (index 0 = bottom)
  - Tests solo layer behavior
  - Tests disabled layer filtering
  - Validates opacity ranges
  - Validates blend mode enums

#### 2. Blend Mode Validation Tests (Task 2.1, 2.5)
- ✅ `FKLayerBlendModeValidationTest::RunTest`
  - Tests all 20 Photoshop blend modes
  - Validates blend mode enum ranges
  - Rejects invalid blend modes

#### 3. Layer Stack Validation Tests (Task 2.1, 2.7)
- ✅ `FKLayerStackValidationTest::RunTest`
  - Rejects empty stacks
  - Validates dimensions (0 < size <= 8192)
  - Validates layer parameters
  - Tests valid stack acceptance

#### 4. Parameter Synchronization Tests (Task 2.3)
- ✅ `FKLayerParameterSyncBlendModeTest::RunTest`
  - Tests blend mode + opacity synchronization
  - Validates CPU-GPU parameter matching
  
- ✅ `FKLayerParameterSyncProceduralTest::RunTest`
  - Tests noise parameters (Perlin, Voronoi, FBM)
  - Validates scale, octaves, persistence, lacunarity, seed
  
- ✅ `FKLayerParameterSyncFilterTest::RunTest`
  - Tests filter parameters (Blur, Sharpen, EdgeDetect)
  - Validates intensity, kernel size, threshold
  
- ✅ `FKLayerParameterSyncAdjustmentTest::RunTest`
  - Tests adjustment parameters (Levels, HSV, Brightness/Contrast)
  - Validates all adjustment parameter ranges
  
- ✅ `FKLayerParameterSyncMaskTest::RunTest`
  - Tests mask texture synchronization
  - Tests inverted mask flag

#### 5. Validation Layer Tests (Task 2.1)
- ✅ `FKLayerValidationEmptyStackTest::RunTest`
- ✅ `FKLayerValidationInvalidDimensionsTest::RunTest`
- ✅ `FKLayerValidationInvalidOpacityTest::RunTest`
- ✅ `FKLayerValidationValidStackTest::RunTest`
- ✅ `FKLayerValidationBlendModeTest::RunTest`
- ✅ `FKLayerValidationFilterTypeTest::RunTest`
- ✅ `FKLayerValidationProceduralParamsTest::RunTest`
- ✅ `FKLayerValidationFilterParamsTest::RunTest`
- ✅ `FKLayerValidationAdjustmentParamsTest::RunTest`

#### 6. Resource Management Tests (Task 2.9)
- ✅ `FMaterializeRDGScopeTest::RunTest`
  - Tests RAII wrapper for RDG resources
  - Validates automatic execution on scope exit
  
- ✅ `FMaterializeCleanupTransientResourcesTest::RunTest`
  - Tests transient resource cleanup
  - Validates no resource leaks
  
- ✅ `FMaterializeValidateRHIResourceTest::RunTest`
  - Tests RHI resource validation
  - Validates texture format checks
  
- ✅ `FMaterializeResourceLeakTest::RunTest`
  - Tests for memory leaks after generation
  - Validates proper resource cleanup

## Running Automated Tests

### Method 1: Unreal Editor Session Frontend

1. Open Unreal Editor with the plugin loaded
2. Go to **Window → Developer Tools → Session Frontend**
3. Click the **Automation** tab
4. In the test tree, expand **Materialize**
5. Select all tests under:
   - `Materialize.Layer.*`
   - `Materialize.ResourceManagement.*`
6. Click **Start Tests**
7. Review results in the output panel

### Method 2: Command Line

```bash
# From UE5 project root
UnrealEditor.exe "<PROJECT>.uproject" -ExecCmds="Automation RunTests Materialize" -unattended -nopause -NullRHI -log
```

### Method 3: Editor Console

1. Open Unreal Editor
2. Press ` (backtick) to open console
3. Run: `Automation RunTests Materialize`
4. Check Output Log for results

## Manual Testing Checklist

### Prerequisites
- Plugin compiled and loaded in Unreal Editor
- Test project with sample textures available

### Test Scenarios

#### Scenario 1: Basic Layer Operations
- [ ] **Add Layer**: Right-click in layer panel → Add Layer
  - Expected: New layer appears at top of stack
  - Expected: Preview updates immediately
  
- [ ] **Remove Layer**: Select layer → Delete key or right-click → Remove
  - Expected: Layer removed from stack
  - Expected: Preview updates without crash
  
- [ ] **Reorder Layers**: Drag layer up/down in stack
  - Expected: Layer order changes
  - Expected: Preview updates to reflect new order
  - Expected: Bottom-to-top compositing maintained

#### Scenario 2: Blend Modes
- [ ] **Test Each Blend Mode**: Create 2 layers, cycle through all 20 blend modes
  - Normal, Multiply, Screen, Overlay, Soft Light, Hard Light
  - Add, Subtract, Difference, Exclusion
  - Darken, Lighten, Color Dodge, Color Burn
  - Linear Dodge, Linear Burn, Vivid Light, Linear Light
  - Pin Light, Hard Mix
  - Expected: Each mode produces visually distinct result
  - Expected: No crashes or visual artifacts
  - Expected: Alpha blending works correctly

#### Scenario 3: Layer Opacity
- [ ] **Adjust Opacity**: Select layer → Drag opacity slider (0.0 to 1.0)
  - Expected: Layer transparency changes smoothly
  - Expected: Preview updates in real-time
  - Expected: 0.0 = fully transparent, 1.0 = fully opaque

#### Scenario 4: Layer Types
- [ ] **Fill Layer**: Add fill layer → Change color
  - Expected: Solid color layer renders correctly
  
- [ ] **Image Layer**: Add image layer → Assign texture
  - Expected: Texture displays correctly
  
- [ ] **Procedural Layer**: Add procedural layer → Adjust noise parameters
  - Expected: Noise generates correctly
  - Expected: Parameters update in real-time

#### Scenario 5: Filters
- [ ] **Blur Filter**: Apply blur to layer → Adjust intensity
  - Expected: Blur effect applies correctly
  - Expected: Kernel size affects blur radius
  
- [ ] **Sharpen Filter**: Apply sharpen → Adjust intensity
  - Expected: Sharpening enhances edges
  
- [ ] **Edge Detect Filter**: Apply edge detect
  - Expected: Edges highlighted correctly

#### Scenario 6: Adjustments
- [ ] **Levels Adjustment**: Apply levels → Adjust input/output black/white
  - Expected: Contrast and brightness adjust correctly
  
- [ ] **HSV Adjustment**: Apply HSV → Adjust hue/saturation/value
  - Expected: Color shifts work correctly
  
- [ ] **Brightness/Contrast**: Apply adjustment → Drag sliders
  - Expected: Image brightness/contrast changes

#### Scenario 7: Masks
- [ ] **Add Mask**: Add mask to layer → Paint on mask
  - Expected: Mask modulates layer visibility
  
- [ ] **Invert Mask**: Toggle invert mask checkbox
  - Expected: Mask inverts correctly

#### Scenario 8: Complex Stacks
- [ ] **10+ Layers**: Create stack with 10+ layers of mixed types
  - Expected: All layers composite correctly
  - Expected: No performance degradation
  - Expected: No crashes or memory leaks
  
- [ ] **Rapid Changes**: Quickly add/remove/reorder layers
  - Expected: System remains stable
  - Expected: No visual glitches

#### Scenario 9: Edge Cases
- [ ] **Empty Stack**: Remove all layers
  - Expected: Graceful error message or default state
  
- [ ] **Invalid Texture**: Assign null/deleted texture to image layer
  - Expected: Validation error, no crash
  
- [ ] **Extreme Parameters**: Set parameters to min/max values
  - Expected: Clamping works correctly
  - Expected: No shader errors

#### Scenario 10: Error Handling
- [ ] **Shader Compilation Failure**: Force shader error (if possible)
  - Expected: User-friendly error message
  - Expected: System remains stable
  
- [ ] **GPU Out of Memory**: Create very large texture (8192x8192)
  - Expected: Graceful degradation or error message
  - Expected: No crash

## Validation Criteria

### Pass Criteria
✅ All automated tests pass (0 failures)
✅ All manual test scenarios complete without crashes
✅ No visual artifacts or glitches observed
✅ No memory leaks detected (check Task Manager during testing)
✅ Error messages are descriptive and user-friendly
✅ Performance is acceptable (< 100ms for 1024x1024 layer composite)

### Fail Criteria
❌ Any automated test fails
❌ Crashes during manual testing
❌ Visual artifacts (incorrect blending, color shifts, etc.)
❌ Memory leaks (increasing memory usage over time)
❌ Silent failures (operations fail without error messages)
❌ Performance degradation (> 500ms for simple operations)

## Known Issues (If Any)

_Document any known issues discovered during testing here_

## Sign-Off

- [ ] Automated tests executed: **PASS / FAIL**
- [ ] Manual testing completed: **PASS / FAIL**
- [ ] No crashes observed: **YES / NO**
- [ ] No memory leaks detected: **YES / NO**
- [ ] Performance acceptable: **YES / NO**

**Tester Name**: _______________
**Date**: _______________
**Notes**: _______________

## Next Steps

Upon successful validation:
- ✅ Mark Task 3 as complete
- ✅ Proceed to Task 4: Toolbar Integration
- ✅ Archive this validation document for reference

Upon failed validation:
- ❌ Document failures in "Known Issues" section
- ❌ Create bug fix tasks
- ❌ Re-run validation after fixes
