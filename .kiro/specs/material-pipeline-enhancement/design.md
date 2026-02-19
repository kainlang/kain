# Design Document

## Overview

This design transforms the KAIN material pipeline from basic node support into a production-ready system capable of generating ANY shader effect. The architecture is built for maximum parallelization - each of the seven features can be implemented independently by different agents.

The core strategy is **Custom HLSL Nodes First** - this single feature unblocks everything else by allowing arbitrary shader code. While other features are being implemented, developers can use custom HLSL to achieve any effect.

## Architecture

### Current System

```
KAIN Source (.kn) → Parser → MaterialGraphDef (AST)
                                    ↓
                          MaterialGraphConverter
                                    ↓
                          MaterialGraph (IR)
                                    ↓
                          MaterialFactoryGenerator
                                    ↓
                          C++ Factory Code → UE5 Material Assets
```

### Enhanced System

```
KAIN Source (.kn) → Parser → MaterialGraphDef (AST)
                                    ↓
                          MaterialGraphConverter (ENHANCED)
                          ├─ Expression → Node Conversion
                          ├─ Custom HLSL Node Generation
                          ├─ Shader Call Resolution
                          ├─ Texture Sampling
                          ├─ UV Manipulation
                          └─ Time Node Generation
                                    ↓
                          MaterialGraph (IR) (ENHANCED)
                          ├─ New Node Types
                          └─ Dynamic Material Metadata
                                    ↓
                          MaterialFactoryGenerator (ENHANCED)
                          ├─ Custom HLSL Codegen
                          ├─ Material Function Calls
                          ├─ Texture Parameter Setup
                          ├─ UV Node Chains
                          ├─ Time-based Animation
                          └─ Dynamic Instance Helpers
                                    ↓
                          C++ Factory Code → UE5 Material Assets
```

## Components and Interfaces

### 1. MaterialNodeType Enum (Enhanced)

**Location:** `crates/ue5-materials/src/material_graph.rs`

**New Variants:**

```rust
pub enum MaterialNodeType {
    // ... existing variants ...
    
    // Feature 1: Custom HLSL
    CustomHLSL {
        code: String,
        output_type: CustomOutputType,
        inputs: Vec<CustomInput>,
    },
    
    // Feature 2: Expression nodes (auto-generated from KAIN expressions)
    // Uses existing nodes (Multiply, Add, etc.) - no new variants needed
    
    // Feature 3: Shader integration
    MaterialFunctionCall {
        function_path: String,
        inputs: Vec<String>, // node IDs
    },
    
    // Feature 4: Texture sampling (already exists, enhance)
    // TextureSampleParameter2D - already exists
    
    // Feature 5: UV manipulation
    UVScroll {
        uv_input: String,
        offset_x: String,
        offset_y: String,
    },
    UVScale {
        uv_input: String,
        scale_x: String,
        scale_y: String,
    },
    UVRotate {
        uv_input: String,
        angle: String,
        center: Option<(String, String)>,
    },
    
    // Feature 6: Time-based effects
    Time,
    Sine { input: String },
    Cosine { input: String },
    
    // Feature 7: Dynamic materials (metadata only, no new nodes)
    
    // Feature 8: Material functions
    MaterialFunctionInput {
        name: String,
        input_type: MaterialInputType,
    },
    MaterialFunctionOutput {
        output_type: MaterialInputType,
    },
    
    // Feature 9: Material layers
    MaterialLayerBlend {
        base: String,
        layer: String,
        mask: Option<String>,
        blend_mode: LayerBlendMode,
    },
    
    // Feature 10: World-space operations
    WorldPosition,
    WorldNormal,
    TriplanarSample {
        texture: String,
        world_scale: String,
        blend_sharpness: String,
    },
    
    // Feature 11: Vertex shaders (uses existing world_position_offset output)
    ObjectPosition,
    VertexNormal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CustomOutputType {
    Float1,  // CMOT_Float1
    Float2,  // CMOT_Float2
    Float3,  // CMOT_Float3
    Float4,  // CMOT_Float4
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CustomInput {
    pub name: String,
    pub input_type: CustomOutputType,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LayerBlendMode {
    Lerp,
    Additive,
    Multiply,
    Overlay,
}
```

