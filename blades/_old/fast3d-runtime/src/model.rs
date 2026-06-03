use serde::{Deserialize, Serialize};

fn default_white_rgba() -> [u8; 4] {
    [255, 255, 255, 255]
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Fast3dSmokeManifest {
    pub title: String,
    #[serde(default)]
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
    #[serde(default)]
    pub scene_instances: Vec<SceneInstanceDefinition>,
    pub textures: Vec<TextureDefinition>,
    #[serde(default)]
    pub shader_overrides: Vec<ShaderOverrideDefinition>,
    pub display_lists: Vec<DisplayListDefinition>,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct ResolutionConfig {
    pub width: usize,
    pub height: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct CameraConfig {
    #[serde(default)]
    pub controller_mode: CameraControllerMode,
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
    #[serde(default = "default_camera_position")]
    pub free_position: [f32; 3],
    #[serde(default = "default_camera_move_speed")]
    pub move_speed: f32,
    #[serde(default = "default_camera_look_speed")]
    pub look_speed: f32,
}

fn default_camera_position() -> [f32; 3] {
    [0.0, 1.8, 6.0]
}

fn default_camera_move_speed() -> f32 {
    5.5
}

fn default_camera_look_speed() -> f32 {
    1.35
}

#[derive(Clone, Copy, Debug, Default, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum CameraControllerMode {
    #[default]
    Orbit,
    Fly,
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
pub struct SceneInstanceDefinition {
    pub id: String,
    pub display_list_id: String,
    #[serde(default)]
    pub transform: SceneTransformDefinition,
    #[serde(default)]
    pub state_binding: Option<String>,
    #[serde(default)]
    pub shader_override_id: Option<String>,
}

#[derive(Clone, Copy, Debug, Default, Deserialize, Serialize)]
pub struct SceneTransformDefinition {
    #[serde(default)]
    pub translation: [f32; 3],
    #[serde(default)]
    pub rotation_degrees: [f32; 3],
    #[serde(default = "default_scene_scale")]
    pub scale: [f32; 3],
}

fn default_scene_scale() -> [f32; 3] {
    [1.0, 1.0, 1.0]
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
    GeneratedNamedTile {
        width: u32,
        height: u32,
        label: String,
        color_a: [u8; 4],
        color_b: [u8; 4],
    },
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct ShaderOverrideDefinition {
    pub id: String,
    #[serde(default)]
    pub combine_mode: Option<CombineMode>,
    #[serde(default)]
    pub primitive_color: Option<[u8; 4]>,
    #[serde(default)]
    pub env_color: Option<[u8; 4]>,
    #[serde(default = "default_shader_color_multiplier")]
    pub color_multiplier: [f32; 4],
    #[serde(default)]
    pub emissive_add: [f32; 4],
}

fn default_shader_color_multiplier() -> [f32; 4] {
    [1.0, 1.0, 1.0, 1.0]
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct DisplayListDefinition {
    pub id: String,
    pub commands: Vec<DisplayListCommand>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(tag = "op", rename_all = "snake_case")]
pub enum DisplayListCommand {
    PushMatrix {
        matrix: [[f32; 4]; 4],
    },
    PopMatrix,
    LoadVertices {
        slot: u16,
        vertices: Vec<Fast3dVertex>,
    },
    DrawTriangles {
        triangles: Vec<[u16; 3]>,
    },
    BindTexture {
        texture_id: String,
    },
    BindTextureSegment {
        segment_id: u8,
    },
    CallDisplayList {
        display_list_id: String,
    },
    CallDisplayListSegment {
        segment_id: u8,
    },
    SetCombineMode {
        mode: CombineMode,
    },
    SetPrimitiveColor {
        color: [u8; 4],
    },
    SetEnvColor {
        color: [u8; 4],
    },
    SetLightGroup {
        light_group_id: String,
    },
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
