//! Shader Validation Module
//!
//! Validates KAIN shader code against HLSL/UE5 shader rules BEFORE codegen.
//! Catches shader errors in milliseconds instead of waiting for UE5 shader compilation.
//!
//! Validation layers:
//! 1. Uniform validation - unique bindings, HLSL-compatible types
//! 2. POD struct validation - alignment, padding, HLSL compatibility
//! 3. HLSL syntax validation - keywords, function signatures, semantics
//! 4. Binding validation - slot ranges, conflicts, UE5 conventions

use kain_core::types::{TypedShader, TypedProgram, TypedItem, TypedStruct};
use kain_core::ast::{Type, ShaderStage};
use std::collections::{HashMap, HashSet};
use crate::shader_knowledge::ShaderKnowledge;
use crate::type_mapping::TYPE_MAPPER;

/// Classification of shader uniform types based on their resource binding semantics
/// 
/// The @N annotation has different meanings depending on the uniform type:
/// - **Scalar**: @N is an ordering index for SHADER_PARAMETER_STRUCT layout (no register binding)
/// - **Texture**: @N is a t-register binding (texture register, range 0-127)
/// - **UAV**: @N is a u-register binding (unordered access view register, range 0-63)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UniformClass {
    /// Scalar parameters (Float, Int, UInt, Vec2-4, IVec2-4, UVec2-4, Mat2-4, structs)
    /// @N is an ordering index for SHADER_PARAMETER_STRUCT layout, NOT a b-register binding
    Scalar,
    
    /// Texture parameters (Texture2D, Texture3D, TextureCube, Sampler2D, etc.)
    /// @N is a t-register binding (texture register, valid range: 0-127)
    Texture,
    
    /// UAV parameters (RWTexture2D, RWTexture3D, RWBuffer, RWStructuredBuffer)
    /// @N is a u-register binding (unordered access view register, valid range: 0-63)
    UAV,
}

/// Classify a uniform type based on its resource binding semantics
/// 
/// This function determines how the @N annotation should be interpreted:
/// - Scalar types: @N is an ordering index (no register binding)
/// - Texture types: @N is a t-register binding
/// - UAV types: @N is a u-register binding
/// 
/// # Parameters
/// - `type_name`: The KAIN type name (e.g., "Vec3", "Texture2D", "RWBuffer")
/// 
/// # Returns
/// The classification of the uniform type
/// 
/// # Classification Rules
/// 
/// **Scalar** (ordering index only):
/// - Primitive types: Float, Int, UInt, Bool
/// - Vector types: Vec2, Vec3, Vec4, IVec2, IVec3, IVec4, UVec2, UVec3, UVec4
/// - Matrix types: Mat2, Mat3, Mat4
/// - Lowercase HLSL variants: float, int, uint, bool, float2-4, int2-4, uint2-4
/// - User-defined structs (any type not matching Texture or UAV patterns)
/// 
/// **Texture** (t-register binding, range 0-127):
/// - Texture types: Texture1D, Texture2D, Texture3D, TextureCube
/// - Texture arrays: Texture1DArray, Texture2DArray, TextureCubeArray
/// - Multisampled: Texture2DMS, Texture2DMSArray
/// - Sampler types: Sampler, SamplerState, SamplerComparisonState, Sampler1D, Sampler2D, Sampler3D, SamplerCube
/// - Buffer types: Buffer, StructuredBuffer, ByteAddressBuffer
/// 
/// **UAV** (u-register binding, range 0-63):
/// - RW-prefixed types: RWBuffer, RWStructuredBuffer, RWByteAddressBuffer
/// - RW textures: RWTexture1D, RWTexture2D, RWTexture3D
/// - RW texture arrays: RWTexture1DArray, RWTexture2DArray
/// - Typed RW textures: RWTexture2D_Float, RWTexture2D_Float2, RWTexture2D_Float3, RWTexture2D_Int, RWTexture2D_UInt
pub fn classify_uniform_type(type_name: &str) -> UniformClass {
    // Check for UAV types (RW-prefixed)
    if type_name.starts_with("RW") {
        return UniformClass::UAV;
    }
    
    // Check for Texture types
    if type_name.starts_with("Texture") {
        return UniformClass::Texture;
    }
    
    // Check for Sampler types
    if type_name.contains("Sampler") {
        return UniformClass::Texture;
    }
    
    // Check for Buffer types (non-RW buffers are read-only, use t-register)
    match type_name {
        "Buffer" | "StructuredBuffer" | "ByteAddressBuffer" => {
            return UniformClass::Texture;
        }
        _ => {}
    }
    
    // Everything else is a scalar (primitives, vectors, matrices, structs)
    UniformClass::Scalar
}

/// Shader validator - validates shaders against HLSL/UE5 rules
pub struct ShaderValidator {
    /// HLSL reserved keywords (loaded from shader_knowledge.json)
    hlsl_keywords: HashSet<String>,
    /// C++ keywords that are valid in HLSL but can cause issues in generated C++ code
    cpp_keywords: HashSet<String>,
    /// Shader knowledge database for intrinsics and type validation
    knowledge: ShaderKnowledge,
}

impl ShaderValidator {
    /// Create a new shader validator
    pub fn new() -> Self {
        let mut validator = Self {
            hlsl_keywords: HashSet::new(),
            cpp_keywords: HashSet::new(),
            knowledge: ShaderKnowledge::new(),
        };
        
        // Load shader knowledge from embedded JSON
        let _ = validator.load_shader_knowledge();
        
        // Load C++ keywords
        validator.load_cpp_keywords();
        
        validator
    }
    
    /// Load shader knowledge from embedded JSON data
    fn load_shader_knowledge(&mut self) -> Result<(), String> {
        // Load the embedded shader_knowledge.json
        let json_data = include_str!("../../../unreal/metadata/shader_knowledge.json");
        
        // Try to load shader knowledge, but don't fail if it can't be parsed
        // The knowledge database is optional for basic validation
        if let Err(_e) = self.knowledge.load(json_data) {
            // Silently continue - we can still do basic validation without full knowledge
            // The knowledge database is primarily for advanced intrinsic validation
        }
        
        // Extract HLSL keywords from the loaded data
        // This is more critical, so we try to load it
        if let Err(_e) = self.load_hlsl_keywords(json_data) {
            // If we can't load keywords, add a minimal set manually
            self.load_minimal_hlsl_keywords();
        }
        
        Ok(())
    }
    
    /// Load a minimal set of HLSL keywords if JSON parsing fails
    fn load_minimal_hlsl_keywords(&mut self) {
        // Control flow keywords
        let keywords = vec![
            "if", "else", "for", "while", "do", "switch", "case", "default",
            "break", "continue", "return", "discard",
            // Type qualifiers
            "const", "static", "uniform", "extern", "precise", "shared",
            "groupshared", "volatile", "row_major", "column_major",
            // Parameter qualifiers
            "in", "out", "inout", "nointerpolation", "linear", "centroid",
            "noperspective", "sample", "point", "line", "triangle",
            // Shader stages
            "vertex", "pixel", "geometry", "hull", "domain", "compute",
        ];
        
        for kw in keywords {
            self.hlsl_keywords.insert(kw.to_string());
        }
    }
    
    /// Load C++ keywords that are valid in HLSL but can cause issues in generated C++ code
    /// These keywords are valid HLSL identifiers but will conflict when generating C++ shader bindings
    fn load_cpp_keywords(&mut self) {
        let keywords = vec![
            // C++ keywords that are NOT HLSL keywords (lowercase)
            "class", "namespace", "template", "typename", "using",
            "public", "private", "protected", "friend", "virtual",
            "explicit", "operator", "this", "new", "delete",
            "try", "catch", "throw", "nullptr", "auto",
            "decltype", "constexpr", "noexcept", "alignas", "alignof",
            "static_assert", "thread_local", "mutable",
            // C++ standard library names that might conflict (lowercase)
            "std", "string", "vector", "map", "set", "list",
            "array", "pair", "tuple", "function", "bind",
        ];
        
        // Add lowercase keywords
        for kw in keywords {
            self.cpp_keywords.insert(kw.to_string());
        }
        
        // Add UE5 macro names (case-sensitive, stored as-is)
        let ue5_macros = vec![
            "UPROPERTY", "UFUNCTION", "UCLASS", "USTRUCT", "UENUM",
            "GENERATED_BODY", "GENERATED_UCLASS_BODY",
            "TEXT", "LOCTEXT", "NSLOCTEXT",
            "check", "checkf", "ensure", "verify",
        ];
        
        for macro_name in ue5_macros {
            // Store both the original and lowercase version for case-insensitive matching
            self.cpp_keywords.insert(macro_name.to_string());
            self.cpp_keywords.insert(macro_name.to_lowercase());
        }
    }
    
    /// Extract HLSL keywords from shader_knowledge.json
    fn load_hlsl_keywords(&mut self, json_data: &str) -> Result<(), String> {
        // Parse JSON to extract hlsl_keywords section
        let value: serde_json::Value = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse shader knowledge JSON: {}", e))?;
        
        if let Some(keywords_obj) = value.get("hlsl_keywords") {
            // Extract control flow keywords
            if let Some(control_flow) = keywords_obj.get("control_flow").and_then(|v| v.as_array()) {
                for kw in control_flow {
                    if let Some(s) = kw.as_str() {
                        self.hlsl_keywords.insert(s.to_string());
                    }
                }
            }
            
            // Extract type qualifiers
            if let Some(type_quals) = keywords_obj.get("type_qualifiers").and_then(|v| v.as_array()) {
                for kw in type_quals {
                    if let Some(s) = kw.as_str() {
                        self.hlsl_keywords.insert(s.to_string());
                    }
                }
            }
            
            // Extract parameter qualifiers
            if let Some(param_quals) = keywords_obj.get("parameter_qualifiers").and_then(|v| v.as_array()) {
                for kw in param_quals {
                    if let Some(s) = kw.as_str() {
                        self.hlsl_keywords.insert(s.to_string());
                    }
                }
            }
            
            // Extract function qualifiers
            if let Some(func_quals) = keywords_obj.get("function_qualifiers").and_then(|v| v.as_array()) {
                for kw in func_quals {
                    if let Some(s) = kw.as_str() {
                        self.hlsl_keywords.insert(s.to_string());
                    }
                }
            }
            
            // Extract shader stages
            if let Some(stages) = keywords_obj.get("shader_stages").and_then(|v| v.as_array()) {
                for kw in stages {
                    if let Some(s) = kw.as_str() {
                        self.hlsl_keywords.insert(s.to_string());
                    }
                }
            }
        }
        
        Ok(())
    }
    
