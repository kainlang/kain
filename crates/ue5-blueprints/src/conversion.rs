use crate::error::Result;
use crate::ir::{
    BlueprintDef, ComponentDef, EventGraphNode, KismetCall, PropertyDef, PropertyValue,
};
use kain_core::ast;

// ─── Data-driven parent class mapping ────────────────────────────────────────
// Maps KAIN attribute values to UE5 parent class paths.
// Extend this table instead of adding if/else branches.
const PARENT_CLASS_MAP: &[(&str, &str)] = &[
    ("actor", "/Script/Engine.Actor"),
    ("pawn", "/Script/Engine.Pawn"),
    ("character", "/Script/Engine.Character"),
    ("gamemode", "/Script/Engine.GameModeBase"),
    ("gamestate", "/Script/Engine.GameStateBase"),
    ("playercontroller", "/Script/Engine.PlayerController"),
    ("hud", "/Script/Engine.HUD"),
    ("playerstate", "/Script/Engine.PlayerState"),
];

const DEFAULT_PARENT_CLASS: &str = "/Script/Engine.Actor";

/// Convert a KAIN AST Actor to a BlueprintDef IR.
pub fn from_ast(actor: &ast::Actor) -> Result<BlueprintDef> {
    // 1. Name & Paths
    let name = format!("BP_{}", actor.name);
    let package_path = format!("/Game/Blueprints/{}", name);

    // 2. Parent class resolution from @parent("...") or @extends("...") attributes
    let parent_class = resolve_parent_class(&actor.attributes);

    let mut bp = BlueprintDef::new(&name, &package_path, &parent_class);

    // 3. Scan state fields for Components & Properties
    for state in &actor.state {
        let is_component = state.attributes.iter().any(|attr| attr.name == "component");

        if is_component {
            let var_name = &state.name;
            let class_name =
                extract_type_name(&state.ty).unwrap_or_else(|| "SceneComponent".to_string());

            let mut comp_def = ComponentDef::new(class_name, var_name);

            // Parse component defaults from struct literal initializer
            // e.g. state mesh: StaticMeshComponent = { static_mesh: "/Game/...", cast_shadow: true }
            if let Some(defaults) = extract_struct_defaults(&state.initial) {
                for prop in defaults {
                    comp_def = comp_def.with_default(prop);
                }
            }

            // @attach("root") → parent component
            if let Some(attach_attr) = state.attributes.iter().find(|a| a.name == "attach") {
                if let Some(first_arg) = attach_attr.args.first() {
                    if let ast::Expr::String(parent_name, _) = first_arg {
                        comp_def = comp_def.with_parent(parent_name);
                    }
                }
            }

            bp = bp.add_component(comp_def);
        } else {
            if let Some(prop) = convert_property(state) {
                bp = bp.add_default(prop);
            }
        }
    }

    // 4. Scan handlers for Event Graph
    for handler in &actor.handlers {
        let calls = convert_block_to_calls(&handler.body)?;
        let event = match handler.message_type.as_str() {
            "init" | "begin_play" => EventGraphNode::begin_play(calls),
            "tick" => EventGraphNode::tick(calls),
            name => EventGraphNode::custom(name, calls),
        };
        bp = bp.add_event(event);
    }

    Ok(bp)
}

// ─── Parent class resolution ─────────────────────────────────────────────────

/// Resolve parent class from actor attributes.
///
/// Checks for `@parent("...")` or `@extends("...")`. The value can be:
///   - A full path: `"/Script/Engine.Character"` → used as-is
///   - A short name: `"Character"` → looked up in PARENT_CLASS_MAP
///   - Missing → defaults to AActor
fn resolve_parent_class(attributes: &[ast::Attribute]) -> String {
    for attr in attributes {
        if attr.name == "parent" || attr.name == "extends" {
            if let Some(ast::Expr::String(value, _)) = attr.args.first() {
                // Full path?
                if value.contains('/') {
                    return value.clone();
                }
                // Short name lookup (case-insensitive)
                let lower = value.to_lowercase();
                if let Some((_, path)) = PARENT_CLASS_MAP
                    .iter()
                    .find(|(key, _)| *key == lower.as_str())
                {
                    return path.to_string();
                }
                // Assume it's a class name in /Script/Engine
                return format!("/Script/Engine.{}", value);
            }
        }
    }
    DEFAULT_PARENT_CLASS.to_string()
}

// ─── Type extraction ─────────────────────────────────────────────────────────

fn extract_type_name(ty: &ast::Type) -> Option<String> {
    match ty {
        ast::Type::Named { name, .. } => Some(name.clone()),
        _ => None,
    }
}

// ─── Property conversion ─────────────────────────────────────────────────────

/// Convert a KAIN state declaration into a PropertyDef.
/// Handles: Float, Int, Bool, String, Vec3, struct literals, and type-hinted enums.
fn convert_property(state: &ast::StateDecl) -> Option<PropertyDef> {
    convert_expr_to_property(&state.name, &state.initial, Some(&state.ty))
}

