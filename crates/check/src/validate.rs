// ============================================================================
//  Proactive semantic validation pass for kain-check.
//
//  This module walks a typed program and checks invariants that the typechecker
//  does not enforce but the codegen/runtime requires. Each validator produces
//  Vec<DiagnosticReport> which check integrates as a failure surface.
//
//  Rule: one function per construct. validate_semantic_stack calls them all.
// ============================================================================

use kain_core::ast::{Block, ElseBranch, Expr, Stmt, Type};
use kain_core::types::{TypedActor, TypedItem, TypedProgram};
use kain_error::{
    CompilerPhase, DiagnosticCode, DiagnosticReport, DiagnosticSeverity, ErrorKind,
};
use kain_error::span::Span;
use kain_ownership::{OwnershipState, OwnershipTransition, OwnershipTransitionError};
use std::collections::{HashMap, HashSet};

/// Run all proactive semantic validators against a typed program.
pub fn validate_semantic_stack(program: &TypedProgram) -> Vec<DiagnosticReport> {
    let mut reports = Vec::new();
    validate_reply_ports(program, &mut reports);
    validate_converge_contracts(program, &mut reports);
    validate_entangle_type_match(program, &mut reports);
    validate_orchestrate_graph(program, &mut reports);
    validate_ownership_transitions(program, &mut reports);
    reports
}

// ---------------------------------------------------------------------------
//  Reply port validation
// ---------------------------------------------------------------------------
// The LLVM codegen requires that `send reply_to.X(...)` uses exactly the
// synthetic message name "Reply" and at most one payload field named "value".
// The typechecker infers the reply contract but does not enforce the message
// name constraint — it's a lowering detail. This validator catches the mismatch
// at check time so users don't hit a cryptic codegen error.

fn is_reply_port_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "P")
}

fn validate_reply_ports(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            validate_actor_reply_ports(actor, reports);
        }
    }
}

fn validate_actor_reply_ports(actor: &TypedActor, reports: &mut Vec<DiagnosticReport>) {
    for handler in &actor.ast.handlers {
        // A handler has a reply port if its first param has type P
        let has_reply_port = handler
            .params
            .first()
            .map(|p| is_reply_port_type(&p.ty))
            .unwrap_or(false);

        if !has_reply_port {
            continue;
        }

        let reply_param_name = handler
            .params
            .first()
            .map(|p| p.name.clone())
            .unwrap_or_default();

        // Walk all send expressions in the handler body looking for sends
        // TO the reply port parameter
        walk_block_for_reply_sends(
            &actor.ast.name,
            &handler.message_type,
            &reply_param_name,
            &handler.body,
            reports,
        );
    }
}

fn walk_block_for_reply_sends(
    actor_name: &str,
    handler_name: &str,
    reply_param: &str,
    block: &Block,
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &block.stmts {
        walk_stmt_for_reply_sends(actor_name, handler_name, reply_param, stmt, reports);
    }
}

fn walk_stmt_for_reply_sends(
    actor_name: &str,
    handler_name: &str,
    reply_param: &str,
    stmt: &Stmt,
    reports: &mut Vec<DiagnosticReport>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, expr, reports);
        }
        Stmt::Return(Some(expr), _) => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, expr, reports);
        }
        Stmt::Let { value: Some(expr), .. } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, expr, reports);
        }
        Stmt::Defer { expr, .. } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, expr, reports);
        }
        Stmt::For { iter, body, .. } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, iter, reports);
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, body, reports);
        }
        Stmt::While {
            condition, body, ..
        } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, condition, reports);
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, body, reports);
        }
        _ => {}
    }
}

fn walk_else_branch_for_reply_sends(
    actor_name: &str,
    handler_name: &str,
    reply_param: &str,
    branch: &ElseBranch,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => {
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, block, reports);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, cond, reports);
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, block, reports);
            if let Some(next_branch) = next {
                walk_else_branch_for_reply_sends(
                    actor_name,
                    handler_name,
                    reply_param,
                    next_branch,
                    reports,
                );
            }
        }
    }
}

