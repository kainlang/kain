# Design Document: USF Shader Pipeline Robustness Fix

## Overview

This design addresses critical issues in the KAIN USF shader compilation pipeline that prevent generated C++ code from compiling in UE5. The core problem is that the codegen fails to distinguish between texture uniforms (Sampler2D, RWTexture2D) and scalar uniforms (Float, Int, Vec3) when generating shader dispatch functions, resulting in type mismatches where texture resource parameters are passed as scalar values.

Additionally, the design addresses compute shader output handling (UAV writes vs returns), texture coordinate normalization, RDG resource transitions, and performance optimizations for GPU execution.

## Architecture

The fix involves modifications to three main components:

1. **USF Codegen** (`kain/src/codegen/usf.rs`): Shader code generation
2. **UE5 Codegen** (`kain/src/codegen/ue5.rs`): C++ binding generation
3. **Type System** (`kain/src/ast.rs`, `kain/src/types.rs`): Uniform classification

### Data Flow

```
KAIN Shader Definition
    ↓
Parse & Type Check
    ↓
Classify Uniforms → [Texture Uniforms] [Scalar Uniforms]
    ↓                      ↓                    ↓
Generate USF Code    Generate C++ Header  Generate C++ Impl
    ↓                      ↓                    ↓
.usf file            .h file (bindings)   .cpp file (dispatch)
```

## Components and Interfaces

### 1. Uniform Classification System

**Purpose**: Distinguish texture uniforms from scalar uniforms during codegen

**Interface**:
```rust
enum UniformKind {
    Texture {
        texture_type: TextureType,  // Sampler2D, RWTexture2D, etc.
        binding: u32,
    },
    Scalar {
        scalar_type: ScalarType,    // Float, Int, Vec3, etc.
        binding: u32,
    },
}

fn classify_uniform(uniform: &Uniform) -> UniformKind {
    match &uniform.ty {
        Type::Named { name, .. } => {
            if is_texture_type(name) {
                UniformKind::Texture { ... }
            } else {
                UniformKind::Scalar { ... }
            }
        }
        _ => UniformKind::Scalar { ... }
    }
}

fn is_texture_type(type_name: &str) -> bool {
    matches!(type_name, 
        "Sampler2D" | "Sampler3D" | "SamplerCube" |
        "Image2D" | "Image3D" |
        "RWTexture2D" | "RWTexture3D" | "RWStructuredBuffer"
    )
}
```


### 2. Dispatch Function Generator

**Purpose**: Generate type-safe C++ dispatch functions that only accept scalar parameters

**Interface**:
```rust
struct DispatchFunctionGenerator {
    shader_name: String,
    scalar_uniforms: Vec<(String, String, u32)>,  // (name, cpp_type, binding)
    texture_uniforms: Vec<(String, String, u32)>,  // (name, usf_type, binding)
}

impl DispatchFunctionGenerator {
    fn generate_signature(&self) -> String {
        // Only include scalar uniforms as parameters
        let params: Vec<String> = self.scalar_uniforms
            .iter()
            .map(|(name, cpp_type, _)| format!("{} {}", cpp_type, name))
            .collect();
        
        format!("void Dispatch{}Shader({})", 
            capitalize_first(&self.shader_name),
            params.join(", ")
        )
    }
    
    fn generate_implementation(&self) -> String {
        // Bind scalar parameters, document texture setup
    }
}
```

### 3. Compute Shader Output Handler

**Purpose**: Generate UAV write statements instead of return statements for compute shaders

**Interface**:
```rust
struct ComputeShaderOutput {
    output_name: String,
    output_type: Type,  // Vec4, Vec3, etc.
    binding: u32,
}

fn generate_compute_output(shader: &TypedShader) -> Option<ComputeShaderOutput> {
    if shader.ast.stage == ShaderStage::Compute && has_return_type(shader) {
        Some(ComputeShaderOutput {
            output_name: "OutputSurface".to_string(),
            output_type: shader.ast.outputs.clone(),
            binding: find_next_binding(&shader.ast.uniforms),
        })
    } else {
        None
    }
}

fn generate_uav_write(output: &ComputeShaderOutput, value_expr: &str) -> String {
    format!("{}[ThreadId.xy] = {};", output.output_name, value_expr)
}
```

