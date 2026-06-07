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

/// Run all proactive semantic validators against a typed program.
pub fn validate_semantic_stack(program: &TypedProgram) -> Vec<DiagnosticReport> {
    let mut reports = Vec::new();
    validate_reply_ports(program, &mut reports);
    validate_converge_contracts(program, &mut reports);
    // Add more validators here:
    // validate_entangle_type_match(program, &mut reports);
    // validate_orchestrate_graph(program, &mut reports);
    // validate_ownership_transitions(program, &mut reports);
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