    /// Main validation entry point - orchestrates all validation checks
    /// Returns Ok(()) if validation passes, Err with list of error messages if it fails
    /// 
    /// # Parameters
    /// - `shader`: The shader to validate
    /// - `program`: Optional program containing struct definitions for POD struct validation
    pub fn validate_shader(&mut self, shader: &TypedShader, program: Option<&TypedProgram>) -> Result<(), Vec<String>> {
        let mut errors = Vec::new();
        
        // Run all validation sub-methods
        self.validate_uniforms(shader, program, &mut errors);
        self.validate_pod_structs(shader, program, &mut errors);
        self.validate_hlsl_syntax(shader, &mut errors);
        self.validate_bindings(shader, &mut errors);
        
        if errors.is_empty() {
            Ok(())
        } else {
            Err(errors)
        }
    }
    
    /// Validate shader uniforms
    /// Requirements:
    /// - Unique binding slots (no conflicts within same resource type)
    /// - HLSL-compatible types
    /// - Valid binding ranges for resource type
    /// - Permutation uniforms follow CFG_* or ENABLE_* naming convention
    fn validate_uniforms(&mut self, shader: &TypedShader, program: Option<&TypedProgram>, errors: &mut Vec<String>) {
        let shader_name = &shader.ast.name;
        
        // Note: Binding conflict detection is now handled in validate_bindings()
        // which properly tracks bindings by resource type
        
        for uniform in &shader.ast.uniforms {
            let uniform_name = &uniform.name;
            
            // Validate uniform type is HLSL-compatible
            self.validate_uniform_type(shader_name, uniform_name, &uniform.ty, program, errors);
            
            // Validate binding range based on resource type
            // Note: This is also checked in validate_bindings() but we keep it here for early detection
            self.validate_binding_range(shader_name, uniform_name, &uniform.ty, uniform.binding, errors);
            
            // Validate permutation naming convention
            self.validate_permutation_naming(shader_name, uniform_name, &uniform.ty, errors);
        }
    }
    
    /// Validate that a uniform type is HLSL-compatible
    fn validate_uniform_type(
        &self,
        shader_name: &str,
        uniform_name: &str,
        ty: &Type,
        program: Option<&TypedProgram>,
        errors: &mut Vec<String>,
    ) {
        match ty {
            Type::Named { name, .. } => {
                // Check if it's a known HLSL type or texture/sampler type
                let type_name = name.as_str();
                
                // Reject String types explicitly - they are not supported in shaders
                if type_name == "String" || type_name == "string" {
                    errors.push(format!(
                        "Shader '{}': Uniform '{}' has String type.

Problem: String types cannot be passed to GPU shaders.
  • Shaders run on GPU hardware which doesn't support dynamic strings
  • HLSL has no string type - only numeric types, vectors, matrices, textures, buffers

How to fix:
  1. Use numeric indices instead of strings
     ❌ uniform texture_name: String @0
     ✅ uniform texture_index: Int @0
  
  2. Use texture/sampler types directly
     ❌ uniform albedo_path: String @0
     ✅ uniform albedo_map: Sampler2D @1
  
  3. For text rendering, use texture atlases with character indices
     ✅ uniform font_atlas: Texture2D @0
     ✅ uniform char_index: Int @1
  
  4. For debug output, use numeric codes
     ✅ uniform debug_mode: Int @0  # 0=off, 1=normals, 2=uvs

Valid shader uniform types:
  • Scalars: Int, UInt, Float, Bool
  • Vectors: Vec2, Vec3, Vec4 (and IVec*, UVec* variants)
  • Matrices: Mat2, Mat3, Mat4
  • Textures: Texture2D, Texture3D, TextureCube, Sampler2D
  • Buffers: Buffer<T>, RWBuffer<T>, StructuredBuffer<T>
  • User-defined POD structs (no strings inside)

Documentation: https://kain.dev/docs/shaders/types",
                        shader_name, uniform_name
                    ));
                    return; // Early return to avoid further validation
                }
                
                // First check if TYPE_MAPPER can map this type (KAIN types like Vec3, UVec2, Mat4, etc.)
                if TYPE_MAPPER.can_map(type_name) {
                    return; // Valid KAIN type that maps to HLSL
                }
                
                // Check for additional HLSL types not in TYPE_MAPPER (lowercase variants, texture types, etc.)
                let additional_hlsl_types = [
                    // Lowercase variants (HLSL native)
                    "float", "float2", "float3", "float4",
                    "int", "int2", "int3", "int4",
                    "uint", "uint2", "uint3", "uint4",
                    "bool", "bool2", "bool3", "bool4",
                    "half", "half2", "half3", "half4",
                    "double", "double2", "double3", "double4",
                    // Matrix types
                    "float2x2", "float3x3", "float4x4",
                    "float3x4", "float4x3",
                    // Texture types
                    "Texture1D", "Texture2D", "Texture3D", "TextureCube",
                    "Texture1DArray", "Texture2DArray", "TextureCubeArray",
                    "Texture2DMS", "Texture2DMSArray",
                    // Sampler types
                    "Sampler", "SamplerState", "SamplerComparisonState",
                    "Sampler1D",
                    // Buffer types
                    "Buffer", "StructuredBuffer", "ByteAddressBuffer",
                    "RWBuffer", "RWStructuredBuffer", "RWByteAddressBuffer",
                    "RWTexture1D", "RWTexture1DArray", "RWTexture2DArray",
                    // KAIN image/texture types
                    "Image2D", "Image3D",
                    "RWTexture2D_Float", "RWTexture2D_Float2", "RWTexture2D_Float3",
                    "RWTexture2D_Int", "RWTexture2D_UInt",
                ];
                
                if additional_hlsl_types.contains(&type_name) {
                    return; // Valid HLSL type
                }
                
                // User-defined structs are valid uniform types for POD parameter blocks.
                // Accept if the type is declared in the program as a struct.
                let is_user_struct = program.map_or(false, |p| {
                    p.items.iter().any(|item| {
                        matches!(item, TypedItem::Struct(s) if s.ast.name == type_name)
                    })
                });

                if is_user_struct {
                    return;
                }

                // Type is invalid - generate error with list of valid types
                let valid_kain_types = TYPE_MAPPER.valid_types();
                let valid_types_list = valid_kain_types.join(", ");
                
                errors.push(format!(
                    "Shader '{}': Uniform '{}' has invalid HLSL type '{}'. \
                    Valid KAIN types: {}. \
                    Also supported: lowercase HLSL types (float, int, uint, etc.), texture types (Texture2D, etc.), \
                    sampler types (SamplerState, etc.), buffer types (RWBuffer, etc.), and user-defined structs.",
                    shader_name, uniform_name, type_name, valid_types_list
                ));
            }
            _ => {
                errors.push(format!(
                    "Shader '{}': Uniform '{}' has invalid type. Uniforms must be named types (scalars, vectors, textures, samplers, buffers).",
                    shader_name, uniform_name
                ));
            }
        }
    }
    
    /// Validate binding slot range based on resource type
    fn validate_binding_range(&self, shader_name: &str, uniform_name: &str, ty: &Type, binding: u32, errors: &mut Vec<String>) {
        if let Type::Named { name, .. } = ty {
            let type_name = name.as_str();
            
            // Texture slots: t0-t127
            if type_name.starts_with("Texture") || type_name == "Buffer" || type_name == "StructuredBuffer" || type_name == "ByteAddressBuffer" {
                if binding > 127 {
                    errors.push(format!(
                        "Shader '{}': Uniform '{}' (texture/buffer) uses binding @{} which exceeds maximum texture slot t127",
                        shader_name, uniform_name, binding
                    ));
                }
            }
            
            // UAV slots: u0-u63
            if type_name.starts_with("RW") {
                if binding > 63 {
                    errors.push(format!(
                        "Shader '{}': Uniform '{}' (UAV) uses binding @{} which exceeds maximum UAV slot u63",
                        shader_name, uniform_name, binding
                    ));
                }
            }
            
            // Sampler slots: s0-s15
            if type_name.contains("Sampler") {
                if binding > 15 {
                    errors.push(format!(
                        "Shader '{}': Uniform '{}' (sampler) uses binding @{} which exceeds maximum sampler slot s15",
                        shader_name, uniform_name, binding
                    ));
                }
            }
            
            // Constant buffer slots: b0-b13 (b0 reserved for View)
            // Note: This is checked in validate_bindings for cbuffer-specific logic
        }
    }
    
    /// Validate permutation uniform naming convention
    /// Permutation uniforms are special compile-time flags that control shader variants.
    /// They must follow the naming convention: CFG_* or ENABLE_*
    /// 
    /// Requirements:
    /// - Permutation uniforms should have CFG_* or ENABLE_* prefix
    /// - Type should be Float (used as boolean flag in HLSL)
    fn validate_permutation_naming(&self, shader_name: &str, uniform_name: &str, ty: &Type, errors: &mut Vec<String>) {
        // Check if this looks like a permutation uniform based on naming
        let is_permutation_name = uniform_name.starts_with("CFG_") || uniform_name.starts_with("ENABLE_");
        
        // Check if the type is Float (permutation uniforms should be Float type)
        let is_float_type = if let Type::Named { name, .. } = ty {
            name == "Float" || name == "float"
        } else {
            false
        };
        
        // If it has a permutation-style name, validate it's properly configured
        if is_permutation_name {
            if !is_float_type {
                errors.push(format!(
                    "Shader '{}': Permutation uniform '{}' should have Float type (used as boolean flag). \
                    Found type: {:?}",
                    shader_name, uniform_name, ty
                ));
            }
            
            // Additional validation: permutation names should be all uppercase with underscores
            if !uniform_name.chars().all(|c| c.is_uppercase() || c.is_numeric() || c == '_') {
                errors.push(format!(
                    "Shader '{}': Permutation uniform '{}' should be all uppercase with underscores (e.g., CFG_HIGH_QUALITY, ENABLE_SHADOWS)",
                    shader_name, uniform_name
                ));
            }
        }
        
        // If it's a Float type but doesn't have permutation naming, suggest it might be a permutation
        // Only suggest if it's not already a valid regular uniform name
        if is_float_type && !is_permutation_name {
            // Check if the name looks like it could be a configuration flag
            let uppercase_name = uniform_name.to_uppercase();
            if uppercase_name.contains("CONFIG") || uppercase_name.contains("ENABLE") || 
               uppercase_name.contains("DISABLE") || uppercase_name.contains("USE") ||
               uppercase_name.contains("FEATURE") || uppercase_name.contains("QUALITY") {
                errors.push(format!(
                    "Shader '{}': Uniform '{}' appears to be a configuration flag but doesn't follow permutation naming convention. \
                    Consider renaming to CFG_{} or ENABLE_{}",
                    shader_name, uniform_name, 
                    uniform_name.to_uppercase().replace("CONFIG_", "").replace("ENABLE_", ""),
                    uniform_name.to_uppercase().replace("CONFIG_", "").replace("ENABLE_", "")
                ));
            }
        }
    }
    
