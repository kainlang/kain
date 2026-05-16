//! KAIN UI subsystem primitives and JSX evaluation helpers.

use crate::ast::{BinaryOp, CallArg, Component, Expr, JSXAttrValue, JSXNode, Program, UnaryOp};
use crate::diagnostics::SpanMapper;
use crate::error::KainResult;
use crate::lexer::Lexer;
use crate::parser::Parser;
use crate::realtime_app_bundle::ui_session_state_string;
use crate::runtime::{eval_expr, Env, Value};
use crate::span::Span;
use kain_ui::{
    default_layout_for_tag, render_debug_tree, ui_runtime_systems_from_tree, widget_kind_for_tag,
    UiAnimationTrack, UiAnimationTrigger, UiBuildOutput, UiComputed, UiDerivedExpr, UiDockNode,
    UiDockPlacement, UiEasingKind, UiEventPhase, UiEventRoute, UiHostBackendKind,
    UiLayoutAlignment, UiLayoutEngineKind, UiLength, UiLengthUnit, UiNode, UiOverflowBehavior,
    UiRenderEngineKind, UiSchedulerPhase, UiSignalId, UiStyleState, UiSurface,
    UiSurfaceCompositionMode, UiSurfaceKind, UiSurfaceRendererPreference, UiSurfaceShaderBinding,
    UiThemeRegistry, UiThemeScope, UiThemeToken, UiThemeVariant, UiTreeBuilder, UiValue,
    UiWidgetKind,
};
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::fmt;

const UI_AUTHORING_CONTRACT_VERSION: &str = "ui_slate_x100.authoring_contract.v1";

/// Named backend targets for KAIN's UI subsystem.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum UIBackendKind {
    Runtime,
    ReactDom,
    BrowserDom,
    Slate,
}

/// Declarative backend capabilities for UI lowering and rendering.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct UIBackendProfile {
    pub kind: UIBackendKind,
    pub event_prefix: &'static str,
    pub supports_fragments: bool,
    pub supports_component_instances: bool,
    pub supports_keyed_children: bool,
    pub bool_attr_true_is_bare: bool,
    pub fragment_tag: &'static str,
}

pub const UI_BACKEND_PROFILES: &[UIBackendProfile] = &[
    UIBackendProfile {
        kind: UIBackendKind::Runtime,
        event_prefix: "on_",
        supports_fragments: true,
        supports_component_instances: true,
        supports_keyed_children: true,
        bool_attr_true_is_bare: true,
        fragment_tag: "Fragment",
    },
    UIBackendProfile {
        kind: UIBackendKind::ReactDom,
        event_prefix: "on",
        supports_fragments: true,
        supports_component_instances: true,
        supports_keyed_children: true,
        bool_attr_true_is_bare: false,
        fragment_tag: "React.Fragment",
    },
    UIBackendProfile {
        kind: UIBackendKind::BrowserDom,
        event_prefix: "on_",
        supports_fragments: true,
        supports_component_instances: false,
        supports_keyed_children: true,
        bool_attr_true_is_bare: true,
        fragment_tag: "DocumentFragment",
    },
    UIBackendProfile {
        kind: UIBackendKind::Slate,
        event_prefix: "on_",
        supports_fragments: true,
        supports_component_instances: true,
        supports_keyed_children: false,
        bool_attr_true_is_bare: false,
        fragment_tag: "SFragment",
    },
];

pub fn ui_backend_profile(kind: UIBackendKind) -> &'static UIBackendProfile {
    UI_BACKEND_PROFILES
        .iter()
        .find(|profile| profile.kind == kind)
        .unwrap_or(&UI_BACKEND_PROFILES[0])
}

/// Normalized UI attribute representation.
#[derive(Clone, Debug)]
pub enum UIAttr {
    Property {
        name: String,
        value: Value,
        expr: Option<Expr>,
    },
    Bool {
        name: String,
        value: bool,
    },
    Event {
        name: String,
        event: UIEvent,
        handler: Value,
        expr: Option<Expr>,
    },
}

/// Runtime UI event metadata.
#[derive(Clone, Debug)]
pub enum UIEvent {
    Click,
    Input,
    Change,
    Submit,
    Focus,
    Blur,
    KeyDown,
    KeyUp,
    PointerDown,
    PointerUp,
    PointerMove,
    Custom(String),
}

/// Runtime component instance snapshot.
#[derive(Clone, Debug)]
pub struct ComponentInstance {
    pub name: String,
    pub props: HashMap<String, Value>,
    pub children: Vec<VNode>,
    pub state: HashMap<String, Value>,
}

/// Runtime VDOM node used by the interpreter and JSX-capable backends.
#[derive(Clone, Debug)]
pub enum VNode {
    Element {
        tag: String,
        attrs: Vec<UIAttr>,
        children: Vec<VNode>,
        key: Option<String>,
    },
    Text(String),
    Fragment(Vec<VNode>),
    Component {
        instance: ComponentInstance,
        rendered: Box<VNode>,
    },
}

impl fmt::Display for VNode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", render_to_string(self))
    }
}

/// Render a runtime UI node into an HTML-like debug string.
pub fn render_to_string(node: &VNode) -> String {
    match node {
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } => {
            let attrs = attrs
                .iter()
                .map(render_attr_to_string)
                .filter(|part| !part.is_empty())
                .collect::<Vec<_>>()
                .join(" ");
            let attr_suffix = if attrs.is_empty() {
                String::new()
            } else {
                format!(" {}", attrs)
            };

            if children.is_empty() {
                format!("<{}{}/>", tag, attr_suffix)
            } else {
                let children = children
                    .iter()
                    .map(render_to_string)
                    .collect::<Vec<_>>()
                    .join("");
                format!("<{}{}>{}</{}>", tag, attr_suffix, children, tag)
            }
        }
        VNode::Text(text) => text.clone(),
        VNode::Fragment(children) => children
            .iter()
            .map(render_to_string)
            .collect::<Vec<_>>()
            .join(""),
        VNode::Component { rendered, .. } => render_to_string(rendered),
    }
}

/// Lightweight reconciliation hook for future diff-based rendering.
///
/// The current implementation is intentionally conservative: if the roots are
/// obviously compatible it preserves the new tree shape directly, otherwise it
/// replaces the old node. This gives the rest of the runtime a stable entry
/// point without pretending to be a full DOM reconciler yet.
pub fn reconcile(current: Option<&VNode>, next: &VNode) -> VNode {
    match (current, next) {
        (
            Some(VNode::Element { tag: old_tag, .. }),
            VNode::Element {
                tag: new_tag,
                attrs,
                children,
                key,
            },
        ) if old_tag == new_tag => VNode::Element {
            tag: new_tag.clone(),
            attrs: attrs.clone(),
            children: children.clone(),
            key: key.clone(),
        },
        (Some(VNode::Text(_)), VNode::Text(text)) => VNode::Text(text.clone()),
        (Some(VNode::Fragment(_)), VNode::Fragment(children)) => VNode::Fragment(children.clone()),
        (
            Some(VNode::Component {
                instance: old_instance,
                ..
            }),
            VNode::Component { instance, rendered },
        ) if old_instance.name == instance.name => VNode::Component {
            instance: instance.clone(),
            rendered: Box::new(reconcile(None, rendered)),
        },
        _ => next.clone(),
    }
}

/// Evaluate a KAIN JSX node into a runtime UI value.
pub fn eval_jsx(env: &mut Env, node: &JSXNode) -> KainResult<Value> {
    match node {
        JSXNode::Element {
            tag,
            attributes,
            children,
            ..
        } => {
            let attrs = eval_attrs(env, attributes)?;
            let children = eval_children(env, children)?;
            let key = find_attr_key(&attrs, "key");

            Ok(Value::JSX(VNode::Element {
                tag: tag.clone(),
                attrs,
                children,
                key,
            }))
        }
        JSXNode::Text(s, _) => Ok(Value::String(s.clone())),
        JSXNode::Expression(expr) => eval_expr(env, expr),
        JSXNode::Fragment(children, _) => {
            Ok(Value::JSX(VNode::Fragment(eval_children(env, children)?)))
        }
        JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let cond = eval_expr(env, condition)?;
            if value_is_truthy(&cond) {
                eval_jsx(env, then_branch)
            } else if let Some(else_branch) = else_branch {
                eval_jsx(env, else_branch)
            } else {
                Ok(Value::JSX(VNode::Fragment(Vec::new())))
            }
        }
        JSXNode::For {
            binding,
            iter,
            body,
            ..
        } => {
            let iter_value = eval_expr(env, iter)?;
            let items = match iter_value {
                Value::Array(items) => items.read().unwrap().clone(),
                Value::Tuple(items) => items,
                _ => Vec::new(),
            };

            let mut children = Vec::new();
            for item in items {
                env.push_scope();
                env.define(binding.clone(), item);
                let rendered = eval_jsx(env, body)?;
                env.pop_scope();
                flatten_value_into_children(rendered, &mut children);
            }

            Ok(Value::JSX(VNode::Fragment(children)))
        }
        JSXNode::ComponentCall {
            name,
            props,
            children,
            ..
        } => eval_component_call(env, name, props, children),
    }
}

/// Parse KAIN source, register its items, and render a root component into a
/// semantic `UiTree`.
pub fn build_ui_output_from_source(
    source: &str,
    root_component: &str,
) -> KainResult<UiBuildOutput> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<ui-source>");
    let mut program = parser.parse()?;
    crate::comptime::eval_program(&mut program)?;
    build_ui_output_from_program(&program, root_component)
}

/// Render a registered root component from an already-parsed program into a
/// semantic `UiTree`.
pub fn build_ui_output_from_program(
    program: &Program,
    root_component: &str,
) -> KainResult<UiBuildOutput> {
    let mut env = Env::new();
    env.register_program_items(program)?;

    let root = JSXNode::ComponentCall {
        name: root_component.to_string(),
        props: Vec::new(),
        children: Vec::new(),
        span: Span::default(),
    };

    let rendered = eval_jsx(&mut env, &root)?;
    let mut output = lower_value_to_ui_tree(rendered);
    append_global_ui_authoring_contracts(&mut output, &env);
    Ok(output)
}

/// Lower any interpreter value into a semantic `UiTree`.
pub fn lower_value_to_ui_tree(value: Value) -> UiBuildOutput {
    let root = coerce_value_to_vnode(value);
    lower_vnode_to_ui_tree(&root)
}

/// Lower a runtime VNode tree into the semantic `kain-ui` tree.
pub fn lower_vnode_to_ui_tree(root: &VNode) -> UiBuildOutput {
    let mut lowering = UiLowering::default();
    let root_id = lower_vnode_into_tree(&mut lowering, root)
        .unwrap_or_else(|| allocate_empty_root(&mut lowering.builder));
    lowering.builder.set_root(root_id);

    let mut output = lowering.builder.finish();
    if !theme_registry_is_empty(&lowering.authored_theme) {
        output.systems.theme_registry =
            merge_authored_theme_registry(&output.systems.theme_registry, lowering.authored_theme);
    }
    lowering.authored_systems.apply_to_output(&mut output);
    output
}

/// Render the semantic tree into a text debug view.
pub fn render_ui_output_debug(output: &UiBuildOutput) -> String {
    render_debug_tree(&output.tree)
}

fn eval_component_call(
    env: &mut Env,
    name: &str,
    props: &[crate::ast::JSXAttribute],
    children: &[JSXNode],
) -> KainResult<Value> {
    let attrs = eval_attrs(env, props)?;
    let rendered_children = eval_children(env, children)?;
    let props_map = attrs_to_props_map(&attrs);

    if let Some(component) = env.lookup_component(name).cloned() {
        let (rendered, state) =
            render_component_definition(env, &component, &props_map, &rendered_children)?;
        let instance = ComponentInstance {
            name: name.to_string(),
            props: props_map,
            children: rendered_children,
            state,
        };

        Ok(Value::JSX(VNode::Component {
            instance,
            rendered: Box::new(rendered),
        }))
    } else {
        let instance = ComponentInstance {
            name: name.to_string(),
            props: props_map,
            children: rendered_children.clone(),
            state: HashMap::new(),
        };

        Ok(Value::JSX(VNode::Component {
            instance,
            rendered: Box::new(VNode::Fragment(rendered_children)),
        }))
    }
}

fn render_component_definition(
    env: &mut Env,
    component: &Component,
    props: &HashMap<String, Value>,
    rendered_children: &[VNode],
) -> KainResult<(VNode, HashMap<String, Value>)> {
    env.push_scope();
    let result = (|| {
        for param in &component.props {
            let value = if let Some(value) = props.get(&param.name) {
                value.clone()
            } else if let Some(default) = &param.default {
                eval_expr(env, default)?
            } else {
                Value::None
            };
            env.define(param.name.clone(), value);
        }

        env.define(
            "children".to_string(),
            Value::JSX(VNode::Fragment(rendered_children.to_vec())),
        );

        let mut state = HashMap::new();
        for declaration in &component.state {
            let initial = eval_expr(env, &declaration.initial)?;
            env.define(declaration.name.clone(), initial.clone());
            state.insert(declaration.name.clone(), initial);
        }

        let rendered = eval_jsx(env, &component.body)?;
        Ok((coerce_value_to_vnode(rendered), state))
    })();
    env.pop_scope();
    result
}

fn coerce_value_to_vnode(value: Value) -> VNode {
    match value {
        Value::JSX(node) => node,
        other => {
            let mut children = Vec::new();
            flatten_value_into_children(other, &mut children);
            match children.len() {
                0 => VNode::Fragment(Vec::new()),
                1 => children.into_iter().next().unwrap(),
                _ => VNode::Fragment(children),
            }
        }
    }
}

struct UiLowering {
    builder: UiTreeBuilder,
    authored_theme: UiThemeRegistry,
    authored_systems: AuthoredUiSystemsAccumulator,
}

impl Default for UiLowering {
    fn default() -> Self {
        Self {
            builder: UiTreeBuilder::new(),
            authored_theme: UiThemeRegistry {
                active_theme: None,
                ..UiThemeRegistry::default()
            },
            authored_systems: AuthoredUiSystemsAccumulator::default(),
        }
    }
}

