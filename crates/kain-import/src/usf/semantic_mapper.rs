// ============================================================================
// USF Semantic Mapper - HLSL/USF → KAIN AST Transformation
// ============================================================================
// Maps USF/HLSL constructs to KAIN equivalents:
// - cbuffer → uniform declarations with binding slots
// - Texture2D/RWTexture2D → KAIN texture/buffer declarations
// - SamplerState → implicit sampler handling
// - Register bindings (register(b0), register(t0), register(u0)) → @N syntax
// - HLSL types → KAIN types (float4 → Vec4, uint → UInt, etc.)
// - Semantics (SV_DispatchThreadID, SV_Position) → KAIN built-ins
// ============================================================================

use std::collections::HashMap;
use kain_core::ast::{Item, Expr, Type, Stmt};

/// Tracks binding slots across different register spaces
#[derive(Debug, Default)]
pub struct BindingTracker {
    /// Constant buffer bindings (register(b0), register(b1), ...)
    cbuffer_bindings: HashMap<String, u32>,
    
    /// Texture bindings (register(t0), register(t1), ...)
    texture_bindings: HashMap<String, u32>,
    
    /// UAV bindings (register(u0), register(u1), ...)
    uav_bindings: HashMap<String, u32>,
    
    /// Sampler bindings (register(s0), register(s1), ...)
    sampler_bindings: HashMap<String, u32>,
    
    /// Next available binding slot for each register space
    next_cbuffer_slot: u32,
    next_texture_slot: u32,
    next_uav_slot: u32,
    next_sampler_slot: u32,
}

impl BindingTracker {
    pub fn new() -> Self {
        Self::default()
    }
    
    /// Register a cbuffer binding and return the slot number
    pub fn register_cbuffer(&mut self, name: &str, explicit_slot: Option<u32>) -> u32 {
        if let Some(slot) = explicit_slot {
            self.cbuffer_bindings.insert(name.to_string(), slot);
            self.next_cbuffer_slot = self.next_cbuffer_slot.max(slot + 1);
            slot
        } else {
            let slot = self.next_cbuffer_slot;
            self.cbuffer_bindings.insert(name.to_string(), slot);
            self.next_cbuffer_slot += 1;
            slot
        }
    }

    /// Register a texture binding and return the slot number
    pub fn register_texture(&mut self, name: &str, explicit_slot: Option<u32>) -> u32 {
        if let Some(slot) = explicit_slot {
            self.texture_bindings.insert(name.to_string(), slot);
            self.next_texture_slot = self.next_texture_slot.max(slot + 1);
            slot
        } else {
            let slot = self.next_texture_slot;
            self.texture_bindings.insert(name.to_string(), slot);
            self.next_texture_slot += 1;
            slot
        }
    }
    
    /// Register a UAV binding and return the slot number
    pub fn register_uav(&mut self, name: &str, explicit_slot: Option<u32>) -> u32 {
        if let Some(slot) = explicit_slot {
            self.uav_bindings.insert(name.to_string(), slot);
            self.next_uav_slot = self.next_uav_slot.max(slot + 1);
            slot
        } else {
            let slot = self.next_uav_slot;
            self.uav_bindings.insert(name.to_string(), slot);
            self.next_uav_slot += 1;
            slot
        }
    }
    
    /// Register a sampler binding and return the slot number
    pub fn register_sampler(&mut self, name: &str, explicit_slot: Option<u32>) -> u32 {
        if let Some(slot) = explicit_slot {
            self.sampler_bindings.insert(name.to_string(), slot);
            self.next_sampler_slot = self.next_sampler_slot.max(slot + 1);
            slot
        } else {
            let slot = self.next_sampler_slot;
            self.sampler_bindings.insert(name.to_string(), slot);
            self.next_sampler_slot += 1;
            slot
        }
    }
}


/// Main semantic mapper for USF → KAIN transformation
pub struct SemanticMapper {
    binding_tracker: BindingTracker,
    
    /// Maps HLSL semantics to KAIN built-in variables
    semantic_map: HashMap<String, String>,
    
    /// Maps HLSL types to KAIN types
    type_map: HashMap<String, String>,
}

