use std::{
    collections::BTreeMap,
    fs::OpenOptions,
    io::Write,
    path::PathBuf,
    time::{Duration, Instant},
};

use eframe::egui::{self, Align, Color32, Frame, Layout, RichText, Stroke, Vec2};
use kain_3d::{
    CameraPose, RenderResolution, RenderStats, RenderViewSettings, SceneCatalog, SoftwareRenderer,
    Vec3,
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
        }
    }
}

impl KainUiNativeRuntimeSettings {
    fn from_env() -> Self {
        let mut settings = Self::default();
        if let Some(renderer) = env_var_trimmed("KAIN_UI_NATIVE_RENDERER") {
            settings.renderer = match renderer.to_ascii_lowercase().as_str() {
                "wgpu" => NativeRendererPreference::Wgpu,
                _ => NativeRendererPreference::Glow,
            };
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
        settings
    }

    fn eframe_renderer(self) -> eframe::Renderer {
        eframe::Renderer::Glow
    }

    fn renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "glow",
            NativeRendererPreference::Wgpu => "wgpu-requested",
        }
    }

    fn effective_renderer_label(self) -> &'static str {
        match self.renderer {
            NativeRendererPreference::Glow => "glow",
            NativeRendererPreference::Wgpu => "glow-fallback",
        }
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

    fn right(&self) -> Vec3 {
        self.forward().cross(Vec3::UP).normalize()
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
    renderer: SoftwareRenderer,
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
        trace_runtime(format!(
            "app_new: title={} root={} boot_mode={} renderer={} effective_renderer={} inspector={} viewports={}",
            config.window_title,
            config.root_component,
            boot_mode.label(),
            runtime_settings.renderer_label(),
            runtime_settings.effective_renderer_label(),
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
            renderer: SoftwareRenderer::default(),
            viewport_surfaces: BTreeMap::new(),
            viewport_input: ViewportInputSnapshot::default(),
            start_time: Instant::now(),
            last_frame_instant: Instant::now(),
            frame_dt_seconds: 1.0 / 60.0,
            boot_mode,
        }
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
                ui.horizontal(|ui| {
                    ui.heading(&self.config.window_title);
                    ui.separator();
                    ui.label(
                        RichText::new(format!(
                            "{} nodes, {} patches",
                            self.output.tree.nodes.len(),
                            self.output.patches.len()
                        ))
                        .color(Color32::from_rgb(173, 216, 255)),
                    );
                    ui.separator();
                    ui.label(
                        RichText::new(format!("boot: {}", self.boot_mode.label()))
                            .monospace()
                            .color(Color32::from_rgb(139, 214, 123)),
                    );
                    ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                        ui.label(
                            RichText::new(format!("root: {}", self.config.root_component))
                                .monospace()
                                .color(Color32::from_rgb(246, 211, 101)),
                        );
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
        egui::CentralPanel::default().show(ctx, |ui| {
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
    visuals.override_text_color = Some(Color32::from_rgb(234, 236, 239));
    visuals.panel_fill = Color32::from_rgb(16, 22, 29);
    visuals.window_fill = Color32::from_rgb(16, 22, 29);
    visuals.widgets.noninteractive.bg_fill = Color32::from_rgb(25, 33, 42);
    visuals.widgets.inactive.bg_fill = Color32::from_rgb(32, 43, 54);
    visuals.widgets.active.bg_fill = Color32::from_rgb(61, 90, 128);
    visuals.widgets.hovered.bg_fill = Color32::from_rgb(74, 109, 153);
    visuals.selection.bg_fill = Color32::from_rgb(46, 88, 130);
    visuals.hyperlink_color = Color32::from_rgb(246, 211, 101);
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
            Frame::group(ui.style())
                .fill(Color32::from_rgb(28, 37, 48))
                .stroke(Stroke::new(1.0, Color32::from_rgb(70, 92, 118)))
                .corner_radius(8.0)
                .inner_margin(12.0)
                .show(ui, |ui| {
                    ui.label(RichText::new(title).strong().size(18.0));
                    ui.add_space(8.0);
                    render_children(app, ui, ctx, tree, node);
                });
        }
        UiWidgetKind::Inspector => {
            let title = prop_text(node, "title").unwrap_or("Inspector");
            egui::CollapsingHeader::new(RichText::new(title).strong())
                .default_open(true)
                .show(ui, |ui| {
                    render_children(app, ui, ctx, tree, node);
                });
        }
        UiWidgetKind::Tree => {
            let title = prop_text(node, "title").unwrap_or("Tree");
            egui::CollapsingHeader::new(
                RichText::new(title).color(Color32::from_rgb(184, 221, 255)),
            )
            .default_open(true)
            .show(ui, |ui| {
                render_children(app, ui, ctx, tree, node);
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
                    painter.rect_filled(rect.shrink(4.0), 10.0, Color32::from_rgb(20, 28, 37));
                    for i in 0..3 {
                        let x = rect.left() + 40.0 + (i as f32 * 140.0);
                        let y = rect.top() + 50.0 + ((i % 2) as f32 * 70.0);
                        let node_rect =
                            egui::Rect::from_min_size(egui::pos2(x, y), Vec2::new(112.0, 50.0));
                        painter.rect_filled(node_rect, 8.0, Color32::from_rgb(61, 90, 128));
                        painter.rect_stroke(
                            node_rect,
                            8.0,
                            Stroke::new(1.0, Color32::from_rgb(173, 216, 255)),
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
                    painter.rect_filled(rect.shrink(4.0), 8.0, Color32::from_rgb(22, 28, 35));
                    for tick in 0..12 {
                        let x = rect.left() + 20.0 + (tick as f32 * 48.0);
                        painter.line_segment(
                            [
                                egui::pos2(x, rect.top() + 16.0),
                                egui::pos2(x, rect.bottom() - 16.0),
                            ],
                            Stroke::new(1.0, Color32::from_rgb(54, 71, 90)),
                        );
                    }
                    let clip = egui::Rect::from_min_size(
                        egui::pos2(rect.left() + 64.0, rect.center().y - 14.0),
                        Vec2::new(220.0, 28.0),
                    );
                    painter.rect_filled(clip, 6.0, Color32::from_rgb(246, 211, 101));
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
        Vec2::new(ui.available_width().max(320.0), 320.0),
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
            let resolution = viewport_render_resolution(inner_rect.size());
            let Some((reference_pose, viewport_summary)) =
                app.scene_catalog
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
                    });
            trace_runtime(format!("viewport: state_ready node={}", node.id.0));
            if surface.scene_name != scene_name {
                surface.scene_name = scene_name.clone();
                surface.controller.recenter(&reference_pose);
                surface.texture = None;
                surface.last_render_at = None;
                surface.last_stats = RenderStats::default();
            }
            sync_viewport_input(
                &app.viewport_input,
                response,
                &reference_pose,
                &mut surface.controller,
                app.frame_dt_seconds,
            );
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
                let view = RenderViewSettings {
                    camera: Some(surface.controller.pose(&reference_pose)),
                };
                let render_start = Instant::now();

                match app.renderer.render_catalog_scene_with_view(
                    &app.scene_catalog,
                    &scene_name,
                    elapsed_seconds,
                    resolution,
                    &view,
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
                        painter.rect_filled(inner_rect, 12.0, Color32::from_rgb(10, 14, 18));
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
                painter.rect_filled(inner_rect, 12.0, Color32::from_rgb(10, 14, 18));
                painter.text(
                    inner_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "warming native viewport...",
                    egui::FontId::proportional(18.0),
                    Color32::from_rgb(129, 198, 255),
                );
            }
            painter.rect_stroke(
                inner_rect,
                12.0,
                Stroke::new(1.0, Color32::from_rgb(76, 214, 255)),
                egui::StrokeKind::Inside,
            );

            let overlay_rect = egui::Rect::from_min_size(
                inner_rect.min + egui::vec2(12.0, 12.0),
                Vec2::new(320.0, 112.0),
            );
            painter.rect_filled(
                overlay_rect,
                8.0,
                Color32::from_rgba_unmultiplied(9, 13, 18, 210),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 10.0),
                egui::Align2::LEFT_TOP,
                format!("scene: {scene_name}"),
                egui::FontId::monospace(13.0),
                Color32::from_rgb(246, 211, 101),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 28.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "tris: {} submitted / {} lit / {} px",
                    surface.last_stats.triangles_submitted,
                    surface.last_stats.triangles_rasterized,
                    surface.last_stats.pixels_shaded
                ),
                egui::FontId::monospace(12.0),
                Color32::from_rgb(173, 216, 255),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 44.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "particles: {} submitted / {} blended",
                    surface.last_stats.particles_submitted, surface.last_stats.particles_shaded
                ),
                egui::FontId::monospace(11.0),
                Color32::from_rgb(139, 214, 123),
            );
            let camera_position = surface.controller.position;
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 61.0),
                egui::Align2::LEFT_TOP,
                format!(
                    "cam: [{:.1}, {:.1}, {:.1}]  speed: {:.1}",
                    camera_position.x,
                    camera_position.y,
                    camera_position.z,
                    surface.controller.move_speed
                ),
                egui::FontId::monospace(11.0),
                Color32::from_rgb(173, 216, 255),
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 78.0),
                egui::Align2::LEFT_TOP,
                "native Kain viewport | roam: WASD + QE | drag: look | wheel: speed",
                egui::FontId::monospace(10.5),
                if response.has_focus() || response.hovered() {
                    Color32::from_rgb(246, 211, 101)
                } else {
                    Color32::from_rgb(145, 152, 167)
                },
            );
            painter.text(
                overlay_rect.min + egui::vec2(10.0, 94.0),
                egui::Align2::LEFT_TOP,
                viewport_summary.as_str(),
                egui::FontId::monospace(10.5),
                Color32::from_rgb(129, 198, 255),
            );
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
    Frame::group(ui.style())
        .fill(Color32::from_rgb(24, 32, 41))
        .stroke(Stroke::new(1.0, Color32::from_rgb(70, 92, 118)))
        .corner_radius(10.0)
        .inner_margin(12.0)
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.label(RichText::new(title).strong());
                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    ui.small(
                        RichText::new(format!("{:?}", node.kind))
                            .monospace()
                            .color(Color32::from_rgb(173, 216, 255)),
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
        controller.yaw -= delta.x * 0.009;
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
    let forward = controller.forward();
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

fn viewport_render_resolution(size: Vec2) -> RenderResolution {
    let width = size.x.max(32.0);
    let height = size.y.max(32.0);
    let dominant_axis = width.max(height);
    let scale = if dominant_axis > 240.0 {
        240.0 / dominant_axis
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
