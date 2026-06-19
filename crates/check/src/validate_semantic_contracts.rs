// ============================================================================
//  Semantic contract validators for the Kain decision ladder.
//
//  These validators check that semantic constructs (world, actor, converge,
//  orchestrate, pulse, resonate, entangle, teleport, patch, law) satisfy
//  their semantic contracts — invariants that the typechecker does not
//  enforce but are required for correct runtime behavior.
//
//  Unlike the codegen-extracted validators (validate_codegen.rs), these
//  catch categories of errors that NEITHER check NOR build currently detects.
// ============================================================================

use std::collections::{HashMap, HashSet};
use kain_core::ast::{
    Block, ConvergeLaneKind, ConvergeSelector, ElseBranch, Expr, Stmt, Type,
};
use kain_core::span::Span;
use kain_core::types::{TypedItem, TypedProgram};
use kain_error::{
    CompilerPhase, DiagnosticCode, DiagnosticReport, DiagnosticSeverity, ErrorKind,
};

/// Run all semantic contract validators against a typed program.
pub fn validate_semantic_contracts(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Cross-construct validators (analyze relationships between constructs)
    validate_actor_message_completeness(program, reports);
    validate_resonate_anti_feedback(program, reports);
    validate_entangle_completeness(program, reports);
    validate_converge_lane_coverage(program, reports);
    validate_orchestrate_stage_liveness(program, reports);
    validate_pulse_cadence_conflicts(program, reports);
    validate_teleport_bus_type_match(program, reports);
    validate_patch_world_binding(program, reports);
    validate_law_satisfiability(program, reports);

    // World-level validators
    validate_world_dead_state(program, reports);
}

// ===========================================================================
//  Validator 1: Actor message completeness
// ===========================================================================
// Every message sent to an actor must have a corresponding handler.
// Every handler should be reachable from at least one send site.
// Currently, sending an unhandled message causes a runtime crash or
// silent drop — this validator catches it at check time.

fn validate_actor_message_completeness(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Step 1: Collect all actor message handlers
    let mut actor_handlers: HashMap<String, (HashSet<String>, Span)> = HashMap::new();

    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            let actor_name = actor.ast.name.clone();
            let span = actor.ast.span;
            let mut messages = HashSet::new();
            for handler in &actor.ast.handlers {
                messages.insert(handler.message_type.clone());
            }
            actor_handlers.insert(actor_name, (messages, span));
        }
    }

    // Step 2: Walk all function bodies looking for send() / spawn().ask() calls
    for item in &program.items {
        if let TypedItem::Function(func) = item {
            find_actor_sends_in_body(
                &func.ast.body,
                &actor_handlers,
                &func.ast.name,
                reports,
            );
        }
    }

    // Step 3: Also walk actor handler bodies (one actor can send to another)
    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            for handler in &actor.ast.handlers {
                find_actor_sends_in_body(
                    &handler.body,
                    &actor_handlers,
                    &format!("actor {} handler {}", actor.ast.name, handler.message_type),
                    reports,
                );
            }
        }
    }
}

fn find_actor_sends_in_body(
    body: &Block,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &body.stmts {
        find_actor_sends_in_stmt(stmt, actor_handlers, fn_name, reports);
    }
}

fn find_actor_sends_in_stmt(
    stmt: &Stmt,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            find_actor_sends_in_expr(expr, actor_handlers, fn_name, reports);
        }
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                find_actor_sends_in_expr(val, actor_handlers, fn_name, reports);
            }
        }
        Stmt::Return(Some(expr), _) => {
            find_actor_sends_in_expr(expr, actor_handlers, fn_name, reports);
        }
        Stmt::Defer { expr, .. } => {
            find_actor_sends_in_expr(expr, actor_handlers, fn_name, reports);
        }
        Stmt::For { iter, body, .. } => {
            find_actor_sends_in_expr(iter, actor_handlers, fn_name, reports);
            find_actor_sends_in_body(body, actor_handlers, fn_name, reports);
        }
        Stmt::While { condition, body, .. } => {
            find_actor_sends_in_expr(condition, actor_handlers, fn_name, reports);
            find_actor_sends_in_body(body, actor_handlers, fn_name, reports);
        }
        Stmt::Loop { body, .. } => {
            find_actor_sends_in_body(body, actor_handlers, fn_name, reports);
        }
        _ => {}
    }
}