    /// Check if a uniform is a permutation uniform (CFG_* or ENABLE_* prefix)
    fn is_permutation_uniform(&self, uniform_name: &str) -> bool {
        uniform_name.starts_with("CFG_") || uniform_name.starts_with("ENABLE_")
    }
    
    /// Validate POD structs used in shaders
    /// Requirements:
    /// - Check for redefinitions (struct defined in both .kn source and generated code)
    /// - Verify field types are HLSL-compatible
    /// - Validate alignment requirements (16-byte for constant buffers)
    /// - No unsupported types (strings, arrays without size, pointers)
    /// 
    /// POD (Plain Old Data) structs are used for shader parameter passing between CPU and GPU.
    /// They must follow strict rules:
    /// 1. All fields must be HLSL-compatible types (no strings, pointers, complex types)
    /// 2. Structs used in constant buffers must have 16-byte alignment
    /// 3. No redefinitions - if a struct is defined in .kn source, codegen shouldn't redefine it
    fn validate_pod_structs(&self, shader: &TypedShader, program: Option<&TypedProgram>, errors: &mut Vec<String>) {
        let shader_name = &shader.ast.name;
        
        // If no program provided, we can't validate POD structs
        if program.is_none() {
            return;
        }
        
        let program = program.unwrap();
        
        // Collect all struct definitions from the program
        let mut struct_defs: HashMap<String, &TypedStruct> = HashMap::new();
        for item in &program.items {
            if let TypedItem::Struct(typed_struct) = item {
                struct_defs.insert(typed_struct.ast.name.clone(), typed_struct);
            }
        }
        
        // Track structs referenced by this shader (via uniforms or other means)
        let mut referenced_structs: HashSet<String> = HashSet::new();
        
        // Check uniforms for struct types
        for uniform in &shader.ast.uniforms {
            if let Type::Named { name, .. } = &uniform.ty {
                // Check if this is a user-defined struct (not a built-in HLSL type)
                if struct_defs.contains_key(name) {
                    referenced_structs.insert(name.clone());
                }
            }
        }
        
        // Validate each referenced struct
        for struct_name in &referenced_structs {
            if let Some(typed_struct) = struct_defs.get(struct_name) {
                self.validate_pod_struct_definition(shader_name, typed_struct, errors);
            }
        }
        
        // Check for potential redefinitions
        // If shader codegen would generate a struct with the same name, that's a redefinition
        self.check_pod_struct_redefinitions(shader_name, &referenced_structs, errors);
    }
    
    /// Validate a single POD struct definition
    fn validate_pod_struct_definition(&self, shader_name: &str, typed_struct: &TypedStruct, errors: &mut Vec<String>) {
        let struct_name = &typed_struct.ast.name;
        
        // Track total size and alignment for 16-byte alignment check
        let mut total_size = 0;
        
        // Validate each field
        for field in &typed_struct.ast.fields {
            let field_name = &field.name;
            let field_type = &field.ty;
            
            // Check if field type is HLSL-compatible
            if !self.is_hlsl_compatible_type(field_type) {
                errors.push(format!(
                    "Shader '{}': POD struct '{}' field '{}' has non-HLSL-compatible type '{:?}'. \
                    POD structs must only contain HLSL-compatible types (scalars, vectors, matrices).",
                    shader_name, struct_name, field_name, field_type
                ));
            }
            
            // Calculate field size for alignment check
            let field_size = self.get_hlsl_type_size(field_type);
            total_size += field_size;
            
            // Check for unsupported types
            match field_type {
                Type::Named { name, .. } => {
                    let type_name = name.as_str();
                    
                    // String types are not allowed in POD structs
                    if type_name == "String" || type_name == "string" {
                        errors.push(format!(
                            "Shader '{}': POD struct '{}' field '{}' has String type. \
                            Strings are not supported in shader POD structs.",
                            shader_name, struct_name, field_name
                        ));
                    }
                    
                    // Pointer types are not allowed (check for * in name)
                    if type_name.ends_with('*') {
                        errors.push(format!(
                            "Shader '{}': POD struct '{}' field '{}' has pointer type. \
                            Pointers are not supported in shader POD structs.",
                            shader_name, struct_name, field_name
                        ));
                    }
                }
                Type::Array(..) => {
                    // Arrays in POD structs must have explicit size
                    // Note: This is a simplified check - proper implementation would verify size is specified
                    errors.push(format!(
                        "Shader '{}': POD struct '{}' field '{}' has array type. \
                        Arrays in POD structs must have explicit size and be HLSL-compatible.",
                        shader_name, struct_name, field_name
                    ));
                }
                Type::Ref { .. } => {
                    errors.push(format!(
                        "Shader '{}': POD struct '{}' field '{}' has reference type. \
                        References are not supported in shader POD structs.",
                        shader_name, struct_name, field_name
                    ));
                }
                _ => {}
            }
        }
        
        // Check 16-byte alignment for constant buffer structs
        // Constant buffers in HLSL must be 16-byte aligned
        if total_size % 16 != 0 {
            let padding_needed = 16 - (total_size % 16);
            errors.push(format!(
                "Shader '{}': POD struct '{}' has size {} bytes which is not 16-byte aligned. \
                Constant buffer structs must be 16-byte aligned. Add {} bytes of padding.",
                shader_name, struct_name, total_size, padding_needed
            ));
        }
    }
    
    /// Check if a type is HLSL-compatible
    fn is_hlsl_compatible_type(&self, ty: &Type) -> bool {
        match ty {
            Type::Named { name, .. } => {
                let type_name = name.as_str();
                
                // Check if TYPE_MAPPER can map this type
                if TYPE_MAPPER.can_map(type_name) {
                    return true;
                }
                
                // Check for additional HLSL types not in TYPE_MAPPER
                let additional_hlsl_types = [
                    // Lowercase variants (HLSL native)
                    "float", "float2", "float3", "float4",
                    "int", "int2", "int3", "int4",
                    "uint", "uint2", "uint3", "uint4",
                    "bool", "bool2", "bool3", "bool4",
                    "half", "half2", "half3", "half4",
                    "double", "double2", "double3", "double4",
                    // Matrix types
                    "float2x2", "float3x3", "float4x4", "float3x4", "float4x3",
                    "matrix", "Matrix",
                ];
                
                additional_hlsl_types.contains(&type_name)
            }
            _ => false,
        }
    }
    
    /// Get the size of an HLSL type in bytes
    fn get_hlsl_type_size(&self, ty: &Type) -> usize {
        match ty {
            Type::Named { name, .. } => {
                let type_name = name.as_str();
                
                // Size mapping for HLSL types
                match type_name {
                    // Scalars (4 bytes each)
                    "float" | "Float" | "int" | "Int" | "uint" | "Uint" | "UInt" | "bool" | "Bool" => 4,
                    // Half precision (2 bytes)
                    "half" | "Half" => 2,
                    // Double precision (8 bytes)
                    "double" | "Double" => 8,
                    // Vectors - float variants
                    "float2" | "Float2" | "Vec2" => 8,
                    "float3" | "Float3" | "Vec3" => 12,
                    "float4" | "Float4" | "Vec4" => 16,
                    // Vectors - int variants
                    "int2" | "Int2" | "IVec2" => 8,
                    "int3" | "Int3" | "IVec3" => 12,
                    "int4" | "Int4" | "IVec4" => 16,
                    // Vectors - uint variants
                    "uint2" | "Uint2" | "UVec2" => 8,
                    "uint3" | "Uint3" | "UVec3" => 12,
                    "uint4" | "Uint4" | "UVec4" => 16,
                    // Vectors - bool variants
                    "bool2" | "Bool2" => 8,
                    "bool3" | "Bool3" => 12,
                    "bool4" | "Bool4" => 16,
                    // Vectors - half variants
                    "half2" | "Half2" => 4,
                    "half3" | "Half3" => 6,
                    "half4" | "Half4" => 8,
                    // Vectors - double variants
                    "double2" | "Double2" => 16,
                    "double3" | "Double3" => 24,
                    "double4" | "Double4" => 32,
                    // Matrices
                    "float2x2" | "Mat2" => 16,
                    "float3x3" | "Mat3" => 36,
                    "float4x4" | "Mat4" => 64,
                    "float3x4" => 48,
                    "float4x3" => 48,
                    // Default to 4 bytes for unknown types
                    _ => 4,
                }
            }
            _ => 4, // Default size
        }
    }
    
    /// Check for POD struct redefinitions
    /// If shader codegen would generate a struct with the same name as one in the source,
    /// that's a redefinition error
    fn check_pod_struct_redefinitions(&self, shader_name: &str, referenced_structs: &HashSet<String>, errors: &mut Vec<String>) {
        // This is a placeholder for checking redefinitions
        // In practice, this would check if the shader codegen would generate a struct
        // with the same name as one already defined in the .kn source
        // 
        // For now, we'll just warn about potential issues
        // The actual implementation would need to know what structs the codegen will generate
        
        // Common shader parameter struct names that might be auto-generated
        let common_generated_names = ["ShaderParams", "MaterialParams", "ComputeParams"];
        
        for generated_name in &common_generated_names {
            if referenced_structs.contains(*generated_name) {
                errors.push(format!(
                    "Shader '{}': Struct '{}' may conflict with auto-generated shader parameter struct. \
                    Consider using a different name to avoid redefinition.",
                    shader_name, generated_name
                ));
            }
        }
    }
    
