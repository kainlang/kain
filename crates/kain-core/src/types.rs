//! KAIN Type System - Rust-like with effect tracking

use crate::ast::*;
use crate::effects::EffectSet;
use crate::span::Span;
use crate::error::{KainError, KainResult};
use crate::diagnostics::SpanMapper;
use std::collections::HashMap;

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
pub enum TypedItem {
    Function(TypedFunction),
    Component(TypedComponent),
    Shader(TypedShader),
    Actor(TypedActor),
    Struct(TypedStruct),
    Enum(TypedEnum),
    // Trait(TypedTrait), // TODO: Agent 4 will implement trait type checking
    Comptime(TypedComptime),
    Const(TypedConst),
    Macro(TypedMacro),
    Use(TypedUse),
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
}

#[derive(Debug, Clone)]
pub struct TypedFunction {
    pub ast: Function,
    pub resolved_type: ResolvedType,
    pub effects: EffectSet,
}

#[derive(Debug, Clone)]
pub struct TypedComponent {
    pub ast: Component,
    pub prop_types: HashMap<String, ResolvedType>,
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
    Ref { mutable: bool, inner: Box<ResolvedType> },
    Function { params: Vec<ResolvedType>, ret: Box<ResolvedType>, effects: EffectSet },
    Struct(String, HashMap<String, ResolvedType>),
    Enum(String, Vec<(String, ResolvedType)>),
    Generic(String),
    Never,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntSize { I8, I16, I32, I64, I128, Isize, U8, U16, U32, U64, U128, Usize }

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FloatSize { F32, F64 }

/// Type environment for checking
pub struct TypeEnv<'a> {
    scopes: Vec<HashMap<String, ResolvedType>>,
    types: HashMap<String, ResolvedType>,
    span_mapper: &'a SpanMapper,
    filename: &'a str,
}

impl<'a> TypeEnv<'a> {
    pub fn new(span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        let mut env = Self { 
            scopes: vec![HashMap::new()], 
            types: HashMap::new(),
            span_mapper,
            filename,
        };
        // Built-in types
        env.types.insert("Int".into(), ResolvedType::Int(IntSize::I64));
        env.types.insert("Float".into(), ResolvedType::Float(FloatSize::F64));
        env.types.insert("Bool".into(), ResolvedType::Bool);
        env.types.insert("String".into(), ResolvedType::String);
        env.types.insert("Vec2".into(), ResolvedType::Tuple(vec![
            ResolvedType::Float(FloatSize::F32),
            ResolvedType::Float(FloatSize::F32),
        ]));
        env.types.insert("Vec3".into(), ResolvedType::Tuple(vec![
            ResolvedType::Float(FloatSize::F32),
            ResolvedType::Float(FloatSize::F32),
            ResolvedType::Float(FloatSize::F32),
        ]));
        env
    }

    pub fn push_scope(&mut self) { self.scopes.push(HashMap::new()); }
    pub fn pop_scope(&mut self) { self.scopes.pop(); }
    
    pub fn define(&mut self, name: String, ty: ResolvedType) {
        if let Some(scope) = self.scopes.last_mut() {
            scope.insert(name, ty);
        }
    }
    
    pub fn lookup(&self, name: &str) -> Option<&ResolvedType> {
        for scope in self.scopes.iter().rev() {
            if let Some(ty) = scope.get(name) { return Some(ty); }
        }
        self.types.get(name)
    }
    
    /// Create a type error with file:line:col format
    fn type_error(&self, message: impl Into<String>, span: Span) -> KainError {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted_message = format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, message.into());
        KainError::type_error(formatted_message, span)
    }
}

/// Main type checking entry point
pub fn check(program: &Program, span_mapper: &SpanMapper, filename: &str) -> KainResult<TypedProgram> {
    let mut env = TypeEnv::new(span_mapper, filename);
    
    // First pass: Register all struct and enum types
    for item in &program.items {
        match item {
            Item::Struct(s) => {
                let mut fields = HashMap::new();
                for f in &s.fields {
                    fields.insert(f.name.clone(), resolve_type(&f.ty)?);
                }
                env.types.insert(s.name.clone(), ResolvedType::Struct(s.name.clone(), fields));
            }
            Item::Enum(e) => {
                let variants: Vec<(String, ResolvedType)> = e.variants.iter()
                    .map(|v| (v.name.clone(), ResolvedType::Unit))
                    .collect();
                env.types.insert(e.name.clone(), ResolvedType::Enum(e.name.clone(), variants));
            }
            _ => {}
        }
    }
    
    // Second pass: Type check all items
    let mut typed_items = Vec::new();
    for item in &program.items {
        // Skip traits for now - trait type checking not yet implemented
        if matches!(item, Item::Trait(_)) {
            continue;
        }
        typed_items.push(check_item(&mut env, item)?);
    }
    
    Ok(TypedProgram { items: typed_items })
}

