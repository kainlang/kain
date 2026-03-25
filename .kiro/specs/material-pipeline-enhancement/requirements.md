# Requirements Document

## Introduction

The KAIN material pipeline currently supports basic material graph generation with parameters, math operations, and constants. This enhancement transforms it into a production-ready system capable of generating ANY shader effect through seven key features. The system is designed for maximum parallelization, allowing multiple features to be implemented simultaneously by different agents.

## Glossary

- **Material_Graph**: The intermediate representation (IR) of a UE5 material, consisting of nodes and connections
- **Material_Node**: A single operation in the material graph (texture sample, math operation, parameter, etc.)
- **Custom_HLSL_Node**: A material node containing arbitrary HLSL code, allowing any shader effect
- **Expression_Node**: A material node generated from KAIN expressions (math, function calls, variables)
- **Shader_Call_Node**: A material node that invokes an existing KAIN shader from within a material
- **Texture_Sample_Node**: A material node that samples a texture at given UV coordinates
- **UV_Node**: A material node that manipulates UV coordinates (scroll, rotate, scale)
- **Time_Node**: A material node that provides engine time for animations
- **Dynamic_Material_Instance**: A UE5 material instance with parameters controllable at runtime
- **Material_Factory**: C++ code that creates materials at Editor startup
- **Codegen**: The process of generating C++ code from KAIN material graphs

## Requirements

### Requirement 1: Custom HLSL Nodes

**User Story:** As a developer, I want to embed arbitrary HLSL code directly in material graphs, so that I can create any shader effect without waiting for specific node types to be implemented.

#### Acceptance Criteria

1. WHEN a material graph contains a custom_hlsl() expression, THE Material_Graph SHALL generate a UMaterialExpressionCustom node
2. WHEN HLSL code is provided as a string literal, THE Codegen SHALL embed it in the Custom node's Code property
3. WHEN output_type is specified, THE Codegen SHALL set the OutputType property to match (CMOT_Float1, CMOT_Float2, CMOT_Float3, CMOT_Float4)
4. WHEN custom HLSL references inputs, THE Codegen SHALL create input pins on the Custom node
5. WHEN custom HLSL is compiled, THE Material_Factory SHALL validate HLSL syntax at material creation time

### Requirement 2: Expression to Node Conversion

**User Story:** As a developer, I want KAIN expressions to automatically convert to material nodes, so that I can write natural code instead of manually specifying node types.

#### Acceptance Criteria

1. WHEN a material graph contains arithmetic expressions (+, -, *, /), THE Codegen SHALL generate corresponding math nodes (Add, Subtract, Multiply, Divide)
2. WHEN a material graph contains function calls (lerp, clamp, pow, etc.), THE Codegen SHALL generate corresponding material nodes
3. WHEN a material graph contains variable references, THE Codegen SHALL wire connections between nodes automatically
4. WHEN expressions are nested, THE Codegen SHALL generate intermediate nodes and wire them correctly
5. WHEN expressions reference undefined variables, THE Codegen SHALL report clear error messages with file:line:col

### Requirement 3: Shader Integration

**User Story:** As a developer, I want to call existing KAIN shaders from within materials, so that I can reuse shader code across material graphs and standalone shaders.

#### Acceptance Criteria

1. WHEN a material graph calls a KAIN shader function, THE Codegen SHALL generate a UMaterialExpressionMaterialFunctionCall node
2. WHEN shader parameters are passed, THE Codegen SHALL wire them to the function call inputs
3. WHEN a shader returns a value, THE Codegen SHALL expose it as the function call output
4. WHEN a shader is not found, THE Codegen SHALL report a clear error message
5. WHEN shaders are modified, THE Material_Factory SHALL regenerate materials that reference them

### Requirement 4: Texture Sampling

**User Story:** As a developer, I want to sample textures with proper UV mapping, so that I can create realistic materials with albedo, normal, and roughness maps.

#### Acceptance Criteria

1. WHEN a material graph declares a Texture2D input, THE Codegen SHALL generate a UMaterialExpressionTextureSampleParameter2D node
2. WHEN texture_sample() is called with a texture and UV coordinates, THE Codegen SHALL wire the UV input to the sample node
3. WHEN UV coordinates are not provided, THE Codegen SHALL use default texture coordinates (index 0)
4. WHEN texture channels are accessed (.rgb, .r, .g, .b, .a), THE Codegen SHALL generate ComponentMask nodes
5. WHEN textures are sampled multiple times, THE Codegen SHALL reuse the same texture parameter node

### Requirement 5: UV Manipulation

**User Story:** As a developer, I want to manipulate UV coordinates (scroll, rotate, scale), so that I can create animated textures and tiling effects.

#### Acceptance Criteria