    /// Validate HLSL syntax
    /// Requirements:
    /// - No use of HLSL reserved keywords as identifiers
    /// - No use of C++ keywords as HLSL identifiers (causes issues in generated C++ code)
    /// - Valid function signatures
    /// - Proper semantic usage
    /// - Function return types are HLSL-compatible
    /// - Function parameter types are HLSL-compatible
    fn validate_hlsl_syntax(&self, shader: &TypedShader, errors: &mut Vec<String>) {
        let shader_name = &shader.ast.name;
        
        // Check shader name doesn't conflict with HLSL keywords
        if self.hlsl_keywords.contains(&shader.ast.name.to_lowercase()) {
            errors.push(format!(
                "Shader '{}': Shader name conflicts with HLSL reserved keyword",
                shader_name
            ));
        }
        
        // Check shader name doesn't conflict with C++ keywords
        if self.cpp_keywords.contains(&shader.ast.name.to_lowercase()) {
            errors.push(format!(
                "Shader '{}': Shader name conflicts with C++ keyword '{}'. \
                This will cause issues in generated C++ shader binding code. \
                Consider renaming to '{}_Shader' or 'Shader_{}'",
                shader_name, shader.ast.name, shader.ast.name, shader.ast.name
            ));
        }
        
        // Check uniform names don't conflict with HLSL keywords
        for uniform in &shader.ast.uniforms {
            if self.hlsl_keywords.contains(&uniform.name.to_lowercase()) {
                errors.push(format!(
                    "Shader '{}': Uniform '{}' conflicts with HLSL reserved keyword",
                    shader_name, uniform.name
                ));
            }
            
            // Check uniform names don't conflict with C++ keywords
            if self.cpp_keywords.contains(&uniform.name.to_lowercase()) {
                errors.push(format!(
                    "Shader '{}': Uniform '{}' conflicts with C++ keyword. \
                    This will cause issues in generated C++ shader parameter struct. \
                    Consider renaming to '{}_param' or 'shader_{}'",
                    shader_name, uniform.name, uniform.name, uniform.name
                ));
            }
            
            // Validate uniform type is HLSL-compatible (for function signature validation)
            self.validate_hlsl_type_compatibility(shader_name, &format!("uniform '{}'", uniform.name), &uniform.ty, errors);
        }
        
        // Check input parameter names don't conflict with HLSL keywords
        for input in &shader.ast.inputs {
            if self.hlsl_keywords.contains(&input.name.to_lowercase()) {
                errors.push(format!(
                    "Shader '{}': Input parameter '{}' conflicts with HLSL reserved keyword",
                    shader_name, input.name
                ));
            }
            
            // Check input parameter names don't conflict with C++ keywords
            if self.cpp_keywords.contains(&input.name.to_lowercase()) {
                errors.push(format!(
                    "Shader '{}': Input parameter '{}' conflicts with C++ keyword. \
                    This will cause issues in generated C++ code. \
                    Consider renaming to '{}_input' or 'in_{}'",
                    shader_name, input.name, input.name, input.name
                ));
            }
            
            // Validate input parameter type is HLSL-compatible
            self.validate_hlsl_type_compatibility(shader_name, &format!("input parameter '{}'", input.name), &input.ty, errors);
        }
        
        // Validate shader return type is HLSL-compatible
        self.validate_hlsl_type_compatibility(shader_name, "return type", &shader.ast.outputs, errors);
        
        // Validate shader stage has appropriate inputs/outputs
        self.validate_shader_stage_signature(shader, errors);
    }
    
    /// Validate that a type is HLSL-compatible for use in function signatures
    /// This checks both the type itself and provides context-specific error messages
    fn validate_hlsl_type_compatibility(&self, shader_name: &str, context: &str, ty: &Type, errors: &mut Vec<String>) {
        match ty {
            Type::Named { name, .. } => {
                let type_name = name.as_str();
                
                // Check for invalid types that should never be used in shaders (check this first)
                let invalid_types = ["String", "string", "Array", "Map", "Set"];
                if invalid_types.contains(&type_name) {
                    errors.push(format!(
                        "Shader '{}': {} has invalid HLSL type '{}'. \
                        This type is not supported in HLSL shaders.",
                        shader_name, context, type_name
                    ));
                    return;
                }
                
                // First check if TYPE_MAPPER can map this type
                if TYPE_MAPPER.can_map(type_name) {
                    return; // Valid KAIN type that maps to HLSL
                }
                
                // Check for additional HLSL types not in TYPE_MAPPER
                let additional_hlsl_types = [
                    // Lowercase variants (HLSL native)
                    "float", "float2", "float3", "float4",
                    "int", "int2", "int3", "int4",
                    "uint", "uint2", "uint3", "uint4",
                    "bool", "bool2", "bool3", "bool4",
                    "half", "half2", "half3", "half4",
                    "double", "double2", "double3", "double4",
                    // Matrix types
                    "float2x2", "float3x3", "float4x4", "float3x4", "float4x3",
                    "matrix", "Matrix",
                    // Texture types (valid for parameters)
                    "Texture1D", "Texture2D", "Texture3D", "TextureCube",
                    "Texture1DArray", "Texture2DArray", "TextureCubeArray",
                    "Texture2DMS", "Texture2DMSArray",
                    // Sampler types
                    "Sampler", "SamplerState", "SamplerComparisonState",
                    "Sampler1D",
                    // Buffer types
                    "Buffer", "StructuredBuffer", "ByteAddressBuffer",
                    "RWBuffer", "RWStructuredBuffer", "RWByteAddressBuffer",
                    "RWTexture1D", "RWTexture1DArray", "RWTexture2DArray",
                    // Special return types
                    "SurfaceOutput", "void",
                ];
                
                if additional_hlsl_types.contains(&type_name) {
                    return; // Valid HLSL type
                }
                
                // Check if it might be a user-defined struct (which is valid)
                // User-defined structs typically start with uppercase
                if type_name.chars().next().map_or(false, |c| c.is_uppercase()) {
                    return; // Likely a user-defined struct
                }
                
                // Type is invalid - generate error with list of valid types
                let valid_kain_types = TYPE_MAPPER.valid_types();
                let valid_types_list = valid_kain_types.join(", ");
                
                errors.push(format!(
                    "Shader '{}': {} has invalid HLSL type '{}'. \
                    Valid KAIN types: {}. \
                    Also supported: lowercase HLSL types (float, int, uint, etc.), texture types (Texture2D, etc.), \
                    sampler types (SamplerState, etc.), buffer types (RWBuffer, etc.), and user-defined structs.",
                    shader_name, context, type_name, valid_types_list
                ));
            }
            Type::Array(..) => {
                errors.push(format!(
                    "Shader '{}': {} uses array type. \
                    Arrays in HLSL function signatures must be passed as buffer types (StructuredBuffer, etc.).",
                    shader_name, context
                ));
            }
            Type::Ref { .. } => {
                errors.push(format!(
                    "Shader '{}': {} uses reference type. \
                    References are not supported in HLSL.",
                    shader_name, context
                ));
            }
            _ => {
                // Other types might be valid, but we can't determine without more context
            }
        }
    }
    
    /// Validate shader stage has appropriate signature
    fn validate_shader_stage_signature(&self, shader: &TypedShader, errors: &mut Vec<String>) {
        let shader_name = &shader.ast.name;
        let stage = shader.ast.stage;
        
        match stage {
            ShaderStage::Fragment => {
                // Fragment shaders should have at least UV input
                if shader.ast.inputs.is_empty() {
                    errors.push(format!(
                        "Shader '{}': Fragment shader should have at least one input parameter (typically UV coordinates)",
                        shader_name
                    ));
                }
            }
            ShaderStage::Compute => {
                // Compute shaders typically have thread ID inputs
                // This is optional as they can use SV_DispatchThreadID semantic
            }
            ShaderStage::Vertex => {
                // Vertex shaders should have position input
                if shader.ast.inputs.is_empty() {
                    errors.push(format!(
                        "Shader '{}': Vertex shader should have input parameters (position, normal, etc.)",
                        shader_name
                    ));
                }
            }
            ShaderStage::Surface => {
                // Surface shaders should have UV input
                if shader.ast.inputs.is_empty() {
                    errors.push(format!(
                        "Shader '{}': Surface shader should have at least one input parameter (typically UV coordinates)",
                        shader_name
                    ));
                }
            }
        }
    }
    
    /// Validate binding slots and UE5 conventions
    /// Requirements:
    /// - No conflicts with UE5 reserved slots (b0 for View)
    /// - Proper slot allocation for resource types
    /// - No conflicts between different resource types using the same binding number
    /// - All slots are within UE5 limits for their resource type
    /// - Warning for non-standard binding patterns
    /// 
    /// Resource types and their binding ranges:
    /// - Textures (t-register): t0-t127
    /// - Samplers (s-register): s0-s15
    /// - UAVs (u-register): u0-u63
    /// - Constant buffers (b-register): b0-b13 (b0 reserved for View)
    fn validate_bindings(&self, shader: &TypedShader, errors: &mut Vec<String>) {
        let shader_name = &shader.ast.name;
        
        // Track bindings by resource type to detect conflicts
        let mut texture_bindings: HashMap<u32, String> = HashMap::new();
        let mut uav_bindings: HashMap<u32, String> = HashMap::new();
        let mut scalar_ordering: HashMap<u32, String> = HashMap::new();
        
        // Classify each uniform by resource type and check for conflicts
        for uniform in &shader.ast.uniforms {
            // Skip permutation uniforms - they're compile-time flags, not runtime parameters
            if self.is_permutation_uniform(&uniform.name) {
                continue;
            }
            
            let binding = uniform.binding;
            let uniform_name = &uniform.name;
            
            if let Type::Named { name, .. } = &uniform.ty {
                let type_name = name.as_str();
                
                // Use classify_uniform_type to determine resource class
                match classify_uniform_type(type_name) {
                    UniformClass::Scalar => {
                        // @N is an ordering index only, not a register binding
                        // No register limit validation needed - any value is valid for ordering
                        
                        // Check for ordering conflicts (same @N used by multiple scalar params)
                        if let Some(existing) = scalar_ordering.get(&binding) {
                            errors.push(format!(
                                "Shader '{}': Scalar parameter '{}' uses ordering index @{} which is already used by parameter '{}'. \
                                Each scalar parameter should have a unique ordering index.",
                                shader_name, uniform_name, binding, existing
                            ));
                        } else {
                            scalar_ordering.insert(binding, uniform_name.clone());
                        }
                        
                        // No register range validation for scalars - they're packed into SHADER_PARAMETER_STRUCT
                    }
                    UniformClass::Texture => {
                        // @N is a t-register binding (texture register, range 0-127)
                        
                        // Check for binding conflicts within texture register space
                        if let Some(existing) = texture_bindings.get(&binding) {
                            errors.push(format!(
                                "Shader '{}': Texture uniform '{}' uses binding @{} (t{}) which is already used by texture '{}'",
                                shader_name, uniform_name, binding, binding, existing
                            ));
                        } else {
                            texture_bindings.insert(binding, uniform_name.clone());
                        }
                        
                        // Validate texture binding range (D3D11 limit: t0-t127)
                        if binding > 127 {
                            errors.push(format!(
                                "Shader '{}': Texture uniform '{}' uses binding @{} which exceeds D3D11 texture register limit (t0-t127)",
                                shader_name, uniform_name, binding
                            ));
                        }
                    }
                    UniformClass::UAV => {
                        // @N is a u-register binding (UAV register, range 0-63)
                        
                        // Check for binding conflicts within UAV register space
                        if let Some(existing) = uav_bindings.get(&binding) {
                            errors.push(format!(
                                "Shader '{}': UAV uniform '{}' uses binding @{} (u{}) which is already used by UAV '{}'",
                                shader_name, uniform_name, binding, binding, existing
                            ));
                        } else {
                            uav_bindings.insert(binding, uniform_name.clone());
                        }
                        
                        // Validate UAV binding range (D3D11 limit: u0-u63)
                        if binding > 63 {
                            errors.push(format!(
                                "Shader '{}': UAV uniform '{}' uses binding @{} which exceeds D3D11 UAV register limit (u0-u63)",
                                shader_name, uniform_name, binding
                            ));
                        }
                    }
                }
            }
        }
        
        // Note: We no longer check for cross-resource-type conflicts between scalars and resources
        // because scalars use ordering indices (not register bindings), so there's no actual conflict.
        // Only check for conflicts between texture and UAV bindings if they somehow overlap
        // (which shouldn't happen in practice since they use different register spaces).
    }
}

