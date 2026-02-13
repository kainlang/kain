# kain: LLM Agent Reference Guide

## IDENTITY
kain = Multi-target compiler (WASM/LLVM/SPIR-V/HLSL/USF/JS/Rust/C++/UE5) + DUAL GODMODE (USF + UE5). Binary: `kain`. File ext: `.kn`. Syntax: Python-like with types.

## 🔥 UE5-SPECIFIC GUIDE
**For UE5 projects, use the token-optimized guide:**
- **Location:** `llm-guides/UE5_GODMODE_GUIDE.md` (~1500 tokens)
- **Example:** `llm-guides/complete_ue5_example.kn` (complete working example)
- **Contains:** UE5 attributes, networking, DataTables, Components, Actors, Blueprint functions, Shaders
- **Why:** 60-70% fewer tokens than this general guide for UE5-specific work

## CORE SYNTAX (Ultra-Compact)
```kn
// Variables
let x: Int = 42
var y = 3.14  // mutable

// Functions
fn add(a: Int, b: Int) -> Int:
    return a + b

// Pattern matching
match value:
    0 => println("zero")
    1 | 2 => println("one or two")
    n if n > 10 => println("big")
    _ => println("other")

// Structs
struct Point:
    x: Float
    y: Float

let p = Point { x: 1.0, y: 2.0 }

// Enums
enum Result:
    Ok(Int)
    Err(String)

// Arrays
let arr = [1, 2, 3, 4]
push(arr, 5)

// Shaders (3 stages: vertex/fragment/compute)
shader fragment MyShader(uv: Vec2, normal: Vec3) -> Vec4:
    uniform time: Float @0
    uniform color: Vec3 @1
    uniform tex: Sampler2D @2
    return vec4(sample(tex, uv).rgb * color, 1.0)

// Surface shaders (UE5 Material System)
shader surface PBR(uv: Vec2) -> SurfaceOutput:
    uniform roughness: Float @0
    uniform metallic: Float @1
    var out: SurfaceOutput
    out.base_color = vec3(1.0, 0.8, 0.6)
    out.roughness = roughness
    out.metallic = metallic
    return out
```

## CLI COMMANDS (kain)
```bash
# Compile targets
kain file.kn -t wasm        # WebAssembly
kain file.kn -t js          # JavaScript
kain file.kn -t rust        # Rust transpile
kain file.kn -t cpp         # C++17 transpile
kain file.kn -t ue5         # UE5 C++ (USTRUCT/UFUNCTION)
kain file.kn -t llvm        # Native binary (requires clang)
kain file.kn -t spirv       # SPIR-V shader
kain file.kn -t hlsl        # HLSL shader (DirectX)
kain file.kn -t usf         # UE5 USF + .h header (GODMODE)
kain file.kn -t hybrid      # WASM + JS loader

# Execute/test
kain file.kn -t run         # Interpret immediately
kain file.kn -t test        # Run tests

# Watch mode (auto-recompile on save)
kain file.kn -t wasm -w     # -w or --watch
kain file.kn -t usf -w -v   # Watch + verbose

# Output control
kain file.kn -t wasm -o out.wasm
kain file.kn -t usf -o shader.usf  # Also generates shader.h

# Subcommands
kain init [path]            # Initialize new project
kain build [file]           # Build from KAIN.toml or file
kain run file.kn            # Execute via interpreter
kain lsp                    # Start Language Server

# Flags
-v, --verbose                   # Verbose output
--dry-run                       # Print actions without executing
--strict                        # Treat warnings as errors
--emit-ast                      # Dump parsed AST
--emit-typed                    # Dump typed AST
```

## GODMODE PIPELINE (USF Target Only)

### Phase 1: Surface Interface
UE5 Material System integration. Use `shader surface` for materials.

### Phase 2: Permutation Engine
**Rule:** Uniforms prefixed `CFG_` or `ENABLE_` → compile-time `#ifdef`
```kn
shader fragment Optimized(uv: Vec2) -> Vec4:
    uniform CFG_HIGH_QUALITY: Float @0
    uniform ENABLE_SHADOWS: Float @1
    uniform color: Vec3 @2
    
    var result = color
    
    // Becomes #ifdef CFG_HIGH_QUALITY
    if CFG_HIGH_QUALITY:
        result = expensive_calculation(result)
    else:
        result = cheap_calculation(result)
    
    // Becomes #ifdef ENABLE_SHADOWS
    if ENABLE_SHADOWS:
        result = result * shadow_factor()
    
    return vec4(result, 1.0)
```
**Output:** Zero-cost shader variants. Desktop/mobile from single source.

