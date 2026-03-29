use std::{
    collections::{hash_map::DefaultHasher, BTreeMap, BTreeSet},
    fs::{self, OpenOptions},
    hash::{Hash, Hasher},
    io::Write,
    path::{Path, PathBuf},
    sync::{Arc, Mutex},
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

use ab_glyph::{point, Font, FontArc, ScaleFont};
use eframe::egui::{self, Align, Color32, Frame, Layout, RichText, Stroke, Vec2};
use egui_wgpu::{self, CallbackTrait, ScreenDescriptor};
use fdsm::{
    bezier::scanline::FillRule, generate::generate_mtsdf, render::correct_sign_mtsdf, shape::Shape,
    transform::Transform,
};
use fdsm_ttf_parser::{load_shape_from_face, ttf_parser::Face};
use image::RgbaImage;
use kain_3d::{
    default_viewport_shader_bundle, prepare_wgpu_frame, wgsl_module_source, CameraPose,
    CpuPickingService, GizmoVertex, GpuVertex, ManipulatorMode, ManipulatorSpace, ParticleVertex,
    PickingHit, PickingQuery, PickingRay, PreparedWgpuFrame, RenderBackend, RenderResolution,
    RenderStats, RenderViewSettings, SceneCatalog, SceneDescription, SceneUniforms,
    SoftwareRenderer, Transform as SceneTransform, Vec3, WgpuRenderer, VIEWPORT_SHADER_MODULE_NAME,
};
use kain_core::{
    build_ui_output_from_source, realtime_app_bundle_from_json, render_ui_output_debug,
    shader_artifact_bundle_from_json, CompiledMaterialDefinition, RealtimeAppBundle,
    RealtimeAssetBinding, RealtimeSceneBinding, RealtimeShaderBundleRef,
    RealtimeShaderCanvasBinding, RealtimeShaderCanvasFontAtlas,
    RealtimeShaderCanvasResourceBinding, RealtimeShaderCanvasTextRun,
    RealtimeViewportCameraBinding, RealtimeViewportGizmoBinding,
    RealtimeViewportPresentationBinding, ShaderArtifactBundle, ShaderEntryPoint,
    ShaderResourceLayout,
};
use kain_ui::{
    ui_resolve_theme_for_node, ui_runtime_bundle_from_json, ui_runtime_bundle_from_output,
    ui_runtime_bundle_to_json, ui_step_animation_runtime, validate_ui_runtime_bundle,
    UiBuildOutput, UiLayoutAlignment, UiLayoutKind, UiLength, UiLengthUnit, UiNode, UiNodeId,
    UiOverflowBehavior, UiPatch, UiResolvedTheme, UiRuntime, UiRuntimeBundle, UiRuntimeMetadata,
    UiRuntimeStepInput, UiStyleState, UiSurface, UiSurfaceCompositionMode, UiSurfaceKind,
    UiSurfaceRendererPreference, UiThemeRegistry, UiTree, UiValue, UiWidgetKind,
    UI_RUNTIME_BUNDLE_SCHEMA_VERSION,
};
use nalgebra::{Affine2, Similarity2, Vector2};
use wgpu::util::DeviceExt;

pub const KAIN_UI_NATIVE_RUNTIME_BUNDLE_SCHEMA_VERSION: u32 = UI_RUNTIME_BUNDLE_SCHEMA_VERSION;
pub type KainUiNativeRuntimeMetadata = UiRuntimeMetadata;
pub type KainUiNativeRuntimeBundle = UiRuntimeBundle;

const DEFAULT_REPAINT_INTERVAL_MS: u64 = 33;
const DEFAULT_VIEWPORT_RENDER_INTERVAL_IDLE_MS: u64 = 180;
const DEFAULT_VIEWPORT_RENDER_INTERVAL_INTERACTIVE_MS: u64 = 66;
const DEFAULT_VIEWPORT_STARTUP_DELAY_MS: u64 = 350;
const DEFAULT_VIEWPORT_MAX_AXIS_PX: u64 = 640;
const KAIN_UI_NATIVE_RUNTIME_BUNDLE_ENV: &str = "KAIN_UI_NATIVE_RUNTIME_BUNDLE";
const KAIN_UI_NATIVE_REALTIME_BUNDLE_ENV: &str = "KAIN_UI_NATIVE_REALTIME_BUNDLE";
const KAIN_UI_NATIVE_SHADER_BUNDLE_ENV: &str = "KAIN_UI_NATIVE_SHADER_BUNDLE";
const KAIN_UI_NATIVE_APP_MANIFEST_ENV: &str = "KAIN_UI_NATIVE_APP_MANIFEST";
const KAIN_UI_NATIVE_APP_SNAPSHOT_ENV: &str = "KAIN_UI_NATIVE_APP_SNAPSHOT";
const KAIN_UI_NATIVE_COMMAND_BRIDGE_ENV: &str = "KAIN_UI_NATIVE_COMMAND_BRIDGE";
const UI_SHADER_SURFACE_VERTEX_ENTRY: &str = "kain_ui_surface_vs_main";
const SHADER_CANVAS_BITMAP_FONT_FAMILY: &str = "kain.builtin.bitmap_5x7";
const SHADER_CANVAS_DEFAULT_FONT_FAMILY: &str = "kain.default-ui-sans";
const UI_SHADER_SURFACE_FULLSCREEN_VERTEX_WGSL: &str = r#"

struct KainUiSurfaceVertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn kain_ui_surface_vs_main(@builtin(vertex_index) vertex_index: u32) -> KainUiSurfaceVertexOut {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>(-1.0, 1.0),
        vec2<f32>(3.0, 1.0),
    );
    var uvs = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 2.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(2.0, 0.0),
    );
    var output: KainUiSurfaceVertexOut;
    output.position = vec4<f32>(positions[vertex_index], 0.0, 1.0);
    output.uv = uvs[vertex_index];
    return output;
}
"#;

struct ShaderCanvasFontFamilySpec {
    aliases: &'static [&'static str],
    path_candidates: &'static [&'static str],
}

const SHADER_CANVAS_FONT_FAMILY_SPECS: &[ShaderCanvasFontFamilySpec] = &[
    ShaderCanvasFontFamilySpec {
        aliases: &[
            SHADER_CANVAS_DEFAULT_FONT_FAMILY,
            "system-ui",
            "ui-sans",
            "sans",
            "sans-serif",
            "segoe ui",
            "arial",
            "dejavu sans",
            "noto sans",
            "liberation sans",
        ],
        path_candidates: &[
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\calibri.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Supplemental/Helvetica.ttc",
        ],
    },
    ShaderCanvasFontFamilySpec {
        aliases: &[
            "kain.default-ui-mono",
            "ui-mono",
            "mono",
            "monospace",
            "consolas",
            "cascadia mono",
            "dejavu sans mono",
            "liberation mono",
        ],
        path_candidates: &[
            "C:\\Windows\\Fonts\\consola.ttf",
            "C:\\Windows\\Fonts\\CascadiaMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
            "/System/Library/Fonts/SFNSMono.ttf",
            "/System/Library/Fonts/Supplemental/Menlo.ttc",
        ],
    },
];

const SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA: &str = "bitmap-alpha";
const SHADER_CANVAS_FONT_ATLAS_ENCODING_ALPHA_COVERAGE: &str = "alpha-coverage";
const SHADER_CANVAS_FONT_ATLAS_ENCODING_MTSDF_RGBA: &str = "msdf-rgba";
const SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_UNKNOWN: u32 = 0;
const SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_BITMAP_ALPHA: u32 = 1;
const SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_ALPHA_COVERAGE: u32 = 2;
const SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_MTSDF_RGBA: u32 = 3;

#[derive(Clone)]
struct LoadedShaderCanvasFont {
    font: FontArc,
    ttf_bytes: Arc<[u8]>,
}

#[derive(Clone, Debug)]
struct UiShaderSurfaceTextureAtlasPayload {
    origin_px: [u32; 2],
    texture_size_px: [u32; 2],
    encoding_tag: u32,
    distance_range_px: u32,
}

