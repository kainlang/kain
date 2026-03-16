use std::{
    collections::BTreeMap,
    fs::OpenOptions,
    io::Write,
    path::PathBuf,
    time::{Duration, Instant},
};

use eframe::egui::{self, Align, Color32, Frame, Layout, RichText, Stroke, Vec2};
use kain_3d::{
    CameraPose, CpuPickingService, ManipulatorMode, PickingHit, PickingQuery, PickingRay,
    PickingService, RenderBackend, RenderResolution, RenderStats, RenderViewSettings, SceneCatalog,
    SoftwareRenderer, Vec3, WgpuRenderer,
};
use kain_core::{build_ui_output_from_source, render_ui_output_debug};
use kain_ui::{
    ui_runtime_bundle_from_json, ui_runtime_bundle_from_output, ui_runtime_bundle_to_json,
    validate_ui_runtime_bundle, UiBuildOutput, UiLayoutKind, UiNode, UiPatch, UiRuntimeBundle,
    UiRuntimeMetadata, UiTree, UiValue, UiWidgetKind, UI_RUNTIME_BUNDLE_SCHEMA_VERSION,
};

pub const KAIN_UI_NATIVE_RUNTIME_BUNDLE_SCHEMA_VERSION: u32 = UI_RUNTIME_BUNDLE_SCHEMA_VERSION;
pub type KainUiNativeRuntimeMetadata = UiRuntimeMetadata;
pub type KainUiNativeRuntimeBundle = UiRuntimeBundle;

const DEFAULT_REPAINT_INTERVAL_MS: u64 = 33;
const DEFAULT_VIEWPORT_RENDER_INTERVAL_IDLE_MS: u64 = 180;
const DEFAULT_VIEWPORT_RENDER_INTERVAL_INTERACTIVE_MS: u64 = 66;
const DEFAULT_VIEWPORT_STARTUP_DELAY_MS: u64 = 350;
const DEFAULT_VIEWPORT_MAX_AXIS_PX: u64 = 640;

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
            renderer: NativeRendererPreference::Glow,
            show_runtime_inspector: true,
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
        eframe::Renderer::Glow
    }

    fn renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "software",
            NativeRendererPreference::Wgpu => "wgpu",
        }
    }

    fn effective_renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "software",
            NativeRendererPreference::Wgpu => "pending-app-init",
        }
    }
}

