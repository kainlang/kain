# Materials Codegen Fix Sprint

> **Date:** February 20, 2026
> **Crate:** `crates/ue5-materials/`
> **Source Audit:** Manual review — 11 bugs found, 3 critical
> **Recommended:** 2 agents (Agent A: Critical fixes, Agent B: Significant fixes)

---

## Priority Map

| # | Severity | Issue | Agent |
|---|---|---|---|
| 1 | 🔴 Critical | `ast_converter.rs` disabled — no KAIN source → IR path | A |
| 2 | 🔴 Critical | CustomHLSL inputs all hardwired to node 0 | A |
| 3 | 🔴 Critical | `TextureSample` serialized as `TextureSampleParameter2D` | A |
| 4 | 🟡 Significant | Unreachable duplicate match arms in `material_factory.rs` | B |
| 5 | 🟡 Significant | UVScroll/UVScale parse node IDs as floats | B |
| 6 | 🟡 Significant | `ColorParameter` alpha channel dropped | B |
| 7 | 🟡 Significant | `material_package_import` stored but never used | B |
| 8 | 🟢 Minor | `MaterialFunctionBuilder` duplicates `MaterialAssetBuilder` | B |
| 9 | 🟢 Minor | `convert_function_node` incomplete match | B |
| 10 | 🟢 Minor | C++ factory TODO placeholders for UV/layer nodes | B |
| 11 | 🟢 Minor | Engine target hardcoded in `serialize_material_graph()` | B |

---

## AGENT A — Critical Fixes (Binary Path Correctness)

### Fix 1: Re-enable `ast_converter.rs`

**File:** `crates/ue5-materials/src/lib.rs` line 6-7

**Current:**
```rust
// pub mod ast_converter;  // TODO: Fix test compilation errors (Span is private, CallArg needs span field)
```

**Fix:**
```rust
pub mod ast_converter;
```

Then fix the two compile errors the comment describes:

**Error A — `Span` is private:**
- Find where `ast_converter.rs` uses `Span` directly
- Use `kain_core::span::Span` with the correct public import path
- OR replace with `0..0` range if span is only used for error reporting

**Error B — `CallArg` needs `span` field:**
- Find the `CallArg` struct usage in `ast_converter.rs`
- Add `.span: Span::default()` or `Span { start: 0, end: 0 }` to each `CallArg` construction

After re-enabling, verify `ast_converter::convert_kain_material()` can be called from the pipeline in `cli/src/lib.rs` or wherever material AST nodes are processed.

---

### Fix 2: CustomHLSL inputs wired to node 0

**File:** `crates/ue5-materials/src/material_serializer.rs` lines 1506-1510

**Current (broken — always 0):**
```rust
let resolved: Vec<(String, usize)> = inputs
    .iter()
    .map(|ci| Ok((ci.name.clone(), 0usize))) // Custom inputs need resolution
    .collect::<Result<Vec<_>, String>>()?;
```

**Fix:** Look up each input by its node ID in `node_map`:
```rust
let resolved: Vec<(String, usize)> = inputs
    .iter()
    .map(|ci| {
        let node_idx = node_map.get(&ci.node_id)
            .copied()
            .ok_or_else(|| format!("CustomHLSL input '{}' references unknown node '{}'", ci.name, ci.node_id))?;
        Ok((ci.name.clone(), node_idx))
    })
    .collect::<Result<Vec<_>, String>>()?;
```

**Note:** Check the `CustomInput` struct to confirm the field is called `node_id` — if it's a different field name, adjust accordingly. The key point is: look it up in `node_map` instead of hardcoding `0usize`.

---

### Fix 3: `TextureSample` serialized as parameter

**File:** `crates/ue5-materials/src/material_serializer.rs` lines 1412-1418

**Current (wrong — uses parameter variant):**
```rust
MaterialNodeType::TextureSample { uv_input, .. } => {
    let uv = uv_input
        .as_ref()
        .and_then(|id| node_map.get(id))
        .copied();
    Ok(builder.add_texture_sample_parameter("TextureSample", uv))
}
```

**Fix — add a proper non-parameter texture sample method to `MaterialAssetBuilder`:**

In `crates/ue5-materials/src/material_serializer.rs` (or wherever `MaterialAssetBuilder` is defined), add:
```rust
pub fn add_texture_sample_node(&mut self, uv_input: Option<usize>) -> usize {
    // Emit a UMaterialExpressionTextureSample (not the parameter variant)
    // Uses the same expression pattern as add_texture_sample_parameter
    // but without ParameterName/Group fields
    self.add_expression(MaterialExpressionType::TextureSample { uv_input })
}
```