#[derive(Clone, Debug)]
struct RasterizedShaderCanvasFontAtlas {
    texture_size_px: [u32; 2],
    encoding: String,
    distance_range_px: u32,
    rgba8: Vec<u8>,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct UiShaderSurfaceUniforms {
    resolution: [f32; 2],
    pointer: [f32; 2],
    time_seconds: f32,
    opacity: f32,
    frame_index: f32,
    aspect_ratio: f32,
    _pad: [f32; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct UiShaderSurfaceStoragePayload {
    resolution: [f32; 2],
    pointer_uv: [f32; 2],
    pointer_px: [f32; 2],
    surface_origin_px: [f32; 2],
    time_seconds: f32,
    frame_index: f32,
    opacity: f32,
    aspect_ratio: f32,
    node_id_low: u32,
    node_id_high: u32,
    _flags: [u32; 2],
    accent_rgba: [f32; 4],
    fill_rgba: [f32; 4],
    text_run_count: u32,
    font_atlas_count: u32,
    resource_binding_count: u32,
    atlas_glyph_bytes: u32,
    text_utf8_bytes: u32,
    surface_texture_width: u32,
    surface_texture_height: u32,
    _reserved: [u32; 1],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct UiShaderSurfaceFontAtlasRecord {
    atlas_origin_px: [u32; 2],
    texture_size_px: [u32; 2],
    cell_size_px: [u32; 2],
    columns: u32,
    rows: u32,
    encoding_tag: u32,
    distance_range_px: u32,
    glyph_start: u32,
    glyph_count: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
struct UiShaderSurfaceTextRunRecord {
    origin_px: [f32; 2],
    atlas_index: u32,
    text_start: u32,
    text_len: u32,
    role_tag: u32,
    _pad0: [u32; 3],
    color_rgba: [f32; 4],
}

#[derive(Clone, Debug)]
struct UiShaderSurfaceTexturePayload {
    size_px: [u32; 2],
    atlases: Vec<UiShaderSurfaceTextureAtlasPayload>,
    rgba8: Arc<[u8]>,
}

#[derive(Clone, Debug)]
struct WatchedRuntimeFile {
    path: String,
    last_modified: Option<SystemTime>,
}

#[derive(Clone, Debug, Default)]
struct RuntimeArtifactWatch {
    runtime_bundle: Option<WatchedRuntimeFile>,
    realtime_bundle: Option<WatchedRuntimeFile>,
    shader_bundle: Option<WatchedRuntimeFile>,
    runtime_snapshot: Option<WatchedRuntimeFile>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeSnapshot {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    panels: Vec<NativeAppRuntimePanel>,
    commands: Vec<NativeAppRuntimeCommand>,
    providers: Vec<NativeAppRuntimeProvider>,
    tools: Vec<NativeAppRuntimeTool>,
    sessions: NativeAppRuntimeSessions,
    recent_sessions: Vec<NativeAppRuntimeRecentSession>,
    workspaces: Vec<NativeAppRuntimeWorkspace>,
    updated_at: String,
    #[serde(default)]
    dcc_suite_state: Option<NativeAppRuntimeDccSuiteState>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimePanel {
    id: String,
    title: String,
    dock: String,
    kind: String,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeCommand {
    id: String,
    label: String,
    surface: String,
    intent: String,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeProvider {
    id: String,
    label: String,
    transport: String,
    profile_kind: String,
    supports_tools: bool,
    supports_streaming: bool,
    active: bool,
    profile_configured: bool,
    profile_keys: Vec<String>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeTool {
    id: String,
    label: String,
    capability: String,
    approval: String,
    decision: Option<String>,
    scope_decisions: Vec<NativeAppRuntimeToolScopeDecision>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeSessions {
    total_sessions: usize,
    active_provider: String,
    recent_session_id: Option<String>,
    recent_session_title: Option<String>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeToolScopeDecision {
    scope: String,
    decision: String,
    updated_at: String,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeRecentSession {
    id: String,
    title: String,
    provider_id: String,
    status: String,
    workspace_root: Option<String>,
    updated_at: String,
    message_count: usize,
    last_message_role: Option<String>,
    last_message_preview: Option<String>,
}

#[derive(Clone, Debug, serde::Deserialize)]
struct NativeAppRuntimeWorkspace {
    root: String,
    session_count: usize,
    recent_session_title: Option<String>,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccSuiteState {
    #[serde(default)]
    session: NativeAppRuntimeDccSuiteSession,
    #[serde(default)]
    derived: NativeAppRuntimeDccSuiteDerivedState,
    #[serde(default)]
    latest_command: Option<NativeAppRuntimeBridgeCommandRecord>,
    #[serde(default)]
    bridge: NativeAppRuntimeBridgeStatus,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccSuiteSession {
    #[serde(default)]
    workspace: NativeAppRuntimeDccWorkspaceState,
    #[serde(default)]
    tooling: NativeAppRuntimeDccToolingState,
    #[serde(default)]
    gizmo: NativeAppRuntimeDccGizmoState,
    #[serde(default)]
    animation: NativeAppRuntimeDccAnimationState,
    #[serde(default)]
    dirty: NativeAppRuntimeDccDirtyState,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccWorkspaceState {
    #[serde(default)]
    active_mode: String,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccToolingState {
    #[serde(default)]
    active_tool: String,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccGizmoState {
    #[serde(default)]
    mode: String,
    #[serde(default)]
    space: String,
    #[serde(default)]
    snap_enabled: bool,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccAnimationState {
    #[serde(default)]
    frame: i64,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccDirtyState {
    #[serde(default)]
    asset_dirty: bool,
    #[serde(default)]
    sculpt_dirty: bool,
    #[serde(default)]
    topology_dirty: bool,
    #[serde(default)]
    material_dirty: bool,
    #[serde(default)]
    rig_dirty: bool,
    #[serde(default)]
    animation_dirty: bool,
    #[serde(default)]
    simulation_dirty: bool,
    #[serde(default)]
    render_dirty: bool,
    #[serde(default)]
    compositor_dirty: bool,
    #[serde(default)]
    publish_dirty: bool,
    #[serde(default)]
    tensor_dirty: bool,
    #[serde(default)]
    session_needs_save: bool,
}

impl NativeAppRuntimeDccDirtyState {
    fn active_count(&self) -> usize {
        [
            self.asset_dirty,
            self.sculpt_dirty,
            self.topology_dirty,
            self.material_dirty,
            self.rig_dirty,
            self.animation_dirty,
            self.simulation_dirty,
            self.render_dirty,
            self.compositor_dirty,
            self.publish_dirty,
            self.tensor_dirty,
            self.session_needs_save,
        ]
        .into_iter()
        .filter(|value| *value)
        .count()
    }
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeDccSuiteDerivedState {
    #[serde(default)]
    active_mode_label: String,
    #[serde(default)]
    active_tool_label: String,
    #[serde(default)]
    gizmo_summary: String,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeBridgeStatus {
    #[serde(default)]
    status: String,
    #[serde(default)]
    command_queue_path: String,
    #[serde(default)]
    processed_command_count: usize,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct NativeAppRuntimeBridgeCommandRecord {
    #[serde(default)]
    command_id: String,
    #[serde(default)]
    label: String,
    #[serde(default)]
    requested_at: String,
}

pub const KAIN_UI_NATIVE_DEMO_SOURCE: &str = r#"
component App():
    render <panel title="Kain UI Retirement Demo" layout="dock" gap={12} padding={12}>
        <panel title="Workspace Shell" layout="row" gap={12}>
            <inspector title="Selection">
                Active document: kain-ui
            </inspector>
            <tree title="Project Tree">
                crates/
                kain-core/
                kain-ui/
                kain-ui-native/
            </tree>
        </panel>
        <graph title="Material Graph" />
        <timeline title="Sequencer" />
        <viewport3d title="Luminous Port Viewport" scene="luminous_port" />
    </panel>
"#;

#[derive(Clone, Debug)]
pub struct KainUiNativeAppConfig {
    pub window_title: String,
    pub root_component: String,
    pub source: String,
    pub initial_window_size: [f32; 2],
}

pub type KainUiNativeDemoConfig = KainUiNativeAppConfig;

fn trace_enabled() -> bool {
    std::env::var("KAIN_UI_NATIVE_TRACE")
        .map(|value| value == "1" || value.eq_ignore_ascii_case("true"))
        .unwrap_or(false)
}

fn trace_path() -> PathBuf {
    std::env::temp_dir().join("kain-ui-native-trace.log")
}

fn trace_runtime(message: impl AsRef<str>) {
    if !trace_enabled() {
        return;
    }

    let line = format!("{}\n", message.as_ref());
    if let Ok(mut file) = OpenOptions::new()
        .create(true)
        .append(true)
        .open(trace_path())
    {
        let _ = file.write_all(line.as_bytes());
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum NativeRendererPreference {
    Glow,
    Wgpu,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct KainUiNativeRuntimeSettings {
    renderer: NativeRendererPreference,
    show_runtime_inspector: bool,
    enable_viewports: bool,
    repaint_interval_ms: u64,
    viewport_render_interval_idle_ms: u64,
    viewport_render_interval_interactive_ms: u64,
    viewport_startup_delay_ms: u64,
    viewport_max_axis_px: u64,
}

impl Default for KainUiNativeRuntimeSettings {
    fn default() -> Self {
        Self {
            renderer: NativeRendererPreference::Wgpu,
            show_runtime_inspector: false,
            enable_viewports: true,
            repaint_interval_ms: DEFAULT_REPAINT_INTERVAL_MS,
            viewport_render_interval_idle_ms: DEFAULT_VIEWPORT_RENDER_INTERVAL_IDLE_MS,
            viewport_render_interval_interactive_ms:
                DEFAULT_VIEWPORT_RENDER_INTERVAL_INTERACTIVE_MS,
            viewport_startup_delay_ms: DEFAULT_VIEWPORT_STARTUP_DELAY_MS,
            viewport_max_axis_px: DEFAULT_VIEWPORT_MAX_AXIS_PX,
        }
    }
}

impl KainUiNativeRuntimeSettings {
    fn from_env() -> Self {
        let mut settings = Self::default();
        if let Some(renderer) = env_var_trimmed("KAIN_UI_NATIVE_VIEWPORT_RENDERER")
            .or_else(|| env_var_trimmed("KAIN_UI_NATIVE_RENDERER"))
        {
            settings.renderer = parse_renderer_preference(&renderer);
        }
        if let Some(value) = env_bool("KAIN_UI_NATIVE_SHOW_INSPECTOR") {
            settings.show_runtime_inspector = value;
        }
        if let Some(value) = env_bool("KAIN_UI_NATIVE_ENABLE_VIEWPORTS") {
            settings.enable_viewports = value;
        }
        if let Some(value) = env_u64("KAIN_UI_NATIVE_REPAINT_MS") {
            settings.repaint_interval_ms = value.max(1);
        }
        if let Some(value) = env_u64("KAIN_UI_NATIVE_VIEWPORT_IDLE_MS") {
            settings.viewport_render_interval_idle_ms = value.max(1);
        }
        if let Some(value) = env_u64("KAIN_UI_NATIVE_VIEWPORT_INTERACTIVE_MS") {
            settings.viewport_render_interval_interactive_ms = value.max(1);
        }
        if let Some(value) = env_u64("KAIN_UI_NATIVE_VIEWPORT_STARTUP_MS") {
            settings.viewport_startup_delay_ms = value;
        }
        if let Some(value) = env_u64("KAIN_UI_NATIVE_VIEWPORT_MAX_AXIS") {
            settings.viewport_max_axis_px = value.max(128);
        }
        settings
    }

    fn eframe_renderer(self) -> eframe::Renderer {
        match self.renderer {
            NativeRendererPreference::Glow => eframe::Renderer::Glow,
            NativeRendererPreference::Wgpu => eframe::Renderer::Wgpu,
        }
    }

    fn renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "software",
            NativeRendererPreference::Wgpu => "wgpu-readback",
        }
    }

    fn effective_renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "software",
            NativeRendererPreference::Wgpu => "wgpu-readback-pending-app-init",
        }
    }
}

fn parse_renderer_preference(value: &str) -> NativeRendererPreference {
    match value.to_ascii_lowercase().as_str() {
        "wgpu" | "wgpu-readback" | "wgpu-surface" => NativeRendererPreference::Wgpu,
        _ => NativeRendererPreference::Glow,
    }
}

fn env_var_trimmed(key: &str) -> Option<String> {
    std::env::var(key)
        .ok()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
}

fn env_bool(key: &str) -> Option<bool> {
    env_var_trimmed(key).and_then(|value| match value.to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Some(true),
        "0" | "false" | "no" | "off" => Some(false),
        _ => None,
    })
}

fn env_u64(key: &str) -> Option<u64> {
    env_var_trimmed(key).and_then(|value| value.parse::<u64>().ok())
}

fn load_json_string_from_env(key: &str) -> Option<(String, String)> {
    let path = env_var_trimmed(key)?;
    let json = fs::read_to_string(&path).ok()?;
    Some((json, path))
}

fn load_runtime_bundle_from_env() -> Option<(UiRuntimeBundle, String)> {
    let (json, path) = load_json_string_from_env(KAIN_UI_NATIVE_RUNTIME_BUNDLE_ENV)?;
    ui_runtime_bundle_from_json(&json)
        .ok()
        .map(|bundle| (bundle, path))
}

fn load_realtime_bundle_from_env() -> Option<(RealtimeAppBundle, String)> {
    let (json, path) = load_json_string_from_env(KAIN_UI_NATIVE_REALTIME_BUNDLE_ENV)?;
    realtime_app_bundle_from_json(&json)
        .ok()
        .map(|bundle| (bundle, path))
}

fn load_shader_bundle_from_env() -> Option<(ShaderArtifactBundle, String)> {
    let (json, path) = load_json_string_from_env(KAIN_UI_NATIVE_SHADER_BUNDLE_ENV)?;
    shader_artifact_bundle_from_json(&json)
        .ok()
        .map(|bundle| (bundle, path))
}

fn load_runtime_snapshot_from_env() -> Option<(NativeAppRuntimeSnapshot, String)> {
    let (json, path) = load_json_string_from_env(KAIN_UI_NATIVE_APP_SNAPSHOT_ENV)?;
    serde_json::from_str(&json)
        .ok()
        .map(|snapshot| (snapshot, path))
}

fn bridge_timestamp_string() -> String {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
        .to_string()
}

fn emit_runtime_command_to_bridge(
    bridge_path: &str,
    command: &NativeAppRuntimeCommand,
) -> Result<(), String> {
    let payload = serde_json::json!({
        "command_id": command.id.clone(),
        "label": command.label.clone(),
        "intent": command.intent.clone(),
        "surface": command.surface.clone(),
        "source": "kain-ui-native",
        "requested_at": bridge_timestamp_string(),
    });
    let line = serde_json::to_string(&payload).map_err(|err| err.to_string())?;
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(bridge_path)
        .map_err(|err| err.to_string())?;
    writeln!(file, "{line}").map_err(|err| err.to_string())
}

fn file_modified_at(path: &str) -> Option<SystemTime> {
    fs::metadata(path).ok()?.modified().ok()
}

impl WatchedRuntimeFile {
    fn new(path: String) -> Self {
        Self {
            last_modified: file_modified_at(&path),
            path,
        }
    }

    fn take_if_changed(&mut self) -> Option<String> {
        let modified = file_modified_at(&self.path)?;
        if self
            .last_modified
            .is_some_and(|previous| previous >= modified)
        {
            return None;
        }
        self.last_modified = Some(modified);
        Some(self.path.clone())
    }
}

impl RuntimeArtifactWatch {
    fn with_runtime_bundle_path(mut self, path: Option<String>) -> Self {
        self.runtime_bundle = path.map(WatchedRuntimeFile::new);
        self
    }

    fn with_realtime_bundle_path(mut self, path: Option<String>) -> Self {
        self.realtime_bundle = path.map(WatchedRuntimeFile::new);
        self
    }

    fn with_shader_bundle_path(mut self, path: Option<String>) -> Self {
        self.shader_bundle = path.map(WatchedRuntimeFile::new);
        self
    }

    fn with_runtime_snapshot_path(mut self, path: Option<String>) -> Self {
        self.runtime_snapshot = path.map(WatchedRuntimeFile::new);
        self
    }
}

impl RealtimeBundleCatalog {
    fn from_bundle(bundle: &RealtimeAppBundle, bundle_path: Option<&str>) -> Self {
        Self {
            scenes_by_viewport: bundle
                .render
                .scenes
                .iter()
                .cloned()
                .map(|scene| (scene.viewport_node.clone(), scene))
                .collect(),
            materials_by_id: bundle
                .render
                .materials
                .iter()
                .cloned()
                .map(|material| (material.id.clone(), material))
                .collect(),
            shader_refs_by_key: bundle
                .shader_bundle_refs
                .iter()
                .cloned()
                .map(|shader_ref| (shader_ref.key.clone(), shader_ref))
                .collect(),
            shader_canvases_by_surface: bundle
                .shader_canvases
                .iter()
                .cloned()
                .map(|binding| (binding.surface_id.clone(), binding))
                .collect(),
            assets_by_key: bundle
                .assets
                .iter()
                .cloned()
                .map(|asset| (asset.key.clone(), asset))
                .collect(),
            asset_base_dir: bundle_path
                .map(PathBuf::from)
                .and_then(|path| path.parent().map(Path::to_path_buf)),
        }
    }
}

fn color_bg_top() -> Color32 {
    Color32::from_rgb(8, 12, 18)
}

fn color_bg_bottom() -> Color32 {
    Color32::from_rgb(16, 22, 31)
}

fn color_surface() -> Color32 {
    Color32::from_rgb(18, 24, 32)
}

fn color_surface_alt() -> Color32 {
    Color32::from_rgb(24, 31, 41)
}

fn color_surface_raised() -> Color32 {
    Color32::from_rgb(31, 39, 51)
}

fn color_surface_overlay() -> Color32 {
    Color32::from_rgba_unmultiplied(7, 11, 17, 208)
}

fn color_outline_soft() -> Color32 {
    Color32::from_rgb(68, 89, 110)
}

fn color_outline_bright() -> Color32 {
    Color32::from_rgb(75, 198, 255)
}

fn color_accent() -> Color32 {
    Color32::from_rgb(81, 198, 255)
}

fn color_accent_soft() -> Color32 {
    Color32::from_rgb(143, 224, 255)
}

fn color_highlight() -> Color32 {
    Color32::from_rgb(255, 209, 102)
}

fn color_success() -> Color32 {
    Color32::from_rgb(135, 223, 153)
}

fn color_muted_text() -> Color32 {
    Color32::from_rgb(160, 171, 186)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum NativeSurfaceMode {
    Flat,
    Layered,
    Glass,
    Canvas,
    Accent,
    Ghost,
}

impl NativeSurfaceMode {
    fn from_str(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "flat" => Some(Self::Flat),
            "layered" | "elevated" => Some(Self::Layered),
            "glass" | "frosted" => Some(Self::Glass),
            "canvas" | "viewport" => Some(Self::Canvas),
            "accent" | "hero" => Some(Self::Accent),
            "ghost" | "outline" | "minimal" => Some(Self::Ghost),
            _ => None,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum NativeDensity {
    Compact,
    Cozy,
    Spacious,
}

impl NativeDensity {
    fn from_str(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "compact" | "dense" | "tight" => Some(Self::Compact),
            "spacious" | "airy" | "loose" => Some(Self::Spacious),
            "cozy" | "comfortable" | "default" => Some(Self::Cozy),
            _ => None,
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct NativeThemePalette {
    bg_top: Color32,
    bg_bottom: Color32,
    surface_base: Color32,
    surface_alt: Color32,
    surface_raised: Color32,
    surface_overlay: Color32,
    outline_soft: Color32,
    outline_bright: Color32,
    accent: Color32,
    accent_soft: Color32,
    highlight: Color32,
    success: Color32,
    text: Color32,
    text_muted: Color32,
}

impl Default for NativeThemePalette {
    fn default() -> Self {
        Self {
            bg_top: color_bg_top(),
            bg_bottom: color_bg_bottom(),
            surface_base: color_surface(),
            surface_alt: color_surface_alt(),
            surface_raised: color_surface_raised(),
            surface_overlay: color_surface_overlay(),
            outline_soft: color_outline_soft(),
            outline_bright: color_outline_bright(),
            accent: color_accent(),
            accent_soft: color_accent_soft(),
            highlight: color_highlight(),
            success: color_success(),
            text: Color32::from_rgb(239, 243, 248),
            text_muted: color_muted_text(),
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct NativeThemeMetrics {
    frame_padding: f32,
    tight_padding: f32,
    section_gap: f32,
    content_gap: f32,
    radius_small: f32,
    radius_medium: f32,
    radius_large: f32,
}

impl NativeThemeMetrics {
    fn from_density(density: NativeDensity, spacing_scale: f32, radius_scale: f32) -> Self {
        let spacing_scale = spacing_scale.clamp(0.6, 1.8);
        let radius_scale = radius_scale.clamp(0.5, 1.8);
        let (frame_padding, tight_padding, section_gap, content_gap) = match density {
            NativeDensity::Compact => (10.0, 8.0, 6.0, 8.0),
            NativeDensity::Cozy => (14.0, 10.0, 8.0, 12.0),
            NativeDensity::Spacious => (18.0, 14.0, 10.0, 16.0),
        };
        let (radius_small, radius_medium, radius_large) = match density {
            NativeDensity::Compact => (7.0, 11.0, 15.0),
            NativeDensity::Cozy => (9.0, 14.0, 18.0),
            NativeDensity::Spacious => (12.0, 18.0, 24.0),
        };

        Self {
            frame_padding: frame_padding * spacing_scale,
            tight_padding: tight_padding * spacing_scale,
            section_gap: section_gap * spacing_scale,
            content_gap: content_gap * spacing_scale,
            radius_small: radius_small * radius_scale,
            radius_medium: radius_medium * radius_scale,
            radius_large: radius_large * radius_scale,
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct NativeTypography {
    heading: f32,
    section: f32,
    body: f32,
    small: f32,
    code: f32,
}

impl NativeTypography {
    fn from_density(density: NativeDensity, type_scale: f32) -> Self {
        let type_scale = type_scale.clamp(0.75, 1.7);
        let (heading, section, body, small, code) = match density {
            NativeDensity::Compact => (20.0, 16.0, 13.5, 11.0, 10.5),
            NativeDensity::Cozy => (22.0, 17.5, 14.5, 11.5, 11.0),
            NativeDensity::Spacious => (25.0, 19.5, 15.5, 12.5, 11.5),
        };
        Self {
            heading: heading * type_scale,
            section: section * type_scale,
            body: body * type_scale,
            small: small * type_scale,
            code: code * type_scale,
        }
    }
}

#[derive(Clone, Debug)]
struct NativeAppTheme {
    name: String,
    density: NativeDensity,
    palette: NativeThemePalette,
    metrics: NativeThemeMetrics,
    typography: NativeTypography,
    chrome_mode: NativeSurfaceMode,
    panel_mode: NativeSurfaceMode,
    inspector_mode: NativeSurfaceMode,
    tree_mode: NativeSurfaceMode,
    graph_mode: NativeSurfaceMode,
    timeline_mode: NativeSurfaceMode,
    viewport_mode: NativeSurfaceMode,
    element_mode: NativeSurfaceMode,
    global_values: BTreeMap<String, UiValue>,
}

impl Default for NativeAppTheme {
    fn default() -> Self {
        let density = NativeDensity::Cozy;
        Self {
            name: "default".to_string(),
            density,
            palette: NativeThemePalette::default(),
            metrics: NativeThemeMetrics::from_density(density, 1.0, 1.0),
            typography: NativeTypography::from_density(density, 1.0),
            chrome_mode: NativeSurfaceMode::Glass,
            panel_mode: NativeSurfaceMode::Layered,
            inspector_mode: NativeSurfaceMode::Glass,
            tree_mode: NativeSurfaceMode::Ghost,
            graph_mode: NativeSurfaceMode::Canvas,
            timeline_mode: NativeSurfaceMode::Accent,
            viewport_mode: NativeSurfaceMode::Canvas,
            element_mode: NativeSurfaceMode::Ghost,
            global_values: BTreeMap::new(),
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct NativeWidgetTheme {
    mode: NativeSurfaceMode,
    fill: Color32,
    stroke: Color32,
    canvas_fill: Color32,
    overlay_fill: Color32,
    header_fill: Color32,
    header_stroke: Color32,
    badge_fill: Color32,
    accent: Color32,
    title_color: Color32,
    body_color: Color32,
    muted_color: Color32,
    tag_color: Color32,
    radius: f32,
    padding: f32,
    gap: f32,
    title_size: f32,
    body_size: f32,
    tag_size: f32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum NativeTextRole {
    Body,
    Hero,
    Title,
    Eyebrow,
    Caption,
    Muted,
    Code,
    Metric,
}

impl NativeWidgetTheme {
    fn from_kind(kind: &UiWidgetKind, app_theme: &NativeAppTheme) -> Self {
        let mode = match kind {
            UiWidgetKind::Panel => app_theme.panel_mode,
            UiWidgetKind::Inspector => app_theme.inspector_mode,
            UiWidgetKind::Tree => app_theme.tree_mode,
            UiWidgetKind::Graph => app_theme.graph_mode,
            UiWidgetKind::Timeline => app_theme.timeline_mode,
            UiWidgetKind::Viewport2D | UiWidgetKind::Viewport3D => app_theme.viewport_mode,
            UiWidgetKind::Element(_) => app_theme.element_mode,
            _ => NativeSurfaceMode::Flat,
        };

        let (fill, stroke, canvas_fill, overlay_fill) =
            surface_colors_for_mode(mode, &app_theme.palette);
        let (radius, padding) = match kind {
            UiWidgetKind::Panel => (
                app_theme.metrics.radius_large,
                app_theme.metrics.frame_padding,
            ),
            UiWidgetKind::Graph | UiWidgetKind::Viewport2D | UiWidgetKind::Viewport3D => (
                app_theme.metrics.radius_large,
                app_theme.metrics.tight_padding,
            ),
            UiWidgetKind::Timeline => (
                app_theme.metrics.radius_medium,
                app_theme.metrics.tight_padding,
            ),
            UiWidgetKind::Inspector | UiWidgetKind::Tree => (
                app_theme.metrics.radius_medium,
                app_theme.metrics.tight_padding,
            ),
            _ => (
                app_theme.metrics.radius_small,
                app_theme.metrics.tight_padding,
            ),
        };

        let title_color = match kind {
            UiWidgetKind::Timeline => app_theme.palette.highlight,
            UiWidgetKind::Graph | UiWidgetKind::Tree => app_theme.palette.accent_soft,
            _ => app_theme.palette.text,
        };

        Self {
            mode,
            fill,
            stroke,
            canvas_fill,
            overlay_fill,
            header_fill: alpha_tint(overlay_fill, 0.94),
            header_stroke: alpha_tint(stroke, 0.86),
            badge_fill: alpha_tint(fill, 0.76),
            accent: app_theme.palette.accent,
            title_color,
            body_color: app_theme.palette.text,
            muted_color: app_theme.palette.text_muted,
            tag_color: app_theme.palette.accent_soft,
            radius,
            padding,
            gap: app_theme.metrics.content_gap,
            title_size: app_theme.typography.section,
            body_size: app_theme.typography.body,
            tag_size: app_theme.typography.small,
        }
    }

    fn chrome(app_theme: &NativeAppTheme) -> Self {
        let (fill, stroke, canvas_fill, overlay_fill) =
            surface_colors_for_mode(app_theme.chrome_mode, &app_theme.palette);
        Self {
            mode: app_theme.chrome_mode,
            fill,
            stroke,
            canvas_fill,
            overlay_fill,
            header_fill: alpha_tint(overlay_fill, 0.98),
            header_stroke: alpha_tint(stroke, 0.9),
            badge_fill: alpha_tint(fill, 0.72),
            accent: app_theme.palette.accent,
            title_color: app_theme.palette.text,
            body_color: app_theme.palette.text,
            muted_color: app_theme.palette.text_muted,
            tag_color: app_theme.palette.accent_soft,
            radius: app_theme.metrics.radius_medium,
            padding: app_theme.metrics.tight_padding,
            gap: app_theme.metrics.section_gap,
            title_size: app_theme.typography.section,
            body_size: app_theme.typography.body,
            tag_size: app_theme.typography.small,
        }
    }
}

fn alpha_tint(color: Color32, alpha_factor: f32) -> Color32 {
    let alpha = ((color.a() as f32) * alpha_factor.clamp(0.0, 1.0)).round() as u8;
    Color32::from_rgba_unmultiplied(color.r(), color.g(), color.b(), alpha)
}

fn surface_colors_for_mode(
    mode: NativeSurfaceMode,
    palette: &NativeThemePalette,
) -> (Color32, Color32, Color32, Color32) {
    match mode {
        NativeSurfaceMode::Flat => (
            palette.surface_base,
            palette.outline_soft,
            palette.bg_top,
            palette.surface_overlay,
        ),
        NativeSurfaceMode::Layered => (
            palette.surface_alt,
            palette.outline_soft,
            palette.bg_top,
            palette.surface_overlay,
        ),
        NativeSurfaceMode::Glass => (
            alpha_tint(palette.surface_raised, 0.82),
            alpha_tint(palette.accent_soft, 0.68),
            alpha_tint(palette.bg_top, 0.92),
            alpha_tint(palette.surface_overlay, 0.86),
        ),
        NativeSurfaceMode::Canvas => (
            palette.surface_base,
            palette.outline_bright,
            palette.bg_top,
            alpha_tint(palette.surface_overlay, 0.92),
        ),
        NativeSurfaceMode::Accent => (
            alpha_tint(palette.accent, 0.18),
            palette.accent_soft,
            alpha_tint(palette.bg_top, 0.94),
            alpha_tint(palette.surface_overlay, 0.88),
        ),
        NativeSurfaceMode::Ghost => (
            Color32::from_rgba_unmultiplied(0, 0, 0, 0),
            alpha_tint(palette.outline_soft, 0.8),
            alpha_tint(palette.bg_top, 0.88),
            alpha_tint(palette.surface_overlay, 0.72),
        ),
    }
}

fn native_theme_preset(name: &str) -> NativeAppTheme {
    let mut theme = NativeAppTheme::default();
    theme.name = name.to_string();
    match name.trim().to_ascii_lowercase().as_str() {
        "forge" => {
            theme.density = NativeDensity::Compact;
            theme.palette = NativeThemePalette {
                bg_top: Color32::from_rgb(18, 14, 12),
                bg_bottom: Color32::from_rgb(31, 24, 20),
                surface_base: Color32::from_rgb(39, 30, 25),
                surface_alt: Color32::from_rgb(53, 39, 31),
                surface_raised: Color32::from_rgb(68, 48, 35),
                surface_overlay: Color32::from_rgba_unmultiplied(13, 9, 7, 214),
                outline_soft: Color32::from_rgb(120, 89, 70),
                outline_bright: Color32::from_rgb(231, 154, 83),
                accent: Color32::from_rgb(233, 119, 51),
                accent_soft: Color32::from_rgb(255, 184, 123),
                highlight: Color32::from_rgb(255, 216, 137),
                success: Color32::from_rgb(162, 223, 141),
                text: Color32::from_rgb(247, 240, 233),
                text_muted: Color32::from_rgb(193, 175, 161),
            };
            theme.chrome_mode = NativeSurfaceMode::Accent;
            theme.panel_mode = NativeSurfaceMode::Layered;
            theme.inspector_mode = NativeSurfaceMode::Flat;
            theme.tree_mode = NativeSurfaceMode::Ghost;
            theme.graph_mode = NativeSurfaceMode::Canvas;
            theme.timeline_mode = NativeSurfaceMode::Accent;
            theme.viewport_mode = NativeSurfaceMode::Glass;
        }
        "signal" => {
            theme.density = NativeDensity::Cozy;
            theme.palette = NativeThemePalette {
                bg_top: Color32::from_rgb(10, 12, 24),
                bg_bottom: Color32::from_rgb(14, 18, 37),
                surface_base: Color32::from_rgb(18, 24, 43),
                surface_alt: Color32::from_rgb(24, 32, 56),
                surface_raised: Color32::from_rgb(33, 43, 72),
                surface_overlay: Color32::from_rgba_unmultiplied(8, 10, 24, 214),
                outline_soft: Color32::from_rgb(87, 99, 142),
                outline_bright: Color32::from_rgb(255, 118, 181),
                accent: Color32::from_rgb(255, 91, 168),
                accent_soft: Color32::from_rgb(255, 172, 214),
                highlight: Color32::from_rgb(255, 214, 120),
                success: Color32::from_rgb(130, 233, 201),
                text: Color32::from_rgb(244, 245, 253),
                text_muted: Color32::from_rgb(176, 184, 211),
            };
            theme.chrome_mode = NativeSurfaceMode::Glass;
            theme.panel_mode = NativeSurfaceMode::Glass;
            theme.inspector_mode = NativeSurfaceMode::Layered;
            theme.tree_mode = NativeSurfaceMode::Ghost;
            theme.graph_mode = NativeSurfaceMode::Accent;
            theme.timeline_mode = NativeSurfaceMode::Canvas;
            theme.viewport_mode = NativeSurfaceMode::Canvas;
        }
        "paper" => {
            theme.density = NativeDensity::Spacious;
            theme.palette = NativeThemePalette {
                bg_top: Color32::from_rgb(240, 234, 224),
                bg_bottom: Color32::from_rgb(226, 219, 209),
                surface_base: Color32::from_rgb(252, 248, 241),
                surface_alt: Color32::from_rgb(244, 238, 229),
                surface_raised: Color32::from_rgb(255, 252, 247),
                surface_overlay: Color32::from_rgba_unmultiplied(244, 238, 229, 228),
                outline_soft: Color32::from_rgb(167, 146, 121),
                outline_bright: Color32::from_rgb(84, 122, 184),
                accent: Color32::from_rgb(55, 98, 166),
                accent_soft: Color32::from_rgb(103, 142, 205),
                highlight: Color32::from_rgb(186, 127, 52),
                success: Color32::from_rgb(77, 142, 93),
                text: Color32::from_rgb(46, 40, 34),
                text_muted: Color32::from_rgb(103, 92, 81),
            };
            theme.chrome_mode = NativeSurfaceMode::Layered;
            theme.panel_mode = NativeSurfaceMode::Flat;
            theme.inspector_mode = NativeSurfaceMode::Ghost;
            theme.tree_mode = NativeSurfaceMode::Ghost;
            theme.graph_mode = NativeSurfaceMode::Canvas;
            theme.timeline_mode = NativeSurfaceMode::Accent;
            theme.viewport_mode = NativeSurfaceMode::Canvas;
        }
        "kade_desktop" | "kade-desktop" => {
            theme.density = NativeDensity::Spacious;
            theme.palette = NativeThemePalette {
                bg_top: Color32::from_rgb(9, 12, 18),
                bg_bottom: Color32::from_rgb(14, 18, 26),
                surface_base: Color32::from_rgb(20, 25, 35),
                surface_alt: Color32::from_rgb(27, 33, 46),
                surface_raised: Color32::from_rgb(34, 42, 58),
                surface_overlay: Color32::from_rgba_unmultiplied(11, 14, 20, 224),
                outline_soft: Color32::from_rgb(70, 78, 95),
                outline_bright: Color32::from_rgb(232, 171, 94),
                accent: Color32::from_rgb(223, 146, 74),
                accent_soft: Color32::from_rgb(245, 194, 135),
                highlight: Color32::from_rgb(120, 198, 181),
                success: Color32::from_rgb(126, 203, 145),
                text: Color32::from_rgb(242, 236, 226),
                text_muted: Color32::from_rgb(164, 160, 153),
            };
            theme.chrome_mode = NativeSurfaceMode::Layered;
            theme.panel_mode = NativeSurfaceMode::Glass;
            theme.inspector_mode = NativeSurfaceMode::Flat;
            theme.tree_mode = NativeSurfaceMode::Ghost;
            theme.graph_mode = NativeSurfaceMode::Canvas;
            theme.timeline_mode = NativeSurfaceMode::Accent;
            theme.viewport_mode = NativeSurfaceMode::Canvas;
            theme.element_mode = NativeSurfaceMode::Ghost;
        }
        _ => {}
    }
    theme.metrics = NativeThemeMetrics::from_density(theme.density, 1.0, 1.0);
    theme.typography = NativeTypography::from_density(theme.density, 1.0);
    theme
}

fn resolve_app_theme(output: &UiBuildOutput) -> NativeAppTheme {
    let root_resolved = output
        .tree
        .root
        .and_then(|root_id| output.tree.node(root_id))
        .map(|node| ui_resolve_theme_for_node(node, &output.systems.theme_registry))
        .unwrap_or_default();

    let theme_name = theme_lookup_string(
        None,
        &root_resolved.values,
        &["theme.name", "app.theme", "theme.active"],
    )
    .or_else(|| output.systems.theme_registry.active_theme.clone())
    .unwrap_or_else(|| "default".to_string());

    let mut theme = native_theme_preset(&theme_name);
    theme.global_values = root_resolved.values.clone();

    if let Some(density) = theme_lookup_string(
        None,
        &theme.global_values,
        &["theme.density", "layout.density", "density"],
    )
    .and_then(|value| NativeDensity::from_str(&value))
    {
        theme.density = density;
    }

    let spacing_scale = theme_lookup_f32(
        None,
        &theme.global_values,
        &["theme.spacing.scale", "spacing.scale"],
    )
    .unwrap_or(1.0);
    let radius_scale = theme_lookup_f32(
        None,
        &theme.global_values,
        &["theme.radius.scale", "radius.scale", "corner.scale"],
    )
    .unwrap_or(1.0);
    let type_scale = theme_lookup_f32(
        None,
        &theme.global_values,
        &["theme.typography.scale", "typography.scale", "font.scale"],
    )
    .unwrap_or(1.0);

    theme.metrics = NativeThemeMetrics::from_density(theme.density, spacing_scale, radius_scale);
    theme.typography = NativeTypography::from_density(theme.density, type_scale);

    theme.palette.bg_top = theme_lookup_color(
        None,
        &theme.global_values,
        &[
            "theme.background.top",
            "theme.bg.top",
            "theme.surface.background",
            "surface.background",
        ],
        theme.palette.bg_top,
    );
    theme.palette.bg_bottom = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.background.bottom", "theme.bg.bottom"],
        theme.palette.bg_bottom,
    );
    theme.palette.surface_base = theme_lookup_color(
        None,
        &theme.global_values,
        &[
            "theme.surface.base",
            "theme.surface.default",
            "surface.fill",
        ],
        theme.palette.surface_base,
    );
    theme.palette.surface_alt = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.surface.alt", "theme.surface.secondary"],
        theme.palette.surface_alt,
    );
    theme.palette.surface_raised = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.surface.raised", "theme.surface.elevated"],
        theme.palette.surface_raised,
    );
    theme.palette.surface_overlay = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.surface.overlay", "surface.overlay"],
        theme.palette.surface_overlay,
    );
    theme.palette.outline_soft = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.outline.soft", "outline.soft"],
        theme.palette.outline_soft,
    );
    theme.palette.outline_bright = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.outline.bright", "outline.bright"],
        theme.palette.outline_bright,
    );
    theme.palette.accent = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.accent.primary", "theme.accent", "accent.color"],
        theme.palette.accent,
    );
    theme.palette.accent_soft = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.accent.soft", "accent.soft"],
        theme.palette.accent_soft,
    );
    theme.palette.highlight = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.highlight", "highlight.color"],
        theme.palette.highlight,
    );
    theme.palette.success = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.success", "success.color"],
        theme.palette.success,
    );
    theme.palette.text = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.text.default", "text.default", "text.color"],
        theme.palette.text,
    );
    theme.palette.text_muted = theme_lookup_color(
        None,
        &theme.global_values,
        &["theme.text.muted", "text.muted"],
        theme.palette.text_muted,
    );

    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["theme.chrome.mode", "chrome.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.chrome_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["widget.panel.surface.mode", "panel.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.panel_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["widget.inspector.surface.mode", "inspector.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.inspector_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["widget.tree.surface.mode", "tree.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.tree_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["widget.graph.surface.mode", "graph.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.graph_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &["widget.timeline.surface.mode", "timeline.surface.mode"],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.timeline_mode = mode;
    }
    if let Some(mode) = theme_lookup_string(
        None,
        &theme.global_values,
        &[
            "widget.viewport3d.surface.mode",
            "widget.viewport2d.surface.mode",
            "viewport.surface.mode",
        ],
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.viewport_mode = mode;
    }
    theme
}

fn show_runtime_topbar(app_theme: &NativeAppTheme, default_visible: bool) -> bool {
    theme_lookup_bool(
        None,
        &app_theme.global_values,
        &[
            "theme.chrome.topbar.visible",
            "chrome.topbar.visible",
            "host.topbar.visible",
        ],
    )
    .unwrap_or(default_visible)
}

fn show_runtime_inspector(app_theme: &NativeAppTheme, default_visible: bool) -> bool {
    theme_lookup_bool(
        None,
        &app_theme.global_values,
        &[
            "theme.chrome.inspector.visible",
            "chrome.inspector.visible",
            "host.inspector.visible",
        ],
    )
    .unwrap_or(default_visible)
}

fn resolve_widget_theme(
    node: &UiNode,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
) -> NativeWidgetTheme {
    let local = ui_resolve_theme_for_node(node, theme_registry);
    let widget_key = widget_kind_key(&node.kind);
    let variant = node.style.variant.as_deref();
    let mut theme = NativeWidgetTheme::from_kind(&node.kind, app_theme);

    if let Some(mode) = theme_lookup_widget_string(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.mode",
    )
    .and_then(|value| NativeSurfaceMode::from_str(&value))
    {
        theme.mode = mode;
        let (fill, stroke, canvas_fill, overlay_fill) =
            surface_colors_for_mode(mode, &app_theme.palette);
        theme.fill = fill;
        theme.stroke = stroke;
        theme.canvas_fill = canvas_fill;
        theme.overlay_fill = overlay_fill;
    }

    theme.fill = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.fill",
        theme.fill,
    );
    theme.stroke = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.stroke",
        theme.stroke,
    );
    theme.canvas_fill = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.canvas",
        theme.canvas_fill,
    );
    theme.overlay_fill = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.overlay",
        theme.overlay_fill,
    );
    theme.header_fill = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.header",
        theme.header_fill,
    );
    theme.header_stroke = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.header.stroke",
        theme.header_stroke,
    );
    theme.badge_fill = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.badge",
        theme.badge_fill,
    );
    theme.accent = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "accent.color",
        theme.accent,
    );
    theme.title_color = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "title.color",
        theme.title_color,
    );
    theme.body_color = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "text.color",
        theme.body_color,
    );
    theme.muted_color = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "text.muted",
        theme.muted_color,
    );
    theme.tag_color = theme_lookup_widget_color(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "tag.color",
        theme.tag_color,
    );

    if let Some(radius) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.radius",
    ) {
        theme.radius = radius.max(0.0);
    }
    if let Some(padding) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.padding",
    ) {
        theme.padding = padding.max(0.0);
    }
    if let Some(gap) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "layout.gap",
    ) {
        theme.gap = gap.max(0.0);
    }
    if let Some(title_size) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "title.size",
    ) {
        theme.title_size = title_size.max(10.0);
    }
    if let Some(body_size) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "body.size",
    ) {
        theme.body_size = body_size.max(8.0);
    }
    if let Some(tag_size) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "tag.size",
    ) {
        theme.tag_size = tag_size.max(8.0);
    }
    if let Some(alpha) = theme_lookup_widget_f32(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "surface.alpha",
    ) {
        theme.fill = alpha_tint(theme.fill, alpha);
    }
    if let Some(density) = theme_lookup_widget_string(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "density",
    )
    .and_then(|value| NativeDensity::from_str(&value))
    {
        let metrics = NativeThemeMetrics::from_density(density, 1.0, 1.0);
        theme.padding = metrics.tight_padding;
        theme.gap = metrics.content_gap;
    }

    for class_name in &node.style.classes {
        match class_name.trim().to_ascii_lowercase().as_str() {
            "hero" | "accent" => {
                theme.mode = NativeSurfaceMode::Accent;
                let (fill, stroke, canvas_fill, overlay_fill) =
                    surface_colors_for_mode(theme.mode, &app_theme.palette);
                theme.fill = fill;
                theme.stroke = stroke;
                theme.canvas_fill = canvas_fill;
                theme.overlay_fill = overlay_fill;
                theme.title_size *= 1.08;
            }
            "glass" | "frosted" => {
                theme.mode = NativeSurfaceMode::Glass;
                let (fill, stroke, canvas_fill, overlay_fill) =
                    surface_colors_for_mode(theme.mode, &app_theme.palette);
                theme.fill = fill;
                theme.stroke = stroke;
                theme.canvas_fill = canvas_fill;
                theme.overlay_fill = overlay_fill;
            }
            "ghost" | "minimal" => {
                theme.mode = NativeSurfaceMode::Ghost;
                let (fill, stroke, canvas_fill, overlay_fill) =
                    surface_colors_for_mode(theme.mode, &app_theme.palette);
                theme.fill = fill;
                theme.stroke = stroke;
                theme.canvas_fill = canvas_fill;
                theme.overlay_fill = overlay_fill;
            }
            "compact" | "dense" | "tight" => {
                theme.padding *= 0.78;
                theme.gap *= 0.78;
                theme.title_size *= 0.95;
                theme.body_size *= 0.95;
            }
            "spacious" | "airy" => {
                theme.padding *= 1.18;
                theme.gap *= 1.18;
                theme.title_size *= 1.05;
            }
            "muted" => {
                theme.title_color = app_theme.palette.text_muted;
                theme.body_color = app_theme.palette.text_muted;
                theme.fill = alpha_tint(theme.fill, 0.78);
            }
            "selected" => {
                theme.stroke = app_theme.palette.accent_soft;
                theme.fill = alpha_tint(theme.fill, 0.95);
            }
            _ => {}
        }
    }

    for state in &node.style.states {
        match state {
            UiStyleState::Hovered => {
                theme.stroke = app_theme.palette.accent;
            }
            UiStyleState::Active | UiStyleState::Focused => {
                theme.stroke = app_theme.palette.outline_bright;
                theme.title_color = app_theme.palette.highlight;
            }
            UiStyleState::Selected => {
                theme.stroke = app_theme.palette.highlight;
                theme.fill = alpha_tint(theme.fill, 0.98);
            }
            UiStyleState::Disabled => {
                theme.fill = alpha_tint(theme.fill, 0.55);
                theme.body_color = alpha_tint(theme.body_color, 0.65);
                theme.title_color = alpha_tint(theme.title_color, 0.75);
            }
            UiStyleState::Dragging => {
                theme.stroke = app_theme.palette.success;
            }
        }
    }

    if node.layout.padding > 0.0 {
        theme.padding = node.layout.padding;
    }
    if node.layout.gap > 0.0 {
        theme.gap = node.layout.gap;
    }

    theme
}

fn widget_kind_key(kind: &UiWidgetKind) -> &'static str {
    match kind {
        UiWidgetKind::Panel => "panel",
        UiWidgetKind::Inspector => "inspector",
        UiWidgetKind::Graph => "graph",
        UiWidgetKind::Timeline => "timeline",
        UiWidgetKind::Table => "table",
        UiWidgetKind::Tree => "tree",
        UiWidgetKind::Viewport2D => "viewport2d",
        UiWidgetKind::Viewport3D => "viewport3d",
        UiWidgetKind::Overlay => "overlay",
        UiWidgetKind::Slot => "slot",
        UiWidgetKind::Text => "text",
        UiWidgetKind::ComponentRef(_) => "component",
        UiWidgetKind::Element(_) => "element",
    }
}

fn candidate_theme_keys(widget_key: &str, variant: Option<&str>, property: &str) -> Vec<String> {
    let mut keys = Vec::with_capacity(4);
    if let Some(variant) = variant {
        keys.push(format!("widget.{widget_key}.variant.{variant}.{property}"));
        keys.push(format!("variant.{variant}.{property}"));
    }
    keys.push(format!("widget.{widget_key}.{property}"));
    keys.push(property.to_string());
    keys
}

fn theme_lookup_widget_string(
    local: &UiResolvedTheme,
    global: &BTreeMap<String, UiValue>,
    widget_key: &str,
    variant: Option<&str>,
    property: &str,
) -> Option<String> {
    let keys = candidate_theme_keys(widget_key, variant, property);
    let refs = keys.iter().map(String::as_str).collect::<Vec<_>>();
    let empty = BTreeMap::new();
    theme_lookup_string(Some(&local.values), &empty, &refs)
        .or_else(|| theme_lookup_string(None, global, &refs))
}

fn theme_lookup_widget_f32(
    local: &UiResolvedTheme,
    global: &BTreeMap<String, UiValue>,
    widget_key: &str,
    variant: Option<&str>,
    property: &str,
) -> Option<f32> {
    let keys = candidate_theme_keys(widget_key, variant, property);
    let refs = keys.iter().map(String::as_str).collect::<Vec<_>>();
    let empty = BTreeMap::new();
    theme_lookup_f32(Some(&local.values), &empty, &refs)
        .or_else(|| theme_lookup_f32(None, global, &refs))
}

fn theme_lookup_widget_bool(
    local: &UiResolvedTheme,
    global: &BTreeMap<String, UiValue>,
    widget_key: &str,
    variant: Option<&str>,
    property: &str,
) -> Option<bool> {
    let keys = candidate_theme_keys(widget_key, variant, property);
    let refs = keys.iter().map(String::as_str).collect::<Vec<_>>();
    let empty = BTreeMap::new();
    theme_lookup_bool(Some(&local.values), &empty, &refs)
        .or_else(|| theme_lookup_bool(None, global, &refs))
}

fn theme_lookup_widget_color(
    local: &UiResolvedTheme,
    global: &BTreeMap<String, UiValue>,
    widget_key: &str,
    variant: Option<&str>,
    property: &str,
    fallback: Color32,
) -> Color32 {
    let keys = candidate_theme_keys(widget_key, variant, property);
    let refs = keys.iter().map(String::as_str).collect::<Vec<_>>();
    let empty = BTreeMap::new();
    theme_lookup_color_option(Some(&local.values), &empty, &refs)
        .or_else(|| theme_lookup_color_option(None, global, &refs))
        .unwrap_or(fallback)
}

fn theme_lookup_string(
    local: Option<&BTreeMap<String, UiValue>>,
    global: &BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<String> {
    keys.iter().find_map(|key| {
        theme_lookup_value(local, global, key, 0).and_then(|value| match value {
            UiValue::String(value) => Some(value.clone()),
            _ => None,
        })
    })
}

fn theme_lookup_f32(
    local: Option<&BTreeMap<String, UiValue>>,
    global: &BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<f32> {
    keys.iter()
        .find_map(|key| theme_lookup_value(local, global, key, 0).and_then(ui_value_as_f32))
}

fn theme_lookup_bool(
    local: Option<&BTreeMap<String, UiValue>>,
    global: &BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<bool> {
    keys.iter()
        .find_map(|key| theme_lookup_value(local, global, key, 0).and_then(ui_value_as_bool))
}

fn theme_lookup_color(
    local: Option<&BTreeMap<String, UiValue>>,
    global: &BTreeMap<String, UiValue>,
    keys: &[&str],
    fallback: Color32,
) -> Color32 {
    theme_lookup_color_option(local, global, keys).unwrap_or(fallback)
}

fn theme_lookup_color_option(
    local: Option<&BTreeMap<String, UiValue>>,
    global: &BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<Color32> {
    keys.iter()
        .find_map(|key| theme_lookup_value(local, global, key, 0).and_then(ui_value_as_color))
}

fn theme_lookup_value<'a>(
    local: Option<&'a BTreeMap<String, UiValue>>,
    global: &'a BTreeMap<String, UiValue>,
    key: &str,
    depth: usize,
) -> Option<&'a UiValue> {
    if depth > 8 {
        return None;
    }
    let value = local
        .and_then(|values| values.get(key))
        .or_else(|| global.get(key))?;

    if let UiValue::String(alias) = value {
        let alias = alias.trim();
        if alias != key
            && (local.is_some_and(|values| values.contains_key(alias))
                || global.contains_key(alias))
        {
            return theme_lookup_value(local, global, alias, depth + 1).or(Some(value));
        }
    }

    Some(value)
}

fn ui_value_as_f32(value: &UiValue) -> Option<f32> {
    match value {
        UiValue::Float(value) => Some(*value as f32),
        UiValue::Int(value) => Some(*value as f32),
        UiValue::String(value) => value.parse::<f32>().ok(),
        _ => None,
    }
}

fn ui_value_as_bool(value: &UiValue) -> Option<bool> {
    match value {
        UiValue::Bool(value) => Some(*value),
        UiValue::Int(value) => Some(*value != 0),
        UiValue::Float(value) => Some(value.abs() > f64::EPSILON),
        UiValue::String(value) => match value.trim().to_ascii_lowercase().as_str() {
            "1" | "true" | "yes" | "on" => Some(true),
            "0" | "false" | "no" | "off" => Some(false),
            _ => None,
        },
        _ => None,
    }
}

fn ui_value_as_i64(value: &UiValue) -> Option<i64> {
    match value {
        UiValue::Int(value) => Some(*value),
        UiValue::Float(value) => Some(*value as i64),
        UiValue::String(value) => value.parse::<i64>().ok(),
        _ => None,
    }
}

fn ui_value_as_color(value: &UiValue) -> Option<Color32> {
    match value {
        UiValue::String(value) => parse_theme_color(value),
        _ => None,
    }
}

fn parse_theme_color(value: &str) -> Option<Color32> {
    let trimmed = value.trim();
    if let Some(hex) = trimmed.strip_prefix('#') {
        return match hex.len() {
            6 => {
                let rgb = u32::from_str_radix(hex, 16).ok()?;
                Some(Color32::from_rgb(
                    ((rgb >> 16) & 0xff) as u8,
                    ((rgb >> 8) & 0xff) as u8,
                    (rgb & 0xff) as u8,
                ))
            }
            8 => {
                let rgba = u32::from_str_radix(hex, 16).ok()?;
                Some(Color32::from_rgba_unmultiplied(
                    ((rgba >> 24) & 0xff) as u8,
                    ((rgba >> 16) & 0xff) as u8,
                    ((rgba >> 8) & 0xff) as u8,
                    (rgba & 0xff) as u8,
                ))
            }
            _ => None,
        };
    }

    match trimmed.to_ascii_lowercase().as_str() {
        "white" => Some(Color32::WHITE),
        "black" => Some(Color32::BLACK),
        "transparent" => Some(Color32::from_rgba_unmultiplied(0, 0, 0, 0)),
        _ => None,
    }
}

fn themed_frame(theme: &NativeWidgetTheme) -> Frame {
    let stroke_width = match theme.mode {
        NativeSurfaceMode::Ghost => 0.0,
        NativeSurfaceMode::Flat => 0.7,
        NativeSurfaceMode::Layered => 0.8,
        NativeSurfaceMode::Glass => 0.9,
        NativeSurfaceMode::Canvas | NativeSurfaceMode::Accent => 1.0,
    };
    Frame::new()
        .fill(theme.fill)
        .stroke(Stroke::new(stroke_width, theme.stroke))
        .corner_radius(theme.radius)
        .inner_margin(theme.padding)
}

fn dock_placement_label(placement: kain_ui::UiDockPlacement) -> &'static str {
    match placement {
        kain_ui::UiDockPlacement::Left => "dock-left",
        kain_ui::UiDockPlacement::Right => "dock-right",
        kain_ui::UiDockPlacement::Top => "dock-top",
        kain_ui::UiDockPlacement::Bottom => "dock-bottom",
        kain_ui::UiDockPlacement::Center => "dock-center",
    }
}

fn widget_chrome_tags(node: &UiNode) -> Vec<String> {
    let mut tags = Vec::new();
    tags.push(widget_kind_key(&node.kind).to_string());

    if let Some(variant) = node.style.variant.as_deref() {
        tags.push(format!("#{variant}"));
    }
    for class_name in node.style.classes.iter().take(2) {
        tags.push(format!(".{class_name}"));
    }
    if let Some(dock) = node.layout.dock {
        tags.push(dock_placement_label(dock).to_string());
    }
    if let Some(group_id) = node.layout.tab_group_id.as_deref() {
        tags.push(format!("tab:{group_id}"));
    }
    if let Some(layout_id) = node.layout.persistent_layout_id.as_deref() {
        tags.push(format!("id:{layout_id}"));
    }

    tags
}

fn render_tag_chip(ui: &mut egui::Ui, theme: &NativeWidgetTheme, label: &str, emphasized: bool) {
    let (fill, stroke, text_color) = if emphasized {
        (
            alpha_tint(theme.accent, 0.24),
            theme.accent,
            theme.title_color,
        )
    } else {
        (
            alpha_tint(theme.badge_fill, 0.92),
            alpha_tint(theme.header_stroke, 0.92),
            theme.tag_color,
        )
    };

    Frame::new()
        .fill(fill)
        .stroke(Stroke::new(1.0, stroke))
        .corner_radius(theme.radius * 0.45)
        .inner_margin(theme.padding * 0.22)
        .show(ui, |ui| {
            ui.label(
                RichText::new(label)
                    .size(theme.tag_size)
                    .monospace()
                    .color(text_color),
            );
        });
}

fn render_widget_header(
    ui: &mut egui::Ui,
    theme: &NativeWidgetTheme,
    title: &str,
    node: &UiNode,
    kind_label: &str,
) {
    let tags = widget_chrome_tags(node);

    Frame::new()
        .fill(theme.header_fill)
        .stroke(Stroke::new(1.0, theme.header_stroke))
        .corner_radius(theme.radius * 0.72)
        .inner_margin(theme.padding * 0.48)
        .show(ui, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.vertical(|ui| {
                    ui.label(
                        RichText::new(title)
                            .strong()
                            .size(theme.title_size)
                            .color(theme.title_color),
                    );
                    ui.label(
                        RichText::new(kind_label)
                            .size(theme.tag_size)
                            .monospace()
                            .color(theme.muted_color),
                    );
                });
                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    for (index, tag) in tags.iter().take(4).rev().enumerate() {
                        if index > 0 {
                            ui.add_space(theme.gap * 0.35);
                        }
                        render_tag_chip(ui, theme, tag, index == 0);
                    }
                });
            });
        });
}

fn render_widget_body_frame<R>(
    ui: &mut egui::Ui,
    theme: &NativeWidgetTheme,
    body: impl FnOnce(&mut egui::Ui) -> R,
) -> R {
    Frame::new()
        .fill(alpha_tint(theme.canvas_fill, 0.88))
        .stroke(Stroke::new(1.0, alpha_tint(theme.stroke, 0.84)))
        .corner_radius(theme.radius * 0.58)
        .inner_margin(theme.padding * 0.62)
        .show(ui, body)
        .inner
}

fn is_product_desktop_theme(
    app_theme: &NativeAppTheme,
    snapshot: Option<&NativeAppRuntimeSnapshot>,
) -> bool {
    let theme_name = app_theme.name.trim().to_ascii_lowercase();
    let known_product_theme = matches!(
        theme_name.as_str(),
        "kade_desktop"
            | "kade-desktop"
            | "kain_fabric_universal_studio"
            | "kain-fabric-universal-studio"
    );
    let known_product_app = snapshot.is_some_and(|value| {
        value.app_id.eq_ignore_ascii_case("kade.desktop")
            || value.app_id.eq_ignore_ascii_case("kain.fabric.dcc-suite")
    });
    let explicit_host_chrome_opt_out =
        !show_runtime_topbar(app_theme, true) && !show_runtime_inspector(app_theme, true);

    known_product_theme || known_product_app || explicit_host_chrome_opt_out
}

fn widget_title_visible(
    node: &UiNode,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
) -> bool {
    let local = ui_resolve_theme_for_node(node, theme_registry);
    let widget_key = widget_kind_key(&node.kind);
    let variant = node.style.variant.as_deref();
    theme_lookup_widget_bool(
        &local,
        &app_theme.global_values,
        widget_key,
        variant,
        "title.visible",
    )
    .unwrap_or(true)
}

fn apply_runtime_visuals(ctx: &egui::Context, app_theme: &NativeAppTheme) {
    let mut visuals = egui::Visuals::dark();
    visuals.override_text_color = Some(app_theme.palette.text);
    visuals.panel_fill = app_theme.palette.bg_bottom;
    visuals.window_fill = app_theme.palette.bg_bottom;
    visuals.faint_bg_color = app_theme.palette.surface_base;
    visuals.extreme_bg_color = app_theme.palette.bg_top;
    visuals.widgets.noninteractive.bg_fill = app_theme.palette.surface_base;
    visuals.widgets.noninteractive.fg_stroke.color = app_theme.palette.text_muted;
    visuals.widgets.noninteractive.bg_stroke = Stroke::new(1.0, app_theme.palette.outline_soft);
    visuals.widgets.inactive.bg_fill = app_theme.palette.surface_alt;
    visuals.widgets.inactive.fg_stroke.color = app_theme.palette.accent_soft;
    visuals.widgets.inactive.bg_stroke = Stroke::new(1.0, app_theme.palette.outline_soft);
    visuals.widgets.hovered.bg_fill = app_theme.palette.surface_raised;
    visuals.widgets.hovered.fg_stroke.color = app_theme.palette.text;
    visuals.widgets.hovered.bg_stroke = Stroke::new(1.0, app_theme.palette.accent);
    visuals.widgets.active.bg_fill = alpha_tint(app_theme.palette.accent, 0.22);
    visuals.widgets.active.fg_stroke.color = app_theme.palette.text;
    visuals.widgets.active.bg_stroke = Stroke::new(1.0, app_theme.palette.accent_soft);
    visuals.widgets.open.bg_fill = app_theme.palette.surface_raised;
    visuals.selection.bg_fill = alpha_tint(app_theme.palette.accent, 0.36);
    visuals.selection.stroke.color = app_theme.palette.accent_soft;
    visuals.hyperlink_color = app_theme.palette.highlight;
    visuals.window_stroke = Stroke::new(1.0, app_theme.palette.outline_soft);
    ctx.set_visuals(visuals);

    let mut style = (*ctx.style()).clone();
    style.spacing.item_spacing =
        egui::vec2(app_theme.metrics.content_gap, app_theme.metrics.section_gap);
    style.spacing.button_padding = egui::vec2(
        app_theme.metrics.tight_padding,
        app_theme.metrics.tight_padding * 0.7,
    );
    style.spacing.indent = app_theme.metrics.content_gap + 4.0;
    style.text_styles.insert(
        egui::TextStyle::Heading,
        egui::FontId::proportional(app_theme.typography.heading),
    );
    style.text_styles.insert(
        egui::TextStyle::Body,
        egui::FontId::proportional(app_theme.typography.body),
    );
    style.text_styles.insert(
        egui::TextStyle::Button,
        egui::FontId::proportional(app_theme.typography.body),
    );
    style.text_styles.insert(
        egui::TextStyle::Small,
        egui::FontId::proportional(app_theme.typography.small),
    );
    style.text_styles.insert(
        egui::TextStyle::Monospace,
        egui::FontId::monospace(app_theme.typography.code),
    );
    ctx.set_style(style);
}

fn resolve_text_role(node: &UiNode) -> NativeTextRole {
    if let Some(role) = prop_text(node, "role").or_else(|| prop_text(node, "text.role")) {
        return parse_text_role(role);
    }
    if let Some(variant) = node.style.variant.as_deref() {
        return parse_text_role(variant);
    }
    for class_name in &node.style.classes {
        let role = parse_text_role(class_name);
        if role != NativeTextRole::Body {
            return role;
        }
    }
    NativeTextRole::Body
}

fn parse_text_role(value: &str) -> NativeTextRole {
    match value.trim().to_ascii_lowercase().as_str() {
        "hero" | "display" => NativeTextRole::Hero,
        "title" | "heading" | "section" => NativeTextRole::Title,
        "eyebrow" | "kicker" | "label" => NativeTextRole::Eyebrow,
        "caption" | "small" => NativeTextRole::Caption,
        "muted" | "secondary" => NativeTextRole::Muted,
        "code" | "mono" | "monospace" => NativeTextRole::Code,
        "metric" | "stat" | "numeric" => NativeTextRole::Metric,
        _ => NativeTextRole::Body,
    }
}

fn render_text_node(
    ui: &mut egui::Ui,
    node: &UiNode,
    app_theme: &NativeAppTheme,
    presentation: NativeNodePresentation,
) {
    let role = resolve_text_role(node);
    let text = prop_text(node, "text").unwrap_or_default();
    let (size, color, monospace, strong) = match role {
        NativeTextRole::Hero => (
            app_theme.typography.heading * 1.3,
            app_theme.palette.text,
            false,
            true,
        ),
        NativeTextRole::Title => (
            app_theme.typography.section * 1.1,
            app_theme.palette.text,
            false,
            true,
        ),
        NativeTextRole::Eyebrow => (
            app_theme.typography.small,
            app_theme.palette.accent_soft,
            true,
            false,
        ),
        NativeTextRole::Caption => (
            app_theme.typography.small,
            app_theme.palette.text_muted,
            false,
            false,
        ),
        NativeTextRole::Muted => (
            app_theme.typography.body,
            app_theme.palette.text_muted,
            false,
            false,
        ),
        NativeTextRole::Code => (
            app_theme.typography.code,
            app_theme.palette.highlight,
            true,
            false,
        ),
        NativeTextRole::Metric => (
            app_theme.typography.heading * 1.15,
            app_theme.palette.highlight,
            true,
            true,
        ),
        NativeTextRole::Body => (
            app_theme.typography.body,
            app_theme.palette.text,
            false,
            false,
        ),
    };

    let mut rich = RichText::new(text)
        .size(size)
        .color(apply_node_presentation_to_color(color, presentation));
    if monospace {
        rich = rich.monospace();
    }
    if strong {
        rich = rich.strong();
    }
    ui.label(rich);
}

fn layout_align(align: UiLayoutAlignment) -> Align {
    match align {
        UiLayoutAlignment::Start | UiLayoutAlignment::Stretch | UiLayoutAlignment::SpaceBetween => {
            Align::Min
        }
        UiLayoutAlignment::Center => Align::Center,
        UiLayoutAlignment::End => Align::Max,
    }
}

fn resolve_ui_length(length: Option<UiLength>, available: f32) -> Option<f32> {
    let length = length?;
    match length.unit {
        UiLengthUnit::Px => Some(length.value.max(0.0)),
        UiLengthUnit::Percent => Some((available * (length.value / 100.0)).max(0.0)),
        UiLengthUnit::Fr => Some((available * length.value.max(0.0)).max(0.0)),
        UiLengthUnit::Auto => None,
    }
}

fn apply_node_layout_constraints(ui: &mut egui::Ui, node: &UiNode) {
    if let Some(min_width) = node.layout.min_width {
        ui.set_min_width(min_width.max(0.0));
    }
    if let Some(min_height) = node.layout.min_height {
        ui.set_min_height(min_height.max(0.0));
    }
    if let Some(max_width) = node.layout.max_width {
        ui.set_max_width(max_width.max(0.0));
    }
    if let Some(max_height) = node.layout.max_height {
        ui.set_max_height(max_height.max(0.0));
    }

    let available = ui.available_size_before_wrap();
    if let Some(width) = resolve_ui_length(node.layout.width, available.x) {
        ui.set_width(width);
    }
    if let Some(height) = resolve_ui_length(node.layout.height, available.y) {
        ui.set_height(height);
    }
}

impl Default for KainUiNativeAppConfig {
    fn default() -> Self {
        Self {
            window_title: "KAIN UI Native Demo".to_string(),
            root_component: "App".to_string(),
            source: KAIN_UI_NATIVE_DEMO_SOURCE.to_string(),
            initial_window_size: [1440.0, 920.0],
        }
    }
}

pub fn build_output(config: &KainUiNativeAppConfig) -> Result<UiBuildOutput, kain_core::KainError> {
    build_ui_output_from_source(&config.source, &config.root_component)
}

pub fn build_runtime_bundle(
    config: &KainUiNativeAppConfig,
) -> Result<KainUiNativeRuntimeBundle, kain_core::KainError> {
    let output = build_output(config)?;
    Ok(runtime_bundle_from_output(config, output))
}

pub fn runtime_bundle_from_output(
    config: &KainUiNativeAppConfig,
    output: UiBuildOutput,
) -> KainUiNativeRuntimeBundle {
    ui_runtime_bundle_from_output(
        KainUiNativeRuntimeMetadata {
            app_name: None,
            window_title: config.window_title.clone(),
            root_component: config.root_component.clone(),
            source_file_name: None,
            initial_window_size: config.initial_window_size,
        },
        output,
    )
}

pub fn runtime_bundle_to_json(
    bundle: &KainUiNativeRuntimeBundle,
) -> Result<String, serde_json::Error> {
    ui_runtime_bundle_to_json(bundle)
}

pub fn runtime_bundle_from_json(
    json: &str,
) -> Result<KainUiNativeRuntimeBundle, serde_json::Error> {
    ui_runtime_bundle_from_json(json)
}

pub fn build_demo_output(
    config: &KainUiNativeDemoConfig,
) -> Result<UiBuildOutput, kain_core::KainError> {
    build_output(config)
}

pub fn run_app(config: KainUiNativeAppConfig) -> Result<(), Box<dyn std::error::Error>> {
    let output = build_output(&config)?;
    run_output(config, output, AppBootMode::Source)
}

pub fn run_bundled_app(
    bundle: KainUiNativeRuntimeBundle,
) -> Result<(), Box<dyn std::error::Error>> {
    validate_runtime_bundle(&bundle)?;
    let config = KainUiNativeAppConfig {
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        source: String::new(),
        initial_window_size: bundle.metadata.initial_window_size,
    };
    run_output(config, bundle.output, AppBootMode::CompiledBundle)
}

pub fn run_bundled_app_json(json: &str) -> Result<(), Box<dyn std::error::Error>> {
    let bundle = runtime_bundle_from_json(json)?;
    run_bundled_app(bundle)
}

pub fn run_demo(config: KainUiNativeDemoConfig) -> Result<(), Box<dyn std::error::Error>> {
    run_app(config)
}

fn validate_runtime_bundle(
    bundle: &KainUiNativeRuntimeBundle,
) -> Result<(), Box<dyn std::error::Error>> {
    validate_ui_runtime_bundle(bundle).map_err(|err| Box::new(err) as Box<dyn std::error::Error>)
}

fn run_output(
    config: KainUiNativeAppConfig,
    output: UiBuildOutput,
    boot_mode: AppBootMode,
) -> Result<(), Box<dyn std::error::Error>> {
    let runtime_settings = KainUiNativeRuntimeSettings::from_env();
    trace_runtime(format!(
        "run_output: window_title={} boot_mode={} renderer={} effective_renderer={} inspector={} viewports={}",
        config.window_title,
        boot_mode.label(),
        runtime_settings.renderer_label(),
        runtime_settings.effective_renderer_label(),
        runtime_settings.show_runtime_inspector,
        runtime_settings.enable_viewports,
    ));
    let window_title = config.window_title.clone();
    let initial_window_size = config.initial_window_size;

    let options = eframe::NativeOptions {
        renderer: runtime_settings.eframe_renderer(),
        viewport: egui::ViewportBuilder::default()
            .with_inner_size(initial_window_size)
            .with_min_inner_size([960.0, 640.0]),
        ..Default::default()
    };

    eframe::run_native(
        &window_title,
        options,
        Box::new(move |cc| {
            trace_runtime("run_native: creation_context received");
            Ok(Box::new(KainUiNativeApp::new(
                cc,
                config.clone(),
                output.clone(),
                boot_mode,
                runtime_settings,
            )))
        }),
    )?;

    Ok(())
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum AppBootMode {
    Source,
    CompiledBundle,
}

impl AppBootMode {
    fn label(self) -> &'static str {
        match self {
            Self::Source => "source",
            Self::CompiledBundle => "compiled bundle",
        }
    }
}

struct ViewportSurfaceState {
    texture: Option<egui::TextureHandle>,
    scene_name: String,
    bundle_viewport_node: Option<String>,
    bundle_material_refs: Vec<String>,
    bundle_shader_ref_keys: Vec<String>,
    bundle_camera: Option<RealtimeViewportCameraBinding>,
    bundle_presentation: Option<RealtimeViewportPresentationBinding>,
    bundle_warning: Option<String>,
    controller: ViewportCameraController,
    last_render_at: Option<Instant>,
    last_stats: RenderStats,
    selected_instance_id: Option<String>,
    manipulator_mode: ManipulatorMode,
    manipulator_space: ManipulatorSpace,
    snap_enabled: bool,
    last_pick: Option<PickingHit>,
    instance_transform_overrides: BTreeMap<String, SceneTransform>,
    active_manipulation: Option<ViewportManipulatorState>,
    presented_state: Option<Arc<Mutex<PresentedViewportGpuState>>>,
}

#[derive(Clone, Debug)]
struct ViewportManipulatorState {
    selected_instance_id: String,
    drag_origin_pointer: egui::Pos2,
    drag_origin_transform: SceneTransform,
}

#[derive(Clone, Debug, Default)]
struct RealtimeBundleCatalog {
    scenes_by_viewport: BTreeMap<String, RealtimeSceneBinding>,
    materials_by_id: BTreeMap<String, CompiledMaterialDefinition>,
    shader_refs_by_key: BTreeMap<String, RealtimeShaderBundleRef>,
    shader_canvases_by_surface: BTreeMap<String, RealtimeShaderCanvasBinding>,
    assets_by_key: BTreeMap<String, RealtimeAssetBinding>,
    asset_base_dir: Option<PathBuf>,
}

#[derive(Clone, Debug)]
struct ResolvedViewportBinding {
    viewport_node: Option<String>,
    scene_name: String,
    material_refs: Vec<String>,
    shader_ref_keys: Vec<String>,
    camera: Option<RealtimeViewportCameraBinding>,
    presentation: Option<RealtimeViewportPresentationBinding>,
    warning: Option<String>,
}

struct ShaderSurfaceState {
    presented_state: Option<Arc<Mutex<PresentedShaderSurfaceGpuState>>>,
    last_signature: Option<String>,
    last_shader_ref: Option<String>,
    last_warning: Option<String>,
}

#[derive(Clone, Debug)]
struct ResolvedUiShaderSurface {
    shader_ref: String,
    shader_name: String,
    module_name: String,
    fragment_entry_point: String,
    stage: String,
    derived_format: String,
    canonical_native_payload: String,
    shader_bundle_ref_key: Option<String>,
    wgsl_source: String,
    resource_layouts: Vec<ShaderResourceLayout>,
    font_atlases: Vec<RealtimeShaderCanvasFontAtlas>,
    font_asset_sources_by_key: BTreeMap<String, String>,
    text_runs: Vec<RealtimeShaderCanvasTextRun>,
    resource_bindings: Vec<RealtimeShaderCanvasResourceBinding>,
    warning: Option<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum UiShaderBindingKind {
    UniformBuffer,
    StorageBuffer,
    Texture2d,
    Sampler,
}

#[derive(Clone, Copy)]
struct PresentedViewportHost {
    target_format: wgpu::TextureFormat,
}

struct PresentedViewportGpuState {
    target_format: wgpu::TextureFormat,
    shader_bundle: Option<ShaderArtifactBundle>,
    resources: Option<PresentedViewportGpuResources>,
    prepared_frame: Option<PreparedWgpuFrame>,
    draw_data: Option<PresentedViewportDrawData>,
}

struct PresentedViewportGpuResources {
    uniform_buffer: wgpu::Buffer,
    uniform_bind_group: wgpu::BindGroup,
    background_pipeline: wgpu::RenderPipeline,
    scene_pipeline: wgpu::RenderPipeline,
    particle_depth_pipeline: wgpu::RenderPipeline,
    particle_overlay_pipeline: wgpu::RenderPipeline,
    gizmo_pipeline: wgpu::RenderPipeline,
}

struct PresentedViewportDrawData {
    scene_buffer: Option<wgpu::Buffer>,
    scene_len: u32,
    depth_particle_buffer: Option<wgpu::Buffer>,
    depth_particle_len: u32,
    overlay_particle_buffer: Option<wgpu::Buffer>,
    overlay_particle_len: u32,
    gizmo_buffer: Option<wgpu::Buffer>,
    gizmo_len: u32,
}

#[derive(Clone)]
struct PresentedViewportCallback {
    state: Arc<Mutex<PresentedViewportGpuState>>,
}

struct PresentedShaderSurfaceGpuState {
    target_format: wgpu::TextureFormat,
    resources: Option<PresentedShaderSurfaceGpuResources>,
    pending_frame: Option<PresentedShaderSurfaceFrame>,
}

struct PresentedShaderSurfaceGpuResources {
    signature: String,
    pipeline: wgpu::RenderPipeline,
    bind_group: Option<wgpu::BindGroup>,
    uniform_buffer: Option<wgpu::Buffer>,
    storage_buffer: Option<wgpu::Buffer>,
    surface_texture: Option<wgpu::Texture>,
    _surface_texture_view: Option<wgpu::TextureView>,
    _surface_sampler: Option<wgpu::Sampler>,
}

#[derive(Clone)]
struct PresentedShaderSurfaceFrame {
    signature: String,
    descriptor: ResolvedUiShaderSurface,
    uniforms: UiShaderSurfaceUniforms,
    storage_payload_bytes: Vec<u8>,
    surface_texture: UiShaderSurfaceTexturePayload,
}

#[derive(Clone)]
struct PresentedShaderSurfaceCallback {
    state: Arc<Mutex<PresentedShaderSurfaceGpuState>>,
}

struct ViewportCameraController {
    position: Vec3,
    yaw: f32,
    pitch: f32,
    move_speed: f32,
    vertical_velocity: f32,
    eye_height: f32,
    grounded: bool,
}

#[derive(Clone, Debug, Default)]
struct ViewportInputSnapshot {
    scroll_delta_y: f32,
    pointer_delta: Vec2,
    modifier_ctrl: bool,
    modifier_alt: bool,
    pressed_hotkeys: BTreeSet<String>,
    move_forward: bool,
    move_backward: bool,
    move_right: bool,
    move_left: bool,
    move_up: bool,
    move_down: bool,
    speed_boost: bool,
    recenter: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ViewportManipulatorDragTrigger {
    PrimaryDrag,
    CtrlPrimaryDrag,
    AltPrimaryDrag,
}

#[derive(Clone, Copy, Debug)]
struct ViewportGizmoSnapSettings {
    translation_step: Option<f32>,
    rotation_step_radians: Option<f32>,
    scale_step: Option<f32>,
}

impl ViewportCameraController {
    fn from_pose(pose: &CameraPose) -> Self {
        let forward = pose.forward();
        Self {
            position: pose.position,
            yaw: forward.z.atan2(forward.x),
            pitch: forward.y.clamp(-0.999, 0.999).asin(),
            move_speed: 7.5,
            vertical_velocity: 0.0,
            eye_height: 1.7,
            grounded: false,
        }
    }

    fn pose(&self, reference: &CameraPose) -> CameraPose {
        CameraPose {
            position: self.position,
            target: self.position + self.forward(),
            up: Vec3::UP,
            fov_y_degrees: reference.fov_y_degrees,
            near_plane: reference.near_plane,
            far_plane: reference.far_plane,
        }
    }

    fn forward(&self) -> Vec3 {
        Vec3::new(
            self.yaw.cos() * self.pitch.cos(),
            self.pitch.sin(),
            self.yaw.sin() * self.pitch.cos(),
        )
        .normalize()
    }

    fn planar_forward(&self) -> Vec3 {
        let forward = self.forward();
        let planar = Vec3::new(forward.x, 0.0, forward.z);
        if planar.length() <= f32::EPSILON {
            Vec3::new(self.yaw.cos(), 0.0, self.yaw.sin()).normalize()
        } else {
            planar.normalize()
        }
    }

    fn right(&self) -> Vec3 {
        Vec3::UP.cross(self.planar_forward()).normalize()
    }

    fn recenter(&mut self, pose: &CameraPose) {
        *self = Self::from_pose(pose);
    }
}

fn default_viewport_gizmo_binding() -> RealtimeViewportGizmoBinding {
    RealtimeViewportGizmoBinding {
        profile_id: Some("viewport.default".to_string()),
        visible: Some(true),
        default_mode: Some("translate".to_string()),
        default_space: Some("world".to_string()),
        drag_trigger: Some("ctrl_primary_drag".to_string()),
        selection_required: Some(true),
        translate_hotkey: Some("T".to_string()),
        rotate_hotkey: Some("R".to_string()),
        scale_hotkey: Some("Y".to_string()),
        cycle_space_hotkey: Some("U".to_string()),
        toggle_snap_hotkey: Some("I".to_string()),
        translate_snap_units: Some(0.5),
        rotate_snap_degrees: Some(15.0),
        scale_snap_percent: Some(10.0),
        snap_default_enabled: Some(false),
    }
}

fn resolved_viewport_gizmo_binding(
    presentation: Option<&RealtimeViewportPresentationBinding>,
) -> RealtimeViewportGizmoBinding {
    let mut resolved = default_viewport_gizmo_binding();
    let Some(gizmo) = presentation.and_then(|presentation| presentation.gizmo.as_ref()) else {
        return resolved;
    };
    if gizmo.profile_id.is_some() {
        resolved.profile_id = gizmo.profile_id.clone();
    }
    if gizmo.visible.is_some() {
        resolved.visible = gizmo.visible;
    }
    if gizmo.default_mode.is_some() {
        resolved.default_mode = gizmo.default_mode.clone();
    }
    if gizmo.default_space.is_some() {
        resolved.default_space = gizmo.default_space.clone();
    }
    if gizmo.drag_trigger.is_some() {
        resolved.drag_trigger = gizmo.drag_trigger.clone();
    }
    if gizmo.selection_required.is_some() {
        resolved.selection_required = gizmo.selection_required;
    }
    if gizmo.translate_hotkey.is_some() {
        resolved.translate_hotkey = gizmo.translate_hotkey.clone();
    }
    if gizmo.rotate_hotkey.is_some() {
        resolved.rotate_hotkey = gizmo.rotate_hotkey.clone();
    }
    if gizmo.scale_hotkey.is_some() {
        resolved.scale_hotkey = gizmo.scale_hotkey.clone();
    }
    if gizmo.cycle_space_hotkey.is_some() {
        resolved.cycle_space_hotkey = gizmo.cycle_space_hotkey.clone();
    }
    if gizmo.toggle_snap_hotkey.is_some() {
        resolved.toggle_snap_hotkey = gizmo.toggle_snap_hotkey.clone();
    }
    if gizmo.translate_snap_units.is_some() {
        resolved.translate_snap_units = gizmo.translate_snap_units;
    }
    if gizmo.rotate_snap_degrees.is_some() {
        resolved.rotate_snap_degrees = gizmo.rotate_snap_degrees;
    }
    if gizmo.scale_snap_percent.is_some() {
        resolved.scale_snap_percent = gizmo.scale_snap_percent;
    }
    if gizmo.snap_default_enabled.is_some() {
        resolved.snap_default_enabled = gizmo.snap_default_enabled;
    }
    resolved
}

fn manipulator_mode_from_binding_value(value: Option<&str>) -> ManipulatorMode {
    match value
        .unwrap_or("translate")
        .trim()
        .to_ascii_lowercase()
        .as_str()
    {
        "rotate" => ManipulatorMode::Rotate,
        "scale" => ManipulatorMode::Scale,
        _ => ManipulatorMode::Translate,
    }
}

fn manipulator_space_from_binding_value(value: Option<&str>) -> ManipulatorSpace {
    match value
        .unwrap_or("world")
        .trim()
        .to_ascii_lowercase()
        .as_str()
    {
        "local" => ManipulatorSpace::Local,
        _ => ManipulatorSpace::World,
    }
}

fn manipulator_mode_label(mode: ManipulatorMode) -> &'static str {
    match mode {
        ManipulatorMode::Translate => "translate",
        ManipulatorMode::Rotate => "rotate",
        ManipulatorMode::Scale => "scale",
    }
}

fn manipulator_space_label(space: ManipulatorSpace) -> &'static str {
    match space {
        ManipulatorSpace::World => "world",
        ManipulatorSpace::Local => "local",
    }
}

fn viewport_drag_trigger_from_binding_value(value: Option<&str>) -> ViewportManipulatorDragTrigger {
    match value
        .unwrap_or("ctrl_primary_drag")
        .trim()
        .to_ascii_lowercase()
        .as_str()
    {
        "primary_drag" => ViewportManipulatorDragTrigger::PrimaryDrag,
        "alt_primary_drag" => ViewportManipulatorDragTrigger::AltPrimaryDrag,
        _ => ViewportManipulatorDragTrigger::CtrlPrimaryDrag,
    }
}

fn viewport_drag_trigger_active(
    input: &ViewportInputSnapshot,
    drag_trigger: ViewportManipulatorDragTrigger,
) -> bool {
    match drag_trigger {
        ViewportManipulatorDragTrigger::PrimaryDrag => true,
        ViewportManipulatorDragTrigger::CtrlPrimaryDrag => input.modifier_ctrl,
        ViewportManipulatorDragTrigger::AltPrimaryDrag => input.modifier_alt,
    }
}

fn viewport_drag_trigger_label(drag_trigger: ViewportManipulatorDragTrigger) -> &'static str {
    match drag_trigger {
        ViewportManipulatorDragTrigger::PrimaryDrag => "drag",
        ViewportManipulatorDragTrigger::CtrlPrimaryDrag => "ctrl+drag",
        ViewportManipulatorDragTrigger::AltPrimaryDrag => "alt+drag",
    }
}

fn viewport_hotkey_label(value: Option<&str>, fallback: &str) -> String {
    value
        .map(str::trim)
        .filter(|token| !token.is_empty())
        .map(|token| token.to_ascii_uppercase())
        .unwrap_or_else(|| fallback.to_string())
}

fn normalize_viewport_hotkey_token(value: &str) -> Option<String> {
    let normalized = value.trim().to_ascii_uppercase();
    (!normalized.is_empty()).then_some(normalized)
}

fn viewport_hotkey_pressed(input: &ViewportInputSnapshot, hotkey: Option<&str>) -> bool {
    hotkey
        .and_then(normalize_viewport_hotkey_token)
        .is_some_and(|token| input.pressed_hotkeys.contains(&token))
}

fn capture_viewport_pressed_hotkeys(input: &egui::InputState) -> BTreeSet<String> {
    const VIEWPORT_HOTKEYS: &[(egui::Key, &str)] = &[
        (egui::Key::A, "A"),
        (egui::Key::B, "B"),
        (egui::Key::C, "C"),
        (egui::Key::D, "D"),
        (egui::Key::E, "E"),
        (egui::Key::F, "F"),
        (egui::Key::G, "G"),
        (egui::Key::H, "H"),
        (egui::Key::I, "I"),
        (egui::Key::J, "J"),
        (egui::Key::K, "K"),
        (egui::Key::L, "L"),
        (egui::Key::M, "M"),
        (egui::Key::N, "N"),
        (egui::Key::O, "O"),
        (egui::Key::P, "P"),
        (egui::Key::Q, "Q"),
        (egui::Key::R, "R"),
        (egui::Key::S, "S"),
        (egui::Key::T, "T"),
        (egui::Key::U, "U"),
        (egui::Key::V, "V"),
        (egui::Key::W, "W"),
        (egui::Key::X, "X"),
        (egui::Key::Y, "Y"),
        (egui::Key::Z, "Z"),
    ];
    VIEWPORT_HOTKEYS
        .iter()
        .filter_map(|(key, label)| input.key_pressed(*key).then_some((*label).to_string()))
        .collect()
}

fn viewport_gizmo_snap_settings(
    binding: &RealtimeViewportGizmoBinding,
) -> ViewportGizmoSnapSettings {
    ViewportGizmoSnapSettings {
        translation_step: binding
            .translate_snap_units
            .filter(|value| *value > f32::EPSILON),
        rotation_step_radians: binding
            .rotate_snap_degrees
            .filter(|value| value.abs() > f32::EPSILON)
            .map(f32::to_radians),
        scale_step: binding
            .scale_snap_percent
            .filter(|value| value.abs() > f32::EPSILON)
            .map(|value| value / 100.0),
    }
}

fn round_to_step(value: f32, step: f32) -> f32 {
    if step.abs() <= f32::EPSILON {
        value
    } else {
        (value / step).round() * step
    }
}

fn snap_vec3(value: Vec3, step: f32) -> Vec3 {
    Vec3::new(
        round_to_step(value.x, step),
        round_to_step(value.y, step),
        round_to_step(value.z, step),
    )
}

struct KainUiNativeApp {
    config: KainUiNativeAppConfig,
    runtime_settings: KainUiNativeRuntimeSettings,
    runtime: UiRuntime,
    output: UiBuildOutput,
    debug_tree: String,
    app_manifest_path: Option<String>,
    command_bridge_path: Option<String>,
    app_runtime_snapshot: Option<NativeAppRuntimeSnapshot>,
    scene_catalog: SceneCatalog,
    realtime_catalog: RealtimeBundleCatalog,
    shader_bundle: Option<ShaderArtifactBundle>,
    renderer: Box<dyn RenderBackend>,
    active_renderer_label: String,
    presented_viewport_host: Option<PresentedViewportHost>,
    viewport_surfaces: BTreeMap<UiNodeId, ViewportSurfaceState>,
    shader_surfaces: BTreeMap<UiNodeId, ShaderSurfaceState>,
    shader_surface_texture_cache: BTreeMap<String, UiShaderSurfaceTexturePayload>,
    runtime_artifact_watch: RuntimeArtifactWatch,
    viewport_input: ViewportInputSnapshot,
    start_time: Instant,
    last_frame_instant: Instant,
    frame_dt_seconds: f32,
    boot_mode: AppBootMode,
}

#[derive(Clone, Copy, Debug)]
struct NativeNodePresentation {
    opacity: f32,
    translate_y: f32,
}

impl Default for NativeNodePresentation {
    fn default() -> Self {
        Self {
            opacity: 1.0,
            translate_y: 0.0,
        }
    }
}

impl KainUiNativeApp {
    fn new(
        cc: &eframe::CreationContext<'_>,
        config: KainUiNativeAppConfig,
        output: UiBuildOutput,
        boot_mode: AppBootMode,
        runtime_settings: KainUiNativeRuntimeSettings,
    ) -> Self {
        let runtime_bundle_origin = env_var_trimmed(KAIN_UI_NATIVE_RUNTIME_BUNDLE_ENV);
        let realtime_bundle = load_realtime_bundle_from_env();
        let shader_bundle = load_shader_bundle_from_env();
        let app_manifest_path = env_var_trimmed(KAIN_UI_NATIVE_APP_MANIFEST_ENV);
        let command_bridge_path = env_var_trimmed(KAIN_UI_NATIVE_COMMAND_BRIDGE_ENV);
        let runtime_snapshot = load_runtime_snapshot_from_env();
        let realtime_bundle_origin = realtime_bundle.as_ref().map(|(_, path)| path.clone());
        let shader_bundle_origin = shader_bundle.as_ref().map(|(_, path)| path.clone());
        let runtime_snapshot_origin = runtime_snapshot.as_ref().map(|(_, path)| path.clone());
        let realtime_catalog = realtime_bundle
            .as_ref()
            .map(|(bundle, path)| RealtimeBundleCatalog::from_bundle(bundle, Some(path.as_str())))
            .unwrap_or_default();
        let runtime_artifact_watch = RuntimeArtifactWatch::default()
            .with_runtime_bundle_path(runtime_bundle_origin.clone())
            .with_realtime_bundle_path(realtime_bundle_origin.clone())
            .with_shader_bundle_path(shader_bundle_origin.clone())
            .with_runtime_snapshot_path(runtime_snapshot_origin.clone());
        let (renderer, active_renderer_label, presented_viewport_host) = select_viewport_renderer(
            runtime_settings,
            cc,
            shader_bundle.as_ref().map(|(bundle, _)| bundle),
        );
        trace_runtime(format!(
            "app_new: title={} root={} boot_mode={} renderer={} effective_renderer={} inspector={} viewports={} runtime_bundle={} realtime_bundle={} shader_bundle={} app_manifest={} command_bridge={} runtime_snapshot={}",
            config.window_title,
            config.root_component,
            boot_mode.label(),
            runtime_settings.renderer_label(),
            active_renderer_label,
            runtime_settings.show_runtime_inspector,
            runtime_settings.enable_viewports,
            runtime_bundle_origin.as_deref().unwrap_or("<none>"),
            realtime_bundle_origin
                .as_deref()
                .unwrap_or("<none>"),
            shader_bundle_origin
                .as_deref()
                .unwrap_or("<none>"),
            app_manifest_path.as_deref().unwrap_or("<none>"),
            command_bridge_path.as_deref().unwrap_or("<none>"),
            runtime_snapshot_origin.as_deref().unwrap_or("<none>"),
        ));
        let runtime = UiRuntime::new(output.tree.clone(), output.systems.clone());
        let debug_tree = render_ui_output_debug(&output);
        Self {
            config,
            runtime_settings,
            runtime,
            output,
            debug_tree,
            app_manifest_path,
            command_bridge_path,
            app_runtime_snapshot: runtime_snapshot.map(|(snapshot, _)| snapshot),
            scene_catalog: SceneCatalog::default(),
            realtime_catalog,
            shader_bundle: shader_bundle.map(|(bundle, _)| bundle),
            renderer,
            active_renderer_label,
            presented_viewport_host,
            viewport_surfaces: BTreeMap::new(),
            shader_surfaces: BTreeMap::new(),
            shader_surface_texture_cache: BTreeMap::new(),
            runtime_artifact_watch,
            viewport_input: ViewportInputSnapshot::default(),
            start_time: Instant::now(),
            last_frame_instant: Instant::now(),
            frame_dt_seconds: 1.0 / 60.0,
            boot_mode,
        }
    }

    fn runtime_output_snapshot(&self) -> UiBuildOutput {
        UiBuildOutput {
            tree: self.runtime.tree.clone(),
            patches: Vec::new(),
            systems: self.runtime.systems.clone(),
        }
    }

    fn sync_render_output_from_runtime(&mut self, patches: Vec<UiPatch>) {
        self.output.tree = self.runtime.tree.clone();
        self.output.systems = self.runtime.systems.clone();
        self.output.patches = patches;
        self.debug_tree = render_ui_output_debug(&self.output);
    }

    fn sync_runtime_from_render_output(&mut self) {
        self.runtime.tree = self.output.tree.clone();
        self.runtime.systems = self.output.systems.clone();
        self.runtime.rebuild_indexes();
    }

    fn remember_active_tab_layout(&mut self, group_id: &str, layout_id: String) {
        self.runtime
            .systems
            .workspace_layout
            .active_tabs
            .insert(group_id.to_string(), layout_id.clone());
        self.output
            .systems
            .workspace_layout
            .active_tabs
            .insert(group_id.to_string(), layout_id);
    }

    fn emit_runtime_command(&self, command: &NativeAppRuntimeCommand) {
        let Some(bridge_path) = self.command_bridge_path.as_deref() else {
            trace_runtime(format!(
                "command_bridge: skipped {} because {} is unset",
                command.id, KAIN_UI_NATIVE_COMMAND_BRIDGE_ENV
            ));
            return;
        };
        match emit_runtime_command_to_bridge(bridge_path, command) {
            Ok(()) => trace_runtime(format!(
                "command_bridge: queued {} -> {}",
                command.id, bridge_path
            )),
            Err(err) => trace_runtime(format!(
                "command_bridge: failed {} -> {} error={}",
                command.id, bridge_path, err
            )),
        }
    }

    fn render_runtime_command_bar(
        &self,
        ui: &mut egui::Ui,
        app_theme: &NativeAppTheme,
        snapshot: &NativeAppRuntimeSnapshot,
    ) {
        if snapshot.commands.is_empty() {
            return;
        }

        let bridge_ready = self.command_bridge_path.is_some();
        let latest_command_id = snapshot
            .dcc_suite_state
            .as_ref()
            .and_then(|state| state.latest_command.as_ref())
            .map(|command| command.command_id.as_str());
        let latest_command_label = snapshot
            .dcc_suite_state
            .as_ref()
            .and_then(|state| state.latest_command.as_ref())
            .map(|command| {
                if command.label.is_empty() {
                    command.command_id.as_str()
                } else {
                    command.label.as_str()
                }
            })
            .unwrap_or("none");

        ui.add_space(8.0);
        ui.horizontal_wrapped(|ui| {
            Frame::new()
                .fill(alpha_tint(app_theme.palette.surface_overlay, 0.82))
                .stroke(Stroke::new(1.0, app_theme.palette.outline_soft))
                .corner_radius(app_theme.metrics.radius_medium)
                .inner_margin(app_theme.metrics.tight_padding)
                .show(ui, |ui| {
                    ui.label(
                        RichText::new(if bridge_ready {
                            format!("command bridge live | latest {latest_command_label}")
                        } else {
                            format!(
                                "command bridge offline | set {}",
                                KAIN_UI_NATIVE_COMMAND_BRIDGE_ENV
                            )
                        })
                        .size(app_theme.typography.small)
                        .color(app_theme.palette.text_muted),
                    );
                });

            for command in &snapshot.commands {
                let is_latest = latest_command_id.is_some_and(|id| id == command.id.as_str());
                let fill = if is_latest {
                    alpha_tint(app_theme.palette.accent, 0.22)
                } else {
                    alpha_tint(app_theme.palette.surface_overlay, 0.78)
                };
                let stroke = if is_latest {
                    app_theme.palette.accent_soft
                } else {
                    app_theme.palette.outline_soft
                };
                let button = egui::Button::new(
                    RichText::new(&command.label)
                        .size(app_theme.typography.small)
                        .color(if bridge_ready {
                            app_theme.palette.text
                        } else {
                            app_theme.palette.text_muted
                        }),
                )
                .fill(fill)
                .stroke(Stroke::new(1.0, stroke))
                .corner_radius(app_theme.metrics.radius_medium)
                .min_size(egui::vec2(92.0, 28.0));
                if ui.add_enabled(bridge_ready, button).clicked() {
                    self.emit_runtime_command(command);
                }
            }
        });
    }

    fn render_dcc_session_badges(
        &self,
        ui: &mut egui::Ui,
        app_theme: &NativeAppTheme,
        snapshot: &NativeAppRuntimeSnapshot,
    ) {
        let Some(dcc_state) = snapshot.dcc_suite_state.as_ref() else {
            return;
        };

        let active_mode = if !dcc_state.derived.active_mode_label.is_empty() {
            dcc_state.derived.active_mode_label.as_str()
        } else {
            dcc_state.session.workspace.active_mode.as_str()
        };
        let active_tool = if !dcc_state.derived.active_tool_label.is_empty() {
            dcc_state.derived.active_tool_label.as_str()
        } else {
            dcc_state.session.tooling.active_tool.as_str()
        };
        let gizmo_summary = if !dcc_state.derived.gizmo_summary.is_empty() {
            dcc_state.derived.gizmo_summary.as_str()
        } else {
            ""
        };
        let dirty_count = dcc_state.session.dirty.active_count();
        let bridge_summary = format!(
            "{} | {} commands",
            if dcc_state.bridge.status.is_empty() {
                "ready"
            } else {
                dcc_state.bridge.status.as_str()
            },
            dcc_state.bridge.processed_command_count
        );

        ui.add_space(6.0);
        ui.horizontal_wrapped(|ui| {
            let mut show_badge = |label: String, fill: Color32, stroke: Color32, text: Color32| {
                Frame::new()
                    .fill(fill)
                    .stroke(Stroke::new(1.0, stroke))
                    .corner_radius(app_theme.metrics.radius_medium)
                    .inner_margin(app_theme.metrics.tight_padding)
                    .show(ui, |ui| {
                        ui.label(
                            RichText::new(label)
                                .size(app_theme.typography.small)
                                .color(text),
                        );
                    });
            };

            if !active_mode.is_empty() {
                show_badge(
                    format!("mode {active_mode}"),
                    alpha_tint(app_theme.palette.surface_overlay, 0.92),
                    app_theme.palette.outline_soft,
                    app_theme.palette.text,
                );
            }
            if !active_tool.is_empty() {
                show_badge(
                    format!("tool {active_tool}"),
                    alpha_tint(app_theme.palette.highlight, 0.12),
                    app_theme.palette.highlight,
                    app_theme.palette.highlight,
                );
            }
            if !gizmo_summary.is_empty() {
                show_badge(
                    format!("gizmo {gizmo_summary}"),
                    alpha_tint(app_theme.palette.accent, 0.12),
                    app_theme.palette.accent_soft,
                    app_theme.palette.accent_soft,
                );
            } else if !dcc_state.session.gizmo.mode.is_empty()
                || !dcc_state.session.gizmo.space.is_empty()
            {
                show_badge(
                    format!(
                        "gizmo {} | {} | snap {}",
                        dcc_state.session.gizmo.mode,
                        dcc_state.session.gizmo.space,
                        if dcc_state.session.gizmo.snap_enabled {
                            "on"
                        } else {
                            "off"
                        }
                    ),
                    alpha_tint(app_theme.palette.accent, 0.12),
                    app_theme.palette.accent_soft,
                    app_theme.palette.accent_soft,
                );
            }
            show_badge(
                format!("frame {}", dcc_state.session.animation.frame),
                alpha_tint(app_theme.palette.surface_overlay, 0.92),
                app_theme.palette.outline_soft,
                app_theme.palette.text_muted,
            );
            show_badge(
                format!("dirty {dirty_count}"),
                alpha_tint(app_theme.palette.highlight, 0.14),
                app_theme.palette.highlight,
                app_theme.palette.highlight,
            );
            show_badge(
                bridge_summary,
                alpha_tint(app_theme.palette.surface_overlay, 0.92),
                app_theme.palette.outline_soft,
                app_theme.palette.text_muted,
            );
        });
    }

    fn poll_runtime_reloads(&mut self) {
        let runtime_bundle_path = self
            .runtime_artifact_watch
            .runtime_bundle
            .as_mut()
            .and_then(WatchedRuntimeFile::take_if_changed);
        if let Some(path) = runtime_bundle_path {
            match load_runtime_bundle_from_env() {
                Some((bundle, _)) => self.apply_runtime_bundle_reload(bundle, &path),
                None => trace_runtime(format!(
                    "runtime_reload: failed to parse runtime bundle {path}"
                )),
            }
        }

        let realtime_bundle_path = self
            .runtime_artifact_watch
            .realtime_bundle
            .as_mut()
            .and_then(WatchedRuntimeFile::take_if_changed);
        if let Some(path) = realtime_bundle_path {
            match load_realtime_bundle_from_env() {
                Some((bundle, bundle_path)) => {
                    self.realtime_catalog =
                        RealtimeBundleCatalog::from_bundle(&bundle, Some(bundle_path.as_str()));
                    self.shader_surface_texture_cache.clear();
                    trace_runtime(format!("runtime_reload: refreshed realtime bundle {path}"));
                }
                None => trace_runtime(format!(
                    "runtime_reload: failed to parse realtime bundle {path}"
                )),
            }
        }

        let shader_bundle_path = self
            .runtime_artifact_watch
            .shader_bundle
            .as_mut()
            .and_then(WatchedRuntimeFile::take_if_changed);
        if let Some(path) = shader_bundle_path {
            match load_shader_bundle_from_env() {
                Some((bundle, _)) => {
                    self.shader_bundle = Some(bundle);
                    self.shader_surfaces.clear();
                    self.shader_surface_texture_cache.clear();
                    self.refresh_viewport_shader_bundle_state();
                    trace_runtime(format!("runtime_reload: refreshed shader bundle {path}"));
                }
                None => trace_runtime(format!(
                    "runtime_reload: failed to parse shader bundle {path}"
                )),
            }
        }

        let runtime_snapshot_path = self
            .runtime_artifact_watch
            .runtime_snapshot
            .as_mut()
            .and_then(WatchedRuntimeFile::take_if_changed);
        if let Some(path) = runtime_snapshot_path {
            match load_runtime_snapshot_from_env() {
                Some((snapshot, _)) => {
                    self.app_runtime_snapshot = Some(snapshot);
                    trace_runtime(format!("runtime_reload: refreshed runtime snapshot {path}"));
                }
                None => trace_runtime(format!(
                    "runtime_reload: failed to parse runtime snapshot {path}"
                )),
            }
        }
    }

    fn apply_runtime_bundle_reload(&mut self, bundle: UiRuntimeBundle, path: &str) {
        if let Err(err) = validate_ui_runtime_bundle(&bundle) {
            trace_runtime(format!(
                "runtime_reload: validation failed for runtime bundle {path}: {err}"
            ));
            return;
        }

        let previous_output = self.runtime_output_snapshot();
        let bundle_patches = bundle.output.patches.clone();
        let reload_output = self.runtime.reload(bundle.output);
        let next_output = self.runtime_output_snapshot();
        let previous_viewport_surfaces = std::mem::take(&mut self.viewport_surfaces);
        let previous_shader_surfaces = std::mem::take(&mut self.shader_surfaces);
        self.viewport_surfaces =
            transfer_surface_state_map(&previous_output, &next_output, previous_viewport_surfaces);
        self.shader_surfaces =
            transfer_surface_state_map(&previous_output, &next_output, previous_shader_surfaces);
        self.shader_surface_texture_cache.clear();
        self.sync_render_output_from_runtime(bundle_patches);
        self.config.window_title = bundle.metadata.window_title.clone();
        self.config.root_component = bundle.metadata.root_component.clone();
        self.config.initial_window_size = bundle.metadata.initial_window_size;

        trace_runtime(format!(
            "runtime_reload: applied runtime bundle {} focus={} selection={} docking={} animations={} session={}",
            path,
            reload_output.report.focus_transferred,
            reload_output.report.selection_transferred,
            reload_output.report.docking_transferred,
            reload_output.report.animation_tracks_transferred,
            reload_output.report.session_values_transferred,
        ));
    }

    fn refresh_viewport_shader_bundle_state(&mut self) {
        let (renderer, active_renderer_label, presented_viewport_host) = build_viewport_renderer(
            self.runtime_settings,
            self.presented_viewport_host,
            self.shader_bundle.as_ref(),
        );
        self.renderer = renderer;
        self.active_renderer_label = active_renderer_label;
        self.presented_viewport_host = presented_viewport_host;

        for surface in self.viewport_surfaces.values_mut() {
            surface.texture = None;
            surface.last_render_at = None;
            if let Some(host) = self.presented_viewport_host {
                if let Some(presented_state) = surface.presented_state.as_ref() {
                    if let Ok(mut state) = presented_state.lock() {
                        state.replace_shader_bundle(self.shader_bundle.as_ref());
                        state.prepared_frame = None;
                        state.draw_data = None;
                    }
                } else {
                    surface.presented_state =
                        Some(Arc::new(Mutex::new(PresentedViewportGpuState::new(
                            host.target_format,
                            self.shader_bundle.as_ref(),
                        ))));
                }
            } else {
                surface.presented_state = None;
            }
        }
    }
}

fn resolve_node_animation_progress(output: &UiBuildOutput, node: &UiNode) -> Option<f32> {
    let track_id = format!("animation.node.{}", node.id.0);
    let has_track = output
        .systems
        .animation_tracks
        .iter()
        .any(|track| track.id == track_id && track.target == node.id);
    if !has_track {
        return None;
    }

    Some(
        output
            .systems
            .animation_state
            .get(&track_id)
            .map(|state| state.eased_progress)
            .unwrap_or(0.0)
            .clamp(0.0, 1.0),
    )
}

fn resolve_node_presentation(output: &UiBuildOutput, node: &UiNode) -> NativeNodePresentation {
    let progress = resolve_node_animation_progress(output, node).unwrap_or(1.0);
    NativeNodePresentation {
        opacity: (0.22 + progress * 0.78).clamp(0.0, 1.0),
        translate_y: ((1.0 - progress) * 22.0).max(0.0),
    }
}

fn apply_node_presentation_to_theme(
    mut theme: NativeWidgetTheme,
    presentation: NativeNodePresentation,
) -> NativeWidgetTheme {
    theme.fill = alpha_tint(theme.fill, presentation.opacity);
    theme.stroke = alpha_tint(theme.stroke, presentation.opacity);
    theme.canvas_fill = alpha_tint(theme.canvas_fill, presentation.opacity);
    theme.overlay_fill = alpha_tint(theme.overlay_fill, presentation.opacity);
    theme.header_fill = alpha_tint(theme.header_fill, presentation.opacity);
    theme.header_stroke = alpha_tint(theme.header_stroke, presentation.opacity);
    theme.badge_fill = alpha_tint(theme.badge_fill, presentation.opacity);
    theme.accent = alpha_tint(theme.accent, presentation.opacity);
    theme.title_color = alpha_tint(theme.title_color, presentation.opacity);
    theme.body_color = alpha_tint(theme.body_color, presentation.opacity);
    theme.muted_color = alpha_tint(theme.muted_color, presentation.opacity);
    theme.tag_color = alpha_tint(theme.tag_color, presentation.opacity);
    theme
}

fn apply_node_presentation_to_color(
    color: Color32,
    presentation: NativeNodePresentation,
) -> Color32 {
    alpha_tint(color, presentation.opacity)
}

fn color32_to_rgba_f32(color: Color32) -> [f32; 4] {
    [
        color.r() as f32 / 255.0,
        color.g() as f32 / 255.0,
        color.b() as f32 / 255.0,
        color.a() as f32 / 255.0,
    ]
}

fn split_node_id(node_id: UiNodeId) -> (u32, u32) {
    let raw = node_id.0;
    (raw as u32, (raw >> 32) as u32)
}

fn select_viewport_renderer(
    runtime_settings: KainUiNativeRuntimeSettings,
    cc: &eframe::CreationContext<'_>,
    shader_bundle: Option<&ShaderArtifactBundle>,
) -> (
    Box<dyn RenderBackend>,
    String,
    Option<PresentedViewportHost>,
) {
    let presented_viewport_host =
        cc.wgpu_render_state
            .as_ref()
            .map(|render_state| PresentedViewportHost {
                target_format: render_state.target_format,
            });
    build_viewport_renderer(runtime_settings, presented_viewport_host, shader_bundle)
}

fn build_viewport_renderer(
    runtime_settings: KainUiNativeRuntimeSettings,
    presented_viewport_host: Option<PresentedViewportHost>,
    shader_bundle: Option<&ShaderArtifactBundle>,
) -> (
    Box<dyn RenderBackend>,
    String,
    Option<PresentedViewportHost>,
) {
    match runtime_settings.renderer {
        NativeRendererPreference::Glow => (
            Box::new(SoftwareRenderer::default()),
            "software".to_string(),
            None,
        ),
        NativeRendererPreference::Wgpu => match shader_bundle
            .cloned()
            .map(|bundle| match WgpuRenderer::new_with_shader_bundle(bundle) {
                Ok(renderer) => Ok(renderer),
                Err(err) => {
                    trace_runtime(format!(
                        "viewport_renderer: external_shader_bundle_failed fallback=default_wgpu error={err}"
                    ));
                    WgpuRenderer::new()
                }
            })
            .unwrap_or_else(WgpuRenderer::new)
        {
            Ok(renderer) => {
                let active_renderer_label = if presented_viewport_host.is_some() {
                    "wgpu-surface".to_string()
                } else {
                    "wgpu-readback".to_string()
                };
                (
                    Box::new(renderer),
                    active_renderer_label,
                    presented_viewport_host,
                )
            }
            Err(err) => {
                trace_runtime(format!(
                    "viewport_renderer: requested=wgpu fallback=software error={err}"
                ));
                (
                    Box::new(SoftwareRenderer::default()),
                    "software".to_string(),
                    None,
                )
            }
        },
    }
}

impl PresentedViewportGpuState {
    fn new(
        target_format: wgpu::TextureFormat,
        shader_bundle: Option<&ShaderArtifactBundle>,
    ) -> Self {
        Self {
            target_format,
            shader_bundle: shader_bundle.cloned(),
            resources: None,
            prepared_frame: None,
            draw_data: None,
        }
    }

    fn replace_shader_bundle(&mut self, shader_bundle: Option<&ShaderArtifactBundle>) {
        self.shader_bundle = shader_bundle.cloned();
        self.resources = None;
    }
}

impl CallbackTrait for PresentedViewportCallback {
    fn prepare(
        &self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        _screen_descriptor: &ScreenDescriptor,
        _egui_encoder: &mut wgpu::CommandEncoder,
        _callback_resources: &mut egui_wgpu::CallbackResources,
    ) -> Vec<wgpu::CommandBuffer> {
        let Ok(mut state) = self.state.lock() else {
            return Vec::new();
        };
        if state.resources.is_none() {
            match build_presented_viewport_resources(
                device,
                state.target_format,
                state.shader_bundle.as_ref(),
            ) {
                Ok(resources) => state.resources = Some(resources),
                Err(err) => {
                    trace_runtime(format!("viewport_presented: init_failed error={err}"));
                    return Vec::new();
                }
            }
        }
        let Some(resources) = state.resources.as_ref() else {
            return Vec::new();
        };
        let Some(prepared_frame) = state.prepared_frame.as_ref() else {
            return Vec::new();
        };
        queue.write_buffer(
            &resources.uniform_buffer,
            0,
            bytemuck::bytes_of(&prepared_frame.uniforms),
        );
        state.draw_data = Some(build_presented_draw_data(device, prepared_frame));
        Vec::new()
    }

    fn paint(
        &self,
        info: egui::PaintCallbackInfo,
        render_pass: &mut wgpu::RenderPass<'static>,
        _callback_resources: &egui_wgpu::CallbackResources,
    ) {
        let Ok(state) = self.state.lock() else {
            return;
        };
        let Some(resources) = state.resources.as_ref() else {
            return;
        };
        let Some(draw_data) = state.draw_data.as_ref() else {
            return;
        };
        let viewport = info.viewport_in_pixels();
        let clip = info.clip_rect_in_pixels();
        render_pass.set_viewport(
            viewport.left_px as f32,
            viewport.top_px as f32,
            viewport.width_px as f32,
            viewport.height_px as f32,
            0.0,
            1.0,
        );
        render_pass.set_scissor_rect(
            clip.left_px.max(0) as u32,
            clip.top_px.max(0) as u32,
            clip.width_px.max(1) as u32,
            clip.height_px.max(1) as u32,
        );
        render_pass.set_bind_group(0, &resources.uniform_bind_group, &[]);
        render_pass.set_pipeline(&resources.background_pipeline);
        render_pass.draw(0..3, 0..1);

        if let Some(buffer) = draw_data.scene_buffer.as_ref() {
            render_pass.set_pipeline(&resources.scene_pipeline);
            render_pass.set_vertex_buffer(0, buffer.slice(..));
            render_pass.draw(0..draw_data.scene_len, 0..1);
        }
        if let Some(buffer) = draw_data.depth_particle_buffer.as_ref() {
            render_pass.set_pipeline(&resources.particle_depth_pipeline);
            render_pass.set_vertex_buffer(0, buffer.slice(..));
            render_pass.draw(0..draw_data.depth_particle_len, 0..1);
        }
        if let Some(buffer) = draw_data.overlay_particle_buffer.as_ref() {
            render_pass.set_pipeline(&resources.particle_overlay_pipeline);
            render_pass.set_vertex_buffer(0, buffer.slice(..));
            render_pass.draw(0..draw_data.overlay_particle_len, 0..1);
        }
        if let Some(buffer) = draw_data.gizmo_buffer.as_ref() {
            render_pass.set_pipeline(&resources.gizmo_pipeline);
            render_pass.set_vertex_buffer(0, buffer.slice(..));
            render_pass.draw(0..draw_data.gizmo_len, 0..1);
        }
    }
}

impl ShaderSurfaceState {
    fn new(target_format: Option<wgpu::TextureFormat>) -> Self {
        Self {
            presented_state: target_format
                .map(|format| Arc::new(Mutex::new(PresentedShaderSurfaceGpuState::new(format)))),
            last_signature: None,
            last_shader_ref: None,
            last_warning: None,
        }
    }
}

impl PresentedShaderSurfaceGpuState {
    fn new(target_format: wgpu::TextureFormat) -> Self {
        Self {
            target_format,
            resources: None,
            pending_frame: None,
        }
    }
}

impl CallbackTrait for PresentedShaderSurfaceCallback {
    fn prepare(
        &self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        _screen_descriptor: &ScreenDescriptor,
        _egui_encoder: &mut wgpu::CommandEncoder,
        _callback_resources: &mut egui_wgpu::CallbackResources,
    ) -> Vec<wgpu::CommandBuffer> {
        let Ok(mut state) = self.state.lock() else {
            return Vec::new();
        };
        let Some(frame) = state.pending_frame.as_ref().cloned() else {
            return Vec::new();
        };
        let resources_need_rebuild = state
            .resources
            .as_ref()
            .is_none_or(|resources| resources.signature != frame.signature);
        if resources_need_rebuild {
            match build_presented_shader_surface_resources(device, state.target_format, &frame) {
                Ok(resources) => state.resources = Some(resources),
                Err(err) => {
                    trace_runtime(format!(
                        "shader_surface_presented: init_failed shader={} error={err}",
                        frame.descriptor.shader_ref
                    ));
                    return Vec::new();
                }
            }
        }
        let Some(resources) = state.resources.as_ref() else {
            return Vec::new();
        };
        if let Some(buffer) = resources.uniform_buffer.as_ref() {
            queue.write_buffer(buffer, 0, bytemuck::bytes_of(&frame.uniforms));
        }
        if let Some(buffer) = resources.storage_buffer.as_ref() {
            queue.write_buffer(buffer, 0, frame.storage_payload_bytes.as_slice());
        }
        if let Some(texture) = resources.surface_texture.as_ref() {
            queue.write_texture(
                wgpu::TexelCopyTextureInfo {
                    texture,
                    mip_level: 0,
                    origin: wgpu::Origin3d::ZERO,
                    aspect: wgpu::TextureAspect::All,
                },
                frame.surface_texture.rgba8.as_ref(),
                wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(4 * frame.surface_texture.size_px[0].max(1)),
                    rows_per_image: Some(frame.surface_texture.size_px[1].max(1)),
                },
                wgpu::Extent3d {
                    width: frame.surface_texture.size_px[0].max(1),
                    height: frame.surface_texture.size_px[1].max(1),
                    depth_or_array_layers: 1,
                },
            );
        }
        Vec::new()
    }

    fn paint(
        &self,
        info: egui::PaintCallbackInfo,
        render_pass: &mut wgpu::RenderPass<'static>,
        _callback_resources: &egui_wgpu::CallbackResources,
    ) {
        let Ok(state) = self.state.lock() else {
            return;
        };
        let Some(resources) = state.resources.as_ref() else {
            return;
        };
        let viewport = info.viewport_in_pixels();
        let clip = info.clip_rect_in_pixels();
        render_pass.set_viewport(
            viewport.left_px as f32,
            viewport.top_px as f32,
            viewport.width_px as f32,
            viewport.height_px as f32,
            0.0,
            1.0,
        );
        render_pass.set_scissor_rect(
            clip.left_px.max(0) as u32,
            clip.top_px.max(0) as u32,
            clip.width_px.max(1) as u32,
            clip.height_px.max(1) as u32,
        );
        render_pass.set_pipeline(&resources.pipeline);
        if let Some(bind_group) = resources.bind_group.as_ref() {
            render_pass.set_bind_group(0, bind_group, &[]);
        }
        render_pass.draw(0..3, 0..1);
    }
}

fn build_presented_shader_surface_resources(
    device: &wgpu::Device,
    target_format: wgpu::TextureFormat,
    frame: &PresentedShaderSurfaceFrame,
) -> Result<PresentedShaderSurfaceGpuResources, String> {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        let vertex_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("kain-ui-native-surface-vertex"),
            source: wgpu::ShaderSource::Wgsl(UI_SHADER_SURFACE_FULLSCREEN_VERTEX_WGSL.into()),
        });
        let fragment_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("kain-ui-native-surface-fragment"),
            source: wgpu::ShaderSource::Wgsl(frame.descriptor.wgsl_source.clone().into()),
        });

        let mut resource_layouts = frame.descriptor.resource_layouts.clone();
        resource_layouts.sort_by_key(|layout| (layout.descriptor_set, layout.binding));

        let needs_uniform_buffer = resource_layouts.iter().any(|layout| {
            layout.descriptor_set == 0
                && matches!(
                    shader_binding_kind(layout),
                    Some(UiShaderBindingKind::UniformBuffer)
                )
        });
        let needs_storage_buffer = resource_layouts.iter().any(|layout| {
            layout.descriptor_set == 0
                && matches!(
                    shader_binding_kind(layout),
                    Some(UiShaderBindingKind::StorageBuffer)
                )
        });
        let needs_texture = resource_layouts.iter().any(|layout| {
            layout.descriptor_set == 0
                && matches!(
                    shader_binding_kind(layout),
                    Some(UiShaderBindingKind::Texture2d)
                )
        });
        let needs_sampler = resource_layouts.iter().any(|layout| {
            layout.descriptor_set == 0
                && matches!(
                    shader_binding_kind(layout),
                    Some(UiShaderBindingKind::Sampler)
                )
        });

        let uniform_buffer = needs_uniform_buffer.then(|| {
            device.create_buffer(&wgpu::BufferDescriptor {
                label: Some("kain-ui-native-surface-uniforms"),
                size: 4096,
                usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
                mapped_at_creation: false,
            })
        });
        let storage_buffer = needs_storage_buffer.then(|| {
            device.create_buffer(&wgpu::BufferDescriptor {
                label: Some("kain-ui-native-surface-storage"),
                size: frame.storage_payload_bytes.len().max(256) as u64,
                usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_DST,
                mapped_at_creation: false,
            })
        });
        let surface_texture = needs_texture.then(|| {
            device.create_texture(&wgpu::TextureDescriptor {
                label: Some("kain-ui-native-surface-texture"),
                size: wgpu::Extent3d {
                    width: frame.surface_texture.size_px[0].max(1),
                    height: frame.surface_texture.size_px[1].max(1),
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8UnormSrgb,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            })
        });
        let surface_texture_view = surface_texture
            .as_ref()
            .map(|texture| texture.create_view(&wgpu::TextureViewDescriptor::default()));
        let surface_sampler = needs_sampler.then(|| {
            device.create_sampler(&wgpu::SamplerDescriptor {
                label: Some("kain-ui-native-surface-sampler"),
                mag_filter: wgpu::FilterMode::Linear,
                min_filter: wgpu::FilterMode::Linear,
                mipmap_filter: wgpu::FilterMode::Linear,
                ..Default::default()
            })
        });

        let mut bind_group_layout_entries = Vec::new();
        let mut bind_group_entries = Vec::new();

        for layout in &resource_layouts {
            if layout.descriptor_set != 0 {
                continue;
            }
            match shader_binding_kind(layout) {
                Some(UiShaderBindingKind::UniformBuffer) => {
                    let buffer = uniform_buffer.as_ref().ok_or_else(|| {
                        "shader surface uniform buffer was not created".to_string()
                    })?;
                    bind_group_layout_entries.push(wgpu::BindGroupLayoutEntry {
                        binding: layout.binding,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Buffer {
                            ty: wgpu::BufferBindingType::Uniform,
                            has_dynamic_offset: false,
                            min_binding_size: None,
                        },
                        count: None,
                    });
                    bind_group_entries.push(wgpu::BindGroupEntry {
                        binding: layout.binding,
                        resource: buffer.as_entire_binding(),
                    });
                }
                Some(UiShaderBindingKind::StorageBuffer) => {
                    let buffer = storage_buffer.as_ref().ok_or_else(|| {
                        "shader surface storage buffer was not created".to_string()
                    })?;
                    bind_group_layout_entries.push(wgpu::BindGroupLayoutEntry {
                        binding: layout.binding,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Buffer {
                            ty: wgpu::BufferBindingType::Storage { read_only: false },
                            has_dynamic_offset: false,
                            min_binding_size: None,
                        },
                        count: None,
                    });
                    bind_group_entries.push(wgpu::BindGroupEntry {
                        binding: layout.binding,
                        resource: buffer.as_entire_binding(),
                    });
                }
                Some(UiShaderBindingKind::Texture2d) => {
                    let view = surface_texture_view
                        .as_ref()
                        .ok_or_else(|| "shader surface texture view was not created".to_string())?;
                    bind_group_layout_entries.push(wgpu::BindGroupLayoutEntry {
                        binding: layout.binding,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Texture {
                            sample_type: wgpu::TextureSampleType::Float { filterable: true },
                            view_dimension: wgpu::TextureViewDimension::D2,
                            multisampled: false,
                        },
                        count: None,
                    });
                    bind_group_entries.push(wgpu::BindGroupEntry {
                        binding: layout.binding,
                        resource: wgpu::BindingResource::TextureView(view),
                    });
                }
                Some(UiShaderBindingKind::Sampler) => {
                    let sampler = surface_sampler
                        .as_ref()
                        .ok_or_else(|| "shader surface sampler was not created".to_string())?;
                    bind_group_layout_entries.push(wgpu::BindGroupLayoutEntry {
                        binding: layout.binding,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                        count: None,
                    });
                    bind_group_entries.push(wgpu::BindGroupEntry {
                        binding: layout.binding,
                        resource: wgpu::BindingResource::Sampler(sampler),
                    });
                }
                None => {}
            }
        }

        let bind_group_layout = if bind_group_layout_entries.is_empty() {
            None
        } else {
            Some(
                device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                    label: Some("kain-ui-native-surface-bind-group-layout"),
                    entries: &bind_group_layout_entries,
                }),
            )
        };

        let bind_group_layout_refs = bind_group_layout.iter().collect::<Vec<_>>();
        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("kain-ui-native-surface-pipeline-layout"),
            bind_group_layouts: &bind_group_layout_refs,
            push_constant_ranges: &[],
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("kain-ui-native-surface-pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &vertex_shader,
                entry_point: Some(UI_SHADER_SURFACE_VERTEX_ENTRY),
                buffers: &[],
                compilation_options: wgpu::PipelineCompilationOptions::default(),
            },
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &fragment_shader,
                entry_point: Some(frame.descriptor.fragment_entry_point.as_str()),
                targets: &[Some(wgpu::ColorTargetState {
                    format: target_format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
                compilation_options: wgpu::PipelineCompilationOptions::default(),
            }),
            multiview: None,
            cache: None,
        });

        let bind_group = bind_group_layout.as_ref().map(|layout| {
            device.create_bind_group(&wgpu::BindGroupDescriptor {
                label: Some("kain-ui-native-surface-bind-group"),
                layout,
                entries: &bind_group_entries,
            })
        });

        Ok(PresentedShaderSurfaceGpuResources {
            signature: frame.signature.clone(),
            pipeline,
            bind_group,
            uniform_buffer,
            storage_buffer,
            surface_texture,
            _surface_texture_view: surface_texture_view,
            _surface_sampler: surface_sampler,
        })
    }))
    .map_err(|_| {
        format!(
            "wgpu surface pipeline creation panicked for shader {}",
            frame.descriptor.shader_ref
        )
    })?
}