#[derive(Debug, Clone)]
struct AuthoredComputedSpec {
    pub id: String,
    pub label: String,
    pub depends_on: Vec<String>,
    pub writes_signal: Option<String>,
    pub expr: Option<Expr>,
    pub invalidates: Vec<String>,
    pub scheduler_phase: UiSchedulerPhase,
}

#[derive(Default)]
struct AuthoredUiSystemsAccumulator {
    focus_scopes: BTreeSet<String>,
    focus_default_scope: Option<String>,
    selection_scopes: BTreeSet<String>,
    selection_default_scope: Option<String>,
    event_routes: Vec<UiEventRoute>,
    animation_tracks: Vec<UiAnimationTrack>,
    surfaces: Vec<UiSurface>,
    computed_specs: Vec<AuthoredComputedSpec>,
    signal_values: BTreeMap<UiSignalId, UiValue>,
    session_state: BTreeMap<String, UiValue>,
    workspace_persistence_key: Option<String>,
    workspace_virtualization_enabled: Option<bool>,
}

impl AuthoredUiSystemsAccumulator {
    fn apply_to_output(&mut self, output: &mut UiBuildOutput) {
        // Scopes
        for scope in self.focus_scopes.iter().cloned() {
            if !output.systems.focus_graph.scopes.contains(&scope) {
                output.systems.focus_graph.scopes.push(scope);
            }
        }
        if output.systems.focus_graph.default_scope.is_none() {
            output.systems.focus_graph.default_scope = self.focus_default_scope.clone();
        }

        for scope in self.selection_scopes.iter().cloned() {
            if !output.systems.selection_model.scopes.contains(&scope) {
                output.systems.selection_model.scopes.push(scope);
            }
        }
        if output.systems.selection_model.active_scope.is_none() {
            output.systems.selection_model.active_scope = self.selection_default_scope.clone();
        }

        // Events
        for route in self.event_routes.drain(..) {
            output.systems.event_routes.push(route);
        }

        // Motion
        for track in self.animation_tracks.drain(..) {
            output.systems.animation_tracks.push(track);
        }

        // Surfaces
        for surface in self.surfaces.drain(..) {
            output.systems.surfaces.push(surface);
        }

        // Signals
        for (id, value) in std::mem::take(&mut self.signal_values) {
            output.systems.signal_values.insert(id, value);
        }

        // Session state metadata (explicit contract surfaces that don't fit in scalar node props).
        for (key, value) in std::mem::take(&mut self.session_state) {
            output.systems.session_state.insert(key, value);
        }

        // Computed declarations can reference nodes and signals by stable authoring keys, so
        // resolve them once the full tree and session-state contract keys are available.
        let computed_specs = std::mem::take(&mut self.computed_specs);
        if !computed_specs.is_empty() {
            let node_ref_index = build_node_ref_index(output);
            let node_contract_index = build_node_contract_index(output);
            let mut computed_registry = Vec::new();

            for spec in computed_specs {
                let resolved = resolve_authored_computed_spec(&spec, &node_ref_index);
                computed_registry.push(build_computed_contract_entry(
                    &spec,
                    &resolved,
                    output,
                    &node_contract_index,
                ));
                output.systems.computed.push(resolved);
            }

            if let Some(json) = serialize_contract_json(&computed_registry) {
                output.systems.session_state.insert(
                    "ui.contract.computed_registry.json".to_string(),
                    UiValue::String(json),
                );
            }
        }

        if !output.systems.event_routes.is_empty() {
            let node_contract_index = build_node_contract_index(output);
            let event_routes = build_event_route_contracts(
                &output.systems.event_routes,
                output,
                &node_contract_index,
            );
            if let Some(json) = serialize_contract_json(&event_routes) {
                output.systems.session_state.insert(
                    "ui.contract.event_routes.json".to_string(),
                    UiValue::String(json),
                );
            }
        }

        if let Some(json) = serialize_workspace_layout_contract(&output.systems.workspace_layout) {
            output.systems.session_state.insert(
                "ui.contract.workspace_layout.json".to_string(),
                UiValue::String(json),
            );
        }

        self.apply_workspace_contract(output);
        ensure_compiler_owned_ui_contract_version(output);
        self.apply_compat_backfill(output);
    }

    fn apply_workspace_contract(&self, output: &mut UiBuildOutput) {
        if output.systems.workspace_layout.persistence_key.is_none() {
            output.systems.workspace_layout.persistence_key =
                self.workspace_persistence_key.clone();
        }
        if !output.systems.workspace_layout.virtualization_enabled {
            if let Some(enabled) = self.workspace_virtualization_enabled {
                output.systems.workspace_layout.virtualization_enabled = enabled;
            }
        }

        if output.systems.workspace_layout.roots.is_empty() {
            let mut roots = Vec::new();
            for node in output.tree.nodes.values() {
                let Some(placement) = node.layout.dock else {
                    continue;
                };
                roots.push(UiDockNode {
                    id: node_layout_id(node),
                    node: node.id,
                    placement,
                    split_ratio: node.layout.split_ratio,
                    children: node.children.clone(),
                    persistent_layout_id: node.layout.persistent_layout_id.clone(),
                });
            }
            if !roots.is_empty() {
                output.systems.workspace_layout.roots = roots;
            }
        }

        if output.systems.workspace_layout.active_tabs.is_empty() {
            output.systems.workspace_layout.active_tabs =
                resolve_active_tabs_from_tree(&output.tree);
        }
    }

    fn apply_compat_backfill(&self, output: &mut UiBuildOutput) {
        // Compatibility-only backfill. Compiler-emitted runtime truth now wins by default;
        // legacy tree inference should only run when the output is still genuinely empty
        // or when a caller explicitly asks for the old bridge path.
        let force_legacy_backfill = matches!(
            output
                .systems
                .session_state
                .get("ui.runtime.force_compatibility_backfill"),
            Some(UiValue::Bool(true))
        );
        let has_authored_contract = output
            .systems
            .session_state
            .contains_key("ui.contract.version");
        let needs_legacy_backfill = force_legacy_backfill
            || (!has_authored_contract && !compiler_emitted_runtime_truth_exists(output));
        if !needs_legacy_backfill {
            return;
        }

        output.systems.session_state.insert(
            "ui.runtime.compatibility_fallback".to_string(),
            UiValue::Bool(true),
        );
        output.systems.session_state.insert(
            "ui.runtime.compatibility_mode".to_string(),
            UiValue::String("legacy_tree_inference".to_string()),
        );

        let inferred = ui_runtime_systems_from_tree(&output.tree);

        if output.systems.workspace_layout.roots.is_empty() {
            output.systems.workspace_layout.roots = inferred.workspace_layout.roots;
        }
        if output.systems.workspace_layout.persistence_key.is_none() {
            output.systems.workspace_layout.persistence_key =
                inferred.workspace_layout.persistence_key;
        }
        if output.systems.workspace_layout.active_tabs.is_empty() {
            output.systems.workspace_layout.active_tabs = inferred.workspace_layout.active_tabs;
        }

        if output.systems.focus_graph.scopes.is_empty() && !inferred.focus_graph.scopes.is_empty() {
            output.systems.focus_graph = inferred.focus_graph;
        }
        if output.systems.selection_model.scopes.is_empty()
            && !inferred.selection_model.scopes.is_empty()
        {
            output.systems.selection_model = inferred.selection_model;
        }
        if output.systems.surfaces.is_empty() && !inferred.surfaces.is_empty() {
            output.systems.surfaces = inferred.surfaces;
        }
        if output.systems.animation_tracks.is_empty() && !inferred.animation_tracks.is_empty() {
            output.systems.animation_tracks = inferred.animation_tracks;
        }
        if output.systems.theme_registry.scopes.is_empty()
            && !inferred.theme_registry.scopes.is_empty()
        {
            output.systems.theme_registry = inferred.theme_registry;
        }
    }
}

fn build_node_ref_index(output: &UiBuildOutput) -> HashMap<String, kain_ui::UiNodeId> {
    let mut index = HashMap::new();
    for (id, node) in &output.tree.nodes {
        if let Some(key) = node.identity_key.as_deref() {
            let canonical = canonical_node_contract_key(key);
            index.insert(key.to_string(), *id);
            index.insert(canonical.clone(), *id);
            index.insert(format!("ui.node::{canonical}"), *id);
        }
        if let Some(persistent_layout_id) = node.layout.persistent_layout_id.as_deref() {
            let canonical = canonical_node_contract_key(persistent_layout_id);
            index.insert(persistent_layout_id.to_string(), *id);
            index.insert(canonical.clone(), *id);
            index.insert(format!("ui.node::{canonical}"), *id);
        }
        let layout_id = node_layout_id(node);
        let canonical = canonical_node_contract_key(&layout_id);
        index.insert(layout_id.clone(), *id);
        index.insert(canonical.clone(), *id);
        index.insert(format!("ui.node::{canonical}"), *id);
    }
    index
}

fn resolve_authored_computed_spec(
    spec: &AuthoredComputedSpec,
    node_ref_index: &HashMap<String, kain_ui::UiNodeId>,
) -> UiComputed {
    let signal_index = resolve_signal_index_from_spec(spec);
    let depends_on = spec
        .depends_on
        .iter()
        .map(|entry| resolve_signal_ref(entry))
        .collect::<Vec<_>>();
    let writes_signal = spec
        .writes_signal
        .as_deref()
        .map(resolve_signal_ref)
        .or_else(|| spec.expr.as_ref().map(|_| resolve_signal_ref(&spec.id)));
    let expr = spec
        .expr
        .as_ref()
        .and_then(|expr| lower_authored_derived_expr(expr, &signal_index));
    let invalidates_nodes = spec
        .invalidates
        .iter()
        .filter_map(|entry| parse_node_ref(entry, node_ref_index))
        .collect::<Vec<_>>();
    UiComputed {
        id: spec.id.clone(),
        label: spec.label.clone(),
        depends_on,
        writes_signal,
        expr,
        invalidates_nodes,
        scheduler_phase: spec.scheduler_phase,
    }
}

fn resolve_signal_index_from_spec(spec: &AuthoredComputedSpec) -> HashMap<String, UiSignalId> {
    let mut index = HashMap::new();
    for entry in &spec.depends_on {
        index.insert(
            canonical_signal_contract_key(entry),
            resolve_signal_ref(entry),
        );
    }
    if let Some(writes_signal) = spec.writes_signal.as_deref() {
        index.insert(
            canonical_signal_contract_key(writes_signal),
            resolve_signal_ref(writes_signal),
        );
    }
    index.insert(
        canonical_signal_contract_key(&spec.id),
        resolve_signal_ref(&spec.id),
    );
    index
}

fn parse_node_ref(
    entry: &str,
    identity_index: &HashMap<String, kain_ui::UiNodeId>,
) -> Option<kain_ui::UiNodeId> {
    let trimmed = entry.trim();
    if let Ok(parsed) = trimmed.parse::<u64>() {
        return Some(kain_ui::UiNodeId(parsed));
    }
    let canonical = canonical_node_contract_key(trimmed);
    identity_index
        .get(trimmed)
        .cloned()
        .or_else(|| identity_index.get(&canonical).cloned())
        .or_else(|| {
            identity_index
                .get(&format!("ui.node::{canonical}"))
                .cloned()
        })
}

fn resolve_signal_ref(entry: &str) -> UiSignalId {
    let trimmed = entry.trim();
    if let Ok(parsed) = trimmed.parse::<u64>() {
        return UiSignalId(parsed);
    }
    UiSignalId(stable_hash_u64(&canonical_signal_contract_key(trimmed)))
}

fn canonical_signal_contract_key(entry: &str) -> String {
    let trimmed = entry.trim();
    if trimmed.starts_with("ui.signal::") {
        trimmed.to_string()
    } else if let Some(rest) = trimmed.strip_prefix("ui.signal.") {
        format!("ui.signal::{rest}")
    } else {
        format!("ui.signal::{trimmed}")
    }
}

fn canonical_node_contract_key(entry: &str) -> String {
    let trimmed = entry.trim();
    if trimmed.starts_with("ui.node::") {
        trimmed
            .strip_prefix("ui.node::")
            .unwrap_or(trimmed)
            .to_string()
    } else if let Some(rest) = trimmed.strip_prefix("ui.node.") {
        rest.to_string()
    } else {
        trimmed.to_string()
    }
}

fn build_node_contract_index(output: &UiBuildOutput) -> HashMap<kain_ui::UiNodeId, String> {
    let mut index = HashMap::new();
    for node in output.tree.nodes.values() {
        index.insert(node.id, stable_node_contract_key(node));
    }
    index
}

fn stable_node_contract_key(node: &UiNode) -> String {
    format!(
        "ui.node::{}",
        canonical_node_contract_key(&node_layout_id(node))
    )
}

fn ui_signal_contract_key(output: &UiBuildOutput, signal: UiSignalId) -> String {
    ui_session_state_string(output, &format!("ui.signal.key.{}", signal.0))
        .unwrap_or_else(|| format!("ui.signal::{}", signal.0))
}

fn ui_node_contract_key(
    output: &UiBuildOutput,
    node_contract_index: &HashMap<kain_ui::UiNodeId, String>,
    node: kain_ui::UiNodeId,
) -> String {
    node_contract_index
        .get(&node)
        .cloned()
        .or_else(|| output.tree.nodes.get(&node).map(stable_node_contract_key))
        .unwrap_or_else(|| format!("ui.node::node-{}", node.0))
}

fn build_computed_contract_entry(
    spec: &AuthoredComputedSpec,
    resolved: &UiComputed,
    output: &UiBuildOutput,
    node_contract_index: &HashMap<kain_ui::UiNodeId, String>,
) -> serde_json::Value {
    let depends_on = resolved
        .depends_on
        .iter()
        .map(|signal| ui_signal_contract_key(output, *signal))
        .collect::<Vec<_>>();
    let invalidates_nodes = resolved
        .invalidates_nodes
        .iter()
        .map(|node| ui_node_contract_key(output, node_contract_index, *node))
        .collect::<Vec<_>>();
    let expr = spec.expr.as_ref().map(render_authored_expr_contract);
    let runtime_expr = resolved
        .expr
        .as_ref()
        .map(|expr| render_runtime_derived_expr_contract(expr, output));

    serde_json::json!({
        "id": spec.id.clone(),
        "label": spec.label.clone(),
        "depends_on": depends_on,
        "writes_signal": resolved.writes_signal.map(|signal| ui_signal_contract_key(output, signal)),
        "expr": expr,
        "runtime_expr": runtime_expr,
        "invalidates_nodes": invalidates_nodes,
        "scheduler_phase": resolved.scheduler_phase,
        "expr_lowered": resolved.expr.is_some() && resolved.writes_signal.is_some(),
    })
}

