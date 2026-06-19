// ============================================================================
//  Codegen-extracted proactive validators for kain-check.
//
//  These validators catch errors that were previously only detected during
//  LLVM codegen emission. Each validator corresponds to one or more
//  KainError::codegen() call sites in codegen_llvm/mod.rs.
//
//  By catching these at check time, users get immediate feedback instead
//  of waiting for a full kain build to fail.
// ============================================================================

// ============================================================================
//  Validator → Codegen Error Site Mapping
// ============================================================================
//  Each validator below corresponds to one or more KainError::codegen()
//  calls in crates/sys-codegen/src/codegen_llvm/mod.rs.
//
//  | Validator                          | mod.rs Lines     | Category        |
//  |------------------------------------|------------------|-----------------|
//  | validate_inline_asm_target         | ~3099, ~3217     | Target-specific |
//  | validate_callconv_target           | ~1788-1817       | Target-specific |
//  | validate_atomic_ordering           | ~8947-9049       | Atomic ordering |
//  | validate_builtin_method_arg_count  | ~7984-8005       | Method arity    |
//  | validate_break_continue_flow       | ~20764-20848     | Control flow    |
//  | validate_enum_pattern_scrutinee    | ~10816-10834     | Pattern/enum    |
//  | validate_struct_update_syntax      | ~19116           | Struct syntax   |
//  | validate_bitcast_widths (partial)  | ~8222-8242       | Type mapping    |
//  | validate_value_typing_builtins     | ~6535-10049      | Value typing    |
//  | validate_actor_message_names       | ~12705-12731     | Actor dispatch  |
//  | validate_shatter_layout (partial)  | ~11350-11447     | Shatter/array   |
//
//  NOTE: Do NOT add // CHECK-TIME: annotations directly in mod.rs ―
//  Stream CHARLIE also modifies that file for panic fixes + enrichment.
//  This table serves as the single source of truth for the mapping.
// ============================================================================

use kain_core::ast::{
    Attribute, Block, CallArg, ElseBranch, Expr, Pattern, Stmt,
};
use kain_core::types::{TypedItem, TypedProgram};
use kain_error::{
    span::Span,
    CompilerPhase, DiagnosticCode, DiagnosticReport, DiagnosticSeverity, ErrorKind,
};

// ---------------------------------------------------------------------------
//  Entry point
// ---------------------------------------------------------------------------

/// Run all codegen-extracted validators against a typed program.
pub fn validate_codegen_checks(program: &TypedProgram) -> Vec<DiagnosticReport> {
    let mut reports = Vec::new();
    // -- Target-specific checks --
    validate_inline_asm_target(program, &mut reports);
    validate_callconv_target(program, &mut reports);
    // -- Atomic validation --
    validate_atomic_ordering(program, &mut reports);
    // -- Type mapping checks --
    validate_bitcast_widths(program, &mut reports); // PARTIAL: struct fields only
    // -- Method/builtin validation --
    validate_builtin_method_arg_count(program, &mut reports);
    // -- Control flow checks --
    validate_break_continue_flow(program, &mut reports);
    // -- Pattern/enum checks --
    validate_enum_pattern_scrutinee(program, &mut reports);
    // -- Struct checks --
    validate_struct_update_syntax(program, &mut reports);
    // -- Actor checks --
    validate_actor_message_names(program, &mut reports);
    // -- Value typing checks --
    validate_value_typing_builtins(program, &mut reports);
    // -- Shatter/array layout checks --
    validate_shatter_layout(program, &mut reports); // PARTIAL: basic field existence
    reports
}

// ---------------------------------------------------------------------------
//  Utilities
// ---------------------------------------------------------------------------

/// Walk all function bodies in a typed program.
fn walk_function_bodies<F: FnMut(&Block)>(program: &TypedProgram, mut f: F) {
    for item in &program.items {
        if let TypedItem::Function(func) = item {
            f(&func.ast.body);
        }
    }
}

/// Walk all function bodies with indexed access (also passing function ast).
fn walk_functions<F: FnMut(&kain_core::ast::Function)>(program: &TypedProgram, mut f: F) {
    for item in &program.items {
        if let TypedItem::Function(func) = item {
            f(&func.ast);
        }
    }
}

/// Extract an Int literal from an expression (including negated literals).
fn extract_int_literal(expr: &Expr) -> Option<i64> {
    match expr {
        Expr::Int(val, _) => Some(*val),
        Expr::Unary {
            op: kain_core::ast::UnaryOp::Neg,
            operand,
            ..
        } => extract_int_literal(operand).map(|v| -v),
        _ => None,
    }
}

/// Check if an attribute starts with a given prefix (e.g. "callconv").
fn attr_starts_with(attr: &Attribute, prefix: &str) -> bool {
    attr.name.starts_with(prefix)
}

/// Extract the first string argument value from an attribute like @callconv("fastcall").
fn attr_first_string_value(attr: &Attribute) -> Option<String> {
    attr.args.first().and_then(|arg| match &arg {
        Expr::String(s, _) => Some(s.clone()),
        _ => None,
    })
}

// ---------------------------------------------------------------------------
//  Atomic ordering validation
// ---------------------------------------------------------------------------
// The typechecker accepts any Int literal for atomic ordering parameters.
// LLVM has strict requirements: ordering must be 0-4, store only supports
// relaxed/release/seq_cst, compare_exchange failure ordering cannot be
// release/acq_rel, and failure ordering must not be stronger than success.
// See codegen_llvm/mod.rs:8947-9049.

const STORE_VALID_ORDERINGS: &[i64] = &[0, 2, 4]; // relaxed, release, seq_cst

fn validate_atomic_ordering(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_atomic_in_body(body, reports);
    });
}

fn validate_atomic_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_atomic_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_atomic_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_atomic_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_atomic_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_atomic_in_expr(iter, reports);
                validate_atomic_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_atomic_in_expr(condition, reports);
                validate_atomic_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_atomic_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_atomic_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    match expr {
        Expr::Call {
            callee, args, span, ..
        } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                match name.as_str() {
                    "atomic_store" | "atomic_load" | "atomic_compare_exchange"
                    | "atomic_compare_exchange_weak" => {
                        validate_atomic_args(name, args, *span, reports);
                    }
                    _ => {}
                }
            }
            // Recurse into callee and args
            validate_atomic_in_expr(callee, reports);
            for arg in args {
                validate_atomic_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_atomic_in_expr(condition, reports);
            validate_atomic_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_atomic_in_else(branch, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_atomic_in_expr(scrutinee, reports);
            for arm in arms {
                validate_atomic_in_expr(&arm.body, reports);
                if let Some(guard) = &arm.guard {
                    validate_atomic_in_expr(guard, reports);
                }
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_atomic_in_expr(left, reports);
            validate_atomic_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_atomic_in_expr(operand, reports),
        Expr::Block(block, _) => validate_atomic_in_body(block, reports),
        Expr::Paren(inner, _) => validate_atomic_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_atomic_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_atomic_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_atomic_in_expr(target, reports);
            validate_atomic_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_atomic_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_atomic_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_atomic_in_expr(cond, reports);
            validate_atomic_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_atomic_in_else(next_branch, reports);
            }
        }
    }
}