fn build_ui_shader_surface_storage_bytes(
    header: &UiShaderSurfaceStoragePayload,
    descriptor: &ResolvedUiShaderSurface,
    surface_texture: &UiShaderSurfaceTexturePayload,
    theme: &NativeWidgetTheme,
) -> Vec<u8> {
    let mut atlas_records = Vec::new();
    let mut atlas_glyph_bytes = Vec::new();
    let mut atlas_indices = BTreeMap::new();

    for (atlas_index, atlas) in descriptor.font_atlases.iter().enumerate() {
        atlas_indices.insert(atlas.key.clone(), atlas_index as u32);
        let glyph_text = sanitize_shader_canvas_text(&atlas.glyphs);
        let glyph_start = atlas_glyph_bytes.len() as u32;
        atlas_glyph_bytes.extend_from_slice(glyph_text.as_bytes());
        let rasterized_atlas = surface_texture.atlases.get(atlas_index);
        atlas_records.push(UiShaderSurfaceFontAtlasRecord {
            atlas_origin_px: rasterized_atlas
                .map(|entry| entry.origin_px)
                .unwrap_or([0, 0]),
            texture_size_px: rasterized_atlas
                .map(|entry| entry.texture_size_px)
                .unwrap_or(atlas.texture_size_px),
            cell_size_px: atlas.cell_size_px,
            columns: atlas.columns,
            rows: atlas.rows,
            encoding_tag: rasterized_atlas
                .map(|entry| entry.encoding_tag)
                .unwrap_or(shader_canvas_font_atlas_encoding_tag(&atlas.encoding)),
            distance_range_px: rasterized_atlas
                .map(|entry| entry.distance_range_px)
                .unwrap_or(atlas.distance_range_px),
            glyph_start,
            glyph_count: glyph_text.len() as u32,
        });
    }

    let mut text_records = Vec::new();
    let mut text_bytes = Vec::new();
    for run in &descriptor.text_runs {
        let sanitized_text = sanitize_shader_canvas_text(&run.text);
        let text_start = text_bytes.len() as u32;
        text_bytes.extend_from_slice(sanitized_text.as_bytes());
        text_records.push(UiShaderSurfaceTextRunRecord {
            origin_px: [run.origin_px[0] as f32, run.origin_px[1] as f32],
            atlas_index: atlas_indices
                .get(&run.atlas_key)
                .copied()
                .unwrap_or_default(),
            text_start,
            text_len: sanitized_text.len() as u32,
            role_tag: shader_canvas_text_role_tag(&run.role),
            _pad0: [0; 3],
            color_rgba: color32_to_rgba_f32(resolve_shader_canvas_text_color(
                theme,
                &run.color_token,
                &run.role,
            )),
        });
    }

    let mut header = *header;
    header.font_atlas_count = atlas_records.len() as u32;
    header.text_run_count = text_records.len() as u32;
    header.resource_binding_count = descriptor.resource_bindings.len() as u32;
    header.atlas_glyph_bytes = atlas_glyph_bytes.len() as u32;
    header.text_utf8_bytes = text_bytes.len() as u32;

    let mut bytes = Vec::with_capacity(
        std::mem::size_of::<UiShaderSurfaceStoragePayload>()
            + (atlas_records.len() * std::mem::size_of::<UiShaderSurfaceFontAtlasRecord>())
            + (text_records.len() * std::mem::size_of::<UiShaderSurfaceTextRunRecord>())
            + atlas_glyph_bytes.len()
            + text_bytes.len(),
    );
    bytes.extend_from_slice(bytemuck::bytes_of(&header));
    if !atlas_records.is_empty() {
        bytes.extend_from_slice(bytemuck::cast_slice(atlas_records.as_slice()));
    }
    if !text_records.is_empty() {
        bytes.extend_from_slice(bytemuck::cast_slice(text_records.as_slice()));
    }
    bytes.extend_from_slice(&atlas_glyph_bytes);
    bytes.extend_from_slice(&text_bytes);
    bytes
}

