use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;

use crate::no_egui::KainUiNativeBackendPlan;
use kain_ui::{
    UiHostBackendKind, UiRenderEngineKind, UiRuntimeBundle, UiSurface, UiSurfaceCompositionMode,
    UiSurfaceKind,
};

#[derive(Clone, Debug, Serialize)]
pub struct QtQuickSessionManifest {
    pub generated_epoch_ms: u128,
    pub app_name: Option<String>,
    pub window_title: String,
    pub root_component: String,
    pub initial_window_size: [f32; 2],
    pub shell_backend: String,
    pub document_backend: String,
    pub devtools_backend: String,
    pub layout_engine: String,
    pub render_engine: String,
    pub mixed_backend_session: bool,
    pub summary_lines: Vec<String>,
    pub document_panes: Vec<QtQuickPane>,
    pub viewport_panes: Vec<QtQuickPane>,
    pub browser_panes: Vec<QtQuickPane>,
    pub shader_panes: Vec<QtQuickPane>,
    pub devtools_panes: Vec<QtQuickPane>,
    pub fallback_panes: Vec<QtQuickPane>,
}

#[derive(Clone, Debug, Serialize)]
pub struct QtQuickPane {
    pub id: String,
    pub title: String,
    pub role: String,
    pub adapter_state: String,
    pub adapter_state_label: String,
    pub summary: String,
    pub detail_lines: Vec<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum SessionPaneRole {
    Document,
    Viewport,
    Browser,
    Shader,
    Devtools,
    Fallback,
}

pub fn build_qt_quick_session_manifest(
    bundle: &UiRuntimeBundle,
    backend_plan: &KainUiNativeBackendPlan,
) -> QtQuickSessionManifest {
    let mut document_panes = Vec::new();
    let mut viewport_panes = Vec::new();
    let mut browser_panes = Vec::new();
    let mut shader_panes = Vec::new();
    let mut devtools_panes = Vec::new();
    let mut fallback_panes = Vec::new();

    for surface in &bundle.output.systems.surfaces {
        let pane = pane_from_surface(surface);
        match classify_surface_role(surface) {
            SessionPaneRole::Document => document_panes.push(pane),
            SessionPaneRole::Viewport => viewport_panes.push(pane),
            SessionPaneRole::Browser => browser_panes.push(pane),
            SessionPaneRole::Shader => shader_panes.push(pane),
            SessionPaneRole::Devtools => devtools_panes.push(pane),
            SessionPaneRole::Fallback => fallback_panes.push(pane),
        }
    }

    if document_panes.is_empty() {
        document_panes.push(placeholder_pane(
            "document-placeholder",
            "Document Surface",
            "No document-class surfaces were emitted by this bundle.",
            "qt-qml-panel-placeholder",
        ));
    }

    if viewport_panes.is_empty() {
        viewport_panes.push(placeholder_pane(
            "viewport-placeholder",
            "Viewport Surface",
            "No viewport-class surfaces were emitted by this bundle.",
            "qt-viewport-slot",
        ));
    }

    if browser_panes.is_empty() {
        browser_panes.push(placeholder_pane(
            "browser-placeholder",
            "Browser Surface",
            "No browser-class surfaces were emitted by this bundle.",
            "qt-webengine-slot",
        ));
    }

    if shader_panes.is_empty() {
        shader_panes.push(placeholder_pane(
            "shader-placeholder",
            "Shader Surface",
            "No shader-backed surfaces were emitted by this bundle.",
            "qt-shader-effect-slot",
        ));
    }

    if devtools_panes.is_empty() {
        devtools_panes.push(placeholder_pane(
            "devtools-placeholder",
            "Devtools Surface",
            "No devtools-class surfaces were emitted by this bundle.",
            "imgui-devtools-slot",
        ));
    }

    let summary_lines = vec![
        format!(
            "shell={} document={} devtools={}",
            host_backend_label(backend_plan.shell_host_backend),
            host_backend_label(backend_plan.document_host_backend),
            host_backend_label(backend_plan.devtools_host_backend),
        ),
        format!(
            "layout={} render={} mixed_session={}",
            layout_engine_label(backend_plan),
            render_engine_label(backend_plan.render_engine),
            backend_plan.mixed_backend_session,
        ),
        format!(
            "document_panes={} viewport_panes={} browser_panes={} shader_panes={} devtools_panes={} fallback_panes={}",
            document_panes.len(),
            viewport_panes.len(),
            browser_panes.len(),
            shader_panes.len(),
            devtools_panes.len(),
            fallback_panes.len(),
        ),
    ];

    QtQuickSessionManifest {
        generated_epoch_ms: SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis(),
        app_name: bundle.metadata.app_name.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        initial_window_size: bundle.metadata.initial_window_size,
        shell_backend: host_backend_label(backend_plan.shell_host_backend).to_string(),
        document_backend: host_backend_label(backend_plan.document_host_backend).to_string(),
        devtools_backend: host_backend_label(backend_plan.devtools_host_backend).to_string(),
        layout_engine: layout_engine_label(backend_plan).to_string(),
        render_engine: render_engine_label(backend_plan.render_engine).to_string(),
        mixed_backend_session: backend_plan.mixed_backend_session,
        summary_lines,
        document_panes,
        viewport_panes,
        browser_panes,
        shader_panes,
        devtools_panes,
        fallback_panes,
    }
}

fn pane_from_surface(surface: &UiSurface) -> QtQuickPane {
    let role = classify_surface_role(surface);
    let adapter_state = adapter_state_for_surface(surface, role);
    let kind_label = surface_kind_label(&surface.kind);
    let title = surface
        .title
        .clone()
        .unwrap_or_else(|| format!("{kind_label} {}", surface.id));
    let summary = format!(
        "{} via {} / {}",
        kind_label,
        host_backend_label(surface.preferred_host_backend),
        render_engine_label(surface.preferred_render_engine),
    );
    let detail_lines = vec![
        format!("surface_id={}", surface.id),
        format!(
            "composition={}",
            composition_label(surface.composition_mode)
        ),
        format!("gpu_backing_required={}", surface.gpu_backing_required),
        format!(
            "renderer_preference={}",
            renderer_preference_label(surface.preferred_render_engine, surface)
        ),
        format!(
            "adapter_note={}",
            adapter_summary_for_surface(surface, role)
        ),
    ];

    QtQuickPane {
        id: surface.id.clone(),
        title,
        role: role_label(role).to_string(),
        adapter_state: adapter_state.to_string(),
        adapter_state_label: adapter_summary_for_surface(surface, role).to_string(),
        summary,
        detail_lines,
    }
}

fn placeholder_pane(id: &str, title: &str, summary: &str, adapter_state: &str) -> QtQuickPane {
    QtQuickPane {
        id: id.to_string(),
        title: title.to_string(),
        role: "placeholder".to_string(),
        adapter_state: adapter_state.to_string(),
        adapter_state_label: summary.to_string(),
        summary: summary.to_string(),
        detail_lines: vec![summary.to_string()],
    }
}

fn classify_surface_role(surface: &UiSurface) -> SessionPaneRole {
    if surface.shader.is_some()
        || surface.composition_mode == UiSurfaceCompositionMode::ShaderCanvas
    {
        return SessionPaneRole::Shader;
    }

    if matches!(
        surface.kind,
        UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D
    ) || surface.composition_mode == UiSurfaceCompositionMode::Viewport
    {
        return SessionPaneRole::Viewport;
    }

    if surface.preferred_host_backend == UiHostBackendKind::Cef
        || matches!(&surface.kind, UiSurfaceKind::Custom(value) if value.contains("browser"))
    {
        return SessionPaneRole::Browser;
    }

    if surface.preferred_host_backend == UiHostBackendKind::Imgui
        || matches!(
            surface.kind,
            UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay
        )
    {
        return SessionPaneRole::Devtools;
    }

    if surface.preferred_host_backend == UiHostBackendKind::Cef {
        return SessionPaneRole::Fallback;
    }

    SessionPaneRole::Document
}

fn role_label(role: SessionPaneRole) -> &'static str {
    match role {
        SessionPaneRole::Document => "document",
        SessionPaneRole::Viewport => "viewport",
        SessionPaneRole::Browser => "browser",
        SessionPaneRole::Shader => "shader",
        SessionPaneRole::Devtools => "devtools",
        SessionPaneRole::Fallback => "fallback",
    }
}

