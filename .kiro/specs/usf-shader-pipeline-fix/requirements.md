# Requirements Document

## Introduction

The KAIN USF shader compilation pipeline currently generates incorrect C++ bindings for compute shaders, specifically failing to distinguish between texture uniforms (Sampler2D) and scalar uniforms (Float, Int, etc.) when generating shader dispatch functions. This results in type mismatches where texture resource parameters are passed as scalar values (0.0f, nullptr) instead of proper FRDGTextureRef or FRHITexture* handles.

## Glossary

- **USF**: Unreal Shader Format - UE5's shader language based on HLSL
- **Codegen**: Code generation - the process of transpiling KAIN shaders to USF and C++
- **Dispatch Function**: C++ helper function that enqueues shader execution from game thread
- **RDG**: Render Dependency Graph - UE5's modern rendering API
- **Uniform**: Shader parameter (texture or scalar value)
- **Sampler2D**: KAIN type for 2D texture sampling in shaders
- **Scalar Uniform**: Non-texture shader parameter (Float, Int, Bool, Vec3, etc.)
- **Texture Uniform**: Shader parameter that references a texture resource (Sampler2D, RWTexture2D, etc.)

## Requirements

### Requirement 1: Type-Safe Uniform Classification

**User Story:** As a shader developer, I want the codegen to correctly identify texture vs scalar uniforms, so that generated C++ code has proper parameter types.

#### Acceptance Criteria

1. WHEN analyzing shader uniforms, THE Codegen SHALL classify each uniform as either texture or scalar based on its KAIN type
2. WHEN a uniform has type Sampler2D, Sampler3D, SamplerCube, Image2D, Image3D, RWTexture2D, or RWStructuredBuffer, THE Codegen SHALL classify it as a texture uniform
3. WHEN a uniform has type Float, Int, Bool, Vec2, Vec3, Vec4, or any other non-texture type, THE Codegen SHALL classify it as a scalar uniform
4. THE Codegen SHALL maintain separate lists for texture uniforms and scalar uniforms during code generation
5. WHEN generating dispatch function signatures, THE Codegen SHALL only include scalar uniforms as parameters

### Requirement 2: Correct Dispatch Function Signatures

**User Story:** As a C++ developer, I want dispatch functions to have correct parameter types, so that I can call them without type conversion errors.

#### Acceptance Criteria

1. WHEN generating a dispatch function signature, THE Codegen SHALL include one parameter for each scalar uniform
2. WHEN generating a dispatch function signature, THE Codegen SHALL NOT include parameters for texture uniforms
3. WHEN a scalar uniform has USF type "float", THE Codegen SHALL generate a C++ parameter of type "float"
4. WHEN a scalar uniform has USF type "float3", THE Codegen SHALL generate a C++ parameter of type "FVector3f"
5. WHEN a scalar uniform has USF type "int", THE Codegen SHALL generate a C++ parameter of type "int32"
6. THE Codegen SHALL preserve the order of scalar uniforms based on their @N binding annotations

### Requirement 3: Texture Resource Handling

**User Story:** As a rendering engineer, I want texture uniforms to be handled through RDG resources, so that shaders can access GPU textures correctly.

#### Acceptance Criteria

1. WHEN generating shader parameter structs, THE Codegen SHALL use SHADER_PARAMETER_TEXTURE macro for texture uniforms
2. WHEN generating shader parameter structs, THE Codegen SHALL use SHADER_PARAMETER macro for scalar uniforms
3. WHEN generating dispatch function implementations, THE Codegen SHALL bind scalar uniforms from function parameters
4. WHEN generating dispatch function implementations, THE Codegen SHALL NOT attempt to bind texture uniforms from function parameters
5. THE Codegen SHALL generate comments indicating that texture resources must be set up separately in RDG passes

### Requirement 4: Actor Integration

**User Story:** As a game developer, I want actors to dispatch shaders with correct parameter passing, so that shader execution works without manual code edits.

#### Acceptance Criteria

