//! KAIN Type System - Rust-like with effect tracking

use crate::ast::*;
use crate::diagnostic_registry::DiagnosticCode;
use crate::diagnostics::SpanMapper;
use crate::effects::{check_effect_call, Effect, EffectSet};
use crate::error::{
    CompilerPhase, DiagnosticReport, DiagnosticSemanticPacket, ErrorKind, KainError, KainResult,
};
use crate::lexer::Lexer;
use crate::low_level_abi::{default_c_abi_policy, CAbiPolicy};
use crate::module_resolution::{
    resolve_filesystem_module_file_with_context, resolve_stdlib_module_file,
    FilesystemModuleResolutionContext,
};
use crate::parser::Parser;
use crate::span::Span;
use crate::stdlib::StdLib;
use kain_actor::{
    validate_actor_definition, ActorDefinition, ActorHandlerSignature, ActorMethodSignature,
    ActorStateSlot, MessageParameter, MessageSignature,
};
use kain_ownership::{
    OwnershipPolicy, OwnershipRegionKind, COLLAPSE_KEYWORD, DECAY_KEYWORD, OBSERVE_KEYWORD,
    SHARE_KEYWORD,
};
use kain_resonate::{DampenWindow, ResonancePlan, ResonanceTarget};
use kain_semantic::enrich_report as enrich_semantic_report;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

const ATTR_SECTION: &str = "section";
const ATTR_LINK_NAME: &str = "link_name";
const ATTR_CALLCONV: &str = "callconv";
const ATTR_THREAD_LOCAL: &str = "thread_local";
const ATTR_PACKED: &str = "packed";
const ATTR_ALIGNED: &str = "aligned";
const ATTR_NAKED: &str = "naked";
const ATTR_INTERRUPT: &str = "interrupt";
const ATTR_MMIO: &str = "mmio";

/// Type-checked AST node
#[derive(Debug, Clone)]
pub struct TypedProgram {
    pub items: Vec<TypedItem>,
}

// Comptime blocks should be empty/removed by now if fully evaluated, or we check them
#[derive(Debug, Clone)]
pub struct TypedComptime {
    pub ast: Block,
}

#[derive(Debug, Clone)]
pub struct TypedConst {
    pub ast: Const,
    pub ty: ResolvedType,
}

#[derive(Debug, Clone)]
pub struct TypedUse {
    pub ast: Use,
}

#[derive(Debug, Clone)]
pub struct TypedImport {
    pub ast: Import,
}

#[derive(Debug, Clone)]
pub struct TypedMod {
    pub ast: Mod,
    pub items: Vec<TypedItem>,
}

#[derive(Debug, Clone)]
pub struct TypedEntangle {
    pub ast: EntangleDef,
    pub endpoint_type: ResolvedType,
    pub endpoint_type_name: String,
}

#[derive(Debug, Clone)]
pub enum TypedItem {
    Function(TypedFunction),
    Patch(TypedPatch),
    Law(TypedLaw),
    Axiom(TypedAxiom),
    Converge(TypedConverge),
    World(TypedWorld),
    Entangle(TypedEntangle),
    Orchestrate(TypedOrchestrate),
    Pulse(TypedPulse),
    Resonate(TypedResonate),
    Component(TypedComponent),
    Shader(TypedShader),
    Actor(TypedActor),
    Struct(TypedStruct),
    Enum(TypedEnum),
    Trait(TypedTrait),
    Comptime(TypedComptime),
    Const(TypedConst),
    Macro(TypedMacro),
    Use(TypedUse),
    Import(TypedImport),
    Mod(TypedMod),
    Impl(TypedImpl),
    Test(TypedTest),
    TypeAlias(TypedTypeAlias),
    MaterialGraph(crate::ast::MaterialGraphDef),
    MaterialFunction(crate::ast::MaterialFunctionDef),
    GraphEditor(crate::ast::GraphEditorDef),
    GraphRuntime(crate::ast::GraphRuntimeDef),
    StateMachine(crate::ast::StateMachineDef),
    AsyncTask(crate::ast::AsyncTaskDef),
    EditorModule(crate::ast::EditorModuleDef),
    GameplayTags(crate::ast::GameplayTagsNamespace),
    GameplayAbility(crate::ast::GameplayAbilityDef),
    GameplayEffect(crate::ast::GameplayEffectDef),
    GameplayCue(crate::ast::GameplayCueDef),
}

#[derive(Debug, Clone)]
pub struct TypedTest {
    pub ast: TestDef,
}

#[derive(Debug, Clone)]
pub struct TypedTypeAlias {
    pub ast: TypeAlias,
}

#[derive(Debug, Clone)]
pub struct TypedImpl {
    pub ast: Impl,
}

#[derive(Debug, Clone)]
pub struct TypedMacro {
    pub ast: MacroDef,
}

#[derive(Debug, Clone)]
pub struct TypedActor {
    pub ast: Actor,
    pub state_types: HashMap<String, ResolvedType>,
    pub actor_contract: ActorDefinition,
}

#[derive(Debug, Clone)]
pub struct TypedFunction {
    pub ast: Function,
    pub resolved_type: ResolvedType,
    pub effects: EffectSet,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PatchUndoMode {
    Reversible,
    BestEffort,
}

#[derive(Debug, Clone)]
pub struct TypedPatch {
    pub ast: PatchDef,
    pub resolved_type: ResolvedType,
    pub effects: EffectSet,
    pub mutation_paths: Vec<String>,
    pub undo_mode: PatchUndoMode,
}

#[derive(Debug, Clone)]
pub struct TypedLaw {
    pub ast: LawDef,
    pub resolved_type: ResolvedType,
    pub effects: EffectSet,
}

#[derive(Debug, Clone)]
pub struct TypedAxiom {
    pub ast: AxiomDef,
}

#[derive(Debug, Clone)]
pub struct TypedPulse {
    pub ast: PulseDef,
}

#[derive(Debug, Clone)]
pub struct TypedResonate {
    pub ast: ResonateDef,
    pub target_type: ResolvedType,
    pub target_type_name: String,
    pub plan: ResonancePlan,
}

#[derive(Debug, Clone)]
pub struct TypedConverge {
    pub ast: ConvergeDef,
    pub resolved_type: ResolvedType,
}

#[derive(Debug, Clone)]
pub struct TypedWorld {
    pub ast: WorldDef,
}

#[derive(Debug, Clone)]
pub struct OrchestrateStageDescriptor {
    pub runtime: OrchestrateStageRuntime,
    pub function: String,
    pub binding_name: String,
    pub selector: Option<OrchestrateSelector>,
    pub metadata: OrchestrateStageGraphMetadata,
}

#[derive(Debug, Clone)]
pub struct TypedOrchestrate {
    pub ast: OrchestrateDef,
    pub resolved_type: ResolvedType,
    pub stages: Vec<OrchestrateStageDescriptor>,
    pub graph: OrchestrateGraphPlan,
}

#[derive(Debug, Clone)]
pub struct TypedComponent {
    pub ast: Component,
    pub prop_types: HashMap<String, ResolvedType>,
    /// Maps state field name to its resolved type (Float → f64, String → String, Int → i64, Bool → i64).
    pub state_types: HashMap<String, ResolvedType>,
    /// Typed inline pulse definitions from the component body.
    pub pulse_types: Vec<TypedPulse>,
    /// Typed inline resonate definitions from the component body.
    pub resonate_types: Vec<TypedResonate>,
}

#[derive(Debug, Clone)]
pub struct TypedShader {
    pub ast: Shader,
    pub input_types: Vec<ResolvedType>,
    pub output_type: ResolvedType,
}

#[derive(Debug, Clone)]
pub struct TypedStruct {
    pub ast: Struct,
    pub field_types: HashMap<String, ResolvedType>,
}

#[derive(Debug, Clone)]
pub struct TypedEnum {
    pub ast: Enum,
    pub variant_payload_types: HashMap<String, Vec<ResolvedType>>,
}

#[derive(Debug, Clone)]
struct EnumVariantTypeInfo {
    payload_types: Vec<ResolvedType>,
    named_fields: Option<HashMap<String, ResolvedType>>,
}

#[derive(Debug, Clone)]
pub struct TypedTrait {
    pub ast: Trait,
}

/// Fully resolved type
#[derive(Debug, Clone, PartialEq)]
pub enum ResolvedType {
    Unit,
    Bool,
    Int(IntSize),
    Float(FloatSize),
    String,
    Char,
    Array(Box<ResolvedType>, usize),
    Slice(Box<ResolvedType>),
    Tuple(Vec<ResolvedType>),
    Option(Box<ResolvedType>),
    Result(Box<ResolvedType>, Box<ResolvedType>),
    Future(Box<ResolvedType>),
    Ref {
        mutable: bool,
        inner: Box<ResolvedType>,
    },
    Ptr {
        mutable: bool,
        inner: Box<ResolvedType>,
    },
    Function {
        params: Vec<ResolvedType>,
        ret: Box<ResolvedType>,
        effects: EffectSet,
    },
    Struct(String, HashMap<String, ResolvedType>),
    Enum(String, Vec<(String, ResolvedType)>),
    Generic(String),
    Never,
    Unknown,
}

#[derive(Clone, Copy)]
enum SelfhostConstructorKind {
    Array,
    String,
    Map,
    Set,
}

struct SelfhostConstructorSpec {
    name: &'static str,
    kind: SelfhostConstructorKind,
}

const SELFHOST_CONSTRUCTOR_SPECS: &[SelfhostConstructorSpec] = &[
    SelfhostConstructorSpec {
        name: "Vec__new_",
        kind: SelfhostConstructorKind::Array,
    },
    SelfhostConstructorSpec {
        name: "String__new_",
        kind: SelfhostConstructorKind::String,
    },
    SelfhostConstructorSpec {
        name: "HashMap__new_",
        kind: SelfhostConstructorKind::Map,
    },
    SelfhostConstructorSpec {
        name: "BTreeMap__new_",
        kind: SelfhostConstructorKind::Map,
    },
    SelfhostConstructorSpec {
        name: "std__collections__HashMap__new_",
        kind: SelfhostConstructorKind::Map,
    },
    SelfhostConstructorSpec {
        name: "HashSet__new_",
        kind: SelfhostConstructorKind::Set,
    },
    SelfhostConstructorSpec {
        name: "BTreeSet__new_",
        kind: SelfhostConstructorKind::Set,
    },
    SelfhostConstructorSpec {
        name: "std__collections__HashSet__new_",
        kind: SelfhostConstructorKind::Set,
    },
];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntSize {
    I8,
    I16,
    I32,
    I64,
    I128,
    Isize,
    U8,
    U16,
    U32,
    U64,
    U128,
    Usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FloatSize {
    F32,
    F64,
}

#[derive(Clone)]
struct SemanticContext {
    function_name: String,
    return_type: ResolvedType,
    effects: EffectSet,
}

#[derive(Debug, Clone)]
struct SymbolOrigin {
    span: Span,
    kind: &'static str,
}

fn context_allows_raw_memory_intrinsics(ctx: Option<&SemanticContext>) -> bool {
    ctx.is_some_and(|ctx| ctx.effects.effects.contains(&Effect::Unsafe))
}

fn recognized_metal_attribute(name: &str) -> bool {
    matches!(
        name,
        ATTR_SECTION
            | ATTR_LINK_NAME
            | ATTR_CALLCONV
            | ATTR_THREAD_LOCAL
            | ATTR_PACKED
            | ATTR_ALIGNED
            | ATTR_NAKED
            | ATTR_INTERRUPT
            | ATTR_MMIO
    )
}

fn function_is_extern_decl(function: &Function) -> bool {
    function
        .attributes
        .iter()
        .any(|attribute| attribute.name == "extern")
        && function.body.stmts.is_empty()
}

fn ensure_metal_attributes_are_unique(env: &TypeEnv, attributes: &[Attribute]) -> KainResult<()> {
    let mut seen = HashSet::new();
    for attribute in attributes {
        if !recognized_metal_attribute(&attribute.name) {
            continue;
        }
        if !seen.insert(attribute.name.clone()) {
            return Err(env.type_error(
                format!(
                    "duplicate @{name} attribute is not allowed",
                    name = attribute.name
                ),
                attribute.span,
            ));
        }
    }
    Ok(())
}

fn expr_as_attribute_string(expr: &Expr) -> Option<String> {
    match expr {
        Expr::String(value, _) => Some(value.clone()),
        Expr::Paren(inner, _) => expr_as_attribute_string(inner),
        _ => None,
    }
}

fn expr_as_attribute_int(expr: &Expr) -> Option<i64> {
    match expr {
        Expr::Int(value, _) => Some(*value),
        Expr::Paren(inner, _) => expr_as_attribute_int(inner),
        _ => None,
    }
}

fn expr_as_named_attribute_arg<'a>(expr: &'a Expr) -> Option<(&'a str, &'a Expr)> {
    match expr {
        Expr::Tuple(parts, _) if parts.len() == 2 => match &parts[0] {
            Expr::Ident(name, _) => Some((name.as_str(), &parts[1])),
            _ => None,
        },
        Expr::Paren(inner, _) => expr_as_named_attribute_arg(inner),
        _ => None,
    }
}

fn attribute_requires_zero_args(env: &TypeEnv, attribute: &Attribute) -> KainResult<()> {
    if attribute.args.is_empty() {
        return Ok(());
    }
    Err(env.type_error(
        format!("@{} does not take any arguments", attribute.name),
        attribute.span,
    ))
}

fn attribute_single_arg<'a>(env: &TypeEnv, attribute: &'a Attribute) -> KainResult<&'a Expr> {
    if attribute.args.len() != 1 {
        return Err(env.type_error(
            format!("@{} expects exactly one argument", attribute.name),
            attribute.span,
        ));
    }
    Ok(&attribute.args[0])
}

fn attribute_string_arg(env: &TypeEnv, attribute: &Attribute) -> KainResult<String> {
    let expr = attribute_single_arg(env, attribute)?;
    expr_as_attribute_string(expr).ok_or_else(|| {
        env.type_error(
            format!("@{} expects a string literal argument", attribute.name),
            expr.span(),
        )
    })
}

fn attribute_int_arg(env: &TypeEnv, attribute: &Attribute) -> KainResult<i64> {
    let expr = attribute_single_arg(env, attribute)?;
    expr_as_attribute_int(expr).ok_or_else(|| {
        env.type_error(
            format!("@{} expects an integer literal argument", attribute.name),
            expr.span(),
        )
    })
}

fn attribute_named_arg<'a>(
    env: &TypeEnv,
    attribute: &'a Attribute,
    expected_name: &str,
) -> KainResult<Option<&'a Expr>> {
    let mut found = None;
    for arg in &attribute.args {
        let Some((name, value)) = expr_as_named_attribute_arg(arg) else {
            continue;
        };
        if name != expected_name {
            continue;
        }
        if found.replace(value).is_some() {
            return Err(env.type_error(
                format!(
                    "@{} cannot repeat named argument '{}'",
                    attribute.name, expected_name
                ),
                arg.span(),
            ));
        }
    }
    Ok(found)
}

fn attribute_requires_named_args_only(env: &TypeEnv, attribute: &Attribute) -> KainResult<()> {
    for arg in &attribute.args {
        if expr_as_named_attribute_arg(arg).is_none() {
            return Err(env.type_error(
                format!("@{} expects only named arguments", attribute.name),
                arg.span(),
            ));
        }
    }
    Ok(())
}

fn validate_alignment_value(env: &TypeEnv, attribute: &Attribute, value: i64) -> KainResult<()> {
    // Proof: crates/core/z3/proofs/memory-public-aligned-attribute-requires-positive-power-of-two.yaml
    if value <= 0 || (value & (value - 1)) != 0 {
        return Err(env.type_error(
            format!(
                "@{} requires a positive power-of-two byte alignment",
                attribute.name
            ),
            attribute.span,
        ));
    }
    Ok(())
}

fn validate_zero_prologue_body(
    env: &TypeEnv,
    function: &Function,
    attribute_name: &str,
) -> KainResult<()> {
    for stmt in &function.body.stmts {
        let allowed = matches!(
            stmt,
            Stmt::Expr(Expr::InlineAsm { .. }) | Stmt::Return(None, _)
        ) || matches!(stmt, Stmt::Expr(Expr::Return(None, _)));
        if !allowed {
            let stmt_span = match stmt {
                Stmt::Let { span, .. }
                | Stmt::Defer { span, .. }
                | Stmt::Dispatch { span, .. }
                | Stmt::Subgroup { span, .. }
                | Stmt::Return(_, span)
                | Stmt::Break(_, span)
                | Stmt::Continue(span)
                | Stmt::For { span, .. }
                | Stmt::Fanout { span, .. }
                | Stmt::While { span, .. }
                | Stmt::Loop { span, .. } => *span,
                Stmt::Expr(expr) => expr.span(),
                Stmt::Item(item) => item_span(item),
            };
            return Err(env.type_error(
                format!(
                    "@{} functions may only contain inline asm statements and bare returns",
                    attribute_name
                ),
                stmt_span,
            ));
        }
    }
    Ok(())
}

fn validate_const_attributes(env: &TypeEnv, constant: &Const) -> KainResult<()> {
    ensure_metal_attributes_are_unique(env, &constant.attributes)?;
    for attribute in &constant.attributes {
        match attribute.name.as_str() {
            ATTR_SECTION | ATTR_LINK_NAME => {
                let value = attribute_string_arg(env, attribute)?;
                if value.trim().is_empty() {
                    return Err(env.type_error(
                        format!("@{} requires a non-empty string literal", attribute.name),
                        attribute.span,
                    ));
                }
            }
            ATTR_THREAD_LOCAL => {
                attribute_requires_zero_args(env, attribute)?;
            }
            ATTR_CALLCONV | ATTR_PACKED | ATTR_ALIGNED | ATTR_NAKED | ATTR_INTERRUPT
            | ATTR_MMIO => {
                return Err(env.type_error(
                    format!(
                        "@{} is not valid on const items; use it on the owning callable or struct instead",
                        attribute.name
                    ),
                    attribute.span,
                ));
            }
            _ => {}
        }
    }
    Ok(())
}

fn validate_function_attributes(
    env: &TypeEnv,
    function: &Function,
    return_type: &ResolvedType,
) -> KainResult<()> {
    ensure_metal_attributes_are_unique(env, &function.attributes)?;
    let has_naked = function
        .attributes
        .iter()
        .any(|attribute| attribute.name == ATTR_NAKED);
    let has_interrupt = function
        .attributes
        .iter()
        .any(|attribute| attribute.name == ATTR_INTERRUPT);

    if has_naked && has_interrupt {
        let span = function
            .attributes
            .iter()
            .find(|attribute| attribute.name == ATTR_INTERRUPT)
            .map(|attribute| attribute.span)
            .unwrap_or(function.span);
        return Err(env.type_error(
            "@naked and @interrupt cannot be combined on the same function",
            span,
        ));
    }

    for attribute in &function.attributes {
        match attribute.name.as_str() {
            ATTR_SECTION | ATTR_LINK_NAME => {
                let value = attribute_string_arg(env, attribute)?;
                if value.trim().is_empty() {
                    return Err(env.type_error(
                        format!("@{} requires a non-empty string literal", attribute.name),
                        attribute.span,
                    ));
                }
            }
            ATTR_CALLCONV => {
                let value = attribute_string_arg(env, attribute)?;
                match value.as_str() {
                    "c" | "sysv64" | "win64" | "fastcall" | "vectorcall" | "stdcall" => {}
                    _ => {
                        return Err(env.type_error(
                            format!(
                                "@callconv only supports \"c\", \"sysv64\", \"win64\", \"fastcall\", \"vectorcall\", or \"stdcall\"; found \"{value}\""
                            ),
                            attribute.span,
                        ));
                    }
                }
                if has_interrupt {
                    return Err(env.type_error(
                        "@interrupt chooses its own ABI contract and cannot be combined with @callconv",
                        attribute.span,
                    ));
                }
            }
            ATTR_NAKED => {
                attribute_requires_zero_args(env, attribute)?;
                if function_is_extern_decl(function) {
                    return Err(env.type_error(
                        "@naked cannot be applied to @extern declarations",
                        attribute.span,
                    ));
                }
                if !function.effects.contains(&Effect::Unsafe) {
                    return Err(env.type_error(
                        "@naked requires `with Unsafe` on the function",
                        attribute.span,
                    ));
                }
                if function.params.len() != 0 {
                    return Err(env.type_error(
                        "@naked functions currently require zero parameters",
                        attribute.span,
                    ));
                }
                if return_type != &ResolvedType::Unit {
                    return Err(env.type_error("@naked functions must return Unit", attribute.span));
                }
                if function
                    .effects
                    .iter()
                    .any(|effect| !matches!(effect, Effect::Pure | Effect::Unsafe))
                {
                    return Err(env.type_error(
                        "@naked functions cannot mix Async, IO, GPU, Reactive, Alloc, or Panic effects",
                        attribute.span,
                    ));
                }
                validate_zero_prologue_body(env, function, ATTR_NAKED)?;
            }
            ATTR_INTERRUPT => {
                if attribute.args.len() > 1 {
                    return Err(env.type_error(
                        "@interrupt expects zero or one string argument",
                        attribute.span,
                    ));
                }
                if let Some(arg) = attribute.args.first() {
                    let value = expr_as_attribute_string(arg).ok_or_else(|| {
                        env.type_error(
                            "@interrupt expects a string literal argument when one is provided",
                            arg.span(),
                        )
                    })?;
                    match value.as_str() {
                        "x86" | "x86_64" | "x86-interrupt" => {}
                        _ => {
                            return Err(env.type_error(
                                format!(
                                    "@interrupt only supports \"x86\", \"x86_64\", or \"x86-interrupt\" in this pass; found \"{value}\""
                                ),
                                arg.span(),
                            ));
                        }
                    }
                }
                if function_is_extern_decl(function) {
                    return Err(env.type_error(
                        "@interrupt cannot be applied to @extern declarations",
                        attribute.span,
                    ));
                }
                if !function.effects.contains(&Effect::Unsafe) {
                    return Err(env.type_error(
                        "@interrupt requires `with Unsafe` on the function",
                        attribute.span,
                    ));
                }
                if function.params.len() != 0 {
                    return Err(env.type_error(
                        "@interrupt handlers currently require zero parameters",
                        attribute.span,
                    ));
                }
                if return_type != &ResolvedType::Unit {
                    return Err(
                        env.type_error("@interrupt handlers must return Unit", attribute.span)
                    );
                }
                if function
                    .effects
                    .iter()
                    .any(|effect| !matches!(effect, Effect::Pure | Effect::Unsafe))
                {
                    return Err(env.type_error(
                        "@interrupt handlers cannot mix Async, IO, GPU, Reactive, Alloc, or Panic effects",
                        attribute.span,
                    ));
                }
                validate_zero_prologue_body(env, function, ATTR_INTERRUPT)?;
            }
            ATTR_THREAD_LOCAL | ATTR_PACKED | ATTR_ALIGNED | ATTR_MMIO => {
                return Err(env.type_error(
                    format!(
                        "@{} is not valid on functions; use it on authored const globals or structs instead",
                        attribute.name
                    ),
                    attribute.span,
                ));
            }
            _ => {}
        }
    }
    Ok(())
}

fn validate_struct_attributes(env: &TypeEnv, structure: &Struct) -> KainResult<()> {
    ensure_metal_attributes_are_unique(env, &structure.attributes)?;
    for attribute in &structure.attributes {
        match attribute.name.as_str() {
            ATTR_PACKED => {
                attribute_requires_zero_args(env, attribute)?;
            }
            ATTR_ALIGNED => {
                let value = attribute_int_arg(env, attribute)?;
                validate_alignment_value(env, attribute, value)?;
            }
            ATTR_MMIO => {
                attribute_requires_named_args_only(env, attribute)?;
                let mut seen_names = HashSet::new();
                for arg in &attribute.args {
                    let Some((name, _)) = expr_as_named_attribute_arg(arg) else {
                        continue;
                    };
                    if !seen_names.insert(name.to_string()) {
                        return Err(env.type_error(
                            format!("@mmio cannot repeat named argument '{name}'"),
                            arg.span(),
                        ));
                    }
                    match name {
                        "base" | "stride" | "endian" | "access" | "barrier" => {}
                        _ => {
                            return Err(env.type_error(
                                format!(
                                    "@mmio only supports named arguments `base`, `stride`, `endian`, `access`, and `barrier`; found `{name}`"
                                ),
                                arg.span(),
                            ));
                        }
                    }
                }
                if let Some(base) = attribute_named_arg(env, attribute, "base")? {
                    let Some(value) = expr_as_attribute_int(base) else {
                        return Err(env.type_error(
                            "@mmio(base: ...) expects an integer literal",
                            base.span(),
                        ));
                    };
                    if value < 0 {
                        return Err(env.type_error(
                            "@mmio(base: ...) requires a non-negative base address",
                            base.span(),
                        ));
                    }
                }
                if let Some(stride) = attribute_named_arg(env, attribute, "stride")? {
                    let Some(value) = expr_as_attribute_int(stride) else {
                        return Err(env.type_error(
                            "@mmio(stride: ...) expects an integer literal",
                            stride.span(),
                        ));
                    };
                    if value <= 0 {
                        return Err(env.type_error(
                            "@mmio(stride: ...) requires a positive byte stride",
                            stride.span(),
                        ));
                    }
                }
                if let Some(endian) = attribute_named_arg(env, attribute, "endian")? {
                    let Some(value) = expr_as_attribute_string(endian) else {
                        return Err(env.type_error(
                            "@mmio(endian: ...) expects a string literal",
                            endian.span(),
                        ));
                    };
                    if !matches!(value.as_str(), "native" | "little" | "big") {
                        return Err(env.type_error(
                            "@mmio endian must be \"native\", \"little\", or \"big\"",
                            endian.span(),
                        ));
                    }
                }
                if let Some(access) = attribute_named_arg(env, attribute, "access")? {
                    let Some(value) = expr_as_attribute_string(access) else {
                        return Err(env.type_error(
                            "@mmio(access: ...) expects a string literal",
                            access.span(),
                        ));
                    };
                    if !matches!(value.as_str(), "rw" | "ro" | "wo" | "w1c") {
                        return Err(env.type_error(
                            "@mmio access must be \"rw\", \"ro\", \"wo\", or \"w1c\"",
                            access.span(),
                        ));
                    }
                }
                if let Some(barrier) = attribute_named_arg(env, attribute, "barrier")? {
                    let Some(value) = expr_as_attribute_string(barrier) else {
                        return Err(env.type_error(
                            "@mmio(barrier: ...) expects a string literal",
                            barrier.span(),
                        ));
                    };
                    if !matches!(value.as_str(), "none" | "acquire" | "release" | "seq_cst") {
                        return Err(env.type_error(
                            "@mmio barrier must be \"none\", \"acquire\", \"release\", or \"seq_cst\"",
                            barrier.span(),
                        ));
                    }
                }
            }
            ATTR_SECTION | ATTR_LINK_NAME | ATTR_CALLCONV | ATTR_THREAD_LOCAL | ATTR_NAKED
            | ATTR_INTERRUPT => {
                return Err(env.type_error(
                    format!(
                        "@{} is not valid on structs; use it on the authored callable or const global instead",
                        attribute.name
                    ),
                    attribute.span,
                ));
            }
            _ => {}
        }
    }
    Ok(())
}

fn resolved_type_bit_width(ty: &ResolvedType, abi: &CAbiPolicy) -> Option<usize> {
    match peel_shared_refs(ty) {
        ResolvedType::Bool => Some(abi.bool_bits),
        ResolvedType::Char => Some(abi.char_bits),
        ResolvedType::Int(size) => Some(match size {
            IntSize::I8 | IntSize::U8 => 8,
            IntSize::I16 | IntSize::U16 => 16,
            IntSize::I32 | IntSize::U32 => 32,
            IntSize::I64 | IntSize::U64 => 64,
            IntSize::I128 | IntSize::U128 => 128,
            IntSize::Isize | IntSize::Usize => abi.pointer_bits,
        }),
        ResolvedType::Float(size) => Some(match size {
            FloatSize::F32 => 32,
            FloatSize::F64 => 64,
        }),
        ResolvedType::Ref { .. } | ResolvedType::Ptr { .. } => Some(abi.pointer_bits),
        _ => None,
    }
}

fn resolved_type_byte_width(ty: &ResolvedType, abi: &CAbiPolicy) -> Option<i64> {
    resolved_type_bit_width(ty, abi).map(|bits| (bits as i64 + 7) / 8)
}

fn resolved_type_supports_bitcast(ty: &ResolvedType) -> bool {
    matches!(
        peel_shared_refs(ty),
        ResolvedType::Bool
            | ResolvedType::Char
            | ResolvedType::Int(_)
            | ResolvedType::Float(_)
            | ResolvedType::Ref { .. }
            | ResolvedType::Ptr { .. }
    )
}

fn resolved_type_is_pointer_like(ty: &ResolvedType) -> bool {
    matches!(
        peel_shared_refs(ty),
        ResolvedType::Ref { .. } | ResolvedType::Ptr { .. }
    )
}

fn resolved_type_is_float_like(ty: &ResolvedType) -> bool {
    matches!(peel_shared_refs(ty), ResolvedType::Float(_))
}

fn resolved_type_is_integer_like(ty: &ResolvedType) -> bool {
    matches!(
        peel_shared_refs(ty),
        ResolvedType::Bool | ResolvedType::Char | ResolvedType::Int(_)
    )
}

fn resolved_type_is_address_like(ty: &ResolvedType) -> bool {
    resolved_type_is_pointer_like(ty) || matches!(peel_shared_refs(ty), ResolvedType::Int(_))
}

fn resolved_type_supports_inline_asm_operand(ty: &ResolvedType) -> bool {
    matches!(
        peel_shared_refs(ty),
        ResolvedType::Bool
            | ResolvedType::Char
            | ResolvedType::Int(_)
            | ResolvedType::Ref { .. }
            | ResolvedType::Ptr { .. }
    )
}

fn ensure_bitcast_compatible(
    env: &TypeEnv<'_>,
    source_ty: &ResolvedType,
    target_ty: &ResolvedType,
    span: Span,
) -> KainResult<()> {
    let abi = default_c_abi_policy();
    let source = peel_shared_refs(source_ty);
    let target = peel_shared_refs(target_ty);
    if !resolved_type_supports_bitcast(source) || !resolved_type_supports_bitcast(target) {
        return Err(env.type_error(
            format!(
                "bitcast requires scalar or pointer-like types, got {} and {}",
                describe_type(source),
                describe_type(target)
            ),
            span,
        ));
    }
    let Some(source_bits) = resolved_type_bit_width(source, abi) else {
        return Err(env.type_error(
            format!(
                "bitcast could not determine bit width for {}",
                describe_type(source)
            ),
            span,
        ));
    };
    let Some(target_bits) = resolved_type_bit_width(target, abi) else {
        return Err(env.type_error(
            format!(
                "bitcast could not determine bit width for {}",
                describe_type(target)
            ),
            span,
        ));
    };
    if source_bits != target_bits {
        return Err(env.type_error(
            format!(
                "bitcast requires equal-width source and target types, got {}-bit {} and {}-bit {}",
                source_bits,
                describe_type(source),
                target_bits,
                describe_type(target)
            ),
            span,
        ));
    }
    let pointer_float_mix = (resolved_type_is_pointer_like(source)
        && resolved_type_is_float_like(target))
        || (resolved_type_is_float_like(source) && resolved_type_is_pointer_like(target));
    if pointer_float_mix {
        return Err(env.type_error(
            format!(
                "bitcast does not support direct pointer/float reinterprets between {} and {}",
                describe_type(source),
                describe_type(target)
            ),
            span,
        ));
    }
    let allowed_mix = resolved_type_is_integer_like(source)
        || resolved_type_is_integer_like(target)
        || (resolved_type_is_pointer_like(source) && resolved_type_is_pointer_like(target))
        || (resolved_type_is_float_like(source) && resolved_type_is_float_like(target));
    if !allowed_mix {
        return Err(env.type_error(
            format!(
                "bitcast only supports integer/float reinterprets, integer/pointer reinterprets, or pointer/pointer reinterprets; got {} and {}",
                describe_type(source),
                describe_type(target)
            ),
            span,
        ));
    }
    Ok(())
}

/// Type environment for checking
#[derive(Clone)]
pub struct TypeEnv<'a> {
    scopes: Vec<HashMap<String, ResolvedType>>,
    moved_scopes: Vec<HashSet<String>>,
    types: HashMap<String, ResolvedType>,
    type_origins: HashMap<String, SymbolOrigin>,
    trait_origins: HashMap<String, SymbolOrigin>,
    globals: HashMap<String, ResolvedType>,
    global_origins: HashMap<String, SymbolOrigin>,
    moved_globals: HashSet<String>,
    methods: HashMap<String, HashMap<String, ResolvedType>>,
    method_origins: HashMap<(String, String), SymbolOrigin>,
    enum_variants: HashMap<String, HashMap<String, EnumVariantTypeInfo>>,
    actor_contracts: HashMap<String, ActorDefinition>,
    loaded_stdlib_modules: HashSet<String>,
    stdlib_registration_depth: usize,
    entangle_endpoints: HashSet<String>,
    shared_region_depth: usize,
    fanout_depth: usize,
    in_converge: bool,
    in_entangle: bool,
    in_patch: bool,
    in_world: bool,
    in_comptime: bool,
    /// True when type-checking inside an `orchestrate` block body.
    /// Enables `with GPU` (no Unsafe) for dispatch statements.
    in_orchestrate: bool,
    relaxed_checks: bool,
    span_mapper: &'a SpanMapper,
    filename: &'a str,
}

fn extend_accumulated_errors(out: &mut Vec<KainError>, error: KainError) {
    match error {
        KainError::Multi(errors) => {
            for error in errors {
                extend_accumulated_errors(out, error);
            }
        }
        other => out.push(other),
    }
}

fn finish_accumulated<T>(errors: Vec<KainError>, value: T) -> KainResult<T> {
    if errors.is_empty() {
        Ok(value)
    } else {
        Err(KainError::multi(errors))
    }
}

impl<'a> TypeEnv<'a> {
    pub fn new(span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        let mut env = Self {
            scopes: vec![HashMap::new()],
            moved_scopes: vec![HashSet::new()],
            types: HashMap::new(),
            type_origins: HashMap::new(),
            trait_origins: HashMap::new(),
            globals: HashMap::new(),
            global_origins: HashMap::new(),
            moved_globals: HashSet::new(),
            methods: HashMap::new(),
            method_origins: HashMap::new(),
            enum_variants: HashMap::new(),
            actor_contracts: HashMap::new(),
            loaded_stdlib_modules: HashSet::new(),
            stdlib_registration_depth: 0,
            entangle_endpoints: HashSet::new(),
            shared_region_depth: 0,
            fanout_depth: 0,
            in_converge: false,
            in_entangle: false,
            in_patch: false,
            in_world: false,
            in_comptime: false,
            in_orchestrate: false,
            relaxed_checks: false,
            span_mapper,
            filename,
        };
        // Built-in types
        env.types
            .insert("Int".into(), ResolvedType::Int(IntSize::I64));
        env.types
            .insert("UInt".into(), ResolvedType::Int(IntSize::U64));
        for (name, size) in [
            ("I8", IntSize::I8),
            ("I16", IntSize::I16),
            ("I32", IntSize::I32),
            ("I64", IntSize::I64),
            ("I128", IntSize::I128),
            ("ISize", IntSize::Isize),
            ("U8", IntSize::U8),
            ("U16", IntSize::U16),
            ("U32", IntSize::U32),
            ("U64", IntSize::U64),
            ("U128", IntSize::U128),
            ("USize", IntSize::Usize),
            ("i8", IntSize::I8),
            ("i16", IntSize::I16),
            ("i32", IntSize::I32),
            ("i64", IntSize::I64),
            ("i128", IntSize::I128),
            ("isize", IntSize::Isize),
            ("u8", IntSize::U8),
            ("u16", IntSize::U16),
            ("u32", IntSize::U32),
            ("u64", IntSize::U64),
            ("u128", IntSize::U128),
            ("usize", IntSize::Usize),
        ] {
            env.types.insert(name.to_string(), ResolvedType::Int(size));
        }
        env.types
            .insert("Float".into(), ResolvedType::Float(FloatSize::F64));
        env.types.insert("Void".into(), ResolvedType::Unit);
        env.types.insert("Bool".into(), ResolvedType::Bool);
        env.types.insert("Char".into(), ResolvedType::Char);
        env.types.insert("String".into(), ResolvedType::String);
        env.types.insert("Map".into(), selfhost_map_type());
        env.types.insert("Set".into(), selfhost_set_type());
        env.types.insert(
            "CommandRunResult".into(),
            ResolvedType::Struct(
                "CommandRunResult".into(),
                HashMap::from([
                    ("program".into(), ResolvedType::String),
                    ("workdir".into(), ResolvedType::String),
                    ("args".into(), dynamic_array_type(ResolvedType::String)),
                    ("stdout".into(), ResolvedType::String),
                    ("stderr".into(), ResolvedType::String),
                    ("status".into(), ResolvedType::Int(IntSize::I64)),
                    ("success".into(), ResolvedType::Bool),
                ]),
            ),
        );
        env.types.insert("FsError".into(), fs_error_type());
        env.types.insert("FsMetadata".into(), fs_metadata_type());
        env.types.insert("FsDirEntry".into(), fs_dir_entry_type());
        env.types.insert("FsChunk".into(), fs_chunk_type());
        env.types
            .insert("FsWatchEvent".into(), fs_watch_event_type());
        env.types
            .insert("FsJournalEntry".into(), fs_journal_entry_type());
        env.types.insert(
            "Vec2".into(),
            ResolvedType::Tuple(vec![
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
            ]),
        );
        env.types.insert(
            "Vec3".into(),
            ResolvedType::Tuple(vec![
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
            ]),
        );
        env.types.insert(
            "Vec4".into(),
            ResolvedType::Tuple(vec![
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
                ResolvedType::Float(FloatSize::F32),
            ]),
        );
        env.define_global(
            "vec2".into(),
            ResolvedType::Function {
                params: vec![
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                ],
                ret: Box::new(
                    env.types
                        .get("Vec2")
                        .cloned()
                        .unwrap_or(ResolvedType::Unknown),
                ),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "vec3".into(),
            ResolvedType::Function {
                params: vec![
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                ],
                ret: Box::new(
                    env.types
                        .get("Vec3")
                        .cloned()
                        .unwrap_or(ResolvedType::Unknown),
                ),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "vec4".into(),
            ResolvedType::Function {
                params: vec![
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                    ResolvedType::Float(FloatSize::F32),
                ],
                ret: Box::new(
                    env.types
                        .get("Vec4")
                        .cloned()
                        .unwrap_or(ResolvedType::Unknown),
                ),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "dispatch_thread_id".into(),
            ResolvedType::Struct(
                "DispatchThreadId".into(),
                HashMap::from([
                    ("x".into(), ResolvedType::Int(IntSize::I64)),
                    ("y".into(), ResolvedType::Int(IntSize::I64)),
                    ("z".into(), ResolvedType::Int(IntSize::I64)),
                ]),
            ),
        );
        register_builtin_global_functions(&mut env);
        register_stdlib_registry_globals(&mut env);
        register_selfhost_constructor_globals(&mut env);
        register_selfhost_collection_methods(&mut env);
        register_selfhost_host_bridge(&mut env);
        env
    }

    fn with_scope<T>(&mut self, f: impl FnOnce(&mut Self) -> KainResult<T>) -> KainResult<T> {
        self.push_scope();
        let result = f(self);
        self.pop_scope();
        result
    }

    pub fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
        self.moved_scopes.push(HashSet::new());
    }

    pub fn pop_scope(&mut self) {
        self.scopes.pop();
        self.moved_scopes.pop();
    }

    pub fn define(&mut self, name: String, ty: ResolvedType) {
        if let Some(moved) = self.moved_scopes.last_mut() {
            moved.remove(&name);
        }
        if let Some(scope) = self.scopes.last_mut() {
            scope.insert(name, ty);
        }
    }

    pub fn define_global(&mut self, name: String, ty: ResolvedType) {
        self.moved_globals.remove(&name);
        self.globals.insert(name, ty);
    }

    fn actor_contract(&self, name: &str) -> Option<&ActorDefinition> {
        self.actor_contracts.get(name)
    }

    pub fn define_method(&mut self, type_name: String, method_name: String, ty: ResolvedType) {
        self.methods
            .entry(type_name)
            .or_default()
            .insert(method_name, ty);
    }

    fn define_type_user(
        &mut self,
        name: String,
        ty: ResolvedType,
        span: Span,
        kind: &'static str,
    ) -> KainResult<()> {
        self.validate_user_symbol_collision(
            &name,
            span,
            kind,
            &self.type_origins,
            self.types.contains_key(&name),
            "type",
            DiagnosticCode::TypeDuplicateSymbol,
            DiagnosticCode::TypeShadowedBuiltin,
            self.is_registering_stdlib() || self.is_stdlib_source_span(span),
        )?;
        self.types.insert(name.clone(), ty);
        self.type_origins.insert(name, SymbolOrigin { span, kind });
        Ok(())
    }

    fn define_trait_user(&mut self, name: String, span: Span) -> KainResult<()> {
        self.validate_user_symbol_collision(
            &name,
            span,
            "trait",
            &self.trait_origins,
            self.trait_origins.contains_key(&name),
            "trait",
            DiagnosticCode::TypeDuplicateSymbol,
            DiagnosticCode::TypeShadowedBuiltin,
            self.is_registering_stdlib() || self.is_stdlib_source_span(span),
        )?;
        self.trait_origins.insert(
            name,
            SymbolOrigin {
                span,
                kind: "trait",
            },
        );
        Ok(())
    }

    fn trait_exists(&self, name: &str) -> bool {
        self.trait_origins.contains_key(name)
            || name
                .rsplit("::")
                .next()
                .is_some_and(|last| self.trait_origins.contains_key(last))
    }

    fn define_global_user(
        &mut self,
        name: String,
        ty: ResolvedType,
        span: Span,
        kind: &'static str,
    ) -> KainResult<()> {
        self.validate_user_symbol_collision(
            &name,
            span,
            kind,
            &self.global_origins,
            self.globals.contains_key(&name),
            "global",
            DiagnosticCode::TypeDuplicateSymbol,
            DiagnosticCode::TypeShadowedBuiltin,
            self.is_registering_stdlib() || self.is_stdlib_source_span(span),
        )?;
        self.define_global(name.clone(), ty);
        self.global_origins
            .insert(name, SymbolOrigin { span, kind });
        Ok(())
    }

    fn define_method_user(
        &mut self,
        type_name: &str,
        method_name: String,
        ty: ResolvedType,
        span: Span,
        kind: &'static str,
    ) -> KainResult<()> {
        let key = (type_name.to_string(), method_name.clone());
        if let Some(existing) = self.method_origins.get(&key) {
            if existing.span != span || existing.kind != kind {
                return Err(self.duplicate_symbol_error(
                    &method_name,
                    span,
                    kind,
                    existing,
                    "method",
                    DiagnosticCode::TypeDuplicateSymbol,
                ));
            }
        }
        let already_defined = self
            .methods
            .get(type_name)
            .and_then(|methods| methods.get(&method_name))
            .is_some();
        if already_defined && !self.method_origins.contains_key(&key) {
            return Err(self.shadow_builtin_symbol_error(
                &method_name,
                span,
                kind,
                "existing method slot",
                DiagnosticCode::TypeShadowedBuiltin,
            ));
        }
        self.define_method(type_name.to_string(), method_name.clone(), ty);
        self.method_origins.insert(key, SymbolOrigin { span, kind });
        Ok(())
    }

    fn validate_user_symbol_collision(
        &self,
        name: &str,
        span: Span,
        kind: &'static str,
        origins: &HashMap<String, SymbolOrigin>,
        already_defined: bool,
        namespace: &'static str,
        duplicate_code: DiagnosticCode,
        shadow_code: DiagnosticCode,
        allow_originless_shadow: bool,
    ) -> KainResult<()> {
        if let Some(existing) = origins.get(name) {
            if !self.same_symbol_declaration(existing, span, kind) {
                return Err(self.duplicate_symbol_error(
                    name,
                    span,
                    kind,
                    existing,
                    namespace,
                    duplicate_code,
                ));
            }
            return Ok(());
        }
        if already_defined && allow_originless_shadow {
            return Ok(());
        }
        if already_defined {
            return Err(self.shadow_builtin_symbol_error(name, span, kind, namespace, shadow_code));
        }
        Ok(())
    }

    fn same_symbol_declaration(
        &self,
        existing: &SymbolOrigin,
        span: Span,
        kind: &'static str,
    ) -> bool {
        if existing.kind != kind {
            return false;
        }
        if existing.span == span {
            return true;
        }
        existing.span.start == span.start
            && self.span_mapper.span_origin_file(existing.span)
                == self.span_mapper.span_origin_file(span)
    }

    fn is_stdlib_source_span(&self, span: Span) -> bool {
        self.span_mapper
            .span_origin_file(span)
            .is_some_and(is_stdlib_source_file)
            || is_stdlib_source_file(self.filename)
    }

    fn push_stdlib_registration(&mut self) {
        self.stdlib_registration_depth += 1;
    }

    fn pop_stdlib_registration(&mut self) {
        self.stdlib_registration_depth = self.stdlib_registration_depth.saturating_sub(1);
    }

    fn is_registering_stdlib(&self) -> bool {
        self.stdlib_registration_depth > 0
    }

    fn duplicate_symbol_error(
        &self,
        name: &str,
        span: Span,
        kind: &'static str,
        existing: &SymbolOrigin,
        namespace: &'static str,
        code: DiagnosticCode,
    ) -> KainError {
        let existing_loc = self
            .span_mapper
            .span_to_location(existing.span, self.filename);
        let report = self
            .type_report(
                code,
                format!(
                    "{kind} '{name}' collides with an existing {namespace} from {}",
                    existing.kind
                ),
                span,
                format!("redeclared {namespace} '{name}'"),
            )
            .label_from_source(
                self.span_mapper,
                existing.span,
                self.filename,
                format!(
                    "previous {} '{}' is here ({}:{}:{})",
                    existing.kind, name, existing_loc.file, existing_loc.line, existing_loc.col
                ),
            )
            .help(
                "Rename one of the declarations, or import the older symbol under an explicit alias.",
            );
        KainError::rich(report)
    }

    fn shadow_builtin_symbol_error(
        &self,
        name: &str,
        span: Span,
        kind: &'static str,
        namespace: &'static str,
        code: DiagnosticCode,
    ) -> KainError {
        let report = self
            .type_report(
                code,
                format!("{kind} '{name}' shadows an existing {namespace} symbol"),
                span,
                format!("shadowed {namespace} symbol '{name}'"),
            )
            .help("Pick a distinct name, or import the existing symbol with an alias to keep both visible.");
        KainError::rich(report)
    }

    pub fn lookup(&self, name: &str) -> Option<&ResolvedType> {
        for scope in self.scopes.iter().rev() {
            if let Some(ty) = scope.get(name) {
                return Some(ty);
            }
        }
        self.globals.get(name).or_else(|| self.types.get(name))
    }

    fn visible_symbol_names(&self) -> Vec<String> {
        let mut seen = HashSet::new();
        let mut names = Vec::new();

        for scope in self.scopes.iter().rev() {
            for name in scope.keys() {
                if seen.insert(name.clone()) {
                    names.push(name.clone());
                }
            }
        }
        for name in self.globals.keys() {
            if seen.insert(name.clone()) {
                names.push(name.clone());
            }
        }
        for name in self.types.keys() {
            if seen.insert(name.clone()) {
                names.push(name.clone());
            }
        }

        names
    }

    pub fn is_moved(&self, name: &str) -> bool {
        self.moved_scopes
            .iter()
            .rev()
            .any(|moved_scope| moved_scope.contains(name))
            || self.moved_globals.contains(name)
    }

    pub fn mark_moved(&mut self, name: &str) {
        for index in (0..self.scopes.len()).rev() {
            if self.scopes[index].contains_key(name) {
                if let Some(moved_scope) = self.moved_scopes.get_mut(index) {
                    moved_scope.insert(name.to_string());
                }
                return;
            }
        }
        if self.globals.contains_key(name) {
            self.moved_globals.insert(name.to_string());
        }
    }

    pub fn lookup_type(&self, name: &str) -> Option<&ResolvedType> {
        self.types.get(name)
    }

    pub fn lookup_method(&self, type_name: &str, method_name: &str) -> Option<&ResolvedType> {
        self.methods
            .get(type_name)
            .and_then(|methods| methods.get(method_name))
    }

    fn push_shared_region(&mut self) {
        self.shared_region_depth += 1;
    }

    fn pop_shared_region(&mut self) {
        self.shared_region_depth = self.shared_region_depth.saturating_sub(1);
    }

    fn in_shared_region(&self) -> bool {
        self.shared_region_depth > 0
    }

    fn push_fanout(&mut self) {
        self.fanout_depth += 1;
    }

    fn pop_fanout(&mut self) {
        self.fanout_depth = self.fanout_depth.saturating_sub(1);
    }

    fn in_fanout(&self) -> bool {
        self.fanout_depth > 0
    }

    pub fn lookup_enum_variant_fields(
        &self,
        enum_name: &str,
        variant_name: &str,
    ) -> Option<&Vec<ResolvedType>> {
        self.enum_variants
            .get(enum_name)
            .and_then(|variants| variants.get(variant_name))
            .map(|info| &info.payload_types)
    }

    pub fn lookup_enum_variant_named_field(
        &self,
        enum_name: &str,
        variant_name: &str,
        field_name: &str,
    ) -> Option<&ResolvedType> {
        self.enum_variants
            .get(enum_name)
            .and_then(|variants| variants.get(variant_name))
            .and_then(|info| info.named_fields.as_ref())
            .and_then(|fields| fields.get(field_name))
    }

    /// Create a type error with file:line:col format
    fn type_error(&self, message: impl Into<String>, span: Span) -> KainError {
        self.type_error_with_code(DiagnosticCode::TypeGeneric, message, span)
    }

    fn diagnostic_primary_text(&self, span: Span) -> String {
        let safe_start = span.start.min(self.span_mapper.source().len());
        let safe_end = span
            .end
            .min(self.span_mapper.source().len())
            .max(safe_start);
        let span = Span::new(safe_start, safe_end);
        self.span_mapper
            .source()
            .get(span.start..span.end)
            .unwrap_or_default()
            .trim()
            .to_string()
    }

    fn diagnostic_source_window(&self, span: Span) -> String {
        let (_, line_content) = self.span_mapper.span_to_line_info(span, self.filename);
        line_content.trim_end().to_string()
    }

    fn diagnostic_source_path(&self, span: Span) -> Option<String> {
        if let Some(origin) = self.span_mapper.span_origin_file(span) {
            if !Self::synthetic_filename(origin) {
                return Some(origin.to_string());
            }
        }
        if !Self::synthetic_filename(self.filename) {
            return Some(self.filename.to_string());
        }
        None
    }

    fn visible_import_names(&self) -> Vec<String> {
        let mut imports: Vec<String> = self.loaded_stdlib_modules.iter().cloned().collect();
        imports.sort();
        imports
    }

    fn nearest_scope_matches_for_text(&self, text: &str) -> Vec<(String, usize)> {
        if text.is_empty() {
            return Vec::new();
        }
        let mut matches: Vec<(String, usize)> = self
            .visible_symbol_names()
            .into_iter()
            .map(|name| {
                let distance = bounded_semantic_edit_distance(text, &name);
                (name, distance)
            })
            .filter(|(_, distance)| *distance <= 2)
            .collect();
        matches.sort_by(|left, right| left.1.cmp(&right.1).then_with(|| left.0.cmp(&right.0)));
        matches.truncate(3);
        matches
    }

    fn semantic_packet_for_span(
        &self,
        code: DiagnosticCode,
        span: Span,
        phase: CompilerPhase,
    ) -> DiagnosticSemanticPacket {
        let primary_text = self.diagnostic_primary_text(span);
        let mut packet = DiagnosticSemanticPacket::new(code, phase, primary_text.clone())
            .source_window(self.diagnostic_source_window(span))
            .visible_symbols(self.visible_symbol_names())
            .visible_imports(self.visible_import_names())
            .nearest_scope_matches(self.nearest_scope_matches_for_text(&primary_text));
        if let Some(path) = self.diagnostic_source_path(span) {
            packet = packet.source_path(path);
        }
        if self.in_converge {
            packet = packet.flag("in_converge_block", true);
        }
        if self.in_entangle {
            packet = packet.flag("in_entangle_block", true);
        }
        if self.in_patch {
            packet = packet.flag("in_patch_block", true);
        }
        if self.in_world {
            packet = packet.flag("in_world_block", true);
        }
        if self.in_comptime {
            packet = packet.flag("in_comptime_block", true);
        }
        packet
    }

    fn enrich_type_report(
        &self,
        report: DiagnosticReport,
        code: DiagnosticCode,
        span: Span,
    ) -> DiagnosticReport {
        let report = self.attach_type_source(report.phase(CompilerPhase::TypeChecking), span);
        enrich_semantic_report(
            report,
            &self.semantic_packet_for_span(code, span, CompilerPhase::TypeChecking),
        )
    }

    fn type_error_with_code(
        &self,
        code: DiagnosticCode,
        message: impl Into<String>,
        span: Span,
    ) -> KainError {
        KainError::rich(self.type_report(code, message, span, "typechecker stopped here"))
    }

    fn type_report(
        &self,
        code: DiagnosticCode,
        message: impl Into<String>,
        span: Span,
        label: impl Into<String>,
    ) -> DiagnosticReport {
        self.enrich_type_report(
            DiagnosticReport::new(ErrorKind::Type, code, message).primary_label(span, label),
            code,
            span,
        )
    }

    fn attach_type_source(&self, report: DiagnosticReport, span: Span) -> DiagnosticReport {
        let report = report.at_source(self.span_mapper, span, self.filename);
        if self.span_mapper.span_origin_file(span).is_some()
            || !Self::synthetic_filename(self.filename)
        {
            report
        } else {
            report.origin(self.filename)
        }
    }

    fn attach_effect_source(&self, error: KainError, span: Span) -> KainError {
        match error {
            KainError::Rich(report) => KainError::rich(
                self.attach_type_source(*report, span)
                    .phase(CompilerPhase::EffectChecking),
            ),
            other => other,
        }
    }

    fn importer_file_for_span(&self, span: Span) -> Option<PathBuf> {
        if let Some(origin) = self.span_mapper.span_origin_file(span) {
            if !Self::synthetic_filename(origin) {
                return Some(PathBuf::from(origin));
            }
        }
        if !Self::synthetic_filename(self.filename) {
            return Some(PathBuf::from(self.filename));
        }
        None
    }

    fn synthetic_filename(filename: &str) -> bool {
        filename.starts_with('<') && filename.ends_with('>')
    }
}

fn bounded_semantic_edit_distance(left: &str, right: &str) -> usize {
    if left == right {
        return 0;
    }
    let left_chars: Vec<char> = left.chars().collect();
    let right_chars: Vec<char> = right.chars().collect();
    let mut prev: Vec<usize> = (0..=right_chars.len()).collect();
    let mut curr = vec![0usize; right_chars.len() + 1];

    for (i, left_char) in left_chars.iter().enumerate() {
        curr[0] = i + 1;
        for (j, right_char) in right_chars.iter().enumerate() {
            let substitution_cost = usize::from(left_char != right_char);
            curr[j + 1] = (prev[j + 1] + 1)
                .min(curr[j] + 1)
                .min(prev[j] + substitution_cost);
        }
        std::mem::swap(&mut prev, &mut curr);
    }

    prev[right_chars.len()]
}

fn selfhost_map_type() -> ResolvedType {
    ResolvedType::Struct("Map".to_string(), HashMap::new())
}

fn selfhost_set_type() -> ResolvedType {
    ResolvedType::Struct("Set".to_string(), HashMap::new())
}

fn selfhost_bootstrap_lexer_result_type() -> ResolvedType {
    ResolvedType::Result(
        Box::new(dynamic_array_type(ResolvedType::Struct(
            "Token".to_string(),
            HashMap::new(),
        ))),
        Box::new(ResolvedType::Enum("KainError".to_string(), Vec::new())),
    )
}

fn selfhost_bootstrap_parser_result_type() -> ResolvedType {
    ResolvedType::Struct("Program".to_string(), HashMap::new())
}

fn selfhost_bootstrap_runtime_value_type() -> ResolvedType {
    ResolvedType::Enum("Value".to_string(), Vec::new())
}

fn selfhost_bootstrap_llvm_ir_type() -> ResolvedType {
    ResolvedType::String
}

fn selfhost_kain_error_type() -> ResolvedType {
    ResolvedType::Enum("KainError".to_string(), Vec::new())
}

fn selfhost_host_result_type(ok: ResolvedType) -> ResolvedType {
    ResolvedType::Result(Box::new(ok), Box::new(selfhost_kain_error_type()))
}

fn selfhost_path_buf_type() -> ResolvedType {
    ResolvedType::Struct("path::PathBuf".to_string(), HashMap::new())
}

fn selfhost_path_type() -> ResolvedType {
    ResolvedType::Struct("path::Path".to_string(), HashMap::new())
}

fn selfhost_dir_entry_type() -> ResolvedType {
    ResolvedType::Struct("DirEntry".to_string(), HashMap::new())
}

fn fs_error_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsError".to_string(),
        HashMap::from([
            ("kind".to_string(), ResolvedType::String),
            ("operation".to_string(), ResolvedType::String),
            ("path".to_string(), ResolvedType::String),
            ("other_path".to_string(), ResolvedType::String),
            ("message".to_string(), ResolvedType::String),
            ("raw_code".to_string(), ResolvedType::Int(IntSize::I64)),
        ]),
    )
}

fn fs_metadata_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsMetadata".to_string(),
        HashMap::from([
            ("file_type".to_string(), ResolvedType::String),
            ("len".to_string(), ResolvedType::Int(IntSize::I64)),
            ("readonly".to_string(), ResolvedType::Bool),
            (
                "created_millis".to_string(),
                ResolvedType::Int(IntSize::I64),
            ),
            (
                "modified_millis".to_string(),
                ResolvedType::Int(IntSize::I64),
            ),
            (
                "accessed_millis".to_string(),
                ResolvedType::Int(IntSize::I64),
            ),
        ]),
    )
}

fn fs_dir_entry_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsDirEntry".to_string(),
        HashMap::from([
            ("path".to_string(), ResolvedType::String),
            ("file_name".to_string(), ResolvedType::String),
            ("file_type".to_string(), ResolvedType::String),
            ("metadata".to_string(), fs_metadata_type()),
        ]),
    )
}

fn fs_chunk_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsChunk".to_string(),
        HashMap::from([
            ("index".to_string(), ResolvedType::Int(IntSize::I64)),
            ("offset".to_string(), ResolvedType::Int(IntSize::I64)),
            ("len".to_string(), ResolvedType::Int(IntSize::I64)),
            ("bytes".to_string(), fs_byte_array_type()),
        ]),
    )
}

fn fs_watch_event_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsWatchEvent".to_string(),
        HashMap::from([
            ("kind".to_string(), ResolvedType::String),
            ("path".to_string(), ResolvedType::String),
            ("before_len".to_string(), ResolvedType::Int(IntSize::I64)),
            ("after_len".to_string(), ResolvedType::Int(IntSize::I64)),
        ]),
    )
}

fn fs_journal_entry_type() -> ResolvedType {
    ResolvedType::Struct(
        "FsJournalEntry".to_string(),
        HashMap::from([
            ("operation".to_string(), ResolvedType::String),
            ("path".to_string(), ResolvedType::String),
            ("other_path".to_string(), ResolvedType::String),
            ("status".to_string(), ResolvedType::String),
            ("message".to_string(), ResolvedType::String),
        ]),
    )
}

fn fs_byte_array_type() -> ResolvedType {
    dynamic_array_type(ResolvedType::Int(IntSize::I64))
}

fn fs_result_type(ok: ResolvedType) -> ResolvedType {
    ResolvedType::Result(Box::new(ok), Box::new(fs_error_type()))
}

fn selfhost_duration_type() -> ResolvedType {
    ResolvedType::Struct("Duration".to_string(), HashMap::new())
}

const SELFHOST_PATH_BUF_TYPE_ALIASES: &[&str] = &["path::PathBuf", "PathBuf"];
const SELFHOST_PATH_TYPE_ALIASES: &[&str] = &["path::Path", "Path"];
const SELFHOST_DIR_ENTRY_TYPE_ALIASES: &[&str] = &["DirEntry", "fs::DirEntry"];
const SELFHOST_DURATION_TYPE_ALIASES: &[&str] = &["Duration", "time::Duration"];

fn selfhost_constructor_return_type(kind: SelfhostConstructorKind) -> ResolvedType {
    match kind {
        SelfhostConstructorKind::Array => dynamic_array_type(ResolvedType::Unknown),
        SelfhostConstructorKind::String => ResolvedType::String,
        SelfhostConstructorKind::Map => selfhost_map_type(),
        SelfhostConstructorKind::Set => selfhost_set_type(),
    }
}

fn selfhost_nullary_function_type(ret: ResolvedType) -> ResolvedType {
    ResolvedType::Function {
        params: Vec::new(),
        ret: Box::new(ret),
        effects: EffectSet::new(),
    }
}

fn builtin_function_type(params: Vec<ResolvedType>, ret: ResolvedType) -> ResolvedType {
    ResolvedType::Function {
        params,
        ret: Box::new(ret),
        effects: EffectSet::new(),
    }
}

fn lowered_impl_function_name(type_name: &str, method_name: &str) -> String {
    format!("{type_name}_{method_name}")
}

fn selfhost_static_impl_function_name(type_name: &str, method_name: &str) -> String {
    format!("{type_name}__{method_name}")
}

fn selfhost_enum_variant_alias_name(enum_name: &str, variant_name: &str) -> String {
    format!("{enum_name}__{variant_name}")
}

fn register_builtin_global_functions(env: &mut TypeEnv<'_>) {
    env.define_global(
        "print".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    env.define_global(
        "println".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    env.define_global(
        "eprint".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    env.define_global(
        "eprintln".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    env.define_global(
        "read_line".into(),
        builtin_function_type(vec![], ResolvedType::String),
    );
    env.define_global(
        "stdout_write".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    env.define_global(
        "stderr_write".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    env.define_global(
        "stdin_read_exact".into(),
        builtin_function_type(vec![ResolvedType::Int(IntSize::I64)], ResolvedType::String),
    );
    env.define_global(
        "to_int".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "substring".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::String,
        ),
    );
    env.define_global(
        "find_substring_from".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "dbg".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unknown),
    );
    env.define_global(
        "assert".into(),
        builtin_function_type(
            vec![ResolvedType::Bool, ResolvedType::String],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "panic".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Never),
    );
    env.define_global(
        "ask".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "ask_timeout".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "json_parse".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unknown),
    );
    env.define_global(
        "json_string".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::String),
    );
    env.define_global(
        "json_get".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "json_any_kind".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "json_any_to_int".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "json_any_to_float".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown],
            ResolvedType::Float(FloatSize::F64),
        ),
    );
    env.define_global(
        "json_any_to_string".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::String),
    );
    env.define_global(
        "json_get_string".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::String,
        ),
    );
    env.define_global(
        "json_get_int".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "json_get_float".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Float(FloatSize::F64),
        ),
    );
    env.define_global(
        "json_get_bool".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Bool,
        ),
    );
    env.define_global(
        "json_has".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Bool,
        ),
    );
    env.define_global(
        "json_object_new".into(),
        builtin_function_type(vec![], ResolvedType::Unknown),
    );
    env.define_global(
        "json_object_set".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "json_array_new".into(),
        builtin_function_type(vec![], dynamic_array_type(ResolvedType::Unknown)),
    );
    env.define_global(
        "json_array_push".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "json_array_len".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "json_array_get".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Int(IntSize::I64)],
            ResolvedType::Unknown,
        ),
    );
    for name in [
        "js_eval",
        "js_eval_raw",
        "js_import",
        "js_import_raw",
        "node_import",
        "js_require",
        "js_require_raw",
        "node_require",
        "js_getattr",
        "js_getattr_raw",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unknown),
        );
    }
    for name in ["js_call", "js_call_raw"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::Unknown, ResolvedType::Unknown],
                ResolvedType::Unknown,
            ),
        );
    }
    env.define_global(
        "py_call_raw_f64_trunc_i64".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Float(FloatSize::F64)],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "py_buffer_view".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unknown),
    );
    env.define_global(
        "py_region_begin".into(),
        builtin_function_type(vec![], ResolvedType::Unknown),
    );
    env.define_global(
        "py_region_end".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "py_region_import".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_getattr_raw".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::String,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_call_args".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_call_attr_args".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_call_raw_args".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_call_raw_attr".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_call_raw_f64_trunc_i64".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::Float(FloatSize::F64),
            ],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "py_region_call_attr_raw_f64_trunc_i64".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Float(FloatSize::F64),
            ],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "py_region_buffer_view".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "py_region_buffer_view_checksum37".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Unknown,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    for name in [
        "py_buffer_view_byte_length",
        "py_buffer_view_element_count",
        "py_buffer_view_element_size",
        "py_buffer_view_c_contiguous",
        "py_buffer_view_writable",
        "py_region_import_cache_hits",
        "py_region_import_cache_misses",
        "py_region_attr_cache_hits",
        "py_region_attr_cache_misses",
        "py_region_views_opened",
        "py_region_views_released",
        "py_region_call_count",
        "py_region_generic_call_count",
        "py_region_fast_call_count",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
        );
    }
    env.define_global(
        "py_buffer_view_release".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    for name in [
        "js_buffer_info",
        "js_buffer_bytes",
        "js_document_info",
        "js_document_text",
        "js_image_info",
        "js_image_text",
        "js_image_bytes",
        "js_image_buffer",
        "kain_shared_buffer_from_js",
        "kain_shared_image_from_js",
        "kain_shared_buffer_info",
        "kain_shared_buffer_bytes",
        "kain_shared_image_info",
        "kain_shared_image_bytes",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unknown),
        );
    }
    for name in [
        "kain_shared_buffer_byte_length",
        "kain_shared_buffer_element_count_value",
        "kain_shared_buffer_element_size",
        "kain_shared_buffer_zero_copy_flag",
        "kain_shared_buffer_shared_ownership",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
        );
    }
    env.define_global(
        "kain_shared_buffer_release".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unit),
    );
    env.define_global(
        "kain_shared_buffer_from_bytes".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::String,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "kain_shared_buffer_replace_bytes".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "kain_shared_image_from_bytes".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "kain_shared_image_replace_bytes".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unit,
        ),
    );
    for name in ["js_call_method", "js_call_method_raw", "node_package_run"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![
                    ResolvedType::Unknown,
                    ResolvedType::String,
                    ResolvedType::Unknown,
                ],
                ResolvedType::Unknown,
            ),
        );
    }
    env.define_global(
        "js_exec".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    env.define_global(
        "js_setattr".into(),
        builtin_function_type(
            vec![
                ResolvedType::Unknown,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "js_hasattr".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::String],
            ResolvedType::Bool,
        ),
    );
    for name in ["codebase_find_root", "codebase_read", "codebase_hash"] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
        );
    }
    for name in [
        "codebase_inspect",
        "codebase_scan",
        "codebase_read_json",
        "codebase_read_toml",
        "cargo_workspace",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::Unknown),
        );
    }
    for name in ["codebase_write"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, ResolvedType::String],
                ResolvedType::Unit,
            ),
        );
    }
    for name in ["codebase_write_json", "codebase_write_toml"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, ResolvedType::Unknown],
                ResolvedType::Unit,
            ),
        );
    }
    env.define_global(
        "codebase_delete".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    for name in [
        "codebase_run",
        "cargo_run",
        "cargo_import_crate",
        "python_run",
        "ts_compile",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![
                    ResolvedType::String,
                    ResolvedType::String,
                    ResolvedType::Unknown,
                ],
                ResolvedType::Unknown,
            ),
        );
    }
    env.define_global(
        "python_import".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "python_call".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "c_compile".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "c_load".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unknown),
    );
    env.define_global(
        "c_call".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "ts_import".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Unknown,
            ],
            ResolvedType::Unknown,
        ),
    );
    env.define_global(
        "read_file".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "write_file".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "file_exists".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
    );
    env.define_global(
        "env".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "cwd".into(),
        builtin_function_type(vec![], ResolvedType::String),
    );
    env.define_global(
        "args".into(),
        builtin_function_type(vec![], dynamic_array_type(ResolvedType::String)),
    );
    env.define_global(
        "command_run".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                dynamic_array_type(ResolvedType::String),
                ResolvedType::String,
            ],
            env.lookup_type("CommandRunResult")
                .cloned()
                .unwrap_or(ResolvedType::Unknown),
        ),
    );
    env.define_global(
        "read_dir".into(),
        builtin_function_type(
            vec![ResolvedType::String],
            dynamic_array_type(ResolvedType::String),
        ),
    );
    env.define_global(
        "create_dir_all".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    env.define_global(
        "copy_file".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "remove_file".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
    );
    env.define_global(
        "path_join".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::String,
        ),
    );
    env.define_global(
        "path_parent".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "path_file_name".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "path_extension".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "path_stem".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
    env.define_global(
        "path_is_file".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
    );
    env.define_global(
        "path_is_dir".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
    );
    register_filesystem_global_functions(env);
    env.define_global(
        "len".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "push".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "char_at".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::Int(IntSize::I64)],
            ResolvedType::String,
        ),
    );
    env.define_global(
        "byte_at".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::Int(IntSize::I64)],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "ord".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "chr".into(),
        builtin_function_type(vec![ResolvedType::Int(IntSize::I64)], ResolvedType::String),
    );
    env.define_global(
        "Box__new_".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Unknown),
    );
    env.define_global(
        "Some".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown],
            ResolvedType::Option(Box::new(ResolvedType::Unknown)),
        ),
    );
    env.define_global(
        "None".into(),
        selfhost_nullary_function_type(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
    );
    env.define_global(
        "__kain_bootstrap_lex_tokens".into(),
        builtin_function_type(
            vec![shared_ref_type(ResolvedType::String)],
            selfhost_bootstrap_lexer_result_type(),
        ),
    );
    env.define_global(
        "__kain_bootstrap_parse_source".into(),
        builtin_function_type(
            vec![
                dynamic_array_type(ResolvedType::Struct("Token".to_string(), HashMap::new())),
                ResolvedType::String,
            ],
            selfhost_bootstrap_parser_result_type(),
        ),
    );
    env.define_global(
        "__kain_bootstrap_run_program".into(),
        builtin_function_type(
            vec![ResolvedType::Struct("Program".to_string(), HashMap::new())],
            selfhost_bootstrap_runtime_value_type(),
        ),
    );
    env.define_global(
        "__kain_bootstrap_generate_llvm_ir".into(),
        builtin_function_type(
            vec![ResolvedType::Struct(
                "TypedProgram".to_string(),
                HashMap::new(),
            )],
            selfhost_bootstrap_llvm_ir_type(),
        ),
    );
    // CLI / system builtins used by kainc.kn and other selfhost scripts
    env.define_global(
        "args".into(),
        builtin_function_type(vec![], dynamic_array_type(ResolvedType::String)),
    );
    env.define_global(
        "exit".into(),
        builtin_function_type(vec![ResolvedType::Int(IntSize::I64)], ResolvedType::Never),
    );
    env.define_global(
        "str".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::String),
    );
    env.define_global(
        "int".into(),
        builtin_function_type(vec![ResolvedType::Unknown], ResolvedType::Int(IntSize::I64)),
    );
    env.define_global(
        "float".into(),
        builtin_function_type(
            vec![ResolvedType::Unknown],
            ResolvedType::Float(FloatSize::F64),
        ),
    );
}

fn register_stdlib_registry_globals(env: &mut TypeEnv<'_>) {
    let stdlib = StdLib::new();
    for (name, function) in stdlib.functions {
        if env.globals.contains_key(&name) {
            continue;
        }
        let params = function
            .params
            .iter()
            .map(|(_, ty)| stdlib_type_name_to_resolved(ty))
            .collect();
        let ret = stdlib_type_name_to_resolved(function.return_type);
        env.define_global(name, builtin_function_type(params, ret));
    }
}

fn stdlib_type_name_to_resolved(name: &str) -> ResolvedType {
    match name.trim() {
        "Unit" | "Void" => ResolvedType::Unit,
        "Bool" => ResolvedType::Bool,
        "Int" | "I64" => ResolvedType::Int(IntSize::I64),
        "UInt" | "U64" => ResolvedType::Int(IntSize::U64),
        "Float" | "F64" => ResolvedType::Float(FloatSize::F64),
        "F32" => ResolvedType::Float(FloatSize::F32),
        "String" => ResolvedType::String,
        "Char" => ResolvedType::Char,
        "Any" => ResolvedType::Unknown,
        "Never" => ResolvedType::Never,
        other => ResolvedType::Struct(other.to_string(), HashMap::new()),
    }
}

fn register_filesystem_global_functions(env: &mut TypeEnv<'_>) {
    for name in [
        "fs_read_text",
        "fs_temp_file",
        "fs_temp_dir",
        "fs_hash_file",
        "fs_path_parent",
        "fs_path_file_name",
        "fs_path_extension",
        "fs_path_stem",
        "fs_path_normalize",
        "fs_path_absolute",
        "fs_path_canonicalize",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
        );
    }

    for name in ["fs_exists", "fs_is_file", "fs_is_dir", "fs_is_symlink"] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
        );
    }

    for name in ["fs_write_text", "fs_append_text", "fs_atomic_write_text"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, ResolvedType::String],
                ResolvedType::Unit,
            ),
        );
    }

    env.define_global(
        "fs_read_bytes".into(),
        builtin_function_type(vec![ResolvedType::String], fs_byte_array_type()),
    );

    for name in ["fs_write_bytes", "fs_append_bytes", "fs_atomic_write_bytes"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, fs_byte_array_type()],
                ResolvedType::Unit,
            ),
        );
    }

    for name in [
        "fs_create_dir",
        "fs_create_dir_all",
        "fs_remove_file",
        "fs_remove_dir",
        "fs_remove_dir_all",
        "fs_remove_path",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
        );
    }

    for name in ["fs_copy_file", "fs_copy_path", "fs_move_path"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, ResolvedType::String],
                ResolvedType::Unit,
            ),
        );
    }
    env.define_global(
        "fs_path_join".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::String,
        ),
    );

    for name in ["fs_metadata", "fs_symlink_metadata"] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], fs_metadata_type()),
        );
    }

    for name in ["fs_read_dir", "fs_walk"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String],
                dynamic_array_type(fs_dir_entry_type()),
            ),
        );
    }

    for name in ["fs_read_dir_paths", "fs_glob"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String],
                dynamic_array_type(ResolvedType::String),
            ),
        );
    }

    env.define_global(
        "fs_try_read_text".into(),
        builtin_function_type(
            vec![ResolvedType::String],
            fs_result_type(ResolvedType::String),
        ),
    );
    env.define_global(
        "fs_try_read_bytes".into(),
        builtin_function_type(
            vec![ResolvedType::String],
            fs_result_type(fs_byte_array_type()),
        ),
    );

    for name in [
        "fs_try_write_text",
        "fs_try_append_text",
        "fs_try_create_dir",
        "fs_try_create_dir_all",
        "fs_try_remove_file",
        "fs_try_remove_dir",
        "fs_try_remove_dir_all",
        "fs_try_remove_path",
        "fs_try_copy_path",
        "fs_try_move_path",
        "fs_try_atomic_write_text",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::Unknown, ResolvedType::Unknown],
                fs_result_type(ResolvedType::Unit),
            ),
        );
    }

    for name in ["fs_try_write_bytes", "fs_try_append_bytes"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String, fs_byte_array_type()],
                fs_result_type(ResolvedType::Unit),
            ),
        );
    }

    env.define_global(
        "fs_try_copy_file".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            fs_result_type(ResolvedType::Int(IntSize::I64)),
        ),
    );

    for name in ["fs_try_metadata", "fs_try_symlink_metadata"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String],
                fs_result_type(fs_metadata_type()),
            ),
        );
    }

    for name in ["fs_try_read_dir", "fs_try_walk"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String],
                fs_result_type(dynamic_array_type(fs_dir_entry_type())),
            ),
        );
    }

    for name in ["fs_try_read_dir_paths", "fs_try_glob"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::String],
                fs_result_type(dynamic_array_type(ResolvedType::String)),
            ),
        );
    }

    env.define_global(
        "fs_try_hash_file".into(),
        builtin_function_type(
            vec![ResolvedType::String],
            fs_result_type(ResolvedType::String),
        ),
    );

    env.define_global(
        "fs_capability_describe".into(),
        builtin_function_type(vec![], ResolvedType::String),
    );
    env.define_global(
        "fs_capability_has".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
    );
    for name in ["fs_capability_grant", "fs_capability_revoke"] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
        );
    }
    env.define_global(
        "fs_sandbox_allow_host_paths".into(),
        builtin_function_type(vec![ResolvedType::Bool], ResolvedType::Unit),
    );
    env.define_global(
        "fs_mount".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::String,
            ],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "fs_unmount".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
    );
    env.define_global(
        "fs_resolve".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );

    for name in ["fs_read_text_range", "fs_read_bytes_range"] {
        let return_type = if name == "fs_read_text_range" {
            ResolvedType::String
        } else {
            fs_byte_array_type()
        };
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![
                    ResolvedType::String,
                    ResolvedType::Int(IntSize::I64),
                    ResolvedType::Int(IntSize::I64),
                ],
                return_type,
            ),
        );
    }
    env.define_global(
        "fs_write_text_at".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::String,
            ],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "fs_write_bytes_at".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
                fs_byte_array_type(),
            ],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "fs_stream_chunks".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::Int(IntSize::I64)],
            dynamic_array_type(fs_chunk_type()),
        ),
    );
    env.define_global(
        "fs_copy_file_streaming".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    for name in [
        "fs_read_bytes_hex",
        "fs_metadata_text",
        "fs_read_dir_paths_text",
        "fs_walk_paths_text",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
        );
    }
    env.define_global(
        "fs_read_byte_range_hex".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
            ],
            ResolvedType::String,
        ),
    );
    env.define_global(
        "fs_write_bytes_hex".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::String],
            ResolvedType::Unit,
        ),
    );
    env.define_global(
        "fs_write_bytes_hex_at".into(),
        builtin_function_type(
            vec![
                ResolvedType::String,
                ResolvedType::Int(IntSize::I64),
                ResolvedType::String,
            ],
            ResolvedType::Unit,
        ),
    );

    env.define_global(
        "fs_watch".into(),
        builtin_function_type(
            vec![ResolvedType::String, ResolvedType::Bool],
            ResolvedType::Int(IntSize::I64),
        ),
    );
    env.define_global(
        "fs_watch_poll".into(),
        builtin_function_type(
            vec![ResolvedType::Int(IntSize::I64)],
            dynamic_array_type(fs_watch_event_type()),
        ),
    );
    env.define_global(
        "fs_watch_close".into(),
        builtin_function_type(vec![ResolvedType::Int(IntSize::I64)], ResolvedType::Bool),
    );

    env.define_global(
        "fs_tx_begin".into(),
        builtin_function_type(vec![], ResolvedType::Int(IntSize::I64)),
    );
    for name in [
        "fs_tx_write_text",
        "fs_tx_append_text",
        "fs_tx_copy_path",
        "fs_tx_move_path",
    ] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![
                    ResolvedType::Int(IntSize::I64),
                    ResolvedType::String,
                    ResolvedType::String,
                ],
                ResolvedType::Unit,
            ),
        );
    }
    env.define_global(
        "fs_tx_remove_path".into(),
        builtin_function_type(
            vec![ResolvedType::Int(IntSize::I64), ResolvedType::String],
            ResolvedType::Unit,
        ),
    );
    for name in ["fs_tx_commit", "fs_tx_rollback"] {
        env.define_global(
            name.into(),
            builtin_function_type(
                vec![ResolvedType::Int(IntSize::I64)],
                dynamic_array_type(fs_journal_entry_type()),
            ),
        );
    }
}

fn method_has_receiver_param(method: &Function) -> bool {
    method
        .params
        .first()
        .is_some_and(|param| matches!(param.name.as_str(), "self" | "_self"))
}

fn module_scoped_name(module_path: &[String], item_name: &str) -> String {
    let mut parts = module_path.to_vec();
    parts.push(item_name.to_string());
    parts.join("__")
}

fn module_scoped_type_name(module_path: &[String], item_name: &str) -> String {
    let mut parts = module_path.to_vec();
    parts.push(item_name.to_string());
    parts.join("::")
}

fn register_selfhost_constructor_globals(env: &mut TypeEnv<'_>) {
    for spec in SELFHOST_CONSTRUCTOR_SPECS {
        env.define_global(
            spec.name.to_string(),
            selfhost_nullary_function_type(selfhost_constructor_return_type(spec.kind)),
        );
    }
}

fn register_selfhost_collection_methods(env: &mut TypeEnv<'_>) {
    let map_receiver = selfhost_map_type();
    let set_receiver = selfhost_set_type();
    env.define_method(
        "Map".to_string(),
        "get".to_string(),
        ResolvedType::Function {
            params: vec![map_receiver.clone(), ResolvedType::Unknown],
            ret: Box::new(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "iter".to_string(),
        ResolvedType::Function {
            params: vec![shared_ref_type(selfhost_map_type())],
            ret: Box::new(dynamic_array_type(ResolvedType::Tuple(vec![
                ResolvedType::String,
                ResolvedType::Unknown,
            ]))),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "insert".to_string(),
        ResolvedType::Function {
            params: vec![
                map_receiver.clone(),
                ResolvedType::Unknown,
                ResolvedType::Unknown,
            ],
            ret: Box::new(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "contains_key".to_string(),
        ResolvedType::Function {
            params: vec![map_receiver.clone(), ResolvedType::Unknown],
            ret: Box::new(ResolvedType::Bool),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "entry".to_string(),
        ResolvedType::Function {
            params: vec![map_receiver, ResolvedType::Unknown],
            ret: Box::new(ResolvedType::Unknown),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "len".to_string(),
        ResolvedType::Function {
            params: vec![shared_ref_type(selfhost_map_type())],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Map".to_string(),
        "is_empty".to_string(),
        ResolvedType::Function {
            params: vec![shared_ref_type(selfhost_map_type())],
            ret: Box::new(ResolvedType::Bool),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Set".to_string(),
        "insert".to_string(),
        ResolvedType::Function {
            params: vec![set_receiver.clone(), ResolvedType::Unknown],
            ret: Box::new(ResolvedType::Bool),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Set".to_string(),
        "contains".to_string(),
        ResolvedType::Function {
            params: vec![set_receiver.clone(), ResolvedType::Unknown],
            ret: Box::new(ResolvedType::Bool),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Set".to_string(),
        "iter".to_string(),
        ResolvedType::Function {
            params: vec![set_receiver],
            ret: Box::new(dynamic_array_type(ResolvedType::Unknown)),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Set".to_string(),
        "len".to_string(),
        ResolvedType::Function {
            params: vec![shared_ref_type(selfhost_set_type())],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::new(),
        },
    );
    env.define_method(
        "Set".to_string(),
        "is_empty".to_string(),
        ResolvedType::Function {
            params: vec![shared_ref_type(selfhost_set_type())],
            ret: Box::new(ResolvedType::Bool),
            effects: EffectSet::new(),
        },
    );
}

fn register_selfhost_host_bridge(env: &mut TypeEnv<'_>) {
    register_selfhost_host_bridge_types(env);
    register_selfhost_host_bridge_methods(env);
}

fn register_selfhost_host_bridge_types(env: &mut TypeEnv<'_>) {
    register_host_type_aliases(
        env,
        SELFHOST_PATH_BUF_TYPE_ALIASES,
        selfhost_path_buf_type(),
    );
    register_host_type_aliases(env, SELFHOST_PATH_TYPE_ALIASES, selfhost_path_type());
    register_host_type_aliases(
        env,
        SELFHOST_DIR_ENTRY_TYPE_ALIASES,
        selfhost_dir_entry_type(),
    );
    register_host_type_aliases(
        env,
        SELFHOST_DURATION_TYPE_ALIASES,
        selfhost_duration_type(),
    );
}

fn register_host_type_aliases(env: &mut TypeEnv<'_>, aliases: &[&str], ty: ResolvedType) {
    for alias in aliases {
        env.types.insert((*alias).to_string(), ty.clone());
    }
}

fn register_selfhost_host_bridge_methods(env: &mut TypeEnv<'_>) {
    register_host_path_methods(
        env,
        SELFHOST_PATH_BUF_TYPE_ALIASES,
        selfhost_path_buf_type(),
    );
    register_host_path_methods(env, SELFHOST_PATH_TYPE_ALIASES, selfhost_path_type());

    for alias in SELFHOST_DIR_ENTRY_TYPE_ALIASES {
        env.define_method(
            (*alias).to_string(),
            "path".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(selfhost_dir_entry_type())],
                ret: Box::new(selfhost_path_buf_type()),
                effects: EffectSet::new(),
            },
        );
    }
}

fn register_host_path_methods(env: &mut TypeEnv<'_>, aliases: &[&str], receiver: ResolvedType) {
    for alias in aliases {
        env.define_method(
            (*alias).to_string(),
            "join".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone()), ResolvedType::Unknown],
                ret: Box::new(selfhost_path_buf_type()),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "exists".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::Bool),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "is_dir".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::Bool),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "is_file".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::Bool),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "parent".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::Option(Box::new(selfhost_path_buf_type()))),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "to_path_buf".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(selfhost_path_buf_type()),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "to_string_lossy".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "display".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            },
        );
        env.define_method(
            (*alias).to_string(),
            "file_name".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(receiver.clone())],
                ret: Box::new(ResolvedType::Option(Box::new(ResolvedType::String))),
                effects: EffectSet::new(),
            },
        );
    }
}

fn infer_selfhost_host_call_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    callee_name: &str,
    args: &[CallArg],
    span: Span,
) -> Option<KainResult<ResolvedType>> {
    match callee_name {
        "std__env__var_" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__env__var_",
            &[ResolvedType::Unknown],
            selfhost_host_result_type(ResolvedType::String),
        )),
        "std__env__current_exe" | "std__env__current_dir" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            callee_name,
            &[],
            selfhost_host_result_type(selfhost_path_buf_type()),
        )),
        "std__env__set_current_dir" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__env__set_current_dir",
            &[ResolvedType::Unknown],
            selfhost_host_result_type(ResolvedType::Unit),
        )),
        "std__env__temp_dir" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__env__temp_dir",
            &[],
            selfhost_path_buf_type(),
        )),
        "std__env__set_var" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__env__set_var",
            &[ResolvedType::Unknown, ResolvedType::Unknown],
            ResolvedType::Unit,
        )),
        "std__env__remove_var" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__env__remove_var",
            &[ResolvedType::Unknown],
            ResolvedType::Unit,
        )),
        "std__fs__read_dir" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__fs__read_dir",
            &[ResolvedType::Unknown],
            selfhost_host_result_type(dynamic_array_type(selfhost_dir_entry_type())),
        )),
        "std__fs__read_to_string" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__fs__read_to_string",
            &[ResolvedType::Unknown],
            selfhost_host_result_type(ResolvedType::String),
        )),
        "std__fs__create_dir_all" | "std__fs__remove_dir_all" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            callee_name,
            &[ResolvedType::Unknown],
            selfhost_host_result_type(ResolvedType::Unit),
        )),
        "std__fs__write" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__fs__write",
            &[ResolvedType::Unknown, ResolvedType::Unknown],
            selfhost_host_result_type(ResolvedType::Unit),
        )),
        "std__iter__empty" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__iter__empty",
            &[],
            dynamic_array_type(ResolvedType::Tuple(vec![
                ResolvedType::String,
                ResolvedType::Enum("ResolvedType".to_string(), Vec::new()),
            ])),
        )),
        "std__mem__discriminant" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__mem__discriminant",
            &[ResolvedType::Unknown],
            ResolvedType::Int(IntSize::I64),
        )),
        "std__mem__take" => Some(infer_selfhost_mem_take_call(env, ctx, args, span)),
        "std__mem__replace" => Some(infer_selfhost_mem_replace_call(env, ctx, args, span)),
        "Duration__from_micros" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "Duration__from_micros",
            &[ResolvedType::Int(IntSize::I64)],
            selfhost_duration_type(),
        )),
        "std__thread__sleep" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__thread__sleep",
            &[ResolvedType::Unknown],
            ResolvedType::Unit,
        )),
        "std__thread__spawn_" => Some(infer_fixed_builtin_call(
            env,
            ctx,
            args,
            span,
            "std__thread__spawn_",
            &[ResolvedType::Unknown],
            ResolvedType::Unit,
        )),
        _ => None,
    }
}

fn infer_fixed_builtin_call(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
    name: &str,
    params: &[ResolvedType],
    ret: ResolvedType,
) -> KainResult<ResolvedType> {
    if args.len() != params.len() {
        return Err(env.type_error(
            format!(
                "{name} expects {} argument(s), found {}",
                params.len(),
                args.len()
            ),
            span,
        ));
    }
    for (param_ty, arg) in params.iter().zip(args.iter()) {
        let arg_ty = infer_expr_type_with_expected(env, &arg.value, ctx, Some(param_ty))?;
        ensure_type_compatible(env, param_ty, &arg_ty, arg.span, name)?;
    }
    Ok(ret)
}

fn infer_selfhost_mem_take_call(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(
            format!("std__mem__take expects 1 argument, found {}", args.len()),
            span,
        ));
    }
    let source_ty = infer_expr_type(env, &args[0].value, ctx)?;
    Ok(match source_ty {
        ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => *inner,
        other => other,
    })
}

fn infer_selfhost_mem_replace_call(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    if args.len() != 2 {
        return Err(env.type_error(
            format!(
                "std__mem__replace expects 2 arguments, found {}",
                args.len()
            ),
            span,
        ));
    }
    let target_ty = infer_expr_type(env, &args[0].value, ctx)?;
    let replaced_ty = match target_ty {
        ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => *inner,
        other => other,
    };
    let replacement_ty =
        infer_expr_type_with_expected(env, &args[1].value, ctx, Some(&replaced_ty))?;
    ensure_type_compatible(
        env,
        &replaced_ty,
        &replacement_ty,
        args[1].span,
        "std__mem__replace",
    )?;
    Ok(replaced_ty)
}

/// Main type checking entry point
pub fn check(
    program: &Program,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<TypedProgram> {
    check_with_extra_globals(
        program,
        span_mapper,
        filename,
        std::iter::empty::<(String, ResolvedType)>(),
    )
}

pub fn check_with_extra_globals<I>(
    program: &Program,
    span_mapper: &SpanMapper,
    filename: &str,
    extra_globals: I,
) -> KainResult<TypedProgram>
where
    I: IntoIterator<Item = (String, ResolvedType)>,
{
    let mut env = TypeEnv::new(span_mapper, filename);
    let mut errors = Vec::new();
    for (name, ty) in extra_globals {
        env.define_global(name, ty);
    }

    // First pass: Predeclare item-owned types so forward references resolve
    // against real type names instead of placeholder struct fallbacks.
    for item in &program.items {
        predeclare_item_types(&mut env, item);
    }

    // Second pass: Register types, globals, and methods against the
    // predeclared graph.
    let mut second_pass_ok = vec![false; program.items.len()];
    for (index, item) in program.items.iter().enumerate() {
        match register_item_types(&mut env, item) {
            Ok(()) => second_pass_ok[index] = true,
            Err(error) => extend_accumulated_errors(&mut errors, error),
        }
    }

    // Third pass: Refresh registrations now that every type shape is present.
    // This resolves recursive payloads like enums that reference structs
    // declared later in the same program.
    let mut third_pass_ok = vec![false; program.items.len()];
    for (index, item) in program.items.iter().enumerate() {
        if !second_pass_ok[index] {
            continue;
        }
        match register_item_types(&mut env, item) {
            Ok(()) => third_pass_ok[index] = true,
            Err(error) => extend_accumulated_errors(&mut errors, error),
        }
    }

    // Fourth pass: Type check all items.
    let mut typed_items = Vec::new();
    for (index, item) in program.items.iter().enumerate() {
        if !second_pass_ok[index] || !third_pass_ok[index] {
            continue;
        }
        if let Err(error) = check_item_into(&mut env, item, &mut typed_items) {
            extend_accumulated_errors(&mut errors, error);
        }
    }

    finish_accumulated(errors, TypedProgram { items: typed_items })
}

/// Type-check with relaxed rules for tree-walk interpretation.
/// Worlds may omit surfaces, converge lane signatures are warnings, and
/// orchestrate guard/residency checks are soft.
pub fn check_for_interpret(
    program: &Program,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<TypedProgram> {
    check_for_interpret_with_extra_globals(
        program,
        span_mapper,
        filename,
        std::iter::empty::<(String, ResolvedType)>(),
    )
}

pub fn check_for_interpret_with_extra_globals<I>(
    program: &Program,
    span_mapper: &SpanMapper,
    filename: &str,
    extra_globals: I,
) -> KainResult<TypedProgram>
where
    I: IntoIterator<Item = (String, ResolvedType)>,
{
    let mut env = TypeEnv::new(span_mapper, filename);
    env.relaxed_checks = true;
    let mut errors = Vec::new();
    for (name, ty) in extra_globals {
        env.define_global(name, ty);
    }

    for item in &program.items {
        predeclare_item_types(&mut env, item);
    }

    let mut second_pass_ok = vec![false; program.items.len()];
    for (index, item) in program.items.iter().enumerate() {
        match register_item_types(&mut env, item) {
            Ok(()) => second_pass_ok[index] = true,
            Err(error) => extend_accumulated_errors(&mut errors, error),
        }
    }

    let mut third_pass_ok = vec![false; program.items.len()];
    for (index, item) in program.items.iter().enumerate() {
        if !second_pass_ok[index] {
            continue;
        }
        match register_item_types(&mut env, item) {
            Ok(()) => third_pass_ok[index] = true,
            Err(error) => extend_accumulated_errors(&mut errors, error),
        }
    }

    let mut typed_items = Vec::new();
    for (index, item) in program.items.iter().enumerate() {
        if !second_pass_ok[index] || !third_pass_ok[index] {
            continue;
        }
        if let Err(error) = check_item_into(&mut env, item, &mut typed_items) {
            extend_accumulated_errors(&mut errors, error);
        }
    }

    finish_accumulated(errors, TypedProgram { items: typed_items })
}

fn predeclare_item_types(env: &mut TypeEnv, item: &Item) {
    match item {
        Item::Struct(s) => {
            predeclare_type_user(
                env,
                &s.name,
                ResolvedType::Struct(s.name.clone(), HashMap::new()),
                s.span,
                "struct",
            );
        }
        Item::Enum(e) => {
            predeclare_type_user(
                env,
                &e.name,
                ResolvedType::Enum(e.name.clone(), Vec::new()),
                e.span,
                "enum",
            );
            env.enum_variants.entry(e.name.clone()).or_default();
        }
        Item::World(world) => {
            predeclare_type_user(
                env,
                &world.name,
                ResolvedType::Struct(world.name.clone(), HashMap::new()),
                world.span,
                "world",
            );
        }
        Item::Component(component) => {
            predeclare_type_user(
                env,
                &component.name,
                ResolvedType::Struct(component.name.clone(), HashMap::new()),
                component.span,
                "component",
            );
        }
        Item::Actor(actor) => {
            predeclare_type_user(
                env,
                &actor.name,
                ResolvedType::Struct(actor.name.clone(), HashMap::new()),
                actor.span,
                "actor",
            );
        }
        Item::Trait(trait_def) => {
            let _ = env.define_trait_user(trait_def.name.clone(), trait_def.span);
        }
        Item::Mod(module) => {
            if let Some(children) = &module.inline {
                let module_path = vec![module.name.clone()];
                predeclare_inline_module_type_aliases(env, children, &module_path);
                for child in children {
                    predeclare_item_types(env, child);
                }
            }
        }
        _ => {}
    }
}

fn predeclare_type_user(
    env: &mut TypeEnv,
    name: &str,
    ty: ResolvedType,
    span: Span,
    kind: &'static str,
) {
    if env.types.contains_key(name) {
        return;
    }
    env.types.insert(name.to_string(), ty);
    env.type_origins
        .insert(name.to_string(), SymbolOrigin { span, kind });
}

fn is_stdlib_source_file(file: &str) -> bool {
    let normalized = file.replace('\\', "/");
    normalized.starts_with("stdlib/") || normalized.contains("/stdlib/")
}

fn register_item_types(env: &mut TypeEnv, item: &Item) -> KainResult<()> {
    match item {
        Item::Struct(s) => {
            validate_generic_constraints(env, &s.generics, s.where_clause.as_ref(), "struct")?;
            let mut fields = HashMap::new();
            for f in &s.fields {
                fields.insert(f.name.clone(), resolve_type_in_env(env, &f.ty)?);
            }
            let self_ty = ResolvedType::Struct(s.name.clone(), fields.clone());
            env.define_type_user(s.name.clone(), self_ty.clone(), s.span, "struct")?;
            register_method_signatures(env, &s.name, &self_ty, &s.methods)?;
        }
        Item::Enum(e) => {
            validate_generic_constraints(env, &e.generics, e.where_clause.as_ref(), "enum")?;
            let mut variants = Vec::new();
            let mut variant_map = HashMap::new();
            let mut variant_aliases = Vec::new();
            for v in &e.variants {
                let (payload_types, named_fields) = match &v.fields {
                    VariantFields::Unit => (Vec::new(), None),
                    VariantFields::Tuple(items) => (
                        items
                            .iter()
                            .map(|ty| resolve_type_in_env(env, ty))
                            .collect::<Result<Vec<_>, _>>()?,
                        None,
                    ),
                    VariantFields::Struct(fields) => {
                        let mut payload_types = Vec::with_capacity(fields.len());
                        let mut named_fields = HashMap::with_capacity(fields.len());
                        for field in fields {
                            let field_ty = resolve_type_in_env(env, &field.ty)?;
                            payload_types.push(field_ty.clone());
                            named_fields.insert(field.name.clone(), field_ty);
                        }
                        (payload_types, Some(named_fields))
                    }
                };
                variants.push((v.name.clone(), ResolvedType::Unit));
                variant_aliases.push((
                    v.name.clone(),
                    payload_types.clone(),
                    matches!(v.fields, VariantFields::Unit),
                ));
                variant_map.insert(
                    v.name.clone(),
                    EnumVariantTypeInfo {
                        payload_types,
                        named_fields,
                    },
                );
            }
            let enum_ty = ResolvedType::Enum(e.name.clone(), variants);
            env.define_type_user(e.name.clone(), enum_ty.clone(), e.span, "enum")?;
            env.enum_variants.insert(e.name.clone(), variant_map);
            for (variant_name, payload_types, is_unit) in variant_aliases {
                let alias = selfhost_enum_variant_alias_name(&e.name, &variant_name);
                let alias_ty = if is_unit {
                    enum_ty.clone()
                } else {
                    ResolvedType::Function {
                        params: payload_types,
                        ret: Box::new(enum_ty.clone()),
                        effects: EffectSet::new(),
                    }
                };
                env.define_global(alias, alias_ty);
            }
        }
        Item::Function(f) => {
            validate_generic_constraints(env, &f.generics, f.where_clause.as_ref(), "function")?;
            env.define_global_user(
                f.name.clone(),
                function_signature(env, f, None)?,
                f.span,
                "function",
            )?;
        }
        Item::Patch(patch) => {
            env.define_global_user(
                patch.name.clone(),
                function_signature(env, &patch_function_view(patch), None)?,
                patch.span,
                "patch",
            )?;
        }
        Item::Law(law) => {
            env.define_global_user(
                law.name.clone(),
                function_signature(env, &law_function_view(law), None)?,
                law.span,
                "law",
            )?;
        }
        Item::Axiom(axiom) => {
            env.define_global_user(axiom.name.clone(), ResolvedType::Unit, axiom.span, "axiom")?;
        }
        Item::Converge(converge) => {
            env.define_global_user(
                converge.name.clone(),
                function_signature(env, &converge_dispatcher_view(converge), None)?,
                converge.span,
                "converge",
            )?;
        }
        Item::World(world) => {
            let mut fields = HashMap::new();
            for state in &world.states {
                fields.insert(state.name.clone(), resolve_type_in_env(env, &state.ty)?);
            }
            let world_ty = ResolvedType::Struct(world.name.clone(), fields);
            env.define_type_user(world.name.clone(), world_ty.clone(), world.span, "world")?;
            env.define_global_user(world.name.clone(), world_ty, world.span, "world")?;
        }
        Item::Orchestrate(orchestrate) => {
            env.define_global_user(
                orchestrate.name.clone(),
                function_signature(env, &orchestrate_function_view(orchestrate), None)?,
                orchestrate.span,
                "orchestrate",
            )?;
        }
        Item::Pulse(pulse) => {
            env.define_global_user(pulse.name.clone(), ResolvedType::Unit, pulse.span, "pulse")?;
        }
        Item::Resonate(resonate) => {
            env.define_global_user(
                resonate.name.clone(),
                ResolvedType::Unit,
                resonate.span,
                "resonate",
            )?;
        }
        Item::Const(c) => {
            env.define_global_user(
                c.name.clone(),
                resolve_type_in_env(env, &c.ty)?,
                c.span,
                "const",
            )?;
        }
        Item::TypeAlias(alias) => {
            validate_generic_constraints(
                env,
                &alias.generics,
                alias.where_clause.as_ref(),
                "type alias",
            )?;
            env.define_type_user(
                alias.name.clone(),
                resolve_type_in_env(env, &alias.target)?,
                alias.span,
                "type alias",
            )?;
        }
        Item::Impl(imp) => {
            validate_generic_constraints(env, &imp.generics, imp.where_clause.as_ref(), "impl")?;
            if let Some(trait_name) = &imp.trait_name {
                validate_trait_name_exists(env, trait_name, imp.span, "impl trait")?;
            }
            let self_ty = resolve_type_in_env(env, &imp.target_type)?;
            if let Some(target_name) = resolved_type_name(&self_ty) {
                register_method_signatures(env, target_name, &self_ty, &imp.methods)?;
            }
        }
        Item::Component(component) => {
            let component_ty = ResolvedType::Struct(component.name.clone(), HashMap::new());
            env.define_type_user(
                component.name.clone(),
                component_ty.clone(),
                component.span,
                "component",
            )?;
            register_method_signatures(env, &component.name, &component_ty, &component.methods)?;
        }
        Item::Actor(actor) => {
            let actor_ty = ResolvedType::Struct(actor.name.clone(), HashMap::new());
            env.define_type_user(actor.name.clone(), actor_ty.clone(), actor.span, "actor")?;
            register_method_signatures(env, &actor.name, &actor_ty, &actor.methods)?;
        }
        Item::Mod(module) => {
            if let Some(children) = &module.inline {
                let module_path = vec![module.name.clone()];
                register_inline_module_type_aliases(env, children, &module_path);
                register_inline_module_global_aliases(env, children, &module_path)?;
                for child in children {
                    register_item_types(env, child)?;
                }
            }
        }
        Item::Trait(trait_def) => {
            env.define_trait_user(trait_def.name.clone(), trait_def.span)?;
            validate_generic_constraints(
                env,
                &trait_def.generics,
                trait_def.where_clause.as_ref(),
                "trait",
            )?;
        }
        // Register imported names as Unknown so that callers of cross-module
        // functions (e.g. `use lexer::tokenize_source`) don't fail the typechecker
        // when the imported module is not present in the current compilation unit.
        // The last path segment is the imported identifier.
        Item::Use(u) => {
            if register_stdlib_import_types(env, u)? {
                return Ok(());
            }

            if register_filesystem_import_types(env, u)? {
                return Ok(());
            }

            let imported_name = if let Some(alias) = &u.alias {
                alias.clone()
            } else if let Some(last) = u.path.last() {
                last.clone()
            } else {
                return Ok(());
            };
            // Only register if not already known (don't overwrite real builtins).
            if env.lookup(&imported_name).is_none() {
                env.define_global_user(imported_name, ResolvedType::Unknown, u.span, "import")?;
            }
        }
        Item::Import(import) => {
            for binding_name in python_import_binding_names(import) {
                if env.lookup(&binding_name).is_none() {
                    env.define_global_user(
                        binding_name,
                        ResolvedType::Unknown,
                        import.span,
                        "python import",
                    )?;
                }
            }
        }
        _ => {}
    }

    Ok(())
}

fn python_import_binding_names(import: &Import) -> Vec<String> {
    if import.members.is_empty() {
        if let Some(alias) = &import.alias {
            return vec![alias.clone()];
        }
        return import
            .module_path
            .first()
            .cloned()
            .into_iter()
            .collect::<Vec<_>>();
    }

    import
        .members
        .iter()
        .map(|member| member.alias.clone().unwrap_or_else(|| member.name.clone()))
        .collect()
}

fn register_stdlib_import_types(env: &mut TypeEnv, u: &Use) -> KainResult<bool> {
    let path = u.path.join("/");
    if !(path.starts_with("std/") || path.starts_with("stdlib/")) {
        return Ok(false);
    }

    let module_name = path
        .trim_start_matches("std/")
        .trim_start_matches("stdlib/");
    if module_name.is_empty() {
        return Ok(false);
    }

    let Some(file_path) = resolve_stdlib_module_file(module_name) else {
        return Ok(false);
    };
    let file_path = canonical_module_identity(&file_path);
    let module_key = file_path.to_string_lossy().to_string();
    if env.span_mapper.has_origin_file(&module_key) {
        env.loaded_stdlib_modules.insert(module_key);
        return Ok(true);
    }
    if env.loaded_stdlib_modules.contains(&module_key) {
        return Ok(true);
    }

    let Ok(source) = kain_fs::read_text(&file_path) else {
        return Ok(false);
    };
    let Ok(tokens) = Lexer::new(&source).tokenize() else {
        return Ok(false);
    };
    let span_mapper = SpanMapper::new(&source);
    let filename = file_path.to_string_lossy().to_string();
    let Ok(program) = Parser::new(&tokens, &span_mapper, &filename).parse() else {
        return Ok(false);
    };

    env.push_stdlib_registration();
    let registration_result: KainResult<()> = (|| {
        for item in &program.items {
            predeclare_item_types(env, item);
        }

        for item in &program.items {
            if matches!(item, Item::Use(_)) {
                continue;
            }
            register_item_types(env, item)?;
        }

        for item in &program.items {
            if matches!(item, Item::Use(_)) {
                continue;
            }
            register_item_types(env, item)?;
        }

        Ok(())
    })();
    env.pop_stdlib_registration();
    registration_result?;

    env.loaded_stdlib_modules.insert(module_key);

    Ok(true)
}

fn canonical_module_identity(path: &Path) -> PathBuf {
    std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn register_filesystem_import_types(env: &mut TypeEnv, u: &Use) -> KainResult<bool> {
    let Some(first_segment) = u.path.first() else {
        return Ok(false);
    };

    if matches!(
        first_segment.as_str(),
        "std" | "stdlib" | "rust" | "node" | "python" | "js"
    ) {
        return Ok(false);
    }

    let context = FilesystemModuleResolutionContext {
        importer_file: env.importer_file_for_span(u.span),
    };
    let Some(resolution) = resolve_filesystem_module_file_with_context(&u.path, &context) else {
        return Ok(false);
    };

    let Ok(source) = kain_fs::read_text(&resolution.file_path) else {
        return Ok(false);
    };
    let Ok(tokens) = Lexer::new(&source).tokenize() else {
        return Ok(false);
    };
    let span_mapper = SpanMapper::new(&source);
    let filename = resolution.file_path.to_string_lossy().to_string();
    let Ok(program) = Parser::new(&tokens, &span_mapper, &filename).parse() else {
        return Ok(false);
    };
    let Ok(items) =
        select_filesystem_import_type_items(program.items, u, resolution.selected_item.as_deref())
    else {
        return Ok(false);
    };

    for item in items {
        if matches!(item, Item::Use(_)) {
            continue;
        }
        if register_item_types(env, &item).is_err() {
            return Ok(false);
        }
    }

    Ok(true)
}

fn select_filesystem_import_type_items(
    items: Vec<Item>,
    u: &Use,
    selected_item: Option<&str>,
) -> KainResult<Vec<Item>> {
    let Some(selected_item) = selected_item else {
        return Ok(items);
    };

    if u.glob {
        return Ok(items);
    }

    let direct_path = u.path.join("/");
    let Some(item) = items
        .into_iter()
        .find(|item| importable_item_name(item).is_some_and(|name| name == selected_item))
    else {
        return Err(KainError::runtime(format!(
            "Module item not found during type registration: {}",
            direct_path
        )));
    };

    Ok(vec![apply_import_alias_for_type_registration(
        item,
        u.alias.as_deref(),
    )?])
}

fn importable_item_name(item: &Item) -> Option<&str> {
    match item {
        Item::Function(f) => Some(&f.name),
        Item::Axiom(axiom) => Some(&axiom.name),
        Item::Pulse(pulse) => Some(&pulse.name),
        Item::Resonate(resonate) => Some(&resonate.name),
        Item::Component(c) => Some(&c.name),
        Item::Struct(s) => Some(&s.name),
        Item::Enum(e) => Some(&e.name),
        Item::Actor(a) => Some(&a.name),
        Item::Const(c) => Some(&c.name),
        Item::Macro(m) => Some(&m.name),
        Item::TypeAlias(alias) => Some(&alias.name),
        Item::Mod(module) => Some(&module.name),
        _ => None,
    }
}

fn apply_import_alias_for_type_registration(
    mut item: Item,
    alias: Option<&str>,
) -> KainResult<Item> {
    let Some(alias) = alias else {
        return Ok(item);
    };

    match &mut item {
        Item::Function(f) => f.name = alias.to_string(),
        Item::Axiom(axiom) => axiom.name = alias.to_string(),
        Item::Pulse(pulse) => pulse.name = alias.to_string(),
        Item::Resonate(resonate) => resonate.name = alias.to_string(),
        Item::Component(c) => c.name = alias.to_string(),
        Item::Struct(s) => s.name = alias.to_string(),
        Item::Enum(e) => e.name = alias.to_string(),
        Item::Actor(a) => a.name = alias.to_string(),
        Item::Const(c) => c.name = alias.to_string(),
        Item::Macro(m) => m.name = alias.to_string(),
        Item::TypeAlias(t) => t.name = alias.to_string(),
        Item::Mod(m) => m.name = alias.to_string(),
        other => {
            return Err(KainError::runtime(format!(
                "Import alias is not supported for item: {:?}",
                other
            )))
        }
    }

    Ok(item)
}

fn register_method_signatures(
    env: &mut TypeEnv,
    type_name: &str,
    self_ty: &ResolvedType,
    methods: &[Function],
) -> KainResult<()> {
    for method in methods {
        let signature = function_signature(env, method, Some(self_ty))?;
        env.define_method_user(
            type_name,
            method.name.clone(),
            signature.clone(),
            method.span,
            "method",
        )?;
        if !method_has_receiver_param(method) {
            env.define_global_user(
                lowered_impl_function_name(type_name, &method.name),
                signature.clone(),
                method.span,
                "static impl helper",
            )?;
            env.define_global_user(
                selfhost_static_impl_function_name(type_name, &method.name),
                signature,
                method.span,
                "selfhost static impl helper",
            )?;
        }
    }
    Ok(())
}

fn register_inline_module_global_aliases(
    env: &mut TypeEnv,
    items: &[Item],
    module_path: &[String],
) -> KainResult<()> {
    for item in items {
        match item {
            Item::Function(f) => {
                env.define_global_user(
                    module_scoped_name(module_path, &f.name),
                    function_signature(env, f, None)?,
                    f.span,
                    "inline module function alias",
                )?;
            }
            Item::Const(c) => {
                env.define_global_user(
                    module_scoped_name(module_path, &c.name),
                    resolve_type_in_env(env, &c.ty)?,
                    c.span,
                    "inline module const alias",
                )?;
            }
            Item::Mod(module) => {
                if let Some(children) = &module.inline {
                    let mut nested_path = module_path.to_vec();
                    nested_path.push(module.name.clone());
                    register_inline_module_global_aliases(env, children, &nested_path)?;
                }
            }
            _ => {}
        }
    }
    Ok(())
}

fn predeclare_inline_module_type_aliases(
    env: &mut TypeEnv,
    items: &[Item],
    module_path: &[String],
) {
    for item in items {
        match item {
            Item::Struct(s) => {
                let scoped_name = module_scoped_type_name(module_path, &s.name);
                env.types
                    .entry(scoped_name)
                    .or_insert_with(|| ResolvedType::Struct(s.name.clone(), HashMap::new()));
            }
            Item::Enum(e) => {
                let scoped_name = module_scoped_type_name(module_path, &e.name);
                env.types
                    .entry(scoped_name.clone())
                    .or_insert_with(|| ResolvedType::Enum(e.name.clone(), Vec::new()));
                env.enum_variants.entry(scoped_name).or_default();
            }
            Item::World(world) => {
                let scoped_name = module_scoped_type_name(module_path, &world.name);
                env.types
                    .entry(scoped_name)
                    .or_insert_with(|| ResolvedType::Struct(world.name.clone(), HashMap::new()));
            }
            Item::Component(component) => {
                let scoped_name = module_scoped_type_name(module_path, &component.name);
                env.types.entry(scoped_name).or_insert_with(|| {
                    ResolvedType::Struct(component.name.clone(), HashMap::new())
                });
            }
            Item::Actor(actor) => {
                let scoped_name = module_scoped_type_name(module_path, &actor.name);
                env.types
                    .entry(scoped_name)
                    .or_insert_with(|| ResolvedType::Struct(actor.name.clone(), HashMap::new()));
            }
            Item::Mod(module) => {
                if let Some(children) = &module.inline {
                    let mut nested_path = module_path.to_vec();
                    nested_path.push(module.name.clone());
                    predeclare_inline_module_type_aliases(env, children, &nested_path);
                }
            }
            _ => {}
        }
    }
}

fn register_inline_module_type_aliases(env: &mut TypeEnv, items: &[Item], module_path: &[String]) {
    for item in items {
        match item {
            Item::Struct(s) => {
                if let Some(resolved) = env.lookup_type(&s.name).cloned() {
                    env.types
                        .insert(module_scoped_type_name(module_path, &s.name), resolved);
                }
            }
            Item::Enum(e) => {
                if let Some(resolved) = env.lookup_type(&e.name).cloned() {
                    let scoped_name = module_scoped_type_name(module_path, &e.name);
                    env.types.insert(scoped_name.clone(), resolved);
                    if let Some(variants) = env.enum_variants.get(&e.name).cloned() {
                        env.enum_variants.insert(scoped_name, variants);
                    }
                }
            }
            Item::TypeAlias(alias) => {
                if let Some(resolved) = env.lookup_type(&alias.name).cloned() {
                    env.types
                        .insert(module_scoped_type_name(module_path, &alias.name), resolved);
                }
            }
            Item::World(world) => {
                if let Some(resolved) = env.lookup_type(&world.name).cloned() {
                    env.types
                        .insert(module_scoped_type_name(module_path, &world.name), resolved);
                }
            }
            Item::Component(component) => {
                if let Some(resolved) = env.lookup_type(&component.name).cloned() {
                    env.types.insert(
                        module_scoped_type_name(module_path, &component.name),
                        resolved,
                    );
                }
            }
            Item::Actor(actor) => {
                if let Some(resolved) = env.lookup_type(&actor.name).cloned() {
                    env.types
                        .insert(module_scoped_type_name(module_path, &actor.name), resolved);
                }
            }
            Item::Mod(module) => {
                if let Some(children) = &module.inline {
                    let mut nested_path = module_path.to_vec();
                    nested_path.push(module.name.clone());
                    register_inline_module_type_aliases(env, children, &nested_path);
                }
            }
            _ => {}
        }
    }
}

fn inline_module_scope_bindings(
    env: &mut TypeEnv,
    items: &[Item],
) -> KainResult<HashMap<String, ResolvedType>> {
    let mut bindings = HashMap::new();
    for item in items {
        match item {
            Item::Function(f) => {
                bindings.insert(f.name.clone(), function_signature(env, f, None)?);
            }
            Item::Const(c) => {
                bindings.insert(c.name.clone(), resolve_type_in_env(env, &c.ty)?);
            }
            _ => {}
        }
    }
    Ok(bindings)
}

fn define_scope_bindings(env: &mut TypeEnv, bindings: &HashMap<String, ResolvedType>) {
    for (name, ty) in bindings {
        env.define(name.clone(), ty.clone());
    }
}

fn check_item_into(env: &mut TypeEnv, item: &Item, out: &mut Vec<TypedItem>) -> KainResult<()> {
    out.push(check_item(env, item)?);
    Ok(())
}

fn check_item(env: &mut TypeEnv, item: &Item) -> KainResult<TypedItem> {
    match item {
        Item::Function(f) => Ok(TypedItem::Function(check_function(env, f)?)),
        Item::Patch(patch) => Ok(TypedItem::Patch(check_patch(env, patch)?)),
        Item::Law(law) => Ok(TypedItem::Law(check_law(env, law)?)),
        Item::Axiom(axiom) => Ok(TypedItem::Axiom(check_axiom(env, axiom)?)),
        Item::Converge(converge) => Ok(TypedItem::Converge(check_converge(env, converge)?)),
        Item::World(world) => Ok(TypedItem::World(check_world(env, world)?)),
        Item::Entangle(entangle) => Ok(TypedItem::Entangle(check_entangle(env, entangle)?)),
        Item::Orchestrate(orchestrate) => {
            Ok(TypedItem::Orchestrate(check_orchestrate(env, orchestrate)?))
        }
        Item::Pulse(pulse) => Ok(TypedItem::Pulse(check_pulse(env, pulse)?)),
        Item::Resonate(resonate) => Ok(TypedItem::Resonate(check_resonate(env, resonate)?)),
        Item::Struct(s) => Ok(TypedItem::Struct(check_struct(env, s)?)),
        Item::Enum(e) => Ok(TypedItem::Enum(check_enum(env, e)?)),
        Item::Trait(t) => Ok(TypedItem::Trait(check_trait(env, t)?)),
        Item::Component(c) => Ok(TypedItem::Component(check_component(env, c)?)),
        Item::Shader(s) => Ok(TypedItem::Shader(check_shader(env, s)?)),
        Item::Actor(a) => Ok(TypedItem::Actor(check_actor(env, a)?)),
        Item::Comptime(b) => Ok(TypedItem::Comptime(TypedComptime {
            ast: b.body.clone(),
        })),
        Item::Const(c) => Ok(TypedItem::Const(check_const(env, c)?)),
        Item::Macro(m) => Ok(TypedItem::Macro(TypedMacro { ast: m.clone() })),
        Item::Use(u) => Ok(TypedItem::Use(TypedUse { ast: u.clone() })),
        Item::Import(import) => Ok(TypedItem::Import(TypedImport {
            ast: import.clone(),
        })),
        Item::Mod(module) => Ok(TypedItem::Mod(check_mod(env, module)?)),
        Item::Impl(i) => Ok(TypedItem::Impl(check_impl(env, i)?)),
        Item::Test(t) => Ok(TypedItem::Test(check_test(env, t)?)),
        Item::TypeAlias(ta) => Ok(TypedItem::TypeAlias(check_type_alias(env, ta)?)),
        Item::MaterialGraph(mg) => Ok(TypedItem::MaterialGraph(mg.clone())),
        Item::MaterialFunction(mf) => Ok(TypedItem::MaterialFunction(mf.clone())),
        Item::GraphEditor(ge) => Ok(TypedItem::GraphEditor(ge.clone())),
        Item::GraphRuntime(gr) => Ok(TypedItem::GraphRuntime(gr.clone())),
        Item::StateMachine(sm) => Ok(TypedItem::StateMachine(sm.clone())),
        Item::AsyncTask(at) => Ok(TypedItem::AsyncTask(at.clone())),
        Item::EditorModule(em) => Ok(TypedItem::EditorModule(em.clone())),
        Item::GameplayTags(gt) => Ok(TypedItem::GameplayTags(gt.clone())),
        Item::GameplayAbility(ga) => Ok(TypedItem::GameplayAbility(ga.clone())),
        Item::GameplayEffect(ge) => Ok(TypedItem::GameplayEffect(ge.clone())),
        Item::GameplayCue(gc) => Ok(TypedItem::GameplayCue(gc.clone())),
        _ => Err(env.type_error(
            "Item type not yet supported in type checker",
            item_span(item),
        )),
    }
}

fn check_mod(env: &mut TypeEnv, module: &Mod) -> KainResult<TypedMod> {
    let mut items = Vec::new();
    let mut errors = Vec::new();
    if let Some(children) = &module.inline {
        let bindings = inline_module_scope_bindings(env, children)?;
        for child in children {
            match child {
                Item::Mod(_) => {
                    if let Err(error) = check_item_into(env, child, &mut items) {
                        extend_accumulated_errors(&mut errors, error);
                    }
                }
                _ => {
                    if let Err(error) = env.with_scope(|env| {
                        define_scope_bindings(env, &bindings);
                        check_item_into(env, child, &mut items)
                    }) {
                        extend_accumulated_errors(&mut errors, error);
                    }
                }
            }
        }
    }
    finish_accumulated(
        errors,
        TypedMod {
            ast: module.clone(),
            items,
        },
    )
}

fn check_const(env: &mut TypeEnv, c: &Const) -> KainResult<TypedConst> {
    validate_const_attributes(env, c)?;
    let ty = resolve_type_in_env(env, &c.ty)?;
    let value_ty = infer_expr_type(env, &c.value, None)?;
    ensure_type_compatible(env, &ty, &value_ty, c.value.span(), "const value")?;
    Ok(TypedConst { ast: c.clone(), ty })
}

fn check_actor(env: &mut TypeEnv, a: &Actor) -> KainResult<TypedActor> {
    let mut state_types = HashMap::new();
    let mut actor_contract = ActorDefinition::new(a.name.clone());
    for s in &a.state {
        let ty = resolve_type_in_env(env, &s.ty)?;
        let initial_ty = infer_expr_type(env, &s.initial, None)?;
        ensure_type_compatible(
            env,
            &ty,
            &initial_ty,
            s.initial.span(),
            "actor state initializer",
        )?;
        actor_contract.state.push(ActorStateSlot::new(
            s.name.clone(),
            resolved_type_contract_name(&ty),
        ));
        state_types.insert(s.name.clone(), ty);
    }

    let self_ty = ResolvedType::Struct(a.name.clone(), state_types.clone());
    let mut handler_errors = Vec::new();
    for handler in &a.handlers {
        let handler_return = ResolvedType::Unit;
        let ctx = SemanticContext {
            function_name: format!("{}_{}", a.name, handler.message_type),
            return_type: handler_return,
            effects: EffectSet::new(),
        };
        let handler_result = env.with_scope(|env| {
            env.define("self".to_string(), self_ty.clone());
            let mut message_params = Vec::with_capacity(handler.params.len());
            let mut handler_param_types = Vec::with_capacity(handler.params.len());
            for param in &handler.params {
                let ty = resolve_param_type(env, param, Some(&self_ty))?;
                handler_param_types.push((param.name.clone(), ty.clone()));
                message_params.push(MessageParameter::required(
                    param.name.clone(),
                    resolved_type_contract_name(&ty),
                ));
                env.define(param.name.clone(), ty);
            }
            check_block_semantics(env, &handler.body, &ctx)?;
            let reply_contract = handler_param_types
                .first()
                .and_then(|(param_name, param_ty)| {
                    if !matches!(param_ty, ResolvedType::Generic(name) if name == "P") {
                        return None;
                    }
                    infer_reply_contract_from_handler_body(env, &handler.body, param_name)
                });
            Ok((message_params, reply_contract))
        });
        match handler_result {
            Ok((message_params, reply_contract)) => {
                let message_signature = if let Some(reply_contract) = reply_contract {
                    MessageSignature::call(
                        handler.message_type.clone(),
                        message_params,
                        reply_contract,
                    )
                } else {
                    MessageSignature::cast(handler.message_type.clone(), message_params)
                };
                actor_contract
                    .handlers
                    .push(ActorHandlerSignature::cast(message_signature));
            }
            Err(error) => extend_accumulated_errors(&mut handler_errors, error),
        }
    }

    for method in &a.methods {
        match check_function_with_self(env, method, &self_ty) {
            Ok(typed_method) => actor_contract
                .methods
                .push(actor_method_contract(method, &typed_method.resolved_type)),
            Err(error) => extend_accumulated_errors(&mut handler_errors, error),
        }
    }

    validate_actor_definition(&actor_contract)
        .map_err(|error| KainError::type_error(error.to_string(), a.span))?;

    if actor_contract.handlers.is_empty() && actor_contract.methods.is_empty() {
        actor_contract
            .capabilities
            .retain(|capability| !matches!(capability, kain_actor::ActorCapability::Ask));
    }

    env.actor_contracts
        .insert(a.name.clone(), actor_contract.clone());

    finish_accumulated(
        handler_errors,
        TypedActor {
            ast: a.clone(),
            state_types,
            actor_contract,
        },
    )
}

fn actor_method_contract(method: &Function, resolved_type: &ResolvedType) -> ActorMethodSignature {
    match resolved_type {
        ResolvedType::Function {
            params,
            ret,
            effects,
        } => {
            let message_params = method
                .params
                .iter()
                .zip(params.iter())
                .map(|(param, ty)| {
                    MessageParameter::required(param.name.clone(), resolved_type_contract_name(ty))
                })
                .collect();
            ActorMethodSignature::new(
                method.name.clone(),
                message_params,
                resolved_type_contract_name(ret),
                effect_contract_names(effects),
            )
        }
        _ => ActorMethodSignature::new(method.name.clone(), Vec::new(), "Unit", Vec::new()),
    }
}

fn effect_contract_names(effects: &EffectSet) -> Vec<String> {
    let mut names: Vec<String> = effects
        .effects
        .iter()
        .map(|effect| format!("{effect:?}"))
        .collect();
    names.sort();
    names
}

fn resolved_type_contract_name(ty: &ResolvedType) -> String {
    match ty {
        ResolvedType::Unit => "Unit".to_string(),
        ResolvedType::Bool => "Bool".to_string(),
        ResolvedType::Int(size) => format!("{size:?}"),
        ResolvedType::Float(size) => format!("{size:?}"),
        ResolvedType::String => "String".to_string(),
        ResolvedType::Char => "Char".to_string(),
        ResolvedType::Array(inner, count) => {
            format!("[{}; {}]", resolved_type_contract_name(inner), count)
        }
        ResolvedType::Slice(inner) => format!("[{}]", resolved_type_contract_name(inner)),
        ResolvedType::Tuple(items) => {
            let labels = items
                .iter()
                .map(resolved_type_contract_name)
                .collect::<Vec<_>>()
                .join(", ");
            format!("({labels})")
        }
        ResolvedType::Option(inner) => format!("Option<{}>", resolved_type_contract_name(inner)),
        ResolvedType::Result(ok, err) => format!(
            "Result<{}, {}>",
            resolved_type_contract_name(ok),
            resolved_type_contract_name(err)
        ),
        ResolvedType::Future(inner) => format!("Future<{}>", resolved_type_contract_name(inner)),
        ResolvedType::Ref { mutable, inner } => {
            if *mutable {
                format!("&mut {}", resolved_type_contract_name(inner))
            } else {
                format!("&{}", resolved_type_contract_name(inner))
            }
        }
        ResolvedType::Ptr { mutable, inner } => {
            if *mutable {
                format!("ptr_mut<{}>", resolved_type_contract_name(inner))
            } else {
                format!("ptr<{}>", resolved_type_contract_name(inner))
            }
        }
        ResolvedType::Function { params, ret, .. } => {
            let labels = params
                .iter()
                .map(resolved_type_contract_name)
                .collect::<Vec<_>>()
                .join(", ");
            format!("fn({labels}) -> {}", resolved_type_contract_name(ret))
        }
        ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) => name.clone(),
        ResolvedType::Generic(name) => name.clone(),
        ResolvedType::Never => "!".to_string(),
        ResolvedType::Unknown => "Unknown".to_string(),
    }
}

fn infer_reply_contract_from_handler_body(
    env: &mut TypeEnv,
    body: &Block,
    reply_port_name: &str,
) -> Option<kain_actor::message::MessageReplyContract> {
    let mut reply_type = None;
    if collect_reply_contract_type(env, body, reply_port_name, &mut reply_type).is_err() {
        return None;
    }
    reply_type.map(|ty| {
        kain_actor::message::MessageReplyContract::new(
            resolved_type_contract_name(&ty),
            kain_actor::lifecycle::DEFAULT_ASK_TIMEOUT_MS,
        )
    })
}

fn collect_reply_contract_type(
    env: &mut TypeEnv,
    block: &Block,
    reply_port_name: &str,
    reply_type: &mut Option<ResolvedType>,
) -> Result<(), ()> {
    for stmt in &block.stmts {
        collect_reply_contract_type_from_stmt(env, stmt, reply_port_name, reply_type)?;
    }
    Ok(())
}

// --- DispatchSize helpers (since DispatchSize is not iterable) ---

fn dispatch_size_exprs_ref(dispatch_size: &DispatchSize) -> Vec<&Expr> {
    match dispatch_size {
        DispatchSize::Fixed([x, y, z]) => vec![x, y, z],
        DispatchSize::Indirect(expr) => vec![expr],
    }
}

fn dispatch_size_for_each<F, E>(dispatch_size: &DispatchSize, mut f: F) -> Result<(), E>
where
    F: FnMut(&Expr) -> Result<(), E>,
{
    match dispatch_size {
        DispatchSize::Fixed([x, y, z]) => {
            f(x)?;
            f(y)?;
            f(z)?;
        }
        DispatchSize::Indirect(expr) => {
            f(expr)?;
        }
    }
    Ok(())
}

fn dispatch_size_any(dispatch_size: &DispatchSize, f: fn(&Expr) -> bool) -> bool {
    match dispatch_size {
        DispatchSize::Fixed([x, y, z]) => f(x) || f(y) || f(z),
        DispatchSize::Indirect(expr) => f(expr),
    }
}

fn dispatch_size_find_map<T>(dispatch_size: &DispatchSize, f: fn(&Expr) -> Option<T>) -> Option<T> {
    match dispatch_size {
        DispatchSize::Fixed([x, y, z]) => f(x).or_else(|| f(y)).or_else(|| f(z)),
        DispatchSize::Indirect(expr) => f(expr),
    }
}

fn collect_reply_contract_type_from_stmt(
    env: &mut TypeEnv,
    stmt: &Stmt,
    reply_port_name: &str,
    reply_type: &mut Option<ResolvedType>,
) -> Result<(), ()> {
    match stmt {
        Stmt::Let { value, .. } => {
            if let Some(value) = value {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Stmt::Expr(expr) => {
            collect_reply_contract_type_from_expr(env, expr, reply_port_name, reply_type)
        }
        Stmt::Defer { expr, .. } => {
            collect_reply_contract_type_from_expr(env, expr, reply_port_name, reply_type)
        }
        Stmt::Dispatch { dispatch_size, .. } => {
            dispatch_size_for_each(dispatch_size, |expr| {
                let _ = collect_reply_contract_type_from_expr(env, expr, reply_port_name, reply_type)?;
                Ok(())
            })?;
            Ok(())
        }
        Stmt::Return(value, _) => {
            if let Some(value) = value {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Stmt::While {
            condition, body, ..
        } => {
            collect_reply_contract_type_from_expr(env, condition, reply_port_name, reply_type)?;
            collect_reply_contract_type(env, body, reply_port_name, reply_type)
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            collect_reply_contract_type_from_expr(env, iter, reply_port_name, reply_type)?;
            collect_reply_contract_type(env, body, reply_port_name, reply_type)
        }
        Stmt::Loop { body, .. } => {
            collect_reply_contract_type(env, body, reply_port_name, reply_type)
        }
        Stmt::Break(value, _) => {
            if let Some(value) = value {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Stmt::Continue(_) | Stmt::Item(_) => Ok(()),
        Stmt::Subgroup { body, .. } => collect_reply_contract_type(env, body, reply_port_name, reply_type),
    }
}

fn collect_reply_contract_type_from_expr(
    env: &mut TypeEnv,
    expr: &Expr,
    reply_port_name: &str,
    reply_type: &mut Option<ResolvedType>,
) -> Result<(), ()> {
    match expr {
        Expr::SendMsg {
            target,
            message,
            data,
            ..
        } => {
            if matches!(target.as_ref(), Expr::Ident(name, _) if name == reply_port_name)
                && message == "Reply"
            {
                if data.len() > 1 {
                    return Err(());
                }
                if let Some((field_name, value)) = data.first() {
                    if field_name != "value" {
                        return Err(());
                    }
                    let value_ty = infer_expr_type_read_only(env, value).map_err(|_| ())?;
                    if let Some(existing) = reply_type.as_ref() {
                        if !types_compatible(existing, &value_ty)
                            || !types_compatible(&value_ty, existing)
                        {
                            return Err(());
                        }
                    } else {
                        *reply_type = Some(value_ty);
                    }
                } else if reply_type.is_none() {
                    *reply_type = Some(ResolvedType::Unit);
                }
            }
            collect_reply_contract_type_from_expr(env, target, reply_port_name, reply_type)?;
            for (_, value) in data {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Emit { data, .. } => {
            for (_, value) in data {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Block(block, _) => {
            collect_reply_contract_type(env, block, reply_port_name, reply_type)
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_reply_contract_type_from_expr(env, condition, reply_port_name, reply_type)?;
            collect_reply_contract_type(env, then_branch, reply_port_name, reply_type)?;
            if let Some(branch) = else_branch {
                collect_reply_contract_type_from_else_branch(
                    env,
                    branch,
                    reply_port_name,
                    reply_type,
                )?;
            }
            Ok(())
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            collect_reply_contract_type_from_expr(env, scrutinee, reply_port_name, reply_type)?;
            for arm in arms {
                if let Some(guard) = &arm.guard {
                    collect_reply_contract_type_from_expr(env, guard, reply_port_name, reply_type)?;
                }
                collect_reply_contract_type_from_expr(env, &arm.body, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Paren(inner, _)
        | Expr::Await(inner, _)
        | Expr::AsyncBlock(inner, _)
        | Expr::Comptime(inner, _)
        | Expr::Deref(inner, _)
        | Expr::Try(inner, _)
        | Expr::Ref { value: inner, .. } => {
            collect_reply_contract_type_from_expr(env, inner, reply_port_name, reply_type)
        }
        Expr::Collapse { target, body, .. }
        | Expr::Observe { target, body, .. }
        | Expr::Share { target, body, .. } => {
            collect_reply_contract_type_from_expr(env, target, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, body, reply_port_name, reply_type)
        }
        Expr::Call { callee, args, .. } => {
            collect_reply_contract_type_from_expr(env, callee, reply_port_name, reply_type)?;
            for arg in args {
                collect_reply_contract_type_from_expr(
                    env,
                    &arg.value,
                    reply_port_name,
                    reply_type,
                )?;
            }
            Ok(())
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_reply_contract_type_from_expr(env, receiver, reply_port_name, reply_type)?;
            for arg in args {
                collect_reply_contract_type_from_expr(
                    env,
                    &arg.value,
                    reply_port_name,
                    reply_type,
                )?;
            }
            Ok(())
        }
        Expr::StageCall { args, .. } => {
            for arg in args {
                collect_reply_contract_type_from_expr(
                    env,
                    &arg.value,
                    reply_port_name,
                    reply_type,
                )?;
            }
            Ok(())
        }
        Expr::MacroCall { args, .. }
        | Expr::Array(args, _)
        | Expr::Tuple(args, _)
        | Expr::FString(args, _) => {
            for arg in args {
                collect_reply_contract_type_from_expr(env, arg, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Struct { fields, rest, .. } => {
            for (_, value) in fields {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            if let Some(rest) = rest {
                collect_reply_contract_type_from_expr(env, rest, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::AggregateInit { fields, .. } => {
            for (_, value) in fields {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Unit => Ok(()),
            EnumVariantFields::Tuple(values) => {
                for value in values {
                    collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
                }
                Ok(())
            }
            EnumVariantFields::Struct(values) => {
                for (_, value) in values {
                    collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
                }
                Ok(())
            }
        },
        Expr::Assign { target, value, .. } => {
            collect_reply_contract_type_from_expr(env, target, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)
        }
        Expr::Binary { left, right, .. } => {
            collect_reply_contract_type_from_expr(env, left, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, right, reply_port_name, reply_type)
        }
        Expr::MemStore { pointer, value, .. }
        | Expr::VolatileStore { pointer, value, .. }
        | Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            collect_reply_contract_type_from_expr(env, pointer, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)
        }
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            collect_reply_contract_type_from_expr(env, pointer, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, offset, reply_port_name, reply_type)
        }
        Expr::Index { object, index, .. } => {
            collect_reply_contract_type_from_expr(env, object, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, index, reply_port_name, reply_type)
        }
        Expr::Field { object, .. }
        | Expr::AddrOf { value: object, .. }
        | Expr::Cast { value: object, .. }
        | Expr::Bitcast { value: object, .. }
        | Expr::Teleport { value: object, .. }
        | Expr::MemLoad {
            pointer: object, ..
        }
        | Expr::VolatileLoad {
            pointer: object, ..
        }
        | Expr::AtomicLoad {
            pointer: object, ..
        }
        | Expr::CpuCacheFlush {
            pointer: object, ..
        }
        | Expr::Decay { target: object, .. } => {
            collect_reply_contract_type_from_expr(env, object, reply_port_name, reply_type)
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => {
            collect_reply_contract_type_from_expr(env, pointer, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, expected, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, desired, reply_port_name, reply_type)
        }
        Expr::Range { start, end, .. } => {
            if let Some(start) = start {
                collect_reply_contract_type_from_expr(env, start, reply_port_name, reply_type)?;
            }
            if let Some(end) = end {
                collect_reply_contract_type_from_expr(env, end, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Lambda { body, .. } => {
            collect_reply_contract_type_from_expr(env, body, reply_port_name, reply_type)
        }
        Expr::Spawn { init, .. } => {
            for (_, value) in init {
                collect_reply_contract_type_from_expr(env, value, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::InlineAsm { operands, .. } => {
            for operand in operands {
                collect_reply_contract_type_from_expr(env, operand, reply_port_name, reply_type)?;
            }
            Ok(())
        }
        Expr::Alloc { size, .. } => {
            collect_reply_contract_type_from_expr(env, size, reply_port_name, reply_type)
        }
        Expr::Realloc { pointer, size, .. } => {
            collect_reply_contract_type_from_expr(env, pointer, reply_port_name, reply_type)?;
            collect_reply_contract_type_from_expr(env, size, reply_port_name, reply_type)
        }
        Expr::Return(Some(inner), _) | Expr::Break(Some(inner), _) => {
            collect_reply_contract_type_from_expr(env, inner, reply_port_name, reply_type)
        }
        Expr::Unary { operand, .. } => {
            collect_reply_contract_type_from_expr(env, operand, reply_port_name, reply_type)
        }
        Expr::AtomicFence { .. }
        | Expr::CpuFence { .. }
        | Expr::JSX(_, _)
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Return(None, _)
        | Expr::Break(None, _)
        | Expr::Continue(_)
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. } => Ok(()),
    }
}

fn collect_reply_contract_type_from_else_branch(
    env: &mut TypeEnv,
    branch: &ElseBranch,
    reply_port_name: &str,
    reply_type: &mut Option<ResolvedType>,
) -> Result<(), ()> {
    match branch {
        ElseBranch::Else(block) => {
            collect_reply_contract_type(env, block, reply_port_name, reply_type)
        }
        ElseBranch::ElseIf(condition, block, next) => {
            collect_reply_contract_type_from_expr(env, condition, reply_port_name, reply_type)?;
            collect_reply_contract_type(env, block, reply_port_name, reply_type)?;
            if let Some(next) = next {
                collect_reply_contract_type_from_else_branch(
                    env,
                    next,
                    reply_port_name,
                    reply_type,
                )?;
            }
            Ok(())
        }
    }
}

fn infer_expr_type_read_only(env: &TypeEnv, expr: &Expr) -> KainResult<ResolvedType> {
    let mut read_only_env = TypeEnv {
        scopes: env.scopes.clone(),
        moved_scopes: env.moved_scopes.clone(),
        types: env.types.clone(),
        type_origins: env.type_origins.clone(),
        trait_origins: env.trait_origins.clone(),
        globals: env.globals.clone(),
        global_origins: env.global_origins.clone(),
        moved_globals: env.moved_globals.clone(),
        methods: env.methods.clone(),
        method_origins: env.method_origins.clone(),
        enum_variants: env.enum_variants.clone(),
        actor_contracts: env.actor_contracts.clone(),
        loaded_stdlib_modules: env.loaded_stdlib_modules.clone(),
        stdlib_registration_depth: env.stdlib_registration_depth,
        entangle_endpoints: env.entangle_endpoints.clone(),
        shared_region_depth: env.shared_region_depth,
        fanout_depth: env.fanout_depth,
        in_converge: env.in_converge,
        in_entangle: env.in_entangle,
        in_patch: env.in_patch,
        in_world: env.in_world,
        in_comptime: env.in_comptime,
        in_orchestrate: env.in_orchestrate,
        relaxed_checks: env.relaxed_checks,
        span_mapper: env.span_mapper,
        filename: env.filename,
    };
    infer_expr_type(&mut read_only_env, expr, None)
}

fn resolved_type_from_contract_name(env: &TypeEnv, contract_name: &str) -> ResolvedType {
    match contract_name {
        "Unit" | "Void" | "()" => ResolvedType::Unit,
        "!" => ResolvedType::Never,
        "Unknown" => ResolvedType::Unknown,
        _ => env
            .lookup_type(contract_name)
            .cloned()
            .unwrap_or(ResolvedType::Unknown),
    }
}

fn infer_actor_ask_call_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    callee_name: &str,
    args: &[CallArg],
    span: Span,
) -> Option<KainResult<ResolvedType>> {
    if callee_name != "ask" && callee_name != "ask_timeout" {
        return None;
    }
    let expected_args = if callee_name == "ask" { 3 } else { 4 };
    if args.len() != expected_args {
        return Some(Err(env.type_error(
            format!(
                "{callee_name} expects {} argument(s), found {}",
                expected_args,
                args.len()
            ),
            span,
        )));
    }
    let target_ty = match infer_expr_type(env, &args[0].value, ctx) {
        Ok(ty) => ty,
        Err(error) => return Some(Err(error)),
    };
    let _ = match infer_expr_type(env, &args[1].value, ctx) {
        Ok(ty) => ty,
        Err(error) => return Some(Err(error)),
    };
    let _ = match infer_expr_type(env, &args[2].value, ctx) {
        Ok(ty) => ty,
        Err(error) => return Some(Err(error)),
    };
    if callee_name == "ask_timeout" {
        let timeout_ty = match infer_expr_type(env, &args[3].value, ctx) {
            Ok(ty) => ty,
            Err(error) => return Some(Err(error)),
        };
        if !types_compatible(&ResolvedType::Int(IntSize::I64), &timeout_ty) {
            return Some(Err(env.type_error(
                format!(
                    "ask_timeout timeout expects Int-compatible value, found {}",
                    describe_type(&timeout_ty)
                ),
                args[3].span,
            )));
        }
    }
    let Some(actor_name) = resolved_type_name(&target_ty) else {
        return Some(Ok(ResolvedType::Unknown));
    };
    let Expr::String(message_name, _) = &args[1].value else {
        return Some(Ok(ResolvedType::Unknown));
    };
    let Some(actor_contract) = env.actor_contract(actor_name) else {
        return Some(Ok(ResolvedType::Unknown));
    };
    let Some(handler) = actor_contract.handler(message_name) else {
        return Some(Ok(ResolvedType::Unknown));
    };
    let Some(reply) = &handler.message.reply else {
        return Some(Ok(ResolvedType::Unit));
    };
    let _ = span;
    Some(Ok(resolved_type_from_contract_name(env, &reply.type_name)))
}

fn check_test(env: &mut TypeEnv, t: &TestDef) -> KainResult<TypedTest> {
    let ctx = SemanticContext {
        function_name: format!("test::{}", t.name),
        return_type: ResolvedType::Unit,
        effects: EffectSet::new(),
    };
    env.with_scope(|env| check_block_semantics(env, &t.body, &ctx))?;
    Ok(TypedTest { ast: t.clone() })
}

fn validate_generic_constraints(
    env: &TypeEnv,
    generics: &[Generic],
    where_clause: Option<&WhereClause>,
    owner_kind: &str,
) -> KainResult<()> {
    let generic_names = generics
        .iter()
        .map(|generic| generic.name.as_str())
        .collect::<HashSet<_>>();
    let mut normalized = HashSet::new();

    for generic in generics {
        for bound in &generic.bounds {
            validate_trait_bound_exists(env, bound, owner_kind)?;
            let key = (generic.name.as_str(), bound.trait_name.as_str());
            if !normalized.insert(key) {
                return Err(env.type_error(
                    format!(
                        "duplicate generic bound '{}: {}' on {owner_kind}",
                        generic.name, bound.trait_name
                    ),
                    bound.span,
                ));
            }
        }
    }

    if let Some(where_clause) = where_clause {
        for where_bound in &where_clause.bounds {
            if !generic_names.contains(where_bound.generic_name.as_str()) {
                let available = generics
                    .iter()
                    .map(|generic| generic.name.as_str())
                    .collect::<Vec<_>>()
                    .join(", ");
                let detail = if available.is_empty() {
                    "this item has no generic parameters".to_string()
                } else {
                    format!("available generics: {available}")
                };
                return Err(env.type_error(
                    format!(
                        "where clause references unknown generic '{}' on {owner_kind}; {detail}",
                        where_bound.generic_name
                    ),
                    where_bound.span,
                ));
            }

            let mut local_bounds = HashSet::new();
            for bound in &where_bound.bounds {
                validate_trait_bound_exists(env, bound, owner_kind)?;
                if !local_bounds.insert(bound.trait_name.as_str()) {
                    return Err(env.type_error(
                        format!(
                            "duplicate where bound '{}: {}' on {owner_kind}",
                            where_bound.generic_name, bound.trait_name
                        ),
                        bound.span,
                    ));
                }
                let key = (where_bound.generic_name.as_str(), bound.trait_name.as_str());
                if !normalized.insert(key) {
                    return Err(env.type_error(
                        format!(
                            "duplicate generic bound '{}: {}' across inline bounds and where clause on {owner_kind}",
                            where_bound.generic_name, bound.trait_name
                        ),
                        bound.span,
                    ));
                }
            }
        }
    }

    Ok(())
}

fn validate_trait_bound_exists(
    env: &TypeEnv,
    bound: &TypeBound,
    owner_kind: &str,
) -> KainResult<()> {
    validate_trait_name_exists(env, &bound.trait_name, bound.span, owner_kind)
}

fn validate_trait_name_exists(
    env: &TypeEnv,
    trait_name: &str,
    span: Span,
    owner_kind: &str,
) -> KainResult<()> {
    if env.trait_exists(trait_name) {
        return Ok(());
    }
    Err(env.type_error(
        format!("{owner_kind} references unknown trait '{trait_name}'"),
        span,
    ))
}

fn check_impl(env: &mut TypeEnv, imp: &Impl) -> KainResult<TypedImpl> {
    validate_generic_constraints(env, &imp.generics, imp.where_clause.as_ref(), "impl")?;
    if let Some(trait_name) = &imp.trait_name {
        validate_trait_name_exists(env, trait_name, imp.span, "impl trait")?;
    }
    let self_ty = resolve_type_in_env(env, &imp.target_type)?;
    for method in &imp.methods {
        check_function_with_self(env, method, &self_ty)?;
    }
    Ok(TypedImpl { ast: imp.clone() })
}

/// Recursively check if a block (or any nested block) contains a bare `return`
/// statement with no expression.
fn block_has_bare_return(block: &Block) -> bool {
    for stmt in &block.stmts {
        if stmt_has_bare_return(stmt) {
            return true;
        }
    }
    false
}

fn stmt_has_bare_return(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Return(None, _) => return true,
        Stmt::Return(Some(expr), _) => return expr_has_bare_return(expr),
        Stmt::Expr(expr) => return expr_has_bare_return(expr),
        Stmt::For { body, .. } | Stmt::While { body, .. } | Stmt::Loop { body, .. } => {
            return block_has_bare_return(body);
        }
        Stmt::Let { value: Some(expr), .. } => return expr_has_bare_return(expr),
        Stmt::Defer { expr, .. } => return expr_has_bare_return(expr),
        _ => false,
    }
}

fn expr_has_bare_return(expr: &Expr) -> bool {
    match expr {
        Expr::If {
            then_branch,
            else_branch,
            ..
        } => {
            if block_has_bare_return(then_branch) {
                return true;
            }
            if let Some(else_branch) = else_branch {
                return else_branch_has_bare_return(else_branch);
            }
            false
        }
        Expr::Match { arms, .. } => {
            for arm in arms {
                if expr_has_bare_return(&arm.body) {
                    return true;
                }
            }
            false
        }
        Expr::Lambda { body, .. } => expr_has_bare_return(body),
        Expr::Block(inner_block, _) => block_has_bare_return(inner_block),
        _ => false,
    }
}

fn else_branch_has_bare_return(branch: &ElseBranch) -> bool {
    match branch {
        ElseBranch::Else(block) => block_has_bare_return(block),
        ElseBranch::ElseIf(cond, block, next) => {
            if block_has_bare_return(block) {
                return true;
            }
            // Also check the condition expression for nested blocks
            if expr_has_bare_return(cond) {
                return true;
            }
            if let Some(next) = next {
                return else_branch_has_bare_return(next);
            }
            false
        }
    }
}

/// Walk a block and collect the types of top-level `return expr` statements.
/// Nested returns (inside if/while/for) are not tracked for type inference —
/// the Unknown fallback handles those cases. Returns Unknown if no valued
/// returns are found.
/// When bare `return` (no expression) statements exist anywhere in the body
/// with no valued returns, the function return type is Unit.
fn infer_return_type_from_body(env: &mut TypeEnv, block: &Block) -> KainResult<ResolvedType> {
    let mut inferred = ResolvedType::Unknown;
    let mut has_bare_return = false;
    for stmt in &block.stmts {
        if let Stmt::Return(Some(expr), _) = stmt {
            let expr_ty = infer_expr_type(env, expr, None)?;
            if inferred == ResolvedType::Unknown {
                inferred = expr_ty;
            } else if !types_compatible(&inferred, &expr_ty) {
                // Conflicting return types — keep Unknown
                return Ok(ResolvedType::Unknown);
            }
        } else if let Stmt::Return(None, _) = stmt {
            has_bare_return = true;
        }
    }
    if inferred == ResolvedType::Unknown && has_bare_return {
        return Ok(ResolvedType::Unit);
    }
    Ok(inferred)
}

fn check_function(env: &mut TypeEnv, f: &Function) -> KainResult<TypedFunction> {
    validate_generic_constraints(env, &f.generics, f.where_clause.as_ref(), "function")?;
    let mut resolved_type = function_signature(env, f, None)?;
    let effects = match &resolved_type {
        ResolvedType::Function { effects, .. } => effects.clone(),
        _ => EffectSet::new(),
    };
    let mut ret = match &resolved_type {
        ResolvedType::Function { ret, .. } => ret.as_ref().clone(),
        _ => ResolvedType::Unit,
    };
    let had_explicit_return_type = f.return_type.is_some();
    validate_function_attributes(env, f, &ret)?;

    let ctx = SemanticContext {
        function_name: f.name.clone(),
        return_type: ret.clone(),
        effects: effects.clone(),
    };

    env.with_scope(|env| {
        for p in &f.params {
            let ty = resolve_param_type(env, p, None)?;
            env.define(p.name.clone(), ty);
        }
        check_block_semantics(env, &f.body, &ctx)
    })?;

    // Infer return type from body when no explicit annotation was given.
    // Collect return expression types and unify; fall back to Unknown if
    // the body has no return statements (e.g. functions that just produce a
    // final expression value or diverge).
    if !had_explicit_return_type {
        let inferred = infer_return_type_from_body(env, &f.body)?;
        if inferred != ResolvedType::Unknown {
            ret = inferred;
        } else if block_has_bare_return(&f.body) {
            // No valued returns found, but bare returns exist (possibly nested
            // inside if/while/match blocks). Default to Unit so codegen emits
            // a void function rather than i64 + ret void mismatch.
            ret = ResolvedType::Unit;
        }
        if ret != (match &resolved_type {
            ResolvedType::Function { ret, .. } => ret.as_ref().clone(),
            _ => ResolvedType::Unit,
        }) {
            resolved_type = ResolvedType::Function {
                params: match &resolved_type {
                    ResolvedType::Function { params, .. } => params.clone(),
                    _ => vec![],
                },
                ret: Box::new(ret.clone()),
                effects: effects.clone(),
            };
        }
    }

    Ok(TypedFunction {
        ast: f.clone(),
        resolved_type,
        effects,
    })
}

fn check_function_with_self(
    env: &mut TypeEnv,
    f: &Function,
    self_ty: &ResolvedType,
) -> KainResult<TypedFunction> {
    validate_generic_constraints(env, &f.generics, f.where_clause.as_ref(), "method")?;
    let resolved_type = function_signature(env, f, Some(self_ty))?;
    let effects = match &resolved_type {
        ResolvedType::Function { effects, .. } => effects.clone(),
        _ => EffectSet::new(),
    };
    let ret = match &resolved_type {
        ResolvedType::Function { ret, .. } => ret.as_ref().clone(),
        _ => ResolvedType::Unit,
    };
    validate_function_attributes(env, f, &ret)?;
    let ctx = SemanticContext {
        function_name: f.name.clone(),
        return_type: ret,
        effects: effects.clone(),
    };

    env.with_scope(|env| {
        for p in &f.params {
            let ty = resolve_param_type(env, p, Some(self_ty))?;
            env.define(p.name.clone(), ty);
        }
        check_block_semantics(env, &f.body, &ctx)
    })?;

    Ok(TypedFunction {
        ast: f.clone(),
        resolved_type,
        effects,
    })
}

fn patch_function_view(patch: &PatchDef) -> Function {
    Function {
        name: patch.name.clone(),
        generics: vec![],
        where_clause: None,
        params: patch.params.clone(),
        return_type: patch.return_type.clone(),
        effects: vec![],
        body: patch.body.clone(),
        visibility: patch.visibility,
        attributes: patch.attributes.clone(),
        span: patch.span,
    }
}

fn law_function_view(law: &LawDef) -> Function {
    Function {
        name: law.name.clone(),
        generics: vec![],
        where_clause: None,
        params: law.params.clone(),
        return_type: Some(law.return_type.clone()),
        effects: vec![],
        body: law.body.clone(),
        visibility: law.visibility,
        attributes: law.attributes.clone(),
        span: law.span,
    }
}

fn converge_dispatcher_view(converge: &ConvergeDef) -> Function {
    Function {
        name: converge.name.clone(),
        generics: vec![],
        where_clause: None,
        params: converge.params.clone(),
        return_type: converge.return_type.clone(),
        effects: vec![],
        body: Block {
            stmts: vec![],
            span: converge.span,
        },
        visibility: converge.visibility,
        attributes: converge.attributes.clone(),
        span: converge.span,
    }
}

fn converge_lane_function_view(converge: &ConvergeDef, lane: &ConvergeLane) -> Function {
    Function {
        name: format!("__kain_converge__{}__{}", converge.name, lane.lane_name),
        generics: vec![],
        where_clause: None,
        params: converge.params.clone(),
        return_type: converge.return_type.clone(),
        effects: vec![],
        body: lane.body.clone(),
        visibility: converge.visibility,
        attributes: converge.attributes.clone(),
        span: lane.span,
    }
}

fn orchestrate_function_view(orchestrate: &OrchestrateDef) -> Function {
    Function {
        name: orchestrate.name.clone(),
        generics: vec![],
        where_clause: None,
        params: orchestrate.params.clone(),
        return_type: orchestrate.return_type.clone(),
        effects: vec![],
        body: orchestrate.body.clone(),
        visibility: orchestrate.visibility,
        attributes: orchestrate.attributes.clone(),
        span: orchestrate.span,
    }
}

fn check_patch(env: &mut TypeEnv, patch: &PatchDef) -> KainResult<TypedPatch> {
    let old_patch = env.in_patch;
    env.in_patch = true;
    let res = (|| {
        let typed_fn = check_function(env, &patch_function_view(patch))?;
        let mutation_paths = collect_patch_mutation_paths_from_block(&patch.body);
        let undo_mode = if patch_body_requires_best_effort(&patch.body) {
            PatchUndoMode::BestEffort
        } else {
            PatchUndoMode::Reversible
        };
        Ok(TypedPatch {
            ast: patch.clone(),
            resolved_type: typed_fn.resolved_type,
            effects: typed_fn.effects,
            mutation_paths,
            undo_mode,
        })
    })();
    env.in_patch = old_patch;
    res
}

fn check_law(env: &mut TypeEnv, law: &LawDef) -> KainResult<TypedLaw> {
    let old_patch = env.in_patch;
    env.in_patch = true;
    let res = (|| {
        let typed_fn = check_function(env, &law_function_view(law))?;
        let resolved_return_type = resolve_type_in_env(env, &law.return_type)?;
        if resolved_return_type != ResolvedType::Bool {
            return Err(env.type_error(
                format!(
                    "law '{}' must return Bool, found {}",
                    law.name,
                    describe_type(&resolved_return_type)
                ),
                law.return_type.span(),
            ));
        }
        Ok(TypedLaw {
            ast: law.clone(),
            resolved_type: typed_fn.resolved_type,
            effects: typed_fn.effects,
        })
    })();
    env.in_patch = old_patch;
    res
}

fn check_axiom(env: &mut TypeEnv, axiom: &AxiomDef) -> KainResult<TypedAxiom> {
    if axiom.predicates.is_empty() {
        return Err(env.type_error(
            format!(
                "axiom '{}' must declare at least one machine predicate",
                axiom.name
            ),
            axiom.span,
        ));
    }
    if axiom.guarantees.is_empty() {
        return Err(env.type_error(
            format!("axiom '{}' must declare at least one guarantee", axiom.name),
            axiom.span,
        ));
    }
    if axiom.fallback.as_deref().map(str::is_empty).unwrap_or(true) {
        return Err(env.type_error(
            format!(
                "axiom '{}' must declare a portable fallback so unsupported machines stay sound",
                axiom.name
            ),
            axiom.span,
        ));
    }

    let mut seen_predicates = HashSet::new();
    for predicate in &axiom.predicates {
        let key = (predicate.kind(), predicate.value().to_string());
        if !seen_predicates.insert(key) {
            return Err(env.type_error(
                format!(
                    "axiom '{}' repeats predicate {}",
                    axiom.name,
                    predicate.authored()
                ),
                axiom.span,
            ));
        }
    }

    Ok(TypedAxiom { ast: axiom.clone() })
}

fn check_pulse(env: &mut TypeEnv, pulse: &PulseDef) -> KainResult<TypedPulse> {
    validate_pulse_duration(env, &pulse.interval, "pulse interval")?;
    if let Some(jitter) = &pulse.jitter {
        validate_pulse_duration(env, jitter, "pulse jitter")?;
    }

    let ctx = SemanticContext {
        function_name: pulse.name.clone(),
        return_type: ResolvedType::Unit,
        effects: EffectSet::new()
            .with(Effect::IO)
            .with(Effect::Async)
            .with(Effect::GPU)
            .with(Effect::Reactive)
            .with(Effect::Unsafe)
            .with(Effect::Alloc)
            .with(Effect::Panic),
    };
    env.with_scope(|env| {
        env.define("pulse_tick".to_string(), ResolvedType::Int(IntSize::I64));
        env.define("pulse_dt_ms".to_string(), ResolvedType::Int(IntSize::I64));
        env.define("pulse_missed".to_string(), ResolvedType::Int(IntSize::I64));
        check_block_semantics(env, &pulse.body, &ctx)
    })?;

    if let Some(budget) = &pulse.budget {
        verify_pulse_budget(env, &pulse.name, budget, &pulse.body)?;
    }

    Ok(TypedPulse { ast: pulse.clone() })
}

/// Verify that a `pulse` callback body respects budget constraints.
///
/// Walks the body AST and rejects any direct call to forbidden operations.
/// Phase 1 rule: only reject direct calls by name — no interprocedural analysis.
fn verify_pulse_budget(
    env: &TypeEnv,
    pulse_name: &str,
    budget: &PulseBudget,
    body: &Block,
) -> KainResult<()> {
    let alloc_limit = budget.alloc;
    let lock_limit = budget.lock;
    let io_limit = budget.io;

    let mut state = BudgetVerifyState {
        alloc_count: 0,
        lock_count: 0,
        io_count: 0,
    };

    verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, body, &mut state)?;
    Ok(())
}

struct BudgetVerifyState {
    alloc_count: u32,
    lock_count: u32,
    io_count: u32,
}

fn verify_block_budget(
    env: &TypeEnv,
    pulse_name: &str,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    block: &Block,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    for stmt in &block.stmts {
        verify_stmt_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, stmt, state)?;
    }
    Ok(())
}

fn verify_stmt_budget(
    env: &TypeEnv,
    pulse_name: &str,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    stmt: &Stmt,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    match stmt {
        Stmt::Expr(expr) | Stmt::Defer { expr, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, expr, state)?;
        }
        Stmt::Let { value, .. } => {
            if let Some(val) = value {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, val, state)?;
            }
        }
        Stmt::Dispatch { dispatch_size, .. } => {
            dispatch_size_for_each(dispatch_size, |d| {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, d, state)
            })?;
        }
        Stmt::Return(Some(expr), _) => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, expr, state)?;
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, iter, state)?;
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, body, state)?;
        }
        Stmt::While { condition, body, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, condition, state)?;
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, body, state)?;
        }
        Stmt::Loop { body, .. } => {
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, body, state)?;
        }
        Stmt::Item(item) => {
            verify_item_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, item, state)?;
        }
        _ => {}
    }
    Ok(())
}

fn verify_item_budget(
    env: &TypeEnv,
    pulse_name: &str,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    item: &Item,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    match item {
        Item::Function(func) => {
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &func.body, state)?;
        }
        Item::Const(c) => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &c.value, state)?;
        }
        Item::Comptime(comptime) => {
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &comptime.body, state)?;
        }
        _ => {}
    }
    Ok(())
}

fn verify_expr_budget(
    env: &TypeEnv,
    pulse_name: &str,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    expr: &Expr,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    match expr {
        Expr::Call { callee, args, span } => {
            // Check direct call by function name
            if let Expr::Ident(name, _) = callee.as_ref() {
                check_call_budget(env, pulse_name, name, span, alloc_limit, lock_limit, io_limit, state)?;
            }
            // Also check the callee itself (could be a method call chain, etc.)
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, callee, state)?;
            for arg in args {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &arg.value, state)?;
            }
        }
        Expr::MethodCall { receiver, method, args, span } => {
            check_method_call_budget(env, pulse_name, method, span, alloc_limit, lock_limit, io_limit, state)?;
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, receiver, state)?;
            for arg in args {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &arg.value, state)?;
            }
        }
        Expr::Collapse { target, .. } | Expr::Decay { target, .. } | Expr::Observe { target, .. } => {
            check_lock_budget(env, pulse_name, &callee_name_for_ownership(expr), span_of_expr(expr), lock_limit, state)?;
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, target, state)?;
        }
        Expr::Binary { left, right, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, left, state)?;
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, right, state)?;
        }
        Expr::Unary { operand, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, operand, state)?;
        }
        Expr::Assign { target, value, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, target, state)?;
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, value, state)?;
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, condition, state)?;
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, then_branch, state)?;
            if let Some(eb) = else_branch {
                match eb.as_ref() {
                    ElseBranch::Else(block) => verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, block, state)?,
                    ElseBranch::ElseIf(cond, block, rest) => {
                        verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, cond, state)?;
                        verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, block, state)?;
                        if let Some(r) = rest {
                            match r.as_ref() {
                                ElseBranch::Else(b) => verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, b, state)?,
                                ElseBranch::ElseIf(c, b, r2) => {
                                    verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, c, state)?;
                                    verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, b, state)?;
                                    // Only handle 2 levels deep for else-if chains in Phase 1
                                    if let Some(rr) = r2 {
                                        match rr.as_ref() {
                                            ElseBranch::Else(bb) => verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, bb, state)?,
                                            _ => {}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, scrutinee, state)?;
            for arm in arms {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &arm.body, state)?;
                if let Some(guard) = &arm.guard {
                    verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, guard, state)?;
                }
            }
        }
        Expr::Block(block, _) => {
            verify_block_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, block, state)?;
        }
        Expr::Lambda { body, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, body, state)?;
        }
        Expr::Index { object, index, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, object, state)?;
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, index, state)?;
        }
        Expr::Range { start, end, .. } => {
            if let Some(s) = start {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, s, state)?;
            }
            if let Some(e) = end {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, e, state)?;
            }
        }
        Expr::Array(elems, _) | Expr::Tuple(elems, _) | Expr::FString(elems, _) => {
            for elem in elems {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, elem, state)?;
            }
        }
        Expr::Struct { fields, rest, .. } => {
            for (_, val) in fields {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, val, state)?;
            }
            if let Some(r) = rest {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, r, state)?;
            }
        }
        Expr::AggregateInit { fields, .. } => {
            for (_, val) in fields {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, val, state)?;
            }
        }
        Expr::EnumVariant { fields, .. } => {
            match &fields {
                EnumVariantFields::Unit => {}
                EnumVariantFields::Tuple(elems) => {
                    for val in elems {
                        verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, val, state)?;
                    }
                }
                EnumVariantFields::Struct(fields) => {
                    for (_, val) in fields {
                        verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, val, state)?;
                    }
                }
            }
        }
        Expr::Ref { value, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, value, state)?;
        }
        Expr::Cast { value, .. } => {
            verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, value, state)?;
        }
        Expr::StageCall { args, .. } => {
            for arg in args {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, &arg.value, state)?;
            }
        }
        Expr::MacroCall { args, .. } => {
            for arg in args {
                verify_expr_budget(env, pulse_name, alloc_limit, lock_limit, io_limit, arg, state)?;
            }
        }
        // Leaf expressions — nothing to recurse into
        Expr::Int(..) | Expr::Float(..) | Expr::String(..) | Expr::Bool(..) | Expr::None(_)
        | Expr::Ident(..) | Expr::Field { .. } => {}
        _ => {
            // For any new expression kind we miss, be conservative and don't error.
            // Phase 1: only reject known-forbidden patterns.
        }
    }
    Ok(())
}

/// Known function names that allocate heap memory.
const ALLOC_FORBIDDEN_NAMES: &[&str] = &[
    "alloc",
    "alloc_zeroed",
    "realloc_mem",
];

/// Known function names that acquire locks or perform atomic compare-exchange.
const LOCK_FORBIDDEN_NAMES: &[&str] = &[
    "atomic_flag_test_and_set_explicit",
    "atomic_compare_exchange_strong_explicit",
    "mcs_mutex_lock",
    "rwlock_write_lock",
    "rwlock_read_lock",
];

/// Known I/O function names.
const IO_FORBIDDEN_PREFIXES: &[&str] = &[
    "print",
    "println",
    "fs_",
    "io_",
    "network_",
    "file_",
];

/// Known I/O function names (exact match).
const IO_FORBIDDEN_NAMES: &[&str] = &[
    "import",
    "include",
];

fn check_call_budget(
    env: &TypeEnv,
    pulse_name: &str,
    name: &str,
    span: &Span,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    // Check alloc
    if let Some(limit) = alloc_limit {
        if ALLOC_FORBIDDEN_NAMES.contains(&name) {
            state.alloc_count += 1;
            if limit == 0 || state.alloc_count > limit {
                return Err(env.type_error_with_code(
                    DiagnosticCode::PulseBudgetAlloc,
                    format!(
                        "allocation in pulse with budget(alloc=0): '{}' in pulse '{}'\n  = note: pulse '{}' budget(alloc={}) forbids allocations\n  = help: pre-allocate buffers before the pulse starts, or increase budget",
                        span_source_snippet(env, span),
                        pulse_name,
                        pulse_name,
                        limit
                    ),
                    *span,
                ));
            }
            return Ok(());
        }
    }

    // Check lock
    if let Some(limit) = lock_limit {
        if is_lock_forbidden(name) {
            state.lock_count += 1;
            if limit == 0 || state.lock_count > limit {
                return Err(env.type_error_with_code(
                    DiagnosticCode::PulseBudgetLock,
                    format!(
                        "locking operation in pulse with budget(lock=0): '{}' in pulse '{}'\n  = note: pulse '{}' budget(lock={}) forbids locking operations\n  = help: move lock acquisition outside the pulse body, or increase budget",
                        span_source_snippet(env, span),
                        pulse_name,
                        pulse_name,
                        limit
                    ),
                    *span,
                ));
            }
            return Ok(());
        }
    }

    // Check IO
    if let Some(limit) = io_limit {
        if is_io_forbidden(name) {
            state.io_count += 1;
            if limit == 0 || state.io_count > limit {
                return Err(env.type_error_with_code(
                    DiagnosticCode::PulseBudgetIO,
                    format!(
                        "I/O operation in pulse with budget(io=0): '{}' in pulse '{}'\n  = note: pulse '{}' budget(io={}) forbids I/O operations\n  = help: perform I/O during initialization, not in the callback, or increase budget",
                        span_source_snippet(env, span),
                        pulse_name,
                        pulse_name,
                        limit
                    ),
                    *span,
                ));
            }
            return Ok(());
        }
    }

    Ok(())
}

fn check_method_call_budget(
    env: &TypeEnv,
    pulse_name: &str,
    method: &str,
    span: &Span,
    alloc_limit: Option<u32>,
    lock_limit: Option<u32>,
    io_limit: Option<u32>,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    // Method calls might be on std types — apply same checks
    check_call_budget(env, pulse_name, method, span, alloc_limit, lock_limit, io_limit, state)
}

fn check_lock_budget(
    env: &TypeEnv,
    pulse_name: &str,
    _op_name: &str,
    span: Span,
    lock_limit: Option<u32>,
    state: &mut BudgetVerifyState,
) -> KainResult<()> {
    if let Some(limit) = lock_limit {
        state.lock_count += 1;
        if limit == 0 || state.lock_count > limit {
            return Err(env.type_error_with_code(
                DiagnosticCode::PulseBudgetLock,
                format!(
                    "ownership operation in pulse with budget(lock=0): '{}' in pulse '{}'\n  = note: pulse '{}' budget(lock={}) forbids ownership operations\n  = help: pre-allocate buffers on the main thread and use non-owning views",
                    span_source_snippet(env, &span),
                    pulse_name,
                    pulse_name,
                    limit
                ),
                span,
            ));
        }
    }
    Ok(())
}

fn is_lock_forbidden(name: &str) -> bool {
    LOCK_FORBIDDEN_NAMES.contains(&name)
}

fn is_io_forbidden(name: &str) -> bool {
    if IO_FORBIDDEN_NAMES.contains(&name) {
        return true;
    }
    for prefix in IO_FORBIDDEN_PREFIXES {
        if name.starts_with(prefix) {
            return true;
        }
    }
    false
}

fn callee_name_for_ownership(expr: &Expr) -> String {
    match expr {
        Expr::Collapse { .. } => "collapse".to_string(),
        Expr::Observe { .. } => "observe".to_string(),
        Expr::Decay { .. } => "decay".to_string(),
        _ => "unknown".to_string(),
    }
}

fn span_of_expr(expr: &Expr) -> Span {
    match expr {
        Expr::Collapse { span, .. } | Expr::Observe { span, .. } | Expr::Decay { span, .. } => *span,
        Expr::Call { span, .. } => *span,
        _ => Span::new(0, 0),
    }
}

fn span_source_snippet(env: &TypeEnv, span: &Span) -> String {
    env.diagnostic_primary_text(*span)
}

fn check_resonate(env: &mut TypeEnv, resonate: &ResonateDef) -> KainResult<TypedResonate> {
    let target_type = resolve_resonate_endpoint_type(env, &resonate.target)?;
    let target_type_name = describe_type(&target_type);
    let direct_mutation_paths = collect_patch_mutation_paths_from_block(&resonate.body);
    let target = ResonanceTarget::new(resonate.target.segments.clone())
        .map_err(|error| env.type_error(error.to_string(), resonate.target.span))?;
    let dampen = match &resonate.dampen {
        Some(dampen) => DampenWindow::new(dampen.value, &dampen.unit)
            .map_err(|error| env.type_error(error.to_string(), dampen.span))?,
        None => DampenWindow::none(),
    };
    let plan = ResonancePlan::new(resonate.name.clone(), target, dampen, direct_mutation_paths);
    if plan.directly_mutates_target() {
        return Err(env.type_error(
            format!(
                "resonate '{}' directly mutates its own target '{}'; route feedback through a different patch/world path or dampened orchestrate stage",
                resonate.name,
                resonate.target.authored_path()
            ),
            resonate.span,
        ));
    }

    let ctx = SemanticContext {
        function_name: resonate.name.clone(),
        return_type: ResolvedType::Unit,
        effects: EffectSet::new()
            .with(Effect::IO)
            .with(Effect::Async)
            .with(Effect::GPU)
            .with(Effect::Reactive)
            .with(Effect::Unsafe)
            .with(Effect::Alloc)
            .with(Effect::Panic),
    };
    env.with_scope(|env| {
        env.define(
            "resonate_old_i64".to_string(),
            ResolvedType::Int(IntSize::I64),
        );
        env.define(
            "resonate_new_i64".to_string(),
            ResolvedType::Int(IntSize::I64),
        );
        env.define("resonate_fired".to_string(), ResolvedType::Bool);
        check_block_semantics(env, &resonate.body, &ctx)
    })?;

    Ok(TypedResonate {
        ast: resonate.clone(),
        target_type,
        target_type_name,
        plan,
    })
}

fn validate_pulse_duration(
    env: &TypeEnv<'_>,
    duration: &PulseDuration,
    label: &str,
) -> KainResult<()> {
    if duration.value <= 0 {
        return Err(env.type_error(format!("{label} must be greater than zero"), duration.span));
    }
    if !pulse_duration_unit_is_valid(&duration.unit) {
        return Err(env.type_error(
            format!(
                "{label} uses unsupported unit '{}'; expected ns, us, ms, s, tick, or ticks",
                duration.unit
            ),
            duration.span,
        ));
    }
    Ok(())
}

fn pulse_duration_unit_is_valid(unit: &str) -> bool {
    matches!(unit, "ns" | "us" | "ms" | "s" | "tick" | "ticks")
}

fn check_converge(env: &mut TypeEnv, converge: &ConvergeDef) -> KainResult<TypedConverge> {
    let old_converge = env.in_converge;
    env.in_converge = true;
    let res = (|| {
        let resolved_type = function_signature(env, &converge_dispatcher_view(converge), None)?;
        let expected_signature = resolved_type.clone();
        if converge.verify_random_count.is_some() {
            ensure_converge_verify_types_supported(env, converge, &expected_signature)?;
        }
        check_function(
            env,
            &converge_lane_function_view(converge, &converge.spec_lane),
        )?;
        for lane in &converge.fast_lanes {
            check_function(env, &converge_lane_function_view(converge, lane))?;
            let lane_signature =
                function_signature(env, &converge_lane_function_view(converge, lane), None)?;
            if lane_signature != expected_signature {
                if env.relaxed_checks {
                    // In interpret mode, lane signature mismatches are warnings.
                    // The tree-walk interpreter handles lane dispatch through
                    // the runtime selector rather than compile-time matching.
                    eprintln!(
                        "note: converge lane '{}' signature does not match dispatcher (tolerated in interpret mode)",
                        lane.lane_name
                    );
                } else {
                    return Err(env.type_error(
                        format!(
                            "Converge lane '{}' does not match dispatcher signature",
                            lane.lane_name
                        ),
                        lane.span,
                    ));
                }
            }
        }
        Ok(TypedConverge {
            ast: converge.clone(),
            resolved_type,
        })
    })();
    env.in_converge = old_converge;
    res
}

fn check_world(env: &mut TypeEnv, world: &WorldDef) -> KainResult<TypedWorld> {
    let old_world = env.in_world;
    env.in_world = true;
    let res = (|| {
        let mut state_types = HashMap::new();
        let mut seen_surface_kinds = HashSet::new();
        for state in &world.states {
            let resolved_ty = resolve_type_in_env(env, &state.ty)?;
            let initial_ty = infer_expr_type(env, &state.initial, None)?;
            ensure_type_compatible(
                env,
                &resolved_ty,
                &initial_ty,
                state.initial.span(),
                "world state initializer",
            )?;
            state_types.insert(state.name.clone(), resolved_ty);
        }
        let world_ty = ResolvedType::Struct(world.name.clone(), state_types);
        env.types.insert(world.name.clone(), world_ty.clone());
        env.define_global(world.name.clone(), world_ty);
        // Worlds without surfaces are valid: they are pure state authorities.
        // The codegen emits no frame loop when there are no surfaces, so no
        // window is created. Use this for benchmarks, CI, server-mode, or any
        // world that only holds state for entangle/patch/law/orchestrate.
        // When a surface IS declared, it controls rendering intent — the
        // surface kind (native_ui, web, viewport3d, ue5) determines which
        // backend the frame loop resolves at runtime.
        for surface in &world.surfaces {
            if !seen_surface_kinds.insert(surface.kind) {
                return Err(env.type_error(
                    format!(
                        "world '{}' declares duplicate '{}' surface",
                        world.name,
                        surface.kind.as_str()
                    ),
                    surface.span,
                ));
            }
            check_world_surface_projection(env, surface)?;
        }
        Ok(TypedWorld { ast: world.clone() })
    })();
    env.in_world = old_world;
    res
}

fn check_entangle(env: &mut TypeEnv, entangle: &EntangleDef) -> KainResult<TypedEntangle> {
    let old_entangle = env.in_entangle;
    env.in_entangle = true;
    let res = (|| {
        let left_path = entangle.left.authored_path();
        let right_path = entangle.right.authored_path();
        for path in [&left_path, &right_path] {
            if !env.entangle_endpoints.insert(path.clone()) {
                return Err(env.type_error(
                    format!("entangle endpoint '{path}' is already coupled"),
                    entangle.span,
                ));
            }
        }

        let left_ty = resolve_entangle_endpoint_type(env, &entangle.left)?;
        let right_ty = resolve_entangle_endpoint_type(env, &entangle.right)?;
        if peel_shared_refs(&left_ty) != peel_shared_refs(&right_ty) {
            return Err(env.type_error(
                format!(
                    "entangle endpoint expected {}, found {}",
                    describe_type(&left_ty),
                    describe_type(&right_ty)
                ),
                entangle.right.span,
            ));
        }

        Ok(TypedEntangle {
            ast: entangle.clone(),
            endpoint_type_name: describe_type(&left_ty),
            endpoint_type: left_ty,
        })
    })();
    env.in_entangle = old_entangle;
    res
}

fn resolve_resonate_endpoint_type(
    env: &TypeEnv,
    endpoint: &ResonateEndpoint,
) -> KainResult<ResolvedType> {
    let root = endpoint
        .segments
        .first()
        .ok_or_else(|| env.type_error("resonate target is empty", endpoint.span))?;
    match env.global_origins.get(root) {
        Some(origin) if origin.kind == "world" => {}
        Some(origin) => {
            return Err(env.type_error(
                format!(
                    "resonate target root '{root}' must be a world, found {}",
                    origin.kind
                ),
                endpoint.span,
            ))
        }
        None => {
            return Err(env.type_error(
                format!("resonate target root '{root}' is not defined"),
                endpoint.span,
            ))
        }
    }
    let entangle_endpoint = EntangleEndpoint {
        segments: endpoint.segments.clone(),
        span: endpoint.span,
    };
    resolve_entangle_endpoint_type(env, &entangle_endpoint)
}

fn resolve_entangle_endpoint_type(
    env: &TypeEnv,
    endpoint: &EntangleEndpoint,
) -> KainResult<ResolvedType> {
    let root = endpoint
        .segments
        .first()
        .ok_or_else(|| env.type_error("entangle endpoint is empty", endpoint.span))?;
    let mut current = env.lookup(root).cloned().ok_or_else(|| {
        env.type_error(
            format!("entangle endpoint root '{root}' is not defined"),
            endpoint.span,
        )
    })?;

    for segment in endpoint.segments.iter().skip(1) {
        current = match peel_shared_refs(&current) {
            ResolvedType::Struct(type_name, fields) => {
                fields.get(segment).cloned().ok_or_else(|| {
                    env.type_error(
                        format!(
                            "entangle endpoint '{}' has no field '{}' on {}",
                            endpoint.authored_path(),
                            segment,
                            type_name
                        ),
                        endpoint.span,
                    )
                })?
            }
            other => {
                return Err(env.type_error(
                    format!(
                    "entangle endpoint '{}' is not an assignable struct path; '{}' resolves to {}",
                    endpoint.authored_path(),
                    segment,
                    describe_type(other)
                ),
                    endpoint.span,
                ))
            }
        };
    }

    Ok(current)
}

fn check_orchestrate(
    env: &mut TypeEnv,
    orchestrate: &OrchestrateDef,
) -> KainResult<TypedOrchestrate> {
    let old_in_orchestrate = env.in_orchestrate;
    env.in_orchestrate = true;
    let res = (|| {
        let typed_fn = check_function(env, &orchestrate_function_view(orchestrate))?;
        let stages = collect_orchestrate_stage_descriptors(env, orchestrate)?;
        let graph = build_orchestrate_graph_plan(env, orchestrate, &stages)?;
        Ok(TypedOrchestrate {
            ast: orchestrate.clone(),
            resolved_type: typed_fn.resolved_type,
            stages,
            graph,
        })
    })();
    env.in_orchestrate = old_in_orchestrate;
    res
}

fn build_orchestrate_graph_plan(
    env: &TypeEnv,
    orchestrate: &OrchestrateDef,
    stages: &[OrchestrateStageDescriptor],
) -> KainResult<OrchestrateGraphPlan> {
    let mut graph = OrchestrateGraphPlan::new(orchestrate.name.clone());
    for stage in stages {
        validate_orchestrate_stage_metadata(env, orchestrate, stage)?;
        graph.push_stage(OrchestrateStagePlan {
            binding_name: stage.binding_name.clone(),
            kind: stage.runtime,
            function: stage.function.clone(),
            selector: stage.selector.clone(),
            metadata: stage.metadata.clone(),
        });
    }
    let validation = graph.validate();
    if !validation.valid {
        return Err(env.type_error(
            format!(
                "orchestrate '{}' graph validation failed: {}",
                orchestrate.name,
                validation.diagnostics.join("; ")
            ),
            orchestrate.span,
        ));
    }
    Ok(graph)
}

fn validate_orchestrate_stage_metadata(
    env: &TypeEnv,
    orchestrate: &OrchestrateDef,
    stage: &OrchestrateStageDescriptor,
) -> KainResult<()> {
    if let Some(guard) = &stage.metadata.guard {
        match env.global_origins.get(guard) {
            Some(origin) if origin.kind == "axiom" => {}
            Some(origin) => {
                if env.relaxed_checks {
                    eprintln!(
                        "note: orchestrate '{}' stage '{}' guard '{}' references a {}, not an axiom (tolerated in interpret mode)",
                        orchestrate.name, stage.binding_name, guard, origin.kind
                    );
                } else {
                    return Err(env.type_error(
                        format!(
                            "orchestrate '{}' stage '{}' guard '{}' must reference an axiom, found {}",
                            orchestrate.name, stage.binding_name, guard, origin.kind
                        ),
                        orchestrate.span,
                    ));
                }
            }
            None => {
                if env.relaxed_checks {
                    eprintln!(
                        "note: orchestrate '{}' stage '{}' guard '{}' does not resolve to an axiom (tolerated in interpret mode)",
                        orchestrate.name, stage.binding_name, guard
                    );
                } else {
                    return Err(env.type_error(
                        format!(
                            "orchestrate '{}' stage '{}' guard '{}' does not resolve to an axiom",
                            orchestrate.name, stage.binding_name, guard
                        ),
                        orchestrate.span,
                    ));
                }
            }
        }
    }

    if let Some(transfer) = stage.metadata.transfer {
        match (transfer, stage.metadata.residency) {
            (OrchestrateTransfer::HostToDevice, Some(OrchestrateResidency::Host))
            | (OrchestrateTransfer::DeviceToHost, Some(OrchestrateResidency::Device))
            | (OrchestrateTransfer::SharedView, Some(OrchestrateResidency::Host))
            | (OrchestrateTransfer::SharedView, Some(OrchestrateResidency::Device)) => {
                if env.relaxed_checks {
                    eprintln!(
                        "note: orchestrate '{}' stage '{}' transfer '{}' is incompatible with residency '{}' (tolerated in interpret mode)",
                        orchestrate.name,
                        stage.binding_name,
                        transfer.as_str(),
                        stage.metadata.residency_name()
                    );
                } else {
                    return Err(env.type_error(
                        format!(
                            "orchestrate '{}' stage '{}' transfer '{}' is incompatible with residency '{}'",
                            orchestrate.name,
                            stage.binding_name,
                            transfer.as_str(),
                            stage.metadata.residency_name()
                        ),
                        orchestrate.span,
                    ));
                }
            }
            _ => {}
        }
    }
    Ok(())
}

fn check_world_surface_projection(
    env: &mut TypeEnv,
    surface: &WorldSurfaceProjection,
) -> KainResult<()> {
    match surface.kind {
        WorldSurfaceKind::NativeUi | WorldSurfaceKind::Web => match &surface.expr {
            Expr::Ident(_, _) => Ok(()),
            Expr::Call { callee, .. } => match callee.as_ref() {
                Expr::Ident(_, _) => Ok(()),
                other => Err(env.type_error(
                    format!(
                        "world surface '{}' expects a component identifier or call, found {:?}",
                        surface.kind.as_str(),
                        other
                    ),
                    surface.span,
                )),
            },
            other => Err(env.type_error(
                format!(
                    "world surface '{}' expects a component identifier or call, found {:?}",
                    surface.kind.as_str(),
                    other
                ),
                surface.span,
            )),
        },
        WorldSurfaceKind::ShaderCanvas => match &surface.expr {
            Expr::Ident(_, _) => Ok(()),
            other => Err(env.type_error(
                format!(
                    "world surface 'shader_canvas' expects a shader fragment identifier, found {:?}",
                    other
                ),
                surface.span,
            )),
        },
        WorldSurfaceKind::Viewport3d | WorldSurfaceKind::Ue5 => match &surface.expr {
            Expr::Ident(_, _) | Expr::String(_, _) => Ok(()),
            other => Err(env.type_error(
                format!(
                    "world surface '{}' expects an identifier or string literal, found {:?}",
                    surface.kind.as_str(),
                    other
                ),
                surface.span,
            )),
        },
    }
}

fn collect_patch_mutation_paths_from_block(block: &Block) -> Vec<String> {
    let mut paths = Vec::new();
    for stmt in &block.stmts {
        collect_patch_mutation_paths_from_stmt(stmt, &mut paths);
    }
    paths.sort();
    paths.dedup();
    paths
}

fn collect_patch_mutation_paths_from_stmt(stmt: &Stmt, output: &mut Vec<String>) {
    match stmt {
        Stmt::Let { value, .. } => {
            if let Some(value) = value {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Stmt::Expr(expr) => collect_patch_mutation_paths_from_expr(expr, output),
        Stmt::Defer { expr, .. } => collect_patch_mutation_paths_from_expr(expr, output),
        Stmt::Dispatch { dispatch_size, .. } => {
            for expr in dispatch_size_exprs_ref(dispatch_size) {
                collect_patch_mutation_paths_from_expr(expr, output);
            }
        }
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            if let Some(value) = value {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            collect_patch_mutation_paths_from_expr(iter, output);
            for stmt in &body.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
        Stmt::While {
            condition, body, ..
        } => {
            collect_patch_mutation_paths_from_expr(condition, output);
            for stmt in &body.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
        Stmt::Loop { body, .. } => {
            for stmt in &body.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
        Stmt::Item(_) | Stmt::Continue(_) => {}
        Stmt::Subgroup { body, .. } => {
            for stmt in &body.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
    }
}

fn collect_patch_mutation_paths_from_expr(expr: &Expr, output: &mut Vec<String>) {
    match expr {
        Expr::Assign { target, value, .. } => {
            if let Some(path) = patch_target_path(target) {
                output.push(path);
            }
            collect_patch_mutation_paths_from_expr(value, output);
        }
        Expr::Call { callee, args, .. } => {
            collect_patch_mutation_paths_from_expr(callee, output);
            for arg in args {
                collect_patch_mutation_paths_from_expr(&arg.value, output);
            }
        }
        Expr::StageCall { args, .. } | Expr::MethodCall { args, .. } => {
            for arg in args {
                collect_patch_mutation_paths_from_expr(&arg.value, output);
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_patch_mutation_paths_from_expr(left, output);
            collect_patch_mutation_paths_from_expr(right, output);
        }
        Expr::Unary { operand, .. }
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Paren(operand, _) => collect_patch_mutation_paths_from_expr(operand, output),
        Expr::Field { object, .. } => collect_patch_mutation_paths_from_expr(object, output),
        Expr::Index { object, index, .. } => {
            collect_patch_mutation_paths_from_expr(object, output);
            collect_patch_mutation_paths_from_expr(index, output);
        }
        Expr::Struct { fields, rest, .. } => {
            for (_, value) in fields {
                collect_patch_mutation_paths_from_expr(value, output);
            }
            if let Some(rest) = rest {
                collect_patch_mutation_paths_from_expr(rest, output);
            }
        }
        Expr::AggregateInit { fields, .. } => {
            for (_, value) in fields {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Tuple(values) => {
                for value in values {
                    collect_patch_mutation_paths_from_expr(value, output);
                }
            }
            EnumVariantFields::Struct(values) => {
                for (_, value) in values {
                    collect_patch_mutation_paths_from_expr(value, output);
                }
            }
            EnumVariantFields::Unit => {}
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) => {
            for value in values {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Expr::Range { start, end, .. } => {
            if let Some(start) = start {
                collect_patch_mutation_paths_from_expr(start, output);
            }
            if let Some(end) = end {
                collect_patch_mutation_paths_from_expr(end, output);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_patch_mutation_paths_from_expr(condition, output);
            for stmt in &then_branch.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
            if let Some(else_branch) = else_branch {
                collect_patch_mutation_paths_from_else_branch(else_branch, output);
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            collect_patch_mutation_paths_from_expr(scrutinee, output);
            for arm in arms {
                if let Some(guard) = &arm.guard {
                    collect_patch_mutation_paths_from_expr(guard, output);
                }
                collect_patch_mutation_paths_from_expr(&arm.body, output);
            }
        }
        Expr::Lambda { body, .. } => collect_patch_mutation_paths_from_expr(body, output),
        Expr::MemStore { pointer, value, .. } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
            collect_patch_mutation_paths_from_expr(value, output);
        }
        Expr::VolatileLoad { pointer, .. } | Expr::AtomicLoad { pointer, .. } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
        }
        Expr::VolatileStore { pointer, value, .. }
        | Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
            collect_patch_mutation_paths_from_expr(value, output);
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
            collect_patch_mutation_paths_from_expr(expected, output);
            collect_patch_mutation_paths_from_expr(desired, output);
        }
        Expr::AtomicFence { .. } => {}
        Expr::CpuFence { .. } => {}
        Expr::CpuCacheFlush { pointer, .. } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
        }
        Expr::InlineAsm { operands, .. } => {
            for operand in operands {
                collect_patch_mutation_paths_from_expr(operand, output);
            }
        }
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
            collect_patch_mutation_paths_from_expr(offset, output);
        }
        Expr::MemLoad { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        }
        | Expr::Teleport { value: pointer, .. }
        | Expr::Cast { value: pointer, .. }
        | Expr::Bitcast { value: pointer, .. }
        | Expr::Comptime(pointer, _) => collect_patch_mutation_paths_from_expr(pointer, output),
        Expr::Observe { target, body, .. }
        | Expr::Collapse { target, body, .. }
        | Expr::Share { target, body, .. } => {
            collect_patch_mutation_paths_from_expr(target, output);
            collect_patch_mutation_paths_from_expr(body, output);
        }
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } | Expr::Emit { data: init, .. } => {
            for (_, value) in init {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Expr::Return(value, _) | Expr::Break(value, _) => {
            if let Some(value) = value {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Expr::JSX(_, _)
        | Expr::MacroCall { .. }
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::FString(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Alloca { .. }
        | Expr::Uninit { .. }
        | Expr::Alloc { .. }
        | Expr::Realloc { .. }
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Continue(_) => {}
        Expr::Block(block, _) => {
            for stmt in &block.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
    }
}

fn collect_patch_mutation_paths_from_else_branch(branch: &ElseBranch, output: &mut Vec<String>) {
    match branch {
        ElseBranch::Else(block) => {
            for stmt in &block.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
        }
        ElseBranch::ElseIf(condition, block, next) => {
            collect_patch_mutation_paths_from_expr(condition, output);
            for stmt in &block.stmts {
                collect_patch_mutation_paths_from_stmt(stmt, output);
            }
            if let Some(next) = next {
                collect_patch_mutation_paths_from_else_branch(next, output);
            }
        }
    }
}

fn patch_target_path(target: &Expr) -> Option<String> {
    match target {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, field, .. } => {
            patch_target_path(object).map(|base| format!("{base}.{field}"))
        }
        Expr::Index { object, index, .. } => patch_target_path(object).map(|base| {
            let suffix = match index.as_ref() {
                Expr::Int(value, _) => format!("[{value}]"),
                _ => "[]".to_string(),
            };
            format!("{base}{suffix}")
        }),
        Expr::Deref(inner, _) => patch_target_path(inner),
        _ => None,
    }
}

fn patch_body_requires_best_effort(block: &Block) -> bool {
    block.stmts.iter().any(stmt_requires_best_effort_patch_mode)
}

fn stmt_requires_best_effort_patch_mode(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Expr(expr) => expr_requires_best_effort_patch_mode(expr),
        Stmt::Defer { expr, .. } => expr_requires_best_effort_patch_mode(expr),
        Stmt::Dispatch { dispatch_size, .. } => {
            dispatch_size_any(dispatch_size, expr_requires_best_effort_patch_mode)
        },
        Stmt::Let { value, .. } => value
            .as_ref()
            .is_some_and(expr_requires_best_effort_patch_mode),
        Stmt::Return(value, _) | Stmt::Break(value, _) => value
            .as_ref()
            .is_some_and(expr_requires_best_effort_patch_mode),
        Stmt::For { iter, body, .. } => {
            expr_requires_best_effort_patch_mode(iter)
                || body.stmts.iter().any(stmt_requires_best_effort_patch_mode)
        }
        Stmt::Fanout { .. } => true,
        Stmt::While {
            condition, body, ..
        } => {
            expr_requires_best_effort_patch_mode(condition)
                || body.stmts.iter().any(stmt_requires_best_effort_patch_mode)
        }
        Stmt::Loop { body, .. } => body.stmts.iter().any(stmt_requires_best_effort_patch_mode),
        Stmt::Item(_) | Stmt::Continue(_) => false,
        Stmt::Subgroup { body, .. } => body.stmts.iter().any(stmt_requires_best_effort_patch_mode),
    }
}

fn expr_requires_best_effort_patch_mode(expr: &Expr) -> bool {
    match expr {
        Expr::Call { .. }
        | Expr::MethodCall { .. }
        | Expr::StageCall { .. }
        | Expr::Spawn { .. }
        | Expr::SendMsg { .. }
        | Expr::Emit { .. }
        | Expr::Await(_, _)
        | Expr::AsyncBlock(_, _) => true,
        Expr::Assign { target, value, .. } => {
            expr_requires_best_effort_patch_mode(target)
                || expr_requires_best_effort_patch_mode(value)
        }
        Expr::Binary { left, right, .. } => {
            expr_requires_best_effort_patch_mode(left)
                || expr_requires_best_effort_patch_mode(right)
        }
        Expr::Unary { operand, .. }
        | Expr::Try(operand, _)
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Paren(operand, _)
        | Expr::Cast { value: operand, .. }
        | Expr::Bitcast { value: operand, .. }
        | Expr::Comptime(operand, _) => expr_requires_best_effort_patch_mode(operand),
        Expr::Field { object, .. } => expr_requires_best_effort_patch_mode(object),
        Expr::Index { object, index, .. } => {
            expr_requires_best_effort_patch_mode(object)
                || expr_requires_best_effort_patch_mode(index)
        }
        Expr::Struct { fields, rest, .. } => {
            fields
                .iter()
                .any(|(_, value)| expr_requires_best_effort_patch_mode(value))
                || rest
                    .as_ref()
                    .is_some_and(|rest| expr_requires_best_effort_patch_mode(rest))
        }
        Expr::AggregateInit { fields, .. } => fields
            .iter()
            .any(|(_, value)| expr_requires_best_effort_patch_mode(value)),
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Tuple(values) => {
                values.iter().any(expr_requires_best_effort_patch_mode)
            }
            EnumVariantFields::Struct(values) => values
                .iter()
                .any(|(_, value)| expr_requires_best_effort_patch_mode(value)),
            EnumVariantFields::Unit => false,
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) => {
            values.iter().any(expr_requires_best_effort_patch_mode)
        }
        Expr::Range { start, end, .. } => {
            start
                .as_ref()
                .is_some_and(|value| expr_requires_best_effort_patch_mode(value))
                || end
                    .as_ref()
                    .is_some_and(|value| expr_requires_best_effort_patch_mode(value))
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            expr_requires_best_effort_patch_mode(condition)
                || then_branch
                    .stmts
                    .iter()
                    .any(stmt_requires_best_effort_patch_mode)
                || else_branch
                    .as_ref()
                    .is_some_and(|branch| match branch.as_ref() {
                        ElseBranch::Else(block) => {
                            block.stmts.iter().any(stmt_requires_best_effort_patch_mode)
                        }
                        ElseBranch::ElseIf(condition, block, next) => {
                            expr_requires_best_effort_patch_mode(condition)
                                || block.stmts.iter().any(stmt_requires_best_effort_patch_mode)
                                || next.as_ref().is_some_and(|next| match next.as_ref() {
                                    ElseBranch::Else(block) => {
                                        block.stmts.iter().any(stmt_requires_best_effort_patch_mode)
                                    }
                                    ElseBranch::ElseIf(..) => true,
                                })
                        }
                    })
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            expr_requires_best_effort_patch_mode(scrutinee)
                || arms.iter().any(|arm| {
                    arm.guard
                        .as_ref()
                        .is_some_and(expr_requires_best_effort_patch_mode)
                        || expr_requires_best_effort_patch_mode(&arm.body)
                })
        }
        Expr::Lambda { body, .. } => expr_requires_best_effort_patch_mode(body),
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            expr_requires_best_effort_patch_mode(pointer)
                || expr_requires_best_effort_patch_mode(offset)
        }
        Expr::MemLoad { pointer, .. }
        | Expr::VolatileLoad { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        } => expr_requires_best_effort_patch_mode(pointer),
        Expr::Teleport { .. } => true,
        Expr::MemStore { pointer, value, .. } | Expr::VolatileStore { pointer, value, .. } => {
            expr_requires_best_effort_patch_mode(pointer)
                || expr_requires_best_effort_patch_mode(value)
        }
        Expr::AtomicLoad { pointer, .. } => expr_requires_best_effort_patch_mode(pointer),
        Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            expr_requires_best_effort_patch_mode(pointer)
                || expr_requires_best_effort_patch_mode(value)
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => {
            expr_requires_best_effort_patch_mode(pointer)
                || expr_requires_best_effort_patch_mode(expected)
                || expr_requires_best_effort_patch_mode(desired)
        }
        Expr::AtomicFence { .. } | Expr::CpuFence { .. } => true,
        Expr::CpuCacheFlush { .. } | Expr::InlineAsm { .. } => true,
        Expr::Observe { target, body, .. }
        | Expr::Collapse { target, body, .. }
        | Expr::Share { target, body, .. } => {
            expr_requires_best_effort_patch_mode(target)
                || expr_requires_best_effort_patch_mode(body)
        }
        Expr::Return(value, _) | Expr::Break(value, _) => value
            .as_ref()
            .is_some_and(|value| expr_requires_best_effort_patch_mode(value)),
        Expr::Block(block, _) => block.stmts.iter().any(stmt_requires_best_effort_patch_mode),
        Expr::JSX(_, _)
        | Expr::MacroCall { .. }
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::FString(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Alloca { .. }
        | Expr::Uninit { .. }
        | Expr::Alloc { .. }
        | Expr::Realloc { .. }
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Continue(_) => false,
    }
}

fn collect_orchestrate_stage_descriptors(
    env: &TypeEnv,
    orchestrate: &OrchestrateDef,
) -> KainResult<Vec<OrchestrateStageDescriptor>> {
    let mut stages = Vec::new();
    let mut seen_non_stage_stmt = false;
    for stmt in &orchestrate.body.stmts {
        match stmt {
            Stmt::Let {
                pattern:
                    Pattern::Binding {
                        name,
                        mutable: _,
                        span: _,
                    },
                value:
                    Some(Expr::StageCall {
                        runtime,
                        function,
                        selector,
                        metadata,
                        ..
                    }),
                ty: _,
                span,
            } => {
                if seen_non_stage_stmt {
                    return Err(env.type_error(
                        format!(
                            "orchestrate '{}' must declare stage steps before local computation",
                            orchestrate.name
                        ),
                        *span,
                    ));
                }
                stages.push(OrchestrateStageDescriptor {
                    runtime: *runtime,
                    function: function.clone(),
                    binding_name: name.clone(),
                    selector: selector.clone(),
                    metadata: metadata.clone(),
                });
            }
            Stmt::Let {
                value: Some(Expr::StageCall { span, .. }),
                ..
            } => {
                return Err(env.type_error(
                    format!(
                        "orchestrate '{}' stage steps must be top-level 'stage binding: <kind> function(...)' or 'let binding = <kind> function(...)' declarations",
                        orchestrate.name
                    ),
                    *span,
                ));
            }
            Stmt::Item(item) => {
                return Err(env.type_error(
                    format!(
                        "orchestrate '{}' cannot declare nested items inside its pipeline body",
                        orchestrate.name
                    ),
                    item_span(item.as_ref()),
                ));
            }
            other => {
                if let Some(stage_span) = first_stage_call_in_stmt(other) {
                    return Err(env.type_error(
                        format!(
                            "orchestrate '{}' only permits stage calls in top-level typed let bindings",
                            orchestrate.name
                        ),
                        stage_span,
                    ));
                }
                seen_non_stage_stmt = true;
            }
        }
    }
    Ok(stages)
}

fn first_stage_call_in_stmt(stmt: &Stmt) -> Option<Span> {
    match stmt {
        Stmt::Let { value, .. } => value.as_ref().and_then(first_stage_call_in_expr),
        Stmt::Expr(expr) => first_stage_call_in_expr(expr),
        Stmt::Defer { expr, .. } => first_stage_call_in_expr(expr),
        Stmt::Dispatch { dispatch_size, .. } => {
            dispatch_size_find_map(dispatch_size, first_stage_call_in_expr)
        }
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            value.as_ref().and_then(first_stage_call_in_expr)
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            first_stage_call_in_expr(iter).or_else(|| first_stage_call_in_block(body))
        }
        Stmt::While {
            condition, body, ..
        } => first_stage_call_in_expr(condition).or_else(|| first_stage_call_in_block(body)),
        Stmt::Loop { body, .. } => first_stage_call_in_block(body),
        Stmt::Item(_) | Stmt::Continue(_) => None,
        Stmt::Subgroup { body, .. } => first_stage_call_in_block(body),
    }
}

fn first_stage_call_in_block(block: &Block) -> Option<Span> {
    block.stmts.iter().find_map(first_stage_call_in_stmt)
}

fn first_stage_call_in_else_branch(branch: &ElseBranch) -> Option<Span> {
    match branch {
        ElseBranch::Else(block) => first_stage_call_in_block(block),
        ElseBranch::ElseIf(condition, block, next) => first_stage_call_in_expr(condition)
            .or_else(|| first_stage_call_in_block(block))
            .or_else(|| {
                next.as_ref()
                    .and_then(|branch| first_stage_call_in_else_branch(branch))
            }),
    }
}

fn first_stage_call_in_expr(expr: &Expr) -> Option<Span> {
    match expr {
        Expr::StageCall { span, .. } => Some(*span),
        Expr::Assign { target, value, .. } => {
            first_stage_call_in_expr(target).or_else(|| first_stage_call_in_expr(value))
        }
        Expr::Call { callee, args, .. } => first_stage_call_in_expr(callee).or_else(|| {
            args.iter()
                .find_map(|arg| first_stage_call_in_expr(&arg.value))
        }),
        Expr::MethodCall { args, .. } => args
            .iter()
            .find_map(|arg| first_stage_call_in_expr(&arg.value)),
        Expr::Binary { left, right, .. } => {
            first_stage_call_in_expr(left).or_else(|| first_stage_call_in_expr(right))
        }
        Expr::Unary { operand, .. }
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Paren(operand, _)
        | Expr::Cast { value: operand, .. }
        | Expr::Bitcast { value: operand, .. }
        | Expr::Comptime(operand, _) => first_stage_call_in_expr(operand),
        Expr::Field { object, .. } => first_stage_call_in_expr(object),
        Expr::Index { object, index, .. } => {
            first_stage_call_in_expr(object).or_else(|| first_stage_call_in_expr(index))
        }
        Expr::Struct { fields, rest, .. } => fields
            .iter()
            .find_map(|(_, value)| first_stage_call_in_expr(value))
            .or_else(|| {
                rest.as_ref()
                    .and_then(|rest| first_stage_call_in_expr(rest))
            }),
        Expr::AggregateInit { fields, .. } => fields
            .iter()
            .find_map(|(_, value)| first_stage_call_in_expr(value)),
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Tuple(values) => values.iter().find_map(first_stage_call_in_expr),
            EnumVariantFields::Struct(values) => values
                .iter()
                .find_map(|(_, value)| first_stage_call_in_expr(value)),
            EnumVariantFields::Unit => None,
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) => {
            values.iter().find_map(first_stage_call_in_expr)
        }
        Expr::Range { start, end, .. } => start
            .as_ref()
            .and_then(|value| first_stage_call_in_expr(value))
            .or_else(|| {
                end.as_ref()
                    .and_then(|value| first_stage_call_in_expr(value))
            }),
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => first_stage_call_in_expr(condition)
            .or_else(|| first_stage_call_in_block(then_branch))
            .or_else(|| {
                else_branch
                    .as_ref()
                    .and_then(|branch| first_stage_call_in_else_branch(branch))
            }),
        Expr::Match {
            scrutinee, arms, ..
        } => first_stage_call_in_expr(scrutinee).or_else(|| {
            arms.iter().find_map(|arm| {
                arm.guard
                    .as_ref()
                    .and_then(first_stage_call_in_expr)
                    .or_else(|| first_stage_call_in_expr(&arm.body))
            })
        }),
        Expr::Lambda { body, .. } => first_stage_call_in_expr(body),
        Expr::MemStore { pointer, value, .. } | Expr::VolatileStore { pointer, value, .. } => {
            first_stage_call_in_expr(pointer).or_else(|| first_stage_call_in_expr(value))
        }
        Expr::VolatileLoad { pointer, .. } | Expr::AtomicLoad { pointer, .. } => {
            first_stage_call_in_expr(pointer)
        }
        Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            first_stage_call_in_expr(pointer).or_else(|| first_stage_call_in_expr(value))
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => first_stage_call_in_expr(pointer)
            .or_else(|| first_stage_call_in_expr(expected))
            .or_else(|| first_stage_call_in_expr(desired)),
        Expr::AtomicFence { .. } => None,
        Expr::CpuFence { .. } => None,
        Expr::CpuCacheFlush { pointer, .. } => first_stage_call_in_expr(pointer),
        Expr::InlineAsm { operands, .. } => operands.iter().find_map(first_stage_call_in_expr),
        Expr::PtrOffset {
            pointer, offset, ..
        } => first_stage_call_in_expr(pointer).or_else(|| first_stage_call_in_expr(offset)),
        Expr::MemLoad { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        }
        | Expr::Teleport { value: pointer, .. } => first_stage_call_in_expr(pointer),
        Expr::Observe { target, body, .. }
        | Expr::Collapse { target, body, .. }
        | Expr::Share { target, body, .. } => {
            first_stage_call_in_expr(target).or_else(|| first_stage_call_in_expr(body))
        }
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } | Expr::Emit { data: init, .. } => init
            .iter()
            .find_map(|(_, value)| first_stage_call_in_expr(value)),
        Expr::Return(value, _) | Expr::Break(value, _) => value
            .as_ref()
            .and_then(|expr| first_stage_call_in_expr(expr.as_ref())),
        Expr::Block(block, _) => first_stage_call_in_block(block),
        Expr::JSX(_, _)
        | Expr::MacroCall { .. }
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::FString(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Alloca { .. }
        | Expr::Uninit { .. }
        | Expr::Alloc { .. }
        | Expr::Realloc { .. }
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Continue(_) => None,
    }
}

fn ensure_converge_verify_types_supported(
    env: &TypeEnv,
    converge: &ConvergeDef,
    signature: &ResolvedType,
) -> KainResult<()> {
    let ResolvedType::Function { params, ret, .. } = signature else {
        return Ok(());
    };
    for (param, ty) in converge.params.iter().zip(params.iter()) {
        if !supports_converge_verify_sampling(ty) {
            return Err(env.type_error(
                format!(
                    "converge '{}' verify random(n) does not support parameter '{}' of type {}",
                    converge.name,
                    param.name,
                    describe_type(ty)
                ),
                param.span,
            ));
        }
    }
    if !supports_converge_verify_sampling(ret.as_ref()) {
        let return_span = converge
            .return_type
            .as_ref()
            .map(Type::span)
            .unwrap_or(converge.span);
        return Err(env.type_error(
            format!(
                "converge '{}' verify random(n) does not support return type {}",
                converge.name,
                describe_type(ret.as_ref())
            ),
            return_span,
        ));
    }
    Ok(())
}

fn supports_converge_verify_sampling(ty: &ResolvedType) -> bool {
    match ty {
        ResolvedType::Bool | ResolvedType::Int(_) | ResolvedType::Float(_) | ResolvedType::Char => {
            true
        }
        ResolvedType::Array(inner, _) | ResolvedType::Option(inner) => {
            supports_converge_verify_sampling(inner)
        }
        ResolvedType::Tuple(items) => items.iter().all(supports_converge_verify_sampling),
        _ => false,
    }
}

fn check_struct(env: &mut TypeEnv, s: &Struct) -> KainResult<TypedStruct> {
    validate_generic_constraints(env, &s.generics, s.where_clause.as_ref(), "struct")?;
    validate_struct_attributes(env, s)?;
    let has_mmio = s
        .attributes
        .iter()
        .any(|attribute| attribute.name == ATTR_MMIO);
    let mmio_stride_bytes = if has_mmio {
        s.attributes
            .iter()
            .find(|attribute| attribute.name == ATTR_MMIO)
            .map(|attribute| -> KainResult<Option<i64>> {
                Ok(attribute_named_arg(env, attribute, "stride")?.and_then(expr_as_attribute_int))
            })
            .transpose()?
            .flatten()
    } else {
        None
    };
    let abi = default_c_abi_policy();
    let mut fields = HashMap::new();
    for f in &s.fields {
        let field_ty = resolve_type_in_env(env, &f.ty)?;
        if has_mmio && !matches!(field_ty, ResolvedType::Int(_)) {
            return Err(env.type_error(
                format!(
                    "@mmio register blocks currently only support integer register fields; field '{}' resolved to {}",
                    f.name,
                    describe_type(&field_ty)
                ),
                f.span,
            ));
        }
        if let Some(stride_bytes) = mmio_stride_bytes {
            let field_bytes = resolved_type_byte_width(&field_ty, &abi).ok_or_else(|| {
                env.type_error(
                    format!(
                        "@mmio could not derive a byte width for register field '{}'",
                        f.name
                    ),
                    f.span,
                )
            })?;
            if field_bytes != stride_bytes {
                return Err(env.type_error(
                    format!(
                        "@mmio(stride: {stride_bytes}) currently requires each register field to occupy exactly {stride_bytes} bytes, but '{}' occupies {field_bytes}",
                        f.name
                    ),
                    f.span,
                ));
            }
        }
        if let Some(default) = &f.default {
            let default_ty = infer_expr_type(env, default, None)?;
            ensure_type_compatible(
                env,
                &field_ty,
                &default_ty,
                default.span(),
                "struct field default",
            )?;
        }
        fields.insert(f.name.clone(), field_ty);
    }

    let self_ty = ResolvedType::Struct(s.name.clone(), fields.clone());
    for method in &s.methods {
        check_function_with_self(env, method, &self_ty)?;
    }

    Ok(TypedStruct {
        ast: s.clone(),
        field_types: fields,
    })
}

fn check_enum(env: &mut TypeEnv, e: &Enum) -> KainResult<TypedEnum> {
    validate_generic_constraints(env, &e.generics, e.where_clause.as_ref(), "enum")?;
    let mut variant_payload_types: HashMap<String, Vec<ResolvedType>> = HashMap::new();

    for v in &e.variants {
        let payload_types = match &v.fields {
            VariantFields::Unit => Vec::new(),
            VariantFields::Tuple(items) => items
                .iter()
                .map(|ty| resolve_type_in_env(env, ty))
                .collect::<Result<Vec<_>, _>>()?,
            VariantFields::Struct(fields) => fields
                .iter()
                .map(|f| resolve_type_in_env(env, &f.ty))
                .collect::<Result<Vec<_>, _>>()?,
        };
        variant_payload_types.insert(v.name.clone(), payload_types);
    }

    Ok(TypedEnum {
        ast: e.clone(),
        variant_payload_types,
    })
}

fn check_trait(env: &mut TypeEnv, t: &Trait) -> KainResult<TypedTrait> {
    validate_generic_constraints(env, &t.generics, t.where_clause.as_ref(), "trait")?;
    for supertrait in &t.supertraits {
        if let Type::Named { name, span, .. } = supertrait {
            validate_trait_name_exists(env, name, *span, "supertrait")?;
        }
    }
    Ok(TypedTrait { ast: t.clone() })
}

fn check_type_alias(env: &mut TypeEnv, alias: &TypeAlias) -> KainResult<TypedTypeAlias> {
    validate_generic_constraints(
        env,
        &alias.generics,
        alias.where_clause.as_ref(),
        "type alias",
    )?;
    let _ = resolve_type_in_env(env, &alias.target)?;
    Ok(TypedTypeAlias { ast: alias.clone() })
}

fn check_component(env: &mut TypeEnv, c: &Component) -> KainResult<TypedComponent> {
    let mut props = HashMap::new();
    for p in &c.props {
        props.insert(p.name.clone(), resolve_param_type(env, p, None)?);
    }

    // Build extended self type: includes both props and state for `self.` access.
    let mut self_fields = props.clone();
    let mut state_types: HashMap<String, ResolvedType> = HashMap::new();

    // Collect state types before the scope so we can return them.
    for state in &c.state {
        let state_ty = resolve_type_in_env(env, &state.ty)?;
        state_types.insert(state.name.clone(), state_ty.clone());
        self_fields.insert(state.name.clone(), state_ty);
    }

    let mut pulse_types: Vec<TypedPulse> = Vec::new();
    let mut resonate_types: Vec<TypedResonate> = Vec::new();

    env.with_scope(|env| {
        let mut component_errors = Vec::new();
        for (name, ty) in &props {
            env.define(name.clone(), ty.clone());
        }
        for method in &c.methods {
            let signature = function_signature(env, method, None)?;
            env.define(method.name.clone(), signature);
        }
        for state in &c.state {
            let state_ty = state_types.get(&state.name).cloned()
                .unwrap_or(ResolvedType::Unknown);
            let initial_ty = infer_expr_type(env, &state.initial, None)?;
            ensure_type_compatible(
                env,
                &state_ty,
                &initial_ty,
                state.initial.span(),
                "component state initializer",
            )?;
            env.define(state.name.clone(), state_ty);
        }

        // Register `self` for component-scoped access to state + props.
        let self_ty = ResolvedType::Struct(c.name.clone(), self_fields);
        env.define("self".to_string(), self_ty.clone());

        for method in &c.methods {
            if let Err(error) = check_function_with_self(env, method, &self_ty) {
                extend_accumulated_errors(&mut component_errors, error);
            }
        }

        // Typecheck inline pulse blocks — `self` is accessible from the outer component scope.
        for pulse in &c.pulses {
            match check_pulse(env, pulse) {
                Ok(tp) => pulse_types.push(tp),
                Err(e) => extend_accumulated_errors(&mut component_errors, e),
            }
        }

        // Typecheck inline resonate blocks — `self` is accessible from the outer component scope.
        for resonate in &c.resonates {
            match check_resonate(env, resonate) {
                Ok(tr) => resonate_types.push(tr),
                Err(e) => extend_accumulated_errors(&mut component_errors, e),
            }
        }

        check_jsx_semantics(env, &c.body, None)?;
        finish_accumulated(component_errors, ())
    })?;

    Ok(TypedComponent {
        ast: c.clone(),
        prop_types: props,
        state_types,
        pulse_types,
        resonate_types,
    })
}

fn check_shader(env: &mut TypeEnv, s: &Shader) -> KainResult<TypedShader> {
    let inputs: Vec<_> = s
        .inputs
        .iter()
        .map(|p| resolve_param_type(env, p, None))
        .collect::<Result<_, _>>()?;
    let output = resolve_type_in_env(env, &s.outputs)?;
    let ctx = SemanticContext {
        function_name: s.name.clone(),
        return_type: output.clone(),
        effects: EffectSet::new(),
    };

    env.with_scope(|env| {
        for (param, ty) in s.inputs.iter().zip(inputs.iter()) {
            env.define(param.name.clone(), ty.clone());
        }
        for uniform in &s.uniforms {
            env.define(uniform.name.clone(), resolve_type_in_env(env, &uniform.ty)?);
        }
        check_block_semantics(env, &s.body, &ctx)
    })?;

    Ok(TypedShader {
        ast: s.clone(),
        input_types: inputs,
        output_type: output,
    })
}

pub fn resolve_type(ty: &Type) -> KainResult<ResolvedType> {
    resolve_type_impl(None, ty)
}

fn resolve_type_in_env(env: &TypeEnv, ty: &Type) -> KainResult<ResolvedType> {
    resolve_type_impl(Some(env), ty)
}

fn resolve_type_impl(env: Option<&TypeEnv>, ty: &Type) -> KainResult<ResolvedType> {
    match ty {
        Type::Named { name, generics, .. } => match name.as_str() {
            "Int" => Ok(ResolvedType::Int(IntSize::I64)),
            "UInt" => Ok(ResolvedType::Int(IntSize::U64)),
            "Float" => Ok(ResolvedType::Float(FloatSize::F64)),
            "i8" => Ok(ResolvedType::Int(IntSize::I8)),
            "i16" => Ok(ResolvedType::Int(IntSize::I16)),
            "i32" => Ok(ResolvedType::Int(IntSize::I32)),
            "i64" => Ok(ResolvedType::Int(IntSize::I64)),
            "i128" => Ok(ResolvedType::Int(IntSize::I128)),
            "isize" => Ok(ResolvedType::Int(IntSize::Isize)),
            "u8" => Ok(ResolvedType::Int(IntSize::U8)),
            "u16" => Ok(ResolvedType::Int(IntSize::U16)),
            "u32" => Ok(ResolvedType::Int(IntSize::U32)),
            "u64" => Ok(ResolvedType::Int(IntSize::U64)),
            "u128" => Ok(ResolvedType::Int(IntSize::U128)),
            "usize" => Ok(ResolvedType::Int(IntSize::Usize)),
            "f32" => Ok(ResolvedType::Float(FloatSize::F32)),
            "f64" => Ok(ResolvedType::Float(FloatSize::F64)),
            "Void" => Ok(ResolvedType::Unit),
            "Any" => Ok(ResolvedType::Unknown),
            "Bool" => Ok(ResolvedType::Bool),
            "Char" => Ok(ResolvedType::Char),
            "String" => Ok(ResolvedType::String),
            "Box" | "Arc" | "Rc" | "Cell" | "RefCell" if generics.len() == 1 => {
                resolve_type_impl(env, &generics[0])
            }
            "Array" if generics.len() == 1 => Ok(ResolvedType::Array(
                Box::new(resolve_type_impl(env, &generics[0])?),
                0,
            )),
            "Map" if generics.len() == 2 => Ok(selfhost_map_type()),
            "Set" if generics.len() == 1 => Ok(selfhost_set_type()),
            "StorageBuffer" if generics.len() == 1 => Ok(ResolvedType::Slice(Box::new(
                resolve_type_impl(env, &generics[0])?,
            ))),
            "Option" if generics.len() == 1 => Ok(ResolvedType::Option(Box::new(
                resolve_type_impl(env, &generics[0])?,
            ))),
            "Result" if generics.len() == 2 => Ok(ResolvedType::Result(
                Box::new(resolve_type_impl(env, &generics[0])?),
                Box::new(resolve_type_impl(env, &generics[1])?),
            )),
            _ => {
                if name.len() == 1
                    && name
                        .chars()
                        .next()
                        .map(|c| c.is_uppercase())
                        .unwrap_or(false)
                {
                    Ok(ResolvedType::Generic(name.clone()))
                } else if name.starts_with('_') && name.len() > 1 {
                    Ok(ResolvedType::Generic(name.clone()))
                } else if let Some(env) = env {
                    if let Some(resolved) = env.lookup_type(name) {
                        Ok(resolved.clone())
                    } else {
                        Ok(ResolvedType::Struct(name.clone(), HashMap::new()))
                    }
                } else {
                    Ok(ResolvedType::Struct(name.clone(), HashMap::new()))
                }
            }
        },
        Type::Unit(_) => Ok(ResolvedType::Unit),
        Type::Never(_) => Ok(ResolvedType::Never),
        Type::Tuple(inner, _) => Ok(ResolvedType::Tuple(
            inner
                .iter()
                .map(|ty| resolve_type_impl(env, ty))
                .collect::<Result<_, _>>()?,
        )),
        Type::Array(inner, len, _) => Ok(ResolvedType::Array(
            Box::new(resolve_type_impl(env, inner)?),
            *len,
        )),
        Type::Slice(inner, _) => Ok(ResolvedType::Slice(Box::new(resolve_type_impl(
            env, inner,
        )?))),
        Type::Option(inner, _) => Ok(ResolvedType::Option(Box::new(resolve_type_impl(
            env, inner,
        )?))),
        Type::Result(ok, err, _) => Ok(ResolvedType::Result(
            Box::new(resolve_type_impl(env, ok)?),
            Box::new(resolve_type_impl(env, err)?),
        )),
        Type::Ref { mutable, inner, .. } => Ok(ResolvedType::Ref {
            mutable: *mutable,
            inner: Box::new(resolve_type_impl(env, inner)?),
        }),
        Type::Function {
            params,
            return_type,
            effects,
            ..
        } => {
            let resolved_params = params
                .iter()
                .map(|ty| resolve_type_impl(env, ty))
                .collect::<Result<Vec<_>, _>>()?;
            let resolved_ret = resolve_type_impl(env, return_type)?;
            Ok(ResolvedType::Function {
                params: resolved_params,
                ret: Box::new(resolved_ret),
                effects: EffectSet::from(effects.clone()),
            })
        }
        Type::Ptr { mutable, inner, .. } => Ok(ResolvedType::Ptr {
            mutable: *mutable,
            inner: Box::new(resolve_type_impl(env, inner)?),
        }),
        Type::Impl {
            trait_name,
            generics,
            ..
        } if trait_name == "Future" => {
            let inner = generics
                .first()
                .map(|ty| resolve_type_impl(env, ty))
                .transpose()?
                .unwrap_or(ResolvedType::Unknown);
            Ok(ResolvedType::Future(Box::new(inner)))
        }
        Type::Infer(_) => Ok(ResolvedType::Unknown),
        _ => Ok(ResolvedType::Unknown),
    }
}

fn item_span(item: &Item) -> Span {
    match item {
        Item::Function(f) => f.span,
        Item::Patch(patch) => patch.span,
        Item::Law(law) => law.span,
        Item::Axiom(axiom) => axiom.span,
        Item::Converge(converge) => converge.span,
        Item::World(world) => world.span,
        Item::Entangle(entangle) => entangle.span,
        Item::Orchestrate(orchestrate) => orchestrate.span,
        Item::Pulse(pulse) => pulse.span,
        Item::Resonate(resonate) => resonate.span,
        Item::Struct(s) => s.span,
        Item::Enum(e) => e.span,
        Item::Trait(t) => t.span,
        Item::Component(c) => c.span,
        Item::Shader(s) => s.span,
        Item::Actor(a) => a.span,
        Item::Comptime(b) => b.span,
        Item::Const(c) => c.span,
        Item::Macro(m) => m.span,
        Item::Use(u) => u.span,
        Item::Import(import) => import.span,
        Item::Mod(m) => m.span,
        Item::Impl(i) => i.span,
        Item::Test(t) => t.span,
        Item::TypeAlias(t) => t.span,
        _ => Span::new(0, 0),
    }
}

impl From<Vec<crate::effects::Effect>> for EffectSet {
    fn from(v: Vec<crate::effects::Effect>) -> Self {
        let mut s = EffectSet::new();
        for e in v {
            s.effects.insert(e);
        }
        s
    }
}

fn function_signature(
    env: &TypeEnv,
    function: &Function,
    self_ty: Option<&ResolvedType>,
) -> KainResult<ResolvedType> {
    let mut params = Vec::new();
    for param in &function.params {
        params.push(resolve_param_type(env, param, self_ty)?);
    }
    let ret = function
        .return_type
        .as_ref()
        .map(|ty| resolve_type_in_env(env, ty))
        .transpose()?
        .unwrap_or(ResolvedType::Unknown);
    Ok(ResolvedType::Function {
        params,
        ret: Box::new(ret),
        effects: EffectSet::from(function.effects.clone()),
    })
}

fn resolve_param_type(
    env: &TypeEnv,
    param: &Param,
    self_ty: Option<&ResolvedType>,
) -> KainResult<ResolvedType> {
    match (&param.ty, self_ty) {
        (Type::Infer(_), Some(self_ty)) if param.name == "self" => Ok(self_ty.clone()),
        _ => resolve_type_in_env(env, &param.ty),
    }
}

fn infer_expr_type_with_expected(
    env: &mut TypeEnv,
    expr: &Expr,
    ctx: Option<&SemanticContext>,
    expected: Option<&ResolvedType>,
) -> KainResult<ResolvedType> {
    if let Expr::Paren(inner, _) = expr {
        return infer_expr_type_with_expected(env, inner, ctx, expected);
    }

    if let (
        Expr::Lambda {
            params,
            return_type,
            body,
            span: _,
        },
        Some(ResolvedType::Function {
            params: expected_params,
            ret: expected_ret,
            ..
        }),
    ) = (expr, expected)
    {
        let annotated_ret = return_type
            .as_ref()
            .map(|ty| resolve_type_in_env(env, ty))
            .transpose()?
            .unwrap_or(ResolvedType::Unknown);
        let (param_types, body_ty) = env.with_scope(|env| {
            let mut param_types = Vec::with_capacity(params.len());
            for (index, param) in params.iter().enumerate() {
                let param_ty = resolve_param_type(env, param, None)?;
                let inferred_ty = match (expected_params.get(index), param_ty) {
                    (Some(expected_param_ty), ResolvedType::Unknown) => expected_param_ty.clone(),
                    (Some(expected_param_ty), actual_ty) => {
                        ensure_type_compatible(
                            env,
                            expected_param_ty,
                            &actual_ty,
                            param.span,
                            "lambda parameter",
                        )?;
                        actual_ty
                    }
                    (None, actual_ty) => actual_ty,
                };
                env.define(param.name.clone(), inferred_ty.clone());
                param_types.push(inferred_ty);
            }
            let body_expected = if matches!(annotated_ret, ResolvedType::Unknown) {
                Some(expected_ret.as_ref())
            } else {
                Some(&annotated_ret)
            };
            let body_ty = infer_expr_type_with_expected(env, body, ctx, body_expected)?;
            Ok((param_types, body_ty))
        })?;

        if !matches!(annotated_ret, ResolvedType::Unknown)
            && !matches!(
                body_ty,
                ResolvedType::Unknown | ResolvedType::Unit | ResolvedType::Never
            )
            && !type_contains_unknown(&annotated_ret)
        {
            ensure_type_compatible(env, &annotated_ret, &body_ty, body.span(), "lambda body")?;
        } else if matches!(annotated_ret, ResolvedType::Unknown)
            && !matches!(
                body_ty,
                ResolvedType::Unknown | ResolvedType::Unit | ResolvedType::Never
            )
            && !matches!(expected_ret.as_ref(), ResolvedType::Unknown)
            && !type_contains_unknown(expected_ret.as_ref())
        {
            ensure_type_compatible(
                env,
                expected_ret.as_ref(),
                &body_ty,
                body.span(),
                "lambda body",
            )?;
        }

        let resolved_ret = if matches!(annotated_ret, ResolvedType::Unknown) {
            if matches!(expected_ret.as_ref(), ResolvedType::Unknown) {
                body_ty.clone()
            } else {
                expected_ret.as_ref().clone()
            }
        } else {
            annotated_ret
        };

        return Ok(ResolvedType::Function {
            params: param_types,
            ret: Box::new(resolved_ret),
            effects: EffectSet::new(),
        });
    }

    let inferred = infer_expr_type(env, expr, ctx)?;
    if let Some(expected_ty) = expected {
        if types_compatible(expected_ty, &inferred) {
            return Ok(inferred);
        }
        if let ResolvedType::Ref {
            mutable: false,
            inner,
        } = expected_ty
        {
            if types_compatible(inner.as_ref(), &inferred) {
                return Ok(expected_ty.clone());
            }
        }
    }
    Ok(inferred)
}

fn check_block_semantics(
    env: &mut TypeEnv,
    block: &Block,
    ctx: &SemanticContext,
) -> KainResult<()> {
    let mut errors = Vec::new();
    for stmt in &block.stmts {
        if let Err(error) = check_stmt_semantics(env, stmt, ctx) {
            extend_accumulated_errors(&mut errors, error);
        }
    }
    finish_accumulated(errors, ())
}

/// Validate subgroup divergence rules:
/// - KAIN-SHADER-0042: No nested subgroups
/// - KAIN-SHADER-0043: No divergent escape via return/break/continue
fn validate_subgroup_divergence(
    body: &Block,
    in_loop: bool,
    env: &TypeEnv,
) -> KainResult<()> {
    for stmt in &body.stmts {
        match stmt {
            // KAIN-SHADER-0042: Nested subgroup
            Stmt::Subgroup { span, .. } => {
                return Err(env.type_error_with_code(
                    DiagnosticCode::ShaderSubgroupNested,
                    "subgroup cannot be nested inside another subgroup",
                    *span,
                ));
            }
            // KAIN-SHADER-0043: Divergent escape via return
            Stmt::Return(Some(_), span) | Stmt::Return(None, span) => {
                return Err(env.type_error_with_code(
                    DiagnosticCode::ShaderSubgroupDivergentEscape,
                    "return inside subgroup would cause divergent warp execution",
                    *span,
                ));
            }
            // break/continue: allowed only if in_loop is true
            Stmt::Break(_, span) | Stmt::Continue(span) => {
                if !in_loop {
                    return Err(env.type_error_with_code(
                        DiagnosticCode::ShaderSubgroupDivergentEscape,
                        "break/continue inside subgroup outside of a loop would cause divergent warp execution",
                        *span,
                    ));
                }
            }
            // for/while/loop inside subgroup: recurse with in_loop=true
            Stmt::For { body, .. }
            | Stmt::Fanout { body, .. }
            | Stmt::While { body, .. }
            | Stmt::Loop { body, .. } => {
                validate_subgroup_divergence(body, true, env)?;
            }
            // Recurse into blocks from other statements (if/match via expr, dispatch blocks, etc.)
            Stmt::Dispatch { .. }
            | Stmt::Let { .. }
            | Stmt::Expr(_)
            | Stmt::Defer { .. }
            | Stmt::Item(_) => {}
        }
    }
    Ok(())
}

fn block_value_type(
    env: &mut TypeEnv,
    block: &Block,
    ctx: &SemanticContext,
) -> KainResult<ResolvedType> {
    env.push_scope();
    let result = (|| {
        check_block_semantics(env, block, ctx)?;
        Ok(match block.stmts.last() {
            Some(Stmt::Expr(expr)) => infer_expr_type(env, expr, Some(ctx))?,
            _ => ResolvedType::Unit,
        })
    })();
    env.pop_scope();
    result
}

fn check_stmt_semantics(env: &mut TypeEnv, stmt: &Stmt, ctx: &SemanticContext) -> KainResult<()> {
    match stmt {
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
        } => {
            let inferred = value
                .as_ref()
                .map(|expr| infer_expr_type(env, expr, Some(ctx)))
                .transpose()?
                .unwrap_or(ResolvedType::Unknown);
            let declared = ty
                .as_ref()
                .map(|decl| resolve_type_in_env(env, decl))
                .transpose()?;
            if let Some(declared) = &declared {
                ensure_type_compatible(env, declared, &inferred, *span, "let binding")?;
            }
            let binding_ty = declared.unwrap_or(inferred);
            bind_pattern_types(env, pattern, &binding_ty)?;
        }
        Stmt::Expr(expr) => {
            let _ = infer_expr_type(env, expr, Some(ctx))?;
        }
        Stmt::Defer { expr, span } => {
            if ownership_expr_contains_early_exit(expr) {
                return Err(env.type_error(
                    "defer payloads must be cleanup expressions and cannot contain return, break, or continue",
                    *span,
                ));
            }
            let _ = infer_expr_type(env, expr, Some(ctx))?;
        }
        Stmt::Dispatch {
            compute_key,
            dispatch_size,
            span,
        } => {
            if compute_key.is_empty() {
                return Err(env.type_error("dispatch compute key cannot be empty", *span));
            }
            // GPU effect check ---------------------------------------------------
            // dispatch requires at least `with GPU` on the enclosing function.
            //   - `with GPU, Unsafe`: always allowed (existing behavior).
            //   - `with GPU` without `Unsafe`: allowed only inside an orchestrate
            //     block whose compiler-owned access_map can prove memory safety.
            //   - No GPU effect: compile error.
            let has_gpu = ctx.effects.effects.contains(&Effect::GPU);
            let has_unsafe = ctx.effects.effects.contains(&Effect::Unsafe);
            if !has_gpu && !has_unsafe {
                return Err(env.type_error(
                    "dispatch requires 'with GPU' effect annotation on the enclosing function",
                    *span,
                ));
            }
            if has_gpu && !has_unsafe && !env.in_orchestrate {
                return Err(env.type_error(
                    "GPU dispatch without 'Unsafe' effect requires an orchestrate wrapper. \
                     Add 'with GPU, Unsafe' or wrap dispatch in an 'orchestrate' block.",
                    *span,
                ));
            }
            for dimension in dispatch_size_exprs_ref(dispatch_size) {
                let dim_ty = infer_expr_type(env, dimension, Some(ctx))?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::Int(IntSize::I64),
                    &dim_ty,
                    dimension.span(),
                    "dispatch dimension",
                )?;
            }
        }
        Stmt::Return(Some(expr), span) => {
            let expr_ty = infer_expr_type(env, expr, Some(ctx))?;
            ensure_type_compatible(env, &ctx.return_type, &expr_ty, *span, "return value")?;
        }
        Stmt::Return(None, span) => {
            ensure_type_compatible(env, &ctx.return_type, &ResolvedType::Unit, *span, "return")?;
        }
        Stmt::While {
            condition, body, ..
        } => {
            let cond_ty = infer_expr_type(env, condition, Some(ctx))?;
            ensure_condition_type_compatible(env, &cond_ty, condition.span(), "while condition")?;
            env.with_scope(|env| check_block_semantics(env, body, ctx))?;
        }
        Stmt::For {
            binding,
            iter,
            body,
            span,
        } => {
            let iter_ty = infer_expr_type(env, iter, Some(ctx))?;
            let item_ty = match iter_ty {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => *inner,
                ResolvedType::Ref { mutable, inner } => match *inner {
                    ResolvedType::Array(item, _) | ResolvedType::Slice(item) => ResolvedType::Ref {
                        mutable,
                        inner: item,
                    },
                    ResolvedType::String => ResolvedType::Ref {
                        mutable,
                        inner: Box::new(ResolvedType::String),
                    },
                    other => {
                        return Err(env.type_error(
                            format!(
                                "for loop expects an iterable reference, found &{}",
                                describe_type(&other)
                            ),
                            *span,
                        ))
                    }
                },
                ResolvedType::String => ResolvedType::String,
                ResolvedType::Option(inner) if matches!(inner.as_ref(), ResolvedType::Unknown) => {
                    ResolvedType::Unknown
                }
                ResolvedType::Struct(_, _) | ResolvedType::Generic(_) => ResolvedType::Unknown,
                ResolvedType::Unknown => ResolvedType::Unknown,
                other => {
                    return Err(env.type_error(
                        format!(
                            "for loop expects an iterable value, found {}",
                            describe_type(&other)
                        ),
                        *span,
                    ))
                }
            };
            env.with_scope(|env| {
                bind_pattern_types(env, binding, &item_ty)?;
                check_block_semantics(env, body, ctx)
            })?;
        }
        Stmt::Fanout {
            binding,
            iter,
            body,
            span,
        } => {
            if !env.in_shared_region() {
                return Err(env.type_error("fanout requires an enclosing share scope in v1", *span));
            }
            if env.in_fanout() {
                return Err(env.type_error("nested fanout is not supported in v1", *span));
            }
            if ownership_block_contains_early_exit(body) {
                return Err(env.type_error(
                    "fanout bodies do not support return, break, or continue in v1",
                    *span,
                ));
            }
            let iter_ty = infer_expr_type(env, iter, Some(ctx))?;
            let item_ty = match iter_ty {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => *inner,
                ResolvedType::Ref { mutable, inner } => match *inner {
                    ResolvedType::Array(item, _) | ResolvedType::Slice(item) => ResolvedType::Ref {
                        mutable,
                        inner: item,
                    },
                    ResolvedType::String => ResolvedType::Ref {
                        mutable,
                        inner: Box::new(ResolvedType::String),
                    },
                    other => {
                        return Err(env.type_error(
                            format!(
                                "fanout expects an iterable reference, found &{}",
                                describe_type(&other)
                            ),
                            *span,
                        ))
                    }
                },
                ResolvedType::String => ResolvedType::String,
                ResolvedType::Option(inner) if matches!(inner.as_ref(), ResolvedType::Unknown) => {
                    ResolvedType::Unknown
                }
                ResolvedType::Struct(_, _) | ResolvedType::Generic(_) => ResolvedType::Unknown,
                ResolvedType::Unknown => ResolvedType::Unknown,
                other => {
                    return Err(env.type_error(
                        format!(
                            "fanout expects an iterable value, found {}",
                            describe_type(&other)
                        ),
                        *span,
                    ))
                }
            };
            env.push_scope();
            env.push_fanout();
            let result = (|| {
                bind_pattern_types(env, binding, &item_ty)?;
                check_block_semantics(env, body, ctx)
            })();
            env.pop_fanout();
            env.pop_scope();
            result?;
        }
        Stmt::Loop { body, .. } => {
            env.with_scope(|env| check_block_semantics(env, body, ctx))?;
        }
        Stmt::Item(item) => {
            let _ = check_item(env, item.as_ref())?;
        }
        Stmt::Break(Some(expr), _) => {
            let _ = infer_expr_type(env, expr, Some(ctx))?;
        }
        Stmt::Break(None, _) | Stmt::Continue(_) => {}
        Stmt::Subgroup { size: _, body, span: _ } => {
            // Validate divergence before typechecking
            validate_subgroup_divergence(body, false, env)?;
            check_block_semantics(env, body, ctx)?;
        }
    }
    Ok(())
}

fn infer_expr_type(
    env: &mut TypeEnv,
    expr: &Expr,
    ctx: Option<&SemanticContext>,
) -> KainResult<ResolvedType> {
    match expr {
        Expr::Int(_, _) => Ok(ResolvedType::Int(IntSize::I64)),
        Expr::Float(_, _) => Ok(ResolvedType::Float(FloatSize::F64)),
        Expr::String(_, _) | Expr::FString(_, _) => Ok(ResolvedType::String),
        Expr::Bool(_, _) => Ok(ResolvedType::Bool),
        Expr::None(_) => Ok(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
        Expr::Ident(name, span) => {
            if env.is_moved(name) {
                return Err(env.type_error(
                    format!(
                        "Identifier '{}' was moved by teleport and cannot be used again",
                        name
                    ),
                    *span,
                ));
            }
            env.lookup(name)
                .cloned()
                .ok_or_else(|| {
                    let mut report = env.type_report(
                        DiagnosticCode::TypeUnknownIdentifier,
                        format!("Unknown identifier '{}'", name),
                        *span,
                        format!("'{name}' is not in scope"),
                    );
                    if name == "slice" {
                        report = report.note(
                            "Kain does not automatically expose Python builtins or host globals as normal identifiers.",
                        );
                    }
                    KainError::rich(report.help(
                        "Check for a misspelling, add the missing import, or explicitly bridge the value into Kain.",
                    ))
                })
        }
        Expr::Paren(inner, _) => infer_expr_type(env, inner, ctx),
        Expr::Block(block, _) => {
            let block_ctx = ctx.cloned().unwrap_or(SemanticContext {
                function_name: "<block>".to_string(),
                return_type: ResolvedType::Unit,
                effects: EffectSet::new(),
            });
            block_value_type(env, block, &block_ctx)
        }
        Expr::Unary { op, operand, span } => {
            let operand_ty = infer_expr_type(env, operand, ctx)?;
            let operand_base_ty = peel_shared_refs(&operand_ty);
            match op {
                UnaryOp::Neg => {
                    if is_numeric_like(operand_base_ty)
                        || matches!(operand_base_ty, ResolvedType::Unknown)
                    {
                        Ok(operand_base_ty.clone())
                    } else {
                        Err(env.type_error(
                            format!(
                                "Unary '-' expects a numeric value, found {}",
                                describe_type(&operand_ty)
                            ),
                            *span,
                        ))
                    }
                }
                UnaryOp::Not => {
                    if matches!(
                        operand_base_ty,
                        ResolvedType::Bool | ResolvedType::Option(_)
                    ) || is_ts_import_scalar_comparison_operand(operand_base_ty)
                        || matches!(
                            operand_base_ty,
                            ResolvedType::Array(_, _)
                                | ResolvedType::Slice(_)
                                | ResolvedType::Tuple(_)
                                | ResolvedType::Struct(_, _)
                                | ResolvedType::Enum(_, _)
                                | ResolvedType::Function { .. }
                        )
                    {
                        Ok(ResolvedType::Bool)
                    } else {
                        Err(env.type_error(
                            format!(
                                "Unary '!' expects Bool, found {}",
                                describe_type(&operand_ty)
                            ),
                            *span,
                        ))
                    }
                }
                UnaryOp::BitNot => {
                    if is_integer_like(operand_base_ty) {
                        Ok(operand_base_ty.clone())
                    } else if is_numeric_like(operand_base_ty) {
                        Ok(ResolvedType::Int(IntSize::I64))
                    } else {
                        Err(env.type_error(
                            format!(
                                "Unary '~' expects an integer value, found {}",
                                describe_type(&operand_ty)
                            ),
                            *span,
                        ))
                    }
                }
                UnaryOp::Ref => Ok(ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(operand_ty),
                }),
                UnaryOp::RefMut => Ok(ResolvedType::Ref {
                    mutable: true,
                    inner: Box::new(operand_ty),
                }),
                UnaryOp::Deref => match operand_ty {
                    ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => Ok(*inner),
                    ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                    other => Err(env.type_error(
                        format!("Cannot dereference {}", describe_type(&other)),
                        *span,
                    )),
                },
            }
        }
        Expr::Binary {
            left,
            op,
            right,
            span,
        } => {
            let left_ty = infer_expr_type(env, left, ctx)?;
            let right_ty = infer_expr_type(env, right, ctx)?;
            infer_binary_type(env, op, &left_ty, &right_ty, *span)
        }
        Expr::StageCall {
            function,
            args,
            span,
            ..
        } => {
            let callee_ty = env.lookup(function).cloned().ok_or_else(|| {
                env.type_error(
                    format!("Unknown orchestrate stage function '{}'", function),
                    *span,
                )
            })?;
            match callee_ty {
                ResolvedType::Function { params, ret, .. } => {
                    if params.len() != args.len() {
                        return Err(env.type_error(
                            format!(
                                "Expected {} argument(s), found {}",
                                params.len(),
                                args.len()
                            ),
                            *span,
                        ));
                    }
                    for (param_ty, arg) in params.iter().zip(args.iter()) {
                        let arg_ty =
                            infer_expr_type_with_expected(env, &arg.value, ctx, Some(param_ty))?;
                        ensure_type_compatible(env, param_ty, &arg_ty, *span, "stage argument")?;
                    }
                    Ok(*ret)
                }
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "Orchestrate stage '{}' does not resolve to a function (found {})",
                        function,
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::Call { callee, args, span } => {
            if let Expr::Ident(callee_name, _) = callee.as_ref() {
                if let Some(actor_call_ty) =
                    infer_actor_ask_call_type(env, ctx, callee_name, args, *span)
                {
                    return actor_call_ty;
                }
                if let Some(constructor_ty) =
                    infer_scalar_type_constructor_call(env, ctx, callee_name, args, *span)
                {
                    return constructor_ty;
                }
                if let Some(host_call_ty) =
                    infer_selfhost_host_call_type(env, ctx, callee_name, args, *span)
                {
                    return host_call_ty;
                }
            }
            let callee_ty = infer_expr_type(env, callee, ctx)?;

            match callee_ty {
                ResolvedType::Function {
                    params,
                    ret,
                    effects,
                } => {
                    for (param_ty, arg) in params.iter().zip(args.iter()) {
                        let arg_ty =
                            infer_expr_type_with_expected(env, &arg.value, ctx, Some(param_ty))?;
                        ensure_type_compatible(env, param_ty, &arg_ty, *span, "function argument")?;
                    }
                    for arg in args.iter().skip(params.len()) {
                        let _ = infer_expr_type(env, &arg.value, ctx)?;
                    }
                    if let (Some(ctx), Expr::Ident(callee_name, _)) = (ctx, callee.as_ref()) {
                        check_effect_call(
                            &ctx.effects,
                            &effects,
                            &ctx.function_name,
                            callee_name,
                            *span,
                        )
                        .map_err(|error| env.attach_effect_source(error, *span))?;
                    }
                    Ok(*ret)
                }
                ResolvedType::Option(inner) => match inner.as_ref() {
                    ResolvedType::Unknown | ResolvedType::Never => Ok(ResolvedType::Unknown),
                    ResolvedType::Function { ret, .. } => Ok(ret.as_ref().clone()),
                    _ => Ok(ResolvedType::Unknown),
                },
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                ResolvedType::Struct(_, _)
                | ResolvedType::Enum(_, _)
                | ResolvedType::Generic(_) => {
                    for arg in args {
                        let _ = infer_expr_type(env, &arg.value, ctx)?;
                    }
                    Ok(ResolvedType::Unknown)
                }
                other => Err(env.type_error(
                    format!(
                        "Cannot call non-function value of type {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::MethodCall {
            receiver,
            method,
            args,
            span,
        } => {
            let receiver_ty = infer_expr_type(env, receiver, ctx)?;
            infer_method_call_type(env, ctx, &receiver_ty, method, args, *span)
        }
        Expr::Field {
            object,
            field,
            span,
        } => {
            let object_ty = infer_expr_type(env, object, ctx)?;
            field_access_type(env, &object_ty, field, *span)
        }
        Expr::Index {
            object,
            index,
            span,
        } => {
            let object_ty = infer_expr_type(env, object, ctx)?;
            if let Expr::Range { start, end, .. } = index.as_ref() {
                for bound in [start.as_ref(), end.as_ref()].into_iter().flatten() {
                    let bound_ty = infer_expr_type(env, bound, ctx)?;
                    ensure_type_compatible(
                        env,
                        &ResolvedType::Int(IntSize::I64),
                        &bound_ty,
                        bound.span(),
                        "range index bound",
                    )?;
                }
                match object_ty {
                    ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => {
                        Ok(ResolvedType::Slice(inner))
                    }
                    ResolvedType::String => Ok(ResolvedType::String),
                    ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
                        match *inner {
                            ResolvedType::Array(item, _) | ResolvedType::Slice(item) => {
                                Ok(ResolvedType::Slice(item))
                            }
                            ResolvedType::String => Ok(ResolvedType::String),
                            ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                            other => Err(env.type_error(
                                format!(
                                    "Range indexing requires an array, slice, or String, found {}",
                                    describe_type(&other)
                                ),
                                *span,
                            )),
                        }
                    }
                    ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                    other => Err(env.type_error(
                        format!(
                            "Range indexing requires an array, slice, or String, found {}",
                            describe_type(&other)
                        ),
                        *span,
                    )),
                }
            } else {
                let index_ty = infer_expr_type(env, index, ctx)?;
                if !types_compatible(&ResolvedType::Int(IntSize::I64), &index_ty)
                    && !matches!(
                        index_ty,
                        ResolvedType::String
                            | ResolvedType::Unknown
                            | ResolvedType::Generic(_)
                            | ResolvedType::Struct(_, _)
                    )
                    && !is_none_placeholder_type(&index_ty)
                {
                    return Err(env.type_error(
                        format!(
                            "index expression expected Int, String, or Unknown, found {}",
                            describe_type(&index_ty)
                        ),
                        index.span(),
                    ));
                }
                match object_ty {
                    ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => Ok(*inner),
                    ResolvedType::Tuple(items) => Ok(tuple_index_type(&items, index.as_ref())),
                    ResolvedType::String => Ok(ResolvedType::String),
                    ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
                        match *inner {
                            ResolvedType::Array(item, _) | ResolvedType::Slice(item) => Ok(*item),
                            ResolvedType::Tuple(items) => {
                                Ok(tuple_index_type(&items, index.as_ref()))
                            }
                            ResolvedType::String => Ok(ResolvedType::String),
                            ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                            other => Err(env.type_error(
                                format!(
                                    "Indexing requires an array, slice, or String, found {}",
                                    describe_type(&other)
                                ),
                                *span,
                            )),
                        }
                    }
                    ResolvedType::Option(inner) => match *inner {
                        ResolvedType::Array(item, _) | ResolvedType::Slice(item) => Ok(*item),
                        ResolvedType::String => Ok(ResolvedType::String),
                        ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                        _ => Ok(ResolvedType::Unknown),
                    },
                    ResolvedType::Struct(name, fields)
                        if name == "Record" || name == "Map" || fields.is_empty() =>
                    {
                        Ok(ResolvedType::Unknown)
                    }
                    ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                    other => Err(env.type_error(
                        format!(
                            "Indexing requires an array, slice, or String, found {}",
                            describe_type(&other)
                        ),
                        *span,
                    )),
                }
            }
        }
        Expr::Assign {
            target,
            value,
            span,
        } => {
            let target_ty = infer_assignment_target_type(env, target, ctx)?;
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_type_compatible(env, &target_ty, &value_ty, *span, "assignment")?;
            Ok(ResolvedType::Unit)
        }
        Expr::Struct {
            name,
            fields,
            rest,
            span,
        } => {
            let struct_ty = env
                .lookup_type(name)
                .cloned()
                .unwrap_or_else(|| ResolvedType::Struct(name.clone(), HashMap::new()));
            match &struct_ty {
                ResolvedType::Struct(_, known_fields) => {
                    for (field_name, field_expr) in fields {
                        let field_ty = known_fields
                            .get(field_name)
                            .cloned()
                            .unwrap_or(ResolvedType::Unknown);
                        let value_ty = infer_expr_type(env, field_expr, ctx)?;
                        ensure_type_compatible(
                            env,
                            &field_ty,
                            &value_ty,
                            field_expr.span(),
                            "struct field",
                        )?;
                    }
                    if let Some(rest_expr) = rest {
                        let rest_ty = infer_expr_type(env, rest_expr, ctx)?;
                        ensure_type_compatible(
                            env,
                            &struct_ty,
                            &rest_ty,
                            rest_expr.span(),
                            "struct rest expression",
                        )?;
                    }
                    Ok(struct_ty)
                }
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "'{}' does not resolve to a struct type (found {})",
                        name,
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::AggregateInit { ty, fields, .. } => {
            let aggregate_ty = resolve_type_in_env(env, ty)?;
            for (_, value) in fields {
                let _ = infer_expr_type(env, value, ctx)?;
            }
            Ok(aggregate_ty)
        }
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            span,
        } => {
            if let Some(builtin_ty) =
                builtin_variant_expr_type(env, ctx, enum_name, variant, fields, *span)?
            {
                return Ok(builtin_ty);
            }
            let enum_ty = env
                .lookup_type(enum_name)
                .cloned()
                .unwrap_or_else(|| ResolvedType::Enum(enum_name.clone(), Vec::new()));
            if let Some(expected_fields) =
                env.lookup_enum_variant_fields(enum_name, variant).cloned()
            {
                match (fields, expected_fields.as_slice()) {
                    (EnumVariantFields::Unit, []) => {}
                    (EnumVariantFields::Tuple(values), expected) => {
                        if values.len() != expected.len() {
                            return Err(env.type_error(
                                format!(
                                    "Variant '{}::{}' expects {} field(s), found {}",
                                    enum_name,
                                    variant,
                                    expected.len(),
                                    values.len()
                                ),
                                *span,
                            ));
                        }
                        for (value, expected_ty) in values.iter().zip(expected.iter()) {
                            let value_ty = infer_expr_type(env, value, ctx)?;
                            ensure_type_compatible(
                                env,
                                expected_ty,
                                &value_ty,
                                value.span(),
                                "enum variant field",
                            )?;
                        }
                    }
                    (EnumVariantFields::Struct(values), expected) => {
                        if values.len() != expected.len() {
                            return Err(env.type_error(
                                format!(
                                    "Variant '{}::{}' expects {} field(s), found {}",
                                    enum_name,
                                    variant,
                                    expected.len(),
                                    values.len()
                                ),
                                *span,
                            ));
                        }
                        for (field_name, value) in values {
                            let Some(expected_ty) = env
                                .lookup_enum_variant_named_field(enum_name, variant, field_name)
                                .cloned()
                            else {
                                return Err(env.type_error(
                                    format!(
                                        "Unknown field '{}::{}.{}'",
                                        enum_name, variant, field_name
                                    ),
                                    value.span(),
                                ));
                            };
                            let value_ty = infer_expr_type(env, value, ctx)?;
                            ensure_type_compatible(
                                env,
                                &expected_ty,
                                &value_ty,
                                value.span(),
                                "enum variant field",
                            )?;
                        }
                    }
                    _ => {}
                }
            }
            Ok(enum_ty)
        }
        Expr::Array(values, _) => infer_array_type(env, values, ctx),
        Expr::Tuple(values, _) => {
            if values.is_empty() {
                Ok(ResolvedType::Unit)
            } else {
                Ok(ResolvedType::Tuple(
                    values
                        .iter()
                        .map(|value| infer_expr_type(env, value, ctx))
                        .collect::<Result<_, _>>()?,
                ))
            }
        }
        Expr::Range {
            start, end, span, ..
        } => {
            if let Some(start) = start {
                let start_ty = infer_expr_type(env, start, ctx)?;
                if !is_numeric_like(&start_ty) && !matches!(start_ty, ResolvedType::Unknown) {
                    return Err(env.type_error(
                        format!(
                            "Range start must be numeric, found {}",
                            describe_type(&start_ty)
                        ),
                        *span,
                    ));
                }
            }
            if let Some(end) = end {
                let end_ty = infer_expr_type(env, end, ctx)?;
                if !is_numeric_like(&end_ty) && !matches!(end_ty, ResolvedType::Unknown) {
                    return Err(env.type_error(
                        format!(
                            "Range end must be numeric, found {}",
                            describe_type(&end_ty)
                        ),
                        *span,
                    ));
                }
            }
            Ok(ResolvedType::Unknown)
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            span: _,
        } => {
            let cond_ty = infer_expr_type(env, condition, ctx)?;
            ensure_condition_type_compatible(env, &cond_ty, condition.span(), "if condition")?;

            let branch_ctx = ctx.cloned().unwrap_or(SemanticContext {
                function_name: "<if>".to_string(),
                return_type: ResolvedType::Unit,
                effects: EffectSet::new(),
            });

            let then_ty = block_value_type(env, then_branch, &branch_ctx)?;

            let else_ty = if let Some(else_branch) = else_branch {
                infer_else_branch_type(env, else_branch.as_ref(), &branch_ctx)?
            } else {
                ResolvedType::Unit
            };

            Ok(unify_types(&then_ty, &else_ty).unwrap_or(ResolvedType::Unknown))
        }
        Expr::Match {
            scrutinee,
            arms,
            span,
        } => infer_match_type(env, scrutinee, arms, *span, ctx),
        Expr::Lambda {
            params,
            return_type,
            body,
            span: _,
        } => {
            let ret = return_type
                .as_ref()
                .map(|ty| resolve_type_in_env(env, ty))
                .transpose()?
                .unwrap_or(ResolvedType::Unknown);
            let (param_types, body_ty) = env.with_scope(|env| {
                let mut param_types = Vec::new();
                for param in params {
                    let ty = resolve_param_type(env, param, None)?;
                    env.define(param.name.clone(), ty.clone());
                    param_types.push(ty);
                }
                let body_ty = infer_expr_type(env, body, ctx)?;
                Ok((param_types, body_ty))
            })?;
            if !matches!(
                (&ret, &body_ty),
                (ResolvedType::Unknown, _)
                    | (_, ResolvedType::Unknown)
                    | (_, ResolvedType::Unit)
                    | (_, ResolvedType::Never)
            ) {
                ensure_type_compatible(env, &ret, &body_ty, body.span(), "lambda body")?;
            }
            Ok(ResolvedType::Function {
                params: param_types,
                ret: Box::new(if matches!(ret, ResolvedType::Unknown) {
                    body_ty
                } else {
                    ret
                }),
                effects: EffectSet::new(),
            })
        }
        Expr::Ref { mutable, value, .. } => Ok(ResolvedType::Ref {
            mutable: *mutable,
            inner: Box::new(infer_expr_type(env, value, ctx)?),
        }),
        Expr::AddrOf {
            value, pointee_ty, ..
        } => Ok(ResolvedType::Ptr {
            mutable: false,
            inner: Box::new(
                pointee_ty
                    .as_ref()
                    .map(|ty| resolve_type_in_env(env, ty))
                    .transpose()?
                    .unwrap_or(infer_expr_type(env, value, ctx)?),
            ),
        }),
        Expr::Deref(value, span) => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            match value_ty {
                ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => Ok(*inner),
                ResolvedType::Struct(_, _) | ResolvedType::Enum(_, _) => Ok(value_ty),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!("Cannot dereference {}", describe_type(&other)),
                    *span,
                )),
            }
        }
        Expr::PtrOffset { pointer, .. } => infer_expr_type(env, pointer, ctx),
        Expr::MemLoad {
            pointer,
            load_ty,
            span,
        } => {
            if env.in_shared_region() {
                return Err(env.type_error(
                    "mem_load is not allowed inside share scopes; use atomic_load in v1",
                    *span,
                ));
            }
            if let Some(load_ty) = load_ty {
                return resolve_type_in_env(env, load_ty);
            }
            let pointer_ty = infer_expr_type(env, pointer, ctx)?;
            match pointer_ty {
                ResolvedType::Ptr { inner, .. } | ResolvedType::Ref { inner, .. } => Ok(*inner),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "mem_load expects a pointer, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::MemStore {
            pointer,
            value,
            store_ty,
            span,
        } => {
            if env.in_shared_region() {
                return Err(env.type_error(
                    "mem_store is not allowed inside share scopes; use atomic_store in v1",
                    *span,
                ));
            }
            let expected_ty = if let Some(store_ty) = store_ty {
                resolve_type_in_env(env, store_ty)?
            } else {
                match infer_expr_type(env, pointer, ctx)? {
                    ResolvedType::Ptr { inner, .. } | ResolvedType::Ref { inner, .. } => *inner,
                    ResolvedType::Unknown => ResolvedType::Unknown,
                    other => {
                        return Err(env.type_error(
                            format!(
                                "mem_store expects a pointer, found {}",
                                describe_type(&other)
                            ),
                            *span,
                        ))
                    }
                }
            };
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &value_ty,
                value.span(),
                "mem_store value",
            )?;
            Ok(ResolvedType::Unit)
        }
        Expr::VolatileLoad {
            pointer,
            load_ty,
            span,
        } => {
            if env.in_shared_region() {
                return Err(env.type_error(
                    "volatile_load is not a synchronization operation inside share scopes; use atomic_load",
                    *span,
                ));
            }
            if let Some(load_ty) = load_ty {
                return resolve_type_in_env(env, load_ty);
            }
            let pointer_ty = infer_expr_type(env, pointer, ctx)?;
            match pointer_ty {
                ResolvedType::Ptr { inner, .. } | ResolvedType::Ref { inner, .. } => Ok(*inner),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "volatile_load expects a pointer, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::VolatileStore {
            pointer,
            value,
            store_ty,
            span,
        } => {
            if env.in_shared_region() {
                return Err(env.type_error(
                    "volatile_store is not a synchronization operation inside share scopes; use atomic_store",
                    *span,
                ));
            }
            let expected_ty = if let Some(store_ty) = store_ty {
                resolve_type_in_env(env, store_ty)?
            } else {
                match infer_expr_type(env, pointer, ctx)? {
                    ResolvedType::Ptr { inner, .. } | ResolvedType::Ref { inner, .. } => *inner,
                    ResolvedType::Unknown => ResolvedType::Unknown,
                    other => {
                        return Err(env.type_error(
                            format!(
                                "volatile_store expects a pointer, found {}",
                                describe_type(&other)
                            ),
                            *span,
                        ))
                    }
                }
            };
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &value_ty,
                value.span(),
                "volatile_store value",
            )?;
            Ok(ResolvedType::Unit)
        }
        Expr::AtomicLoad {
            pointer,
            load_ty,
            span,
            ..
        } => {
            if !env.in_shared_region() && !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    "atomic_load requires an enclosing share scope or Unsafe effect",
                    *span,
                ));
            }
            infer_atomic_element_type(env, pointer, load_ty.as_ref(), ctx, *span, "atomic_load")
        }
        Expr::AtomicStore {
            pointer,
            value,
            store_ty,
            span,
            ..
        } => {
            if !env.in_shared_region() && !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    "atomic_store requires an enclosing share scope or Unsafe effect",
                    *span,
                ));
            }
            let expected_ty = infer_atomic_element_type(
                env,
                pointer,
                store_ty.as_ref(),
                ctx,
                *span,
                "atomic_store",
            )?;
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &value_ty,
                value.span(),
                "atomic_store value",
            )?;
            Ok(ResolvedType::Unit)
        }
        Expr::AtomicAdd {
            pointer,
            value,
            op_ty,
            span,
            ..
        }
        | Expr::AtomicSub {
            pointer,
            value,
            op_ty,
            span,
            ..
        }
        | Expr::AtomicAnd {
            pointer,
            value,
            op_ty,
            span,
            ..
        }
        | Expr::AtomicOr {
            pointer,
            value,
            op_ty,
            span,
            ..
        }
        | Expr::AtomicXor {
            pointer,
            value,
            op_ty,
            span,
            ..
        }
        | Expr::AtomicExchange {
            pointer,
            value,
            op_ty,
            span,
            ..
        } => {
            if !env.in_shared_region() && !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    "atomic operations require an enclosing share scope or Unsafe effect",
                    *span,
                ));
            }
            let expected_ty = infer_atomic_element_type(
                env,
                pointer,
                op_ty.as_ref(),
                ctx,
                *span,
                "atomic operation",
            )?;
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &value_ty,
                value.span(),
                "atomic operation value",
            )?;
            Ok(expected_ty)
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            op_ty,
            span,
            ..
        } => {
            if !env.in_shared_region() && !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    "atomic_compare_exchange requires an enclosing share scope or Unsafe effect",
                    *span,
                ));
            }
            let expected_ty = infer_atomic_element_type(
                env,
                pointer,
                op_ty.as_ref(),
                ctx,
                *span,
                "atomic_compare_exchange",
            )?;
            let compare_ty = infer_expr_type(env, expected, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &compare_ty,
                expected.span(),
                "atomic_compare_exchange expected value",
            )?;
            let desired_ty = infer_expr_type(env, desired, ctx)?;
            ensure_type_compatible(
                env,
                &expected_ty,
                &desired_ty,
                desired.span(),
                "atomic_compare_exchange desired value",
            )?;
            Ok(ResolvedType::Bool)
        }
        Expr::AtomicFence { span, .. } => {
            if !env.in_shared_region() && !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    "atomic_fence requires an enclosing share scope or Unsafe effect",
                    *span,
                ));
            }
            Ok(ResolvedType::Unit)
        }
        Expr::CpuFence { kind, span } => {
            if !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error(
                    format!("{} requires Unsafe effect", kind.intrinsic_name()),
                    *span,
                ));
            }
            Ok(ResolvedType::Unit)
        }
        Expr::CpuCacheFlush { pointer, span } => {
            if !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error("clflush requires Unsafe effect", *span));
            }
            let pointer_ty = infer_expr_type(env, pointer, ctx)?;
            match pointer_ty {
                ResolvedType::Unknown => Ok(ResolvedType::Unit),
                other if resolved_type_is_address_like(&other) => Ok(ResolvedType::Unit),
                other => Err(env.type_error(
                    format!(
                        "clflush expects a pointer or integer address, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::InlineAsm {
            operands,
            options,
            span,
            ..
        } => {
            if !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error("asm requires Unsafe effect", *span));
            }
            if !options.constraints.is_empty() && options.constraints.len() != operands.len() {
                return Err(env.type_error(
                    format!(
                        "asm constraints expected {} entries for {} operands, found {}",
                        operands.len(),
                        operands.len(),
                        options.constraints.len()
                    ),
                    *span,
                ));
            }
            for operand in operands {
                let operand_ty = infer_expr_type(env, operand, ctx)?;
                if matches!(operand_ty, ResolvedType::Unknown) {
                    continue;
                }
                if !resolved_type_supports_inline_asm_operand(&operand_ty) {
                    return Err(env.type_error(
                        format!(
                            "asm operands must be integer-like or pointer-like in v1, found {}",
                            describe_type(&operand_ty)
                        ),
                        operand.span(),
                    ));
                }
            }
            Ok(ResolvedType::Unit)
        }
        Expr::SizeOfType { .. } | Expr::AlignOfType { .. } => Ok(ResolvedType::Int(IntSize::I64)),
        Expr::Alloca { ty, .. } => Ok(ResolvedType::Ptr {
            mutable: true,
            inner: Box::new(resolve_type_in_env(env, ty)?),
        }),
        Expr::Uninit { ty, .. } => resolve_type_in_env(env, ty),
        Expr::Alloc { ty, .. } => Ok(ResolvedType::Ptr {
            mutable: true,
            inner: Box::new(
                ty.as_ref()
                    .map(|ty| resolve_type_in_env(env, ty))
                    .transpose()?
                    .unwrap_or(ResolvedType::Unknown),
            ),
        }),
        Expr::Realloc { ty, .. } => Ok(ResolvedType::Ptr {
            mutable: true,
            inner: Box::new(
                ty.as_ref()
                    .map(|ty| resolve_type_in_env(env, ty))
                    .transpose()?
                    .unwrap_or(ResolvedType::Unknown),
            ),
        }),
        Expr::Observe { target, body, span } => {
            if env.in_shared_region() {
                return Err(
                    env.type_error("observe is not allowed inside share scopes in v1", *span)
                );
            }
            validate_ownership_target(env, target, OBSERVE_KEYWORD, *span)?;
            ensure_ownership_scope_has_structured_exit(env, body, OBSERVE_KEYWORD, *span)?;
            infer_expr_type(env, body, ctx)
        }
        Expr::Collapse { target, body, span } => {
            if env.in_shared_region() {
                return Err(
                    env.type_error("collapse is not allowed inside share scopes in v1", *span)
                );
            }
            validate_ownership_target(env, target, COLLAPSE_KEYWORD, *span)?;
            ensure_ownership_scope_has_structured_exit(env, body, COLLAPSE_KEYWORD, *span)?;
            infer_expr_type(env, body, ctx)
        }
        Expr::Decay { target, span } => {
            if env.in_shared_region() {
                return Err(env.type_error("decay is not allowed inside share scopes in v1", *span));
            }
            validate_ownership_target(env, target, DECAY_KEYWORD, *span)?;
            Ok(ResolvedType::Unit)
        }
        Expr::Share { target, body, span } => {
            if env.in_fanout() {
                return Err(env.type_error(
                    "share scopes cannot begin inside fanout bodies in v1",
                    *span,
                ));
            }
            validate_ownership_target(env, target, SHARE_KEYWORD, *span)?;
            ensure_ownership_scope_has_structured_exit(env, body, SHARE_KEYWORD, *span)?;
            env.push_shared_region();
            let result = infer_expr_type(env, body, ctx);
            env.pop_shared_region();
            result
        }
        Expr::Teleport {
            value,
            source_world,
            target_world,
            channel,
            span,
        } => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            ensure_teleport_world_reference(env, source_world, "source", *span)?;
            ensure_teleport_world_reference(env, target_world, "target", *span)?;
            if source_world == target_world {
                return Err(
                    env.type_error("teleport requires distinct source and target worlds", *span)
                );
            }
            if channel.as_ref().is_some_and(|name| name.trim().is_empty()) {
                return Err(env.type_error("teleport channel cannot be empty", *span));
            }
            if let Expr::Ident(name, _) = value.as_ref() {
                env.mark_moved(name);
            }
            Ok(value_ty)
        }
        Expr::Cast { target, .. } => resolve_type_in_env(env, target),
        Expr::Bitcast {
            value,
            target,
            span,
        } => {
            if !context_allows_raw_memory_intrinsics(ctx) {
                return Err(env.type_error("bitcast requires Unsafe effect", *span));
            }
            let source_ty = infer_expr_type(env, value, ctx)?;
            let target_ty = resolve_type_in_env(env, target)?;
            ensure_bitcast_compatible(env, &source_ty, &target_ty, *span)?;
            Ok(target_ty)
        }
        Expr::Try(value, span) => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            match value_ty {
                ResolvedType::Result(ok, err) => {
                    if let Some(ctx) = ctx {
                        match &ctx.return_type {
                            ResolvedType::Result(_, return_err) => ensure_type_compatible(
                                env,
                                return_err.as_ref(),
                                err.as_ref(),
                                *span,
                                "propagated error",
                            )?,
                            ResolvedType::Unknown => {}
                            other => {
                                return Err(env.type_error(
                                    format!(
                                        "'?' on Result requires enclosing function to return Result, found {}",
                                        describe_type(other)
                                    ),
                                    *span,
                                ));
                            }
                        }
                    }
                    Ok(*ok)
                }
                ResolvedType::Option(inner) => {
                    if let Some(ctx) = ctx {
                        match &ctx.return_type {
                            ResolvedType::Option(_) | ResolvedType::Unknown => {}
                            other => {
                                return Err(env.type_error(
                                    format!(
                                        "'?' on Option requires enclosing function to return Option, found {}",
                                        describe_type(other)
                                    ),
                                    *span,
                                ));
                            }
                        }
                    }
                    Ok(*inner)
                }
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "'?' expects a Result or Option value, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::Await(value, _span) => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            match value_ty {
                ResolvedType::Future(inner) => Ok(*inner),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                _ => Ok(ResolvedType::Unknown),
            }
        }
        Expr::AsyncBlock(value, _) => Ok(ResolvedType::Future(Box::new(infer_expr_type(
            env, value, ctx,
        )?))),
        Expr::Spawn { actor, .. } => Ok(ResolvedType::Struct(actor.clone(), HashMap::new())),
        Expr::SendMsg { .. } => Ok(ResolvedType::Unit),
        Expr::Emit { .. } => Ok(ResolvedType::Unit),
        Expr::Comptime(value, _) => infer_expr_type(env, value, ctx),
        Expr::MacroCall { name, args, .. } => infer_macro_type(env, name, args, ctx),
        Expr::JSX(node, _) => {
            check_jsx_semantics(env, node, ctx)?;
            Ok(ResolvedType::Unit)
        }
        Expr::Return(Some(value), span) => {
            if let Some(ctx) = ctx {
                let value_ty = infer_expr_type(env, value, Some(ctx))?;
                ensure_type_compatible(env, &ctx.return_type, &value_ty, *span, "return value")?;
            }
            Ok(ResolvedType::Never)
        }
        Expr::Return(None, span) => {
            if let Some(ctx) = ctx {
                ensure_type_compatible(
                    env,
                    &ctx.return_type,
                    &ResolvedType::Unit,
                    *span,
                    "return",
                )?;
            }
            Ok(ResolvedType::Never)
        }
        Expr::Break(_, _) | Expr::Continue(_) => Ok(ResolvedType::Never),
    }
}

fn ensure_teleport_world_reference(
    env: &TypeEnv<'_>,
    world_name: &str,
    label: &str,
    span: Span,
) -> KainResult<()> {
    if matches!(
        env.lookup_type(world_name),
        Some(ResolvedType::Struct(_, _))
    ) || matches!(env.lookup(world_name), Some(ResolvedType::Struct(_, _)))
    {
        return Ok(());
    }
    Err(env.type_error(
        format!("teleport {label} world '{}' is not declared", world_name),
        span,
    ))
}

fn validate_ownership_target(
    env: &mut TypeEnv,
    target: &Expr,
    operation: &str,
    span: Span,
) -> KainResult<()> {
    let target_ty = infer_expr_type(env, target, None)?;

    // ── Infer the actual ownership region from context ─────────────────
    // Instead of hardcoding HeapAllocation (the most permissive policy),
    // determine the region from the expression context so that
    // region-specific restrictions are enforced at check time.
    let region_kind = infer_ownership_region(env, target, &target_ty);
    let policy = OwnershipPolicy::for_region(region_kind);

    let supported = match operation {
        OBSERVE_KEYWORD => policy.supports_observe(),
        COLLAPSE_KEYWORD => policy.supports_collapse(),
        DECAY_KEYWORD => policy.supports_decay(),
        SHARE_KEYWORD => policy.supports_share(),
        _ => false,
    };
    if !supported {
        let explanation = match (operation, region_kind) {
            (DECAY_KEYWORD, OwnershipRegionKind::WorldState) =>
                "World state is compiler-owned; you cannot decay it. \
                 World state persists for the program lifetime.",
            (DECAY_KEYWORD, OwnershipRegionKind::EntangledMirror) =>
                "Entangled mirrors are read-only snapshots; you cannot decay them. \
                 Only the entangled authority can modify the source world.",
            (COLLAPSE_KEYWORD, OwnershipRegionKind::EntangledMirror) =>
                "Entangled mirrors are read-only snapshots; you cannot collapse them. \
                 The mirror reflects the entangled world -- it's not a mutable allocation.",
            (SHARE_KEYWORD, OwnershipRegionKind::LocalAlloca) =>
                "Stack-allocated (local) pointers cannot be shared. \
                 Share requires a heap allocation. Use alloc() to create a shareable pointer.",
            (SHARE_KEYWORD, OwnershipRegionKind::WorldState) =>
                "World state pointers cannot be shared. \
                 World state is compiler-managed and does not support parallel write lanes.",
            _ => "This ownership operation is not valid for this memory region.",
        };
        let help = match (operation, region_kind) {
            (DECAY_KEYWORD, OwnershipRegionKind::WorldState) =>
                "Remove the decay - world state is automatically managed by the compiler.",
            (SHARE_KEYWORD, OwnershipRegionKind::LocalAlloca) =>
                "Use alloc() or alloc_zeroed() instead of stack allocation.",
            _ => "Check that the pointer targets a compatible memory region.",
        };
        return Err(env.type_error(
            format!(
                "{operation} is not supported for {} ownership regions: {} {}",
                region_kind.as_str(),
                explanation,
                help
            ),
            span,
        ));
    }

    match peel_shared_refs(&target_ty) {
        ResolvedType::Ptr { .. } | ResolvedType::Ref { .. } | ResolvedType::Unknown => Ok(()),
        other => Err(env.type_error(
            format!(
                "{operation} expects a pointer-like ownership region, found {}",
                describe_type(other)
            ),
            target.span(),
        )),
    }
}

/// Infer the ownership region kind from context.
///
/// This determines what kind of memory region a pointer targets,
/// which gates which ownership operations are legal on it.
/// When the region cannot be confidently determined, falls back to
/// `HeapAllocation` (the most permissive policy) to avoid false positives.
fn infer_ownership_region(
    env: &TypeEnv,
    target: &Expr,
    _target_ty: &ResolvedType,
) -> OwnershipRegionKind {
    // Check 1: alloc() / alloc_zeroed() / realloc_mem() calls
    //   These are comptime-known heap allocation functions.
    if is_alloc_call(target) {
        return OwnershipRegionKind::HeapAllocation;
    }

    // Check 2: Dotted access whose base is a known type
    //   e.g., MyWorld.my_field -- if MyWorld is in env.types,
    //   this is a world state field access.
    if let Expr::Field { object, .. } = target {
        if let Some(base_name) = extract_world_name(object) {
            if env.lookup_type(&base_name).is_some() {
                return OwnershipRegionKind::WorldState;
            }
        }
    }

    // Check 3: Direct ident -- distinguish local variables from types/globals
    if let Expr::Ident(name, _) = target {
        // If the name is registered as a type AND resolves, classify as
        // WorldState (worlds are registered as struct types).
        if env.lookup_type(name).is_some() {
            return OwnershipRegionKind::WorldState;
        }
        // If NOT a type but resolves to a Ptr/Ref type, treat as local
        // alloca (stack variable or function parameter).
        if let Some(ty) = env.lookup(name) {
            match ty {
                ResolvedType::Ptr { .. } | ResolvedType::Ref { .. } => {
                    return OwnershipRegionKind::LocalAlloca;
                }
                _ => {}
            }
        }
    }

    // Default: most permissive (preserves backward compatibility)
    // This ensures we never REJECT valid code, only ADD new checks
    OwnershipRegionKind::HeapAllocation
}

/// Check if an expression is a call to alloc() or alloc_zeroed().
fn is_alloc_call(expr: &Expr) -> bool {
    if let Expr::Call { callee, .. } = expr {
        if let Expr::Ident(name, _) = callee.as_ref() {
            return name == "alloc" || name == "alloc_zeroed" || name == "realloc_mem";
        }
    }
    false
}

/// Try to extract a world name from an expression (e.g., MyWorld.field → "MyWorld").
fn extract_world_name(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, .. } => extract_world_name(object),
        _ => None,
    }
}

fn resolve_atomic_pointer_element_type(
    env: &TypeEnv,
    pointer_ty: ResolvedType,
    span: Span,
    operation: &str,
) -> KainResult<ResolvedType> {
    match pointer_ty {
        ResolvedType::Ptr { inner, .. } | ResolvedType::Ref { inner, .. } => Ok(*inner),
        ResolvedType::Unknown => Ok(ResolvedType::Unknown),
        other => Err(env.type_error(
            format!(
                "{operation} expects a pointer, found {}",
                describe_type(&other)
            ),
            span,
        )),
    }
}

fn ensure_supported_atomic_type(
    env: &TypeEnv,
    ty: &ResolvedType,
    span: Span,
    operation: &str,
) -> KainResult<()> {
    match peel_shared_refs(ty) {
        ResolvedType::Bool
        | ResolvedType::Int(IntSize::I32)
        | ResolvedType::Int(IntSize::I64)
        | ResolvedType::Int(IntSize::U32)
        | ResolvedType::Int(IntSize::U64)
        | ResolvedType::Unknown => Ok(()),
        other => Err(env.type_error(
            format!(
                "{operation} only supports Bool, I32, I64, U32, and U64 in v1, found {}",
                describe_type(other)
            ),
            span,
        )),
    }
}

fn infer_atomic_element_type(
    env: &mut TypeEnv,
    pointer: &Expr,
    explicit_ty: Option<&Type>,
    ctx: Option<&SemanticContext>,
    span: Span,
    operation: &str,
) -> KainResult<ResolvedType> {
    let pointer_ty = infer_expr_type(env, pointer, ctx)?;
    let pointee_ty = resolve_atomic_pointer_element_type(env, pointer_ty, span, operation)?;
    let resolved = if let Some(explicit_ty) = explicit_ty {
        let explicit = resolve_type_in_env(env, explicit_ty)?;
        ensure_type_compatible(
            env,
            &pointee_ty,
            &explicit,
            span,
            &format!("{operation} pointer element"),
        )?;
        explicit
    } else {
        pointee_ty
    };
    ensure_supported_atomic_type(env, &resolved, span, operation)?;
    Ok(resolved)
}

fn ensure_ownership_scope_has_structured_exit(
    env: &TypeEnv,
    body: &Expr,
    operation: &str,
    span: Span,
) -> KainResult<()> {
    if ownership_expr_contains_early_exit(body) {
        return Err(env.type_error(
            format!(
                "{operation} scopes do not support return, break, or continue in v1; move the control flow outside the ownership block"
            ),
            span,
        ));
    }
    Ok(())
}

fn ownership_block_contains_early_exit(block: &Block) -> bool {
    block.stmts.iter().any(ownership_stmt_contains_early_exit)
}

fn ownership_stmt_contains_early_exit(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Return(_, _) | Stmt::Break(_, _) | Stmt::Continue(_) => true,
        Stmt::Let { value, .. } => value
            .as_ref()
            .is_some_and(ownership_expr_contains_early_exit),
        Stmt::Expr(expr) => ownership_expr_contains_early_exit(expr),
        Stmt::Defer { expr, .. } => ownership_expr_contains_early_exit(expr),
        Stmt::Dispatch { dispatch_size, .. } => {
            dispatch_size_any(dispatch_size, ownership_expr_contains_early_exit)
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            ownership_expr_contains_early_exit(iter) || ownership_block_contains_early_exit(body)
        }
        Stmt::While {
            condition, body, ..
        } => {
            ownership_expr_contains_early_exit(condition)
                || ownership_block_contains_early_exit(body)
        }
        Stmt::Loop { body, .. } => ownership_block_contains_early_exit(body),
        Stmt::Item(_) => false,
        Stmt::Subgroup { body, .. } => ownership_block_contains_early_exit(body),
    }
}

fn ownership_expr_contains_early_exit(expr: &Expr) -> bool {
    match expr {
        Expr::Return(_, _) | Expr::Break(_, _) | Expr::Continue(_) => true,
        Expr::Binary { left, right, .. } => {
            ownership_expr_contains_early_exit(left) || ownership_expr_contains_early_exit(right)
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Cast { value: operand, .. }
        | Expr::Bitcast { value: operand, .. }
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => ownership_expr_contains_early_exit(operand),
        Expr::Call { callee, args, .. } => {
            ownership_expr_contains_early_exit(callee)
                || args
                    .iter()
                    .any(|arg| ownership_expr_contains_early_exit(&arg.value))
        }
        Expr::StageCall { args, .. } => args
            .iter()
            .any(|arg| ownership_expr_contains_early_exit(&arg.value)),
        Expr::MacroCall { args, .. } => args.iter().any(ownership_expr_contains_early_exit),
        Expr::MethodCall { receiver, args, .. } => {
            ownership_expr_contains_early_exit(receiver)
                || args
                    .iter()
                    .any(|arg| ownership_expr_contains_early_exit(&arg.value))
        }
        Expr::Field { object, .. } => ownership_expr_contains_early_exit(object),
        Expr::Index { object, index, .. } => {
            ownership_expr_contains_early_exit(object) || ownership_expr_contains_early_exit(index)
        }
        Expr::Assign { target, value, .. } => {
            ownership_expr_contains_early_exit(target) || ownership_expr_contains_early_exit(value)
        }
        Expr::Struct { fields, rest, .. } => {
            fields
                .iter()
                .any(|(_, value)| ownership_expr_contains_early_exit(value))
                || rest
                    .as_ref()
                    .is_some_and(|value| ownership_expr_contains_early_exit(value))
        }
        Expr::AggregateInit { fields, .. } => fields
            .iter()
            .any(|(_, value)| ownership_expr_contains_early_exit(value)),
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Unit => false,
            EnumVariantFields::Tuple(values) => {
                values.iter().any(ownership_expr_contains_early_exit)
            }
            EnumVariantFields::Struct(values) => values
                .iter()
                .any(|(_, value)| ownership_expr_contains_early_exit(value)),
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
            values.iter().any(ownership_expr_contains_early_exit)
        }
        Expr::Range { start, end, .. } => {
            start
                .as_ref()
                .is_some_and(|value| ownership_expr_contains_early_exit(value))
                || end
                    .as_ref()
                    .is_some_and(|value| ownership_expr_contains_early_exit(value))
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            ownership_expr_contains_early_exit(condition)
                || ownership_block_contains_early_exit(then_branch)
                || else_branch
                    .as_ref()
                    .is_some_and(|branch| ownership_else_branch_contains_early_exit(branch))
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            ownership_expr_contains_early_exit(scrutinee)
                || arms.iter().any(|arm| {
                    arm.guard
                        .as_ref()
                        .is_some_and(ownership_expr_contains_early_exit)
                        || ownership_expr_contains_early_exit(&arm.body)
                })
        }
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            ownership_expr_contains_early_exit(pointer)
                || ownership_expr_contains_early_exit(offset)
        }
        Expr::MemLoad { pointer, .. }
        | Expr::VolatileLoad { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        }
        | Expr::Teleport { value: pointer, .. } => ownership_expr_contains_early_exit(pointer),
        Expr::MemStore { pointer, value, .. } | Expr::VolatileStore { pointer, value, .. } => {
            ownership_expr_contains_early_exit(pointer) || ownership_expr_contains_early_exit(value)
        }
        Expr::AtomicLoad { pointer, .. } => ownership_expr_contains_early_exit(pointer),
        Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            ownership_expr_contains_early_exit(pointer) || ownership_expr_contains_early_exit(value)
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => {
            ownership_expr_contains_early_exit(pointer)
                || ownership_expr_contains_early_exit(expected)
                || ownership_expr_contains_early_exit(desired)
        }
        Expr::AtomicFence { .. } => false,
        Expr::CpuFence { .. } => false,
        Expr::CpuCacheFlush { pointer, .. } => ownership_expr_contains_early_exit(pointer),
        Expr::InlineAsm { operands, .. } => operands.iter().any(ownership_expr_contains_early_exit),
        Expr::Alloc { size, .. } => ownership_expr_contains_early_exit(size),
        Expr::Realloc { pointer, size, .. } => {
            ownership_expr_contains_early_exit(pointer) || ownership_expr_contains_early_exit(size)
        }
        Expr::Observe { target, body, .. }
        | Expr::Collapse { target, body, .. }
        | Expr::Share { target, body, .. } => {
            ownership_expr_contains_early_exit(target) || ownership_expr_contains_early_exit(body)
        }
        Expr::SendMsg { target, data, .. } => {
            ownership_expr_contains_early_exit(target)
                || data
                    .iter()
                    .any(|(_, value)| ownership_expr_contains_early_exit(value))
        }
        Expr::Emit { data, .. } => {
            data
                .iter()
                .any(|(_, value)| ownership_expr_contains_early_exit(value))
        }
        Expr::Block(block, _) => ownership_block_contains_early_exit(block),
        Expr::Lambda { .. } => false,
        Expr::Spawn { init, .. } => init
            .iter()
            .any(|(_, value)| ownership_expr_contains_early_exit(value)),
        Expr::JSX(_, _)
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. } => false,
    }
}

fn ownership_else_branch_contains_early_exit(branch: &ElseBranch) -> bool {
    match branch {
        ElseBranch::Else(block) => ownership_block_contains_early_exit(block),
        ElseBranch::ElseIf(condition, block, next) => {
            ownership_expr_contains_early_exit(condition)
                || ownership_block_contains_early_exit(block)
                || next
                    .as_ref()
                    .is_some_and(|branch| ownership_else_branch_contains_early_exit(branch))
        }
    }
}

fn infer_scalar_type_constructor_call(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    callee_name: &str,
    args: &[CallArg],
    span: Span,
) -> Option<KainResult<ResolvedType>> {
    if value_binding_exists(env, callee_name) {
        return None;
    }
    let target_ty = env.lookup_type(callee_name).cloned()?;
    if !is_scalar_constructor_target(&target_ty) {
        return None;
    }
    if args.len() != 1 {
        return Some(Err(env.type_error(
            format!("{callee_name} constructor expects exactly one argument"),
            span,
        )));
    }
    Some(
        infer_expr_type(env, &args[0].value, ctx)
            .and_then(|source_ty| {
                validate_scalar_constructor_input(env, &target_ty, &source_ty, span)
            })
            .map(|_| target_ty),
    )
}

fn value_binding_exists(env: &TypeEnv<'_>, name: &str) -> bool {
    env.scopes
        .iter()
        .rev()
        .any(|scope| scope.contains_key(name))
        || env.globals.contains_key(name)
}

fn is_scalar_constructor_target(ty: &ResolvedType) -> bool {
    matches!(
        ty,
        ResolvedType::Int(_) | ResolvedType::Float(_) | ResolvedType::Bool | ResolvedType::Char
    )
}

fn validate_scalar_constructor_input(
    env: &TypeEnv<'_>,
    target_ty: &ResolvedType,
    source_ty: &ResolvedType,
    span: Span,
) -> KainResult<()> {
    let source_ty = peel_shared_refs(source_ty);
    if matches!(source_ty, ResolvedType::Unknown) {
        return Ok(());
    }
    let allowed = match target_ty {
        ResolvedType::Int(_) | ResolvedType::Float(_) => is_numeric_like(source_ty),
        ResolvedType::Bool => matches!(source_ty, ResolvedType::Bool) || is_numeric_like(source_ty),
        ResolvedType::Char => matches!(source_ty, ResolvedType::Char | ResolvedType::String),
        _ => false,
    };
    if allowed {
        Ok(())
    } else {
        Err(env.type_error(
            format!(
                "Cannot cast {} to {} with a scalar constructor",
                describe_type(source_ty),
                describe_type(target_ty)
            ),
            span,
        ))
    }
}

fn infer_else_branch_type(
    env: &mut TypeEnv,
    else_branch: &ElseBranch,
    ctx: &SemanticContext,
) -> KainResult<ResolvedType> {
    match else_branch {
        ElseBranch::Else(block) => block_value_type(env, block, ctx),
        ElseBranch::ElseIf(cond, block, next) => {
            let cond_ty = infer_expr_type(env, cond, Some(ctx))?;
            ensure_condition_type_compatible(env, &cond_ty, cond.span(), "else-if condition")?;
            let current_ty = block_value_type(env, block, ctx)?;
            let next_ty = if let Some(next) = next {
                infer_else_branch_type(env, next.as_ref(), ctx)?
            } else {
                ResolvedType::Unit
            };
            Ok(unify_types(&current_ty, &next_ty).unwrap_or(ResolvedType::Unknown))
        }
    }
}

fn infer_match_type(
    env: &mut TypeEnv,
    scrutinee: &Expr,
    arms: &[MatchArm],
    span: Span,
    ctx: Option<&SemanticContext>,
) -> KainResult<ResolvedType> {
    let scrutinee_ty = infer_expr_type(env, scrutinee, ctx)?;
    let mut seen_catch_all = false;
    let mut seen_literal_patterns = HashSet::new();
    let mut result_ty: Option<ResolvedType> = None;

    for arm in arms {
        if seen_catch_all {
            return Err(env.type_error("Unreachable match arm after a catch-all pattern", arm.span));
        }

        let is_catch_all = matches!(arm.pattern, Pattern::Wildcard(_))
            || matches!(arm.pattern, Pattern::Binding { .. });
        if is_catch_all {
            seen_catch_all = true;
        }

        if let Pattern::Literal(Expr::Bool(value, _)) = &arm.pattern {
            let key = format!("bool:{value}");
            if !seen_literal_patterns.insert(key) {
                return Err(env.type_error("Duplicate boolean match arm", arm.span));
            }
        }

        let arm_ty = env.with_scope(|env| {
            check_pattern_compatibility(env, &arm.pattern, &scrutinee_ty)?;
            bind_pattern_types(env, &arm.pattern, &scrutinee_ty)?;
            if let Some(guard) = &arm.guard {
                let guard_ty = infer_expr_type(env, guard, ctx)?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::Bool,
                    &guard_ty,
                    guard.span(),
                    "match guard",
                )?;
            }
            infer_expr_type(env, &arm.body, ctx)
        })?;

        result_ty = Some(if let Some(current) = result_ty {
            unify_types(&current, &arm_ty).ok_or_else(|| {
                env.type_error(
                    format!(
                        "match arms do not agree on a type: {} vs {}",
                        describe_type(&current),
                        describe_type(&arm_ty)
                    ),
                    arm.span,
                )
            })?
        } else {
            arm_ty
        });
    }

    if result_ty.is_none() {
        return Err(env.type_error("match expression must have at least one arm", span));
    }

    Ok(result_ty.unwrap_or(ResolvedType::Unit))
}

fn infer_assignment_target_type(
    env: &mut TypeEnv,
    expr: &Expr,
    ctx: Option<&SemanticContext>,
) -> KainResult<ResolvedType> {
    match expr {
        Expr::Ident(_, _) | Expr::Field { .. } | Expr::Index { .. } | Expr::Deref(_, _) => {
            infer_expr_type(env, expr, ctx)
        }
        other => Err(env.type_error(
            format!("Invalid assignment target: {:?}", other),
            other.span(),
        )),
    }
}

fn infer_array_type(
    env: &mut TypeEnv,
    values: &[Expr],
    ctx: Option<&SemanticContext>,
) -> KainResult<ResolvedType> {
    let mut element_ty = ResolvedType::Unknown;
    for value in values {
        let value_ty = infer_expr_type(env, value, ctx)?;
        element_ty = if matches!(element_ty, ResolvedType::Unknown) {
            value_ty
        } else {
            unify_types(&element_ty, &value_ty).unwrap_or(ResolvedType::Unknown)
        };
    }
    Ok(ResolvedType::Array(Box::new(element_ty), values.len()))
}

fn infer_method_call_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    receiver_ty: &ResolvedType,
    method: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    if method == "clone" {
        if args.is_empty() {
            return Ok(peel_shared_refs(receiver_ty).clone());
        }
        return Err(env.type_error("clone expects no arguments".to_string(), span));
    }
    match receiver_ty {
        ResolvedType::Ref { mutable, inner } => {
            if let ResolvedType::Array(item_ty, _) = inner.as_ref() {
                if method == "as_slice" {
                    if args.is_empty() {
                        return Ok(ResolvedType::Ref {
                            mutable: *mutable,
                            inner: Box::new(ResolvedType::Slice(Box::new(
                                item_ty.as_ref().clone(),
                            ))),
                        });
                    }
                    return Err(
                        env.type_error("Array.as_slice expects no arguments".to_string(), span)
                    );
                }
            }
            if matches!(inner.as_ref(), ResolvedType::Array(_, _)) && method == "push" && !*mutable
            {
                return Err(
                    env.type_error("Array.push requires a mutable receiver".to_string(), span)
                );
            }
            if matches!(inner.as_ref(), ResolvedType::Option(_)) && method == "take" && !*mutable {
                return Err(
                    env.type_error("Option.take requires a mutable receiver".to_string(), span)
                );
            }
            if let ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) = inner.as_ref() {
                return infer_named_method_call_type(
                    env,
                    ctx,
                    name,
                    receiver_ty,
                    method,
                    args,
                    span,
                );
            }
            infer_method_call_type(env, ctx, inner.as_ref(), method, args, span)
        }
        ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) => {
            infer_named_method_call_type(env, ctx, name, receiver_ty, method, args, span)
        }
        ResolvedType::Array(inner, _) => match method {
            "len" => Ok(ResolvedType::Int(IntSize::I64)),
            "is_empty" => {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error("Array.is_empty expects no arguments", span))
                }
            }
            "iter" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(shared_ref_type(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Array.iter expects no arguments", span))
                }
            }
            "into_iter" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error("Array.into_iter expects no arguments", span))
                }
            }
            "peekable" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error("Array.peekable expects no arguments", span))
                }
            }
            "next" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Array.next expects no arguments", span))
                }
            }
            "peek" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(shared_ref_type(
                        inner.as_ref().clone(),
                    ))))
                } else {
                    Err(env.type_error("Array.peek expects no arguments", span))
                }
            }
            "enumerate" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(ResolvedType::Tuple(vec![
                        ResolvedType::Int(IntSize::I64),
                        inner.as_ref().clone(),
                    ])))
                } else {
                    Err(env.type_error("Array.enumerate expects no arguments", span))
                }
            }
            "push" => {
                if args.len() == 1 {
                    let arg_ty = infer_expr_type_with_expected(
                        env,
                        &args[0].value,
                        ctx,
                        Some(inner.as_ref()),
                    )?;
                    ensure_type_compatible(env, inner.as_ref(), &arg_ty, span, "array push")?;
                    Ok(ResolvedType::Unit)
                } else {
                    Err(env.type_error("Array.push expects exactly one argument", span))
                }
            }
            "contains" => {
                if args.len() != 1 {
                    return Err(env.type_error("Array.contains expects exactly one argument", span));
                }
                let expected_borrowed = ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(inner.as_ref().clone()),
                };
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&expected_borrowed),
                )?;
                let stripped_arg_ty = peel_shared_refs(&arg_ty);
                if !types_compatible(inner.as_ref(), &arg_ty)
                    && !types_compatible(&expected_borrowed, &arg_ty)
                    && !types_compatible(inner.as_ref(), stripped_arg_ty)
                {
                    return Err(env.type_error(
                        format!(
                            "array contains expected {} or {}, found {}",
                            describe_type(inner.as_ref()),
                            describe_type(&expected_borrowed),
                            describe_type(&arg_ty)
                        ),
                        args[0].span,
                    ));
                }
                Ok(ResolvedType::Bool)
            }
            "binary_search" => {
                if args.len() != 1 {
                    return Err(
                        env.type_error("Array.binary_search expects exactly one argument", span)
                    );
                }
                let expected_borrowed = ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(inner.as_ref().clone()),
                };
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&expected_borrowed),
                )?;
                let stripped_arg_ty = peel_shared_refs(&arg_ty);
                if !types_compatible(inner.as_ref(), &arg_ty)
                    && !types_compatible(&expected_borrowed, &arg_ty)
                    && !types_compatible(inner.as_ref(), stripped_arg_ty)
                {
                    return Err(env.type_error(
                        format!(
                            "array binary_search expected {} or {}, found {}",
                            describe_type(inner.as_ref()),
                            describe_type(&expected_borrowed),
                            describe_type(&arg_ty)
                        ),
                        args[0].span,
                    ));
                }
                Ok(ResolvedType::Result(
                    Box::new(ResolvedType::Int(IntSize::I64)),
                    Box::new(ResolvedType::Int(IntSize::I64)),
                ))
            }
            "map" => {
                infer_builtin_transform_method(env, ctx, inner.as_ref(), args, span, |mapped| {
                    dynamic_array_type(mapped)
                })
            }
            "filter" => {
                let _ = infer_builtin_predicate_method(
                    env,
                    ctx,
                    inner.as_ref(),
                    args,
                    span,
                    "Array.filter",
                )?;
                Ok(dynamic_array_type(inner.as_ref().clone()))
            }
            "includes" => {
                if args.len() != 1 {
                    return Err(env.type_error("Array.includes expects exactly one argument", span));
                }
                let _ = infer_expr_type(env, &args[0].value, ctx)?;
                Ok(ResolvedType::Bool)
            }
            "reverse" | "toReversed" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error(format!("Array.{} expects no arguments", method), span))
                }
            }
            "sort" | "toSorted" | "slice" | "splice" | "toSpliced" | "concat" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(dynamic_array_type(inner.as_ref().clone()))
            }
            "unshift" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "shift" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Array.shift expects no arguments", span))
                }
            }
            "indexOf" | "lastIndexOf" | "findIndex" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "some" | "every" => {
                let _ = infer_builtin_predicate_method(
                    env,
                    ctx,
                    inner.as_ref(),
                    args,
                    span,
                    &format!("Array.{}", method),
                )?;
                Ok(ResolvedType::Bool)
            }
            "forEach" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unit)
            }
            "flat" | "flatMap" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(dynamic_array_type(ResolvedType::Unknown))
            }
            "reduce" | "reduceRight" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
            "any" => {
                infer_builtin_predicate_method(env, ctx, inner.as_ref(), args, span, "Array.any")
            }
            "all" => {
                infer_builtin_predicate_method(env, ctx, inner.as_ref(), args, span, "Array.all")
            }
            "find" => infer_builtin_find_method(env, ctx, inner.as_ref(), args, span, "Array.find"),
            "find_map" => infer_builtin_find_map_method(
                env,
                ctx,
                inner.as_ref(),
                args,
                span,
                "Array.find_map",
            ),
            "collect" => {
                infer_builtin_collect_method(inner.as_ref(), args, span, env, "Array.collect")
            }
            "join" => infer_builtin_join_method(env, ctx, inner.as_ref(), args, span, "Array.join"),
            "first" => infer_builtin_first_method(inner.as_ref(), args, span, env, "Array.first"),
            "last" => infer_builtin_first_method(inner.as_ref(), args, span, env, "Array.last"),
            "pop" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Array.pop expects no arguments", span))
                }
            }
            "cloned" | "copied" => {
                infer_builtin_clone_adapter(inner.as_ref(), args, span, env, method)
            }
            "zip" => infer_builtin_zip_method(env, ctx, inner.as_ref(), args, span, "Array.zip"),
            "sum" => infer_builtin_sum_method(inner.as_ref(), args, span, env, "Array.sum"),
            "max" => infer_builtin_max_method(inner.as_ref(), args, span, env, "Array.max"),
            "as_slice" => {
                if args.is_empty() {
                    Ok(ResolvedType::Slice(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Array.as_slice expects no arguments", span))
                }
            }
            "get" => infer_index_lookup_method(env, ctx, inner.as_ref(), args, span, "Array.get"),
            _ => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
        },
        ResolvedType::Slice(inner) => match method {
            "len" => Ok(ResolvedType::Int(IntSize::I64)),
            "is_empty" => {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error("Slice.is_empty expects no arguments", span))
                }
            }
            "iter" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(shared_ref_type(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Slice.iter expects no arguments", span))
                }
            }
            "into_iter" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error("Slice.into_iter expects no arguments", span))
                }
            }
            "peekable" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error("Slice.peekable expects no arguments", span))
                }
            }
            "next" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error("Slice.next expects no arguments", span))
                }
            }
            "peek" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(shared_ref_type(
                        inner.as_ref().clone(),
                    ))))
                } else {
                    Err(env.type_error("Slice.peek expects no arguments", span))
                }
            }
            "enumerate" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(ResolvedType::Tuple(vec![
                        ResolvedType::Int(IntSize::I64),
                        inner.as_ref().clone(),
                    ])))
                } else {
                    Err(env.type_error("Slice.enumerate expects no arguments", span))
                }
            }
            "contains" => {
                if args.len() != 1 {
                    return Err(env.type_error("Slice.contains expects exactly one argument", span));
                }
                let expected_borrowed = ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(inner.as_ref().clone()),
                };
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&expected_borrowed),
                )?;
                let stripped_arg_ty = peel_shared_refs(&arg_ty);
                if !types_compatible(inner.as_ref(), &arg_ty)
                    && !types_compatible(&expected_borrowed, &arg_ty)
                    && !types_compatible(inner.as_ref(), stripped_arg_ty)
                {
                    return Err(env.type_error(
                        format!(
                            "slice contains expected {} or {}, found {}",
                            describe_type(inner.as_ref()),
                            describe_type(&expected_borrowed),
                            describe_type(&arg_ty)
                        ),
                        args[0].span,
                    ));
                }
                Ok(ResolvedType::Bool)
            }
            "binary_search" => {
                if args.len() != 1 {
                    return Err(
                        env.type_error("Slice.binary_search expects exactly one argument", span)
                    );
                }
                let expected_borrowed = ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(inner.as_ref().clone()),
                };
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&expected_borrowed),
                )?;
                let stripped_arg_ty = peel_shared_refs(&arg_ty);
                if !types_compatible(inner.as_ref(), &arg_ty)
                    && !types_compatible(&expected_borrowed, &arg_ty)
                    && !types_compatible(inner.as_ref(), stripped_arg_ty)
                {
                    return Err(env.type_error(
                        format!(
                            "slice binary_search expected {} or {}, found {}",
                            describe_type(inner.as_ref()),
                            describe_type(&expected_borrowed),
                            describe_type(&arg_ty)
                        ),
                        args[0].span,
                    ));
                }
                Ok(ResolvedType::Result(
                    Box::new(ResolvedType::Int(IntSize::I64)),
                    Box::new(ResolvedType::Int(IntSize::I64)),
                ))
            }
            "map" => {
                infer_builtin_transform_method(env, ctx, inner.as_ref(), args, span, |mapped| {
                    dynamic_array_type(mapped)
                })
            }
            "filter" => {
                let _ = infer_builtin_predicate_method(
                    env,
                    ctx,
                    inner.as_ref(),
                    args,
                    span,
                    "Slice.filter",
                )?;
                Ok(dynamic_array_type(inner.as_ref().clone()))
            }
            "includes" => {
                if args.len() != 1 {
                    return Err(env.type_error("Slice.includes expects exactly one argument", span));
                }
                let _ = infer_expr_type(env, &args[0].value, ctx)?;
                Ok(ResolvedType::Bool)
            }
            "reverse" | "toReversed" => {
                if args.is_empty() {
                    Ok(dynamic_array_type(inner.as_ref().clone()))
                } else {
                    Err(env.type_error(format!("Slice.{} expects no arguments", method), span))
                }
            }
            "sort" | "toSorted" | "slice" | "splice" | "toSpliced" | "concat" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(dynamic_array_type(inner.as_ref().clone()))
            }
            "push" | "unshift" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "shift" | "pop" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(inner.as_ref().clone())))
                } else {
                    Err(env.type_error(format!("Slice.{} expects no arguments", method), span))
                }
            }
            "indexOf" | "lastIndexOf" | "findIndex" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "some" | "every" => {
                let _ = infer_builtin_predicate_method(
                    env,
                    ctx,
                    inner.as_ref(),
                    args,
                    span,
                    &format!("Slice.{}", method),
                )?;
                Ok(ResolvedType::Bool)
            }
            "forEach" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unit)
            }
            "flat" | "flatMap" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(dynamic_array_type(ResolvedType::Unknown))
            }
            "reduce" | "reduceRight" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
            "any" => {
                infer_builtin_predicate_method(env, ctx, inner.as_ref(), args, span, "Slice.any")
            }
            "all" => {
                infer_builtin_predicate_method(env, ctx, inner.as_ref(), args, span, "Slice.all")
            }
            "find" => infer_builtin_find_method(env, ctx, inner.as_ref(), args, span, "Slice.find"),
            "find_map" => infer_builtin_find_map_method(
                env,
                ctx,
                inner.as_ref(),
                args,
                span,
                "Slice.find_map",
            ),
            "collect" => {
                infer_builtin_collect_method(inner.as_ref(), args, span, env, "Slice.collect")
            }
            "join" => infer_builtin_join_method(env, ctx, inner.as_ref(), args, span, "Slice.join"),
            "first" => infer_builtin_first_method(inner.as_ref(), args, span, env, "Slice.first"),
            "last" => infer_builtin_first_method(inner.as_ref(), args, span, env, "Slice.last"),
            "cloned" | "copied" => {
                infer_builtin_clone_adapter(inner.as_ref(), args, span, env, method)
            }
            "zip" => infer_builtin_zip_method(env, ctx, inner.as_ref(), args, span, "Slice.zip"),
            "sum" => infer_builtin_sum_method(inner.as_ref(), args, span, env, "Slice.sum"),
            "max" => infer_builtin_max_method(inner.as_ref(), args, span, env, "Slice.max"),
            "get" => infer_index_lookup_method(env, ctx, inner.as_ref(), args, span, "Slice.get"),
            _ => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
        },
        ResolvedType::Option(inner) => match method {
            "map" => {
                infer_builtin_transform_method(env, ctx, inner.as_ref(), args, span, |mapped| {
                    ResolvedType::Option(Box::new(mapped))
                })
            }
            "take" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(inner.clone()))
                } else {
                    Err(env.type_error("Option.take expects no arguments", span))
                }
            }
            "unwrap_or" => {
                if args.len() != 1 {
                    return Err(
                        env.type_error("Option.unwrap_or expects exactly one argument", span)
                    );
                }
                let default_ty =
                    infer_expr_type_with_expected(env, &args[0].value, ctx, Some(inner.as_ref()))?;
                ensure_type_compatible(
                    env,
                    inner.as_ref(),
                    &default_ty,
                    args[0].span,
                    "option default",
                )?;
                Ok(inner.as_ref().clone())
            }
            "and_then" => infer_builtin_option_chain_method(env, ctx, inner.as_ref(), args, span),
            "filter" => infer_builtin_option_filter_method(env, ctx, inner.as_ref(), args, span),
            "or_else" => infer_builtin_nullary_callback_method(
                env,
                ctx,
                args,
                span,
                "Option.or_else",
                |mapped| ensure_option_return_type(mapped, inner.as_ref()),
            ),
            "cloned" | "copied" => {
                infer_builtin_option_clone_adapter(inner.as_ref(), args, span, env, method)
            }
            "or" | "or_" => {
                if args.len() != 1 {
                    return Err(env.type_error("Option.or expects exactly one argument", span));
                }
                let fallback_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&ResolvedType::Option(inner.clone())),
                )?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::Option(inner.clone()),
                    &fallback_ty,
                    args[0].span,
                    "option fallback",
                )?;
                Ok(ResolvedType::Option(inner.clone()))
            }
            "unwrap" | "expect" => {
                if method == "expect" && args.len() != 1 {
                    return Err(env.type_error("Option.expect expects exactly one argument", span));
                }
                if method == "unwrap" && !args.is_empty() {
                    return Err(env.type_error("Option.unwrap expects no arguments", span));
                }
                if method == "expect" {
                    let message_ty = infer_expr_type_with_expected(
                        env,
                        &args[0].value,
                        ctx,
                        Some(&ResolvedType::String),
                    )?;
                    ensure_type_compatible(
                        env,
                        &ResolvedType::String,
                        &message_ty,
                        args[0].span,
                        "Option.expect message",
                    )?;
                }
                Ok(inner.as_ref().clone())
            }
            "unwrap_or_else" => infer_builtin_nullary_callback_method(
                env,
                ctx,
                args,
                span,
                "Option.unwrap_or_else",
                |mapped| ensure_type_compatible_option(mapped, inner.as_ref()).map(|ret| ret),
            ),
            "is_some" | "is_none" => {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error(format!("Option.{method} expects no arguments"), span))
                }
            }
            "is_some_and" => infer_builtin_predicate_method(
                env,
                ctx,
                inner.as_ref(),
                args,
                span,
                "Option.is_some_and",
            ),
            "ok_or_else" => infer_builtin_nullary_callback_method(
                env,
                ctx,
                args,
                span,
                "Option.ok_or_else",
                |mapped| {
                    Ok(ResolvedType::Result(
                        Box::new(inner.as_ref().clone()),
                        Box::new(mapped),
                    ))
                },
            ),
            _ => Ok(ResolvedType::Unknown),
        },
        ResolvedType::Result(ok, err) => match method {
            "map" => infer_builtin_transform_method(env, ctx, ok.as_ref(), args, span, |mapped| {
                ResolvedType::Result(Box::new(mapped), Box::new(err.as_ref().clone()))
            }),
            "map_err" => {
                infer_builtin_transform_method(env, ctx, err.as_ref(), args, span, |mapped| {
                    ResolvedType::Result(Box::new(ok.as_ref().clone()), Box::new(mapped))
                })
            }
            "unwrap_or" => {
                if args.len() != 1 {
                    return Err(
                        env.type_error("Result.unwrap_or expects exactly one argument", span)
                    );
                }
                let default_ty =
                    infer_expr_type_with_expected(env, &args[0].value, ctx, Some(ok.as_ref()))?;
                ensure_type_compatible(
                    env,
                    ok.as_ref(),
                    &default_ty,
                    args[0].span,
                    "result default",
                )?;
                Ok(ok.as_ref().clone())
            }
            "or_else" => infer_builtin_unary_result_callback(
                env,
                ctx,
                err.as_ref(),
                args,
                span,
                "Result.or_else",
                |mapped| ensure_result_type(mapped, ok.as_ref()),
            ),
            "unwrap_or_else" => infer_builtin_unary_result_callback(
                env,
                ctx,
                err.as_ref(),
                args,
                span,
                "Result.unwrap_or_else",
                |mapped| ensure_type_compatible_option(mapped, ok.as_ref()),
            ),
            "unwrap" | "expect" => {
                if method == "expect" && args.len() != 1 {
                    return Err(env.type_error("Result.expect expects exactly one argument", span));
                }
                if method == "unwrap" && !args.is_empty() {
                    return Err(env.type_error("Result.unwrap expects no arguments", span));
                }
                if method == "expect" {
                    let message_ty = infer_expr_type_with_expected(
                        env,
                        &args[0].value,
                        ctx,
                        Some(&ResolvedType::String),
                    )?;
                    ensure_type_compatible(
                        env,
                        &ResolvedType::String,
                        &message_ty,
                        args[0].span,
                        "Result.expect message",
                    )?;
                }
                Ok(ok.as_ref().clone())
            }
            "ok" => {
                if args.is_empty() {
                    Ok(ResolvedType::Option(Box::new(ok.as_ref().clone())))
                } else {
                    Err(env.type_error("Result.ok expects no arguments", span))
                }
            }
            "is_ok" | "is_err" => {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error(format!("Result.{method} expects no arguments"), span))
                }
            }
            _ => Err(env.type_error(format!("Unknown method '{}' on Result", method), span)),
        },
        ResolvedType::Int(_)
        | ResolvedType::Float(_)
        | ResolvedType::Bool
        | ResolvedType::Char
        | ResolvedType::String => match method {
            "len" if matches!(receiver_ty, ResolvedType::String) => {
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "is_empty" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error("String.is_empty expects no arguments", span))
                }
            }
            "chars" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(dynamic_array_type(ResolvedType::String))
                } else {
                    Err(env.type_error("String.chars expects no arguments", span))
                }
            }
            "char_indices" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(dynamic_array_type(ResolvedType::Tuple(vec![
                        ResolvedType::Int(IntSize::I64),
                        ResolvedType::String,
                    ])))
                } else {
                    Err(env.type_error("String.char_indices expects no arguments", span))
                }
            }
            "as_str" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(shared_ref_type(ResolvedType::String))
                } else {
                    Err(env.type_error("String.as_str expects no arguments", span))
                }
            }
            "trim" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(shared_ref_type(ResolvedType::String))
                } else {
                    Err(env.type_error("String.trim expects no arguments", span))
                }
            }
            "to_string_lossy" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(ResolvedType::String)
                } else {
                    Err(env.type_error("String.to_string_lossy expects no arguments", span))
                }
            }
            "starts_with" | "eq_ignore_ascii_case"
                if matches!(receiver_ty, ResolvedType::String) =>
            {
                infer_builtin_string_arg_predicate(
                    env,
                    ctx,
                    args,
                    span,
                    if method == "starts_with" {
                        "String.starts_with"
                    } else {
                        "String.eq_ignore_ascii_case"
                    },
                )
            }
            "startsWith" | "endsWith" | "includes"
                if matches!(receiver_ty, ResolvedType::String) =>
            {
                infer_builtin_string_arg_predicate(
                    env,
                    ctx,
                    args,
                    span,
                    &format!("String.{}", method),
                )
            }
            "find" if matches!(receiver_ty, ResolvedType::String) => {
                infer_builtin_string_arg_method(
                    env,
                    ctx,
                    args,
                    span,
                    "String.find",
                    ResolvedType::Option(Box::new(ResolvedType::Int(IntSize::I64))),
                )
            }
            "indexOf" if matches!(receiver_ty, ResolvedType::String) => {
                infer_builtin_string_arg_method(
                    env,
                    ctx,
                    args,
                    span,
                    "String.indexOf",
                    ResolvedType::Int(IntSize::I64),
                )
            }
            "split" if matches!(receiver_ty, ResolvedType::String) => {
                infer_builtin_string_arg_method(
                    env,
                    ctx,
                    args,
                    span,
                    "String.split",
                    dynamic_array_type(shared_ref_type(ResolvedType::String)),
                )
            }
            "strip_prefix" if matches!(receiver_ty, ResolvedType::String) => {
                infer_builtin_string_arg_method(
                    env,
                    ctx,
                    args,
                    span,
                    "String.strip_prefix",
                    ResolvedType::Option(Box::new(shared_ref_type(ResolvedType::String))),
                )
            }
            "slice" | "substring" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let arg_ty = infer_expr_type_with_expected(
                        env,
                        &arg.value,
                        ctx,
                        Some(&ResolvedType::Int(IntSize::I64)),
                    )?;
                    ensure_type_compatible(
                        env,
                        &ResolvedType::Int(IntSize::I64),
                        &arg_ty,
                        arg.span,
                        &format!("String.{}", method),
                    )?;
                }
                Ok(ResolvedType::String)
            }
            "push_str" if matches!(receiver_ty, ResolvedType::String) => {
                if args.len() != 1 {
                    return Err(
                        env.type_error("String.push_str expects exactly one argument", span)
                    );
                }
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&ResolvedType::String),
                )?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::String,
                    &arg_ty,
                    args[0].span,
                    "String.push_str",
                )?;
                Ok(ResolvedType::Unit)
            }
            "push" if matches!(receiver_ty, ResolvedType::String) => {
                if args.len() != 1 {
                    return Err(env.type_error("String.push expects exactly one argument", span));
                }
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&ResolvedType::Char),
                )?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::Char,
                    &arg_ty,
                    args[0].span,
                    "String.push",
                )?;
                Ok(ResolvedType::Unit)
            }
            "repeat" if matches!(receiver_ty, ResolvedType::String) => {
                if args.len() != 1 {
                    return Err(env.type_error("String.repeat expects exactly one argument", span));
                }
                let arg_ty = infer_expr_type_with_expected(
                    env,
                    &args[0].value,
                    ctx,
                    Some(&ResolvedType::Int(IntSize::I64)),
                )?;
                ensure_type_compatible(
                    env,
                    &ResolvedType::Int(IntSize::I64),
                    &arg_ty,
                    args[0].span,
                    "String.repeat",
                )?;
                Ok(ResolvedType::String)
            }
            "to_ascii_lowercase" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(ResolvedType::String)
                } else {
                    Err(env.type_error("String.to_ascii_lowercase expects no arguments", span))
                }
            }
            "toLowerCase" | "toUpperCase" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(ResolvedType::String)
                } else {
                    Err(env.type_error(format!("String.{} expects no arguments", method), span))
                }
            }
            "padStart" | "padEnd" | "replace" | "replaceAll" | "trimStart" | "trimEnd"
                if matches!(receiver_ty, ResolvedType::String) =>
            {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::String)
            }
            "test_" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Bool)
            }
            "match_" | "matchAll" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
            "search" | "localeCompare" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "charAt" | "at" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let _ = infer_expr_type_with_expected(
                        env,
                        &arg.value,
                        ctx,
                        Some(&ResolvedType::Int(IntSize::I64)),
                    )?;
                }
                Ok(ResolvedType::String)
            }
            "charCodeAt" | "codePointAt" if matches!(receiver_ty, ResolvedType::String) => {
                for arg in args {
                    let _ = infer_expr_type_with_expected(
                        env,
                        &arg.value,
                        ctx,
                        Some(&ResolvedType::Int(IntSize::I64)),
                    )?;
                }
                Ok(ResolvedType::Int(IntSize::I64))
            }
            "parse" if matches!(receiver_ty, ResolvedType::String) => {
                if args.is_empty() {
                    Ok(ResolvedType::Result(
                        Box::new(ResolvedType::Unknown),
                        Box::new(ResolvedType::String),
                    ))
                } else {
                    Err(env.type_error("String.parse expects no arguments", span))
                }
            }
            "toFixed" | "toPrecision" | "toExponential"
                if matches!(receiver_ty, ResolvedType::Int(_) | ResolvedType::Float(_)) =>
            {
                for arg in args {
                    let arg_ty = infer_expr_type_with_expected(
                        env,
                        &arg.value,
                        ctx,
                        Some(&ResolvedType::Int(IntSize::I64)),
                    )?;
                    if !types_compatible(&ResolvedType::Int(IntSize::I64), &arg_ty)
                        && !is_none_placeholder_type(&arg_ty)
                    {
                        return Err(env.type_error(
                            format!(
                                "Number.{} expected optional integer precision, found {}",
                                method,
                                describe_type(&arg_ty)
                            ),
                            arg.span,
                        ));
                    }
                }
                Ok(ResolvedType::String)
            }
            "min" | "max"
                if matches!(receiver_ty, ResolvedType::Int(_) | ResolvedType::Float(_)) =>
            {
                infer_builtin_same_type_numeric_method(env, ctx, receiver_ty, args, span, method)
            }
            "div_ceil" if matches!(receiver_ty, ResolvedType::Int(_)) => {
                infer_builtin_same_type_numeric_method(
                    env,
                    ctx,
                    receiver_ty,
                    args,
                    span,
                    "div_ceil",
                )
            }
            "saturating_add" | "saturating_sub" | "saturating_mul" | "wrapping_add"
            | "wrapping_sub" | "wrapping_mul" | "wrapping_shl" | "wrapping_shr"
                if matches!(receiver_ty, ResolvedType::Int(_)) =>
            {
                infer_builtin_same_type_numeric_method(env, ctx, receiver_ty, args, span, method)
            }
            "is_uppercase"
            | "is_ascii_uppercase"
            | "is_ascii_alphabetic"
            | "is_ascii_alphanumeric"
                if matches!(receiver_ty, ResolvedType::Char | ResolvedType::String) =>
            {
                if args.is_empty() {
                    Ok(ResolvedType::Bool)
                } else {
                    Err(env.type_error(
                        format!(
                            "{}.{} expects no arguments",
                            describe_type(receiver_ty),
                            method
                        ),
                        span,
                    ))
                }
            }
            "to_string" => {
                if args.is_empty() {
                    Ok(ResolvedType::String)
                } else {
                    Err(env.type_error(
                        format!(
                            "{}.to_string expects no arguments",
                            describe_type(receiver_ty)
                        ),
                        span,
                    ))
                }
            }
            "toString" => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::String)
            }
            _ => {
                for arg in args {
                    let _ = infer_expr_type(env, &arg.value, ctx)?;
                }
                Ok(ResolvedType::Unknown)
            }
        },
        ResolvedType::Unknown | ResolvedType::Generic(_) => Ok(ResolvedType::Unknown),
        ResolvedType::Function { params, ret, .. }
            if params.is_empty()
                && matches!(ret.as_ref(), ResolvedType::Unknown | ResolvedType::Never) =>
        {
            Ok(ResolvedType::Unknown)
        }
        other => Err(env.type_error(
            format!(
                "Method call requires a receiver with known methods, found {}",
                describe_type(other)
            ),
            span,
        )),
    }
}

fn infer_named_method_call_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    type_name: &str,
    receiver_ty: &ResolvedType,
    method: &str,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    if let Some(method_ty) = env.lookup_method(type_name, method).cloned() {
        if let ResolvedType::Function {
            params,
            ret,
            effects,
        } = method_ty
        {
            let start_index = usize::from(
                params
                    .first()
                    .map(|ty| method_receiver_param_matches(ty, receiver_ty))
                    .unwrap_or(false),
            );
            let params = &params[start_index..];
            for (param_ty, arg) in params.iter().zip(args.iter()) {
                let arg_ty = infer_expr_type_with_expected(env, &arg.value, ctx, Some(param_ty))?;
                ensure_type_compatible(env, param_ty, &arg_ty, span, "method argument")?;
            }
            for arg in args.iter().skip(params.len()) {
                let _ = infer_expr_type(env, &arg.value, ctx)?;
            }
            if let Some(ctx) = ctx {
                check_effect_call(&ctx.effects, &effects, &ctx.function_name, method, span)
                    .map_err(|error| env.attach_effect_source(error, span))?;
            }
            Ok(*ret)
        } else {
            Ok(ResolvedType::Unknown)
        }
    } else if method == "to_string" {
        if args.is_empty() {
            Ok(ResolvedType::String)
        } else {
            Err(env.type_error(format!("{type_name}.to_string expects no arguments"), span))
        }
    } else {
        for arg in args {
            let _ = infer_expr_type(env, &arg.value, ctx)?;
        }
        Ok(ResolvedType::Unknown)
    }
}

fn method_receiver_param_matches(param_ty: &ResolvedType, receiver_ty: &ResolvedType) -> bool {
    if types_compatible(param_ty, receiver_ty) {
        return true;
    }
    let owned_receiver = peel_receiver_refs(receiver_ty).clone();
    types_compatible(param_ty, &owned_receiver)
        || types_compatible(param_ty, &shared_ref_type(owned_receiver))
}

fn infer_index_lookup_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    inner_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let index_ty = infer_expr_type_with_expected(
        env,
        &args[0].value,
        ctx,
        Some(&ResolvedType::Int(IntSize::I64)),
    )?;
    ensure_type_compatible(
        env,
        &ResolvedType::Int(IntSize::I64),
        &index_ty,
        args[0].span,
        method_name,
    )?;
    Ok(ResolvedType::Option(Box::new(ResolvedType::Ref {
        mutable: false,
        inner: Box::new(inner_ty.clone()),
    })))
}

fn dynamic_array_type(item_ty: ResolvedType) -> ResolvedType {
    ResolvedType::Array(Box::new(item_ty), 0)
}

fn shared_ref_type(inner: ResolvedType) -> ResolvedType {
    ResolvedType::Ref {
        mutable: false,
        inner: Box::new(inner),
    }
}

fn infer_builtin_predicate_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    let predicate_ty =
        infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, method_name)?;
    ensure_type_compatible(
        env,
        &ResolvedType::Bool,
        &predicate_ty,
        args[0].span,
        method_name,
    )?;
    Ok(ResolvedType::Bool)
}

fn infer_builtin_find_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    let predicate_ty =
        infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, method_name)?;
    ensure_type_compatible(
        env,
        &ResolvedType::Bool,
        &predicate_ty,
        args[0].span,
        method_name,
    )?;
    Ok(ResolvedType::Option(Box::new(input_ty.clone())))
}

fn infer_builtin_find_map_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    let mapped_ty =
        infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, method_name)?;
    match mapped_ty {
        ResolvedType::Option(inner) => Ok(ResolvedType::Option(inner)),
        ResolvedType::Unknown => Ok(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
        other => Err(env.type_error(
            format!(
                "{method_name} expects a callback returning Option<T>, found {}",
                describe_type(&other)
            ),
            args[0].span,
        )),
    }
}

fn infer_builtin_collect_method(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    match item_ty {
        ResolvedType::Result(ok, err) => Ok(ResolvedType::Result(
            Box::new(dynamic_array_type(ok.as_ref().clone())),
            err.clone(),
        )),
        ResolvedType::Option(inner) => Ok(ResolvedType::Option(Box::new(dynamic_array_type(
            inner.as_ref().clone(),
        )))),
        ResolvedType::Tuple(items) if items.len() == 2 => Ok(selfhost_map_type()),
        other => Ok(dynamic_array_type(other.clone())),
    }
}

fn infer_builtin_join_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    let _ = item_ty;
    if args.is_empty() {
        return Ok(ResolvedType::String);
    }
    infer_builtin_string_arg_method(env, ctx, args, span, method_name, ResolvedType::String)
}

fn infer_builtin_first_method(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    let first_ty = match item_ty {
        ResolvedType::Ref { .. } => item_ty.clone(),
        other => shared_ref_type(other.clone()),
    };
    Ok(ResolvedType::Option(Box::new(first_ty)))
}

fn infer_builtin_clone_adapter(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    let cloned_item = match item_ty {
        ResolvedType::Ref {
            mutable: false,
            inner,
        } => inner.as_ref().clone(),
        other => other.clone(),
    };
    Ok(dynamic_array_type(cloned_item))
}

fn infer_builtin_option_clone_adapter(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    let cloned_item = match item_ty {
        ResolvedType::Ref {
            mutable: false,
            inner,
        } => inner.as_ref().clone(),
        other => other.clone(),
    };
    Ok(ResolvedType::Option(Box::new(cloned_item)))
}

fn infer_builtin_zip_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let other_ty = infer_expr_type(env, &args[0].value, ctx)?;
    let other_item_ty = match other_ty {
        ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => *inner,
        ResolvedType::Ref { inner, .. } => match *inner {
            ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => *inner,
            other => {
                return Err(env.type_error(
                    format!(
                        "{method_name} expects an array or slice argument, found {}",
                        describe_type(&other)
                    ),
                    args[0].span,
                ))
            }
        },
        other => {
            return Err(env.type_error(
                format!(
                    "{method_name} expects an array or slice argument, found {}",
                    describe_type(&other)
                ),
                args[0].span,
            ))
        }
    };
    Ok(dynamic_array_type(ResolvedType::Tuple(vec![
        item_ty.clone(),
        other_item_ty,
    ])))
}

fn infer_builtin_sum_method(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    let numeric_ty = peel_shared_refs(item_ty);
    if matches!(numeric_ty, ResolvedType::Int(_) | ResolvedType::Float(_)) {
        Ok(numeric_ty.clone())
    } else {
        Err(env.type_error(
            format!(
                "{method_name} requires numeric items, found {}",
                describe_type(item_ty)
            ),
            span,
        ))
    }
}

fn infer_builtin_max_method(
    item_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    env: &mut TypeEnv,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if !args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects no arguments"), span));
    }
    Ok(ResolvedType::Option(Box::new(item_ty.clone())))
}

fn infer_builtin_unary_callback_return(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let expected_callback = ResolvedType::Function {
        params: vec![input_ty.clone()],
        ret: Box::new(ResolvedType::Unknown),
        effects: EffectSet::new(),
    };
    let callback_ty =
        infer_expr_type_with_expected(env, &args[0].value, ctx, Some(&expected_callback))?;
    match callback_ty {
        ResolvedType::Function { params, ret, .. } if params.len() == 1 => {
            ensure_type_compatible(env, input_ty, &params[0], args[0].span, method_name)?;
            Ok(*ret)
        }
        other if is_none_placeholder_type(&other) => Ok(ResolvedType::Unknown),
        ResolvedType::Unknown => Ok(ResolvedType::Unknown),
        other => Err(env.type_error(
            format!(
                "{method_name} expects a unary function, found {}",
                describe_type(&other)
            ),
            args[0].span,
        )),
    }
}

fn infer_builtin_nullary_callback_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
    method_name: &str,
    wrap_output: impl FnOnce(ResolvedType) -> KainResult<ResolvedType>,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let expected_callback = ResolvedType::Function {
        params: vec![],
        ret: Box::new(ResolvedType::Unknown),
        effects: EffectSet::new(),
    };
    let callback_ty =
        infer_expr_type_with_expected(env, &args[0].value, ctx, Some(&expected_callback))?;
    match callback_ty {
        ResolvedType::Function { params, ret, .. } if params.is_empty() => wrap_output(*ret),
        ResolvedType::Unknown => wrap_output(ResolvedType::Unknown),
        other => Err(env.type_error(
            format!(
                "{method_name} expects a nullary function, found {}",
                describe_type(&other)
            ),
            args[0].span,
        )),
    }
}

fn infer_builtin_unary_result_callback(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
    wrap_output: impl FnOnce(ResolvedType) -> KainResult<ResolvedType>,
) -> KainResult<ResolvedType> {
    let mapped = infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, method_name)?;
    wrap_output(mapped)
}

fn ensure_type_compatible_option(
    mapped: ResolvedType,
    expected: &ResolvedType,
) -> KainResult<ResolvedType> {
    if matches!(mapped, ResolvedType::Unknown) || types_compatible(expected, &mapped) {
        Ok(if matches!(mapped, ResolvedType::Unknown) {
            expected.clone()
        } else {
            mapped
        })
    } else {
        Ok(ResolvedType::Unknown)
    }
}

fn ensure_option_return_type(
    mapped: ResolvedType,
    expected: &ResolvedType,
) -> KainResult<ResolvedType> {
    match mapped {
        ResolvedType::Option(inner) => {
            if matches!(inner.as_ref(), ResolvedType::Unknown)
                || types_compatible(expected, inner.as_ref())
            {
                Ok(ResolvedType::Option(Box::new(
                    if matches!(inner.as_ref(), ResolvedType::Unknown) {
                        expected.clone()
                    } else {
                        inner.as_ref().clone()
                    },
                )))
            } else {
                Ok(ResolvedType::Option(Box::new(ResolvedType::Unknown)))
            }
        }
        ResolvedType::Unknown => Ok(ResolvedType::Option(Box::new(expected.clone()))),
        other => Ok(ResolvedType::Option(Box::new(other))),
    }
}

fn ensure_result_type(mapped: ResolvedType, ok_ty: &ResolvedType) -> KainResult<ResolvedType> {
    match mapped {
        ResolvedType::Result(ok, err) => {
            if types_compatible(ok_ty, ok.as_ref()) {
                Ok(ResolvedType::Result(Box::new(ok_ty.clone()), err))
            } else {
                Ok(ResolvedType::Result(Box::new(ResolvedType::Unknown), err))
            }
        }
        ResolvedType::Unknown => Ok(ResolvedType::Result(
            Box::new(ok_ty.clone()),
            Box::new(ResolvedType::Unknown),
        )),
        other => Ok(other),
    }
}

fn infer_builtin_string_arg_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
    method_name: &str,
    return_ty: ResolvedType,
) -> KainResult<ResolvedType> {
    if args.is_empty() {
        return Err(env.type_error(format!("{method_name} expects at least one argument"), span));
    }
    let expected_borrowed = shared_ref_type(ResolvedType::String);
    let arg_ty = infer_expr_type_with_expected(env, &args[0].value, ctx, Some(&expected_borrowed))?;
    if !types_compatible(&ResolvedType::String, &arg_ty)
        && !types_compatible(&expected_borrowed, &arg_ty)
        && !types_compatible(&ResolvedType::String, peel_shared_refs(&arg_ty))
        && !is_none_placeholder_type(&arg_ty)
    {
        return Err(env.type_error(
            format!(
                "{method_name} expected String or &String, found {}",
                describe_type(&arg_ty)
            ),
            args[0].span,
        ));
    }
    for arg in args.iter().skip(1) {
        let _ = infer_expr_type(env, &arg.value, ctx)?;
    }
    Ok(return_ty)
}

fn infer_builtin_string_arg_predicate(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    infer_builtin_string_arg_method(env, ctx, args, span, method_name, ResolvedType::Bool)
}

fn infer_builtin_same_type_numeric_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    receiver_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    method_name: &str,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let arg_ty = infer_expr_type_with_expected(env, &args[0].value, ctx, Some(receiver_ty))?;
    ensure_type_compatible(env, receiver_ty, &arg_ty, args[0].span, method_name)?;
    Ok(receiver_ty.clone())
}

fn peel_shared_refs<'a>(ty: &'a ResolvedType) -> &'a ResolvedType {
    match ty {
        ResolvedType::Ref {
            mutable: false,
            inner,
        } => peel_shared_refs(inner.as_ref()),
        other => other,
    }
}

fn peel_receiver_refs<'a>(ty: &'a ResolvedType) -> &'a ResolvedType {
    match ty {
        ResolvedType::Ref { inner, .. } => peel_receiver_refs(inner.as_ref()),
        other => other,
    }
}

fn infer_builtin_transform_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
    wrap_output: impl FnOnce(ResolvedType) -> ResolvedType,
) -> KainResult<ResolvedType> {
    if args.len() != 1 {
        return Err(env.type_error("transform method expects exactly one argument", span));
    }
    let expected_mapper = ResolvedType::Function {
        params: vec![input_ty.clone()],
        ret: Box::new(ResolvedType::Unknown),
        effects: EffectSet::new(),
    };
    let mapper_ty =
        infer_expr_type_with_expected(env, &args[0].value, ctx, Some(&expected_mapper))?;
    match mapper_ty {
        ResolvedType::Function { params, ret, .. } if params.len() == 1 => {
            ensure_type_compatible(
                env,
                input_ty,
                &params[0],
                args[0].span,
                "transform callback input",
            )?;
            Ok(wrap_output(*ret))
        }
        other if is_none_placeholder_type(&other) => Ok(wrap_output(ResolvedType::Unknown)),
        ResolvedType::Unknown => Ok(wrap_output(ResolvedType::Unknown)),
        other => Err(env.type_error(
            format!(
                "transform method expects a unary function, found {}",
                describe_type(&other)
            ),
            args[0].span,
        )),
    }
}

fn infer_builtin_option_chain_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    let mapped =
        infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, "Option.and_then")?;
    ensure_option_return_type(mapped, input_ty)
}

fn infer_builtin_option_filter_method(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    input_ty: &ResolvedType,
    args: &[CallArg],
    span: Span,
) -> KainResult<ResolvedType> {
    let predicate =
        infer_builtin_unary_callback_return(env, ctx, input_ty, args, span, "Option.filter")?;
    ensure_type_compatible(
        env,
        &ResolvedType::Bool,
        &predicate,
        span,
        "Option.filter predicate",
    )?;
    Ok(ResolvedType::Option(Box::new(input_ty.clone())))
}

fn infer_macro_type(
    env: &mut TypeEnv,
    name: &str,
    args: &[Expr],
    ctx: Option<&SemanticContext>,
) -> KainResult<ResolvedType> {
    match name {
        "vec" => {
            let arg_types = args
                .iter()
                .map(|arg| infer_expr_type(env, arg, ctx))
                .collect::<Result<Vec<_>, _>>()?;
            let element_ty = arg_types
                .into_iter()
                .fold(ResolvedType::Unknown, |acc, ty| {
                    if matches!(acc, ResolvedType::Unknown) {
                        ty
                    } else {
                        unify_types(&acc, &ty).unwrap_or(ResolvedType::Unknown)
                    }
                });
            Ok(ResolvedType::Array(Box::new(element_ty), args.len()))
        }
        "format" | "type_name" => Ok(ResolvedType::String),
        "panic" => Ok(ResolvedType::Never),
        _ => Ok(ResolvedType::Unknown),
    }
}

fn pattern_match_subject_type(ty: &ResolvedType) -> &ResolvedType {
    match ty {
        ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => inner.as_ref(),
        other => other,
    }
}

fn borrowed_pattern_binding_type(
    container_ty: &ResolvedType,
    field_ty: &ResolvedType,
) -> ResolvedType {
    match container_ty {
        ResolvedType::Ref { mutable, .. } => ResolvedType::Ref {
            mutable: *mutable,
            inner: Box::new(field_ty.clone()),
        },
        ResolvedType::Ptr { mutable, .. } => ResolvedType::Ptr {
            mutable: *mutable,
            inner: Box::new(field_ty.clone()),
        },
        _ => field_ty.clone(),
    }
}

fn check_pattern_compatibility(
    env: &mut TypeEnv,
    pattern: &Pattern,
    scrutinee_ty: &ResolvedType,
) -> KainResult<()> {
    match pattern {
        Pattern::Wildcard(_) | Pattern::Binding { .. } => Ok(()),
        Pattern::Literal(expr) => {
            let literal_ty = infer_expr_type(env, expr, None)?;
            ensure_type_compatible(
                env,
                pattern_match_subject_type(scrutinee_ty),
                &literal_ty,
                expr.span(),
                "match pattern",
            )
        }
        Pattern::Tuple(patterns, span) => match pattern_match_subject_type(scrutinee_ty) {
            ResolvedType::Tuple(items) if items.len() == patterns.len() => {
                for (pattern, item_ty) in patterns.iter().zip(items.iter()) {
                    check_pattern_compatibility(env, pattern, item_ty)?;
                }
                Ok(())
            }
            ResolvedType::Unknown => Ok(()),
            other => Err(env.type_error(
                format!("Tuple pattern does not match {}", describe_type(other)),
                *span,
            )),
        },
        Pattern::Struct {
            name, fields, span, ..
        } => match pattern_match_subject_type(scrutinee_ty) {
            ResolvedType::Struct(struct_name, known_fields)
                if struct_name == name
                    || matches!(
                        pattern_match_subject_type(scrutinee_ty),
                        ResolvedType::Unknown
                    ) =>
            {
                for (field_name, field_pattern) in fields {
                    let field_ty = known_fields
                        .get(field_name)
                        .cloned()
                        .unwrap_or(ResolvedType::Unknown);
                    check_pattern_compatibility(env, field_pattern, &field_ty)?;
                }
                Ok(())
            }
            ResolvedType::Unknown => Ok(()),
            other => Err(env.type_error(
                format!(
                    "Struct pattern '{}' does not match {}",
                    name,
                    describe_type(other)
                ),
                *span,
            )),
        },
        Pattern::Variant {
            enum_name,
            variant,
            fields,
            span,
        } => {
            let match_ty = pattern_match_subject_type(scrutinee_ty);
            let expected_enum = enum_name
                .as_deref()
                .or_else(|| match match_ty {
                    ResolvedType::Enum(name, _) => Some(name.as_str()),
                    _ => None,
                })
                .unwrap_or_default();
            let field_types = if !expected_enum.is_empty() {
                env.lookup_enum_variant_fields(expected_enum, variant)
                    .cloned()
            } else {
                builtin_variant_field_types(match_ty, variant)
            };
            if let Some(field_types) = field_types {
                match (fields, field_types.as_slice()) {
                    (VariantPatternFields::Unit, []) => {}
                    (VariantPatternFields::Tuple(patterns), types)
                        if patterns.len() == types.len() =>
                    {
                        for (pattern, field_ty) in patterns.iter().zip(types.iter()) {
                            check_pattern_compatibility(env, pattern, field_ty)?;
                        }
                    }
                    (VariantPatternFields::Struct(patterns), _) => {
                        for (field_name, pattern) in patterns {
                            if let Some(field_ty) = env
                                .lookup_enum_variant_named_field(expected_enum, variant, field_name)
                                .cloned()
                            {
                                check_pattern_compatibility(env, pattern, &field_ty)?;
                            } else {
                                return Err(env.type_error(
                                    format!(
                                        "Unknown field '{}::{}.{}'",
                                        expected_enum, variant, field_name
                                    ),
                                    *span,
                                ));
                            }
                        }
                    }
                    _ => {}
                }
            }
            match match_ty {
                ResolvedType::Enum(enum_name, _)
                    if expected_enum.is_empty() || expected_enum == enum_name =>
                {
                    Ok(())
                }
                ResolvedType::Struct(type_name, _)
                    if !expected_enum.is_empty() && expected_enum == type_name =>
                {
                    Ok(())
                }
                ResolvedType::Enum(_, _)
                | ResolvedType::Option(_)
                | ResolvedType::Result(_, _)
                | ResolvedType::Unknown => Ok(()),
                other => Err(env.type_error(
                    format!("Variant pattern does not match {}", describe_type(other)),
                    *span,
                )),
            }
        }
        Pattern::Slice { patterns, span, .. } => match pattern_match_subject_type(scrutinee_ty) {
            ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => {
                for pattern in patterns {
                    check_pattern_compatibility(env, pattern, inner.as_ref())?;
                }
                Ok(())
            }
            ResolvedType::Unknown => Ok(()),
            other => Err(env.type_error(
                format!("Slice pattern does not match {}", describe_type(other)),
                *span,
            )),
        },
        Pattern::Or(patterns, _) => {
            for pattern in patterns {
                check_pattern_compatibility(env, pattern, scrutinee_ty)?;
            }
            Ok(())
        }
        Pattern::Range {
            start, end, span, ..
        } => {
            if let Some(start) = start {
                let start_ty = infer_expr_type(env, start, None)?;
                ensure_type_compatible(env, scrutinee_ty, &start_ty, *span, "range pattern")?;
            }
            if let Some(end) = end {
                let end_ty = infer_expr_type(env, end, None)?;
                ensure_type_compatible(env, scrutinee_ty, &end_ty, *span, "range pattern")?;
            }
            Ok(())
        }
    }
}

fn bind_pattern_types(env: &mut TypeEnv, pattern: &Pattern, ty: &ResolvedType) -> KainResult<()> {
    match pattern {
        Pattern::Wildcard(_) | Pattern::Literal(_) => Ok(()),
        Pattern::Binding { name, .. } => {
            env.define(name.clone(), ty.clone());
            Ok(())
        }
        Pattern::Tuple(patterns, _) => {
            match pattern_match_subject_type(ty) {
                ResolvedType::Tuple(items) => {
                    for (pattern, item_ty) in patterns.iter().zip(items.iter()) {
                        let bound_ty = borrowed_pattern_binding_type(ty, item_ty);
                        bind_pattern_types(env, pattern, &bound_ty)?;
                    }
                }
                ResolvedType::Unknown => {
                    for pattern in patterns {
                        bind_pattern_types(env, pattern, &ResolvedType::Unknown)?;
                    }
                }
                _ => {}
            }
            Ok(())
        }
        Pattern::Struct { fields, .. } => {
            if let ResolvedType::Struct(_, known_fields) = pattern_match_subject_type(ty) {
                for (field_name, pattern) in fields {
                    if let Some(field_ty) = known_fields.get(field_name) {
                        let bound_ty = borrowed_pattern_binding_type(ty, field_ty);
                        bind_pattern_types(env, pattern, &bound_ty)?;
                    }
                }
            }
            Ok(())
        }
        Pattern::Variant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let match_ty = pattern_match_subject_type(ty);
            let enum_name = enum_name.as_deref().or_else(|| match match_ty {
                ResolvedType::Enum(name, _) => Some(name.as_str()),
                _ => None,
            });
            let field_types = if let Some(enum_name) = enum_name {
                env.lookup_enum_variant_fields(enum_name, variant).cloned()
            } else {
                builtin_variant_field_types(match_ty, variant)
            };
            if let Some(field_types) = field_types {
                match fields {
                    VariantPatternFields::Tuple(patterns) => {
                        for (pattern, field_ty) in patterns.iter().zip(field_types.iter()) {
                            let bound_ty = borrowed_pattern_binding_type(ty, field_ty);
                            bind_pattern_types(env, pattern, &bound_ty)?;
                        }
                    }
                    VariantPatternFields::Struct(patterns) => {
                        if let Some(enum_name) = enum_name {
                            for (field_name, pattern) in patterns {
                                if let Some(field_ty) = env
                                    .lookup_enum_variant_named_field(enum_name, variant, field_name)
                                {
                                    let bound_ty = borrowed_pattern_binding_type(ty, field_ty);
                                    bind_pattern_types(env, pattern, &bound_ty)?;
                                }
                            }
                        }
                    }
                    VariantPatternFields::Unit => {}
                }
            } else if matches!(match_ty, ResolvedType::Unknown) {
                match fields {
                    VariantPatternFields::Tuple(patterns) => {
                        for pattern in patterns {
                            bind_pattern_types(env, pattern, &ResolvedType::Unknown)?;
                        }
                    }
                    VariantPatternFields::Struct(patterns) => {
                        for (_, pattern) in patterns {
                            bind_pattern_types(env, pattern, &ResolvedType::Unknown)?;
                        }
                    }
                    VariantPatternFields::Unit => {}
                }
            }
            Ok(())
        }
        Pattern::Slice { patterns, rest, .. } => {
            let item_ty = match pattern_match_subject_type(ty) {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => {
                    inner.as_ref().clone()
                }
                _ => ResolvedType::Unknown,
            };
            for pattern in patterns {
                let bound_ty = borrowed_pattern_binding_type(ty, &item_ty);
                bind_pattern_types(env, pattern, &bound_ty)?;
            }
            if let Some(rest_name) = rest {
                let rest_ty =
                    borrowed_pattern_binding_type(ty, &ResolvedType::Slice(Box::new(item_ty)));
                env.define(rest_name.clone(), rest_ty);
            }
            Ok(())
        }
        Pattern::Or(patterns, _) => {
            for pattern in patterns {
                bind_pattern_types(env, pattern, ty)?;
            }
            Ok(())
        }
        Pattern::Range { .. } => Ok(()),
    }
}

/// Known JSX event callback names (after stripping "on_" prefix).
/// These are the valid event_kind values for `JSXAttrValue::Callback`.
const KNOWN_JSX_EVENT_KINDS: &[&str] = &[
    "click", "change", "toggle", "focus", "blur",
    "mouseenter", "mouseleave", "submit", "cancel",
    "hover", "press", "release", "drag",
];

/// Validate a JSX event callback attribute.
/// Checks that the event_kind is known and the expression is a function type.
fn validate_jsx_event_callback(
    env: &mut TypeEnv,
    event_kind: &str,
    expr: &Expr,
    attr_span: Span,
) -> KainResult<()> {
    // 1. Validate event kind is known
    if !KNOWN_JSX_EVENT_KINDS.contains(&event_kind) {
        return Err(env.type_error(
            format!(
                "Unknown JSX event '{}'. Known events: {}",
                event_kind,
                KNOWN_JSX_EVENT_KINDS.join(", ")
            ),
            attr_span,
        ));
    }

    // 2. Validate the callback expression is a function type
    // Accept: () -> Void, (Event) -> Void, (Float) -> Void (for slider on_change)
    let fn_ty = infer_expr_type(env, expr, None)?;
    match &fn_ty {
        ResolvedType::Function { params, ret, effects: _ } => {
            // Return type must be Unit-compatible (void, nothing, or inferred)
            if !matches!(ret.as_ref(), ResolvedType::Unit | ResolvedType::Never | ResolvedType::Unknown) {
                return Err(env.type_error(
                    format!(
                        "Event callback '{}' must return Void (unit), found {}",
                        event_kind,
                        describe_type(ret)
                    ),
                    attr_span,
                ));
            }
            // Parameter count check: 0, 1, or 2 params are acceptable
            if params.len() > 2 {
                return Err(env.type_error(
                    format!(
                        "Event callback '{}' takes too many parameters (expected 0-2, got {})",
                        event_kind,
                        params.len()
                    ),
                    attr_span,
                ));
            }
            Ok(())
        }
        _ => Err(env.type_error(
            format!(
                "Event callback '{}' must be a function, got {}",
                event_kind,
                describe_type(&fn_ty)
            ),
            attr_span,
        )),
    }
}

fn check_jsx_semantics(
    env: &mut TypeEnv,
    node: &JSXNode,
    ctx: Option<&SemanticContext>,
) -> KainResult<()> {
    match node {
        JSXNode::Element {
            attributes,
            children,
            ..
        }
        | JSXNode::ComponentCall {
            props: attributes,
            children,
            ..
        } => {
            for attribute in attributes {
                match &attribute.value {
                    JSXAttrValue::Expr(expr) => {
                        let _ = infer_expr_type(env, expr, ctx)?;
                    }
                    JSXAttrValue::Callback(event_kind, expr) => {
                        validate_jsx_event_callback(env, event_kind, expr, attribute.span)?;
                    }
                    _ => {} // String, Bool — no validation needed
                }
            }
            for child in children {
                check_jsx_semantics(env, child, ctx)?;
            }
        }
        JSXNode::Expression(expr) => {
            let _ = infer_expr_type(env, expr, ctx)?;
        }
        JSXNode::For {
            binding,
            iter,
            body,
            ..
        } => {
            let iter_ty = infer_expr_type(env, iter, ctx)?;
            let item_ty = match iter_ty {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => *inner,
                ResolvedType::String => ResolvedType::String,
                _ => ResolvedType::Unknown,
            };
            env.with_scope(|env| {
                env.define(binding.clone(), item_ty);
                check_jsx_semantics(env, body, ctx)
            })?;
        }
        JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let cond_ty = infer_expr_type(env, condition, ctx)?;
            ensure_condition_type_compatible(env, &cond_ty, condition.span(), "jsx if condition")?;
            check_jsx_semantics(env, then_branch, ctx)?;
            if let Some(else_branch) = else_branch {
                check_jsx_semantics(env, else_branch, ctx)?;
            }
        }
        JSXNode::Fragment(children, _) => {
            for child in children {
                check_jsx_semantics(env, child, ctx)?;
            }
        }
        JSXNode::Text(_, _) => {}
    }
    Ok(())
}

fn infer_binary_type(
    env: &TypeEnv,
    op: &BinaryOp,
    left: &ResolvedType,
    right: &ResolvedType,
    span: Span,
) -> KainResult<ResolvedType> {
    use BinaryOp::*;
    match op {
        Add | Sub | Mul | Div | Mod | Pow => {
            if matches!(op, Add)
                && (matches!(left, ResolvedType::String) || matches!(right, ResolvedType::String))
            {
                return Ok(ResolvedType::String);
            }
            if is_numeric_like(left) && is_numeric_like(right) {
                return Ok(promote_numeric_type(left, right));
            }
            if matches!(left, ResolvedType::Generic(_))
                && matches!(right, ResolvedType::Generic(_))
                && types_compatible(left, right)
            {
                return Ok(left.clone());
            }
            if matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
                || is_none_placeholder_type(left)
                || is_none_placeholder_type(right)
            {
                return Ok(ResolvedType::Unknown);
            }
            Err(env.type_error(
                format!(
                    "Binary operator expects compatible numeric operands, found {} and {}",
                    describe_type(left),
                    describe_type(right)
                ),
                span,
            ))
        }
        Eq | Ne => {
            if (is_numeric_like(left) && is_numeric_like(right))
                || types_compatible(left, right)
                || matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
                || is_none_placeholder_type(left)
                || is_none_placeholder_type(right)
                || (is_ts_import_scalar_comparison_operand(left)
                    && is_ts_import_scalar_comparison_operand(right))
            {
                Ok(ResolvedType::Bool)
            } else {
                Err(env.type_error(
                    format!(
                        "Equality operands do not agree: {} vs {}",
                        describe_type(left),
                        describe_type(right)
                    ),
                    span,
                ))
            }
        }
        Lt | Gt | Le | Ge => {
            if (is_numeric_like(left) && is_numeric_like(right))
                || types_compatible(left, right)
                || matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
                || is_none_placeholder_type(left)
                || is_none_placeholder_type(right)
                || (is_ts_import_scalar_comparison_operand(left)
                    && is_ts_import_scalar_comparison_operand(right))
            {
                Ok(ResolvedType::Bool)
            } else {
                Err(env.type_error(
                    format!(
                        "Comparison operands do not agree: {} vs {}",
                        describe_type(left),
                        describe_type(right)
                    ),
                    span,
                ))
            }
        }
        And | Or => {
            if matches!(left, ResolvedType::Bool) && matches!(right, ResolvedType::Bool) {
                Ok(ResolvedType::Bool)
            } else if matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
                || is_none_placeholder_type(left)
                || is_none_placeholder_type(right)
            {
                Ok(ResolvedType::Unknown)
            } else {
                Ok(unify_types(left, right).unwrap_or(ResolvedType::Unknown))
            }
        }
        BitAnd | BitOr | BitXor | Shl | Shr => {
            if is_integer_like(left) && is_integer_like(right) {
                Ok(promote_numeric_type(left, right))
            } else if is_numeric_like(left) && is_numeric_like(right) {
                Ok(ResolvedType::Int(IntSize::I64))
            } else if matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
                || is_none_placeholder_type(left)
                || is_none_placeholder_type(right)
            {
                Ok(ResolvedType::Unknown)
            } else {
                Err(env.type_error(
                    format!(
                        "Bitwise operator expects integer operands, found {} and {}",
                        describe_type(left),
                        describe_type(right)
                    ),
                    span,
                ))
            }
        }
        Assign | AddAssign | SubAssign | MulAssign | DivAssign => Ok(ResolvedType::Unit),
        Range | RangeInclusive => Ok(ResolvedType::Unknown),
    }
}

fn ensure_type_compatible(
    env: &TypeEnv,
    expected: &ResolvedType,
    actual: &ResolvedType,
    span: Span,
    context: &str,
) -> KainResult<()> {
    if types_compatible(expected, actual) {
        Ok(())
    } else {
        Err(env.type_error(
            format!(
                "{} expected {}, found {}",
                context,
                describe_type(expected),
                describe_type(actual)
            ),
            span,
        ))
    }
}

fn ensure_condition_type_compatible(
    env: &TypeEnv,
    actual: &ResolvedType,
    span: Span,
    context: &str,
) -> KainResult<()> {
    let condition_ty = peel_shared_refs(actual);
    match condition_ty {
        ResolvedType::Bool
        | ResolvedType::Option(_)
        | ResolvedType::String
        | ResolvedType::Char
        | ResolvedType::Int(_)
        | ResolvedType::Float(_)
        | ResolvedType::Array(_, _)
        | ResolvedType::Slice(_)
        | ResolvedType::Tuple(_)
        | ResolvedType::Struct(_, _)
        | ResolvedType::Enum(_, _)
        | ResolvedType::Function { .. }
        | ResolvedType::Unknown
        | ResolvedType::Never
        | ResolvedType::Generic(_) => Ok(()),
        _ => Err(env.type_error(
            format!("{} expected Bool, found {}", context, describe_type(actual)),
            span,
        )),
    }
}

fn types_compatible(expected: &ResolvedType, actual: &ResolvedType) -> bool {
    match (expected, actual) {
        (ResolvedType::Unknown, _) | (_, ResolvedType::Unknown) => true,
        (ResolvedType::Never, _) | (_, ResolvedType::Never) => true,
        (ResolvedType::Generic(_), _) | (_, ResolvedType::Generic(_)) => true,
        (expected, actual) if type_contains_unknown(expected) || type_contains_unknown(actual) => {
            true
        }
        (expected, actual)
            if is_none_placeholder_type(expected) || is_none_placeholder_type(actual) =>
        {
            true
        }
        (ResolvedType::Unit, ResolvedType::Unit)
        | (ResolvedType::Bool, ResolvedType::Bool)
        | (ResolvedType::String, ResolvedType::String)
        | (ResolvedType::Char, ResolvedType::Char) => true,
        (
            ResolvedType::String,
            ResolvedType::Ref {
                mutable: false,
                inner,
            },
        ) => types_compatible(expected, peel_shared_refs(inner.as_ref())),
        (ResolvedType::Int(_), ResolvedType::Int(_)) => true,
        (ResolvedType::Float(_), ResolvedType::Float(_)) => true,
        (ResolvedType::Int(_), ResolvedType::Float(_))
        | (ResolvedType::Float(_), ResolvedType::Int(_)) => true,
        (ResolvedType::Array(left, left_len), ResolvedType::Array(right, right_len)) => {
            (left_len == right_len || *left_len == 0 || *right_len == 0)
                && collection_element_types_compatible(left, right)
        }
        (ResolvedType::Slice(left), ResolvedType::Slice(right)) => {
            collection_element_types_compatible(left, right)
        }
        (ResolvedType::Slice(left), ResolvedType::Array(right, _))
        | (ResolvedType::Array(left, _), ResolvedType::Slice(right)) => {
            collection_element_types_compatible(left, right)
        }
        (ResolvedType::Tuple(left), ResolvedType::Tuple(right)) => {
            left.len() == right.len()
                && left
                    .iter()
                    .zip(right.iter())
                    .all(|(left, right)| types_compatible(left, right))
        }
        (ResolvedType::Tuple(_), ResolvedType::Array(_, _) | ResolvedType::Slice(_))
        | (ResolvedType::Array(_, _) | ResolvedType::Slice(_), ResolvedType::Tuple(_)) => true,
        (
            ResolvedType::Struct(_, _),
            ResolvedType::Array(_, _) | ResolvedType::Slice(_) | ResolvedType::Tuple(_),
        )
        | (
            ResolvedType::Array(_, _) | ResolvedType::Slice(_) | ResolvedType::Tuple(_),
            ResolvedType::Struct(_, _),
        ) => true,
        (ResolvedType::Option(left), ResolvedType::Option(right)) => types_compatible(left, right),
        (
            ResolvedType::Option(left),
            ResolvedType::Ref {
                mutable: false,
                inner,
            },
        ) => match inner.as_ref() {
            ResolvedType::Option(right) => {
                types_compatible(left, &shared_ref_type(right.as_ref().clone()))
            }
            _ => false,
        },
        (
            ResolvedType::Ref {
                mutable: false,
                inner,
            },
            ResolvedType::Option(right),
        ) => match inner.as_ref() {
            ResolvedType::Option(left) => {
                types_compatible(&shared_ref_type(left.as_ref().clone()), right)
            }
            _ => false,
        },
        (ResolvedType::Result(left_ok, left_err), ResolvedType::Result(right_ok, right_err)) => {
            types_compatible(left_ok, right_ok) && types_compatible(left_err, right_err)
        }
        (ResolvedType::Future(left), ResolvedType::Future(right)) => types_compatible(left, right),
        (
            ResolvedType::Ref {
                mutable: expected_mutable,
                inner: expected_inner,
            },
            ResolvedType::Ref {
                mutable: actual_mutable,
                inner: actual_inner,
            },
        ) => {
            (!expected_mutable || *actual_mutable)
                && if *expected_mutable {
                    types_compatible(expected_inner, actual_inner)
                } else {
                    types_compatible(expected_inner, peel_shared_refs(actual_inner))
                }
        }
        (
            ResolvedType::Ptr {
                mutable: expected_mutable,
                inner: expected_inner,
            },
            ResolvedType::Ptr {
                mutable: actual_mutable,
                inner: actual_inner,
            },
        ) => {
            (!expected_mutable || *actual_mutable) && types_compatible(expected_inner, actual_inner)
        }
        (_, ResolvedType::Function { params, ret, .. })
            if params.is_empty()
                && matches!(ret.as_ref(), ResolvedType::Unknown | ResolvedType::Never) =>
        {
            true
        }
        (
            ResolvedType::Function {
                params: left_params,
                ret: left_ret,
                ..
            },
            ResolvedType::Function {
                params: right_params,
                ret: right_ret,
                ..
            },
        ) => {
            left_params.len() == right_params.len()
                && left_params
                    .iter()
                    .zip(right_params.iter())
                    .all(|(left, right)| types_compatible(left, right))
                && types_compatible(left_ret, right_ret)
        }
        (ResolvedType::Struct(_, _), ResolvedType::Struct(_, _)) => true,
        (ResolvedType::Enum(left, _), ResolvedType::Enum(right, _)) => left == right,
        _ => false,
    }
}

fn tuple_index_type(items: &[ResolvedType], index: &Expr) -> ResolvedType {
    if let Expr::Int(index, _) = index {
        if let Ok(index) = usize::try_from(*index) {
            return items.get(index).cloned().unwrap_or(ResolvedType::Unknown);
        }
    }
    ResolvedType::Unknown
}

fn collection_element_types_compatible(expected: &ResolvedType, actual: &ResolvedType) -> bool {
    types_compatible(expected, actual)
        || is_none_placeholder_type(expected)
        || is_none_placeholder_type(actual)
        || matches!(
            (expected, actual),
            (ResolvedType::Array(_, _) | ResolvedType::Slice(_), _)
                | (_, ResolvedType::Array(_, _) | ResolvedType::Slice(_))
        )
}

fn is_none_placeholder_type(ty: &ResolvedType) -> bool {
    matches!(ty, ResolvedType::Option(inner) if matches!(inner.as_ref(), ResolvedType::Unknown))
}

fn type_contains_unknown(ty: &ResolvedType) -> bool {
    match ty {
        ResolvedType::Unknown => true,
        ResolvedType::Array(inner, _)
        | ResolvedType::Slice(inner)
        | ResolvedType::Option(inner)
        | ResolvedType::Future(inner)
        | ResolvedType::Ref { inner, .. }
        | ResolvedType::Ptr { inner, .. } => type_contains_unknown(inner.as_ref()),
        ResolvedType::Result(ok, err) => {
            type_contains_unknown(ok.as_ref()) || type_contains_unknown(err.as_ref())
        }
        ResolvedType::Tuple(items) => items.iter().any(type_contains_unknown),
        ResolvedType::Function { params, ret, .. } => {
            params.iter().any(type_contains_unknown) || type_contains_unknown(ret.as_ref())
        }
        ResolvedType::Unit
        | ResolvedType::Never
        | ResolvedType::Bool
        | ResolvedType::Int(_)
        | ResolvedType::Float(_)
        | ResolvedType::String
        | ResolvedType::Char
        | ResolvedType::Struct(_, _)
        | ResolvedType::Enum(_, _)
        | ResolvedType::Generic(_) => false,
    }
}

fn is_ts_import_scalar_comparison_operand(ty: &ResolvedType) -> bool {
    matches!(
        ty,
        ResolvedType::Bool
            | ResolvedType::Char
            | ResolvedType::String
            | ResolvedType::Int(_)
            | ResolvedType::Float(_)
            | ResolvedType::Generic(_)
            | ResolvedType::Unknown
    ) || is_none_placeholder_type(ty)
}

fn unify_types(left: &ResolvedType, right: &ResolvedType) -> Option<ResolvedType> {
    match (left, right) {
        (ResolvedType::Unknown, other) | (other, ResolvedType::Unknown) => Some(other.clone()),
        (ResolvedType::Never, other) | (other, ResolvedType::Never) => Some(other.clone()),
        (ResolvedType::Int(_), ResolvedType::Int(_))
        | (ResolvedType::Float(_), ResolvedType::Float(_))
        | (ResolvedType::Int(_), ResolvedType::Float(_))
        | (ResolvedType::Float(_), ResolvedType::Int(_)) => Some(promote_numeric_type(left, right)),
        (ResolvedType::Generic(_), other) | (other, ResolvedType::Generic(_)) => {
            Some(other.clone())
        }
        _ if types_compatible(left, right) => Some(left.clone()),
        _ => None,
    }
}

fn promote_numeric_type(left: &ResolvedType, right: &ResolvedType) -> ResolvedType {
    if matches!(left, ResolvedType::Float(_)) || matches!(right, ResolvedType::Float(_)) {
        ResolvedType::Float(FloatSize::F64)
    } else {
        ResolvedType::Int(IntSize::I64)
    }
}

fn is_numeric_like(ty: &ResolvedType) -> bool {
    matches!(
        ty,
        ResolvedType::Int(_) | ResolvedType::Float(_) | ResolvedType::Unknown
    )
}

fn is_integer_like(ty: &ResolvedType) -> bool {
    matches!(ty, ResolvedType::Int(_) | ResolvedType::Unknown)
}

fn describe_type(ty: &ResolvedType) -> String {
    match ty {
        ResolvedType::Unit => "Unit".to_string(),
        ResolvedType::Bool => "Bool".to_string(),
        ResolvedType::Int(_) => "Int".to_string(),
        ResolvedType::Float(_) => "Float".to_string(),
        ResolvedType::String => "String".to_string(),
        ResolvedType::Char => "Char".to_string(),
        ResolvedType::Array(inner, _) => format!("Array<{}>", describe_type(inner)),
        ResolvedType::Slice(inner) => format!("Slice<{}>", describe_type(inner)),
        ResolvedType::Tuple(items) => format!(
            "({})",
            items
                .iter()
                .map(describe_type)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        ResolvedType::Option(inner) => format!("Option<{}>", describe_type(inner)),
        ResolvedType::Result(ok, err) => {
            format!("Result<{}, {}>", describe_type(ok), describe_type(err))
        }
        ResolvedType::Future(inner) => format!("Future<{}>", describe_type(inner)),
        ResolvedType::Ref { mutable, inner } => {
            if *mutable {
                format!("&mut {}", describe_type(inner))
            } else {
                format!("&{}", describe_type(inner))
            }
        }
        ResolvedType::Ptr { mutable, inner } => {
            if *mutable {
                format!("ptr_mut<{}>", describe_type(inner))
            } else {
                format!("ptr<{}>", describe_type(inner))
            }
        }
        ResolvedType::Function { params, ret, .. } => format!(
            "fn({}) -> {}",
            params
                .iter()
                .map(describe_type)
                .collect::<Vec<_>>()
                .join(", "),
            describe_type(ret)
        ),
        ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) => name.clone(),
        ResolvedType::Generic(name) => name.clone(),
        ResolvedType::Never => "!".to_string(),
        ResolvedType::Unknown => "Unknown".to_string(),
    }
}

fn resolved_type_name(ty: &ResolvedType) -> Option<&str> {
    match ty {
        ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) => Some(name.as_str()),
        _ => None,
    }
}

fn tuple_field_type(
    env: &TypeEnv,
    items: &[ResolvedType],
    field: &str,
    span: Span,
) -> KainResult<ResolvedType> {
    let index = match field {
        "x" | "r" => 0,
        "y" | "g" => 1,
        "z" | "b" => 2,
        "w" | "a" => 3,
        _ => match field.strip_prefix("__kain_tuple_") {
            Some(index) => index.parse::<usize>().map_err(|_| {
                env.type_error(format!("Unknown tuple/vector field '{}'", field), span)
            })?,
            None => {
                return Err(env.type_error(format!("Unknown tuple/vector field '{}'", field), span))
            }
        },
    };
    items.get(index).cloned().ok_or_else(|| {
        env.type_error(
            format!("Field '{}' is out of bounds for this tuple", field),
            span,
        )
    })
}

fn field_access_type(
    env: &TypeEnv,
    object_ty: &ResolvedType,
    field: &str,
    span: Span,
) -> KainResult<ResolvedType> {
    match object_ty {
        ResolvedType::Struct(name, fields) => {
            if let Some(field_ty) = fields.get(field) {
                return Ok(field_ty.clone());
            }
            if let Some(ResolvedType::Struct(_, refreshed_fields)) = env.lookup_type(name) {
                if let Some(field_ty) = refreshed_fields.get(field) {
                    return Ok(field_ty.clone());
                }
            }
            Ok(ResolvedType::Unknown)
        }
        ResolvedType::Tuple(items) => tuple_field_type(env, items, field, span),
        ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
            field_access_type(env, inner, field, span)
        }
        ResolvedType::Function { params, ret, .. }
            if params.is_empty()
                && matches!(ret.as_ref(), ResolvedType::Unknown | ResolvedType::Never) =>
        {
            Ok(ResolvedType::Unknown)
        }
        ResolvedType::Unknown
        | ResolvedType::Generic(_)
        | ResolvedType::String
        | ResolvedType::Char
        | ResolvedType::Bool
        | ResolvedType::Int(_)
        | ResolvedType::Float(_)
        | ResolvedType::Option(_)
        | ResolvedType::Result(_, _)
        | ResolvedType::Future(_)
        | ResolvedType::Function { .. }
        | ResolvedType::Enum(_, _)
        | ResolvedType::Array(_, _)
        | ResolvedType::Slice(_) => Ok(ResolvedType::Unknown),
        other => Err(env.type_error(
            format!(
                "Field access requires a struct value, found {}",
                describe_type(other)
            ),
            span,
        )),
    }
}

fn builtin_variant_field_types(ty: &ResolvedType, variant: &str) -> Option<Vec<ResolvedType>> {
    match (ty, variant) {
        (ResolvedType::Option(_), "None") => Some(Vec::new()),
        (ResolvedType::Option(inner), "Some") => Some(vec![inner.as_ref().clone()]),
        (ResolvedType::Result(ok, _), "Ok") => Some(vec![ok.as_ref().clone()]),
        (ResolvedType::Result(_, err), "Err") => Some(vec![err.as_ref().clone()]),
        _ => None,
    }
}

fn builtin_variant_expr_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    enum_name: &str,
    variant: &str,
    fields: &EnumVariantFields,
    span: Span,
) -> KainResult<Option<ResolvedType>> {
    match (enum_name, variant, fields) {
        ("Option", "None", EnumVariantFields::Unit) => {
            Ok(Some(ResolvedType::Option(Box::new(ResolvedType::Unknown))))
        }
        ("Option", "Some", EnumVariantFields::Tuple(values)) if values.len() == 1 => Ok(Some(
            ResolvedType::Option(Box::new(infer_expr_type(env, &values[0], ctx)?)),
        )),
        ("Result", "Ok", EnumVariantFields::Tuple(values)) if values.len() == 1 => {
            Ok(Some(ResolvedType::Result(
                Box::new(infer_expr_type(env, &values[0], ctx)?),
                Box::new(ResolvedType::Unknown),
            )))
        }
        ("Result", "Err", EnumVariantFields::Tuple(values)) if values.len() == 1 => {
            Ok(Some(ResolvedType::Result(
                Box::new(ResolvedType::Unknown),
                Box::new(infer_expr_type(env, &values[0], ctx)?),
            )))
        }
        ("Option", "None", _) => Err(env.type_error(
            "Variant 'Option::None' does not accept fields".to_string(),
            span,
        )),
        ("Option", "Some", _) => Err(env.type_error(
            "Variant 'Option::Some' expects exactly one tuple field".to_string(),
            span,
        )),
        ("Result", "Ok", _) => Err(env.type_error(
            "Variant 'Result::Ok' expects exactly one tuple field".to_string(),
            span,
        )),
        ("Result", "Err", _) => Err(env.type_error(
            "Variant 'Result::Err' expects exactly one tuple field".to_string(),
            span,
        )),
        _ => Ok(None),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::diagnostics::{SourceOriginSegment, SpanMapper};
    use crate::error::{ErrorKind, KainError};
    use crate::lexer::Lexer;
    use crate::parser::Parser;

    fn parse_source_for_typecheck(
        source: &str,
        span_mapper: &SpanMapper,
        filename: &str,
    ) -> Program {
        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        Parser::new(&tokens, span_mapper, filename)
            .parse()
            .expect("source should parse")
    }

    fn typecheck_source(source: &str) -> KainResult<TypedProgram> {
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");
        check(&program, &span_mapper, "<test>")
    }

    #[test]
    fn typecheck_accepts_declared_where_clause_traits() {
        typecheck_source(
            r#"trait Fold:
    fn fold(x: Int) -> Int

fn foo<T>(x: T) -> Int where T: Fold:
    return 1
"#,
        )
        .expect("declared where-clause trait should typecheck");
    }

    #[test]
    fn typecheck_rejects_where_clause_unknown_generic() {
        let err = typecheck_source(
            r#"trait Fold:
    fn fold(x: Int) -> Int

fn foo<T>(x: T) -> Int where U: Fold:
    return 1
"#,
        )
        .expect_err("unknown where generic should fail");
        assert!(err.to_string().contains("unknown generic 'U'"));
    }

    #[test]
    fn typecheck_rejects_where_clause_unknown_trait_and_duplicate_bounds() {
        let unknown_trait = typecheck_source(
            r#"fn foo<T>(x: T) -> Int where T: Missing:
    return 1
"#,
        )
        .expect_err("unknown trait should fail");
        assert!(unknown_trait
            .to_string()
            .contains("unknown trait 'Missing'"));

        let duplicate = typecheck_source(
            r#"trait Fold:
    fn fold(x: Int) -> Int

fn foo<T: Fold>(x: T) -> Int where T: Fold:
    return 1
"#,
        )
        .expect_err("duplicate inline and where bound should fail");
        assert!(duplicate.to_string().contains("duplicate generic bound"));
    }

    fn combine_sources_with_origins(units: &[(&str, &str)]) -> (String, Vec<SourceOriginSegment>) {
        let mut combined = String::new();
        let mut origins = Vec::new();
        let mut offset = 0usize;
        for (file, source) in units {
            combined.push_str(source);
            let end = offset + source.len();
            origins.push(SourceOriginSegment {
                file: (*file).to_string(),
                combined_span: Span::new(offset, end),
                source: (*source).to_string(),
            });
            offset = end;
            if !source.ends_with('\n') {
                combined.push('\n');
                offset += 1;
            }
        }
        (combined, origins)
    }

    fn repo_test_path(relative: &str) -> String {
        std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .join(relative)
            .to_string_lossy()
            .replace('\\', "/")
    }

    fn register_program_items_for_test<'a>(
        program: &Program,
        span_mapper: &'a SpanMapper,
        filename: &'a str,
    ) -> TypeEnv<'a> {
        let mut env = TypeEnv::new(span_mapper, filename);
        for item in &program.items {
            predeclare_item_types(&mut env, item);
        }
        for item in &program.items {
            register_item_types(&mut env, item).expect("registration pass should succeed");
        }
        for item in &program.items {
            register_item_types(&mut env, item).expect("refresh pass should succeed");
        }
        env
    }

    fn expect_typecheck_error_contains(source: &str, needle: &str) {
        let err = typecheck_source(source).expect_err("source should fail typecheck");
        assert!(
            err.to_string().contains(needle),
            "unexpected diagnostic: {err}"
        );
    }

    #[test]
    fn typecheck_combined_stdlib_ascii_and_base64_bundle() {
        let entry = r#"
use std::base64

fn main() -> Int:
    return len(base64_encode("ok"))
"#;
        let (combined, origins) = combine_sources_with_origins(&[
            (
                &repo_test_path("stdlib/ascii.kn"),
                include_str!("../../../stdlib/ascii.kn"),
            ),
            (
                &repo_test_path("stdlib/base64.kn"),
                include_str!("../../../stdlib/base64.kn"),
            ),
            ("<test>", entry),
        ]);
        let span_mapper = SpanMapper::with_origins(&combined, origins);
        let program = parse_source_for_typecheck(&combined, &span_mapper, "<test>");
        let env = register_program_items_for_test(&program, &span_mapper, "<test>");
        assert!(
            env.lookup("ascii_is_byte").is_some(),
            "combined stdlib bundle should keep ascii_is_byte registered"
        );
        check(&program, &span_mapper, "<test>")
            .expect("combined ascii/base64 stdlib bundle should typecheck");
    }

    #[test]
    fn typecheck_combined_stdlib_fs_bundle_keeps_native_fs_externs_registered() {
        let entry = r#"
use std::fs

fn main() -> Int:
    return len(fs_read_text("shadow-smoke.txt"))
"#;
        let (combined, origins) = combine_sources_with_origins(&[
            (
                &repo_test_path("stdlib/ascii.kn"),
                include_str!("../../../stdlib/ascii.kn"),
            ),
            (
                &repo_test_path("stdlib/base64.kn"),
                include_str!("../../../stdlib/base64.kn"),
            ),
            (
                &repo_test_path("stdlib/text.kn"),
                include_str!("../../../stdlib/text.kn"),
            ),
            (
                &repo_test_path("stdlib/fs.kn"),
                include_str!("../../../stdlib/fs.kn"),
            ),
            ("<test>", entry),
        ]);
        let span_mapper = SpanMapper::with_origins(&combined, origins);
        let program = parse_source_for_typecheck(&combined, &span_mapper, "<test>");
        let env = register_program_items_for_test(&program, &span_mapper, "<test>");
        assert!(
            env.lookup("abi_fs_metadata_text").is_some(),
            "combined stdlib bundle should keep abi_fs_metadata_text registered"
        );
        check(&program, &span_mapper, "<test>")
            .expect("combined fs stdlib bundle should typecheck");
    }

    #[test]
    fn typecheck_allows_thread_local_const_with_symbol_controls() {
        let source = r#"
@thread_local
@section(".tls")
@link_name("__kain_tls_counter")
const TLS_COUNTER: Int = 7

fn main() -> Int:
    return TLS_COUNTER
"#;
        typecheck_source(source).expect("thread_local const should typecheck");
    }

    #[test]
    fn typecheck_rejects_thread_local_on_functions() {
        let source = r#"
@thread_local
fn bad() -> Int:
    return 1
"#;
        expect_typecheck_error_contains(source, "@thread_local is not valid on functions");
    }

    #[test]
    fn typecheck_rejects_non_power_of_two_alignment() {
        let source = r#"
@aligned(24)
struct Packet:
    value: Int
"#;
        expect_typecheck_error_contains(source, "positive power-of-two byte alignment");
    }

    #[test]
    fn typecheck_allows_mmio_integer_register_blocks() {
        let source = r#"
@packed
@aligned(16)
@mmio(base: 4096, stride: 8, endian: "native")
struct DeviceRegs:
    control: Int
    status: Int
"#;
        typecheck_source(source).expect("mmio register block should typecheck");
    }

    #[test]
    fn typecheck_rejects_mmio_non_integer_fields() {
        let source = r#"
@mmio(base: 4096, stride: 8, endian: "native")
struct DeviceRegs:
    control: Float
"#;
        expect_typecheck_error_contains(
            source,
            "@mmio register blocks currently only support integer register fields",
        );
    }

    #[test]
    fn typecheck_rejects_mmio_stride_that_does_not_match_register_width() {
        let source = r#"
@mmio(base: 4096, stride: 4, endian: "native")
struct DeviceRegs:
    control: Int
"#;
        expect_typecheck_error_contains(
            source,
            "@mmio(stride: 4) currently requires each register field to occupy exactly 4 bytes",
        );
    }

    #[test]
    fn typecheck_rejects_bad_calling_convention_name() {
        let source = r#"
@callconv("vectorcall")
fn lane() -> Int:
    return 1
"#;
        expect_typecheck_error_contains(source, "@callconv only supports");
    }

    #[test]
    fn typecheck_rejects_naked_functions_with_non_asm_bodies() {
        let source = r#"
@naked
fn lane() with Unsafe:
    let value = 7
"#;
        expect_typecheck_error_contains(
            source,
            "@naked functions may only contain inline asm statements and bare returns",
        );
    }

    #[test]
    fn typecheck_allows_naked_inline_asm_lane() {
        let source = r#"
@naked
fn lane() with Unsafe:
    asm("ret")
"#;
        typecheck_source(source).expect("naked inline asm lane should typecheck");
    }

    #[test]
    fn type_env_registers_stdlib_registry_bridge_globals() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");
        assert!(
            env.lookup("kain_input_reset").is_some(),
            "stdlib registry globals should be visible to the typechecker"
        );
    }

    #[test]
    fn typecheck_predeclared_user_types_do_not_shadow_their_declarations() {
        let source = r#"
struct Packet:
    value: Int

fn main() -> Int:
    let packet = Packet { value: 7 }
    return packet.value
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");

        check(&program, &span_mapper, "<test>")
            .expect("predeclared struct should register cleanly");
    }

    #[test]
    fn typecheck_allows_stdlib_origin_to_wrap_builtin_global() {
        let source = r#"
pub fn fs_read_text(path: String) -> String:
    return path
"#;
        let span_mapper = SpanMapper::with_origins(
            source,
            vec![SourceOriginSegment {
                file: repo_test_path("stdlib/fs.kn"),
                combined_span: Span::new(0, source.len()),
                source: source.to_string(),
            }],
        );
        let program = parse_source_for_typecheck(source, &span_mapper, "<input>");

        check(&program, &span_mapper, "<input>")
            .expect("stdlib wrapper should be allowed to occupy builtin global name");
    }

    #[test]
    fn typecheck_dynamic_stdlib_import_can_wrap_builtin_global() {
        let source = r#"
use std::fs

fn main() -> String:
    return fs_read_text("shadow-smoke.txt")
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");

        check(&program, &span_mapper, "<test>")
            .expect("dynamic stdlib import should wrap builtin globals cleanly");
    }

    #[test]
    fn typecheck_dynamic_stdlib_import_can_register_collections_types_once() {
        let source = r#"
use std::collections

fn main() -> Int:
    let map = typed_map_set(typed_map_new(), "route", 41)
    return typed_map_get(map, "route")
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");

        check(&program, &span_mapper, "<test>")
            .expect("dynamic stdlib import should not self-shadow StringIntMap");
    }

    #[test]
    fn typecheck_stdlib_extern_declarations_are_idempotent() {
        let source = r#"
@extern
fn abi_runtime_init() -> Int

pub fn runtime_init() -> Int:
    return abi_runtime_init()
"#;
        let span_mapper = SpanMapper::new(source);
        let runtime_path = repo_test_path("stdlib/runtime.kn");
        let program = parse_source_for_typecheck(source, &span_mapper, &runtime_path);

        check(&program, &span_mapper, &runtime_path)
            .expect("stdlib @extern declarations should register cleanly across passes");
    }

    #[test]
    fn typecheck_dynamic_stdlib_runtime_import_registers_extern_wrappers_once() {
        let source = r#"
use std::runtime

fn main() -> Int:
    return runtime_init()
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");

        check(&program, &span_mapper, "<test>")
            .expect("dynamic stdlib runtime import should register extern wrappers cleanly");
    }

    #[test]
    fn typecheck_real_stdlib_runtime_declarations_do_not_self_collide() {
        let source = include_str!("../../../stdlib/runtime.kn");
        let filename = repo_test_path("stdlib/runtime.kn");
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, &filename);

        check(&program, &span_mapper, &filename)
            .expect("real stdlib/runtime.kn should not self-collide during registration");
    }

    #[test]
    fn typecheck_rejects_user_origin_shadowing_builtin_global() {
        let source = r#"
fn fs_read_text(path: String) -> String:
    return path
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");
        let err = check(&program, &span_mapper, "<test>")
            .expect_err("user wrapper should still shadow builtin global");

        assert!(
            err.to_string()
                .contains("shadows an existing global symbol"),
            "unexpected diagnostic: {err}"
        );
    }

    #[test]
    fn typecheck_rejects_user_origin_shadowing_builtin_type() {
        let source = r#"
struct String:
    value: Int
"#;
        let span_mapper = SpanMapper::new(source);
        let program = parse_source_for_typecheck(source, &span_mapper, "<test>");
        let err = check(&program, &span_mapper, "<test>")
            .expect_err("user type should still shadow builtin type");

        assert!(
            err.to_string().contains("shadows an existing type symbol"),
            "unexpected diagnostic: {err}"
        );
    }

    #[test]
    fn typecheck_resolves_forward_enum_references_in_struct_fields() {
        let span = Span::default();
        let stmt_type = Type::Named {
            name: "Stmt".to_string(),
            generics: vec![],
            span,
        };
        let array_of_stmt = Type::Named {
            name: "Array".to_string(),
            generics: vec![stmt_type.clone()],
            span,
        };
        let block_type = Type::Named {
            name: "Block".to_string(),
            generics: vec![],
            span,
        };

        let program = Program {
            items: vec![
                Item::Struct(Struct {
                    name: "Block".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![Field {
                        name: "stmts".to_string(),
                        ty: array_of_stmt,
                        attributes: vec![],
                        visibility: Visibility::Private,
                        default: None,
                        weak: false,
                        span,
                    }],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Enum(Enum {
                    name: "Stmt".to_string(),
                    generics: vec![],
                    where_clause: None,
                    variants: vec![Variant {
                        name: "Item".to_string(),
                        fields: VariantFields::Unit,
                        span,
                    }],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "walk".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "block".to_string(),
                        ty: block_type,
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::For {
                            binding: Pattern::Binding {
                                name: "stmt".to_string(),
                                mutable: false,
                                span,
                            },
                            iter: Expr::Field {
                                object: Box::new(Expr::Ident("block".to_string(), span)),
                                field: "stmts".to_string(),
                                span,
                            },
                            body: Block {
                                stmts: vec![Stmt::Expr(Expr::Match {
                                    scrutinee: Box::new(Expr::Ident("stmt".to_string(), span)),
                                    arms: vec![MatchArm {
                                        pattern: Pattern::Variant {
                                            enum_name: Some("Stmt".to_string()),
                                            variant: "Item".to_string(),
                                            fields: VariantPatternFields::Unit,
                                            span,
                                        },
                                        guard: None,
                                        body: Expr::None(span),
                                        span,
                                    }],
                                    span,
                                })],
                                span,
                            },
                            span,
                        }],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check(&program, &span_mapper, "<test>").expect("forward enum reference should typecheck");
    }

    #[test]
    fn typecheck_treats_box_as_transparent_wrapper() {
        let span = Span::default();
        let item_type = Type::Named {
            name: "Item".to_string(),
            generics: vec![],
            span,
        };
        let boxed_item_type = Type::Named {
            name: "Box".to_string(),
            generics: vec![item_type.clone()],
            span,
        };
        let program = Program {
            items: vec![
                Item::Enum(Enum {
                    name: "Item".to_string(),
                    generics: vec![],
                    where_clause: None,
                    variants: vec![Variant {
                        name: "Value".to_string(),
                        fields: VariantFields::Unit,
                        span,
                    }],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "item".to_string(),
                        ty: boxed_item_type,
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::Match {
                            scrutinee: Box::new(Expr::Ident("item".to_string(), span)),
                            arms: vec![MatchArm {
                                pattern: Pattern::Variant {
                                    enum_name: Some("Item".to_string()),
                                    variant: "Value".to_string(),
                                    fields: VariantPatternFields::Unit,
                                    span,
                                },
                                guard: None,
                                body: Expr::None(span),
                                span,
                            }],
                            span,
                        })],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check(&program, &span_mapper, "<test>").expect("Box wrapper should be transparent");
    }

    #[test]
    fn typecheck_refreshes_enum_payload_struct_fields_after_registration() {
        let span = Span::default();
        let block_type = Type::Named {
            name: "Block".to_string(),
            generics: vec![],
            span,
        };
        let comptime_block_type = Type::Named {
            name: "ComptimeBlock".to_string(),
            generics: vec![],
            span,
        };

        let program = Program {
            items: vec![
                Item::Enum(Enum {
                    name: "Item".to_string(),
                    generics: vec![],
                    where_clause: None,
                    variants: vec![Variant {
                        name: "Comptime".to_string(),
                        fields: VariantFields::Tuple(vec![comptime_block_type.clone()]),
                        span,
                    }],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Struct(Struct {
                    name: "ComptimeBlock".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![Field {
                        name: "body".to_string(),
                        ty: block_type,
                        attributes: vec![],
                        visibility: Visibility::Private,
                        default: None,
                        weak: false,
                        span,
                    }],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Struct(Struct {
                    name: "Block".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "item".to_string(),
                        ty: Type::Named {
                            name: "Item".to_string(),
                            generics: vec![],
                            span,
                        },
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::Match {
                            scrutinee: Box::new(Expr::Ident("item".to_string(), span)),
                            arms: vec![MatchArm {
                                pattern: Pattern::Variant {
                                    enum_name: Some("Item".to_string()),
                                    variant: "Comptime".to_string(),
                                    fields: VariantPatternFields::Tuple(vec![Pattern::Binding {
                                        name: "comptime".to_string(),
                                        mutable: false,
                                        span,
                                    }]),
                                    span,
                                },
                                guard: None,
                                body: Expr::Field {
                                    object: Box::new(Expr::Ident("comptime".to_string(), span)),
                                    field: "body".to_string(),
                                    span,
                                },
                                span,
                            }],
                            span,
                        })],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check(&program, &span_mapper, "<test>")
            .expect("enum payload structs should refresh to concrete field sets");
    }

    #[test]
    fn typecheck_accepts_option_variant_patterns() {
        let span = Span::default();
        let option_value_type = Type::Option(
            Box::new(Type::Named {
                name: "ComputeMetadata".to_string(),
                generics: vec![],
                span,
            }),
            span,
        );
        let program = Program {
            items: vec![
                Item::Struct(Struct {
                    name: "ComputeMetadata".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "value".to_string(),
                        ty: option_value_type,
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::Match {
                            scrutinee: Box::new(Expr::Ident("value".to_string(), span)),
                            arms: vec![
                                MatchArm {
                                    pattern: Pattern::Variant {
                                        enum_name: None,
                                        variant: "Some".to_string(),
                                        fields: VariantPatternFields::Tuple(vec![
                                            Pattern::Binding {
                                                name: "metadata".to_string(),
                                                mutable: false,
                                                span,
                                            },
                                        ]),
                                        span,
                                    },
                                    guard: None,
                                    body: Expr::None(span),
                                    span,
                                },
                                MatchArm {
                                    pattern: Pattern::Wildcard(span),
                                    guard: None,
                                    body: Expr::None(span),
                                    span,
                                },
                            ],
                            span,
                        })],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check(&program, &span_mapper, "<test>")
            .expect("Option variants should pattern-match like built-in enums");
    }

    #[test]
    fn typecheck_infers_builtin_result_variant_expression_generics() {
        let span = Span::default();
        let program = Program {
            items: vec![Item::Function(Function {
                name: "load".to_string(),
                generics: vec![],
                where_clause: None,
                params: vec![],
                return_type: Some(Type::Result(
                    Box::new(Type::Option(
                        Box::new(Type::Named {
                            name: "Int".to_string(),
                            generics: vec![],
                            span,
                        }),
                        span,
                    )),
                    Box::new(Type::Named {
                        name: "String".to_string(),
                        generics: vec![],
                        span,
                    }),
                    span,
                )),
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Return(
                        Some(Expr::EnumVariant {
                            enum_name: "Result".to_string(),
                            variant: "Ok".to_string(),
                            fields: EnumVariantFields::Tuple(vec![Expr::EnumVariant {
                                enum_name: "Option".to_string(),
                                variant: "Some".to_string(),
                                fields: EnumVariantFields::Tuple(vec![Expr::Int(7, span)]),
                                span,
                            }]),
                            span,
                        }),
                        span,
                    )],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            })],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check(&program, &span_mapper, "<test>")
            .expect("Result/Option variants should infer built-in generic payloads");
    }

    #[test]
    fn typecheck_for_loops_over_borrowed_arrays_as_borrowed_items() {
        let span = Span::default();
        let program = Program {
            items: vec![Item::Function(Function {
                name: "visit".to_string(),
                generics: vec![],
                where_clause: None,
                params: vec![Param {
                    name: "values".to_string(),
                    ty: Type::Ref {
                        mutable: false,
                        inner: Box::new(Type::Array(
                            Box::new(Type::Named {
                                name: "String".to_string(),
                                generics: vec![],
                                span,
                            }),
                            0,
                            span,
                        )),
                        lifetime: None,
                        span,
                    },
                    mutable: false,
                    default: None,
                    span,
                }],
                return_type: None,
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::For {
                        binding: Pattern::Binding {
                            name: "value".to_string(),
                            mutable: false,
                            span,
                        },
                        iter: Expr::Ident("values".to_string(), span),
                        body: Block {
                            stmts: vec![Stmt::Expr(Expr::Call {
                                callee: Box::new(Expr::Ident(
                                    "is_compute_plan_binding".to_string(),
                                    span,
                                )),
                                args: vec![CallArg {
                                    name: None,
                                    value: Expr::Ident("value".to_string(), span),
                                    span,
                                }],
                                span,
                            })],
                            span,
                        },
                        span,
                    }],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            })],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            vec![(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("for loop items from borrowed arrays should stay borrowed");
    }

    #[test]
    fn typecheck_infers_lambda_parameters_from_expected_function_type() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let string_ref = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(ResolvedType::String),
        };
        let expected = ResolvedType::Function {
            params: vec![string_ref.clone()],
            ret: Box::new(ResolvedType::Option(Box::new(string_ref.clone()))),
            effects: EffectSet::new(),
        };
        let lambda = Expr::Lambda {
            params: vec![Param {
                name: "value".to_string(),
                ty: Type::Infer(span),
                mutable: false,
                default: None,
                span,
            }],
            return_type: None,
            body: Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Ident("value".to_string(), span)]),
                span,
            }),
            span,
        };

        let inferred =
            infer_expr_type_with_expected(&mut env, &lambda, None, Some(&expected)).unwrap();

        assert_eq!(inferred, expected);
    }

    #[test]
    fn typecheck_infers_lambda_arguments_for_higher_order_methods() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let wrapper_ty = ResolvedType::Struct("Wrapper".to_string(), HashMap::new());
        env.types.insert("Wrapper".to_string(), wrapper_ty.clone());
        let string_ref = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(ResolvedType::String),
        };
        env.define_method(
            "Wrapper".to_string(),
            "apply".to_string(),
            ResolvedType::Function {
                params: vec![
                    wrapper_ty.clone(),
                    ResolvedType::Function {
                        params: vec![string_ref.clone()],
                        ret: Box::new(ResolvedType::Option(Box::new(string_ref.clone()))),
                        effects: EffectSet::new(),
                    },
                ],
                ret: Box::new(ResolvedType::Unit),
                effects: EffectSet::new(),
            },
        );

        let callback = Expr::Lambda {
            params: vec![Param {
                name: "value".to_string(),
                ty: Type::Infer(span),
                mutable: false,
                default: None,
                span,
            }],
            return_type: None,
            body: Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Ident("value".to_string(), span)]),
                span,
            }),
            span,
        };

        let result = infer_method_call_type(
            &mut env,
            None,
            &wrapper_ty,
            "apply",
            &[CallArg {
                name: None,
                value: callback,
                span,
            }],
            span,
        )
        .expect("higher-order method should typecheck");

        assert_eq!(result, ResolvedType::Unit);
    }

    #[test]
    fn typecheck_borrowed_variant_patterns_bind_borrowed_payloads() {
        let span = Span::default();
        let option_string_ref = Type::Ref {
            mutable: false,
            inner: Box::new(Type::Option(
                Box::new(Type::Named {
                    name: "String".to_string(),
                    generics: vec![],
                    span,
                }),
                span,
            )),
            lifetime: None,
            span,
        };
        let program = Program {
            items: vec![Item::Function(Function {
                name: "inspect".to_string(),
                generics: vec![],
                where_clause: None,
                params: vec![Param {
                    name: "value".to_string(),
                    ty: option_string_ref,
                    mutable: false,
                    default: None,
                    span,
                }],
                return_type: None,
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("value".to_string(), span)),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Variant {
                                    enum_name: None,
                                    variant: "Some".to_string(),
                                    fields: VariantPatternFields::Tuple(vec![Pattern::Binding {
                                        name: "item".to_string(),
                                        mutable: false,
                                        span,
                                    }]),
                                    span,
                                },
                                guard: None,
                                body: Expr::Call {
                                    callee: Box::new(Expr::Ident(
                                        "is_compute_plan_binding".to_string(),
                                        span,
                                    )),
                                    args: vec![CallArg {
                                        name: None,
                                        value: Expr::Ident("item".to_string(), span),
                                        span,
                                    }],
                                    span,
                                },
                                span,
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span),
                                guard: None,
                                body: Expr::Bool(false, span),
                                span,
                            },
                        ],
                        span,
                    })],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            })],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            [(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("borrowed enum payloads should stay borrowed in match bindings");
    }

    #[test]
    fn typecheck_allows_field_access_through_borrowed_structs() {
        let span = Span::default();
        let string_type = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span,
        };
        let program = Program {
            items: vec![
                Item::Struct(Struct {
                    name: "Wrapper".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![Field {
                        name: "name".to_string(),
                        ty: string_type.clone(),
                        visibility: Visibility::Private,
                        attributes: vec![],
                        default: None,
                        weak: false,
                        span,
                    }],
                    methods: vec![],
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "value".to_string(),
                        ty: Type::Ref {
                            mutable: false,
                            inner: Box::new(Type::Named {
                                name: "Wrapper".to_string(),
                                generics: vec![],
                                span,
                            }),
                            lifetime: None,
                            span,
                        },
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::Call {
                            callee: Box::new(Expr::Ident(
                                "is_compute_plan_binding".to_string(),
                                span,
                            )),
                            args: vec![CallArg {
                                name: None,
                                value: Expr::Ref {
                                    mutable: false,
                                    value: Box::new(Expr::Field {
                                        object: Box::new(Expr::Ident("value".to_string(), span)),
                                        field: "name".to_string(),
                                        span,
                                    }),
                                    span,
                                },
                                span,
                            }],
                            span,
                        })],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            [(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("field access through borrowed structs should typecheck");
    }

    #[test]
    fn typecheck_supports_imported_synthetic_tuple_field_access() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define(
            "pair".to_string(),
            ResolvedType::Tuple(vec![ResolvedType::String, ResolvedType::Bool]),
        );

        let field_ty = infer_expr_type(
            &mut env,
            &Expr::Field {
                object: Box::new(Expr::Ident("pair".to_string(), span)),
                field: "__kain_tuple_1".to_string(),
                span,
            },
            None,
        )
        .expect("synthetic tuple field access should resolve to the indexed item");
        assert_eq!(field_ty, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_binds_tuple_pattern_names_even_when_tuple_type_is_unknown() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let pattern = Pattern::Tuple(
            vec![
                Pattern::Binding {
                    name: "left".to_string(),
                    mutable: false,
                    span,
                },
                Pattern::Binding {
                    name: "right".to_string(),
                    mutable: false,
                    span,
                },
            ],
            span,
        );

        bind_pattern_types(&mut env, &pattern, &ResolvedType::Unknown)
            .expect("unknown tuple bindings should still introduce names");
        assert_eq!(env.lookup("left"), Some(&ResolvedType::Unknown));
        assert_eq!(env.lookup("right"), Some(&ResolvedType::Unknown));
    }

    #[test]
    fn typecheck_binds_variant_payload_names_even_when_variant_type_is_unknown() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let pattern = Pattern::Variant {
            enum_name: None,
            variant: "Some".to_string(),
            fields: VariantPatternFields::Tuple(vec![Pattern::Binding {
                name: "width".to_string(),
                mutable: false,
                span,
            }]),
            span,
        };

        bind_pattern_types(&mut env, &pattern, &ResolvedType::Unknown)
            .expect("unknown variant payloads should still introduce names");
        assert_eq!(env.lookup("width"), Some(&ResolvedType::Unknown));
    }

    #[test]
    fn typecheck_binds_struct_variant_fields_by_name_under_borrow() {
        let span = Span::default();
        let program = Program {
            items: vec![
                Item::Enum(Enum {
                    name: "Packet".to_string(),
                    generics: vec![],
                    where_clause: None,
                    variants: vec![Variant {
                        name: "Data".to_string(),
                        fields: VariantFields::Struct(vec![
                            Field {
                                name: "ty".to_string(),
                                ty: Type::Named {
                                    name: "Int".to_string(),
                                    generics: vec![],
                                    span,
                                },
                                attributes: vec![],
                                visibility: Visibility::Private,
                                default: None,
                                weak: false,
                                span,
                            },
                            Field {
                                name: "value".to_string(),
                                ty: Type::Named {
                                    name: "String".to_string(),
                                    generics: vec![],
                                    span,
                                },
                                attributes: vec![],
                                visibility: Visibility::Private,
                                default: None,
                                weak: false,
                                span,
                            },
                        ]),
                        span,
                    }],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
                    where_clause: None,
                    params: vec![Param {
                        name: "packet".to_string(),
                        ty: Type::Ref {
                            mutable: false,
                            inner: Box::new(Type::Named {
                                name: "Packet".to_string(),
                                generics: vec![],
                                span,
                            }),
                            lifetime: None,
                            span,
                        },
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::Match {
                            scrutinee: Box::new(Expr::Ident("packet".to_string(), span)),
                            arms: vec![
                                MatchArm {
                                    pattern: Pattern::Variant {
                                        enum_name: Some("Packet".to_string()),
                                        variant: "Data".to_string(),
                                        fields: VariantPatternFields::Struct(vec![
                                            (
                                                "value".to_string(),
                                                Pattern::Binding {
                                                    name: "item".to_string(),
                                                    mutable: false,
                                                    span,
                                                },
                                            ),
                                            ("ty".to_string(), Pattern::Wildcard(span)),
                                        ]),
                                        span,
                                    },
                                    guard: None,
                                    body: Expr::Call {
                                        callee: Box::new(Expr::Ident(
                                            "is_compute_plan_binding".to_string(),
                                            span,
                                        )),
                                        args: vec![CallArg {
                                            name: None,
                                            value: Expr::Ident("item".to_string(), span),
                                            span,
                                        }],
                                        span,
                                    },
                                    span,
                                },
                                MatchArm {
                                    pattern: Pattern::Wildcard(span),
                                    guard: None,
                                    body: Expr::Bool(false, span),
                                    span,
                                },
                            ],
                            span,
                        })],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }),
            ],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            [(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("borrowed struct-variant bindings should resolve by field name");
    }

    #[test]
    fn typecheck_infers_result_map_callback_types() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let result_ty =
            ResolvedType::Result(Box::new(ResolvedType::String), Box::new(ResolvedType::Bool));
        let callback = Expr::Lambda {
            params: vec![Param {
                name: "value".to_string(),
                ty: Type::Infer(span),
                mutable: false,
                default: None,
                span,
            }],
            return_type: None,
            body: Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Ident("value".to_string(), span)]),
                span,
            }),
            span,
        };

        let mapped = infer_method_call_type(
            &mut env,
            None,
            &result_ty,
            "map",
            &[CallArg {
                name: None,
                value: callback,
                span,
            }],
            span,
        )
        .expect("Result.map should typecheck with inferred callback types");

        assert!(matches!(
            mapped,
            ResolvedType::Result(_, err) if *err == ResolvedType::Bool
        ));
    }

    #[test]
    fn typecheck_treats_empty_tuple_literal_as_unit() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let ty = infer_expr_type(&mut env, &Expr::Tuple(Vec::new(), span), None)
            .expect("empty tuple literal should infer as unit");
        assert_eq!(ty, ResolvedType::Unit);
    }

    #[test]
    fn typecheck_allows_array_contains() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let array_ty = ResolvedType::Array(Box::new(ResolvedType::String), 2);
        let result = infer_method_call_type(
            &mut env,
            None,
            &array_ty,
            "contains",
            &[CallArg {
                name: None,
                value: Expr::String("plan".to_string(), span),
                span,
            }],
            span,
        )
        .expect("Array.contains should typecheck");
        assert_eq!(result, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_allows_array_contains_with_redundant_shared_borrow() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let string_ref = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(ResolvedType::String),
        };
        env.define("name".to_string(), string_ref);
        let array_ty = ResolvedType::Array(Box::new(ResolvedType::String), 2);

        let result = infer_method_call_type(
            &mut env,
            None,
            &array_ty,
            "contains",
            &[CallArg {
                name: None,
                value: Expr::Ref {
                    mutable: false,
                    value: Box::new(Expr::Ident("name".to_string(), span)),
                    span,
                },
                span,
            }],
            span,
        )
        .expect("Array.contains should accept redundant shared borrows");
        assert_eq!(result, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_allows_primitive_to_string_methods() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let stringified_int = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::Int(IntSize::I64),
            "to_string",
            &[],
            span,
        )
        .expect("Int.to_string should typecheck");
        assert_eq!(stringified_int, ResolvedType::String);

        let stringified_string = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "to_string",
            &[],
            span,
        )
        .expect("String.to_string should typecheck");
        assert_eq!(stringified_string, ResolvedType::String);
    }

    #[test]
    fn typecheck_allows_named_enum_to_string_method() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let stringified_error = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::Enum("KainError".to_string(), Vec::new()),
            "to_string",
            &[],
            span,
        )
        .expect("Named enums should support builtin to_string");
        assert_eq!(stringified_error, ResolvedType::String);
    }

    #[test]
    fn typecheck_allows_borrowed_array_as_slice_and_get() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let borrowed_array = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(ResolvedType::Array(Box::new(ResolvedType::String), 2)),
        };
        env.define("items".to_string(), borrowed_array.clone());

        let slice_ty =
            infer_method_call_type(&mut env, None, &borrowed_array, "as_slice", &[], span)
                .expect("borrowed arrays should expose as_slice");
        assert_eq!(
            slice_ty,
            ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::Slice(Box::new(ResolvedType::String))),
            }
        );

        let get_ty = infer_method_call_type(
            &mut env,
            None,
            &slice_ty,
            "get",
            &[CallArg {
                name: None,
                value: Expr::Int(1, span),
                span,
            }],
            span,
        )
        .expect("slices should expose get");
        assert_eq!(
            get_ty,
            ResolvedType::Option(Box::new(ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::String),
            }))
        );

        let index_ty = infer_expr_type(
            &mut env,
            &Expr::Index {
                object: Box::new(Expr::Ident("items".to_string(), span)),
                index: Box::new(Expr::Int(0, span)),
                span,
            },
            None,
        )
        .expect("borrowed arrays should support direct indexing");
        assert_eq!(index_ty, ResolvedType::String);
    }

    #[test]
    fn typecheck_borrowed_slice_patterns_bind_borrowed_items() {
        let span = Span::default();
        let string_slice_ref = Type::Ref {
            mutable: false,
            inner: Box::new(Type::Slice(
                Box::new(Type::Named {
                    name: "String".to_string(),
                    generics: vec![],
                    span,
                }),
                span,
            )),
            lifetime: None,
            span,
        };
        let program = Program {
            items: vec![Item::Function(Function {
                name: "inspect".to_string(),
                generics: vec![],
                where_clause: None,
                params: vec![Param {
                    name: "items".to_string(),
                    ty: string_slice_ref,
                    mutable: false,
                    default: None,
                    span,
                }],
                return_type: None,
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("items".to_string(), span)),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Slice {
                                    patterns: vec![Pattern::Binding {
                                        name: "first".to_string(),
                                        mutable: false,
                                        span,
                                    }],
                                    rest: None,
                                    span,
                                },
                                guard: None,
                                body: Expr::Call {
                                    callee: Box::new(Expr::Ident(
                                        "is_compute_plan_binding".to_string(),
                                        span,
                                    )),
                                    args: vec![CallArg {
                                        name: None,
                                        value: Expr::Ident("first".to_string(), span),
                                        span,
                                    }],
                                    span,
                                },
                                span,
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span),
                                guard: None,
                                body: Expr::Bool(false, span),
                                span,
                            },
                        ],
                        span,
                    })],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            })],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            [(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("borrowed slice patterns should keep borrowed item types");
    }

    #[test]
    fn typecheck_allows_autoref_for_shared_string_arguments() {
        let span = Span::default();
        let program = Program {
            items: vec![Item::Function(Function {
                name: "inspect".to_string(),
                generics: vec![],
                where_clause: None,
                params: vec![],
                return_type: None,
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::Call {
                        callee: Box::new(Expr::Ident("is_compute_plan_binding".to_string(), span)),
                        args: vec![CallArg {
                            name: None,
                            value: Expr::String("dispatch".to_string(), span),
                            span,
                        }],
                        span,
                    })],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            })],
            span,
        };

        let span_mapper = SpanMapper::new("");
        check_with_extra_globals(
            &program,
            &span_mapper,
            "<test>",
            [(
                "is_compute_plan_binding".to_string(),
                ResolvedType::Function {
                    params: vec![ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    }],
                    ret: Box::new(ResolvedType::Bool),
                    effects: EffectSet::new(),
                },
            )],
        )
        .expect("shared-reference arguments should autoref compatible string values");
    }

    #[test]
    fn typecheck_allows_shared_string_arguments_for_owned_string_params() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define(
            "takes_owned".to_string(),
            ResolvedType::Function {
                params: vec![ResolvedType::String],
                ret: Box::new(ResolvedType::Bool),
                effects: EffectSet::new(),
            },
        );
        env.define(
            "borrowed_name".to_string(),
            ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::String),
            },
        );

        let call_ty = infer_expr_type(
            &mut env,
            &Expr::Call {
                callee: Box::new(Expr::Ident("takes_owned".to_string(), span)),
                args: vec![CallArg {
                    name: None,
                    value: Expr::Ident("borrowed_name".to_string(), span),
                    span,
                }],
                span,
            },
            None,
        )
        .expect("shared borrowed strings should satisfy owned string parameters");

        assert_eq!(call_ty, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_allows_array_iter_and_enumerate() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let array_ty = ResolvedType::Array(Box::new(ResolvedType::String), 3);

        let iter_ty =
            infer_method_call_type(&mut env, None, &array_ty, "iter", &[], span).expect("iter");
        assert_eq!(
            iter_ty,
            ResolvedType::Array(
                Box::new(ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(ResolvedType::String),
                }),
                0,
            )
        );

        let enumerate_ty = infer_method_call_type(&mut env, None, &iter_ty, "enumerate", &[], span)
            .expect("enumerate");
        assert_eq!(
            enumerate_ty,
            ResolvedType::Array(
                Box::new(ResolvedType::Tuple(vec![
                    ResolvedType::Int(IntSize::I64),
                    ResolvedType::Ref {
                        mutable: false,
                        inner: Box::new(ResolvedType::String),
                    },
                ])),
                0,
            )
        );
    }

    #[test]
    fn typecheck_allows_iterator_style_array_next_peek_and_peekable() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let array_ty = ResolvedType::Array(Box::new(ResolvedType::String), 0);

        let peekable_ty = infer_method_call_type(&mut env, None, &array_ty, "peekable", &[], span)
            .expect("peekable");
        assert_eq!(peekable_ty, array_ty);

        let next_ty =
            infer_method_call_type(&mut env, None, &array_ty, "next", &[], span).expect("next");
        assert_eq!(
            next_ty,
            ResolvedType::Option(Box::new(ResolvedType::String))
        );

        let peek_ty =
            infer_method_call_type(&mut env, None, &array_ty, "peek", &[], span).expect("peek");
        assert_eq!(
            peek_ty,
            ResolvedType::Option(Box::new(ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::String),
            }))
        );
    }

    #[test]
    fn typecheck_allows_string_char_iteration_helpers_for_selfhost() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let chars_ty =
            infer_method_call_type(&mut env, None, &ResolvedType::String, "chars", &[], span)
                .expect("chars");
        assert_eq!(
            chars_ty,
            ResolvedType::Array(Box::new(ResolvedType::String), 0)
        );

        let indices_ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "char_indices",
            &[],
            span,
        )
        .expect("char_indices");
        assert_eq!(
            indices_ty,
            ResolvedType::Array(
                Box::new(ResolvedType::Tuple(vec![
                    ResolvedType::Int(IntSize::I64),
                    ResolvedType::String,
                ])),
                0,
            )
        );

        let predicate_ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "is_ascii_uppercase",
            &[],
            span,
        )
        .expect("is_ascii_uppercase");
        assert_eq!(predicate_ty, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_allows_numeric_selfhost_helper_methods() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let int_ty = ResolvedType::Int(IntSize::I64);

        let min_ty = infer_method_call_type(
            &mut env,
            None,
            &int_ty,
            "min",
            &[CallArg {
                name: None,
                value: Expr::Int(4, span),
                span,
            }],
            span,
        )
        .expect("min");
        assert_eq!(min_ty, int_ty);

        let saturating_ty = infer_method_call_type(
            &mut env,
            None,
            &int_ty,
            "saturating_sub",
            &[CallArg {
                name: None,
                value: Expr::Int(1, span),
                span,
            }],
            span,
        )
        .expect("saturating_sub");
        assert_eq!(saturating_ty, int_ty);
    }

    #[test]
    fn typecheck_allows_array_binary_search_with_borrowed_needles() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let array_ty = ResolvedType::Array(Box::new(ResolvedType::Int(IntSize::I64)), 0);
        env.define(
            "needle".to_string(),
            ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::Int(IntSize::I64)),
            },
        );

        let result_ty = infer_method_call_type(
            &mut env,
            None,
            &array_ty,
            "binary_search",
            &[CallArg {
                name: None,
                value: Expr::Ident("needle".to_string(), span),
                span,
            }],
            span,
        )
        .expect("binary_search");

        assert_eq!(
            result_ty,
            ResolvedType::Result(
                Box::new(ResolvedType::Int(IntSize::I64)),
                Box::new(ResolvedType::Int(IntSize::I64)),
            )
        );
    }

    #[test]
    fn typecheck_allows_string_push_str_and_repeat() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let push_ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "push_str",
            &[CallArg {
                name: None,
                value: Expr::String("world".to_string(), span),
                span,
            }],
            span,
        )
        .expect("push_str");
        assert_eq!(push_ty, ResolvedType::Unit);

        let repeat_ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "repeat",
            &[CallArg {
                name: None,
                value: Expr::Int(3, span),
                span,
            }],
            span,
        )
        .expect("repeat");
        assert_eq!(repeat_ty, ResolvedType::String);
    }

    #[test]
    fn typecheck_allows_string_and_array_range_indexing() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define("source".to_string(), ResolvedType::String);
        env.define(
            "items".to_string(),
            ResolvedType::Array(Box::new(ResolvedType::String), 0),
        );

        let string_slice_ty = infer_expr_type(
            &mut env,
            &Expr::Index {
                object: Box::new(Expr::Ident("source".to_string(), span)),
                index: Box::new(Expr::Range {
                    start: Some(Box::new(Expr::Int(1, span))),
                    end: Some(Box::new(Expr::Int(3, span))),
                    inclusive: false,
                    span,
                }),
                span,
            },
            None,
        )
        .expect("string range slice");
        assert_eq!(string_slice_ty, ResolvedType::String);

        let array_slice_ty = infer_expr_type(
            &mut env,
            &Expr::Index {
                object: Box::new(Expr::Ident("items".to_string(), span)),
                index: Box::new(Expr::Range {
                    start: Some(Box::new(Expr::Int(0, span))),
                    end: Some(Box::new(Expr::Int(2, span))),
                    inclusive: false,
                    span,
                }),
                span,
            },
            None,
        )
        .expect("array range slice");
        assert_eq!(
            array_slice_ty,
            ResolvedType::Slice(Box::new(ResolvedType::String))
        );
    }

    #[test]
    fn typecheck_collects_sequence_of_results() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let mapped_ty = ResolvedType::Array(
            Box::new(ResolvedType::Result(
                Box::new(ResolvedType::String),
                Box::new(ResolvedType::Bool),
            )),
            0,
        );
        let collected = infer_method_call_type(&mut env, None, &mapped_ty, "collect", &[], span)
            .expect("collect should fold array results");
        assert_eq!(
            collected,
            ResolvedType::Result(
                Box::new(ResolvedType::Array(Box::new(ResolvedType::String), 0)),
                Box::new(ResolvedType::Bool),
            )
        );
    }

    #[test]
    fn typecheck_collects_sequence_of_pairs_into_map() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let mapped_ty = ResolvedType::Array(
            Box::new(ResolvedType::Tuple(vec![
                ResolvedType::String,
                ResolvedType::Unknown,
            ])),
            0,
        );
        let collected = infer_method_call_type(&mut env, None, &mapped_ty, "collect", &[], span)
            .expect("collect should fold tuple pairs into a Map");
        assert_eq!(collected, selfhost_map_type());
    }

    #[test]
    fn typecheck_supports_option_and_result_expectation_helpers() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let option_ty = ResolvedType::Option(Box::new(ResolvedType::String));
        let unwrapped = infer_method_call_type(
            &mut env,
            None,
            &option_ty,
            "unwrap_or_else",
            &[CallArg {
                name: None,
                value: Expr::Lambda {
                    params: vec![],
                    return_type: None,
                    body: Box::new(Expr::String("fallback".to_string(), span)),
                    span,
                },
                span,
            }],
            span,
        )
        .expect("Option.unwrap_or_else should typecheck");
        assert_eq!(unwrapped, ResolvedType::String);

        let array_ty = ResolvedType::Array(Box::new(ResolvedType::String), 0);
        let last_item = infer_method_call_type(&mut env, None, &array_ty, "last", &[], span)
            .expect("Array.last should typecheck");
        assert_eq!(
            last_item,
            ResolvedType::Option(Box::new(ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::String),
            }))
        );

        let chained_option = infer_method_call_type(
            &mut env,
            None,
            &option_ty,
            "or_",
            &[CallArg {
                name: None,
                value: Expr::EnumVariant {
                    enum_name: "Option".to_string(),
                    variant: "Some".to_string(),
                    fields: EnumVariantFields::Tuple(vec![Expr::String(
                        "fallback".to_string(),
                        span,
                    )]),
                    span,
                },
                span,
            }],
            span,
        )
        .expect("Option.or_ should typecheck");
        assert_eq!(chained_option, option_ty);

        let flattened_option = infer_method_call_type(
            &mut env,
            None,
            &option_ty,
            "and_then",
            &[CallArg {
                name: None,
                value: Expr::Lambda {
                    params: vec![Param {
                        name: "value".to_string(),
                        ty: Type::Infer(span),
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    body: Box::new(Expr::EnumVariant {
                        enum_name: "Option".to_string(),
                        variant: "Some".to_string(),
                        fields: EnumVariantFields::Tuple(vec![Expr::Ident(
                            "value".to_string(),
                            span,
                        )]),
                        span,
                    }),
                    span,
                },
                span,
            }],
            span,
        )
        .expect("Option.and_then should flatten callback options");
        assert_eq!(flattened_option, option_ty);

        let copied_option = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::Option(Box::new(ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::Bool),
            })),
            "copied",
            &[],
            span,
        )
        .expect("Option.copied should typecheck");
        assert_eq!(
            copied_option,
            ResolvedType::Option(Box::new(ResolvedType::Bool))
        );

        let filtered_option = infer_method_call_type(
            &mut env,
            None,
            &option_ty,
            "filter",
            &[CallArg {
                name: None,
                value: Expr::Lambda {
                    params: vec![Param {
                        name: "value".to_string(),
                        ty: Type::Infer(span),
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: None,
                    body: Box::new(Expr::Bool(true, span)),
                    span,
                },
                span,
            }],
            span,
        )
        .expect("Option.filter should keep the option shape");
        assert_eq!(filtered_option, option_ty);

        let taken_option = infer_method_call_type(&mut env, None, &option_ty, "take", &[], span)
            .expect("Option.take should keep the option shape");
        assert_eq!(taken_option, option_ty);

        let result_ty =
            ResolvedType::Result(Box::new(ResolvedType::String), Box::new(ResolvedType::Bool));
        let ok_ty =
            infer_method_call_type(&mut env, None, &result_ty, "ok", &[], span).expect("Result.ok");
        assert_eq!(ok_ty, ResolvedType::Option(Box::new(ResolvedType::String)));
    }

    #[test]
    fn typecheck_supports_try_for_option_and_result_in_matching_function_contexts() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let option_ctx = SemanticContext {
            function_name: "unwrap_option".to_string(),
            return_type: ResolvedType::Option(Box::new(ResolvedType::String)),
            effects: EffectSet::new(),
        };
        env.define(
            "maybe_name".to_string(),
            ResolvedType::Option(Box::new(ResolvedType::String)),
        );
        let option_try_ty = infer_expr_type(
            &mut env,
            &Expr::Try(Box::new(Expr::Ident("maybe_name".to_string(), span)), span),
            Some(&option_ctx),
        )
        .expect("Option-based '?' should typecheck inside Option-returning functions");
        assert_eq!(option_try_ty, ResolvedType::String);

        let result_ctx = SemanticContext {
            function_name: "unwrap_result".to_string(),
            return_type: ResolvedType::Result(
                Box::new(ResolvedType::Bool),
                Box::new(ResolvedType::String),
            ),
            effects: EffectSet::new(),
        };
        env.define(
            "parsed".to_string(),
            ResolvedType::Result(
                Box::new(ResolvedType::Int(IntSize::I64)),
                Box::new(ResolvedType::String),
            ),
        );
        let result_try_ty = infer_expr_type(
            &mut env,
            &Expr::Try(Box::new(Expr::Ident("parsed".to_string(), span)), span),
            Some(&result_ctx),
        )
        .expect("Result-based '?' should typecheck inside Result-returning functions");
        assert_eq!(result_try_ty, ResolvedType::Int(IntSize::I64));
    }

    #[test]
    fn typecheck_rejects_try_when_residual_shape_or_error_type_mismatch_return_context() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define(
            "maybe_name".to_string(),
            ResolvedType::Option(Box::new(ResolvedType::String)),
        );
        let result_ctx = SemanticContext {
            function_name: "wrong_shape".to_string(),
            return_type: ResolvedType::Result(
                Box::new(ResolvedType::String),
                Box::new(ResolvedType::String),
            ),
            effects: EffectSet::new(),
        };
        let option_error = infer_expr_type(
            &mut env,
            &Expr::Try(Box::new(Expr::Ident("maybe_name".to_string(), span)), span),
            Some(&result_ctx),
        )
        .expect_err("Option-based '?' should reject Result-returning functions");
        assert!(
            option_error
                .to_string()
                .contains("'?' on Option requires enclosing function to return Option"),
            "unexpected diagnostic: {option_error}"
        );

        env.define(
            "parsed".to_string(),
            ResolvedType::Result(
                Box::new(ResolvedType::Int(IntSize::I64)),
                Box::new(ResolvedType::Bool),
            ),
        );
        let mismatched_result_ctx = SemanticContext {
            function_name: "wrong_error".to_string(),
            return_type: ResolvedType::Result(
                Box::new(ResolvedType::String),
                Box::new(ResolvedType::String),
            ),
            effects: EffectSet::new(),
        };
        let result_error = infer_expr_type(
            &mut env,
            &Expr::Try(Box::new(Expr::Ident("parsed".to_string(), span)), span),
            Some(&mismatched_result_ctx),
        )
        .expect_err("Result-based '?' should validate propagated error types");
        assert!(
            result_error
                .to_string()
                .contains("propagated error expected String, found Bool"),
            "unexpected diagnostic: {result_error}"
        );
    }

    #[test]
    fn typecheck_allows_string_literal_patterns_against_shared_string_refs() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let pattern = Pattern::Literal(Expr::String("gcc".to_string(), span));
        let scrutinee_ty = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(ResolvedType::String),
        };

        check_pattern_compatibility(&mut env, &pattern, &scrutinee_ty)
            .expect("string literals should match shared string refs");
    }

    #[test]
    fn typecheck_allows_explicit_enum_variant_patterns_against_unresolved_named_types() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let pattern = Pattern::Variant {
            enum_name: Some("CompileTarget".to_string()),
            variant: "Ue5".to_string(),
            fields: VariantPatternFields::Unit,
            span,
        };
        let scrutinee_ty = ResolvedType::Struct("CompileTarget".to_string(), HashMap::new());

        check_pattern_compatibility(&mut env, &pattern, &scrutinee_ty)
            .expect("explicit enum variant patterns should match unresolved named enum carriers");
    }

    #[test]
    fn typecheck_allows_comparisons_between_concrete_and_generic_ints() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let comparison = Expr::Binary {
            left: Box::new(Expr::Int(7, span)),
            op: BinaryOp::Le,
            right: Box::new(Expr::Cast {
                value: Box::new(Expr::Int(9, span)),
                target: Type::Named {
                    name: "i64".to_string(),
                    generics: vec![],
                    span,
                },
                span,
            }),
            span,
        };

        let ty =
            infer_expr_type(&mut env, &comparison, None).expect("mixed-width ints should compare");
        assert_eq!(ty, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_allows_redundant_deref_of_owned_enum_values() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define(
            "expr_ref".to_string(),
            ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::Enum("Expr".to_string(), Vec::new())),
            },
        );
        let expr = Expr::Deref(
            Box::new(Expr::Deref(
                Box::new(Expr::Ident("expr_ref".to_string(), span)),
                span,
            )),
            span,
        );

        let ty = infer_expr_type(&mut env, &expr, None)
            .expect("redundant deref over owned enum values should typecheck");
        assert_eq!(ty, ResolvedType::Enum("Expr".to_string(), Vec::new()));
    }

    #[test]
    fn typecheck_registers_selfhost_constructor_helpers() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        for (name, expected_ret) in [
            ("Vec__new_", dynamic_array_type(ResolvedType::Unknown)),
            ("String__new_", ResolvedType::String),
            ("HashMap__new_", selfhost_map_type()),
            ("std__collections__HashMap__new_", selfhost_map_type()),
            ("HashSet__new_", selfhost_set_type()),
            ("std__collections__HashSet__new_", selfhost_set_type()),
        ] {
            let ty = env
                .lookup(name)
                .cloned()
                .expect("selfhost helper should be registered");
            assert_eq!(
                ty,
                ResolvedType::Function {
                    params: Vec::new(),
                    ret: Box::new(expected_ret),
                    effects: EffectSet::new(),
                }
            );
        }

        assert_eq!(
            env.lookup("Box__new_")
                .cloned()
                .expect("box helper should be registered"),
            ResolvedType::Function {
                params: vec![ResolvedType::Unknown],
                ret: Box::new(ResolvedType::Unknown),
                effects: EffectSet::new(),
            }
        );

        assert_eq!(
            env.lookup("Some")
                .cloned()
                .expect("Some helper should be registered"),
            ResolvedType::Function {
                params: vec![ResolvedType::Unknown],
                ret: Box::new(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
                effects: EffectSet::new(),
            }
        );

        assert_eq!(
            env.lookup("None")
                .cloned()
                .expect("None helper should be registered"),
            ResolvedType::Function {
                params: Vec::new(),
                ret: Box::new(ResolvedType::Option(Box::new(ResolvedType::Unknown))),
                effects: EffectSet::new(),
            }
        );
    }

    #[test]
    fn typecheck_registers_builtin_panic_global() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        assert_eq!(
            env.lookup("panic").cloned(),
            Some(ResolvedType::Function {
                params: vec![ResolvedType::String],
                ret: Box::new(ResolvedType::Never),
                effects: EffectSet::new(),
            })
        );
    }

    #[test]
    fn typecheck_registers_filesystem_script_globals() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        for (name, expected) in [
            (
                "file_exists",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
            ),
            (
                "env",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "read_dir",
                builtin_function_type(
                    vec![ResolvedType::String],
                    dynamic_array_type(ResolvedType::String),
                ),
            ),
            (
                "create_dir_all",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
            ),
            (
                "copy_file",
                builtin_function_type(
                    vec![ResolvedType::String, ResolvedType::String],
                    ResolvedType::Unit,
                ),
            ),
            (
                "remove_file",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::Unit),
            ),
            (
                "path_join",
                builtin_function_type(
                    vec![ResolvedType::String, ResolvedType::String],
                    ResolvedType::String,
                ),
            ),
            (
                "path_parent",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "path_file_name",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "path_extension",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "path_stem",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "path_is_file",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
            ),
            (
                "path_is_dir",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::Bool),
            ),
            (
                "fs_read_text",
                builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
            ),
            (
                "fs_try_read_text",
                builtin_function_type(
                    vec![ResolvedType::String],
                    fs_result_type(ResolvedType::String),
                ),
            ),
            (
                "fs_read_bytes",
                builtin_function_type(vec![ResolvedType::String], fs_byte_array_type()),
            ),
            (
                "fs_metadata",
                builtin_function_type(vec![ResolvedType::String], fs_metadata_type()),
            ),
            (
                "fs_read_dir",
                builtin_function_type(
                    vec![ResolvedType::String],
                    dynamic_array_type(fs_dir_entry_type()),
                ),
            ),
            (
                "fs_path_join",
                builtin_function_type(
                    vec![ResolvedType::String, ResolvedType::String],
                    ResolvedType::String,
                ),
            ),
        ] {
            assert_eq!(
                env.lookup(name).cloned(),
                Some(expected),
                "missing filesystem builtin {name}"
            );
        }
    }

    #[test]
    fn typecheck_registers_bootstrap_lexer_intrinsic() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        assert_eq!(
            env.lookup("__kain_bootstrap_lex_tokens").cloned(),
            Some(ResolvedType::Function {
                params: vec![ResolvedType::Ref {
                    mutable: false,
                    inner: Box::new(ResolvedType::String),
                }],
                ret: Box::new(ResolvedType::Result(
                    Box::new(ResolvedType::Array(
                        Box::new(ResolvedType::Struct("Token".to_string(), HashMap::new())),
                        0,
                    )),
                    Box::new(ResolvedType::Enum("KainError".to_string(), Vec::new())),
                )),
                effects: EffectSet::new(),
            })
        );
    }

    #[test]
    fn typecheck_registers_bootstrap_parser_intrinsic() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        assert_eq!(
            env.lookup("__kain_bootstrap_parse_source").cloned(),
            Some(ResolvedType::Function {
                params: vec![
                    dynamic_array_type(ResolvedType::Struct("Token".to_string(), HashMap::new())),
                    ResolvedType::String,
                ],
                ret: Box::new(ResolvedType::Struct("Program".to_string(), HashMap::new())),
                effects: EffectSet::new(),
            })
        );
    }

    #[test]
    fn typecheck_registers_bootstrap_runtime_intrinsic() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        assert_eq!(
            env.lookup("__kain_bootstrap_run_program").cloned(),
            Some(ResolvedType::Function {
                params: vec![ResolvedType::Struct("Program".to_string(), HashMap::new())],
                ret: Box::new(ResolvedType::Enum("Value".to_string(), Vec::new())),
                effects: EffectSet::new(),
            })
        );
    }

    #[test]
    fn typecheck_registers_bootstrap_llvm_intrinsic() {
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        assert_eq!(
            env.lookup("__kain_bootstrap_generate_llvm_ir").cloned(),
            Some(ResolvedType::Function {
                params: vec![ResolvedType::Struct(
                    "TypedProgram".to_string(),
                    HashMap::new()
                )],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            })
        );
    }

    #[test]
    fn typecheck_selfhost_host_bridge_calls_and_path_methods() {
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let span = Span::default();

        let current_dir_call = Expr::Call {
            callee: Box::new(Expr::Ident("std__env__current_dir".to_string(), span)),
            args: Vec::new(),
            span,
        };
        let current_dir_ty = infer_expr_type(&mut env, &current_dir_call, None)
            .expect("current_dir should typecheck");
        assert_eq!(
            current_dir_ty,
            selfhost_host_result_type(selfhost_path_buf_type())
        );

        let path_ty = infer_method_call_type(&mut env, None, &current_dir_ty, "unwrap", &[], span)
            .expect("Result.unwrap should produce a path");
        assert_eq!(path_ty, selfhost_path_buf_type());

        let join_ty = infer_method_call_type(
            &mut env,
            None,
            &path_ty,
            "join",
            &[CallArg {
                name: None,
                value: Expr::String("stdlib".to_string(), span),
                span,
            }],
            span,
        )
        .expect("PathBuf.join should typecheck");
        assert_eq!(join_ty, selfhost_path_buf_type());

        let file_name_ty = infer_method_call_type(&mut env, None, &join_ty, "file_name", &[], span)
            .expect("PathBuf.file_name should typecheck");
        let file_name_inner =
            infer_method_call_type(&mut env, None, &file_name_ty, "unwrap", &[], span)
                .expect("Option.unwrap should produce the file name");
        assert_eq!(file_name_inner, ResolvedType::String);

        let lossy_ty = infer_method_call_type(
            &mut env,
            None,
            &file_name_inner,
            "to_string_lossy",
            &[],
            span,
        )
        .expect("String.to_string_lossy should typecheck");
        assert_eq!(lossy_ty, ResolvedType::String);
    }

    #[test]
    fn typecheck_selfhost_host_bridge_polymorphic_memory_helpers() {
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let span = Span::default();
        let stored_ty = dynamic_array_type(ResolvedType::String);
        env.define("errors".to_string(), stored_ty.clone());

        let take_expr = Expr::Call {
            callee: Box::new(Expr::Ident("std__mem__take".to_string(), span)),
            args: vec![CallArg {
                name: None,
                value: Expr::Ident("errors".to_string(), span),
                span,
            }],
            span,
        };
        let taken_ty = infer_expr_type(&mut env, &take_expr, None)
            .expect("std__mem__take should preserve the stored type");
        assert_eq!(taken_ty, stored_ty);

        let replace_expr = Expr::Call {
            callee: Box::new(Expr::Ident("std__mem__replace".to_string(), span)),
            args: vec![
                CallArg {
                    name: None,
                    value: Expr::Ident("errors".to_string(), span),
                    span,
                },
                CallArg {
                    name: None,
                    value: Expr::Array(vec![Expr::String("new".to_string(), span)], span),
                    span,
                },
            ],
            span,
        };
        let replaced_ty = infer_expr_type(&mut env, &replace_expr, None)
            .expect("std__mem__replace should preserve the replaced type");
        assert_eq!(replaced_ty, stored_ty);
    }

    #[test]
    fn typecheck_registers_static_impl_methods_under_selfhost_and_legacy_names() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let program = Program {
            items: vec![
                Item::Struct(Struct {
                    name: "Env".to_string(),
                    generics: vec![],
                    where_clause: None,
                    fields: vec![],
                    methods: vec![],
                    visibility: Visibility::Public,
                    attributes: vec![],
                    span,
                }),
                Item::Impl(Impl {
                    generics: vec![],
                    where_clause: None,
                    trait_name: None,
                    trait_generics: vec![],
                    target_type: Type::Named {
                        name: "Env".to_string(),
                        generics: vec![],
                        span,
                    },
                    methods: vec![Function {
                        name: "new_".to_string(),
                        generics: vec![],
                        where_clause: None,
                        params: vec![],
                        return_type: Some(Type::Named {
                            name: "Env".to_string(),
                            generics: vec![],
                            span,
                        }),
                        effects: vec![],
                        body: Block {
                            stmts: vec![Stmt::Expr(Expr::Struct {
                                name: "Env".to_string(),
                                fields: vec![],
                                rest: None,
                                span,
                            })],
                            span,
                        },
                        visibility: Visibility::Public,
                        attributes: vec![],
                        span,
                    }],
                    span,
                }),
            ],
            span,
        };

        let typed = check(&program, &span_mapper, "<test>").expect("static impl should typecheck");
        assert_eq!(typed.items.len(), 2);

        let env = TypeEnv::new(&span_mapper, "<test>");
        let mut registration_env = env;
        for item in &program.items {
            predeclare_item_types(&mut registration_env, item);
        }
        for item in &program.items {
            register_item_types(&mut registration_env, item).expect("register item");
        }

        for name in ["Env_new_", "Env__new_"] {
            let ty = registration_env
                .lookup(name)
                .cloned()
                .expect("static impl helper should be registered");
            assert_eq!(
                ty,
                ResolvedType::Function {
                    params: Vec::new(),
                    ret: Box::new(ResolvedType::Struct("Env".to_string(), HashMap::new())),
                    effects: EffectSet::new(),
                }
            );
        }
    }

    #[test]
    fn typecheck_prefers_inline_module_sibling_functions_over_root_globals() {
        let source = r#"
fn eval_block() -> Int:
    7

mod helper_module:
    fn eval_block() -> String:
        "local"

    fn caller() -> String:
        return eval_block()
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        check(&program, &span_mapper, "<test>")
            .expect("inline module functions should resolve sibling helpers lexically");
    }

    #[test]
    fn typecheck_resolves_inline_module_enum_type_paths() {
        let source = r#"
mod lexer:
    pub enum TokenKind:
        Fn

fn describe(kind: lexer::TokenKind) -> String:
    match kind:
        lexer::TokenKind::Fn => "fn"
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        check(&program, &span_mapper, "<test>")
            .expect("inline module enum type paths should resolve to enum shapes");
    }

    #[test]
    fn typecheck_resolves_imported_map_and_set_types() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let env = TypeEnv::new(&span_mapper, "<test>");

        let map_ty = resolve_type_in_env(
            &env,
            &Type::Named {
                name: "Map".to_string(),
                generics: vec![
                    Type::Named {
                        name: "String".to_string(),
                        generics: vec![],
                        span,
                    },
                    Type::Named {
                        name: "Int".to_string(),
                        generics: vec![],
                        span,
                    },
                ],
                span,
            },
        )
        .expect("Map should resolve");
        assert_eq!(map_ty, selfhost_map_type());

        let set_ty = resolve_type_in_env(
            &env,
            &Type::Named {
                name: "Set".to_string(),
                generics: vec![Type::Named {
                    name: "String".to_string(),
                    generics: vec![],
                    span,
                }],
                span,
            },
        )
        .expect("Set should resolve");
        assert_eq!(set_ty, selfhost_set_type());
    }

    #[test]
    fn typecheck_binds_component_props_in_render_scope() {
        let source = r#"
component Hello(name: String):
    render <h1>{name}</h1>
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        check(&program, &span_mapper, "<test>")
            .expect("component render should resolve prop bindings");
    }

    #[test]
    fn typecheck_binds_component_props_and_state_in_component_scope() {
        let source = r#"
component Hello(name: String):
    state label: String = name
    render <h1>{label}</h1>
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        check(&program, &span_mapper, "<test>")
            .expect("component state should resolve against component-local bindings");
    }

    #[test]
    fn typecheck_supports_selfhost_collection_methods() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let map_get_ty = infer_method_call_type(
            &mut env,
            None,
            &selfhost_map_type(),
            "get",
            &[CallArg {
                name: None,
                value: Expr::String("kind".to_string(), span),
                span,
            }],
            span,
        )
        .expect("Map.get should typecheck");
        assert_eq!(
            map_get_ty,
            ResolvedType::Option(Box::new(ResolvedType::Unknown))
        );

        let map_iter_ty =
            infer_method_call_type(&mut env, None, &selfhost_map_type(), "iter", &[], span)
                .expect("Map.iter should typecheck");
        assert_eq!(
            map_iter_ty,
            ResolvedType::Array(
                Box::new(ResolvedType::Tuple(vec![
                    ResolvedType::String,
                    ResolvedType::Unknown
                ])),
                0
            )
        );

        let set_contains_ty = infer_method_call_type(
            &mut env,
            None,
            &selfhost_set_type(),
            "contains",
            &[CallArg {
                name: None,
                value: Expr::String("kind".to_string(), span),
                span,
            }],
            span,
        )
        .expect("Set.contains should typecheck");
        assert_eq!(set_contains_ty, ResolvedType::Bool);

        let set_insert_ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::Ref {
                mutable: true,
                inner: Box::new(selfhost_set_type()),
            },
            "insert",
            &[CallArg {
                name: None,
                value: Expr::String("kind".to_string(), span),
                span,
            }],
            span,
        )
        .expect("Set.insert should typecheck through mutable receivers");
        assert_eq!(set_insert_ty, ResolvedType::Bool);

        let map_is_empty_ty =
            infer_method_call_type(&mut env, None, &selfhost_map_type(), "is_empty", &[], span)
                .expect("Map.is_empty should typecheck");
        assert_eq!(map_is_empty_ty, ResolvedType::Bool);

        let set_is_empty_ty =
            infer_method_call_type(&mut env, None, &selfhost_set_type(), "is_empty", &[], span)
                .expect("Set.is_empty should typecheck");
        assert_eq!(set_is_empty_ty, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_supports_string_push_for_selfhost_emitters() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define("ch".to_string(), ResolvedType::Char);

        let ty = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::String,
            "push",
            &[CallArg {
                name: None,
                value: Expr::Ident("ch".to_string(), span),
                span,
            }],
            span,
        )
        .expect("String.push should typecheck");
        assert_eq!(ty, ResolvedType::Unit);
    }

    #[test]
    fn typecheck_supports_generic_clone_for_borrowed_values() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");

        let cloned = infer_method_call_type(
            &mut env,
            None,
            &ResolvedType::Ref {
                mutable: false,
                inner: Box::new(ResolvedType::String),
            },
            "clone",
            &[],
            span,
        )
        .expect("borrowed values should expose clone");
        assert_eq!(cloned, ResolvedType::String);
    }

    #[test]
    fn typecheck_handles_borrowed_enum_variant_payloads_inside_nested_optional_matches() {
        let source = r#"
enum Item:
    Function(Function)

struct Function:
    name: String

struct Filter:
    custom_filter_method: Option<Function>

fn collect_type_names_from_item(item: &Item, out_: &mut Set<String>):
    ()

fn demo(filter: Option<Filter>, out_: &mut Set<String>):
    match &filter:
        Some(filter) =>
            match &filter.custom_filter_method:
                Some(custom_filter) =>
                    collect_type_names_from_item(&Item::Function(custom_filter.clone()), out_)
                    ()
                _ => ()
            ()
        _ => ()
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        let Item::Function(demo) = &program.items[4] else {
            panic!("expected demo function");
        };
        let Stmt::Expr(Expr::Match { arms, .. }) = &demo.body.stmts[0] else {
            panic!("expected top-level match");
        };
        let Expr::Block(outer_some_body, _) = &arms[0].body else {
            panic!("expected outer Some arm to be a block");
        };
        let Stmt::Expr(Expr::Match {
            arms: inner_arms, ..
        }) = &outer_some_body.stmts[0]
        else {
            panic!("expected nested match in outer Some arm");
        };
        let Expr::Block(inner_some_body, _) = &inner_arms[0].body else {
            panic!("expected inner Some arm to be a block");
        };
        let Stmt::Expr(Expr::Call { callee, args, .. }) = &inner_some_body.stmts[0] else {
            panic!("expected first inner Some statement to be a call");
        };
        assert!(
            matches!(callee.as_ref(), Expr::Ident(name, _) if name == "collect_type_names_from_item")
        );
        assert_eq!(args.len(), 2);
        assert!(matches!(
            &args[0].value,
            Expr::Ref { value, .. }
                if matches!(
                    value.as_ref(),
                    Expr::EnumVariant { enum_name, variant, .. }
                        if enum_name == "Item" && variant == "Function"
                )
        ));
        assert!(matches!(
            inner_some_body.stmts.get(1),
            Some(Stmt::Expr(Expr::Tuple(items, _))) if items.is_empty()
        ));

        check(&program, &span_mapper, "<test>")
            .expect("nested borrowed optional matches should typecheck");
    }

    #[test]
    fn types_compatible_accepts_borrowed_option_views() {
        let widget = ResolvedType::Struct("Widget".to_string(), HashMap::new());
        let owned_option = ResolvedType::Option(Box::new(widget.clone()));
        let borrowed_option = ResolvedType::Option(Box::new(shared_ref_type(widget.clone())));
        let borrowed_owned_option = ResolvedType::Ref {
            mutable: false,
            inner: Box::new(owned_option.clone()),
        };

        assert!(types_compatible(&borrowed_option, &borrowed_owned_option));
        assert!(types_compatible(&borrowed_owned_option, &borrowed_option));
        assert!(!types_compatible(&owned_option, &borrowed_owned_option));
    }

    #[test]
    fn typecheck_uses_final_expression_type_for_block_valued_match_arms() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        env.define("flag".to_string(), ResolvedType::Bool);

        let ty = infer_expr_type(
            &mut env,
            &Expr::Match {
                scrutinee: Box::new(Expr::Ident("flag".to_string(), span)),
                arms: vec![
                    MatchArm {
                        pattern: Pattern::Literal(Expr::Bool(true, span)),
                        guard: None,
                        body: Expr::Block(
                            Block {
                                stmts: vec![
                                    Stmt::Let {
                                        pattern: Pattern::Binding {
                                            name: "rendered".to_string(),
                                            mutable: false,
                                            span,
                                        },
                                        ty: None,
                                        value: Some(Expr::String("ok".to_string(), span)),
                                        span,
                                    },
                                    Stmt::Expr(Expr::Ident("rendered".to_string(), span)),
                                ],
                                span,
                            },
                            span,
                        ),
                        span,
                    },
                    MatchArm {
                        pattern: Pattern::Wildcard(span),
                        guard: None,
                        body: Expr::String("fallback".to_string(), span),
                        span,
                    },
                ],
                span,
            },
            None,
        )
        .expect("block-valued match arms should infer from their final expression");

        assert_eq!(ty, ResolvedType::String);
    }

    #[test]
    fn typecheck_allows_borrowed_receivers_for_shared_self_methods() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let type_value = ResolvedType::Enum("Type".to_string(), Vec::new());
        env.define_method(
            "Type".to_string(),
            "contains_raw_ptr".to_string(),
            ResolvedType::Function {
                params: vec![shared_ref_type(type_value.clone())],
                ret: Box::new(ResolvedType::Bool),
                effects: EffectSet::new(),
            },
        );

        let borrowed_result = infer_method_call_type(
            &mut env,
            None,
            &shared_ref_type(type_value.clone()),
            "contains_raw_ptr",
            &[],
            span,
        )
        .expect("shared-self methods should dispatch through borrowed receivers");
        assert_eq!(borrowed_result, ResolvedType::Bool);

        let owned_result =
            infer_method_call_type(&mut env, None, &type_value, "contains_raw_ptr", &[], span)
                .expect("shared-self methods should also dispatch through owned receivers");
        assert_eq!(owned_result, ResolvedType::Bool);
    }

    #[test]
    fn typecheck_refreshes_stale_struct_field_placeholders_on_access() {
        let span = Span::default();
        let span_mapper = SpanMapper::new("");
        let mut env = TypeEnv::new(&span_mapper, "<test>");
        let mut refreshed_fields = HashMap::new();
        refreshed_fields.insert("ty".to_string(), ResolvedType::String);
        env.types.insert(
            "Param".to_string(),
            ResolvedType::Struct("Param".to_string(), refreshed_fields),
        );

        let field_ty = field_access_type(
            &env,
            &ResolvedType::Struct("Param".to_string(), HashMap::new()),
            "ty",
            span,
        )
        .expect("field access should refresh stale struct placeholders from registered types");
        assert_eq!(field_ty, ResolvedType::String);
    }

    #[test]
    fn duplicate_top_level_functions_report_rich_locations() {
        let source = r#"
fn ping() -> Int:
    return 1

fn ping() -> Int:
    return 2
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        let err =
            check(&program, &span_mapper, "<test>").expect_err("duplicate function should fail");
        let KainError::Rich(report) = err else {
            panic!("expected rich duplicate diagnostic");
        };

        assert_eq!(report.kind, ErrorKind::Type);
        assert_eq!(report.location, Some((5, 1)));
        assert!(report.message.contains("ping"));
        assert!(report.message.contains("collides"));
        assert!(report
            .labels
            .iter()
            .any(|label| label.primary && label.message.contains("redeclared global")));
    }

    #[test]
    fn shadowing_builtin_global_reports_guidance() {
        let source = r#"
fn print() -> Int:
    return 1
"#;

        let tokens = Lexer::new(source).tokenize().expect("source should lex");
        let span_mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("source should parse");

        let err =
            check(&program, &span_mapper, "<test>").expect_err("shadowing builtin should fail");
        let KainError::Rich(report) = err else {
            panic!("expected rich shadowing diagnostic");
        };

        assert_eq!(report.kind, ErrorKind::Type);
        assert_eq!(report.location, Some((2, 1)));
        assert!(report.message.contains("shadows"));
        assert!(report
            .help
            .iter()
            .any(|help| help.contains("distinct name")));
    }
}