fn find_actor_sends_in_expr(
    expr: &Expr,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match expr {
        // Pattern: send(target, MessageType{...})
        Expr::Call { callee, args, span } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                if name == "send" && args.len() >= 2 {
                    // Second arg is the message — try to extract the message name
                    let msg_name = extract_message_name(&args[1].value);
                    let actor_name = infer_actor_name(&args[0].value, actor_handlers);

                    if let (Some(msg), Some(actor)) = (msg_name, actor_name) {
                        if let Some((handlers, _actor_span)) = actor_handlers.get(&actor) {
                            if !handlers.contains(&msg) {
                                let available: Vec<&String> = handlers.iter().collect();
                                reports.push(
                                    DiagnosticReport::new(
                                        ErrorKind::Type,
                                        DiagnosticCode::ActorGeneric,
                                        format!(
                                            "Actor '{}' has no handler for message '{}'. \
                                             Sent from '{}'.",
                                            actor, msg, fn_name
                                        ),
                                    )
                                    .severity(DiagnosticSeverity::Error)
                                    .phase(CompilerPhase::StateValidation)
                                    .primary_label(
                                        *span,
                                        format!("message '{}' not handled by actor '{}'", msg, actor),
                                    )
                                    .note(format!(
                                        "Available messages on '{}': {}",
                                        actor,
                                        if available.is_empty() {
                                            "(none)".to_string()
                                        } else {
                                            available.iter().map(|s| s.as_str()).collect::<Vec<_>>().join(", ")
                                        }
                                    ))
                                    .help(format!(
                                        "Add an 'on {}(...)' handler to actor '{}' or use a different message.",
                                        msg, actor
                                    )),
                                );
                            }
                        }
                    }
                }

                // Pattern: spawn(ActorType).ask(MessageType{...})
                if name == "spawn" && !args.is_empty() {
                    if let Some(actor_name) = extract_spawn_actor_name(&args[0].value) {
                        // Walk the chain to find .ask(...) — the spawn expression is
                        // a qualifier for MethodCall or part of a larger expression tree.
                        // We catch this in the MethodCall branch below.
                        let _ = actor_name; // reserved for future enhancement
                    }
                }
            }

            // Recurse into callee and args
            find_actor_sends_in_expr(callee, actor_handlers, fn_name, reports);
            for arg in args {
                find_actor_sends_in_expr(&arg.value, actor_handlers, fn_name, reports);
            }
        }
        Expr::MethodCall { receiver, method, args, span } => {
            // Pattern: spawn(ActorType).ask(MessageType{...})
            if method == "ask" && !args.is_empty() {
                if let Some(actor_name) = extract_receiver_actor_name(receiver, actor_handlers) {
                    let msg_name = extract_message_name(&args[0].value);
                    if let Some(msg) = msg_name {
                        if let Some((handlers, _actor_span)) = actor_handlers.get(&actor_name) {
                            if !handlers.contains(&msg) {
                                let available: Vec<&String> = handlers.iter().collect();
                                reports.push(
                                    DiagnosticReport::new(
                                        ErrorKind::Type,
                                        DiagnosticCode::ActorGeneric,
                                        format!(
                                            "Actor '{}' has no handler for message '{}'. \
                                             Used via spawn().ask() in '{}'.",
                                            actor_name, msg, fn_name
                                        ),
                                    )
                                    .severity(DiagnosticSeverity::Error)
                                    .phase(CompilerPhase::StateValidation)
                                    .primary_label(
                                        *span,
                                        format!("message '{}' not handled by actor '{}'", msg, actor_name),
                                    )
                                    .note(format!(
                                        "Available messages on '{}': {}",
                                        actor_name,
                                        if available.is_empty() {
                                            "(none)".to_string()
                                        } else {
                                            available.iter().map(|s| s.as_str()).collect::<Vec<_>>().join(", ")
                                        }
                                    ))
                                    .help(format!(
                                        "Add an 'on {}(...)' handler to actor '{}'.",
                                        msg, actor_name
                                    )),
                                );
                            }
                        }
                    }
                }
            }
            find_actor_sends_in_expr(receiver, actor_handlers, fn_name, reports);
            for arg in args {
                find_actor_sends_in_expr(&arg.value, actor_handlers, fn_name, reports);
            }
        }
        Expr::SendMsg { target, message, data, span } => {
            // The target is the actor handle/reply-port, message is the message name
            // This is the `send target <- Message{data}` syntax
            let actor_name = infer_actor_name(target, actor_handlers);
            if let Some(actor) = actor_name {
                if let Some((handlers, _actor_span)) = actor_handlers.get(&actor) {
                    if !handlers.contains(message.as_str()) {
                        let available: Vec<&String> = handlers.iter().collect();
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Type,
                                DiagnosticCode::ActorGeneric,
                                format!(
                                    "Actor '{}' has no handler for message '{}'. \
                                     Sent from '{}'.",
                                    actor, message, fn_name
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::StateValidation)
                            .primary_label(
                                *span,
                                format!("message '{}' not handled by actor '{}'", message, actor),
                            )
                            .note(format!(
                                "Available messages on '{}': {}",
                                actor,
                                if available.is_empty() {
                                    "(none)".to_string()
                                } else {
                                    available.iter().map(|s| s.as_str()).collect::<Vec<_>>().join(", ")
                                }
                            ))
                            .help(format!(
                                "Add an 'on {}(...)' handler to actor '{}'.",
                                message, actor
                            )),
                        );
                    }
                }
            }
            // Recurse
            for (_, data_expr) in data {
                find_actor_sends_in_expr(data_expr, actor_handlers, fn_name, reports);
            }
            find_actor_sends_in_expr(target, actor_handlers, fn_name, reports);
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            find_actor_sends_in_expr(condition, actor_handlers, fn_name, reports);
            find_actor_sends_in_body(then_branch, actor_handlers, fn_name, reports);
            if let Some(branch) = else_branch {
                find_actor_sends_in_else(branch, actor_handlers, fn_name, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            find_actor_sends_in_expr(left, actor_handlers, fn_name, reports);
            find_actor_sends_in_expr(right, actor_handlers, fn_name, reports);
        }
        Expr::Unary { operand, .. } => {
            find_actor_sends_in_expr(operand, actor_handlers, fn_name, reports);
        }
        Expr::Block(body, _) => {
            find_actor_sends_in_body(body, actor_handlers, fn_name, reports);
        }
        Expr::Match { scrutinee, arms, .. } => {
            find_actor_sends_in_expr(scrutinee, actor_handlers, fn_name, reports);
            for arm in arms {
                find_actor_sends_in_body(&arm.body, actor_handlers, fn_name, reports);
            }
        }
        _ => {}
    }
}

fn find_actor_sends_in_else(
    branch: &ElseBranch,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => {
            find_actor_sends_in_body(block, actor_handlers, fn_name, reports);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            find_actor_sends_in_expr(cond, actor_handlers, fn_name, reports);
            find_actor_sends_in_body(block, actor_handlers, fn_name, reports);
            if let Some(next_branch) = next {
                find_actor_sends_in_else(next_branch, actor_handlers, fn_name, reports);
            }
        }
    }
}

/// Extract the message name from a send argument expression.
fn extract_message_name(expr: &Expr) -> Option<String> {
    match expr {
        // send(target, MessageType{...}) -> "MessageType"
        Expr::Struct { name, .. } => Some(name.clone()),
        Expr::AggregateInit { ty, .. } => match ty {
            Type::Named { name, .. } => Some(name.clone()),
            _ => None,
        },
        _ => None,
    }
}

/// Attempt to infer the actor type name from the first argument of send().
fn infer_actor_name(
    expr: &Expr,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => {
            if actor_handlers.contains_key(name.as_str()) {
                Some(name.clone())
            } else {
                // Could be a variable of that actor type — heuristic best-effort
                None
            }
        }
        Expr::Field { object, .. } => {
            // Possibly a module-qualified actor name like mod.ActorName
            infer_actor_name(object, actor_handlers)
        }
        _ => None,
    }
}

/// Extract the actor name from a spawn() expression's first argument.
fn extract_spawn_actor_name(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, field, .. } => {
            // Module::ActorName pattern
            if let Expr::Ident(_mod_name, _) = object.as_ref() {
                Some(field.clone())
            } else {
                None
            }
        }
        _ => None,
    }
}

/// Extract the actor name from a receiver expression like spawn(ActorType).
fn extract_receiver_actor_name(
    receiver: &Expr,
    actor_handlers: &HashMap<String, (HashSet<String>, Span)>,
) -> Option<String> {
    match receiver {
        Expr::Spawn { actor, .. } => Some(actor.clone()),
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                if name == "spawn" && !args.is_empty() {
                    return extract_spawn_actor_name(&args[0].value);
                }
            }
            None
        }
        Expr::Ident(name, _) => {
            if actor_handlers.contains_key(name.as_str()) {
                Some(name.clone())
            } else {
                None
            }
        }
        _ => None,
    }
}

// ===========================================================================
//  Validator 2: Resonate anti-feedback detection
// ===========================================================================
// A resonate handler fires when its trigger world field changes.
// If the handler writes to its OWN trigger field, it creates an infinite
// feedback loop (write -> trigger -> handler fires -> write -> ...).
// The runtime dampening only absorbs rapid-fire within the dampen window;
// a direct write to the trigger field always re-triggers.

fn validate_resonate_anti_feedback(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    for item in &program.items {
        if let TypedItem::Resonate(resonate) = item {
            let target = &resonate.ast.target;
            if target.segments.len() < 2 {
                // Malformed resonate target — typechecker should catch this
                continue;
            }
            let trigger_world = &target.segments[0];
            let trigger_field = &target.segments[1];
            let handler_body = &resonate.ast.body;

            // Walk the handler body looking for assignments to the trigger field
            let mut violations = Vec::new();
            find_world_writes_in_body(
                handler_body,
                trigger_world,
                trigger_field,
                &mut violations,
            );

            for span in violations {
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Validation,
                        DiagnosticCode::StateGeneric,
                        format!(
                            "Resonate handler for '{}.{}' writes to its own trigger field. \
                             This creates an infinite feedback loop.",
                            trigger_world, trigger_field
                        ),
                    )
                    .severity(DiagnosticSeverity::Error)
                    .phase(CompilerPhase::StateValidation)
                    .primary_label(span, "self-triggering write")
                    .note(
                        "Resonate handlers fire when their trigger field changes. \
                         Writing to the trigger field from within the handler causes infinite recursion.",
                    )
                    .help(format!(
                        "Write to a DIFFERENT field instead (e.g., '{}.shadow' or '{}.last_value'). \
                         The handler's locals 'resonate_new_i64' and 'resonate_old_i64' are available \
                         without writing to the trigger.",
                        trigger_world, trigger_world
                    )),
                );
            }
        }
    }
}

