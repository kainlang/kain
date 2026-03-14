//! KAIN UI runtime and semantic interface graph.
//!
//! `kain-ui` is the home for KAIN's native-first UI runtime model. The crate
//! focuses on semantic nodes, retained tree state, renderer capability tables,
//! and patch streams instead of a virtual DOM-first execution model.

use std::{
    collections::BTreeMap,
    io::{Error as IoError, ErrorKind},
};

use serde::{Deserialize, Serialize};

/// Stable identifier for a node within a retained UI tree.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct UiNodeId(pub u64);

/// Stable identifier for a reactive signal.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct UiSignalId(pub u64);

/// Supported renderer families for KAIN UI.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum UiRendererKind {
    Native,
    Web,
    Slate,
    Debug,
}

/// Declarative backend capability profile.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct UiBackendCapabilities {
    pub renderer: UiRendererKind,
    pub supports_real_windowing: bool,
    pub supports_dom_embedding: bool,
    pub supports_gpu_viewports: bool,
    pub supports_docking: bool,
    pub supports_rich_text: bool,
    pub supports_pointer_capture: bool,
    pub supports_accessibility_tree: bool,
}

/// Data-driven backend capability registry.
pub const UI_BACKEND_CAPABILITIES: &[UiBackendCapabilities] = &[
    UiBackendCapabilities {
        renderer: UiRendererKind::Native,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: true,
    },
    UiBackendCapabilities {
        renderer: UiRendererKind::Web,
        supports_real_windowing: false,
        supports_dom_embedding: true,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: true,
    },
    UiBackendCapabilities {
        renderer: UiRendererKind::Slate,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: false,
    },
    UiBackendCapabilities {
        renderer: UiRendererKind::Debug,
        supports_real_windowing: false,
        supports_dom_embedding: false,
        supports_gpu_viewports: false,
        supports_docking: false,
        supports_rich_text: false,
        supports_pointer_capture: false,
        supports_accessibility_tree: false,
    },
];

pub fn backend_capabilities(renderer: UiRendererKind) -> &'static UiBackendCapabilities {
    UI_BACKEND_CAPABILITIES
        .iter()
        .find(|entry| entry.renderer == renderer)
        .unwrap_or(&UI_BACKEND_CAPABILITIES[0])
}

/// Scalar runtime value used by UI props and patch payloads.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum UiValue {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
}

impl UiValue {
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::String(value) => Some(value.as_str()),
            _ => None,
        }
    }
}

impl From<&str> for UiValue {
    fn from(value: &str) -> Self {
        Self::String(value.to_string())
    }
}

impl From<String> for UiValue {
    fn from(value: String) -> Self {
        Self::String(value)
    }
}

impl From<bool> for UiValue {
    fn from(value: bool) -> Self {
        Self::Bool(value)
    }
}

impl From<i64> for UiValue {
    fn from(value: i64) -> Self {
        Self::Int(value)
    }
}

impl From<f64> for UiValue {
    fn from(value: f64) -> Self {
        Self::Float(value)
    }
}

/// Semantic widgets the runtime understands before lowering to any host API.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiWidgetKind {
    Element(String),
    ComponentRef(String),
    Text,
    Panel,
    Inspector,
    Graph,
    Timeline,
    Table,
    Tree,
    Viewport2D,
    Viewport3D,
    Overlay,
    Slot,
}

/// Core layout strategies supported by the semantic UI graph.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiLayoutKind {
    Flow,
    FlexRow,
    FlexColumn,
    Grid,
    Dock,
    Stack,
    Absolute,
}

/// Semantic layout specification. Values remain data so backends can map them
/// to the host layout engine without changing authoring semantics.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiLayoutSpec {
    pub kind: UiLayoutKind,
    pub gap: f32,
    pub padding: f32,
    pub min_width: Option<f32>,
    pub min_height: Option<f32>,
}

impl Default for UiLayoutSpec {
    fn default() -> Self {
        Self {
            kind: UiLayoutKind::Flow,
            gap: 0.0,
            padding: 0.0,
            min_width: None,
            min_height: None,
        }
    }
}