impl Default for ShaderValidator {
    fn default() -> Self {
        Self::new()
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Shader, Param, Block, Uniform};
    use kain_core::span::Span;
    
    fn make_test_shader(name: &str, uniforms: Vec<Uniform>) -> TypedShader {
        TypedShader {
            ast: Shader {
                name: name.to_string(),
                stage: ShaderStage::Fragment,
                inputs: vec![Param {
                    name: "uv".to_string(),
                    ty: Type::Named {
                        name: "Vec2".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    mutable: false,
                    default: None,
                    span: Span::default(),
                }],
                outputs: Type::Named {
                    name: "Vec4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                uniforms,
                body: Block {
                    stmts: vec![],
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: vec![],
            output_type: kain_core::types::ResolvedType::Unit,
        }
    }
    
    #[test]
    fn test_validator_creation() {
        let validator = ShaderValidator::new();
        assert!(!validator.hlsl_keywords.is_empty(), "HLSL keywords should be loaded");
    }
    
    #[test]
    fn test_binding_conflict_detection() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "color1".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "color2".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Conflict!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect binding conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("already used")), "Should report binding conflict");
    }
    
    #[test]
    fn test_hlsl_keyword_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "if".to_string(), // HLSL keyword!
                ty: Type::Named {
                    name: "float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect HLSL keyword conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("reserved keyword")), "Should report keyword conflict");
    }
    
    #[test]
    fn test_cpp_keyword_conflict_in_shader_name() {
        let mut validator = ShaderValidator::new();
        
        // Create a shader with a C++ keyword as name
        let shader = TypedShader {
            ast: Shader {
                name: "class".to_string(), // C++ keyword!
                stage: ShaderStage::Fragment,
                inputs: vec![Param {
                    name: "uv".to_string(),
                    ty: Type::Named {
                        name: "Vec2".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    mutable: false,
                    default: None,
                    span: Span::default(),
                }],
                outputs: Type::Named {
                    name: "Vec4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                uniforms: vec![],
                body: Block {
                    stmts: vec![],
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: vec![],
            output_type: kain_core::types::ResolvedType::Unit,
        };
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect C++ keyword conflict in shader name");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.contains("class")), 
                "Should report C++ keyword conflict: {:?}", errors);
    }
    
    #[test]
    fn test_cpp_keyword_conflict_in_uniform_name() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "template".to_string(), // C++ keyword!
                ty: Type::Named {
                    name: "float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect C++ keyword conflict in uniform name");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.contains("template")), 
                "Should report C++ keyword conflict: {:?}", errors);
    }
    
    #[test]
    fn test_cpp_keyword_conflict_in_input_name() {
        let mut validator = ShaderValidator::new();
        
        let shader = TypedShader {
            ast: Shader {
                name: "TestShader".to_string(),
                stage: ShaderStage::Fragment,
                inputs: vec![Param {
                    name: "namespace".to_string(), // C++ keyword!
                    ty: Type::Named {
                        name: "Vec2".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    mutable: false,
                    default: None,
                    span: Span::default(),
                }],
                outputs: Type::Named {
                    name: "Vec4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                uniforms: vec![],
                body: Block {
                    stmts: vec![],
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: vec![],
            output_type: kain_core::types::ResolvedType::Unit,
        };
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect C++ keyword conflict in input parameter name");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.contains("namespace")), 
                "Should report C++ keyword conflict: {:?}", errors);
    }
    
    #[test]
    fn test_multiple_cpp_keyword_conflicts() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "class".to_string(), // C++ keyword!
                ty: Type::Named {
                    name: "float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "namespace".to_string(), // C++ keyword!
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect multiple C++ keyword conflicts");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.contains("class")), 
                "Should report 'class' conflict: {:?}", errors);
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.contains("namespace")), 
                "Should report 'namespace' conflict: {:?}", errors);
    }
    
    #[test]
    fn test_ue5_macro_name_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "UPROPERTY".to_string(), // UE5 macro name!
                ty: Type::Named {
                    name: "float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1, // Use binding 1 to avoid the b0 reserved check
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect UE5 macro name conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("C++ keyword") && e.to_lowercase().contains("uproperty")), 
                "Should report UE5 macro conflict: {:?}", errors);
    }
    
    #[test]
    fn test_valid_names_pass_cpp_check() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("MyCustomShader", vec![
            Uniform {
                name: "my_color".to_string(), // Valid name
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "texture_map".to_string(), // Valid name
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        // Should pass or only have non-C++-keyword-related errors
        if let Err(errors) = result {
            assert!(!errors.iter().any(|e| e.contains("C++ keyword")), 
                    "Should not report C++ keyword conflicts for valid names: {:?}", errors);
        }
    }
    
    #[test]
    fn test_function_signature_validation_return_type() {
        let mut validator = ShaderValidator::new();
        
        // Test with invalid return type
        let shader = TypedShader {
            ast: Shader {
                name: "TestShader".to_string(),
                stage: ShaderStage::Fragment,
                inputs: vec![Param {
                    name: "uv".to_string(),
                    ty: Type::Named {
                        name: "Vec2".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    mutable: false,
                    default: None,
                    span: Span::default(),
                }],
                outputs: Type::Named {
                    name: "String".to_string(), // Invalid HLSL return type!
                    generics: vec![],
                    span: Span::default(),
                },
                uniforms: vec![],
                body: Block {
                    stmts: vec![],
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: vec![],
            output_type: kain_core::types::ResolvedType::Unit,
        };
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect invalid return type");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("return type") && e.contains("String")), 
                "Should report invalid return type: {:?}", errors);
    }
    
    #[test]
    fn test_function_signature_validation_parameter_type() {
        let mut validator = ShaderValidator::new();
        
        // Test with invalid parameter type
        let shader = TypedShader {
            ast: Shader {
                name: "TestShader".to_string(),
                stage: ShaderStage::Fragment,
                inputs: vec![Param {
                    name: "data".to_string(),
                    ty: Type::Array(
                        Box::new(Type::Named {
                            name: "float".to_string(),
                            generics: vec![],
                            span: Span::default(),
                        }),
                        10,
                        Span::default(),
                    ), // Array type in function signature!
                    mutable: false,
                    default: None,
                    span: Span::default(),
                }],
                outputs: Type::Named {
                    name: "Vec4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                uniforms: vec![],
                body: Block {
                    stmts: vec![],
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: vec![],
            output_type: kain_core::types::ResolvedType::Unit,
        };
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect invalid parameter type");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("input parameter") && e.contains("array")), 
                "Should report invalid parameter type: {:?}", errors);
    }
    
    #[test]
    fn test_valid_shader_passes() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "base_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 2,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), "Valid shader should pass validation");
    }
    
    #[test]
    fn test_permutation_naming_valid_cfg() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "CFG_HIGH_QUALITY".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), "Valid CFG_* permutation should pass: {:?}", result);
    }
    
    #[test]
    fn test_permutation_naming_valid_enable() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "ENABLE_SHADOWS".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), "Valid ENABLE_* permutation should pass: {:?}", result);
    }
    
    #[test]
    fn test_permutation_naming_invalid_type() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "CFG_HIGH_QUALITY".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(), // Wrong type - should be Float
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Permutation with wrong type should fail");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("should have Float type")), 
                "Should report type error for permutation: {:?}", errors);
    }
    
    #[test]
    fn test_permutation_naming_invalid_case() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "CFG_HighQuality".to_string(), // Mixed case - should be all uppercase
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Permutation with mixed case should fail");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("should be all uppercase")), 
                "Should report case error for permutation: {:?}", errors);
    }
    
    #[test]
    fn test_permutation_naming_suggestion() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "enable_feature".to_string(), // Looks like a config flag but wrong naming
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Float uniform with config-like name should get suggestion");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("Consider renaming to CFG_") || e.contains("Consider renaming to ENABLE_")), 
                "Should suggest permutation naming: {:?}", errors);
    }
    
    #[test]
    fn test_permutation_naming_multiple_permutations() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "CFG_HIGH_QUALITY".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "ENABLE_SHADOWS".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
            Uniform {
                name: "CFG_MOBILE".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 2,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), "Multiple valid permutations should pass: {:?}", result);
    }
    
    // ═══════════════════════════════════════════════════════════════════
    // POD Struct Validation Tests
    // ═══════════════════════════════════════════════════════════════════
    
    use kain_core::types::{TypedProgram, TypedItem};
    use kain_core::ast::{Struct, Field, Visibility};
    
    fn make_test_program_with_struct(struct_name: &str, fields: Vec<Field>) -> TypedProgram {
        TypedProgram {
            items: vec![
                TypedItem::Struct(kain_core::types::TypedStruct {
                    ast: Struct {
                        name: struct_name.to_string(),
                        generics: vec![],
                        fields,
                        methods: vec![],
                        attributes: vec![],
                        visibility: Visibility::Public,
                        span: Span::default(),
                    },
                    field_types: std::collections::HashMap::new(),
                }),
            ],
        }
    }
    
    fn make_field(name: &str, type_name: &str) -> Field {
        Field {
            name: name.to_string(),
            ty: Type::Named {
                name: type_name.to_string(),
                generics: vec![],
                span: Span::default(),
            },
            attributes: vec![],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: Span::default(),
        }
    }
    
    #[test]
    fn test_uniform_user_struct_type_is_accepted() {
        let mut validator = ShaderValidator::new();

        // Use a 16-byte aligned struct so the test isolates uniform type acceptance.
        let program = make_test_program_with_struct("GpuParams", vec![
            make_field("albedo", "Vec4"),
        ]);

        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "GpuParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);

        let result = validator.validate_shader(&shader, Some(&program));
        if let Err(errors) = result {
            assert!(
                !errors
                    .iter()
                    .any(|e| e.contains("Uniform 'params' has potentially invalid HLSL type 'GpuParams'")),
                "User struct uniform should not be rejected as invalid type: {:?}",
                errors
            );
        }
    }

    #[test]
    fn test_pod_struct_valid_hlsl_types() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with a valid POD struct
        let program = make_test_program_with_struct("ShaderParams", vec![
            make_field("color", "Vec3"),
            make_field("intensity", "Float"),
        ]);
        
        // Create a shader that references this struct
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "ShaderParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program));
        // Note: This will fail due to alignment, but that's expected
        // We're testing that HLSL-compatible types are recognized
        if let Err(errors) = result {
            // Should only have alignment error, not type errors
            assert!(!errors.iter().any(|e| e.contains("non-HLSL-compatible type")), 
                    "Should not report type errors for valid HLSL types: {:?}", errors);
        }
    }
    
    #[test]
    fn test_pod_struct_invalid_string_type() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with an invalid POD struct (contains String)
        let program = make_test_program_with_struct("InvalidParams", vec![
            make_field("name", "String"),
            make_field("value", "Float"),
        ]);
        
        // Create a shader that references this struct
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "InvalidParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program));
        assert!(result.is_err(), "Should detect String type in POD struct");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("String") && e.contains("not supported")), 
                "Should report String type error: {:?}", errors);
    }
    
    #[test]
    fn test_pod_struct_alignment_check() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with a struct that's not 16-byte aligned
        // Vec3 (12 bytes) + Float (4 bytes) = 16 bytes (aligned)
        let program_aligned = make_test_program_with_struct("AlignedParams", vec![
            make_field("color", "Vec3"),
            make_field("intensity", "Float"),
        ]);
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "AlignedParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program_aligned));
        // Should pass alignment check
        if let Err(errors) = &result {
            assert!(!errors.iter().any(|e| e.contains("not 16-byte aligned")), 
                    "Should not report alignment error for 16-byte aligned struct: {:?}", errors);
        }
        
        // Now test with unaligned struct
        // Vec3 (12 bytes) only = not aligned
        let program_unaligned = make_test_program_with_struct("UnalignedParams", vec![
            make_field("color", "Vec3"),
        ]);
        
        let shader2 = make_test_shader("TestShader2", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "UnalignedParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result2 = validator.validate_shader(&shader2, Some(&program_unaligned));
        assert!(result2.is_err(), "Should detect alignment issue");
        
        let errors = result2.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("not 16-byte aligned")), 
                "Should report alignment error: {:?}", errors);
    }
    
    #[test]
    fn test_pod_struct_array_type() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with a struct containing an array
        let program = make_test_program_with_struct("ArrayParams", vec![
            Field {
                name: "values".to_string(),
                ty: Type::Array(
                    Box::new(Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: Span::default(),
                    }),
                    10, // Array size
                    Span::default(),
                ),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: Span::default(),
            },
        ]);
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "ArrayParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program));
        assert!(result.is_err(), "Should detect array type in POD struct");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("array type")), 
                "Should report array type error: {:?}", errors);
    }
    
    #[test]
    fn test_pod_struct_redefinition_warning() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with a struct that has a common auto-generated name
        let program = make_test_program_with_struct("ShaderParams", vec![
            make_field("color", "Vec3"),
            make_field("intensity", "Float"),
        ]);
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "ShaderParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program));
        assert!(result.is_err(), "Should warn about potential redefinition");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("may conflict") || e.contains("redefinition")), 
                "Should warn about redefinition: {:?}", errors);
    }
    
    #[test]
    fn test_pod_struct_no_program_provided() {
        let mut validator = ShaderValidator::new();
        
        // Create a shader without providing a program
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        // Should not crash when program is None
        let result = validator.validate_shader(&shader, None);
        // Should pass since there are no POD struct validation errors
        assert!(result.is_ok(), "Should handle None program gracefully: {:?}", result);
    }
    
    #[test]
    fn test_pod_struct_multiple_fields() {
        let mut validator = ShaderValidator::new();
        
        // Create a program with a struct with multiple valid fields
        let program = make_test_program_with_struct("ComplexParams", vec![
            make_field("position", "Vec3"),
            make_field("normal", "Vec3"),
            make_field("uv", "Vec2"),
            make_field("color", "Vec4"),
            make_field("metallic", "Float"),
            make_field("roughness", "Float"),
        ]);
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "params".to_string(),
                ty: Type::Named {
                    name: "ComplexParams".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, Some(&program));
        // Check that all fields are validated
        if let Err(errors) = result {
            // Should not have type compatibility errors for valid HLSL types
            assert!(!errors.iter().any(|e| e.contains("non-HLSL-compatible type")), 
                    "Should not report type errors for valid HLSL types: {:?}", errors);
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════
    // Binding Conflict Detection Tests
    // ═══════════════════════════════════════════════════════════════════
    
    #[test]
    fn test_binding_texture_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "normal_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Conflict with albedo_map!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect texture binding conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("t0") && e.contains("already used")), 
                "Should report texture binding conflict: {:?}", errors);
    }
    
    #[test]
    fn test_binding_sampler_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "sampler1".to_string(),
                ty: Type::Named {
                    name: "SamplerState".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "sampler2".to_string(),
                ty: Type::Named {
                    name: "SamplerState".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Conflict with sampler1!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect sampler binding conflict");
        
        let errors = result.unwrap_err();
        // Samplers are classified as Texture type (t-register) in the new classification system
        assert!(errors.iter().any(|e| e.contains("t0") && e.contains("already used")), 
                "Should report sampler binding conflict (samplers use t-register): {:?}", errors);
    }
    
    #[test]
    fn test_binding_uav_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "output1".to_string(),
                ty: Type::Named {
                    name: "RWTexture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "output2".to_string(),
                ty: Type::Named {
                    name: "RWBuffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Conflict with output1!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect UAV binding conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("u0") && e.contains("already used")), 
                "Should report UAV binding conflict: {:?}", errors);
    }
    
    #[test]
    fn test_binding_cbuffer_conflict() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "color1".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
            Uniform {
                name: "color2".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1, // Conflict with color1!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect scalar ordering index conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("ordering index @1") && e.contains("already used")), 
                "Should report scalar ordering index conflict: {:?}", errors);
    }
    
    #[test]
    fn test_binding_cross_resource_type_no_warning() {
        let mut validator = ShaderValidator::new();
        
        // Use the same binding number for texture and scalar
        // This is valid because scalars use ordering indices (not register bindings)
        // and textures use t-registers, so there's no actual conflict
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0
                span: Span::default(),
            },
            Uniform {
                name: "base_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // ordering index 0 - no conflict with t0
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), 
                "Should NOT warn about scalar ordering index vs texture register - different semantics: {:?}", 
                result.err());
    }
    
    #[test]
    fn test_binding_texture_exceeds_limit() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "texture_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 128, // Exceeds t127 limit!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect texture binding exceeds limit");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("exceeds D3D11 texture register limit")), 
                "Should report texture binding limit exceeded: {:?}", errors);
    }
    
    #[test]
    fn test_binding_sampler_exceeds_limit() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "sampler_state".to_string(),
                ty: Type::Named {
                    name: "SamplerState".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 128, // Exceeds t127 limit (samplers use t-register)
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect sampler binding exceeds limit");
        
        let errors = result.unwrap_err();
        // Samplers are classified as Texture type, so they use t-register limit (0-127)
        assert!(errors.iter().any(|e| e.contains("exceeds D3D11 texture register limit")), 
                "Should report sampler binding limit exceeded (samplers use t-register): {:?}", errors);
    }
    
    #[test]
    fn test_binding_uav_exceeds_limit() {
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "output_buffer".to_string(),
                ty: Type::Named {
                    name: "RWBuffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 64, // Exceeds u63 limit!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect UAV binding exceeds limit");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("exceeds D3D11 UAV register limit")), 
                "Should report UAV binding limit exceeded: {:?}", errors);
    }
    
    #[test]
    fn test_binding_cbuffer_exceeds_limit() {
        let mut validator = ShaderValidator::new();
        
        // NOTE: In KAIN, scalar/vector uniforms are packed into a SHADER_PARAMETER_STRUCT,
        // not declared as explicit cbuffer registers. The @N annotation is a KAIN-internal
        // ordering index only, so no b-register conflict or limit applies to scalar params.
        // This test verifies that scalar uniforms with high binding numbers are accepted.
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 14, // High binding number is valid for scalars
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), 
                "Scalar uniforms should accept high binding numbers (used as ordering index, not b-register): {:?}", 
                result.err());
    }
    
    #[test]
    fn test_binding_cbuffer_b0_not_reserved_for_shader_params() {
        let mut validator = ShaderValidator::new();

        // KAIN uses SHADER_PARAMETER_STRUCT (not explicit cbuffer b0) so @0 is valid.
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "my_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // @0 is fine — it's a struct member index, not b0 register
                span: Span::default(),
            },
        ]);

        let result = validator.validate_shader(&shader, None);
        // Should NOT error on @0 for scalar/vector params in SHADER_PARAMETER_STRUCT
        if let Err(errors) = result {
            assert!(!errors.iter().any(|e| e.contains("b0") && e.contains("reserved for UE5 View")),
                    "Should NOT report b0 reserved error for SHADER_PARAMETER_STRUCT members: {:?}", errors);
        }
    }
    
    #[test]
    fn test_binding_valid_separate_resource_types() {
        let mut validator = ShaderValidator::new();
        
        // Use different binding numbers for different resource types - should pass
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0
                span: Span::default(),
            },
            Uniform {
                name: "normal_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1, // t1
                span: Span::default(),
            },
            Uniform {
                name: "sampler_state".to_string(),
                ty: Type::Named {
                    name: "SamplerState".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 2, // t2 - samplers use t-register in new classification
                span: Span::default(),
            },
            Uniform {
                name: "base_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // ordering index 0 - scalars use ordering indices, not register bindings
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), "Valid separate bindings should pass: {:?}", result);
    }
    
    #[test]
    fn test_binding_buffer_types() {
        let mut validator = ShaderValidator::new();
        
        // Test different buffer types use texture register space
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "buffer1".to_string(),
                ty: Type::Named {
                    name: "Buffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0
                span: Span::default(),
            },
            Uniform {
                name: "buffer2".to_string(),
                ty: Type::Named {
                    name: "StructuredBuffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0 - conflict!
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect buffer binding conflict");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("t0") && e.contains("already used")), 
                "Should report buffer binding conflict: {:?}", errors);
    }
    
    #[test]
    fn test_binding_permutation_uniforms_ignored() {
        let mut validator = ShaderValidator::new();
        
        // Permutation uniforms should be ignored in binding validation
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "CFG_HIGH_QUALITY".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Permutation uniform - ignored in validation
                span: Span::default(),
            },
            Uniform {
                name: "ENABLE_SHADOWS".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Same binding as CFG_HIGH_QUALITY, but both are permutations
                span: Span::default(),
            },
            Uniform {
                name: "base_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // ordering index 0 - regular scalar uniform
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        // Should not report conflicts for permutation uniforms (they're ignored)
        // Regular scalar uniforms use ordering indices, not register bindings
        assert!(result.is_ok(), 
                "Should not report conflicts for permutation uniforms or scalar ordering indices: {:?}", 
                result);
    }
    
    #[test]
    fn test_binding_multiple_conflicts() {
        let mut validator = ShaderValidator::new();
        
        // Test multiple types of conflicts in one shader:
        // - Texture binding conflict (two textures on t0)
        // - Sampler binding out of range (s20 > s15)
        // NOTE: Scalar uniforms (Vec3) use SHADER_PARAMETER_STRUCT, NOT explicit b-registers,
        // so b0 reservation does not apply to them.
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "texture1".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0
                span: Span::default(),
            },
            Uniform {
                name: "texture2".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0 - conflict!
                span: Span::default(),
            },
            Uniform {
                name: "sampler1".to_string(),
                ty: Type::Named {
                    name: "SamplerState".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 20, // s20 - exceeds limit!
                span: Span::default(),
            },
            Uniform {
                name: "color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // Valid for scalars - they use SHADER_PARAMETER_STRUCT
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should detect multiple binding errors");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("t0") && e.contains("already used")), 
                "Should report texture conflict: {:?}", errors);
        assert!(errors.iter().any(|e| e.contains("exceeds maximum sampler slot")), 
                "Should report sampler limit: {:?}", errors);
        // No b0 reservation check - scalars don't use explicit b-registers
    }
    
    // ═══════════════════════════════════════════════════════════════════
    // TYPE_MAPPER Integration Tests
    // ═══════════════════════════════════════════════════════════════════
    
    #[test]
    fn test_type_mapper_kain_scalar_types_accepted() {
        let mut validator = ShaderValidator::new();
        
        // Test that all KAIN scalar types from TYPE_MAPPER are accepted
        let kain_scalars = vec!["Float", "Int", "UInt", "Bool"];
        
        for scalar_type in kain_scalars {
            let shader = make_test_shader("TestShader", vec![
                Uniform {
                    name: "test_uniform".to_string(),
                    ty: Type::Named {
                        name: scalar_type.to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    binding: 0,
                    span: Span::default(),
                },
            ]);
            
            let result = validator.validate_shader(&shader, None);
            assert!(result.is_ok(), 
                    "TYPE_MAPPER type '{}' should be accepted but got errors: {:?}", 
                    scalar_type, result.err());
        }
    }
    
    #[test]
    fn test_type_mapper_kain_vector_types_accepted() {
        let mut validator = ShaderValidator::new();
        
        // Test that all KAIN vector types from TYPE_MAPPER are accepted
        let kain_vectors = vec![
            "Vec2", "Vec3", "Vec4",
            "IVec2", "IVec3", "IVec4",
            "UVec2", "UVec3", "UVec4",
        ];
        
        for vector_type in kain_vectors {
            let shader = make_test_shader("TestShader", vec![
                Uniform {
                    name: "test_uniform".to_string(),
                    ty: Type::Named {
                        name: vector_type.to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    binding: 0,
                    span: Span::default(),
                },
            ]);
            
            let result = validator.validate_shader(&shader, None);
            assert!(result.is_ok(), 
                    "TYPE_MAPPER type '{}' should be accepted but got errors: {:?}", 
                    vector_type, result.err());
        }
    }
    
    #[test]
    fn test_type_mapper_kain_matrix_types_accepted() {
        let mut validator = ShaderValidator::new();
        
        // Test that all KAIN matrix types from TYPE_MAPPER are accepted
        let kain_matrices = vec!["Mat2", "Mat3", "Mat4"];
        
        for matrix_type in kain_matrices {
            let shader = make_test_shader("TestShader", vec![
                Uniform {
                    name: "test_uniform".to_string(),
                    ty: Type::Named {
                        name: matrix_type.to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    binding: 0,
                    span: Span::default(),
                },
            ]);
            
            let result = validator.validate_shader(&shader, None);
            assert!(result.is_ok(), 
                    "TYPE_MAPPER type '{}' should be accepted but got errors: {:?}", 
                    matrix_type, result.err());
        }
    }
    
    #[test]
    fn test_type_mapper_kain_texture_types_accepted() {
        let mut validator = ShaderValidator::new();
        
        // Test that all KAIN texture types from TYPE_MAPPER are accepted
        let kain_textures = vec![
            "Sampler2D", "Sampler3D", "SamplerCube",
            "RWBuffer", "RWTexture2D", "RWTexture3D",
        ];
        
        for texture_type in kain_textures {
            let shader = make_test_shader("TestShader", vec![
                Uniform {
                    name: "test_uniform".to_string(),
                    ty: Type::Named {
                        name: texture_type.to_string(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    binding: 0,
                    span: Span::default(),
                },
            ]);
            
            let result = validator.validate_shader(&shader, None);
            assert!(result.is_ok(), 
                    "TYPE_MAPPER type '{}' should be accepted but got errors: {:?}", 
                    texture_type, result.err());
        }
    }
    
    #[test]
    fn test_invalid_type_error_lists_valid_types() {
        let mut validator = ShaderValidator::new();
        
        // Test that error message for invalid type lists valid KAIN types from TYPE_MAPPER
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "invalid_uniform".to_string(),
                ty: Type::Named {
                    name: "InvalidType".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_err(), "Should reject invalid type");
        
        let errors = result.unwrap_err();
        let error_msg = errors.join(" ");
        
        // Error message should list valid KAIN types from TYPE_MAPPER
        assert!(error_msg.contains("Valid KAIN types:"), 
                "Error should list valid types: {}", error_msg);
        assert!(error_msg.contains("Float"), 
                "Error should list Float: {}", error_msg);
        assert!(error_msg.contains("Vec3"), 
                "Error should list Vec3: {}", error_msg);
        assert!(error_msg.contains("UVec2"), 
                "Error should list UVec2: {}", error_msg);
        assert!(error_msg.contains("Mat4"), 
                "Error should list Mat4: {}", error_msg);
    }
    
    #[test]
    fn test_type_mapper_synchronization_with_codegen() {
        // This test verifies that validator accepts all types that TYPE_MAPPER can map
        // This ensures validator-codegen synchronization (Requirement 22.4, 22.5)
        let mut validator = ShaderValidator::new();
        
        // Get all valid types from TYPE_MAPPER
        let valid_types = crate::type_mapping::TYPE_MAPPER.valid_types();
        
        // Verify validator accepts all of them
        for type_name in valid_types {
            let shader = make_test_shader("TestShader", vec![
                Uniform {
                    name: "test_uniform".to_string(),
                    ty: Type::Named {
                        name: type_name.clone(),
                        generics: vec![],
                        span: Span::default(),
                    },
                    binding: 0,
                    span: Span::default(),
                },
            ]);
            
            let result = validator.validate_shader(&shader, None);
            assert!(result.is_ok(), 
                    "Validator should accept TYPE_MAPPER type '{}' but got errors: {:?}", 
                    type_name, result.err());
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════
    // Uniform Classification Tests (Task 5.1)
    // ═══════════════════════════════════════════════════════════════════
    
    #[test]
    fn test_classify_uniform_scalar_types() {
        // Test primitive scalar types
        assert_eq!(classify_uniform_type("Float"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Int"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("UInt"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Bool"), UniformClass::Scalar);
        
        // Test lowercase HLSL variants
        assert_eq!(classify_uniform_type("float"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("int"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("uint"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("bool"), UniformClass::Scalar);
    }
    
    #[test]
    fn test_classify_uniform_vector_types() {
        // Test KAIN vector types
        assert_eq!(classify_uniform_type("Vec2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Vec3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Vec4"), UniformClass::Scalar);
        
        // Test integer vector types
        assert_eq!(classify_uniform_type("IVec2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("IVec3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("IVec4"), UniformClass::Scalar);
        
        // Test unsigned integer vector types
        assert_eq!(classify_uniform_type("UVec2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("UVec3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("UVec4"), UniformClass::Scalar);
        
        // Test lowercase HLSL vector types
        assert_eq!(classify_uniform_type("float2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("int2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("int3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("int4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("uint2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("uint3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("uint4"), UniformClass::Scalar);
    }
    
    #[test]
    fn test_classify_uniform_matrix_types() {
        // Test KAIN matrix types
        assert_eq!(classify_uniform_type("Mat2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Mat3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Mat4"), UniformClass::Scalar);
        
        // Test HLSL matrix types
        assert_eq!(classify_uniform_type("float2x2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float3x3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float4x4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float3x4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("float4x3"), UniformClass::Scalar);
    }
    
    #[test]
    fn test_classify_uniform_texture_types() {
        // Test Texture types (use t-register)
        assert_eq!(classify_uniform_type("Texture1D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Texture2D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Texture3D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("TextureCube"), UniformClass::Texture);
        
        // Test Texture array types
        assert_eq!(classify_uniform_type("Texture1DArray"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Texture2DArray"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("TextureCubeArray"), UniformClass::Texture);
        
        // Test multisampled texture types
        assert_eq!(classify_uniform_type("Texture2DMS"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Texture2DMSArray"), UniformClass::Texture);
    }
    
    #[test]
    fn test_classify_uniform_sampler_types() {
        // Test Sampler types (use t-register)
        assert_eq!(classify_uniform_type("Sampler"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("SamplerState"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("SamplerComparisonState"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Sampler1D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Sampler2D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Sampler3D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("SamplerCube"), UniformClass::Texture);
    }
    
    #[test]
    fn test_classify_uniform_buffer_types() {
        // Test read-only buffer types (use t-register)
        assert_eq!(classify_uniform_type("Buffer"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("StructuredBuffer"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("ByteAddressBuffer"), UniformClass::Texture);
    }
    
    #[test]
    fn test_classify_uniform_uav_types() {
        // Test RW buffer types (use u-register)
        assert_eq!(classify_uniform_type("RWBuffer"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWStructuredBuffer"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWByteAddressBuffer"), UniformClass::UAV);
        
        // Test RW texture types
        assert_eq!(classify_uniform_type("RWTexture1D"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture3D"), UniformClass::UAV);
        
        // Test RW texture array types
        assert_eq!(classify_uniform_type("RWTexture1DArray"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2DArray"), UniformClass::UAV);
        
        // Test typed RW texture types
        assert_eq!(classify_uniform_type("RWTexture2D_Float"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_Float2"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_Float3"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_Float4"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_Int"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_UInt"), UniformClass::UAV);
    }
    
    #[test]
    fn test_classify_uniform_user_struct_types() {
        // User-defined structs should be classified as Scalar
        assert_eq!(classify_uniform_type("MyCustomStruct"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("ShaderParams"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("MaterialData"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("LightingParams"), UniformClass::Scalar);
    }
    
    #[test]
    fn test_classify_uniform_edge_cases() {
        // Test edge cases and potential confusion
        
        // Types that contain "Texture" but aren't texture types
        assert_eq!(classify_uniform_type("TextureData"), UniformClass::Texture); // Still classified as Texture due to prefix
        
        // Types that contain "Sampler" but aren't sampler types
        assert_eq!(classify_uniform_type("SamplerConfig"), UniformClass::Texture); // Still classified as Texture due to substring
        
        // Types that start with "RW" but aren't UAVs (hypothetical)
        assert_eq!(classify_uniform_type("RWConfig"), UniformClass::UAV); // Still classified as UAV due to prefix
        
        // Empty string (edge case)
        assert_eq!(classify_uniform_type(""), UniformClass::Scalar);
    }
    
    #[test]
    fn test_uniform_classification_semantics_scalar() {
        // Verify that scalar uniforms with high binding numbers are accepted
        // @N is an ordering index for scalars, not a b-register binding
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "param0".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "param14".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 14, // High binding number - valid for scalars
                span: Span::default(),
            },
            Uniform {
                name: "param29".to_string(),
                ty: Type::Named {
                    name: "Mat4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 29, // Very high binding number - still valid for scalars
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        assert!(result.is_ok(), 
                "Scalar uniforms should accept any binding number (ordering index): {:?}", 
                result.err());
    }
    
    #[test]
    fn test_uniform_classification_semantics_texture() {
        // Verify that texture uniforms respect t-register limits (0-127)
        let mut validator = ShaderValidator::new();
        
        // Valid texture binding
        let shader_valid = make_test_shader("TestShader", vec![
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 127, // Maximum valid t-register
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader_valid, None);
        assert!(result.is_ok(), 
                "Texture uniform with binding 127 should be valid: {:?}", 
                result.err());
        
        // Invalid texture binding (exceeds limit)
        let shader_invalid = make_test_shader("TestShader", vec![
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 128, // Exceeds t127 limit
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader_invalid, None);
        assert!(result.is_err(), "Texture uniform with binding 128 should be invalid");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("exceeds maximum texture slot t127")), 
                "Should report texture binding limit exceeded: {:?}", errors);
    }
    
    #[test]
    fn test_uniform_classification_semantics_uav() {
        // Verify that UAV uniforms respect u-register limits (0-63)
        let mut validator = ShaderValidator::new();
        
        // Valid UAV binding
        let shader_valid = make_test_shader("TestShader", vec![
            Uniform {
                name: "output_buffer".to_string(),
                ty: Type::Named {
                    name: "RWBuffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 63, // Maximum valid u-register
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader_valid, None);
        assert!(result.is_ok(), 
                "UAV uniform with binding 63 should be valid: {:?}", 
                result.err());
        
        // Invalid UAV binding (exceeds limit)
        let shader_invalid = make_test_shader("TestShader", vec![
            Uniform {
                name: "output_buffer".to_string(),
                ty: Type::Named {
                    name: "RWTexture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 64, // Exceeds u63 limit
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader_invalid, None);
        assert!(result.is_err(), "UAV uniform with binding 64 should be invalid");
        
        let errors = result.unwrap_err();
        assert!(errors.iter().any(|e| e.contains("exceeds maximum UAV slot u63")), 
                "Should report UAV binding limit exceeded: {:?}", errors);
    }
    
    #[test]
    fn test_uniform_classification_mixed_types() {
        // Test shader with mixed uniform types (scalar, texture, UAV)
        let mut validator = ShaderValidator::new();
        
        let shader = make_test_shader("TestShader", vec![
            // Scalar uniforms (ordering indices)
            Uniform {
                name: "base_color".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "roughness".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1,
                span: Span::default(),
            },
            // Texture uniforms (t-register bindings)
            Uniform {
                name: "albedo_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // t0
                span: Span::default(),
            },
            Uniform {
                name: "normal_map".to_string(),
                ty: Type::Named {
                    name: "Texture2D".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 1, // t1
                span: Span::default(),
            },
            // UAV uniforms (u-register bindings)
            Uniform {
                name: "output_buffer".to_string(),
                ty: Type::Named {
                    name: "RWBuffer".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0, // u0
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        // Note: This will generate warnings about cross-resource-type binding reuse (binding 0 and 1)
        // but should not fail validation since they're in different register spaces
        if let Err(errors) = &result {
            // Should only have warnings about cross-resource-type binding reuse, not hard errors
            assert!(errors.iter().all(|e| e.contains("multiple resource types") || e.contains("can be confusing")), 
                    "Should only have cross-resource-type warnings: {:?}", errors);
        }
    }
    
    #[test]
    fn test_uniform_classification_comprehensive() {
        // Comprehensive test covering all classification categories
        
        // Scalar types
        assert_eq!(classify_uniform_type("Float"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Vec3"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("Mat4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("UVec2"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("IVec4"), UniformClass::Scalar);
        assert_eq!(classify_uniform_type("CustomStruct"), UniformClass::Scalar);
        
        // Texture types
        assert_eq!(classify_uniform_type("Texture2D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Texture3D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("TextureCube"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Sampler2D"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("SamplerState"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("Buffer"), UniformClass::Texture);
        assert_eq!(classify_uniform_type("StructuredBuffer"), UniformClass::Texture);
        
        // UAV types
        assert_eq!(classify_uniform_type("RWBuffer"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture3D"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWStructuredBuffer"), UniformClass::UAV);
        assert_eq!(classify_uniform_type("RWTexture2D_Float"), UniformClass::UAV);
    }

    #[test]
    fn test_shader_with_30_plus_scalar_params() {
        // Test that shaders with 30+ scalar parameters are accepted
        // This validates Requirement 8.4: shaders with 30+ scalar parameters with @N ordering
        // should NOT be rejected by the validator
        let mut validator = ShaderValidator::new();
        
        // Create a shader with 32 scalar parameters (simulating a PBR material shader)
        let mut uniforms = vec![];
        
        // Add 32 scalar parameters with various types
        for i in 0..32 {
            let param_name = format!("param_{}", i);
            let param_type = match i % 4 {
                0 => "Float",
                1 => "Vec2",
                2 => "Vec3",
                3 => "Vec4",
                _ => "Float",
            };
            
            uniforms.push(Uniform {
                name: param_name,
                ty: Type::Named {
                    name: param_type.to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: i, // @N ordering index
                span: Span::default(),
            });
        }
        
        // Add a couple of texture parameters (should use t-register bindings)
        uniforms.push(Uniform {
            name: "albedo_map".to_string(),
            ty: Type::Named {
                name: "Texture2D".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            binding: 0, // t0
            span: Span::default(),
        });
        
        uniforms.push(Uniform {
            name: "normal_map".to_string(),
            ty: Type::Named {
                name: "Texture2D".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            binding: 1, // t1
            span: Span::default(),
        });
        
        // Add a UAV parameter
        uniforms.push(Uniform {
            name: "output_buffer".to_string(),
            ty: Type::Named {
                name: "RWTexture2D".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            binding: 0, // u0
            span: Span::default(),
        });
        
        let shader = make_test_shader("PBRMaterialShader", uniforms);
        
        let result = validator.validate_shader(&shader, None);
        
        // The shader should be valid - scalar params can have any @N ordering index
        // Only textures and UAVs have register binding limits
        assert!(result.is_ok(), 
                "Shader with 32 scalar parameters should be valid (Requirement 8.4): {:?}", 
                result.err());
    }
    
    #[test]
    fn test_shader_with_many_scalar_params_no_binding_limit() {
        // Test that scalar parameters do NOT have a binding > 13 limit
        // This validates Requirement 8.11: USF_Validator SHALL NOT conflate @N ordering 
        // with D3D11 b-register indices for scalar parameters
        let mut validator = ShaderValidator::new();
        
        // Create a shader with scalar parameters using high binding numbers
        let shader = make_test_shader("TestShader", vec![
            Uniform {
                name: "param_0".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 0,
                span: Span::default(),
            },
            Uniform {
                name: "param_13".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 13, // At the old incorrect limit
                span: Span::default(),
            },
            Uniform {
                name: "param_14".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 14, // Beyond the old incorrect limit - should be valid
                span: Span::default(),
            },
            Uniform {
                name: "param_50".to_string(),
                ty: Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 50, // Very high binding number - should be valid
                span: Span::default(),
            },
            Uniform {
                name: "param_100".to_string(),
                ty: Type::Named {
                    name: "Mat4".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                binding: 100, // Extremely high binding number - should be valid
                span: Span::default(),
            },
        ]);
        
        let result = validator.validate_shader(&shader, None);
        
        assert!(result.is_ok(), 
                "Scalar parameters should NOT have binding > 13 limit (Requirement 8.11): {:?}", 
                result.err());
    }
}