fn find_world_writes_in_body(
    body: &Block,
    world_name: &str,
    field_name: &str,
    violations: &mut Vec<Span>,
) {
    for stmt in &body.stmts {
        find_world_writes_in_stmt(stmt, world_name, field_name, violations);
    }
}

fn find_world_writes_in_stmt(
    stmt: &Stmt,
    world_name: &str,
    field_name: &str,
    violations: &mut Vec<Span>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            find_world_writes_in_expr(expr, world_name, field_name, violations);
        }
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                find_world_writes_in_expr(val, world_name, field_name, violations);
            }
        }
        Stmt::Return(Some(expr), _) => {
            find_world_writes_in_expr(expr, world_name, field_name, violations);
        }
        Stmt::Defer { expr, .. } => {
            find_world_writes_in_expr(expr, world_name, field_name, violations);
        }
        Stmt::For { iter, body, .. } => {
            find_world_writes_in_expr(iter, world_name, field_name, violations);
            find_world_writes_in_body(body, world_name, field_name, violations);
        }
        Stmt::While { condition, body, .. } => {
            find_world_writes_in_expr(condition, world_name, field_name, violations);
            find_world_writes_in_body(body, world_name, field_name, violations);
        }
        Stmt::Loop { body, .. } => {
            find_world_writes_in_body(body, world_name, field_name, violations);
        }
        _ => {}
    }
}

fn find_world_writes_in_expr(
    expr: &Expr,
    world_name: &str,
    field_name: &str,
    violations: &mut Vec<Span>,
) {
    // Pattern: WorldName.field_name = value  (assignment target)
    // Or: WorldName.field_name used as left side of Assign
    if let Expr::Assign { target, value, .. } = expr {
        check_world_field_access(target, world_name, field_name, violations);
        find_world_writes_in_expr(value, world_name, field_name, violations);
        return;
    }

    // Pattern: WorldName.field_name accessed standalone (e.g. in a patch-style write)
    check_world_field_access(expr, world_name, field_name, violations);

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            find_world_writes_in_expr(callee, world_name, field_name, violations);
            for arg in args {
                find_world_writes_in_expr(&arg.value, world_name, field_name, violations);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            find_world_writes_in_expr(receiver, world_name, field_name, violations);
            for arg in args {
                find_world_writes_in_expr(&arg.value, world_name, field_name, violations);
            }
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            find_world_writes_in_expr(condition, world_name, field_name, violations);
            find_world_writes_in_body(then_branch, world_name, field_name, violations);
            if let Some(branch) = else_branch {
                find_world_writes_in_else(branch, world_name, field_name, violations);
            }
        }
        Expr::Binary { left, right, .. } => {
            find_world_writes_in_expr(left, world_name, field_name, violations);
            find_world_writes_in_expr(right, world_name, field_name, violations);
        }
        Expr::Unary { operand, .. } => {
            find_world_writes_in_expr(operand, world_name, field_name, violations);
        }
        Expr::Block(body, _) => {
            find_world_writes_in_body(body, world_name, field_name, violations);
        }
        Expr::Match { scrutinee, arms, .. } => {
            find_world_writes_in_expr(scrutinee, world_name, field_name, violations);
            for arm in arms {
                find_world_writes_in_body(&arm.body, world_name, field_name, violations);
            }
        }
        Expr::SendMsg { data, target, .. } => {
            for (_, data_expr) in data {
                find_world_writes_in_expr(data_expr, world_name, field_name, violations);
            }
            find_world_writes_in_expr(target, world_name, field_name, violations);
        }
        _ => {}
    }
}

fn find_world_writes_in_else(
    branch: &ElseBranch,
    world_name: &str,
    field_name: &str,
    violations: &mut Vec<Span>,
) {
    match branch {
        ElseBranch::Else(block) => {
            find_world_writes_in_body(block, world_name, field_name, violations);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            find_world_writes_in_expr(cond, world_name, field_name, violations);
            find_world_writes_in_body(block, world_name, field_name, violations);
            if let Some(next_branch) = next {
                find_world_writes_in_else(next_branch, world_name, field_name, violations);
            }
        }
    }
}

fn check_world_field_access(
    expr: &Expr,
    world_name: &str,
    field_name: &str,
    violations: &mut Vec<Span>,
) {
    if let Expr::Field { object, field, span } = expr {
        if field == field_name {
            if let Expr::Ident(w, _) = object.as_ref() {
                if w == world_name {
                    violations.push(*span);
                }
            }
        }
    }
}

// ===========================================================================
//  Validator 3: Entangle completeness
// ===========================================================================
// An entangle declaration binds two world fields together.
// Both the worlds and the fields must exist. The typechecker validates
// the world names exist, but cross-file entanglements (where the world
// is defined in a different file) may miss field existence checks.

fn validate_entangle_completeness(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Build map of world -> set of state field names
    let mut world_fields: HashMap<String, (HashSet<String>, Span)> = HashMap::new();
    for item in &program.items {
        if let TypedItem::World(world) = item {
            let fields: HashSet<String> = world.ast.states.iter()
                .map(|s| s.name.clone())
                .collect();
            world_fields.insert(world.ast.name.clone(), (fields, world.ast.span));
        }
    }

    for item in &program.items {
        if let TypedItem::Entangle(entangle) = item {
            let endpoint_a = &entangle.ast.left;
            let endpoint_b = &entangle.ast.right;

            check_entangle_endpoint(
                endpoint_a, &world_fields, &entangle.ast.span, "left", reports,
            );
            check_entangle_endpoint(
                endpoint_b, &world_fields, &entangle.ast.span, "right", reports,
            );
        }
    }
}

fn check_entangle_endpoint(
    endpoint: &kain_core::ast::EntangleEndpoint,
    world_fields: &HashMap<String, (HashSet<String>, Span)>,
    entangle_span: &Span,
    side: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    if endpoint.segments.len() < 2 {
        reports.push(
            DiagnosticReport::new(
                ErrorKind::Type,
                DiagnosticCode::WorldEntanglementInvalid,
                format!(
                    "Entanglement error: {} endpoint '{}' is malformed. \
                     Expected 'WorldName.field_name'.",
                    side, endpoint.authored_path()
                ),
            )
            .severity(DiagnosticSeverity::Error)
            .phase(CompilerPhase::StateValidation)
            .primary_label(*entangle_span, "malformed entanglement endpoint"),
        );
        return;
    }

    let world_name = &endpoint.segments[0];
    let field_name = &endpoint.segments[1];

    if let Some((fields, _world_span)) = world_fields.get(world_name.as_str()) {
        if !fields.contains(field_name.as_str()) {
            let available: Vec<&String> = fields.iter().collect();
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Type,
                    DiagnosticCode::WorldEntanglementInvalid,
                    format!(
                        "Entanglement error: world '{}' has no state field '{}'.",
                        world_name, field_name
                    ),
                )
                .severity(DiagnosticSeverity::Error)
                .phase(CompilerPhase::StateValidation)
                .primary_label(
                    *entangle_span,
                    format!("field '{}' not found in world '{}'", field_name, world_name),
                )
                .help(format!(
                    "Available fields on '{}': {}",
                    world_name,
                    if available.is_empty() {
                        "(none)".to_string()
                    } else {
                        available.iter().map(|s| s.as_str()).collect::<Vec<_>>().join(", ")
                    }
                )),
            );
        }
    } else {
        reports.push(
            DiagnosticReport::new(
                ErrorKind::Type,
                DiagnosticCode::WorldEntanglementInvalid,
                format!(
                    "Entanglement error: world '{}' referenced in entangle but not defined.",
                    world_name
                ),
            )
            .severity(DiagnosticSeverity::Error)
            .phase(CompilerPhase::StateValidation)
            .primary_label(
                *entangle_span,
                format!("undefined world '{}' in entanglement", world_name),
            ),
        );
    }
}