### 2. MaterialGraphConverter (Enhanced)

**Location:** `crates/ue5-materials/src/ast_converter.rs`

**New Methods:**

```rust
impl MaterialGraphConverter {
    // Feature 1: Custom HLSL
    fn convert_custom_hlsl(&mut self, graph: &mut MaterialGraph, code: &str, output_type: &str, inputs: &[CustomInput]) -> Result<String, String>;
    
    // Feature 2: Expression conversion (enhance existing convert_expr)
    fn convert_expr(&mut self, graph: &mut MaterialGraph, expr: &Expr) -> Result<String, String>;
    // This already exists, but needs enhancement for:
    // - Nested expressions
    // - More function calls (pow, clamp, lerp, etc.)
    // - Better error messages with file:line:col
    
    // Feature 3: Shader integration
    fn convert_shader_call(&mut self, graph: &mut MaterialGraph, shader_name: &str, args: &[Expr]) -> Result<String, String>;
    fn resolve_shader_path(&self, shader_name: &str) -> Result<String, String>;
    
    // Feature 4: Texture sampling (enhance existing)
    fn convert_texture_sample(&mut self, graph: &mut MaterialGraph, texture: &Expr, uv: &Expr) -> Result<String, String>;
    fn create_default_uv_node(&mut self, graph: &mut MaterialGraph) -> String;
    
    // Feature 5: UV manipulation
    fn convert_uv_scroll(&mut self, graph: &mut MaterialGraph, uv: &Expr, offset_x: &Expr, offset_y: &Expr) -> Result<String, String>;
    fn convert_uv_scale(&mut self, graph: &mut MaterialGraph, uv: &Expr, scale_x: &Expr, scale_y: &Expr) -> Result<String, String>;
    fn convert_uv_rotate(&mut self, graph: &mut MaterialGraph, uv: &Expr, angle: &Expr, center: Option<(&Expr, &Expr)>) -> Result<String, String>;
    
    // Feature 6: Time-based effects
    fn create_time_node(&mut self, graph: &mut MaterialGraph) -> String;
    fn convert_sine(&mut self, graph: &mut MaterialGraph, input: &Expr) -> Result<String, String>;
    fn convert_cosine(&mut self, graph: &mut MaterialGraph, input: &Expr) -> Result<String, String>;
    
    // Feature 7: Dynamic materials
    fn mark_material_dynamic(&mut self, graph: &mut MaterialGraph);
    fn extract_runtime_parameters(&self, graph: &MaterialGraph) -> Vec<MaterialParameter>;
    
    // Feature 8: Material functions
    fn convert_material_function_def(&mut self, def: &MaterialFunctionDef) -> Result<MaterialFunction, String>;
    fn convert_function_input(&mut self, graph: &mut MaterialGraph, input: &MaterialInput) -> Result<String, String>;
    fn convert_function_output(&mut self, graph: &mut MaterialGraph, output: &Expr) -> Result<String, String>;
    fn resolve_function_call(&mut self, graph: &mut MaterialGraph, func_name: &str, args: &[Expr]) -> Result<String, String>;
    
    // Feature 9: Material layers
    fn convert_layer_blend(&mut self, graph: &mut MaterialGraph, base: &Expr, layer: &Expr, mask: Option<&Expr>, blend_mode: &str) -> Result<String, String>;
    fn create_layer_stack(&mut self, graph: &mut MaterialGraph, layers: &[LayerDef]) -> Result<String, String>;
    
    // Feature 10: World-space operations
    fn create_world_position_node(&mut self, graph: &mut MaterialGraph) -> String;
    fn create_world_normal_node(&mut self, graph: &mut MaterialGraph) -> String;
    fn convert_triplanar_sample(&mut self, graph: &mut MaterialGraph, texture: &Expr, world_scale: &Expr, blend_sharpness: &Expr) -> Result<String, String>;
    fn create_world_to_uv_transform(&mut self, graph: &mut MaterialGraph, world_pos: &str, scale: &str) -> String;
    
    // Feature 11: Vertex shaders
    fn convert_vertex_displacement(&mut self, graph: &mut MaterialGraph, offset: &Expr, space: CoordinateSpace) -> Result<String, String>;
    fn create_object_position_node(&mut self, graph: &mut MaterialGraph) -> String;
    fn create_vertex_normal_node(&mut self, graph: &mut MaterialGraph) -> String;
}
```