### Phase 3: C++ Reflection Headers
**Auto-generates:** Type-safe UE5 bindings in `.h` file
```kn
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform time: Float @0
    uniform light_pos: Vec3 @1
    uniform albedo: Sampler2D @2
    return vec4(1.0)
```
**Generates MyShader.h:**
```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(float, time)
    SHADER_PARAMETER(FVector3f, light_pos)
    SHADER_PARAMETER_TEXTURE(Texture2D, albedo)
    SHADER_PARAMETER_SAMPLER(SamplerState, albedoSampler)
END_SHADER_PARAMETER_STRUCT()
```

### Phase 4: Interpolator Packing
**Auto-optimizes:** Vertex→Fragment data transfer. Packs into minimal `float4` registers.
```kn
// Input: position(vec3) + uv(vec2) + color(vec3) + depth(float) = 9 floats
// Naive: 4 registers (9/16 = 56% waste)
// GODMODE: 3 registers (9/12 = 25% waste)
// Savings: 25% fewer interpolators
```
**Vertex shader outputs packed automatically. Fragment shader unpacks automatically.**

## TYPE SYSTEM
```kn
// Scalars
Int, Float, Bool

// Vectors
Vec2, Vec3, Vec4
IVec2, IVec3, IVec4  // int vectors
BVec2, BVec3, BVec4  // bool vectors

// Matrices
Mat2, Mat3, Mat4

// Textures (shaders only)
Sampler2D, Sampler3D, SamplerCube

// UE5 Surface Output
SurfaceOutput:
    base_color: Vec3
    metallic: Float
    roughness: Float
    normal: Vec3
    emissive: Vec3
    opacity: Float
```

## BUILT-IN FUNCTIONS (Shaders)
```kn
// Math
sin(x), cos(x), tan(x), sqrt(x), pow(x,y), abs(x), floor(x), ceil(x)
min(a,b), max(a,b), clamp(x,min,max), mix(a,b,t), step(edge,x), smoothstep(e0,e1,x)

// Vector
dot(a,b), cross(a,b), normalize(v), length(v), distance(a,b), reflect(i,n)

// Texture
sample(sampler, uv) -> Vec4
sample_lod(sampler, uv, lod) -> Vec4

// Constructors
vec2(x,y), vec3(x,y,z), vec4(x,y,z,w)
vec3(v2, z), vec4(v3, w)  // swizzle-style
```

## BUILT-IN FUNCTIONS (General)
```kn
// I/O
print(x), println(x), read_line() -> String
read_file(path) -> String, write_file(path, content)

// Collections
push(arr, item), pop(arr) -> T, len(arr) -> Int
map(arr, fn), filter(arr, fn), reduce(arr, fn, init)

// String
split(s, delim), join(arr, delim), trim(s), replace(s, old, new)
substring(s, start, end)

// JSON
json_parse(s) -> Value, json_stringify(v) -> String

// HTTP
http_get(url) -> String, http_post(url, body) -> String

// Python FFI
py_exec(code), py_call(fn_name, args) -> Value
```

## UNIFORM BINDINGS
```kn
// @N = binding slot
uniform time: Float @0
uniform color: Vec3 @1
uniform tex: Sampler2D @2

// Permutation flags (Phase 2)
uniform CFG_MOBILE: Float @10      // #ifdef CFG_MOBILE
uniform ENABLE_FOG: Float @11      // #ifdef ENABLE_FOG
```

## EXAMPLES

### Example 1: Simple Fragment Shader
```kn
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform tint: Vec3 @0
    uniform intensity: Float @1
    return vec4(tint * intensity, 1.0)
```
**Compile:** `kain color_tint.kn -t usf -o ColorTint.usf`
**Output:** `ColorTint.usf` + `ColorTint.h`