fn walk_expr_for_reply_sends(
    actor_name: &str,
    handler_name: &str,
    reply_param: &str,
    expr: &Expr,
    reports: &mut Vec<DiagnosticReport>,
) {
    match expr {
        Expr::SendMsg {
            target,
            message,
            data,
            span,
        } => {
            // Check if this send targets the reply parameter
            if let Expr::Ident(name, _) = target.as_ref() {
                if name == reply_param {
                    // This is a send TO the reply port — validate the message name
                    if message != "Reply" {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Codegen,
                                DiagnosticCode::ActorGeneric,
                                format!(
                                    "Reply port handles only accept the synthetic 'Reply' message, found '{}'",
                                    message
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, format!("send to reply port uses '{}' instead of 'Reply'", message))
                            .note(format!(
                                "In actor '{}' handler '{}', the reply port parameter '{}' must use the synthetic 'Reply' message.",
                                actor_name, handler_name, reply_param
                            ))
                            .help("Change to: send reply_to.Reply(value = ...)")
                        );
                    }

                    // Validate payload field names
                    for (field_name, _) in data {
                        if field_name != "value" {
                            reports.push(
                                DiagnosticReport::new(
                                    ErrorKind::Codegen,
                                    DiagnosticCode::ActorGeneric,
                                    format!(
                                        "Reply port payload field must be named 'value', found '{}'",
                                        field_name
                                    ),
                                )
                                .severity(DiagnosticSeverity::Error)
                                .phase(CompilerPhase::TypeChecking)
                                .primary_label(*span, format!("field '{}' is not 'value'", field_name))
                                .note("Reply port messages accept exactly one payload field named 'value'.")
                                .help("Change to: send reply_to.Reply(value = your_value)")
                            );
                        }
                    }
                }
            }

            // Recurse into data expressions
            for (_, data_expr) in data {
                walk_expr_for_reply_sends(actor_name, handler_name, reply_param, data_expr, reports);
            }
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, target, reports);
        }
        Expr::Call {
            callee, args, ..
        } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, callee, reports);
            for arg in args {
                walk_expr_for_reply_sends(
                    actor_name,
                    handler_name,
                    reply_param,
                    &arg.value,
                    reports,
                );
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, condition, reports);
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, then_branch, reports);
            if let Some(branch) = else_branch {
                walk_else_branch_for_reply_sends(
                    actor_name,
                    handler_name,
                    reply_param,
                    branch,
                    reports,
                );
            }
        }
        Expr::Binary { left, right, .. } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, left, reports);
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, right, reports);
        }
        Expr::Unary { operand, .. } => {
            walk_expr_for_reply_sends(actor_name, handler_name, reply_param, operand, reports);
        }
        Expr::Block(block, _) => {
            walk_block_for_reply_sends(actor_name, handler_name, reply_param, block, reports);
        }
        _ => {}
    }
}

// ---------------------------------------------------------------------------
//  Converge contract validation
// ---------------------------------------------------------------------------
// Converge declarations must have exactly one spec lane and at least one fast
// lane. The typechecker validates type signatures match, but does not enforce
// the structural contract (spec presence, fast lane count).