fn check_item(env: &mut TypeEnv, item: &Item) -> KainResult<TypedItem> {
    match item {
        Item::Function(f) => Ok(TypedItem::Function(check_function(env, f)?)),
        Item::Struct(s) => Ok(TypedItem::Struct(check_struct(env, s)?)),
        Item::Enum(e) => Ok(TypedItem::Enum(check_enum(env, e)?)),
        Item::Component(c) => Ok(TypedItem::Component(check_component(env, c)?)),
        Item::Shader(s) => Ok(TypedItem::Shader(check_shader(env, s)?)),
        Item::Actor(a) => Ok(TypedItem::Actor(check_actor(env, a)?)),
        Item::Comptime(b) => Ok(TypedItem::Comptime(TypedComptime { ast: b.body.clone() })),
        Item::Const(c) => Ok(TypedItem::Const(check_const(env, c)?)),
        Item::Macro(m) => Ok(TypedItem::Macro(TypedMacro { ast: m.clone() })),
        Item::Use(u) => Ok(TypedItem::Use(TypedUse { ast: u.clone() })),
        Item::Impl(i) => Ok(TypedItem::Impl(TypedImpl { ast: i.clone() })),
        Item::Test(t) => Ok(TypedItem::Test(TypedTest { ast: t.clone() })),
        Item::TypeAlias(ta) => Ok(TypedItem::TypeAlias(TypedTypeAlias { ast: ta.clone() })),
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
        Item::Trait(_) => {
            // This should never be reached because we filter traits in check()
            unreachable!("Traits should be filtered before check_item")
        },
        _ => {
            Err(env.type_error("Item type not yet supported in type checker", item_span(item)))
        }
    }
}

fn check_const(_env: &mut TypeEnv, c: &Const) -> KainResult<TypedConst> {
    let ty = resolve_type(&c.ty)?;
    Ok(TypedConst { ast: c.clone(), ty })
}

fn check_actor(env: &mut TypeEnv, a: &Actor) -> KainResult<TypedActor> {
    let mut state_types = HashMap::new();
    for s in &a.state {
        state_types.insert(s.name.clone(), resolve_type(&s.ty)?);
    }
    
    // Check actor handlers for enum vs struct syntax errors
    for handler in &a.handlers {
        check_block_for_syntax_errors(env, &handler.body)?;
    }
    
    // Check actor methods for enum vs struct syntax errors
    for method in &a.methods {
        check_block_for_syntax_errors(env, &method.body)?;
    }
    
    Ok(TypedActor { ast: a.clone(), state_types })
}

fn check_function(env: &mut TypeEnv, f: &Function) -> KainResult<TypedFunction> {
    env.push_scope();
    let mut param_types = Vec::new();
    for p in &f.params {
        let ty = resolve_type(&p.ty)?;
        env.define(p.name.clone(), ty.clone());
        param_types.push(ty);
    }
    let ret = f.return_type.as_ref().map(|t| resolve_type(t)).transpose()?.unwrap_or(ResolvedType::Unit);
    let effects = EffectSet::from(f.effects.clone());
    
    // Check function body for enum vs struct syntax errors
    check_block_for_syntax_errors(env, &f.body)?;
    
    env.pop_scope();
    
    Ok(TypedFunction {
        ast: f.clone(),
        resolved_type: ResolvedType::Function { params: param_types, ret: Box::new(ret), effects: effects.clone() },
        effects,
    })
}

fn check_struct(env: &mut TypeEnv, s: &Struct) -> KainResult<TypedStruct> {
    let mut fields = HashMap::new();
    for f in &s.fields {
        fields.insert(f.name.clone(), resolve_type(&f.ty)?);
    }
    
    // Check struct methods for enum vs struct syntax errors
    for method in &s.methods {
        check_block_for_syntax_errors(env, &method.body)?;
    }
    
    Ok(TypedStruct { ast: s.clone(), field_types: fields })
}

fn check_enum(_env: &mut TypeEnv, e: &Enum) -> KainResult<TypedEnum> {
    let mut variant_payload_types: HashMap<String, Vec<ResolvedType>> = HashMap::new();

    for v in &e.variants {
        let payload_types = match &v.fields {
            VariantFields::Unit => Vec::new(),
            VariantFields::Tuple(items) => items.iter().map(resolve_type).collect::<Result<Vec<_>, _>>()?,
            VariantFields::Struct(fields) => fields.iter().map(|f| resolve_type(&f.ty)).collect::<Result<Vec<_>, _>>()?,
        };
        variant_payload_types.insert(v.name.clone(), payload_types);
    }

    Ok(TypedEnum {
        ast: e.clone(),
        variant_payload_types,
    })
}