Then update the match arm:
```rust
MaterialNodeType::TextureSample { uv_input, texture_path } => {
    let uv = uv_input
        .as_ref()
        .and_then(|id| node_map.get(id))
        .copied();
    // Use static texture if texture_path is specified, otherwise plain sample
    Ok(builder.add_texture_sample_node(uv))
}
MaterialNodeType::TextureSampleParameter { name, uv_input, .. } => {
    let uv = uv_input
        .as_ref()
        .and_then(|id| node_map.get(id))
        .copied();
    Ok(builder.add_texture_sample_parameter(name, uv))
}
```

---

## AGENT B — Significant + Minor Fixes

### Fix 4: Remove unreachable duplicate match arms

**File:** `crates/ue5-materials/src/material_factory.rs`

Search for `DotProduct`, `AppendVector`, `ConstantVector3`, `ConstantVector4` in the match statement. The pattern is: first arm matches the real variant, later "alias" arms try to match the same variant again but are unreachable.

```bash
rg -n "DotProduct|AppendVector|ConstantVector3|ConstantVector4" crates/ue5-materials/src/material_factory.rs
```

Remove all duplicate/alias arms. Keep only the first (real) match arm for each. The compiler's dead_code or unreachable_patterns lint should flag these — run `cargo clippy` to find them all at once.

---

### Fix 5: UVScroll/UVScale parse node IDs as floats

**File:** `crates/ue5-materials/src/material_serializer.rs` lines 1461-1463

**Current (always wrong — parses node ID string as float):**
```rust
let sx = offset_x.parse::<f32>().unwrap_or(0.1);
let sy = offset_y.parse::<f32>().unwrap_or(0.0);
```

**Fix:** `offset_x`/`offset_y` are node IDs, not literal floats. Look them up in `node_map`:
```rust
// UVScroll: offset_x and offset_y are node references, not literals
let sx_node = offset_x.as_ref().and_then(|id| node_map.get(id)).copied();
let sy_node = offset_y.as_ref().and_then(|id| node_map.get(id)).copied();
// Pass node indices to the UV scroll builder, not float values
Ok(builder.add_uv_scroll_node(uv_node, sx_node, sy_node))
```

Apply the same pattern to `UVScale` with `scale_x`/`scale_y`.

**Note:** If `add_uv_scroll_node` doesn't exist yet on `MaterialAssetBuilder`, add it — it should wire the scroll/scale inputs as expression connections, not constants.

---

### Fix 6: `ColorParameter` alpha dropped

**File:** `crates/ue5-materials/src/material_serializer.rs` lines 1317-1319

**Current (drops alpha):**
```rust
MaterialNodeType::ColorParameter { name, default } => {
    Ok(builder.add_vector_parameter_node(name, [default[0], default[1], default[2]]))
}
```

**Fix:**
```rust
MaterialNodeType::ColorParameter { name, default } => {
    // default is [f32; 4] — pass all 4 components
    Ok(builder.add_vector_parameter_node(name, [default[0], default[1], default[2], default[3]]))
}
```

If `add_vector_parameter_node` only accepts `[f32; 3]`, update its signature to `[f32; 4]`. Color parameters in UE5 are `FLinearColor` (4 components) — truncating to 3 loses alpha and breaks transparency/masking materials.

---

### Fix 7: Remove unused `material_package_import`

**File:** `crates/ue5-materials/src/material_serializer.rs` lines 101-108

**Current:**
```rust
let material_package_import = ImportBuilder::get_or_add_package(&mut asset, material_name);
```

This is stored but never used — it adds a spurious import to every `.uasset`. If it's genuinely needed as a package anchor (some UE5 serializers require the self-referential package import), add a comment explaining why. If it's not needed, remove it.

**To verify:** Check if any UE5 `.uasset` inspector shows an unexpected self-import entry. If yes, remove the line. If removing it causes asset loading failures, keep it with a comment.

---

### Fix 8: Extract shared builder logic (structural)

**Files:**  
- `crates/ue5-materials/src/material_serializer.rs` — `MaterialAssetBuilder`  
- `crates/ue5-materials/src/material_function_builder.rs` — `MaterialFunctionBuilder`

The two builders share ~60% identical methods. Create a shared trait:

```rust
// crates/ue5-materials/src/builder_common.rs (new file)
pub trait MaterialExpressionBuilder {
    fn add_expression_export(&mut self, ...) -> usize;
    fn make_expression_ref(&self, idx: usize) -> ...;
    fn make_input_property(&self, ...) -> ...;
    fn add_constant_node(&mut self, value: f32) -> usize;
    fn add_constant3_node(&mut self, r: f32, g: f32, b: f32) -> usize;
    fn add_binary_op_node(&mut self, op: BinaryOp, a: usize, b: usize) -> usize;
}
```