/// Named semantic widgets that map authoring tags into explicit runtime
/// widget kinds without baking the mapping into renderer code.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiSemanticWidget {
    Panel,
    Inspector,
    Graph,
    Timeline,
    Table,
    Tree,
    Viewport2D,
    Viewport3D,
    Overlay,
    Slot,
}

/// Declarative tag-to-widget mapping for semantic authoring tags.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct UiSemanticTagProfile {
    pub tag: &'static str,
    pub widget: UiSemanticWidget,
    pub default_layout: UiLayoutKind,
}

pub const UI_SEMANTIC_TAG_PROFILES: &[UiSemanticTagProfile] = &[
    UiSemanticTagProfile {
        tag: "panel",
        widget: UiSemanticWidget::Panel,
        default_layout: UiLayoutKind::FlexColumn,
    },
    UiSemanticTagProfile {
        tag: "inspector",
        widget: UiSemanticWidget::Inspector,
        default_layout: UiLayoutKind::FlexColumn,
    },
    UiSemanticTagProfile {
        tag: "graph",
        widget: UiSemanticWidget::Graph,
        default_layout: UiLayoutKind::Absolute,
    },
    UiSemanticTagProfile {
        tag: "timeline",
        widget: UiSemanticWidget::Timeline,
        default_layout: UiLayoutKind::FlexColumn,
    },
    UiSemanticTagProfile {
        tag: "table",
        widget: UiSemanticWidget::Table,
        default_layout: UiLayoutKind::Grid,
    },
    UiSemanticTagProfile {
        tag: "tree",
        widget: UiSemanticWidget::Tree,
        default_layout: UiLayoutKind::FlexColumn,
    },
    UiSemanticTagProfile {
        tag: "viewport2d",
        widget: UiSemanticWidget::Viewport2D,
        default_layout: UiLayoutKind::Absolute,
    },
    UiSemanticTagProfile {
        tag: "viewport3d",
        widget: UiSemanticWidget::Viewport3D,
        default_layout: UiLayoutKind::Absolute,
    },
    UiSemanticTagProfile {
        tag: "overlay",
        widget: UiSemanticWidget::Overlay,
        default_layout: UiLayoutKind::Stack,
    },
    UiSemanticTagProfile {
        tag: "slot",
        widget: UiSemanticWidget::Slot,
        default_layout: UiLayoutKind::Flow,
    },
];

pub fn semantic_tag_profile(tag: &str) -> Option<&'static UiSemanticTagProfile> {
    UI_SEMANTIC_TAG_PROFILES
        .iter()
        .find(|profile| profile.tag.eq_ignore_ascii_case(tag))
}

pub fn widget_kind_for_tag(tag: &str) -> UiWidgetKind {
    if let Some(profile) = semantic_tag_profile(tag) {
        match profile.widget {
            UiSemanticWidget::Panel => UiWidgetKind::Panel,
            UiSemanticWidget::Inspector => UiWidgetKind::Inspector,
            UiSemanticWidget::Graph => UiWidgetKind::Graph,
            UiSemanticWidget::Timeline => UiWidgetKind::Timeline,
            UiSemanticWidget::Table => UiWidgetKind::Table,
            UiSemanticWidget::Tree => UiWidgetKind::Tree,
            UiSemanticWidget::Viewport2D => UiWidgetKind::Viewport2D,
            UiSemanticWidget::Viewport3D => UiWidgetKind::Viewport3D,
            UiSemanticWidget::Overlay => UiWidgetKind::Overlay,
            UiSemanticWidget::Slot => UiWidgetKind::Slot,
        }
    } else {
        UiWidgetKind::Element(tag.to_string())
    }
}

pub fn default_layout_for_tag(tag: &str) -> UiLayoutSpec {
    if let Some(profile) = semantic_tag_profile(tag) {
        UiLayoutSpec {
            kind: profile.default_layout,
            ..UiLayoutSpec::default()
        }
    } else {
        UiLayoutSpec::default()
    }
}

/// Style tokens and literal overrides. Higher-level styling should compile
/// into this shape rather than coupling authoring to a specific renderer.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiStyleSpec {
    pub tokens: Vec<String>,
    pub values: BTreeMap<String, UiValue>,
}