fn check_component(_env: &mut TypeEnv, c: &Component) -> KainResult<TypedComponent> {
    let mut props = HashMap::new();
    for p in &c.props {
        props.insert(p.name.clone(), resolve_type(&p.ty)?);
    }
    Ok(TypedComponent { ast: c.clone(), prop_types: props })
}

fn check_shader(_env: &mut TypeEnv, s: &Shader) -> KainResult<TypedShader> {
    let inputs: Vec<_> = s.inputs.iter().map(|p| resolve_type(&p.ty)).collect::<Result<_, _>>()?;
    let output = resolve_type(&s.outputs)?;
    Ok(TypedShader { ast: s.clone(), input_types: inputs, output_type: output })
}

pub fn resolve_type(ty: &Type) -> KainResult<ResolvedType> {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Int" => Ok(ResolvedType::Int(IntSize::I64)),
            "Float" => Ok(ResolvedType::Float(FloatSize::F64)),
            "Bool" => Ok(ResolvedType::Bool),
            "String" => Ok(ResolvedType::String),
            _ => {
                // Check if this is a generic type parameter (single uppercase letter or _T style)
                if name.len() == 1 && name.chars().next().map(|c| c.is_uppercase()).unwrap_or(false) {
                    Ok(ResolvedType::Generic(name.clone()))
                } else if name.starts_with('_') && name.len() > 1 {
                    // _T, _Item, etc are also generic
                    Ok(ResolvedType::Generic(name.clone()))
                } else {
                    // Assume it's a struct
                    Ok(ResolvedType::Struct(name.clone(), HashMap::new()))
                }
            }
        },
        Type::Unit(_) => Ok(ResolvedType::Unit),
        Type::Never(_) => Ok(ResolvedType::Never),
        Type::Tuple(inner, _) => Ok(ResolvedType::Tuple(inner.iter().map(resolve_type).collect::<Result<_, _>>()?)),
        Type::Function { params, return_type, effects, .. } => {
            let resolved_params = params.iter().map(resolve_type).collect::<Result<Vec<_>, _>>()?;
            let resolved_ret = resolve_type(return_type)?;
            Ok(ResolvedType::Function {
                params: resolved_params,
                ret: Box::new(resolved_ret),
                effects: EffectSet::from(effects.clone()),
            })
        }
        _ => Ok(ResolvedType::Unknown),
    }
}

fn item_span(item: &Item) -> Span {
    match item {
        Item::Function(f) => f.span,
        Item::Struct(s) => s.span,
        Item::Enum(e) => e.span,
        Item::Component(c) => c.span,
        Item::Shader(s) => s.span,
        Item::Actor(a) => a.span,
        Item::Comptime(b) => b.span,
        Item::Const(c) => c.span,
        Item::Macro(m) => m.span,
        Item::Use(u) => u.span,
        Item::Impl(i) => i.span,
        Item::Test(t) => t.span,
        _ => Span::new(0, 0),
    }
}

impl From<Vec<crate::effects::Effect>> for EffectSet {
    fn from(v: Vec<crate::effects::Effect>) -> Self {
        let mut s = EffectSet::new();
        for e in v { s.effects.insert(e); }
        s
    }
}

/// Check a block for enum vs struct syntax errors
fn check_block_for_syntax_errors(env: &TypeEnv, block: &Block) -> KainResult<()> {
    for stmt in &block.stmts {
        check_stmt_for_syntax_errors(env, stmt)?;
    }
    Ok(())
}

/// Check a statement for enum vs struct syntax errors
fn check_stmt_for_syntax_errors(env: &TypeEnv, stmt: &Stmt) -> KainResult<()> {
    match stmt {
        Stmt::Let { value, .. } => {
            if let Some(expr) = value {
                check_expr_for_syntax_errors(env, expr)?;
            }
        }
        Stmt::Expr(expr) => {
            check_expr_for_syntax_errors(env, expr)?;
        }
        Stmt::Return(Some(expr), _) => {
            check_expr_for_syntax_errors(env, expr)?;
        }
        Stmt::While { condition, body, .. } => {
            check_expr_for_syntax_errors(env, condition)?;
            check_block_for_syntax_errors(env, body)?;
        }
        Stmt::For { iter, body, .. } => {
            check_expr_for_syntax_errors(env, iter)?;
            check_block_for_syntax_errors(env, body)?;
        }
        Stmt::Loop { body, .. } => {
            check_block_for_syntax_errors(env, body)?;
        }
        _ => {}
    }
    Ok(())
}

