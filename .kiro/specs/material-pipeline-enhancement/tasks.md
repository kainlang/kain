# Implementation Plan: Material Pipeline Enhancement

## Overview

This plan implements 11 features to transform the KAIN material pipeline into a production-ready system. The implementation is designed for maximum parallelization - up to 6 agents can work simultaneously. Total estimated time: ~12 hours with full parallelization (vs ~30 hours sequential).

## Tasks

- [x] 1. Phase 1: Custom HLSL Nodes (CRITICAL - HIGHEST PRIORITY)
  - [x] 1.1 Add CustomHLSL variant to MaterialNodeType enum
    - Add CustomHLSL { code: String, output_type: CustomOutputType, inputs: Vec<CustomInput> } to MaterialNodeType in `crates/ue5-materials/src/material_graph.rs`
    - Add CustomOutputType enum (Float1, Float2, Float3, Float4)
    - Add CustomInput struct (name, input_type)
    - _Requirements: 1.1, 1.3, 1.4_
  
  - [x] 1.2 Implement custom_hlsl() parsing in KAIN syntax
    - Add custom_hlsl() function recognition to parser
    - Parse HLSL code string literal
    - Parse output_type named argument
    - Parse inputs array argument
    - _Requirements: 1.1, 1.2_
  
  - [x] 1.3 Implement convert_custom_hlsl() in MaterialGraphConverter
    - Add convert_custom_hlsl() method to `crates/ue5-materials/src/ast_converter.rs`
    - Extract HLSL code string from AST
    - Extract output_type and map to CustomOutputType enum
    - Extract inputs and create CustomInput structs
    - Generate CustomHLSL node and add to graph
    - _Requirements: 1.1, 1.2, 1.3, 1.4_
  
  - [x] 1.4 Implement generate_custom_hlsl_node() in MaterialFactoryGenerator
    - Add generate_custom_hlsl_node() method to `crates/ue5-materials/src/material_factory.rs`
    - Generate UMaterialExpressionCustom node creation code
    - Set Code property with HLSL string
    - Set OutputType property (CMOT_Float1/2/3/4)
    - Create input pins for each CustomInput
    - Add #include "Materials/MaterialExpressionCustom.h"
    - _Requirements: 1.1, 1.2, 1.3, 1.4_
  
  - [ ]* 1.5 Write property tests for custom HLSL
    - **Property 1: Custom HLSL Code Preservation** - Validates: Requirements 1.2
    - **Property 2: Custom HLSL Output Type Mapping** - Validates: Requirements 1.3
    - **Property 3: Custom HLSL Input Pin Generation** - Validates: Requirements 1.4
    - Test with random HLSL code strings
    - Test all four output types
    - Test various input configurations

- [x] 2. Phase 2: Expression to Node Conversion (CRITICAL - REQUIRED BY GROUP B)
  - [x] 2.1 Enhance convert_expr() for arithmetic operations
    - Extend convert_expr() in `crates/ue5-materials/src/ast_converter.rs`
    - Handle all binary operators: +, -, *, /
    - Generate Add, Subtract, Multiply, Divide nodes
    - Wire connections correctly
    - _Requirements: 2.1_
  
  - [x] 2.2 Add support for nested expressions
    - Implement recursive expression tree traversal
    - Generate intermediate nodes for nested operations
    - Maintain correct parent-child relationships
    - Wire connections in correct order
    - _Requirements: 2.4_
  
  - [x] 2.3 Implement function call recognition
    - Add function call handlers for: lerp, clamp, pow, dot, cross, normalize, length, distance, abs, min, max, saturate, frac, floor, ceil, round, sqrt, exp, log
    - Map each function to corresponding UE5 node type
    - Generate correct node types
    - Wire function arguments to node inputs
    - _Requirements: 2.2_
  
  - [x] 2.4 Implement variable resolution and wiring
    - Track variable definitions in variable_map
    - Resolve variable references to node IDs
    - Wire connections between nodes
    - Validate all references are defined
    - _Requirements: 2.3_
  
  - [x] 2.5 Add error messages with file:line:col
    - Capture source location (Span) for all AST nodes
    - Format errors as "file:line:col: error_type: message"
    - Include source line and caret indicator
    - Add "Help:" suggestions for common errors
    - _Requirements: 2.5_
  
  - [ ]* 2.6 Write property tests for expression conversion
    - **Property 4: Expression to Node Conversion** - Validates: Requirements 2.1, 2.4
    - **Property 5: Function Call to Node Conversion** - Validates: Requirements 2.2
    - **Property 6: Variable Reference Wiring** - Validates: Requirements 2.3
    - **Property 7: Error Messages with Location** - Validates: Requirements 2.5
    - Test with random expression trees
    - Test all supported functions
    - Test undefined variable errors

