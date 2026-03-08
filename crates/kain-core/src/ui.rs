//! KAIN UI subsystem primitives and JSX evaluation helpers.

use crate::ast::{JSXAttrValue, JSXNode};
use crate::error::KainResult;
use crate::runtime::{eval_expr, Env, Value};
use std::collections::HashMap;
use std::fmt;

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
    Property { name: String, value: Value },
    Bool { name: String, value: bool },
    Event { name: String, event: UIEvent, handler: Value },
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
        (Some(VNode::Fragment(_)), VNode::Fragment(children)) => {
            VNode::Fragment(children.clone())
        }
        (
            Some(VNode::Component { instance: old_instance, .. }),
            VNode::Component {
                instance,
                rendered,
            },
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
        JSXNode::Fragment(children, _) => Ok(Value::JSX(VNode::Fragment(eval_children(
            env, children,
        )?))),
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
        } => {
            let attrs = eval_attrs(env, props)?;
            let rendered_children = eval_children(env, children)?;
            let props = attrs_to_props_map(&attrs);
            let instance = ComponentInstance {
                name: name.clone(),
                props,
                children: rendered_children.clone(),
                state: HashMap::new(),
            };

            Ok(Value::JSX(VNode::Component {
                instance,
                rendered: Box::new(VNode::Fragment(rendered_children)),
            }))
        }
    }
}

fn eval_attrs(
    env: &mut Env,
    attributes: &[crate::ast::JSXAttribute],
) -> KainResult<Vec<UIAttr>> {
    let mut attrs = Vec::new();
    for attr in attributes {
        let lowered = match &attr.value {
            JSXAttrValue::String(s) => UIAttr::Property {
                name: attr.name.clone(),
                value: Value::String(s.clone()),
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
                    }
                } else {
                    UIAttr::Property {
                        name: attr.name.clone(),
                        value,
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
            UIAttr::Property { name, value } => {
                props.insert(name.clone(), value.clone());
            }
            UIAttr::Bool { name, value } => {
                props.insert(name.clone(), Value::Bool(*value));
            }
            UIAttr::Event { name, handler, .. } => {
                props.insert(name.clone(), handler.clone());
            }
        }
    }
    props
}

fn render_attr_to_string(attr: &UIAttr) -> String {
    match attr {
        UIAttr::Property { name, value } => format!(r#"{}="{}""#, name, value),
        UIAttr::Bool { name, value } => {
            if *value {
                name.clone()
            } else {
                String::new()
            }
        }
        UIAttr::Event { name, event, .. } => {
            format!(r#"{}="[event:{}]""#, name, event_name(event))
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
    use crate::ast::{Expr, JSXAttribute};
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
            else_branch: Some(Box::new(JSXNode::Text(
                "no".to_string(),
                Span::default(),
            ))),
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
    fn test_eval_jsx_component_call_creates_instance() {
        let mut env = Env::new();
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
                assert_eq!(render_to_string(&rendered), "Body");
            }
            other => panic!("expected component result, got {:?}", other),
        }
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
}