fn adapter_state_for_surface(surface: &UiSurface, role: SessionPaneRole) -> &'static str {
    match role {
        SessionPaneRole::Document => {
            if surface.preferred_host_backend == UiHostBackendKind::Qt {
                "qt-qml-backed"
            } else {
                "qt-qml-adapter-pending"
            }
        }
        SessionPaneRole::Viewport => "kain-3d-preview-pending",
        SessionPaneRole::Browser => "qt-webengine-backed",
        SessionPaneRole::Shader => "qt-graphical-effects-backed",
        SessionPaneRole::Devtools => "imgui-devtools-handoff-pending",
        SessionPaneRole::Fallback => "staged-backend-placeholder",
    }
}

fn adapter_summary_for_surface(surface: &UiSurface, role: SessionPaneRole) -> &'static str {
    match role {
        SessionPaneRole::Document => {
            if surface.preferred_host_backend == UiHostBackendKind::Qt {
                "Qt Quick panel shell is live; semantic surface rendering is still a metadata-first panel adapter."
            } else {
                "Surface is routed into the Qt session as a document lane, but its dedicated adapter is not live yet."
            }
        }
        SessionPaneRole::Viewport => {
            if surface.preferred_render_engine == UiRenderEngineKind::Wgpu {
                "Kain 3D software preview is embedded in the Qt session; the native bgfx handoff remains the next adapter cut."
            } else {
                "Viewport slot is reserved inside the Qt session; in-process bgfx embedding is the next adapter cut."
            }
        }
        SessionPaneRole::Browser => {
            "Qt WebEngine is presenting a live browser panel inside the workstation shell."
        }
        SessionPaneRole::Shader => {
            "Qt GraphicalEffects is rendering a real shader-backed surface inside the shell."
        }
        SessionPaneRole::Devtools => {
            "Devtools rail is represented in the Qt session, but the ImGui pane bridge is still pending."
        }
        SessionPaneRole::Fallback => {
            "Requested backend remains staged, so the Qt host renders a deliberate placeholder instead of failing startup."
        }
    }
}

