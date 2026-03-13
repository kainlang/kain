mod math;
mod renderer;
mod scene;

pub use math::{ColorRgb, Mat4, Transform, Vec3};
pub use renderer::{
    RenderError, RenderFrame, RenderResolution, RenderStats, SoftwareRenderer,
    SoftwareRendererConfig,
};
pub use scene::{
    BackgroundGradient, Camera, DirectionalLight, LightingRig, Material, Mesh, PointLight,
    SceneAnimation, SceneCatalog, SceneDescription, SceneInstance, Vertex,
};