fn validate_converge_contracts(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    for item in &program.items {
        if let TypedItem::Converge(converge) = item {
            // spec_lane is a required field, always present
            let has_fast = !converge.ast.fast_lanes.is_empty();

            if !has_fast {
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Type,
                        DiagnosticCode::TypeGeneric,
                        format!(
                            "Converge '{}' has no fast lanes. Add at least one 'fast' lane with a selector.",
                            converge.ast.name
                        ),
                    )
                    .severity(DiagnosticSeverity::Error)
                    .phase(CompilerPhase::TypeChecking)
                    .primary_label(converge.ast.span, "converge has no fast lanes")
                    .note("A converge with only a spec lane is just a function. Fast lanes provide platform-specific optimizations.")
                    .help("Add a 'fast lane_name when target(\"llvm\"):' lane with the optimized implementation."),
                );
            }

            if converge.ast.verify_random_count.is_none() {
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Type,
                        DiagnosticCode::TypeGeneric,
                        format!(
                            "Converge '{}' is missing a verify clause. Use 'verify random(N)' to fuzz-test fast lanes against the spec.",
                            converge.ast.name
                        ),
                    )
                    .severity(DiagnosticSeverity::Warning)
                    .phase(CompilerPhase::TypeChecking)
                    .primary_label(converge.ast.span, "converge missing verify clause")
                    .help("Add 'verify random(8)' to test fast lanes against the spec at selection time."),
                );
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Entangle type match validation
// ---------------------------------------------------------------------------
// Entangled world fields must have compatible types across the entanglement
// endpoints. The typechecker validates that the worlds and fields exist, but
// does not enforce that the types are identical or coercible. This validator
// catches mismatches that would otherwise fail at codegen or runtime.

fn validate_entangle_type_match(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    for item in &program.items {
        if let TypedItem::Entangle(entangle) = item {
            // EntangleEndpoint.segments = [world_name, field_name]
            let world_a_name = entangle.ast.left.segments.first().cloned();
            let field_a_name = entangle.ast.left.segments.get(1).cloned();
            let world_b_name = entangle.ast.right.segments.first().cloned();
            let field_b_name = entangle.ast.right.segments.get(1).cloned();

            let world_a = world_a_name.as_deref().and_then(|name| find_world(program, name));
            let world_b = world_b_name.as_deref().and_then(|name| find_world(program, name));

            if let (Some(world_a_def), Some(world_b_def)) = (world_a, world_b) {
                let ty_a = field_a_name
                    .as_deref()
                    .and_then(|field| find_world_state_field(world_a_def, field));
                let ty_b = field_b_name
                    .as_deref()
                    .and_then(|field| find_world_state_field(world_b_def, field));

                if let (Some(ty_a), Some(ty_b)) = (ty_a, ty_b) {
                    let ty_a_str = type_to_string(&ty_a);
                    let ty_b_str = type_to_string(&ty_b);
                    if ty_a_str != ty_b_str {
                        let wa = world_a_name.as_deref().unwrap_or("?");
                        let fa = field_a_name.as_deref().unwrap_or("?");
                        let wb = world_b_name.as_deref().unwrap_or("?");
                        let fb = field_b_name.as_deref().unwrap_or("?");
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Entangle,
                                DiagnosticCode::TypeGeneric,
                                format!(
                                    "Entangled fields have incompatible types: '{}.{}' is '{}' but '{}.{}' is '{}'",
                                    wa, fa, ty_a_str, wb, fb, ty_b_str,
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::StateValidation)
                            .primary_label(
                                entangle.ast.span,
                                "entangled fields must have the same type",
                            )
                            .note("Entangled fields are bidirectionally synchronized. Their types must be identical.")
                            .help(format!(
                                "Change '{}.{}' from '{}' to '{}' or '{}.{}' from '{}' to '{}'",
                                wa, fa, ty_a_str, ty_b_str, wb, fb, ty_b_str, ty_a_str,
                            )),
                        );
                    }
                }
            }
        }
    }
}

/// Find a world definition in the typed program by name.
fn find_world<'a>(program: &'a TypedProgram, name: &str) -> Option<&'a TypedItem> {
    program.items.iter().find(|item| {
        if let TypedItem::World(world) = item {
            world.ast.name == name
        } else {
            false
        }
    })
}

/// Find a state field type in a world definition.
fn find_world_state_field(item: &TypedItem, field_name: &str) -> Option<Type> {
    if let TypedItem::World(world) = item {
        world.ast.states.iter()
            .find(|state| state.name == field_name)
            .map(|state| state.ty.clone())
    } else {
        None
    }
}

