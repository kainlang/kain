use kain_ui::{UiLayoutKind, UiNativeProjection, UiNativeProjectionKind};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct UiParityMetadata {
    window_title: String,
}

#[derive(Debug, Deserialize)]
struct UiParityBundle {
    metadata: UiParityMetadata,
    native_projection: UiNativeProjection,
}

#[test]
fn ui_native_projection_parity_fixture_is_stable() {
    const FIXTURE_JSON: &str = include_str!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../runtime/conformance/ui_runtime/fixtures/ui_runtime_parity_bundle.json"
    ));

    let bundle: UiParityBundle =
        serde_json::from_str(FIXTURE_JSON).expect("fixture json should parse");

    assert_eq!(bundle.metadata.window_title, "Kain UI Parity Fixture");

    let projection = bundle.native_projection;
    assert_eq!(projection.root_id, Some(1));
    assert_eq!(
        projection.primary_panel_title.as_deref(),
        Some("UI Surface")
    );
    assert_eq!(
        projection.primary_viewport_title.as_deref(),
        Some("Viewport")
    );
    assert_eq!(
        projection.primary_viewport_scene.as_deref(),
        Some("magma_terraces")
    );

    assert_eq!(projection.nodes.len(), 3);

    let root = &projection.nodes[0];
    assert_eq!(root.id, 1);
    assert_eq!(root.parent_id, None);
    assert_eq!(root.depth, 0);
    assert_eq!(root.kind, UiNativeProjectionKind::Panel);
    assert_eq!(root.title.as_deref(), Some("Root Panel"));
    assert_eq!(root.text.as_deref(), Some("compiled overlay"));
    assert_eq!(root.tag.as_deref(), Some("panel"));
    assert_eq!(root.scene.as_deref(), Some("magma_terraces"));
    assert_eq!(root.layout_kind, UiLayoutKind::Stack);
    assert_eq!(root.child_count, 2);

    let editable = &projection.nodes[1];
    assert_eq!(editable.id, 2);
    assert_eq!(editable.parent_id, Some(1));
    assert_eq!(editable.depth, 1);
    assert_eq!(editable.kind, UiNativeProjectionKind::Element);
    assert_eq!(editable.title.as_deref(), Some("Name Field"));
    assert_eq!(editable.text.as_deref(), Some("Ada"));
    assert_eq!(editable.tag.as_deref(), Some("input"));
    assert_eq!(editable.scene.as_deref(), Some("magma_terraces"));
    assert_eq!(editable.layout_kind, UiLayoutKind::Flow);
    assert_eq!(editable.child_count, 0);

    let viewport = &projection.nodes[2];
    assert_eq!(viewport.id, 3);
    assert_eq!(viewport.parent_id, Some(1));
    assert_eq!(viewport.depth, 1);
    assert_eq!(viewport.kind, UiNativeProjectionKind::Viewport3D);
    assert_eq!(viewport.title.as_deref(), Some("Viewport"));
    assert_eq!(viewport.text.as_deref(), Some(""));
    assert_eq!(viewport.tag.as_deref(), Some("viewport"));
    assert_eq!(viewport.scene.as_deref(), Some("magma_terraces"));
    assert_eq!(viewport.layout_kind, UiLayoutKind::Absolute);
    assert_eq!(viewport.child_count, 0);

    // Extra guard: ensure Serde keeps the string tags the native C loader depends on.
    let kind_json = serde_json::to_string(&viewport.kind).expect("serialize kind");
    assert_eq!(kind_json, "\"Viewport3D\"");
}

