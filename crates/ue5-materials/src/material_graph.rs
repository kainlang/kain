use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: MaterialOutputs,
    pub properties: MaterialProperties,
    pub is_dynamic: bool,  // Feature 7: Dynamic Materials - set to true when Time nodes are used
    pub dynamic_parameters: Vec<DynamicParameter>,  // Feature 7.1: Runtime-modifiable parameters

    // Phase 7.5: Vertex Shader Support
    pub uses_vertex_shader: bool,  // True when WorldPositionOffset is connected
    pub vertex_displacement_scale: Option<f32>,  // Optional displacement magnitude multiplier
}


/// Feature 7.1: Dynamic parameter metadata for runtime modification
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DynamicParameter {
    pub name: String,
    pub param_type: DynamicParameterType,
    pub default_value: DynamicParameterValue,
    pub min_value: Option<f32>,
    pub max_value: Option<f32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum DynamicParameterType {
    Scalar,
    Vector,
    Color,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum DynamicParameterValue {
    Scalar(f32),
    Vector([f32; 3]),
    Color([f32; 4]),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialInput {
    pub name: String,
    pub input_type: MaterialInputType,
    pub default_value: Option<String>,
    pub is_dynamic: bool,  // Feature 7.1: Mark parameter as runtime-modifiable via MID
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MaterialInputType {
    Texture2D,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
}

/// Output type for Custom HLSL nodes
/// Maps to UE5's ECustomMaterialOutputType enum values
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CustomOutputType {
    Float1,  // CMOT_Float1 - single float
    Float2,  // CMOT_Float2 - 2D vector
    Float3,  // CMOT_Float3 - 3D vector
    Float4,  // CMOT_Float4 - 4D vector
}

/// Input definition for Custom HLSL nodes
/// Each input becomes a pin on the UMaterialExpressionCustom node
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CustomInput {
    pub name: String,
    pub input_type: CustomOutputType,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialNode {
    pub id: String,
    pub node_type: MaterialNodeType,
    pub position: (i32, i32),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MaterialNodeType {
    TextureSample { texture_input: Option<String>, uv_input: Option<String> },
    TextureSampleParameter2D { param_name: String, default_texture: Option<String>, uv_input: Option<String> },
    ScalarParameter { name: String, default: f32 },
    VectorParameter { name: String, default: [f32; 3] },
    ColorParameter { name: String, default: [f32; 4] },
    Multiply { a: String, b: String },
    Add { a: String, b: String },
    Subtract { a: String, b: String },
    Divide { a: String, b: String },
    Lerp { a: String, b: String, alpha: String },
    Dot { a: String, b: String },
    DotProduct { a: String, b: String },  // Alias for Dot
    Cross { a: String, b: String },
    Normalize { input: String },
    Length { input: String },
    Distance { a: String, b: String },
    Power { base: String, exponent: String },
    Clamp { input: String, min: String, max: String },
    Abs { input: String },
    Min { a: String, b: String },
    Max { a: String, b: String },
    Saturate { input: String },
    Frac { input: String },
    Floor { input: String },
    Ceil { input: String },
    Round { input: String },
    Sqrt { input: String },
    Exp { input: String },
    Log { input: String },
    Sine { input: String },
    Cosine { input: String },
    Fresnel { exponent: String, base_reflect_fraction: String },
    ComponentMask { input: String, mask: String },
    Append { a: String, b: String },
    AppendVector { a: String, b: String },  // Alias for Append
    ConstantFloat { value: f32 },
    ConstantVec3 { value: [f32; 3] },
    ConstantVec4 { value: [f32; 4] },
    ConstantVector3 { value: [f32; 3] },  // Alias for ConstantVec3
    ConstantVector4 { value: [f32; 4] },  // Alias for ConstantVec4
    TextureCoordinate { index: u32, tiling: [f32; 2] },
    
    // Feature 1: Custom HLSL Nodes
    // Allows embedding arbitrary HLSL code directly in material graphs
    // Validates Requirements: 1.1, 1.3, 1.4
    CustomHLSL {
        code: String,
        output_type: CustomOutputType,
        inputs: Vec<CustomInput>,
    },
    
    // Feature 6: Time-Based Effects
    // Provides engine time for animations and pulsing effects
    // Validates Requirements: 6.1
    Time,  // UMaterialExpressionTime - provides engine time for animations
    
    // Feature 5: UV Manipulation
    // Allows scrolling, scaling, and rotating UV coordinates for animated textures
    // Validates Requirements: 5.1, 5.2, 5.3
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
    
    // Feature 3: Shader Integration
    // Allows calling existing KAIN shaders from within materials
    // Validates Requirements: 3.1
    MaterialFunctionCall {
        function_path: String,
        inputs: Vec<String>, // node IDs
    },
    
    // Feature 7.3: Material Layers
    // Allows blending multiple material graphs together with different blend modes
    // Validates Requirements: 7.3.1, 7.3.2, 7.3.3
    MaterialLayer {
        base_layer: String,      // node_id of base material
        blend_layer: String,     // node_id of blend material
        blend_mode: LayerBlendMode,
        alpha: String,           // node_id for blend alpha
    },
    MaterialLayerBlend {
        layers: Vec<String>,     // node_ids of layers to blend
        blend_modes: Vec<LayerBlendMode>,
        alphas: Vec<String>,     // node_ids for blend alphas
    },
    
    // Feature 7.4: World-Space Operations
    // Provides world-space position and normal data for procedural effects
    // Validates Requirements: 7.4.1, 7.4.2, 7.4.3
    WorldPosition,  // UMaterialExpressionWorldPosition - absolute world position
    WorldNormal,    // UMaterialExpressionVertexNormalWS - world-space vertex normal
    AbsoluteWorldPosition,  // UMaterialExpressionAbsoluteWorldPosition - absolute world position (no camera offset)
    CameraPosition,  // UMaterialExpressionCameraPositionWS - world-space camera position
    ObjectPosition,  // UMaterialExpressionObjectPositionWS - object pivot world position
    ObjectOrientation,  // UMaterialExpressionObjectOrientation - object rotation as vector
    
    // Feature 7.4: Triplanar Sampling
    // Samples texture from 3 axes and blends based on surface normal
    // Validates Requirements: 7.4.4
    TriplanarSample {
        texture: String,     // node_id of texture parameter
        world_position: Option<String>,  // optional custom position (defaults to WorldPosition)
        blend_sharpness: f32,  // controls blend between axes (higher = sharper transitions)
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialOutputs {
    pub base_color: Option<String>,
    pub metallic: Option<String>,
    pub specular: Option<String>,
    pub roughness: Option<String>,
    pub emissive: Option<String>,
    pub opacity: Option<String>,
    pub normal: Option<String>,
    pub ambient_occlusion: Option<String>,
    pub world_position_offset: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialProperties {
    pub domain: MaterialDomain,
    pub blend_mode: BlendMode,
    pub shading_model: ShadingModel,
    pub two_sided: bool,
    pub expose_parameters: bool,  // Feature 7.1: Enable runtime parameter modification via MID
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MaterialDomain {
    Surface,
    DeferredDecal,
    LightFunction,
    PostProcess,
    UI,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum BlendMode {
    Opaque,
    Masked,
    Translucent,
    Additive,
    Modulate,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ShadingModel {
    DefaultLit,
    Unlit,
    Subsurface,
    PreintegratedSkin,
    ClearCoat,
    SubsurfaceProfile,
    TwoSidedFoliage,
    Hair,
    Cloth,
    Eye,
}

/// Blend modes for material layer composition
/// Maps to UE5's material layer blending operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LayerBlendMode {
    Lerp,      // Linear interpolation between layers
    Add,       // Additive blending
    Multiply,  // Multiplicative blending
    Overlay,   // Overlay blending (screen + multiply)
    Screen,    // Screen blending (inverted multiply)
}

impl MaterialGraph {
    pub fn new(name: String) -> Self {
        Self {
            name,
            inputs: Vec::new(),
            nodes: Vec::new(),
            outputs: MaterialOutputs::default(),
            properties: MaterialProperties::default(),
            is_dynamic: false,
            dynamic_parameters: Vec::new(),
            uses_vertex_shader: false,  // Phase 7.5: Vertex shader disabled by default
            vertex_displacement_scale: None,  // Phase 7.5: No displacement scaling by default
        }
    }
    
    /// Feature 7.1: Mark a parameter as runtime-modifiable
    pub fn mark_parameter_dynamic(&mut self, param_name: &str) -> Result<(), String> {
        // Find the parameter in inputs
        let input = self.inputs.iter_mut()
            .find(|i| i.name == param_name)
            .ok_or_else(|| format!("Parameter '{}' not found", param_name))?;
        
        input.is_dynamic = true;
        
        // Add to dynamic_parameters list
        let (param_type, default_value) = match input.input_type {
            MaterialInputType::Float => {
                let default = input.default_value.as_ref()
                    .and_then(|s| s.parse::<f32>().ok())
                    .unwrap_or(0.0);
                (DynamicParameterType::Scalar, DynamicParameterValue::Scalar(default))
            }
            MaterialInputType::Vec3 => {
                (DynamicParameterType::Vector, DynamicParameterValue::Vector([0.0, 0.0, 0.0]))
            }
            MaterialInputType::Color | MaterialInputType::Vec4 => {
                (DynamicParameterType::Color, DynamicParameterValue::Color([1.0, 1.0, 1.0, 1.0]))
            }
            _ => {
                let type_name = match input.input_type {
                    MaterialInputType::Texture2D => "Texture2D",
                    // MaterialInputType::TextureCube => "TextureCube",  // TODO: Add TextureCube variant to MaterialInputType enum
                    _ => "unknown type",
                };
                return Err(format!("Parameter type '{}' cannot be made dynamic", type_name));
            }
        };
        
        self.dynamic_parameters.push(DynamicParameter {
            name: param_name.to_string(),
            param_type,
            default_value,
            min_value: None,
            max_value: None,
        });
        
        Ok(())
    }
}

impl Default for MaterialOutputs {
    fn default() -> Self {
        Self {
            base_color: None,
            metallic: None,
            specular: None,
            roughness: None,
            emissive: None,
            opacity: None,
            normal: None,
            ambient_occlusion: None,
            world_position_offset: None,
        }
    }
}

impl Default for MaterialProperties {
    fn default() -> Self {
        Self {
            domain: MaterialDomain::Surface,
            blend_mode: BlendMode::Opaque,
            shading_model: ShadingModel::DefaultLit,
            two_sided: false,
            expose_parameters: false,  // Feature 7.1: Disabled by default
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mark_parameter_dynamic_scalar() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Add a scalar input
        graph.inputs.push(MaterialInput {
            name: "Roughness".to_string(),
            input_type: MaterialInputType::Float,
            default_value: Some("0.5".to_string()),
            is_dynamic: false,
        });
        
        // Mark as dynamic
        let result = graph.mark_parameter_dynamic("Roughness");
        assert!(result.is_ok());
        
        // Verify input is marked dynamic
        assert!(graph.inputs[0].is_dynamic);
        
        // Verify dynamic_parameters list is updated
        assert_eq!(graph.dynamic_parameters.len(), 1);
        assert_eq!(graph.dynamic_parameters[0].name, "Roughness");
        assert!(matches!(graph.dynamic_parameters[0].param_type, DynamicParameterType::Scalar));
        assert!(matches!(graph.dynamic_parameters[0].default_value, DynamicParameterValue::Scalar(0.5)));
    }

    #[test]
    fn test_mark_parameter_dynamic_vector() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Add a vector input
        graph.inputs.push(MaterialInput {
            name: "Tint".to_string(),
            input_type: MaterialInputType::Vec3,
            default_value: None,
            is_dynamic: false,
        });
        
        // Mark as dynamic
        let result = graph.mark_parameter_dynamic("Tint");
        assert!(result.is_ok());
        
        // Verify input is marked dynamic
        assert!(graph.inputs[0].is_dynamic);
        
        // Verify dynamic_parameters list is updated
        assert_eq!(graph.dynamic_parameters.len(), 1);
        assert_eq!(graph.dynamic_parameters[0].name, "Tint");
        assert!(matches!(graph.dynamic_parameters[0].param_type, DynamicParameterType::Vector));
    }

    #[test]
    fn test_mark_parameter_dynamic_color() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Add a color input
        graph.inputs.push(MaterialInput {
            name: "EmissiveColor".to_string(),
            input_type: MaterialInputType::Color,
            default_value: None,
            is_dynamic: false,
        });
        
        // Mark as dynamic
        let result = graph.mark_parameter_dynamic("EmissiveColor");
        assert!(result.is_ok());
        
        // Verify input is marked dynamic
        assert!(graph.inputs[0].is_dynamic);
        
        // Verify dynamic_parameters list is updated
        assert_eq!(graph.dynamic_parameters.len(), 1);
        assert_eq!(graph.dynamic_parameters[0].name, "EmissiveColor");
        assert!(matches!(graph.dynamic_parameters[0].param_type, DynamicParameterType::Color));
    }

    #[test]
    fn test_mark_parameter_dynamic_not_found() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Try to mark non-existent parameter
        let result = graph.mark_parameter_dynamic("NonExistent");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not found"));
    }

    #[test]
    fn test_mark_parameter_dynamic_texture_fails() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Add a texture input
        graph.inputs.push(MaterialInput {
            name: "AlbedoMap".to_string(),
            input_type: MaterialInputType::Texture2D,
            default_value: None,
            is_dynamic: false,
        });
        
        // Try to mark texture as dynamic (should fail)
        let result = graph.mark_parameter_dynamic("AlbedoMap");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("cannot be made dynamic"));
    }

    #[test]
    fn test_multiple_dynamic_parameters() {
        let mut graph = MaterialGraph::new("TestMaterial".to_string());
        
        // Add multiple inputs
        graph.inputs.push(MaterialInput {
            name: "Roughness".to_string(),
            input_type: MaterialInputType::Float,
            default_value: Some("0.5".to_string()),
            is_dynamic: false,
        });
        graph.inputs.push(MaterialInput {
            name: "Metallic".to_string(),
            input_type: MaterialInputType::Float,
            default_value: Some("0.0".to_string()),
            is_dynamic: false,
        });
        graph.inputs.push(MaterialInput {
            name: "Tint".to_string(),
            input_type: MaterialInputType::Vec3,
            default_value: None,
            is_dynamic: false,
        });
        
        // Mark all as dynamic
        assert!(graph.mark_parameter_dynamic("Roughness").is_ok());
        assert!(graph.mark_parameter_dynamic("Metallic").is_ok());
        assert!(graph.mark_parameter_dynamic("Tint").is_ok());
        
        // Verify all are marked
        assert_eq!(graph.dynamic_parameters.len(), 3);
        assert!(graph.inputs.iter().all(|i| i.is_dynamic));
    }
}