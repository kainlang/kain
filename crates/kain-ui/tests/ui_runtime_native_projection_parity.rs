use kain_ui::{
    ui_runtime_bundle_from_output, ui_runtime_bundle_from_output_with_native_projection,
    ui_runtime_bundle_to_json, UiLayoutKind, UiNode, UiRuntimeMetadata, UiTreeBuilder,
    UiWidgetKind,
};
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
}

#[test]
fn canonical_runtime_bundle_omits_native_projection_until_requested() {
    let mut builder = UiTreeBuilder::new();
    let root_id = builder.alloc_id();

    builder.add_node(UiNode::new(root_id, UiWidgetKind::Panel));
    builder.set_root(root_id);

    let output = builder.finish();
    let metadata = UiRuntimeMetadata {
        app_name: Some("parity-fixture".to_string()),
        window_title: "Kain UI Parity Fixture".to_string(),
        root_component: "App".to_string(),
        source_file_name: Some("ui_runtime_parity.kn".to_string()),
        initial_window_size: [1440.0, 920.0],
    };

    let canonical_bundle = ui_runtime_bundle_from_output(metadata.clone(), output.clone());
    let canonical_json =
        ui_runtime_bundle_to_json(&canonical_bundle).expect("canonical bundle should serialize");
    let canonical_value: serde_json::Value =
        serde_json::from_str(&canonical_json).expect("canonical bundle json should parse");

    assert!(
        canonical_value.get("native_projection").is_none(),
        "canonical bundles should not serialize the compatibility sidecar"
    );

    let compatibility_bundle =
        ui_runtime_bundle_from_output_with_native_projection(metadata, output);
    let compatibility_json = ui_runtime_bundle_to_json(&compatibility_bundle)
        .expect("compatibility bundle should serialize");
    let compatibility_value: serde_json::Value =
        serde_json::from_str(&compatibility_json).expect("compatibility bundle json should parse");
    let compatibility_projection = compatibility_value
        .get("native_projection")
        .expect("explicit compatibility helper should emit the sidecar");

    assert_eq!(
        compatibility_projection
            .get("root_id")
            .and_then(serde_json::Value::as_u64),
        Some(root_id.0)
    );
}