/// Declarative node in the retained semantic tree.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiNode {
    pub id: UiNodeId,
    pub kind: UiWidgetKind,
    pub props: BTreeMap<String, UiValue>,
    pub children: Vec<UiNodeId>,
    pub layout: UiLayoutSpec,
    pub style: UiStyleSpec,
    pub watches: Vec<UiSignalId>,
}

impl UiNode {
    pub fn new(id: UiNodeId, kind: UiWidgetKind) -> Self {
        Self {
            id,
            kind,
            props: BTreeMap::new(),
            children: Vec::new(),
            layout: UiLayoutSpec::default(),
            style: UiStyleSpec::default(),
            watches: Vec::new(),
        }
    }
}

/// Retained tree state. The runtime owns this graph and emits patches when it
/// changes instead of rebuilding a fresh virtual tree every update.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiTree {
    pub root: Option<UiNodeId>,
    pub nodes: BTreeMap<UiNodeId, UiNode>,
}

impl UiTree {
    pub fn root_node(&self) -> Option<&UiNode> {
        self.root.and_then(|id| self.nodes.get(&id))
    }

    pub fn node(&self, id: UiNodeId) -> Option<&UiNode> {
        self.nodes.get(&id)
    }

    pub fn node_mut(&mut self, id: UiNodeId) -> Option<&mut UiNode> {
        self.nodes.get_mut(&id)
    }
}

/// Patch stream emitted by the UI runtime. Backends consume these commands and
/// translate them into minimal host updates.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum UiPatch {
    SetRoot {
        id: UiNodeId,
    },
    CreateNode {
        node: UiNode,
    },
    DestroyNode {
        id: UiNodeId,
    },
    ReplaceChildren {
        parent: UiNodeId,
        children: Vec<UiNodeId>,
    },
    InsertChild {
        parent: UiNodeId,
        index: usize,
        child: UiNodeId,
    },
    RemoveChild {
        parent: UiNodeId,
        child: UiNodeId,
    },
    SetProp {
        id: UiNodeId,
        key: String,
        value: UiValue,
    },
    SetStyle {
        id: UiNodeId,
        style: UiStyleSpec,
    },
    SetLayout {
        id: UiNodeId,
        layout: UiLayoutSpec,
    },
}

/// Build result used when lowering authoring/runtime structures into a
/// retained `UiTree`.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiBuildOutput {
    pub tree: UiTree,
    pub patches: Vec<UiPatch>,
}

/// Flat raw-native projection of the semantic tree for runtimes that do not yet
/// consume the full retained tree format directly.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiNativeProjection {
    pub root_id: Option<u64>,
    pub primary_panel_title: Option<String>,
    pub primary_viewport_title: Option<String>,
    pub primary_viewport_scene: Option<String>,
    pub nodes: Vec<UiNativeProjectionNode>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiNativeProjectionNode {
    pub id: u64,
    pub parent_id: Option<u64>,
    pub depth: u32,
    pub kind: UiNativeProjectionKind,
    pub title: Option<String>,
    pub text: Option<String>,
    pub tag: Option<String>,
    pub scene: Option<String>,
    pub layout_kind: UiLayoutKind,
    pub child_count: usize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiNativeProjectionKind {
    Element,
    ComponentRef,
    Text,
    Panel,
    Inspector,
    Graph,
    Timeline,
    Table,
    Tree,
    Viewport2D,
    Viewport3D,
    Overlay,
    Slot,
}

/// Stable runtime bundle schema version for compiled KAIN UI apps.
pub const UI_RUNTIME_BUNDLE_SCHEMA_VERSION: u32 = 1;

/// Backend-agnostic metadata for a compiled KAIN UI runtime bundle.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeMetadata {
    pub app_name: Option<String>,
    pub window_title: String,
    pub root_component: String,
    pub source_file_name: Option<String>,
    pub initial_window_size: [f32; 2],
}

/// Serialized ABI boundary between the KAIN compiler and host runtimes.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeBundle {
    pub schema_version: u32,
    pub metadata: UiRuntimeMetadata,
    pub output: UiBuildOutput,
    #[serde(default)]
    pub native_projection: UiNativeProjection,
}

pub fn ui_runtime_bundle_from_output(
    metadata: UiRuntimeMetadata,
    output: UiBuildOutput,
) -> UiRuntimeBundle {
    let native_projection = ui_native_projection_from_output(&output);
    UiRuntimeBundle {
        schema_version: UI_RUNTIME_BUNDLE_SCHEMA_VERSION,
        metadata,
        output,
        native_projection,
    }
}