1. WHEN an actor's Tick() method dispatches a shader, THE Codegen SHALL only pass scalar uniform values
2. WHEN an actor's Tick() method dispatches a shader, THE Codegen SHALL NOT pass texture resources as function parameters
3. WHEN an actor has state variables matching scalar uniform names, THE Codegen SHALL pass those state variables to the dispatch function
4. WHEN an actor does not have a matching state variable for a scalar uniform, THE Codegen SHALL pass a default value
5. THE Codegen SHALL generate comments in actor code indicating that texture resources must be created and bound separately

### Requirement 5: Compilation Validation

**User Story:** As a plugin developer, I want generated code to compile in UE5 without errors, so that I can use shaders immediately after codegen.

#### Acceptance Criteria

1. WHEN compiling generated C++ code in UE5, THE Compiler SHALL NOT produce type conversion errors for dispatch function calls
2. WHEN compiling generated C++ code in UE5, THE Compiler SHALL NOT produce "cannot convert argument" errors
3. WHEN compiling generated shader .usf files in UE5, THE Shader Compiler SHALL successfully compile all shader entry points
4. WHEN linking generated code in UE5, THE Linker SHALL successfully resolve all shader dispatch function references
5. THE Generated Code SHALL compile without requiring any manual edits

### Requirement 6: Multi-Shader Support

**User Story:** As a technical artist, I want to use multiple compute shaders in one plugin, so that I can implement complex GPU-driven effects.

#### Acceptance Criteria

1. WHEN a KAIN program contains multiple shader definitions, THE Codegen SHALL generate separate dispatch functions for each shader
2. WHEN generating dispatch functions for multiple shaders, THE Codegen SHALL correctly classify uniforms independently for each shader
3. WHEN shader A has 3 texture uniforms and shader B has 2 texture uniforms, THE Codegen SHALL generate correct signatures for both
4. WHEN multiple shaders share uniform names, THE Codegen SHALL handle each shader's uniforms independently
5. THE Codegen SHALL generate separate .h and .cpp files for each shader

### Requirement 7: Error Messages and Diagnostics

**User Story:** As a shader developer, I want clear error messages when shader compilation fails, so that I can quickly fix issues.

#### Acceptance Criteria

1. WHEN a shader uniform has an unsupported type, THE Codegen SHALL produce an error message indicating the uniform name and unsupported type
2. WHEN a shader has conflicting binding annotations, THE Codegen SHALL produce an error message indicating the conflict
3. WHEN generated C++ code fails to compile, THE Error Message SHALL include the file name, line number, and specific issue
4. WHEN a dispatch function is called with wrong parameter types, THE Compiler Error SHALL clearly indicate expected vs actual types
5. THE Codegen SHALL validate uniform bindings are unique within each shader

### Requirement 8: Compute Shader Output Handling

**User Story:** As a shader developer, I want compute shaders to write results to UAV resources, so that shader computations are not discarded.

#### Acceptance Criteria

1. WHEN a compute shader has a return type, THE Codegen SHALL generate a RWTexture2D UAV parameter for output
2. WHEN a compute shader returns Vec4, THE Codegen SHALL generate code that writes to OutputSurface[ThreadId.xy]
3. WHEN generating shader parameter structs, THE Codegen SHALL include SHADER_PARAMETER_RDG_TEXTURE_UAV for output textures
4. WHEN a compute shader has multiple outputs, THE Codegen SHALL generate separate UAV parameters for each output
5. THE Codegen SHALL NOT generate "return" statements in compute shader entry points

### Requirement 9: Texture Coordinate Normalization

**User Story:** As a shader developer, I want texture sampling to use correct UV coordinates, so that textures are sampled properly.

#### Acceptance Criteria