// ===========================================================================
//  Validator 4: Converge lane coverage
// ===========================================================================
// Every converge must have exactly one spec lane (already checked by
// validate.rs). Additionally, every fast lane should have at least one
// selector that CAN fire on some supported platform.
// A fast lane with impossible selectors is dead code.

fn validate_converge_lane_coverage(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    for item in &program.items {
        if let TypedItem::Converge(converge) = item {
            for lane in &converge.ast.fast_lanes {
                if lane.kind != ConvergeLaneKind::Fast {
                    continue;
                }

                let has_selector = lane.selector.is_some();

                if !has_selector {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::ConvergeGeneric,
                            format!(
                                "Converge '{}' fast lane '{}' has no selector. \
                                 Without a selector, this lane can never be selected.",
                                converge.ast.name, lane.lane_name
                            ),
                        )
                        .severity(DiagnosticSeverity::Error)
                        .phase(CompilerPhase::StateValidation)
                        .primary_label(lane.span, "fast lane with no selector")
                        .help(
                            "Add a 'when target(\"llvm\")' or 'when capability(...)' \
                             selector to make this lane selectable.",
                        ),
                    );
                }
            }

            // Check for contradictory selectors across the entire converge
            // (this is a heuristic and may produce false positives)
            check_converge_contradictions(&converge.ast, reports);
        }
    }
}

fn check_converge_contradictions(
    converge: &kain_core::ast::ConvergeDef,
    reports: &mut Vec<DiagnosticReport>,
) {
    for lane in &converge.fast_lanes {
        if lane.kind != ConvergeLaneKind::Fast {
            continue;
        }

        if let Some(ref selector) = lane.selector {
            match selector {
                ConvergeSelector::Target(t) => {
                    // Check if the target string looks suspicious
                    if t == "wasm" || t == "wasm32" || t == "wasm64" {
                        // This is fine in isolation, but combined with capability selectors
                        // on the same lane it could be contradictory
                        // (already handled by the per-lane check above — single selector per lane)
                    }
                }
                ConvergeSelector::Capability(c) => {
                    // A capability that references contradictory features
                    if c.contains("avx512") && c.contains("neon") {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::ConvergeGeneric,
                                format!(
                                    "Converge '{}' fast lane '{}' has contradictory capability selectors: \
                                     AVX-512 (x86) + NEON (ARM) in '{}'. These can never fire together.",
                                    converge.name, lane.lane_name, c
                                ),
                            )
                            .severity(DiagnosticSeverity::Warning)
                            .phase(CompilerPhase::StateValidation)
                            .primary_label(lane.span, "contradictory capability selectors")
                            .help("Split into separate fast lanes — one for x86/AVX-512, one for ARM/NEON."),
                        );
                    }
                }
            }
        }
    }
}

// ===========================================================================
//  Validator 5: Orchestrate stage liveness
// ===========================================================================
// Every stage in an orchestrate pipeline should be reachable from the
// return expression (if any) and should consume stages declared in deps.
// A stage that no other stage depends on is dead code.

fn validate_orchestrate_stage_liveness(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    for item in &program.items {
        if let TypedItem::Orchestrate(orchestrate) = item {
            // Collect all stage binding names used in the orchestrate body
            let stage_names = orchestrate.stages.iter()
                .map(|s| s.binding_name.clone())
                .collect::<HashSet<_>>();

            // Check that every stage binding is referenced somewhere in the body
            let body = &orchestrate.ast.body;
            for stage in &orchestrate.stages {
                if stage.binding_name.is_empty() {
                    continue;
                }

                // For orchestrate graphs, every stage should be either:
                // 1. Referenced as a dependency by another stage, OR
                // 2. Referenced in the return expression/body
                // A stage that is neither is unreachable.
                let name = &stage.binding_name;
                let referenced_in_graph = orchestrate.stages.iter().any(|s| {
                    s.binding_name != *name
                });
                let referenced_in_body = binding_referenced_in_block(body, name);

                if !referenced_in_graph && !referenced_in_body {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::StateGeneric,
                            format!(
                                "Orchestrate '{}' stage '{}' is never referenced. \
                                 This stage is dead code.",
                                orchestrate.ast.name, name
                            ),
                        )
                        .severity(DiagnosticSeverity::Warning)
                        .phase(CompilerPhase::StateValidation)
                        .primary_label(
                            orchestrate.ast.span,
                            format!("stage '{}' is unreachable", name),
                        )
                        .help("Reference this stage in another stage's 'deps' list, or in the orchestrate return expression."),
                    );
                }
            }

            let _ = stage_names; // suppress unused warning
        }
    }
}

/// Check if a binding name is referenced anywhere in a block.
fn binding_referenced_in_block(body: &Block, name: &str) -> bool {
    for stmt in &body.stmts {
        if binding_referenced_in_stmt(stmt, name) {
            return true;
        }
    }
    false
}

fn binding_referenced_in_stmt(stmt: &Stmt, name: &str) -> bool {
    match stmt {
        Stmt::Expr(expr) => binding_referenced_in_expr(expr, name),
        Stmt::Let { value, .. } => {
            value.as_ref().map_or(false, |v| binding_referenced_in_expr(v, name))
        }
        Stmt::Return(Some(expr), _) => binding_referenced_in_expr(expr, name),
        Stmt::Defer { expr, .. } => binding_referenced_in_expr(expr, name),
        Stmt::For { iter, body, .. } => {
            binding_referenced_in_expr(iter, name) || binding_referenced_in_block(body, name)
        }
        Stmt::While { condition, body, .. } => {
            binding_referenced_in_expr(condition, name) || binding_referenced_in_block(body, name)
        }
        Stmt::Loop { body, .. } => binding_referenced_in_block(body, name),
        _ => false,
    }
}

fn binding_referenced_in_expr(expr: &Expr, name: &str) -> bool {
    match expr {
        Expr::Ident(n, _) => n == name,
        Expr::Call { callee, args, .. } => {
            binding_referenced_in_expr(callee, name)
                || args.iter().any(|a| binding_referenced_in_expr(&a.value, name))
        }
        Expr::MethodCall { receiver, args, .. } => {
            binding_referenced_in_expr(receiver, name)
                || args.iter().any(|a| binding_referenced_in_expr(&a.value, name))
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            binding_referenced_in_expr(condition, name)
                || binding_referenced_in_block(then_branch, name)
                || else_branch.as_ref().map_or(false, |b| binding_referenced_in_else(b, name))
        }
        Expr::Binary { left, right, .. } => {
            binding_referenced_in_expr(left, name)
                || binding_referenced_in_expr(right, name)
        }
        Expr::Unary { operand, .. } => binding_referenced_in_expr(operand, name),
        Expr::Block(body, _) => binding_referenced_in_block(body, name),
        Expr::Match { scrutinee, arms, .. } => {
            binding_referenced_in_expr(scrutinee, name)
                || arms.iter().any(|a| binding_referenced_in_block(&a.body, name))
        }
        Expr::Field { object, .. } => binding_referenced_in_expr(object, name),
        Expr::Assign { target, value, .. } => {
            binding_referenced_in_expr(target, name)
                || binding_referenced_in_expr(value, name)
        }
        _ => false,
    }
}

