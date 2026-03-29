# KAIN Material Graph Showcase - Complete Feature Reference

> **Generated:** 2026-02-20  
> **Source Analysis:** Comprehensive scan of `Kain/crates/ue5-materials/src/`  
> **Purpose:** Document EVERY feature supported by the ue5-materials crate with code evidence

---

## Table of Contents

1. [Overview](#overview)
2. [Material Node Types (50+)](#material-node-types)
3. [Material Properties](#material-properties)
4. [Advanced Features](#advanced-features)
5. [Binary Asset Generation](#binary-asset-generation)
6. [Code Evidence Index](#code-evidence-index)

---

## Overview

The `ue5-materials` crate provides comprehensive material graph generation for UE5. This document catalogs **every feature** found in the crate with file:line references proving each capability exists.

### Crate Structure

```
ue5-materials/src/
├── material_graph.rs (493 lines)    - IR definitions, 50+ node types
├── ast_converter.rs (2048 lines)    - KAIN AST → Material IR conversion
├── material_nodes.rs (128 lines)    - Node builder helpers
├── material_factory.rs (62.6KB)     - C++ factory code generation
├── material_serializer.rs (71.2KB)  - Binary .uasset generation
├── material_function_builder.rs     - Material function support
└── lib.rs                           - Public API
```

---

## Material Node Types

### Category 1: Parameter Nodes

Parameters expose values to the material editor and can be modified at runtime.


#### 1.1 ScalarParameter
**File:** `material_graph.rs:90`  
**Type:** `MaterialNodeType::ScalarParameter { name: String, default: f32 }`  
**Usage:** `input roughness: Float = 0.5`  
**Generates:** `UMaterialExpressionScalarParameter` with editable float value

#### 1.2 VectorParameter
**File:** `material_graph.rs:91`  
**Type:** `MaterialNodeType::VectorParameter { name: String, default: [f32; 3] }`  
**Usage:** `input tint: Vec3 = vec3(1.0, 1.0, 1.0)`  
**Generates:** `UMaterialExpressionVectorParameter` with RGB color picker

#### 1.3 ColorParameter
**File:** `material_graph.rs:92`  
**Type:** `MaterialNodeType::ColorParameter { name: String, default: [f32; 4] }`  
**Usage:** `input color: Vec4 = vec4(1.0, 0.5, 0.0, 1.0)`  
**Generates:** `UMaterialExpressionVectorParameter` with RGBA color picker

---

### Category 2: Texture Nodes

Texture sampling and UV coordinate generation.

#### 2.1 TextureSampleParameter2D
**File:** `material_graph.rs:89`  
**Type:** `MaterialNodeType::TextureSampleParameter2D { param_name, default_texture, uv_input }`  
**Usage:** `input albedo_map: Sampler2D` + `let albedo = sample(albedo_map)`  
**Generates:** `UMaterialExpressionTextureSampleParameter2D`  
**Converter:** `ast_converter.rs:561-613` - Handles `sample()` function calls

#### 2.2 TextureSample
**File:** `material_graph.rs:88`  
**Type:** `MaterialNodeType::TextureSample { texture_input, uv_input }`  
**Usage:** `let sampled = texture_sample(texture, uv)`  
**Generates:** `UMaterialExpressionTextureSample`

#### 2.3 TextureCoordinate
**File:** `material_graph.rs:128`  
**Type:** `MaterialNodeType::TextureCoordinate { index: u32, tiling: [f32; 2] }`  
**Usage:** Auto-created when `sample()` called without UV argument  
**Generates:** `UMaterialExpressionTextureCoordinate`  
**Deduplication:** `ast_converter.rs:1246-1272` - Only one default UV node per material

---

### Category 3: Binary Math Operations

Basic arithmetic operations on scalars and vectors.