- [x] 3. Checkpoint - Ensure core conversion works
  - Ensure all tests pass, ask the user if questions arise.

- [x] 4. Phase 3: Shader Integration
  - [x] 4.1 Add MaterialFunctionCall variant to MaterialNodeType
    - Add MaterialFunctionCall { function_path: String, inputs: Vec<String> } to MaterialNodeType
    - _Requirements: 3.1_
  
  - [x] 4.2 Implement call_shader() parsing
    - Add call_shader() function recognition to parser
    - Parse shader name argument
    - Parse shader parameter arguments
    - _Requirements: 3.1_
  
  - [x] 4.3 Implement convert_shader_call() in MaterialGraphConverter
    - Add convert_shader_call() method
    - Resolve shader name to UE5 function path
    - Convert argument expressions to node IDs
    - Generate MaterialFunctionCall node
    - Wire inputs to argument nodes
    - _Requirements: 3.1, 3.2, 3.3_
  
  - [x] 4.4 Implement resolve_shader_path()
    - Add resolve_shader_path() method
    - Look up KAIN shader definitions
    - Map to UE5 material function paths
    - Return error if shader not found
    - _Requirements: 3.4_
  
  - [x] 4.5 Implement generate_material_function_call() in MaterialFactoryGenerator
    - Add generate_material_function_call() method
    - Generate UMaterialExpressionMaterialFunctionCall node creation code
    - Set MaterialFunction property to function path
    - Wire input connections
    - Add #include "Materials/MaterialExpressionMaterialFunctionCall.h"
    - _Requirements: 3.1, 3.2, 3.3_
  
  - [ ]* 4.6 Write property tests for shader integration
    - **Property 8: Shader Call Node Generation** - Validates: Requirements 3.1, 3.3
    - Test with random shader calls
    - Test parameter wiring
    - Test missing shader errors

- [x] 5. Phase 4: Texture Sampling
  - [x] 5.1 Enhance texture_sample() conversion
    - Extend convert_expr() to handle texture_sample() calls
    - Parse texture argument (Texture2D input)
    - Parse UV argument (optional)
    - Generate TextureSampleParameter2D node
    - Wire UV input if provided
    - _Requirements: 4.1, 4.2_
  
  - [x] 5.2 Implement create_default_uv_node()
    - Add create_default_uv_node() method
    - Generate TextureCoordinate node with index 0
    - Cache node ID to avoid duplicates
    - Return cached node ID if already created
    - _Requirements: 4.3_
  
  - [x] 5.3 Add support for texture channel access
    - Handle field access on texture samples (.r, .g, .b, .a, .rgb, .rgba)
    - Generate ComponentMask nodes
    - Set correct channel flags (R, G, B, A)
    - Wire input to texture sample node
    - _Requirements: 4.4_
  
  - [x] 5.4 Implement texture parameter deduplication
    - Track texture parameter nodes by input name
    - Reuse existing nodes for multiple samples
    - Only create one TextureSampleParameter2D per texture input
    - _Requirements: 4.5_
  
  - [ ]* 5.5 Write property tests for texture sampling
    - **Property 9: Texture Parameter Node Generation** - Validates: Requirements 4.1
    - **Property 10: Default UV Coordinate Generation** - Validates: Requirements 4.3
    - **Property 11: Texture Channel Access** - Validates: Requirements 4.4
    - **Property 12: Texture Parameter Deduplication** - Validates: Requirements 4.5
    - Test with various texture sample patterns
    - Test all channel combinations
    - Test multiple samples of same texture