fn parse_renderer_preference(value: &str) -> NativeRendererPreference {
    match value.to_ascii_lowercase().as_str() {
        "wgpu" => NativeRendererPreference::Wgpu,
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
        Box::new(move |_cc| {
            trace_runtime("run_native: creation_context received");
            Ok(Box::new(KainUiNativeApp::new(
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
    controller: ViewportCameraController,
    last_render_at: Option<Instant>,
    last_stats: RenderStats,
    selected_instance_id: Option<String>,
    manipulator_mode: ManipulatorMode,
    last_pick: Option<PickingHit>,
}

struct ViewportCameraController {
    position: Vec3,
    yaw: f32,
    pitch: f32,
    move_speed: f32,
}

#[derive(Clone, Debug, Default)]
struct ViewportInputSnapshot {
    scroll_delta_y: f32,
    pointer_delta: Vec2,
    move_forward: bool,
    move_backward: bool,
    move_right: bool,
    move_left: bool,
    move_up: bool,
    move_down: bool,
    speed_boost: bool,
    recenter: bool,
    gizmo_translate: bool,
    gizmo_rotate: bool,
    gizmo_scale: bool,
}

impl ViewportCameraController {
    fn from_pose(pose: &CameraPose) -> Self {
        let forward = pose.forward();
        Self {
            position: pose.position,
            yaw: forward.z.atan2(forward.x),
            pitch: forward.y.clamp(-0.999, 0.999).asin(),
            move_speed: 7.5,
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
        self.planar_forward().cross(Vec3::UP).normalize()
    }

    fn recenter(&mut self, pose: &CameraPose) {
        *self = Self::from_pose(pose);
    }
}

struct KainUiNativeApp {
    config: KainUiNativeAppConfig,
    runtime_settings: KainUiNativeRuntimeSettings,
    output: UiBuildOutput,
    debug_tree: String,
    scene_catalog: SceneCatalog,
    renderer: Box<dyn RenderBackend>,
    active_renderer_label: String,
    viewport_surfaces: BTreeMap<kain_ui::UiNodeId, ViewportSurfaceState>,
    viewport_input: ViewportInputSnapshot,
    start_time: Instant,
    last_frame_instant: Instant,
    frame_dt_seconds: f32,
    boot_mode: AppBootMode,
}

impl KainUiNativeApp {
    fn new(
        config: KainUiNativeAppConfig,
        output: UiBuildOutput,
        boot_mode: AppBootMode,
        runtime_settings: KainUiNativeRuntimeSettings,
    ) -> Self {
        let (renderer, active_renderer_label) = select_viewport_renderer(runtime_settings);
        trace_runtime(format!(
            "app_new: title={} root={} boot_mode={} renderer={} effective_renderer={} inspector={} viewports={}",
            config.window_title,
            config.root_component,
            boot_mode.label(),
            runtime_settings.renderer_label(),
            active_renderer_label,
            runtime_settings.show_runtime_inspector,
            runtime_settings.enable_viewports,
        ));
        let debug_tree = render_ui_output_debug(&output);
        Self {
            config,
            runtime_settings,
            output,
            debug_tree,
            scene_catalog: SceneCatalog::default(),
            renderer,
            active_renderer_label,
            viewport_surfaces: BTreeMap::new(),
            viewport_input: ViewportInputSnapshot::default(),
            start_time: Instant::now(),
            last_frame_instant: Instant::now(),
            frame_dt_seconds: 1.0 / 60.0,
            boot_mode,
        }
    }
}

fn select_viewport_renderer(
    runtime_settings: KainUiNativeRuntimeSettings,
) -> (Box<dyn RenderBackend>, String) {
    match runtime_settings.renderer {
        NativeRendererPreference::Glow => (
            Box::new(SoftwareRenderer::default()),
            "software".to_string(),
        ),
        NativeRendererPreference::Wgpu => match WgpuRenderer::new() {
            Ok(renderer) => (Box::new(renderer), "wgpu".to_string()),
            Err(err) => {
                trace_runtime(format!(
                    "viewport_renderer: requested=wgpu fallback=software error={err}"
                ));
                (
                    Box::new(SoftwareRenderer::default()),
                    "software-fallback".to_string(),
                )
            }
        },
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

        apply_demo_visuals(ctx);
        ctx.request_repaint_after(Duration::from_millis(
            self.runtime_settings.repaint_interval_ms,
        ));
        self.viewport_input = snapshot_viewport_input(ctx);
        self.viewport_surfaces
            .retain(|id, _| self.output.tree.nodes.contains_key(id));

        trace_runtime("app_update: topbar");
        egui::TopBottomPanel::top("kain_ui_native_topbar")
            .resizable(false)
            .show(ctx, |ui| {
                Frame::new()
                    .fill(color_bg_top())
                    .inner_margin(egui::Margin::same(8))
                    .show(ui, |ui| {
                        ui.horizontal(|ui| {
                            ui.heading(
                                RichText::new(&self.config.window_title)
                                    .size(20.0)
                                    .color(Color32::from_rgb(241, 245, 250)),
                            );
                            ui.add_space(6.0);
                            ui.label(
                                RichText::new(format!(
                                    "{} nodes  |  {} patches",
                                    self.output.tree.nodes.len(),
                                    self.output.patches.len()
                                ))
                                .monospace()
                                .color(color_accent_soft()),
                            );
                            ui.add_space(6.0);
                            ui.label(
                                RichText::new(format!("boot: {}", self.boot_mode.label()))
                                    .monospace()
                                    .color(color_success()),
                            );
                            ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                                ui.label(
                                    RichText::new(format!("root: {}", self.config.root_component))
                                        .monospace()
                                        .color(color_highlight()),
                                );
                            });
                        });
                    });
                });
        

        if self.runtime_settings.show_runtime_inspector {
            trace_runtime("app_update: inspector");
            egui::SidePanel::right("kain_ui_native_inspector")
                .default_width(360.0)
                .show(ctx, |ui| {
                    ui.heading("Runtime Inspector");
                    ui.label("Retained semantic tree, emitted patch stream, and compiled viewport surfaces.");
                    ui.separator();
                    ui.label(
                        RichText::new(format!("boot source: {}", self.boot_mode.label()))
                            .monospace()
                            .color(Color32::from_rgb(173, 216, 255)),
                    );

                    ui.collapsing("Semantic Tree", |ui| {
                        egui::ScrollArea::vertical().show(ui, |ui| {
                            ui.code(&self.debug_tree);
                        });
                    });

                    ui.separator();
                    ui.heading("Patch Stream");
                    egui::ScrollArea::vertical()
                        .auto_shrink([false, false])
                        .show(ui, |ui| {
                            for patch in &self.output.patches {
                                ui.label(RichText::new(format!("{patch:?}")).monospace());
                            }
                        });

                    ui.separator();
                    ui.heading("Runtime Systems");
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
        }

        trace_runtime("app_update: central_panel");
        egui::CentralPanel::default()
            .frame(Frame::new().fill(color_bg_bottom()).inner_margin(12.0))
            .show(ctx, |ui| {
                if let Some(root_id) = self.output.tree.root {
                    let tree = self.output.tree.clone();
                    render_node(self, ui, ctx, &tree, root_id);
                }
            });
        trace_runtime("app_update: end");
    }
}

fn apply_demo_visuals(ctx: &egui::Context) {
    let mut visuals = egui::Visuals::dark();
    visuals.override_text_color = Some(Color32::from_rgb(239, 243, 248));
    visuals.panel_fill = color_bg_bottom();
    visuals.window_fill = color_bg_bottom();
    visuals.faint_bg_color = color_surface();
    visuals.extreme_bg_color = color_bg_top();
    visuals.widgets.noninteractive.bg_fill = color_surface();
    visuals.widgets.noninteractive.fg_stroke.color = color_muted_text();
    visuals.widgets.inactive.bg_fill = color_surface_alt();
    visuals.widgets.inactive.fg_stroke.color = color_accent_soft();
    visuals.widgets.hovered.bg_fill = color_surface_raised();
    visuals.widgets.hovered.fg_stroke.color = Color32::WHITE;
    visuals.widgets.active.bg_fill = Color32::from_rgb(28, 70, 94);
    visuals.widgets.active.fg_stroke.color = Color32::WHITE;
    visuals.widgets.open.bg_fill = color_surface_raised();
    visuals.selection.bg_fill = Color32::from_rgb(24, 93, 131);
    visuals.selection.stroke.color = color_accent_soft();
    visuals.hyperlink_color = color_highlight();
    visuals.window_stroke = Stroke::new(1.0, color_outline_soft());
    visuals.widgets.noninteractive.bg_stroke = Stroke::new(1.0, color_outline_soft());
    visuals.widgets.inactive.bg_stroke = Stroke::new(1.0, color_outline_soft());
    visuals.widgets.hovered.bg_stroke = Stroke::new(1.0, color_accent());
    visuals.widgets.active.bg_stroke = Stroke::new(1.0, color_accent());
    ctx.set_visuals(visuals);
}

fn render_node(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    id: kain_ui::UiNodeId,
) {
    let Some(node) = tree.node(id) else {
        ui.colored_label(Color32::LIGHT_RED, format!("missing node {}", id.0));
        return;
    };

    match &node.kind {
        UiWidgetKind::Text => {
            ui.label(prop_text(node, "text").unwrap_or_default());
        }
        UiWidgetKind::Panel => {
            let title = prop_text(node, "title").unwrap_or("Panel");
            Frame::new()
                .fill(color_surface_alt())
                .stroke(Stroke::new(1.0, color_outline_soft()))
                .corner_radius(14.0)
                .inner_margin(14.0)
                .show(ui, |ui| {
                    ui.label(
                        RichText::new(title)
                            .strong()
                            .size(18.0)
                            .color(Color32::from_rgb(243, 247, 251)),
                    );
                    ui.add_space(10.0);
                    render_children(app, ui, ctx, tree, node);
                });
        }
        UiWidgetKind::Inspector => {
            let title = prop_text(node, "title").unwrap_or("Inspector");
            Frame::new()
                .fill(color_surface())
                .stroke(Stroke::new(1.0, color_outline_soft()))
                .corner_radius(12.0)
                .inner_margin(12.0)
                .show(ui, |ui| {
                    egui::CollapsingHeader::new(
                        RichText::new(title).strong().color(color_highlight()),
                    )
                    .default_open(true)
                    .show(ui, |ui| {
                        render_children(app, ui, ctx, tree, node);
                    });
                });
        }
        UiWidgetKind::Tree => {
            let title = prop_text(node, "title").unwrap_or("Tree");
            Frame::new()
                .fill(color_surface())
                .stroke(Stroke::new(1.0, color_outline_soft()))
                .corner_radius(12.0)
                .inner_margin(12.0)
                .show(ui, |ui| {
                    egui::CollapsingHeader::new(RichText::new(title).color(color_accent_soft()))
                        .default_open(true)
                        .show(ui, |ui| {
                            render_children(app, ui, ctx, tree, node);
                        });
                });
        }
        UiWidgetKind::Graph => {
            render_surface_frame(
                ui,
                node,
                "Graph Canvas",
                Vec2::new(ui.available_width().max(280.0), 220.0),
                egui::Sense::hover(),
                |ui, rect, _response| {
                    let painter = ui.painter();
                    painter.rect_filled(rect.shrink(4.0), 12.0, color_bg_top());
                    for i in 0..3 {
                        let x = rect.left() + 40.0 + (i as f32 * 140.0);
                        let y = rect.top() + 50.0 + ((i % 2) as f32 * 70.0);
                        let node_rect =
                            egui::Rect::from_min_size(egui::pos2(x, y), Vec2::new(112.0, 50.0));
                        painter.rect_filled(node_rect, 10.0, Color32::from_rgb(21, 91, 123));
                        painter.rect_stroke(
                            node_rect,
                            10.0,
                            Stroke::new(1.0, color_accent_soft()),
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
                "Timeline",
                Vec2::new(ui.available_width().max(280.0), 120.0),
                egui::Sense::hover(),
                |ui, rect, _response| {
                    let painter = ui.painter();
                    painter.rect_filled(rect.shrink(4.0), 10.0, color_bg_top());
                    for tick in 0..12 {
                        let x = rect.left() + 20.0 + (tick as f32 * 48.0);
                        painter.line_segment(
                            [
                                egui::pos2(x, rect.top() + 16.0),
                                egui::pos2(x, rect.bottom() - 16.0),
                            ],
                            Stroke::new(1.0, color_outline_soft()),
                        );
                    }
                    let clip = egui::Rect::from_min_size(
                        egui::pos2(rect.left() + 64.0, rect.center().y - 14.0),
                        Vec2::new(220.0, 28.0),
                    );
                    painter.rect_filled(clip, 8.0, color_highlight());
                },
            );
        }
        UiWidgetKind::Viewport2D | UiWidgetKind::Viewport3D => {
            let label = match node.kind {
                UiWidgetKind::Viewport2D => "Viewport 2D",
                _ => "Viewport 3D",
            };
            render_viewport_surface(app, ui, ctx, node, label);
        }
        UiWidgetKind::ComponentRef(name) => {
            ui.label(
                RichText::new(format!("component {name}"))
                    .monospace()
                    .color(Color32::from_rgb(246, 211, 101)),
            );
            render_children(app, ui, ctx, tree, node);
        }
        UiWidgetKind::Element(tag) => {
            Frame::group(ui.style())
                .fill(Color32::from_rgb(23, 30, 39))
                .inner_margin(8.0)
                .show(ui, |ui| {
                    ui.small(
                        RichText::new(format!("<{tag}>"))
                            .monospace()
                            .color(Color32::from_rgb(173, 216, 255)),
                    );
                    render_children(app, ui, ctx, tree, node);
                });
        }
        UiWidgetKind::Table | UiWidgetKind::Overlay | UiWidgetKind::Slot => {
            render_children(app, ui, ctx, tree, node);
        }
    }
}

fn render_children(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    tree: &UiTree,
    node: &UiNode,
) {
    match node.layout.kind {
        UiLayoutKind::FlexRow => {
            ui.horizontal_top(|ui| {
                for child in &node.children {
                    render_node(app, ui, ctx, tree, *child);
                }
            });
        }
        _ => {
            for child in &node.children {
                render_node(app, ui, ctx, tree, *child);
            }
        }
    }
}

fn render_viewport_surface(
    app: &mut KainUiNativeApp,
    ui: &mut egui::Ui,
    ctx: &egui::Context,
    node: &UiNode,
    fallback_title: &str,
) {
    render_surface_frame(
        ui,
        node,
        fallback_title,
        Vec2::new(
            ui.available_width().max(420.0),
            ui.available_height().clamp(420.0, 780.0),
        ),
        egui::Sense::click_and_drag(),
        |ui, rect, response| {
            let painter = ui.painter();
            let inner_rect = rect.shrink(4.0);
            let scene_name = prop_text(node, "scene")
                .unwrap_or(app.scene_catalog.default_scene.as_str())
                .to_string();
            trace_runtime(format!(
                "viewport: enter node={} scene={} enabled={}",
                node.id.0, scene_name, app.runtime_settings.enable_viewports
            ));
            if !app.runtime_settings.enable_viewports {
                painter.rect_filled(inner_rect, 12.0, Color32::from_rgb(10, 14, 18));
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "viewport runtime disabled by KAIN_UI_NATIVE_ENABLE_VIEWPORTS=0",
                    egui::FontId::proportional(15.0),
                    Color32::from_rgb(246, 211, 101),
                );
                trace_runtime(format!("viewport: skipped node={}", node.id.0));
                return;
            }
            let elapsed_seconds = app.start_time.elapsed().as_secs_f32();
            let resolution =
                viewport_render_resolution(inner_rect.size(), app.runtime_settings.viewport_max_axis_px);
            let Some((reference_pose, viewport_summary)) = app
                .scene_catalog
                .scene(&scene_name)
                .map(|scene| (scene.camera.pose_at(0.0), scene.viewport_summary.clone()))
            else {
                painter.rect_filled(inner_rect, 12.0, Color32::from_rgb(10, 14, 18));
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
            let surface =
                app.viewport_surfaces
                    .entry(node.id)
                    .or_insert_with(|| ViewportSurfaceState {
                        texture: None,
                        scene_name: scene_name.clone(),
                        controller: ViewportCameraController::from_pose(&reference_pose),
                        last_render_at: None,
                        last_stats: RenderStats::default(),
                        selected_instance_id: None,
                        manipulator_mode: ManipulatorMode::Translate,
                        last_pick: None,
                    });
            trace_runtime(format!("viewport: state_ready node={}", node.id.0));
            if surface.scene_name != scene_name {
                surface.scene_name = scene_name.clone();
                surface.controller.recenter(&reference_pose);
                surface.texture = None;
                surface.last_render_at = None;
                surface.last_stats = RenderStats::default();
                surface.selected_instance_id = None;
                surface.last_pick = None;
            }
            sync_viewport_input(
                &app.viewport_input,
                response,
                &reference_pose,
                &mut surface.controller,
                app.frame_dt_seconds,
            );
            if response.hovered() || response.has_focus() {
                if app.viewport_input.gizmo_translate {
                    surface.manipulator_mode = ManipulatorMode::Translate;
                } else if app.viewport_input.gizmo_rotate {
                    surface.manipulator_mode = ManipulatorMode::Rotate;
                } else if app.viewport_input.gizmo_scale {
                    surface.manipulator_mode = ManipulatorMode::Scale;
                }
            }
            let active_camera = surface.controller.pose(&reference_pose);
            let render_view = RenderViewSettings {
                camera: Some(active_camera),
                selected_instance_id: surface.selected_instance_id.clone(),
                manipulator_mode: Some(surface.manipulator_mode),
            };
            if response.clicked() {
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
                            Ok(None) => CpuPickingService.pick_catalog_scene(
                                &app.scene_catalog,
                                &scene_name,
                                &query,
                            ),
                            Err(err) => {
                                trace_runtime(format!(
                                    "viewport: gpu_pick_failed node={} error={err}",
                                    node.id.0
                                ));
                                CpuPickingService.pick_catalog_scene(
                                    &app.scene_catalog,
                                    &scene_name,
                                    &query,
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
                && (surface.texture.is_none()
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
                        trace_runtime(format!("viewport: failed node={} error={err}", node.id.0));
                        painter.rect_filled(inner_rect, 14.0, color_bg_top());
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

            if let Some(texture) = surface.texture.as_ref() {
                painter.image(
                    texture.id(),
                    inner_rect,
                    egui::Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)),
                    Color32::WHITE,
                );
            } else {
                painter.rect_filled(inner_rect, 14.0, color_bg_top());
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "warming native viewport...",
                    egui::FontId::proportional(18.0),
                    color_accent_soft(),
                );
            }
            painter.rect_stroke(
                inner_rect,
                14.0,
                Stroke::new(1.0, color_outline_bright()),
                egui::StrokeKind::Inside,
            );

            let overlay_rect = egui::Rect::from_min_size(
                inner_rect.min + egui::vec2(12.0, 12.0),
                Vec2::new(336.0, 108.0),
            );
            painter.rect_filled(overlay_rect, 12.0, color_surface_overlay());
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 10.0),
                egui::Align2::LEFT_TOP,
                format!("scene: {scene_name}"),
                egui::FontId::monospace(13.0),
                color_highlight(),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 28.0),
                egui::Align2::LEFT_TOP,
                format!("renderer: {}", app.active_renderer_label),
                egui::FontId::monospace(11.0),
                color_success(),
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
                color_accent_soft(),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 61.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "selection: {}  |  gizmo: {:?}",
                    surface.selected_instance_id.as_deref().unwrap_or("none"),
                    surface.manipulator_mode
                ),
                egui::FontId::monospace(10.5),
                color_muted_text(),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 77.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "particles: {} submitted / {} blended",
                    surface.last_stats.particles_submitted, surface.last_stats.particles_shaded
                ),
                egui::FontId::monospace(11.0),
                color_success(),
            );
            let camera_position = surface.controller.position;
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 93.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "cam: [{:.1}, {:.1}, {:.1}]  speed: {:.1}",
                    camera_position.x,
                    camera_position.y,
                    camera_position.z,
                    surface.controller.move_speed
                ),
                egui::FontId::monospace(11.0),
                color_accent_soft(),
            );
            let controls_rect = egui::Rect::from_min_size(
                egui::pos2(inner_rect.left() + 12.0, inner_rect.bottom() - 38.0),
                Vec2::new(420.0, 26.0),
            );
            painter.rect_filled(controls_rect, 10.0, color_surface_overlay());
            painter.text(
                controls_rect.center(),
                egui::Align2::CENTER_CENTER,
                "roam WASD + QE  |  drag to look  |  wheel changes speed  |  T / R / Y gizmos",
                egui::FontId::monospace(10.5),
                if response.has_focus() || response.hovered() {
                    color_highlight()
                } else {
                    color_muted_text()
                },
            );
            let summary_rect = egui::Rect::from_min_size(
                egui::pos2(inner_rect.right() - 250.0, inner_rect.top() + 12.0),
                Vec2::new(238.0, 40.0),
            );
            painter.rect_filled(summary_rect, 10.0, color_surface_overlay());
            painter.text(
                summary_rect.min + egui::vec2(10.0, 8.0),
                egui::Align2::LEFT_TOP,
                viewport_summary.as_str(),
                egui::FontId::monospace(10.5),
                color_accent_soft(),
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
                    color_highlight(),
                );
            }
        },
    );
}

fn render_surface_frame(
    ui: &mut egui::Ui,
    node: &UiNode,
    fallback_title: &str,
    desired_size: Vec2,
    sense: egui::Sense,
    paint: impl FnOnce(&mut egui::Ui, egui::Rect, &egui::Response),
) {
    let title = prop_text(node, "title").unwrap_or(fallback_title);
    Frame::new()
        .fill(color_surface())
        .stroke(Stroke::new(1.0, color_outline_soft()))
        .corner_radius(16.0)
        .inner_margin(14.0)
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new(title)
                        .strong()
                        .size(16.5)
                        .color(Color32::from_rgb(241, 245, 250)),
                );
                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    ui.small(
                        RichText::new(format!("{:?}", node.kind))
                            .monospace()
                            .color(color_accent_soft()),
                    );
                });
            });
            ui.add_space(8.0);
            let (rect, response) = ui.allocate_exact_size(desired_size, sense);
            paint(ui, rect, &response);
        });
}