### 4. Texture Coordinate Normalizer

**Purpose**: Generate correct UV coordinate calculations for texture sampling

**Interface**:
```rust
struct TextureSamplingContext {
    is_simulation_data: bool,  // position, velocity, etc.
    resolution_uniform: String,
}

fn generate_texture_sample(
    texture_name: &str,
    ctx: &TextureSamplingContext
) -> String {
    if ctx.is_simulation_data {
        // Use Load() for exact pixel reads (no interpolation)
        format!("{}.Load(int3(ThreadId.xy, 0))", texture_name)
    } else {
        // Use Sample() with normalized UVs for visual data
        format!(
            "{}.Sample({}Sampler, (float2(ThreadId.xy) + 0.5) / {})",
            texture_name,
            texture_name,
            ctx.resolution_uniform
        )
    }
}

fn is_simulation_texture(name: &str) -> bool {
    name.contains("position") || 
    name.contains("velocity") || 
    name.contains("force") ||
    name.contains("acceleration")
}
```


### 5. RDG Resource Transition Manager

**Purpose**: Generate proper resource state transitions for RDG passes

**Interface**:
```rust
enum ResourceAccessMode {
    SRVCompute,  // Shader Resource View for compute shader reads
    UAV,         // Unordered Access View for writes
    RenderTarget, // Render target for pixel shader writes
}

struct ResourceTransition {
    resource_name: String,
    access_mode: ResourceAccessMode,
}

fn generate_resource_transitions(
    texture_uniforms: &[(String, String, u32)],
    uav_outputs: &[ComputeShaderOutput]
) -> Vec<ResourceTransition> {
    let mut transitions = Vec::new();
    
    // Input textures need SRVCompute access
    for (name, _, _) in texture_uniforms {
        transitions.push(ResourceTransition {
            resource_name: name.clone(),
            access_mode: ResourceAccessMode::SRVCompute,
        });
    }
    
    // Output UAVs need UAV access
    for output in uav_outputs {
        transitions.push(ResourceTransition {
            resource_name: output.output_name.clone(),
            access_mode: ResourceAccessMode::UAV,
        });
    }
    
    transitions
}

fn generate_transition_code(transition: &ResourceTransition) -> String {
    match transition.access_mode {
        ResourceAccessMode::SRVCompute => {
            format!(
                "GraphBuilder.UseExternalAccessMode({}, ERHIAccess::SRVCompute);",
                transition.resource_name
            )
        }
        ResourceAccessMode::UAV => {
            format!(
                "// {} will be transitioned to UAV by RDG automatically",
                transition.resource_name
            )
        }
        _ => String::new(),
    }
}
```

### 6. Thread Group Size Configuration

**Purpose**: Support configurable thread group sizes via @compute annotation

**Interface**:
```rust
struct ThreadGroupSize {
    x: u32,
    y: u32,
    z: u32,
}

impl ThreadGroupSize {
    fn from_annotation(shader: &Shader) -> Self {
        // Look for @compute(X, Y, Z) attribute
        for attr in &shader.attributes {
            if attr.name == "compute" {
                if let Some(args) = &attr.args {
                    return Self::parse_args(args);
                }
            }
        }
        
        // Default based on shader dimensionality
        Self::default_for_shader(shader)
    }
    
    fn default_for_shader(shader: &Shader) -> Self {
        // Analyze shader to determine if it's 1D or 2D workload
        if is_1d_workload(shader) {
            Self { x: 64, y: 1, z: 1 }  // Optimal for 1D buffers
        } else {
            Self { x: 32, y: 32, z: 1 }  // Optimal for 2D textures
        }
    }
    
    fn validate(&self) -> Result<(), String> {
        let total = self.x * self.y * self.z;
        if total > 1024 {
            Err(format!(
                "Thread group size {}x{}x{} = {} exceeds GPU limit of 1024",
                self.x, self.y, self.z, total
            ))
        } else {
            Ok(())
        }
    }
    
    fn to_hlsl(&self) -> String {
        format!("[numthreads({}, {}, {})]", self.x, self.y, self.z)
    }
}
```


## Data Models

### UniformClassification