pub fn ui_runtime_bundle_to_json(bundle: &UiRuntimeBundle) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(bundle)
}

pub fn ui_runtime_bundle_from_json(json: &str) -> Result<UiRuntimeBundle, serde_json::Error> {
    serde_json::from_str(json)
}

pub fn validate_ui_runtime_bundle(bundle: &UiRuntimeBundle) -> Result<(), IoError> {
    if bundle.schema_version != UI_RUNTIME_BUNDLE_SCHEMA_VERSION {
        return Err(IoError::new(
            ErrorKind::InvalidData,
            format!(
                "Unsupported KAIN UI runtime bundle schema version {} (expected {})",
                bundle.schema_version, UI_RUNTIME_BUNDLE_SCHEMA_VERSION
            ),
        ));
    }

    if bundle.output.tree.root.is_none() {
        return Err(IoError::new(
            ErrorKind::InvalidData,
            "Compiled KAIN UI runtime bundle did not contain a root node",
        ));
    }

    Ok(())
}

pub fn ui_native_projection_from_output(output: &UiBuildOutput) -> UiNativeProjection {
    let mut projection = UiNativeProjection {
        root_id: output.tree.root.map(|id| id.0),
        ..UiNativeProjection::default()
    };

    let Some(root_id) = output.tree.root else {
        return projection;
    };

    collect_native_projection_nodes(&output.tree, root_id, None, 0, &mut projection);
    projection
}

fn collect_native_projection_nodes(
    tree: &UiTree,
    id: UiNodeId,
    parent_id: Option<UiNodeId>,
    depth: u32,
    projection: &mut UiNativeProjection,
) {
    let Some(node) = tree.node(id) else {
        return;
    };

    let title = node_prop_string(node, "title");
    let text = node_prop_string(node, "text");
    let scene = node_prop_string(node, "scene");
    let tag = node_prop_string(node, "tag");
    let kind = UiNativeProjectionKind::from_widget_kind(&node.kind);

    if projection.primary_panel_title.is_none() && kind == UiNativeProjectionKind::Panel {
        projection.primary_panel_title = title.clone();
    }
    if projection.primary_viewport_title.is_none() && kind == UiNativeProjectionKind::Viewport3D {
        projection.primary_viewport_title = title.clone();
    }
    if projection.primary_viewport_scene.is_none() && kind == UiNativeProjectionKind::Viewport3D {
        projection.primary_viewport_scene = scene.clone();
    }

    projection.nodes.push(UiNativeProjectionNode {
        id: id.0,
        parent_id: parent_id.map(|value| value.0),
        depth,
        kind,
        title,
        text,
        tag,
        scene,
        layout_kind: node.layout.kind,
        child_count: node.children.len(),
    });

    for child in &node.children {
        collect_native_projection_nodes(tree, *child, Some(id), depth + 1, projection);
    }
}

fn node_prop_string(node: &UiNode, key: &str) -> Option<String> {
    node.props.get(key).and_then(|value| match value {
        UiValue::String(value) => Some(value.clone()),
        _ => None,
    })
}

impl UiNativeProjectionKind {
    fn from_widget_kind(kind: &UiWidgetKind) -> Self {
        match kind {
            UiWidgetKind::Element(_) => Self::Element,
            UiWidgetKind::ComponentRef(_) => Self::ComponentRef,
            UiWidgetKind::Text => Self::Text,
            UiWidgetKind::Panel => Self::Panel,
            UiWidgetKind::Inspector => Self::Inspector,
            UiWidgetKind::Graph => Self::Graph,
            UiWidgetKind::Timeline => Self::Timeline,
            UiWidgetKind::Table => Self::Table,
            UiWidgetKind::Tree => Self::Tree,
            UiWidgetKind::Viewport2D => Self::Viewport2D,
            UiWidgetKind::Viewport3D => Self::Viewport3D,
            UiWidgetKind::Overlay => Self::Overlay,
            UiWidgetKind::Slot => Self::Slot,
        }
    }
}

