//! KAIN UI runtime and semantic interface graph.
//!
//! `kain-ui` is the home for KAIN's native-first UI runtime model. The crate
//! focuses on semantic nodes, retained tree state, renderer capability tables,
//! and patch streams instead of a virtual DOM-first execution model.

mod runtime_execution;

use std::{
    collections::{BTreeMap, BTreeSet},
    io::{Error as IoError, ErrorKind},
};

use serde::{Deserialize, Serialize};

pub use runtime_execution::*;

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

/// Canonical layout engines that can execute KAIN semantic layout intent.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiLayoutEngineKind {
    #[default]
    Auto,
    Native,
    Yoga,
    LegacyEgui,
}

/// Canonical render engines that can paint KAIN semantic surfaces.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiRenderEngineKind {
    #[default]
    Auto,
    Native,
    Skia,
    Wgpu,
    Shader,
    Browser,
    LegacyEgui,
}

/// Host backends that can present KAIN semantic sessions.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiHostBackendKind {
    #[default]
    Auto,
    Native,
    LegacyEgui,
    Imgui,
    RmlUi,
    Slint,
    Qt,
    Cef,
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

/// Role-specific host backend capability profile.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct UiHostBackendCapabilities {
    pub host_backend: UiHostBackendKind,
    pub layout_engine: UiLayoutEngineKind,
    pub render_engine: UiRenderEngineKind,
    pub supports_real_windowing: bool,
    pub supports_dom_embedding: bool,
    pub supports_gpu_viewports: bool,
    pub supports_docking: bool,
    pub supports_rich_text: bool,
    pub supports_pointer_capture: bool,
    pub supports_accessibility_tree: bool,
}

pub const UI_HOST_BACKEND_CAPABILITIES: &[UiHostBackendCapabilities] = &[
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::LegacyEgui,
        layout_engine: UiLayoutEngineKind::LegacyEgui,
        render_engine: UiRenderEngineKind::LegacyEgui,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: false,
    },
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::Imgui,
        layout_engine: UiLayoutEngineKind::Yoga,
        render_engine: UiRenderEngineKind::Skia,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: false,
    },
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::RmlUi,
        layout_engine: UiLayoutEngineKind::Yoga,
        render_engine: UiRenderEngineKind::Skia,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: false,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: false,
    },
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::Slint,
        layout_engine: UiLayoutEngineKind::Yoga,
        render_engine: UiRenderEngineKind::Native,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: false,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: true,
    },
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::Qt,
        layout_engine: UiLayoutEngineKind::Yoga,
        render_engine: UiRenderEngineKind::Skia,
        supports_real_windowing: true,
        supports_dom_embedding: false,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: true,
    },
    UiHostBackendCapabilities {
        host_backend: UiHostBackendKind::Cef,
        layout_engine: UiLayoutEngineKind::Auto,
        render_engine: UiRenderEngineKind::Browser,
        supports_real_windowing: false,
        supports_dom_embedding: true,
        supports_gpu_viewports: true,
        supports_docking: true,
        supports_rich_text: true,
        supports_pointer_capture: true,
        supports_accessibility_tree: true,
    },
];