impl SemanticMapper {
    pub fn new() -> Self {
        let mut semantic_map = HashMap::new();
        
        // Compute shader semantics
        semantic_map.insert("SV_DispatchThreadID".to_string(), "thread_id".to_string());
        semantic_map.insert("SV_GroupThreadID".to_string(), "local_thread_id".to_string());
        semantic_map.insert("SV_GroupID".to_string(), "group_id".to_string());
        semantic_map.insert("SV_GroupIndex".to_string(), "group_index".to_string());
        
        // Vertex shader semantics
        semantic_map.insert("SV_Position".to_string(), "position".to_string());
        semantic_map.insert("SV_VertexID".to_string(), "vertex_id".to_string());
        semantic_map.insert("SV_InstanceID".to_string(), "instance_id".to_string());
        
        // Pixel shader semantics
        semantic_map.insert("SV_Target".to_string(), "color_output".to_string());
        semantic_map.insert("SV_Target0".to_string(), "color_output_0".to_string());
        semantic_map.insert("SV_Target1".to_string(), "color_output_1".to_string());
        semantic_map.insert("SV_Depth".to_string(), "depth_output".to_string());
        
        let mut type_map = HashMap::new();
        
        // Scalar types
        type_map.insert("float".to_string(), "Float".to_string());
        type_map.insert("int".to_string(), "Int".to_string());
        type_map.insert("uint".to_string(), "UInt".to_string());
        type_map.insert("bool".to_string(), "Bool".to_string());

        
        // Vector types - float variants
        type_map.insert("float2".to_string(), "Vec2".to_string());
        type_map.insert("float3".to_string(), "Vec3".to_string());
        type_map.insert("float4".to_string(), "Vec4".to_string());
        
        // Vector types - int variants
        type_map.insert("int2".to_string(), "IVec2".to_string());
        type_map.insert("int3".to_string(), "IVec3".to_string());
        type_map.insert("int4".to_string(), "IVec4".to_string());
        
        // Vector types - uint variants
        type_map.insert("uint2".to_string(), "UVec2".to_string());
        type_map.insert("uint3".to_string(), "UVec3".to_string());
        type_map.insert("uint4".to_string(), "UVec4".to_string());
        
        // Matrix types
        type_map.insert("float2x2".to_string(), "Mat2".to_string());
        type_map.insert("float3x3".to_string(), "Mat3".to_string());
        type_map.insert("float4x4".to_string(), "Mat4".to_string());
        
        // Texture types
        type_map.insert("Texture2D".to_string(), "Sampler2D".to_string());
        type_map.insert("Texture3D".to_string(), "Sampler3D".to_string());
        type_map.insert("TextureCube".to_string(), "SamplerCube".to_string());
        
        // Buffer types (UAVs)
        type_map.insert("RWBuffer".to_string(), "RWBuffer".to_string());
        type_map.insert("RWTexture2D".to_string(), "RWTexture2D".to_string());
        type_map.insert("RWTexture3D".to_string(), "RWTexture3D".to_string());
        
        Self {
            binding_tracker: BindingTracker::new(),
            semantic_map,
            type_map,
        }
    }

    
    /// Maps an HLSL type to a KAIN type
    /// 
    /// # Examples
    /// ```
    /// let mapper = SemanticMapper::new();
    /// assert_eq!(mapper.map_type("float4"), Some("Vec4"));
    /// assert_eq!(mapper.map_type("uint"), Some("UInt"));
    /// assert_eq!(mapper.map_type("Texture2D"), Some("Sampler2D"));
    /// ```
    pub fn map_type(&self, hlsl_type: &str) -> Option<&str> {
        self.type_map.get(hlsl_type).map(|s| s.as_str())
    }
    
    /// Maps an HLSL semantic to a KAIN built-in variable name
    /// 
    /// # Examples
    /// ```
    /// let mapper = SemanticMapper::new();
    /// assert_eq!(mapper.map_semantic("SV_DispatchThreadID"), Some("thread_id"));
    /// assert_eq!(mapper.map_semantic("SV_Position"), Some("position"));
    /// ```
    pub fn map_semantic(&self, semantic: &str) -> Option<&str> {
        self.semantic_map.get(semantic).map(|s| s.as_str())
    }
    