```rust
pub struct UniformClassification {
    pub texture_uniforms: Vec<TextureUniform>,
    pub scalar_uniforms: Vec<ScalarUniform>,
}

pub struct TextureUniform {
    pub name: String,
    pub kain_type: String,      // "Sampler2D", "RWTexture2D", etc.
    pub usf_type: String,        // "Texture2D<float4>", "RWTexture2D<float4>", etc.
    pub binding: u32,
    pub is_uav: bool,            // true for RW* types
}

pub struct ScalarUniform {
    pub name: String,
    pub kain_type: String,       // "Float", "Vec3", etc.
    pub usf_type: String,        // "float", "float3", etc.
    pub cpp_type: String,        // "float", "FVector3f", etc.
    pub binding: u32,
}
```

### ShaderGenerationContext

```rust
pub struct ShaderGenerationContext {
    pub shader_name: String,
    pub stage: ShaderStage,
    pub uniforms: UniformClassification,
    pub output: Option<ComputeShaderOutput>,
    pub thread_group_size: ThreadGroupSize,
    pub resolution_uniform: Option<String>,  // For UV normalization
}
```

### GeneratedShaderFiles

```rust
pub struct GeneratedShaderFiles {
    pub usf_file: ShaderFile,
    pub header_file: CppFile,
    pub impl_file: CppFile,
}

pub struct ShaderFile {
    pub filename: String,
    pub content: String,
}

pub struct CppFile {
    pub filename: String,
    pub content: String,
}
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Uniform Classification Completeness

*For any* shader with N uniforms, classifying all uniforms should result in exactly N uniforms distributed between texture and scalar lists, with no overlap.

**Validates: Requirements 1.1, 1.4**

### Property 2: Texture Type Recognition

*For any* uniform with type in {Sampler2D, Sampler3D, SamplerCube, Image2D, Image3D, RWTexture2D, RWTexture3D, RWStructuredBuffer}, the classification should identify it as a texture uniform.

**Validates: Requirements 1.2**

### Property 3: Scalar Type Recognition

*For any* uniform with type not in the texture type set, the classification should identify it as a scalar uniform.

**Validates: Requirements 1.3**

### Property 4: Dispatch Function Signature Correctness

*For any* shader, the generated dispatch function signature should contain exactly one parameter for each scalar uniform and zero parameters for texture uniforms.

**Validates: Requirements 1.5, 2.1, 2.2**

### Property 5: Type Mapping Consistency

*For any* scalar uniform, the C++ parameter type should correctly map from the USF type according to the type mapping table (float→float, float3→FVector3f, int→int32, etc.).

**Validates: Requirements 2.3, 2.4, 2.5**

### Property 6: Parameter Order Preservation

*For any* shader, the order of parameters in the dispatch function signature should match the ascending order of scalar uniform binding annotations.

**Validates: Requirements 2.6**

### Property 7: Shader Parameter Macro Selection

*For any* uniform, the generated shader parameter struct should use SHADER_PARAMETER_TEXTURE for texture uniforms and SHADER_PARAMETER for scalar uniforms.

**Validates: Requirements 3.1, 3.2**

### Property 8: Scalar Parameter Binding

*For any* scalar uniform, the dispatch function implementation should contain a binding statement that assigns the function parameter to the shader parameter struct.

**Validates: Requirements 3.3**

### Property 9: Texture Parameter Non-Binding

*For any* texture uniform, the dispatch function implementation should not contain binding statements from function parameters.

**Validates: Requirements 3.4**


### Property 10: Actor Dispatch Parameter Matching

*For any* actor with state variables and a shader with scalar uniforms, the actor's dispatch call should pass state variables for matching names and default values for non-matching names.

**Validates: Requirements 4.1, 4.3, 4.4**

### Property 11: Multi-Shader Independence

*For any* KAIN program with multiple shaders, each shader's uniform classification and dispatch function generation should be independent (changing shader A's uniforms should not affect shader B's generated code).

**Validates: Requirements 6.1, 6.2, 6.4**

### Property 12: Binding Uniqueness Validation

*For any* shader, all uniform bindings should be unique (no two uniforms should have the same @N annotation).

**Validates: Requirements 7.5**

### Property 13: Compute Shader UAV Generation

*For any* compute shader with a return type, the generated code should include a RWTexture2D UAV parameter and write statements to that UAV, with no return statements in the entry point.

**Validates: Requirements 8.1, 8.2, 8.5**

### Property 14: UAV Parameter Macro Usage

*For any* compute shader output, the generated shader parameter struct should use SHADER_PARAMETER_RDG_TEXTURE_UAV macro.

**Validates: Requirements 8.3**

### Property 15: UV Normalization for Sampling

*For any* texture sample operation in a compute shader, if using Sample() method, the UV coordinates should be normalized using the formula (float2(ThreadId.xy) + 0.5) / Resolution.

**Validates: Requirements 9.1, 9.2**

### Property 16: Simulation Texture Point Sampling

*For any* texture with name containing "position", "velocity", "force", or "acceleration", the generated sampling code should use Load(int3(ThreadId.xy, 0)) instead of Sample().

**Validates: Requirements 9.3, 13.1, 13.5**

### Property 17: Resolution Uniform Addition

*For any* compute shader that uses texture sampling with Sample(), the generated code should include a resolution uniform parameter.

**Validates: Requirements 9.4**

### Property 18: Resource Transition Generation

*For any* texture uniform used as input, the generated RDG pass setup should include resource transition code to SRVCompute state.

**Validates: Requirements 10.1, 10.2**

### Property 19: UAV Resource Transition

*For any* UAV output, the generated RDG pass setup should handle UAV state transitions (either explicitly or via RDG automatic handling).

**Validates: Requirements 10.3**

### Property 20: Thread Group Size Validation

*For any* thread group size configuration, the total thread count (X * Y * Z) should not exceed 1024.

**Validates: Requirements 11.4**

### Property 21: Thread Group Size Annotation Parsing

*For any* shader with @compute(X, Y, Z) annotation, the generated [numthreads(X, Y, Z)] directive should match the annotation values.

**Validates: Requirements 11.1**

### Property 22: Documentation Comment Generation

*For any* generated dispatch function, shader parameter struct, or actor dispatch call, the code should include comments explaining parameter meanings, texture resource setup requirements, or shader behavior.

**Validates: Requirements 3.5, 4.5, 9.5, 10.5, 11.5, 12.4, 12.5, 13.3, 14.1-14.5**


## Error Handling

### Uniform Classification Errors

**Error**: Unsupported uniform type
```rust
if !is_known_type(&uniform.ty) {
    return Err(KainError::UnsupportedUniformType {
        uniform_name: uniform.name.clone(),
        type_name: format!("{:?}", uniform.ty),
        location: uniform.span,
    });
}
```

**Error**: Duplicate binding annotation
```rust
let mut seen_bindings = HashSet::new();
for uniform in &shader.uniforms {
    if !seen_bindings.insert(uniform.binding) {
        return Err(KainError::DuplicateBinding {
            shader_name: shader.name.clone(),
            binding: uniform.binding,
            location: uniform.span,
        });
    }
}
```

### Thread Group Size Errors

**Error**: Thread group size exceeds GPU limits
```rust
let thread_group_size = ThreadGroupSize::from_annotation(shader);
if let Err(msg) = thread_group_size.validate() {
    return Err(KainError::InvalidThreadGroupSize {
        shader_name: shader.name.clone(),
        message: msg,
        location: shader.span,
    });
}
```

### Type Mapping Errors

**Error**: Unknown type in type mapping
```rust
fn map_usf_type_to_cpp(usf_type: &str) -> Result<String, KainError> {
    match usf_type {
        "float" => Ok("float".to_string()),
        "float2" => Ok("FVector2f".to_string()),
        "float3" => Ok("FVector3f".to_string()),
        "float4" => Ok("FVector4f".to_string()),
        "int" => Ok("int32".to_string()),
        // ... more mappings
        _ => Err(KainError::UnknownTypeMapping {
            usf_type: usf_type.to_string(),
        })
    }
}
```

## Testing Strategy

### Unit Tests

Unit tests will focus on specific components and edge cases:

1. **Uniform Classification Tests**
   - Test classification of each texture type
   - Test classification of each scalar type
   - Test mixed uniform lists
   - Test empty uniform lists

2. **Type Mapping Tests**
   - Test each USF→C++ type mapping
   - Test unknown type handling
   - Test vector type conversions

3. **Dispatch Function Generation Tests**
   - Test signature generation with various uniform combinations
   - Test parameter ordering
   - Test implementation binding code

4. **Thread Group Size Tests**
   - Test annotation parsing
   - Test default size selection
   - Test validation (boundary cases: 1024, 1025)

5. **Error Handling Tests**
   - Test duplicate binding detection
   - Test unsupported type errors
   - Test thread group size validation errors

### Property-Based Tests

Property-based tests will verify universal correctness properties across randomized inputs. Each test will run a minimum of 100 iterations.

1. **Property Test: Uniform Classification Completeness**
   ```rust
   #[test]
   fn prop_uniform_classification_complete() {
       // Feature: usf-shader-pipeline-fix, Property 1: Uniform Classification Completeness
       // For any shader with N uniforms, classification results in exactly N uniforms
       // distributed between texture and scalar lists with no overlap
       
       quickcheck(|shader: ArbitraryShader| {
           let classification = classify_uniforms(&shader.uniforms);
           let total = classification.texture_uniforms.len() + 
                      classification.scalar_uniforms.len();
           
           // Check completeness
           assert_eq!(total, shader.uniforms.len());
           
           // Check no overlap
           let texture_names: HashSet<_> = classification.texture_uniforms
               .iter().map(|u| &u.name).collect();
           let scalar_names: HashSet<_> = classification.scalar_uniforms
               .iter().map(|u| &u.name).collect();
           assert!(texture_names.is_disjoint(&scalar_names));
           
           true
       });
   }
   ```


2. **Property Test: Type Recognition**
   ```rust
   #[test]
   fn prop_texture_type_recognition() {
       // Feature: usf-shader-pipeline-fix, Property 2: Texture Type Recognition
       // For any uniform with texture type, classification identifies it as texture
       
       let texture_types = vec![
           "Sampler2D", "Sampler3D", "SamplerCube",
           "Image2D", "Image3D",
           "RWTexture2D", "RWTexture3D", "RWStructuredBuffer"
       ];
       
       quickcheck(|name: String, binding: u32| {
           for texture_type in &texture_types {
               let uniform = create_uniform(&name, texture_type, binding);
               let kind = classify_uniform(&uniform);
               assert!(matches!(kind, UniformKind::Texture { .. }));
           }
           true
       });
   }
   ```

3. **Property Test: Dispatch Function Signature**
   ```rust
   #[test]
   fn prop_dispatch_signature_correctness() {
       // Feature: usf-shader-pipeline-fix, Property 4: Dispatch Function Signature Correctness
       // For any shader, dispatch function has one param per scalar uniform, zero for textures
       
       quickcheck(|shader: ArbitraryShader| {
           let classification = classify_uniforms(&shader.uniforms);
           let signature = generate_dispatch_signature(&shader.name, &classification);
           
           // Count parameters in signature
           let param_count = count_parameters(&signature);
           assert_eq!(param_count, classification.scalar_uniforms.len());
           
           // Verify no texture types in signature
           for texture_uniform in &classification.texture_uniforms {
               assert!(!signature.contains(&texture_uniform.usf_type));
           }
           
           true
       });
   }
   ```

4. **Property Test: Parameter Order Preservation**
   ```rust
   #[test]
   fn prop_parameter_order_preserved() {
       // Feature: usf-shader-pipeline-fix, Property 6: Parameter Order Preservation
       // For any shader, parameter order matches ascending binding order
       
       quickcheck(|shader: ArbitraryShader| {
           let classification = classify_uniforms(&shader.uniforms);
           let signature = generate_dispatch_signature(&shader.name, &classification);
           
           // Extract parameter names from signature
           let param_names = extract_parameter_names(&signature);
           
           // Get expected order (sorted by binding)
           let mut expected_order: Vec<_> = classification.scalar_uniforms
               .iter()
               .map(|u| (u.binding, u.name.clone()))
               .collect();
           expected_order.sort_by_key(|(binding, _)| *binding);
           let expected_names: Vec<_> = expected_order.iter()
               .map(|(_, name)| name.clone())
               .collect();
           
           assert_eq!(param_names, expected_names);
           true
       });
   }
   ```

5. **Property Test: Binding Uniqueness**
   ```rust
   #[test]
   fn prop_binding_uniqueness_validated() {
       // Feature: usf-shader-pipeline-fix, Property 12: Binding Uniqueness Validation
       // For any shader, all uniform bindings should be unique
       
       quickcheck(|shader: ArbitraryShader| {
           let result = validate_shader(&shader);
           
           // Check if shader has duplicate bindings
           let mut bindings = HashSet::new();
           let has_duplicates = shader.uniforms.iter()
               .any(|u| !bindings.insert(u.binding));
           
           if has_duplicates {
               assert!(result.is_err());
               assert!(matches!(result.unwrap_err(), 
                   KainError::DuplicateBinding { .. }));
           } else {
               assert!(result.is_ok());
           }
           
           true
       });
   }
   ```

6. **Property Test: Compute Shader UAV Generation**
   ```rust
   #[test]
   fn prop_compute_shader_uav_generation() {
       // Feature: usf-shader-pipeline-fix, Property 13: Compute Shader UAV Generation
       // For any compute shader with return type, generated code has UAV and writes
       
       quickcheck(|shader: ArbitraryComputeShader| {
           if shader.has_return_type() {
               let usf_code = generate_usf(&shader);
               
               // Should have UAV parameter
               assert!(usf_code.contains("RWTexture2D"));
               assert!(usf_code.contains("OutputSurface"));
               
               // Should have write statement
               assert!(usf_code.contains("OutputSurface[ThreadId.xy]"));
               
               // Should NOT have return statement in entry point
               let entry_point = extract_entry_point(&usf_code);
               assert!(!entry_point.contains("return"));
           }
           true
       });
   }
   ```

7. **Property Test: Simulation Texture Point Sampling**
   ```rust
   #[test]
   fn prop_simulation_texture_point_sampling() {
       // Feature: usf-shader-pipeline-fix, Property 16: Simulation Texture Point Sampling
       // For any texture with simulation name, use Load() not Sample()
       
       let simulation_keywords = vec!["position", "velocity", "force", "acceleration"];
       
       quickcheck(|shader: ArbitraryComputeShader| {
           let usf_code = generate_usf(&shader);
           
           for uniform in &shader.uniforms {
               let is_simulation = simulation_keywords.iter()
                   .any(|kw| uniform.name.to_lowercase().contains(kw));
               
               if is_simulation {
                   // Should use Load()
                   assert!(usf_code.contains(&format!("{}.Load", uniform.name)));
                   // Should NOT use Sample()
                   assert!(!usf_code.contains(&format!("{}.Sample", uniform.name)));
               }
           }
           true
       });
   }
   ```

8. **Property Test: Thread Group Size Validation**
   ```rust
   #[test]
   fn prop_thread_group_size_validation() {
       // Feature: usf-shader-pipeline-fix, Property 20: Thread Group Size Validation
       // For any thread group size, total threads should not exceed 1024
       
       quickcheck(|x: u32, y: u32, z: u32| {
           let size = ThreadGroupSize { x, y, z };
           let result = size.validate();
           
           let total = x * y * z;
           if total > 1024 {
               assert!(result.is_err());
           } else {
               assert!(result.is_ok());
           }
           true
       });
   }
   ```

### Integration Tests

Integration tests will verify end-to-end compilation in UE5:

1. **Test: Simple Compute Shader Compilation**
   - Generate code for a compute shader with mixed texture/scalar uniforms
   - Compile in UE5 project
   - Verify no compilation errors

2. **Test: Multi-Shader Plugin Compilation**
   - Generate code for 3 compute shaders (ParticleVelocity, ParticlePosition, ParticleRender)
   - Compile in UE5 project
   - Verify all shaders compile and link

3. **Test: Actor Integration**
   - Generate actor with shader dispatch in Tick()
   - Compile in UE5 project
   - Verify actor compiles and can be placed in level

4. **Test: Runtime Shader Execution**
   - Run generated shader in UE5 editor
   - Verify shader executes without crashes
   - Verify output textures contain expected data

### Test Configuration

All property-based tests will use the following configuration:
- Minimum 100 iterations per test
- Random seed for reproducibility
- Shrinking enabled for minimal failing cases
- Timeout: 60 seconds per test