1. WHEN uv_scroll() is called with offset values, THE Codegen SHALL generate Add nodes to offset UV coordinates
2. WHEN uv_scale() is called with scale values, THE Codegen SHALL generate Multiply nodes to scale UV coordinates
3. WHEN uv_rotate() is called with an angle, THE Codegen SHALL generate rotation matrix nodes
4. WHEN UV operations are chained, THE Codegen SHALL wire them in sequence
5. WHEN UV coordinates are used, THE Codegen SHALL generate a TextureCoordinate node if not already present

### Requirement 6: Time-Based Effects

**User Story:** As a developer, I want to access engine time for pulsing and animated materials, so that I can create dynamic visual effects without Blueprint code.

#### Acceptance Criteria

1. WHEN time() is called in a material graph, THE Codegen SHALL generate a UMaterialExpressionTime node
2. WHEN sine() is called with time, THE Codegen SHALL generate a UMaterialExpressionSine node
3. WHEN cosine() is called with time, THE Codegen SHALL generate a UMaterialExpressionCosine node
4. WHEN time is multiplied by a speed parameter, THE Codegen SHALL generate Multiply nodes for animation speed control
5. WHEN time-based effects are used, THE Material_Factory SHALL mark materials as dynamic (not static)

### Requirement 7: Dynamic Materials

**User Story:** As a developer, I want to control material parameters at runtime from Blueprint/C++, so that I can create interactive materials that respond to gameplay.

#### Acceptance Criteria

1. WHEN a material has input parameters, THE Material_Factory SHALL expose them as material instance parameters
2. WHEN Blueprint code calls SetScalarParameterValue, THE Material SHALL update the parameter value immediately
3. WHEN Blueprint code calls SetVectorParameterValue, THE Material SHALL update the parameter value immediately
4. WHEN parameters are modified at runtime, THE Material SHALL recompile only if necessary (dynamic parameters don't trigger recompile)
5. WHEN material instances are created, THE Codegen SHALL generate helper functions for parameter access

### Requirement 8: Material Functions

**User Story:** As a developer, I want to define reusable material functions in KAIN, so that I can build a library of common shader effects and avoid code duplication.

#### Acceptance Criteria

1. WHEN a material function is defined with @material_function, THE Codegen SHALL generate a UMaterialFunction asset
2. WHEN a material function has input parameters, THE Codegen SHALL create function input nodes
3. WHEN a material function has an output, THE Codegen SHALL create a function output node
4. WHEN a material calls a material function, THE Codegen SHALL generate a MaterialFunctionCall node with correct wiring
5. WHEN material functions are nested (function calls function), THE Codegen SHALL resolve dependencies and generate correct call chains

### Requirement 9: Material Layers

**User Story:** As a developer, I want to blend multiple material layers with masks, so that I can create complex surfaces like weathered metal, dirt on surfaces, and decals.

#### Acceptance Criteria

1. WHEN a material defines multiple layers, THE Codegen SHALL generate layer blend nodes (MatLayerBlend_Simple, MatLayerBlend_Standard)
2. WHEN layers have blend masks, THE Codegen SHALL wire mask textures to layer blend alpha inputs
3. WHEN layers have blend modes (Lerp, Additive, Multiply), THE Codegen SHALL set the correct blend mode on layer nodes
4. WHEN layers are stacked, THE Codegen SHALL wire them in order (bottom to top)
5. WHEN layer parameters are exposed, THE Material_Factory SHALL create material instances with per-layer parameter control

### Requirement 10: World-Space Operations

**User Story:** As a developer, I want to use world-space coordinates for triplanar mapping and world-aligned textures, so that I can create seamless textures on any geometry without UV seams.

#### Acceptance Criteria

1. WHEN world_position() is called, THE Codegen SHALL generate a WorldPosition node
2. WHEN world_normal() is called, THE Codegen SHALL generate a VertexNormalWS node
3. WHEN triplanar_sample() is called, THE Codegen SHALL generate triplanar projection nodes (sample texture 3 times, blend by normal)
4. WHEN world-space UVs are used, THE Codegen SHALL generate coordinate transformation nodes (world pos → UV space)
5. WHEN triplanar blending is specified, THE Codegen SHALL generate blend weight calculation based on surface normal

### Requirement 11: Vertex Shaders

**User Story:** As a developer, I want to modify vertex positions in the material, so that I can create effects like waving grass, cloth simulation, and vertex deformation.

#### Acceptance Criteria

1. WHEN world_position_offset output is set, THE Codegen SHALL wire it to the material's WorldPositionOffset pin
2. WHEN vertex displacement uses world-space offsets, THE Codegen SHALL generate world-space displacement nodes
3. WHEN vertex displacement uses object-space offsets, THE Codegen SHALL generate object-space displacement nodes
4. WHEN vertex displacement uses time-based animation, THE Codegen SHALL generate Time nodes and animation logic
5. WHEN vertex normals are modified, THE Codegen SHALL generate normal transformation nodes to maintain correct lighting
