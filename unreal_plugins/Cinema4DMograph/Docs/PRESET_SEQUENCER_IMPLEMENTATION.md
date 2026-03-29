# Modifier Preset System & Sequencer Integration

## Overview

This document describes the Modifier Preset System and Sequencer Integration added to the Cinema4DMograph KAIN plugin. These features enable:

1. **Preset Management** - Save/load modifier configurations for reuse
2. **Expression-Based Modifiers** - Create custom modifiers using math expressions
3. **Sequencer Keyframing** - Animate modifier properties in UE5's Sequencer timeline

---

## 1. Modifier Preset System

### Types Added (types.kn)

#### `ExpressionModifierPreset` (DataTable)
A data-driven modifier that uses math expressions to define motion behavior.

**Fields:**
- `display_name` - Name shown in UI
- `category` - Grouping category ("Motion", "Sci-Fi", "Nature", etc.)
- `description` - Tooltip description
- `position_expression` - Math expression for position offset
- `rotation_expression` - Math expression for rotation offset
- `scale_expression` - Math expression for scale offset
- `variable_0/1/2_name` - User-defined slider names
- `variable_0/1/2_default/min/max` - Slider ranges
- `step` - Time offset per instance (cascading effects)
- `speed_multiplier` - Global speed control

**Expression Variables:**
- `t` - Current time in seconds
- `i` - Instance index (0, 1, 2, ...)
- `n` - Total instance count
- `x, y, z` - Current position
- `rx, ry, rz` - Current rotation (degrees)
- `sx, sy, sz` - Current scale
- `v0, v1, v2...` - User-defined slider values

**Expression Functions:**
- Trigonometry: `sin, cos, tan, asin, acos, atan, atan2`
- Math: `abs, floor, ceil, round, clamp, min, max`
- Advanced: `sqrt, pow, exp, log, log10, fmod, frac`
- Interpolation: `lerp, smoothstep`
- Noise: `noise` (Perlin-like)

**Example Presets:**
1. **Spiral Wave** - Spiraling wave motion with adjustable amplitude
2. **Pulsing Grid** - Grid pattern with distance-based pulsing
3. **Orbital Dance** - Instances orbit around original position
4. **Noise Displacement** - Organic noise-based displacement

### Utilities Added (utilities.kn)

#### Preset Management Functions

**`SaveModifierPreset(actor: ClonerActor, preset_name: String) -> Bool`**
- Saves current modifier stack configuration to a preset
- Creates a new DataAsset in Content/Presets/
- Returns true on success

**`LoadModifierPreset(actor: ClonerActor, preset: ModifierPresetData) -> Bool`**
- Loads a preset and replaces current modifier stack
- Deserializes preset data and creates modifier instances
- Returns true on success

**`ApplyPresetToActor(actor: ClonerActor, preset: ModifierPresetData) -> Bool`**
- Applies a preset without clearing existing modifiers
- Adds preset modifiers to the stack
- Returns true on success

**`ValidateExpressionPreset(preset: ExpressionModifierPreset) -> Bool`**
- Validates expression syntax
- Checks for undefined variables/functions
- Returns true if valid, false with error message

**`GetPresetVariableValue(preset: ExpressionModifierPreset, variable_index: Int) -> Float`**
- Gets default value for a preset variable
- Used when initializing UI sliders

**`GetPresetVariableName(preset: ExpressionModifierPreset, variable_index: Int) -> String`**
- Gets name for a preset variable
- Used when building UI

---

## 2. Sequencer Integration

### Keyframing Functions (utilities.kn)

**`KeyModifierProperty(actor: ClonerActor, property_name: String, time: Float) -> Bool`**
- Keys a single modifier property at specified time
- Creates keyframe on active Sequencer track
- Returns true on success

**`KeyAllModifierProperties(actor: ClonerActor, time: Float) -> Bool`**
- Keys all interpolatable properties of all modifiers
- Properties include: Influence, Speed, Enabled, Position/Rotation/Scale offsets
- Returns true on success

**`GetSequencerTime() -> Float`**
- Gets current playhead time from active Sequencer
- Returns 0.0 if no Sequencer is open

**`IsSequencerOpen() -> Bool`**
- Checks if a Sequencer window is currently open
- Used to enable/disable keyframing UI