/// Convert an AST expression to a PropertyDef with optional type hint.
fn convert_expr_to_property(
    name: &str,
    expr: &ast::Expr,
    _ty_hint: Option<&ast::Type>,
) -> Option<PropertyDef> {
    match expr {
        ast::Expr::Float(v, _) => Some(PropertyDef::float(name, *v as f32)),
        ast::Expr::Int(v, _) => {
            // If the type is explicitly Int64, use that
            Some(PropertyDef::int(name, *v as i32))
        }
        ast::Expr::Bool(v, _) => Some(PropertyDef::bool(name, *v)),
        ast::Expr::String(v, _) => {
            if v.starts_with("/Game/") || v.starts_with("/Script/") {
                Some(PropertyDef::soft_object(name, v))
            } else {
                Some(PropertyDef::str(name, v))
            }
        }
        // vec3(x, y, z) → Vector property
        ast::Expr::Call { callee, args, .. } => {
            if let ast::Expr::Ident(func_name, _) = &**callee {
                match func_name.as_str() {
                    "vec3" if args.len() == 3 => {
                        let x = eval_float(&args[0].value).unwrap_or(0.0);
                        let y = eval_float(&args[1].value).unwrap_or(0.0);
                        let z = eval_float(&args[2].value).unwrap_or(0.0);
                        Some(PropertyDef::vector(name, x, y, z))
                    }
                    "rotator" if args.len() == 3 => {
                        let p = eval_float(&args[0].value).unwrap_or(0.0);
                        let y = eval_float(&args[1].value).unwrap_or(0.0);
                        let r = eval_float(&args[2].value).unwrap_or(0.0);
                        Some(PropertyDef::rotator(name, p, y, r))
                    }
                    "color" | "linear_color" if args.len() >= 3 => {
                        let r = eval_float(&args[0].value).unwrap_or(0.0);
                        let g = eval_float(&args[1].value).unwrap_or(0.0);
                        let b = eval_float(&args[2].value).unwrap_or(0.0);
                        let a = if args.len() >= 4 {
                            eval_float(&args[3].value).unwrap_or(1.0)
                        } else {
                            1.0
                        };
                        Some(PropertyDef::color(name, r, g, b, a))
                    }
                    _ => None,
                }
            } else {
                None
            }
        }
        // EnumType::Variant → Enum property
        ast::Expr::EnumVariant {
            enum_name, variant, ..
        } => Some(PropertyDef::enum_val(name, enum_name, variant)),
        // Struct literal: Point { x: 1, y: 2 } → Struct property
        ast::Expr::Struct {
            name: struct_name,
            fields,
            ..
        } => {
            let inner: Vec<PropertyDef> = fields
                .iter()
                .filter_map(|(fname, fexpr)| convert_expr_to_property(fname, fexpr, None))
                .collect();
            Some(PropertyDef {
                name: name.to_string(),
                value: PropertyValue::Struct {
                    struct_type: struct_name.clone(),
                    fields: inner,
                },
            })
        }
        _ => None,
    }
}

/// Extract struct-literal field defaults from an initializer expression.
/// e.g. `{ static_mesh: "/Game/...", cast_shadow: true }` → Vec<PropertyDef>
fn extract_struct_defaults(expr: &ast::Expr) -> Option<Vec<PropertyDef>> {
    match expr {
        ast::Expr::Struct { fields, .. } => {
            let props: Vec<PropertyDef> = fields
                .iter()
                .filter_map(|(name, val)| convert_expr_to_property(name, val, None))
                .collect();
            if props.is_empty() {
                None
            } else {
                Some(props)
            }
        }
        _ => None,
    }
}

/// Check if a type hint matches a given name (case-insensitive).
#[allow(dead_code)]
fn matches_type_name(ty: Option<&ast::Type>, expected: &str) -> bool {
    match ty {
        Some(ast::Type::Named { name, .. }) => name.eq_ignore_ascii_case(expected),
        _ => false,
    }
}

/// Evaluate a simple numeric expression to f32.
fn eval_float(expr: &ast::Expr) -> Option<f32> {
    match expr {
        ast::Expr::Float(v, _) => Some(*v as f32),
        ast::Expr::Int(v, _) => Some(*v as f32),
        ast::Expr::Unary {
            op: ast::UnaryOp::Neg,
            operand,
            ..
        } => eval_float(operand).map(|v| -v),
        _ => None,
    }
}

// ─── Block → KismetCall conversion ──────────────────────────────────────────

/// Convert a block of statements into a linear chain of Kismet calls.
fn convert_block_to_calls(block: &ast::Block) -> Result<Vec<KismetCall>> {
    let mut calls = Vec::new();

    for stmt in &block.stmts {
        if let ast::Stmt::Expr(expr) = stmt {
            // function_name(args)
            if let ast::Expr::Call { callee, .. } = expr {
                if let ast::Expr::Ident(func_name, _) = &**callee {
                    calls.push(KismetCall::function(func_name));
                }
            }
            // object.method(args) → targeted call
            else if let ast::Expr::MethodCall {
                receiver, method, ..
            } = expr
            {
                if let ast::Expr::Ident(target_name, _) = &**receiver {
                    calls.push(KismetCall::function(method).on(target_name));
                }
            }
        }
    }

    Ok(calls)
}