    /// Extracts register binding from HLSL register syntax
    /// 
    /// # Examples
    /// ```
    /// assert_eq!(extract_register_binding("register(b0)"), Some(('b', 0)));
    /// assert_eq!(extract_register_binding("register(t5)"), Some(('t', 5)));
    /// assert_eq!(extract_register_binding("register(u2)"), Some(('u', 2)));
    /// ```
    fn extract_register_binding(register_str: &str) -> Option<(char, u32)> {
        // Parse "register(b0)" → ('b', 0)
        let inner = register_str.strip_prefix("register(")?.strip_suffix(")")?;
        let register_type = inner.chars().next()?;
        let slot = inner[1..].parse::<u32>().ok()?;
        Some((register_type, slot))
    }

    
    /// Maps a cbuffer declaration to multiple KAIN uniform declarations
    /// 
    /// # HLSL Input
    /// ```hlsl
    /// cbuffer MyConstants : register(b0)
    /// {
    ///     float4 Color;
    ///     float Intensity;
    ///     float2 Offset;
    /// };
    /// ```
    /// 
    /// # KAIN Output
    /// ```kain
    /// uniform Color: Vec4 @0
    /// uniform Intensity: Float @0
    /// uniform Offset: Vec2 @0
    /// ```
    pub fn map_cbuffer(
        &mut self,
        cbuffer_name: &str,
        fields: Vec<(String, String)>, // (field_name, hlsl_type)
        register_binding: Option<&str>,
    ) -> Result<Vec<String>, String> {
        // Extract register binding if present
        let slot = if let Some(reg) = register_binding {
            let (reg_type, slot_num) = Self::extract_register_binding(reg)
                .ok_or_else(|| format!("Invalid register binding: {}", reg))?;
            
            if reg_type != 'b' {
                return Err(format!("cbuffer must use 'b' register, got '{}'", reg_type));
            }
            
            self.binding_tracker.register_cbuffer(cbuffer_name, Some(slot_num))
        } else {
            self.binding_tracker.register_cbuffer(cbuffer_name, None)
        };
        
        let mut uniforms = Vec::new();
        
        for (field_name, hlsl_type) in fields {
            let kain_type = self.map_type(&hlsl_type)
                .ok_or_else(|| format!("Unknown HLSL type: {}", hlsl_type))?;
            
            // All fields in a cbuffer share the same binding slot
            uniforms.push(format!("uniform {}: {} @{}", field_name, kain_type, slot));
        }
        
        Ok(uniforms)
    }

    
    /// Maps a Texture2D declaration to a KAIN uniform texture
    /// 
    /// # HLSL Input
    /// ```hlsl
    /// Texture2D MyTexture : register(t0);
    /// SamplerState MySampler : register(s0);
    /// ```
    /// 
    /// # KAIN Output
    /// ```kain
    /// uniform MyTexture: Sampler2D @0
    /// ```
    /// 
    /// Note: SamplerState is implicit in KAIN - no separate declaration needed
    pub fn map_texture(
        &mut self,
        texture_name: &str,
        texture_type: &str, // "Texture2D", "Texture3D", "TextureCube"
        register_binding: Option<&str>,
    ) -> Result<String, String> {
        // Extract register binding if present
        let slot = if let Some(reg) = register_binding {
            let (reg_type, slot_num) = Self::extract_register_binding(reg)
                .ok_or_else(|| format!("Invalid register binding: {}", reg))?;
            
            if reg_type != 't' {
                return Err(format!("Texture must use 't' register, got '{}'", reg_type));
            }
            
            self.binding_tracker.register_texture(texture_name, Some(slot_num))
        } else {
            self.binding_tracker.register_texture(texture_name, None)
        };
        
        let kain_type = self.map_type(texture_type)
            .ok_or_else(|| format!("Unknown texture type: {}", texture_type))?;
        
        Ok(format!("uniform {}: {} @{}", texture_name, kain_type, slot))
    }

    
    /// Maps an RWTexture2D/RWBuffer declaration to a KAIN buffer declaration
    /// 
    /// # HLSL Input
    /// ```hlsl
    /// RWTexture2D<float4> OutputTexture : register(u0);
    /// RWBuffer<float> OutputBuffer : register(u1);
    /// ```
    /// 
    /// # KAIN Output
    /// ```kain
    /// buffer OutputTexture: RWTexture2D @0
    /// buffer OutputBuffer: RWBuffer @1
    /// ```
    pub fn map_rw_texture(
        &mut self,
        buffer_name: &str,
        buffer_type: &str, // "RWTexture2D", "RWTexture3D", "RWBuffer"
        register_binding: Option<&str>,
    ) -> Result<String, String> {
        // Extract register binding if present
        let slot = if let Some(reg) = register_binding {
            let (reg_type, slot_num) = Self::extract_register_binding(reg)
                .ok_or_else(|| format!("Invalid register binding: {}", reg))?;
            
            if reg_type != 'u' {
                return Err(format!("UAV must use 'u' register, got '{}'", reg_type));
            }
            
            self.binding_tracker.register_uav(buffer_name, Some(slot_num))
        } else {
            self.binding_tracker.register_uav(buffer_name, None)
        };
        
        let kain_type = self.map_type(buffer_type)
            .ok_or_else(|| format!("Unknown buffer type: {}", buffer_type))?;
        
        Ok(format!("buffer {}: {} @{}", buffer_name, kain_type, slot))
    }

    
    /// Maps a SamplerState declaration (usually ignored in KAIN as samplers are implicit)
    /// 
    /// # HLSL Input
    /// ```hlsl
    /// SamplerState MySampler : register(s0);
    /// ```
    /// 
    /// # KAIN Output
    /// None - samplers are implicit in KAIN texture sampling
    pub fn map_sampler_state(
        &mut self,
        sampler_name: &str,
        register_binding: Option<&str>,
    ) -> Result<Option<String>, String> {
        // Track the binding for completeness, but don't generate KAIN code
        if let Some(reg) = register_binding {
            let (reg_type, slot_num) = Self::extract_register_binding(reg)
                .ok_or_else(|| format!("Invalid register binding: {}", reg))?;
            
            if reg_type != 's' {
                return Err(format!("SamplerState must use 's' register, got '{}'", reg_type));
            }
            
            self.binding_tracker.register_sampler(sampler_name, Some(slot_num));
        } else {
            self.binding_tracker.register_sampler(sampler_name, None);
        }
        
        // Return None - KAIN doesn't need explicit sampler declarations
        Ok(None)
    }
    