**`KeyModifierPropertyAtCurrentTime(actor: ClonerActor, property_name: String) -> Bool`**
- Convenience function that keys at current playhead
- Called by "Quick Key" buttons

**`KeyAllModifierPropertiesAtCurrentTime(actor: ClonerActor) -> Bool`**
- Convenience function that keys all properties at current playhead
- Called by "Key All" buttons

**`GetModifierPropertyValue(actor: ClonerActor, modifier_index: Int, property_name: String) -> Float`**
- Gets current value of a modifier property
- Used when creating keyframes

**`SetModifierPropertyValue(actor: ClonerActor, modifier_index: Int, property_name: String, value: Float) -> Bool`**
- Sets value of a modifier property
- Called by Sequencer evaluation during playback

### Editor UI (editor.kn)

#### Sequencer Quick Key Section
Added to `ClonerActorDetails`:

**Quick Key Info**
- Displays instructions for keyframing

**Modifier Quick Keys**
- Dynamic buttons for each modifier in the stack
- "Key All" button for each modifier
- Buttons disabled when Sequencer is closed
- Calls `KeyAllModifierPropertiesAtCurrentTime()` on click

#### Preset Management Section
Added to `ClonerActorDetails`:

**Save Preset**
- Text box for preset name
- "Save Preset" button
- Calls `SaveModifierPreset()`

**Load Preset**
- Combo box with preset list
- "Load Preset" button (replaces stack)
- "Apply Preset" button (adds to stack)

**Validate Preset**
- "Validate Expression Preset" button
- Checks expression syntax
- Displays validation results

---

## 3. Sequencer Implementation Notes

### C++ Components Required

The Sequencer integration requires manual C++ implementation (KAIN does not generate this):

**1. UMovieSceneKClonerModifierTrack**
- Inherits from `UMovieSceneNameableTrack`
- Represents the track in Sequencer timeline
- Contains one or more sections

**2. UMovieSceneKClonerModifierSection**
- Inherits from `UMovieSceneSection`
- Stores keyframe data for all modifier properties
- Contains `FMovieSceneFloatChannel`, `FMovieSceneBoolChannel`, `FMovieSceneVectorChannel`

**3. FKClonerModifierSectionTemplate**
- Inherits from `FMovieSceneEvalTemplate`
- **THE CRITICAL COMPONENT** - evaluation engine
- Called every frame during Sequencer playback
- Reads keyframe values and applies to modifiers

### Property Binding

- Each modifier has a unique `FGuid` (ModifierGuid)
- Properties identified by `FName` (PropertyName)
- Channels store keyframes as `FFrameNumber → Value` pairs
- Evaluation uses `FFrameTime` for sub-frame interpolation

### Channel Types

- **FMovieSceneFloatChannel** - Float properties (Influence, Speed, Radius, etc.)
- **FMovieSceneBoolChannel** - Bool properties (Enabled, Invert, etc.)
- **FMovieSceneVectorChannel** - FVector properties (Position, Rotation, Scale offsets)
  - Stored as 3 separate FMovieSceneFloatChannel (X, Y, Z)

### Keyframing Workflow

1. User clicks "Key All" button in Details panel
2. Calls `KeyAllModifierPropertiesAtCurrentTime()`
3. Gets current Sequencer playhead time
4. Iterates all modifiers and their Interp properties
5. Creates/updates keyframes in appropriate channels
6. Sequencer UI updates automatically

### Playback Workflow

1. Sequencer evaluates all tracks at current time
2. Calls `FKClonerModifierSectionTemplate::Evaluate()`
3. Template creates execution tokens
4. Tokens execute and call `UMovieSceneKClonerModifierSection::EvaluateAndApply()`
5. EvaluateAndApply reads channel values and sets modifier properties
6. Actor rebuilds instances with new modifier values

### Reflection Usage

- `FindPropertyByName()` to locate properties at runtime
- `CastField<FFloatProperty>()` to get typed property access
- `SetPropertyValue_InContainer()` to write values
- Handles Float, Double, Bool, and FVector types

### Performance Considerations

- Evaluation happens every frame during playback
- Only modified properties trigger actor rebuild
- `bAnyChanged` flag prevents unnecessary rebuilds
- Channel evaluation is highly optimized (binary search)

---

