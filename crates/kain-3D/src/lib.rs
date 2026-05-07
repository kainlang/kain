mod authoring;
mod host;
mod interaction;
mod math;
mod prelude;
mod primitive;
mod renderer;
mod scene;
mod shader_bundle;
mod wgpu_renderer;

pub use authoring::{
    AttributeDomain, AttributeValues, Brush, BrushFalloff, Effector, Field, Geometry,
    GeometryAttribute, GeometryError, GeometryTopology, InstancePattern, Instancer, Light,
    MeshNode, Modifier, Node, NodeId, NodeKind, Scene, SceneBuildError, Spline, SplineType,
    ToolContext, Volume,
};
pub use host::{install_runtime_natives, Kain3dSession, KAIN_3D_MODULE_NAME};
pub use interaction::{
    apply_manipulator_drag, CpuPickingService, ManipulatorAxis, ManipulatorDelta, ManipulatorMode,
    ManipulatorSnapSettings, ManipulatorSpace, ManipulatorState, PickTargetId, PickingHit,
    PickingQuery, PickingRay, PickingService, SceneCommand, SceneTransaction,
};
pub use math::{ColorRgb, Mat4, Transform, Vec2, Vec3};
pub use prelude::{emit_kain_prelude, reflected_type_registry};
pub use primitive::{PrimitiveDefinition, PrimitiveLibrary, PrimitiveShape};
pub use renderer::{
    FrameCameraSource, FrameDiagnostics, RenderBackend, RenderError, RenderFrame, RenderResolution,
    RenderStats, RenderViewSettings, SoftwareRenderer, SoftwareRendererConfig,
};
pub use scene::{
    BackgroundGradient, BlackHole, Camera, CameraPose, DirectionalLight, LightingRig, Material,
    Mesh, ParticleEmitter, PointLight, SceneAnimation, SceneBounds, SceneCatalog,
    SceneCatalogEntry, SceneCatalogSummary, SceneCompositionSummary, SceneDescription,
    SceneInstance, SceneResolution, SceneResolutionKind, TerrainSurface, Vertex,
};
pub use shader_bundle::{
    default_viewport_shader_bundle, wgsl_module_source, VIEWPORT_SHADER_MODULE_NAME,
    VIEWPORT_SHADER_SOURCE_ORIGIN,
};
pub use wgpu_renderer::{
    prepare_wgpu_frame, GizmoVertex, GpuVertex, ParticleVertex, PreparedWgpuFrame, SceneUniforms,
    WgpuRenderer, WgpuRendererInitError,
};
