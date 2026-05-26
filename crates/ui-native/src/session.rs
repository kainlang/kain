use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;

use crate::app::KainUiNativeBackendPlan;
use kain_ui::{
    ui_native_projection_from_output, UiHostBackendKind, UiLayoutEngineKind, UiNativeProjection,
    UiRenderEngineKind, UiRuntimeBundle, UiSurface, UiSurfaceCompositionMode, UiSurfaceKind,
};

#[derive(Clone, Debug, Serialize)]
pub struct KainUiNativeSessionManifest {
    pub generated_epoch_ms: u128,
    pub app_name: Option<String>,
    pub window_title: String,
    pub root_component: String,
    pub initial_window_size: [f32; 2],
    pub backend_plan: KainUiNativeBackendManifest,
    pub native_projection: UiNativeProjection,
    pub authored_surfaces: Vec<KainUiAuthoredSurface>,
}

impl KainUiNativeSessionManifest {
    pub fn authored_node_count(&self) -> usize {
        self.native_projection.nodes.len()
    }

    pub fn authored_surface_count(&self) -> usize {
        self.authored_surfaces.len()
    }

    pub fn has_authored_ui(&self) -> bool {
        !self.native_projection.nodes.is_empty() || !self.authored_surfaces.is_empty()
    }
}

#[derive(Clone, Debug, Serialize)]
pub struct KainUiNativeBackendManifest {
    pub shell_host_backend: String,
    pub document_host_backend: String,
    pub devtools_host_backend: String,
    pub layout_engine: String,
    pub render_engine: String,
    pub compatibility_host_backend: String,
    pub mixed_backend_session: bool,
}

#[derive(Clone, Debug, Serialize)]
pub struct KainUiAuthoredSurface {
    pub id: String,
    pub title: Option<String>,
    pub node_id: u64,
    pub kind: String,
    pub composition_mode: String,
    pub preferred_host_backend: String,
    pub preferred_layout_engine: String,
    pub preferred_render_engine: String,
    pub gpu_backing_required: bool,
    pub shader_ref: Option<String>,
}

pub fn build_qt_quick_session_manifest(
    bundle: &UiRuntimeBundle,
    backend_plan: &KainUiNativeBackendPlan,
) -> KainUiNativeSessionManifest {
    let native_projection = if bundle.native_projection.is_empty() {
        ui_native_projection_from_output(&bundle.output)
    } else {
        bundle.native_projection.clone()
    };

    KainUiNativeSessionManifest {
        generated_epoch_ms: SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis(),
        app_name: bundle.metadata.app_name.clone(),
        window_title: bundle.metadata.window_title.clone(),
        root_component: bundle.metadata.root_component.clone(),
        initial_window_size: bundle.metadata.initial_window_size,
        backend_plan: backend_manifest_from_plan(backend_plan),
        native_projection,
        authored_surfaces: bundle
            .output
            .systems
            .surfaces
            .iter()
            .map(authored_surface_from_runtime_surface)
            .collect(),
    }
}

fn backend_manifest_from_plan(
    backend_plan: &KainUiNativeBackendPlan,
) -> KainUiNativeBackendManifest {
    KainUiNativeBackendManifest {
        shell_host_backend: host_backend_label(backend_plan.shell_host_backend).to_string(),
        document_host_backend: host_backend_label(backend_plan.document_host_backend).to_string(),
        devtools_host_backend: host_backend_label(backend_plan.devtools_host_backend).to_string(),
        layout_engine: layout_engine_label(backend_plan.layout_engine).to_string(),
        render_engine: render_engine_label(backend_plan.render_engine).to_string(),
        compatibility_host_backend: host_backend_label(backend_plan.compatibility_host_backend)
            .to_string(),
        mixed_backend_session: backend_plan.mixed_backend_session,
    }
}

fn authored_surface_from_runtime_surface(surface: &UiSurface) -> KainUiAuthoredSurface {
    KainUiAuthoredSurface {
        id: surface.id.clone(),
        title: surface.title.clone(),
        node_id: surface.node.0,
        kind: surface_kind_label(&surface.kind),
        composition_mode: composition_label(surface.composition_mode).to_string(),
        preferred_host_backend: host_backend_label(surface.preferred_host_backend).to_string(),
        preferred_layout_engine: layout_engine_label(surface.preferred_layout_engine).to_string(),
        preferred_render_engine: render_engine_label(surface.preferred_render_engine).to_string(),
        gpu_backing_required: surface.gpu_backing_required,
        shader_ref: surface
            .shader
            .as_ref()
            .map(|shader| shader.shader_ref.clone()),
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
        UiHostBackendKind::Tauri => "tauri",
    }
}

