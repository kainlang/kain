use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: MaterialOutputs,
    pub properties: MaterialProperties,
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
    Power { base: String, exponent: String },
    Clamp { input: String, min: String, max: String },
    Fresnel { exponent: String, base_reflect_fraction: String },
    ComponentMask { input: String, r: bool, g: bool, b: bool, a: bool },
    Append { a: String, b: String },
    AppendVector { a: String, b: String },  // Alias for Append
    ConstantFloat { value: f32 },
    ConstantVec3 { value: [f32; 3] },
    ConstantVec4 { value: [f32; 4] },
    ConstantVector3 { value: [f32; 3] },  // Alias for ConstantVec3
    ConstantVector4 { value: [f32; 4] },  // Alias for ConstantVec4
    TextureCoordinate { index: u32, tiling: [f32; 2] },
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