/// Convert a Type to a human-readable string for error messages.
fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } if generics.is_empty() => name.clone(),
        Type::Named { name, generics, .. } => {
            let gens: Vec<String> = generics.iter().map(type_to_string).collect();
            format!("{}<{}>", name, gens.join(", "))
        }
        Type::Ptr { inner, mutable, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", type_to_string(inner))
            } else {
                format!("ptr<{}>", type_to_string(inner))
            }
        }
        Type::Ref { inner, mutable, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        Type::Array(inner, size, ..) => {
            format!("[{}; {}]", type_to_string(inner), size)
        }
        Type::Slice(inner, ..) => {
            format!("[{}]", type_to_string(inner))
        }
        Type::Tuple(types, ..) => {
            let ts: Vec<String> = types.iter().map(type_to_string).collect();
            format!("({})", ts.join(", "))
        }
        Type::Function { params, return_type, .. } => {
            let ps: Vec<String> = params.iter().map(type_to_string).collect();
            format!("fn({}) -> {}", ps.join(", "), type_to_string(return_type))
        }
        Type::Option(inner, ..) => format!("{}?", type_to_string(inner)),
        Type::Result(ok, err, ..) => {
            format!("{}!{}", type_to_string(ok), type_to_string(err))
        }
        Type::Impl { trait_name, generics, .. } if generics.is_empty() => {
            format!("impl {}", trait_name)
        }
        Type::Impl { trait_name, generics, .. } => {
            let gens: Vec<String> = generics.iter().map(type_to_string).collect();
            format!("impl {}<{}>", trait_name, gens.join(", "))
        }
        Type::Infer(..) => "_".to_string(),
        Type::Never(..) => "!".to_string(),
        Type::Unit(..) => "()".to_string(),
    }
}

// ---------------------------------------------------------------------------
//  Orchestrate graph validation
// ---------------------------------------------------------------------------
// Orchestrate declarations define a DAG of stages with dependencies.
// The typechecker validates individual stage syntax but not graph-level
// invariants: cycles, unreachable stages, or missing dependency targets.
// This validator catches those structural issues at check time.

fn validate_orchestrate_graph(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    for item in &program.items {
        if let TypedItem::Orchestrate(orchestrate) = item {
            let graph = &orchestrate.graph;
            let stage_names: HashSet<&str> = graph.stages.iter()
                .map(|s| s.binding_name.as_str())
                .collect();

            // Build adjacency list for cycle detection
            let mut deps: HashMap<&str, Vec<&str>> = HashMap::new();
            for stage in &graph.stages {
                let stage_deps: Vec<&str> = stage.metadata.dependencies.iter()
                    .map(|d| d.as_str())
                    .collect();
                deps.insert(&stage.binding_name, stage_deps);
            }

            // Check 1: All dependency targets exist
            for stage in &graph.stages {
                for dep in &stage.metadata.dependencies {
                    if !stage_names.contains(dep.as_str()) {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Type,
                                DiagnosticCode::TypeGeneric,
                                format!(
                                    "Orchestrate '{}': stage '{}' depends on unknown stage '{}'",
                                    graph.name, stage.binding_name, dep
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::StateValidation)
                            .primary_label(
                                orchestrate.ast.span,
                                format!("dependency '{}' not found in orchestrate stages", dep),
                            )
                            .help(format!(
                                "Available stages: {}",
                                stage_names.iter().cloned().collect::<Vec<_>>().join(", ")
                            )),
                        );
                    }
                }
            }

            // Check 2: Cycle detection (DFS)
            if let Some(cycle) = detect_cycle(&deps) {
                let cycle_str = cycle.iter().cloned().collect::<Vec<_>>().join(" → ");
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Type,
                        DiagnosticCode::TypeGeneric,
                        format!(
                            "Orchestrate '{}' has a cyclic stage dependency: {}",
                            graph.name, cycle_str
                        ),
                    )
                    .severity(DiagnosticSeverity::Error)
                    .phase(CompilerPhase::StateValidation)
                    .primary_label(orchestrate.ast.span, "cycle detected in stage dependencies")
                    .help("Break the cycle by removing or reversing one of the dependencies."),
                );
            }
        }
    }
}

