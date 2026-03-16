mod authoring;
mod host;
mod math;
mod prelude;
mod renderer;
mod scene;

pub use authoring::{
    AttributeDomain, AttributeValues, Brush, BrushFalloff, Effector, Field, Geometry,
    GeometryAttribute, GeometryError, GeometryTopology, InstancePattern, Instancer, Light, MeshNode,
    Modifier, Node, NodeId, NodeKind, Scene, SceneBuildError, Spline, SplineType, ToolContext,
    Volume,
};
pub use host::{install_runtime_natives, Kain3dSession, KAIN_3D_MODULE_NAME};
pub use math::{ColorRgb, Mat4, Transform, Vec2, Vec3};
pub use prelude::{emit_kain_prelude, reflected_type_registry};
pub use renderer::{
    RenderError, RenderFrame, RenderResolution, RenderStats, RenderViewSettings, SoftwareRenderer,
    SoftwareRendererConfig,
};
pub use scene::{
    BackgroundGradient, BlackHole, Camera, CameraPose, DirectionalLight, LightingRig, Material,
    Mesh, ParticleEmitter, PointLight, SceneAnimation, SceneCatalog, SceneDescription,
    SceneInstance, Vertex,
};