### Example 2: Textured with Permutations
```kn
shader fragment Advanced(uv: Vec2, normal: Vec3) -> Vec4:
    uniform albedo_map: Sampler2D @0
    uniform time: Float @1
    uniform CFG_ANIMATED: Float @2
    uniform ENABLE_LIGHTING: Float @3
    
    var color = sample(albedo_map, uv).rgb
    
    if CFG_ANIMATED:
        let wave = sin(time + uv.x * 10.0) * 0.5 + 0.5
        color = color * wave
    
    if ENABLE_LIGHTING:
        let light = max(dot(normal, vec3(0,1,0)), 0.0)
        color = color * light
    
    return vec4(color, 1.0)
```
**Compile:** `kain advanced.kn -t usf`
**Result:** 4 shader variants (2^2 permutations), optimal interpolators, type-safe C++

### Example 3: UE5 Surface Shader
```kn
shader surface MetallicPaint(uv: Vec2) -> SurfaceOutput:
    uniform base_color_map: Sampler2D @0
    uniform roughness_map: Sampler2D @1
    uniform metallic: Float @2
    
    var out: SurfaceOutput
    out.base_color = sample(base_color_map, uv).rgb
    out.roughness = sample(roughness_map, uv).r
    out.metallic = metallic
    out.normal = vec3(0, 0, 1)
    out.emissive = vec3(0, 0, 0)
    out.opacity = 1.0
    return out
```

### Example 4: Compute Shader
```kn
shader compute Blur(dispatch_id: UVec3) -> Void:
    uniform input_tex: Sampler2D @0
    uniform output_tex: RWTexture2D @1
    uniform blur_radius: Float @2
    
    let uv = vec2(dispatch_id.xy) / vec2(1920, 1080)
    var sum = vec3(0, 0, 0)
    var count = 0.0
    
    for i in range(-2, 3):
        for j in range(-2, 3):
            let offset = vec2(i, j) * blur_radius
            sum = sum + sample(input_tex, uv + offset).rgb
            count = count + 1.0
    
    let result = sum / count
    write_texture(output_tex, dispatch_id.xy, vec4(result, 1.0))
```

## WATCH MODE (Hot Reload)
```bash
# Auto-recompile on file save
kain shader.kn -t usf -w

# Watch + verbose
kain shader.kn -t usf -w -v

# Watch + custom output
kain shader.kn -t wasm -o out.wasm -w
```
**Use case:** Edit shader in IDE, save, instantly see results in UE5/browser.

## INTERPRET MODE (Scripting)
```kn
// script.kn
fn main():
    println("Hello from KAIN!")
    let x = 42
    println(x)
```
```bash
kain script.kn -t run  # Execute immediately
```

## PYTHON FFI (Visual Apps)
```kn
fn main():
    let code = "
import tkinter as tk
root = tk.Tk()
root.title('KAIN App')
label = tk.Label(root, text='Hello!')
label.pack()
root.mainloop()
"
    py_exec(code)
```
**Scope persists** across multiple `py_exec()` calls.

### Python Interop Functions
```kn
py_exec(code: String)              # Execute Python code
py_call(fn: String, args: Array)   # Call Python function
# Example: py_call("math.sqrt", [16.0]) -> 4.0
```

## ACTOR SYSTEM (Erlang-style)
```kn
actor Counter:
    state count: Int = 0  // Use 'state' for actor fields
    
    on Increment(n: Int):
        count = count + n
    
    on GetCount -> Int:
        return count

fn main():
    let counter = spawn Counter()
    send counter Increment(5)
    let result = await counter GetCount()
    println(result)  # 5
```
**Features:** Message passing, async/await, channel-based concurrency  
**Note:** Actors use `state` keyword for fields, not `var`

## C++ / UE5 CODEGEN
```bash
# Generic C++17 output
kain game.kn -t cpp -o game.cpp

# UE5 C++ with USTRUCT/UENUM/UFUNCTION (generates .h + .cpp)
kain game.kn -t ue5 -o Game
# Outputs: Game.h and Game.cpp
```

### Example Input (game.kn)
```kn
struct Point:
    x: Float
    y: Float

enum Direction:
    North
    South
    East
    West

fn add(a: Int, b: Int) -> Int:
    a + b
```

### C++ Output (-t cpp)
```cpp
struct Point {
    double x;
    double y;
};

enum class Direction {
    North,
    South,
    East,
    West,
};

int64_t add(const int64_t a, const int64_t b) {
    return (a + b);
}
```

### UE5 Output (-t ue5)

**Game.h:**
```cpp
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Generated.generated.h"

USTRUCT(BlueprintType)
struct GAME_API FPoint  // Auto-prefixed with F
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float x;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float y;
};

UENUM(BlueprintType)
enum class EDirection : uint8  // Auto-prefixed with E
{
    North UMETA(DisplayName = "North"),
    South UMETA(DisplayName = "South"),
    East UMETA(DisplayName = "East"),
    West UMETA(DisplayName = "West"),
};
```

