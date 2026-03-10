# USF Importer - Research & LLM Training Mode

## Overview

The USF importer transforms Unreal Engine 5 USF shaders into KAIN for:
- **Algorithm study and pattern analysis**
- **LLM training corpus generation** (shader patterns, techniques, optimizations)
- **Cross-compilation research** (SPIR-V, HLSL, WGSL, Metal)
- **Shader optimization technique extraction**

## Legal Notice

⚠️ **IMPORTANT**: This importer is designed for **RESEARCH and EDUCATIONAL purposes only**.

- Imported UE5 engine shaders remain **copyright Epic Games, Inc.**
- Generated KAIN files should **NOT be distributed or used commercially**
- Use this tool to **LEARN techniques**, then **implement your own versions**

## Architecture

```
USF Source → Preprocessor → Lexer → Parser → Semantic Mapper → KAIN AST
     ↓            ↓           ↓        ↓            ↓              ↓
  Strip      Expand      Tokenize  Parse     Map Types/      Generate
  Includes   Macros                HLSL      Bindings        KAIN Code
```

### Module Structure

- **`preprocessor.rs`** - Strips UE5 dependencies, expands macros
- **`parser.rs`** - HLSL/USF → KAIN AST transformation
- **`semantic_mapper.rs`** - Type/binding/semantic mapping
- **`types.rs`** - Token and AST type definitions

## Usage

### CLI (Recommended)

```bash
# ❌ Blocked by default (engine shader)
kain import-usf "C:/UE5/Engine/Shaders/Private/Lumen/LumenRadianceCache.usf"

# ✅ Research mode (adds warnings + attribution)
kain import-usf "C:/UE5/Engine/Shaders/Private/Lumen/LumenRadianceCache.usf" \
    --research \
    --output research/lumen_radiance_cache.kn

# ✅ Your own shader (no restrictions)
kain import-usf "MyPlugin/Shaders/TerrainDeform.usf" \
    --output src/terrain_deform.kn

# ✅ Flatten includes for LLM training
kain import-usf "Engine/Shaders/Private/DeferredLightingCommon.usf" \
    --research \
    --flatten-includes \
    --engine-path "C:/UE5/Engine/Shaders" \
    --output research/deferred_lighting.kn
```

### Programmatic API

```rust
use kain_import::usf::{import_usf_file, UsfImportConfig};

// Research mode with all features
let config = UsfImportConfig {
    research_mode: true,
    preserve_comments: true,
    add_attribution: true,
    llm_annotations: true,
    flatten_includes: true,
    engine_shaders_path: Some("C:/UE5/Engine/Shaders".into()),
};

let program = import_usf_file(
    Path::new("Engine/Shaders/Private/Lumen/LumenRadianceCache.usf"),
    config,
)?;

// Or use the quick helper
let program = import_for_research(
    Path::new("Engine/Shaders/Private/Lumen/LumenRadianceCache.usf"),
    Path::new("C:/UE5/Engine/Shaders"),
)?;
```

## What Gets Preserved vs Stripped

### ✅ PRESERVED (Algorithm & Semantics)

```
✅ Core algorithm logic (distance calculations, falloff curves, etc.)
✅ Mathematical operations (pow, saturate, length, dot, cross)
✅ Control flow (if/for/while, break/continue)
✅ Texture sampling patterns
✅ Compute dispatch logic
✅ Variable names (converted to snake_case)
✅ Comments (optional)
✅ Binding semantics (register slots → @N)
✅ Type information (float4 → Vec4)
```

### ❌ STRIPPED (UE5-Specific Dependencies)

```
❌ #include "/Engine/..." (engine dependencies)
❌ View.WorldToClip (UE5 uniform buffer structure)
❌ PLATFORM_SUPPORTS_* macros (expanded to 1/0)
❌ UE5-specific helper functions (unless you implement them)
❌ SceneTextures references (UE5 render target naming)
❌ Material/Primitive uniform buffers
❌ UE5 pragmas (#pragma once, etc.)
```

## Example Transformation

### Input: UE5 Lumen Shader

```hlsl
// Engine/Shaders/Private/Lumen/LumenRadianceCache.usf
#include "../Common.ush"
#include "../DeferredShadingCommon.ush"
#include "LumenRadianceCacheCommon.ush"

cbuffer RadianceCacheConstants : register(b0) {
    float4x4 WorldToClip;
    float3 ProbeSpacing;
    float MaxDistance;
};

RWStructuredBuffer<FRadianceCacheProbe> ProbeBuffer : register(u0);

[numthreads(64, 1, 1)]
void UpdateRadianceCacheCS(uint3 DispatchThreadId : SV_DispatchThreadID) {
    uint ProbeIndex = DispatchThreadId.x;
    FRadianceCacheProbe Probe = ProbeBuffer[ProbeIndex];
    
    // Epic's proprietary probe update algorithm
    float3 Irradiance = ComputeProbeIrradiance(Probe);
    UpdateSphericalHarmonics(Probe.SH, Irradiance);
    
    ProbeBuffer[ProbeIndex] = Probe;
}
```