    /// Get the current binding tracker state (useful for debugging)
    pub fn binding_tracker(&self) -> &BindingTracker {
        &self.binding_tracker
    }
}

impl Default for SemanticMapper {
    fn default() -> Self {
        Self::new()
    }
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_type_mapping() {
        let mapper = SemanticMapper::new();
        
        // Scalar types
        assert_eq!(mapper.map_type("float"), Some("Float"));
        assert_eq!(mapper.map_type("int"), Some("Int"));
        assert_eq!(mapper.map_type("uint"), Some("UInt"));
        assert_eq!(mapper.map_type("bool"), Some("Bool"));
        
        // Vector types
        assert_eq!(mapper.map_type("float2"), Some("Vec2"));
        assert_eq!(mapper.map_type("float3"), Some("Vec3"));
        assert_eq!(mapper.map_type("float4"), Some("Vec4"));
        assert_eq!(mapper.map_type("uint3"), Some("UVec3"));
        
        // Matrix types
        assert_eq!(mapper.map_type("float4x4"), Some("Mat4"));
        
        // Texture types
        assert_eq!(mapper.map_type("Texture2D"), Some("Sampler2D"));
        assert_eq!(mapper.map_type("RWTexture2D"), Some("RWTexture2D"));
        
        // Unknown type
        assert_eq!(mapper.map_type("UnknownType"), None);
    }