fn build_event_route_contracts(
    routes: &[UiEventRoute],
    output: &UiBuildOutput,
    node_contract_index: &HashMap<kain_ui::UiNodeId, String>,
) -> Vec<serde_json::Value> {
    routes
        .iter()
        .map(|route| {
            let route_key = format!("ui.event.route.{}", route.route_id);
            let target = ui_node_contract_key(output, node_contract_index, route.target);
            let handler = ui_session_state_string(output, &format!("{route_key}.handler"))
                .or_else(|| route.handler_id.clone())
                .unwrap_or_else(|| "unknown".to_string());
            let command = ui_session_state_string(output, &format!("{route_key}.command"))
                .or_else(|| route.dispatch_command.clone());
            let transaction_label =
                ui_session_state_string(output, &format!("{route_key}.transaction"))
                    .or_else(|| route.transaction_label.clone());

            serde_json::json!({
                "route_id": route.route_id.clone(),
                "target": target,
                "event": route.event.clone(),
                "phase": route.phase,
                "handler_id": handler,
                "dispatch_command": command,
                "transaction_label": transaction_label,
                // Back-compat alias for older readers that still expect the shorter key.
                "transaction": transaction_label,
            })
        })
        .collect()
}

fn serialize_contract_json(value: &[serde_json::Value]) -> Option<String> {
    serde_json::to_string_pretty(value).ok()
}

fn serialize_workspace_layout_contract(layout: &kain_ui::UiWorkspaceLayout) -> Option<String> {
    serde_json::to_string_pretty(layout).ok()
}

fn ensure_compiler_owned_ui_contract_version(output: &mut UiBuildOutput) {
    if output
        .systems
        .session_state
        .contains_key("ui.contract.version")
    {
        return;
    }
    if !compiler_emitted_runtime_truth_exists(output) {
        return;
    }
    output.systems.session_state.insert(
        "ui.contract.version".to_string(),
        UiValue::String(UI_AUTHORING_CONTRACT_VERSION.to_string()),
    );
}

fn compiler_emitted_runtime_truth_exists(output: &UiBuildOutput) -> bool {
    !output.systems.computed.is_empty()
        || !output.systems.event_routes.is_empty()
        || !output.systems.focus_graph.scopes.is_empty()
        || output.systems.focus_graph.default_scope.is_some()
        || !output.systems.selection_model.scopes.is_empty()
        || output.systems.selection_model.active_scope.is_some()
        || !output.systems.animation_tracks.is_empty()
        || !output.systems.surfaces.is_empty()
        || !theme_registry_is_empty(&output.systems.theme_registry)
        || !output.systems.workspace_layout.roots.is_empty()
        || output.systems.workspace_layout.persistence_key.is_some()
        || output.systems.workspace_layout.virtualization_enabled
        || !output.systems.workspace_layout.active_tabs.is_empty()
        || output
            .systems
            .session_state
            .keys()
            .any(|key| key.starts_with("ui.contract.") || key.starts_with("ui.event.route."))
}

pub(crate) fn render_authored_expr_contract(expr: &Expr) -> String {
    match expr {
        Expr::Int(value, _) => value.to_string(),
        Expr::Float(value, _) => value.to_string(),
        Expr::String(value, _) => {
            serde_json::to_string(value).unwrap_or_else(|_| format!(r#""{}""#, value))
        }
        Expr::Bool(value, _) => value.to_string(),
        Expr::None(_) => "null".to_string(),
        Expr::Ident(name, _) => name.clone(),
        Expr::Paren(inner, _) => format!("({})", render_authored_expr_contract(inner)),
        Expr::Unary {
            op: UnaryOp::Not,
            operand,
            ..
        } => format!("!{}", render_authored_expr_contract(operand)),
        Expr::Binary {
            left,
            op: BinaryOp::Add,
            right,
            ..
        } => format!(
            "({} + {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Sub,
            right,
            ..
        } => format!(
            "({} - {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Mul,
            right,
            ..
        } => format!(
            "({} * {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Div,
            right,
            ..
        } => format!(
            "({} / {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Eq,
            right,
            ..
        } => format!(
            "({} == {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Ne,
            right,
            ..
        } => format!(
            "({} != {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::And,
            right,
            ..
        } => format!(
            "({} && {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Binary {
            left,
            op: BinaryOp::Or,
            right,
            ..
        } => format!(
            "({} || {})",
            render_authored_expr_contract(left),
            render_authored_expr_contract(right)
        ),
        Expr::Call { callee, args, .. } => {
            let callee_text = render_authored_expr_contract(callee);
            let rendered_args = args
                .iter()
                .map(render_authored_call_arg_contract)
                .collect::<Vec<_>>()
                .join(", ");
            format!("{callee_text}({rendered_args})")
        }
        Expr::StageCall {
            runtime,
            function,
            args,
            ..
        } => {
            let rendered_args = args
                .iter()
                .map(render_authored_call_arg_contract)
                .collect::<Vec<_>>()
                .join(", ");
            format!("{} {function}({rendered_args})", runtime.as_str())
        }
        Expr::MethodCall {
            receiver,
            method,
            args,
            ..
        } => {
            let rendered_args = args
                .iter()
                .map(render_authored_call_arg_contract)
                .collect::<Vec<_>>()
                .join(", ");
            format!(
                "{}.{method}({rendered_args})",
                render_authored_expr_contract(receiver)
            )
        }
        Expr::Field { object, field, .. } => {
            format!("{}.{}", render_authored_expr_contract(object), field)
        }
        Expr::Index { object, index, .. } => format!(
            "{}[{}]",
            render_authored_expr_contract(object),
            render_authored_expr_contract(index)
        ),
        Expr::Observe { target, body, .. } => format!(
            "observe {}: {}",
            render_authored_expr_contract(target),
            render_authored_expr_contract(body)
        ),
        Expr::Collapse { target, body, .. } => format!(
            "collapse {}: {}",
            render_authored_expr_contract(target),
            render_authored_expr_contract(body)
        ),
        Expr::Decay { target, .. } => {
            format!("decay {}", render_authored_expr_contract(target))
        }
        Expr::Teleport {
            value,
            source_world,
            target_world,
            channel,
            ..
        } => {
            let mut rendered = format!(
                "teleport {} from {} to {}",
                render_authored_expr_contract(value),
                source_world,
                target_world
            );
            if let Some(channel) = channel {
                rendered.push_str(&format!(" via {channel}"));
            }
            rendered
        }
        Expr::Lambda { params, body, .. } => {
            let rendered_params = params
                .iter()
                .map(|param| param.name.clone())
                .collect::<Vec<_>>()
                .join(", ");
            format!(
                "|{rendered_params}| {}",
                render_authored_expr_contract(body)
            )
        }
        Expr::Unary { .. } | Expr::Binary { .. } => format!("{:?}", expr),
        Expr::FString(_, _)
        | Expr::MacroCall { .. }
        | Expr::Assign { .. }
        | Expr::Struct { .. }
        | Expr::AggregateInit { .. }
        | Expr::EnumVariant { .. }
        | Expr::Array(_, _)
        | Expr::Tuple(_, _)
        | Expr::Range { .. }
        | Expr::If { .. }
        | Expr::Match { .. }
        | Expr::Ref { .. }
        | Expr::AddrOf { .. }
        | Expr::Deref(_, _)
        | Expr::PtrOffset { .. }
        | Expr::MemLoad { .. }
        | Expr::MemStore { .. }
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. }
        | Expr::Alloc { .. }
        | Expr::Realloc { .. }
        | Expr::Cast { .. }
        | Expr::Try(_, _)
        | Expr::Await(_, _)
        | Expr::AsyncBlock(_, _)
        | Expr::Spawn { .. }
        | Expr::SendMsg { .. }
        | Expr::Comptime(_, _)
        | Expr::Block(_, _)
        | Expr::JSX(_, _)
        | Expr::Return(_, _)
        | Expr::Break(_, _)
        | Expr::Continue(_) => format!("{:?}", expr),
    }
}

fn render_authored_call_arg_contract(arg: &CallArg) -> String {
    match arg.name.as_deref() {
        Some(name) => format!("{name}: {}", render_authored_expr_contract(&arg.value)),
        None => render_authored_expr_contract(&arg.value),
    }
}

fn authored_signal_contract_path(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, field, .. } => {
            authored_signal_contract_path(object).map(|prefix| format!("{prefix}.{field}"))
        }
        Expr::Index { object, index, .. } => {
            authored_signal_contract_path(object).and_then(|prefix| match &**index {
                Expr::String(value, _) => Some(format!("{prefix}.{value}")),
                Expr::Ident(value, _) => Some(format!("{prefix}.{value}")),
                _ => None,
            })
        }
        Expr::Paren(inner, _) => authored_signal_contract_path(inner),
        _ => None,
    }
}

fn render_runtime_derived_expr_contract(expr: &UiDerivedExpr, output: &UiBuildOutput) -> String {
    match expr {
        UiDerivedExpr::Literal(value) => render_ui_value_contract(value),
        UiDerivedExpr::Signal(signal) => ui_signal_contract_key(output, *signal),
        UiDerivedExpr::Not(inner) => {
            format!("!{}", render_runtime_derived_expr_contract(inner, output))
        }
        UiDerivedExpr::And(left, right) => format!(
            "({} && {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Or(left, right) => format!(
            "({} || {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Eq(left, right) => format!(
            "({} == {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Add(left, right) => format!(
            "({} + {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Sub(left, right) => format!(
            "({} - {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Mul(left, right) => format!(
            "({} * {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::Div(left, right) => format!(
            "({} / {})",
            render_runtime_derived_expr_contract(left, output),
            render_runtime_derived_expr_contract(right, output)
        ),
        UiDerivedExpr::ToString(inner) => {
            format!(
                "to_string({})",
                render_runtime_derived_expr_contract(inner, output)
            )
        }
    }
}