/// Builder that assigns stable ids, retains nodes, and emits patch commands
/// while the semantic graph is constructed.
#[derive(Clone, Debug, Default)]
pub struct UiTreeBuilder {
    next_node_id: u64,
    tree: UiTree,
    patches: Vec<UiPatch>,
}

impl UiTreeBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn alloc_id(&mut self) -> UiNodeId {
        self.next_node_id += 1;
        UiNodeId(self.next_node_id)
    }

    pub fn add_node(&mut self, node: UiNode) {
        self.tree.nodes.insert(node.id, node.clone());
        self.patches.push(UiPatch::CreateNode { node });
    }

    pub fn set_root(&mut self, id: UiNodeId) {
        self.tree.root = Some(id);
        self.patches.push(UiPatch::SetRoot { id });
    }

    pub fn replace_children(&mut self, parent: UiNodeId, children: Vec<UiNodeId>) {
        if let Some(node) = self.tree.node_mut(parent) {
            node.children = children.clone();
        }
        self.patches
            .push(UiPatch::ReplaceChildren { parent, children });
    }

    pub fn set_prop(&mut self, id: UiNodeId, key: impl Into<String>, value: UiValue) {
        let key = key.into();
        if let Some(node) = self.tree.node_mut(id) {
            node.props.insert(key.clone(), value.clone());
        }
        self.patches.push(UiPatch::SetProp { id, key, value });
    }

    pub fn set_layout(&mut self, id: UiNodeId, layout: UiLayoutSpec) {
        if let Some(node) = self.tree.node_mut(id) {
            node.layout = layout.clone();
        }
        self.patches.push(UiPatch::SetLayout { id, layout });
    }

    pub fn set_style(&mut self, id: UiNodeId, style: UiStyleSpec) {
        if let Some(node) = self.tree.node_mut(id) {
            node.style = style.clone();
        }
        self.patches.push(UiPatch::SetStyle { id, style });
    }

    pub fn finish(self) -> UiBuildOutput {
        UiBuildOutput {
            tree: self.tree,
            patches: self.patches,
        }
    }
}

/// Debug helper for inspecting the semantic tree without a renderer.
pub fn render_debug_tree(tree: &UiTree) -> String {
    let Some(root) = tree.root else {
        return "<empty-ui-tree>".to_string();
    };

    let mut out = Vec::new();
    render_debug_tree_node(tree, root, 0, &mut out);
    out.join("\n")
}