1. WHEN sampling textures in compute shaders, THE Codegen SHALL normalize thread IDs to 0.0-1.0 range
2. WHEN calculating UV coordinates, THE Codegen SHALL use formula: (float2(ThreadId.xy) + 0.5) / Resolution
3. WHEN the shader needs exact pixel reads, THE Codegen SHALL use Texture.Load(int3(ThreadId.xy, 0)) instead of Sample()
4. WHEN generating shader code, THE Codegen SHALL pass simulation resolution as a uniform parameter
5. THE Codegen SHALL generate comments explaining UV coordinate calculation

### Requirement 10: RDG Resource Transition Management

**User Story:** As a rendering engineer, I want textures to be properly transitioned to correct states, so that shader execution doesn't produce garbage or flickering.

#### Acceptance Criteria

1. WHEN generating RDG pass setup code, THE Codegen SHALL include resource transition logic for texture parameters
2. WHEN a texture is used as shader input, THE Codegen SHALL transition it to SRVCompute state
3. WHEN a texture is used as UAV output, THE Codegen SHALL transition it to UAV state
4. WHEN registering external textures, THE Codegen SHALL use GraphBuilder.RegisterExternalTexture()
5. THE Codegen SHALL generate comments explaining resource state transitions

### Requirement 11: Configurable Thread Group Sizes

**User Story:** As a performance engineer, I want to configure compute shader thread group sizes, so that I can optimize for different GPU architectures.

#### Acceptance Criteria

1. WHEN a KAIN shader has @compute(X, Y, Z) annotation, THE Codegen SHALL use those values for [numthreads(X, Y, Z)]
2. WHEN no @compute annotation is present, THE Codegen SHALL default to [numthreads(32, 32, 1)] for 2D workloads
3. WHEN no @compute annotation is present, THE Codegen SHALL default to [numthreads(64, 1, 1)] for 1D workloads
4. THE Codegen SHALL validate thread group sizes are within GPU limits (max 1024 threads per group)
5. THE Codegen SHALL generate comments explaining thread group size choices

### Requirement 12: Efficient Texture Format Selection

**User Story:** As a performance engineer, I want textures to use minimal bandwidth, so that shader execution is faster.

#### Acceptance Criteria

1. WHEN a KAIN uniform has type Vec3, THE Codegen SHALL generate Texture2D<float4> with documentation that .w is padding
2. WHEN a KAIN uniform has type Vec4, THE Codegen SHALL generate Texture2D<float4>
3. WHEN a KAIN uniform has type Vec2, THE Codegen SHALL generate Texture2D<float4> with documentation that .zw are padding
4. THE Codegen SHALL include comments indicating which texture channels are used vs padding
5. THE Codegen SHALL document memory bandwidth implications in generated code

### Requirement 13: Safe Texture Sampling for Simulation

**User Story:** As a physics programmer, I want simulation textures to use point sampling, so that particles don't ghost or teleport due to interpolation.

#### Acceptance Criteria

1. WHEN sampling position or velocity textures, THE Codegen SHALL use Texture.Load(int3(ThreadId.xy, 0)) instead of Sample()
2. WHEN sampling visual/color textures, THE Codegen SHALL use Sample() with linear filtering
3. WHEN generating texture sampling code, THE Codegen SHALL include comments explaining sampling method choice
4. THE Codegen SHALL distinguish between simulation data (point sample) and visual data (linear sample)
5. WHEN a texture is used for physics calculations, THE Codegen SHALL enforce point sampling

### Requirement 14: Documentation Generation

**User Story:** As a plugin user, I want generated code to include documentation comments, so that I understand how to use the shader dispatch functions.

#### Acceptance Criteria

1. WHEN generating dispatch function declarations, THE Codegen SHALL include comments explaining parameter meanings
2. WHEN generating dispatch function declarations, THE Codegen SHALL include comments indicating which texture resources must be set up
3. WHEN generating shader parameter structs, THE Codegen SHALL include comments mapping parameters to KAIN uniform names
4. WHEN generating actor Tick() methods, THE Codegen SHALL include comments explaining shader dispatch behavior
5. THE Generated Code SHALL include examples of how to create and bind texture resources in RDG passes