- [x] 6. Phase 5: UV Manipulation
  - [x] 6.1 Add UV manipulation variants to MaterialNodeType
    - Add UVScroll { uv_input: String, offset_x: String, offset_y: String }
    - Add UVScale { uv_input: String, scale_x: String, scale_y: String }
    - Add UVRotate { uv_input: String, angle: String, center: Option<(String, String)> }
    - _Requirements: 5.1, 5.2, 5.3_
  
  - [x] 6.2 Implement uv_scroll() conversion
    - Add convert_uv_scroll() method
    - Parse UV input and offset arguments
    - Generate Add nodes for X and Y offsets
    - Wire UV input to Add nodes
    - _Requirements: 5.1_
  
  - [x] 6.3 Implement uv_scale() conversion
    - Add convert_uv_scale() method
    - Parse UV input and scale arguments
    - Generate Multiply nodes for X and Y scales
    - Wire UV input to Multiply nodes
    - _Requirements: 5.2_
  
  - [x] 6.4 Implement uv_rotate() conversion
    - Add convert_uv_rotate() method
    - Parse UV input, angle, and optional center
    - Generate rotation matrix nodes (Sine, Cosine, Multiply, Add)
    - Wire rotation chain correctly
    - _Requirements: 5.3_
  
  - [x] 6.5 Implement UV operation chaining
    - Support chaining multiple UV operations
    - Wire output of operation N to input of operation N+1
    - Maintain correct order
    - _Requirements: 5.4_
  
  - [x] 6.6 Implement generate_uv_*_node() methods in MaterialFactoryGenerator
    - Add generate_uv_scroll_node()
    - Add generate_uv_scale_node()
    - Add generate_uv_rotate_node()
    - Generate correct UE5 node chains for each operation
    - _Requirements: 5.1, 5.2, 5.3_
  
  - [ ]* 6.7 Write property tests for UV manipulation
    - **Property 13: UV Manipulation Node Chains** - Validates: Requirements 5.1, 5.2, 5.3
    - **Property 14: UV Operation Chaining** - Validates: Requirements 5.4
    - **Property 15: Automatic TextureCoordinate Node Creation** - Validates: Requirements 5.5
    - Test all UV operations
    - Test chained operations
    - Test automatic UV node creation

- [x] 7. Phase 6: Time-Based Effects
  - [x] 7.1 Add time-based variants to MaterialNodeType
    - Add Time variant (no fields)
    - Add Sine { input: String }
    - Add Cosine { input: String }
    - _Requirements: 6.1, 6.2, 6.3_
  
  - [x] 7.2 Implement create_time_node() with deduplication
    - Add create_time_node() method
    - Generate Time node
    - Cache node ID to avoid duplicates
    - Return cached node ID if already created
    - _Requirements: 6.1_
  
  - [x] 7.3 Implement convert_sine() and convert_cosine()
    - Add convert_sine() method
    - Add convert_cosine() method
    - Parse input argument
    - Generate Sine/Cosine nodes
    - Wire input connections
    - _Requirements: 6.2, 6.3_
  
  - [x] 7.4 Implement mark_material_dynamic()
    - Add mark_material_dynamic() method
    - Set is_dynamic flag on MaterialGraph
    - Call when Time node is created
    - _Requirements: 6.5_
  
  - [x] 7.5 Implement generate_time_node(), generate_sine_node(), generate_cosine_node()
    - Add generate_time_node() to MaterialFactoryGenerator
    - Add generate_sine_node()
    - Add generate_cosine_node()
    - Generate UMaterialExpressionTime, UMaterialExpressionSine, UMaterialExpressionCosine
    - Add #include directives
    - _Requirements: 6.1, 6.2, 6.3_
  
  - [ ]* 7.6 Write property tests for time-based effects
    - **Property 16: Time Node Generation** - Validates: Requirements 6.1
    - **Property 17: Time-based Animation Node Chains** - Validates: Requirements 6.4
    - **Property 18: Dynamic Material Marking** - Validates: Requirements 6.5
    - Test time node deduplication
    - Test sine/cosine generation
    - Test dynamic flag setting