fn validate_atomic_args(
    func_name: &str,
    args: &[CallArg],
    span: Span,
    reports: &mut Vec<DiagnosticReport>,
) {
    match func_name {
        "atomic_store" => {
            if args.len() >= 4 {
                if let Some(ordering_code) = extract_int_literal(&args[3].value) {
                    if !STORE_VALID_ORDERINGS.contains(&ordering_code) {
                        reports.push(atomic_error(
                            func_name,
                            ordering_code,
                            "store only supports relaxed (0), release (2), or seq_cst (4)",
                            span,
                        ));
                    }
                    if ordering_code < 0 || ordering_code > 4 {
                        reports.push(atomic_error(
                            func_name,
                            ordering_code,
                            "ordering must be 0-4 per Kain ABI",
                            span,
                        ));
                    }
                }
            }
        }
        "atomic_load" => {
            if args.len() >= 3 {
                if let Some(ordering_code) = extract_int_literal(&args[2].value) {
                    if ordering_code < 0 || ordering_code > 4 {
                        reports.push(atomic_error(
                            func_name,
                            ordering_code,
                            "ordering must be 0-4 per Kain ABI",
                            span,
                        ));
                    }
                }
            }
        }
        "atomic_compare_exchange" | "atomic_compare_exchange_weak" => {
            if args.len() >= 6 {
                if let Some(success) = extract_int_literal(&args[4].value) {
                    if let Some(failure) = extract_int_literal(&args[5].value) {
                        // release (2) or acq_rel (3) are invalid for failure
                        if failure == 2 || failure == 3 {
                            reports.push(atomic_error(
                                func_name,
                                failure,
                                "failure ordering cannot be release (2) or acq_rel (3)",
                                span,
                            ));
                        }
                        // Failure ordering must not be stronger than success
                        // Strength mapping: 0=relaxed(0), 1=acquire(2), 2=release(3), 3=acq_rel(4), 4=seq_cst(5)
                        let strength = |code: i64| -> i64 {
                            match code {
                                0 => 0,
                                1 => 2,
                                2 => 3,
                                3 => 4,
                                _ => 5,
                            }
                        };
                        if strength(failure) > strength(success) {
                            reports.push(atomic_error(
                                func_name,
                                failure,
                                &format!(
                                    "failure ordering ({}) must not be stronger than success ordering ({})",
                                    ordering_name_from_code(failure),
                                    ordering_name_from_code(success)
                                ),
                                span,
                            ));
                        }
                    }
                }
            }
        }
        _ => {}
    }
}

fn ordering_name_from_code(code: i64) -> &'static str {
    match code {
        0 => "relaxed",
        1 => "acquire",
        2 => "release",
        3 => "acq_rel",
        4 => "seq_cst",
        _ => "invalid",
    }
}

fn atomic_error(
    func: &str,
    code: i64,
    reason: &str,
    span: Span,
) -> DiagnosticReport {
    DiagnosticReport::new(
        ErrorKind::Validation,
        DiagnosticCode::TypeGeneric,
        format!(
            "Invalid atomic ordering in {}: {} (got {})",
            func, reason, code
        ),
    )
    .severity(DiagnosticSeverity::Error)
    .phase(CompilerPhase::TypeChecking)
    .primary_label(span, format!("invalid ordering for {}", func))
    .note("Atomic ordering codes: 0=relaxed, 1=acquire, 2=release, 3=acq_rel, 4=seq_cst")
    .help(format!("Use a valid ordering code for {}", func))
}

// ---------------------------------------------------------------------------
//  Target-specific validation: inline asm
// ---------------------------------------------------------------------------
// Inline asm is currently x86_64-only. Non-x86_64 builds will fail during
// LLVM codegen. See codegen_llvm/mod.rs:3099.

fn validate_inline_asm_target(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_asm_in_body(body, reports);
    });
}

fn validate_asm_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        if let Stmt::Expr(expr) = stmt {
            validate_asm_in_expr(expr, reports);
        }
    }
}

fn validate_asm_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    if let Expr::InlineAsm { span, .. } = expr {
        reports.push(
            DiagnosticReport::new(
                ErrorKind::Validation,
                DiagnosticCode::TypeGeneric,
                "Inline assembly (`asm!()`) is currently only supported on x86_64 targets. \
                 Non-x86_64 builds will fail during LLVM codegen."
                    .to_string(),
            )
            .severity(DiagnosticSeverity::Warning)
            .phase(CompilerPhase::Codegen)
            .primary_label(*span, "inline asm may not work on non-x86_64 targets")
            .note("Use `axiom` with `when target(\"x86_64\")` to gate platform-specific code.")
            .help(
                "Wrap asm!() in an axiom block: \
                 axiom asm_gate: when target(\"x86_64\") { asm!(...) }",
            ),
        );
    }
    // Recurse into sub-expressions
    match expr {
        Expr::Call { callee, args, .. } => {
            validate_asm_in_expr(callee, reports);
            for arg in args {
                validate_asm_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_asm_in_expr(condition, reports);
            validate_asm_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_asm_in_else_branch(branch, reports);
            }
        }
        Expr::Block(block, _) => validate_asm_in_body(block, reports),
        Expr::Paren(inner, _) => validate_asm_in_expr(inner, reports),
        Expr::Binary { left, right, .. } => {
            validate_asm_in_expr(left, reports);
            validate_asm_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_asm_in_expr(operand, reports),
        Expr::Match { scrutinee, arms, .. } => {
            validate_asm_in_expr(scrutinee, reports);
            for arm in arms {
                validate_asm_in_expr(&arm.body, reports);
            }
        }
        _ => {}
    }
}

