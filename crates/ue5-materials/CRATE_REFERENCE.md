# ue5-materials — UE5 Material Graph Codegen Reference

> **Last Updated:** 2026-03-01
> **Status:** Core material graph codegen functional. Binary `.uasset` serialization active. Some stale AST field references require fixes.

---

## Purpose

Generates UE5 Material assets from KAIN `material` items. Produces both C++ factory code (for editor registration) and direct binary `.uasset` serialization (for production use without editor).

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `ast_converter.rs` | 99KB | `MaterialAstConverter` — KAIN AST → material node graph IR |
| `material_serializer.rs` | 71KB | Binary `.uasset` writer — UE5 asset format |
| `material_factory.rs` | 63KB | C++ `UMaterialFactoryNew` + `UMaterial` setup code |
| `material_function_builder.rs` | 33KB | Reusable `UMaterialFunction` asset generation |
| `material_graph.rs` | 17KB | `MaterialGraph` IR — node/connection/property graph |
| `material_nodes.rs` | 4KB | Node type enum + metadata |
| `bin/uasset_scan.rs` | 6.4KB | CLI tool: scan a `.uasset` file and decode its structure |

---

## KAIN Material Syntax

```kain
material PBRGround:
    input albedo: Texture2D
    input roughness: Float = 0.5
    input normal_map: Texture2D
    
    base_color = texture_sample(albedo).rgb
    roughness = roughness
    normal = unpack_normal(texture_sample(normal_map).rgb)
    metallic = 0.0
```

---

## Material Node Types (`material_graphs.rs`)

30+ supported material node types:

| KAIN expression | UE5 Node |
|---|---|
| `texture_sample(tex)` | `UMaterialExpressionTextureSample` |
| `lerp(a, b, t)` | `UMaterialExpressionLinearInterpolate` |
| `clamp(x, lo, hi)` | `UMaterialExpressionClamp` |
| `dot(a, b)` | `UMaterialExpressionDotProduct` |
| `normalize(v)` | `UMaterialExpressionNormalize` |
| `time()` | `UMaterialExpressionTime` (deduplication — only one per material) |
| `sine(x)`, `cosine(x)` | `UMaterialExpressionSine` / `UMaterialExpressionCosine` |
| `uv_scroll(uv, speed)` | UV + Time + Add chain |
| `uv_scale(uv, scale)` | `UMaterialExpressionMultiply` on UV |
| `uv_rotate(uv, angle)` | Rotation matrix × UV chain |
| `custom_hlsl(code, inputs)` | `UMaterialExpressionCustom` |
| `call_shader(shader_fn)` | Shader function integration node |
| `.r` / `.g` / `.b` / `.a` / `.rgb` | `UMaterialExpressionComponentMask` |
| `abs`, `pow`, `sqrt`, `exp`, `log` | Math expression nodes |
| `floor`, `ceil`, `round`, `frac`, `saturate` | Corresponding UE5 math nodes |
| Scalar constants | `UMaterialExpressionScalarParameter` |
| Vector constants | `UMaterialExpressionVectorParameter` |
| `distance(a, b)` | Distance expression |
| `cross(a, b)` | Cross product expression |
| `length(v)` | Length expression |

### UV Chaining

UV manipulation nodes can be chained:
```kain
base_color = texture_sample(albedo, uv_scroll(uv_scale(uv, 2.0), 0.1)).rgb
```
→ UV → Scale → Scroll → TexSample chain.

### Time Deduplication

Multiple uses of `time()` in one material automatically share a single `UMaterialExpressionTime` node.

---

## Binary `.uasset` Serializer (`material_serializer.rs`, 71KB)

Direct binary serialization without requiring the UE5 editor:

- UE5 asset file header with engine version parameterization (5.0 through 5.4+)
- `UMaterial` object export with all property types
- Material expression graph nodes as `UObject` exports
- Connection wiring via `FExpressionInput` / `FExpressionOutput` connection records
- Dynamic material flag auto-marking

Supports 14 property types: Bool, Int, Float, String, Object, Class, Soft Object, Enum, Struct, Array, Map, Set, Name, Text.

---

## C++ Factory (`material_factory.rs`, 63KB)

For cases where binary serialization is insufficient (complex materials, large graphs), generates C++ factory code:

```cpp
UCLASS()
class UPBRGroundFactory : public UMaterialFactoryNew {
    GENERATED_BODY()
public:
    UPBRGroundFactory();
    virtual UObject* FactoryCreateNew(...) override;
};
```

Includes setup for all material parameters, expressions, and connections.

---

## Known Issues

> **⚠️ Stale AST Field References**
> Some code in `ast_converter.rs` references older AST field names that no longer match the current `kain-core` AST. Specific locations:
> - Lines that reference deprecated `Function::body` as `Option<Block>` — now always `Block`
> - Lines that access removed `Shader::uniforms` field — now accessed via `Shader::params`
> - Some `Item::Shader` destructuring patterns for the old `kind: ShaderKind` enum variant

These cause compilation errors in `ue5-materials` that block the crate from building. Fix requires updating the field access sites to use current AST field names.

---

## `uasset_scan` Binary

The crate includes a standalone binary at `bin/uasset_scan.rs` (6.4KB) for inspecting generated `.uasset` files:

```bash
cargo run --bin uasset_scan -- path/to/material.uasset
```

Decodes and pretty-prints the asset file structure.