### 3. MaterialFactoryGenerator (Enhanced)

**Location:** `crates/ue5-materials/src/material_factory.rs`

**New Methods:**

```rust
impl MaterialFactoryGenerator {
    // Feature 1: Custom HLSL
    fn generate_custom_hlsl_node(&self, node: &MaterialNode) -> String;
    
    // Feature 3: Shader integration
    fn generate_material_function_call(&self, node: &MaterialNode) -> String;
    
    // Feature 5: UV manipulation
    fn generate_uv_scroll_node(&self, node: &MaterialNode) -> String;
    fn generate_uv_scale_node(&self, node: &MaterialNode) -> String;
    fn generate_uv_rotate_node(&self, node: &MaterialNode) -> String;
    
    // Feature 6: Time-based effects
    fn generate_time_node(&self, node: &MaterialNode) -> String;
    fn generate_sine_node(&self, node: &MaterialNode) -> String;
    fn generate_cosine_node(&self, node: &MaterialNode) -> String;
    
    // Feature 7: Dynamic materials
    fn generate_dynamic_material_helpers(&self, graph: &MaterialGraph) -> String;
    fn generate_parameter_setter_functions(&self, graph: &MaterialGraph) -> String;
    
    // Feature 8: Material functions
    fn generate_material_function_asset(&self, func: &MaterialFunction) -> String;
    fn generate_function_input_node(&self, node: &MaterialNode) -> String;
    fn generate_function_output_node(&self, node: &MaterialNode) -> String;
    fn generate_function_call_node(&self, node: &MaterialNode) -> String;
    
    // Feature 9: Material layers
    fn generate_layer_blend_node(&self, node: &MaterialNode) -> String;
    fn generate_layer_stack(&self, graph: &MaterialGraph) -> String;
    
    // Feature 10: World-space operations
    fn generate_world_position_node(&self, node: &MaterialNode) -> String;
    fn generate_world_normal_node(&self, node: &MaterialNode) -> String;
    fn generate_triplanar_sample_nodes(&self, node: &MaterialNode) -> String;
    
    // Feature 11: Vertex shaders
    fn generate_vertex_displacement_nodes(&self, graph: &MaterialGraph) -> String;
    fn generate_object_position_node(&self, node: &MaterialNode) -> String;
    fn generate_vertex_normal_node(&self, node: &MaterialNode) -> String;
}
```

### 4. KAIN Syntax Extensions

**New Functions Available in Material Graphs:**

```kn
// Feature 1: Custom HLSL
custom_hlsl("""
    float3 result = Input0 * Input1;
    return result;
""", output_type: "float3", inputs: [(Input0, "float3"), (Input1, "float3")])

// Feature 2: Expression conversion (automatic)
let result = (a + b) * c / d  // Auto-converts to Add → Multiply → Divide nodes

// Feature 3: Shader integration
let effect = call_shader("MyCustomShader", param1, param2)

// Feature 4: Texture sampling
let albedo = sample(albedo_map, uv)
let normal = sample(normal_map, uv).rgb

// Feature 5: UV manipulation
let scrolled_uv = uv_scroll(uv, time * 0.1, 0.0)
let scaled_uv = uv_scale(uv, 2.0, 2.0)
let rotated_uv = uv_rotate(uv, time * 45.0)

// Feature 6: Time-based effects
let t = time()
let pulse = sine(t * speed) * 0.5 + 0.5

// Feature 7: Dynamic materials (automatic from input parameters)
input glow_intensity: Float = 1.0  // Automatically exposed at runtime
```