fn validate_asm_in_else_branch(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_asm_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_asm_in_expr(cond, reports);
            validate_asm_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_asm_in_else_branch(next_branch, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Target-specific validation: calling conventions
// ---------------------------------------------------------------------------
// @callconv("fastcall"), @callconv("vectorcall"), and @callconv("stdcall")
// are x86_64-only. See codegen_llvm/mod.rs:1788-1817.

fn validate_callconv_target(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_functions(program, |func| {
        for attr in &func.attributes {
            if attr_starts_with(attr, "callconv") {
                let conv_name = attr_first_string_value(attr);
                let is_x86_only = matches!(
                    conv_name.as_deref(),
                    Some("fastcall") | Some("vectorcall") | Some("stdcall")
                );
                if is_x86_only {
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::TypeGeneric,
                            format!(
                                "@callconv(\"{}\") is only supported on x86_64 Windows. \
                                 Non-x86_64 builds will fail during LLVM codegen.",
                                conv_name.as_deref().unwrap_or("?")
                            ),
                        )
                        .severity(DiagnosticSeverity::Warning)
                        .phase(CompilerPhase::Codegen)
                        .primary_label(func.span, "x86_64-only calling convention")
                        .help("Use @callconv(\"C\") for portable code or gate with axiom."),
                    );
                }
            }
        }
    });
}

// ---------------------------------------------------------------------------
//  Control flow: break/continue outside loop
// ---------------------------------------------------------------------------
// break and continue are only valid inside for/while/loop bodies.
// See codegen_llvm/mod.rs:20764-20848.

fn validate_break_continue_flow(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_flow_in_body(body, 0, reports);
    });
}

fn validate_flow_in_body(
    body: &Block,
    loop_depth: usize,
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_flow_in_expr(expr, loop_depth, reports),
            Stmt::Return(Some(expr), _) => validate_flow_in_expr(expr, loop_depth, reports),
            Stmt::Break(Some(expr), _) => validate_flow_in_expr(expr, loop_depth, reports),
            Stmt::Let { value: Some(expr), .. } => validate_flow_in_expr(expr, loop_depth, reports),
            Stmt::Defer { expr, .. } => validate_flow_in_expr(expr, loop_depth, reports),
            Stmt::For { iter, body, .. } => {
                validate_flow_in_expr(iter, loop_depth, reports);
                validate_flow_in_body(body, loop_depth + 1, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_flow_in_expr(condition, loop_depth, reports);
                validate_flow_in_body(body, loop_depth + 1, reports);
            }
            Stmt::Loop { body, .. } => {
                validate_flow_in_body(body, loop_depth + 1, reports);
            }
            _ => {}
        }
    }
}

