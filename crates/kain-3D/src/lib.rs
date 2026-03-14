mod math;
mod renderer;
mod scene;

pub use math::{ColorRgb, Mat4, Transform, Vec3};
pub use renderer::{
    RenderError, RenderFrame, RenderResolution, RenderStats, RenderViewSettings, SoftwareRenderer,
    SoftwareRendererConfig,
};
pub use scene::{
    BackgroundGradient, BlackHole, Camera, CameraPose, DirectionalLight, LightingRig, Material,
    Mesh, ParticleEmitter, PointLight, SceneAnimation, SceneCatalog, SceneDescription,
    SceneInstance, Vertex,
};