Implement the trait for both builders, replacing duplicated method bodies with the shared implementation.

**Priority:** Low — do this only after the critical fixes are verified working.

---

### Fix 9: Complete `convert_function_node` match

**File:** `crates/ue5-materials/src/material_function_builder.rs` lines 619-621

**Current:**
```rust
_ => Err(format!("Node type {:?} not yet supported in material functions", node_type)),
```

Add handlers for the most common missing node types:

```rust
MaterialNodeType::TextureSampleParameter { name, uv_input, .. } => {
    let uv = uv_input.as_ref().and_then(|id| node_map.get(id)).copied();
    Ok(builder.add_texture_sample_parameter(name, uv))
}
MaterialNodeType::Time => {
    Ok(builder.add_time_node())
}
MaterialNodeType::WorldPosition => {
    Ok(builder.add_world_position_node())
}
MaterialNodeType::CustomHLSL { code, inputs } => {
    // Same fix as Fix 2 — resolve inputs via node_map
    Ok(builder.add_custom_hlsl_node(code, resolved_inputs))
}
```

Run `cargo test` and the existing `test_material_graph_minimal`/`test_material_graph_parsing` tests to confirm nothing broke.

---

### Fix 10: Implement TODO placeholder nodes in C++ factory

**File:** `crates/ue5-materials/src/material_factory.rs`

Search for `// TODO` comments in the match arms:
```bash
rg -n "TODO" crates/ue5-materials/src/material_factory.rs
```

For UV nodes, emit actual UE5 C++ equivalents:

```rust
// UVScroll → UMaterialExpressionTextureCoordinate + UMaterialExpressionAdd
MaterialNodeType::UVScroll { uv_input, offset_x, offset_y } => {
    code.push_str(&format!(
        "    // UV Scroll\n    UMaterialExpressionAdd* {}_scroll = NewObject<UMaterialExpressionAdd>(Material);\n",
        node_id
    ));
    // Wire offset_x, offset_y as constant or expression inputs
}
```

For `MaterialLayer`/`MaterialLayerBlend`, emit `UMaterialExpressionMaterialAttributeLayers` nodes.

---

### Fix 11: Pass engine target through `serialize_material_graph()`

**File:** `crates/ue5-materials/src/material_serializer.rs`

**Current:** Always uses `KainEngineTarget::default()`

**Fix:** Add target parameter:
```rust
pub fn serialize_material_graph(
    graph: &MaterialGraph,
    target: KainEngineTarget,  // NEW parameter
) -> Result<Vec<u8>, String> {
    // Use `target` instead of KainEngineTarget::default()
}
```

Update all call sites to pass the target. The value should come from `kain.toml` → `[ue5] engine_version` field which is already parsed in the CLI pipeline.

---

## Verification After Fixes

```bash
# Build must be clean
cargo build -p ue5-materials

# Existing tests must pass
cargo test -p ue5-materials

# Rebuild AssetPipelineTest and check generated output
cd testing/AssetPipelineTest
kain build --ue5

# Confirm in generated MaterialFactories.cpp:
# ✅ Material->BlendMode = BLEND_Opaque;           (NOT GetEditorOnlyData)
# ✅ Material->SetShadingModel(MSM_DefaultLit);    (NOT GetEditorOnlyData)
# ✅ CustomHLSL inputs: node_idx > 0 where applicable
# ✅ ColorParameter: 4-component vector
```

---

## Notes on BlendMode C2039 Error

The `GetEditorOnlyData()->BlendMode` bug seen in `MaterialFactories.cpp` is NOT
explained by any of the 11 issues above — `material_factory.rs` correctly emits
`Material->BlendMode` in `generate_material_properties()` (line 1206). The bug
is likely in `generate_connections()` (line 973–1182) which emits the output
expression connections via `GetEditorOnlyData()`. The end of that function may
have a stray block that also emits property settings through the same accessor,
or the string returned by `generate_material_properties()` is being concatenated
into the `GetEditorOnlyData()` chain from a format string in `generate_connections`.

**Quick diagnostic:** Add a `println!` or `eprintln!` in `generate_material_properties()`
to print exactly what it returns — if the returned string starts correctly with
`Material->BlendMode` but the generated file shows `GetEditorOnlyData()->BlendMode`,
the bug is in how the two strings are joined by the caller at line ~168.
