use std::collections::HashMap;
use once_cell::sync::Lazy;

/// TypeMapper provides a single source of truth for KAIN→HLSL type mappings.
/// This ensures the validator and codegen use the same type definitions,
/// eliminating false-positive validation errors and maintenance burden.
///
/// # Design Rationale
/// Previously, the validator maintained a hardcoded allowlist separate from
/// the codegen type mapping table. When new KAIN type aliases were added to
/// codegen (UVec2, UInt, Mat4, IVec2), the validator would reject them as
/// invalid even though codegen correctly mapped them to HLSL types.
///
/// With TypeMapper, adding a new type requires only one entry in the mappings
/// HashMap, and both validator and codegen automatically use it.
pub struct TypeMapper {
    mappings: HashMap<String, String>,
}

impl TypeMapper {
    /// Creates a new TypeMapper with all KAIN→HLSL type mappings.
    pub fn new() -> Self {
        let mut mappings = HashMap::new();
        
        // Scalar types
        mappings.insert("Float".to_string(), "float".to_string());
        mappings.insert("Int".to_string(), "int".to_string());
        mappings.insert("UInt".to_string(), "uint".to_string());
        mappings.insert("Bool".to_string(), "bool".to_string());
        
        // Vector types - float variants
        mappings.insert("Vec2".to_string(), "float2".to_string());
        mappings.insert("Vec3".to_string(), "float3".to_string());
        mappings.insert("Vec4".to_string(), "float4".to_string());
        
        // Vector types - int variants
        mappings.insert("IVec2".to_string(), "int2".to_string());
        mappings.insert("IVec3".to_string(), "int3".to_string());
        mappings.insert("IVec4".to_string(), "int4".to_string());
        
        // Vector types - uint variants
        mappings.insert("UVec2".to_string(), "uint2".to_string());
        mappings.insert("UVec3".to_string(), "uint3".to_string());
        mappings.insert("UVec4".to_string(), "uint4".to_string());
        
        // Matrix types
        mappings.insert("Mat2".to_string(), "float2x2".to_string());
        mappings.insert("Mat3".to_string(), "float3x3".to_string());
        mappings.insert("Mat4".to_string(), "float4x4".to_string());
        
        // Texture types
        mappings.insert("Sampler2D".to_string(), "Texture2D".to_string());
        mappings.insert("Sampler3D".to_string(), "Texture3D".to_string());
        mappings.insert("SamplerCube".to_string(), "TextureCube".to_string());
        
        // Buffer types (UAVs)
        mappings.insert("RWBuffer".to_string(), "RWBuffer".to_string());
        mappings.insert("RWTexture2D".to_string(), "RWTexture2D".to_string());
        mappings.insert("RWTexture3D".to_string(), "RWTexture3D".to_string());
        
        TypeMapper { mappings }
    }
    
    /// Checks if a KAIN type can be mapped to HLSL.
    /// Returns true if the type is valid for use in shaders.
    ///
    /// # Example
    /// ```
    /// use ue5_shaders::type_mapping::TYPE_MAPPER;
    /// assert!(TYPE_MAPPER.can_map("Float"));
    /// assert!(TYPE_MAPPER.can_map("Vec3"));
    /// assert!(TYPE_MAPPER.can_map("UVec2"));
    /// assert!(!TYPE_MAPPER.can_map("InvalidType"));
    /// ```
    pub fn can_map(&self, kain_type: &str) -> bool {
        self.mappings.contains_key(kain_type)
    }
    
    /// Maps a KAIN type to its HLSL equivalent.
    /// Returns Some(hlsl_type) if the mapping exists, None otherwise.
    ///
    /// # Example
    /// ```
    /// use ue5_shaders::type_mapping::TYPE_MAPPER;
    /// assert_eq!(TYPE_MAPPER.map_to_hlsl("Float"), Some("float".to_string()));
    /// assert_eq!(TYPE_MAPPER.map_to_hlsl("Vec3"), Some("float3".to_string()));
    /// assert_eq!(TYPE_MAPPER.map_to_hlsl("UVec2"), Some("uint2".to_string()));
    /// assert_eq!(TYPE_MAPPER.map_to_hlsl("Mat4"), Some("float4x4".to_string()));
    /// assert_eq!(TYPE_MAPPER.map_to_hlsl("InvalidType"), None);
    /// ```
    pub fn map_to_hlsl(&self, kain_type: &str) -> Option<String> {
        self.mappings.get(kain_type).cloned()
    }
    
    /// Returns a list of all valid KAIN type names for error messages.
    pub fn valid_types(&self) -> Vec<String> {
        let mut types: Vec<String> = self.mappings.keys().cloned().collect();
        types.sort();
        types
    }
}

impl Default for TypeMapper {
    fn default() -> Self {
        Self::new()
    }
}