### Output: KAIN (Research Mode)

```kain
// Imported from UE5 for research purposes only
// Original: Engine/Shaders/Private/Lumen/LumenRadianceCache.usf
// Copyright Epic Games, Inc.
// DO NOT DISTRIBUTE OR USE IN COMMERCIAL PRODUCTS

// [Stripped: ../Common.ush]
// [Stripped: ../DeferredShadingCommon.ush]
// [Stripped: LumenRadianceCacheCommon.ush]

struct RadianceCacheProbe:
    world_position: Vec3
    sh: Array<Vec3, 9>  // Spherical harmonics
    packed_data: UInt

uniform world_to_clip: Mat4 @0
uniform probe_spacing: Vec3 @0
uniform max_distance: Float @0

buffer probe_buffer: RWStructuredBuffer<RadianceCacheProbe> @0

shader compute UpdateRadianceCache(thread_id: Vec3):
    let probe_index = thread_id.x
    let probe = probe_buffer[probe_index]
    
    // Epic's proprietary probe update algorithm
    let irradiance = compute_probe_irradiance(probe)
    update_spherical_harmonics(probe.sh, irradiance)
    
    probe_buffer[probe_index] = probe
```

## LLM Training Use Cases

### 1. Pattern Extraction

```bash
# Import 500+ UE5 shaders for pattern analysis
for shader in Engine/Shaders/**/*.usf; do
    kain import-usf "$shader" --research --output "corpus/$(basename $shader .usf).kn"
done

# Result: Massive KAIN shader corpus showing:
# - Epic's coding patterns
# - Optimization techniques
# - Algorithm implementations
# - Data structure usage
```

### 2. Technique Study

```bash
# Study Lumen's radiance cache implementation
kain import-usf Engine/Shaders/Private/Lumen/*.usf --research --flatten-includes

# Study Nanite's cluster culling
kain import-usf Engine/Shaders/Private/Nanite/*.usf --research --flatten-includes

# Study temporal anti-aliasing
kain import-usf Engine/Shaders/Private/TemporalAA.usf --research
```

### 3. Cross-Platform Research

```bash
# Can Lumen run on WebGPU?
kain import-usf LumenRadianceCache.usf --research -o lumen.kn
kain build lumen.kn --target wgsl

# Can Nanite run on Metal?
kain import-usf NaniteRasterize.usf --research -o nanite.kn
kain build nanite.kn --target spirv | spirv-cross --msl
```

### 4. SPIR-V Compiler Testing

```bash
# Generate massive SPIR-V test corpus
for shader in research/*.kn; do
    kain build "$shader" --target spirv -o "spirv/$(basename $shader .kn).spv"
done

# Result: 500+ SPIR-V shaders for compiler validation
```

## Semantic Mapping Reference

### Type Mappings

| HLSL Type | KAIN Type |
|-----------|-----------|
| `float` | `Float` |
| `float2` | `Vec2` |
| `float3` | `Vec3` |
| `float4` | `Vec4` |
| `int` | `Int` |
| `uint` | `UInt` |
| `float4x4` | `Mat4` |
| `Texture2D` | `Sampler2D` |
| `RWTexture2D` | `RWTexture2D` |

### Semantic Mappings

| HLSL Semantic | KAIN Built-in |
|---------------|---------------|
| `SV_DispatchThreadID` | `thread_id` |
| `SV_GroupThreadID` | `local_thread_id` |
| `SV_GroupID` | `group_id` |
| `SV_Position` | `position` |
| `SV_VertexID` | `vertex_id` |
| `SV_Target` | `color_output` |

### Register Bindings

| HLSL Register | KAIN Binding |
|---------------|--------------|
| `register(b0)` | `@0` (cbuffer) |
| `register(t0)` | `@0` (texture) |
| `register(u0)` | `@0` (UAV/buffer) |
| `register(s0)` | Implicit (sampler) |

## Implementation Status

### ✅ Complete

- Preprocessor (macro expansion, include stripping)
- Semantic mapper (type/binding/semantic mapping)
- Parser structure (40+ methods, 1500+ lines)
- Type system (22 type mappings)
- CLI integration hooks

### 🚧 In Progress

- Lexer (HLSL tokenization)
- Full parser integration
- LLM annotation system
- Comment preservation

### 📋 Planned

- Batch import tool (import entire UE5 shader directory)
- Pattern analysis tool (extract common patterns)
- Diff tool (compare UE5 versions)
- Shader complexity metrics

## Contributing

When adding new features:
1. Preserve algorithm semantics (don't lose information)
2. Add tests for new HLSL constructs
3. Document new type/semantic mappings
4. Update this README

## References

- UE5 Shader Directory: `Engine/Shaders/`
- HLSL Reference: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl
- KAIN Shader Docs: `Kain/crates/ue5-shaders/CRATE_REFERENCE.md`