    #[test]
    fn test_semantic_mapping() {
        let mapper = SemanticMapper::new();
        
        // Compute shader semantics
        assert_eq!(mapper.map_semantic("SV_DispatchThreadID"), Some("thread_id"));
        assert_eq!(mapper.map_semantic("SV_GroupThreadID"), Some("local_thread_id"));
        assert_eq!(mapper.map_semantic("SV_GroupID"), Some("group_id"));
        
        // Vertex shader semantics
        assert_eq!(mapper.map_semantic("SV_Position"), Some("position"));
        assert_eq!(mapper.map_semantic("SV_VertexID"), Some("vertex_id"));
        
        // Pixel shader semantics
        assert_eq!(mapper.map_semantic("SV_Target"), Some("color_output"));
        
        // Unknown semantic
        assert_eq!(mapper.map_semantic("UNKNOWN"), None);
    }


    #[test]
    fn test_extract_register_binding() {
        assert_eq!(
            SemanticMapper::extract_register_binding("register(b0)"),
            Some(('b', 0))
        );
        assert_eq!(
            SemanticMapper::extract_register_binding("register(t5)"),
            Some(('t', 5))
        );
        assert_eq!(
            SemanticMapper::extract_register_binding("register(u2)"),
            Some(('u', 2))
        );
        assert_eq!(
            SemanticMapper::extract_register_binding("register(s1)"),
            Some(('s', 1))
        );
        
        // Invalid formats
        assert_eq!(SemanticMapper::extract_register_binding("invalid"), None);
        assert_eq!(SemanticMapper::extract_register_binding("register(x0)"), Some(('x', 0)));
        assert_eq!(SemanticMapper::extract_register_binding("register(b)"), None);
    }

    #[test]
    fn test_cbuffer_mapping() {
        let mut mapper = SemanticMapper::new();
        
        let fields = vec![
            ("Color".to_string(), "float4".to_string()),
            ("Intensity".to_string(), "float".to_string()),
            ("Offset".to_string(), "float2".to_string()),
        ];
        
        let result = mapper.map_cbuffer("MyConstants", fields, Some("register(b0)"));
        assert!(result.is_ok());
        
        let uniforms = result.unwrap();
        assert_eq!(uniforms.len(), 3);
        assert_eq!(uniforms[0], "uniform Color: Vec4 @0");
        assert_eq!(uniforms[1], "uniform Intensity: Float @0");
        assert_eq!(uniforms[2], "uniform Offset: Vec2 @0");
    }


    #[test]
    fn test_cbuffer_auto_binding() {
        let mut mapper = SemanticMapper::new();
        
        let fields1 = vec![("Value1".to_string(), "float".to_string())];
        let result1 = mapper.map_cbuffer("Buffer1", fields1, None);
        assert!(result1.is_ok());
        assert_eq!(result1.unwrap()[0], "uniform Value1: Float @0");
        
        let fields2 = vec![("Value2".to_string(), "float".to_string())];
        let result2 = mapper.map_cbuffer("Buffer2", fields2, None);
        assert!(result2.is_ok());
        assert_eq!(result2.unwrap()[0], "uniform Value2: Float @1");
    }

    #[test]
    fn test_texture_mapping() {
        let mut mapper = SemanticMapper::new();
        
        let result = mapper.map_texture("MyTexture", "Texture2D", Some("register(t0)"));
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "uniform MyTexture: Sampler2D @0");
        