fn render_debug_tree_node(tree: &UiTree, id: UiNodeId, depth: usize, out: &mut Vec<String>) {
    let Some(node) = tree.node(id) else {
        out.push(format!("{}<missing {:?}>", "  ".repeat(depth), id));
        return;
    };

    let props = if node.props.is_empty() {
        String::new()
    } else {
        let props = node
            .props
            .iter()
            .map(|(key, value)| format!("{key}={value:?}"))
            .collect::<Vec<_>>()
            .join(", ");
        format!(" {{{props}}}")
    };

    out.push(format!(
        "{}{:?}#{:?}{}",
        "  ".repeat(depth),
        node.kind,
        node.id.0,
        props
    ));

    for child in &node.children {
        render_debug_tree_node(tree, *child, depth + 1, out);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn backend_capabilities_are_data_driven() {
        let native = backend_capabilities(UiRendererKind::Native);
        let web = backend_capabilities(UiRendererKind::Web);

        assert!(native.supports_real_windowing);
        assert!(native.supports_docking);
        assert!(web.supports_dom_embedding);
        assert!(!web.supports_real_windowing);
    }

    #[test]
    fn semantic_tag_profiles_drive_widget_selection() {
        assert_eq!(widget_kind_for_tag("panel"), UiWidgetKind::Panel);
        assert_eq!(widget_kind_for_tag("viewport3d"), UiWidgetKind::Viewport3D);
        assert_eq!(
            widget_kind_for_tag("button"),
            UiWidgetKind::Element("button".to_string())
        );
        assert_eq!(
            default_layout_for_tag("panel").kind,
            UiLayoutKind::FlexColumn
        );
    }

    #[test]
    fn ui_tree_builder_tracks_nodes_and_patches() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let child_id = builder.alloc_id();

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut child = UiNode::new(child_id, UiWidgetKind::Inspector);
        child
            .props
            .insert("title".to_string(), UiValue::from("Details"));
        child.watches.push(UiSignalId(7));
        builder.add_node(child);
        builder.replace_children(root_id, vec![child_id]);
        builder.set_root(root_id);

        let build = builder.finish();

        assert_eq!(build.tree.root, Some(root_id));
        assert_eq!(build.tree.nodes[&root_id].children, vec![child_id]);
        assert_eq!(
            build.tree.nodes[&child_id].props.get("title"),
            Some(&UiValue::String("Details".to_string()))
        );
        assert!(build
            .patches
            .iter()
            .any(|patch| matches!(patch, UiPatch::SetRoot { id } if *id == root_id)));
    }

    #[test]
    fn debug_tree_renders_hierarchy() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let text_id = builder.alloc_id();

        builder.add_node(UiNode::new(root_id, UiWidgetKind::Panel));

        let mut text = UiNode::new(text_id, UiWidgetKind::Text);
        text.props
            .insert("text".to_string(), UiValue::from("hello"));
        builder.add_node(text);
        builder.replace_children(root_id, vec![text_id]);
        builder.set_root(root_id);

        let build = builder.finish();
        let rendered = render_debug_tree(&build.tree);

        assert!(rendered.contains("Panel"));
        assert!(rendered.contains("Text"));
        assert!(rendered.contains("hello"));
    }

    #[test]
    fn runtime_bundle_round_trip_preserves_ui_output() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        builder.add_node(UiNode::new(root_id, UiWidgetKind::Panel));
        builder.set_root(root_id);
        let output = builder.finish();

        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                app_name: Some("studio-shell".to_string()),
                window_title: "Studio Shell".to_string(),
                root_component: "App".to_string(),
                source_file_name: Some("studio.kn".to_string()),
                initial_window_size: [1600.0, 900.0],
            },
            output.clone(),
        );

        let json = ui_runtime_bundle_to_json(&bundle).expect("bundle should serialize");
        let decoded = ui_runtime_bundle_from_json(&json).expect("bundle should deserialize");

        assert_eq!(decoded.schema_version, UI_RUNTIME_BUNDLE_SCHEMA_VERSION);
        assert_eq!(decoded.metadata.root_component, "App");
        assert_eq!(decoded.output, output);
        assert_eq!(decoded.native_projection.root_id, Some(root_id.0));
        validate_ui_runtime_bundle(&decoded).expect("bundle should validate");
    }

    #[test]
    fn runtime_bundle_validation_requires_root_node() {
        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                app_name: None,
                window_title: "Invalid".to_string(),
                root_component: "App".to_string(),
                source_file_name: None,
                initial_window_size: [1440.0, 920.0],
            },
            UiBuildOutput::default(),
        );

        let err = validate_ui_runtime_bundle(&bundle).expect_err("bundle should be rejected");
        assert_eq!(err.kind(), ErrorKind::InvalidData);
    }

    #[test]
    fn native_projection_collects_panel_and_viewport_metadata() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let viewport_id = builder.alloc_id();

        let mut panel = UiNode::new(root_id, UiWidgetKind::Panel);
        panel
            .props
            .insert("title".to_string(), UiValue::from("Viewport Lab"));
        builder.add_node(panel);

        let mut viewport = UiNode::new(viewport_id, UiWidgetKind::Viewport3D);
        viewport
            .props
            .insert("title".to_string(), UiValue::from("Hero View"));
        viewport
            .props
            .insert("scene".to_string(), UiValue::from("luminous_port"));
        builder.add_node(viewport);
        builder.replace_children(root_id, vec![viewport_id]);
        builder.set_root(root_id);

        let output = builder.finish();
        let projection = ui_native_projection_from_output(&output);

        assert_eq!(projection.root_id, Some(root_id.0));
        assert_eq!(projection.primary_panel_title.as_deref(), Some("Viewport Lab"));
        assert_eq!(projection.primary_viewport_title.as_deref(), Some("Hero View"));
        assert_eq!(
            projection.primary_viewport_scene.as_deref(),
            Some("luminous_port")
        );
        assert_eq!(projection.nodes.len(), 2);
        assert!(projection
            .nodes
            .iter()
            .any(|node| node.kind == UiNativeProjectionKind::Viewport3D));
    }
}