## Data Models

### MaterialGraph (Enhanced)

```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: MaterialOutputs,
    pub properties: MaterialProperties,
    pub is_dynamic: bool,  // NEW: Feature 7
    pub runtime_parameters: Vec<MaterialParameter>,  // NEW: Feature 7
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialParameter {
    pub name: String,
    pub param_type: MaterialParameterType,
    pub default_value: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MaterialParameterType {
    Scalar,
    Vector,
    Texture,
}
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system - essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*


### Property 1: Custom HLSL Code Preservation
*For any* custom_hlsl() expression with HLSL code string, the generated C++ code should contain the exact same HLSL string in the UMaterialExpressionCustom node's Code property
**Validates: Requirements 1.2**

### Property 2: Custom HLSL Output Type Mapping
*For any* custom_hlsl() expression with output_type specified as "float1", "float2", "float3", or "float4", the generated C++ should set OutputType to the corresponding CMOT enum value (CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4)
**Validates: Requirements 1.3**

### Property 3: Custom HLSL Input Pin Generation
*For any* custom_hlsl() expression with N input declarations, the generated UMaterialExpressionCustom node should have exactly N input pins with matching names and types
**Validates: Requirements 1.4**

### Property 4: Expression to Node Conversion
*For any* arithmetic expression tree (using +, -, *, /), the generated MaterialGraph should contain nodes that match the expression structure, with correct node types (Add, Subtract, Multiply, Divide) and correct parent-child relationships
**Validates: Requirements 2.1, 2.4**

### Property 5: Function Call to Node Conversion
*For any* supported function call (lerp, clamp, pow, sine, cosine, etc.), the generated MaterialGraph should contain the corresponding UE5 node type (LinearInterpolate, Clamp, Power, Sine, Cosine, etc.)
**Validates: Requirements 2.2, 6.2, 6.3**

### Property 6: Variable Reference Wiring
*For any* material graph with variable references, all node connections should be wired such that following the connection chain from any output back to inputs never encounters an undefined node ID
**Validates: Requirements 2.3, 3.2, 4.2**

### Property 7: Error Messages with Location
*For any* error condition (undefined variable, missing shader, invalid function), the error message should contain file path, line number, and column number in the format "file:line:col"
**Validates: Requirements 2.5**

### Property 8: Shader Call Node Generation
*For any* call_shader() expression, the generated MaterialGraph should contain a MaterialFunctionCall node with the function_path set to the resolved shader path
**Validates: Requirements 3.1, 3.3**

### Property 9: Texture Parameter Node Generation
*For any* material input of type Texture2D, the generated MaterialGraph should contain exactly one TextureSampleParameter2D node with ParameterName matching the input name
**Validates: Requirements 4.1**

### Property 10: Default UV Coordinate Generation
*For any* texture_sample() call without explicit UV argument, the generated MaterialGraph should contain a TextureCoordinate node with CoordinateIndex = 0 and that node should be wired to the sample node's Coordinates input
**Validates: Requirements 4.3**

### Property 11: Texture Channel Access
*For any* field access on a texture sample (.r, .g, .b, .a, .rgb, .rgba), the generated MaterialGraph should contain a ComponentMask node with the correct channel flags set (R, G, B, A)
**Validates: Requirements 4.4**

### Property 12: Texture Parameter Deduplication
*For any* material that samples the same texture input multiple times, the generated MaterialGraph should contain exactly one TextureSampleParameter2D node for that texture, with multiple nodes referencing it
**Validates: Requirements 4.5**

### Property 13: UV Manipulation Node Chains
*For any* UV manipulation function (uv_scroll, uv_scale, uv_rotate), the generated MaterialGraph should contain the correct node chain: uv_scroll → Add nodes, uv_scale → Multiply nodes, uv_rotate → rotation matrix (Sine, Cosine, Multiply, Add nodes)
**Validates: Requirements 5.1, 5.2, 5.3**

### Property 14: UV Operation Chaining
*For any* sequence of UV operations (e.g., uv_scale then uv_scroll), the generated MaterialGraph should wire them in order such that the output of operation N is the input to operation N+1
**Validates: Requirements 5.4**

### Property 15: Automatic TextureCoordinate Node Creation
*For any* material that uses UV coordinates (in texture sampling or UV manipulation), if no explicit UV source is provided, the generated MaterialGraph should contain exactly one TextureCoordinate node with CoordinateIndex = 0
**Validates: Requirements 5.5**

### Property 16: Time Node Generation
*For any* time() function call, the generated MaterialGraph should contain exactly one Time node (deduplication), and all time() references should wire to that same node
**Validates: Requirements 6.1**

### Property 17: Time-based Animation Node Chains
*For any* expression involving time() multiplied by a parameter (e.g., time() * speed), the generated MaterialGraph should contain a Multiply node with the Time node as one input and the speed parameter as the other input
**Validates: Requirements 6.4**

### Property 18: Dynamic Material Marking
*For any* material that contains a Time node, the MaterialGraph's is_dynamic flag should be set to true
**Validates: Requirements 6.5**

### Property 19: Material Parameter Exposure
*For any* material with input parameters, the generated C++ factory code should contain parameter setup code (ParameterName, DefaultValue) for each input, and the parameter should be accessible via UE5's material instance API
**Validates: Requirements 7.1, 7.5**

## Error Handling

### Error Categories

1. **Syntax Errors** (Parser level)
   - Invalid custom_hlsl() syntax
   - Malformed expressions
   - Invalid function calls

2. **Semantic Errors** (Converter level)
   - Undefined variable references
   - Type mismatches
   - Missing shader references
   - Invalid output types for custom HLSL

3. **Codegen Errors** (Factory Generator level)
   - Invalid node connections
   - Missing required properties
   - UE5 API misuse

### Error Message Format

All errors must follow this format:
```
{file_path}:{line}:{col}: {error_type}: {message}