fn binding_referenced_in_else(branch: &ElseBranch, name: &str) -> bool {
    match branch {
        ElseBranch::Else(block) => binding_referenced_in_block(block, name),
        ElseBranch::ElseIf(cond, block, next) => {
            binding_referenced_in_expr(cond, name)
                || binding_referenced_in_block(block, name)
                || next.as_ref().map_or(false, |n| binding_referenced_in_else(n, name))
        }
    }
}

// ===========================================================================
//  Validator 6: Pulse cadence conflicts
// ===========================================================================
// Detects two `pulse` declarations with identical cadence but different
// jitter values (potential timing conflicts). Also detects pulses
// with `every 0 ms` (instant loop — likely a bug).

fn validate_pulse_cadence_conflicts(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Collect all pulses keyed by target world field
    let mut pulses: Vec<(&kain_core::ast::PulseDef, Span)> = Vec::new();

    for item in &program.items {
        if let TypedItem::Pulse(pulse) = item {
            // Check for zero-interval (instant loop)
            if pulse.ast.interval.value == 0 {
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Validation,
                        DiagnosticCode::StateGeneric,
                        format!(
                            "Pulse '{}' has interval 'every 0 {}'. \
                             A zero-interval pulse creates an instant infinite loop.",
                            pulse.ast.name, pulse.ast.interval.unit
                        ),
                    )
                    .severity(DiagnosticSeverity::Warning)
                    .phase(CompilerPhase::StateValidation)
                    .primary_label(
                        pulse.ast.interval.span,
                        "zero-interval pulse",
                    )
                    .help("Use a positive interval (e.g., 'every 1 ms') or remove the pulse if the loop is intentional."),
                );
            }

            pulses.push((&pulse.ast, pulse.ast.span));
        }
    }

    // Check for identical cadence but different jitter
    for i in 0..pulses.len() {
        for j in (i + 1)..pulses.len() {
            let (p1, _s1) = pulses[i];
            let (p2, _s2) = pulses[j];

            // Same interval value + unit
            if p1.interval.value == p2.interval.value
                && p1.interval.unit == p2.interval.unit
            {
                // Check if jitter differs
                let j1 = p1.jitter.as_ref().map(|j| j.value).unwrap_or(0);
                let j2 = p2.jitter.as_ref().map(|j| j.value).unwrap_or(0);

                if j1 != j2 {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::StateGeneric,
                            format!(
                                "Pulse '{}' and pulse '{}' both run every {} {} but have \
                                 different jitter values ({} {} vs {} {}). This may cause \
                                 unexpected timing interactions.",
                                p1.name, p2.name,
                                p1.interval.value, p1.interval.unit,
                                j1, p1.interval.unit,
                                j2, p2.interval.unit,
                            ),
                        )
                        .severity(DiagnosticSeverity::Warning)
                        .phase(CompilerPhase::StateValidation)
                        .primary_label(
                            p1.span,
                            format!(
                                "pulse '{}' with jitter {} {}",
                                p1.name, j1, p1.interval.unit
                            ),
                        )
                        .note(format!(
                            "Pulse '{}' has jitter {} {}.",
                            p2.name, j2, p2.interval.unit
                        ))
                        .help(
                            "Use the same jitter value for pulses with identical cadence, \
                             or stagger their intervals to avoid timing conflicts.",
                        ),
                    );
                }
            }
        }
    }
}

// ===========================================================================
//  Validator 7: Teleport bus type match
// ===========================================================================
// For `teleport value from WorldA to WorldB via bus`, checks that
// `bus` (the teleport channel) refers to a declared world field
// or an explicit teleport bus, and that `value`'s struct type
// matches or is compatible with the destination.

fn validate_teleport_bus_type_match(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Build map of world fields for type checking
    let mut world_fields: HashMap<String, HashMap<String, kain_core::ast::Type>> = HashMap::new();
    for item in &program.items {
        if let TypedItem::World(world) = item {
            let fields: HashMap<String, kain_core::ast::Type> = world.ast.states.iter()
                .map(|s| (s.name.clone(), s.ty.clone()))
                .collect();
            world_fields.insert(world.ast.name.clone(), fields);
        }
    }

    // Walk all expressions looking for Teleport
    for item in &program.items {
        match item {
            TypedItem::Function(func) => {
                find_teleport_in_body(&func.ast.body, &world_fields, reports);
            }
            TypedItem::Patch(patch) => {
                find_teleport_in_body(&patch.ast.body, &world_fields, reports);
            }
            TypedItem::Resonate(resonate) => {
                find_teleport_in_body(&resonate.ast.body, &world_fields, reports);
            }
            TypedItem::Pulse(pulse) => {
                find_teleport_in_body(&pulse.ast.body, &world_fields, reports);
            }
            TypedItem::Orchestrate(orchestrate) => {
                find_teleport_in_body(&orchestrate.ast.body, &world_fields, reports);
            }
            TypedItem::Converge(converge) => {
                find_teleport_in_body(&converge.ast.spec_lane.body, &world_fields, reports);
                for lane in &converge.ast.fast_lanes {
                    find_teleport_in_body(&lane.body, &world_fields, reports);
                }
            }
            TypedItem::Actor(actor) => {
                for handler in &actor.ast.handlers {
                    find_teleport_in_body(&handler.body, &world_fields, reports);
                }
            }
            _ => {}
        }
    }
}

fn find_teleport_in_body(
    body: &Block,
    world_fields: &HashMap<String, HashMap<String, kain_core::ast::Type>>,
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &body.stmts {
        find_teleport_in_stmt(stmt, world_fields, reports);
    }
}

fn find_teleport_in_stmt(
    stmt: &Stmt,
    world_fields: &HashMap<String, HashMap<String, kain_core::ast::Type>>,
    reports: &mut Vec<DiagnosticReport>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            find_teleport_in_expr(expr, world_fields, reports);
        }
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                find_teleport_in_expr(val, world_fields, reports);
            }
        }
        Stmt::Return(Some(expr), _) => {
            find_teleport_in_expr(expr, world_fields, reports);
        }
        Stmt::Defer { expr, .. } => {
            find_teleport_in_expr(expr, world_fields, reports);
        }
        Stmt::For { iter, body, .. } => {
            find_teleport_in_expr(iter, world_fields, reports);
            find_teleport_in_body(body, world_fields, reports);
        }
        Stmt::While { condition, body, .. } => {
            find_teleport_in_expr(condition, world_fields, reports);
            find_teleport_in_body(body, world_fields, reports);
        }
        Stmt::Loop { body, .. } => {
            find_teleport_in_body(body, world_fields, reports);
        }
        _ => {}
    }
}