        let result2 = mapper.map_texture("MyTexture3D", "Texture3D", Some("register(t1)"));
        assert!(result2.is_ok());
        assert_eq!(result2.unwrap(), "uniform MyTexture3D: Sampler3D @1");
    }

    #[test]
    fn test_texture_auto_binding() {
        let mut mapper = SemanticMapper::new();
        
        let result1 = mapper.map_texture("Tex1", "Texture2D", None);
        assert!(result1.is_ok());
        assert_eq!(result1.unwrap(), "uniform Tex1: Sampler2D @0");
        
        let result2 = mapper.map_texture("Tex2", "Texture2D", None);
        assert!(result2.is_ok());
        assert_eq!(result2.unwrap(), "uniform Tex2: Sampler2D @1");
    }


    #[test]
    fn test_rw_texture_mapping() {
        let mut mapper = SemanticMapper::new();
        
        let result = mapper.map_rw_texture("OutputTex", "RWTexture2D", Some("register(u0)"));
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "buffer OutputTex: RWTexture2D @0");
        
        let result2 = mapper.map_rw_texture("OutputBuf", "RWBuffer", Some("register(u1)"));
        assert!(result2.is_ok());
        assert_eq!(result2.unwrap(), "buffer OutputBuf: RWBuffer @1");
    }

    #[test]
    fn test_rw_texture_auto_binding() {
        let mut mapper = SemanticMapper::new();
        
        let result1 = mapper.map_rw_texture("Out1", "RWTexture2D", None);
        assert!(result1.is_ok());
        assert_eq!(result1.unwrap(), "buffer Out1: RWTexture2D @0");
        
        let result2 = mapper.map_rw_texture("Out2", "RWTexture3D", None);
        assert!(result2.is_ok());
        assert_eq!(result2.unwrap(), "buffer Out2: RWTexture3D @1");
    }

    #[test]
    fn test_sampler_state_mapping() {
        let mut mapper = SemanticMapper::new();
        
        // SamplerState should return None (implicit in KAIN)
        let result = mapper.map_sampler_state("MySampler", Some("register(s0)"));
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), None);
        
        // But it should still track the binding
        assert_eq!(mapper.binding_tracker().sampler_bindings.get("MySampler"), Some(&0));
    }


    #[test]
    fn test_wrong_register_type_for_cbuffer() {
        let mut mapper = SemanticMapper::new();
        
        let fields = vec![("Value".to_string(), "float".to_string())];
        let result = mapper.map_cbuffer("MyBuffer", fields, Some("register(t0)"));
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("cbuffer must use 'b' register"));
    }

    #[test]
    fn test_wrong_register_type_for_texture() {
        let mut mapper = SemanticMapper::new();
        
        let result = mapper.map_texture("MyTexture", "Texture2D", Some("register(b0)"));
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Texture must use 't' register"));
    }

    #[test]
    fn test_wrong_register_type_for_uav() {
        let mut mapper = SemanticMapper::new();
        
        let result = mapper.map_rw_texture("MyBuffer", "RWTexture2D", Some("register(t0)"));
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("UAV must use 'u' register"));
    }

    #[test]
    fn test_unknown_hlsl_type() {
        let mut mapper = SemanticMapper::new();
        
        let fields = vec![("Value".to_string(), "UnknownType".to_string())];
        let result = mapper.map_cbuffer("MyBuffer", fields, None);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Unknown HLSL type"));
    }


    #[test]
    fn test_binding_tracker_explicit_slots() {
        let mut tracker = BindingTracker::new();
        
        // Register explicit slots
        assert_eq!(tracker.register_cbuffer("Buf1", Some(5)), 5);
        assert_eq!(tracker.register_cbuffer("Buf2", Some(2)), 2);
        
        // Next auto slot should be after the highest explicit slot
        assert_eq!(tracker.register_cbuffer("Buf3", None), 6);
    }

    #[test]
    fn test_binding_tracker_mixed_slots() {
        let mut tracker = BindingTracker::new();
        
        // Mix of auto and explicit
        assert_eq!(tracker.register_texture("Tex1", None), 0);
        assert_eq!(tracker.register_texture("Tex2", Some(5)), 5);
        assert_eq!(tracker.register_texture("Tex3", None), 6);
        assert_eq!(tracker.register_texture("Tex4", Some(3)), 3);
        assert_eq!(tracker.register_texture("Tex5", None), 7);
    }

    #[test]
    fn test_binding_tracker_separate_spaces() {
        let mut tracker = BindingTracker::new();
        
        // Different register spaces should have independent slot counters
        assert_eq!(tracker.register_cbuffer("CB", None), 0);
        assert_eq!(tracker.register_texture("Tex", None), 0);
        assert_eq!(tracker.register_uav("UAV", None), 0);
        assert_eq!(tracker.register_sampler("Samp", None), 0);
        
        assert_eq!(tracker.register_cbuffer("CB2", None), 1);
        assert_eq!(tracker.register_texture("Tex2", None), 1);
        assert_eq!(tracker.register_uav("UAV2", None), 1);
        assert_eq!(tracker.register_sampler("Samp2", None), 1);
    }
}