{source_line}
{caret_indicator}

Help: {suggestion}
```

Example:
```
materials/hologram.kn:15:23: Undefined variable: 'glow_color'

    let glow = glow_color * intensity
                      ^

Help: Did you mean 'glow_colour'? Or add 'input glow_color: Vec3 = ...'
```

### Error Recovery

- **Parser errors**: Stop at first error, report location
- **Converter errors**: Collect all errors in a pass, report all at once
- **Codegen errors**: Fail fast with detailed context

## Testing Strategy

### Dual Testing Approach

This feature requires both unit tests and property-based tests:

**Unit Tests** - Specific examples and edge cases:
- Custom HLSL with empty code string
- Custom HLSL with special characters in code
- Expressions with deeply nested parentheses
- Shader calls with zero parameters
- Texture sampling with all channel combinations
- UV operations with negative values
- Time-based effects with zero speed
- Materials with no parameters

**Property Tests** - Universal properties across all inputs:
- All 19 correctness properties listed above
- Each property test should run minimum 100 iterations
- Use property-based testing library for Rust (proptest or quickcheck)

### Property Test Configuration

Each property test must:
1. Run minimum 100 iterations (due to randomization)
2. Reference its design document property in a comment
3. Use this tag format: `// Feature: material-pipeline-enhancement, Property N: {property_text}`

Example:
```rust
#[test]
fn property_4_expression_to_node_conversion() {
    // Feature: material-pipeline-enhancement, Property 4: Expression to Node Conversion
    // For any arithmetic expression tree, generated nodes match expression structure
    
    proptest!(|(expr in arb_arithmetic_expr())| {
        let mut converter = MaterialGraphConverter::new();
        let mut graph = MaterialGraph::new("test".to_string());
        
        let node_id = converter.convert_expr(&mut graph, &expr)?;
        
        // Verify node structure matches expression tree
        assert_expression_structure_matches(&graph, &expr, &node_id);
    });
}
```