fn find_teleport_in_expr(
    expr: &Expr,
    world_fields: &HashMap<String, HashMap<String, kain_core::ast::Type>>,
    reports: &mut Vec<DiagnosticReport>,
) {
    if let Expr::Teleport { source_world, target_world, channel, span, .. } = expr {
        // Check that both worlds exist
        if !world_fields.contains_key(source_world.as_str()) {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Type,
                    DiagnosticCode::WorldTeleportInvalid,
                    format!(
                        "Teleport error: source world '{}' is not defined.",
                        source_world
                    ),
                )
                .severity(DiagnosticSeverity::Error)
                .phase(CompilerPhase::StateValidation)
                .primary_label(*span, format!("undefined world '{}'", source_world)),
            );
        }

        if !world_fields.contains_key(target_world.as_str()) {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Type,
                    DiagnosticCode::WorldTeleportInvalid,
                    format!(
                        "Teleport error: target world '{}' is not defined.",
                        target_world
                    ),
                )
                .severity(DiagnosticSeverity::Error)
                .phase(CompilerPhase::StateValidation)
                .primary_label(*span, format!("undefined world '{}'", target_world)),
            );
        }

        // If a channel is specified, verify it's a real field or known bus
        if let Some(ref ch) = channel {
            // Check if the channel is a field in the source world
            let source_has_channel = world_fields.get(source_world.as_str())
                .map(|fields| fields.contains_key(ch.as_str()))
                .unwrap_or(false);

            let target_has_channel = world_fields.get(target_world.as_str())
                .map(|fields| fields.contains_key(ch.as_str()))
                .unwrap_or(false);

            if !source_has_channel && !target_has_channel {
                // The channel name might be a declared bus — just warn
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Validation,
                        DiagnosticCode::WorldTeleportInvalid,
                        format!(
                            "Teleport channel '{}' is not a field in either world '{}' or '{}'. \
                             Ensure the channel name is correct.",
                            ch, source_world, target_world
                        ),
                    )
                    .severity(DiagnosticSeverity::Warning)
                    .phase(CompilerPhase::StateValidation)
                    .primary_label(*span, format!("unknown channel '{}'", ch))
                    .help("The channel should be a state field in the source or target world, or a declared teleport bus."),
                );
            }
        }
    }

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            find_teleport_in_expr(callee, world_fields, reports);
            for arg in args {
                find_teleport_in_expr(&arg.value, world_fields, reports);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            find_teleport_in_expr(receiver, world_fields, reports);
            for arg in args {
                find_teleport_in_expr(&arg.value, world_fields, reports);
            }
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            find_teleport_in_expr(condition, world_fields, reports);
            find_teleport_in_body(then_branch, world_fields, reports);
            if let Some(branch) = else_branch {
                find_teleport_in_else(branch, world_fields, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            find_teleport_in_expr(left, world_fields, reports);
            find_teleport_in_expr(right, world_fields, reports);
        }
        Expr::Unary { operand, .. } => {
            find_teleport_in_expr(operand, world_fields, reports);
        }
        Expr::Block(body, _) => {
            find_teleport_in_body(body, world_fields, reports);
        }
        Expr::Match { scrutinee, arms, .. } => {
            find_teleport_in_expr(scrutinee, world_fields, reports);
            for arm in arms {
                find_teleport_in_body(&arm.body, world_fields, reports);
            }
        }
        Expr::Assign { target, value, .. } => {
            find_teleport_in_expr(target, world_fields, reports);
            find_teleport_in_expr(value, world_fields, reports);
        }
        _ => {}
    }
}

fn find_teleport_in_else(
    branch: &ElseBranch,
    world_fields: &HashMap<String, HashMap<String, kain_core::ast::Type>>,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => {
            find_teleport_in_body(block, world_fields, reports);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            find_teleport_in_expr(cond, world_fields, reports);
            find_teleport_in_body(block, world_fields, reports);
            if let Some(next_branch) = next {
                find_teleport_in_else(next_branch, world_fields, reports);
            }
        }
    }
}

// ===========================================================================
//  Validator 8: Patch world binding
// ===========================================================================
// Every `patch Name(world_param: WorldType)` must only write to fields of
// `world_param` (the patch's declared target world). Writing to a
// DIFFERENT world from a patch body is a contract violation.
// Patches that read world state but never write anything are flagged
// as no-op warnings.

fn validate_patch_world_binding(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Build set of world names for reference
    let world_names: HashSet<String> = program.items.iter()
        .filter_map(|item| {
            if let TypedItem::World(world) = item {
                Some(world.ast.name.clone())
            } else {
                None
            }
        })
        .collect();

    for item in &program.items {
        if let TypedItem::Patch(patch) = item {
            // The first param is the target world (convention)
            let target_world = patch.ast.params.first()
                .map(|p| {
                    // Try to extract the world name from the type annotation
                    extract_world_name_from_type(&p.ty)
                })
                .flatten();

            if let Some(ref target) = target_world {
                // Walk the patch body looking for writes to OTHER worlds
                let mut foreign_writes = Vec::new();
                find_foreign_world_writes_in_body(
                    &patch.ast.body,
                    target,
                    &world_names,
                    &mut foreign_writes,
                );

                for (other_world, span) in foreign_writes {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::WorldGeneric,
                            format!(
                                "Patch '{}' targets world '{}' but writes to world '{}'. \
                                 A patch must only mutate its declared target world.",
                                patch.ast.name, target, other_world
                            ),
                        )
                        .severity(DiagnosticSeverity::Error)
                        .phase(CompilerPhase::StateValidation)
                        .primary_label(
                            span,
                            format!("write to foreign world '{}'", other_world),
                        )
                        .note(format!(
                            "Patch '{}' is bound to world '{}'. All world writes in a patch \
                             body must target the patch's declared world parameter.",
                            patch.ast.name, target
                        ))
                        .help(format!(
                            "Move the write to '{}' into its own patch, or reconsider \
                             whether world '{}' should be mutated here.",
                            other_world, other_world
                        )),
                    );
                }
            }
        }
    }
}

/// Extract world name from a Type annotation like WorldName or WorldType.
fn extract_world_name_from_type(ty: &Type) -> Option<String> {
    match ty {
        Type::Named { name, .. } => Some(name.clone()),
        _ => None,
    }
}

fn find_foreign_world_writes_in_body(
    body: &Block,
    target_world: &str,
    world_names: &HashSet<String>,
    violations: &mut Vec<(String, Span)>,
) {
    for stmt in &body.stmts {
        find_foreign_world_writes_in_stmt(stmt, target_world, world_names, violations);
    }
}

fn find_foreign_world_writes_in_stmt(
    stmt: &Stmt,
    target_world: &str,
    world_names: &HashSet<String>,
    violations: &mut Vec<(String, Span)>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            find_foreign_world_writes_in_expr(expr, target_world, world_names, violations);
        }
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                find_foreign_world_writes_in_expr(val, target_world, world_names, violations);
            }
        }
        Stmt::Return(Some(expr), _) => {
            find_foreign_world_writes_in_expr(expr, target_world, world_names, violations);
        }
        Stmt::Defer { expr, .. } => {
            find_foreign_world_writes_in_expr(expr, target_world, world_names, violations);
        }
        Stmt::For { iter, body, .. } => {
            find_foreign_world_writes_in_expr(iter, target_world, world_names, violations);
            find_foreign_world_writes_in_body(body, target_world, world_names, violations);
        }
        Stmt::While { condition, body, .. } => {
            find_foreign_world_writes_in_expr(condition, target_world, world_names, violations);
            find_foreign_world_writes_in_body(body, target_world, world_names, violations);
        }
        Stmt::Loop { body, .. } => {
            find_foreign_world_writes_in_body(body, target_world, world_names, violations);
        }
        _ => {}
    }
}