/// Global singleton instance of TypeMapper.
/// Use this for all type validation and mapping operations.
pub static TYPE_MAPPER: Lazy<TypeMapper> = Lazy::new(TypeMapper::new);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scalar_types() {
        assert!(TYPE_MAPPER.can_map("Float"));
        assert!(TYPE_MAPPER.can_map("Int"));
        assert!(TYPE_MAPPER.can_map("UInt"));
        assert!(TYPE_MAPPER.can_map("Bool"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Float"), Some("float".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Int"), Some("int".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UInt"), Some("uint".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Bool"), Some("bool".to_string()));
    }

    #[test]
    fn test_vector_types_float() {
        assert!(TYPE_MAPPER.can_map("Vec2"));
        assert!(TYPE_MAPPER.can_map("Vec3"));
        assert!(TYPE_MAPPER.can_map("Vec4"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Vec2"), Some("float2".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Vec3"), Some("float3".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Vec4"), Some("float4".to_string()));
    }

    #[test]
    fn test_vector_types_int() {
        assert!(TYPE_MAPPER.can_map("IVec2"));
        assert!(TYPE_MAPPER.can_map("IVec3"));
        assert!(TYPE_MAPPER.can_map("IVec4"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("IVec2"), Some("int2".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("IVec3"), Some("int3".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("IVec4"), Some("int4".to_string()));
    }

    #[test]
    fn test_vector_types_uint() {
        assert!(TYPE_MAPPER.can_map("UVec2"));
        assert!(TYPE_MAPPER.can_map("UVec3"));
        assert!(TYPE_MAPPER.can_map("UVec4"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UVec2"), Some("uint2".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UVec3"), Some("uint3".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UVec4"), Some("uint4".to_string()));
    }

    #[test]
    fn test_matrix_types() {
        assert!(TYPE_MAPPER.can_map("Mat2"));
        assert!(TYPE_MAPPER.can_map("Mat3"));
        assert!(TYPE_MAPPER.can_map("Mat4"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Mat2"), Some("float2x2".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Mat3"), Some("float3x3".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Mat4"), Some("float4x4".to_string()));
    }

    #[test]
    fn test_texture_types() {
        assert!(TYPE_MAPPER.can_map("Sampler2D"));
        assert!(TYPE_MAPPER.can_map("Sampler3D"));
        assert!(TYPE_MAPPER.can_map("SamplerCube"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Sampler2D"), Some("Texture2D".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Sampler3D"), Some("Texture3D".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("SamplerCube"), Some("TextureCube".to_string()));
    }

    #[test]
    fn test_buffer_types() {
        assert!(TYPE_MAPPER.can_map("RWBuffer"));
        assert!(TYPE_MAPPER.can_map("RWTexture2D"));
        assert!(TYPE_MAPPER.can_map("RWTexture3D"));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("RWBuffer"), Some("RWBuffer".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("RWTexture2D"), Some("RWTexture2D".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("RWTexture3D"), Some("RWTexture3D".to_string()));
    }

    #[test]
    fn test_invalid_types() {
        assert!(!TYPE_MAPPER.can_map("InvalidType"));
        assert!(!TYPE_MAPPER.can_map("String"));
        assert!(!TYPE_MAPPER.can_map("Array"));
        assert!(!TYPE_MAPPER.can_map(""));
        
        assert_eq!(TYPE_MAPPER.map_to_hlsl("InvalidType"), None);
        assert_eq!(TYPE_MAPPER.map_to_hlsl("String"), None);
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Array"), None);
        assert_eq!(TYPE_MAPPER.map_to_hlsl(""), None);
    }

    #[test]
    fn test_case_sensitivity() {
        // KAIN types are case-sensitive
        assert!(TYPE_MAPPER.can_map("Float"));
        assert!(!TYPE_MAPPER.can_map("float"));
        assert!(!TYPE_MAPPER.can_map("FLOAT"));
        
        assert!(TYPE_MAPPER.can_map("Vec3"));
        assert!(!TYPE_MAPPER.can_map("vec3"));
        assert!(!TYPE_MAPPER.can_map("VEC3"));
    }

    #[test]
    fn test_valid_types_list() {
        let valid_types = TYPE_MAPPER.valid_types();
        
        // Should contain all expected types
        assert!(valid_types.contains(&"Float".to_string()));
        assert!(valid_types.contains(&"Vec3".to_string()));
        assert!(valid_types.contains(&"UVec2".to_string()));
        assert!(valid_types.contains(&"Mat4".to_string()));
        assert!(valid_types.contains(&"Sampler2D".to_string()));
        assert!(valid_types.contains(&"RWBuffer".to_string()));
        
        // Should be sorted
        let mut sorted = valid_types.clone();
        sorted.sort();
        assert_eq!(valid_types, sorted);
        
        // Should have exactly the expected count
        // 4 scalars + 9 vectors + 3 matrices + 3 textures + 3 buffers = 22 types
        assert_eq!(valid_types.len(), 22);
    }

    #[test]
    fn test_all_mappings_are_bidirectional() {
        // Every type that can_map returns true for should have a valid map_to_hlsl result
        for kain_type in TYPE_MAPPER.valid_types() {
            assert!(TYPE_MAPPER.can_map(&kain_type));
            assert!(TYPE_MAPPER.map_to_hlsl(&kain_type).is_some());
        }
    }
}
