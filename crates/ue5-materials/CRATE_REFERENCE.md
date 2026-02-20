# UE5 Materials Crate Reference

> **Last Updated:** 2026-02-20  
> **Purpose:** Complete reference for the `ue5-materials` crate - generates UE5 material graphs and material functions  
> **Status:** Phase 2 complete - Node-based material generation with factory and binary .uasset support

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Material Graph System](#material-graph-system)
4. [Material Nodes](#material-nodes)
5. [Material Functions](#material-functions)
6. [AST Conversion](#ast-conversion)
7. [Binary Serialization](#binary-serialization)
8. [File Structure](#file-structure)
9. [Examples](#examples)

---

## Overview

The `ue5-materials` crate is the **material graph code generator** for the KAIN compiler. It transforms KAIN material definitions into production-ready Unreal Engine 5 materials with node-based graphs.

### What It Generates

- **Material Graphs** - UMaterial assets with node networks
- **Material Functions** - Reusable material subgraphs
- **Material Instances** - Parameterized material variants (future)
- **Material Parameter Collections** - Shared parameters (future)

### Key Features

- **Node-Based Graphs** - Declarative material node syntax
- **Automatic Layout** - Smart node positioning
- **Type Safety** - Compile-time type checking
- **PBR Support** - Full physically-based rendering
- **Dual Output** - Binary .uasset + C++ factory fallback
- **Material Functions** - Reusable subgraphs

---

## Architecture

### Entry Points

```rust
// Generate material from AST
pub fn generate_material(material: &MaterialGraph) -> Result<MaterialOutput>

// Generate material function
pub fn generate_material_function(func: &MaterialFunction) -> Result<MaterialFunctionOutput>

// Binary .uasset generation
pub fn serialize_material(material: &MaterialGraph) -> Result<Vec<u8>>
```

### Output Structure

```rust
pub struct MaterialOutput {
    pub header: String,              // .h file (factory)
    pub source: String,              // .cpp file (factory)
    pub uasset: Option<Vec<u8>>,     // Binary .uasset (if supported)
}
```

### Core Components

1. **MaterialGraph** - IR for material definition
2. **MaterialNode** - Individual node in graph
3. **MaterialFactory** - C++ factory generator
4. **MaterialSerializer** - Binary .uasset writer
5. **ASTConverter** - KAIN AST → Material IR

---


## Material Graph System

Materials are defined using a node-based graph syntax in KAIN.

### Basic Material

**KAIN:**
```kain
@material_graph
material SimplePBR:
    input roughness: Float = 0.5
    input metallic: Float = 0.0
    input tint: Vec3 = vec3(1.0, 1.0, 1.0)
    
    output base_color = tint
    output roughness = roughness
    output metallic = metallic
```

**Generated IR:**
```rust
MaterialGraph {
    name: "SimplePBR",
    blend_mode: BlendMode::Opaque,
    shading_model: ShadingModel::DefaultLit,
    inputs: vec![
        MaterialInput { name: "roughness", ty: Float, default: 0.5 },
        MaterialInput { name: "metallic", ty: Float, default: 0.0 },
        MaterialInput { name: "tint", ty: Vec3, default: vec3(1.0, 1.0, 1.0) },
    ],
    nodes: vec![
        // Parameter nodes
        MaterialNode::ScalarParameter { name: "roughness", value: 0.5 },
        MaterialNode::VectorParameter { name: "tint", value: vec3(1.0, 1.0, 1.0) },
    ],
    outputs: vec![
        MaterialOutput { pin: "base_color", node: "tint" },
        MaterialOutput { pin: "roughness", node: "roughness" },
        MaterialOutput { pin: "metallic", node: "metallic" },
    ],
}
```

### Material Properties

```rust
pub struct MaterialGraph {
    pub name: String,
    pub blend_mode: BlendMode,
    pub shading_model: ShadingModel,
    pub two_sided: bool,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: Vec<MaterialOutput>,
}

pub enum BlendMode {
    Opaque,
    Masked,
    Translucent,
    Additive,
    Modulate,
}

pub enum ShadingModel {
    DefaultLit,
    Unlit,
    Subsurface,
    PreintegratedSkin,
    ClearCoat,
    SubsurfaceProfile,
    TwoSidedFoliage,
    Hair,
    Cloth,
    Eye,
}
```

---


## Material Nodes

The crate supports 30+ material node types.

### Parameter Nodes

```rust
pub enum MaterialNode {
    ScalarParameter { name: String, value: f64 },
    VectorParameter { name: String, value: Vec3 },
    ColorParameter { name: String, value: Vec4 },
    TextureParameter { name: String, texture_path: String },
}
```

**KAIN:**
```kain
let roughness_param = scalar_parameter("Roughness", 0.5)
let tint_param = vector_parameter("Tint", vec3(1.0, 1.0, 1.0))
let color_param = color_parameter("Color", vec4(1.0, 0.5, 0.0, 1.0))
```

### Math Nodes

```rust
pub enum MaterialNode {
    Add { a: NodeRef, b: NodeRef },
    Subtract { a: NodeRef, b: NodeRef },
    Multiply { a: NodeRef, b: NodeRef },
    Divide { a: NodeRef, b: NodeRef },
    Lerp { a: NodeRef, b: NodeRef, alpha: NodeRef },
    Dot { a: NodeRef, b: NodeRef },
    Power { base: NodeRef, exponent: NodeRef },
    Clamp { input: NodeRef, min: NodeRef, max: NodeRef },
}
```

**KAIN:**
```kain
let sum = add(a, b)
let product = multiply(a, b)
let interpolated = lerp(a, b, 0.5)
let clamped = clamp(input, 0.0, 1.0)
```

### Utility Nodes

```rust
pub enum MaterialNode {
    ComponentMask { input: NodeRef, r: bool, g: bool, b: bool, a: bool },
    Append { a: NodeRef, b: NodeRef },
    Fresnel { exponent: NodeRef, base_reflect_fraction: NodeRef },
    ConstantFloat { value: f64 },
    ConstantVec3 { value: Vec3 },
    ConstantVec4 { value: Vec4 },
}
```

**KAIN:**
```kain
let red_channel = component_mask(color, r: true, g: false, b: false, a: false)
let vec4_result = append(vec3_value, float_value)
let fresnel_result = fresnel(3.0, 0.0)
```

### Texture Nodes

```rust
pub enum MaterialNode {
    TextureCoordinate { index: u32, tiling: Vec2 },
    TextureSample { texture: NodeRef, uv: NodeRef },
}
```

**KAIN:**
```kain
let uv = texture_coordinate(0, vec2(1.0, 1.0))
let sampled = texture_sample(albedo_map, uv)
```

---


## Material Functions

Material functions are reusable subgraphs that can be called from multiple materials.

### Basic Material Function

**KAIN:**
```kain
@material_function
fn tint_and_scale(color: Vec3, tint: Vec3, scale: Float) -> Vec3:
    let tinted = multiply(color, tint)
    let scaled = multiply(tinted, scale)
    return scaled
```

**Generated IR:**
```rust
MaterialFunction {
    name: "tint_and_scale",
    inputs: vec![
        FunctionInput { name: "color", ty: Vec3 },
        FunctionInput { name: "tint", ty: Vec3 },
        FunctionInput { name: "scale", ty: Float },
    ],
    nodes: vec![
        MaterialNode::Multiply { a: "color", b: "tint" },
        MaterialNode::Multiply { a: "tinted", b: "scale" },
    ],
    output: "scaled",
}
```

### Using Material Functions

**KAIN:**
```kain
@material_graph
material TintedMaterial:
    input base_color: Vec3 = vec3(0.8, 0.8, 0.8)
    input tint_color: Vec3 = vec3(1.0, 0.5, 0.0)
    input intensity: Float = 1.5
    
    let final_color = tint_and_scale(base_color, tint_color, intensity)
    
    output base_color = final_color
```

---

## AST Conversion

The `ast_converter` module bridges KAIN AST to Material IR.

```rust
pub fn convert_material_graph(ast: &MaterialGraphDef) -> MaterialGraph {
    let mut graph = MaterialGraph::new(&ast.name);

    // Set properties
    graph.blend_mode = convert_blend_mode(&ast.blend_mode);
    graph.shading_model = convert_shading_model(&ast.shading_model);
    graph.two_sided = ast.two_sided;

    // Convert inputs
    for input in &ast.inputs {
        graph.add_input(convert_input(input));
    }

    // Convert body (let statements → nodes)
    for stmt in &ast.body {
        if let Stmt::Let { pattern, value, .. } = stmt {
            let node = convert_expr_to_node(value);
            graph.add_node(&pattern.name, node);
        }
    }

    // Convert outputs
    for output in &ast.outputs {
        graph.add_output(&output.pin, &output.node);
    }

    graph
}
```

---


## Binary Serialization

The `material_serializer` module generates `.uasset` files directly.

```rust
pub fn serialize_material(material: &MaterialGraph) -> Result<Vec<u8>> {
    let mut asset = Asset::new_empty(EngineVersion::VER_UE5_3);

    // Add imports
    add_material_imports(&mut asset)?;

    // Create material export
    let material_export = create_material_export(&mut asset, material)?;

    // Add expression nodes
    for (name, node) in &material.nodes {
        add_expression_node(&mut asset, material_export, name, node)?;
    }

    // Wire outputs
    wire_material_outputs(&mut asset, material_export, &material.outputs)?;

    // Serialize to bytes
    let mut cursor = Cursor::new(Vec::new());
    asset.write_data(&mut cursor)?;
    Ok(cursor.into_inner())
}
```

### Expression Node Types

UE5 materials use expression nodes:

```rust
fn add_expression_node(
    asset: &mut Asset,
    material_index: PackageIndex,
    name: &str,
    node: &MaterialNode,
) -> Result<PackageIndex> {
    match node {
        MaterialNode::ScalarParameter { value, .. } => {
            create_scalar_parameter_expression(asset, material_index, name, *value)
        }
        MaterialNode::Add { a, b } => {
            create_add_expression(asset, material_index, a, b)
        }
        MaterialNode::Multiply { a, b } => {
            create_multiply_expression(asset, material_index, a, b)
        }
        // ... more node types
    }
}
```

---

## File Structure

```
crates/ue5-materials/
├── src/
│   ├── lib.rs                    # Public API
│   ├── material_graph.rs         # Material IR
│   ├── material_factory.rs       # C++ factory generator
│   ├── material_nodes.rs         # Node type definitions
│   ├── material_serializer.rs    # Binary .uasset writer
│   ├── material_function_builder.rs  # Material function support
│   └── ast_converter.rs          # KAIN AST → Material IR
├── tests/                        # Integration tests
├── Cargo.toml
├── CRATE_REFERENCE.md            # This file
├── MATERIAL_GRAPH_SYNTAX.md      # User-facing syntax guide
└── README.md                     # Quick start guide
```

---

## Examples

### Example 1: Emissive Glow Material

**KAIN:**
```kain
@material_graph(blend_mode: Additive)
material GlowMaterial:
    input glow_color: Vec3 = vec3(0.0, 1.0, 1.0)
    input glow_intensity: Float = 2.0
    
    let glow = multiply(glow_color, glow_intensity)
    
    output emissive = glow
```

**Generated:** Additive material with adjustable glow color and intensity.

### Example 2: Fresnel Rim Light

**KAIN:**
```kain
@material_graph
material FresnelRim:
    input base_color: Vec3 = vec3(0.1, 0.1, 0.1)
    input rim_color: Vec3 = vec3(0.0, 0.5, 1.0)
    input rim_power: Float = 3.0
    input rim_intensity: Float = 2.0
    
    let fresnel_value = fresnel(rim_power, constant_float(0.0))
    let rim_effect = multiply(multiply(rim_color, fresnel_value), rim_intensity)
    
    output base_color = base_color
    output emissive = rim_effect
    output roughness = constant_float(0.5)
```

**Generated:** Material with rim lighting effect for force fields, shields, holograms.

### Example 3: Tinted Metal

**KAIN:**
```kain
@material_graph
material TintedMetal:
    input base_color: Vec3 = vec3(0.8, 0.8, 0.8)
    input tint_color: Vec3 = vec3(1.0, 0.5, 0.0)
    input tint_strength: Float = 0.5
    input roughness: Float = 0.3
    
    let tinted = multiply(multiply(base_color, tint_color), tint_strength)
    let final_color = add(base_color, tinted)
    
    output base_color = final_color
    output metallic = constant_float(1.0)
    output roughness = roughness
```

**Generated:** Metallic material with adjustable color tinting.

---

## Summary

The `ue5-materials` crate provides comprehensive material graph generation with node-based syntax, automatic layout, type safety, and dual output (binary + factory). It transforms KAIN material definitions into production-ready UE5 materials with zero manual intervention.