fn resolve_ui_shader_surface_texture_payload(
    cache: &mut BTreeMap<String, UiShaderSurfaceTexturePayload>,
    descriptor: &ResolvedUiShaderSurface,
) -> UiShaderSurfaceTexturePayload {
    let signature = shader_surface_texture_payload_signature(descriptor);
    if let Some(payload) = cache.get(&signature) {
        return payload.clone();
    }

    let payload = build_ui_shader_surface_texture_payload(descriptor);
    cache.insert(signature, payload.clone());
    payload
}

fn shader_surface_texture_payload_signature(descriptor: &ResolvedUiShaderSurface) -> String {
    let mut hasher = DefaultHasher::new();
    for atlas in &descriptor.font_atlases {
        atlas.key.hash(&mut hasher);
        atlas.family.hash(&mut hasher);
        atlas.asset_key.hash(&mut hasher);
        atlas.encoding.hash(&mut hasher);
        atlas.distance_range_px.hash(&mut hasher);
        atlas.glyphs.hash(&mut hasher);
        atlas.cell_size_px.hash(&mut hasher);
        atlas.texture_size_px.hash(&mut hasher);
        atlas.columns.hash(&mut hasher);
        atlas.rows.hash(&mut hasher);
        descriptor
            .font_asset_sources_by_key
            .get(&atlas.key)
            .hash(&mut hasher);
    }
    format!("atlas:{:016x}", hasher.finish())
}

fn build_ui_shader_surface_texture_payload(
    descriptor: &ResolvedUiShaderSurface,
) -> UiShaderSurfaceTexturePayload {
    if descriptor.font_atlases.is_empty() {
        return UiShaderSurfaceTexturePayload {
            size_px: [1, 1],
            atlases: Vec::new(),
            rgba8: Arc::from(vec![255, 255, 255, 255]),
        };
    }

    let rasterized_atlases = descriptor
        .font_atlases
        .iter()
        .map(|atlas| {
            rasterize_shader_surface_font_atlas(
                atlas,
                descriptor
                    .font_asset_sources_by_key
                    .get(&atlas.key)
                    .map(String::as_str),
            )
        })
        .collect::<Vec<_>>();
    let packed_width = rasterized_atlases
        .iter()
        .map(|atlas| atlas.texture_size_px[0].max(1))
        .max()
        .unwrap_or(1);
    let packed_height = rasterized_atlases
        .iter()
        .map(|atlas| atlas.texture_size_px[1].max(1))
        .sum::<u32>()
        .max(1);
    let mut rgba8 = vec![0u8; (packed_width * packed_height * 4) as usize];
    let mut atlases = Vec::with_capacity(rasterized_atlases.len());
    let mut atlas_y = 0u32;

    for rasterized_atlas in &rasterized_atlases {
        let atlas_width = rasterized_atlas.texture_size_px[0].max(1);
        let atlas_height = rasterized_atlas.texture_size_px[1].max(1);
        atlases.push(UiShaderSurfaceTextureAtlasPayload {
            origin_px: [0, atlas_y],
            texture_size_px: rasterized_atlas.texture_size_px,
            encoding_tag: shader_canvas_font_atlas_encoding_tag(&rasterized_atlas.encoding),
            distance_range_px: rasterized_atlas.distance_range_px,
        });

        for row in 0..atlas_height as usize {
            let src_offset = row * atlas_width as usize * 4;
            let dst_offset = (((atlas_y as usize + row) * packed_width as usize) * 4) as usize;
            let dst_end = dst_offset + atlas_width as usize * 4;
            rgba8[dst_offset..dst_end].copy_from_slice(
                &rasterized_atlas.rgba8[src_offset..src_offset + atlas_width as usize * 4],
            );
        }

        atlas_y += atlas_height;
    }

    UiShaderSurfaceTexturePayload {
        size_px: [packed_width, packed_height],
        atlases,
        rgba8: Arc::from(rgba8),
    }
}

fn rasterize_shader_surface_font_atlas(
    atlas: &RealtimeShaderCanvasFontAtlas,
    asset_source: Option<&str>,
) -> RasterizedShaderCanvasFontAtlas {
    if let Some(loaded_font) = try_load_shader_canvas_font(asset_source, &atlas.family) {
        if shader_canvas_font_atlas_prefers_mtsdf(atlas) {
            if let Some(mtsdf_atlas) =
                rasterize_shader_surface_font_atlas_with_mtsdf(atlas, &loaded_font)
            {
                return mtsdf_atlas;
            }
        }

        return rasterize_shader_surface_font_atlas_with_ab_glyph(atlas, &loaded_font.font);
    }

    rasterize_shader_surface_font_atlas_bitmap(atlas)
}

fn try_load_shader_canvas_font(
    asset_source: Option<&str>,
    family: &str,
) -> Option<LoadedShaderCanvasFont> {
    if let Some(asset_source) = asset_source {
        let asset_path = PathBuf::from(asset_source);
        if let Ok(bytes) = fs::read(&asset_path) {
            if let Ok(font) = FontArc::try_from_vec(bytes.clone()) {
                return Some(LoadedShaderCanvasFont {
                    font,
                    ttf_bytes: Arc::from(bytes),
                });
            }
        }
    }

    let normalized_family = normalize_shader_canvas_font_family(family);
    if normalized_family.is_empty()
        || normalized_family
            == normalize_shader_canvas_font_family(SHADER_CANVAS_BITMAP_FONT_FAMILY)
    {
        return None;
    }

    for candidate in shader_canvas_font_candidate_paths(family) {
        let Ok(bytes) = fs::read(&candidate) else {
            continue;
        };
        let Ok(font) = FontArc::try_from_vec(bytes.clone()) else {
            continue;
        };
        return Some(LoadedShaderCanvasFont {
            font,
            ttf_bytes: Arc::from(bytes),
        });
    }

    None
}

