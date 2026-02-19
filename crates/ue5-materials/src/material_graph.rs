use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: MaterialOutputs,
    pub properties: MaterialProperties,
    pub is_dynamic: bool,  // Feature 7: Dynamic Materials - set to true when Time nodes are used
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialInput {
    pub name: String,
    pub input_type: MaterialInputType,
    pub default_value: Option<String>,
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

impl MaterialGraph {
    pub fn new(name: String) -> Self {
        Self {
            name,
            inputs: Vec::new(),
            nodes: Vec::new(),
            outputs: MaterialOutputs::default(),
            properties: MaterialProperties::default(),
            is_dynamic: false,
        }
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
        }
    }
}