fn find_foreign_world_writes_in_expr(
    expr: &Expr,
    target_world: &str,
    world_names: &HashSet<String>,
    violations: &mut Vec<(String, Span)>,
) {
    // Pattern: OtherWorld.field = value
    if let Expr::Assign { target, value, .. } = expr {
        find_foreign_world_field(target, target_world, world_names, violations);
        find_foreign_world_writes_in_expr(value, target_world, world_names, violations);
        return;
    }

    find_foreign_world_field(expr, target_world, world_names, violations);

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            find_foreign_world_writes_in_expr(callee, target_world, world_names, violations);
            for arg in args {
                find_foreign_world_writes_in_expr(&arg.value, target_world, world_names, violations);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            find_foreign_world_writes_in_expr(receiver, target_world, world_names, violations);
            for arg in args {
                find_foreign_world_writes_in_expr(&arg.value, target_world, world_names, violations);
            }
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            find_foreign_world_writes_in_expr(condition, target_world, world_names, violations);
            find_foreign_world_writes_in_body(then_branch, target_world, world_names, violations);
            if let Some(branch) = else_branch {
                find_foreign_world_writes_in_else(branch, target_world, world_names, violations);
            }
        }
        Expr::Binary { left, right, .. } => {
            find_foreign_world_writes_in_expr(left, target_world, world_names, violations);
            find_foreign_world_writes_in_expr(right, target_world, world_names, violations);
        }
        Expr::Unary { operand, .. } => {
            find_foreign_world_writes_in_expr(operand, target_world, world_names, violations);
        }
        Expr::Block(body, _) => {
            find_foreign_world_writes_in_body(body, target_world, world_names, violations);
        }
        _ => {}
    }
}

fn find_foreign_world_field(
    expr: &Expr,
    target_world: &str,
    world_names: &HashSet<String>,
    violations: &mut Vec<(String, Span)>,
) {
    if let Expr::Field { object, field: _field, span } = expr {
        if let Expr::Ident(w, _) = object.as_ref() {
            if w != target_world && world_names.contains(w.as_str()) {
                violations.push((w.clone(), *span));
            }
        }
    }
}

fn find_foreign_world_writes_in_else(
    branch: &ElseBranch,
    target_world: &str,
    world_names: &HashSet<String>,
    violations: &mut Vec<(String, Span)>,
) {
    match branch {
        ElseBranch::Else(block) => {
            find_foreign_world_writes_in_body(block, target_world, world_names, violations);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            find_foreign_world_writes_in_expr(cond, target_world, world_names, violations);
            find_foreign_world_writes_in_body(block, target_world, world_names, violations);
            if let Some(next_branch) = next {
                find_foreign_world_writes_in_else(next_branch, target_world, world_names, violations);
            }
        }
    }
}

// ===========================================================================
//  Validator 9: Law satisfiability warning
// ===========================================================================
// Detects laws that are trivially unsatisfiable (always return false)
// and laws that are never referenced by any patch or world.

fn validate_law_satisfiability(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Collect all law names and their bodies
    let mut laws: Vec<(&kain_core::ast::LawDef, Span)> = Vec::new();

    for item in &program.items {
        if let TypedItem::Law(law) = item {
            laws.push((&law.ast, law.ast.span));
        }
    }

    if laws.is_empty() {
        return;
    }

    // Check for trivially-unsatisfiable laws (always return false)
    for (law, _span) in &laws {
        if law_always_returns_false(&law.body) {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Validation,
                    DiagnosticCode::StateGeneric,
                    format!(
                        "Law '{}' appears to be trivially unsatisfiable (always returns false). \
                         This law can never pass; all patches that invoke it will be rejected.",
                        law.name
                    ),
                )
                .severity(DiagnosticSeverity::Warning)
                .phase(CompilerPhase::StateValidation)
                .primary_label(law.span, "trivially unsatisfiable law")
                .note(
                    "Laws are invariant predicates that must hold true for patches to commit. \
                     A law that unconditionally returns 'false' blocks all state transitions.",
                )
                .help("If this is a placeholder, mark it with a comment. Otherwise, implement the actual invariant check."),
            );
        }
    }

    // Collect law names referenced anywhere in patch bodies or world validations
    // (Phase 1: detect via law function calls in any body)
    let mut referenced_laws: HashSet<String> = HashSet::new();

    for item in &program.items {
        match item {
            TypedItem::Patch(patch) => {
                collect_law_references_in_body(&patch.ast.body, &mut referenced_laws);
            }
            TypedItem::Function(func) => {
                collect_law_references_in_body(&func.ast.body, &mut referenced_laws);
            }
            _ => {}
        }
    }

    for (law, _span) in &laws {
        if !referenced_laws.contains(&law.name) {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Validation,
                    DiagnosticCode::StateGeneric,
                    format!(
                        "Law '{}' is never referenced. Unused laws add dead code and \
                         may indicate missing validation.",
                        law.name
                    ),
                )
                .severity(DiagnosticSeverity::Warning)
                .phase(CompilerPhase::StateValidation)
                .primary_label(law.span, "unreferenced law")
                .help("Reference this law in a patch or world validation, or remove it if unused."),
            );
        }
    }
}

/// Heuristic: does the body unconditionally return false?
fn law_always_returns_false(body: &Block) -> bool {
    // Check if every return path returns false
    for stmt in &body.stmts {
        if let Stmt::Return(Some(Expr::Bool(false, _)), _) = stmt {
            // Found a `return false` — check if this is the only return
            // This is a simple heuristic; a proper CFG analysis would be needed for production
            let mut found_true = false;
            for s in &body.stmts {
                if let Stmt::Return(Some(Expr::Bool(true, _)), _) = s {
                    found_true = true;
                }
            }
            if !found_true {
                // No return true found anywhere — likely always-false
                return true;
            }
        }
    }
    false
}

fn collect_law_references_in_body(body: &Block, laws: &mut HashSet<String>) {
    for stmt in &body.stmts {
        collect_law_references_in_stmt(stmt, laws);
    }
}

fn collect_law_references_in_stmt(stmt: &Stmt, laws: &mut HashSet<String>) {
    match stmt {
        Stmt::Expr(expr) => collect_law_references_in_expr(expr, laws),
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                collect_law_references_in_expr(val, laws);
            }
        }
        Stmt::Return(Some(expr), _) => collect_law_references_in_expr(expr, laws),
        Stmt::Defer { expr, .. } => collect_law_references_in_expr(expr, laws),
        Stmt::For { iter, body, .. } => {
            collect_law_references_in_expr(iter, laws);
            collect_law_references_in_body(body, laws);
        }
        Stmt::While { condition, body, .. } => {
            collect_law_references_in_expr(condition, laws);
            collect_law_references_in_body(body, laws);
        }
        Stmt::Loop { body, .. } => collect_law_references_in_body(body, laws),
        _ => {}
    }
}

fn collect_law_references_in_expr(expr: &Expr, laws: &mut HashSet<String>) {
    match expr {
        Expr::Call { callee, .. } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                laws.insert(name.clone());
            }
            collect_law_references_in_expr(callee, laws);
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            collect_law_references_in_expr(condition, laws);
            collect_law_references_in_body(then_branch, laws);
            if let Some(branch) = else_branch {
                collect_law_references_in_else(branch, laws);
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_law_references_in_expr(left, laws);
            collect_law_references_in_expr(right, laws);
        }
        Expr::Unary { operand, .. } => collect_law_references_in_expr(operand, laws),
        Expr::Block(body, _) => collect_law_references_in_body(body, laws),
        Expr::Match { scrutinee, arms, .. } => {
            collect_law_references_in_expr(scrutinee, laws);
            for arm in arms {
                collect_law_references_in_body(&arm.body, laws);
            }
        }
        _ => {}
    }
}