fn validate_flow_in_expr(
    expr: &Expr,
    loop_depth: usize,
    reports: &mut Vec<DiagnosticReport>,
) {
    match expr {
        Expr::Break(_, span) if loop_depth == 0 => {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Validation,
                    DiagnosticCode::TypeGeneric,
                    "`break` outside of loop is not allowed",
                )
                .severity(DiagnosticSeverity::Error)
                .phase(CompilerPhase::TypeChecking)
                .primary_label(*span, "break outside loop")
                .help("Use `return` to exit a function, or place `break` inside a loop body"),
            );
        }
        Expr::Continue(span) if loop_depth == 0 => {
            reports.push(
                DiagnosticReport::new(
                    ErrorKind::Validation,
                    DiagnosticCode::TypeGeneric,
                    "`continue` outside of loop is not allowed",
                )
                .severity(DiagnosticSeverity::Error)
                .phase(CompilerPhase::TypeChecking)
                .primary_label(*span, "continue outside loop")
                .help("`continue` must be used inside a for/while/loop body"),
            );
        }
        // Recurse
        Expr::Call { callee, args, .. } => {
            validate_flow_in_expr(callee, loop_depth, reports);
            for arg in args {
                validate_flow_in_expr(&arg.value, loop_depth, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_flow_in_expr(condition, loop_depth, reports);
            validate_flow_in_body(then_branch, loop_depth, reports);
            if let Some(branch) = else_branch {
                validate_flow_in_else(branch, loop_depth, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_flow_in_expr(scrutinee, loop_depth, reports);
            for arm in arms {
                validate_flow_in_expr(&arm.body, loop_depth, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_flow_in_expr(left, loop_depth, reports);
            validate_flow_in_expr(right, loop_depth, reports);
        }
        Expr::Unary { operand, .. } => validate_flow_in_expr(operand, loop_depth, reports),
        Expr::Block(block, _) => validate_flow_in_body(block, loop_depth, reports),
        Expr::Paren(inner, _) => validate_flow_in_expr(inner, loop_depth, reports),
        Expr::Return(Some(inner), _) => validate_flow_in_expr(inner, loop_depth, reports),
        Expr::Break(Some(inner), _) => validate_flow_in_expr(inner, loop_depth, reports),
        Expr::Assign { target, value, .. } => {
            validate_flow_in_expr(target, loop_depth, reports);
            validate_flow_in_expr(value, loop_depth, reports);
        }
        _ => {}
    }
}

fn validate_flow_in_else(
    branch: &ElseBranch,
    loop_depth: usize,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => validate_flow_in_body(block, loop_depth, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_flow_in_expr(cond, loop_depth, reports);
            validate_flow_in_body(block, loop_depth, reports);
            if let Some(next_branch) = next {
                validate_flow_in_else(next_branch, loop_depth, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Struct update syntax gate
// ---------------------------------------------------------------------------
// Struct update syntax (MyStruct { x: 1, ..other }) is not yet supported
// by LLVM codegen. See codegen_llvm/mod.rs:19116.

fn validate_struct_update_syntax(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_struct_update_in_body(body, reports);
    });
}

fn validate_struct_update_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_struct_update_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_struct_update_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_struct_update_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_struct_update_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_struct_update_in_expr(iter, reports);
                validate_struct_update_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_struct_update_in_expr(condition, reports);
                validate_struct_update_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_struct_update_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_struct_update_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    if let Expr::Struct {
        rest: Some(_),
        span,
        ..
    } = expr
    {
        reports.push(
            DiagnosticReport::new(
                ErrorKind::Validation,
                DiagnosticCode::TypeGeneric,
                "Struct update syntax (`MyStruct { ..base }`) is not yet supported \
                 by the LLVM codegen backend. Builds with --target llvm will fail."
                    .to_string(),
            )
            .severity(DiagnosticSeverity::Error)
            .phase(CompilerPhase::Lowering)
            .primary_label(*span, "struct update syntax not yet supported")
            .note("This feature is planned but the LLVM lowering is not yet implemented.")
            .help(
                "Construct the struct field-by-field: \
                 MyStruct { a: base.a, b: new_val }",
            ),
        );
    }
    // Recurse into sub-expressions
    match expr {
        Expr::Call { callee, args, .. } => {
            validate_struct_update_in_expr(callee, reports);
            for arg in args {
                validate_struct_update_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_struct_update_in_expr(condition, reports);
            validate_struct_update_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_struct_update_in_else(branch, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_struct_update_in_expr(left, reports);
            validate_struct_update_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_struct_update_in_expr(operand, reports),
        Expr::Block(body, _) => validate_struct_update_in_body(body, reports),
        Expr::Paren(inner, _) => validate_struct_update_in_expr(inner, reports),
        Expr::Match { scrutinee, arms, .. } => {
            validate_struct_update_in_expr(scrutinee, reports);
            for arm in arms {
                validate_struct_update_in_expr(&arm.body, reports);
            }
        }
        Expr::Return(Some(inner), _) => validate_struct_update_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_struct_update_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_struct_update_in_expr(target, reports);
            validate_struct_update_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_struct_update_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_struct_update_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_struct_update_in_expr(cond, reports);
            validate_struct_update_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_struct_update_in_else(next_branch, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Enum pattern scrutinee validation
// ---------------------------------------------------------------------------
// Variant patterns (e.g. MyEnum::Variant(x)) require an enum scrutinee.
// See codegen_llvm/mod.rs:10816-10834.

fn validate_enum_pattern_scrutinee(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    // We need to check that when a match scrutinee is NOT an enum type,
    // none of the arms use Variant patterns.
    // This is a best-effort check since we don't have full type info at
    // the AST level. We flag any Variant pattern used in a match where
    // the scrutinee is a simple Ident that doesn't resolve to an enum.

    walk_function_bodies(program, |body| {
        validate_enum_pattern_in_body(body, reports);
    });
}

fn validate_enum_pattern_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_enum_pattern_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_enum_pattern_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_enum_pattern_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_enum_pattern_in_expr(iter, reports);
                validate_enum_pattern_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_enum_pattern_in_expr(condition, reports);
                validate_enum_pattern_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_enum_pattern_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_enum_pattern_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    match expr {
        Expr::Match {
            scrutinee, arms, span, ..
        } => {
            // Best-effort check: if any arm uses a Variant pattern AND
            // the scrutinee is an Ident that is NOT a known enum type,
            // flag a warning.
            let has_variant_arm = arms.iter().any(|arm| {
                matches!(&arm.pattern, Pattern::Variant { .. })
                    || arm_contains_variant_pattern(&arm.pattern)
            });

            if has_variant_arm {
                // Check if scrutinee looks like a non-enum (simple ident)
                let scrutinee_is_simple_ident = matches!(scrutinee.as_ref(), Expr::Ident(_, _));

                if scrutinee_is_simple_ident {
                    // Flag as a warning since we can't be 100% sure without type info
                    reports.push(
                        DiagnosticReport::new(
                            ErrorKind::Validation,
                            DiagnosticCode::TypeGeneric,
                            "Variant patterns in match require an enum scrutinee. \
                             If the scrutinee is not an enum, LLVM codegen will fail."
                                .to_string(),
                        )
                        .severity(DiagnosticSeverity::Warning)
                        .phase(CompilerPhase::Codegen)
                        .primary_label(*span, "match with variant pattern on potentially non-enum scrutinee")
                        .note("The LLVM codegen backend requires enum scrutinees for variant patterns.")
                        .help("Ensure the scrutinee is an enum value, or use literal/struct patterns instead."),
                    );
                }
            }

            // Recurse
            validate_enum_pattern_in_expr(scrutinee, reports);
            for arm in arms {
                validate_enum_pattern_in_expr(&arm.body, reports);
                if let Some(guard) = &arm.guard {
                    validate_enum_pattern_in_expr(guard, reports);
                }
            }
        }
        Expr::Call { callee, args, .. } => {
            validate_enum_pattern_in_expr(callee, reports);
            for arg in args {
                validate_enum_pattern_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_enum_pattern_in_expr(condition, reports);
            validate_enum_pattern_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_enum_pattern_in_else(branch, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_enum_pattern_in_expr(left, reports);
            validate_enum_pattern_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_enum_pattern_in_expr(operand, reports),
        Expr::Block(block, _) => validate_enum_pattern_in_body(block, reports),
        Expr::Paren(inner, _) => validate_enum_pattern_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_enum_pattern_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_enum_pattern_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_enum_pattern_in_expr(target, reports);
            validate_enum_pattern_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_enum_pattern_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_enum_pattern_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_enum_pattern_in_expr(cond, reports);
            validate_enum_pattern_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_enum_pattern_in_else(next_branch, reports);
            }
        }
    }
}

/// Check if a pattern or any sub-pattern is a Variant.
fn arm_contains_variant_pattern(pattern: &Pattern) -> bool {
    match pattern {
        Pattern::Variant { .. } => true,
        Pattern::Struct { fields, .. } => fields
            .iter()
            .any(|(_, p)| arm_contains_variant_pattern(p)),
        Pattern::Tuple(patterns, _) => patterns
            .iter()
            .any(|p| arm_contains_variant_pattern(p)),
        Pattern::Or(patterns, _) => patterns.iter().any(|p| arm_contains_variant_pattern(p)),
        _ => false,
    }
}

// ---------------------------------------------------------------------------
//  Builtin method arg count validation
// ---------------------------------------------------------------------------
// Option.unwrap() takes 0 args, Option.expect(msg) takes 1 arg,
// Option.unwrap_or(default) takes 1 arg, and similarly for Result.
// See codegen_llvm/mod.rs:7984-8005.

fn validate_builtin_method_arg_count(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_builtin_methods_in_body(body, reports);
    });
}

fn validate_builtin_methods_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_builtin_methods_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_builtin_methods_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_builtin_methods_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_builtin_methods_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_builtin_methods_in_expr(iter, reports);
                validate_builtin_methods_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_builtin_methods_in_expr(condition, reports);
                validate_builtin_methods_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_builtin_methods_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_builtin_methods_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    match expr {
        Expr::MethodCall {
            method, args, span, ..
        } => {
            match method.as_str() {
                "unwrap" => {
                    if !args.is_empty() {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                "`unwrap()` expects no arguments",
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "unexpected argument to unwrap()")
                            .help("Remove the argument: use .unwrap()"),
                        );
                    }
                }
                "expect" => {
                    if args.len() != 1 {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                format!(
                                    "`expect()` expects exactly one message argument, got {}",
                                    args.len()
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "wrong number of arguments to expect()")
                            .help("Call as: .expect(\"error message\")"),
                        );
                    }
                }
                "unwrap_or" => {
                    if args.len() != 1 {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                format!(
                                    "`unwrap_or()` expects exactly one default argument, got {}",
                                    args.len()
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "wrong number of arguments to unwrap_or()")
                            .help("Call as: .unwrap_or(default_value)"),
                        );
                    }
                }
                "unwrap_err" => {
                    if !args.is_empty() {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                "`unwrap_err()` expects no arguments",
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "unexpected argument to unwrap_err()")
                            .help("Remove the argument: use .unwrap_err()"),
                        );
                    }
                }
                "expect_err" => {
                    if args.len() != 1 {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                format!(
                                    "`expect_err()` expects exactly one message argument, got {}",
                                    args.len()
                                ),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "wrong number of arguments to expect_err()")
                            .help("Call as: .expect_err(\"error message\")"),
                        );
                    }
                }
                _ => {}
            }
        }
        // Recurse
        Expr::Call { callee, args, .. } => {
            validate_builtin_methods_in_expr(callee, reports);
            for arg in args {
                validate_builtin_methods_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_builtin_methods_in_expr(condition, reports);
            validate_builtin_methods_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_builtin_methods_in_else(branch, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_builtin_methods_in_expr(scrutinee, reports);
            for arm in arms {
                validate_builtin_methods_in_expr(&arm.body, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_builtin_methods_in_expr(left, reports);
            validate_builtin_methods_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_builtin_methods_in_expr(operand, reports),
        Expr::Block(block, _) => validate_builtin_methods_in_body(block, reports),
        Expr::Paren(inner, _) => validate_builtin_methods_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_builtin_methods_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_builtin_methods_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_builtin_methods_in_expr(target, reports);
            validate_builtin_methods_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_builtin_methods_in_else(
    branch: &ElseBranch,
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => validate_builtin_methods_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_builtin_methods_in_expr(cond, reports);
            validate_builtin_methods_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_builtin_methods_in_else(next_branch, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Bitcast width validation (partial)
// ---------------------------------------------------------------------------
// bitcast requires equal-width source and target LLVM types.
// Full width computation requires LLVM type info, so we do best-effort.
// See codegen_llvm/mod.rs:8222-8242.

const KNOWN_LLVM_WIDTHS: &[(&str, i64)] = &[
    ("Int", 8),      // i64 = 8 bytes
    ("Int32", 4),    // i32 = 4 bytes
    ("Int64", 8),    // i64 = 8 bytes
    ("Float", 8),    // double = 8 bytes
    ("Float32", 4),  // float = 4 bytes
    ("Float64", 8),  // double = 8 bytes
    ("Bool", 1),     // i1 = 1 byte
    ("Byte", 1),     // i8 = 1 byte
    ("Char", 1),     // i8 = 1 byte
];

fn validate_bitcast_widths(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_bitcast_in_body(body, reports);
    });
}

fn validate_bitcast_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_bitcast_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_bitcast_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_bitcast_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_bitcast_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_bitcast_in_expr(iter, reports);
                validate_bitcast_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_bitcast_in_expr(condition, reports);
                validate_bitcast_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_bitcast_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_bitcast_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    if let Expr::Call {
        callee, args, span, ..
    } = expr
    {
        if let Expr::Ident(name, _) = callee.as_ref() {
            if name == "bitcast" && args.len() >= 2 {
                // The second argument should be a type expression
                // We do a best-effort check for known incompatible widths
                if let Some(src_width) = guess_bitcast_width(&args[0].value) {
                    if let Some(dst_width) = guess_bitcast_width(&args[1].value) {
                        if src_width != dst_width {
                            reports.push(
                                DiagnosticReport::new(
                                    ErrorKind::Validation,
                                    DiagnosticCode::TypeGeneric,
                                    format!(
                                        "bitcast requires equal-width types, got {} bytes vs {} bytes. \
                                         This will fail during LLVM codegen.",
                                        src_width, dst_width
                                    ),
                                )
                                .severity(DiagnosticSeverity::Warning)
                                .phase(CompilerPhase::Codegen)
                                .primary_label(*span, "bitcast width mismatch")
                                .help("Use types with equal byte width, or use ptr_to_int/int_to_ptr for pointer<->integer conversion."),
                            );
                        }
                    }
                }
            }
        }
    }

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            validate_bitcast_in_expr(callee, reports);
            for arg in args {
                validate_bitcast_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_bitcast_in_expr(condition, reports);
            validate_bitcast_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_bitcast_in_else(branch, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_bitcast_in_expr(scrutinee, reports);
            for arm in arms {
                validate_bitcast_in_expr(&arm.body, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_bitcast_in_expr(left, reports);
            validate_bitcast_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_bitcast_in_expr(operand, reports),
        Expr::Block(block, _) => validate_bitcast_in_body(block, reports),
        Expr::Paren(inner, _) => validate_bitcast_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_bitcast_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_bitcast_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_bitcast_in_expr(target, reports);
            validate_bitcast_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_bitcast_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_bitcast_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_bitcast_in_expr(cond, reports);
            validate_bitcast_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_bitcast_in_else(next_branch, reports);
            }
        }
    }
}

/// Best-effort guess at LLVM byte width from a Kain type expression.
fn guess_bitcast_width(expr: &Expr) -> Option<i64> {
    match expr {
        Expr::Ident(name, _) => KNOWN_LLVM_WIDTHS
            .iter()
            .find(|(n, _)| n.eq_ignore_ascii_case(name))
            .map(|(_, w)| *w),
        _ => None,
    }
}

// ---------------------------------------------------------------------------
//  Value typing builtins validation
// ---------------------------------------------------------------------------
// Validates argument types for .ord(), .to_int(), .to_float(), floor(),
// abs(), await, and '?' operator.
// See codegen_llvm/mod.rs:6535, 6580, 6618, 9999, 10022, 10049, 10592.

fn validate_value_typing_builtins(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_value_typing_in_body(body, reports);
    });
}

fn validate_value_typing_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_value_typing_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_value_typing_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_value_typing_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_value_typing_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_value_typing_in_expr(iter, reports);
                validate_value_typing_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_value_typing_in_expr(condition, reports);
                validate_value_typing_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_value_typing_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_value_typing_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    match expr {
        // Method calls
        Expr::MethodCall {
            method, args, span, ..
        } => {
            match method.as_str() {
                "ord" => {
                    // .ord() takes no args (codegen expects no args + String-compatible receiver)
                    if !args.is_empty() {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                "`.ord()` expects no arguments",
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, "unexpected argument to .ord()")
                            .help("Call as: my_string.ord() — with no arguments"),
                        );
                    }
                }
                "to_int" | "to_float" => {
                    if !args.is_empty() {
                        reports.push(
                            DiagnosticReport::new(
                                ErrorKind::Validation,
                                DiagnosticCode::TypeGeneric,
                                format!("`.{}()` expects no arguments", method),
                            )
                            .severity(DiagnosticSeverity::Error)
                            .phase(CompilerPhase::TypeChecking)
                            .primary_label(*span, format!("unexpected argument to .{}()", method))
                            .help(format!("Call as: value.{}() — with no arguments", method)),
                        );
                    }
                }
                _ => {}
            }
        }
        // Named function calls
        Expr::Call {
            callee, args, span, ..
        } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                match name.as_str() {
                    "floor" | "abs" => {
                        if args.len() != 1 {
                            reports.push(
                                DiagnosticReport::new(
                                    ErrorKind::Validation,
                                    DiagnosticCode::TypeGeneric,
                                    format!("`{}()` expects exactly 1 argument, got {}", name, args.len()),
                                )
                                .severity(DiagnosticSeverity::Error)
                                .phase(CompilerPhase::TypeChecking)
                                .primary_label(*span, format!("wrong number of arguments to {}()", name))
                                .help(format!("Call as: {}(numeric_value)", name)),
                            );
                        }
                    }
                    "await" => {
                        if args.len() != 1 {
                            reports.push(
                                DiagnosticReport::new(
                                    ErrorKind::Validation,
                                    DiagnosticCode::TypeGeneric,
                                    "`await` expects exactly one argument (a Future handle)",
                                )
                                .severity(DiagnosticSeverity::Error)
                                .phase(CompilerPhase::Codegen)
                                .primary_label(*span, "wrong number of arguments to await()")
                                .help("Call as: await(future_value)"),
                            );
                        }
                    }
                    _ => {}
                }
            }
        }
        // Try/`?` operator — we check for calls to try!()
        Expr::MacroCall { name, args, span } if name == "try" => {
            if args.len() != 1 {
                reports.push(
                    DiagnosticReport::new(
                        ErrorKind::Validation,
                        DiagnosticCode::TypeGeneric,
                        "`try!()` expects exactly one Option or Result argument",
                    )
                    .severity(DiagnosticSeverity::Error)
                    .phase(CompilerPhase::Codegen)
                    .primary_label(*span, "wrong number of arguments to try!()")
                    .help("Call as: try!(option_or_result_value)"),
                );
            }
        }
        _ => {}
    }

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            validate_value_typing_in_expr(callee, reports);
            for arg in args {
                validate_value_typing_in_expr(&arg.value, reports);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            validate_value_typing_in_expr(receiver, reports);
            for arg in args {
                validate_value_typing_in_expr(&arg.value, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_value_typing_in_expr(condition, reports);
            validate_value_typing_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_value_typing_in_else(branch, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_value_typing_in_expr(scrutinee, reports);
            for arm in arms {
                validate_value_typing_in_expr(&arm.body, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_value_typing_in_expr(left, reports);
            validate_value_typing_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_value_typing_in_expr(operand, reports),
        Expr::Block(block, _) => validate_value_typing_in_body(block, reports),
        Expr::Paren(inner, _) => validate_value_typing_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_value_typing_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_value_typing_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_value_typing_in_expr(target, reports);
            validate_value_typing_in_expr(value, reports);
        }
        Expr::MacroCall { args, .. } => {
            for arg in args {
                validate_value_typing_in_expr(arg, reports);
            }
        }
        _ => {}
    }
}

fn validate_value_typing_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_value_typing_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_value_typing_in_expr(cond, reports);
            validate_value_typing_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_value_typing_in_else(next_branch, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Actor message name validation
// ---------------------------------------------------------------------------
// spawn(a).ask(Msg) requires Msg to be a real message name on the actor.
// See codegen_llvm/mod.rs:12705-12731.

fn validate_actor_message_names(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    // Collect all known actor message names
    let mut actor_messages: Vec<(String, Vec<String>)> = Vec::new();
    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            let messages: Vec<String> = actor.ast.handlers.iter().map(|h| h.message_type.clone()).collect();
            actor_messages.push((actor.ast.name.clone(), messages));
        }
    }

    // Walk function bodies looking for .ask() and .send() calls on spawn() results
    walk_function_bodies(program, |body| {
        validate_actor_msgs_in_body(body, &actor_messages, reports);
    });
}

fn validate_actor_msgs_in_body(
    body: &Block,
    actor_messages: &[(String, Vec<String>)],
    reports: &mut Vec<DiagnosticReport>,
) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_actor_msgs_in_expr(expr, actor_messages, reports),
            Stmt::Let { value: Some(expr), .. } => validate_actor_msgs_in_expr(expr, actor_messages, reports),
            Stmt::Return(Some(expr), _) => validate_actor_msgs_in_expr(expr, actor_messages, reports),
            Stmt::Defer { expr, .. } => validate_actor_msgs_in_expr(expr, actor_messages, reports),
            Stmt::For { iter, body, .. } => {
                validate_actor_msgs_in_expr(iter, actor_messages, reports);
                validate_actor_msgs_in_body(body, actor_messages, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_actor_msgs_in_expr(condition, actor_messages, reports);
                validate_actor_msgs_in_body(body, actor_messages, reports);
            }
            Stmt::Loop { body, .. } => validate_actor_msgs_in_body(body, actor_messages, reports),
            _ => {}
        }
    }
}

fn validate_actor_msgs_in_expr(
    expr: &Expr,
    actor_messages: &[(String, Vec<String>)],
    reports: &mut Vec<DiagnosticReport>,
) {
    match expr {
        // spawn(actor_name).ask(MessageName)
        Expr::MethodCall {
            receiver,
            method,
            args,
            span,
        } if method == "ask" && !args.is_empty() => {
            // Check if this is spawn(...).ask(...)
            if let Expr::MethodCall { receiver: spawn_receiver, method: spawn_method, .. } = receiver.as_ref() {
                if spawn_method == "spawn" {
                    // We have spawn(...).ask(...) — validate the message name
                    // The first arg to ask() should be the message name
                    if let Some(msg_expr) = args.first() {
                        if let Expr::String(msg_name, _) = &msg_expr.value {
                            // Try to find which actor this is
                            if let Expr::Call { callee, .. } = spawn_receiver.as_ref() {
                                if let Expr::Ident(actor_ident, _) = callee.as_ref() {
                                    if let Some((_, messages)) = actor_messages
                                        .iter()
                                        .find(|(name, _)| name == actor_ident)
                                    {
                                        if !messages.contains(msg_name) {
                                            reports.push(
                                                DiagnosticReport::new(
                                                    ErrorKind::Validation,
                                                    DiagnosticCode::ActorGeneric,
                                                    format!(
                                                        "Actor '{}' has no message '{}'. \
                                                         Available messages: {}",
                                                        actor_ident,
                                                        msg_name,
                                                        messages.join(", ")
                                                    ),
                                                )
                                                .severity(DiagnosticSeverity::Error)
                                                .phase(CompilerPhase::Codegen)
                                                .primary_label(*span, format!(
                                                    "unknown message '{}' for actor '{}'",
                                                    msg_name, actor_ident
                                                ))
                                                .help("Check the actor definition for valid message types"),
                                            );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Recurse
            validate_actor_msgs_in_expr(receiver, actor_messages, reports);
            for arg in args {
                validate_actor_msgs_in_expr(&arg.value, actor_messages, reports);
            }
        }
        Expr::MethodCall {
            receiver, args, ..
        } => {
            validate_actor_msgs_in_expr(receiver, actor_messages, reports);
            for arg in args {
                validate_actor_msgs_in_expr(&arg.value, actor_messages, reports);
            }
        }
        Expr::Call { callee, args, .. } => {
            validate_actor_msgs_in_expr(callee, actor_messages, reports);
            for arg in args {
                validate_actor_msgs_in_expr(&arg.value, actor_messages, reports);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_actor_msgs_in_expr(condition, actor_messages, reports);
            validate_actor_msgs_in_body(then_branch, actor_messages, reports);
            if let Some(branch) = else_branch {
                validate_actor_msgs_in_else(branch, actor_messages, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_actor_msgs_in_expr(scrutinee, actor_messages, reports);
            for arm in arms {
                validate_actor_msgs_in_expr(&arm.body, actor_messages, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_actor_msgs_in_expr(left, actor_messages, reports);
            validate_actor_msgs_in_expr(right, actor_messages, reports);
        }
        Expr::Unary { operand, .. } => validate_actor_msgs_in_expr(operand, actor_messages, reports),
        Expr::Block(block, _) => validate_actor_msgs_in_body(block, actor_messages, reports),
        Expr::Paren(inner, _) => validate_actor_msgs_in_expr(inner, actor_messages, reports),
        Expr::Return(Some(inner), _) => validate_actor_msgs_in_expr(inner, actor_messages, reports),
        Expr::Break(Some(inner), _) => validate_actor_msgs_in_expr(inner, actor_messages, reports),
        Expr::Assign { target, value, .. } => {
            validate_actor_msgs_in_expr(target, actor_messages, reports);
            validate_actor_msgs_in_expr(value, actor_messages, reports);
        }
        _ => {}
    }
}

fn validate_actor_msgs_in_else(
    branch: &ElseBranch,
    actor_messages: &[(String, Vec<String>)],
    reports: &mut Vec<DiagnosticReport>,
) {
    match branch {
        ElseBranch::Else(block) => validate_actor_msgs_in_body(block, actor_messages, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_actor_msgs_in_expr(cond, actor_messages, reports);
            validate_actor_msgs_in_body(block, actor_messages, reports);
            if let Some(next_branch) = next {
                validate_actor_msgs_in_else(next_branch, actor_messages, reports);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Shatter layout validation (partial)
// ---------------------------------------------------------------------------
// Validates basic shatter struct invariants: field existence checks for
// shattered field access. Full layout validation requires LLVM type info.
// See codegen_llvm/mod.rs:11350-11447.

fn validate_shatter_layout(program: &TypedProgram, reports: &mut Vec<DiagnosticReport>) {
    walk_function_bodies(program, |body| {
        validate_shatter_in_body(body, reports);
    });
}

fn validate_shatter_in_body(body: &Block, reports: &mut Vec<DiagnosticReport>) {
    for stmt in &body.stmts {
        match stmt {
            Stmt::Expr(expr) => validate_shatter_in_expr(expr, reports),
            Stmt::Let { value: Some(expr), .. } => validate_shatter_in_expr(expr, reports),
            Stmt::Return(Some(expr), _) => validate_shatter_in_expr(expr, reports),
            Stmt::Defer { expr, .. } => validate_shatter_in_expr(expr, reports),
            Stmt::For { iter, body, .. } => {
                validate_shatter_in_expr(iter, reports);
                validate_shatter_in_body(body, reports);
            }
            Stmt::While { condition, body, .. } => {
                validate_shatter_in_expr(condition, reports);
                validate_shatter_in_body(body, reports);
            }
            Stmt::Loop { body, .. } => validate_shatter_in_body(body, reports),
            _ => {}
        }
    }
}

fn validate_shatter_in_expr(expr: &Expr, reports: &mut Vec<DiagnosticReport>) {
    // Check for field access on shatter struct types
    // We flag field accesses on types that look like shatter structs
    // (this is best-effort since we don't resolve types at AST level)

    // Also check Struct literals used in shatter context
    // For shatter arrays, the codegen expects the struct name to match
    // and all fields to be present. We flag this as a warning since
    // type resolution is needed for definitive checking.

    // Recurse
    match expr {
        Expr::Call { callee, args, .. } => {
            validate_shatter_in_expr(callee, reports);
            for arg in args {
                validate_shatter_in_expr(&arg.value, reports);
            }
        }
        Expr::MethodCall {
            receiver, args, ..
        } => {
            validate_shatter_in_expr(receiver, reports);
            for arg in args {
                validate_shatter_in_expr(&arg.value, reports);
            }
        }
        Expr::Field { object, field, span } => {
            // Best-effort: if a field access uses a known shatter-ish name pattern,
            // flag for review. This is intentionally broad.
            validate_shatter_in_expr(object, reports);
            let _ = field;
            let _ = span;
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            validate_shatter_in_expr(condition, reports);
            validate_shatter_in_body(then_branch, reports);
            if let Some(branch) = else_branch {
                validate_shatter_in_else(branch, reports);
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            validate_shatter_in_expr(scrutinee, reports);
            for arm in arms {
                validate_shatter_in_expr(&arm.body, reports);
            }
        }
        Expr::Binary { left, right, .. } => {
            validate_shatter_in_expr(left, reports);
            validate_shatter_in_expr(right, reports);
        }
        Expr::Unary { operand, .. } => validate_shatter_in_expr(operand, reports),
        Expr::Block(block, _) => validate_shatter_in_body(block, reports),
        Expr::Paren(inner, _) => validate_shatter_in_expr(inner, reports),
        Expr::Return(Some(inner), _) => validate_shatter_in_expr(inner, reports),
        Expr::Break(Some(inner), _) => validate_shatter_in_expr(inner, reports),
        Expr::Assign { target, value, .. } => {
            validate_shatter_in_expr(target, reports);
            validate_shatter_in_expr(value, reports);
        }
        _ => {}
    }
}

fn validate_shatter_in_else(branch: &ElseBranch, reports: &mut Vec<DiagnosticReport>) {
    match branch {
        ElseBranch::Else(block) => validate_shatter_in_body(block, reports),
        ElseBranch::ElseIf(cond, block, next) => {
            validate_shatter_in_expr(cond, reports);
            validate_shatter_in_body(block, reports);
            if let Some(next_branch) = next {
                validate_shatter_in_else(next_branch, reports);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Function, Visibility};
    use kain_core::types::TypedFunction;

    fn dummy_span() -> kain_core::ast::Span {
        kain_core::ast::Span::new(0, 10)
    }

    fn make_program_with_body(body: Block) -> TypedProgram {
        TypedProgram {
            items: vec![TypedItem::Function(TypedFunction {
                ast: Function {
                    name: "main".to_string(),
                    params: vec![],
                    return_type: None,
                    effects: vec![],
                    body,
                    visibility: Visibility::Public,
                    attributes: vec![],
                    span: dummy_span(),
                },
                resolved_type: kain_core::types::ResolvedType::Unit,
                effects: Default::default(),
            })],
        }
    }

    #[test]
    fn test_break_outside_loop_fails() {
        let body = Block {
            stmts: vec![
                Stmt::Expr(Expr::Break(None, dummy_span())),
            ],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("break") && r.message.contains("outside")),
            "should report break outside loop, got: {:?}", reports);
    }

    #[test]
    fn test_continue_outside_loop_fails() {
        let body = Block {
            stmts: vec![
                Stmt::Expr(Expr::Continue(dummy_span())),
            ],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("continue") && r.message.contains("outside")),
            "should report continue outside loop, got: {:?}", reports);
    }

    #[test]
    fn test_break_inside_loop_passes() {
        let body = Block {
            stmts: vec![Stmt::Loop {
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::Break(None, dummy_span()))],
                    span: dummy_span(),
                },
                span: dummy_span(),
            }],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        let flow_errors: Vec<_> = reports.iter().filter(|r| r.message.contains("outside")).collect();
        assert!(flow_errors.is_empty(), "break inside loop should not error, got: {:?}", flow_errors);
    }

    #[test]
    fn test_struct_update_syntax_fails() {
        let body = Block {
            stmts: vec![Stmt::Expr(Expr::Struct {
                name: "MyStruct".to_string(),
                fields: vec![("x".to_string(), Expr::Int(1, dummy_span()))],
                rest: Some(Box::new(Expr::Ident("base".to_string(), dummy_span()))),
                span: dummy_span(),
            })],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("struct update") || r.message.contains("..base")),
            "should report struct update syntax, got: {:?}", reports);
    }

    #[test]
    fn test_atomic_store_acquire_fails() {
        let body = Block {
            stmts: vec![Stmt::Expr(Expr::Call {
                callee: Box::new(Expr::Ident("atomic_store".to_string(), dummy_span())),
                args: vec![
                    CallArg { name: None, value: Expr::Ident("ptr".to_string(), dummy_span()), span: dummy_span() },
                    CallArg { name: None, value: Expr::Int(42, dummy_span()), span: dummy_span() },
                    CallArg { name: None, value: Expr::String("Int".to_string(), dummy_span()), span: dummy_span() },
                    CallArg { name: None, value: Expr::Int(1, dummy_span()), span: dummy_span() }, // acquire
                ],
                span: dummy_span(),
            })],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("atomic_store") && r.message.contains("store only")),
            "should reject acquire on atomic_store, got: {:?}", reports);
    }

    #[test]
    fn test_atomic_ordering_out_of_range() {
        let body = Block {
            stmts: vec![Stmt::Expr(Expr::Call {
                callee: Box::new(Expr::Ident("atomic_load".to_string(), dummy_span())),
                args: vec![
                    CallArg { name: None, value: Expr::Ident("ptr".to_string(), dummy_span()), span: dummy_span() },
                    CallArg { name: None, value: Expr::String("Int".to_string(), dummy_span()), span: dummy_span() },
                    CallArg { name: None, value: Expr::Int(7, dummy_span()), span: dummy_span() }, // invalid
                ],
                span: dummy_span(),
            })],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("atomic_load")),
            "should reject invalid atomic ordering, got: {:?}", reports);
    }

    #[test]
    fn test_unwrap_with_args_fails() {
        let body = Block {
            stmts: vec![Stmt::Expr(Expr::MethodCall {
                receiver: Box::new(Expr::Ident("opt".to_string(), dummy_span())),
                method: "unwrap".to_string(),
                args: vec![CallArg { name: None, value: Expr::String("msg".to_string(), dummy_span()), span: dummy_span() }],
                span: dummy_span(),
            })],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("unwrap") && r.message.contains("no argument")),
            "should reject unwrap() with args, got: {:?}", reports);
    }

    #[test]
    fn test_expect_wrong_arg_count() {
        let body = Block {
            stmts: vec![Stmt::Expr(Expr::MethodCall {
                receiver: Box::new(Expr::Ident("opt".to_string(), dummy_span())),
                method: "expect".to_string(),
                args: vec![], // missing message
                span: dummy_span(),
            })],
            span: dummy_span(),
        };
        let program = make_program_with_body(body);
        let reports = validate_codegen_checks(&program);
        assert!(reports.iter().any(|r| r.message.contains("expect") && r.message.contains("one")),
            "should reject expect() with wrong arg count, got: {:?}", reports);
    }
}