/// Detect a cycle in a dependency graph. Returns the first cycle found.
fn detect_cycle<'a>(deps: &HashMap<&'a str, Vec<&'a str>>) -> Option<Vec<&'a str>> {
    let mut visited: HashSet<&str> = HashSet::new();
    let mut in_stack: HashSet<&str> = HashSet::new();
    let mut path: Vec<&str> = Vec::new();

    fn dfs<'a>(
        node: &'a str,
        deps: &HashMap<&'a str, Vec<&'a str>>,
        visited: &mut HashSet<&'a str>,
        in_stack: &mut HashSet<&'a str>,
        path: &mut Vec<&'a str>,
    ) -> Option<Vec<&'a str>> {
        visited.insert(node);
        in_stack.insert(node);
        path.push(node);

        if let Some(neighbors) = deps.get(node) {
            for &neighbor in neighbors {
                if !visited.contains(neighbor) {
                    if let Some(cycle) = dfs(neighbor, deps, visited, in_stack, path) {
                        return Some(cycle);
                    }
                } else if in_stack.contains(neighbor) {
                    // Found cycle – extract it from path
                    if let Some(pos) = path.iter().position(|&n| n == neighbor) {
                        let mut cycle: Vec<&str> = path[pos..].to_vec();
                        cycle.push(neighbor); // close the cycle
                        return Some(cycle);
                    }
                }
            }
        }

        path.pop();
        in_stack.remove(node);
        None
    }

    for &node in deps.keys() {
        if !visited.contains(node) {
            path.clear();
            if let Some(cycle) = dfs(node, deps, &mut visited, &mut in_stack, &mut path) {
                return Some(cycle);
            }
        }
    }

    None
}

// ---------------------------------------------------------------------------
//  Ownership transition validation
// ---------------------------------------------------------------------------
// The typechecker validates ownership constructs structurally (target type
// must be Ptr/Ref, no early exits from ownership scopes). But it does NOT
// track the full ownership state machine: Idle→Collapsed→Idle,
// Idle→Observed(n)→Idle, Idle→Decayed (terminal).
//
// This validator performs a lightweight intra-procedural walk over each
// function body, tracking per-variable ownership state transitions and
// flagging violations at check time.
//
// NOTE: This is an intra-procedural best-effort pass. Cross-function and
// cross-region analysis requires the full ownership crate integration
// (planned for a future pass). We catch the most common violations:
// double-collapse, double-decay, collapse-while-observed, etc.

fn validate_ownership_transitions(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    for item in &program.items {
        if let TypedItem::Function(func) = item {
            let mut tracker = OwnershipTracker::new();
            validate_ownership_in_body(
                &func.ast.body,
                &mut tracker,
                &func.ast.name,
                reports,
            );
        }
    }
}

/// Tracks ownership states for pointer variables within a function body.
struct OwnershipTracker {
    states: HashMap<String, OwnershipState>,
}

impl OwnershipTracker {
    fn new() -> Self {
        Self {
            states: HashMap::new(),
        }
    }

    fn apply(
        &mut self,
        ptr_name: &str,
        transition: OwnershipTransition,
    ) -> Result<(), OwnershipTransitionError> {
        let current = self
            .states
            .get(ptr_name)
            .copied()
            .unwrap_or(OwnershipState::Idle);
        let next = current.apply(transition)?;
        self.states.insert(ptr_name.to_string(), next);
        Ok(())
    }
}

fn validate_ownership_in_body(
    body: &Block,
    tracker: &mut OwnershipTracker,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &body.stmts {
        validate_ownership_in_stmt(stmt, tracker, fn_name, reports);
    }
}

