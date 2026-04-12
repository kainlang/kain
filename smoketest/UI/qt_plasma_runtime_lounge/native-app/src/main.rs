use std::{collections::BTreeMap, error::Error};

use kain_ui::{
    ui_runtime_bundle_from_output, UiBuildOutput, UiHostBackendKind, UiLayoutEngineKind,
    UiLayoutKind, UiLayoutSpec, UiNode, UiNodeId, UiRenderEngineKind, UiRuntimeBundle,
    UiRuntimeMetadata, UiRuntimeSystems, UiStyleSpec, UiSurface, UiSurfaceCompositionMode,
    UiSurfaceKind, UiSurfaceRendererPreference, UiTree, UiWidgetKind,
};
use kain_ui_native::run_bundled_app;

fn main() -> Result<(), Box<dyn Error>> {
    run_bundled_app(build_runtime_bundle())
}

fn build_runtime_bundle() -> UiRuntimeBundle {
    ui_runtime_bundle_from_output(runtime_metadata(), runtime_output())
}

fn runtime_metadata() -> UiRuntimeMetadata {
    UiRuntimeMetadata {
        app_name: Some("ui-smoke-qt-plasma-runtime-lounge".to_string()),
        window_title: "Kain Plasma Runtime Lounge".to_string(),
        root_component: "PlasmaControlDeck".to_string(),
        source_file_name: Some("smoketest/UI/qt_plasma_runtime_lounge".to_string()),
        initial_window_size: [1560.0, 960.0],
        preferred_shell_host_backend: UiHostBackendKind::Qt,
        preferred_document_host_backend: UiHostBackendKind::Qt,
        preferred_devtools_host_backend: UiHostBackendKind::Imgui,
        preferred_layout_engine: UiLayoutEngineKind::Yoga,
        preferred_render_engine: UiRenderEngineKind::Skia,
        compatibility_host_backend: UiHostBackendKind::Qt,
        mixed_backend_session: true,
    }
}

fn runtime_output() -> UiBuildOutput {
    UiBuildOutput {
        tree: smoke_tree(),
        patches: Vec::new(),
        systems: smoke_runtime_systems(),
    }
}

fn smoke_tree() -> UiTree {
    let mut nodes = BTreeMap::new();

    let mut root = node(1, UiWidgetKind::Panel, "plasma-root");
    root.layout.kind = UiLayoutKind::Dock;
    root.children = vec![
        UiNodeId(10),
        UiNodeId(20),
        UiNodeId(30),
        UiNodeId(40),
        UiNodeId(50),
        UiNodeId(60),
    ];
    nodes.insert(root.id, root);

    nodes.insert(
        UiNodeId(10),
        node(10, UiWidgetKind::Panel, "session-browser"),
    );
    nodes.insert(
        UiNodeId(20),
        node(20, UiWidgetKind::Panel, "signal-storyboard"),
    );
    nodes.insert(
        UiNodeId(30),
        node(30, UiWidgetKind::Viewport3D, "nebula-viewport"),
    );
    nodes.insert(
        UiNodeId(40),
        node(40, UiWidgetKind::Graph, "runtime-inspector"),
    );
    nodes.insert(
        UiNodeId(50),
        node(50, UiWidgetKind::Timeline, "transport-timeline"),
    );
    nodes.insert(
        UiNodeId(60),
        node(60, UiWidgetKind::Panel, "browser-fallback"),
    );

    UiTree {
        root: Some(UiNodeId(1)),
        nodes,
    }
}

fn node(id: u64, kind: UiWidgetKind, identity_key: &str) -> UiNode {
    let mut node = UiNode::new(UiNodeId(id), kind);
    node.identity_key = Some(identity_key.to_string());
    node.layout = UiLayoutSpec::default();
    node.style = UiStyleSpec::default();
    node
}

fn smoke_runtime_systems() -> UiRuntimeSystems {
    let mut systems = UiRuntimeSystems::default();
    systems.surfaces = vec![
        UiSurface {
            id: "session-browser".to_string(),
            kind: UiSurfaceKind::Canvas,
            node: UiNodeId(10),
            title: Some("Session Browser".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Native,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::Qt,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Skia,
            gpu_backing_required: false,
            shader: None,
        },
        UiSurface {
            id: "signal-storyboard".to_string(),
            kind: UiSurfaceKind::Table,
            node: UiNodeId(20),
            title: Some("Signal Storyboard".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Dom,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::RmlUi,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Skia,
            gpu_backing_required: false,
            shader: None,
        },
        UiSurface {
            id: "nebula-viewport".to_string(),
            kind: UiSurfaceKind::Viewport3D,
            node: UiNodeId(30),
            title: Some("Nebula Viewport".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Wgpu,
            composition_mode: UiSurfaceCompositionMode::Viewport,
            preferred_host_backend: UiHostBackendKind::Qt,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "runtime-inspector".to_string(),
            kind: UiSurfaceKind::Graph,
            node: UiNodeId(40),
            title: Some("Runtime Inspector".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Native,
            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
            preferred_host_backend: UiHostBackendKind::Imgui,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "transport-timeline".to_string(),
            kind: UiSurfaceKind::Timeline,
            node: UiNodeId(50),
            title: Some("Transport Timeline".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Native,
            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
            preferred_host_backend: UiHostBackendKind::Imgui,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "browser-fallback".to_string(),
            kind: UiSurfaceKind::Custom("browser_panel".to_string()),
            node: UiNodeId(60),
            title: Some("Reference Browser".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Dom,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::Cef,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Browser,
            gpu_backing_required: false,
            shader: None,
        },
    ];
    systems
}