fn shader_canvas_font_candidate_paths(family: &str) -> Vec<PathBuf> {
    let mut candidates = Vec::new();
    let trimmed_family = family.trim();
    if trimmed_family.is_empty() {
        return candidates;
    }

    let requested_path = PathBuf::from(trimmed_family);
    if requested_path.exists() {
        candidates.push(requested_path);
    }

    let normalized_family = normalize_shader_canvas_font_family(trimmed_family);
    for spec in SHADER_CANVAS_FONT_FAMILY_SPECS {
        if !spec
            .aliases
            .iter()
            .any(|alias| normalize_shader_canvas_font_family(alias) == normalized_family)
        {
            continue;
        }

        for candidate in spec.path_candidates {
            let candidate_path = PathBuf::from(candidate);
            if !candidates
                .iter()
                .any(|existing| existing == &candidate_path)
            {
                candidates.push(candidate_path);
            }
        }
        break;
    }

    candidates
}

fn normalize_shader_canvas_font_family(family: &str) -> String {
    family
        .trim()
        .chars()
        .filter(|ch| ch.is_ascii_alphanumeric())
        .flat_map(|ch| ch.to_lowercase())
        .collect()
}

fn shader_canvas_font_atlas_encoding_tag(encoding: &str) -> u32 {
    match encoding.trim().to_ascii_lowercase().as_str() {
        SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA => {
            SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_BITMAP_ALPHA
        }
        SHADER_CANVAS_FONT_ATLAS_ENCODING_ALPHA_COVERAGE => {
            SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_ALPHA_COVERAGE
        }
        SHADER_CANVAS_FONT_ATLAS_ENCODING_MTSDF_RGBA => {
            SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_MTSDF_RGBA
        }
        _ => SHADER_CANVAS_FONT_ATLAS_ENCODING_TAG_UNKNOWN,
    }
}

fn shader_canvas_font_atlas_prefers_mtsdf(atlas: &RealtimeShaderCanvasFontAtlas) -> bool {
    atlas
        .encoding
        .eq_ignore_ascii_case(SHADER_CANVAS_FONT_ATLAS_ENCODING_MTSDF_RGBA)
        && !atlas
            .family
            .trim()
            .eq_ignore_ascii_case(SHADER_CANVAS_BITMAP_FONT_FAMILY)
}

fn rasterize_shader_surface_font_atlas_with_mtsdf(
    atlas: &RealtimeShaderCanvasFontAtlas,
    loaded_font: &LoadedShaderCanvasFont,
) -> Option<RasterizedShaderCanvasFontAtlas> {
    let face = Face::parse(loaded_font.ttf_bytes.as_ref(), 0).ok()?;
    let width = atlas.texture_size_px[0].max(1);
    let height = atlas.texture_size_px[1].max(1);
    let mut rgba8 = vec![0u8; (width * height * 4) as usize];
    let cell_width = atlas.cell_size_px[0].max(1);
    let cell_height = atlas.cell_size_px[1].max(1);
    let range_px = atlas.distance_range_px.max(1) as f64;
    let fallback_glyph = face.glyph_index('?');

    for (glyph_index, glyph) in atlas.glyphs.chars().enumerate() {
        let columns = atlas.columns.max(1) as usize;
        let cell_x = glyph_index % columns;
        let cell_y = glyph_index / columns;
        if cell_y >= atlas.rows.max(1) as usize || glyph.is_whitespace() {
            continue;
        }

        let Some(glyph_id) = face.glyph_index(glyph).or(fallback_glyph) else {
            continue;
        };
        let Some(glyph_bounds) = face.glyph_bounding_box(glyph_id) else {
            continue;
        };
        let Some(mut shape) = load_shape_from_face(&face, glyph_id) else {
            continue;
        };
        let glyph_width = (glyph_bounds.x_max - glyph_bounds.x_min).max(1) as f64;
        let glyph_height = (glyph_bounds.y_max - glyph_bounds.y_min).max(1) as f64;
        let padding = range_px.min((cell_width.min(cell_height) as f64 * 0.45).max(1.0));
        let drawable_width = (cell_width as f64 - (padding * 2.0)).max(1.0);
        let drawable_height = (cell_height as f64 - (padding * 2.0)).max(1.0);
        let shrinkage = (glyph_width / drawable_width)
            .max(glyph_height / drawable_height)
            .max(1.0);
        let glyph_width_px = glyph_width / shrinkage;
        let glyph_height_px = glyph_height / shrinkage;
        let transformation = nalgebra::convert::<_, Affine2<f64>>(Similarity2::new(
            Vector2::new(
                padding + ((drawable_width - glyph_width_px).max(0.0) * 0.5)
                    - glyph_bounds.x_min as f64 / shrinkage,
                padding + ((drawable_height - glyph_height_px).max(0.0) * 0.5)
                    - glyph_bounds.y_min as f64 / shrinkage,
            ),
            0.0,
            1.0 / shrinkage,
        ));
        shape.transform(&transformation);
        let colored_shape = Shape::edge_coloring_simple(shape, 0.03, 69441337420);
        let prepared_colored_shape = colored_shape.prepare();
        let mut mtsdf = RgbaImage::new(cell_width, cell_height);
        generate_mtsdf(&prepared_colored_shape, range_px, &mut mtsdf);
        correct_sign_mtsdf(&mut mtsdf, &prepared_colored_shape, FillRule::Nonzero);
        for (local_x, local_y, pixel) in mtsdf.enumerate_pixels() {
            let px = (cell_x * cell_width as usize) + local_x as usize;
            let py = (cell_y * cell_height as usize)
                + (cell_height.saturating_sub(1).saturating_sub(local_y)) as usize;
            if px >= width as usize || py >= height as usize {
                continue;
            }
            let offset = ((py * width as usize) + px) * 4;
            rgba8[offset..offset + 4].copy_from_slice(&pixel.0);
        }
    }

    Some(RasterizedShaderCanvasFontAtlas {
        texture_size_px: [width, height],
        encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_MTSDF_RGBA.to_string(),
        distance_range_px: atlas.distance_range_px.max(1),
        rgba8,
    })
}

fn rasterize_shader_surface_font_atlas_with_ab_glyph(
    atlas: &RealtimeShaderCanvasFontAtlas,
    font: &FontArc,
) -> RasterizedShaderCanvasFontAtlas {
    let width = atlas.texture_size_px[0].max(1);
    let height = atlas.texture_size_px[1].max(1);
    let mut rgba8 = vec![0u8; (width * height * 4) as usize];
    let cell_width = atlas.cell_size_px[0].max(1) as usize;
    let cell_height = atlas.cell_size_px[1].max(1) as usize;
    let glyph_scale_px = (cell_width.min(cell_height) as f32 * 0.82).max(8.0);
    let scaled_font = font.as_scaled(glyph_scale_px);
    let fallback_glyph = font.glyph_id('?');

    for (glyph_index, glyph) in atlas.glyphs.chars().enumerate() {
        let columns = atlas.columns.max(1) as usize;
        let cell_x = glyph_index % columns;
        let cell_y = glyph_index / columns;
        if cell_y >= atlas.rows.max(1) as usize {
            continue;
        }

        let glyph_id = font.glyph_id(glyph);
        let mut template_outline =
            outline_shader_canvas_glyph(&scaled_font, glyph_id, glyph_scale_px, point(0.0, 0.0));
        let resolved_glyph_id = if template_outline.is_some() {
            glyph_id
        } else {
            template_outline = outline_shader_canvas_glyph(
                &scaled_font,
                fallback_glyph,
                glyph_scale_px,
                point(0.0, 0.0),
            );
            fallback_glyph
        };
        let Some(template_outline) = template_outline else {
            continue;
        };
        let template_bounds = template_outline.px_bounds();
        let glyph_width = (template_bounds.max.x - template_bounds.min.x).max(1.0);
        let glyph_height = (template_bounds.max.y - template_bounds.min.y).max(1.0);
        let position = point(
            cell_x as f32 * cell_width as f32 + ((cell_width as f32 - glyph_width).max(0.0) * 0.5)
                - template_bounds.min.x,
            cell_y as f32 * cell_height as f32
                + ((cell_height as f32 - glyph_height).max(0.0) * 0.5)
                - template_bounds.min.y,
        );
        let Some(outlined_glyph) =
            outline_shader_canvas_glyph(&scaled_font, resolved_glyph_id, glyph_scale_px, position)
        else {
            continue;
        };
        let glyph_bounds = outlined_glyph.px_bounds();
        outlined_glyph.draw(|glyph_x, glyph_y, coverage| {
            let px = glyph_bounds.min.x.floor() as i32 + glyph_x as i32;
            let py = glyph_bounds.min.y.floor() as i32 + glyph_y as i32;
            if px < 0 || py < 0 {
                return;
            }
            let px = px as usize;
            let py = py as usize;
            if px >= width as usize || py >= height as usize {
                return;
            }
            let coverage_u8 = (coverage.clamp(0.0, 1.0) * 255.0).round() as u8;
            if coverage_u8 == 0 {
                return;
            }
            let offset = ((py * width as usize) + px) * 4;
            rgba8[offset] = 255;
            rgba8[offset + 1] = 255;
            rgba8[offset + 2] = 255;
            rgba8[offset + 3] = rgba8[offset + 3].max(coverage_u8);
        });
    }

    RasterizedShaderCanvasFontAtlas {
        texture_size_px: [width, height],
        encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_ALPHA_COVERAGE.to_string(),
        distance_range_px: 0,
        rgba8,
    }
}

fn outline_shader_canvas_glyph(
    scaled_font: &ab_glyph::PxScaleFont<&FontArc>,
    glyph_id: ab_glyph::GlyphId,
    glyph_scale_px: f32,
    position: ab_glyph::Point,
) -> Option<ab_glyph::OutlinedGlyph> {
    ScaleFont::outline_glyph(
        scaled_font,
        glyph_id.with_scale_and_position(glyph_scale_px, position),
    )
}

fn rasterize_shader_surface_font_atlas_bitmap(
    atlas: &RealtimeShaderCanvasFontAtlas,
) -> RasterizedShaderCanvasFontAtlas {
    let width = atlas.texture_size_px[0].max(1);
    let height = atlas.texture_size_px[1].max(1);
    let mut rgba8 = vec![0u8; (width * height * 4) as usize];
    let cell_width = atlas.cell_size_px[0].max(1) as usize;
    let cell_height = atlas.cell_size_px[1].max(1) as usize;
    let glyph_offset_x = cell_width.saturating_sub(5) / 2;
    let glyph_offset_y = cell_height.saturating_sub(7) / 2;

    for (glyph_index, glyph) in atlas.glyphs.chars().enumerate() {
        let columns = atlas.columns.max(1) as usize;
        let cell_x = glyph_index % columns;
        let cell_y = glyph_index / columns;
        if cell_y >= atlas.rows.max(1) as usize {
            continue;
        }
        let bitmap = bitmap_font_5x7(glyph);
        for (row_index, row_bits) in bitmap.iter().enumerate() {
            for column_index in 0..5 {
                let mask = 1 << (4 - column_index);
                if (row_bits & mask) == 0 {
                    continue;
                }
                let px = (cell_x * cell_width) + glyph_offset_x + column_index;
                let py = (cell_y * cell_height) + glyph_offset_y + row_index;
                if px >= width as usize || py >= height as usize {
                    continue;
                }
                let offset = ((py * width as usize) + px) * 4;
                rgba8[offset] = 255;
                rgba8[offset + 1] = 255;
                rgba8[offset + 2] = 255;
                rgba8[offset + 3] = 255;
            }
        }
    }

    RasterizedShaderCanvasFontAtlas {
        texture_size_px: [width, height],
        encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
        distance_range_px: 0,
        rgba8,
    }
}

fn resolve_shader_canvas_text_color(
    theme: &NativeWidgetTheme,
    color_token: &str,
    role: &str,
) -> Color32 {
    let token = color_token.to_ascii_lowercase();
    if token.contains("muted") {
        theme.muted_color
    } else if token.contains("accent") {
        theme.accent
    } else if token.contains("fill") || token.contains("surface") {
        theme.canvas_fill
    } else if token.contains("body") {
        theme.body_color
    } else if token.contains("title") || token.contains("default") {
        theme.title_color
    } else if role.eq_ignore_ascii_case("title") {
        theme.title_color
    } else {
        theme.body_color
    }
}

fn shader_canvas_text_role_tag(role: &str) -> u32 {
    match role.trim().to_ascii_lowercase().as_str() {
        "title" => 1,
        "body" => 2,
        "label" => 3,
        "caption" => 4,
        _ => 0,
    }
}

fn sanitize_shader_canvas_text(text: &str) -> String {
    text.chars().map(sanitize_shader_canvas_glyph).collect()
}

fn sanitize_shader_canvas_glyph(ch: char) -> char {
    if ch.is_ascii() {
        ch
    } else {
        '?'
    }
}

fn bitmap_font_5x7(ch: char) -> [u8; 7] {
    match if ch.is_ascii_lowercase() {
        ch.to_ascii_uppercase()
    } else {
        ch
    } {
        'A' => [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'B' => [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
        'C' => [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
        'D' => [0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E],
        'E' => [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
        'F' => [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
        'G' => [0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F],
        'H' => [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
        'I' => [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
        'J' => [0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E],
        'K' => [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
        'L' => [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
        'M' => [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
        'N' => [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
        'O' => [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'P' => [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
        'Q' => [0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D],
        'R' => [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
        'S' => [0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E],
        'T' => [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
        'U' => [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
        'V' => [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04],
        'W' => [0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A],
        'X' => [0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11],
        'Y' => [0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04],
        'Z' => [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
        '0' => [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
        '1' => [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
        '2' => [0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F],
        '3' => [0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E],
        '4' => [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02],
        '5' => [0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E],
        '6' => [0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E],
        '7' => [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
        '8' => [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E],
        '9' => [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E],
        '!' => [0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04],
        '?' => [0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04],
        '.' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04],
        ',' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08],
        ':' => [0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00],
        ';' => [0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x08],
        '-' => [0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00],
        '_' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F],
        '+' => [0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00],
        '=' => [0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, 0x00],
        '/' => [0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10],
        '\\' => [0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01],
        '(' => [0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02],
        ')' => [0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08],
        '[' => [0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E],
        ']' => [0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E],
        '<' => [0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02],
        '>' => [0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08],
        '"' => [0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00],
        '\'' => [0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00],
        ' ' => [0x00; 7],
        _ => [0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04],
    }
}

fn shader_binding_kind(layout: &ShaderResourceLayout) -> Option<UiShaderBindingKind> {
    let kind = layout.kind.to_ascii_lowercase();
    let ty = layout.ty.to_ascii_lowercase();
    if kind.contains("uniform") {
        Some(UiShaderBindingKind::UniformBuffer)
    } else if kind.contains("storage") {
        Some(UiShaderBindingKind::StorageBuffer)
    } else if kind.contains("sampler") && !kind.contains("texture") {
        Some(UiShaderBindingKind::Sampler)
    } else if kind.contains("texture")
        || ty.contains("sampler2d")
        || ty.contains("texture2d")
        || ty.contains("texture")
    {
        Some(UiShaderBindingKind::Texture2d)
    } else {
        None
    }
}

fn build_presented_viewport_resources(
    device: &wgpu::Device,
    target_format: wgpu::TextureFormat,
    shader_bundle: Option<&ShaderArtifactBundle>,
) -> Result<PresentedViewportGpuResources, String> {
    let shader_bundle = shader_bundle
        .cloned()
        .unwrap_or_else(default_viewport_shader_bundle);
    let shader_source = wgsl_module_source(&shader_bundle, VIEWPORT_SHADER_MODULE_NAME)
        .map_err(|err| err.to_string())?;
    let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some("kain-ui-native-presented-viewport-shader"),
        source: wgpu::ShaderSource::Wgsl(shader_source),
    });

    let bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
        label: Some("kain-ui-native-presented-viewport-bind-layout"),
        entries: &[wgpu::BindGroupLayoutEntry {
            binding: 0,
            visibility: wgpu::ShaderStages::VERTEX_FRAGMENT,
            ty: wgpu::BindingType::Buffer {
                ty: wgpu::BufferBindingType::Uniform,
                has_dynamic_offset: false,
                min_binding_size: None,
            },
            count: None,
        }],
    });
    let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
        label: Some("kain-ui-native-presented-viewport-pipeline-layout"),
        bind_group_layouts: &[&bind_group_layout],
        push_constant_ranges: &[],
    });
    let uniform_buffer = device.create_buffer(&wgpu::BufferDescriptor {
        label: Some("kain-ui-native-presented-viewport-uniform-buffer"),
        size: std::mem::size_of::<SceneUniforms>() as u64,
        usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        mapped_at_creation: false,
    });
    let uniform_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("kain-ui-native-presented-viewport-bind-group"),
        layout: &bind_group_layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: uniform_buffer.as_entire_binding(),
        }],
    });

    let color_target = Some(wgpu::ColorTargetState {
        format: target_format,
        blend: Some(wgpu::BlendState::ALPHA_BLENDING),
        write_mask: wgpu::ColorWrites::ALL,
    });

    let background_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("kain-ui-native-presented-background-pipeline"),
        layout: Some(&pipeline_layout),
        vertex: wgpu::VertexState {
            module: &shader,
            entry_point: Some("background_vs_main"),
            compilation_options: Default::default(),
            buffers: &[],
        },
        primitive: wgpu::PrimitiveState::default(),
        depth_stencil: None,
        multisample: wgpu::MultisampleState::default(),
        fragment: Some(wgpu::FragmentState {
            module: &shader,
            entry_point: Some("background_fs_main"),
            compilation_options: Default::default(),
            targets: &[color_target.clone()],
        }),
        multiview: None,
        cache: None,
    });
    let scene_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("kain-ui-native-presented-scene-pipeline"),
        layout: Some(&pipeline_layout),
        vertex: wgpu::VertexState {
            module: &shader,
            entry_point: Some("scene_vs_main"),
            compilation_options: Default::default(),
            buffers: &[GpuVertex::layout()],
        },
        primitive: wgpu::PrimitiveState::default(),
        depth_stencil: None,
        multisample: wgpu::MultisampleState::default(),
        fragment: Some(wgpu::FragmentState {
            module: &shader,
            entry_point: Some("scene_fs_main"),
            compilation_options: Default::default(),
            targets: &[color_target.clone()],
        }),
        multiview: None,
        cache: None,
    });
    let particle_pipeline = |label: &'static str, entry: &'static str| {
        device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some(label),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("particle_vs_main"),
                compilation_options: Default::default(),
                buffers: &[ParticleVertex::layout()],
            },
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some(entry),
                compilation_options: Default::default(),
                targets: &[color_target.clone()],
            }),
            multiview: None,
            cache: None,
        })
    };
    let particle_depth_pipeline = particle_pipeline(
        "kain-ui-native-presented-particle-depth-pipeline",
        "particle_fs_main",
    );
    let particle_overlay_pipeline = particle_pipeline(
        "kain-ui-native-presented-particle-overlay-pipeline",
        "particle_fs_main",
    );
    let gizmo_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("kain-ui-native-presented-gizmo-pipeline"),
        layout: Some(&pipeline_layout),
        vertex: wgpu::VertexState {
            module: &shader,
            entry_point: Some("gizmo_vs_main"),
            compilation_options: Default::default(),
            buffers: &[GizmoVertex::layout()],
        },
        primitive: wgpu::PrimitiveState {
            topology: wgpu::PrimitiveTopology::LineList,
            ..Default::default()
        },
        depth_stencil: None,
        multisample: wgpu::MultisampleState::default(),
        fragment: Some(wgpu::FragmentState {
            module: &shader,
            entry_point: Some("gizmo_fs_main"),
            compilation_options: Default::default(),
            targets: &[color_target],
        }),
        multiview: None,
        cache: None,
    });

    Ok(PresentedViewportGpuResources {
        uniform_buffer,
        uniform_bind_group,
        background_pipeline,
        scene_pipeline,
        particle_depth_pipeline,
        particle_overlay_pipeline,
        gizmo_pipeline,
    })
}

fn build_presented_draw_data(
    device: &wgpu::Device,
    prepared_frame: &PreparedWgpuFrame,
) -> PresentedViewportDrawData {
    PresentedViewportDrawData {
        scene_len: prepared_frame.scene_vertices.len() as u32,
        scene_buffer: buffer_for_vertices(
            device,
            "kain-ui-native-presented-scene-buffer",
            &prepared_frame.scene_vertices,
        ),
        depth_particle_len: prepared_frame.depth_particles.len() as u32,
        depth_particle_buffer: buffer_for_vertices(
            device,
            "kain-ui-native-presented-depth-particles",
            &prepared_frame.depth_particles,
        ),
        overlay_particle_len: prepared_frame.overlay_particles.len() as u32,
        overlay_particle_buffer: buffer_for_vertices(
            device,
            "kain-ui-native-presented-overlay-particles",
            &prepared_frame.overlay_particles,
        ),
        gizmo_len: prepared_frame.gizmo_vertices.len() as u32,
        gizmo_buffer: buffer_for_vertices(
            device,
            "kain-ui-native-presented-gizmo-buffer",
            &prepared_frame.gizmo_vertices,
        ),
    }
}

fn buffer_for_vertices<T: bytemuck::Pod>(
    device: &wgpu::Device,
    label: &'static str,
    vertices: &[T],
) -> Option<wgpu::Buffer> {
    if vertices.is_empty() {
        None
    } else {
        Some(
            device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label: Some(label),
                contents: bytemuck::cast_slice(vertices),
                usage: wgpu::BufferUsages::VERTEX,
            }),
        )
    }
}

impl eframe::App for KainUiNativeApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        trace_runtime("app_update: begin");
        let frame_now = Instant::now();
        self.frame_dt_seconds = (frame_now - self.last_frame_instant)
            .as_secs_f32()
            .clamp(1.0 / 240.0, 0.050);
        self.last_frame_instant = frame_now;
        self.poll_runtime_reloads();
        ctx.request_repaint_after(Duration::from_millis(
            self.runtime_settings.repaint_interval_ms,
        ));
        self.viewport_input = snapshot_viewport_input(ctx);
        self.viewport_surfaces
            .retain(|id, _| self.runtime.tree.nodes.contains_key(id));
        self.shader_surfaces
            .retain(|id, _| self.runtime.tree.nodes.contains_key(id));
        let animation_delta_ms = (self.frame_dt_seconds * 1000.0).round().clamp(1.0, 64.0) as u32;
        let step_output = self.runtime.step(&UiRuntimeStepInput {
            transaction_label: Some("native_frame_tick".to_string()),
            delta_ms: animation_delta_ms,
            events: Vec::new(),
            signal_updates: Vec::new(),
        });
        self.sync_render_output_from_runtime(step_output.tree_patches);
        let app_theme = resolve_app_theme(&self.output);
        apply_runtime_visuals(ctx, &app_theme);
        let theme_registry = self.output.systems.theme_registry.clone();
        let product_shell =
            is_product_desktop_theme(&app_theme, self.app_runtime_snapshot.as_ref());
        let show_topbar = show_runtime_topbar(&app_theme, !product_shell);
        let show_inspector = show_runtime_inspector(
            &app_theme,
            self.runtime_settings.show_runtime_inspector || !product_shell,
        );

        if show_topbar {
            trace_runtime("app_update: topbar");
            egui::TopBottomPanel::top("kain_ui_native_topbar")
                .resizable(false)
                .show(ctx, |ui| {
                    let chrome_theme = NativeWidgetTheme::chrome(&app_theme);
                    Frame::new()
                        .fill(chrome_theme.fill)
                        .stroke(Stroke::new(1.0, chrome_theme.stroke))
                        .corner_radius(app_theme.metrics.radius_medium)
                        .inner_margin(app_theme.metrics.tight_padding)
                        .show(ui, |ui| {
                            ui.horizontal(|ui| {
                                ui.vertical(|ui| {
                                    ui.label(
                                        RichText::new(&self.config.window_title)
                                            .size(app_theme.typography.heading)
                                            .strong()
                                            .color(app_theme.palette.text),
                                    );
                                    if let Some(snapshot) = &self.app_runtime_snapshot {
                                        let subtitle = snapshot
                                            .sessions
                                            .recent_session_title
                                            .clone()
                                            .unwrap_or_else(|| {
                                                snapshot.sessions.active_provider.clone()
                                            });
                                        ui.label(
                                            RichText::new(subtitle)
                                                .size(app_theme.typography.small)
                                                .color(app_theme.palette.text_muted),
                                        );
                                    }
                                });
                                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                                    if let Some(snapshot) = &self.app_runtime_snapshot {
                                        Frame::new()
                                            .fill(alpha_tint(app_theme.palette.accent, 0.12))
                                            .stroke(Stroke::new(1.0, app_theme.palette.accent_soft))
                                            .corner_radius(app_theme.metrics.radius_medium)
                                            .inner_margin(app_theme.metrics.tight_padding)
                                            .show(ui, |ui| {
                                                ui.label(
                                                    RichText::new(
                                                        snapshot.sessions.active_provider.clone(),
                                                    )
                                                    .size(app_theme.typography.small)
                                                    .color(app_theme.palette.accent_soft),
                                                );
                                            });
                                        ui.add_space(8.0);
                                        ui.label(
                                            RichText::new(format!(
                                                "{} session{}",
                                                snapshot.sessions.total_sessions,
                                                if snapshot.sessions.total_sessions == 1 {
                                                    ""
                                                } else {
                                                    "s"
                                                }
                                            ))
                                            .size(app_theme.typography.small)
                                            .color(app_theme.palette.text_muted),
                                        );
                                    }
                                });
                            });
                            if let Some(snapshot) = &self.app_runtime_snapshot {
                                self.render_dcc_session_badges(ui, &app_theme, snapshot);
                            }
                        });
                });
        }

        if show_inspector {
            trace_runtime("app_update: inspector");
            egui::SidePanel::right("kain_ui_native_inspector")
                .default_width(match app_theme.density {
                    NativeDensity::Compact => 320.0,
                    NativeDensity::Cozy => 360.0,
                    NativeDensity::Spacious => 420.0,
                })
                .show(ctx, |ui| {
                    themed_frame(&NativeWidgetTheme::chrome(&app_theme)).show(ui, |ui| {
                        ui.heading(RichText::new("Runtime Devtools").color(app_theme.palette.text));
                        ui.label(
                            RichText::new(
                                "Opt-in diagnostics for the retained semantic tree, patch stream, and compiled viewport surfaces.",
                            )
                            .size(app_theme.typography.body)
                            .color(app_theme.palette.text_muted),
                        );
                        ui.separator();
                        ui.label(
                            RichText::new(format!("boot source: {}", self.boot_mode.label()))
                                .monospace()
                                .color(app_theme.palette.accent_soft),
                        );
                        ui.label(
                            RichText::new(format!("active theme: {}", app_theme.name))
                                .monospace()
                                .color(app_theme.palette.highlight),
                        );
                        if let Some(path) = &self.app_manifest_path {
                            ui.label(
                                RichText::new(format!("app manifest: {path}"))
                                    .monospace()
                                    .color(app_theme.palette.accent_soft),
                            );
                        }
                        if let Some(path) = &self.command_bridge_path {
                            ui.label(
                                RichText::new(format!("command bridge: {path}"))
                                    .monospace()
                                    .color(app_theme.palette.highlight),
                            );
                        }
                        if let Some(snapshot) = &self.app_runtime_snapshot {
                            ui.separator();
                            ui.heading(RichText::new("Desktop Snapshot").color(app_theme.palette.text));
                            ui.label(
                                RichText::new(format!(
                                    "{} {}  |  app_id={}  |  layout={}",
                                    snapshot.name, snapshot.version, snapshot.app_id, snapshot.layout_id
                                ))
                                .monospace()
                                .color(app_theme.palette.accent_soft),
                            );
                            ui.label(
                                RichText::new(format!(
                                    "window={}  |  root={}  |  updated={}",
                                    snapshot.window_title, snapshot.root_component, snapshot.updated_at
                                ))
                                .monospace()
                                .color(app_theme.palette.highlight),
                            );
                            ui.label(
                                RichText::new(format!(
                                    "sessions={}  |  active_provider={}  |  recent_session={}",
                                    snapshot.sessions.total_sessions,
                                    snapshot.sessions.active_provider,
                                    snapshot
                                        .sessions
                                        .recent_session_title
                                        .as_deref()
                                        .unwrap_or("<none>")
                                ))
                                .monospace()
                                .color(app_theme.palette.success),
                            );
                            if let Some(session_id) = &snapshot.sessions.recent_session_id {
                                ui.label(
                                    RichText::new(format!("recent_session_id={session_id}"))
                                        .monospace()
                                        .color(app_theme.palette.accent_soft),
                                );
                            }
                            self.render_runtime_command_bar(ui, &app_theme, snapshot);
                            ui.collapsing("Required Capabilities", |ui| {
                                for capability in &snapshot.required_runtime_capabilities {
                                    ui.label(RichText::new(capability).monospace());
                                }
                            });
                            ui.collapsing("Panels", |ui| {
                                for panel in &snapshot.panels {
                                    ui.label(
                                        RichText::new(format!(
                                            "{} [{}] -> {} ({})",
                                            panel.title, panel.id, panel.dock, panel.kind
                                        ))
                                        .monospace(),
                                    );
                                }
                            });
                            ui.collapsing("Commands", |ui| {
                                for command in &snapshot.commands {
                                    ui.horizontal_wrapped(|ui| {
                                        let button = egui::Button::new(
                                            RichText::new(&command.label)
                                                .size(app_theme.typography.small)
                                                .color(app_theme.palette.text),
                                        )
                                        .fill(alpha_tint(app_theme.palette.surface_overlay, 0.82))
                                        .stroke(Stroke::new(1.0, app_theme.palette.outline_soft))
                                        .corner_radius(app_theme.metrics.radius_medium);
                                        if ui
                                            .add_enabled(
                                                self.command_bridge_path.is_some(),
                                                button,
                                            )
                                            .clicked()
                                        {
                                            self.emit_runtime_command(command);
                                        }
                                        ui.label(
                                            RichText::new(format!(
                                                "[{}] -> {} @ {}",
                                                command.id, command.intent, command.surface
                                            ))
                                            .monospace()
                                            .color(app_theme.palette.text_muted),
                                        );
                                    });
                                }
                            });
                            if let Some(dcc_state) = &snapshot.dcc_suite_state {
                                ui.collapsing("DCC Suite", |ui| {
                                    ui.label(
                                        RichText::new(format!(
                                            "mode={} tool={} frame={} dirty={} bridge={} processed={}",
                                            dcc_state.session.workspace.active_mode,
                                            dcc_state.session.tooling.active_tool,
                                            dcc_state.session.animation.frame,
                                            dcc_state.session.dirty.active_count(),
                                            dcc_state.bridge.status,
                                            dcc_state.bridge.processed_command_count
                                        ))
                                        .monospace(),
                                    );
                                    ui.label(
                                        RichText::new(format!(
                                            "gizmo={} | {} | snap={}",
                                            dcc_state.session.gizmo.mode,
                                            dcc_state.session.gizmo.space,
                                            dcc_state.session.gizmo.snap_enabled
                                        ))
                                        .monospace()
                                        .color(app_theme.palette.accent_soft),
                                    );
                                    if let Some(command) = &dcc_state.latest_command {
                                        ui.label(
                                            RichText::new(format!(
                                                "latest_command={} requested_at={}",
                                                if command.label.is_empty() {
                                                    command.command_id.as_str()
                                                } else {
                                                    command.label.as_str()
                                                },
                                                command.requested_at
                                            ))
                                            .monospace()
                                            .color(app_theme.palette.highlight),
                                        );
                                    }
                                });
                            }
                            ui.collapsing("Providers", |ui| {
                                for provider in &snapshot.providers {
                                    ui.label(
                                        RichText::new(format!(
                                            "{} [{}] transport={} profile={} tools={} streaming={} active={} configured={} keys={}",
                                            provider.label,
                                            provider.id,
                                            provider.transport,
                                            provider.profile_kind,
                                            provider.supports_tools,
                                            provider.supports_streaming,
                                            provider.active,
                                            provider.profile_configured,
                                            if provider.profile_keys.is_empty() {
                                                "<none>".to_string()
                                            } else {
                                                provider.profile_keys.join(",")
                                            }
                                        ))
                                        .monospace(),
                                    );
                                }
                            });
                            ui.collapsing("Tools", |ui| {
                                for tool in &snapshot.tools {
                                    ui.label(
                                        RichText::new(format!(
                                            "{} [{}] {} approval={} decision={}",
                                            tool.label,
                                            tool.id,
                                            tool.capability,
                                            tool.approval,
                                            tool.decision.as_deref().unwrap_or("<unset>")
                                        ))
                                        .monospace(),
                                    );
                                    for scoped in &tool.scope_decisions {
                                        ui.label(
                                            RichText::new(format!(
                                                "  {} => {} ({})",
                                                scoped.scope, scoped.decision, scoped.updated_at
                                            ))
                                            .monospace()
                                            .color(app_theme.palette.accent_soft),
                                        );
                                    }
                                }
                            });
                            ui.collapsing("Recent Sessions", |ui| {
                                for session in &snapshot.recent_sessions {
                                    ui.label(
                                        RichText::new(format!(
                                            "{} [{}] provider={} status={} messages={} updated={}",
                                            session.title,
                                            session.id,
                                            session.provider_id,
                                            session.status,
                                            session.message_count,
                                            session.updated_at
                                        ))
                                        .monospace(),
                                    );
                                    if let Some(workspace_root) = &session.workspace_root {
                                        ui.label(
                                            RichText::new(format!("  workspace={workspace_root}"))
                                                .monospace()
                                                .color(app_theme.palette.highlight),
                                        );
                                    }
                                    if let Some(preview) = &session.last_message_preview {
                                        ui.label(
                                            RichText::new(format!(
                                                "  {}: {}",
                                                session
                                                    .last_message_role
                                                    .as_deref()
                                                    .unwrap_or("message"),
                                                preview
                                            ))
                                            .monospace()
                                            .color(app_theme.palette.accent_soft),
                                        );
                                    }
                                }
                            });
                            ui.collapsing("Workspaces", |ui| {
                                for workspace in &snapshot.workspaces {
                                    ui.label(
                                        RichText::new(format!(
                                            "{} sessions={} recent={}",
                                            workspace.root,
                                            workspace.session_count,
                                            workspace
                                                .recent_session_title
                                                .as_deref()
                                                .unwrap_or("<none>")
                                        ))
                                        .monospace(),
                                    );
                                }
                            });
                        }

                        ui.collapsing("Semantic Tree", |ui| {
                            egui::ScrollArea::vertical().show(ui, |ui| {
                                ui.code(&self.debug_tree);
                            });
                        });

                        ui.separator();
                        ui.heading(RichText::new("Patch Stream").color(app_theme.palette.text));
                        egui::ScrollArea::vertical()
                            .auto_shrink([false, false])
                            .show(ui, |ui| {
                                for patch in &self.output.patches {
                                    ui.label(RichText::new(format!("{patch:?}")).monospace());
                                }
                            });

                        ui.separator();
                        ui.heading(RichText::new("Runtime Systems").color(app_theme.palette.text));
                        ui.label(format!(
                            "computed={} surfaces={} animations={} theme_scopes={} dock_roots={}",
                            self.output.systems.computed.len(),
                            self.output.systems.surfaces.len(),
                            self.output.systems.animation_tracks.len(),
                            self.output.systems.theme_registry.scopes.len(),
                            self.output.systems.workspace_layout.roots.len(),
                        ));
                        ui.label(format!(
                            "focus_scopes={} selection_scopes={} scheduler_pending={} reload_aliases={}",
                            self.output.systems.focus_graph.scopes.len(),
                            self.output.systems.selection_model.scopes.len(),
                            self.output.systems.scheduler.pending.len(),
                            self.output.systems.hot_reload.identity_aliases.len(),
                        ));

                        ui.collapsing("Theme Scopes", |ui| {
                            for scope in &self.output.systems.theme_registry.scopes {
                                ui.label(
                                    RichText::new(format!("{} -> {}", scope.name, scope.selector))
                                        .monospace(),
                                );
                            }
                        });

                        ui.collapsing("Scheduler", |ui| {
                            for entry in &self.output.systems.scheduler.pending {
                                ui.label(
                                    RichText::new(format!("{:?}: {}", entry.phase, entry.label))
                                        .monospace(),
                                );
                            }
                        });

                        ui.collapsing("Hot Reload", |ui| {
                            for alias in &self.output.systems.hot_reload.identity_aliases {
                                ui.label(
                                    RichText::new(format!("{} -> {}", alias.from, alias.to))
                                        .monospace(),
                                );
                            }
                        });
                    });
                });
        }

        trace_runtime("app_update: central_panel");
        egui::CentralPanel::default()
            .frame(
                Frame::new()
                    .fill(app_theme.palette.bg_bottom)
                    .inner_margin(app_theme.metrics.frame_padding),
            )
            .show(ctx, |ui| {
                if let Some(root_id) = self.output.tree.root {
                    let tree = self.output.tree.clone();
                    render_node(self, ui, ctx, &tree, &theme_registry, &app_theme, root_id);
                }
            });
        self.sync_runtime_from_render_output();
        trace_runtime("app_update: end");
    }
}