fn validate_ownership_in_stmt(
    stmt: &Stmt,
    tracker: &mut OwnershipTracker,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match stmt {
        Stmt::Expr(expr) => {
            validate_ownership_in_expr(expr, tracker, fn_name, reports);
        }
        Stmt::Let { pattern, value, .. } => {
            if let Some(expr) = value {
                validate_ownership_in_expr(expr, tracker, fn_name, reports);
            }
            // Register variable from pattern name for tracking
            let name = match pattern {
                kain_core::ast::Pattern::Ident(name, _) => Some(name.as_str()),
                _ => None,
            };
            if let Some(name) = name {
                tracker
                    .states
                    .entry(name.to_string())
                    .or_insert(OwnershipState::Idle);
            }
        }
        Stmt::Return(Some(expr), _) => {
            validate_ownership_in_expr(expr, tracker, fn_name, reports);
        }
        Stmt::Defer { expr, .. } => {
            validate_ownership_in_expr(expr, tracker, fn_name, reports);
        }
        Stmt::For { iter, body, .. } => {
            validate_ownership_in_expr(iter, tracker, fn_name, reports);
            validate_ownership_in_body(body, tracker, fn_name, reports);
        }
        Stmt::While {
            condition, body, ..
        } => {
            validate_ownership_in_expr(condition, tracker, fn_name, reports);
            validate_ownership_in_body(body, tracker, fn_name, reports);
        }
        Stmt::Loop { body, .. } => {
            validate_ownership_in_body(body, tracker, fn_name, reports);
        }
        Stmt::Fanout { iter, body, .. } => {
            validate_ownership_in_expr(iter, tracker, fn_name, reports);
            validate_ownership_in_body(body, tracker, fn_name, reports);
        }
        Stmt::Dispatch { .. }
        | Stmt::Return(None, _)
        | Stmt::Break(..)
        | Stmt::Continue(_)
        | Stmt::Item(_) => {}
    }
}

fn validate_ownership_in_expr(
    expr: &Expr,
    tracker: &mut OwnershipTracker,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match expr {
        Expr::Collapse { target, body, span } => {
            if let Some(ptr_name) = extract_ptr_name(target) {
                let transition = OwnershipTransition::BeginCollapse;
                if let Err(err) = tracker.apply(&ptr_name, transition) {
                    reports.push(ownership_violation_report(
                        fn_name, &ptr_name, &err, "collapse", *span,
                    ));
                } else {
                    // Walk body expression, then end collapse
                    validate_ownership_in_expr(body, tracker, fn_name, reports);
                    let _ = tracker.apply(&ptr_name, OwnershipTransition::EndCollapse);
                }
            }
        }
        Expr::Observe { target, body, span } => {
            if let Some(ptr_name) = extract_ptr_name(target) {
                let transition = OwnershipTransition::BeginObserve;
                if let Err(err) = tracker.apply(&ptr_name, transition) {
                    reports.push(ownership_violation_report(
                        fn_name, &ptr_name, &err, "observe", *span,
                    ));
                } else {
                    validate_ownership_in_expr(body, tracker, fn_name, reports);
                    let _ = tracker.apply(&ptr_name, OwnershipTransition::EndObserve);
                }
            }
        }
        Expr::Decay { target, span } => {
            if let Some(ptr_name) = extract_ptr_name(target) {
                let transition = OwnershipTransition::Decay;
                if let Err(err) = tracker.apply(&ptr_name, transition) {
                    reports.push(ownership_violation_report(
                        fn_name, &ptr_name, &err, "decay", *span,
                    ));
                }
            }
        }
        Expr::Share { target, body, span } => {
            if let Some(ptr_name) = extract_ptr_name(target) {
                let transition = OwnershipTransition::BeginShare;
                if let Err(err) = tracker.apply(&ptr_name, transition) {
                    reports.push(ownership_violation_report(
                        fn_name, &ptr_name, &err, "share", *span,
                    ));
                } else {
                    validate_ownership_in_expr(body, tracker, fn_name, reports);
                    let _ = tracker.apply(&ptr_name, OwnershipTransition::EndShare);
                }
            }
        }
        // Recurse into complex expressions
        Expr::Call { callee, args, .. } => {
            validate_ownership_in_expr(callee, tracker, fn_name, reports);
            for arg in args {
                validate_ownership_in_expr(&arg.value, tracker, fn_name, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_ownership_in_expr(condition, tracker, fn_name, reports);
            validate_ownership_in_body(then_branch, tracker, fn_name, reports);
            if let Some(branch) = else_branch {
                validate_ownership_in_else(branch, tracker, fn_name, reports);
            }
        }
        Expr::Block(block, _) => {
            validate_ownership_in_body(block, tracker, fn_name, reports);
        }
        Expr::Binary { left, right, .. } => {
            validate_ownership_in_expr(left, tracker, fn_name, reports);
            validate_ownership_in_expr(right, tracker, fn_name, reports);
        }
        Expr::Unary { operand, .. } => {
            validate_ownership_in_expr(operand, tracker, fn_name, reports);
        }
        Expr::Return(Some(inner), _) => {
            validate_ownership_in_expr(inner, tracker, fn_name, reports);
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            validate_ownership_in_expr(scrutinee, tracker, fn_name, reports);
            for arm in arms {
                validate_ownership_in_expr(&arm.body, tracker, fn_name, reports);
            }
        }
        Expr::Assign { target, value, .. } => {
            validate_ownership_in_expr(target, tracker, fn_name, reports);
            validate_ownership_in_expr(value, tracker, fn_name, reports);
        }
        Expr::SendMsg {
            target, data, ..
        } => {
            validate_ownership_in_expr(target, tracker, fn_name, reports);
            for (_, data_expr) in data {
                validate_ownership_in_expr(data_expr, tracker, fn_name, reports);
            }
        }
        _ => {}
    }
}

fn validate_ownership_in_else(
    branch: &ElseBranch,
    tracker: &mut OwnershipTracker,
    fn_name: &str,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => {
            validate_ownership_in_body(block, tracker, fn_name, reports);
        }
        ElseBranch::ElseIf(cond, block, next) => {
            validate_ownership_in_expr(cond, tracker, fn_name, reports);
            validate_ownership_in_body(block, tracker, fn_name, reports);
            if let Some(next_branch) = next {
                validate_ownership_in_else(next_branch, tracker, fn_name, reports);
            }
        }
    }
}

/// Extract the variable name from an ownership target expression.
fn extract_ptr_name(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        _ => None,
    }
}