**Game.cpp:**
```cpp
#include "Generated.generated.h"

// Implementation code here
```

### UE5 Naming Conventions (Auto-applied)
- `struct Point` → `FPoint`
- `actor Player` → `APlayer` (AActor subclass)
- `enum State` → `EState`
- `@component Health` → `UHealthComponent`

## UE5 INTEGRATION WORKFLOW

### Shader Pipeline (USF)
```bash
# 1. Write shader
vim MyShader.kn

# 2. Compile with GODMODE
kain MyShader.kn -t usf -o MyShader.usf

# 3. Files generated
# - MyShader.usf (shader code)
# - MyShader.h (C++ bindings)

# 4. Copy to UE5 plugin
cp MyShader.usf /path/to/plugin/Shaders/
cp MyShader.h /path/to/plugin/Source/

# 5. Use in C++
#include "MyShader.h"
FMyShaderShader::FParameters* Params = ...;
Params->time = CurrentTime;
Params->albedo_map = MyTexture;
```

### Game Code Pipeline (UE5)
```bash
# 1. Write game code
vim MyPlugin.kn

# 2. Compile to UE5 C++
kain MyPlugin.kn -t ue5 -o MyPlugin

# 3. Files generated
# - MyPlugin.h (declarations)
# - MyPlugin.cpp (implementations)

# 4. Copy to UE5 plugin
cp MyPlugin.h /path/to/plugin/Source/Public/
cp MyPlugin.cpp /path/to/plugin/Source/Private/

# 5. Add to .Build.cs and compile
# 6. Use in Blueprints immediately!
```

## COMMON PATTERNS

### Pattern: Quality Levels
```kn
uniform CFG_QUALITY_LOW: Float @10
uniform CFG_QUALITY_HIGH: Float @11

if CFG_QUALITY_HIGH:
    // 4K textures, complex lighting
elif CFG_QUALITY_LOW:
    // 512px textures, simple lighting
```

### Pattern: Platform Variants
```kn
uniform CFG_MOBILE: Float @20
uniform CFG_DESKTOP: Float @21

if CFG_MOBILE:
    // Bandwidth-optimized
else:
    // Quality-optimized
```

### Pattern: Feature Toggles
```kn
uniform ENABLE_SHADOWS: Float @30
uniform ENABLE_REFLECTIONS: Float @31
uniform ENABLE_AO: Float @32

if ENABLE_SHADOWS:
    color = color * shadow_map()
if ENABLE_REFLECTIONS:
    color = color + reflection_probe()
if ENABLE_AO:
    color = color * ambient_occlusion()
```

### Pattern: UE5 DataTable
```kn
@datatable
struct ItemData:
    id: Int
    name: String
    icon: String
    value: Int
    stack_size: Int
```

### Pattern: UE5 Component
```kn
@component
struct HealthComponent:
    @replicated
    current: Float
    
    max: Float
    
    @transient
    regen_rate: Float
```

### Pattern: UE5 Networking
```kn
actor GameMode:
    on Server_StartMatch():
        println("Server: Match starting")
    
    on Client_ShowVictory(winner: String):
        println("Client: Winner announced")
    
    on Multicast_BroadcastScore(score: Int):
        println("All clients: Score updated")
```

### Pattern: UE5 Blueprint Functions
```kn
@blueprint
fn calculate_damage(base: Float, mult: Float, armor: Float) -> Float:
    let raw = base * mult
    let mitigated = raw * (1.0 - armor / 100.0)
    return mitigated

@blueprint
fn get_rarity_color(rarity: ItemRarity) -> Vec3:
    match rarity:
        ItemRarity::Common => return vec3(1.0, 1.0, 1.0)
        ItemRarity::Rare => return vec3(0.0, 0.5, 1.0)
        ItemRarity::Legendary => return vec3(1.0, 0.5, 0.0)
```

## PERFORMANCE NOTES
- **Permutations:** Zero runtime cost (compile-time)
- **Interpolators:** 30-50% bandwidth reduction (Phase 4)
- **Type safety:** Zero type mismatch bugs (Phase 3)
- **Development:** 3-6x faster than manual USF/HLSL