## 4. Reference Implementation

See the complete C++ implementation in:
- `Research/UEProj/Project_5.4/Plugins/KCloner/Source/KCloner/Public/KClonerModifierPreset.h`
- `Research/UEProj/Project_5.4/Plugins/KCloner/Source/KCloner/Private/KClonerModifierPreset.cpp`
- `Research/UEProj/Project_5.4/Plugins/KCloner/Source/KCloner/Public/KClonerSequencer.h`
- `Research/UEProj/Project_5.4/Plugins/KCloner/Source/KCloner/Private/KClonerSequencer.cpp`

---

## 5. Usage Examples

### Creating an Expression Preset

```kain
# In a CSV file for ExpressionModifierPreset DataTable:
id,display_name,category,description,position_expression,rotation_expression,scale_expression,variable_0_name,variable_0_default,variable_0_min,variable_0_max,step,speed_multiplier
1,"Spiral Wave","Motion","Creates spiraling wave motion","x += sin(t * v0 + i * v1) * v2; y += cos(t * v0 + i * v1) * v2;","ry += t * 45.0 + i * v1;","sx *= 1.0 + sin(t * v0) * 0.3; sy := sx; sz := sx;","Speed",2.0,0.1,10.0,0.1,1.0
```

### Saving a Preset in Blueprint

```cpp
// Get the cloner actor
AClonerActor* Cloner = GetClonerActor();

// Save current modifier configuration
bool Success = SaveModifierPreset(Cloner, "MyCustomPreset");
```

### Keyframing in Sequencer

```cpp
// Get the cloner actor
AClonerActor* Cloner = GetClonerActor();

// Key all modifier properties at current time
bool Success = KeyAllModifierPropertiesAtCurrentTime(Cloner);
```

---

## 6. Future Enhancements

### Planned Features
- **Preset Library Browser** - Visual browser for presets with thumbnails
- **Expression Editor** - Syntax-highlighted editor with autocomplete
- **Preset Marketplace** - Share presets with the community
- **Curve Editor Integration** - Edit keyframe curves directly in Details panel
- **Preset Blending** - Blend between multiple presets
- **Expression Debugging** - Step-by-step expression evaluation

### Known Limitations
- Expression evaluator not implemented (requires C++)
- Sequencer track editor requires manual C++ implementation
- Preset validation is basic (full validation requires C++)
- No preset thumbnails yet
- Limited to 3 user variables per preset (can be expanded)

---

## 7. Testing Checklist

- [ ] Save modifier preset with valid name
- [ ] Load modifier preset and verify stack replacement
- [ ] Apply modifier preset and verify stack addition
- [ ] Validate expression preset with correct syntax
- [ ] Validate expression preset with incorrect syntax
- [ ] Key single modifier property in Sequencer
- [ ] Key all modifier properties in Sequencer
- [ ] Scrub Sequencer timeline and verify modifier updates
- [ ] Play Sequencer and verify smooth animation
- [ ] Test preset with all 3 user variables
- [ ] Test preset with position/rotation/scale expressions
- [ ] Test preset with noise() function
- [ ] Test preset with trigonometric functions
- [ ] Test preset with step and speed_multiplier

---

## 8. Summary

The Modifier Preset System and Sequencer Integration add powerful animation and reusability features to the Cinema4DMograph plugin:

**Preset System:**
- ✅ Expression-based custom modifiers
- ✅ Save/load/apply preset functions
- ✅ Validation utilities
- ✅ 4 example presets included
- ✅ Full expression syntax documentation

**Sequencer Integration:**
- ✅ Keyframing utility functions
- ✅ "Quick Key" UI buttons
- ✅ Sequencer time queries
- ✅ Property get/set functions
- ✅ Complete implementation documentation

**What KAIN Generates:**
- Blueprint utility functions
- Editor UI (Details panels, buttons, dialogs)
- DataTable structures
- Documentation and examples

**What Requires Manual C++:**
- Expression evaluator (FKClonerExpressionEvaluator)
- Sequencer track/section/template classes
- Property reflection and binding
- Channel evaluation logic

This implementation follows the LLM-first development philosophy - KAIN generates the high-level structure and Blueprint integration, while complex UE5-specific systems (expression evaluation, Sequencer API) are documented for manual implementation.