fn ownership_violation_report(
    fn_name: &str,
    ptr_name: &str,
    err: &OwnershipTransitionError,
    operation: &str,
    span: Span,
) -> DiagnosticReport {
    DiagnosticReport::new(
        ErrorKind::Borrow,
        DiagnosticCode::TypeGeneric,
        format!(
            "Ownership violation in '{}': cannot {} '{}' — {}",
            fn_name, operation, ptr_name, err
        ),
    )
    .severity(DiagnosticSeverity::Error)
    .phase(CompilerPhase::BorrowChecking)
    .primary_label(span, format!("invalid {} on '{}'", operation, ptr_name))
    .help(format!(
        "Check that '{}' is in the correct ownership state for {}. \
         Valid states: collapse requires Idle, observe requires Idle or Observed, \
         decay requires Idle (terminal).",
        ptr_name, operation
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_actor::definition::ActorDefinition;
    use kain_core::ast::{
        Actor, Block, ConvergeDef, ConvergeLane, ConvergeLaneKind, MessageHandler, Param,
        Visibility,
    };
    use kain_error::span::Span;

    fn make_handler_with_send(message_name: &str, field_name: &str) -> MessageHandler {
        MessageHandler {
            message_type: "Ping".to_string(),
            params: vec![Param {
                name: "reply_to".to_string(),
                ty: Type::Named {
                    name: "P".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                mutable: false,
                default: None,
                span: Span::default(),
            }],
            body: Block {
                stmts: vec![Stmt::Expr(Expr::SendMsg {
                    target: Box::new(Expr::Ident("reply_to".to_string(), Span::default())),
                    message: message_name.to_string(),
                    data: vec![(
                        field_name.to_string(),
                        Expr::Int(42, Span::default()),
                    )],
                    span: Span::new(100, 150),
                })],
                span: Span::default(),
            },
            span: Span::default(),
        }
    }

    fn make_actor_with_handler(handler: MessageHandler) -> TypedActor {
        TypedActor {
            ast: Actor {
                name: "TestActor".to_string(),
                state: vec![],
                handlers: vec![handler],
                methods: vec![],
                attributes: vec![],
                span: Span::default(),
            },
            state_types: Default::default(),
            actor_contract: ActorDefinition::new("TestActor".to_string()),
        }
    }

    #[test]
    fn test_reply_port_wrong_message_name() {
        let handler = make_handler_with_send("Report", "value");
        let actor = make_actor_with_handler(handler);
        let program = TypedProgram {
            items: vec![TypedItem::Actor(actor)],
        };

        let reports = validate_semantic_stack(&program);
        assert_eq!(reports.len(), 1, "should report wrong message name");
        assert!(
            reports[0].message.contains("'Report'"),
            "error should mention 'Report', got: {}",
            reports[0].message
        );
    }

    #[test]
    fn test_reply_port_correct_message_name() {
        let handler = make_handler_with_send("Reply", "value");
        let actor = make_actor_with_handler(handler);
        let program = TypedProgram {
            items: vec![TypedItem::Actor(actor)],
        };

        let reports = validate_semantic_stack(&program);
        assert!(
            reports.is_empty(),
            "correct reply should produce no errors, got: {:?}",
            reports
        );
    }

    #[test]
    fn test_reply_port_wrong_payload_field() {
        let handler = make_handler_with_send("Reply", "result");
        let actor = make_actor_with_handler(handler);
        let program = TypedProgram {
            items: vec![TypedItem::Actor(actor)],
        };

        let reports = validate_semantic_stack(&program);
        assert_eq!(reports.len(), 1, "should report wrong payload field");
        assert!(
            reports[0].message.contains("'result'"),
            "error should mention 'result', got: {}",
            reports[0].message
        );
    }

    #[test]
    fn test_converge_missing_fast_lanes() {
        let converge = kain_core::types::TypedConverge {
            ast: ConvergeDef {
                name: "mix".to_string(),
                params: vec![],
                return_type: None,
                spec_lane: ConvergeLane {
                    kind: ConvergeLaneKind::Spec,
                    lane_name: "reference".to_string(),
                    selector: None,
                    body: Block {
                        stmts: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                fast_lanes: vec![],
                verify_random_count: Some(8),
                visibility: Visibility::Private,
                attributes: vec![],
                span: Span::default(),
            },
            resolved_type: kain_core::types::ResolvedType::Int(kain_core::types::IntSize::I64),
        };

        let program = TypedProgram {
            items: vec![TypedItem::Converge(converge)],
        };

        let reports = validate_semantic_stack(&program);
        assert!(
            reports.iter().any(|r| r.message.contains("no fast lanes")),
            "should report missing fast lanes, got: {:?}",
            reports
        );
    }

    #[test]
    fn test_converge_missing_verify() {
        let converge = kain_core::types::TypedConverge {
            ast: ConvergeDef {
                name: "mix".to_string(),
                params: vec![],
                return_type: None,
                spec_lane: ConvergeLane {
                    kind: ConvergeLaneKind::Spec,
                    lane_name: "reference".to_string(),
                    selector: None,
                    body: Block {
                        stmts: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                },
                fast_lanes: vec![ConvergeLane {
                    kind: ConvergeLaneKind::Fast,
                    lane_name: "fast_lane".to_string(),
                    selector: None,
                    body: Block {
                        stmts: vec![],
                        span: Span::default(),
                    },
                    span: Span::default(),
                }],
                verify_random_count: None,
                visibility: Visibility::Private,
                attributes: vec![],
                span: Span::default(),
            },
            resolved_type: kain_core::types::ResolvedType::Int(kain_core::types::IntSize::I64),
        };

        let program = TypedProgram {
            items: vec![TypedItem::Converge(converge)],
        };

        let reports = validate_semantic_stack(&program);
        assert!(
            reports
                .iter()
                .any(|r| r.message.contains("missing a verify clause")),
            "should report missing verify clause, got: {:?}",
            reports
        );
    }
}