- [x] 8. Checkpoint - Ensure basic features work
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 9. Phase 7: Dynamic Materials
  - [ ] 9.1 Add dynamic material fields to MaterialGraph
    - Add is_dynamic: bool field
    - Add runtime_parameters: Vec<MaterialParameter> field
    - Add MaterialParameter struct (name, param_type, default_value)
    - Add MaterialParameterType enum (Scalar, Vector, Texture)
    - _Requirements: 7.1_
  
  - [ ] 9.2 Implement extract_runtime_parameters()
    - Add extract_runtime_parameters() method
    - Scan material inputs
    - Create MaterialParameter for each input
    - Populate runtime_parameters field
    - _Requirements: 7.1_
  
  - [ ] 9.3 Implement generate_dynamic_material_helpers()
    - Add generate_dynamic_material_helpers() method to MaterialFactoryGenerator
    - Generate helper class for parameter access
    - Generate getter/setter methods
    - _Requirements: 7.5_
  
  - [ ] 9.4 Implement generate_parameter_setter_functions()
    - Add generate_parameter_setter_functions() method
    - Generate SetScalarParameter() wrappers
    - Generate SetVectorParameter() wrappers
    - Generate SetTextureParameter() wrappers
    - _Requirements: 7.5_
  
  - [ ]* 9.5 Write property tests for dynamic materials
    - **Property 19: Material Parameter Exposure** - Validates: Requirements 7.1, 7.5
    - Test parameter extraction
    - Test helper function generation
    - Test all parameter types

- [ ] 10. Phase 8: Material Functions
  - [ ] 10.1 Add material function types
    - Add MaterialFunctionInput { name: String, input_type: MaterialInputType } to MaterialNodeType
    - Add MaterialFunctionOutput { output_type: MaterialInputType } to MaterialNodeType
    - Add MaterialFunction struct (name, inputs, nodes, output)
    - _Requirements: 8.1, 8.2, 8.3_
  
  - [ ] 10.2 Implement @material_function parsing
    - Add @material_function attribute recognition to parser
    - Parse function name, inputs, body, output
    - Create MaterialFunctionDef AST node
    - _Requirements: 8.1_
  
  - [ ] 10.3 Implement convert_material_function_def()
    - Add convert_material_function_def() method to MaterialGraphConverter
    - Convert function inputs to FunctionInput nodes
    - Convert function body to material nodes
    - Convert function output to FunctionOutput node
    - Return MaterialFunction struct
    - _Requirements: 8.1, 8.2, 8.3_
  
  - [ ] 10.4 Implement resolve_function_call()
    - Add resolve_function_call() method
    - Look up MaterialFunction by name
    - Generate MaterialFunctionCall node
    - Wire arguments to function inputs
    - _Requirements: 8.4_
  
  - [ ] 10.5 Add dependency resolution for nested functions
    - Track function dependencies
    - Resolve nested function calls
    - Generate correct call chains
    - Detect circular dependencies
    - _Requirements: 8.5_
  
  - [ ] 10.6 Implement generate_material_function_asset()
    - Add generate_material_function_asset() method to MaterialFactoryGenerator
    - Generate UMaterialFunction asset creation code
    - Generate FunctionInput nodes
    - Generate FunctionOutput node
    - Generate function body nodes
    - Save as .uasset file
    - _Requirements: 8.1, 8.2, 8.3_
  
  - [ ]* 10.7 Write property tests for material functions
    - **Property 20: Material Function Asset Generation** - Validates: Requirements 8.1
    - **Property 21: Material Function Input/Output Nodes** - Validates: Requirements 8.2, 8.3
    - **Property 22: Material Function Call Wiring** - Validates: Requirements 8.4
    - **Property 23: Nested Material Function Resolution** - Validates: Requirements 8.5
    - Test function asset generation
    - Test input/output nodes
    - Test function calls
    - Test nested functions