fn snapshot_viewport_input(ctx: &egui::Context) -> ViewportInputSnapshot {
    ctx.input(|input| ViewportInputSnapshot {
        scroll_delta_y: input.raw_scroll_delta.y,
        pointer_delta: input.pointer.delta(),
        move_forward: input.key_down(egui::Key::W),
        move_backward: input.key_down(egui::Key::S),
        move_right: input.key_down(egui::Key::D),
        move_left: input.key_down(egui::Key::A),
        move_up: input.key_down(egui::Key::Q),
        move_down: input.key_down(egui::Key::E),
        speed_boost: input.modifiers.shift,
        recenter: input.key_pressed(egui::Key::Space),
        gizmo_translate: input.key_pressed(egui::Key::T),
        gizmo_rotate: input.key_pressed(egui::Key::R),
        gizmo_scale: input.key_pressed(egui::Key::Y),
    })
}

fn sync_viewport_input(
    input: &ViewportInputSnapshot,
    response: &egui::Response,
    reference_pose: &CameraPose,
    controller: &mut ViewportCameraController,
    dt_seconds: f32,
) {
    if response.hovered() && input.scroll_delta_y.abs() > f32::EPSILON {
        let speed_scale = (1.0 + input.scroll_delta_y * 0.0015).clamp(0.5, 2.0);
        controller.move_speed = (controller.move_speed * speed_scale).clamp(1.0, 42.0);
    }

    if response.dragged_by(egui::PointerButton::Primary) {
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
}