fn render_ui_value_contract(value: &UiValue) -> String {
    match value {
        UiValue::Null => "null".to_string(),
        UiValue::Bool(value) => value.to_string(),
        UiValue::Int(value) => value.to_string(),
        UiValue::Float(value) => value.to_string(),
        UiValue::String(value) => {
            serde_json::to_string(value).unwrap_or_else(|_| format!(r#""{}""#, value))
        }
    }
}

fn lower_authored_derived_expr(
    expr: &Expr,
    signal_index: &HashMap<String, UiSignalId>,
) -> Option<UiDerivedExpr> {
    match expr {
        Expr::Int(value, _) => Some(UiDerivedExpr::Literal(UiValue::Int(*value))),
        Expr::Float(value, _) => Some(UiDerivedExpr::Literal(UiValue::Float(*value))),
        Expr::String(value, _) => Some(UiDerivedExpr::Literal(UiValue::String(value.clone()))),
        Expr::Bool(value, _) => Some(UiDerivedExpr::Literal(UiValue::Bool(*value))),
        Expr::None(_) => Some(UiDerivedExpr::Literal(UiValue::Null)),
        Expr::Ident(..) | Expr::Field { .. } | Expr::Index { .. } => {
            authored_signal_contract_path(expr).and_then(|signal_name| {
                signal_index
                    .get(&canonical_signal_contract_key(&signal_name))
                    .cloned()
                    .map(UiDerivedExpr::Signal)
            })
        }
        Expr::Paren(inner, _) => lower_authored_derived_expr(inner, signal_index),
        Expr::Unary {
            op: UnaryOp::Not,
            operand,
            ..
        } => lower_authored_derived_expr(operand, signal_index)
            .map(|expr| UiDerivedExpr::Not(Box::new(expr))),
        Expr::Binary {
            left,
            op: BinaryOp::And,
            right,
            ..
        } => Some(UiDerivedExpr::And(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Or,
            right,
            ..
        } => Some(UiDerivedExpr::Or(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Eq,
            right,
            ..
        } => Some(UiDerivedExpr::Eq(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Ne,
            right,
            ..
        } => Some(UiDerivedExpr::Not(Box::new(UiDerivedExpr::Eq(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )))),
        Expr::Binary {
            left,
            op: BinaryOp::Add,
            right,
            ..
        } => Some(UiDerivedExpr::Add(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Sub,
            right,
            ..
        } => Some(UiDerivedExpr::Sub(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Mul,
            right,
            ..
        } => Some(UiDerivedExpr::Mul(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Binary {
            left,
            op: BinaryOp::Div,
            right,
            ..
        } => Some(UiDerivedExpr::Div(
            Box::new(lower_authored_derived_expr(left, signal_index)?),
            Box::new(lower_authored_derived_expr(right, signal_index)?),
        )),
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                if (name == "to_string" || name == "string") && args.len() == 1 {
                    return lower_authored_derived_expr(&args[0].value, signal_index)
                        .map(|expr| UiDerivedExpr::ToString(Box::new(expr)));
                }
                if name == "signal" && args.len() == 1 {
                    if let Expr::String(signal_name, _) = &args[0].value {
                        return signal_index
                            .get(&canonical_signal_contract_key(signal_name))
                            .cloned()
                            .map(UiDerivedExpr::Signal);
                    }
                }
            }
            None
        }
        Expr::MethodCall {
            receiver,
            method,
            args,
            ..
        } if args.is_empty() && (method == "to_string" || method == "toString") => {
            lower_authored_derived_expr(receiver, signal_index)
                .map(|expr| UiDerivedExpr::ToString(Box::new(expr)))
        }
        _ => None,
    }
}

fn stable_hash_u64(input: &str) -> u64 {
    // Stable FNV-1a 64-bit hash. We use this for signal ids so authored ids remain stable across
    // rebuilds even when tree structure changes.
    const OFFSET_BASIS: u64 = 0xcbf29ce484222325;
    const PRIME: u64 = 0x00000100000001B3;

    let mut hash = OFFSET_BASIS;
    for byte in input.as_bytes() {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(PRIME);
    }
    hash
}

fn lower_vnode_into_tree(lowering: &mut UiLowering, node: &VNode) -> Option<kain_ui::UiNodeId> {
    match node {
        VNode::Text(text) => {
            let id = lowering.builder.alloc_id();
            let mut ui_node = UiNode::new(id, UiWidgetKind::Text);
            ui_node
                .props
                .insert("text".to_string(), UiValue::String(text.clone()));
            lowering.builder.add_node(ui_node);
            Some(id)
        }
        VNode::Fragment(children) => {
            let child_ids = children
                .iter()
                .filter_map(|child| lower_vnode_into_tree(lowering, child))
                .collect::<Vec<_>>();
            let id = lowering.builder.alloc_id();
            let mut ui_node = UiNode::new(id, UiWidgetKind::Slot);
            ui_node.layout = default_layout_for_tag("slot");
            ui_node.children = child_ids.clone();
            lowering.builder.add_node(ui_node);
            if !child_ids.is_empty() {
                lowering.builder.replace_children(id, child_ids);
            }
            Some(id)
        }
        VNode::Element {
            tag,
            attrs,
            children,
            key,
        } => {
            if tag.eq_ignore_ascii_case("theme") {
                extract_theme_block(&mut lowering.authored_theme, attrs, children);
                return None;
            }
            if tag.eq_ignore_ascii_case("signal") {
                extract_signal_decl(&mut lowering.authored_systems, attrs);
                return None;
            }
            if tag.eq_ignore_ascii_case("computed") {
                extract_computed_decl(&mut lowering.authored_systems, attrs);
                return None;
            }
            if tag.eq_ignore_ascii_case("workspace") {
                extract_workspace_decl(&mut lowering.authored_systems, attrs);
                return None;
            }
            if tag.eq_ignore_ascii_case("focus_scope") {
                extract_focus_scope_decl(&mut lowering.authored_systems, attrs);
                return None;
            }
            if tag.eq_ignore_ascii_case("selection_scope") {
                extract_selection_scope_decl(&mut lowering.authored_systems, attrs);
                return None;
            }

            if tag.eq_ignore_ascii_case("text") {
                return Some(lower_text_element(lowering, attrs, children, key));
            }

            let child_ids = children
                .iter()
                .filter_map(|child| lower_vnode_into_tree(lowering, child))
                .collect::<Vec<_>>();
            let id = lowering.builder.alloc_id();
            let mut ui_node = UiNode::new(id, widget_kind_for_tag(tag));
            ui_node.layout = layout_from_attrs(tag, attrs);
            ui_node.children = child_ids.clone();
            ui_node
                .props
                .insert("tag".to_string(), UiValue::String(tag.clone()));
            if let Some(key) = key {
                ui_node.identity_key = Some(key.clone());
                ui_node
                    .props
                    .insert("key".to_string(), UiValue::String(key.clone()));
            }
            apply_canonical_attrs_to_ui_node(&mut ui_node, tag, attrs);
            lowering
                .authored_systems
                .record_authored_semantics_for_node(id, &ui_node, attrs);
            apply_attrs_to_ui_props(&mut ui_node.props, attrs);
            lowering.builder.add_node(ui_node);
            if !child_ids.is_empty() {
                lowering.builder.replace_children(id, child_ids);
            }
            Some(id)
        }
        VNode::Component { instance, rendered } => {
            let rendered_id = lower_vnode_into_tree(lowering, rendered);
            let id = lowering.builder.alloc_id();
            let mut ui_node = UiNode::new(id, UiWidgetKind::ComponentRef(instance.name.clone()));
            if let Some(rendered_id) = rendered_id {
                ui_node.children = vec![rendered_id];
            }
            for (name, value) in &instance.props {
                ui_node
                    .props
                    .insert(name.clone(), runtime_value_to_ui_value(value));
            }
            lowering.authored_systems.record_component_state_signals(
                id,
                &instance.name,
                &instance.props,
                &instance.state,
                &mut ui_node,
            );
            lowering.builder.add_node(ui_node);
            if let Some(rendered_id) = rendered_id {
                lowering.builder.replace_children(id, vec![rendered_id]);
            }
            Some(id)
        }
    }
}

fn apply_attrs_to_ui_props(props: &mut BTreeMap<String, UiValue>, attrs: &[UIAttr]) {
    for attr in attrs {
        match attr {
            UIAttr::Property { name, value, .. } => {
                if should_skip_prop_attr(name) {
                    continue;
                }
                props.insert(name.clone(), runtime_value_to_ui_value(value));
            }
            UIAttr::Bool { name, value } => {
                if should_skip_prop_attr(name) {
                    continue;
                }
                props.insert(name.clone(), UiValue::Bool(*value));
            }
            // Events are compiler-owned semantics. They do not lower to opaque prop strings.
            UIAttr::Event { .. } => {}
        }
    }
}

fn lower_text_element(
    lowering: &mut UiLowering,
    attrs: &[UIAttr],
    children: &[VNode],
    key: &Option<String>,
) -> kain_ui::UiNodeId {
    let id = lowering.builder.alloc_id();
    let mut ui_node = UiNode::new(id, UiWidgetKind::Text);
    ui_node.layout = layout_from_attrs("text", attrs);
    if let Some(key) = key {
        ui_node
            .props
            .insert("key".to_string(), UiValue::String(key.clone()));
    }
    ui_node.props.insert(
        "text".to_string(),
        UiValue::String(render_text_children(children)),
    );
    apply_canonical_attrs_to_ui_node(&mut ui_node, "text", attrs);
    apply_attrs_to_ui_props(&mut ui_node.props, attrs);
    lowering.builder.add_node(ui_node);
    id
}

fn allocate_empty_root(builder: &mut UiTreeBuilder) -> kain_ui::UiNodeId {
    let id = builder.alloc_id();
    let mut ui_node = UiNode::new(id, UiWidgetKind::Slot);
    ui_node.layout = default_layout_for_tag("slot");
    builder.add_node(ui_node);
    id
}

fn layout_from_attrs(tag: &str, attrs: &[UIAttr]) -> kain_ui::UiLayoutSpec {
    let mut layout = default_layout_for_tag(tag);
    for attr in attrs {
        match attr {
            UIAttr::Property { name, value, .. } if name == "layout" => {
                if let Value::String(name) = value {
                    if let Some(kind) = parse_layout_kind(name) {
                        layout.kind = kind;
                    }
                }
            }
            UIAttr::Property { name, value, .. } if name == "gap" => {
                layout.gap = runtime_value_to_f32(value).unwrap_or(layout.gap);
            }
            UIAttr::Property { name, value, .. } if name == "padding" => {
                layout.padding = runtime_value_to_f32(value).unwrap_or(layout.padding);
            }
            UIAttr::Property { name, value, .. } if name == "min_width" => {
                layout.min_width = runtime_value_to_f32(value).or(layout.min_width);
            }
            UIAttr::Property { name, value, .. } if name == "min_height" => {
                layout.min_height = runtime_value_to_f32(value).or(layout.min_height);
            }
            UIAttr::Property { name, value, .. } if name == "max_width" => {
                layout.max_width = runtime_value_to_f32(value).or(layout.max_width);
            }
            UIAttr::Property { name, value, .. } if name == "max_height" => {
                layout.max_height = runtime_value_to_f32(value).or(layout.max_height);
            }
            UIAttr::Property { name, value, .. } if name == "width" => {
                layout.width = parse_ui_length_value(value).or(layout.width);
            }
            UIAttr::Property { name, value, .. } if name == "height" => {
                layout.height = parse_ui_length_value(value).or(layout.height);
            }
            UIAttr::Property { name, value, .. } if name == "flex_grow" => {
                layout.flex_grow = runtime_value_to_f32(value).unwrap_or(layout.flex_grow);
            }
            UIAttr::Property { name, value, .. } if name == "flex_shrink" => {
                layout.flex_shrink = runtime_value_to_f32(value).unwrap_or(layout.flex_shrink);
            }
            UIAttr::Property { name, value, .. } if name == "align" || name == "align_items" => {
                if let Some(alignment) = value_as_str(value).and_then(parse_layout_alignment) {
                    layout.align_items = alignment;
                }
            }
            UIAttr::Property { name, value, .. }
                if name == "justify" || name == "justify_content" =>
            {
                if let Some(alignment) = value_as_str(value).and_then(parse_layout_alignment) {
                    layout.justify_content = alignment;
                }
            }
            UIAttr::Property { name, value, .. } if name == "overflow" => {
                if let Some(behavior) = value_as_str(value).and_then(parse_overflow_behavior) {
                    layout.overflow_x = behavior;
                    layout.overflow_y = behavior;
                }
            }
            UIAttr::Property { name, value, .. } if name == "overflow_x" => {
                if let Some(behavior) = value_as_str(value).and_then(parse_overflow_behavior) {
                    layout.overflow_x = behavior;
                }
            }
            UIAttr::Property { name, value, .. } if name == "overflow_y" => {
                if let Some(behavior) = value_as_str(value).and_then(parse_overflow_behavior) {
                    layout.overflow_y = behavior;
                }
            }
            UIAttr::Property { name, value, .. } if name == "dock" => {
                layout.dock = value_as_str(value)
                    .and_then(parse_dock_placement)
                    .or(layout.dock);
            }
            UIAttr::Property { name, value, .. } if name == "split_ratio" => {
                layout.split_ratio = runtime_value_to_f32(value).or(layout.split_ratio);
            }
            UIAttr::Property { name, value, .. } if name == "persistent_layout_id" => {
                layout.persistent_layout_id = value_as_str(value).map(ToString::to_string);
            }
            UIAttr::Property { name, value, .. } if name == "tab_group_id" => {
                layout.tab_group_id = value_as_str(value).map(ToString::to_string);
            }
            UIAttr::Property { name, value, .. } if name == "tab_label" => {
                layout.tab_label = value_as_str(value).map(ToString::to_string);
            }
            UIAttr::Property { name, value, .. } if name == "tab_order" => {
                layout.tab_order = runtime_value_to_i32(value).or(layout.tab_order);
            }
            UIAttr::Bool { name, value } if name == "tab_default_active" => {
                layout.tab_default_active = *value;
            }
            UIAttr::Property { name, value, .. } if name == "tab_default_active" => {
                if let Some(value) = value_as_bool(value) {
                    layout.tab_default_active = value;
                }
            }
            UIAttr::Bool { name, value } if name == "tab_closable" => {
                layout.tab_closable = *value;
            }
            UIAttr::Property { name, value, .. } if name == "tab_closable" => {
                if let Some(value) = value_as_bool(value) {
                    layout.tab_closable = value;
                }
            }
            UIAttr::Bool { name, value } if name == "resizable" => {
                layout.resizable = *value;
            }
            UIAttr::Property { name, value, .. } if name == "resizable" => {
                if let Some(value) = value_as_bool(value) {
                    layout.resizable = value;
                }
            }
            _ => {}
        }
    }
    layout
}

fn apply_canonical_attrs_to_ui_node(node: &mut UiNode, tag: &str, attrs: &[UIAttr]) {
    if let Some(identity) = attr_string(attrs, "identity_key")
        .or_else(|| attr_string(attrs, "identity"))
        .or_else(|| attr_string(attrs, "id"))
    {
        node.identity_key = Some(identity);
    }

    if let Some(scope) = attr_string(attrs, "focus_scope") {
        node.focus_scope = Some(scope);
    }

    if let Some(scope) = attr_string(attrs, "selection_scope") {
        node.selection_scope = Some(scope);
    }

    if let Some(role) = attr_string(attrs, "chrome_role") {
        node.props
            .insert("ui.chrome_role".to_string(), UiValue::String(role));
    }

    if let Some(zone) = attr_string(attrs, "anchor_zone").or_else(|| attr_string(attrs, "anchor")) {
        node.props
            .insert("ui.anchor_zone".to_string(), UiValue::String(zone));
    }

    if let Some(target) = attr_string(attrs, "anchor_target") {
        node.props
            .insert("ui.anchor_target".to_string(), UiValue::String(target));
    }

    if let Some(scope) = attr_string(attrs, "scope").or_else(|| attr_string(attrs, "theme_scope")) {
        node.style.theme_scope = Some(scope);
    }

    if let Some(variant) = attr_string(attrs, "variant") {
        node.style.variant = Some(variant);
    }

    node.style
        .classes
        .extend(attr_list(attrs, &["class", "classes"]));
    node.style.tokens.extend(attr_list(attrs, &["tokens"]));

    for state in attr_list(attrs, &["states"]) {
        if let Some(parsed) = parse_style_state(&state) {
            if !node.style.states.contains(&parsed) {
                node.style.states.push(parsed);
            }
        }
    }

    if let Some(role) = attr_string(attrs, "role") {
        node.props
            .insert("role".to_string(), UiValue::String(role.clone()));
        if tag.eq_ignore_ascii_case("text") && node.style.variant.is_none() {
            node.style.variant = Some(role);
        }
    }

    // Style and paint values are explicit semantic data, not backend-local props.
    for attr in attrs {
        let UIAttr::Property { name, value, .. } = attr else {
            continue;
        };
        if let Some(key) = name.strip_prefix("style_") {
            node.style
                .values
                .insert(normalize_style_key(key), runtime_value_to_ui_value(value));
            continue;
        }
        if let Some(key) = name.strip_prefix("paint_") {
            node.style.values.insert(
                format!("paint.{}", normalize_style_key(key)),
                runtime_value_to_ui_value(value),
            );
            continue;
        }
    }
}

fn normalize_style_key(input: &str) -> String {
    // Attribute names in `.kn` JSX are typically identifier-shaped. We use `_` as a stable
    // separator that authors can type easily, but the runtime treats the keys as dotted
    // namespaces.
    input.replace('_', ".")
}

fn should_skip_prop_attr(name: &str) -> bool {
    if name.starts_with("style_") || name.starts_with("paint_") || name.starts_with("motion_") {
        return true;
    }

    matches!(
        name,
        "layout"
            | "gap"
            | "padding"
            | "min_width"
            | "min_height"
            | "max_width"
            | "max_height"
            | "width"
            | "height"
            | "flex_grow"
            | "flex_shrink"
            | "align"
            | "align_items"
            | "justify"
            | "justify_content"
            | "overflow"
            | "overflow_x"
            | "overflow_y"
            | "dock"
            | "split_ratio"
            | "resizable"
            | "persistent_layout_id"
            | "tab_group_id"
            | "tab_label"
            | "tab_order"
            | "tab_default_active"
            | "tab_closable"
            | "scope"
            | "theme_scope"
            | "variant"
            | "class"
            | "classes"
            | "tokens"
            | "states"
            | "role"
            | "id"
            | "identity"
            | "identity_key"
            | "key"
            | "focus_scope"
            | "selection_scope"
            | "focus_scope_default"
            | "selection_scope_default"
            | "event_phase"
            | "command"
            | "transaction"
            | "chrome_role"
            | "anchor_zone"
            | "anchor"
            | "anchor_target"
    )
}

fn extract_signal_decl(systems: &mut AuthoredUiSystemsAccumulator, attrs: &[UIAttr]) {
    let Some(id) = attr_string(attrs, "id").or_else(|| attr_string(attrs, "name")) else {
        return;
    };
    let initial = attr_value(attrs, "initial")
        .or_else(|| attr_value(attrs, "value"))
        .map(runtime_value_to_ui_value)
        .unwrap_or(UiValue::Null);
    let signal_key = format!("ui.signal::{id}");
    let signal_id = UiSignalId(stable_hash_u64(&signal_key));
    systems.signal_values.insert(signal_id, initial);
    systems.session_state.insert(
        format!("ui.signal.key.{}", signal_id.0),
        UiValue::String(signal_key),
    );
}

fn extract_computed_decl(systems: &mut AuthoredUiSystemsAccumulator, attrs: &[UIAttr]) {
    let Some(id) = attr_string(attrs, "id") else {
        return;
    };
    let label = attr_string(attrs, "label").unwrap_or_else(|| id.clone());
    let depends_on = attr_list(attrs, &["depends_on", "depends", "signals"]);
    let writes_signal = attr_string_any(
        attrs,
        &[
            "writes_signal",
            "signal",
            "target_signal",
            "output_signal",
            "output",
        ],
    );
    let expr = attr_expr(attrs, &["expr", "value", "derived", "expression"]).cloned();
    let invalidates = attr_list(attrs, &["invalidates_nodes", "invalidates", "nodes"]);
    let scheduler_phase = attr_string(attrs, "phase")
        .and_then(parse_scheduler_phase)
        .unwrap_or(UiSchedulerPhase::Signals);
    systems.computed_specs.push(AuthoredComputedSpec {
        id,
        label,
        depends_on,
        writes_signal,
        expr,
        invalidates,
        scheduler_phase,
    });
}

fn extract_workspace_decl(systems: &mut AuthoredUiSystemsAccumulator, attrs: &[UIAttr]) {
    if let Some(key) = attr_string(attrs, "persistence_key")
        .or_else(|| attr_string(attrs, "layout_key"))
        .or_else(|| attr_string(attrs, "workspace_key"))
    {
        systems.workspace_persistence_key = Some(key);
    }

    if let Some(enabled) = attr_value(attrs, "virtualization_enabled")
        .and_then(value_as_bool)
        .or_else(|| attr_value(attrs, "virtualize").and_then(value_as_bool))
    {
        systems.workspace_virtualization_enabled = Some(enabled);
    }

    if let Some(preset) = attr_string(attrs, "preset") {
        systems
            .session_state
            .insert("ui.workspace.preset".to_string(), UiValue::String(preset));
    }
}

fn parse_scheduler_phase(input: String) -> Option<UiSchedulerPhase> {
    match input.trim().to_ascii_lowercase().as_str() {
        "signals" => Some(UiSchedulerPhase::Signals),
        "resources" => Some(UiSchedulerPhase::Resources),
        "layout" => Some(UiSchedulerPhase::Layout),
        "animation" => Some(UiSchedulerPhase::Animation),
        "patches" => Some(UiSchedulerPhase::Patches),
        "effects" => Some(UiSchedulerPhase::Effects),
        _ => None,
    }
}

fn node_layout_id(node: &UiNode) -> String {
    node.layout
        .persistent_layout_id
        .clone()
        .or_else(|| node.identity_key.clone())
        .unwrap_or_else(|| format!("node-{}", node.id.0))
}

fn resolve_active_tabs_from_tree(tree: &kain_ui::UiTree) -> BTreeMap<String, String> {
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

fn extract_focus_scope_decl(systems: &mut AuthoredUiSystemsAccumulator, attrs: &[UIAttr]) {
    let Some(name) = attr_string(attrs, "name") else {
        return;
    };
    systems.focus_scopes.insert(name.clone());
    let is_default = attr_string(attrs, "default")
        .and_then(|value| value.parse::<bool>().ok())
        .unwrap_or(false);
    if is_default {
        systems.focus_default_scope = Some(name);
    }
}

fn extract_selection_scope_decl(systems: &mut AuthoredUiSystemsAccumulator, attrs: &[UIAttr]) {
    let Some(name) = attr_string(attrs, "name") else {
        return;
    };
    systems.selection_scopes.insert(name.clone());
    let is_default = attr_string(attrs, "default")
        .and_then(|value| value.parse::<bool>().ok())
        .unwrap_or(false);
    if is_default {
        systems.selection_default_scope = Some(name);
    }
}

impl AuthoredUiSystemsAccumulator {
    fn record_authored_semantics_for_node(
        &mut self,
        id: kain_ui::UiNodeId,
        node: &UiNode,
        attrs: &[UIAttr],
    ) {
        if let Some(scope) = node.focus_scope.as_deref() {
            self.focus_scopes.insert(scope.to_string());
            if attr_string(attrs, "focus_scope_default")
                .and_then(|value| value.parse::<bool>().ok())
                .unwrap_or(false)
            {
                self.focus_default_scope = Some(scope.to_string());
            }
        }

        if let Some(scope) = node.selection_scope.as_deref() {
            self.selection_scopes.insert(scope.to_string());
            if attr_string(attrs, "selection_scope_default")
                .and_then(|value| value.parse::<bool>().ok())
                .unwrap_or(false)
            {
                self.selection_default_scope = Some(scope.to_string());
            }
        }

        self.record_event_routes_and_handlers(id, node, attrs);
        self.record_motion_tracks(id, node, attrs);
        self.record_surface_decl(id, node, attrs);
    }

    fn record_event_routes_and_handlers(
        &mut self,
        id: kain_ui::UiNodeId,
        node: &UiNode,
        attrs: &[UIAttr],
    ) {
        let phase = match attr_string(attrs, "event_phase")
            .unwrap_or_else(|| "bubble".to_string())
            .as_str()
        {
            "bubble" => UiEventPhase::Bubble,
            "capture" => UiEventPhase::Capture,
            "direct" => UiEventPhase::Direct,
            _ => UiEventPhase::Bubble,
        };
        let command = attr_string(attrs, "command");
        let transaction = attr_string(attrs, "transaction");
        let target_key = stable_node_contract_key(node);
        let phase_key = match phase {
            UiEventPhase::Bubble => "bubble",
            UiEventPhase::Capture => "capture",
            UiEventPhase::Direct => "direct",
        };

        for attr in attrs {
            let UIAttr::Event {
                event,
                handler,
                expr,
                ..
            } = attr
            else {
                continue;
            };
            let event_name = event_name(event).to_string();
            let handler_id = expr
                .as_ref()
                .map(render_authored_expr_contract)
                .unwrap_or_else(|| handler_ref_string(handler));
            let route_id = format!("{target_key}::{event_name}::{phase_key}");
            let route_prefix = format!("ui.event.route.{route_id}");
            self.event_routes.push(UiEventRoute {
                route_id: route_id.clone(),
                event: event_name.clone(),
                target: id,
                phase,
                handler_id: Some(handler_id.clone()),
                dispatch_command: command.clone(),
                transaction_label: transaction.clone(),
            });

            self.session_state.insert(
                format!("{route_prefix}.target"),
                UiValue::String(target_key.clone()),
            );
            self.session_state.insert(
                format!("{route_prefix}.event"),
                UiValue::String(event_name.clone()),
            );
            self.session_state.insert(
                format!("{route_prefix}.phase"),
                UiValue::String(phase_key.to_string()),
            );
            self.session_state.insert(
                format!("{route_prefix}.handler"),
                UiValue::String(handler_id),
            );
            if let Some(command) = command.as_deref() {
                self.session_state.insert(
                    format!("{route_prefix}.command"),
                    UiValue::String(command.to_string()),
                );
            }
            if let Some(label) = transaction.as_deref() {
                let label = label.to_string();
                self.session_state.insert(
                    format!("{route_prefix}.transaction"),
                    UiValue::String(label.clone()),
                );
                self.session_state.insert(
                    format!("{route_prefix}.transaction_label"),
                    UiValue::String(label),
                );
            }
        }
    }

    fn record_motion_tracks(&mut self, id: kain_ui::UiNodeId, node: &UiNode, attrs: &[UIAttr]) {
        let Some(property) =
            attr_string(attrs, "motion_property").or_else(|| attr_string(attrs, "motion_prop"))
        else {
            return;
        };

        let track_id = attr_string(attrs, "motion_id").unwrap_or_else(|| {
            let target = node
                .identity_key
                .as_deref()
                .map(ToString::to_string)
                .unwrap_or_else(|| id.0.to_string());
            format!("motion.{target}.{property}")
        });
        let duration_ms = attr_value(attrs, "motion_duration_ms")
            .and_then(runtime_value_to_u32)
            .unwrap_or(250);
        let trigger = attr_string(attrs, "motion_trigger")
            .and_then(parse_animation_trigger)
            .unwrap_or(UiAnimationTrigger::Mount);
        let easing = attr_string(attrs, "motion_easing")
            .and_then(parse_easing_kind)
            .unwrap_or(UiEasingKind::EaseInOut);
        let preserve_on_reload = attr_value(attrs, "motion_preserve_on_reload")
            .and_then(value_as_bool)
            .unwrap_or(true);

        self.animation_tracks.push(UiAnimationTrack {
            id: track_id,
            target: id,
            property,
            duration_ms,
            trigger,
            easing,
            preserve_on_reload,
        });
    }

    fn record_surface_decl(&mut self, id: kain_ui::UiNodeId, node: &UiNode, attrs: &[UIAttr]) {
        let kind = match node.kind {
            UiWidgetKind::Graph => UiSurfaceKind::Graph,
            UiWidgetKind::Timeline => UiSurfaceKind::Timeline,
            UiWidgetKind::Table => UiSurfaceKind::Table,
            UiWidgetKind::Tree => UiSurfaceKind::Tree,
            UiWidgetKind::Viewport2D => UiSurfaceKind::Viewport2D,
            UiWidgetKind::Viewport3D => UiSurfaceKind::Viewport3D,
            UiWidgetKind::Overlay => UiSurfaceKind::Overlay,
            _ => return,
        };

        let surface_id = attr_string(attrs, "surface_id")
            .or_else(|| node.identity_key.clone())
            .unwrap_or_else(|| format!("surface.{}", id.0));

        let renderer_preference = attr_string(attrs, "surface_renderer")
            .and_then(parse_surface_renderer_preference)
            .unwrap_or_default();
        let composition_mode = attr_string(attrs, "surface_composition")
            .and_then(parse_surface_composition_mode)
            .unwrap_or_default();
        let gpu_backing_required = attr_value(attrs, "gpu_backing_required")
            .and_then(value_as_bool)
            .unwrap_or(false);
        let title = attr_string(attrs, "title");

        let shader_ref = attr_string(attrs, "shader_ref");
        let shader = shader_ref.map(|shader_ref| UiSurfaceShaderBinding {
            shader_ref,
            entry_point: attr_string(attrs, "shader_entry_point"),
            stage: attr_string(attrs, "shader_stage"),
            derived_format: attr_string(attrs, "shader_derived_format"),
        });
        let preferred_host_backend = attr_string(attrs, "host_backend")
            .or_else(|| attr_string(attrs, "surface_backend"))
            .and_then(parse_surface_host_backend)
            .unwrap_or_else(|| match kind {
                UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay => {
                    UiHostBackendKind::Imgui
                }
                UiSurfaceKind::Canvas | UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D => {
                    UiHostBackendKind::Qt
                }
                _ => UiHostBackendKind::Auto,
            });
        let preferred_layout_engine = attr_string(attrs, "layout_engine")
            .or_else(|| attr_string(attrs, "surface_layout_engine"))
            .and_then(parse_surface_layout_engine)
            .unwrap_or(UiLayoutEngineKind::Yoga);
        let preferred_render_engine = attr_string(attrs, "render_engine")
            .or_else(|| attr_string(attrs, "surface_render_engine"))
            .and_then(parse_surface_render_engine)
            .unwrap_or_else(|| match renderer_preference {
                UiSurfaceRendererPreference::Wgpu => UiRenderEngineKind::Wgpu,
                UiSurfaceRendererPreference::Shader => UiRenderEngineKind::Shader,
                UiSurfaceRendererPreference::Dom => UiRenderEngineKind::Browser,
                UiSurfaceRendererPreference::Native => UiRenderEngineKind::Native,
                UiSurfaceRendererPreference::Auto => UiRenderEngineKind::Auto,
            });

        self.surfaces.push(UiSurface {
            id: surface_id,
            kind,
            node: id,
            title,
            renderer_preference,
            composition_mode,
            preferred_host_backend,
            preferred_layout_engine,
            preferred_render_engine,
            gpu_backing_required,
            shader,
        });
    }

    fn record_component_state_signals(
        &mut self,
        component_node_id: kain_ui::UiNodeId,
        component_name: &str,
        props: &HashMap<String, Value>,
        state: &HashMap<String, Value>,
        ui_node: &mut UiNode,
    ) {
        let identity = props
            .get("key")
            .and_then(value_as_str)
            .or_else(|| props.get("id").and_then(value_as_str))
            .map(ToString::to_string)
            .unwrap_or_else(|| format!("{}", component_node_id.0));
        ui_node.identity_key.get_or_insert(identity.clone());

        for (state_name, initial_value) in state {
            let signal_key =
                format!("ui.signal.component_state::{component_name}::{identity}::{state_name}");
            let signal_id = UiSignalId(stable_hash_u64(&signal_key));
            ui_node.watches.push(signal_id);

            self.signal_values
                .insert(signal_id, runtime_value_to_ui_value(initial_value));
            // Provide a deterministic bridge for other layers (runtime/backends/tools) without
            // reintroducing `state.<name>=<value>` prop flattening.
            ui_node.props.insert(
                format!("ui.state_signal.{state_name}"),
                UiValue::String(signal_id.0.to_string()),
            );
            self.session_state.insert(
                format!("ui.signal.key.{}", signal_id.0),
                UiValue::String(signal_key),
            );
            self.session_state.insert(
                format!("ui.signal.owner.{}", signal_id.0),
                UiValue::String(format!("component:{component_name}")),
            );
        }
    }
}

fn runtime_value_to_u32(value: &Value) -> Option<u32> {
    match value {
        Value::Int(value) => u32::try_from(*value).ok(),
        Value::Float(value) => u32::try_from(*value as i64).ok(),
        Value::String(value) => value.parse::<u32>().ok(),
        _ => None,
    }
}

fn parse_animation_trigger(input: String) -> Option<UiAnimationTrigger> {
    match input.trim().to_ascii_lowercase().as_str() {
        "mount" => Some(UiAnimationTrigger::Mount),
        "unmount" => Some(UiAnimationTrigger::Unmount),
        "signal_change" | "signalchange" => Some(UiAnimationTrigger::SignalChange),
        "hover" => Some(UiAnimationTrigger::Hover),
        "focus" => Some(UiAnimationTrigger::Focus),
        "layout_change" | "layoutchange" => Some(UiAnimationTrigger::LayoutChange),
        "reload" => Some(UiAnimationTrigger::Reload),
        _ => None,
    }
}

fn parse_easing_kind(input: String) -> Option<UiEasingKind> {
    match input.trim().to_ascii_lowercase().as_str() {
        "linear" => Some(UiEasingKind::Linear),
        "ease_in" | "easein" => Some(UiEasingKind::EaseIn),
        "ease_out" | "easeout" => Some(UiEasingKind::EaseOut),
        "ease_in_out" | "easeinout" => Some(UiEasingKind::EaseInOut),
        "spring" => Some(UiEasingKind::Spring),
        _ => None,
    }
}

fn parse_surface_renderer_preference(input: String) -> Option<UiSurfaceRendererPreference> {
    match input.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiSurfaceRendererPreference::Auto),
        "native" => Some(UiSurfaceRendererPreference::Native),
        "dom" => Some(UiSurfaceRendererPreference::Dom),
        "wgpu" => Some(UiSurfaceRendererPreference::Wgpu),
        "shader" => Some(UiSurfaceRendererPreference::Shader),
        _ => None,
    }
}

fn parse_surface_host_backend(input: String) -> Option<UiHostBackendKind> {
    match input.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiHostBackendKind::Auto),
        "native" | "host" => Some(UiHostBackendKind::Native),
        "legacy_egui" | "legacy-egui" | "egui" => Some(UiHostBackendKind::LegacyEgui),
        "imgui" => Some(UiHostBackendKind::Imgui),
        "rml" | "rmlui" => Some(UiHostBackendKind::RmlUi),
        "slint" => Some(UiHostBackendKind::Slint),
        "qt" => Some(UiHostBackendKind::Qt),
        "cef" | "browser" => Some(UiHostBackendKind::Cef),
        "tauri" | "webview" => Some(UiHostBackendKind::Tauri),
        _ => None,
    }
}

fn parse_surface_layout_engine(input: String) -> Option<UiLayoutEngineKind> {
    match input.trim().to_ascii_lowercase().as_str() {
        "auto" => Some(UiLayoutEngineKind::Auto),
        "native" => Some(UiLayoutEngineKind::Native),
        "yoga" => Some(UiLayoutEngineKind::Yoga),
        "legacy_egui" | "legacy-egui" | "egui" => Some(UiLayoutEngineKind::LegacyEgui),
        _ => None,
    }
}

fn parse_surface_render_engine(input: String) -> Option<UiRenderEngineKind> {
    match input.trim().to_ascii_lowercase().as_str() {
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

fn parse_surface_composition_mode(input: String) -> Option<UiSurfaceCompositionMode> {
    match input.trim().to_ascii_lowercase().as_str() {
        "host" => Some(UiSurfaceCompositionMode::Host),
        "layered_gpu" | "layeredgpu" => Some(UiSurfaceCompositionMode::LayeredGpu),
        "viewport" => Some(UiSurfaceCompositionMode::Viewport),
        "shader_canvas" | "shadercanvas" => Some(UiSurfaceCompositionMode::ShaderCanvas),
        _ => None,
    }
}

fn handler_ref_string(handler: &Value) -> String {
    match handler {
        Value::Function(name) => name.clone(),
        Value::NativeFn(name, _) => name.clone(),
        Value::String(name) => name.clone(),
        Value::Closure(params, _, _) => format!("[closure params={}]", params.len()),
        other => format!("{}", other),
    }
}

fn append_global_ui_authoring_contracts(output: &mut UiBuildOutput, env: &Env) {
    let mut inserted_any = false;

    if let Some(widget_registry) = env.lookup_value("ui_widget_registry") {
        if let Some(json) = runtime_value_to_json_string(&widget_registry) {
            output.systems.session_state.insert(
                "ui.contract.widget_registry.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if let Some(paint_registry) = env.lookup_value("ui_paint_registry") {
        if let Some(json) = runtime_value_to_json_string(&paint_registry) {
            output.systems.session_state.insert(
                "ui.contract.paint_registry.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if let Some(motion_registry) = env.lookup_value("ui_motion_registry") {
        if let Some(json) = runtime_value_to_json_string(&motion_registry) {
            output.systems.session_state.insert(
                "ui.contract.motion_registry.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if let Some(command_registry) = env.lookup_value("ui_command_registry") {
        if let Some(json) = runtime_value_to_json_string(&command_registry) {
            output.systems.session_state.insert(
                "ui.contract.command_registry.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if let Some(motion_policy) = env.lookup_value("ui_motion_policy") {
        if let Some(json) = runtime_value_to_json_string(&motion_policy) {
            output.systems.session_state.insert(
                "ui.contract.motion_policy.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if let Some(workspace_schema) = env.lookup_value("ui_workspace_schema") {
        if let Some(json) = runtime_value_to_json_string(&workspace_schema) {
            output.systems.session_state.insert(
                "ui.contract.workspace_schema.json".to_string(),
                UiValue::String(json),
            );
            inserted_any = true;
        }
    }

    if inserted_any {
        output.systems.session_state.insert(
            "ui.contract.version".to_string(),
            UiValue::String(UI_AUTHORING_CONTRACT_VERSION.to_string()),
        );
    }
}

fn runtime_value_to_json_string(value: &Value) -> Option<String> {
    fn to_json(v: &Value) -> serde_json::Value {
        match v {
            Value::Unit | Value::None => serde_json::Value::Null,
            Value::Bool(b) => serde_json::Value::Bool(*b),
            Value::Int(i) => serde_json::json!(i),
            Value::Float(f) => serde_json::json!(f),
            Value::String(s) => serde_json::Value::String(s.clone()),
            Value::Array(arr) => {
                let arr = arr.read().unwrap();
                serde_json::Value::Array(arr.iter().map(to_json).collect())
            }
            Value::Tuple(items) => serde_json::Value::Array(items.iter().map(to_json).collect()),
            Value::Struct(_, fields) => {
                let fields = fields.read().unwrap();
                let mut map = serde_json::Map::new();
                for (k, v) in fields.iter() {
                    map.insert(k.clone(), to_json(v));
                }
                serde_json::Value::Object(map)
            }
            other => serde_json::Value::String(format!("{}", other)),
        }
    }

    serde_json::to_string_pretty(&to_json(value)).ok()
}

fn parse_layout_kind(name: &str) -> Option<kain_ui::UiLayoutKind> {
    const LAYOUT_KIND_PROFILES: &[(&str, kain_ui::UiLayoutKind)] = &[
        ("flow", kain_ui::UiLayoutKind::Flow),
        ("row", kain_ui::UiLayoutKind::FlexRow),
        ("flex-row", kain_ui::UiLayoutKind::FlexRow),
        ("column", kain_ui::UiLayoutKind::FlexColumn),
        ("flex-column", kain_ui::UiLayoutKind::FlexColumn),
        ("grid", kain_ui::UiLayoutKind::Grid),
        ("dock", kain_ui::UiLayoutKind::Dock),
        ("stack", kain_ui::UiLayoutKind::Stack),
        ("absolute", kain_ui::UiLayoutKind::Absolute),
    ];

    LAYOUT_KIND_PROFILES
        .iter()
        .find(|(candidate, _)| candidate.eq_ignore_ascii_case(name))
        .map(|(_, kind)| *kind)
}

fn parse_layout_alignment(value: &str) -> Option<UiLayoutAlignment> {
    const ALIGNMENTS: &[(&str, UiLayoutAlignment)] = &[
        ("start", UiLayoutAlignment::Start),
        ("center", UiLayoutAlignment::Center),
        ("end", UiLayoutAlignment::End),
        ("stretch", UiLayoutAlignment::Stretch),
        ("space-between", UiLayoutAlignment::SpaceBetween),
        ("space_between", UiLayoutAlignment::SpaceBetween),
        ("between", UiLayoutAlignment::SpaceBetween),
    ];

    ALIGNMENTS
        .iter()
        .find(|(candidate, _)| candidate.eq_ignore_ascii_case(value))
        .map(|(_, alignment)| *alignment)
}

fn parse_overflow_behavior(value: &str) -> Option<UiOverflowBehavior> {
    const BEHAVIORS: &[(&str, UiOverflowBehavior)] = &[
        ("visible", UiOverflowBehavior::Visible),
        ("hidden", UiOverflowBehavior::Hidden),
        ("scroll", UiOverflowBehavior::Scroll),
        ("auto", UiOverflowBehavior::Auto),
    ];

    BEHAVIORS
        .iter()
        .find(|(candidate, _)| candidate.eq_ignore_ascii_case(value))
        .map(|(_, behavior)| *behavior)
}

fn parse_dock_placement(value: &str) -> Option<UiDockPlacement> {
    const PLACEMENTS: &[(&str, UiDockPlacement)] = &[
        ("center", UiDockPlacement::Center),
        ("left", UiDockPlacement::Left),
        ("right", UiDockPlacement::Right),
        ("top", UiDockPlacement::Top),
        ("bottom", UiDockPlacement::Bottom),
        ("tab", UiDockPlacement::Tab),
    ];

    PLACEMENTS
        .iter()
        .find(|(candidate, _)| candidate.eq_ignore_ascii_case(value))
        .map(|(_, placement)| *placement)
}

fn parse_style_state(value: &str) -> Option<UiStyleState> {
    const STATES: &[(&str, UiStyleState)] = &[
        ("hovered", UiStyleState::Hovered),
        ("active", UiStyleState::Active),
        ("focused", UiStyleState::Focused),
        ("disabled", UiStyleState::Disabled),
        ("selected", UiStyleState::Selected),
        ("dragging", UiStyleState::Dragging),
    ];

    STATES
        .iter()
        .find(|(candidate, _)| candidate.eq_ignore_ascii_case(value))
        .map(|(_, state)| *state)
}

fn parse_ui_length_value(value: &Value) -> Option<UiLength> {
    match value {
        Value::Int(value) => Some(UiLength {
            value: *value as f32,
            unit: UiLengthUnit::Px,
        }),
        Value::Float(value) => Some(UiLength {
            value: *value as f32,
            unit: UiLengthUnit::Px,
        }),
        Value::String(value) => parse_ui_length(value),
        _ => None,
    }
}

fn parse_ui_length(value: &str) -> Option<UiLength> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return None;
    }

    if trimmed.eq_ignore_ascii_case("auto") {
        return Some(UiLength {
            value: 0.0,
            unit: UiLengthUnit::Auto,
        });
    }

    for (suffix, unit) in [
        ("px", UiLengthUnit::Px),
        ("%", UiLengthUnit::Percent),
        ("fr", UiLengthUnit::Fr),
    ] {
        if let Some(raw) = trimmed.strip_suffix(suffix) {
            let value = raw.trim().parse::<f32>().ok()?;
            return Some(UiLength { value, unit });
        }
    }

    trimmed.parse::<f32>().ok().map(|value| UiLength {
        value,
        unit: UiLengthUnit::Px,
    })
}

fn runtime_value_to_f32(value: &Value) -> Option<f32> {
    match value {
        Value::Int(value) => Some(*value as f32),
        Value::Float(value) => Some(*value as f32),
        Value::String(value) => value.trim().parse::<f32>().ok(),
        _ => None,
    }
}

fn runtime_value_to_i32(value: &Value) -> Option<i32> {
    match value {
        Value::Int(value) => i32::try_from(*value).ok(),
        Value::Float(value) => {
            if value.is_finite() && *value >= i32::MIN as f64 && *value <= i32::MAX as f64 {
                Some(*value as i32)
            } else {
                None
            }
        }
        Value::String(value) => value.trim().parse::<i32>().ok(),
        _ => None,
    }
}

fn runtime_value_to_ui_value(value: &Value) -> UiValue {
    match value {
        Value::Unit | Value::None => UiValue::Null,
        Value::Bool(value) => UiValue::Bool(*value),
        Value::Int(value) => UiValue::Int(*value),
        Value::Float(value) => UiValue::Float(*value),
        Value::String(value) => UiValue::String(value.clone()),
        Value::JSX(node) => UiValue::String(render_to_string(node)),
        _ => UiValue::String(value.to_string()),
    }
}

fn extract_theme_block(registry: &mut UiThemeRegistry, attrs: &[UIAttr], children: &[VNode]) {
    if registry.active_theme.is_none() {
        registry.active_theme = theme_block_name(attrs);
    }

    for child in children {
        extract_theme_directive(registry, child, None);
    }
}

fn extract_theme_directive(
    registry: &mut UiThemeRegistry,
    node: &VNode,
    inherited_scope: Option<&str>,
) {
    match node {
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } if tag.eq_ignore_ascii_case("scope") => {
            let Some(name) = attr_string(attrs, "name") else {
                return;
            };
            push_theme_scope(
                registry,
                UiThemeScope {
                    selector: attr_string(attrs, "selector")
                        .unwrap_or_else(|| format!("scope:{name}")),
                    parent: attr_string(attrs, "parent"),
                    name: name.clone(),
                },
            );
            push_theme_diff_key(registry, name.clone());
            for child in children {
                extract_theme_directive(registry, child, Some(name.as_str()));
            }
        }
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } if tag.eq_ignore_ascii_case("token") => {
            if let Some(token) = theme_token_from_attrs(attrs, None) {
                push_theme_token(registry, token);
            }
            for child in children {
                extract_theme_directive(registry, child, inherited_scope);
            }
        }
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } if tag.eq_ignore_ascii_case("variant") => {
            let Some(name) = attr_string(attrs, "name") else {
                return;
            };
            let scope = attr_string(attrs, "scope")
                .or_else(|| inherited_scope.map(ToString::to_string))
                .unwrap_or_else(|| "default".to_string());
            let mut tokens = attr_list(attrs, &["tokens"]);
            for child in children {
                if let Some(token) =
                    theme_token_from_vnode(child, Some(&format!("variant.{name}.")))
                {
                    tokens.push(token.name.clone());
                    push_theme_token(registry, token);
                } else {
                    extract_theme_directive(registry, child, Some(scope.as_str()));
                }
            }
            push_theme_variant(
                registry,
                UiThemeVariant {
                    scope,
                    name,
                    tokens,
                },
            );
        }
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } if tag.eq_ignore_ascii_case("widget") => {
            let Some(kind) = attr_string(attrs, "kind") else {
                return;
            };
            let scope = attr_string(attrs, "scope")
                .or_else(|| inherited_scope.map(ToString::to_string))
                .unwrap_or_else(|| "default".to_string());
            let variant = attr_string(attrs, "variant");
            let prefix = if let Some(variant) = variant.as_deref() {
                format!("widget.{kind}.variant.{variant}.")
            } else {
                format!("widget.{kind}.")
            };
            let mut tokens = attr_list(attrs, &["tokens"]);
            for child in children {
                if let Some(token) = theme_token_from_vnode(child, Some(prefix.as_str())) {
                    tokens.push(token.name.clone());
                    push_theme_token(registry, token);
                } else {
                    extract_theme_directive(registry, child, Some(scope.as_str()));
                }
            }
            if let Some(variant) = variant {
                push_theme_variant(
                    registry,
                    UiThemeVariant {
                        scope,
                        name: variant,
                        tokens,
                    },
                );
            }
        }
        VNode::Element {
            tag,
            attrs,
            children,
            ..
        } if tag.eq_ignore_ascii_case("textvariant") => {
            let mut widget_attrs = attrs.to_vec();
            if attr_string(attrs, "kind").is_none() {
                widget_attrs.push(UIAttr::Property {
                    name: "kind".to_string(),
                    value: Value::String("text".to_string()),
                    expr: None,
                });
            }
            if attr_string(attrs, "variant").is_none() {
                if let Some(name) = attr_string(attrs, "name") {
                    widget_attrs.push(UIAttr::Property {
                        name: "variant".to_string(),
                        value: Value::String(name),
                        expr: None,
                    });
                }
            }
            extract_theme_directive(
                registry,
                &VNode::Element {
                    tag: "widget".to_string(),
                    attrs: widget_attrs,
                    children: children.to_vec(),
                    key: None,
                },
                inherited_scope,
            );
        }
        VNode::Element { children, .. } | VNode::Fragment(children) => {
            for child in children {
                extract_theme_directive(registry, child, inherited_scope);
            }
        }
        VNode::Component { rendered, .. } => {
            extract_theme_directive(registry, rendered, inherited_scope);
        }
        VNode::Text(_) => {}
    }
}

fn theme_block_name(attrs: &[UIAttr]) -> Option<String> {
    match attr_value(attrs, "active") {
        Some(Value::String(name)) if !name.trim().is_empty() => Some(name.clone()),
        Some(value) if value_as_bool(value) == Some(true) => attr_string(attrs, "name"),
        Some(_) => None,
        None => attr_string(attrs, "name"),
    }
}

fn theme_token_from_vnode(node: &VNode, prefix: Option<&str>) -> Option<UiThemeToken> {
    match node {
        VNode::Element {
            tag,
            attrs,
            children: _,
            ..
        } if tag.eq_ignore_ascii_case("token") => theme_token_from_attrs(attrs, prefix),
        VNode::Component { rendered, .. } => theme_token_from_vnode(rendered, prefix),
        _ => None,
    }
}

fn theme_token_from_attrs(attrs: &[UIAttr], prefix: Option<&str>) -> Option<UiThemeToken> {
    let name = attr_string(attrs, "name")?;
    let value = attr_value(attrs, "value")?;
    let full_name = format!("{}{}", prefix.unwrap_or_default(), name);
    Some(UiThemeToken {
        category: attr_string(attrs, "category")
            .unwrap_or_else(|| infer_theme_token_category(&full_name)),
        name: full_name,
        value: runtime_value_to_ui_value(value),
    })
}

fn infer_theme_token_category(name: &str) -> String {
    let category = name
        .split('.')
        .find(|segment| {
            !segment.eq_ignore_ascii_case("widget") && !segment.eq_ignore_ascii_case("variant")
        })
        .unwrap_or("generic");
    category.to_string()
}

fn merge_authored_theme_registry(
    derived: &UiThemeRegistry,
    authored: UiThemeRegistry,
) -> UiThemeRegistry {
    let mut merged = UiThemeRegistry {
        active_theme: authored
            .active_theme
            .or_else(|| derived.active_theme.clone()),
        ..UiThemeRegistry::default()
    };

    for scope in &derived.scopes {
        push_theme_scope(&mut merged, scope.clone());
    }
    for key in &derived.diff_keys {
        push_theme_diff_key(&mut merged, key.clone());
    }
    for scope in authored.scopes {
        push_theme_scope(&mut merged, scope);
    }
    for token in authored.semantic_tokens {
        push_theme_token(&mut merged, token);
    }
    for variant in authored.variants {
        push_theme_variant(&mut merged, variant);
    }
    for key in authored.diff_keys {
        push_theme_diff_key(&mut merged, key);
    }

    merged
}

fn theme_registry_is_empty(registry: &UiThemeRegistry) -> bool {
    registry.active_theme.is_none()
        && registry.scopes.is_empty()
        && registry.semantic_tokens.is_empty()
        && registry.variants.is_empty()
        && registry.diff_keys.is_empty()
}

fn push_theme_scope(registry: &mut UiThemeRegistry, scope: UiThemeScope) {
    if let Some(existing) = registry
        .scopes
        .iter_mut()
        .find(|entry| entry.name == scope.name)
    {
        *existing = scope;
    } else {
        registry.scopes.push(scope);
    }
}

fn push_theme_token(registry: &mut UiThemeRegistry, token: UiThemeToken) {
    if let Some(existing) = registry
        .semantic_tokens
        .iter_mut()
        .find(|entry| entry.name == token.name)
    {
        *existing = token;
    } else {
        registry.semantic_tokens.push(token);
    }
}

fn push_theme_variant(registry: &mut UiThemeRegistry, variant: UiThemeVariant) {
    if let Some(existing) = registry
        .variants
        .iter_mut()
        .find(|entry| entry.scope == variant.scope && entry.name == variant.name)
    {
        for token in variant.tokens {
            if !existing.tokens.contains(&token) {
                existing.tokens.push(token);
            }
        }
    } else {
        registry.variants.push(variant);
    }
}

fn push_theme_diff_key(registry: &mut UiThemeRegistry, key: String) {
    if !registry.diff_keys.contains(&key) {
        registry.diff_keys.push(key);
    }
}

fn render_text_children(children: &[VNode]) -> String {
    children
        .iter()
        .map(render_to_string)
        .collect::<Vec<_>>()
        .join("")
}

fn attr_value<'a>(attrs: &'a [UIAttr], name: &str) -> Option<&'a Value> {
    attrs.iter().find_map(|attr| match attr {
        UIAttr::Property {
            name: attr_name,
            value,
            ..
        } if attr_name == name => Some(value),
        _ => None,
    })
}

fn attr_expr<'a>(attrs: &'a [UIAttr], names: &[&str]) -> Option<&'a Expr> {
    for name in names {
        if let Some(expr) = attrs.iter().find_map(|attr| match attr {
            UIAttr::Property {
                name: attr_name,
                expr: Some(expr),
                ..
            } if attr_name == name => Some(expr),
            UIAttr::Event {
                name: attr_name,
                expr: Some(expr),
                ..
            } if attr_name == name => Some(expr),
            _ => None,
        }) {
            return Some(expr);
        }
    }
    None
}

fn attr_string_any(attrs: &[UIAttr], names: &[&str]) -> Option<String> {
    for name in names {
        if let Some(value) = attr_string(attrs, name) {
            return Some(value);
        }
    }
    None
}

fn attr_string(attrs: &[UIAttr], name: &str) -> Option<String> {
    match attrs.iter().find(|attr| match attr {
        UIAttr::Property {
            name: attr_name, ..
        }
        | UIAttr::Bool {
            name: attr_name, ..
        }
        | UIAttr::Event {
            name: attr_name, ..
        } => attr_name == name,
    }) {
        Some(UIAttr::Property { value, .. }) => value_as_str(value).map(ToString::to_string),
        Some(UIAttr::Bool { value, .. }) => Some(value.to_string()),
        _ => None,
    }
}

fn attr_list(attrs: &[UIAttr], names: &[&str]) -> Vec<String> {
    let mut values = Vec::new();
    for name in names {
        if let Some(value) = attr_string(attrs, name) {
            for entry in value
                .split(|c: char| c == ',' || c == '|' || c.is_whitespace())
                .map(str::trim)
                .filter(|entry| !entry.is_empty())
            {
                let entry = entry.to_string();
                if !values.contains(&entry) {
                    values.push(entry);
                }
            }
        }
    }
    values
}

fn value_as_str(value: &Value) -> Option<&str> {
    match value {
        Value::String(value) => Some(value.as_str()),
        _ => None,
    }
}

fn value_as_bool(value: &Value) -> Option<bool> {
    match value {
        Value::Bool(value) => Some(*value),
        Value::String(value) => match value.trim().to_ascii_lowercase().as_str() {
            "true" | "yes" | "on" => Some(true),
            "false" | "no" | "off" => Some(false),
            _ => None,
        },
        _ => None,
    }
}

fn eval_attrs(env: &mut Env, attributes: &[crate::ast::JSXAttribute]) -> KainResult<Vec<UIAttr>> {
    let mut attrs = Vec::new();
    for attr in attributes {
        let lowered = match &attr.value {
            JSXAttrValue::String(s) => UIAttr::Property {
                name: attr.name.clone(),
                value: Value::String(s.clone()),
                expr: None,
            },
            JSXAttrValue::Bool(b) => UIAttr::Bool {
                name: attr.name.clone(),
                value: *b,
            },
            JSXAttrValue::Expr(expr) => {
                let value = eval_expr(env, expr)?;
                if let Some(event) = parse_event_name(&attr.name) {
                    UIAttr::Event {
                        name: attr.name.clone(),
                        event,
                        handler: value,
                        expr: Some(expr.clone()),
                    }
                } else {
                    UIAttr::Property {
                        name: attr.name.clone(),
                        value,
                        expr: Some(expr.clone()),
                    }
                }
            }
        };
        attrs.push(lowered);
    }
    Ok(attrs)
}

fn eval_children(env: &mut Env, children: &[JSXNode]) -> KainResult<Vec<VNode>> {
    let mut out = Vec::new();
    for child in children {
        let rendered = eval_jsx(env, child)?;
        flatten_value_into_children(rendered, &mut out);
    }
    Ok(out)
}

fn flatten_value_into_children(value: Value, out: &mut Vec<VNode>) {
    match value {
        Value::JSX(node) => out.push(node),
        Value::String(s) => out.push(VNode::Text(s)),
        Value::Int(n) => out.push(VNode::Text(n.to_string())),
        Value::Float(n) => out.push(VNode::Text(n.to_string())),
        Value::Bool(b) => out.push(VNode::Text(b.to_string())),
        Value::Array(items) => {
            for item in items.read().unwrap().iter().cloned() {
                flatten_value_into_children(item, out);
            }
        }
        Value::Tuple(items) => {
            for item in items {
                flatten_value_into_children(item, out);
            }
        }
        _ => {}
    }
}

fn attrs_to_props_map(attrs: &[UIAttr]) -> HashMap<String, Value> {
    let mut props = HashMap::new();
    for attr in attrs {
        match attr {
            UIAttr::Property { name, value, .. } => {
                props.insert(name.clone(), value.clone());
            }
            UIAttr::Bool { name, value } => {
                props.insert(name.clone(), Value::Bool(*value));
            }
            UIAttr::Event { .. } => {
                // Events are semantic routes, not component props.
                // Keep them out of the runtime prop map so we don't smuggle
                // event meaning through a backend-shaped shortcut.
            }
        }
    }
    props
}

fn render_attr_to_string(attr: &UIAttr) -> String {
    match attr {
        UIAttr::Property { name, value, .. } => format!(r#"{}="{}""#, name, value),
        UIAttr::Bool { name, value } => {
            if *value {
                name.clone()
            } else {
                String::new()
            }
        }
        UIAttr::Event { name, .. } => {
            // Keep debug-string rendering opaque so event semantics do not leak back
            // out through HTML-like debug output.
            format!(r#"{}="[event-route]""#, name)
        }
    }
}

fn parse_event_name(name: &str) -> Option<UIEvent> {
    let normalized = name
        .strip_prefix("on_")
        .or_else(|| name.strip_prefix("on"))
        .unwrap_or(name)
        .to_ascii_lowercase();

    Some(match normalized.as_str() {
        "click" => UIEvent::Click,
        "input" => UIEvent::Input,
        "change" => UIEvent::Change,
        "submit" => UIEvent::Submit,
        "focus" => UIEvent::Focus,
        "blur" => UIEvent::Blur,
        "keydown" => UIEvent::KeyDown,
        "keyup" => UIEvent::KeyUp,
        "pointerdown" => UIEvent::PointerDown,
        "pointerup" => UIEvent::PointerUp,
        "pointermove" => UIEvent::PointerMove,
        _ if name.starts_with("on") || name.starts_with("on_") => UIEvent::Custom(normalized),
        _ => return None,
    })
}

fn event_name(event: &UIEvent) -> &str {
    match event {
        UIEvent::Click => "click",
        UIEvent::Input => "input",
        UIEvent::Change => "change",
        UIEvent::Submit => "submit",
        UIEvent::Focus => "focus",
        UIEvent::Blur => "blur",
        UIEvent::KeyDown => "keydown",
        UIEvent::KeyUp => "keyup",
        UIEvent::PointerDown => "pointerdown",
        UIEvent::PointerUp => "pointerup",
        UIEvent::PointerMove => "pointermove",
        UIEvent::Custom(name) => name.as_str(),
    }
}

fn value_is_truthy(value: &Value) -> bool {
    match value {
        Value::Unit | Value::None => false,
        Value::Bool(v) => *v,
        Value::Int(v) => *v != 0,
        Value::Float(v) => *v != 0.0,
        Value::String(v) => !v.is_empty(),
        Value::Array(items) => !items.read().unwrap().is_empty(),
        Value::Tuple(items) => !items.is_empty(),
        Value::JSX(_) => true,
        _ => true,
    }
}

fn find_attr_key(attrs: &[UIAttr], name: &str) -> Option<String> {
    for attr in attrs {
        match attr {
            UIAttr::Property {
                name: attr_name,
                value,
                ..
            } if attr_name == name => return Some(value_to_key_string(value)),
            UIAttr::Bool {
                name: attr_name,
                value,
            } if attr_name == name => return Some(value.to_string()),
            _ => {}
        }
    }
    None
}

fn value_to_key_string(value: &Value) -> String {
    match value {
        Value::String(v) => v.clone(),
        Value::Int(v) => v.to_string(),
        Value::Float(v) => v.to_string(),
        Value::Bool(v) => v.to_string(),
        _ => format!("{}", value),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ast::{Component, Expr, JSXAttribute, Param, Type, Visibility};
    use crate::runtime::Env;
    use crate::span::Span;
    use std::sync::{Arc, RwLock};

    #[test]
    fn test_render_to_string_element_with_attrs() {
        let node = VNode::Element {
            tag: "div".to_string(),
            attrs: vec![
                UIAttr::Property {
                    name: "class".to_string(),
                    value: Value::String("panel".to_string()),
                    expr: None,
                },
                UIAttr::Bool {
                    name: "hidden".to_string(),
                    value: true,
                },
            ],
            children: vec![VNode::Text("Hello".to_string())],
            key: Some("root".to_string()),
        };

        assert_eq!(
            render_to_string(&node),
            r#"<div class="panel" hidden>Hello</div>"#
        );
    }

    #[test]
    fn test_eval_jsx_if_uses_then_branch_when_true() {
        let mut env = Env::new();
        let node = JSXNode::If {
            condition: Box::new(Expr::Bool(true, Span::default())),
            then_branch: Box::new(JSXNode::Text("yes".to_string(), Span::default())),
            else_branch: Some(Box::new(JSXNode::Text("no".to_string(), Span::default()))),
            span: Span::default(),
        };

        let rendered = eval_jsx(&mut env, &node).unwrap();
        match rendered {
            Value::String(text) => assert_eq!(text, "yes"),
            other => panic!("expected string branch, got {:?}", other),
        }
    }

    #[test]
    fn test_eval_jsx_for_expands_array_children() {
        let mut env = Env::new();
        env.define(
            "items".to_string(),
            Value::Array(Arc::new(RwLock::new(vec![
                Value::String("A".to_string()),
                Value::String("B".to_string()),
            ]))),
        );
        let node = JSXNode::For {
            binding: "item".to_string(),
            iter: Box::new(Expr::Ident("items".to_string(), Span::default())),
            body: Box::new(JSXNode::Element {
                tag: "span".to_string(),
                attributes: Vec::new(),
                children: vec![JSXNode::Expression(Box::new(Expr::Ident(
                    "item".to_string(),
                    Span::default(),
                )))],
                span: Span::default(),
            }),
            span: Span::default(),
        };

        let rendered = eval_jsx(&mut env, &node).unwrap();
        match rendered {
            Value::JSX(VNode::Fragment(children)) => {
                assert_eq!(children.len(), 2);
                assert_eq!(render_to_string(&children[0]), "<span>A</span>");
                assert_eq!(render_to_string(&children[1]), "<span>B</span>");
            }
            other => panic!("expected fragment result, got {:?}", other),
        }
    }

    #[test]
    fn test_eval_jsx_component_call_renders_registered_component() {
        let mut env = Env::new();
        env.register_component(Component {
            name: "Panel".to_string(),
            props: vec![Param {
                name: "title".to_string(),
                ty: Type::Named {
                    name: "String".to_string(),
                    generics: Vec::new(),
                    span: Span::default(),
                },
                mutable: false,
                default: None,
                span: Span::default(),
            }],
            state: Vec::new(),
            methods: Vec::new(),
            effects: Vec::new(),
            body: JSXNode::Element {
                tag: "panel".to_string(),
                attributes: vec![JSXAttribute {
                    name: "title".to_string(),
                    value: JSXAttrValue::Expr(Expr::Ident("title".to_string(), Span::default())),
                    span: Span::default(),
                }],
                children: vec![JSXNode::Expression(Box::new(Expr::Ident(
                    "children".to_string(),
                    Span::default(),
                )))],
                span: Span::default(),
            },
            visibility: Visibility::Public,
            attributes: Vec::new(),
            span: Span::default(),
        });

        let node = JSXNode::ComponentCall {
            name: "Panel".to_string(),
            props: vec![JSXAttribute {
                name: "title".to_string(),
                value: JSXAttrValue::String("Inspector".to_string()),
                span: Span::default(),
            }],
            children: vec![JSXNode::Text("Body".to_string(), Span::default())],
            span: Span::default(),
        };

        let rendered = eval_jsx(&mut env, &node).unwrap();
        match rendered {
            Value::JSX(VNode::Component { instance, rendered }) => {
                assert_eq!(instance.name, "Panel");
                assert_eq!(
                    instance.props.get("title").map(ToString::to_string),
                    Some("Inspector".to_string())
                );
                assert_eq!(
                    render_to_string(&rendered),
                    r#"<panel title="Inspector">Body</panel>"#
                );
            }
            other => panic!("expected component result, got {:?}", other),
        }
    }

    #[test]
    fn test_lower_value_to_ui_tree_maps_semantic_tags() {
        let output = lower_value_to_ui_tree(Value::JSX(VNode::Element {
            tag: "panel".to_string(),
            attrs: vec![UIAttr::Property {
                name: "title".to_string(),
                value: Value::String("Inspector".to_string()),
                expr: None,
            }],
            children: vec![VNode::Text("Body".to_string())],
            key: None,
        }));

        let root = output.tree.root_node().expect("root node should exist");
        assert_eq!(root.kind, UiWidgetKind::Panel);
        assert_eq!(
            root.props.get("title"),
            Some(&UiValue::String("Inspector".to_string()))
        );
        assert_eq!(output.tree.nodes.len(), 2);
    }

    #[test]
    fn test_build_ui_output_from_source_renders_root_component() {
        let source = r#"
component App():
    render <panel title="Kain UI"><inspector title="Selection">Ready</inspector></panel>
"#;

        let output = build_ui_output_from_source(source, "App").expect("source should render");
        let debug = render_ui_output_debug(&output);

        assert!(debug.contains("Panel"));
        assert!(debug.contains("Inspector"));
        assert!(debug.contains("Ready"));
    }

    #[test]
    fn test_build_ui_output_from_source_extracts_theme_blocks_and_text_roles() {
        let source = r##"
component App():
    render <slot>
        <theme name="forge">
            <scope name="app_shell" selector="app-shell" />
            <token name="surface.background" category="color" value="#101418" />
            <variant scope="app_shell" name="hero">
                <token name="surface.mode" category="surface" value="glass" />
            </variant>
            <widget kind="panel" scope="app_shell" variant="hero">
                <token name="surface.padding" category="space" value={14} />
                <token name="density" category="layout" value="compact" />
            </widget>
            <textvariant scope="app_shell" name="hero">
                <token name="body.size" category="type" value={30} />
            </textvariant>
        </theme>
        <panel scope="app_shell" variant="hero" class="dense accent">
            <text role="hero">Studio</text>
        </panel>
    </slot>
"##;

        let output = build_ui_output_from_source(source, "App").expect("source should render");
        let registry = &output.systems.theme_registry;

        assert_eq!(registry.active_theme.as_deref(), Some("forge"));
        assert!(registry
            .scopes
            .iter()
            .any(|scope| scope.name == "app_shell" && scope.selector == "app-shell"));
        assert!(registry
            .semantic_tokens
            .iter()
            .any(|token| token.name == "surface.background"
                && token.value == UiValue::String("#101418".to_string())));
        assert!(registry
            .semantic_tokens
            .iter()
            .any(|token| token.name == "variant.hero.surface.mode"
                && token.value == UiValue::String("glass".to_string())));
        assert!(registry.semantic_tokens.iter().any(|token| token.name
            == "widget.panel.variant.hero.surface.padding"
            && token.value == UiValue::Int(14)));
        assert!(registry
            .semantic_tokens
            .iter()
            .any(|token| token.name == "widget.text.variant.hero.body.size"
                && token.value == UiValue::Int(30)));
        assert!(registry.variants.iter().any(|variant| {
            variant.scope == "app_shell"
                && variant.name == "hero"
                && variant
                    .tokens
                    .contains(&"widget.panel.variant.hero.density".to_string())
                && variant
                    .tokens
                    .contains(&"widget.text.variant.hero.body.size".to_string())
        }));
        assert!(!registry
            .semantic_tokens
            .iter()
            .any(|token| token.name == "text.default"));

        let root = output.tree.root_node().expect("root node should exist");
        assert_eq!(root.kind, UiWidgetKind::ComponentRef("App".to_string()));
        assert_eq!(output.tree.nodes.len(), 4);

        let slot = output
            .tree
            .node(root.children[0])
            .expect("slot node should exist");
        assert_eq!(slot.kind, UiWidgetKind::Slot);

        let panel = output
            .tree
            .node(slot.children[0])
            .expect("panel node should exist");
        assert_eq!(panel.kind, UiWidgetKind::Panel);
        assert_eq!(panel.style.theme_scope.as_deref(), Some("app_shell"));
        assert_eq!(panel.style.variant.as_deref(), Some("hero"));
        assert!(panel.style.classes.contains(&"dense".to_string()));
        assert!(panel.style.classes.contains(&"accent".to_string()));

        let text = output
            .tree
            .node(panel.children[0])
            .expect("text node should exist");
        assert_eq!(text.kind, UiWidgetKind::Text);
        assert_eq!(text.style.variant.as_deref(), Some("hero"));
        assert_eq!(
            text.props.get("role"),
            Some(&UiValue::String("hero".to_string()))
        );
        assert_eq!(
            text.props.get("text"),
            Some(&UiValue::String("Studio".to_string()))
        );
    }

    #[test]
    fn test_layout_attrs_lower_into_semantic_layout() {
        let source = r#"
component App():
    render <panel layout="dock" dock="left" split_ratio={0.25} width="35%" min_width={220} max_width={480} flex_grow={1} flex_shrink={0} align="stretch" justify="space-between" overflow="hidden" resizable={true} persistent_layout_id="shell_left" tab_group_id="center_tabs" tab_label="Scene" tab_order={2} tab_default_active={true} tab_closable={true} />
"#;

        let output = build_ui_output_from_source(source, "App").expect("source should render");
        let root = output.tree.root_node().expect("root node should exist");
        let panel = output
            .tree
            .node(root.children[0])
            .expect("panel node should exist");

        assert_eq!(panel.layout.kind, kain_ui::UiLayoutKind::Dock);
        assert_eq!(panel.layout.dock, Some(UiDockPlacement::Left));
        assert_eq!(panel.layout.split_ratio, Some(0.25));
        assert_eq!(
            panel.layout.width.map(|value| value.unit),
            Some(UiLengthUnit::Percent)
        );
        assert_eq!(panel.layout.width.map(|value| value.value), Some(35.0));
        assert_eq!(panel.layout.min_width, Some(220.0));
        assert_eq!(panel.layout.max_width, Some(480.0));
        assert_eq!(panel.layout.flex_grow, 1.0);
        assert_eq!(panel.layout.flex_shrink, 0.0);
        assert_eq!(panel.layout.align_items, UiLayoutAlignment::Stretch);
        assert_eq!(
            panel.layout.justify_content,
            UiLayoutAlignment::SpaceBetween
        );
        assert_eq!(panel.layout.overflow_x, UiOverflowBehavior::Hidden);
        assert_eq!(panel.layout.overflow_y, UiOverflowBehavior::Hidden);
        assert!(panel.layout.resizable);
        assert_eq!(
            panel.layout.persistent_layout_id.as_deref(),
            Some("shell_left")
        );
        assert_eq!(panel.layout.tab_group_id.as_deref(), Some("center_tabs"));
        assert_eq!(panel.layout.tab_label.as_deref(), Some("Scene"));
        assert_eq!(panel.layout.tab_order, Some(2));
        assert!(panel.layout.tab_default_active);
        assert!(panel.layout.tab_closable);
        assert!(!panel.props.contains_key("layout"));
        assert!(!panel.props.contains_key("dock"));
        assert!(!panel.props.contains_key("resizable"));
        assert!(!panel.props.contains_key("tab_group_id"));
        assert!(!panel.props.contains_key("tab_default_active"));
    }

    #[test]
    fn test_ui_backend_profiles_are_data_driven() {
        let react = ui_backend_profile(UIBackendKind::ReactDom);
        let slate = ui_backend_profile(UIBackendKind::Slate);

        assert_eq!(react.event_prefix, "on");
        assert!(react.supports_keyed_children);
        assert_eq!(slate.fragment_tag, "SFragment");
        assert!(!slate.supports_keyed_children);
    }

    #[test]
    fn test_parse_surface_host_backend_supports_tauri_aliases() {
        assert_eq!(
            parse_surface_host_backend("tauri".to_string()),
            Some(UiHostBackendKind::Tauri)
        );
        assert_eq!(
            parse_surface_host_backend("webview".to_string()),
            Some(UiHostBackendKind::Tauri)
        );
    }
}