- [ ] 11. Phase 9: Material Layers
  - [ ] 11.1 Add material layer types
    - Add MaterialLayerBlend { base: String, layer: String, mask: Option<String>, blend_mode: LayerBlendMode } to MaterialNodeType
    - Add LayerBlendMode enum (Lerp, Additive, Multiply, Overlay)
    - Add LayerDef struct (name, base_color, roughness, metallic, normal, mask, blend_mode)
    - _Requirements: 9.1, 9.3_
  
  - [ ] 11.2 Implement layer_blend() parsing
    - Add layer_blend() function recognition to parser
    - Parse base, layer, mask, blend_mode arguments
    - _Requirements: 9.1_
  
  - [ ] 11.3 Implement convert_layer_blend()
    - Add convert_layer_blend() method to MaterialGraphConverter
    - Parse base and layer expressions
    - Parse optional mask expression
    - Parse blend_mode string
    - Generate MaterialLayerBlend node
    - Wire base, layer, and mask inputs
    - _Requirements: 9.1, 9.2, 9.3_
  
  - [ ] 11.4 Implement create_layer_stack()
    - Add create_layer_stack() method
    - Process layers in order (bottom to top)
    - Generate blend nodes for each layer
    - Wire layers in sequence
    - _Requirements: 9.4_
  
  - [ ] 11.5 Implement generate_layer_blend_node()
    - Add generate_layer_blend_node() method to MaterialFactoryGenerator
    - Generate UMaterialExpressionMaterialLayerBlend node (or equivalent)
    - Set blend mode property
    - Wire base, layer, and alpha inputs
    - _Requirements: 9.1, 9.2, 9.3_
  
  - [ ]* 11.6 Write property tests for material layers
    - **Property 24: Material Layer Blend Node Generation** - Validates: Requirements 9.1
    - **Property 25: Layer Mask Wiring** - Validates: Requirements 9.2
    - **Property 26: Layer Blend Mode Configuration** - Validates: Requirements 9.3
    - **Property 27: Layer Stack Ordering** - Validates: Requirements 9.4
    - Test layer blend generation
    - Test mask wiring
    - Test all blend modes
    - Test layer ordering

- [ ] 12. Phase 10: World-Space Operations
  - [ ] 12.1 Add world-space variants to MaterialNodeType
    - Add WorldPosition variant (no fields)
    - Add WorldNormal variant (no fields)
    - Add TriplanarSample { texture: String, world_scale: String, blend_sharpness: String }
    - _Requirements: 10.1, 10.2, 10.3_
  
  - [ ] 12.2 Implement create_world_position_node() and create_world_normal_node()
    - Add create_world_position_node() with deduplication
    - Add create_world_normal_node() with deduplication
    - Cache node IDs to avoid duplicates
    - _Requirements: 10.1, 10.2_
  
  - [ ] 12.3 Implement convert_triplanar_sample()
    - Add convert_triplanar_sample() method
    - Parse texture, world_scale, blend_sharpness arguments
    - Generate 3 TextureSample nodes (X, Y, Z projections)
    - Generate ComponentMask nodes for blend weights
    - Generate blend nodes to combine samples
    - Wire all connections
    - _Requirements: 10.3, 10.5_
  
  - [ ] 12.4 Implement create_world_to_uv_transform()
    - Add create_world_to_uv_transform() method
    - Generate coordinate transformation nodes
    - Convert WorldPosition to UV space
    - Apply scale factor
    - _Requirements: 10.4_
  
  - [ ] 12.5 Implement generate_world_*_nodes() in MaterialFactoryGenerator
    - Add generate_world_position_node()
    - Add generate_world_normal_node()
    - Add generate_triplanar_sample_nodes()
    - Generate UMaterialExpressionWorldPosition, UMaterialExpressionVertexNormalWS
    - Generate triplanar projection node chains
    - _Requirements: 10.1, 10.2, 10.3_
  
  - [ ]* 12.6 Write property tests for world-space operations
    - **Property 28: World Position Node Generation** - Validates: Requirements 10.1
    - **Property 29: World Normal Node Generation** - Validates: Requirements 10.2
    - **Property 30: Triplanar Sampling Node Chain** - Validates: Requirements 10.3, 10.5
    - **Property 31: World-to-UV Transformation** - Validates: Requirements 10.4
    - Test world position/normal deduplication
    - Test triplanar sampling
    - Test coordinate transformation