### Test Generators

Property tests need generators for:
- Random arithmetic expressions (nested, various operators)
- Random function calls (all supported functions)
- Random HLSL code strings (valid HLSL syntax)
- Random UV manipulation chains
- Random material parameter configurations

### Integration Testing

Beyond unit and property tests, integration tests should:
1. Generate complete materials from KAIN source
2. Verify generated C++ compiles
3. Verify generated materials load in UE5 (requires UE5 test environment)
4. Verify materials render correctly (visual regression testing)

## Implementation Phases

### Phase 1: Custom HLSL Nodes (CRITICAL - DO FIRST)
**Estimated: 1-2 hours**

This is the highest priority because it unblocks everything else. Once custom HLSL nodes work, developers can achieve ANY shader effect while waiting for other features.

**Tasks:**
1. Add CustomHLSL variant to MaterialNodeType enum
2. Implement convert_custom_hlsl() in MaterialGraphConverter
3. Implement generate_custom_hlsl_node() in MaterialFactoryGenerator
4. Add KAIN syntax parsing for custom_hlsl()
5. Write property tests for custom HLSL code preservation, output type mapping, and input pin generation

**Deliverable:** Developers can write `custom_hlsl("...", output_type: "float3")` and get working UE5 materials

### Phase 2: Expression to Node Conversion (CRITICAL)
**Estimated: 2-3 hours**

This makes the system actually usable for real materials. Without this, developers have to manually specify every node.

**Tasks:**
1. Enhance convert_expr() to handle all arithmetic operators
2. Add support for nested expressions (recursive traversal)
3. Implement function call recognition (lerp, clamp, pow, etc.)
4. Add variable resolution and connection wiring
5. Implement error messages with file:line:col
6. Write property tests for expression conversion, wiring, and error handling

**Deliverable:** Developers can write natural expressions like `(a + b) * c` and get correct node graphs

### Phase 3: Shader Integration
**Estimated: 2-3 hours**

Allows code reuse between standalone shaders and materials.

**Tasks:**
1. Add MaterialFunctionCall variant to MaterialNodeType
2. Implement convert_shader_call() in MaterialGraphConverter
3. Implement resolve_shader_path() to find KAIN shaders
4. Implement generate_material_function_call() in MaterialFactoryGenerator
5. Write property tests for shader call node generation and parameter wiring

**Deliverable:** Developers can call existing KAIN shaders from materials

### Phase 4: Texture Sampling
**Estimated: 2-3 hours**

Essential for realistic materials with albedo, normal, roughness maps.

**Tasks:**
1. Enhance texture_sample() conversion to handle explicit UVs
2. Implement create_default_uv_node() for automatic UV generation
3. Add support for texture channel access (.rgb, .r, etc.)
4. Implement texture parameter deduplication
5. Write property tests for texture sampling, default UVs, channel access, and deduplication

**Deliverable:** Developers can sample textures with proper UV mapping

### Phase 5: UV Manipulation
**Estimated: 1 hour**

Enables animated textures and tiling effects.

**Tasks:**
1. Add UVScroll, UVScale, UVRotate variants to MaterialNodeType
2. Implement convert_uv_scroll(), convert_uv_scale(), convert_uv_rotate()
3. Implement UV operation chaining
4. Implement generate_uv_*_node() methods in MaterialFactoryGenerator
5. Write property tests for UV manipulation and chaining

**Deliverable:** Developers can scroll, scale, and rotate UVs

### Phase 6: Time-Based Effects
**Estimated: 2-3 hours**

Allows pulsing, animated materials without Blueprint code.