fn render_node(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    id: kain_ui::UiNodeId,
) {
    let Some(node) = tree.node(id) else {
        ui.colored_label(Color32::LIGHT_RED, format!("missing node {}", id.0));
        return;
    };

    let node_identity = node_layout_identity(node);
    ui.push_id(node_identity, |ui| {
        ui.scope(|ui| {
            let presentation = resolve_node_presentation(&app.output, node);
            let product_shell =
                is_product_desktop_theme(app_theme, app.app_runtime_snapshot.as_ref());
            if presentation.translate_y > f32::EPSILON {
                ui.add_space(presentation.translate_y);
            }
            apply_node_layout_constraints(ui, node);

            match &node.kind {
                UiWidgetKind::Text => {
                    render_text_node(ui, node, app_theme, presentation);
                }
                UiWidgetKind::Panel => {
                    let title = prop_text(node, "title").unwrap_or("Panel");
                    let theme = apply_node_presentation_to_theme(
                        resolve_widget_theme(node, theme_registry, app_theme),
                        presentation,
                    );
                    let show_title = widget_title_visible(node, theme_registry, app_theme);
                    themed_frame(&theme).show(ui, |ui| {
                        let header_title = if show_title { title } else { "Panel" };
                        render_widget_header(ui, &theme, header_title, node, "panel");
                        ui.add_space(theme.gap * 0.68);
                        render_widget_body_frame(ui, &theme, |ui| {
                            render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
                        });
                    });
                }
                UiWidgetKind::Inspector => {
                    let title = prop_text(node, "title").unwrap_or("Inspector");
                    let theme = apply_node_presentation_to_theme(
                        resolve_widget_theme(node, theme_registry, app_theme),
                        presentation,
                    );
                    let show_title = widget_title_visible(node, theme_registry, app_theme);
                    themed_frame(&theme).show(ui, |ui| {
                        if product_shell {
                            let header_title = if show_title { title } else { "Inspector" };
                            render_widget_header(ui, &theme, header_title, node, "inspector");
                            ui.add_space(theme.gap * 0.62);
                            render_widget_body_frame(ui, &theme, |ui| {
                                render_children(
                                    app,
                                    ui,
                                    ctx,
                                    tree,
                                    theme_registry,
                                    app_theme,
                                    node,
                                );
                            });
                        } else {
                            egui::CollapsingHeader::new(
                                RichText::new(title)
                                    .strong()
                                    .size(theme.title_size)
                                    .color(theme.title_color),
                            )
                            .default_open(true)
                            .show(ui, |ui| {
                                render_children(
                                    app,
                                    ui,
                                    ctx,
                                    tree,
                                    theme_registry,
                                    app_theme,
                                    node,
                                );
                            });
                        }
                    });
                }
                UiWidgetKind::Tree => {
                    let title = prop_text(node, "title").unwrap_or("Tree");
                    let theme = apply_node_presentation_to_theme(
                        resolve_widget_theme(node, theme_registry, app_theme),
                        presentation,
                    );
                    let show_title = widget_title_visible(node, theme_registry, app_theme);
                    themed_frame(&theme).show(ui, |ui| {
                        if product_shell {
                            let header_title = if show_title { title } else { "Tree" };
                            render_widget_header(ui, &theme, header_title, node, "tree");
                            ui.add_space(theme.gap * 0.52);
                            render_widget_body_frame(ui, &theme, |ui| {
                                render_children(
                                    app,
                                    ui,
                                    ctx,
                                    tree,
                                    theme_registry,
                                    app_theme,
                                    node,
                                );
                            });
                        } else {
                            egui::CollapsingHeader::new(
                                RichText::new(title)
                                    .size(theme.title_size)
                                    .color(theme.title_color),
                            )
                            .default_open(true)
                            .show(ui, |ui| {
                                render_children(
                                    app,
                                    ui,
                                    ctx,
                                    tree,
                                    theme_registry,
                                    app_theme,
                                    node,
                                );
                            });
                        }
                    });
                }
                UiWidgetKind::Graph => {
                    render_surface_frame(
                        ui,
                        node,
                        theme_registry,
                        app_theme,
                        presentation,
                        "Graph Canvas",
                        Vec2::new(ui.available_width().max(280.0), 220.0),
                        egui::Sense::hover(),
                        |ui, rect, _response, theme| {
                            let painter = ui.painter();
                            let inner_radius = (theme.radius - 4.0).max(6.0);
                            painter.rect_filled(rect.shrink(4.0), inner_radius, theme.canvas_fill);
                            for i in 0..3 {
                                let x = rect.left() + 40.0 + (i as f32 * 140.0);
                                let y = rect.top() + 50.0 + ((i % 2) as f32 * 70.0);
                                let node_rect = egui::Rect::from_min_size(
                                    egui::pos2(x, y),
                                    Vec2::new(112.0, 50.0),
                                );
                                painter.rect_filled(
                                    node_rect,
                                    10.0,
                                    alpha_tint(theme.accent, 0.22),
                                );
                                painter.rect_stroke(
                                    node_rect,
                                    10.0,
                                    Stroke::new(1.0, theme.tag_color),
                                    egui::StrokeKind::Inside,
                                );
                            }
                        },
                    );
                }
                UiWidgetKind::Timeline => {
                    render_surface_frame(
                        ui,
                        node,
                        theme_registry,
                        app_theme,
                        presentation,
                        "Timeline",
                        Vec2::new(ui.available_width().max(280.0), 120.0),
                        egui::Sense::hover(),
                        |ui, rect, _response, theme| {
                            let painter = ui.painter();
                            let inner_radius = (theme.radius - 4.0).max(6.0);
                            painter.rect_filled(rect.shrink(4.0), inner_radius, theme.canvas_fill);
                            for tick in 0..12 {
                                let x = rect.left() + 20.0 + (tick as f32 * 48.0);
                                painter.line_segment(
                                    [
                                        egui::pos2(x, rect.top() + 16.0),
                                        egui::pos2(x, rect.bottom() - 16.0),
                                    ],
                                    Stroke::new(1.0, theme.stroke),
                                );
                            }
                            let clip = egui::Rect::from_min_size(
                                egui::pos2(rect.left() + 64.0, rect.center().y - 14.0),
                                Vec2::new(220.0, 28.0),
                            );
                            painter.rect_filled(clip, 8.0, alpha_tint(theme.accent, 0.9));
                        },
                    );
                }
                UiWidgetKind::Viewport2D | UiWidgetKind::Viewport3D => {
                    let label = match node.kind {
                        UiWidgetKind::Viewport2D => "Viewport 2D",
                        _ => "Viewport 3D",
                    };
                    render_viewport_surface(
                        app,
                        ui,
                        ctx,
                        node,
                        theme_registry,
                        app_theme,
                        presentation,
                        label,
                    );
                }
                UiWidgetKind::ComponentRef(name) => {
                    let theme = apply_node_presentation_to_theme(
                        resolve_widget_theme(node, theme_registry, app_theme),
                        presentation,
                    );
                    if product_shell {
                        render_widget_header(ui, &theme, name, node, "component");
                        ui.add_space(theme.gap * 0.52);
                    } else {
                        ui.label(
                            RichText::new(format!("component {name}"))
                                .monospace()
                                .color(apply_node_presentation_to_color(
                                    app_theme.palette.highlight,
                                    presentation,
                                )),
                        );
                    }
                    render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
                }
                UiWidgetKind::Element(tag) => {
                    if let Some(surface) = surface_descriptor_for_node(app, node).cloned() {
                        if surface.kind == UiSurfaceKind::Canvas
                            || surface.shader.is_some()
                            || surface.gpu_backing_required
                        {
                            render_gpu_canvas_surface(
                                app,
                                ui,
                                ctx,
                                tree,
                                theme_registry,
                                app_theme,
                                node,
                                tag,
                                &surface,
                                presentation,
                            );
                        } else {
                            render_generic_element_node(
                                app,
                                ui,
                                ctx,
                                tree,
                                theme_registry,
                                app_theme,
                                node,
                                tag,
                                presentation,
                                product_shell,
                            );
                        }
                    } else {
                        render_generic_element_node(
                            app,
                            ui,
                            ctx,
                            tree,
                            theme_registry,
                            app_theme,
                            node,
                            tag,
                            presentation,
                            product_shell,
                        );
                    }
                }
                UiWidgetKind::Table | UiWidgetKind::Overlay | UiWidgetKind::Slot => {
                    let title = prop_text(node, "title").unwrap_or(match &node.kind {
                        UiWidgetKind::Table => "Table",
                        UiWidgetKind::Overlay => "Overlay",
                        UiWidgetKind::Slot => "Slot",
                        _ => "Surface",
                    });
                    let theme = apply_node_presentation_to_theme(
                        resolve_widget_theme(node, theme_registry, app_theme),
                        presentation,
                    );
                    themed_frame(&theme).show(ui, |ui| {
                        render_widget_header(ui, &theme, title, node, widget_kind_key(&node.kind));
                        ui.add_space(theme.gap * 0.55);
                        render_widget_body_frame(ui, &theme, |ui| {
                            render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
                        });
                    });
                }
            }
        });
    });
}

fn render_generic_element_node(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    node: &UiNode,
    tag: &str,
    presentation: NativeNodePresentation,
    product_shell: bool,
) {
    let theme = apply_node_presentation_to_theme(
        resolve_widget_theme(node, theme_registry, app_theme),
        presentation,
    );
    themed_frame(&theme).show(ui, |ui| {
        if product_shell {
            render_widget_header(ui, &theme, tag, node, "element");
            ui.add_space(theme.gap * 0.48);
            render_widget_body_frame(ui, &theme, |ui| {
                render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
            });
        } else {
            ui.small(
                RichText::new(format!("<{tag}>"))
                    .monospace()
                    .color(theme.tag_color),
            );
            render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
        }
    });
}

fn render_gpu_canvas_surface(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    node: &UiNode,
    tag: &str,
    surface: &UiSurface,
    presentation: NativeNodePresentation,
) {
    let fallback_title = match surface.title.as_deref() {
        Some(title) if !title.trim().is_empty() => title,
        _ => "GPU Surface",
    };
    let runtime_renderer = app.runtime_settings.effective_renderer_label().to_string();
    let resolved_shader = resolve_ui_shader_surface(app, surface);
    let shader_ref = surface
        .shader
        .as_ref()
        .map(|binding| binding.shader_ref.clone())
        .unwrap_or_else(|| "<none>".to_string());
    let shader_stage = resolved_shader
        .as_ref()
        .ok()
        .map(|descriptor| descriptor.stage.clone())
        .or_else(|| {
            surface
                .shader
                .as_ref()
                .and_then(|binding| binding.stage.clone())
        })
        .unwrap_or_else(|| "runtime-selected".to_string());
    let shader_format = resolved_shader
        .as_ref()
        .ok()
        .map(|descriptor| descriptor.derived_format.clone())
        .or_else(|| {
            surface
                .shader
                .as_ref()
                .and_then(|binding| binding.derived_format.clone())
        })
        .unwrap_or_else(|| "runtime-selected".to_string());
    let shader_payload = resolved_shader
        .as_ref()
        .ok()
        .map(|descriptor| {
            format!(
                "{} text:{} atlas:{} resources:{}",
                descriptor.canonical_native_payload,
                descriptor.text_runs.len(),
                descriptor.font_atlases.len(),
                descriptor.resource_bindings.len()
            )
        })
        .unwrap_or_else(|| "runtime-selected".to_string());
    let shader_binding_key = resolved_shader
        .as_ref()
        .ok()
        .and_then(|descriptor| descriptor.shader_bundle_ref_key.clone())
        .unwrap_or_else(|| "surface-local".to_string());
    let descriptor_tag = format!(
        "{} / {}",
        surface_renderer_preference_label(surface.renderer_preference),
        surface_composition_mode_label(surface.composition_mode)
    );
    let resolved_warning = resolved_shader.as_ref().err().cloned().or_else(|| {
        resolved_shader
            .as_ref()
            .ok()
            .and_then(|descriptor| descriptor.warning.clone())
    });

    render_surface_frame(
        ui,
        node,
        theme_registry,
        app_theme,
        presentation,
        fallback_title,
        Vec2::new(ui.available_width().max(280.0), 180.0),
        egui::Sense::hover(),
        |ui, rect, response, theme| {
            let painter = ui.painter();
            let inner = rect.shrink(4.0);
            let inner_radius = (theme.radius - 4.0).max(6.0);
            let mut rendered_with_gpu = false;

            if let (Ok(descriptor), Some(presented_host)) =
                (resolved_shader.as_ref(), app.presented_viewport_host)
            {
                let state = app
                    .shader_surfaces
                    .entry(node.id)
                    .or_insert_with(|| ShaderSurfaceState::new(Some(presented_host.target_format)));
                let signature = shader_surface_signature(descriptor);
                state.last_signature = Some(signature.clone());
                state.last_shader_ref = Some(descriptor.shader_ref.clone());
                state.last_warning = descriptor.warning.clone();
                if state.presented_state.is_none() {
                    state.presented_state = Some(Arc::new(Mutex::new(
                        PresentedShaderSurfaceGpuState::new(presented_host.target_format),
                    )));
                }
                if let Some(presented_state) = state.presented_state.as_ref() {
                    let hover = response.hover_pos().unwrap_or(inner.center());
                    let width = inner.width().max(1.0);
                    let height = inner.height().max(1.0);
                    let pointer = [
                        ((hover.x - inner.left()) / width).clamp(0.0, 1.0),
                        ((hover.y - inner.top()) / height).clamp(0.0, 1.0),
                    ];
                    let time_seconds = app.start_time.elapsed().as_secs_f32();
                    let frame_index = time_seconds / (app.frame_dt_seconds.max(1.0 / 240.0));
                    let uniforms = UiShaderSurfaceUniforms {
                        resolution: [width, height],
                        pointer,
                        time_seconds,
                        opacity: presentation.opacity,
                        frame_index,
                        aspect_ratio: width / height,
                        _pad: [0.0; 8],
                    };
                    let pointer_px = [hover.x - inner.left(), hover.y - inner.top()];
                    let (node_id_low, node_id_high) = split_node_id(node.id);
                    let mut storage_payload = UiShaderSurfaceStoragePayload {
                        resolution: [width, height],
                        pointer_uv: pointer,
                        pointer_px,
                        surface_origin_px: [inner.left(), inner.top()],
                        time_seconds,
                        frame_index,
                        opacity: presentation.opacity,
                        aspect_ratio: width / height,
                        node_id_low,
                        node_id_high,
                        _flags: [response.hovered() as u32, rendered_with_gpu as u32],
                        accent_rgba: color32_to_rgba_f32(theme.accent),
                        fill_rgba: color32_to_rgba_f32(theme.canvas_fill),
                        text_run_count: 0,
                        font_atlas_count: 0,
                        resource_binding_count: 0,
                        atlas_glyph_bytes: 0,
                        text_utf8_bytes: 0,
                        surface_texture_width: 1,
                        surface_texture_height: 1,
                        _reserved: [0; 1],
                    };
                    let surface_texture = resolve_ui_shader_surface_texture_payload(
                        &mut app.shader_surface_texture_cache,
                        descriptor,
                    );
                    storage_payload.surface_texture_width = surface_texture.size_px[0];
                    storage_payload.surface_texture_height = surface_texture.size_px[1];
                    let storage_payload_bytes = build_ui_shader_surface_storage_bytes(
                        &storage_payload,
                        descriptor,
                        &surface_texture,
                        theme,
                    );
                    if let Ok(mut gpu_state) = presented_state.lock() {
                        gpu_state.pending_frame = Some(PresentedShaderSurfaceFrame {
                            signature,
                            descriptor: descriptor.clone(),
                            uniforms,
                            storage_payload_bytes,
                            surface_texture,
                        });
                    }
                    painter.add(egui::Shape::Callback(
                        egui_wgpu::Callback::new_paint_callback(
                            inner,
                            PresentedShaderSurfaceCallback {
                                state: Arc::clone(presented_state),
                            },
                        ),
                    ));
                    rendered_with_gpu = true;
                }
            }

            if !rendered_with_gpu {
                painter.rect_filled(inner, inner_radius, theme.canvas_fill);

                let stripe_height = (inner.height() * 0.34).clamp(24.0, 72.0);
                let stripe = egui::Rect::from_min_max(
                    egui::pos2(inner.left(), inner.bottom() - stripe_height),
                    inner.right_bottom(),
                );
                painter.rect_filled(stripe, inner_radius, alpha_tint(theme.accent, 0.18));

                for index in 0..7 {
                    let t = index as f32 / 6.0;
                    let x = inner.left() + (inner.width() * t);
                    painter.line_segment(
                        [
                            egui::pos2(x, inner.top()),
                            egui::pos2(x - 18.0, inner.bottom()),
                        ],
                        Stroke::new(1.0, alpha_tint(theme.stroke, 0.38)),
                    );
                }
            }

            painter.rect_stroke(
                inner,
                inner_radius,
                Stroke::new(1.0, alpha_tint(theme.accent, 0.78)),
                egui::StrokeKind::Inside,
            );
            painter.rect_filled(
                egui::Rect::from_min_size(inner.left_top(), egui::vec2(inner.width(), 82.0)),
                inner_radius,
                alpha_tint(theme.overlay_fill, 0.76),
            );

            painter.text(
                inner.left_top() + egui::vec2(14.0, 14.0),
                egui::Align2::LEFT_TOP,
                format!("<{tag}>"),
                egui::FontId::monospace(11.0),
                theme.tag_color,
            );
            painter.text(
                inner.left_top() + egui::vec2(14.0, 34.0),
                egui::Align2::LEFT_TOP,
                format!("shader: {shader_ref}"),
                egui::FontId::monospace(12.0),
                theme.title_color,
            );
            painter.text(
                inner.left_top() + egui::vec2(14.0, 54.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "stage: {shader_stage}  format: {shader_format}  payload: {shader_payload}"
                ),
                egui::FontId::monospace(10.5),
                theme.body_color,
            );
            painter.text(
                inner.left_top() + egui::vec2(14.0, 72.0),
                egui::Align2::LEFT_TOP,
                format!("surface: {descriptor_tag}  host: {runtime_renderer}  ref: {shader_binding_key}"),
                egui::FontId::monospace(10.0),
                theme.muted_color,
            );
            if let Some(warning) = resolved_warning.as_deref() {
                painter.text(
                    inner.left_bottom() - egui::vec2(-14.0, 18.0),
                    egui::Align2::LEFT_BOTTOM,
                    warning,
                    egui::FontId::monospace(9.5),
                    Color32::LIGHT_YELLOW,
                );
            }
            painter.text(
                inner.right_bottom() - egui::vec2(14.0, 14.0),
                egui::Align2::RIGHT_BOTTOM,
                if rendered_with_gpu {
                    "wgpu-live"
                } else if surface.gpu_backing_required {
                    "gpu-preview"
                } else {
                    "cpu-preview"
                },
                egui::FontId::monospace(10.0),
                theme.tag_color,
            );
        },
    );

    render_children(app, ui, ctx, tree, theme_registry, app_theme, node);
}

fn resolve_ui_shader_surface(
    app: &KainUiNativeApp,
    surface: &UiSurface,
) -> Result<ResolvedUiShaderSurface, String> {
    resolve_ui_shader_surface_from_catalog(
        surface,
        &app.realtime_catalog,
        app.shader_bundle.as_ref(),
    )
}

fn resolve_ui_shader_surface_from_catalog(
    surface: &UiSurface,
    realtime_catalog: &RealtimeBundleCatalog,
    shader_bundle: Option<&ShaderArtifactBundle>,
) -> Result<ResolvedUiShaderSurface, String> {
    let runtime_canvas_binding = realtime_catalog.shader_canvases_by_surface.get(&surface.id);
    let binding = surface.shader.as_ref();
    let shader_bundle =
        shader_bundle.ok_or_else(|| "no shader bundle is loaded for this runtime".to_string())?;
    let shader_ref = binding
        .map(|entry| entry.shader_ref.trim())
        .filter(|value| !value.is_empty())
        .or_else(|| {
            runtime_canvas_binding
                .map(|entry| entry.shader_ref.trim())
                .filter(|value| !value.is_empty())
        })
        .ok_or_else(|| "surface shader binding was empty".to_string())?;

    let resolved_ref = runtime_canvas_binding
        .and_then(|entry| {
            entry
                .shader_bundle_ref_key
                .as_ref()
                .and_then(|key| realtime_catalog.shader_refs_by_key.get(key))
                .cloned()
        })
        .or_else(|| realtime_catalog.shader_refs_by_key.get(shader_ref).cloned())
        .or_else(|| {
            realtime_catalog
                .shader_refs_by_key
                .values()
                .find(|candidate| {
                    candidate.module_name == shader_ref || candidate.shader == shader_ref
                })
                .cloned()
        });

    let mut warning = None;
    let shader_name = runtime_canvas_binding
        .and_then(|entry| entry.shader_name.clone())
        .or_else(|| resolved_ref.as_ref().map(|entry| entry.shader.clone()))
        .unwrap_or_else(|| shader_ref.to_string());
    let module_name = runtime_canvas_binding
        .and_then(|entry| entry.module_name.clone())
        .or_else(|| resolved_ref.as_ref().map(|entry| entry.module_name.clone()))
        .unwrap_or_else(|| shader_ref.to_string());
    let stage = binding
        .and_then(|entry| entry.stage.clone())
        .or_else(|| runtime_canvas_binding.map(|entry| entry.stage.clone()))
        .or_else(|| resolved_ref.as_ref().map(|entry| entry.stage.clone()))
        .unwrap_or_else(|| "fragment".to_string());
    let fragment_entry_point = binding
        .and_then(|entry| entry.entry_point.clone())
        .or_else(|| runtime_canvas_binding.and_then(|entry| entry.entry_point.clone()))
        .or_else(|| {
            resolved_ref
                .as_ref()
                .filter(|entry| entry.stage.eq_ignore_ascii_case("fragment"))
                .map(|entry| entry.entry_point.clone())
        })
        .or_else(|| find_fragment_entry_point(shader_bundle, &module_name, &shader_name))
        .unwrap_or_else(|| "main".to_string());

    let wgsl_source = match wgsl_module_source(shader_bundle, &module_name) {
        Ok(source) => source.into_owned(),
        Err(err) => {
            return Err(format!(
                "module `{module_name}` could not be resolved from shader bundle: {err}"
            ));
        }
    };

    if runtime_canvas_binding.is_none() {
        warning = Some(format!(
            "shader canvas `{}` was not registered in realtime metadata; using surface-local binding",
            surface.id
        ));
    }
    if resolved_ref.is_none() {
        warning = Some(format!(
            "shader ref `{shader_ref}` was not registered in realtime shader refs; using module fallback"
        ));
    }

    let resource_layouts = shader_bundle
        .resource_layouts
        .iter()
        .filter(|layout| layout.shader == shader_name)
        .cloned()
        .collect::<Vec<_>>();
    let font_atlases = runtime_canvas_binding
        .map(|entry| entry.font_atlases.clone())
        .unwrap_or_default();
    let font_asset_sources_by_key = font_atlases
        .iter()
        .filter_map(|atlas| {
            atlas.asset_key.as_deref().and_then(|asset_key| {
                resolve_realtime_asset_source_path(realtime_catalog, asset_key)
                    .map(|asset_source| (atlas.key.clone(), asset_source))
            })
        })
        .collect();

    Ok(ResolvedUiShaderSurface {
        shader_ref: shader_ref.to_string(),
        shader_name,
        module_name,
        fragment_entry_point,
        stage,
        derived_format: binding
            .and_then(|entry| entry.derived_format.clone())
            .or_else(|| runtime_canvas_binding.and_then(|entry| entry.derived_format.clone()))
            .unwrap_or_else(|| {
                format!(
                    "{}-runtime",
                    shader_bundle.canonical_native_payload.as_str()
                )
            }),
        canonical_native_payload: shader_bundle.canonical_native_payload.as_str().to_string(),
        shader_bundle_ref_key: runtime_canvas_binding
            .and_then(|entry| entry.shader_bundle_ref_key.clone())
            .or_else(|| resolved_ref.as_ref().map(|entry| entry.key.clone())),
        wgsl_source,
        resource_layouts,
        font_atlases,
        font_asset_sources_by_key,
        text_runs: runtime_canvas_binding
            .map(|entry| entry.text_runs.clone())
            .unwrap_or_default(),
        resource_bindings: runtime_canvas_binding
            .map(|entry| entry.resource_bindings.clone())
            .unwrap_or_default(),
        warning,
    })
}

fn resolve_realtime_asset_source_path(
    realtime_catalog: &RealtimeBundleCatalog,
    asset_key: &str,
) -> Option<String> {
    let asset = realtime_catalog.assets_by_key.get(asset_key)?;
    let source_path = PathBuf::from(&asset.source);
    if source_path.is_absolute() {
        return Some(source_path.to_string_lossy().to_string());
    }

    realtime_catalog
        .asset_base_dir
        .as_ref()
        .map(|base_dir| base_dir.join(&source_path))
        .unwrap_or(source_path)
        .to_str()
        .map(|value| value.to_string())
}

fn find_fragment_entry_point(
    shader_bundle: &ShaderArtifactBundle,
    module_name: &str,
    shader_name: &str,
) -> Option<String> {
    shader_bundle
        .entry_points
        .iter()
        .find_map(|entry| match_shader_entry(entry, module_name, shader_name, "fragment"))
        .or_else(|| {
            shader_bundle
                .entry_points
                .iter()
                .find_map(|entry| match_shader_entry(entry, module_name, shader_name, "surface"))
        })
}

fn match_shader_entry(
    entry: &ShaderEntryPoint,
    module_name: &str,
    shader_name: &str,
    stage: &str,
) -> Option<String> {
    if entry.stage.eq_ignore_ascii_case(stage)
        && (entry.module_name == module_name || entry.shader == shader_name)
    {
        Some(entry.entry_point.clone())
    } else {
        None
    }
}

fn shader_surface_signature(descriptor: &ResolvedUiShaderSurface) -> String {
    let mut hasher = DefaultHasher::new();
    descriptor.shader_name.hash(&mut hasher);
    descriptor.module_name.hash(&mut hasher);
    descriptor.fragment_entry_point.hash(&mut hasher);
    descriptor.canonical_native_payload.hash(&mut hasher);
    descriptor.shader_bundle_ref_key.hash(&mut hasher);
    descriptor.wgsl_source.hash(&mut hasher);
    for atlas in &descriptor.font_atlases {
        atlas.key.hash(&mut hasher);
        atlas.family.hash(&mut hasher);
        atlas.encoding.hash(&mut hasher);
        atlas.distance_range_px.hash(&mut hasher);
        atlas.glyphs.hash(&mut hasher);
        atlas.cell_size_px.hash(&mut hasher);
        atlas.texture_size_px.hash(&mut hasher);
        atlas.columns.hash(&mut hasher);
        atlas.rows.hash(&mut hasher);
    }
    for run in &descriptor.text_runs {
        run.id.hash(&mut hasher);
        run.text.hash(&mut hasher);
        run.role.hash(&mut hasher);
        run.atlas_key.hash(&mut hasher);
        run.origin_px.hash(&mut hasher);
        run.color_token.hash(&mut hasher);
    }
    for binding in &descriptor.resource_bindings {
        binding.binding_name.hash(&mut hasher);
        binding.resource_kind.hash(&mut hasher);
        binding.source_kind.hash(&mut hasher);
        binding.atlas_key.hash(&mut hasher);
    }
    format!(
        "{}:{}:{:016x}",
        descriptor.module_name,
        descriptor.fragment_entry_point,
        hasher.finish()
    )
}

fn transfer_surface_state_map<T>(
    previous_output: &UiBuildOutput,
    next_output: &UiBuildOutput,
    previous_states: BTreeMap<UiNodeId, T>,
) -> BTreeMap<UiNodeId, T> {
    let previous_identity_map = native_node_identity_map(&previous_output.tree);
    let next_identity_map = native_node_identity_map(&next_output.tree);
    let aliases = next_output
        .systems
        .hot_reload
        .identity_aliases
        .iter()
        .map(|alias| (alias.from.clone(), alias.to.clone()))
        .collect::<BTreeMap<_, _>>();
    let mut transferred = BTreeMap::new();

    for (previous_node_id, state) in previous_states {
        if next_output.tree.nodes.contains_key(&previous_node_id) {
            transferred.insert(previous_node_id, state);
            continue;
        }
        let Some(previous_identity) = previous_identity_map.get(&previous_node_id) else {
            continue;
        };
        let next_identity = aliases.get(previous_identity).unwrap_or(previous_identity);
        if let Some((&next_node_id, _)) = next_identity_map
            .iter()
            .find(|(_, identity)| *identity == next_identity)
        {
            transferred.insert(next_node_id, state);
        }
    }

    transferred
}

fn native_node_identity_map(tree: &UiTree) -> BTreeMap<UiNodeId, String> {
    tree.nodes
        .values()
        .filter_map(|node| {
            node.identity_key
                .clone()
                .or_else(|| node.layout.persistent_layout_id.clone())
                .map(|identity| (node.id, identity))
        })
        .collect()
}

fn surface_descriptor_for_node<'a>(
    app: &'a KainUiNativeApp,
    node: &UiNode,
) -> Option<&'a UiSurface> {
    app.output
        .systems
        .surfaces
        .iter()
        .find(|surface| surface.node == node.id)
}

fn surface_renderer_preference_label(preference: UiSurfaceRendererPreference) -> &'static str {
    match preference {
        UiSurfaceRendererPreference::Auto => "auto",
        UiSurfaceRendererPreference::Native => "native",
        UiSurfaceRendererPreference::Dom => "dom",
        UiSurfaceRendererPreference::Wgpu => "wgpu",
        UiSurfaceRendererPreference::Shader => "shader",
    }
}

fn surface_composition_mode_label(mode: UiSurfaceCompositionMode) -> &'static str {
    match mode {
        UiSurfaceCompositionMode::Host => "host",
        UiSurfaceCompositionMode::LayeredGpu => "layered-gpu",
        UiSurfaceCompositionMode::Viewport => "viewport",
        UiSurfaceCompositionMode::ShaderCanvas => "shader-canvas",
    }
}

fn render_children(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    node: &UiNode,
) {
    let item_spacing = ui.spacing().item_spacing;
    let layout_gap = if node.layout.gap > 0.0 {
        node.layout.gap
    } else {
        app_theme.metrics.content_gap
    };
    ui.spacing_mut().item_spacing = egui::vec2(layout_gap, layout_gap);
    let overflow_x = matches!(
        node.layout.overflow_x,
        UiOverflowBehavior::Scroll | UiOverflowBehavior::Auto
    );
    let overflow_y = matches!(
        node.layout.overflow_y,
        UiOverflowBehavior::Scroll | UiOverflowBehavior::Auto
    );

    if overflow_x || overflow_y {
        let scroll_area = if overflow_x && overflow_y {
            egui::ScrollArea::both()
        } else if overflow_x {
            egui::ScrollArea::horizontal()
        } else {
            egui::ScrollArea::vertical()
        };
        scroll_area
            .id_salt(node_layout_identity(node))
            .auto_shrink([false, false])
            .show(ui, |ui| {
                render_children_content(
                    app,
                    ui,
                    ctx,
                    tree,
                    theme_registry,
                    app_theme,
                    node,
                    layout_gap,
                );
            });
    } else {
        render_children_content(
            app,
            ui,
            ctx,
            tree,
            theme_registry,
            app_theme,
            node,
            layout_gap,
        );
    }

    ui.spacing_mut().item_spacing = item_spacing;
}

#[derive(Clone, Debug)]
enum ChildRenderEntry {
    Single(UiNodeId),
    TabGroup {
        group_id: String,
        tabs: Vec<UiNodeId>,
    },
}

fn node_layout_identity(node: &UiNode) -> String {
    node.layout
        .persistent_layout_id
        .clone()
        .or_else(|| node.identity_key.clone())
        .unwrap_or_else(|| format!("node-{}", node.id.0))
}

fn collect_child_render_entries(tree: &UiTree, node: &UiNode) -> Vec<ChildRenderEntry> {
    let mut entries = Vec::new();
    let mut seen_groups = BTreeSet::new();

    for child_id in &node.children {
        let Some(child_node) = tree.node(*child_id) else {
            continue;
        };
        if let Some(group_id) = child_node.layout.tab_group_id.clone() {
            if !seen_groups.insert(group_id.clone()) {
                continue;
            }
            let mut tabs = node
                .children
                .iter()
                .copied()
                .filter(|candidate| {
                    tree.node(*candidate)
                        .and_then(|candidate_node| candidate_node.layout.tab_group_id.as_deref())
                        == Some(group_id.as_str())
                })
                .collect::<Vec<_>>();
            tabs.sort_by_key(|candidate| {
                tree.node(*candidate)
                    .map(|candidate_node| {
                        (
                            candidate_node.layout.tab_order.unwrap_or(i32::MAX),
                            candidate_node.id.0,
                        )
                    })
                    .unwrap_or((i32::MAX, candidate.0))
            });
            entries.push(ChildRenderEntry::TabGroup { group_id, tabs });
        } else {
            entries.push(ChildRenderEntry::Single(*child_id));
        }
    }

    entries
}

fn child_entry_dock_placement(tree: &UiTree, entry: &ChildRenderEntry) -> kain_ui::UiDockPlacement {
    let representative_id = match entry {
        ChildRenderEntry::Single(id) => Some(*id),
        ChildRenderEntry::TabGroup { tabs, .. } => tabs.first().copied(),
    };
    representative_id
        .and_then(|child_id| tree.node(child_id))
        .and_then(|child_node| child_node.layout.dock)
        .unwrap_or(kain_ui::UiDockPlacement::Center)
}

fn render_child_entry(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    entry: &ChildRenderEntry,
) {
    match entry {
        ChildRenderEntry::Single(id) => {
            render_node(app, ui, ctx, tree, theme_registry, app_theme, *id);
        }
        ChildRenderEntry::TabGroup { group_id, tabs } => {
            render_tab_group(
                app,
                ui,
                ctx,
                tree,
                theme_registry,
                app_theme,
                group_id,
                tabs,
            );
        }
    }
}

fn resolve_active_tab_child(
    app: &mut KainUiNativeApp,
    tree: &UiTree,
    group_id: &str,
    tabs: &[UiNodeId],
) -> Option<UiNodeId> {
    let mut resolved_tabs = Vec::new();
    for tab_id in tabs {
        let Some(tab_node) = tree.node(*tab_id) else {
            continue;
        };
        resolved_tabs.push((
            *tab_id,
            node_layout_identity(tab_node),
            tab_node.layout.tab_default_active,
        ));
    }

    if resolved_tabs.is_empty() {
        return None;
    }

    let stored_layout_id = app
        .output
        .systems
        .workspace_layout
        .active_tabs
        .get(group_id)
        .cloned();
    if let Some(active_layout_id) = stored_layout_id {
        if let Some((tab_id, _, _)) = resolved_tabs
            .iter()
            .find(|(_, layout_id, _)| *layout_id == active_layout_id)
        {
            return Some(*tab_id);
        }
    }

    let fallback = resolved_tabs
        .iter()
        .find(|(_, _, default_active)| *default_active)
        .or_else(|| resolved_tabs.first())
        .cloned();
    if let Some((tab_id, layout_id, _)) = fallback {
        app.remember_active_tab_layout(group_id, layout_id);
        Some(tab_id)
    } else {
        None
    }
}