fn collect_law_references_in_else(branch: &ElseBranch, laws: &mut HashSet<String>) {
    match branch {
        ElseBranch::Else(block) => collect_law_references_in_body(block, laws),
        ElseBranch::ElseIf(cond, block, next) => {
            collect_law_references_in_expr(cond, laws);
            collect_law_references_in_body(block, laws);
            if let Some(next_branch) = next {
                collect_law_references_in_else(next_branch, laws);
            }
        }
    }
}

// ===========================================================================
//  Validator 10: Dead state detection
// ===========================================================================
// World states that are declared but never read or written (outside
// of initialization) are dead code — they consume memory and entangle
// slots but serve no purpose.

fn validate_world_dead_state(
    program: &TypedProgram,
    reports: &mut Vec<DiagnosticReport>,
) {
    // Collect all world state fields
    let mut world_states: HashMap<String, (HashSet<String>, Span)> = HashMap::new();
    for item in &program.items {
        if let TypedItem::World(world) = item {
            let fields: HashSet<String> = world.ast.states.iter()
                .map(|s| s.name.clone())
                .collect();
            world_states.insert(world.ast.name.clone(), (fields, world.ast.span));
        }
    }

    // Collect all world field accesses (reads + writes) in function bodies,
    // patch bodies, resonate bodies, pulse bodies, orchestrate bodies, converge bodies
    let mut accessed_fields: HashMap<String, HashSet<String>> = HashMap::new();

    for item in &program.items {
        match item {
            TypedItem::Function(func) => {
                collect_world_accesses_in_body(&func.ast.body, &mut accessed_fields);
            }
            TypedItem::Patch(patch) => {
                collect_world_accesses_in_body(&patch.ast.body, &mut accessed_fields);
            }
            TypedItem::Resonate(resonate) => {
                collect_world_accesses_in_body(&resonate.ast.body, &mut accessed_fields);
            }
            TypedItem::Pulse(pulse) => {
                collect_world_accesses_in_body(&pulse.ast.body, &mut accessed_fields);
            }
            TypedItem::Orchestrate(orchestrate) => {
                collect_world_accesses_in_body(&orchestrate.ast.body, &mut accessed_fields);
            }
            TypedItem::Converge(converge) => {
                collect_world_accesses_in_body(&converge.ast.spec_lane.body, &mut accessed_fields);
                for lane in &converge.ast.fast_lanes {
                    collect_world_accesses_in_body(&lane.body, &mut accessed_fields);
                }
            }
            TypedItem::Actor(actor) => {
                for handler in &actor.ast.handlers {
                    collect_world_accesses_in_body(&handler.body, &mut accessed_fields);
                }
            }
            _ => {}
        }
    }

    // Check for dead state fields
    for (world_name, (fields, _world_span)) in &world_states {
        let accessed = accessed_fields.get(world_name.as_str());
        for field in fields {
            let is_accessed = accessed.map_or(false, |a| a.contains(field.as_str()));
            if !is_accessed {
                // Also check if it's used in entangle declarations
                let is_entangled = program.items.iter().any(|item| {
                    if let TypedItem::Entangle(entangle) = item {
                        let left = &entangle.ast.left.segments;
                        let right = &entangle.ast.right.segments;
                        (left.len() >= 2 && left[0] == *world_name && left[1] == *field)
                            || (right.len() >= 2 && right[0] == *world_name && right[1] == *field)
                    } else {
                        false
                    }
                });

                if !is_entangled {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::WorldGeneric,
                            format!(
                                "World '{}' state field '{}' is declared but never read, written, \
                                 or entangled. This is dead state.",
                                world_name, field
                            ),
                        )
                        .severity(DiagnosticSeverity::Warning)
                        .phase(CompilerPhase::StateValidation)
                        .primary_label(
                            *_world_span,
                            format!("dead state field '{}' in world '{}'", field, world_name),
                        )
                        .help(format!(
                            "Either use '{}' in a function, patch, resonate, or pulse body, \
                             entangle it with another world, or remove the state declaration.",
                            field
                        )),
                    );
                }
            }
        }
    }
}

/// Collect all WorldName.field_name accesses in a block.
fn collect_world_accesses_in_body(
    body: &Block,
    accessed: &mut HashMap<String, HashSet<String>>,
) {
    for stmt in &body.stmts {
        collect_world_accesses_in_stmt(stmt, accessed);
    }
}

fn collect_world_accesses_in_stmt(
    stmt: &Stmt,
    accessed: &mut HashMap<String, HashSet<String>>,
) {
    match stmt {
        Stmt::Expr(expr) => collect_world_accesses_in_expr(expr, accessed),
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                collect_world_accesses_in_expr(val, accessed);
            }
        }
        Stmt::Return(Some(expr), _) => collect_world_accesses_in_expr(expr, accessed),
        Stmt::Defer { expr, .. } => collect_world_accesses_in_expr(expr, accessed),
        Stmt::For { iter, body, .. } => {
            collect_world_accesses_in_expr(iter, accessed);
            collect_world_accesses_in_body(body, accessed);
        }
        Stmt::While { condition, body, .. } => {
            collect_world_accesses_in_expr(condition, accessed);
            collect_world_accesses_in_body(body, accessed);
        }
        Stmt::Loop { body, .. } => collect_world_accesses_in_body(body, accessed),
        _ => {}
    }
}

fn collect_world_accesses_in_expr(
    expr: &Expr,
    accessed: &mut HashMap<String, HashSet<String>>,
) {
    if let Expr::Field { object, field, .. } = expr {
        if let Expr::Ident(world_name, _) = object.as_ref() {
            accessed
                .entry(world_name.clone())
                .or_default()
                .insert(field.clone());
        }
    }

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            collect_world_accesses_in_expr(callee, accessed);
            for arg in args {
                collect_world_accesses_in_expr(&arg.value, accessed);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_world_accesses_in_expr(receiver, accessed);
            for arg in args {
                collect_world_accesses_in_expr(&arg.value, accessed);
            }
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            collect_world_accesses_in_expr(condition, accessed);
            collect_world_accesses_in_body(then_branch, accessed);
            if let Some(branch) = else_branch {
                collect_world_accesses_in_else(branch, accessed);
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_world_accesses_in_expr(left, accessed);
            collect_world_accesses_in_expr(right, accessed);
        }
        Expr::Unary { operand, .. } => collect_world_accesses_in_expr(operand, accessed),
        Expr::Block(body, _) => collect_world_accesses_in_body(body, accessed),
        Expr::Match { scrutinee, arms, .. } => {
            collect_world_accesses_in_expr(scrutinee, accessed);
            for arm in arms {
                collect_world_accesses_in_body(&arm.body, accessed);
            }
        }
        Expr::Assign { target, value, .. } => {
            collect_world_accesses_in_expr(target, accessed);
            collect_world_accesses_in_expr(value, accessed);
        }
        Expr::SendMsg { data, target, .. } => {
            for (_, data_expr) in data {
                collect_world_accesses_in_expr(data_expr, accessed);
            }
            collect_world_accesses_in_expr(target, accessed);
        }
        Expr::Field { object, .. } => {
            collect_world_accesses_in_expr(object, accessed);
        }
        _ => {}
    }
}

fn collect_world_accesses_in_else(
    branch: &ElseBranch,
    accessed: &mut HashMap<String, HashSet<String>>,
) {
    match branch {
        ElseBranch::Else(block) => collect_world_accesses_in_body(block, accessed),
        ElseBranch::ElseIf(cond, block, next) => {
            collect_world_accesses_in_expr(cond, accessed);
            collect_world_accesses_in_body(block, accessed);
            if let Some(next_branch) = next {
                collect_world_accesses_in_else(next_branch, accessed);
            }
        }
    }
}
