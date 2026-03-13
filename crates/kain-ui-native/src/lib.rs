use eframe::egui::{self, Align, Color32, Frame, Layout, RichText, Stroke, Vec2};
use kain_core::{build_ui_output_from_source, render_ui_output_debug};
use kain_ui::{UiBuildOutput, UiLayoutKind, UiNode, UiPatch, UiTree, UiValue, UiWidgetKind};

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
        <viewport3d title="Runtime Preview" />
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

pub fn build_demo_output(
    config: &KainUiNativeDemoConfig,
) -> Result<UiBuildOutput, kain_core::KainError> {
    build_output(config)
}

pub fn run_app(config: KainUiNativeAppConfig) -> Result<(), Box<dyn std::error::Error>> {
    let output = build_output(&config)?;
    let window_title = config.window_title.clone();
    let initial_window_size = config.initial_window_size;

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size(initial_window_size)
            .with_min_inner_size([960.0, 640.0]),
        ..Default::default()
    };

    eframe::run_native(
        &window_title,
        options,
        Box::new(move |_cc| {
            Ok(Box::new(KainUiNativeApp::new(
                config.clone(),
                output.clone(),
            )))
        }),
    )?;

    Ok(())
}

pub fn run_demo(config: KainUiNativeDemoConfig) -> Result<(), Box<dyn std::error::Error>> {
    run_app(config)
}

#[derive(Clone)]
struct KainUiNativeApp {
    config: KainUiNativeAppConfig,
    output: UiBuildOutput,
    debug_tree: String,
}

impl KainUiNativeApp {
    fn new(config: KainUiNativeAppConfig, output: UiBuildOutput) -> Self {
        let debug_tree = render_ui_output_debug(&output);
        Self {
            config,
            output,
            debug_tree,
        }
    }
}

impl eframe::App for KainUiNativeApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        apply_demo_visuals(ctx);

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
                    ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                        ui.label(
                            RichText::new(format!("root: {}", self.config.root_component))
                                .monospace()
                                .color(Color32::from_rgb(246, 211, 101)),
                        );
                    });
                });
            });

        egui::SidePanel::right("kain_ui_native_inspector")
            .default_width(360.0)
            .show(ctx, |ui| {
                ui.heading("Runtime Inspector");
                ui.label("Retained semantic tree and emitted patch stream.");
                ui.separator();

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
            });

        egui::CentralPanel::default().show(ctx, |ui| {
            if let Some(root_id) = self.output.tree.root {
                render_node(ui, &self.output.tree, root_id);
            }
        });
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

fn render_node(ui: &mut egui::Ui, tree: &UiTree, id: kain_ui::UiNodeId) {
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
                    render_children(ui, tree, node);
                });
        }
        UiWidgetKind::Inspector => {
            let title = prop_text(node, "title").unwrap_or("Inspector");
            egui::CollapsingHeader::new(RichText::new(title).strong())
                .default_open(true)
                .show(ui, |ui| {
                    render_children(ui, tree, node);
                });
        }
        UiWidgetKind::Tree => {
            let title = prop_text(node, "title").unwrap_or("Tree");
            egui::CollapsingHeader::new(
                RichText::new(title).color(Color32::from_rgb(184, 221, 255)),
            )
            .default_open(true)
            .show(ui, |ui| {
                render_children(ui, tree, node);
            });
        }
        UiWidgetKind::Graph => {
            render_surface_frame(
                ui,
                node,
                "Graph Canvas",
                Vec2::new(ui.available_width(), 220.0),
                |ui, rect| {
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
                Vec2::new(ui.available_width(), 120.0),
                |ui, rect| {
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
            render_surface_frame(
                ui,
                node,
                label,
                Vec2::new(ui.available_width(), 260.0),
                |ui, rect| {
                    let painter = ui.painter();
                    painter.rect_filled(rect.shrink(4.0), 12.0, Color32::from_rgb(10, 14, 18));
                    painter.circle_filled(rect.center(), 42.0, Color32::from_rgb(61, 90, 128));
                    painter.circle_stroke(
                        rect.center(),
                        72.0,
                        Stroke::new(2.0, Color32::from_rgb(246, 211, 101)),
                    );
                    painter.text(
                        rect.center(),
                        egui::Align2::CENTER_CENTER,
                        "KAIN",
                        egui::FontId::proportional(26.0),
                        Color32::from_rgb(234, 236, 239),
                    );
                },
            );
        }
        UiWidgetKind::ComponentRef(name) => {
            ui.label(
                RichText::new(format!("component {name}"))
                    .monospace()
                    .color(Color32::from_rgb(246, 211, 101)),
            );
            render_children(ui, tree, node);
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
                    render_children(ui, tree, node);
                });
        }
        UiWidgetKind::Table | UiWidgetKind::Overlay | UiWidgetKind::Slot => {
            render_children(ui, tree, node);
        }
    }
}

fn render_children(ui: &mut egui::Ui, tree: &UiTree, node: &UiNode) {
    match node.layout.kind {
        UiLayoutKind::FlexRow => {
            ui.horizontal_top(|ui| {
                for child in &node.children {
                    render_node(ui, tree, *child);
                }
            });
        }
        _ => {
            for child in &node.children {
                render_node(ui, tree, *child);
            }
        }
    }
}

fn render_surface_frame(
    ui: &mut egui::Ui,
    node: &UiNode,
    fallback_title: &str,
    desired_size: Vec2,
    paint: impl FnOnce(&mut egui::Ui, egui::Rect),
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
            let (rect, _) = ui.allocate_exact_size(desired_size, egui::Sense::hover());
            paint(ui, rect);
        });
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