fn render_tab_group(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    group_id: &str,
    tabs: &[UiNodeId],
) {
    let Some(active_tab_id) = resolve_active_tab_child(app, tree, group_id, tabs) else {
        return;
    };
    let Some(active_node) = tree.node(active_tab_id) else {
        return;
    };
    let chrome_theme = resolve_widget_theme(active_node, theme_registry, app_theme);
    let mut pending_layout_id = None::<String>;
    let active_title = active_node
        .layout
        .tab_label
        .clone()
        .or_else(|| prop_text(active_node, "title").map(|value| value.to_string()))
        .unwrap_or_else(|| format!("Page {}", active_node.id.0));

    Frame::new()
        .fill(alpha_tint(chrome_theme.header_fill, 0.9))
        .stroke(Stroke::new(
            1.0,
            alpha_tint(chrome_theme.header_stroke, 0.92),
        ))
        .corner_radius(app_theme.metrics.radius_large)
        .inner_margin(app_theme.metrics.frame_padding)
        .show(ui, |ui| {
            ui.horizontal_wrapped(|ui| {
                ui.vertical(|ui| {
                    ui.label(
                        RichText::new(group_id)
                            .strong()
                            .size(chrome_theme.title_size)
                            .color(chrome_theme.title_color),
                    );
                    ui.label(
                        RichText::new(format!("{} tabs", tabs.len()))
                            .size(chrome_theme.tag_size)
                            .monospace()
                            .color(chrome_theme.muted_color),
                    );
                });
                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    render_tag_chip(ui, &chrome_theme, &active_title, true);
                    render_tag_chip(ui, &chrome_theme, "tab well", false);
                });
            });
            ui.add_space(chrome_theme.gap * 0.58);
            render_widget_body_frame(ui, &chrome_theme, |ui| {
                ui.horizontal_wrapped(|ui| {
                    for tab_id in tabs {
                        let Some(tab_node) = tree.node(*tab_id) else {
                            continue;
                        };
                        let layout_id = node_layout_identity(tab_node);
                        let is_active = *tab_id == active_tab_id;
                        let label = tab_node
                            .layout
                            .tab_label
                            .clone()
                            .or_else(|| prop_text(tab_node, "title").map(|value| value.to_string()))
                            .unwrap_or_else(|| format!("Page {}", tab_node.id.0));
                        let button = egui::Button::new(
                            RichText::new(label)
                                .size(app_theme.typography.body)
                                .strong()
                                .color(if is_active {
                                    chrome_theme.title_color
                                } else {
                                    chrome_theme.muted_color
                                }),
                        )
                        .fill(if is_active {
                            alpha_tint(chrome_theme.accent, 0.28)
                        } else {
                            alpha_tint(chrome_theme.fill, 0.52)
                        })
                        .stroke(Stroke::new(
                            1.0,
                            if is_active {
                                chrome_theme.accent
                            } else {
                                alpha_tint(chrome_theme.stroke, 0.72)
                            },
                        ))
                        .corner_radius(app_theme.metrics.radius_small)
                        .min_size(egui::vec2(108.0, 34.0));
                        if ui.add(button).clicked() {
                            pending_layout_id = Some(layout_id);
                        }
                    }
                });
            });
            if let Some(layout_id) = pending_layout_id.take() {
                app.remember_active_tab_layout(group_id, layout_id);
            }
            ui.add_space(chrome_theme.gap * 0.8);
            render_node(app, ui, ctx, tree, theme_registry, app_theme, active_tab_id);
        });
}

fn render_children_content(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    node: &UiNode,
    layout_gap: f32,
) {
    let cross_align = layout_align(node.layout.align_items);
    let child_entries = collect_child_render_entries(tree, node);

    match node.layout.kind {
        UiLayoutKind::FlexRow => {
            ui.with_layout(Layout::left_to_right(cross_align), |ui| {
                for entry in &child_entries {
                    render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                }
            });
        }
        UiLayoutKind::FlexColumn => {
            ui.with_layout(Layout::top_down(cross_align), |ui| {
                for entry in &child_entries {
                    render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                }
            });
        }
        UiLayoutKind::Grid => {
            let columns = node
                .props
                .get("columns")
                .and_then(ui_value_as_i64)
                .unwrap_or(2)
                .max(1) as usize;
            egui::Grid::new(format!(
                "kain_ui_native_grid_{}",
                node_layout_identity(node)
            ))
            .num_columns(columns)
            .spacing(egui::vec2(layout_gap, layout_gap))
            .show(ui, |ui| {
                for (index, entry) in child_entries.iter().enumerate() {
                    render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                    if (index + 1) % columns == 0 {
                        ui.end_row();
                    }
                }
            });
        }
        UiLayoutKind::Stack | UiLayoutKind::Absolute => {
            let overlay_theme = resolve_widget_theme(node, theme_registry, app_theme);
            for (index, entry) in child_entries.iter().enumerate() {
                if index > 0 {
                    ui.add_space(-overlay_theme.gap * 0.35);
                }
                ui.scope(|ui| {
                    ui.set_width(ui.available_width());
                    render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                });
            }
        }
        UiLayoutKind::Dock => {
            let split = node.layout.split_ratio.unwrap_or(0.28).clamp(0.15, 0.65);
            let mut left = Vec::new();
            let mut right = Vec::new();
            let mut top = Vec::new();
            let mut bottom = Vec::new();
            let mut center = Vec::new();

            for entry in child_entries {
                let placement = child_entry_dock_placement(tree, &entry);
                match placement {
                    kain_ui::UiDockPlacement::Left => left.push(entry),
                    kain_ui::UiDockPlacement::Right => right.push(entry),
                    kain_ui::UiDockPlacement::Top => top.push(entry),
                    kain_ui::UiDockPlacement::Bottom => bottom.push(entry),
                    _ => center.push(entry),
                }
            }

            if !top.is_empty() {
                ui.scope(|ui| {
                    ui.set_width(ui.available_width());
                    for entry in &top {
                        render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                    }
                });
                ui.add_space(layout_gap);
            }

            ui.horizontal_top(|ui| {
                if !left.is_empty() {
                    let width = ui.available_width() * split;
                    ui.scope(|ui| {
                        ui.set_width(width);
                        for entry in &left {
                            render_child_entry(
                                app,
                                ui,
                                ctx,
                                tree,
                                theme_registry,
                                app_theme,
                                entry,
                            );
                        }
                    });
                    ui.add_space(layout_gap);
                }

                ui.scope(|ui| {
                    for entry in &center {
                        render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                    }
                });

                if !right.is_empty() {
                    ui.add_space(layout_gap);
                    let width = ui.available_width() * split;
                    ui.scope(|ui| {
                        ui.set_width(width);
                        for entry in &right {
                            render_child_entry(
                                app,
                                ui,
                                ctx,
                                tree,
                                theme_registry,
                                app_theme,
                                entry,
                            );
                        }
                    });
                }
            });

            if !bottom.is_empty() {
                ui.add_space(layout_gap);
                ui.scope(|ui| {
                    ui.set_width(ui.available_width());
                    for entry in &bottom {
                        render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
                    }
                });
            }
        }
        _ => {
            for entry in &child_entries {
                render_child_entry(app, ui, ctx, tree, theme_registry, app_theme, entry);
            }
        }
    }
}

fn render_viewport_surface(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    node: &UiNode,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    presentation: NativeNodePresentation,
    fallback_title: &str,
) {
    render_surface_frame(
        ui,
        node,
        theme_registry,
        app_theme,
        presentation,
        fallback_title,
        Vec2::new(
            ui.available_width().max(420.0),
            ui.available_height().clamp(420.0, 780.0),
        ),
        egui::Sense::click_and_drag(),
        |ui, rect, response, theme| {
            let painter = ui.painter();
            let inner_rect = rect.shrink(4.0);
            let binding = resolve_viewport_binding(app, node);
            let scene_name = binding.scene_name.clone();
            trace_runtime(format!(
                "viewport: enter node={} scene={} bundle_node={} enabled={}",
                node.id.0,
                scene_name,
                binding
                    .viewport_node
                    .as_deref()
                    .unwrap_or("<prop-or-default>"),
                app.runtime_settings.enable_viewports
            ));
            if !app.runtime_settings.enable_viewports {
                painter.rect_filled(inner_rect, theme.radius.max(10.0), theme.canvas_fill);
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "viewport runtime disabled by KAIN_UI_NATIVE_ENABLE_VIEWPORTS=0",
                    egui::FontId::proportional(theme.body_size),
                    app_theme.palette.highlight,
                );
                trace_runtime(format!("viewport: skipped node={}", node.id.0));
                return;
            }
            let elapsed_seconds = app.start_time.elapsed().as_secs_f32();
            let resolution = viewport_render_resolution(
                inner_rect.size(),
                app.runtime_settings.viewport_max_axis_px,
            );
            let Some((scene_snapshot, reference_pose, viewport_summary)) =
                app.scene_catalog.scene(&scene_name).map(|scene| {
                    (
                        scene.clone(),
                        resolve_viewport_reference_pose(scene, binding.camera.as_ref()),
                        scene.viewport_summary.clone(),
                    )
                })
            else {
                painter.rect_filled(inner_rect, theme.radius.max(10.0), theme.canvas_fill);
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    format!("Scene `{scene_name}` was not found"),
                    egui::FontId::proportional(16.0),
                    Color32::LIGHT_RED,
                );
                return;
            };
            trace_runtime(format!("viewport: resolved scene node={}", node.id.0));
            let initial_gizmo_binding =
                resolved_viewport_gizmo_binding(binding.presentation.as_ref());
            let surface =
                app.viewport_surfaces
                    .entry(node.id)
                    .or_insert_with(|| ViewportSurfaceState {
                        texture: None,
                        scene_name: scene_name.clone(),
                        bundle_viewport_node: binding.viewport_node.clone(),
                        bundle_material_refs: binding.material_refs.clone(),
                        bundle_shader_ref_keys: binding.shader_ref_keys.clone(),
                        bundle_camera: binding.camera.clone(),
                        bundle_presentation: binding.presentation.clone(),
                        bundle_warning: binding.warning.clone(),
                        controller: ViewportCameraController::from_pose(&reference_pose),
                        last_render_at: None,
                        last_stats: RenderStats::default(),
                        selected_instance_id: None,
                        manipulator_mode: manipulator_mode_from_binding_value(
                            initial_gizmo_binding.default_mode.as_deref(),
                        ),
                        manipulator_space: manipulator_space_from_binding_value(
                            initial_gizmo_binding.default_space.as_deref(),
                        ),
                        snap_enabled: initial_gizmo_binding.snap_default_enabled.unwrap_or(false),
                        last_pick: None,
                        instance_transform_overrides: BTreeMap::new(),
                        active_manipulation: None,
                        presented_state: app.presented_viewport_host.map(|host| {
                            Arc::new(Mutex::new(PresentedViewportGpuState::new(
                                host.target_format,
                                app.shader_bundle.as_ref(),
                            )))
                        }),
                    });
            trace_runtime(format!("viewport: state_ready node={}", node.id.0));
            if surface.scene_name != scene_name
                || surface.bundle_camera.as_ref() != binding.camera.as_ref()
                || surface.bundle_presentation.as_ref() != binding.presentation.as_ref()
            {
                surface.scene_name = scene_name.clone();
                surface.bundle_viewport_node = binding.viewport_node.clone();
                surface.bundle_material_refs = binding.material_refs.clone();
                surface.bundle_shader_ref_keys = binding.shader_ref_keys.clone();
                surface.bundle_camera = binding.camera.clone();
                surface.bundle_presentation = binding.presentation.clone();
                surface.bundle_warning = binding.warning.clone();
                surface.controller.recenter(&reference_pose);
                surface.texture = None;
                surface.last_render_at = None;
                surface.last_stats = RenderStats::default();
                surface.selected_instance_id = None;
                surface.last_pick = None;
                surface.instance_transform_overrides.clear();
                surface.active_manipulation = None;
                let reset_gizmo_binding =
                    resolved_viewport_gizmo_binding(binding.presentation.as_ref());
                surface.manipulator_mode = manipulator_mode_from_binding_value(
                    reset_gizmo_binding.default_mode.as_deref(),
                );
                surface.manipulator_space = manipulator_space_from_binding_value(
                    reset_gizmo_binding.default_space.as_deref(),
                );
                surface.snap_enabled = reset_gizmo_binding.snap_default_enabled.unwrap_or(false);
                if let Some(host) = app.presented_viewport_host {
                    surface.presented_state =
                        Some(Arc::new(Mutex::new(PresentedViewportGpuState::new(
                            host.target_format,
                            app.shader_bundle.as_ref(),
                        ))));
                } else {
                    surface.presented_state = None;
                }
            } else {
                surface.bundle_viewport_node = binding.viewport_node.clone();
                surface.bundle_material_refs = binding.material_refs.clone();
                surface.bundle_shader_ref_keys = binding.shader_ref_keys.clone();
                surface.bundle_camera = binding.camera.clone();
                surface.bundle_presentation = binding.presentation.clone();
                surface.bundle_warning = binding.warning.clone();
            }
            let gizmo_binding =
                resolved_viewport_gizmo_binding(surface.bundle_presentation.as_ref());
            let drag_trigger =
                viewport_drag_trigger_from_binding_value(gizmo_binding.drag_trigger.as_deref());
            sync_viewport_input(
                &app.viewport_input,
                response,
                &reference_pose,
                &mut surface.controller,
                drag_trigger,
                app.frame_dt_seconds,
            );
            apply_viewport_grounding(
                &scene_snapshot,
                &app.viewport_input,
                &mut surface.controller,
                elapsed_seconds,
                app.frame_dt_seconds,
            );
            sync_viewport_manipulation(
                &scene_snapshot,
                response,
                inner_rect,
                resolution,
                &app.viewport_input,
                elapsed_seconds,
                &surface.controller.pose(&reference_pose),
                &gizmo_binding,
                surface,
            );
            if response.hovered() || response.has_focus() {
                if viewport_hotkey_pressed(
                    &app.viewport_input,
                    gizmo_binding.translate_hotkey.as_deref(),
                ) {
                    surface.manipulator_mode = ManipulatorMode::Translate;
                } else if viewport_hotkey_pressed(
                    &app.viewport_input,
                    gizmo_binding.rotate_hotkey.as_deref(),
                ) {
                    surface.manipulator_mode = ManipulatorMode::Rotate;
                } else if viewport_hotkey_pressed(
                    &app.viewport_input,
                    gizmo_binding.scale_hotkey.as_deref(),
                ) {
                    surface.manipulator_mode = ManipulatorMode::Scale;
                }
                if viewport_hotkey_pressed(
                    &app.viewport_input,
                    gizmo_binding.cycle_space_hotkey.as_deref(),
                ) {
                    surface.manipulator_space = match surface.manipulator_space {
                        ManipulatorSpace::World => ManipulatorSpace::Local,
                        ManipulatorSpace::Local => ManipulatorSpace::World,
                    };
                }
                if viewport_hotkey_pressed(
                    &app.viewport_input,
                    gizmo_binding.toggle_snap_hotkey.as_deref(),
                ) {
                    surface.snap_enabled = !surface.snap_enabled;
                }
            }
            let active_camera = surface.controller.pose(&reference_pose);
            let render_view = RenderViewSettings {
                camera: Some(active_camera),
                selected_instance_id: surface.selected_instance_id.clone(),
                manipulator_mode: gizmo_binding
                    .visible
                    .unwrap_or(true)
                    .then_some(surface.manipulator_mode),
                fog_density: surface
                    .bundle_presentation
                    .as_ref()
                    .and_then(|presentation| presentation.fog_density),
                instance_transform_overrides: surface.instance_transform_overrides.clone(),
            };
            if response.clicked()
                && !viewport_drag_trigger_active(&app.viewport_input, drag_trigger)
            {
                if let Some(pointer_pos) = response.interact_pointer_pos() {
                    if inner_rect.contains(pointer_pos) {
                        let relative = pointer_pos - inner_rect.min;
                        let ray = PickingRay::from_viewport_pixel(
                            relative.x,
                            relative.y,
                            resolution,
                            render_view
                                .camera
                                .as_ref()
                                .expect("viewport render view should always include a camera"),
                        );
                        let query = PickingQuery::new(ray, elapsed_seconds);
                        let gpu_pick = app.renderer.pick_catalog_scene_at(
                            &app.scene_catalog,
                            &scene_name,
                            elapsed_seconds,
                            resolution,
                            &render_view,
                            relative.x,
                            relative.y,
                        );
                        surface.last_pick = match gpu_pick {
                            Ok(Some(hit)) => Some(hit),
                            Ok(None) => CpuPickingService.pick_catalog_scene_with_overrides(
                                &app.scene_catalog,
                                &scene_name,
                                &query,
                                &surface.instance_transform_overrides,
                            ),
                            Err(err) => {
                                trace_runtime(format!(
                                    "viewport: gpu_pick_failed node={} error={err}",
                                    node.id.0
                                ));
                                CpuPickingService.pick_catalog_scene_with_overrides(
                                    &app.scene_catalog,
                                    &scene_name,
                                    &query,
                                    &surface.instance_transform_overrides,
                                )
                            }
                        };
                        surface.selected_instance_id = surface
                            .last_pick
                            .as_ref()
                            .map(|hit| hit.target.instance_id.clone());
                    }
                }
            }
            let interactive = response.hovered() || response.dragged();
            let render_interval = if interactive {
                Duration::from_millis(app.runtime_settings.viewport_render_interval_interactive_ms)
            } else {
                Duration::from_millis(app.runtime_settings.viewport_render_interval_idle_ms)
            };
            let should_render = elapsed_seconds
                >= (app.runtime_settings.viewport_startup_delay_ms as f32 / 1000.0)
                && ((surface.presented_state.is_some() && surface.last_render_at.is_none())
                    || surface.texture.is_none()
                    || surface
                        .last_render_at
                        .is_none_or(|instant| instant.elapsed() >= render_interval));
            trace_runtime(format!(
                "viewport: node={} interactive={} should_render={} elapsed_ms={:.0} resolution={}x{}",
                node.id.0,
                interactive,
                should_render,
                elapsed_seconds * 1000.0,
                resolution.width,
                resolution.height
            ));

            if should_render {
                let render_start = Instant::now();

                if let Some(presented_state) = surface.presented_state.as_ref() {
                    match prepare_wgpu_frame(
                        &scene_snapshot,
                        elapsed_seconds,
                        resolution,
                        &render_view,
                    ) {
                        Ok(prepared_frame) => {
                            surface.last_stats = prepared_frame.stats.clone();
                            surface.last_render_at = Some(Instant::now());
                            if let Ok(mut state) = presented_state.lock() {
                                state.prepared_frame = Some(prepared_frame);
                            }
                            trace_runtime(format!(
                                "viewport: presented node={} ms={} tris={} particles={}",
                                node.id.0,
                                render_start.elapsed().as_millis(),
                                surface.last_stats.triangles_submitted,
                                surface.last_stats.particles_submitted
                            ));
                        }
                        Err(err) => {
                            trace_runtime(format!(
                                "viewport: failed node={} error={err}",
                                node.id.0
                            ));
                            painter.rect_filled(
                                inner_rect,
                                theme.radius.max(10.0),
                                theme.canvas_fill,
                            );
                            painter.text(
                                inner_rect.center(),
                                egui::Align2::CENTER_CENTER,
                                format!("Viewport render failed:\n{err}"),
                                egui::FontId::proportional(16.0),
                                Color32::LIGHT_RED,
                            );
                            return;
                        }
                    }
                } else {
                    match app.renderer.render_catalog_scene_with_view(
                        &app.scene_catalog,
                        &scene_name,
                        elapsed_seconds,
                        resolution,
                        &render_view,
                    ) {
                        Ok(frame) => {
                            let image = egui::ColorImage::from_rgba_unmultiplied(
                                [frame.width, frame.height],
                                &frame.rgba,
                            );
                            let texture_name = format!("kain_viewport_surface_{}", node.id.0);
                            if let Some(texture) = surface.texture.as_mut() {
                                texture.set(image, egui::TextureOptions::LINEAR);
                            } else {
                                surface.texture = Some(ctx.load_texture(
                                    texture_name,
                                    image,
                                    egui::TextureOptions::LINEAR,
                                ));
                            }
                            surface.last_stats = frame.stats;
                            surface.last_render_at = Some(Instant::now());
                            trace_runtime(format!(
                                "viewport: rendered node={} ms={} tris={} particles={}",
                                node.id.0,
                                render_start.elapsed().as_millis(),
                                surface.last_stats.triangles_submitted,
                                surface.last_stats.particles_submitted
                            ));
                        }
                        Err(err) => {
                            trace_runtime(format!(
                                "viewport: failed node={} error={err}",
                                node.id.0
                            ));
                            painter.rect_filled(
                                inner_rect,
                                theme.radius.max(10.0),
                                theme.canvas_fill,
                            );
                            painter.text(
                                inner_rect.center(),
                                egui::Align2::CENTER_CENTER,
                                format!("Viewport render failed:\n{err}"),
                                egui::FontId::proportional(16.0),
                                Color32::LIGHT_RED,
                            );
                            return;
                        }
                    }
                }
            }

            if let Some(presented_state) = surface.presented_state.as_ref() {
                let has_frame = presented_state
                    .lock()
                    .ok()
                    .and_then(|state| state.prepared_frame.as_ref().map(|_| ()))
                    .is_some();
                if has_frame {
                    painter.add(egui::Shape::Callback(
                        egui_wgpu::Callback::new_paint_callback(
                            inner_rect,
                            PresentedViewportCallback {
                                state: Arc::clone(presented_state),
                            },
                        ),
                    ));
                } else {
                    painter.rect_filled(inner_rect, theme.radius.max(10.0), theme.canvas_fill);
                    painter.text(
                        inner_rect.center(),
                        egui::Align2::CENTER_CENTER,
                        "warming native viewport...",
                        egui::FontId::proportional(theme.title_size),
                        theme.tag_color,
                    );
                }
            } else if let Some(texture) = surface.texture.as_ref() {
                painter.image(
                    texture.id(),
                    inner_rect,
                    egui::Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)),
                    Color32::WHITE,
                );
            } else {
                painter.rect_filled(inner_rect, theme.radius.max(10.0), theme.canvas_fill);
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "warming native viewport...",
                    egui::FontId::proportional(theme.title_size),
                    theme.tag_color,
                );
            }
            painter.rect_stroke(
                inner_rect,
                theme.radius.max(10.0),
                Stroke::new(1.0, theme.stroke),
                egui::StrokeKind::Inside,
            );

            let overlay_rect = egui::Rect::from_min_size(
                inner_rect.min + egui::vec2(12.0, 12.0),
                Vec2::new(372.0, 142.0),
            );
            painter.rect_filled(overlay_rect, 12.0, theme.overlay_fill);
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 10.0),
                egui::Align2::LEFT_TOP,
                match surface.bundle_viewport_node.as_deref() {
                    Some(viewport_node) => {
                        format!("scene: {scene_name}  |  bundle: {viewport_node}")
                    }
                    None => format!("scene: {scene_name}"),
                },
                egui::FontId::monospace(13.0),
                app_theme.palette.highlight,
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 28.0),
                egui::Align2::LEFT_TOP,
                format!("renderer: {}", app.active_renderer_label),
                egui::FontId::monospace(11.0),
                app_theme.palette.success,
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 44.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "tris: {} submitted / {} lit / {} px",
                    surface.last_stats.triangles_submitted,
                    surface.last_stats.triangles_rasterized,
                    surface.last_stats.pixels_shaded
                ),
                egui::FontId::monospace(12.0),
                app_theme.palette.accent_soft,
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 61.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "selection: {}  |  gizmo: {}/{}  |  snap: {}",
                    surface.selected_instance_id.as_deref().unwrap_or("none"),
                    manipulator_mode_label(surface.manipulator_mode),
                    manipulator_space_label(surface.manipulator_space),
                    if surface.snap_enabled { "on" } else { "off" }
                ),
                egui::FontId::monospace(10.5),
                app_theme.palette.text_muted,
            );
            let material_line = if surface.bundle_material_refs.is_empty() {
                "materials: none".to_string()
            } else {
                format!("materials: {}", surface.bundle_material_refs.join(", "))
            };
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 77.0),
                egui::Align2::LEFT_TOP,
                material_line,
                egui::FontId::monospace(10.5),
                app_theme.palette.success,
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 93.0),
                egui::Align2::LEFT_TOP,
                if surface.bundle_shader_ref_keys.is_empty() {
                    format!(
                        "particles: {} submitted / {} blended",
                        surface.last_stats.particles_submitted, surface.last_stats.particles_shaded
                    )
                } else {
                    format!("shader refs: {}", surface.bundle_shader_ref_keys.join(", "))
                },
                egui::FontId::monospace(10.0),
                app_theme.palette.accent_soft,
            );
            let camera_position = surface.controller.position;
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 109.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "cam: [{:.1}, {:.1}, {:.1}]  speed: {:.1}  {}",
                    camera_position.x,
                    camera_position.y,
                    camera_position.z,
                    surface.controller.move_speed,
                    if surface.controller.grounded {
                        "grounded"
                    } else {
                        "falling"
                    }
                ),
                egui::FontId::monospace(10.0),
                app_theme.palette.accent_soft,
            );
            if let Some(warning) = surface.bundle_warning.as_deref() {
                painter.text(
                    overlay_rect.min + egui::vec2(10.0, 125.0),
                    egui::Align2::LEFT_TOP,
                    format!("bundle warning: {warning}"),
                    egui::FontId::monospace(9.5),
                    Color32::LIGHT_RED,
                );
            }
            let controls_rect = egui::Rect::from_min_size(
                egui::pos2(inner_rect.left() + 12.0, inner_rect.bottom() - 38.0),
                Vec2::new(620.0, 26.0),
            );
            painter.rect_filled(controls_rect, 10.0, theme.overlay_fill);
            painter.text(
                controls_rect.center(),
                egui::Align2::CENTER_CENTER,
                format!(
                    "roam WASD + QE  |  drag to look  |  {} transforms  |  {} / {} / {} modes  |  {} space  |  {} snap",
                    viewport_drag_trigger_label(drag_trigger),
                    viewport_hotkey_label(gizmo_binding.translate_hotkey.as_deref(), "T"),
                    viewport_hotkey_label(gizmo_binding.rotate_hotkey.as_deref(), "R"),
                    viewport_hotkey_label(gizmo_binding.scale_hotkey.as_deref(), "Y"),
                    viewport_hotkey_label(gizmo_binding.cycle_space_hotkey.as_deref(), "U"),
                    viewport_hotkey_label(gizmo_binding.toggle_snap_hotkey.as_deref(), "I")
                ),
                egui::FontId::monospace(10.5),
                if response.has_focus() || response.hovered() {
                    app_theme.palette.highlight
                } else {
                    app_theme.palette.text_muted
                },
            );
            let summary_rect = egui::Rect::from_min_size(
                egui::pos2(inner_rect.right() - 250.0, inner_rect.top() + 12.0),
                Vec2::new(238.0, 40.0),
            );
            painter.rect_filled(summary_rect, 10.0, theme.overlay_fill);
            painter.text(
                summary_rect.min + egui::vec2(10.0, 8.0),
                egui::Align2::LEFT_TOP,
                viewport_summary.as_str(),
                egui::FontId::monospace(10.5),
                app_theme.palette.accent_soft,
            );
            if let Some(hit) = surface.last_pick.as_ref() {
                painter.text(
                    summary_rect.min + egui::vec2(10.0, 22.0),
                    egui::Align2::LEFT_TOP,
                    format!(
                        "pick: {} @ {:.2}m [{:.2}, {:.2}, {:.2}]",
                        hit.target.instance_id,
                        hit.distance,
                        hit.position.x,
                        hit.position.y,
                        hit.position.z
                    ),
                    egui::FontId::monospace(10.0),
                    app_theme.palette.highlight,
                );
            }
        },
    );
}

fn resolve_viewport_binding(app: &KainUiNativeApp, node: &UiNode) -> ResolvedViewportBinding {
    let viewport_node = format!("surface.node.{}", node.id.0);
    if let Some(binding) = app.realtime_catalog.scenes_by_viewport.get(&viewport_node) {
        let mut warnings = Vec::new();
        for material_ref in &binding.material_refs {
            if !app
                .realtime_catalog
                .materials_by_id
                .contains_key(material_ref)
            {
                warnings.push(format!("missing material `{material_ref}`"));
            }
        }
        for shader_ref_key in &binding.shader_bundle_ref_keys {
            if !app
                .realtime_catalog
                .shader_refs_by_key
                .contains_key(shader_ref_key)
            {
                warnings.push(format!("missing shader ref `{shader_ref_key}`"));
            }
        }
        return ResolvedViewportBinding {
            viewport_node: Some(viewport_node),
            scene_name: binding.scene.clone(),
            material_refs: binding.material_refs.clone(),
            shader_ref_keys: binding.shader_bundle_ref_keys.clone(),
            camera: binding.camera.clone(),
            presentation: binding.presentation.clone(),
            warning: (!warnings.is_empty()).then(|| warnings.join(" | ")),
        };
    }

    ResolvedViewportBinding {
        viewport_node: None,
        scene_name: prop_text(node, "scene")
            .unwrap_or(app.scene_catalog.default_scene.as_str())
            .to_string(),
        material_refs: prop_text(node, "material")
            .map(|value| vec![value.to_string()])
            .unwrap_or_default(),
        shader_ref_keys: Vec::new(),
        camera: resolve_viewport_camera_binding_from_node(node),
        presentation: resolve_viewport_presentation_binding_from_node(node),
        warning: None,
    }
}

fn render_surface_frame(
    ui: &mut egui::Ui,
    node: &UiNode,
    theme_registry: &UiThemeRegistry,
    app_theme: &NativeAppTheme,
    presentation: NativeNodePresentation,
    fallback_title: &str,
    desired_size: Vec2,
    sense: egui::Sense,
    paint: impl FnOnce(&mut egui::Ui, egui::Rect, &egui::Response, &NativeWidgetTheme),
) {
    let title = prop_text(node, "title").unwrap_or(fallback_title);
    let theme = apply_node_presentation_to_theme(
        resolve_widget_theme(node, theme_registry, app_theme),
        presentation,
    );
    themed_frame(&theme).show(ui, |ui| {
        render_widget_header(ui, &theme, title, node, widget_kind_key(&node.kind));
        ui.add_space(theme.gap * 0.58);
        render_widget_body_frame(ui, &theme, |ui| {
            let (rect, response) = ui.allocate_exact_size(desired_size, sense);
            paint(ui, rect, &response, &theme);
        });
    });
}

fn snapshot_viewport_input(ctx: &egui::Context) -> ViewportInputSnapshot {
    ctx.input(|input| ViewportInputSnapshot {
        scroll_delta_y: input.raw_scroll_delta.y,
        pointer_delta: input.pointer.delta(),
        modifier_ctrl: input.modifiers.ctrl,
        modifier_alt: input.modifiers.alt,
        pressed_hotkeys: capture_viewport_pressed_hotkeys(input),
        move_forward: input.key_down(egui::Key::W),
        move_backward: input.key_down(egui::Key::S),
        move_right: input.key_down(egui::Key::D),
        move_left: input.key_down(egui::Key::A),
        move_up: input.key_down(egui::Key::Q),
        move_down: input.key_down(egui::Key::E),
        speed_boost: input.modifiers.shift,
        recenter: input.key_pressed(egui::Key::Space),
    })
}

fn sync_viewport_input(
    input: &ViewportInputSnapshot,
    response: &egui::Response,
    reference_pose: &CameraPose,
    controller: &mut ViewportCameraController,
    drag_trigger: ViewportManipulatorDragTrigger,
    dt_seconds: f32,
) {
    if response.hovered() && input.scroll_delta_y.abs() > f32::EPSILON {
        let speed_scale = (1.0 + input.scroll_delta_y * 0.0015).clamp(0.5, 2.0);
        controller.move_speed = (controller.move_speed * speed_scale).clamp(1.0, 42.0);
    }

    if response.dragged_by(egui::PointerButton::Primary)
        && !viewport_drag_trigger_active(input, drag_trigger)
    {
        let delta = input.pointer_delta;
        controller.yaw += delta.x * 0.009;
        controller.pitch = (controller.pitch - delta.y * 0.009).clamp(-1.45, 1.45);
    }

    let accepts_movement = response.hovered() || response.dragged();
    if !accepts_movement {
        return;
    }

    if input.recenter {
        controller.recenter(reference_pose);
    }

    let mut movement = Vec3::ZERO;
    let forward = controller.planar_forward();
    let right = controller.right();
    if input.move_forward {
        movement += forward;
    }
    if input.move_backward {
        movement += forward * -1.0;
    }
    if input.move_right {
        movement += right;
    }
    if input.move_left {
        movement += right * -1.0;
    }
    if input.move_up {
        movement += Vec3::UP;
    }
    if input.move_down {
        movement += Vec3::UP * -1.0;
    }

    if movement.length() > f32::EPSILON {
        let speed = if input.speed_boost {
            controller.move_speed * 2.8
        } else {
            controller.move_speed
        };
        controller.position += movement.normalize() * dt_seconds * speed;
    }
}

fn sync_viewport_manipulation(
    scene: &SceneDescription,
    response: &egui::Response,
    inner_rect: egui::Rect,
    resolution: RenderResolution,
    input: &ViewportInputSnapshot,
    scene_time_seconds: f32,
    camera: &CameraPose,
    gizmo_binding: &RealtimeViewportGizmoBinding,
    surface: &mut ViewportSurfaceState,
) {
    if !gizmo_binding.visible.unwrap_or(true) {
        surface.active_manipulation = None;
        return;
    }
    let drag_trigger =
        viewport_drag_trigger_from_binding_value(gizmo_binding.drag_trigger.as_deref());
    if !viewport_drag_trigger_active(input, drag_trigger) {
        surface.active_manipulation = None;
        return;
    }

    let Some(selected_instance_id) = surface.selected_instance_id.clone() else {
        surface.active_manipulation = None;
        return;
    };

    if !response.dragged_by(egui::PointerButton::Primary) {
        surface.active_manipulation = None;
        return;
    }

    let Some(pointer_pos) = response.interact_pointer_pos() else {
        surface.active_manipulation = None;
        return;
    };
    if !inner_rect.contains(pointer_pos) {
        surface.active_manipulation = None;
        return;
    }

    let drag_state_needs_reset = surface
        .active_manipulation
        .as_ref()
        .is_none_or(|state| state.selected_instance_id != selected_instance_id);
    if drag_state_needs_reset {
        let Some(drag_origin_transform) = resolve_effective_instance_transform(
            scene,
            scene_time_seconds,
            &surface.instance_transform_overrides,
            &selected_instance_id,
        ) else {
            surface.active_manipulation = None;
            return;
        };
        surface.active_manipulation = Some(ViewportManipulatorState {
            selected_instance_id: selected_instance_id.clone(),
            drag_origin_pointer: pointer_pos,
            drag_origin_transform,
        });
    }

    let Some(active_manipulation) = surface.active_manipulation.as_ref() else {
        return;
    };
    let pointer_delta = pointer_pos - active_manipulation.drag_origin_pointer;
    let updated_transform = manipulated_transform_from_drag(
        camera,
        resolution,
        surface.manipulator_mode,
        surface.manipulator_space,
        &active_manipulation.drag_origin_transform,
        pointer_delta,
        surface.snap_enabled,
        viewport_gizmo_snap_settings(gizmo_binding),
    );
    surface
        .instance_transform_overrides
        .insert(selected_instance_id, updated_transform);
    surface.last_render_at = None;
}

fn resolve_effective_instance_transform(
    scene: &SceneDescription,
    scene_time_seconds: f32,
    instance_transform_overrides: &BTreeMap<String, SceneTransform>,
    instance_id: &str,
) -> Option<SceneTransform> {
    scene
        .animated_instances_with_overrides(scene_time_seconds, instance_transform_overrides)
        .into_iter()
        .find(|instance| instance.id == instance_id)
        .map(|instance| instance.transform)
}

