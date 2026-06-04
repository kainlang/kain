//! Compatibility shim for KAIN -> HLSL type mappings used by UE5 shader codegen.
//!
//! The canonical scalar/vector/matrix table now lives in `kain-shader-text` so
//! generic HLSL, WGSL, and USF codegen do not drift apart.

pub use kain_shader_text::TypeMapper;
pub use kain_shader_text::TYPE_MAPPER;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_core_scalar_vector_and_matrix_types_to_hlsl() {
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Float"), Some("float".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Int"), Some("int".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UInt"), Some("uint".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Bool"), Some("bool".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("Vec3"), Some("float3".to_string()));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("UVec3"), Some("uint3".to_string()));
        assert_eq!(
            TYPE_MAPPER.map_to_hlsl("Mat4"),
            Some("float4x4".to_string())
        );
    }

    #[test]
    fn rejects_non_shader_types() {
        assert!(!TYPE_MAPPER.can_map("String"));
        assert_eq!(TYPE_MAPPER.map_to_hlsl("InvalidType"), None);
    }
}