- [ ] 13. Phase 11: Vertex Shaders
  - [ ] 13.1 Add vertex shader variants to MaterialNodeType
    - Add ObjectPosition variant (no fields)
    - Add VertexNormal variant (no fields)
    - Add CoordinateSpace enum (World, Object, Tangent)
    - _Requirements: 11.2, 11.3_
  
  - [ ] 13.2 Implement convert_vertex_displacement()
    - Add convert_vertex_displacement() method
    - Parse displacement expression
    - Detect coordinate space (world_position vs object_position)
    - Generate displacement nodes
    - Wire to world_position_offset output
    - _Requirements: 11.1, 11.2, 11.3_
  
  - [ ] 13.3 Implement create_object_position_node() and create_vertex_normal_node()
    - Add create_object_position_node()
    - Add create_vertex_normal_node()
    - Generate ObjectPosition and VertexNormal nodes
    - _Requirements: 11.2, 11.3_
  
  - [ ] 13.4 Add support for time-based vertex animation
    - Detect time() usage in world_position_offset
    - Mark material as dynamic
    - Generate Time nodes
    - _Requirements: 11.4_
  
  - [ ] 13.5 Implement generate_vertex_displacement_nodes()
    - Add generate_vertex_displacement_nodes() method to MaterialFactoryGenerator
    - Generate vertex displacement node chains
    - Wire to WorldPositionOffset output pin
    - Handle world-space and object-space displacement
    - _Requirements: 11.1, 11.2, 11.3_
  
  - [ ]* 13.6 Write property tests for vertex shaders
    - **Property 32: Vertex Displacement Output Wiring** - Validates: Requirements 11.1
    - **Property 33: Coordinate Space Displacement** - Validates: Requirements 11.2, 11.3
    - **Property 34: Animated Vertex Displacement** - Validates: Requirements 11.4
    - Test vertex displacement wiring
    - Test coordinate spaces
    - Test animated displacement

- [ ] 14. Final Checkpoint - Ensure all features work
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 15. Integration Testing
  - [ ] 15.1 Create comprehensive test materials
    - Create test material using all 11 features
    - Custom HLSL nodes
    - Complex expressions
    - Shader calls
    - Texture sampling with UV manipulation
    - Time-based animation
    - Dynamic parameters
    - Material functions
    - Layered materials
    - World-space operations
    - Vertex displacement
  
  - [ ] 15.2 Verify generated C++ compiles
    - Build generated C++ code
    - Check for compilation errors
    - Verify all includes are present
    - Verify all node types are correct
  
  - [ ] 15.3 Test in UE5 (if available)
    - Load generated materials in UE5
    - Verify materials appear in Content Browser
    - Verify parameters are exposed
    - Verify materials render correctly
    - Test runtime parameter modification

- [ ] 16. Documentation
  - [ ] 16.1 Update MATERIAL_GRAPH_SYNTAX.md
    - Document custom_hlsl() syntax
    - Document all new functions (call_shader, uv_scroll, uv_scale, uv_rotate, time, sine, cosine, world_position, world_normal, triplanar_sample, layer_blend)
    - Add examples for each feature
    - Update node type reference
  
  - [ ] 16.2 Update README.md
    - Add feature list
    - Add quick start examples
    - Update architecture diagram
    - Add performance notes
  
  - [ ] 16.3 Create migration guide
    - Document breaking changes (if any)
    - Provide upgrade path for existing materials
    - Add troubleshooting section

## Notes

- Tasks marked with `*` are optional property tests and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Phases 1, 2, 5, 6, 10, 11 can be implemented in parallel (Wave 1)
- Phases 3, 4, 8, 9 can be implemented in parallel after Phase 2 completes (Wave 2)
- Phase 7 should be implemented after all others (Wave 3)
- Maximum parallelization: 6 agents in Wave 1
- Total estimated time: ~12 hours with full parallelization