/// Check an expression for enum vs struct syntax errors
fn check_expr_for_syntax_errors(env: &TypeEnv, expr: &Expr) -> KainResult<()> {
    match expr {
        Expr::EnumVariant { enum_name, variant, span, .. } => {
            // Check if enum_name refers to a struct type
            if let Some(ty) = env.types.get(enum_name) {
                if matches!(ty, ResolvedType::Struct(..)) {
                    return Err(env.type_error(
                        format!(
                            "Cannot use '::' on struct type '{}'. Use '.' for field access instead.\nExample: {}.{} (not {}::{})",
                            enum_name, enum_name.to_lowercase(), variant, enum_name, variant
                        ),
                        *span
                    ));
                }
            }
        }
        Expr::Binary { left, right, .. } => {
            check_expr_for_syntax_errors(env, left)?;
            check_expr_for_syntax_errors(env, right)?;
        }
        Expr::Unary { operand, .. } => {
            check_expr_for_syntax_errors(env, operand)?;
        }
        Expr::Call { args, .. } => {
            for arg in args {
                check_expr_for_syntax_errors(env, &arg.value)?;
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            check_expr_for_syntax_errors(env, receiver)?;
            for arg in args {
                check_expr_for_syntax_errors(env, &arg.value)?;
            }
        }
        Expr::Field { object, .. } => {
            check_expr_for_syntax_errors(env, object)?;
        }
        Expr::Index { object, index, .. } => {
            check_expr_for_syntax_errors(env, object)?;
            check_expr_for_syntax_errors(env, index)?;
        }
        Expr::Assign { target, value, .. } => {
            check_expr_for_syntax_errors(env, target)?;
            check_expr_for_syntax_errors(env, value)?;
        }
        Expr::Array(exprs, _) => {
            for e in exprs {
                check_expr_for_syntax_errors(env, e)?;
            }
        }
        Expr::Tuple(exprs, _) => {
            for e in exprs {
                check_expr_for_syntax_errors(env, e)?;
            }
        }
        Expr::If { condition, then_branch, else_branch, .. } => {
            check_expr_for_syntax_errors(env, condition)?;
            check_block_for_syntax_errors(env, then_branch)?;
            if let Some(else_b) = else_branch {
                match else_b.as_ref() {
                    ElseBranch::Else(block) => check_block_for_syntax_errors(env, block)?,
                    ElseBranch::ElseIf(cond, block, next_else) => {
                        check_expr_for_syntax_errors(env, cond)?;
                        check_block_for_syntax_errors(env, block)?;
                        if let Some(next) = next_else {
                            match next.as_ref() {
                                ElseBranch::Else(b) => check_block_for_syntax_errors(env, b)?,
                                ElseBranch::ElseIf(c, b, _) => {
                                    check_expr_for_syntax_errors(env, c)?;
                                    check_block_for_syntax_errors(env, b)?;
                                }
                            }
                        }
                    }
                }
            }
        }
        Expr::Match { scrutinee, arms, .. } => {
            check_expr_for_syntax_errors(env, scrutinee)?;
            for arm in arms {
                check_expr_for_syntax_errors(env, &arm.body)?;
            }
        }
        Expr::Block(block, _) => {
            check_block_for_syntax_errors(env, block)?;
        }
        Expr::Cast { value, .. } => {
            check_expr_for_syntax_errors(env, value)?;
        }
        Expr::Range { start, end, .. } => {
            if let Some(s) = start {
                check_expr_for_syntax_errors(env, s)?;
            }
            if let Some(e) = end {
                check_expr_for_syntax_errors(env, e)?;
            }
        }
        Expr::Struct { fields, .. } => {
            for (_, field_expr) in fields {
                check_expr_for_syntax_errors(env, field_expr)?;
            }
        }
        Expr::Lambda { body, .. } => {
            check_expr_for_syntax_errors(env, body)?;
        }
        Expr::Ref { value, .. } => {
            check_expr_for_syntax_errors(env, value)?;
        }
        Expr::Deref(inner, _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::Try(inner, _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::Await(inner, _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::Spawn { init, .. } => {
            for (_, init_expr) in init {
                check_expr_for_syntax_errors(env, init_expr)?;
            }
        }
        Expr::SendMsg { target, data, .. } => {
            check_expr_for_syntax_errors(env, target)?;
            for (_, data_expr) in data {
                check_expr_for_syntax_errors(env, data_expr)?;
            }
        }
        Expr::Comptime(inner, _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::MacroCall { args, .. } => {
            for arg in args {
                check_expr_for_syntax_errors(env, arg)?;
            }
        }
        Expr::Return(Some(inner), _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::Break(Some(inner), _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        Expr::Paren(inner, _) => {
            check_expr_for_syntax_errors(env, inner)?;
        }
        _ => {}
    }
    Ok(())
}