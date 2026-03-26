use serde::{Deserialize, Serialize};

fn default_white_rgba() -> [u8; 4] {
    [255, 255, 255, 255]
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Fast3dSmokeManifest {
    pub title: String,
    pub root_display_list: String,
    pub resolution: ResolutionConfig,
    pub clear_color: [u8; 4],
    pub camera: CameraConfig,
    #[serde(default)]
    pub auto_rotation_radians_per_second: f32,
    #[serde(default)]
    pub segment_bindings: Vec<SegmentBinding>,
    #[serde(default)]
    pub light_groups: Vec<LightGroupDefinition>,
    pub textures: Vec<TextureDefinition>,
    pub display_lists: Vec<DisplayListDefinition>,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct ResolutionConfig {
    pub width: usize,
    pub height: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct CameraConfig {
    pub target: [f32; 3],
    pub orbit_radius: f32,
    pub orbit_height: f32,
    #[serde(default)]
    pub initial_yaw_radians: f32,
    #[serde(default)]
    pub initial_pitch_radians: f32,
    pub fov_y_degrees: f32,
    pub near_plane: f32,
    pub far_plane: f32,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct SegmentBinding {
    pub segment_id: u8,
    pub kind: SegmentBindingKind,
    pub target_id: String,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum SegmentBindingKind {
    Texture,
    DisplayList,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct LightGroupDefinition {
    pub id: String,
    pub ambient_color: [u8; 4],
    pub diffuse_color: [u8; 4],
    pub direction: [f32; 3],
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct TextureDefinition {
    pub id: String,
    #[serde(flatten)]
    pub source: TextureSource,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum TextureSource {
    Checkerboard {
        width: u32,
        height: u32,
        cell_size: u32,
        color_a: [u8; 4],
        color_b: [u8; 4],
    },
    Stripes {
        width: u32,
        height: u32,
        stripe_height: u32,
        color_a: [u8; 4],
        color_b: [u8; 4],
    },
    GeneratedSm64TitleCard {
        width: u32,
        height: u32,
    },
    GeneratedMarioEyesFront {
        width: u32,
        height: u32,
    },
    GeneratedMarioMustache {
        width: u32,
        height: u32,
    },
    GeneratedMarioSideburn {
        width: u32,
        height: u32,
    },
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct DisplayListDefinition {
    pub id: String,
    pub commands: Vec<DisplayListCommand>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(tag = "op", rename_all = "snake_case")]
pub enum DisplayListCommand {
    PushMatrix { matrix: [[f32; 4]; 4] },
    PopMatrix,
    LoadVertices { slot: u16, vertices: Vec<Fast3dVertex> },
    DrawTriangles { triangles: Vec<[u16; 3]> },
    BindTexture { texture_id: String },
    BindTextureSegment { segment_id: u8 },
    CallDisplayList { display_list_id: String },
    CallDisplayListSegment { segment_id: u8 },
    SetCombineMode { mode: CombineMode },
    SetPrimitiveColor { color: [u8; 4] },
    SetEnvColor { color: [u8; 4] },
    SetLightGroup { light_group_id: String },
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct Fast3dVertex {
    pub position: [f32; 3],
    pub uv: [f32; 2],
    #[serde(default = "default_white_rgba")]
    pub color: [u8; 4],
    #[serde(default)]
    pub normal: Option<[f32; 3]>,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum CombineMode {
    Texture,
    TextureVertex,
    TexturePrimitive,
    TextureVertexPrimitive,
    TextureEnvMix,
    Primitive,
    Vertex,
}