fn manipulated_transform_from_drag(
    camera: &CameraPose,
    resolution: RenderResolution,
    manipulator_mode: ManipulatorMode,
    manipulator_space: ManipulatorSpace,
    drag_origin_transform: &SceneTransform,
    pointer_delta: Vec2,
    snap_enabled: bool,
    snap_settings: ViewportGizmoSnapSettings,
) -> SceneTransform {
    let viewport_scale = (resolution.width.min(resolution.height) as f32).max(1.0);
    let depth_scale = (camera.position.distance(drag_origin_transform.translation) * 2.4
        / viewport_scale)
        .clamp(0.006, 0.18);
    let camera_right = camera.right();
    let camera_up = camera.right().cross(camera.forward()).normalize();

    let mut updated_transform = drag_origin_transform.clone();
    match manipulator_mode {
        ManipulatorMode::Translate => {
            updated_transform.translation = drag_origin_transform.translation
                + camera_right * (pointer_delta.x * depth_scale)
                + camera_up * (-pointer_delta.y * depth_scale);
            if snap_enabled {
                if let Some(step) = snap_settings.translation_step {
                    updated_transform.translation = snap_vec3(updated_transform.translation, step);
                }
            }
        }
        ManipulatorMode::Rotate => {
            updated_transform.rotation_radians = drag_origin_transform.rotation_radians
                + Vec3::new(pointer_delta.y * -0.008, pointer_delta.x * 0.008, 0.0);
            if snap_enabled {
                if let Some(step) = snap_settings.rotation_step_radians {
                    updated_transform.rotation_radians =
                        snap_vec3(updated_transform.rotation_radians, step);
                }
            }
        }
        ManipulatorMode::Scale => {
            let scale_factor = (1.0 + (pointer_delta.x - pointer_delta.y) * 0.004).clamp(0.25, 5.0);
            updated_transform.scale = drag_origin_transform.scale * scale_factor;
            if snap_enabled {
                if let Some(step) = snap_settings.scale_step {
                    updated_transform.scale = Vec3::new(
                        round_to_step(updated_transform.scale.x, step).max(step),
                        round_to_step(updated_transform.scale.y, step).max(step),
                        round_to_step(updated_transform.scale.z, step).max(step),
                    );
                }
            }
        }
    }
    let _ = manipulator_space;
    updated_transform
}

fn apply_viewport_grounding(
    scene: &SceneDescription,
    input: &ViewportInputSnapshot,
    controller: &mut ViewportCameraController,
    scene_time_seconds: f32,
    dt_seconds: f32,
) {
    let Some(ground_y) = scene.ground_height_at(controller.position, scene_time_seconds) else {
        controller.grounded = false;
        return;
    };
    let target_eye_height = ground_y + controller.eye_height;
    if input.move_up || input.move_down {
        controller.grounded = false;
        controller.vertical_velocity = 0.0;
        return;
    }

    if controller.position.y <= target_eye_height + 0.08 {
        controller.position.y = target_eye_height;
        controller.vertical_velocity = 0.0;
        controller.grounded = true;
        return;
    }

    controller.vertical_velocity -= 28.0 * dt_seconds;
    controller.position.y += controller.vertical_velocity * dt_seconds;
    if controller.position.y <= target_eye_height {
        controller.position.y = target_eye_height;
        controller.vertical_velocity = 0.0;
        controller.grounded = true;
    } else {
        controller.grounded = false;
    }
}

fn viewport_render_resolution(size: Vec2, max_axis_px: u64) -> RenderResolution {
    let width = size.x.max(32.0);
    let height = size.y.max(32.0);
    let dominant_axis = width.max(height);
    let max_axis = max_axis_px.max(128) as f32;
    let scale = if dominant_axis > max_axis {
        max_axis / dominant_axis
    } else {
        1.0
    };
    RenderResolution::new(
        (width * scale).round() as usize,
        (height * scale).round() as usize,
    )
}

fn prop_text<'a>(node: &'a UiNode, key: &str) -> Option<&'a str> {
    node.props.get(key).and_then(ui_value_as_str)
}

fn prop_f32(node: &UiNode, key: &str) -> Option<f32> {
    node.props.get(key).and_then(ui_value_as_f32)
}

fn prop_bool(node: &UiNode, key: &str) -> Option<bool> {
    node.props.get(key).and_then(ui_value_as_bool)
}

fn prop_i64(node: &UiNode, key: &str) -> Option<i64> {
    node.props.get(key).and_then(ui_value_as_i64)
}

fn prop_vec3(node: &UiNode, key_prefix: &str) -> Option<[f32; 3]> {
    Some([
        prop_f32(node, &format!("{key_prefix}.x"))?,
        prop_f32(node, &format!("{key_prefix}.y"))?,
        prop_f32(node, &format!("{key_prefix}.z"))?,
    ])
}

fn resolve_viewport_camera_binding_from_node(
    node: &UiNode,
) -> Option<RealtimeViewportCameraBinding> {
    let camera = RealtimeViewportCameraBinding {
        position: prop_vec3(node, "camera.position"),
        target: prop_vec3(node, "camera.target"),
        fov_y_degrees: prop_f32(node, "camera.fov_y_degrees")
            .or_else(|| prop_f32(node, "camera.fov_y")),
        near_plane: prop_f32(node, "camera.near_plane").or_else(|| prop_f32(node, "camera.near")),
        far_plane: prop_f32(node, "camera.far_plane").or_else(|| prop_f32(node, "camera.far")),
    };
    (camera.position.is_some()
        || camera.target.is_some()
        || camera.fov_y_degrees.is_some()
        || camera.near_plane.is_some()
        || camera.far_plane.is_some())
    .then_some(camera)
}

fn resolve_viewport_presentation_binding_from_node(
    node: &UiNode,
) -> Option<RealtimeViewportPresentationBinding> {
    let particle_budget = prop_i64(node, "viewport.particle_budget")
        .or_else(|| prop_i64(node, "viewport_particle_budget"))
        .and_then(|value| u32::try_from(value).ok());
    let gizmo = resolve_viewport_gizmo_binding_from_node(node);
    let presentation = RealtimeViewportPresentationBinding {
        profile: prop_text(node, "viewport.profile")
            .or_else(|| prop_text(node, "viewport_profile"))
            .map(ToString::to_string),
        fog_density: prop_f32(node, "viewport.fog_density")
            .or_else(|| prop_f32(node, "viewport_fog_density")),
        particle_budget,
        gizmo,
    };
    (presentation.profile.is_some()
        || presentation.fog_density.is_some()
        || presentation.particle_budget.is_some()
        || presentation.gizmo.is_some())
    .then_some(presentation)
}

fn resolve_viewport_gizmo_binding_from_node(node: &UiNode) -> Option<RealtimeViewportGizmoBinding> {
    let gizmo = RealtimeViewportGizmoBinding {
        profile_id: prop_text(node, "gizmo.profile")
            .or_else(|| prop_text(node, "gizmo_profile"))
            .map(ToString::to_string),
        visible: prop_bool(node, "gizmo.visible").or_else(|| prop_bool(node, "gizmo_visible")),
        default_mode: prop_text(node, "gizmo.default_mode")
            .or_else(|| prop_text(node, "gizmo_default_mode"))
            .map(ToString::to_string),
        default_space: prop_text(node, "gizmo.default_space")
            .or_else(|| prop_text(node, "gizmo_default_space"))
            .map(ToString::to_string),
        drag_trigger: prop_text(node, "gizmo.drag_trigger")
            .or_else(|| prop_text(node, "gizmo_drag_trigger"))
            .map(ToString::to_string),
        selection_required: prop_bool(node, "gizmo.selection_required")
            .or_else(|| prop_bool(node, "gizmo_selection_required")),
        translate_hotkey: prop_text(node, "gizmo.hotkey.translate")
            .or_else(|| prop_text(node, "gizmo_hotkey_translate"))
            .map(ToString::to_string),
        rotate_hotkey: prop_text(node, "gizmo.hotkey.rotate")
            .or_else(|| prop_text(node, "gizmo_hotkey_rotate"))
            .map(ToString::to_string),
        scale_hotkey: prop_text(node, "gizmo.hotkey.scale")
            .or_else(|| prop_text(node, "gizmo_hotkey_scale"))
            .map(ToString::to_string),
        cycle_space_hotkey: prop_text(node, "gizmo.hotkey.cycle_space")
            .or_else(|| prop_text(node, "gizmo_hotkey_cycle_space"))
            .map(ToString::to_string),
        toggle_snap_hotkey: prop_text(node, "gizmo.hotkey.toggle_snap")
            .or_else(|| prop_text(node, "gizmo_hotkey_toggle_snap"))
            .map(ToString::to_string),
        translate_snap_units: prop_f32(node, "gizmo.snap.translate")
            .or_else(|| prop_f32(node, "gizmo_snap_translate")),
        rotate_snap_degrees: prop_f32(node, "gizmo.snap.rotate_degrees")
            .or_else(|| prop_f32(node, "gizmo_snap_rotate_degrees")),
        scale_snap_percent: prop_f32(node, "gizmo.snap.scale_percent")
            .or_else(|| prop_f32(node, "gizmo_snap_scale_percent")),
        snap_default_enabled: prop_bool(node, "gizmo.snap.default_enabled")
            .or_else(|| prop_bool(node, "gizmo_snap_default_enabled")),
    };
    (gizmo.profile_id.is_some()
        || gizmo.visible.is_some()
        || gizmo.default_mode.is_some()
        || gizmo.default_space.is_some()
        || gizmo.drag_trigger.is_some()
        || gizmo.selection_required.is_some()
        || gizmo.translate_hotkey.is_some()
        || gizmo.rotate_hotkey.is_some()
        || gizmo.scale_hotkey.is_some()
        || gizmo.cycle_space_hotkey.is_some()
        || gizmo.toggle_snap_hotkey.is_some()
        || gizmo.translate_snap_units.is_some()
        || gizmo.rotate_snap_degrees.is_some()
        || gizmo.scale_snap_percent.is_some()
        || gizmo.snap_default_enabled.is_some())
    .then_some(gizmo)
}

fn vec3_from_triplet(value: [f32; 3]) -> Vec3 {
    Vec3::new(value[0], value[1], value[2])
}

fn resolve_viewport_reference_pose(
    scene: &SceneDescription,
    camera_binding: Option<&RealtimeViewportCameraBinding>,
) -> CameraPose {
    let mut pose = scene.camera.pose_at(0.0);
    let Some(camera_binding) = camera_binding else {
        return pose;
    };

    if let Some(position) = camera_binding.position {
        pose.position = vec3_from_triplet(position);
    }
    if let Some(target) = camera_binding.target {
        pose.target = vec3_from_triplet(target);
    }
    if let Some(fov_y_degrees) = camera_binding.fov_y_degrees {
        pose.fov_y_degrees = fov_y_degrees.clamp(1.0, 175.0);
    }
    if let Some(near_plane) = camera_binding.near_plane {
        pose.near_plane = near_plane.max(0.001);
    }
    if let Some(far_plane) = camera_binding.far_plane {
        pose.far_plane = far_plane.max(pose.near_plane + 0.1);
    }
    if pose.far_plane <= pose.near_plane {
        pose.far_plane = pose.near_plane + 0.1;
    }
    pose
}

fn ui_value_as_str(value: &UiValue) -> Option<&str> {
    match value {
        UiValue::String(value) => Some(value.as_str()),
        _ => None,
    }
}

pub fn summarize_patches(patches: &[UiPatch]) -> String {
    patches
        .iter()
        .map(|patch| format!("{patch:?}"))
        .collect::<Vec<_>>()
        .join("\n")
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::{
        DerivedShaderArtifact, ShaderArtifactBundle, ShaderArtifactFormat, ShaderDebugBundle,
        ShaderEntryPoint, ShaderReflectionSummary, ShaderResourceLayout,
        SHADER_ARTIFACT_SCHEMA_VERSION,
    };
    use kain_ui::{
        ui_runtime_systems_from_tree, UiNativeProjectionKind, UiNodeId, UiReloadIdentityAlias,
        UiSurface, UiSurfaceCompositionMode, UiSurfaceKind, UiSurfaceRendererPreference,
        UiSurfaceShaderBinding, UiTreeBuilder,
    };
    use std::fs;
    use std::path::Path;

    #[test]
    fn native_runtime_defaults_to_wgpu_renderer() {
        let settings = KainUiNativeRuntimeSettings::default();
        assert_eq!(settings.renderer, NativeRendererPreference::Wgpu);
        assert_eq!(settings.eframe_renderer(), eframe::Renderer::Wgpu);
    }

    #[test]
    fn runtime_bundle_json_round_trip_preserves_compiled_ui_output() {
        let config = KainUiNativeAppConfig::default();
        let output = build_output(&config).expect("demo source should compile");
        let bundle = runtime_bundle_from_output(&config, output.clone());

        let json = runtime_bundle_to_json(&bundle).expect("bundle should serialize");
        let decoded = runtime_bundle_from_json(&json).expect("bundle should deserialize");

        assert_eq!(
            decoded.schema_version,
            KAIN_UI_NATIVE_RUNTIME_BUNDLE_SCHEMA_VERSION
        );
        assert_eq!(decoded.metadata.root_component, "App");
        assert_eq!(decoded.output, output);
    }

    #[test]
    fn runtime_bundle_prefers_canonical_output_tree_in_shared_fixture() {
        let fixture = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../../runtime/conformance/ui_runtime/fixtures/ui_runtime_parity_bundle.json");
        let json = fs::read_to_string(&fixture).expect("shared UI bundle fixture should exist");
        let bundle =
            runtime_bundle_from_json(&json).expect("shared UI bundle fixture should deserialize");

        assert_eq!(
            bundle.schema_version,
            KAIN_UI_NATIVE_RUNTIME_BUNDLE_SCHEMA_VERSION
        );
        assert_eq!(bundle.metadata.window_title, "Kain UI Parity Fixture");
        assert_eq!(bundle.output.tree.root, Some(UiNodeId(1)));
        assert_eq!(bundle.output.tree.nodes.len(), 3);
        assert_eq!(
            bundle
                .output
                .tree
                .nodes
                .get(&UiNodeId(1))
                .map(|node| &node.kind),
            Some(&UiWidgetKind::Panel)
        );
        assert_eq!(
            bundle
                .output
                .tree
                .nodes
                .get(&UiNodeId(2))
                .map(|node| &node.kind),
            Some(UiWidgetKind::Element("input".to_string())).as_ref()
        );
        assert_eq!(
            bundle
                .output
                .tree
                .nodes
                .get(&UiNodeId(3))
                .map(|node| &node.kind),
            Some(&UiWidgetKind::Viewport3D)
        );
        assert_eq!(bundle.native_projection.root_id, bundle.output.tree.root.map(|id| id.0));
        assert!(bundle
            .native_projection
            .nodes
            .iter()
            .any(|node| matches!(node.kind, UiNativeProjectionKind::Viewport3D)));
        assert!(bundle
            .native_projection
            .nodes
            .iter()
            .any(|node| matches!(node.kind, UiNativeProjectionKind::Panel)));
        assert!(matches!(
            bundle
                .output
                .tree
                .nodes
                .get(&UiNodeId(1))
                .map(|node| &node.kind),
            Some(UiWidgetKind::Panel)
        ));
        assert!(matches!(
            bundle.output.tree.nodes.get(&UiNodeId(2)).map(|node| &node.kind),
            Some(UiWidgetKind::Element(value)) if value == "input"
        ));
        assert!(matches!(
            bundle
                .output
                .tree
                .nodes
                .get(&UiNodeId(3))
                .map(|node| &node.kind),
            Some(UiWidgetKind::Viewport3D)
        ));
    }

    #[test]
    fn app_theme_global_widget_maps_flow_into_child_widgets() {
        let child_id = UiNodeId(2);
        let output = themed_test_output(None);
        let app_theme = resolve_app_theme(&output);
        let child = output
            .tree
            .node(child_id)
            .expect("child panel should exist");
        let widget_theme = resolve_widget_theme(child, &output.systems.theme_registry, &app_theme);

        assert_eq!(app_theme.density, NativeDensity::Compact);
        assert_eq!(widget_theme.mode, NativeSurfaceMode::Glass);
        assert_eq!(widget_theme.fill, Color32::from_rgb(0x22, 0x33, 0x44));
    }

    #[test]
    fn node_local_style_overrides_global_widget_theme() {
        let child_id = UiNodeId(2);
        let mut child_overrides = BTreeMap::new();
        child_overrides.insert(
            "surface.mode".to_string(),
            UiValue::String("accent".to_string()),
        );
        child_overrides.insert(
            "surface.fill".to_string(),
            UiValue::String("#aa5522".to_string()),
        );
        let output = themed_test_output(Some(child_overrides));
        let app_theme = resolve_app_theme(&output);
        let child = output
            .tree
            .node(child_id)
            .expect("child panel should exist");
        let widget_theme = resolve_widget_theme(child, &output.systems.theme_registry, &app_theme);

        assert_eq!(widget_theme.mode, NativeSurfaceMode::Accent);
        assert_eq!(widget_theme.fill, Color32::from_rgb(0xaa, 0x55, 0x22));
    }

    #[test]
    fn widget_classes_and_states_shift_theme_behavior() {
        let output = themed_test_output(None);
        let app_theme = resolve_app_theme(&output);
        let mut node = UiNode::new(UiNodeId(99), UiWidgetKind::Inspector);
        node.style.classes = vec!["compact".to_string(), "hero".to_string()];
        node.style.states = vec![UiStyleState::Focused, UiStyleState::Selected];

        let widget_theme = resolve_widget_theme(&node, &output.systems.theme_registry, &app_theme);

        assert_eq!(widget_theme.mode, NativeSurfaceMode::Accent);
        assert_eq!(widget_theme.stroke, app_theme.palette.highlight);
        assert!(widget_theme.padding < app_theme.metrics.tight_padding);
    }

    #[test]
    fn text_roles_resolve_from_variant_and_class() {
        let mut hero = UiNode::new(UiNodeId(7), UiWidgetKind::Text);
        hero.style.variant = Some("hero".to_string());
        assert_eq!(resolve_text_role(&hero), NativeTextRole::Hero);

        let mut code = UiNode::new(UiNodeId(8), UiWidgetKind::Text);
        code.style.classes.push("code".to_string());
        assert_eq!(resolve_text_role(&code), NativeTextRole::Code);
    }

    #[test]
    fn app_theme_can_hide_runtime_topbar() {
        let mut output = themed_test_output(None);
        let root = output
            .tree
            .root
            .and_then(|root_id| output.tree.nodes.get_mut(&root_id))
            .expect("root node should exist");
        root.style.values.insert(
            "theme.chrome.topbar.visible".to_string(),
            UiValue::Bool(false),
        );

        let app_theme = resolve_app_theme(&output);

        assert!(!show_runtime_topbar(&app_theme, true));
    }

    #[test]
    fn node_presentation_reflects_mount_animation_progress() {
        let output = build_output(&KainUiNativeAppConfig::default())
            .expect("demo source should compile for animation coverage");
        let mut systems = output.systems.clone();
        let first_tick = ui_step_animation_runtime(&mut systems, 30);
        let animated_frame = first_tick
            .iter()
            .find(|frame| frame.property == "surface.opacity" && !frame.completed)
            .expect("panel surfaces should emit in-flight mount animation tracks");

        let mut animated_output = output.clone();
        animated_output.systems = systems;
        let child = animated_output
            .tree
            .node(animated_frame.target)
            .expect("animated surface target should exist");
        let presentation = resolve_node_presentation(&animated_output, child);

        assert!(presentation.opacity > 0.22 && presentation.opacity < 1.0);
        assert!(presentation.translate_y > 0.0);
    }

    #[test]
    fn shader_surface_state_map_transfers_across_hot_reload_aliases() {
        let previous = surface_test_output(UiNodeId(1), UiNodeId(2), "hero.surface");
        let mut next = surface_test_output(UiNodeId(10), UiNodeId(11), "hero.surface.next");
        next.systems
            .hot_reload
            .identity_aliases
            .push(UiReloadIdentityAlias {
                from: "hero.surface".to_string(),
                to: "hero.surface.next".to_string(),
            });

        let mut previous_states = BTreeMap::new();
        previous_states.insert(
            UiNodeId(2),
            ShaderSurfaceState {
                presented_state: None,
                last_signature: Some("hero-surface-signature".to_string()),
                last_shader_ref: Some("ui.hero_surface".to_string()),
                last_warning: None,
            },
        );

        let transferred = transfer_surface_state_map(&previous, &next, previous_states);
        let transferred_state = transferred
            .get(&UiNodeId(11))
            .expect("shader surface state should move to aliased node identity");

        assert_eq!(
            transferred_state.last_signature.as_deref(),
            Some("hero-surface-signature")
        );
        assert_eq!(
            transferred_state.last_shader_ref.as_deref(),
            Some("ui.hero_surface")
        );
        assert!(!transferred.contains_key(&UiNodeId(2)));
    }

    #[test]
    fn resolve_shader_surface_uses_runtime_catalog_and_wgsl_output() {
        let surface = UiSurface {
            id: "surface.hero".to_string(),
            kind: UiSurfaceKind::Canvas,
            node: UiNodeId(7),
            title: Some("Hero Surface".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Shader,
            composition_mode: UiSurfaceCompositionMode::ShaderCanvas,
            gpu_backing_required: true,
            shader: Some(UiSurfaceShaderBinding {
                shader_ref: "ui.hero_surface".to_string(),
                entry_point: None,
                stage: None,
                derived_format: Some("wgsl".to_string()),
            }),
        };
        let mut realtime_catalog = RealtimeBundleCatalog::default();
        realtime_catalog.shader_refs_by_key.insert(
            "ui.hero_surface".to_string(),
            RealtimeShaderBundleRef {
                key: "ui.hero_surface".to_string(),
                shader: "hero_surface_fragment".to_string(),
                module_name: "ui_hero_surface".to_string(),
                stage: "fragment".to_string(),
                entry_point: "hero_main".to_string(),
                source: "inline".to_string(),
                execution_domain: None,
                workgroup_size: None,
                dispatch_size: None,
                resource_bindings: Vec::new(),
                tensor_bindings: Vec::new(),
                stream_bindings: Vec::new(),
                neural_nodes: Vec::new(),
            },
        );
        realtime_catalog.shader_canvases_by_surface.insert(
            "surface.hero".to_string(),
            RealtimeShaderCanvasBinding {
                surface_id: "surface.hero".to_string(),
                shader_ref: "ui.hero_surface".to_string(),
                shader_bundle_ref_key: Some("ui.hero_surface".to_string()),
                shader_name: Some("hero_surface_fragment".to_string()),
                module_name: Some("ui_hero_surface".to_string()),
                stage: "fragment".to_string(),
                entry_point: Some("hero_main".to_string()),
                derived_format: Some("wgsl".to_string()),
                renderer_preference: "shader".to_string(),
                composition_mode: "shader-canvas".to_string(),
                font_atlases: vec![
                    RealtimeShaderCanvasFontAtlas {
                        key: "surface.hero.atlas.default".to_string(),
                        family: "kain.builtin.bitmap_5x7".to_string(),
                        asset_key: None,
                        encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                        distance_range_px: 0,
                        glyphs: " HEROFASTLANE".to_string(),
                        cell_size_px: [8, 8],
                        texture_size_px: [96, 8],
                        columns: 12,
                        rows: 1,
                    },
                    RealtimeShaderCanvasFontAtlas {
                        key: "surface.hero.atlas.metrics".to_string(),
                        family: "kain.builtin.bitmap_5x7".to_string(),
                        asset_key: None,
                        encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                        distance_range_px: 0,
                        glyphs: "0123456789".to_string(),
                        cell_size_px: [8, 8],
                        texture_size_px: [80, 8],
                        columns: 10,
                        rows: 1,
                    },
                ],
                text_runs: vec![
                    RealtimeShaderCanvasTextRun {
                        id: "surface.hero.title".to_string(),
                        text: "Hero Surface".to_string(),
                        role: "title".to_string(),
                        atlas_key: "surface.hero.atlas.default".to_string(),
                        origin_px: [16, 16],
                        color_token: "theme.text.default".to_string(),
                    },
                    RealtimeShaderCanvasTextRun {
                        id: "surface.hero.metric".to_string(),
                        text: "120".to_string(),
                        role: "metric".to_string(),
                        atlas_key: "surface.hero.atlas.metrics".to_string(),
                        origin_px: [16, 32],
                        color_token: "theme.accent".to_string(),
                    },
                ],
                resource_bindings: vec![
                    RealtimeShaderCanvasResourceBinding {
                        binding_name: "surface_uniforms".to_string(),
                        resource_kind: "uniform-buffer".to_string(),
                        source_kind: "runtime.surface.uniforms".to_string(),
                        atlas_key: None,
                    },
                    RealtimeShaderCanvasResourceBinding {
                        binding_name: "font_atlas".to_string(),
                        resource_kind: "texture-2d".to_string(),
                        source_kind: "runtime.surface.font-atlas".to_string(),
                        atlas_key: Some("surface.hero.atlas.default".to_string()),
                    },
                    RealtimeShaderCanvasResourceBinding {
                        binding_name: "metrics_atlas".to_string(),
                        resource_kind: "texture-2d".to_string(),
                        source_kind: "runtime.surface.font-atlas".to_string(),
                        atlas_key: Some("surface.hero.atlas.metrics".to_string()),
                    },
                ],
            },
        );
        let shader_bundle = test_shader_artifact_bundle();

        let resolved = resolve_ui_shader_surface_from_catalog(
            &surface,
            &realtime_catalog,
            Some(&shader_bundle),
        )
        .expect("shader surface should resolve from runtime metadata");

        assert_eq!(resolved.shader_ref, "ui.hero_surface");
        assert_eq!(resolved.shader_name, "hero_surface_fragment");
        assert_eq!(resolved.module_name, "ui_hero_surface");
        assert_eq!(resolved.fragment_entry_point, "hero_main");
        assert_eq!(resolved.derived_format, "wgsl");
        assert_eq!(resolved.canonical_native_payload, "wgsl");
        assert_eq!(
            resolved.shader_bundle_ref_key.as_deref(),
            Some("ui.hero_surface")
        );
        assert_eq!(resolved.resource_layouts.len(), 1);
        assert_eq!(resolved.font_atlases.len(), 2);
        assert_eq!(resolved.text_runs.len(), 2);
        assert_eq!(resolved.resource_bindings.len(), 3);
        assert!(resolved.wgsl_source.contains("hero_main"));
        assert!(resolved.warning.is_none());
    }

    #[test]
    fn shader_surface_texture_payload_packs_multiple_font_atlases() {
        let atlases = vec![
            RealtimeShaderCanvasFontAtlas {
                key: "atlas.primary".to_string(),
                family: "kain.builtin.bitmap_5x7".to_string(),
                asset_key: None,
                encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                distance_range_px: 0,
                glyphs: "ABC".to_string(),
                cell_size_px: [8, 8],
                texture_size_px: [24, 8],
                columns: 3,
                rows: 1,
            },
            RealtimeShaderCanvasFontAtlas {
                key: "atlas.metrics".to_string(),
                family: "kain.builtin.bitmap_5x7".to_string(),
                asset_key: None,
                encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                distance_range_px: 0,
                glyphs: "123456".to_string(),
                cell_size_px: [8, 8],
                texture_size_px: [48, 8],
                columns: 6,
                rows: 1,
            },
        ];
        let descriptor = ResolvedUiShaderSurface {
            shader_ref: "ui.hero_surface".to_string(),
            shader_name: "hero_surface_fragment".to_string(),
            module_name: "ui_hero_surface".to_string(),
            fragment_entry_point: "hero_main".to_string(),
            stage: "fragment".to_string(),
            derived_format: "wgsl".to_string(),
            canonical_native_payload: "wgsl".to_string(),
            shader_bundle_ref_key: Some("ui.hero_surface".to_string()),
            wgsl_source:
                "@fragment fn hero_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0); }"
                    .to_string(),
            resource_layouts: Vec::new(),
            font_atlases: atlases,
            font_asset_sources_by_key: BTreeMap::new(),
            text_runs: Vec::new(),
            resource_bindings: Vec::new(),
            warning: None,
        };

        let payload = build_ui_shader_surface_texture_payload(&descriptor);

        assert_eq!(payload.size_px, [48, 16]);
        assert_eq!(
            payload
                .atlases
                .iter()
                .map(|atlas| atlas.origin_px)
                .collect::<Vec<_>>(),
            vec![[0, 0], [0, 8]]
        );
        assert_eq!(payload.rgba8.len(), (48 * 16 * 4) as usize);
    }

    #[test]
    fn shader_surface_texture_payload_cache_reuses_packed_texture() {
        let descriptor = ResolvedUiShaderSurface {
            shader_ref: "ui.hero_surface".to_string(),
            shader_name: "hero_surface_fragment".to_string(),
            module_name: "ui_hero_surface".to_string(),
            fragment_entry_point: "hero_main".to_string(),
            stage: "fragment".to_string(),
            derived_format: "wgsl".to_string(),
            canonical_native_payload: "wgsl".to_string(),
            shader_bundle_ref_key: Some("ui.hero_surface".to_string()),
            wgsl_source:
                "@fragment fn hero_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0); }"
                    .to_string(),
            resource_layouts: Vec::new(),
            font_atlases: vec![
                RealtimeShaderCanvasFontAtlas {
                    key: "atlas.primary".to_string(),
                    family: "kain.builtin.bitmap_5x7".to_string(),
                    asset_key: None,
                    encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                    distance_range_px: 0,
                    glyphs: "ABC".to_string(),
                    cell_size_px: [8, 8],
                    texture_size_px: [24, 8],
                    columns: 3,
                    rows: 1,
                },
                RealtimeShaderCanvasFontAtlas {
                    key: "atlas.metrics".to_string(),
                    family: "kain.builtin.bitmap_5x7".to_string(),
                    asset_key: None,
                    encoding: SHADER_CANVAS_FONT_ATLAS_ENCODING_BITMAP_ALPHA.to_string(),
                    distance_range_px: 0,
                    glyphs: "123456".to_string(),
                    cell_size_px: [8, 8],
                    texture_size_px: [48, 8],
                    columns: 6,
                    rows: 1,
                },
            ],
            font_asset_sources_by_key: BTreeMap::new(),
            text_runs: Vec::new(),
            resource_bindings: Vec::new(),
            warning: None,
        };
        let mut cache = BTreeMap::new();

        let first = resolve_ui_shader_surface_texture_payload(&mut cache, &descriptor);
        let second = resolve_ui_shader_surface_texture_payload(&mut cache, &descriptor);

        assert_eq!(cache.len(), 1);
        assert_eq!(first.size_px, second.size_px);
        assert_eq!(first.atlases.len(), second.atlases.len());
        assert_eq!(
            first
                .atlases
                .iter()
                .map(|atlas| atlas.origin_px)
                .collect::<Vec<_>>(),
            second
                .atlases
                .iter()
                .map(|atlas| atlas.origin_px)
                .collect::<Vec<_>>()
        );
        assert!(Arc::ptr_eq(&first.rgba8, &second.rgba8));
    }

    #[test]
    fn viewport_translate_drag_moves_selected_instance_in_camera_plane() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = SceneTransform::identity().with_translation(Vec3::new(1.0, 2.0, 3.0));

        let updated = manipulated_transform_from_drag(
            &camera,
            RenderResolution::new(1280, 720),
            ManipulatorMode::Translate,
            ManipulatorSpace::World,
            &origin,
            Vec2::new(120.0, -48.0),
            false,
            viewport_gizmo_snap_settings(&default_viewport_gizmo_binding()),
        );

        assert!(updated.translation.x > origin.translation.x);
        assert!(updated.translation.y > origin.translation.y);
    }

    #[test]
    fn viewport_scale_drag_remains_positive() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = SceneTransform::identity().with_scale(Vec3::new(2.0, 3.0, 4.0));

        let updated = manipulated_transform_from_drag(
            &camera,
            RenderResolution::new(1280, 720),
            ManipulatorMode::Scale,
            ManipulatorSpace::World,
            &origin,
            Vec2::new(-900.0, 900.0),
            false,
            viewport_gizmo_snap_settings(&default_viewport_gizmo_binding()),
        );

        assert!(updated.scale.x > 0.0);
        assert!(updated.scale.y > 0.0);
        assert!(updated.scale.z > 0.0);
    }

    #[test]
    fn viewport_translate_drag_snaps_when_enabled() {
        let camera = CameraPose {
            position: Vec3::new(0.0, 4.0, 10.0),
            target: Vec3::ZERO,
            up: Vec3::UP,
            fov_y_degrees: 60.0,
            near_plane: 0.1,
            far_plane: 100.0,
        };
        let origin = SceneTransform::identity().with_translation(Vec3::new(0.3, 0.7, 1.1));

        let updated = manipulated_transform_from_drag(
            &camera,
            RenderResolution::new(1280, 720),
            ManipulatorMode::Translate,
            ManipulatorSpace::World,
            &origin,
            Vec2::new(67.0, -19.0),
            true,
            ViewportGizmoSnapSettings {
                translation_step: Some(0.5),
                rotation_step_radians: None,
                scale_step: None,
            },
        );

        assert!((updated.translation.x * 2.0).fract().abs() <= 1.0e-4);
        assert!((updated.translation.y * 2.0).fract().abs() <= 1.0e-4);
    }

    fn themed_test_output(child_values: Option<BTreeMap<String, UiValue>>) -> UiBuildOutput {
        let root_id = UiNodeId(1);
        let child_id = UiNodeId(2);

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.children.push(child_id);
        root.style.values.insert(
            "theme.density".to_string(),
            UiValue::String("compact".to_string()),
        );
        root.style.values.insert(
            "widget.panel.surface.mode".to_string(),
            UiValue::String("glass".to_string()),
        );
        root.style.values.insert(
            "widget.panel.surface.fill".to_string(),
            UiValue::String("theme.surface.background".to_string()),
        );
        root.style.values.insert(
            "theme.surface.background".to_string(),
            UiValue::String("#223344".to_string()),
        );

        let mut child = UiNode::new(child_id, UiWidgetKind::Panel);
        if let Some(values) = child_values {
            child.style.values.extend(values);
        }

        let mut tree = UiTree::default();
        tree.root = Some(root_id);
        tree.nodes.insert(root_id, root);
        tree.nodes.insert(child_id, child);

        UiBuildOutput {
            tree: tree.clone(),
            patches: Vec::new(),
            systems: ui_runtime_systems_from_tree(&tree),
        }
    }

    fn surface_test_output(
        root_id: UiNodeId,
        surface_node_id: UiNodeId,
        identity: &str,
    ) -> UiBuildOutput {
        let mut builder = UiTreeBuilder::new();
        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some(format!("root-{}", root_id.0));
        builder.add_node(root);

        let mut surface = UiNode::new(surface_node_id, UiWidgetKind::Element("canvas".to_string()));
        surface.identity_key = Some(identity.to_string());
        surface.layout.persistent_layout_id = Some(identity.to_string());
        builder.add_node(surface);
        builder.replace_children(root_id, vec![surface_node_id]);
        builder.set_root(root_id);
        builder.finish()
    }

    fn test_shader_artifact_bundle() -> ShaderArtifactBundle {
        ShaderArtifactBundle {
            schema_version: SHADER_ARTIFACT_SCHEMA_VERSION,
            canonical_native_payload: ShaderArtifactFormat::Wgsl,
            spirv_modules: Vec::new(),
            reflection: ShaderReflectionSummary {
                emitted: true,
                shaders: Vec::new(),
                notes: Vec::new(),
            },
            resource_layouts: vec![ShaderResourceLayout {
                shader: "hero_surface_fragment".to_string(),
                name: "UiRuntime".to_string(),
                binding: 0,
                descriptor_set: 0,
                ty: "uniform_buffer".to_string(),
                kind: "uniform_buffer".to_string(),
            }],
            entry_points: vec![ShaderEntryPoint {
                shader: "hero_surface_fragment".to_string(),
                module_name: "ui_hero_surface".to_string(),
                entry_point: "hero_main".to_string(),
                stage: "fragment".to_string(),
            }],
            stage_metadata: Vec::new(),
            specialization_constants: Vec::new(),
            debug: ShaderDebugBundle {
                source_map: Vec::new(),
                notes: Vec::new(),
            },
            derived_outputs: vec![DerivedShaderArtifact {
                format: ShaderArtifactFormat::Wgsl,
                module_name: "ui_hero_surface".to_string(),
                contents: r#"
struct UiRuntime {
    resolution: vec2<f32>,
    pointer: vec2<f32>,
    time_seconds: f32,
    opacity: f32,
    frame_index: f32,
    aspect_ratio: f32,
    _pad0: vec4<f32>,
    _pad1: vec4<f32>,
};

@group(0) @binding(0)
var<uniform> ui_runtime: UiRuntime;

@fragment
fn hero_main(@builtin(position) position: vec4<f32>) -> @location(0) vec4<f32> {
    let pulse = 0.5 + 0.5 * sin(ui_runtime.time_seconds + position.x * 0.01);
    return vec4<f32>(pulse, 0.25, 1.0 - pulse, ui_runtime.opacity);
}
"#
                .trim()
                .to_string(),
            }],
        }
    }
}