pub fn host_backend_capabilities(
    host_backend: UiHostBackendKind,
) -> &'static UiHostBackendCapabilities {
    UI_HOST_BACKEND_CAPABILITIES
        .iter()
        .find(|entry| entry.host_backend == host_backend)
        .unwrap_or(&UI_HOST_BACKEND_CAPABILITIES[0])
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

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum UiLengthUnit {
    Auto,
    Px,
    Percent,
    Fr,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiLength {
    pub value: f32,
    pub unit: UiLengthUnit,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiLayoutAlignment {
    Start,
    Center,
    End,
    Stretch,
    SpaceBetween,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiOverflowBehavior {
    Visible,
    Hidden,
    Scroll,
    Auto,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiDockPlacement {
    Center,
    Left,
    Right,
    Top,
    Bottom,
    Tab,
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
    pub width: Option<UiLength>,
    pub height: Option<UiLength>,
    pub max_width: Option<f32>,
    pub max_height: Option<f32>,
    pub flex_grow: f32,
    pub flex_shrink: f32,
    pub align_items: UiLayoutAlignment,
    pub justify_content: UiLayoutAlignment,
    pub overflow_x: UiOverflowBehavior,
    pub overflow_y: UiOverflowBehavior,
    pub dock: Option<UiDockPlacement>,
    pub split_ratio: Option<f32>,
    pub resizable: bool,
    pub persistent_layout_id: Option<String>,
    #[serde(default)]
    pub tab_group_id: Option<String>,
    #[serde(default)]
    pub tab_label: Option<String>,
    #[serde(default)]
    pub tab_order: Option<i32>,
    #[serde(default)]
    pub tab_default_active: bool,
    #[serde(default)]
    pub tab_closable: bool,
}

impl Default for UiLayoutSpec {
    fn default() -> Self {
        Self {
            kind: UiLayoutKind::Flow,
            gap: 0.0,
            padding: 0.0,
            min_width: None,
            min_height: None,
            width: None,
            height: None,
            max_width: None,
            max_height: None,
            flex_grow: 0.0,
            flex_shrink: 1.0,
            align_items: UiLayoutAlignment::Start,
            justify_content: UiLayoutAlignment::Start,
            overflow_x: UiOverflowBehavior::Visible,
            overflow_y: UiOverflowBehavior::Visible,
            dock: None,
            split_ratio: None,
            resizable: false,
            persistent_layout_id: None,
            tab_group_id: None,
            tab_label: None,
            tab_order: None,
            tab_default_active: false,
            tab_closable: false,
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
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiStyleState {
    Hovered,
    Active,
    Focused,
    Disabled,
    Selected,
    Dragging,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiStyleSpec {
    pub tokens: Vec<String>,
    pub values: BTreeMap<String, UiValue>,
    pub classes: Vec<String>,
    pub theme_scope: Option<String>,
    pub variant: Option<String>,
    pub states: Vec<UiStyleState>,
}

/// Declarative node in the retained semantic tree.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiNode {
    pub id: UiNodeId,
    pub identity_key: Option<String>,
    pub kind: UiWidgetKind,
    pub props: BTreeMap<String, UiValue>,
    pub children: Vec<UiNodeId>,
    pub layout: UiLayoutSpec,
    pub style: UiStyleSpec,
    pub watches: Vec<UiSignalId>,
    pub focus_scope: Option<String>,
    pub selection_scope: Option<String>,
}

impl UiNode {
    pub fn new(id: UiNodeId, kind: UiWidgetKind) -> Self {
        Self {
            id,
            identity_key: None,
            kind,
            props: BTreeMap::new(),
            children: Vec::new(),
            layout: UiLayoutSpec::default(),
            style: UiStyleSpec::default(),
            watches: Vec::new(),
            focus_scope: None,
            selection_scope: None,
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
    #[serde(default)]
    pub systems: UiRuntimeSystems,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiComputed {
    pub id: String,
    pub label: String,
    pub depends_on: Vec<UiSignalId>,
    /// Optional derived-signal output written by this computed entry.
    #[serde(default)]
    pub writes_signal: Option<UiSignalId>,
    /// Optional derived expression evaluated by the runtime when inputs change.
    ///
    /// This is intentionally small and backend-neutral; it exists so derived values can
    /// be compiler-emitted and runtime-executed without host-local invention.
    #[serde(default)]
    pub expr: Option<UiDerivedExpr>,
    pub invalidates_nodes: Vec<UiNodeId>,
    pub scheduler_phase: UiSchedulerPhase,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum UiDerivedExpr {
    Literal(UiValue),
    Signal(UiSignalId),
    Not(Box<UiDerivedExpr>),
    And(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Or(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Eq(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Add(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Sub(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Mul(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    Div(Box<UiDerivedExpr>, Box<UiDerivedExpr>),
    ToString(Box<UiDerivedExpr>),
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiResource {
    pub id: String,
    pub kind: String,
    pub owner: Option<UiNodeId>,
    pub state: UiResourceState,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiResourceState {
    Idle,
    Loading,
    Ready,
    Failed,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiTransaction {
    pub label: String,
    /// Runtime-generated transaction id (monotonic within a runtime instance).
    #[serde(default)]
    pub id: u64,
    /// Number of semantic-tree patches emitted during this transaction.
    #[serde(default)]
    pub patch_count: usize,
    pub touched_nodes: Vec<UiNodeId>,
    pub changed_signals: Vec<UiSignalId>,
    /// Commands dispatched during this transaction (backend may execute externally).
    #[serde(default)]
    pub dispatched_commands: Vec<String>,
}

impl UiTransaction {
    pub fn default_runtime(id: u64) -> Self {
        Self {
            label: String::new(),
            id,
            patch_count: 0,
            touched_nodes: Vec::new(),
            changed_signals: Vec::new(),
            dispatched_commands: Vec::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiFocusGraph {
    pub scopes: Vec<String>,
    pub default_scope: Option<String>,
    /// Current focus owner per scope.
    #[serde(default)]
    pub focused: BTreeMap<String, UiNodeId>,
    /// Explicit focus traversal edges (Tab, arrows, etc) when the runtime/compiler provides them.
    #[serde(default)]
    pub traversal_edges: Vec<UiFocusTraversalEdge>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiFocusTraversalKind {
    TabForward,
    TabBackward,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Next,
    Prev,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiFocusTraversalEdge {
    pub scope: String,
    pub kind: UiFocusTraversalKind,
    pub from: UiNodeId,
    pub to: UiNodeId,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiEventPhase {
    /// Default event route behavior (bubble up through parents).
    #[default]
    Bubble,
    /// Capture phase (root down to target).
    Capture,
    /// Direct dispatch to the target only.
    Direct,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiEventRoute {
    /// Stable route id (useful for patch traces and debugging).
    #[serde(default)]
    pub route_id: String,
    pub event: String,
    pub target: UiNodeId,
    #[serde(default)]
    pub phase: UiEventPhase,
    /// Optional handler id emitted by the compiler/authoring layer.
    #[serde(default)]
    pub handler_id: Option<String>,
    /// Optional command name to dispatch when this route fires.
    #[serde(default)]
    pub dispatch_command: Option<String>,
    /// Optional transaction label to stamp on the resulting runtime command.
    #[serde(default)]
    pub transaction_label: Option<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiAnimationTrigger {
    Mount,
    Unmount,
    SignalChange,
    Hover,
    Focus,
    LayoutChange,
    Reload,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiEasingKind {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Spring,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiAnimationTrack {
    pub id: String,
    pub target: UiNodeId,
    pub property: String,
    pub duration_ms: u32,
    pub trigger: UiAnimationTrigger,
    pub easing: UiEasingKind,
    pub preserve_on_reload: bool,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiMotionMode {
    #[default]
    Full,
    Balanced,
    Reduced,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiPerformanceTier {
    #[default]
    Warp,
    Cruise,
    Safe,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiCapacitorState {
    #[default]
    Charged,
    Priming,
    Discharged,
}

/// Runtime motion policy. This is intended to be stable, inspectable truth:
/// motion changes are not ad-hoc booleans on random widgets.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiMotionPolicy {
    pub mode: UiMotionMode,
    pub performance_tier: UiPerformanceTier,
    pub is_resizing: bool,
    pub is_pointer_active: bool,
    pub capacitor: UiCapacitorState,
}

impl UiMotionPolicy {
    pub fn recompute_capacitor(&mut self) {
        self.capacitor = match self.mode {
            UiMotionMode::Reduced => UiCapacitorState::Discharged,
            _ if self.is_resizing => UiCapacitorState::Priming,
            _ if self.performance_tier == UiPerformanceTier::Safe => UiCapacitorState::Priming,
            _ if self.is_pointer_active => UiCapacitorState::Charged,
            _ => {
                if self.performance_tier == UiPerformanceTier::Warp {
                    UiCapacitorState::Charged
                } else {
                    UiCapacitorState::Priming
                }
            }
        };
    }

    pub fn should_animate(&self) -> bool {
        self.mode != UiMotionMode::Reduced
            && self.performance_tier != UiPerformanceTier::Safe
            && !self.is_resizing
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiSurface {
    pub id: String,
    pub kind: UiSurfaceKind,
    pub node: UiNodeId,
    pub title: Option<String>,
    #[serde(default)]
    pub renderer_preference: UiSurfaceRendererPreference,
    #[serde(default)]
    pub composition_mode: UiSurfaceCompositionMode,
    #[serde(default)]
    pub preferred_host_backend: UiHostBackendKind,
    #[serde(default)]
    pub preferred_layout_engine: UiLayoutEngineKind,
    #[serde(default)]
    pub preferred_render_engine: UiRenderEngineKind,
    #[serde(default)]
    pub gpu_backing_required: bool,
    #[serde(default)]
    pub shader: Option<UiSurfaceShaderBinding>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiSurfaceKind {
    Canvas,
    Graph,
    Timeline,
    Table,
    Tree,
    Viewport2D,
    Viewport3D,
    Overlay,
    Custom(String),
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiSurfaceRendererPreference {
    #[default]
    Auto,
    Native,
    Dom,
    Wgpu,
    Shader,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiSurfaceCompositionMode {
    #[default]
    Host,
    LayeredGpu,
    Viewport,
    ShaderCanvas,
}

#[derive(Clone, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiSurfaceShaderBinding {
    pub shader_ref: String,
    pub entry_point: Option<String>,
    pub stage: Option<String>,
    pub derived_format: Option<String>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiAnchorPlacement {
    #[default]
    Auto,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
    Center,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum UiAnchorConstraint {
    #[default]
    ClampToViewport,
    AllowOverflow,
}

/// Anchor definition for overlays/popovers/menus/tooltips.
///
/// This is backend-neutral spatial intent. Backends can still choose the final
/// pixel placement, but the relationship must be inspectable and stable.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiAnchorSpec {
    pub target: UiNodeId,
    #[serde(default)]
    pub placement: UiAnchorPlacement,
    #[serde(default)]
    pub offset_px: [f32; 2],
    #[serde(default)]
    pub constraint: UiAnchorConstraint,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiOverlayEntry {
    pub id: String,
    pub node: UiNodeId,
    /// Higher orders draw above lower orders.
    #[serde(default)]
    pub order: i32,
    #[serde(default)]
    pub anchor: Option<UiAnchorSpec>,
    #[serde(default)]
    pub modal: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiOverlayStack {
    pub entries: Vec<UiOverlayEntry>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiSchedulerPhase {
    Signals,
    Resources,
    Layout,
    Animation,
    Patches,
    Effects,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiSchedulerEntry {
    pub phase: UiSchedulerPhase,
    pub label: String,
    pub target_nodes: Vec<UiNodeId>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiScheduler {
    pub phases: Vec<UiSchedulerPhase>,
    pub pending: Vec<UiSchedulerEntry>,
}

impl Default for UiScheduler {
    fn default() -> Self {
        Self {
            phases: vec![
                UiSchedulerPhase::Signals,
                UiSchedulerPhase::Resources,
                UiSchedulerPhase::Layout,
                UiSchedulerPhase::Animation,
                UiSchedulerPhase::Patches,
                UiSchedulerPhase::Effects,
            ],
            pending: Vec::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiSelectionModel {
    pub scopes: Vec<String>,
    pub active_scope: Option<String>,
    /// Primary selection per scope (single node).
    #[serde(default)]
    pub primary: BTreeMap<String, UiNodeId>,
    /// Multi-selection per scope.
    #[serde(default)]
    pub selected: BTreeMap<String, BTreeSet<UiNodeId>>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiCommand {
    pub name: String,
    pub target: Option<UiNodeId>,
    pub payload: BTreeMap<String, UiValue>,
    #[serde(default)]
    pub transaction_label: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiCommandRejection {
    pub command_name: String,
    pub reason: String,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiCommandBuffer {
    pub pending: Vec<UiCommand>,
    #[serde(default)]
    pub executed: Vec<UiCommand>,
    #[serde(default)]
    pub rejections: Vec<UiCommandRejection>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum UiCommandEffectKind {
    /// Runtime-owned mutation of the semantic/runtime graph.
    RuntimeMutation,
    /// External effect executed by the embedding host (file IO, process launch, etc).
    ExternalEffect,
}

impl Default for UiCommandEffectKind {
    fn default() -> Self {
        Self::RuntimeMutation
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiCommandDescriptor {
    pub name: String,
    pub label: String,
    #[serde(default)]
    pub description: Option<String>,
    #[serde(default)]
    pub keywords: Vec<String>,
    #[serde(default)]
    pub category: Option<String>,
    #[serde(default)]
    pub shortcut: Option<String>,
    #[serde(default)]
    pub effect: UiCommandEffectKind,
}

/// Data-driven command registry that survives authoring, bundling, runtime, and backend realization.
///
/// Inspired by the explicit, source-owned registration model used in K_OS.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiCommandRegistry {
    pub commands_by_source: BTreeMap<String, Vec<UiCommandDescriptor>>,
    pub snapshot: Vec<UiCommandDescriptor>,
}

impl UiCommandRegistry {
    pub fn rebuild_snapshot(&mut self) {
        let mut all = Vec::new();
        for commands in self.commands_by_source.values() {
            all.extend(commands.iter().cloned());
        }
        all.sort_by(|a, b| a.name.cmp(&b.name));
        self.snapshot = all;
    }
}

pub fn ui_register_command_descriptors(
    registry: &mut UiCommandRegistry,
    source_id: &str,
    commands: Vec<UiCommandDescriptor>,
) {
    registry
        .commands_by_source
        .insert(source_id.to_string(), commands);
    registry.rebuild_snapshot();
}

pub fn ui_unregister_command_descriptors(registry: &mut UiCommandRegistry, source_id: &str) {
    if registry.commands_by_source.remove(source_id).is_some() {
        registry.rebuild_snapshot();
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiThemeToken {
    pub name: String,
    pub category: String,
    pub value: UiValue,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiThemeVariant {
    pub scope: String,
    pub name: String,
    pub tokens: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiThemeScope {
    pub name: String,
    pub selector: String,
    pub parent: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiThemeRegistry {
    pub active_theme: Option<String>,
    pub scopes: Vec<UiThemeScope>,
    pub semantic_tokens: Vec<UiThemeToken>,
    pub variants: Vec<UiThemeVariant>,
    pub diff_keys: Vec<String>,
}

impl Default for UiThemeRegistry {
    fn default() -> Self {
        Self {
            active_theme: Some("default".to_string()),
            scopes: Vec::new(),
            semantic_tokens: Vec::new(),
            variants: Vec::new(),
            diff_keys: Vec::new(),
        }
    }
}

impl UiThemeRegistry {
    fn is_empty(&self) -> bool {
        self.scopes.is_empty()
            && self.semantic_tokens.is_empty()
            && self.variants.is_empty()
            && self.diff_keys.is_empty()
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiDockNode {
    pub id: String,
    pub node: UiNodeId,
    pub placement: UiDockPlacement,
    pub split_ratio: Option<f32>,
    pub children: Vec<UiNodeId>,
    pub persistent_layout_id: Option<String>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiWorkspaceLayout {
    pub roots: Vec<UiDockNode>,
    pub persistence_key: Option<String>,
    pub virtualization_enabled: bool,
    #[serde(default)]
    pub active_tabs: BTreeMap<String, String>,
}

impl UiWorkspaceLayout {
    fn is_empty(&self) -> bool {
        self.roots.is_empty()
            && self.persistence_key.is_none()
            && !self.virtualization_enabled
            && self.active_tabs.is_empty()
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiReloadIdentityAlias {
    pub from: String,
    pub to: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiHotReloadNodeLink {
    pub stable_identity: String,
    pub previous_node: Option<UiNodeId>,
    pub next_node: Option<UiNodeId>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiHotReloadSignalLink {
    pub signal_key: String,
    pub previous_signal: Option<UiSignalId>,
    pub next_signal: Option<UiSignalId>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiHotReloadOverlayLink {
    pub overlay_id: String,
    pub previous_node: Option<UiNodeId>,
    pub next_node: Option<UiNodeId>,
    pub previous_anchor_target: Option<UiNodeId>,
    pub next_anchor_target: Option<UiNodeId>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiHotReloadPlan {
    pub preserve_focus: bool,
    pub preserve_selection: bool,
    pub preserve_docking: bool,
    pub preserve_overlays: bool,
    pub preserve_motion_policy: bool,
    pub preserve_animation_state: bool,
    pub preserve_signal_values: bool,
    pub preserve_session_state: bool,
    pub identity_aliases: Vec<UiReloadIdentityAlias>,
}

impl Default for UiHotReloadPlan {
    fn default() -> Self {
        Self {
            preserve_focus: true,
            preserve_selection: true,
            preserve_docking: true,
            preserve_overlays: true,
            preserve_motion_policy: true,
            preserve_animation_state: true,
            preserve_signal_values: true,
            preserve_session_state: true,
            identity_aliases: Vec::new(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiAnimationPlaybackState {
    pub elapsed_ms: u32,
    pub progress: f32,
    pub eased_progress: f32,
    pub completed: bool,
}

impl Default for UiAnimationPlaybackState {
    fn default() -> Self {
        Self {
            elapsed_ms: 0,
            progress: 0.0,
            eased_progress: 0.0,
            completed: false,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct UiRuntimeSystems {
    pub computed: Vec<UiComputed>,
    pub resources: Vec<UiResource>,
    pub transactions: Vec<UiTransaction>,
    pub focus_graph: UiFocusGraph,
    pub event_routes: Vec<UiEventRoute>,
    pub animation_tracks: Vec<UiAnimationTrack>,
    pub motion_policy: UiMotionPolicy,
    pub surfaces: Vec<UiSurface>,
    pub overlay_stack: UiOverlayStack,
    pub scheduler: UiScheduler,
    pub selection_model: UiSelectionModel,
    pub command_buffer: UiCommandBuffer,
    pub command_registry: UiCommandRegistry,
    pub theme_registry: UiThemeRegistry,
    pub workspace_layout: UiWorkspaceLayout,
    pub hot_reload: UiHotReloadPlan,
    pub signal_values: BTreeMap<UiSignalId, UiValue>,
    pub animation_state: BTreeMap<String, UiAnimationPlaybackState>,
    pub session_state: BTreeMap<String, UiValue>,
}

impl UiRuntimeSystems {
    pub fn is_empty(&self) -> bool {
        self.computed.is_empty()
            && self.resources.is_empty()
            && self.transactions.is_empty()
            && self.focus_graph.scopes.is_empty()
            && self.focus_graph.default_scope.is_none()
            && self.focus_graph.focused.is_empty()
            && self.event_routes.is_empty()
            && self.animation_tracks.is_empty()
            && self.motion_policy == UiMotionPolicy::default()
            && self.surfaces.is_empty()
            && self.overlay_stack.entries.is_empty()
            && self.scheduler.pending.is_empty()
            && self.selection_model.scopes.is_empty()
            && self.selection_model.active_scope.is_none()
            && self.selection_model.primary.is_empty()
            && self.selection_model.selected.is_empty()
            && self.command_buffer.pending.is_empty()
            && self.command_buffer.executed.is_empty()
            && self.command_buffer.rejections.is_empty()
            && self.command_registry.snapshot.is_empty()
            && self.command_registry.commands_by_source.is_empty()
            && self.theme_registry.is_empty()
            && self.workspace_layout.is_empty()
            && self.hot_reload == UiHotReloadPlan::default()
            && self.signal_values.is_empty()
            && self.animation_state.is_empty()
            && self.session_state.is_empty()
    }
}

impl Default for UiFocusGraph {
    fn default() -> Self {
        Self {
            scopes: Vec::new(),
            default_scope: None,
            focused: BTreeMap::new(),
            traversal_edges: Vec::new(),
        }
    }
}

impl Default for UiSelectionModel {
    fn default() -> Self {
        Self {
            scopes: Vec::new(),
            active_scope: None,
            primary: BTreeMap::new(),
            selected: BTreeMap::new(),
        }
    }
}

/// Flat raw-native projection of the semantic tree.
///
/// Compatibility-only sidecar for legacy native/C consumers.
/// Keep this out of the canonical cross-backend ABI and do not
/// let new authoring paths depend on it.
///
/// New UI semantics should be emitted through `UiRuntimeBundle` and
/// compiler-owned contract payloads, not promoted into this projection.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiNativeProjection {
    pub root_id: Option<u64>,
    pub primary_panel_title: Option<String>,
    pub primary_viewport_title: Option<String>,
    pub primary_viewport_scene: Option<String>,
    #[serde(default)]
    pub tab_groups: Vec<UiNativeProjectionTabGroup>,
    pub nodes: Vec<UiNativeProjectionNode>,
}

impl UiNativeProjection {
    pub fn is_empty(&self) -> bool {
        self.root_id.is_none()
            && self.primary_panel_title.is_none()
            && self.primary_viewport_title.is_none()
            && self.primary_viewport_scene.is_none()
            && self.tab_groups.is_empty()
            && self.nodes.is_empty()
    }
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
    #[serde(default)]
    pub dock_placement: Option<UiDockPlacement>,
    #[serde(default)]
    pub split_ratio: Option<f32>,
    #[serde(default)]
    pub resizable: bool,
    #[serde(default)]
    pub persistent_layout_id: Option<String>,
    #[serde(default)]
    pub tab_group_id: Option<String>,
    #[serde(default)]
    pub tab_label: Option<String>,
    #[serde(default)]
    pub tab_order: Option<i32>,
    #[serde(default)]
    pub tab_default_active: bool,
    #[serde(default)]
    pub tab_closable: bool,
    #[serde(default)]
    pub tab_is_active: bool,
    pub child_count: usize,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiNativeProjectionTabGroup {
    pub id: String,
    pub active_tab_layout_id: Option<String>,
    pub tab_count: usize,
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
const UI_RUNTIME_FORCE_COMPATIBILITY_BACKFILL_KEY: &str = "ui.runtime.force_compatibility_backfill";
const UI_RUNTIME_COMPATIBILITY_FALLBACK_KEY: &str = "ui.runtime.compatibility_fallback";
const UI_RUNTIME_COMPATIBILITY_MODE_KEY: &str = "ui.runtime.compatibility_mode";
const UI_RUNTIME_NATIVE_PROJECTION_MODE_KEY: &str = "ui.runtime.native_projection_mode";

/// Backend-agnostic metadata for a compiled KAIN UI runtime bundle.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeMetadata {
    pub app_name: Option<String>,
    pub window_title: String,
    pub root_component: String,
    pub source_file_name: Option<String>,
    pub initial_window_size: [f32; 2],
    #[serde(default)]
    pub preferred_shell_host_backend: UiHostBackendKind,
    #[serde(default)]
    pub preferred_document_host_backend: UiHostBackendKind,
    #[serde(default)]
    pub preferred_devtools_host_backend: UiHostBackendKind,
    #[serde(default)]
    pub preferred_layout_engine: UiLayoutEngineKind,
    #[serde(default)]
    pub preferred_render_engine: UiRenderEngineKind,
    #[serde(default)]
    pub compatibility_host_backend: UiHostBackendKind,
    #[serde(default)]
    pub mixed_backend_session: bool,
}

impl Default for UiRuntimeMetadata {
    fn default() -> Self {
        Self {
            app_name: None,
            window_title: String::new(),
            root_component: String::new(),
            source_file_name: None,
            initial_window_size: [1440.0, 920.0],
            preferred_shell_host_backend: UiHostBackendKind::Qt,
            preferred_document_host_backend: UiHostBackendKind::RmlUi,
            preferred_devtools_host_backend: UiHostBackendKind::Imgui,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            compatibility_host_backend: UiHostBackendKind::Qt,
            mixed_backend_session: true,
        }
    }
}

/// Serialized ABI boundary between the KAIN compiler and host runtimes.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiRuntimeBundle {
    pub schema_version: u32,
    pub metadata: UiRuntimeMetadata,
    pub output: UiBuildOutput,
    /// Compatibility-only sidecar for legacy raw-native consumers.
    ///
    /// Canonical bundles keep semantic ownership on `output.tree` plus
    /// `output.systems` and omit this field unless an explicit compatibility
    /// helper asks for it.
    #[serde(default, skip_serializing_if = "UiNativeProjection::is_empty")]
    pub native_projection: UiNativeProjection,
}

/// Canonical runtime bundle builder.
///
/// This keeps the bundle centered on `output.tree` and `output.systems`
/// and leaves the compatibility projection empty unless a caller explicitly
/// opts into the legacy helper.
pub fn ui_runtime_bundle_from_output(
    metadata: UiRuntimeMetadata,
    output: UiBuildOutput,
) -> UiRuntimeBundle {
    let mut output = output;
    if ui_runtime_bundle_requires_compatibility_backfill(&output) {
        // Compatibility-only fallback for legacy bundles.
        // New authored UI must emit runtime systems explicitly.
        output.systems = ui_runtime_systems_from_tree(&output.tree);
        output.systems.session_state.insert(
            UI_RUNTIME_COMPATIBILITY_FALLBACK_KEY.to_string(),
            UiValue::Bool(true),
        );
        output.systems.session_state.insert(
            UI_RUNTIME_COMPATIBILITY_MODE_KEY.to_string(),
            UiValue::String("tree_shape_backfill".to_string()),
        );
    }
    UiRuntimeBundle {
        schema_version: UI_RUNTIME_BUNDLE_SCHEMA_VERSION,
        metadata,
        output,
        native_projection: UiNativeProjection::default(),
    }
}

/// Compatibility-only bundle builder that explicitly materializes the raw-native
/// projection sidecar for legacy consumers.
pub fn ui_runtime_bundle_from_output_with_native_projection(
    metadata: UiRuntimeMetadata,
    output: UiBuildOutput,
) -> UiRuntimeBundle {
    let mut bundle = ui_runtime_bundle_from_output(metadata, output);
    bundle.output.systems.session_state.insert(
        UI_RUNTIME_NATIVE_PROJECTION_MODE_KEY.to_string(),
        UiValue::String("compatibility_sidecar".to_string()),
    );
    bundle.native_projection = build_compatibility_native_projection(&bundle.output);
    bundle
}

fn ui_runtime_bundle_requires_compatibility_backfill(output: &UiBuildOutput) -> bool {
    matches!(
        output
            .systems
            .session_state
            .get(UI_RUNTIME_FORCE_COMPATIBILITY_BACKFILL_KEY),
        Some(UiValue::Bool(true))
    ) || output.systems.is_empty()
}

fn build_compatibility_native_projection(output: &UiBuildOutput) -> UiNativeProjection {
    ui_native_projection_from_output(output)
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

/// Legacy-only inference path that backfills runtime systems from tree shape.
/// New authored UI must emit runtime systems explicitly.
pub fn ui_runtime_systems_from_tree(tree: &UiTree) -> UiRuntimeSystems {
    let mut systems = UiRuntimeSystems::default();
    let mut theme_scopes = BTreeSet::new();
    let mut focus_scopes = BTreeSet::new();
    let mut selection_scopes = BTreeSet::new();

    for node in tree.nodes.values() {
        if !node.watches.is_empty() {
            systems.computed.push(UiComputed {
                id: format!("computed.node.{}", node.id.0),
                label: format!("node-{}-dependencies", node.id.0),
                depends_on: node.watches.clone(),
                writes_signal: None,
                expr: None,
                invalidates_nodes: vec![node.id],
                scheduler_phase: UiSchedulerPhase::Signals,
            });
            systems.scheduler.pending.push(UiSchedulerEntry {
                phase: UiSchedulerPhase::Signals,
                label: format!("invalidate-node-{}", node.id.0),
                target_nodes: vec![node.id],
            });
        }

        if let Some(scope) = node.style.theme_scope.clone() {
            if theme_scopes.insert(scope.clone()) {
                systems.theme_registry.scopes.push(UiThemeScope {
                    name: scope.clone(),
                    selector: format!("scope:{scope}"),
                    parent: None,
                });
                systems.theme_registry.diff_keys.push(scope);
            }
        }

        if let Some(scope) = node.focus_scope.clone() {
            if focus_scopes.insert(scope.clone()) {
                systems.focus_graph.scopes.push(scope.clone());
                if systems.focus_graph.default_scope.is_none() {
                    systems.focus_graph.default_scope = Some(scope);
                }
            }
        }

        if let Some(scope) = node.selection_scope.clone() {
            if selection_scopes.insert(scope.clone()) {
                systems.selection_model.scopes.push(scope.clone());
                if systems.selection_model.active_scope.is_none() {
                    systems.selection_model.active_scope = Some(scope);
                }
            }
        }

        if matches!(node.layout.kind, UiLayoutKind::Dock) {
            systems.workspace_layout.roots.push(UiDockNode {
                id: format!("dock.node.{}", node.id.0),
                node: node.id,
                placement: node.layout.dock.unwrap_or(UiDockPlacement::Center),
                split_ratio: node.layout.split_ratio,
                children: node.children.clone(),
                persistent_layout_id: node
                    .layout
                    .persistent_layout_id
                    .clone()
                    .or_else(|| node.identity_key.clone()),
            });
            systems.workspace_layout.virtualization_enabled = true;
            if systems.workspace_layout.persistence_key.is_none() {
                systems.workspace_layout.persistence_key = node
                    .layout
                    .persistent_layout_id
                    .clone()
                    .or_else(|| node.identity_key.clone());
            }
            systems.scheduler.pending.push(UiSchedulerEntry {
                phase: UiSchedulerPhase::Layout,
                label: format!("layout-pass-node-{}", node.id.0),
                target_nodes: vec![node.id],
            });
        }

        if let Some(surface) = ui_surface_for_node(node) {
            systems.surfaces.push(surface);
            systems.animation_tracks.push(UiAnimationTrack {
                id: format!("animation.node.{}", node.id.0),
                target: node.id,
                property: "surface.opacity".to_string(),
                duration_ms: 180,
                trigger: UiAnimationTrigger::Mount,
                easing: UiEasingKind::EaseOut,
                preserve_on_reload: true,
            });
            systems.scheduler.pending.push(UiSchedulerEntry {
                phase: UiSchedulerPhase::Animation,
                label: format!("animation-pass-node-{}", node.id.0),
                target_nodes: vec![node.id],
            });
        }

        if let Some(identity_key) = node.identity_key.clone() {
            if let Some(layout_key) = node.layout.persistent_layout_id.clone() {
                if layout_key != identity_key {
                    systems
                        .hot_reload
                        .identity_aliases
                        .push(UiReloadIdentityAlias {
                            from: identity_key.clone(),
                            to: layout_key.clone(),
                        });
                    systems
                        .hot_reload
                        .identity_aliases
                        .push(UiReloadIdentityAlias {
                            from: layout_key,
                            to: identity_key,
                        });
                }
            }
        }

        // Compatibility: treat `Overlay` nodes as members of an explicit overlay stack so
        // z-order and anchoring are inspectable without backend heuristics.
        if matches!(node.kind, UiWidgetKind::Overlay) {
            let order = node
                .props
                .get("overlay_order")
                .and_then(|value| match value {
                    UiValue::Int(v) => Some(*v as i32),
                    UiValue::Float(v) => Some(*v as i32),
                    UiValue::String(v) => v.parse::<i32>().ok(),
                    _ => None,
                })
                .unwrap_or_else(|| (node.id.0.min(i32::MAX as u64)) as i32);

            systems.overlay_stack.entries.push(UiOverlayEntry {
                id: format!("overlay.node.{}", node.id.0),
                node: node.id,
                order,
                anchor: None,
                modal: false,
            });
        }
    }

    systems.workspace_layout.active_tabs = resolve_workspace_active_tabs(tree, None);
    systems.motion_policy.recompute_capacitor();
    systems
        .overlay_stack
        .entries
        .sort_by(|a, b| (a.order, a.id.clone()).cmp(&(b.order, b.id.clone())));

    if !systems.theme_registry.scopes.is_empty() {
        systems.theme_registry.variants.push(UiThemeVariant {
            scope: systems
                .theme_registry
                .scopes
                .first()
                .map(|scope| scope.name.clone())
                .unwrap_or_else(|| "default".to_string()),
            name: "base".to_string(),
            tokens: vec!["surface.background".to_string(), "text.default".to_string()],
        });
        systems.theme_registry.semantic_tokens.push(UiThemeToken {
            name: "surface.background".to_string(),
            category: "color".to_string(),
            value: UiValue::String("theme.surface.background".to_string()),
        });
        systems.theme_registry.semantic_tokens.push(UiThemeToken {
            name: "text.default".to_string(),
            category: "color".to_string(),
            value: UiValue::String("theme.text.default".to_string()),
        });
    }

    systems
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiSignalUpdate {
    pub signal: UiSignalId,
    pub value: UiValue,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiInvalidationResult {
    pub changed_signals: Vec<UiSignalId>,
    pub invalidated_nodes: Vec<UiNodeId>,
    pub scheduled: Vec<UiSchedulerEntry>,
    pub transaction: Option<UiTransaction>,
}

pub fn ui_execute_signal_updates(
    systems: &mut UiRuntimeSystems,
    updates: &[UiSignalUpdate],
) -> UiInvalidationResult {
    let mut result = UiInvalidationResult::default();
    let mut invalidated = BTreeSet::new();

    for update in updates {
        let changed = systems.signal_values.get(&update.signal) != Some(&update.value);
        if !changed {
            continue;
        }

        systems
            .signal_values
            .insert(update.signal, update.value.clone());
        result.changed_signals.push(update.signal);

        for computed in &systems.computed {
            if computed.depends_on.contains(&update.signal) {
                for node in &computed.invalidates_nodes {
                    if invalidated.insert(*node) {
                        result.invalidated_nodes.push(*node);
                    }
                }

                let entry = UiSchedulerEntry {
                    phase: computed.scheduler_phase,
                    label: computed.label.clone(),
                    target_nodes: computed.invalidates_nodes.clone(),
                };
                systems.scheduler.pending.push(entry.clone());
                result.scheduled.push(entry);
            }
        }
    }

    if !result.changed_signals.is_empty() {
        let transaction = UiTransaction {
            label: format!("signals:{}", result.changed_signals.len()),
            touched_nodes: result.invalidated_nodes.clone(),
            changed_signals: result.changed_signals.clone(),
            ..UiTransaction::default_runtime(0)
        };
        systems.transactions.push(transaction.clone());
        result.transaction = Some(transaction);
    }

    result
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiResolvedTheme {
    pub active_theme: Option<String>,
    pub scope_chain: Vec<String>,
    pub applied_tokens: Vec<String>,
    pub values: BTreeMap<String, UiValue>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiThemeDiffEntry {
    pub key: String,
    pub before: Option<UiValue>,
    pub after: Option<UiValue>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiThemeDiff {
    pub changes: Vec<UiThemeDiffEntry>,
}

impl UiThemeDiff {
    pub fn is_empty(&self) -> bool {
        self.changes.is_empty()
    }
}

pub fn ui_resolve_theme_for_node(node: &UiNode, registry: &UiThemeRegistry) -> UiResolvedTheme {
    let mut resolved = UiResolvedTheme {
        active_theme: registry.active_theme.clone(),
        scope_chain: theme_scope_chain(node.style.theme_scope.as_deref(), registry),
        ..UiResolvedTheme::default()
    };

    for token in &registry.semantic_tokens {
        resolved
            .values
            .insert(token.name.clone(), token.value.clone());
        resolved.applied_tokens.push(token.name.clone());
    }

    for scope in &resolved.scope_chain {
        for variant in registry.variants.iter().filter(|variant| {
            variant.scope == *scope
                && match node.style.variant.as_deref() {
                    Some(name) => variant.name == name || variant.name == "base",
                    None => variant.name == "base",
                }
        }) {
            for token in &variant.tokens {
                if !resolved.applied_tokens.contains(token) {
                    resolved.applied_tokens.push(token.clone());
                }
            }
        }
    }

    for token in &node.style.tokens {
        if !resolved.applied_tokens.contains(token) {
            resolved.applied_tokens.push(token.clone());
        }
    }

    for class_name in &node.style.classes {
        resolved
            .values
            .insert(format!("class:{class_name}"), UiValue::Bool(true));
    }

    for state in &node.style.states {
        resolved.values.insert(
            format!("state:{:?}", state).to_ascii_lowercase(),
            UiValue::Bool(true),
        );
    }

    for (key, value) in &node.style.values {
        resolved.values.insert(key.clone(), value.clone());
    }

    resolved
}

pub fn ui_diff_resolved_theme(previous: &UiResolvedTheme, next: &UiResolvedTheme) -> UiThemeDiff {
    let mut changes = Vec::new();
    let keys = previous
        .values
        .keys()
        .chain(next.values.keys())
        .cloned()
        .collect::<BTreeSet<_>>();

    for key in keys {
        let before = previous.values.get(&key).cloned();
        let after = next.values.get(&key).cloned();
        if before != after {
            changes.push(UiThemeDiffEntry { key, before, after });
        }
    }

    UiThemeDiff { changes }
}

fn theme_scope_chain(scope_name: Option<&str>, registry: &UiThemeRegistry) -> Vec<String> {
    let mut chain = Vec::new();
    let mut current = scope_name
        .map(|value| value.to_string())
        .or_else(|| registry.scopes.first().map(|scope| scope.name.clone()));

    while let Some(scope_name) = current {
        if chain.contains(&scope_name) {
            break;
        }
        chain.push(scope_name.clone());
        current = registry
            .scopes
            .iter()
            .find(|scope| scope.name == scope_name)
            .and_then(|scope| scope.parent.clone());
    }

    chain
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiRect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiResolvedLayoutNode {
    pub node: UiNodeId,
    pub rect: UiRect,
    pub layout_kind: UiLayoutKind,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiResolvedLayout {
    pub viewport: UiRect,
    pub nodes: Vec<UiResolvedLayoutNode>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiPersistedDockState {
    pub placement: UiDockPlacement,
    pub split_ratio: Option<f32>,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiWorkspaceLayoutSnapshot {
    pub persistence_key: Option<String>,
    #[serde(default)]
    pub active_tabs: BTreeMap<String, String>,
    pub nodes: BTreeMap<String, UiPersistedDockState>,
}

pub fn ui_solve_workspace_layout(
    tree: &UiTree,
    systems: &UiRuntimeSystems,
    viewport_size: [f32; 2],
) -> UiResolvedLayout {
    let viewport = UiRect {
        x: 0.0,
        y: 0.0,
        width: viewport_size[0].max(1.0),
        height: viewport_size[1].max(1.0),
    };
    let mut resolved = UiResolvedLayout {
        viewport,
        nodes: Vec::new(),
    };

    if let Some(root) = tree.root {
        solve_layout_node(tree, root, viewport, systems, &mut resolved);
    }

    resolved
}

pub fn ui_workspace_layout_snapshot(
    tree: &UiTree,
    systems: &UiRuntimeSystems,
) -> UiWorkspaceLayoutSnapshot {
    let mut snapshot = UiWorkspaceLayoutSnapshot {
        persistence_key: systems.workspace_layout.persistence_key.clone(),
        active_tabs: systems.workspace_layout.active_tabs.clone(),
        nodes: BTreeMap::new(),
    };

    for node in tree.nodes.values() {
        if let Some(layout_id) = node
            .layout
            .persistent_layout_id
            .as_ref()
            .or(node.identity_key.as_ref())
        {
            snapshot.nodes.insert(
                layout_id.clone(),
                UiPersistedDockState {
                    placement: node.layout.dock.unwrap_or(UiDockPlacement::Center),
                    split_ratio: node.layout.split_ratio,
                },
            );
        }
    }

    snapshot
}

pub fn ui_apply_workspace_layout_snapshot(
    tree: &mut UiTree,
    systems: &mut UiRuntimeSystems,
    snapshot: &UiWorkspaceLayoutSnapshot,
) -> usize {
    let mut applied = 0;
    systems.workspace_layout.persistence_key = snapshot.persistence_key.clone();
    systems.workspace_layout.active_tabs = snapshot.active_tabs.clone();

    for node in tree.nodes.values_mut() {
        let Some(layout_id) = node
            .layout
            .persistent_layout_id
            .clone()
            .or_else(|| node.identity_key.clone())
        else {
            continue;
        };

        let Some(saved) = snapshot.nodes.get(&layout_id) else {
            continue;
        };

        node.layout.dock = Some(saved.placement);
        node.layout.split_ratio = saved.split_ratio;
        applied += 1;
    }

    let mut rebuilt = ui_runtime_systems_from_tree(tree).workspace_layout;
    rebuilt.persistence_key = snapshot.persistence_key.clone();
    rebuilt.active_tabs = resolve_workspace_active_tabs(tree, Some(&snapshot.active_tabs));
    systems.workspace_layout = rebuilt;
    applied
}

fn solve_layout_node(
    tree: &UiTree,
    id: UiNodeId,
    rect: UiRect,
    systems: &UiRuntimeSystems,
    resolved: &mut UiResolvedLayout,
) {
    let Some(node) = tree.node(id) else {
        return;
    };

    resolved.nodes.push(UiResolvedLayoutNode {
        node: id,
        rect,
        layout_kind: node.layout.kind,
    });

    if node.children.is_empty() {
        return;
    }

    match node.layout.kind {
        UiLayoutKind::Dock => solve_dock_children(tree, node, rect, systems, resolved),
        UiLayoutKind::FlexRow => solve_row_children(tree, node, rect, systems, resolved),
        UiLayoutKind::FlexColumn | UiLayoutKind::Flow => {
            solve_column_children(tree, node, rect, systems, resolved)
        }
        UiLayoutKind::Stack | UiLayoutKind::Absolute | UiLayoutKind::Grid => {
            for child in &node.children {
                solve_layout_node(tree, *child, rect, systems, resolved);
            }
        }
    }
}

fn solve_dock_children(
    tree: &UiTree,
    node: &UiNode,
    rect: UiRect,
    systems: &UiRuntimeSystems,
    resolved: &mut UiResolvedLayout,
) {
    let mut remaining = rect;

    for child_id in &node.children {
        let Some(child) = tree.node(*child_id) else {
            continue;
        };

        let preferred_ratio = child
            .layout
            .split_ratio
            .or_else(|| persisted_split_ratio(child, systems))
            .unwrap_or(0.25)
            .clamp(0.1, 0.9);

        let dock_placement = child.layout.dock.unwrap_or(UiDockPlacement::Center);
        if dock_placement == UiDockPlacement::Tab && !node_is_active_tab(tree, child, systems) {
            continue;
        }

        let child_rect = match dock_placement {
            UiDockPlacement::Left => {
                let width = remaining.width * preferred_ratio;
                let child_rect = UiRect {
                    x: remaining.x,
                    y: remaining.y,
                    width,
                    height: remaining.height,
                };
                remaining.x += width;
                remaining.width -= width;
                child_rect
            }
            UiDockPlacement::Right => {
                let width = remaining.width * preferred_ratio;
                let child_rect = UiRect {
                    x: remaining.x + remaining.width - width,
                    y: remaining.y,
                    width,
                    height: remaining.height,
                };
                remaining.width -= width;
                child_rect
            }
            UiDockPlacement::Top => {
                let height = remaining.height * preferred_ratio;
                let child_rect = UiRect {
                    x: remaining.x,
                    y: remaining.y,
                    width: remaining.width,
                    height,
                };
                remaining.y += height;
                remaining.height -= height;
                child_rect
            }
            UiDockPlacement::Bottom => {
                let height = remaining.height * preferred_ratio;
                let child_rect = UiRect {
                    x: remaining.x,
                    y: remaining.y + remaining.height - height,
                    width: remaining.width,
                    height,
                };
                remaining.height -= height;
                child_rect
            }
            UiDockPlacement::Center | UiDockPlacement::Tab => remaining,
        };

        solve_layout_node(tree, *child_id, child_rect, systems, resolved);
    }
}

fn solve_row_children(
    tree: &UiTree,
    node: &UiNode,
    rect: UiRect,
    systems: &UiRuntimeSystems,
    resolved: &mut UiResolvedLayout,
) {
    let total_gap = node.layout.gap * node.children.len().saturating_sub(1) as f32;
    let total_flex = node
        .children
        .iter()
        .filter_map(|child_id| tree.node(*child_id))
        .map(|child| child.layout.flex_grow.max(1.0))
        .sum::<f32>()
        .max(1.0);
    let mut cursor_x = rect.x;

    for child_id in &node.children {
        let Some(child) = tree.node(*child_id) else {
            continue;
        };
        let share = child.layout.flex_grow.max(1.0) / total_flex;
        let width = ((rect.width - total_gap).max(1.0)) * share;
        let child_rect = UiRect {
            x: cursor_x,
            y: rect.y,
            width,
            height: rect.height,
        };
        cursor_x += width + node.layout.gap;
        solve_layout_node(tree, *child_id, child_rect, systems, resolved);
    }
}

fn solve_column_children(
    tree: &UiTree,
    node: &UiNode,
    rect: UiRect,
    systems: &UiRuntimeSystems,
    resolved: &mut UiResolvedLayout,
) {
    let total_gap = node.layout.gap * node.children.len().saturating_sub(1) as f32;
    let total_flex = node
        .children
        .iter()
        .filter_map(|child_id| tree.node(*child_id))
        .map(|child| child.layout.flex_grow.max(1.0))
        .sum::<f32>()
        .max(1.0);
    let mut cursor_y = rect.y;

    for child_id in &node.children {
        let Some(child) = tree.node(*child_id) else {
            continue;
        };
        let share = child.layout.flex_grow.max(1.0) / total_flex;
        let height = ((rect.height - total_gap).max(1.0)) * share;
        let child_rect = UiRect {
            x: rect.x,
            y: cursor_y,
            width: rect.width,
            height,
        };
        cursor_y += height + node.layout.gap;
        solve_layout_node(tree, *child_id, child_rect, systems, resolved);
    }
}

fn persisted_split_ratio(node: &UiNode, systems: &UiRuntimeSystems) -> Option<f32> {
    let persisted_id = node
        .layout
        .persistent_layout_id
        .as_ref()
        .or(node.identity_key.as_ref())?;

    systems
        .workspace_layout
        .roots
        .iter()
        .find(|entry| entry.persistent_layout_id.as_ref() == Some(persisted_id))
        .and_then(|entry| entry.split_ratio)
}

fn node_layout_id(node: &UiNode) -> String {
    node.layout
        .persistent_layout_id
        .clone()
        .or_else(|| node.identity_key.clone())
        .unwrap_or_else(|| format!("node-{}", node.id.0))
}

fn resolve_workspace_active_tabs(
    tree: &UiTree,
    persisted: Option<&BTreeMap<String, String>>,
) -> BTreeMap<String, String> {
    let mut grouped_tabs = BTreeMap::<String, Vec<(&UiNode, String)>>::new();
    let mut resolved = BTreeMap::new();

    for node in tree.nodes.values() {
        let Some(group_id) = node.layout.tab_group_id.clone() else {
            continue;
        };
        grouped_tabs
            .entry(group_id)
            .or_default()
            .push((node, node_layout_id(node)));
    }

    for (group_id, mut tabs) in grouped_tabs {
        if let Some(saved_layout_id) = persisted.and_then(|saved| saved.get(&group_id)) {
            if tabs
                .iter()
                .any(|(_, layout_id)| layout_id == saved_layout_id)
            {
                resolved.insert(group_id, saved_layout_id.clone());
                continue;
            }
        }

        tabs.sort_by_key(|(node, _)| (node.layout.tab_order.unwrap_or(i32::MAX), node.id.0));
        if let Some(layout_id) = tabs
            .iter()
            .find(|(node, _)| node.layout.tab_default_active)
            .or_else(|| tabs.first())
            .map(|(_, layout_id)| layout_id.clone())
        {
            resolved.insert(group_id, layout_id);
        }
    }

    resolved
}

fn node_is_active_tab(tree: &UiTree, node: &UiNode, systems: &UiRuntimeSystems) -> bool {
    let Some(group_id) = node.layout.tab_group_id.as_deref() else {
        return true;
    };
    let active_tabs = if systems.workspace_layout.active_tabs.is_empty() {
        resolve_workspace_active_tabs(tree, None)
    } else {
        systems.workspace_layout.active_tabs.clone()
    };

    active_tabs.get(group_id).map_or(true, |active_layout_id| {
        active_layout_id == &node_layout_id(node)
    })
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct UiAnimationFrame {
    pub track_id: String,
    pub target: UiNodeId,
    pub property: String,
    pub progress: f32,
    pub eased_progress: f32,
    pub completed: bool,
}

pub fn ui_step_animation_runtime(
    systems: &mut UiRuntimeSystems,
    delta_ms: u32,
) -> Vec<UiAnimationFrame> {
    let mut frames = Vec::new();

    for track in &systems.animation_tracks {
        let state = systems.animation_state.entry(track.id.clone()).or_default();
        if state.completed {
            continue;
        }

        state.elapsed_ms = state.elapsed_ms.saturating_add(delta_ms);
        let duration = track.duration_ms.max(1);
        state.progress = (state.elapsed_ms as f32 / duration as f32).clamp(0.0, 1.0);
        state.eased_progress = ease_progress(track.easing, state.progress);
        state.completed = state.progress >= 1.0;

        frames.push(UiAnimationFrame {
            track_id: track.id.clone(),
            target: track.target,
            property: track.property.clone(),
            progress: state.progress,
            eased_progress: state.eased_progress,
            completed: state.completed,
        });
    }

    frames
}

fn ease_progress(easing: UiEasingKind, progress: f32) -> f32 {
    match easing {
        UiEasingKind::Linear => progress,
        UiEasingKind::EaseIn => progress * progress,
        UiEasingKind::EaseOut => 1.0 - (1.0 - progress) * (1.0 - progress),
        UiEasingKind::EaseInOut => {
            if progress < 0.5 {
                2.0 * progress * progress
            } else {
                1.0 - (-2.0 * progress + 2.0).powf(2.0) / 2.0
            }
        }
        UiEasingKind::Spring => {
            let overshoot = 1.70158;
            let shifted = progress - 1.0;
            1.0 + shifted * shifted * ((overshoot + 1.0) * shifted + overshoot)
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct UiHotReloadTransferReport {
    pub focus_transferred: bool,
    pub focus_nodes_transferred: usize,
    pub focus_traversal_edges_transferred: usize,
    pub selection_transferred: bool,
    pub selection_nodes_transferred: usize,
    pub docking_transferred: bool,
    pub workspace_nodes_transferred: usize,
    pub overlay_transferred: bool,
    pub overlay_entries_transferred: usize,
    pub motion_policy_transferred: bool,
    pub animation_tracks_transferred: usize,
    pub signal_values_transferred: usize,
    pub session_values_transferred: usize,
    pub identity_links: Vec<UiHotReloadNodeLink>,
    pub workspace_layout_links: Vec<UiHotReloadNodeLink>,
    pub overlay_links: Vec<UiHotReloadOverlayLink>,
    pub animation_track_links: Vec<UiHotReloadNodeLink>,
    pub signal_links: Vec<UiHotReloadSignalLink>,
    pub focus_scopes_preserved: Vec<String>,
    pub focus_scopes_dropped: Vec<String>,
    pub selection_scopes_preserved: Vec<String>,
    pub selection_scopes_dropped: Vec<String>,
    pub session_keys_preserved: Vec<String>,
    pub session_keys_dropped: Vec<String>,
    pub dropped_previous_nodes: Vec<UiNodeId>,
    pub invalidated_nodes: Vec<UiNodeId>,
}

pub fn ui_transfer_hot_reload_state(
    previous: &UiBuildOutput,
    next: &mut UiBuildOutput,
) -> UiHotReloadTransferReport {
    let mut report = UiHotReloadTransferReport::default();
    let previous_reload_identity_map = node_identity_map(&previous.tree);
    let next_reload_identity_map = node_identity_map(&next.tree);
    let previous_layout_identity_map = node_layout_identity_map(&previous.tree);
    let next_layout_identity_map = node_layout_identity_map(&next.tree);
    let aliases = next
        .systems
        .hot_reload
        .identity_aliases
        .iter()
        .map(|alias| (alias.from.clone(), alias.to.clone()))
        .collect::<BTreeMap<_, _>>();

    report.identity_links =
        build_node_links(&previous_reload_identity_map, &next_reload_identity_map);
    report.workspace_layout_links =
        build_node_links(&previous_layout_identity_map, &next_layout_identity_map);
    report.dropped_previous_nodes =
        dropped_previous_nodes(&previous_reload_identity_map, &next_reload_identity_map);
    report.invalidated_nodes = Vec::new();

    if next.systems.hot_reload.preserve_motion_policy {
        next.systems.motion_policy = previous.systems.motion_policy.clone();
        next.systems.motion_policy.recompute_capacitor();
        report.motion_policy_transferred = true;
    }

    if next.systems.hot_reload.preserve_docking {
        let snapshot = ui_workspace_layout_snapshot(&previous.tree, &previous.systems);
        let applied =
            ui_apply_workspace_layout_snapshot(&mut next.tree, &mut next.systems, &snapshot);
        report.docking_transferred =
            applied > 0 || snapshot.persistence_key.is_some() || !snapshot.active_tabs.is_empty();
        report.workspace_nodes_transferred = applied;
    }

    if next.systems.hot_reload.preserve_focus {
        let previous_scopes = previous
            .systems
            .focus_graph
            .scopes
            .iter()
            .cloned()
            .collect::<BTreeSet<_>>();
        let next_scopes = next
            .systems
            .focus_graph
            .scopes
            .iter()
            .cloned()
            .collect::<BTreeSet<_>>();
        report.focus_scopes_preserved = previous_scopes
            .intersection(&next_scopes)
            .cloned()
            .collect::<Vec<_>>();
        report.focus_scopes_dropped = previous_scopes
            .difference(&next_scopes)
            .cloned()
            .collect::<Vec<_>>();

        if let Some(default_scope) = previous.systems.focus_graph.default_scope.clone() {
            if next_scopes.contains(&default_scope) {
                next.systems.focus_graph.default_scope = Some(default_scope);
                report.focus_transferred = true;
            }
        }

        let remapped_focus = remap_scoped_node_map(
            &previous.systems.focus_graph.focused,
            &next_scopes,
            &previous_reload_identity_map,
            &next_reload_identity_map,
            &aliases,
        );
        report.focus_nodes_transferred = remapped_focus.len();
        next.systems.focus_graph.focused = remapped_focus;

        let mut traversal_edges = Vec::new();
        for edge in &previous.systems.focus_graph.traversal_edges {
            if !next_scopes.contains(&edge.scope) {
                continue;
            }
            let Some(from_identity) =
                previous_reload_identity_map
                    .iter()
                    .find_map(|(node_id, identity)| {
                        if *node_id == edge.from {
                            Some(identity.clone())
                        } else {
                            None
                        }
                    })
            else {
                continue;
            };
            let Some(to_identity) =
                previous_reload_identity_map
                    .iter()
                    .find_map(|(node_id, identity)| {
                        if *node_id == edge.to {
                            Some(identity.clone())
                        } else {
                            None
                        }
                    })
            else {
                continue;
            };
            let Some(from_node) =
                resolve_identity_to_node(&from_identity, &next_reload_identity_map, &aliases)
            else {
                continue;
            };
            let Some(to_node) =
                resolve_identity_to_node(&to_identity, &next_reload_identity_map, &aliases)
            else {
                continue;
            };
            traversal_edges.push(UiFocusTraversalEdge {
                scope: edge.scope.clone(),
                kind: edge.kind,
                from: from_node,
                to: to_node,
            });
        }
        report.focus_traversal_edges_transferred = traversal_edges.len();
        if !traversal_edges.is_empty() {
            next.systems.focus_graph.traversal_edges = traversal_edges;
            report.focus_transferred = true;
        }
    }

    if next.systems.hot_reload.preserve_selection {
        let previous_scopes = previous
            .systems
            .selection_model
            .scopes
            .iter()
            .cloned()
            .collect::<BTreeSet<_>>();
        let next_scopes = next
            .systems
            .selection_model
            .scopes
            .iter()
            .cloned()
            .collect::<BTreeSet<_>>();
        report.selection_scopes_preserved = previous_scopes
            .intersection(&next_scopes)
            .cloned()
            .collect::<Vec<_>>();
        report.selection_scopes_dropped = previous_scopes
            .difference(&next_scopes)
            .cloned()
            .collect::<Vec<_>>();

        if let Some(active_scope) = previous.systems.selection_model.active_scope.clone() {
            if next_scopes.contains(&active_scope) {
                next.systems.selection_model.active_scope = Some(active_scope);
                report.selection_transferred = true;
            }
        }

        let remapped_primary = remap_scoped_node_map(
            &previous.systems.selection_model.primary,
            &next_scopes,
            &previous_reload_identity_map,
            &next_reload_identity_map,
            &aliases,
        );
        report.selection_nodes_transferred += remapped_primary.len();
        next.systems.selection_model.primary = remapped_primary;

        let remapped_selected = remap_scoped_node_set_map(
            &previous.systems.selection_model.selected,
            &next_scopes,
            &previous_reload_identity_map,
            &next_reload_identity_map,
            &aliases,
        );
        report.selection_nodes_transferred +=
            remapped_selected.values().map(BTreeSet::len).sum::<usize>();
        next.systems.selection_model.selected = remapped_selected;
        report.selection_transferred =
            report.selection_transferred || report.selection_nodes_transferred > 0;
    }

    if next.systems.hot_reload.preserve_overlays {
        let mut remapped_entries = Vec::new();
        for overlay in &previous.systems.overlay_stack.entries {
            let Some(previous_node_identity) =
                previous_reload_identity_map
                    .iter()
                    .find_map(|(node_id, identity)| {
                        if *node_id == overlay.node {
                            Some(identity.clone())
                        } else {
                            None
                        }
                    })
            else {
                continue;
            };
            let Some(next_node) = resolve_identity_to_node(
                &previous_node_identity,
                &next_reload_identity_map,
                &aliases,
            ) else {
                continue;
            };
            let previous_anchor_target = overlay.anchor.as_ref().map(|anchor| anchor.target);
            let next_anchor_target = overlay
                .anchor
                .as_ref()
                .and_then(|anchor| {
                    previous_reload_identity_map
                        .iter()
                        .find_map(|(node_id, identity)| {
                            if *node_id == anchor.target {
                                Some(identity.clone())
                            } else {
                                None
                            }
                        })
                })
                .and_then(|identity| {
                    resolve_identity_to_node(&identity, &next_reload_identity_map, &aliases)
                });
            let remapped_anchor = overlay.anchor.as_ref().and_then(|anchor| {
                next_anchor_target.map(|target| UiAnchorSpec {
                    target,
                    placement: anchor.placement,
                    offset_px: anchor.offset_px,
                    constraint: anchor.constraint,
                })
            });

            if overlay.anchor.is_some() && remapped_anchor.is_none() {
                report.invalidated_nodes.push(next_node);
            }

            remapped_entries.push(UiOverlayEntry {
                id: overlay.id.clone(),
                node: next_node,
                order: overlay.order,
                anchor: remapped_anchor,
                modal: overlay.modal,
            });
            report.overlay_links.push(UiHotReloadOverlayLink {
                overlay_id: overlay.id.clone(),
                previous_node: Some(overlay.node),
                next_node: Some(next_node),
                previous_anchor_target,
                next_anchor_target,
            });
        }
        remapped_entries.sort_by(|a, b| (a.order, a.id.clone()).cmp(&(b.order, b.id.clone())));
        report.overlay_entries_transferred = remapped_entries.len();
        report.overlay_transferred = report.overlay_entries_transferred > 0;
        next.systems.overlay_stack.entries = remapped_entries;
    }

    if next.systems.hot_reload.preserve_signal_values {
        let previous_signal_keys = build_signal_key_index(previous);
        let next_signal_keys = build_signal_key_index(next);
        let mut remapped_signals = Vec::new();
        let mut transferred_signal_ids = BTreeSet::<UiSignalId>::new();

        for (signal_key, previous_signal) in previous_signal_keys {
            let Some(next_signal) = next_signal_keys.get(&signal_key).copied() else {
                continue;
            };
            let Some(previous_value) = previous
                .systems
                .signal_values
                .get(&previous_signal)
                .cloned()
            else {
                continue;
            };
            next.systems
                .signal_values
                .insert(next_signal, previous_value);
            remapped_signals.push(UiHotReloadSignalLink {
                signal_key: signal_key.clone(),
                previous_signal: Some(previous_signal),
                next_signal: Some(next_signal),
            });
            transferred_signal_ids.insert(next_signal);
        }

        report.signal_values_transferred = remapped_signals.len();
        report.signal_links = remapped_signals;
        report.invalidated_nodes.extend(
            transferred_signal_ids
                .into_iter()
                .filter_map(|signal| signal_owner_node(&next.tree, &next.systems, signal)),
        );
    }

    if next.systems.hot_reload.preserve_session_state {
        let mut preserved = Vec::new();
        let mut dropped = Vec::new();
        for (key, value) in &previous.systems.session_state {
            if is_reload_transient_session_key(key) {
                dropped.push(key.clone());
                continue;
            }
            next.systems
                .session_state
                .insert(key.clone(), value.clone());
            preserved.push(key.clone());
        }
        report.session_values_transferred = preserved.len();
        report.session_keys_preserved = preserved;
        report.session_keys_dropped = dropped;
    }

    if next.systems.hot_reload.preserve_animation_state {
        let next_animation_identity_map = node_reload_identity_map(&next.tree);
        let mut remapped_tracks = Vec::new();
        for track in &next.systems.animation_tracks {
            if !track.preserve_on_reload {
                continue;
            }

            let Some(target_identity) = next_animation_identity_map.get(&track.target) else {
                continue;
            };
            let previous_identity = aliases
                .iter()
                .find_map(|(from, to)| {
                    if to == target_identity {
                        Some(from.clone())
                    } else {
                        None
                    }
                })
                .unwrap_or_else(|| target_identity.clone());
            let Some(previous_node) =
                previous_reload_identity_map
                    .iter()
                    .find_map(|(node_id, identity)| {
                        if identity == &previous_identity {
                            Some(*node_id)
                        } else {
                            None
                        }
                    })
            else {
                continue;
            };
            let Some(previous_track) = previous.systems.animation_tracks.iter().find(|candidate| {
                candidate.target == previous_node && candidate.property == track.property
            }) else {
                continue;
            };

            if let Some(state) = previous.systems.animation_state.get(&previous_track.id) {
                next.systems
                    .animation_state
                    .insert(track.id.clone(), state.clone());
                remapped_tracks.push(UiHotReloadNodeLink {
                    stable_identity: track.id.clone(),
                    previous_node: Some(previous_node),
                    next_node: Some(track.target),
                });
                report.animation_tracks_transferred += 1;
            }
        }
        report.animation_track_links = remapped_tracks;
    }

    report.invalidated_nodes.sort();
    report.invalidated_nodes.dedup();
    report.dropped_previous_nodes.sort();
    report.dropped_previous_nodes.dedup();
    report
}

fn node_identity_map(tree: &UiTree) -> BTreeMap<UiNodeId, String> {
    tree.nodes
        .values()
        .filter_map(|node| {
            node.identity_key
                .clone()
                .or_else(|| node.layout.persistent_layout_id.clone())
                .map(|identity| (node.id, identity))
        })
        .collect()
}

fn node_reload_identity_map(tree: &UiTree) -> BTreeMap<UiNodeId, String> {
    node_identity_map(tree)
}

fn node_layout_identity_map(tree: &UiTree) -> BTreeMap<UiNodeId, String> {
    tree.nodes
        .values()
        .filter_map(|node| {
            node.layout
                .persistent_layout_id
                .clone()
                .or_else(|| node.identity_key.clone())
                .map(|identity| (node.id, identity))
        })
        .collect()
}

fn build_node_links(
    previous: &BTreeMap<UiNodeId, String>,
    next: &BTreeMap<UiNodeId, String>,
) -> Vec<UiHotReloadNodeLink> {
    let mut identities = BTreeSet::<String>::new();
    identities.extend(previous.values().cloned());
    identities.extend(next.values().cloned());

    identities
        .into_iter()
        .map(|stable_identity| UiHotReloadNodeLink {
            previous_node: previous
                .iter()
                .find_map(|(node, identity)| (identity == &stable_identity).then_some(*node)),
            next_node: next
                .iter()
                .find_map(|(node, identity)| (identity == &stable_identity).then_some(*node)),
            stable_identity,
        })
        .collect()
}

fn dropped_previous_nodes(
    previous: &BTreeMap<UiNodeId, String>,
    next: &BTreeMap<UiNodeId, String>,
) -> Vec<UiNodeId> {
    let next_identities = next.values().cloned().collect::<BTreeSet<_>>();
    previous
        .iter()
        .filter_map(|(node, identity)| (!next_identities.contains(identity)).then_some(*node))
        .collect()
}

fn resolve_identity_to_node(
    identity: &str,
    next: &BTreeMap<UiNodeId, String>,
    aliases: &BTreeMap<String, String>,
) -> Option<UiNodeId> {
    let mut visited = BTreeSet::<String>::new();
    let mut cursor = identity.to_string();
    loop {
        if !visited.insert(cursor.clone()) {
            return None;
        }
        if let Some(node) = next
            .iter()
            .find_map(|(node_id, candidate)| (candidate == &cursor).then_some(*node_id))
        {
            return Some(node);
        }
        let Some(next_identity) = aliases.get(&cursor).cloned() else {
            return None;
        };
        cursor = next_identity;
    }
}

fn remap_scoped_node_map(
    previous: &BTreeMap<String, UiNodeId>,
    next_scopes: &BTreeSet<String>,
    previous_identity_map: &BTreeMap<UiNodeId, String>,
    next_identity_map: &BTreeMap<UiNodeId, String>,
    aliases: &BTreeMap<String, String>,
) -> BTreeMap<String, UiNodeId> {
    let mut remapped = BTreeMap::new();
    for (scope, node) in previous {
        if !next_scopes.contains(scope) {
            continue;
        }
        let Some(identity) = previous_identity_map.get(node) else {
            continue;
        };
        let Some(next_node) = resolve_identity_to_node(identity, next_identity_map, aliases) else {
            continue;
        };
        remapped.insert(scope.clone(), next_node);
    }
    remapped
}

fn remap_scoped_node_set_map(
    previous: &BTreeMap<String, BTreeSet<UiNodeId>>,
    next_scopes: &BTreeSet<String>,
    previous_identity_map: &BTreeMap<UiNodeId, String>,
    next_identity_map: &BTreeMap<UiNodeId, String>,
    aliases: &BTreeMap<String, String>,
) -> BTreeMap<String, BTreeSet<UiNodeId>> {
    let mut remapped = BTreeMap::new();
    for (scope, nodes) in previous {
        if !next_scopes.contains(scope) {
            continue;
        }
        let remapped_nodes = nodes
            .iter()
            .filter_map(|node| previous_identity_map.get(node))
            .filter_map(|identity| resolve_identity_to_node(identity, next_identity_map, aliases))
            .collect::<BTreeSet<_>>();
        if !remapped_nodes.is_empty() {
            remapped.insert(scope.clone(), remapped_nodes);
        }
    }
    remapped
}

fn build_signal_key_index(output: &UiBuildOutput) -> BTreeMap<String, UiSignalId> {
    let mut index = BTreeMap::new();
    for (key, value) in &output.systems.session_state {
        let Some(raw_id) = key.strip_prefix("ui.signal.key.") else {
            continue;
        };
        let Ok(parsed_id) = raw_id.parse::<u64>() else {
            continue;
        };
        let UiValue::String(signal_key) = value else {
            continue;
        };
        index.insert(signal_key.clone(), UiSignalId(parsed_id));
    }
    index
}

fn signal_owner_node(
    tree: &UiTree,
    systems: &UiRuntimeSystems,
    signal: UiSignalId,
) -> Option<UiNodeId> {
    let signal_key = systems
        .session_state
        .get(&format!("ui.signal.key.{}", signal.0))
        .and_then(|value| match value {
            UiValue::String(value) => Some(value.as_str()),
            _ => None,
        });

    tree.nodes.values().find_map(|node| {
        if node.watches.contains(&signal) {
            return Some(node.id);
        }
        let prop_match = node.props.values().any(|value| match value {
            UiValue::String(value) => {
                value == &signal.0.to_string() || signal_key.is_some_and(|key| value == key)
            }
            _ => false,
        });
        prop_match.then_some(node.id)
    })
}

fn is_reload_transient_session_key(key: &str) -> bool {
    key.starts_with("ui.reload.")
        || key.starts_with("ui.runtime.")
        || key.starts_with("ui.devtools.")
        || key.ends_with(".transient")
}

pub fn ui_native_projection_from_output(output: &UiBuildOutput) -> UiNativeProjection {
    let mut projection = UiNativeProjection {
        root_id: output.tree.root.map(|id| id.0),
        ..UiNativeProjection::default()
    };
    let active_tabs = if output.systems.workspace_layout.active_tabs.is_empty() {
        resolve_workspace_active_tabs(&output.tree, None)
    } else {
        output.systems.workspace_layout.active_tabs.clone()
    };

    let Some(root_id) = output.tree.root else {
        return projection;
    };

    projection.tab_groups = build_native_projection_tab_groups(&output.tree, &active_tabs);
    collect_native_projection_nodes(
        &output.tree,
        root_id,
        None,
        0,
        &active_tabs,
        &mut projection,
    );
    projection
}

fn ui_surface_for_node(node: &UiNode) -> Option<UiSurface> {
    let kind = surface_kind_for_node(node)?;
    let shader = surface_shader_binding_for_node(node);
    let renderer_preference = surface_renderer_preference_for_node(node, shader.as_ref(), &kind);
    let composition_mode = surface_composition_mode_for_node(node, shader.as_ref(), &kind);
    let preferred_host_backend = surface_host_backend_for_node(node, &kind);
    let preferred_layout_engine = surface_layout_engine_for_node(node);
    let preferred_render_engine =
        surface_render_engine_for_node(node, shader.as_ref(), &kind, renderer_preference);
    let gpu_backing_required = surface_gpu_backing_required(
        node,
        shader.as_ref(),
        &kind,
        renderer_preference,
        composition_mode,
    );

    Some(UiSurface {
        id: format!("surface.node.{}", node.id.0),
        kind,
        node: node.id,
        title: node_prop_string(node, "title"),
        renderer_preference,
        composition_mode,
        preferred_host_backend,
        preferred_layout_engine,
        preferred_render_engine,
        gpu_backing_required,
        shader,
    })
}

fn surface_kind_for_node(node: &UiNode) -> Option<UiSurfaceKind> {
    match node.kind {
        UiWidgetKind::Element(ref tag) if is_canvas_surface_element(tag, node) => {
            Some(UiSurfaceKind::Canvas)
        }
        UiWidgetKind::Graph => Some(UiSurfaceKind::Graph),
        UiWidgetKind::Timeline => Some(UiSurfaceKind::Timeline),
        UiWidgetKind::Table => Some(UiSurfaceKind::Table),
        UiWidgetKind::Tree => Some(UiSurfaceKind::Tree),
        UiWidgetKind::Viewport2D => Some(UiSurfaceKind::Viewport2D),
        UiWidgetKind::Viewport3D => Some(UiSurfaceKind::Viewport3D),
        UiWidgetKind::Overlay => Some(UiSurfaceKind::Overlay),
        _ => None,
    }
}

fn is_canvas_surface_element(tag: &str, node: &UiNode) -> bool {
    matches!(
        tag.trim().to_ascii_lowercase().as_str(),
        "canvas" | "gpu" | "gpu_surface" | "shader_surface"
    ) || surface_shader_binding_for_node(node).is_some()
}

fn surface_shader_binding_for_node(node: &UiNode) -> Option<UiSurfaceShaderBinding> {
    let shader_ref = first_string_prop(
        node,
        &[
            "shader_ref",
            "shader",
            "shader_bundle_ref",
            "shader_ref_key",
            "surface_shader",
        ],
    )?;

    Some(UiSurfaceShaderBinding {
        shader_ref,
        entry_point: first_string_prop(
            node,
            &["shader_entry", "shader_entry_point", "surface_shader_entry"],
        ),
        stage: first_string_prop(node, &["shader_stage", "surface_shader_stage"]),
        derived_format: first_string_prop(
            node,
            &["shader_format", "shader_payload", "surface_shader_format"],
        ),
    })
}

fn surface_renderer_preference_for_node(
    node: &UiNode,
    shader: Option<&UiSurfaceShaderBinding>,
    kind: &UiSurfaceKind,
) -> UiSurfaceRendererPreference {
    if let Some(explicit) = first_string_prop(
        node,
        &[
            "renderer",
            "surface_renderer",
            "gpu_renderer",
            "ui_renderer",
        ],
    )
    .and_then(parse_surface_renderer_preference)
    {
        return explicit;
    }

    if shader.is_some() {
        return UiSurfaceRendererPreference::Shader;
    }

    match kind {
        UiSurfaceKind::Canvas | UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D => {
            UiSurfaceRendererPreference::Wgpu
        }
        _ => UiSurfaceRendererPreference::Auto,
    }
}

fn surface_composition_mode_for_node(
    node: &UiNode,
    shader: Option<&UiSurfaceShaderBinding>,
    kind: &UiSurfaceKind,
) -> UiSurfaceCompositionMode {
    if let Some(explicit) = first_string_prop(
        node,
        &[
            "composition",
            "surface_composition",
            "gpu_composition",
            "surface_mode",
        ],
    )
    .and_then(parse_surface_composition_mode)
    {
        return explicit;
    }

    if shader.is_some() {
        return UiSurfaceCompositionMode::ShaderCanvas;
    }

    match kind {
        UiSurfaceKind::Canvas => UiSurfaceCompositionMode::ShaderCanvas,
        UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D => UiSurfaceCompositionMode::Viewport,
        UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay => {
            UiSurfaceCompositionMode::LayeredGpu
        }
        _ => UiSurfaceCompositionMode::Host,
    }
}

fn surface_host_backend_for_node(node: &UiNode, kind: &UiSurfaceKind) -> UiHostBackendKind {
    if let Some(explicit) = first_string_prop(
        node,
        &[
            "host_backend",
            "surface_backend",
            "ui_backend",
            "panel_backend",
        ],
    )
    .and_then(parse_surface_host_backend)
    {
        return explicit;
    }

    match kind {
        UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay => {
            UiHostBackendKind::Imgui
        }
        UiSurfaceKind::Canvas | UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D => {
            UiHostBackendKind::Qt
        }
        _ => UiHostBackendKind::Auto,
    }
}

fn surface_layout_engine_for_node(node: &UiNode) -> UiLayoutEngineKind {
    first_string_prop(node, &["layout_engine", "surface_layout_engine", "ui_layout_engine"])
        .and_then(parse_surface_layout_engine)
        .unwrap_or(UiLayoutEngineKind::Yoga)
}

fn surface_render_engine_for_node(
    node: &UiNode,
    shader: Option<&UiSurfaceShaderBinding>,
    kind: &UiSurfaceKind,
    renderer_preference: UiSurfaceRendererPreference,
) -> UiRenderEngineKind {
    if let Some(explicit) = first_string_prop(
        node,
        &[
            "render_engine",
            "surface_render_engine",
            "paint_engine",
            "ui_render_engine",
        ],
    )
    .and_then(parse_surface_render_engine)
    {
        return explicit;
    }

    if shader.is_some() {
        return UiRenderEngineKind::Shader;
    }

    match renderer_preference {
        UiSurfaceRendererPreference::Wgpu => UiRenderEngineKind::Wgpu,
        UiSurfaceRendererPreference::Shader => UiRenderEngineKind::Shader,
        UiSurfaceRendererPreference::Dom => UiRenderEngineKind::Browser,
        UiSurfaceRendererPreference::Native => UiRenderEngineKind::Native,
        UiSurfaceRendererPreference::Auto => match kind {
            UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay => {
                UiRenderEngineKind::Skia
            }
            _ => UiRenderEngineKind::Auto,
        },
    }
}

fn surface_gpu_backing_required(
    node: &UiNode,
    shader: Option<&UiSurfaceShaderBinding>,
    kind: &UiSurfaceKind,
    renderer_preference: UiSurfaceRendererPreference,
    composition_mode: UiSurfaceCompositionMode,
) -> bool {
    if let Some(explicit) = first_bool_prop(node, &["gpu", "gpu_backed", "surface_gpu"]) {
        return explicit;
    }

    shader.is_some()
        || matches!(
            kind,
            UiSurfaceKind::Canvas | UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D
        )
        || matches!(
            renderer_preference,
            UiSurfaceRendererPreference::Wgpu | UiSurfaceRendererPreference::Shader
        )
        || !matches!(composition_mode, UiSurfaceCompositionMode::Host)
}

fn parse_surface_renderer_preference(value: String) -> Option<UiSurfaceRendererPreference> {
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiSurfaceRendererPreference::Auto),
        "native" | "host" => Some(UiSurfaceRendererPreference::Native),
        "dom" | "web" => Some(UiSurfaceRendererPreference::Dom),
        "wgpu" | "gpu" => Some(UiSurfaceRendererPreference::Wgpu),
        "shader" | "shader_canvas" => Some(UiSurfaceRendererPreference::Shader),
        _ => None,
    }
}

fn parse_surface_host_backend(value: String) -> Option<UiHostBackendKind> {
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiHostBackendKind::Auto),
        "native" | "host" => Some(UiHostBackendKind::Native),
        "legacy_egui" | "legacy-egui" | "egui" => Some(UiHostBackendKind::LegacyEgui),
        "imgui" => Some(UiHostBackendKind::Imgui),
        "rml" | "rmlui" => Some(UiHostBackendKind::RmlUi),
        "slint" => Some(UiHostBackendKind::Slint),
        "qt" => Some(UiHostBackendKind::Qt),
        "cef" | "browser" => Some(UiHostBackendKind::Cef),
        _ => None,
    }
}

fn parse_surface_layout_engine(value: String) -> Option<UiLayoutEngineKind> {
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiLayoutEngineKind::Auto),
        "native" => Some(UiLayoutEngineKind::Native),
        "yoga" => Some(UiLayoutEngineKind::Yoga),
        "legacy_egui" | "legacy-egui" | "egui" => Some(UiLayoutEngineKind::LegacyEgui),
        _ => None,
    }
}

fn parse_surface_render_engine(value: String) -> Option<UiRenderEngineKind> {
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiRenderEngineKind::Auto),
        "native" => Some(UiRenderEngineKind::Native),
        "skia" => Some(UiRenderEngineKind::Skia),
        "wgpu" | "gpu" => Some(UiRenderEngineKind::Wgpu),
        "shader" | "shader_canvas" => Some(UiRenderEngineKind::Shader),
        "browser" | "dom" | "web" => Some(UiRenderEngineKind::Browser),
        "legacy_egui" | "legacy-egui" | "egui" => Some(UiRenderEngineKind::LegacyEgui),
        _ => None,
    }
}

fn parse_surface_composition_mode(value: String) -> Option<UiSurfaceCompositionMode> {
    match value.trim().to_ascii_lowercase().as_str() {
        "host" | "retained" => Some(UiSurfaceCompositionMode::Host),
        "layered_gpu" | "gpu_layer" | "layered-gpu" => Some(UiSurfaceCompositionMode::LayeredGpu),
        "viewport" => Some(UiSurfaceCompositionMode::Viewport),
        "shader_canvas" | "shader-canvas" | "canvas" => {
            Some(UiSurfaceCompositionMode::ShaderCanvas)
        }
        _ => None,
    }
}

fn collect_native_projection_nodes(
    tree: &UiTree,
    id: UiNodeId,
    parent_id: Option<UiNodeId>,
    depth: u32,
    active_tabs: &BTreeMap<String, String>,
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
    let persistent_layout_id = Some(node_layout_id(node));
    let tab_is_active = node
        .layout
        .tab_group_id
        .as_ref()
        .and_then(|group_id| active_tabs.get(group_id))
        .map_or(true, |active_layout_id| {
            persistent_layout_id
                .as_ref()
                .is_some_and(|layout_id| active_layout_id == layout_id)
        });

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
        title: title.clone(),
        text,
        tag,
        scene,
        layout_kind: node.layout.kind,
        dock_placement: node.layout.dock,
        split_ratio: node.layout.split_ratio,
        resizable: node.layout.resizable,
        persistent_layout_id,
        tab_group_id: node.layout.tab_group_id.clone(),
        tab_label: node.layout.tab_label.clone().or_else(|| title.clone()),
        tab_order: node.layout.tab_order,
        tab_default_active: node.layout.tab_default_active,
        tab_closable: node.layout.tab_closable,
        tab_is_active,
        child_count: node.children.len(),
    });

    for child in &node.children {
        collect_native_projection_nodes(tree, *child, Some(id), depth + 1, active_tabs, projection);
    }
}

fn build_native_projection_tab_groups(
    tree: &UiTree,
    active_tabs: &BTreeMap<String, String>,
) -> Vec<UiNativeProjectionTabGroup> {
    let mut counts = BTreeMap::<String, usize>::new();

    for node in tree.nodes.values() {
        if let Some(group_id) = node.layout.tab_group_id.clone() {
            *counts.entry(group_id).or_default() += 1;
        }
    }

    counts
        .into_iter()
        .map(|(id, tab_count)| UiNativeProjectionTabGroup {
            active_tab_layout_id: active_tabs.get(&id).cloned(),
            id,
            tab_count,
        })
        .collect()
}

fn node_prop_string(node: &UiNode, key: &str) -> Option<String> {
    node.props.get(key).and_then(|value| match value {
        UiValue::String(value) => Some(value.clone()),
        _ => None,
    })
}

fn first_string_prop(node: &UiNode, keys: &[&str]) -> Option<String> {
    keys.iter().find_map(|key| node_prop_string(node, key))
}

fn first_bool_prop(node: &UiNode, keys: &[&str]) -> Option<bool> {
    keys.iter().find_map(|key| {
        node.props.get(*key).and_then(|value| match value {
            UiValue::Bool(value) => Some(*value),
            UiValue::Int(value) => Some(*value != 0),
            UiValue::Float(value) => Some(value.abs() > f64::EPSILON),
            UiValue::String(value) => match value.trim().to_ascii_lowercase().as_str() {
                "1" | "true" | "yes" | "on" => Some(true),
                "0" | "false" | "no" | "off" => Some(false),
                _ => None,
            },
            UiValue::Null => None,
        })
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
        let systems = ui_runtime_systems_from_tree(&self.tree);
        UiBuildOutput {
            tree: self.tree,
            patches: self.patches,
            systems,
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
        root.identity_key = Some("workspace-root".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("workspace-main".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut child = UiNode::new(child_id, UiWidgetKind::Inspector);
        child.focus_scope = Some("inspector".to_string());
        child.selection_scope = Some("selection".to_string());
        child.style.theme_scope = Some("studio".to_string());
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
        assert_eq!(build.systems.computed.len(), 1);
        assert_eq!(
            build.systems.focus_graph.default_scope.as_deref(),
            Some("inspector")
        );
        assert_eq!(
            build.systems.selection_model.active_scope.as_deref(),
            Some("selection")
        );
        assert_eq!(build.systems.theme_registry.scopes.len(), 1);
        assert_eq!(build.systems.workspace_layout.roots.len(), 1);
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
        let metadata = UiRuntimeMetadata {
            app_name: Some("studio-shell".to_string()),
            window_title: "Studio Shell".to_string(),
            root_component: "App".to_string(),
            source_file_name: Some("studio.kn".to_string()),
            initial_window_size: [1600.0, 900.0],
            ..UiRuntimeMetadata::default()
        };

        let bundle = ui_runtime_bundle_from_output(metadata.clone(), output.clone());

        let json = ui_runtime_bundle_to_json(&bundle).expect("bundle should serialize");
        let json_value: serde_json::Value =
            serde_json::from_str(&json).expect("bundle json should parse");
        let decoded = ui_runtime_bundle_from_json(&json).expect("bundle should deserialize");

        assert_eq!(decoded.schema_version, UI_RUNTIME_BUNDLE_SCHEMA_VERSION);
        assert_eq!(decoded.metadata.root_component, "App");
        assert_eq!(decoded.output, bundle.output);
        assert!(decoded.native_projection.is_empty());
        assert!(
            json_value.get("native_projection").is_none(),
            "canonical bundles should omit the compatibility sidecar"
        );
        validate_ui_runtime_bundle(&decoded).expect("bundle should validate");

        let compatibility_bundle =
            ui_runtime_bundle_from_output_with_native_projection(metadata, output.clone());
        let compatibility_json = ui_runtime_bundle_to_json(&compatibility_bundle)
            .expect("compatibility bundle should serialize");
        let compatibility_value: serde_json::Value = serde_json::from_str(&compatibility_json)
            .expect("compatibility bundle json should parse");
        let compatibility_projection = compatibility_value
            .get("native_projection")
            .expect("legacy helper should materialize the compatibility sidecar");
        assert_eq!(
            compatibility_projection
                .get("root_id")
                .and_then(serde_json::Value::as_u64),
            Some(root_id.0)
        );
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
                ..UiRuntimeMetadata::default()
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
        panel.identity_key = Some("viewport-lab".to_string());
        panel
            .props
            .insert("title".to_string(), UiValue::from("Viewport Lab"));
        builder.add_node(panel);

        let mut viewport = UiNode::new(viewport_id, UiWidgetKind::Viewport3D);
        viewport.style.theme_scope = Some("viewport".to_string());
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
        assert_eq!(
            projection.primary_panel_title.as_deref(),
            Some("Viewport Lab")
        );
        assert_eq!(
            projection.primary_viewport_title.as_deref(),
            Some("Hero View")
        );
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

    #[test]
    fn workspace_layout_snapshot_preserves_active_tab_state() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let scene_id = builder.alloc_id();
        let materials_id = builder.alloc_id();
        let stats_id = builder.alloc_id();

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some("workspace".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("workspace".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut scene = UiNode::new(scene_id, UiWidgetKind::Viewport3D);
        scene.layout = UiLayoutSpec {
            kind: UiLayoutKind::Absolute,
            dock: Some(UiDockPlacement::Tab),
            persistent_layout_id: Some("scene-tab".to_string()),
            tab_group_id: Some("center-tabs".to_string()),
            tab_label: Some("Scene".to_string()),
            tab_order: Some(0),
            ..UiLayoutSpec::default()
        };
        builder.add_node(scene);

        let mut materials = UiNode::new(materials_id, UiWidgetKind::Inspector);
        materials.layout = UiLayoutSpec {
            kind: UiLayoutKind::FlexColumn,
            dock: Some(UiDockPlacement::Tab),
            persistent_layout_id: Some("materials-tab".to_string()),
            tab_group_id: Some("center-tabs".to_string()),
            tab_label: Some("Materials".to_string()),
            tab_order: Some(1),
            tab_default_active: true,
            ..UiLayoutSpec::default()
        };
        builder.add_node(materials);

        let mut stats = UiNode::new(stats_id, UiWidgetKind::Table);
        stats.layout = UiLayoutSpec {
            kind: UiLayoutKind::FlexColumn,
            dock: Some(UiDockPlacement::Tab),
            persistent_layout_id: Some("stats-tab".to_string()),
            tab_group_id: Some("center-tabs".to_string()),
            tab_label: Some("Stats".to_string()),
            tab_order: Some(2),
            ..UiLayoutSpec::default()
        };
        builder.add_node(stats);

        builder.replace_children(root_id, vec![scene_id, materials_id, stats_id]);
        builder.set_root(root_id);

        let mut build = builder.finish();
        assert_eq!(
            build
                .systems
                .workspace_layout
                .active_tabs
                .get("center-tabs"),
            Some(&"materials-tab".to_string())
        );

        let snapshot = ui_workspace_layout_snapshot(&build.tree, &build.systems);
        build
            .systems
            .workspace_layout
            .active_tabs
            .insert("center-tabs".to_string(), "scene-tab".to_string());
        let applied =
            ui_apply_workspace_layout_snapshot(&mut build.tree, &mut build.systems, &snapshot);
        assert_eq!(applied, 4);
        assert_eq!(
            build
                .systems
                .workspace_layout
                .active_tabs
                .get("center-tabs"),
            Some(&"materials-tab".to_string())
        );
    }

    #[test]
    fn native_projection_emits_tab_group_shell_metadata() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let scene_id = builder.alloc_id();
        let materials_id = builder.alloc_id();

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some("shell".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("shell".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut scene = UiNode::new(scene_id, UiWidgetKind::Viewport3D);
        scene
            .props
            .insert("title".to_string(), UiValue::from("Scene"));
        scene.layout = UiLayoutSpec {
            kind: UiLayoutKind::Absolute,
            dock: Some(UiDockPlacement::Tab),
            persistent_layout_id: Some("scene-tab".to_string()),
            tab_group_id: Some("center-tabs".to_string()),
            tab_order: Some(0),
            ..UiLayoutSpec::default()
        };
        builder.add_node(scene);

        let mut materials = UiNode::new(materials_id, UiWidgetKind::Inspector);
        materials
            .props
            .insert("title".to_string(), UiValue::from("Materials"));
        materials.layout = UiLayoutSpec {
            kind: UiLayoutKind::FlexColumn,
            dock: Some(UiDockPlacement::Tab),
            persistent_layout_id: Some("materials-tab".to_string()),
            tab_group_id: Some("center-tabs".to_string()),
            tab_order: Some(1),
            tab_default_active: true,
            tab_closable: true,
            ..UiLayoutSpec::default()
        };
        builder.add_node(materials);

        builder.replace_children(root_id, vec![scene_id, materials_id]);
        builder.set_root(root_id);

        let output = builder.finish();
        let projection = ui_native_projection_from_output(&output);
        let materials = projection
            .nodes
            .iter()
            .find(|node| node.persistent_layout_id.as_deref() == Some("materials-tab"))
            .expect("materials tab should be projected");

        assert_eq!(projection.tab_groups.len(), 1);
        assert_eq!(projection.tab_groups[0].id, "center-tabs");
        assert_eq!(
            projection.tab_groups[0].active_tab_layout_id.as_deref(),
            Some("materials-tab")
        );
        assert!(materials.tab_is_active);
        assert_eq!(materials.tab_group_id.as_deref(), Some("center-tabs"));
        assert_eq!(materials.tab_label.as_deref(), Some("Materials"));
        assert!(materials.tab_closable);
    }

    #[test]
    fn runtime_systems_derive_surfaces_and_reload_identity() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let graph_id = builder.alloc_id();

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some("shell".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            dock: Some(UiDockPlacement::Center),
            persistent_layout_id: Some("shell-layout".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut graph = UiNode::new(graph_id, UiWidgetKind::Graph);
        graph.identity_key = Some("material-graph".to_string());
        graph.style.theme_scope = Some("editor".to_string());
        graph.watches.push(UiSignalId(42));
        builder.add_node(graph);
        builder.replace_children(root_id, vec![graph_id]);
        builder.set_root(root_id);

        let build = builder.finish();

        assert_eq!(build.systems.surfaces.len(), 1);
        assert_eq!(build.systems.surfaces[0].kind, UiSurfaceKind::Graph);
        assert_eq!(
            build.systems.surfaces[0].composition_mode,
            UiSurfaceCompositionMode::LayeredGpu
        );
        assert_eq!(build.systems.animation_tracks.len(), 1);
        assert_eq!(build.systems.hot_reload.identity_aliases.len(), 2);
        assert_eq!(
            build.systems.workspace_layout.persistence_key.as_deref(),
            Some("shell-layout")
        );
        assert!(build
            .systems
            .scheduler
            .pending
            .iter()
            .any(|entry| entry.phase == UiSchedulerPhase::Signals));
    }

    #[test]
    fn runtime_bundle_backfills_runtime_systems_for_manual_output() {
        let root_id = UiNodeId(1);
        let mut tree = UiTree::default();
        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some("manual-root".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            ..UiLayoutSpec::default()
        };
        tree.nodes.insert(root_id, root);
        tree.root = Some(root_id);

        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                app_name: Some("manual".to_string()),
                window_title: "Manual".to_string(),
                root_component: "App".to_string(),
                source_file_name: None,
                initial_window_size: [1280.0, 720.0],
                ..UiRuntimeMetadata::default()
            },
            UiBuildOutput {
                tree,
                patches: vec![UiPatch::SetRoot { id: root_id }],
                systems: UiRuntimeSystems::default(),
            },
        );

        assert!(!bundle.output.systems.is_empty());
        assert_eq!(bundle.output.systems.workspace_layout.roots.len(), 1);
    }

    #[test]
    fn canvas_elements_with_shader_refs_promote_to_gpu_surfaces() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let canvas_id = builder.alloc_id();

        let root = UiNode::new(root_id, UiWidgetKind::Panel);
        builder.add_node(root);

        let mut canvas = UiNode::new(canvas_id, UiWidgetKind::Element("canvas".to_string()));
        canvas
            .props
            .insert("title".to_string(), UiValue::from("Hero Surface"));
        canvas
            .props
            .insert("shader_ref".to_string(), UiValue::from("ui.hero_surface"));
        canvas
            .props
            .insert("shader_stage".to_string(), UiValue::from("fragment"));
        canvas
            .props
            .insert("shader_format".to_string(), UiValue::from("wgsl"));
        builder.add_node(canvas);
        builder.replace_children(root_id, vec![canvas_id]);
        builder.set_root(root_id);

        let build = builder.finish();
        let surface = build
            .systems
            .surfaces
            .iter()
            .find(|surface| surface.node == canvas_id)
            .expect("canvas node should become a runtime surface");

        assert_eq!(surface.kind, UiSurfaceKind::Canvas);
        assert_eq!(surface.title.as_deref(), Some("Hero Surface"));
        assert_eq!(
            surface.renderer_preference,
            UiSurfaceRendererPreference::Shader
        );
        assert_eq!(
            surface.composition_mode,
            UiSurfaceCompositionMode::ShaderCanvas
        );
        assert!(surface.gpu_backing_required);
        assert_eq!(
            surface
                .shader
                .as_ref()
                .map(|binding| binding.shader_ref.as_str()),
            Some("ui.hero_surface")
        );
        assert_eq!(
            surface
                .shader
                .as_ref()
                .and_then(|binding| binding.stage.as_deref()),
            Some("fragment")
        );
        assert_eq!(
            surface
                .shader
                .as_ref()
                .and_then(|binding| binding.derived_format.as_deref()),
            Some("wgsl")
        );
    }

    #[test]
    fn signal_updates_invalidate_exact_dependencies() {
        let mut systems = UiRuntimeSystems::default();
        systems.computed.push(UiComputed {
            id: "selection.computed".to_string(),
            label: "selection".to_string(),
            depends_on: vec![UiSignalId(7)],
            writes_signal: None,
            expr: None,
            invalidates_nodes: vec![UiNodeId(11), UiNodeId(12)],
            scheduler_phase: UiSchedulerPhase::Signals,
        });

        let result = ui_execute_signal_updates(
            &mut systems,
            &[UiSignalUpdate {
                signal: UiSignalId(7),
                value: UiValue::Int(3),
            }],
        );

        assert_eq!(result.changed_signals, vec![UiSignalId(7)]);
        assert_eq!(result.invalidated_nodes, vec![UiNodeId(11), UiNodeId(12)]);
        assert_eq!(systems.transactions.len(), 1);
        assert!(systems
            .scheduler
            .pending
            .iter()
            .any(|entry| entry.label == "selection"));
    }

    #[test]
    fn theme_resolution_and_diff_detect_style_changes() {
        let mut registry = UiThemeRegistry::default();
        registry.scopes.push(UiThemeScope {
            name: "studio".to_string(),
            selector: "scope:studio".to_string(),
            parent: Some("base".to_string()),
        });
        registry.scopes.push(UiThemeScope {
            name: "base".to_string(),
            selector: "scope:base".to_string(),
            parent: None,
        });
        registry.semantic_tokens.push(UiThemeToken {
            name: "surface.background".to_string(),
            category: "color".to_string(),
            value: UiValue::String("#111111".to_string()),
        });
        registry.variants.push(UiThemeVariant {
            scope: "studio".to_string(),
            name: "base".to_string(),
            tokens: vec!["surface.background".to_string()],
        });

        let mut node = UiNode::new(UiNodeId(1), UiWidgetKind::Panel);
        node.style.theme_scope = Some("studio".to_string());
        let before = ui_resolve_theme_for_node(&node, &registry);

        node.style.values.insert(
            "surface.background".to_string(),
            UiValue::String("#222222".to_string()),
        );
        let after = ui_resolve_theme_for_node(&node, &registry);
        let diff = ui_diff_resolved_theme(&before, &after);

        assert_eq!(
            before.scope_chain,
            vec!["studio".to_string(), "base".to_string()]
        );
        assert!(!diff.is_empty());
        assert!(diff
            .changes
            .iter()
            .any(|entry| entry.key == "surface.background"));
    }

    #[test]
    fn workspace_layout_solver_and_snapshot_round_trip() {
        let mut builder = UiTreeBuilder::new();
        let root_id = builder.alloc_id();
        let left_id = builder.alloc_id();
        let center_id = builder.alloc_id();

        let mut root = UiNode::new(root_id, UiWidgetKind::Panel);
        root.identity_key = Some("workspace".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("workspace".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(root);

        let mut left = UiNode::new(left_id, UiWidgetKind::Inspector);
        left.identity_key = Some("left-pane".to_string());
        left.layout = UiLayoutSpec {
            kind: UiLayoutKind::FlexColumn,
            dock: Some(UiDockPlacement::Left),
            split_ratio: Some(0.3),
            persistent_layout_id: Some("left-pane".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(left);

        let mut center = UiNode::new(center_id, UiWidgetKind::Graph);
        center.identity_key = Some("center-pane".to_string());
        center.layout = UiLayoutSpec {
            kind: UiLayoutKind::Absolute,
            dock: Some(UiDockPlacement::Center),
            persistent_layout_id: Some("center-pane".to_string()),
            ..UiLayoutSpec::default()
        };
        builder.add_node(center);

        builder.replace_children(root_id, vec![left_id, center_id]);
        builder.set_root(root_id);

        let mut build = builder.finish();
        let resolved = ui_solve_workspace_layout(&build.tree, &build.systems, [1000.0, 800.0]);
        let left_layout = resolved
            .nodes
            .iter()
            .find(|entry| entry.node == left_id)
            .expect("left layout should exist");
        assert!((left_layout.rect.width - 300.0).abs() < 0.5);

        let snapshot = ui_workspace_layout_snapshot(&build.tree, &build.systems);
        if let Some(node) = build.tree.node_mut(left_id) {
            node.layout.dock = Some(UiDockPlacement::Right);
            node.layout.split_ratio = Some(0.2);
        }
        let applied =
            ui_apply_workspace_layout_snapshot(&mut build.tree, &mut build.systems, &snapshot);
        assert_eq!(applied, 3);
        assert_eq!(
            build.tree.node(left_id).and_then(|node| node.layout.dock),
            Some(UiDockPlacement::Left)
        );
        assert_eq!(
            build
                .tree
                .node(left_id)
                .and_then(|node| node.layout.split_ratio),
            Some(0.3)
        );
    }

    #[test]
    fn animation_runtime_advances_tracks_to_completion() {
        let mut systems = UiRuntimeSystems::default();
        systems.animation_tracks.push(UiAnimationTrack {
            id: "fade".to_string(),
            target: UiNodeId(9),
            property: "opacity".to_string(),
            duration_ms: 100,
            trigger: UiAnimationTrigger::Mount,
            easing: UiEasingKind::EaseOut,
            preserve_on_reload: true,
        });

        let first = ui_step_animation_runtime(&mut systems, 40);
        let second = ui_step_animation_runtime(&mut systems, 60);

        assert_eq!(first.len(), 1);
        assert!(first[0].eased_progress > first[0].progress);
        assert_eq!(second[0].progress, 1.0);
        assert!(second[0].completed);
        assert!(systems
            .animation_state
            .get("fade")
            .is_some_and(|state| state.completed));
    }

    #[test]
    fn hot_reload_transfer_preserves_runtime_state() {
        let mut previous_builder = UiTreeBuilder::new();
        let previous_root = previous_builder.alloc_id();
        let previous_graph = previous_builder.alloc_id();

        let mut root = UiNode::new(previous_root, UiWidgetKind::Panel);
        root.identity_key = Some("shell".to_string());
        root.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("shell".to_string()),
            ..UiLayoutSpec::default()
        };
        previous_builder.add_node(root);

        let mut graph = UiNode::new(previous_graph, UiWidgetKind::Graph);
        graph.identity_key = Some("graph".to_string());
        graph.focus_scope = Some("selection".to_string());
        graph.selection_scope = Some("selection".to_string());
        previous_builder.add_node(graph);
        previous_builder.replace_children(previous_root, vec![previous_graph]);
        previous_builder.set_root(previous_root);
        let mut previous = previous_builder.finish();
        previous.systems.focus_graph.default_scope = Some("selection".to_string());
        previous.systems.selection_model.active_scope = Some("selection".to_string());
        previous
            .systems
            .session_state
            .insert("tab".to_string(), UiValue::String("materials".to_string()));
        previous.systems.animation_state.insert(
            "animation.node.2".to_string(),
            UiAnimationPlaybackState {
                elapsed_ms: 90,
                progress: 0.5,
                eased_progress: 0.75,
                completed: false,
            },
        );

        let mut next_builder = UiTreeBuilder::new();
        let next_root = next_builder.alloc_id();
        let next_graph = next_builder.alloc_id();
        let mut next_root_node = UiNode::new(next_root, UiWidgetKind::Panel);
        next_root_node.identity_key = Some("shell".to_string());
        next_root_node.layout = UiLayoutSpec {
            kind: UiLayoutKind::Dock,
            persistent_layout_id: Some("shell".to_string()),
            ..UiLayoutSpec::default()
        };
        next_builder.add_node(next_root_node);
        let mut next_graph_node = UiNode::new(next_graph, UiWidgetKind::Graph);
        next_graph_node.identity_key = Some("graph".to_string());
        next_graph_node.focus_scope = Some("selection".to_string());
        next_graph_node.selection_scope = Some("selection".to_string());
        next_builder.add_node(next_graph_node);
        next_builder.replace_children(next_root, vec![next_graph]);
        next_builder.set_root(next_root);
        let mut next = next_builder.finish();

        let report = ui_transfer_hot_reload_state(&previous, &mut next);

        assert!(report.focus_transferred);
        assert!(report.selection_transferred);
        assert!(report.docking_transferred);
        assert_eq!(report.animation_tracks_transferred, 1);
        assert_eq!(report.session_values_transferred, 1);
        assert_eq!(
            next.systems.focus_graph.default_scope.as_deref(),
            Some("selection")
        );
        assert_eq!(
            next.systems.session_state.get("tab"),
            Some(&UiValue::String("materials".to_string()))
        );
    }
}
