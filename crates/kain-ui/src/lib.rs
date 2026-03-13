//! KAIN UI runtime and semantic interface graph.
//!
//! `kain-ui` is the home for KAIN's native-first UI runtime model. The crate
//! focuses on semantic nodes, retained tree state, renderer capability tables,
//! and patch streams instead of a virtual DOM-first execution model.

use std::collections::BTreeMap;

/// Stable identifier for a node within a retained UI tree.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct UiNodeId(pub u64);

/// Stable identifier for a reactive signal.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct UiSignalId(pub u64);

/// Supported renderer families for KAIN UI.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum UiRendererKind {
    Native,
    Web,
    Slate,
    Debug,
}

/// Declarative backend capability profile.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
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
#[derive(Clone, Debug, PartialEq)]
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
#[derive(Clone, Debug, PartialEq, Eq)]
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
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
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
#[derive(Clone, Debug, PartialEq)]
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
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
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
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
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
#[derive(Clone, Debug, Default, PartialEq)]
pub struct UiStyleSpec {
    pub tokens: Vec<String>,
    pub values: BTreeMap<String, UiValue>,
}

/// Declarative node in the retained semantic tree.
#[derive(Clone, Debug, PartialEq)]
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
#[derive(Clone, Debug, Default, PartialEq)]
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
#[derive(Clone, Debug, PartialEq)]
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
#[derive(Clone, Debug, Default, PartialEq)]
pub struct UiBuildOutput {
    pub tree: UiTree,
    pub patches: Vec<UiPatch>,
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
}