**Tasks:**
1. Add Time, Sine, Cosine variants to MaterialNodeType
2. Implement create_time_node() with deduplication
3. Implement convert_sine() and convert_cosine()
4. Implement mark_material_dynamic()
5. Implement generate_time_node(), generate_sine_node(), generate_cosine_node()
6. Write property tests for time node generation, animation chains, and dynamic marking

**Deliverable:** Developers can create time-based animated materials

### Phase 7: Dynamic Materials
**Estimated: 2-3 hours**

Enables runtime parameter control from Blueprint/C++.

**Tasks:**
1. Add is_dynamic and runtime_parameters fields to MaterialGraph
2. Implement extract_runtime_parameters()
3. Implement generate_dynamic_material_helpers()
4. Implement generate_parameter_setter_functions()
5. Write property tests for parameter exposure and helper generation

**Deliverable:** Developers can control material parameters at runtime

## Parallelization Strategy

These phases can be implemented in parallel by different agents:

**Group A (Independent):**
- Phase 1: Custom HLSL Nodes
- Phase 5: UV Manipulation
- Phase 6: Time-Based Effects

**Group B (Depends on Phase 2):**
- Phase 2: Expression to Node Conversion (must be done first)
- Phase 3: Shader Integration (needs expression conversion)
- Phase 4: Texture Sampling (needs expression conversion)

**Group C (Depends on all others):**
- Phase 7: Dynamic Materials (needs all features to test parameter exposure)

**Recommended Parallel Execution:**
1. Start Phase 1 immediately (highest priority, unblocks everything)
2. Start Phase 2 immediately (required by Group B)
3. Once Phase 2 is done, start Phases 3, 4, 5, 6 in parallel
4. Once all others are done, start Phase 7

This allows up to 5 agents working simultaneously (Phases 1, 2, 3, 4, 5 or 6).

## Dependencies

### External Dependencies
- `kain-core` - AST types, parser
- `serde` - Serialization for MaterialGraph IR
- `proptest` or `quickcheck` - Property-based testing library

### Internal Dependencies
- Parser must support custom_hlsl() syntax
- Parser must support call_shader() syntax
- Parser must support uv_*() function syntax
- Parser must support time() function syntax

### UE5 API Dependencies
- `UMaterialExpressionCustom` - Custom HLSL nodes
- `UMaterialExpressionMaterialFunctionCall` - Shader integration
- `UMaterialExpressionTime` - Time-based effects
- `UMaterialExpressionSine` - Sine wave
- `UMaterialExpressionCosine` - Cosine wave
- All existing material expression types (already used)

## Performance Considerations

### Compile-Time Performance
- Expression conversion is O(n) where n = number of AST nodes
- Node deduplication (textures, time) requires HashMap lookups: O(1) average
- Total material compilation should be < 100ms for typical materials

### Runtime Performance (UE5)
- Custom HLSL nodes have same performance as hand-written material nodes
- Time nodes mark materials as dynamic (slightly more expensive than static)
- UV manipulation adds 1-3 instructions per operation (negligible)
- Texture sampling performance unchanged from hand-written materials

### Memory Usage
- MaterialGraph IR: ~1KB per material (small)
- Generated C++ code: ~10-50KB per material (acceptable)
- UE5 material assets: ~100KB-1MB per material (standard)

## Future Enhancements

### Phase 8: Material Functions (Future)
- Define reusable material functions in KAIN
- Call material functions from other materials
- Build a library of common material effects

### Phase 9: Material Instances (Future)
- Generate material instances at compile time
- Override parameters without duplicating materials
- Reduce memory usage for material variations

### Phase 10: Visual Material Editor (Future)
- VS Code extension for visual material editing
- Drag-and-drop node graph editor
- Live preview of materials
- Bidirectional sync between visual editor and KAIN code

### Phase 11: Material Optimization (Future)
- Dead code elimination (remove unused nodes)
- Constant folding (evaluate constant expressions at compile time)
- Node fusion (combine multiple operations into single nodes)
- Instruction count analysis and warnings
