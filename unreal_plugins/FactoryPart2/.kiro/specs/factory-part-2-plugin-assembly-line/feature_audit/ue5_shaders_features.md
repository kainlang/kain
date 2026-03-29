# ue5-shaders Features Audit

> **Crate:** `Kain/crates/ue5-shaders`
> **Status:** Production - Largest UE5 codegen crate
> **Last Updated:** 2026-03-02

---

## Overview

The ue5-shaders crate generates UE5 Shader Format (`.usf`) code from KAIN `shader` items. It produces:
- `.usf` shader source files (HLSL/USF)
- C++ `FGlobalShader` / `FMeshMaterialShader` subclasses
- Dispatch helper functions
- POD mirror structs for CPU→GPU parameter passing

**Total Size:** ~394KB across 5 core files

---

## Feature Categories

### 1. Shader Types

#### 1.1 Compute Shaders
**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
shader compute VoxelGenerator(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2
    let noise = perlin_noise(thread_id * noise_scale)
    output[thread_id.x] = noise
```

**Generated Output:**
- `.usf` file with `[numthreads(X,Y,Z)]` compute shader entry point
- `FGlobalShader` subclass with parameter binding
- Dispatch helper: `void Dispatch_VoxelGenerator(FRHICommandListImmediate& RHICommandList, int X, int Y, int Z)`
- UAV resource transitions handled automatically

**Factory Part 1 Examples:**
- **Materialize** (30+ compute shaders): `GradientCS`, `HeightIntegrationCS`, `FinalPBRCS`, `BlurHorizontalCS`, `BlurVerticalCS`, `SharpenCS`, `EdgeDetectCS`, `LevelsCS`, `HSLAdjustCS`, `InvertCS`, `GrayscaleCS`, `GenerateNoiseCS`, `SeamlessCS`, `PackORMCS`, `LayerBlendCS`, `ProceduralNoiseCS`, `TextureCombineCS`, `UVTransformCS`, `ColorSpaceConvertCS`, `NormalMapConvertCS`
- **Materialize** particle system: `ParticleSpawn`, `ParticleUpdate`, `ParticleRender`
- **VoxelForgePro** (19 GPU compute shaders): Terrain generation, voxel processing

**Key Features:**
- Auto thread group size calculation (max 1024 threads)
- RWTexture2D and RWStructuredBuffer support
- UAV binding slot management
- Texture coordinate normalization
- Simulation texture point sampling via `Load()`

---

#### 1.2 Fragment Shaders (Pixel Shaders)
**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform albedo_map: Sampler2D @1
    let tex_color = sample(albedo_map, uv).rgb
    return vec4(tex_color * base_color, 1.0)
```

**Generated Output:**
- `.usf` file with `void Name_PS(FPSInput In, out FPSOutput Out)` signature
- `FGlobalShader` subclass
- Input/output semantics: `TEXCOORD0`, `SV_Target`

**Factory Part 1 Examples:**
- **UltimateVFX** (16 fragment shaders): `AtmosphericScattering`, `VolumetricClouds`, `OceanRendering`, `VolumetricFog`, `GodRays`, `BloomLensFlare`, `ScreenSpaceReflections`, `AmbientOcclusion`, `DepthOfField`, `MotionBlur`, `ColorGrading`, `ChromaticAberration`, `FilmGrain`, `Sharpen`, `RainDrops`, `ProceduralSky`
- **Materialize** (10+ fragment shaders): `GlossyClearCoatPS`, `GlossyDualLobePS`, `GlossySubsurfacePS`, `MetalAnisotropicSpecularPS`, `MetalFresnelRimPS`

**Key Features:**
- UV coordinate input handling
- Texture sampling with `Texture2D` + `SamplerState`
- Screen-space effects support
- Post-processing pipeline integration

---

#### 1.3 Vertex Shaders
**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
shader vertex TransformVertex(pos: Vec3) -> Vec4:
    uniform mvp_matrix: Mat4 @0
    return mvp_matrix * vec4(pos, 1.0)
```

**Generated Output:**
- `.usf` file with `void Name_VS(FVSInput In, out FVSOutput Out)` signature
- `FGlobalShader` subclass
- Input semantics: `POSITION`, `NORMAL`, `TEXCOORD0`
- Output semantics: `SV_Position`

**Factory Part 1 Examples:**
- Limited direct vertex shader usage (most use surface shaders or material graphs)

---

#### 1.4 Surface Shaders
**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
shader surface PBRSurface:
    uniform roughness: Float @0
    uniform metallic: Float @1
    base_color = vec3(0.8, 0.8, 0.8)
    roughness = roughness
    metallic = metallic
```

**Generated Output:**
- Surface expression graph
- `FMeshMaterialShader` subclass
- Material input connections (BaseColor, Roughness, Metallic, Normal, etc.)

**Factory Part 1 Examples:**
- Used in conjunction with material graphs for PBR workflows

---

### 2. Uniform Classification

**Status:** ✅ Full Support

The shader compiler automatically classifies uniforms into two categories:

#### 2.1 Texture Uniforms
**Types:** `Sampler2D`, `Texture2D`, `RWTexture2D`, `RWTexture3D`

**Generated Code:**
```hlsl
Texture2D albedo_map : register(t0);
SamplerState albedo_mapSampler : register(s0);
```

**Behavior:**
- Excluded from dispatch signature (bound separately via RHI)
- Auto-generates `SamplerState` for `Sampler2D` types
- Register binding slot validation

---

#### 2.2 Scalar Uniforms
**Types:** `Float`, `Vec2/3/4`, `Int`, `UInt`, `Bool`, `Mat4`

**Generated Code:**
```hlsl
cbuffer ShaderConstants : register(b0) {
    float base_color;
    int grid_size;
    float4x4 mvp_matrix;
};
```

**Behavior:**
- Packed into constant buffer
- Included in dispatch signature
- 16-byte alignment validation

---

### 3. Shader Permutations

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
shader compute FogCompute(thread_id: Vec3):
    uniform CFG_ENABLE_FOG: Bool @3
    uniform ENABLE_SHADOWS: Bool @4
    
    if CFG_ENABLE_FOG:
        apply_fog()
    
    if ENABLE_SHADOWS:
        calculate_shadows()
```

**Generated Code:**
```hlsl
#define ENABLE_FOG_PERMUTATION_BOOL
SHADER_PERMUTATION_BOOL("ENABLE_FOG");

#define ENABLE_SHADOWS_PERMUTATION_BOOL
SHADER_PERMUTATION_BOOL("ENABLE_SHADOWS");
```

**Key Features:**
- Zero runtime cost (compile-time branches)
- Prefix detection: `CFG_*` or `ENABLE_*`
- Multiple permutations per shader
- Automatic shader variant generation

**Factory Part 1 Examples:**
- Used in VoxelForgePro for terrain generation variants
- Used in UltimateVFX for quality level permutations

---

### 4. Shared Shader Libraries (.ush)

**Status:** ✅ Full Support

**Behavior:**
When a plugin has >1 shader, a `{PluginName}Common.ush` is auto-generated containing shared math helpers.

**Auto-Generated Functions:**
```hlsl
// Bounds checking
bool IsInBounds(float3 pos, float3 bounds);

// UV utilities
float2 PixelToUV(float2 pixel, float2 texSize);

// Noise functions
float HashNoise(float3 pos);

// Color utilities
float Grayscale(float3 color);
```

**Factory Part 1 Examples:**
- **Materialize**: `MaterializeCommon.ush` with 20+ shared functions
- **VoxelForgePro**: `VoxelForgeProCommon.ush` with voxel utilities
- **UltimateVFX**: `UltimateVFXCommon.ush` with atmospheric functions

**Key Features:**
- Automatic deduplication across shaders
- Included after `Platform.ush`
- Plugin-scoped namespace

---

### 5. Validation System

**Status:** ✅ Full Support (136KB validation.rs)

#### 5.1 Thread Group Size Validation
**Rule:** X × Y × Z ≤ 1024 total threads

**Error Example:**
```
Error: Compute shader 'LargeCompute' exceeds max thread group size
  [numthreads(32,32,2)] = 2048 threads (max: 1024)
```

---

#### 5.2 Binding Slot Uniqueness
**Rule:** No duplicate `@slot` assignments

**Error Example:**
```
Error: Duplicate binding slot @2 in shader 'MyShader'
  uniform texture_a: Texture2D @2
  uniform texture_b: Texture2D @2  // ❌ Conflict
```

---

#### 5.3 UAV Type Consistency
**Rule:** `RWTexture2D` and `RWBuffer` cannot share slot with SRV

**Error Example:**
```
Error: UAV/SRV slot conflict at @1
  uniform input_tex: Texture2D @1      // SRV
  uniform output_tex: RWTexture2D @1   // UAV ❌
```

---

#### 5.4 POD Struct Validation
**Rule:** Shader parameter structs must be plain data (no virtuals, no strings)

**Valid:**
```kain
struct ParticleData:
    position: Vec3
    velocity: Float
    flags: Int
```

**Invalid:**
```kain
struct ParticleData:
    name: String  // ❌ Not POD
    position: Vec3
```

---

#### 5.5 Conditional Shader Directory Mapping
**Rule:** Duplicate file path assertions prevented

**Behavior:**
- Guards against multiple shaders writing to same `.usf` file
- Validates shader name uniqueness per plugin

---

### 6. POD Mirror Structs

**Status:** ✅ Full Support (30KB pod_mirror.rs)

**Purpose:** Generate C++ structs for CPU→GPU data upload

**KAIN Input:**
```kain
struct ParticleData:
    position: Vec3
    velocity: Float
    flags: Int
```

**Generated C++:**
```cpp
struct FParticleData_GPUMirror {
    FVector3f Position;   // float3 packed
    float     Velocity;   // float scalar
    uint32    Flags;      // uint bitfield
};
static_assert(sizeof(FParticleData_GPUMirror) % 16 == 0, "Must be 16-byte aligned");
```

**Key Features:**
- 16-byte alignment enforcement
- Field order preservation
- Type mapping validation
- No non-POD members allowed

**Factory Part 1 Examples:**
- **Materialize**: `FParticleData_GPUMirror` for particle system
- **VoxelForgePro**: `FVoxelData_GPUMirror` for voxel data

---

### 7. Type Mapping

**Status:** ✅ Full Support (10KB type_mapping.rs)

**Complete Type Map:**

| KAIN Type | HLSL/USF Type |
|-----------|---------------|
| `Float` | `float` |
| `Vec2` | `float2` |
| `Vec3` | `float3` |
| `Vec4` | `float4` |
| `Mat4` | `float4x4` |
| `Int` | `int` |
| `UInt` | `uint` |
| `Bool` | `bool` |
| `Sampler2D` | `Texture2D` + `SamplerState` |
| `Texture2D` | `Texture2D` |
| `RWTexture2D` | `RWTexture2D<float4>` |
| `RWTexture3D` | `RWTexture3D<float4>` |
| `Buffer<T>` | `StructuredBuffer<T>` |
| `RWBuffer<T>` | `RWStructuredBuffer<T>` |

**Key Features:**
- Pointer detection for UObject-derived types
- Generic type expansion (`Array<T>` → `TArray<T>`)
- Engine type recognition

---

### 8. Shader Knowledge System

**Status:** ✅ Full Support (19KB shader_knowledge.rs)

**Data-Driven Registry:**
- Supported input/output semantics per shader stage
- Built-in HLSL intrinsic function signatures
- Valid parameter attribute combinations
- Engine-specific USF includes by usage category

**Metadata Location:** `Kain/unreal/metadata/shader_knowledge.json`

---

## Feature Coverage Summary

| Feature | Status | Factory Part 1 Usage |
|---------|--------|---------------------|
| Compute Shaders | ✅ Full | 50+ shaders across 5 plugins |
| Fragment Shaders | ✅ Full | 30+ shaders across 3 plugins |
| Vertex Shaders | ✅ Full | Limited direct usage |
| Surface Shaders | ✅ Full | Used with material graphs |
| Shader Permutations | ✅ Full | VoxelForgePro, UltimateVFX |
| Shared Libraries (.ush) | ✅ Full | All multi-shader plugins |
| Uniform Classification | ✅ Full | All shaders |
| POD Mirror Structs | ✅ Full | Materialize, VoxelForgePro |
| Type Mapping | ✅ Full | All shaders |
| Validation System | ✅ Full | All shaders |

---

## Known Limitations

1. **No geometry shader support** - UE5 rarely uses geometry shaders
2. **No tessellation shader support** - Not yet implemented
3. **Limited ray tracing support** - DXR shaders not yet supported

---

## Test Coverage

**85 tests passing** covering:
- Compute shader codegen
- Fragment shader codegen
- Vertex shader codegen
- Surface shader codegen
- Shader permutations
- Shared library generation
- Uniform classification
- POD mirror struct generation
- Type mapping
- Validation rules

---

## Factory Part 1 Plugin Examples

### Materialize (30+ compute shaders)
- Image processing pipeline
- Particle system (spawn, update, render)
- PBR material generation
- Seamless texture generation
- Layer blending and filtering

### VoxelForgePro (19 compute shaders)
- Terrain generation
- Voxel processing
- Noise generation
- Marching cubes

### UltimateVFX (16 fragment shaders)
- Atmospheric scattering
- Volumetric clouds and fog
- Ocean rendering
- Post-processing effects
- Screen-space reflections

---

## Crate Files

| File | Size | Purpose |
|------|------|---------|
| `codegen_usf.rs` | 189KB | Main USF codegen |
| `validation.rs` | 136KB | Shader validation |
| `pod_mirror.rs` | 30KB | POD struct generation |
| `shader_knowledge.rs` | 19KB | Data-driven registry |
| `type_mapping.rs` | 10KB | Type mapping |

**Total:** ~394KB
