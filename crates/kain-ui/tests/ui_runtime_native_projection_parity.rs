use kain_ui::{UiLayoutKind, UiNativeProjectionKind, UiWidgetKind};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct UiParityMetadata {
    window_title: String,
}

#[derive(Debug, Deserialize)]
struct UiParityOutput {
    tree: UiParityTree,
}

#[derive(Debug, Deserialize)]
struct UiParityTree {
    root: u64,
    nodes: std::collections::BTreeMap<String, UiParityNode>,
}

#[derive(Debug, Deserialize)]
struct UiParityNode {
    id: u64,
    kind: UiWidgetKind,
    children: Vec<u64>,
    layout: UiParityLayout,
}

#[derive(Debug, Deserialize)]
struct UiParityLayout {
    kind: UiLayoutKind,
}

#[derive(Debug, Deserialize)]
struct UiParityBundle {
    metadata: UiParityMetadata,
    output: UiParityOutput,
    native_projection: Option<serde_json::Value>,
}

#[test]
fn ui_runtime_bundle_fixture_keeps_canonical_output_tree_stable() {
    const FIXTURE_JSON: &str = include_str!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../runtime/conformance/ui_runtime/fixtures/ui_runtime_parity_bundle.json"
    ));

    let bundle: UiParityBundle =
        serde_json::from_str(FIXTURE_JSON).expect("fixture json should parse");

    assert_eq!(bundle.metadata.window_title, "Kain UI Parity Fixture");
    assert_eq!(bundle.output.tree.root, 1);
    assert_eq!(bundle.output.tree.nodes.len(), 3);

    let root = bundle
        .output
        .tree
        .nodes
        .get("1")
        .expect("root node should exist in canonical tree");
    assert_eq!(root.id, 1);
    assert!(matches!(root.kind, UiWidgetKind::Panel));
    assert_eq!(root.children, vec![2, 3]);
    assert_eq!(root.layout.kind, UiLayoutKind::Stack);

    let editable = bundle
        .output
        .tree
        .nodes
        .get("2")
        .expect("editable node should exist in canonical tree");
    assert_eq!(editable.id, 2);
    assert!(matches!(editable.kind, UiWidgetKind::Element(ref value) if value == "input"));
    assert!(editable.children.is_empty());
    assert_eq!(editable.layout.kind, UiLayoutKind::Flow);

    let viewport = bundle
        .output
        .tree
        .nodes
        .get("3")
        .expect("viewport node should exist in canonical tree");
    assert_eq!(viewport.id, 3);
    assert!(matches!(viewport.kind, UiWidgetKind::Viewport3D));
    assert!(viewport.children.is_empty());
    assert_eq!(viewport.layout.kind, UiLayoutKind::Absolute);

    let native_projection = bundle
        .native_projection
        .expect("compatibility sidecar should still exist while raw-native depends on it");
    let projection_root_id = native_projection
        .get("root_id")
        .and_then(serde_json::Value::as_u64);
    assert_eq!(projection_root_id, Some(bundle.output.tree.root));

    let projection_nodes = native_projection
        .get("nodes")
        .and_then(serde_json::Value::as_array)
        .expect("compatibility sidecar nodes should remain serializable");
    assert_eq!(projection_nodes.len(), bundle.output.tree.nodes.len());

    let kind_json = projection_nodes[2]
        .get("kind")
        .cloned()
        .expect("compatibility node kind should exist")
        .to_string();
    assert_eq!(kind_json, "\"Viewport3D\"");
}
