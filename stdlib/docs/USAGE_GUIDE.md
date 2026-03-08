# KAIN Stdlib Usage Guide

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Audience:** KAIN developers building UE5 plugins

## Table of Contents

1. [Introduction](#introduction)
2. [Automatic Usage](#automatic-usage)
3. [Extending the Stdlib](#extending-the-stdlib)
4. [Overriding Stdlib Functions](#overriding-stdlib-functions)
5. [Disabling the Stdlib](#disabling-the-stdlib)
6. [Troubleshooting](#troubleshooting)
7. [Best Practices](#best-practices)
8. [Advanced Topics](#advanced-topics)

## Introduction

The KAIN standard library (stdlib) provides 377 pre-written functions automatically available in all UE5 plugin compilations. The stdlib eliminates boilerplate code and achieves 1:9 to 1:13 compression ratio (stdlib usage alone) or 1:20+ compression (combined with KAIN syntax).

**Key Features:**
- **Zero Configuration:** Works out-of-box without KAIN.toml changes
- **Automatic Discovery:** Stdlib files discovered via environment variable or filesystem walking
- **Graceful Degradation:** Compilation succeeds without stdlib if files aren't found
- **Production Validated:** 50+ functions tested in Factory/Example plugin

**Stdlib Categories:**
- actor.kn (49 functions) - Actor lifecycle, transforms, attachment
- gameplay.kn (23 functions) - Health, damage, XP, inventory, cooldowns, buffs, loot, quests
- shaders.kn (134 functions) - PBR, noise, color grading, UV, volumetric, SSS, post-processing
- world.kn (36 functions) - Time, network, spawning, debug drawing, line traces
- skeletal_mesh.kn (33 functions) - Animation, bone manipulation
- math.kn (30 functions) - Vector math, interpolation, clamping
- utilities.kn (26 functions) - Remapping, smoothing, random, formatting
- particles.kn (24 functions) - Niagara variable control
- materials.kn (22 functions) - Dynamic material instances, parameter control
- components.kn, patterns.kn, common.kn - Type definitions and aliases

## Automatic Usage

### How It Works

The stdlib is automatically loaded and prepended to your source code before compilation. You don't need to import, include, or configure anything - stdlib functions are immediately available.

**Compilation Flow:**
```
1. Stdlib Discovery (KAIN_STDLIB_PATH → exe walk → CWD walk)
2. Stdlib Loading (read all .kn files, skip READMEs, sort alphabetically)
3. Prepending (stdlib_source + "\n" + user_source)
4. Parsing & Type Checking (stdlib + user code as single program)
5. Codegen (generate C++ for stdlib function calls)
```

### Basic Example

**Your Code (Factory/MyPlugin/Kain/player.kn):**
```kain
actor Player:
    state health: Float = 100.0
    state max_health: Float = 100.0
    
    on BeginPlay():
        let location = GetActorLocation()  # stdlib function
        println("Player spawned at: {location}")
    
    on Server_TakeDamage(damage: Float, armor: Float):
        health = apply_damage(health, max_health, damage, armor)  # stdlib function
        if health <= 0.0:
            DestroyActor()  # stdlib function
```

**No imports needed!** The stdlib functions `GetActorLocation()`, `apply_damage()`, and `DestroyActor()` are automatically available.

### Discovery Mechanism

The stdlib loader uses a three-tier discovery mechanism:

#### 1. KAIN_STDLIB_PATH Environment Variable (Highest Priority)

Set this environment variable to explicitly specify the stdlib location:

**Windows:**
```cmd
set KAIN_STDLIB_PATH=M:\Code\Kain\stdlib
kain build --ue5
```

**Linux/Mac:**
```bash
export KAIN_STDLIB_PATH=/path/to/Kain/stdlib
kain build --ue5
```

The loader checks for a `ue5/` subdirectory in this path.

#### 2. Executable Location Walk (Second Priority)

The loader walks up from the `kain.exe` location looking for `stdlib/ue5/`:

```
C:\Users\Admin\.cargo\bin\kain.exe
C:\Users\Admin\.cargo\bin\
C:\Users\Admin\.cargo\
C:\Users\Admin\
C:\Users\
C:\
```

This works automatically if you installed KAIN with `cargo install`.

#### 3. Current Working Directory Walk (Third Priority)

The loader walks up from your current working directory looking for `stdlib/ue5/`:

```
M:\Code\Kain\Factory\Example\
M:\Code\Kain\Factory\
M:\Code\Kain\
M:\Code\
M:\
```

This works automatically if you run `kain build --ue5` from within the Kain repository.

#### 4. Graceful Degradation (No stdlib found)

If no stdlib directory is found, compilation proceeds without stdlib:

```
Warning: Stdlib not found, compiling without standard library
```

Your code compiles normally but stdlib functions are not available.

### Verification

To verify stdlib is loaded, check the compilation output:

```bash
kain build --ue5 --verbose
```

Look for:
```
Loading stdlib from: M:\Code\Kain\stdlib\ue5
Loaded 12 stdlib files: actor.kn, common.kn, components.kn, gameplay.kn, materials.kn, math.kn, particles.kn, patterns.kn, shaders.kn, skeletal_mesh.kn, utilities.kn, world.kn
```

## Extending the Stdlib

You can add custom functions to the stdlib for project-specific patterns.

### Adding a New Stdlib File

1. Create a new `.kn` file in `Kain/stdlib/ue5/` directory
2. Add your functions with appropriate annotations (@extern or @blueprint)
3. Document functions with doc comments
4. Rebuild your plugin with `kain build --ue5`

**Example:** `Kain/stdlib/ue5/custom.kn`
```kain
# Custom Project Functions

/// Calculate fibonacci number recursively
///
/// # Parameters
/// - n: Int - The fibonacci index
///
/// # Returns
/// Int - The nth fibonacci number
///
/// # Side Effects
/// None (pure calculation)
@blueprint
fn fibonacci(n: Int) -> Int:
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

/// Calculate factorial recursively
///
/// # Parameters
/// - n: Int - The number to calculate factorial for
///
/// # Returns
/// Int - The factorial of n
///
/// # Side Effects
/// None (pure calculation)
@blueprint
fn factorial(n: Int) -> Int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)
```

### Adding Functions to Existing Files

You can also add functions to existing stdlib files:

**Example:** Add to `Kain/stdlib/ue5/gameplay.kn`
```kain
# Add at end of file

/// Calculate poison damage over time
///
/// # Parameters
/// - base_damage: Float - Base poison damage per tick
/// - stacks: Int - Number of poison stacks
/// - duration: Float - Remaining poison duration
///
/// # Returns
/// Float - Damage to apply this tick
///
/// # Side Effects
/// None (pure calculation)
@blueprint
fn calculate_poison_damage(base_damage: Float, stacks: Int, duration: Float) -> Float:
    return base_damage * stacks * (duration / 10.0)
```

### Choosing @extern vs @blueprint

**Use @extern for:**
- Functions that exist in UE5 C++ engine code
- Engine API bindings (GetActorLocation, SpawnActor, etc.)
- Functions with no body (declaration only)

**Use @blueprint for:**
- Functions implemented in KAIN
- Pure logic functions (calculations, algorithms)
- Functions you want exposed to Blueprints
- Functions with complete implementations

**Example:**
```kain
# @extern - exists in UE5 engine
@extern
fn GetActorLocation() -> Vec3

# @blueprint - implemented in KAIN
@blueprint
fn calculate_distance_2d(a: Vec3, b: Vec3) -> Float:
    let dx = b.x - a.x
    let dy = b.y - a.y
    return sqrt(dx * dx + dy * dy)
```

### Stdlib File Organization

Follow these conventions when adding functions:

1. **Group by Category:** Place functions in appropriate category files
2. **Use Section Headers:** Organize functions into logical sections with `# Section Name` comments
3. **Document Thoroughly:** Add doc comments with purpose, parameters, returns, side effects
4. **Alphabetical Ordering:** Keep functions in alphabetical order within sections
5. **Consistent Naming:** Use snake_case for function names

## Overriding Stdlib Functions

You can override stdlib functions by defining the same function in your user code.

### How It Works

When the same function is defined in both stdlib and user code, the user code definition takes precedence. This allows you to customize stdlib behavior for specific plugins.

### Example: Override remap Function

**Stdlib Definition (stdlib/ue5/utilities.kn):**
```kain
@blueprint
fn remap(value: Float, in_min: Float, in_max: Float, out_min: Float, out_max: Float) -> Float:
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min)
```

**Your Override (Factory/MyPlugin/Kain/utilities.kn):**
```kain
# Custom remap with clamping
@blueprint
fn remap(value: Float, in_min: Float, in_max: Float, out_min: Float, out_max: Float) -> Float:
    let normalized = clamp_float((value - in_min) / (in_max - in_min), 0.0, 1.0)
    return out_min + normalized * (out_max - out_min)
```

Now all calls to `remap()` in your plugin use your custom implementation with clamping.

### When to Override

**Good Reasons to Override:**
- Add input validation or clamping
- Optimize for specific use cases
- Add logging or debugging
- Change behavior for project-specific requirements

**Bad Reasons to Override:**
- Changing function signature (breaks compatibility)
- Removing functionality (breaks other code)
- Making function less general (reduces reusability)

### Best Practices for Overriding

1. **Maintain Signature:** Keep the same parameters and return type
2. **Document Changes:** Add doc comment explaining why you overrode
3. **Test Thoroughly:** Ensure override doesn't break existing code
4. **Consider Alternatives:** Sometimes a new function is better than an override

## Disabling the Stdlib

You can disable stdlib loading for testing or debugging.

### Method 1: Set KAIN_STDLIB_PATH to Empty Directory

```bash
mkdir empty_stdlib
set KAIN_STDLIB_PATH=M:\Code\empty_stdlib
kain build --ue5
```

The loader finds the empty directory and loads no stdlib files.

### Method 2: Remove stdlib Directory

Temporarily rename or remove the `Kain/stdlib/` directory:

```bash
cd M:\Code\Kain
mv stdlib stdlib_backup
kain build --ue5
mv stdlib_backup stdlib
```

### Method 3: Compile from Isolated Directory

Run `kain build` from a directory where filesystem walking won't find stdlib:

```bash
cd C:\Temp\MyPlugin
kain build --ue5
```

If stdlib isn't in parent directories, it won't be found.

### Why Disable Stdlib?

**Testing:**
- Verify your code doesn't accidentally depend on stdlib
- Test compilation without stdlib for portability
- Debug stdlib-related issues

**Performance:**
- Reduce compilation time for large projects (stdlib adds ~377 functions to parse)
- Profile compilation with and without stdlib

**Debugging:**
- Isolate issues caused by stdlib functions
- Test custom implementations without stdlib interference

## Troubleshooting

### Issue: "Stdlib not found, compiling without standard library"

**Cause:** Stdlib directory not found in any search location

**Solutions:**
1. Set KAIN_STDLIB_PATH environment variable:
   ```bash
   set KAIN_STDLIB_PATH=M:\Code\Kain\stdlib
   ```
2. Ensure `Kain/stdlib/ue5/` directory exists
3. Run `kain build --ue5` from within Kain repository
4. Verify stdlib files exist:
   ```bash
   dir M:\Code\Kain\stdlib\ue5\*.kn
   ```

### Issue: Syntax error in stdlib file

**Cause:** Stdlib file has invalid KAIN syntax

**Error Message:**
```
Syntax error in stdlib/ue5/actor.kn:42:5: Expected 'fn' but found 'var'
```

**Solutions:**
1. Fix syntax error in stdlib file
2. Temporarily remove problematic stdlib file
3. Report issue to KAIN development team
4. Check for typos or incorrect syntax

### Issue: Type error in stdlib function

**Cause:** Stdlib function has incorrect type signature

**Error Message:**
```
Type error in stdlib function GetActorLocation in actor.kn: Expected Vec3 but found Float
```

**Solutions:**
1. Fix type signature in stdlib file
2. Ensure all type references resolve to known types
3. Check for typos in type names (Vec3 vs Vector3)
4. Verify custom types are defined before use

### Issue: Duplicate function name

**Cause:** Same function defined in multiple stdlib files

**Error Message:**
```
Duplicate function GetActorLocation found in actor.kn and world.kn
```

**Solutions:**
1. Remove duplicate function from one file
2. Rename one function to avoid conflict
3. Report issue to KAIN development team

### Issue: Shader compilation error with stdlib

**Cause:** String type validator-codegen mismatch (known issue)

**Error Message:**
```
Type 'String' should have been rejected by validator. This indicates a validator-codegen synchronization bug.
Location: crates\ue5-shaders\src\codegen_usf.rs:2206:21
```

**Workaround:**
1. Temporarily disable stdlib:
   ```bash
   set KAIN_STDLIB_PATH=M:\Code\empty_stdlib
   kain build --ue5
   ```
2. Use shader stdlib functions in separate test files
3. Wait for backend fix (update shader validator to reject String types)

**Permanent Fix (In Progress):**
- Update `ue5-shaders` crate to reject String types in shader context
- Remove String parameters from shader stdlib functions

### Issue: Stdlib function not found

**Cause:** Function doesn't exist in stdlib or typo in function name

**Error Message:**
```
Undefined function: GetActorLocaton
```

**Solutions:**
1. Check function name spelling (GetActorLocation not GetActorLocaton)
2. Verify function exists in stdlib files:
   ```bash
   grep -r "fn GetActorLocation" M:\Code\Kain\stdlib\ue5\
   ```
3. Check stdlib README for available functions
4. Ensure stdlib is loaded (check compilation output)

### Issue: Stdlib function has wrong signature

**Cause:** Function signature changed or you're using wrong parameters

**Error Message:**
```
Type error: Function GetActorLocation expects 0 parameters but got 1
```

**Solutions:**
1. Check function signature in stdlib file
2. Verify parameter types match
3. Check stdlib documentation for correct usage
4. Ensure you're calling the right function

## Best Practices

### 1. Use Stdlib Functions Whenever Possible

**Good:**
```kain
actor Player:
    on BeginPlay():
        let location = GetActorLocation()  # stdlib
        let rotation = GetActorRotation()  # stdlib
        SetActorLocation(location + vec3(0.0, 0.0, 100.0))  # stdlib
```

**Bad:**
```kain
actor Player:
    on BeginPlay():
        # Manual implementation instead of stdlib
        let location = self.location
        let rotation = self.rotation
        self.location = location + vec3(0.0, 0.0, 100.0)
```

**Why:** Stdlib functions generate correct UE5 C++ code and handle edge cases.

### 2. Combine Stdlib Functions for Complex Logic

**Good:**
```kain
@blueprint
fn CalculateDamage(base_damage: Float, armor: Float, crit_chance: Float) -> Float:
    let mitigated = calculate_armor_mitigation(base_damage, armor)  # stdlib
    if should_crit(crit_chance):  # stdlib
        return calculate_crit_damage(mitigated, 2.0)  # stdlib
    return mitigated
```

**Why:** Composing stdlib functions creates readable, maintainable code.

### 3. Document Custom Functions Like Stdlib

**Good:**
```kain
/// Calculate poison damage over time
///
/// # Parameters
/// - base_damage: Float - Base poison damage per tick
/// - stacks: Int - Number of poison stacks
///
/// # Returns
/// Float - Damage to apply this tick
@blueprint
fn calculate_poison_damage(base_damage: Float, stacks: Int) -> Float:
    return base_damage * stacks
```

**Why:** Consistent documentation makes code easier to understand and maintain.

### 4. Use Appropriate Annotations

**Good:**
```kain
# Engine binding - use @extern
@extern
fn GetActorLocation() -> Vec3

# Pure logic - use @blueprint
@blueprint
fn calculate_distance(a: Vec3, b: Vec3) -> Float:
    return distance(a, b)
```

**Bad:**
```kain
# Don't use @blueprint for engine bindings
@blueprint
fn GetActorLocation() -> Vec3:
    # This will generate duplicate C++ code!
    return vec3(0.0, 0.0, 0.0)
```

**Why:** @extern avoids generating duplicate C++ code for engine functions.

### 5. Organize Code by Category

**Good:**
```kain
# Actor lifecycle
on BeginPlay():
    let location = GetActorLocation()
    SetActorLocation(location + vec3(0.0, 0.0, 100.0))

# Combat logic
on Server_TakeDamage(damage: Float):
    health = apply_damage(health, max_health, damage, armor)
```

**Why:** Grouping related code improves readability.

### 6. Test Stdlib Usage

**Good:**
```kain
# Test stdlib functions in isolation
@blueprint
fn TestActorFunctions():
    let location = GetActorLocation()
    assert(location.x >= 0.0)
    
    SetActorLocation(vec3(100.0, 200.0, 300.0))
    let new_location = GetActorLocation()
    assert(new_location.x == 100.0)
```

**Why:** Testing ensures stdlib functions work as expected in your plugin.

## Advanced Topics

### Stdlib Loading Order

Stdlib files are loaded in alphabetical order:

1. actor.kn
2. common.kn
3. components.kn
4. gameplay.kn
5. materials.kn
6. math.kn
7. particles.kn
8. patterns.kn
9. shaders.kn
10. skeletal_mesh.kn
11. utilities.kn
12. world.kn

This ensures consistent loading across all compilations.

### Stdlib and Type Checking

Stdlib functions are type-checked along with your code:

```kain
# Type error: GetActorLocation returns Vec3, not Float
let location: Float = GetActorLocation()  # ERROR

# Correct: GetActorLocation returns Vec3
let location: Vec3 = GetActorLocation()  # OK
```

The type checker validates stdlib function calls just like user code.

### Stdlib and Codegen

Stdlib functions generate different C++ code based on annotation:

**@extern functions:**
```kain
@extern
fn GetActorLocation() -> Vec3
```

**Generated C++:** No function body, calls existing UE5 API:
```cpp
FVector location = GetActorLocation();
```

**@blueprint functions:**
```kain
@blueprint
fn apply_damage(current_health: Float, max_health: Float, damage: Float, armor: Float) -> Float:
    let mitigated_damage = damage * (1.0 - armor / 100.0)
    return max(current_health - mitigated_damage, 0.0)
```

**Generated C++:** Full function body with UFUNCTION macro:
```cpp
UFUNCTION(BlueprintCallable, Category="Gameplay")
float apply_damage(float current_health, float max_health, float damage, float armor) {
    float mitigated_damage = damage * (1.0f - armor / 100.0f);
    return FMath::Max(current_health - mitigated_damage, 0.0f);
}
```

### Stdlib and Compression Ratio

Stdlib contributes to KAIN's 1:20 compression ratio:

**Compression Layers:**
1. **KAIN Syntax (1:5):** Concise syntax vs verbose C++
2. **UE5 Codegen (1:3):** Automatic UCLASS/UPROPERTY/UFUNCTION macros
3. **Stdlib (1:1.33):** Stdlib function calls vs manual implementations

**Combined:** 1:5 × 1:3 × 1:1.33 = **1:20 compression ratio**

**Example:**
```kain
# 1 line KAIN
health = apply_damage(health, max_health, damage, armor)
```

**Generated C++ (20+ lines):**
```cpp
// Function declaration
UFUNCTION(BlueprintCallable, Category="Gameplay")
float apply_damage(float current_health, float max_health, float damage, float armor);

// Function implementation
float UMyClass::apply_damage(float current_health, float max_health, float damage, float armor) {
    float mitigated_damage = damage * (1.0f - armor / 100.0f);
    float new_health = current_health - mitigated_damage;
    return FMath::Max(new_health, 0.0f);
}

// Function call
health = apply_damage(health, max_health, damage, armor);
```

### Stdlib and Performance

**Compilation Performance:**
- Stdlib adds ~377 functions to parse (~0.5-1 second overhead)
- Negligible impact on large projects (1000+ lines)
- Can be disabled for faster iteration during development

**Runtime Performance:**
- @extern functions: Zero overhead (direct UE5 API calls)
- @blueprint functions: Inlined by C++ compiler (zero overhead)
- No performance difference vs manual implementations

### Stdlib Versioning

**Current Version:** 1.0.0

**Future Plans:**
- Semantic versioning (MAJOR.MINOR.PATCH)
- Compatibility checking (warn if stdlib version mismatch)
- Migration guides for breaking changes
- Deprecation warnings for old functions

### Stdlib and Multi-Module Plugins

Stdlib works with multi-module plugins:

**KAIN.toml:**
```toml
[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"
source_globs = ["src/runtime/**"]

[[ue5.modules]]
name = "MyPluginEditor"
type = "Editor"
depends_on = ["MyPlugin"]
source_globs = ["src/editor/**"]
```

Stdlib is loaded once and available to all modules.

## Conclusion

The KAIN stdlib provides 377 pre-written functions that eliminate boilerplate code and achieve 1:20 compression ratio. The stdlib works automatically with zero configuration, can be extended with custom functions, and can be overridden for project-specific needs.

**Key Takeaways:**
- Stdlib is automatically loaded and prepended to your code
- Use stdlib functions whenever possible for cleaner code
- Extend stdlib with custom functions for project-specific patterns
- Override stdlib functions when you need custom behavior
- Disable stdlib for testing or debugging
- Follow best practices for consistent, maintainable code

**Next Steps:**
- Review stdlib README for available functions
- Check Factory/Example plugin for usage examples
- Add custom functions to stdlib for your project
- Report issues or suggest improvements to KAIN development team

**Resources:**
- Stdlib README: `Kain/stdlib/README.md`
- Documentation Status: `Kain/stdlib/DOCUMENTATION_STATUS.md`
- Pattern Extraction Guide: `Kain/stdlib/PATTERN_EXTRACTION_GUIDE.md`
- Example Plugin: `Factory/Example/Kain/ultimate_showcase.kn`
- Validation Report: `Factory/Example/_Docs/STDLIB_VALIDATION_REPORT.md`

---

**Version:** 1.0.0  
**Last Updated:** 2026-01-XX  
**Feedback:** Report issues to KAIN development team