## ERROR HANDLING
```bash
# Errors show file/line/column
kain broken.kn -t usf
# Error: Type mismatch at line 12, column 5
#   Expected: Vec4
#   Found: Vec3
```

## MULTI-TARGET BUILD (KAIN.toml)
```toml
[package]
name = "my-project"
version = "0.1.0"

[targets]
wasm = { entry = "src/main.kn", output = "dist/app.wasm" }
js = { entry = "src/main.kn", output = "dist/app.js" }
```
```bash
kain build  # Builds all targets from KAIN.toml
```

## QUICK REFERENCE CARD
```
COMPILE:  kain file.kn -t <target>
TARGETS:  wasm|js|rust|cpp|ue5|llvm|spirv|hlsl|usf|hybrid|run|test
WATCH:    -w or --watch
OUTPUT:   -o <file>
VERBOSE:  -v
INIT:     kain init [path]
LSP:      kain lsp

USF GODMODE (Shaders):
  - Permutations: CFG_* or ENABLE_* uniforms
  - Auto .h generation
  - Auto interpolator packing
  - Zero-cost variants

UE5 GODMODE (Game Code):
  - @datatable → FTableRowBase (CSV import)
  - @component → UActorComponent
  - @blueprint → UBlueprintFunctionLibrary
  - Server_*/Client_*/Multicast_* → RPCs
  - Auto F/A/E/U prefixes
  - Separate .h/.cpp files

TYPES: Int Float Bool String Vec2/3/4 Mat2/3/4 Sampler2D Array
STAGES: vertex fragment compute surface
BINDINGS: uniform name: Type @N
ACTORS: spawn, send, await
PYTHON: py_exec(code), py_call(fn, args)
STDLIB: print, read_file, write_file, http_get, json_parse
```

## AGENT INSTRUCTIONS

### General Instructions
When user asks to:
1. **"Write a shader"** → Use `shader fragment/vertex/compute/surface` syntax
2. **"Compile for UE5"** → Use `-t usf` for shaders, `-t ue5` for game code
3. **"Optimize for mobile"** → Use `CFG_MOBILE` permutation pattern
4. **"Add quality levels"** → Use `CFG_QUALITY_*` permutations
5. **"Watch for changes"** → Add `-w` flag
6. **"Make it fast"** → Permutations are zero-cost, interpolators auto-packed
7. **"Create GUI app"** → Use Python FFI with tkinter
8. **"Add concurrency"** → Use actor system with spawn/send/await
9. **"Transpile to Rust"** → Use `-t rust`
10. **"Web app"** → Use `-t wasm` or `-t hybrid` for WASM+JS
11. **"Generate C++"** → Use `-t cpp` for C++17, `-t ue5` for UE5 with macros

### UE5-Specific Instructions
**For UE5 projects, refer to `llm-guides/UE5_GODMODE_GUIDE.md` for:**
12. **"UE5 Actor/Struct"** → Use `-t ue5`, auto-prefixes: F/A/E/U
13. **"UE5 DataTable"** → Use `@datatable` attribute, generates FTableRowBase
14. **"UE5 Component"** → Use `@component` attribute, generates UActorComponent
15. **"UE5 Networking"** → Use `Server_*`, `Client_*`, `Multicast_*` naming for RPCs
16. **"Blueprint functions"** → Use `@blueprint` attribute, generates UBlueprintFunctionLibrary
17. **"Complete UE5 plugin"** → See `llm-guides/complete_ue5_example.kn` for full example
18. **"UE5 best practices"** → Refer to UE5_GODMODE_GUIDE.md for patterns and workflow

## CRITICAL RULES
- Always use `kain` (NOT `kain`)
- USF target auto-generates `.h` file
- Permutation uniforms MUST start with `CFG_` or `ENABLE_`
- Binding slots `@N` must be unique per shader
- Surface shaders return `SurfaceOutput` type
- Watch mode requires file path, not stdin

## TOKEN OPTIMIZATION NOTES
This guide optimized for:
- Minimal tokens (~2000)
- Maximum information density
- Pattern-based learning (examples > prose)
- Quick reference format
- LLM-friendly structure (no fluff)

**For UE5 projects specifically:**
- Use `llm-guides/UE5_GODMODE_GUIDE.md` instead (~1500 tokens)
- Includes complete UE5 example file
- 60-70% token savings for UE5 work
- Covers: DataTables, Components, Actors, Networking, Blueprint functions, Shaders