fn layout_engine_label(layout_engine: UiLayoutEngineKind) -> &'static str {
    match layout_engine {
        UiLayoutEngineKind::Auto => "auto",
        UiLayoutEngineKind::Native => "native",
        UiLayoutEngineKind::Yoga => "yoga",
        UiLayoutEngineKind::LegacyEgui => "legacy-egui",
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

#[cfg(test)]
mod tests {
    use super::*;
    use kain_ui::{
        ui_runtime_bundle_from_output, UiBuildOutput, UiNode, UiNodeId, UiRuntimeMetadata,
        UiRuntimeSystems, UiSurfaceRendererPreference, UiSurfaceShaderBinding, UiValue,
        UiWidgetKind,
    };

    #[test]
    fn empty_runtime_bundle_does_not_synthesize_ui() {
        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                window_title: "Blank".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            UiBuildOutput::default(),
        );

        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());

        assert_eq!(manifest.authored_node_count(), 0);
        assert_eq!(manifest.authored_surface_count(), 0);
        assert!(!manifest.has_authored_ui());
        assert!(manifest.authored_surfaces.is_empty());
    }

    #[test]
    fn authored_surfaces_are_manifest_data_not_lane_catalogs() {
        let bundle = UiRuntimeBundle {
            schema_version: 1,
            metadata: UiRuntimeMetadata {
                window_title: "Authored".to_string(),
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
                            preferred_layout_engine: UiLayoutEngineKind::Yoga,
                            preferred_render_engine: UiRenderEngineKind::Wgpu,
                            gpu_backing_required: true,
                            shader: None,
                        },
                        UiSurface {
                            id: "shader.canvas".to_string(),
                            kind: UiSurfaceKind::Canvas,
                            node: UiNodeId(2),
                            title: Some("Shader Canvas".to_string()),
                            renderer_preference: UiSurfaceRendererPreference::Shader,
                            composition_mode: UiSurfaceCompositionMode::ShaderCanvas,
                            preferred_host_backend: UiHostBackendKind::Qt,
                            preferred_layout_engine: UiLayoutEngineKind::Yoga,
                            preferred_render_engine: UiRenderEngineKind::Shader,
                            gpu_backing_required: true,
                            shader: Some(UiSurfaceShaderBinding {
                                shader_ref: "kain://shader/plasma-glow".to_string(),
                                entry_point: Some("main".to_string()),
                                stage: Some("fragment".to_string()),
                                derived_format: Some("rgba8unorm".to_string()),
                            }),
                        },
                    ],
                    ..UiRuntimeSystems::default()
                },
            },
            native_projection: Default::default(),
        };

        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());

        assert_eq!(manifest.authored_surfaces.len(), 2);
        assert_eq!(manifest.authored_surfaces[0].id, "viewport.main");
        assert_eq!(
            manifest.authored_surfaces[1].shader_ref.as_deref(),
            Some("kain://shader/plasma-glow")
        );
    }

    #[test]
    fn manifest_uses_projection_built_from_output_tree() {
        let root_id = UiNodeId(1);
        let panel_id = UiNodeId(2);
        let viewport_id = UiNodeId(3);

        let mut root = UiNode::new(root_id, UiWidgetKind::ComponentRef("App".to_string()));
        root.children.push(panel_id);

        let mut panel = UiNode::new(panel_id, UiWidgetKind::Panel);
        panel.children.push(viewport_id);
        panel.props.insert(
            "title".to_string(),
            UiValue::String("Workbench".to_string()),
        );

        let mut viewport = UiNode::new(viewport_id, UiWidgetKind::Viewport3D);
        viewport
            .props
            .insert("title".to_string(), UiValue::String("Caldera".to_string()));
        viewport.props.insert(
            "scene".to_string(),
            UiValue::String("geometry_fixture".to_string()),
        );

        let mut output = UiBuildOutput::default();
        output.tree.root = Some(root_id);
        output.tree.nodes.insert(root_id, root);
        output.tree.nodes.insert(panel_id, panel);
        output.tree.nodes.insert(viewport_id, viewport);

        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                window_title: "Projection".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            output,
        );

        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());

        assert_eq!(manifest.native_projection.root_id, Some(root_id.0));
        assert_eq!(
            manifest.native_projection.primary_panel_title.as_deref(),
            Some("Workbench")
        );
        assert_eq!(
            manifest.native_projection.primary_viewport_scene.as_deref(),
            Some("geometry_fixture")
        );
    }
}