fn host_backend_label(backend: UiHostBackendKind) -> &'static str {
    match backend {
        UiHostBackendKind::Auto => "auto",
        UiHostBackendKind::Native => "native",
        UiHostBackendKind::LegacyEgui => "legacy-egui",
        UiHostBackendKind::Imgui => "imgui",
        UiHostBackendKind::RmlUi => "rmlui",
        UiHostBackendKind::Slint => "slint",
        UiHostBackendKind::Qt => "qt",
        UiHostBackendKind::Cef => "cef",
    }
}

fn layout_engine_label(backend_plan: &KainUiNativeBackendPlan) -> &'static str {
    match backend_plan.layout_engine {
        kain_ui::UiLayoutEngineKind::Auto => "auto",
        kain_ui::UiLayoutEngineKind::Native => "native",
        kain_ui::UiLayoutEngineKind::Yoga => "yoga",
        kain_ui::UiLayoutEngineKind::LegacyEgui => "legacy-egui",
    }
}

fn render_engine_label(render_engine: UiRenderEngineKind) -> &'static str {
    match render_engine {
        UiRenderEngineKind::Auto => "auto",
        UiRenderEngineKind::Native => "native",
        UiRenderEngineKind::Skia => "skia",
        UiRenderEngineKind::Wgpu => "wgpu",
        UiRenderEngineKind::Shader => "shader",
        UiRenderEngineKind::Browser => "browser",
        UiRenderEngineKind::LegacyEgui => "legacy-egui",
    }
}

fn surface_kind_label(kind: &UiSurfaceKind) -> String {
    match kind {
        UiSurfaceKind::Canvas => "canvas".to_string(),
        UiSurfaceKind::Graph => "graph".to_string(),
        UiSurfaceKind::Timeline => "timeline".to_string(),
        UiSurfaceKind::Table => "table".to_string(),
        UiSurfaceKind::Tree => "tree".to_string(),
        UiSurfaceKind::Viewport2D => "viewport2d".to_string(),
        UiSurfaceKind::Viewport3D => "viewport3d".to_string(),
        UiSurfaceKind::Overlay => "overlay".to_string(),
        UiSurfaceKind::Custom(value) => value.clone(),
    }
}

fn composition_label(mode: UiSurfaceCompositionMode) -> &'static str {
    match mode {
        UiSurfaceCompositionMode::Host => "host",
        UiSurfaceCompositionMode::LayeredGpu => "layered_gpu",
        UiSurfaceCompositionMode::Viewport => "viewport",
        UiSurfaceCompositionMode::ShaderCanvas => "shader_canvas",
    }
}

