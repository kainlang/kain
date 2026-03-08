# KAIN Stdlib Pattern Extraction Guide

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Audience:** KAIN developers contributing to stdlib

## Table of Contents

1. [Introduction](#introduction)
2. [Extraction Methodology](#extraction-methodology)
3. [Pattern Categorization](#categorization)
4. [Prioritization Criteria](#prioritization)
5. [Documentation Standards](#documentation)
6. [Testing Requirements](#testing)
7. [Submission Process](#submission)
8. [Case Studies](#case-studies)

## Introduction

This guide documents the methodology for extracting reusable patterns from existing KAIN codebases and adding them to the standard library. The stdlib currently contains 377 functions extracted from 20 Factory plugins, kn_library/ (30+ shader files), and FluidFlow CFD shaders (50+ compute shaders).

**Extraction Goals:**
- Identify commonly repeated code patterns
- Reduce boilerplate across all plugins
- Achieve 1:20 compression ratio (20x compression)
- Maintain high code quality and documentation standards

**Extraction Sources:**
- Factory plugins (20 production UE5 plugins)
- kn_library/shaders/ (29 shader files)
- kn_library/actors/ (10 actor files)
- kn_library/components/ (5 component files)
- kn_library/utilities/ (33 utility files)
- kn_library/editor/ (7 editor files)
- FluidFlow/HyperFluidDynamics_EXPANDED.kn (50+ CFD compute shaders)

## Extraction Methodology

### Step 1: Identify Candidate Patterns

**Frequency Analysis:**
- Scan all source files for repeated code patterns
- Count occurrences of similar functions across plugins
- Identify functions appearing in 3+ plugins

**LOC Impact Analysis:**
- Measure lines of code for each pattern
- Calculate potential LOC savings per usage
- Prioritize functions saving 50+ lines per usage

**Complexity Analysis:**
- Identify complex algorithms (10+ lines)
- Identify GPU algorithms (shaders, compute)
- Identify mathematical formulas (PBR, noise, etc.)

**Tools:**
```bash
# Search for function patterns
grep -r "fn calculate_damage" Factory/*/Kain/*.kn

# Count occurrences
grep -r "fn calculate_damage" Factory/*/Kain/*.kn | wc -l

# Measure function length
sed -n '/fn calculate_damage/,/^fn /p' Factory/Example/Kain/gameplay.kn | wc -l
```

### Step 2: Analyze Pattern Variations

**Compare Implementations:**
- Read all implementations of the same pattern
- Identify common parameters and logic
- Identify variations and edge cases
- Determine most general implementation

**Example: Health Damage Calculation**

**Plugin A:**
```kain
fn apply_damage(health: Float, damage: Float) -> Float:
    return max(health - damage, 0.0)
```

**Plugin B:**
```kain
fn apply_damage(health: Float, max_health: Float, damage: Float, armor: Float) -> Float:
    let mitigated = damage * (1.0 - armor / 100.0)
    return max(health - mitigated, 0.0)
```

**Plugin C:**
```kain
fn apply_damage(health: Float, damage: Float, armor: Float, resistance: Float) -> Float:
    let mitigated = damage * (1.0 - armor / 100.0) * (1.0 - resistance / 100.0)
    return max(health - mitigated, 0.0)
```

**Stdlib Implementation (Most General):**
```kain
@blueprint
fn apply_damage(current_health: Float, max_health: Float, damage: Float, armor: Float) -> Float:
    let mitigated_damage = damage * (1.0 - armor / 100.0)
    let new_health = current_health - mitigated_damage
    return max(new_health, 0.0)
```

### Step 3: Generalize the Pattern

**Generalization Principles:**
1. **Parameterize Variations:** Make variations into parameters
2. **Use Sensible Defaults:** Provide default values where appropriate
3. **Maintain Simplicity:** Don't over-generalize (keep it usable)
4. **Preserve Semantics:** Ensure generalized version works for all cases

**Example: Generalized Remap Function**

**Specific Implementation:**
```kain
fn remap_health_to_color(health: Float, max_health: Float) -> Vec3:
    let percentage = health / max_health
    let hue = percentage * 120.0  # 0 (red) to 120 (green)
    return hsv_to_rgb(vec3(hue, 1.0, 1.0))
```

**Generalized Implementation:**
```kain
@blueprint
fn remap(value: Float, in_min: Float, in_max: Float, out_min: Float, out_max: Float) -> Float:
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min)
```

**Usage:**
```kain
let hue = remap(health, 0.0, max_health, 0.0, 120.0)
```

### Step 4: Extract and Document

**Extract Function:**
1. Copy generalized implementation
2. Add appropriate annotation (@extern or @blueprint)
3. Add comprehensive doc comment
4. Place in appropriate stdlib category file

**Documentation Template:**
```kain
/// Brief one-line description of function purpose
///
/// # Parameters
/// - param_name: Type - Description of parameter
/// - param_name2: Type - Description of parameter
///
/// # Returns
/// Type - Description of return value
///
/// # Side Effects
/// Description of side effects or "None (pure calculation)"
///
/// # Example (for complex functions)
/// ```kain
/// let result = function_name(arg1, arg2)
/// ```
///
/// # Formula (for mathematical functions)
/// Mathematical formula or algorithm description
@blueprint
fn function_name(param: Type) -> ReturnType:
    # Implementation
```

### Step 5: Test Extraction

**Testing Checklist:**
1. ✅ Function compiles without errors
2. ✅ Function generates correct C++ code
3. ✅ Function works in at least one Factory plugin
4. ✅ Function handles edge cases correctly
5. ✅ Function documentation is complete
6. ✅ Function follows naming conventions

**Test in Example Plugin:**
```kain
# Factory/Example/Kain/test_stdlib.kn
@blueprint
fn TestNewStdlibFunction():
    let result = new_stdlib_function(arg1, arg2)
    assert(result == expected_value)
```

**Compile and Validate:**
```bash
cd Factory/Example
kain build --ue5
```

## Categorization

### Category Selection Criteria

Choose the appropriate stdlib category file based on function domain:

| Category | Criteria | Examples |
|----------|----------|----------|
| **actor.kn** | AActor functions, lifecycle, transforms | GetActorLocation, SetActorRotation, DestroyActor |
| **gameplay.kn** | Game mechanics, RPG systems | apply_damage, add_experience, roll_loot_drop |
| **shaders.kn** | GPU algorithms, shader math | fresnel_schlick, fbm, ray_march_volume |
| **world.kn** | UWorld functions, spawning, traces | SpawnActor, LineTraceSingle, GetGameMode |
| **skeletal_mesh.kn** | Animation, bones, sockets | PlayAnimMontage, SetBoneLocationByName |
| **math.kn** | Mathematical operations | dot, cross, lerp, clamp |
| **utilities.kn** | Pure KAIN helpers | remap, smooth_step, format_vector |
| **particles.kn** | Niagara systems | SetNiagaraVariableFloat, ResetNiagaraSystem |
| **materials.kn** | Material parameters | CreateDynamicMaterialInstance, SetScalarParameterValue |
| **components.kn** | Component structs | TimerHandle, InputAction |
| **patterns.kn** | Game system types | LootRarity, BuffType, QuestStatus |
| **common.kn** | Type aliases | Common UE5 type aliases |

### Category Decision Tree

```
Is it a UE5 engine function?
├─ Yes → Is it actor-related?
│  ├─ Yes → actor.kn
│  ├─ No → Is it world-related?
│  │  ├─ Yes → world.kn
│  │  ├─ No → Is it animation-related?
│  │  │  ├─ Yes → skeletal_mesh.kn
│  │  │  ├─ No → Is it material-related?
│  │  │  │  ├─ Yes → materials.kn
│  │  │  │  └─ No → Is it particle-related?
│  │  │  │     ├─ Yes → particles.kn
│  │  │  │     └─ No → math.kn or world.kn
│  │  │  └─ No → Other category
│  │  └─ No → Other category
│  └─ No → Other category
└─ No → Is it a shader function?
   ├─ Yes → shaders.kn
   ├─ No → Is it gameplay logic?
   │  ├─ Yes → gameplay.kn
   │  ├─ No → Is it pure math?
   │  │  ├─ Yes → math.kn
   │  │  ├─ No → Is it a utility helper?
   │  │  │  ├─ Yes → utilities.kn
   │  │  │  ├─ No → Is it a type definition?
   │  │  │  │  ├─ Yes → patterns.kn or components.kn
   │  │  │  │  └─ No → common.kn
   │  │  │  └─ No → Other category
   │  │  └─ No → Other category
   │  └─ No → Other category
   └─ No → Other category
```

### Multi-Category Functions

Some functions could fit multiple categories. Use these tiebreakers:

1. **Primary Domain:** Choose category based on primary use case
2. **User Expectation:** Where would users expect to find it?
3. **Existing Patterns:** Follow existing stdlib organization
4. **Alphabetical:** If truly ambiguous, choose alphabetically first category

**Example: GetActorBounds**
- Could be: actor.kn (actor function) or math.kn (returns Vec3)
- Choose: actor.kn (primary domain is actors)

## Prioritization

### Extraction Priority Formula

```
Priority Score = (Frequency × 10) + (LOC_Savings / 10) + (Complexity × 5)

Where:
- Frequency: Number of plugins using this pattern (0-20)
- LOC_Savings: Lines of code saved per usage (0-1000+)
- Complexity: Complexity rating (1-10, where 10 is most complex)
```

### Priority Tiers

**Tier 1: Critical (Score 100+)**
- Appears in 10+ plugins
- Saves 100+ lines per usage
- High complexity (shader algorithms, CFD, PBR)

**Examples:**
- Shader functions (PBR, noise, volumetric rendering)
- CFD algorithms (Lattice Boltzmann, SPH)
- Complex gameplay systems (inventory, quest, loot)

**Tier 2: High (Score 50-99)**
- Appears in 5-9 plugins
- Saves 50-99 lines per usage
- Medium complexity (gameplay logic, math)

**Examples:**
- Damage calculations
- XP/leveling systems
- Cooldown management

**Tier 3: Medium (Score 20-49)**
- Appears in 3-4 plugins
- Saves 20-49 lines per usage
- Low-medium complexity (utilities, helpers)

**Examples:**
- Remapping functions
- Formatting functions
- Simple math helpers

**Tier 4: Low (Score < 20)**
- Appears in 1-2 plugins
- Saves < 20 lines per usage
- Low complexity (simple helpers)

**Examples:**
- Single-line wrappers
- Trivial calculations
- Rarely used utilities

### Extraction Order

1. **Tier 1 (Critical):** Extract immediately
2. **Tier 2 (High):** Extract in next batch
3. **Tier 3 (Medium):** Extract when time permits
4. **Tier 4 (Low):** Consider if widely applicable

### Compression Ratio Impact

Prioritize functions with highest compression ratio impact:

**Shader Functions (1:30+ compression):**
- 1 line KAIN → 30+ lines C++/USF
- Highest leverage area for compression
- Priority: CRITICAL

**Actor/Gameplay Functions (1:20-30 compression):**
- 1 line KAIN → 20-30 lines C++
- High leverage for compression
- Priority: HIGH

**Math/Utility Functions (1:10-15 compression):**
- 1 line KAIN → 10-15 lines C++
- Medium leverage for compression
- Priority: MEDIUM

## Documentation

### Documentation Requirements

All stdlib functions MUST have:

1. **Purpose Description:** One-line summary
2. **Parameters:** Name, type, description for each
3. **Return Value:** Type and description
4. **Side Effects:** Description or "None (pure calculation)"

Complex functions (10+ lines) SHOULD have:

5. **Example:** Usage example
6. **Formula:** Mathematical formula (for math functions)
7. **Note:** Additional notes or warnings

### Documentation Examples

**Simple Function:**
```kain
/// Get the actor's current world location
///
/// # Returns
/// Vec3 - The actor's location in world space
///
/// # Side Effects
/// None (read-only)
@extern
fn GetActorLocation() -> Vec3
```

**Complex Function:**
```kain
/// Calculate Fresnel reflection using Schlick's approximation
///
/// # Parameters
/// - cos_theta: Float - Cosine of angle between view and half vector
/// - f0: Vec3 - Base reflectivity at normal incidence (RGB)
///
/// # Returns
/// Vec3 - Fresnel reflection coefficient (RGB)
///
/// # Side Effects
/// None (pure calculation)
///
/// # Formula
/// F = F0 + (1 - F0) * (1 - cos_theta)^5
///
/// # Example
/// ```kain
/// let view_dir = normalize(camera_pos - surface_pos)
/// let half_vec = normalize(view_dir + light_dir)
/// let cos_theta = max(dot(view_dir, half_vec), 0.0)
/// let fresnel = fresnel_schlick(cos_theta, vec3(0.04, 0.04, 0.04))
/// ```
///
/// # Reference
/// Schlick, Christophe. "An Inexpensive BRDF Model for Physically-based Rendering." 1994.
@blueprint
fn fresnel_schlick(cos_theta: Float, f0: Vec3) -> Vec3:
    return f0 + (vec3(1.0, 1.0, 1.0) - f0) * pow(1.0 - cos_theta, 5.0)
```

### Documentation Style Guide

**Purpose Description:**
- Start with verb (Calculate, Get, Set, Apply, etc.)
- Be specific and concise
- Avoid redundancy with function name

**Good:** "Calculate Fresnel reflection using Schlick's approximation"  
**Bad:** "Fresnel schlick function that calculates fresnel"

**Parameter Descriptions:**
- Describe what the parameter represents
- Include units or ranges where applicable
- Mention constraints or requirements

**Good:** "The critical hit chance percentage (0-100)"  
**Bad:** "Crit chance"

**Return Value Descriptions:**
- Describe what the return value represents
- Include units or ranges where applicable
- Mention special cases (null, 0, etc.)

**Good:** "The new health value after damage (clamped to 0)"  
**Bad:** "Health"

**Side Effects:**
- List all side effects (I/O, state changes, random generation)
- Use "None (pure calculation)" for pure functions
- Be specific about what changes

**Good:** "Modifies actor location, may trigger collision events"  
**Bad:** "Changes stuff"

## Testing

### Testing Requirements

All extracted functions MUST:

1. ✅ Compile without syntax errors
2. ✅ Generate correct C++ code
3. ✅ Work in at least one Factory plugin
4. ✅ Handle edge cases correctly
5. ✅ Have complete documentation

### Testing Methodology

**Step 1: Syntax Testing**
```bash
# Test stdlib file parses correctly
kain build --ue5 --verbose
```

**Step 2: Integration Testing**
```kain
# Add to Factory/Example/Kain/test_stdlib.kn
@blueprint
fn TestNewFunction():
    let result = new_function(arg1, arg2)
    assert(result == expected_value)
```

**Step 3: Compilation Testing**
```bash
cd Factory/Example
kain build --ue5
```

**Step 4: Build Testing**
```bash
cd Factory/Example
FULLBUILD.bat
```

**Step 5: Edge Case Testing**
```kain
@blueprint
fn TestEdgeCases():
    # Test zero values
    assert(new_function(0.0, 0.0) == 0.0)
    
    # Test negative values
    assert(new_function(-1.0, 1.0) >= 0.0)
    
    # Test large values
    assert(new_function(1000000.0, 1000000.0) < Float::MAX)
    
    # Test boundary values
    assert(new_function(0.0, 1.0) >= 0.0)
    assert(new_function(1.0, 1.0) <= 1.0)
```

### Test Coverage Goals

- **Syntax:** 100% (all functions must parse)
- **Compilation:** 100% (all functions must compile)
- **Integration:** 80%+ (most functions tested in Example plugin)
- **Edge Cases:** 50%+ (critical functions have edge case tests)

## Submission Process

### Pre-Submission Checklist

Before submitting extracted functions:

- [ ] Function appears in 3+ plugins OR saves 50+ lines per usage
- [ ] Function is generalized to work for all use cases
- [ ] Function has appropriate annotation (@extern or @blueprint)
- [ ] Function has complete documentation (purpose, parameters, returns, side effects)
- [ ] Function is placed in correct category file
- [ ] Function compiles without errors
- [ ] Function tested in at least one Factory plugin
- [ ] Function handles edge cases correctly
- [ ] Function follows naming conventions (snake_case)
- [ ] Function doesn't duplicate existing stdlib functions

### Submission Steps

1. **Create Branch:**
   ```bash
   git checkout -b stdlib/add-new-functions
   ```

2. **Add Functions:**
   - Edit appropriate stdlib category file
   - Add functions with documentation
   - Maintain alphabetical order within sections

3. **Test Functions:**
   ```bash
   cd Factory/Example
   kain build --ue5
   FULLBUILD.bat
   ```

4. **Update Documentation:**
   - Update stdlib README with new function counts
   - Update DOCUMENTATION_STATUS.md
   - Add usage examples to USAGE_GUIDE.md if needed

5. **Commit Changes:**
   ```bash
   git add Kain/stdlib/ue5/*.kn
   git add Kain/stdlib/*.md
   git commit -m "stdlib: Add [category] functions ([count] functions)"
   ```

6. **Create Pull Request:**
   - Title: "stdlib: Add [category] functions ([count] functions)"
   - Description: List functions added, extraction sources, priority scores
   - Link to validation results

### Review Criteria

Submissions are reviewed for:

1. **Correctness:** Functions work as documented
2. **Generality:** Functions work for all use cases
3. **Documentation:** Complete and accurate documentation
4. **Testing:** Adequate test coverage
5. **Style:** Follows stdlib conventions
6. **Impact:** Provides meaningful compression ratio improvement

## Case Studies

### Case Study 1: Shader PBR Functions

**Source:** kn_library/shaders/pbr_material.kn

**Pattern Identified:**
- Fresnel calculations appear in 8+ shader files
- Each implementation is 5-10 lines
- Slight variations in formula

**Extraction Process:**
1. Analyzed all implementations
2. Chose Schlick's approximation (most common)
3. Generalized to accept f0 parameter
4. Added comprehensive documentation with formula and reference
5. Tested in 3 shader files

**Result:**
```kain
@blueprint
fn fresnel_schlick(cos_theta: Float, f0: Vec3) -> Vec3:
    return f0 + (vec3(1.0, 1.0, 1.0) - f0) * pow(1.0 - cos_theta, 5.0)
```

**Impact:**
- Used in 8+ plugins
- Saves 5-10 lines per usage
- Compression ratio: 1:30 (shader context)
- Priority Score: 80 + 5 + 40 = 125 (Tier 1: Critical)

### Case Study 2: Gameplay Damage Calculation

**Source:** Factory plugins (AeroTunnel, TitanGraph, NarrativeGraph, etc.)

**Pattern Identified:**
- Damage calculations appear in 12+ plugins
- Each implementation is 3-5 lines
- Variations in armor formula

**Extraction Process:**
1. Analyzed all implementations
2. Chose standard armor formula: damage * (1 - armor / 100)
3. Added max_health parameter for consistency
4. Added comprehensive documentation
5. Tested in 5 plugins

**Result:**
```kain
@blueprint
fn apply_damage(current_health: Float, max_health: Float, damage: Float, armor: Float) -> Float:
    let mitigated_damage = damage * (1.0 - armor / 100.0)
    let new_health = current_health - mitigated_damage
    return max(new_health, 0.0)
```

**Impact:**
- Used in 12+ plugins
- Saves 3-5 lines per usage
- Compression ratio: 1:20 (gameplay context)
- Priority Score: 120 + 4 + 25 = 149 (Tier 1: Critical)

### Case Study 3: Utility Remap Function

**Source:** Factory plugins (multiple)

**Pattern Identified:**
- Remapping values appears in 6+ plugins
- Each implementation is 1-2 lines
- Identical formula across all

**Extraction Process:**
1. Analyzed all implementations
2. Extracted standard remap formula
3. Added comprehensive documentation
4. Tested in 3 plugins

**Result:**
```kain
@blueprint
fn remap(value: Float, in_min: Float, in_max: Float, out_min: Float, out_max: Float) -> Float:
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min)
```

**Impact:**
- Used in 6+ plugins
- Saves 1-2 lines per usage
- Compression ratio: 1:10 (utility context)
- Priority Score: 60 + 1.5 + 10 = 71.5 (Tier 2: High)

## Conclusion

Pattern extraction is a systematic process of identifying, analyzing, generalizing, documenting, and testing reusable code patterns. By following this guide, you can contribute high-quality functions to the KAIN stdlib that reduce boilerplate code and improve compression ratios across all plugins.

**Key Takeaways:**
- Identify patterns appearing in 3+ plugins or saving 50+ lines
- Generalize patterns to work for all use cases
- Document thoroughly with purpose, parameters, returns, side effects
- Test in at least one Factory plugin
- Follow stdlib conventions and style guide
- Prioritize high-impact functions (shaders, gameplay, actors)

**Next Steps:**
- Review existing Factory plugins for extraction candidates
- Analyze kn_library/ files for shader patterns
- Extract high-priority functions (Tier 1: Critical)
- Test extracted functions in Example plugin
- Submit pull request with new functions

**Resources:**
- Stdlib README: `Kain/stdlib/README.md`
- Usage Guide: `Kain/stdlib/USAGE_GUIDE.md`
- Documentation Status: `Kain/stdlib/DOCUMENTATION_STATUS.md`
- Example Plugin: `Factory/Example/Kain/ultimate_showcase.kn`
- Validation Report: `Factory/Example/_Docs/STDLIB_VALIDATION_REPORT.md`

---

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Feedback:** Report issues to KAIN development team