fn renderer_preference_label(render_engine: UiRenderEngineKind, surface: &UiSurface) -> String {
    format!(
        "{} / preferred_host={}",
        render_engine_label(render_engine),
        host_backend_label(surface.preferred_host_backend)
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::no_egui::KainUiNativeBackendPlan;
    use kain_ui::{
        ui_runtime_bundle_from_output, UiBuildOutput, UiHostBackendKind, UiNodeId,
        UiRuntimeMetadata, UiRuntimeSystems, UiSurface, UiSurfaceCompositionMode, UiSurfaceKind,
        UiSurfaceRendererPreference, UiSurfaceShaderBinding,
    };

    #[test]
    fn qt_quick_manifest_routes_mixed_surface_roles() {
        let bundle = UiRuntimeBundle {
            schema_version: 1,
            metadata: UiRuntimeMetadata {
                window_title: "Qt Hybrid".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            output: UiBuildOutput {
                tree: Default::default(),
                patches: Vec::new(),
                systems: UiRuntimeSystems {
                    surfaces: vec![
                        UiSurface {
                            id: "viewport.main".to_string(),
                            kind: UiSurfaceKind::Viewport3D,
                            node: UiNodeId(1),
                            title: Some("Viewport".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Wgpu,
                            composition_mode: UiSurfaceCompositionMode::Viewport,
                            preferred_host_backend: UiHostBackendKind::Qt,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Wgpu,
                            gpu_backing_required: true,
                            shader: None,
                        },
                        UiSurface {
                            id: "graph.tools".to_string(),
                            kind: UiSurfaceKind::Graph,
                            node: UiNodeId(2),
                            title: Some("Graph".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Auto,
                            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
                            preferred_host_backend: UiHostBackendKind::Imgui,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Wgpu,
                            gpu_backing_required: false,
                            shader: None,
                        },
                        UiSurface {
                            id: "document.inspector".to_string(),
                            kind: UiSurfaceKind::Table,
                            node: UiNodeId(3),
                            title: Some("Inspector".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Dom,
                            composition_mode: UiSurfaceCompositionMode::Host,
                            preferred_host_backend: UiHostBackendKind::RmlUi,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Skia,
                            gpu_backing_required: false,
                            shader: None,
                        },
                        UiSurface {
                            id: "browser.panel".to_string(),
                            kind: UiSurfaceKind::Custom("browser_panel".to_string()),
                            node: UiNodeId(4),
                            title: Some("Browser".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Dom,
                            composition_mode: UiSurfaceCompositionMode::Host,
                            preferred_host_backend: UiHostBackendKind::Cef,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Browser,
                            gpu_backing_required: false,
                            shader: None,
                        },
                        UiSurface {
                            id: "shader.canvas".to_string(),
                            kind: UiSurfaceKind::Canvas,
                            node: UiNodeId(5),
                            title: Some("Shader Canvas".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Shader,
                            composition_mode: UiSurfaceCompositionMode::ShaderCanvas,
                            preferred_host_backend: UiHostBackendKind::Qt,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Shader,
                            gpu_backing_required: true,
                            shader: Some(UiSurfaceShaderBinding {
                                shader_ref: "kain://shader/plasma-glow".to_string(),
                                entry_point: Some("main".to_string()),
                                stage: Some("fragment".to_string()),
                                derived_format: Some("rgba8unorm".to_string()),
                            }),
                        },
                        UiSurface {
                            id: "devtools.timeline".to_string(),
                            kind: UiSurfaceKind::Timeline,
                            node: UiNodeId(6),
                            title: Some("Timeline".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Native,
                            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
                            preferred_host_backend: UiHostBackendKind::Imgui,
                            preferred_layout_engine: Default::default(),
                            preferred_render_engine: UiRenderEngineKind::Wgpu,
                            gpu_backing_required: true,
                            shader: None,
                        },
                    ],
                    ..UiRuntimeSystems::default()
                },
            },
            native_projection: Default::default(),
        };

        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());

        assert_eq!(manifest.document_panes.len(), 1);
        assert_eq!(manifest.viewport_panes.len(), 1);
        assert_eq!(manifest.browser_panes.len(), 1);
        assert_eq!(manifest.shader_panes.len(), 1);
        assert_eq!(manifest.devtools_panes.len(), 2);
        assert_eq!(manifest.fallback_panes.len(), 0);
    }

    #[test]
    fn qt_quick_manifest_synthesizes_missing_lane_placeholders() {
        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                window_title: "Qt Hybrid".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            UiBuildOutput::default(),
        );

        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());

        assert_eq!(manifest.document_panes.len(), 1);
        assert_eq!(manifest.viewport_panes.len(), 1);
        assert_eq!(manifest.browser_panes.len(), 1);
        assert_eq!(manifest.shader_panes.len(), 1);
        assert_eq!(manifest.devtools_panes.len(), 1);
        assert_eq!(manifest.document_panes[0].role, "placeholder");
        assert_eq!(manifest.browser_panes[0].role, "placeholder");
        assert_eq!(manifest.shader_panes[0].role, "placeholder");
    }
}
