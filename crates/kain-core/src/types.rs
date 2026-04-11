//! KAIN Type System - Rust-like with effect tracking

use crate::ast::*;
use crate::diagnostics::SpanMapper;
use crate::effects::{check_effect_call, EffectSet};
use crate::error::{KainError, KainResult};
use crate::span::Span;
use std::collections::{HashMap, HashSet};

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
pub struct TypedMod {
    pub ast: Mod,
    pub items: Vec<TypedItem>,
}

#[derive(Debug, Clone)]
pub enum TypedItem {
    Function(TypedFunction),
    Patch(TypedPatch),
    Law(TypedLaw),
    Converge(TypedConverge),
    World(TypedWorld),
    Orchestrate(TypedOrchestrate),
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
}

#[derive(Debug, Clone)]
pub struct TypedOrchestrate {
    pub ast: OrchestrateDef,
    pub resolved_type: ResolvedType,
    pub stages: Vec<OrchestrateStageDescriptor>,
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

/// Type environment for checking
pub struct TypeEnv<'a> {
    scopes: Vec<HashMap<String, ResolvedType>>,
    types: HashMap<String, ResolvedType>,
    globals: HashMap<String, ResolvedType>,
    methods: HashMap<String, HashMap<String, ResolvedType>>,
    enum_variants: HashMap<String, HashMap<String, Vec<ResolvedType>>>,
    span_mapper: &'a SpanMapper,
    filename: &'a str,
}

impl<'a> TypeEnv<'a> {
    pub fn new(span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        let mut env = Self {
            scopes: vec![HashMap::new()],
            types: HashMap::new(),
            globals: HashMap::new(),
            methods: HashMap::new(),
            enum_variants: HashMap::new(),
            span_mapper,
            filename,
        };
        // Built-in types
        env.types
            .insert("Int".into(), ResolvedType::Int(IntSize::I64));
        env.types
            .insert("UInt".into(), ResolvedType::Int(IntSize::U64));
        env.types
            .insert("Float".into(), ResolvedType::Float(FloatSize::F64));
        env.types.insert("Void".into(), ResolvedType::Unit);
        env.types.insert("Bool".into(), ResolvedType::Bool);
        env.types.insert("Char".into(), ResolvedType::Char);
        env.types.insert("String".into(), ResolvedType::String);
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
        env.define_global(
            "println".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::Unknown],
                ret: Box::new(ResolvedType::Unit),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "read_file".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::String],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "len".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::Unknown],
                ret: Box::new(ResolvedType::Int(IntSize::I64)),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "push".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::Unknown, ResolvedType::Unknown],
                ret: Box::new(ResolvedType::Unit),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "char_at".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::String, ResolvedType::Int(IntSize::I64)],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "ord".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::String],
                ret: Box::new(ResolvedType::Int(IntSize::I64)),
                effects: EffectSet::new(),
            },
        );
        env.define_global(
            "chr".into(),
            ResolvedType::Function {
                params: vec![ResolvedType::Int(IntSize::I64)],
                ret: Box::new(ResolvedType::String),
                effects: EffectSet::new(),
            },
        );
        env
    }

    pub fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
    }

    pub fn pop_scope(&mut self) {
        self.scopes.pop();
    }

    pub fn define(&mut self, name: String, ty: ResolvedType) {
        if let Some(scope) = self.scopes.last_mut() {
            scope.insert(name, ty);
        }
    }

    pub fn define_global(&mut self, name: String, ty: ResolvedType) {
        self.globals.insert(name, ty);
    }

    pub fn define_method(&mut self, type_name: String, method_name: String, ty: ResolvedType) {
        self.methods
            .entry(type_name)
            .or_default()
            .insert(method_name, ty);
    }

    pub fn lookup(&self, name: &str) -> Option<&ResolvedType> {
        for scope in self.scopes.iter().rev() {
            if let Some(ty) = scope.get(name) {
                return Some(ty);
            }
        }
        self.globals.get(name).or_else(|| self.types.get(name))
    }

    pub fn lookup_type(&self, name: &str) -> Option<&ResolvedType> {
        self.types.get(name)
    }

    pub fn lookup_method(&self, type_name: &str, method_name: &str) -> Option<&ResolvedType> {
        self.methods
            .get(type_name)
            .and_then(|methods| methods.get(method_name))
    }

    pub fn lookup_enum_variant_fields(
        &self,
        enum_name: &str,
        variant_name: &str,
    ) -> Option<&Vec<ResolvedType>> {
        self.enum_variants
            .get(enum_name)
            .and_then(|variants| variants.get(variant_name))
    }

    /// Create a type error with file:line:col format
    fn type_error(&self, message: impl Into<String>, span: Span) -> KainError {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted_message =
            format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, message.into());
        KainError::type_error(formatted_message, span)
    }
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
    for (name, ty) in extra_globals {
        env.define_global(name, ty);
    }

    // First pass: Register types, globals, and methods.
    for item in &program.items {
        register_item_types(&mut env, item)?;
    }

    // Second pass: Type check all items.
    let mut typed_items = Vec::new();
    for item in &program.items {
        check_item_into(&mut env, item, &mut typed_items)?;
    }

    Ok(TypedProgram { items: typed_items })
}

fn register_item_types(env: &mut TypeEnv, item: &Item) -> KainResult<()> {
    match item {
        Item::Struct(s) => {
            let mut fields = HashMap::new();
            for f in &s.fields {
                fields.insert(f.name.clone(), resolve_type_in_env(env, &f.ty)?);
            }
            let self_ty = ResolvedType::Struct(s.name.clone(), fields.clone());
            env.types.insert(s.name.clone(), self_ty.clone());
            register_method_signatures(env, &s.name, &self_ty, &s.methods)?;
        }
        Item::Enum(e) => {
            let mut variants = Vec::new();
            let mut variant_map = HashMap::new();
            for v in &e.variants {
                let payload_types = match &v.fields {
                    VariantFields::Unit => Vec::new(),
                    VariantFields::Tuple(items) => items
                        .iter()
                        .map(|ty| resolve_type_in_env(env, ty))
                        .collect::<Result<Vec<_>, _>>()?,
                    VariantFields::Struct(fields) => fields
                        .iter()
                        .map(|field| resolve_type_in_env(env, &field.ty))
                        .collect::<Result<Vec<_>, _>>()?,
                };
                variants.push((v.name.clone(), ResolvedType::Unit));
                variant_map.insert(v.name.clone(), payload_types);
            }
            env.types
                .insert(e.name.clone(), ResolvedType::Enum(e.name.clone(), variants));
            env.enum_variants.insert(e.name.clone(), variant_map);
        }
        Item::Function(f) => {
            env.define_global(f.name.clone(), function_signature(env, f, None)?);
        }
        Item::Patch(patch) => {
            env.define_global(
                patch.name.clone(),
                function_signature(env, &patch_function_view(patch), None)?,
            );
        }
        Item::Law(law) => {
            env.define_global(
                law.name.clone(),
                function_signature(env, &law_function_view(law), None)?,
            );
        }
        Item::Converge(converge) => {
            env.define_global(
                converge.name.clone(),
                function_signature(env, &converge_dispatcher_view(converge), None)?,
            );
        }
        Item::World(world) => {
            let mut fields = HashMap::new();
            for state in &world.states {
                fields.insert(state.name.clone(), resolve_type_in_env(env, &state.ty)?);
            }
            let world_ty = ResolvedType::Struct(world.name.clone(), fields);
            env.types.insert(world.name.clone(), world_ty.clone());
            env.define_global(world.name.clone(), world_ty);
        }
        Item::Orchestrate(orchestrate) => {
            env.define_global(
                orchestrate.name.clone(),
                function_signature(env, &orchestrate_function_view(orchestrate), None)?,
            );
        }
        Item::Const(c) => {
            env.define_global(c.name.clone(), resolve_type_in_env(env, &c.ty)?);
        }
        Item::TypeAlias(alias) => {
            env.types
                .insert(alias.name.clone(), resolve_type_in_env(env, &alias.target)?);
        }
        Item::Impl(imp) => {
            let self_ty = resolve_type_in_env(env, &imp.target_type)?;
            if let Some(target_name) = resolved_type_name(&self_ty) {
                register_method_signatures(env, target_name, &self_ty, &imp.methods)?;
            }
        }
        Item::Component(component) => {
            let component_ty = ResolvedType::Struct(component.name.clone(), HashMap::new());
            env.types
                .insert(component.name.clone(), component_ty.clone());
            register_method_signatures(env, &component.name, &component_ty, &component.methods)?;
        }
        Item::Actor(actor) => {
            let actor_ty = ResolvedType::Struct(actor.name.clone(), HashMap::new());
            env.types.insert(actor.name.clone(), actor_ty.clone());
            register_method_signatures(env, &actor.name, &actor_ty, &actor.methods)?;
        }
        Item::Mod(module) => {
            if let Some(children) = &module.inline {
                for child in children {
                    register_item_types(env, child)?;
                }
            }
        }
        _ => {}
    }

    Ok(())
}

fn register_method_signatures(
    env: &mut TypeEnv,
    type_name: &str,
    self_ty: &ResolvedType,
    methods: &[Function],
) -> KainResult<()> {
    for method in methods {
        let signature = function_signature(env, method, Some(self_ty))?;
        env.define_method(type_name.to_string(), method.name.clone(), signature);
    }
    Ok(())
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
        Item::Converge(converge) => Ok(TypedItem::Converge(check_converge(env, converge)?)),
        Item::World(world) => Ok(TypedItem::World(check_world(env, world)?)),
        Item::Orchestrate(orchestrate) => {
            Ok(TypedItem::Orchestrate(check_orchestrate(env, orchestrate)?))
        }
        Item::Struct(s) => Ok(TypedItem::Struct(check_struct(env, s)?)),
        Item::Enum(e) => Ok(TypedItem::Enum(check_enum(env, e)?)),
        Item::Trait(t) => Ok(TypedItem::Trait(TypedTrait { ast: t.clone() })),
        Item::Component(c) => Ok(TypedItem::Component(check_component(env, c)?)),
        Item::Shader(s) => Ok(TypedItem::Shader(check_shader(env, s)?)),
        Item::Actor(a) => Ok(TypedItem::Actor(check_actor(env, a)?)),
        Item::Comptime(b) => Ok(TypedItem::Comptime(TypedComptime {
            ast: b.body.clone(),
        })),
        Item::Const(c) => Ok(TypedItem::Const(check_const(env, c)?)),
        Item::Macro(m) => Ok(TypedItem::Macro(TypedMacro { ast: m.clone() })),
        Item::Use(u) => Ok(TypedItem::Use(TypedUse { ast: u.clone() })),
        Item::Mod(module) => Ok(TypedItem::Mod(check_mod(env, module)?)),
        Item::Impl(i) => Ok(TypedItem::Impl(check_impl(env, i)?)),
        Item::Test(t) => Ok(TypedItem::Test(check_test(env, t)?)),
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
        _ => Err(env.type_error(
            "Item type not yet supported in type checker",
            item_span(item),
        )),
    }
}

fn check_mod(env: &mut TypeEnv, module: &Mod) -> KainResult<TypedMod> {
    let mut items = Vec::new();
    if let Some(children) = &module.inline {
        for child in children {
            check_item_into(env, child, &mut items)?;
        }
    }
    Ok(TypedMod {
        ast: module.clone(),
        items,
    })
}

fn check_const(env: &mut TypeEnv, c: &Const) -> KainResult<TypedConst> {
    let ty = resolve_type_in_env(env, &c.ty)?;
    let value_ty = infer_expr_type(env, &c.value, None)?;
    ensure_type_compatible(env, &ty, &value_ty, c.value.span(), "const value")?;
    Ok(TypedConst { ast: c.clone(), ty })
}

fn check_actor(env: &mut TypeEnv, a: &Actor) -> KainResult<TypedActor> {
    let mut state_types = HashMap::new();
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
        state_types.insert(s.name.clone(), ty);
    }

    let self_ty = ResolvedType::Struct(a.name.clone(), state_types.clone());
    for handler in &a.handlers {
        let handler_return = ResolvedType::Unit;
        let ctx = SemanticContext {
            function_name: format!("{}_{}", a.name, handler.message_type),
            return_type: handler_return,
            effects: EffectSet::new(),
        };
        env.push_scope();
        env.define("self".to_string(), self_ty.clone());
        for param in &handler.params {
            let ty = resolve_param_type(env, param, Some(&self_ty))?;
            env.define(param.name.clone(), ty);
        }
        check_block_semantics(env, &handler.body, &ctx)?;
        env.pop_scope();
    }

    for method in &a.methods {
        check_function_with_self(env, method, &self_ty)?;
    }

    Ok(TypedActor {
        ast: a.clone(),
        state_types,
    })
}

fn check_test(env: &mut TypeEnv, t: &TestDef) -> KainResult<TypedTest> {
    let ctx = SemanticContext {
        function_name: format!("test::{}", t.name),
        return_type: ResolvedType::Unit,
        effects: EffectSet::new(),
    };
    env.push_scope();
    check_block_semantics(env, &t.body, &ctx)?;
    env.pop_scope();
    Ok(TypedTest { ast: t.clone() })
}

fn check_impl(env: &mut TypeEnv, imp: &Impl) -> KainResult<TypedImpl> {
    let self_ty = resolve_type_in_env(env, &imp.target_type)?;
    for method in &imp.methods {
        check_function_with_self(env, method, &self_ty)?;
    }
    Ok(TypedImpl { ast: imp.clone() })
}

fn check_function(env: &mut TypeEnv, f: &Function) -> KainResult<TypedFunction> {
    let resolved_type = function_signature(env, f, None)?;
    let effects = match &resolved_type {
        ResolvedType::Function { effects, .. } => effects.clone(),
        _ => EffectSet::new(),
    };
    let ret = match &resolved_type {
        ResolvedType::Function { ret, .. } => ret.as_ref().clone(),
        _ => ResolvedType::Unit,
    };

    let ctx = SemanticContext {
        function_name: f.name.clone(),
        return_type: ret,
        effects: effects.clone(),
    };

    env.push_scope();
    for p in &f.params {
        let ty = resolve_param_type(env, p, None)?;
        env.define(p.name.clone(), ty);
    }
    check_block_semantics(env, &f.body, &ctx)?;
    env.pop_scope();

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
    let resolved_type = function_signature(env, f, Some(self_ty))?;
    let effects = match &resolved_type {
        ResolvedType::Function { effects, .. } => effects.clone(),
        _ => EffectSet::new(),
    };
    let ret = match &resolved_type {
        ResolvedType::Function { ret, .. } => ret.as_ref().clone(),
        _ => ResolvedType::Unit,
    };
    let ctx = SemanticContext {
        function_name: f.name.clone(),
        return_type: ret,
        effects: effects.clone(),
    };

    env.push_scope();
    for p in &f.params {
        let ty = resolve_param_type(env, p, Some(self_ty))?;
        env.define(p.name.clone(), ty);
    }
    check_block_semantics(env, &f.body, &ctx)?;
    env.pop_scope();

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
}

fn check_law(env: &mut TypeEnv, law: &LawDef) -> KainResult<TypedLaw> {
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
}

fn check_converge(env: &mut TypeEnv, converge: &ConvergeDef) -> KainResult<TypedConverge> {
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
            return Err(env.type_error(
                format!(
                    "Converge lane '{}' does not match dispatcher signature",
                    lane.lane_name
                ),
                lane.span,
            ));
        }
    }
    Ok(TypedConverge {
        ast: converge.clone(),
        resolved_type,
    })
}

fn check_world(env: &mut TypeEnv, world: &WorldDef) -> KainResult<TypedWorld> {
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
    if world.surfaces.is_empty() {
        return Err(env.type_error(
            format!("world '{}' must declare at least one surface", world.name),
            world.span,
        ));
    }
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
}

fn check_orchestrate(
    env: &mut TypeEnv,
    orchestrate: &OrchestrateDef,
) -> KainResult<TypedOrchestrate> {
    let typed_fn = check_function(env, &orchestrate_function_view(orchestrate))?;
    let stages = collect_orchestrate_stage_descriptors(env, orchestrate)?;
    Ok(TypedOrchestrate {
        ast: orchestrate.clone(),
        resolved_type: typed_fn.resolved_type,
        stages,
    })
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
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            if let Some(value) = value {
                collect_patch_mutation_paths_from_expr(value, output);
            }
        }
        Stmt::For { iter, body, .. } => {
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
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            collect_patch_mutation_paths_from_expr(pointer, output);
            collect_patch_mutation_paths_from_expr(offset, output);
        }
        Expr::MemLoad { pointer, .. }
        | Expr::Cast { value: pointer, .. }
        | Expr::Comptime(pointer, _) => collect_patch_mutation_paths_from_expr(pointer, output),
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => {
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
        Stmt::While {
            condition, body, ..
        } => {
            expr_requires_best_effort_patch_mode(condition)
                || body.stmts.iter().any(stmt_requires_best_effort_patch_mode)
        }
        Stmt::Loop { body, .. } => body.stmts.iter().any(stmt_requires_best_effort_patch_mode),
        Stmt::Item(_) | Stmt::Continue(_) => false,
    }
}

fn expr_requires_best_effort_patch_mode(expr: &Expr) -> bool {
    match expr {
        Expr::Call { .. }
        | Expr::MethodCall { .. }
        | Expr::StageCall { .. }
        | Expr::Spawn { .. }
        | Expr::SendMsg { .. }
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
        Expr::MemLoad { pointer, .. } => expr_requires_best_effort_patch_mode(pointer),
        Expr::MemStore { pointer, value, .. } => {
            expr_requires_best_effort_patch_mode(pointer)
                || expr_requires_best_effort_patch_mode(value)
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
                ty: Some(_),
                value:
                    Some(Expr::StageCall {
                        runtime, function, ..
                    }),
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
                });
            }
            Stmt::Let {
                value: Some(Expr::StageCall { span, .. }),
                ..
            } => {
                return Err(env.type_error(
                    format!(
                        "orchestrate '{}' stage steps must be top-level 'let binding: Type = <runtime> function(...)' declarations",
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
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            value.as_ref().and_then(first_stage_call_in_expr)
        }
        Stmt::For { iter, body, .. } => {
            first_stage_call_in_expr(iter).or_else(|| first_stage_call_in_block(body))
        }
        Stmt::While {
            condition, body, ..
        } => first_stage_call_in_expr(condition).or_else(|| first_stage_call_in_block(body)),
        Stmt::Loop { body, .. } => first_stage_call_in_block(body),
        Stmt::Item(_) | Stmt::Continue(_) => None,
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
            .or_else(|| next.as_ref().and_then(|branch| first_stage_call_in_else_branch(branch))),
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
        | Expr::Comptime(operand, _) => first_stage_call_in_expr(operand),
        Expr::Field { object, .. } => first_stage_call_in_expr(object),
        Expr::Index { object, index, .. } => {
            first_stage_call_in_expr(object).or_else(|| first_stage_call_in_expr(index))
        }
        Expr::Struct { fields, rest, .. } => fields
            .iter()
            .find_map(|(_, value)| first_stage_call_in_expr(value))
            .or_else(|| rest.as_ref().and_then(|rest| first_stage_call_in_expr(rest))),
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
            .or_else(|| end.as_ref().and_then(|value| first_stage_call_in_expr(value))),
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
        Expr::MemStore { pointer, value, .. } => {
            first_stage_call_in_expr(pointer).or_else(|| first_stage_call_in_expr(value))
        }
        Expr::PtrOffset {
            pointer, offset, ..
        } => first_stage_call_in_expr(pointer).or_else(|| first_stage_call_in_expr(offset)),
        Expr::MemLoad { pointer, .. } => first_stage_call_in_expr(pointer),
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => init
            .iter()
            .find_map(|(_, value)| first_stage_call_in_expr(value)),
        Expr::Return(value, _) | Expr::Break(value, _) => {
            value
                .as_ref()
                .and_then(|expr| first_stage_call_in_expr(expr.as_ref()))
        }
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
        ResolvedType::Bool
        | ResolvedType::Int(_)
        | ResolvedType::Float(_)
        | ResolvedType::Char => true,
        ResolvedType::Array(inner, _) | ResolvedType::Option(inner) => {
            supports_converge_verify_sampling(inner)
        }
        ResolvedType::Tuple(items) => items.iter().all(supports_converge_verify_sampling),
        _ => false,
    }
}

fn check_struct(env: &mut TypeEnv, s: &Struct) -> KainResult<TypedStruct> {
    let mut fields = HashMap::new();
    for f in &s.fields {
        let field_ty = resolve_type_in_env(env, &f.ty)?;
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

fn check_component(env: &mut TypeEnv, c: &Component) -> KainResult<TypedComponent> {
    let mut props = HashMap::new();
    for p in &c.props {
        props.insert(p.name.clone(), resolve_param_type(env, p, None)?);
    }

    let self_ty = ResolvedType::Struct(c.name.clone(), props.clone());
    for state in &c.state {
        let state_ty = resolve_type_in_env(env, &state.ty)?;
        let initial_ty = infer_expr_type(env, &state.initial, None)?;
        ensure_type_compatible(
            env,
            &state_ty,
            &initial_ty,
            state.initial.span(),
            "component state initializer",
        )?;
    }

    for method in &c.methods {
        check_function_with_self(env, method, &self_ty)?;
    }

    check_jsx_semantics(env, &c.body, None)?;

    Ok(TypedComponent {
        ast: c.clone(),
        prop_types: props,
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

    env.push_scope();
    for (param, ty) in s.inputs.iter().zip(inputs.iter()) {
        env.define(param.name.clone(), ty.clone());
    }
    for uniform in &s.uniforms {
        env.define(uniform.name.clone(), resolve_type_in_env(env, &uniform.ty)?);
    }
    check_block_semantics(env, &s.body, &ctx)?;
    env.pop_scope();

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
            "Void" => Ok(ResolvedType::Unit),
            "Bool" => Ok(ResolvedType::Bool),
            "Char" => Ok(ResolvedType::Char),
            "String" => Ok(ResolvedType::String),
            "Array" if generics.len() == 1 => Ok(ResolvedType::Array(
                Box::new(resolve_type_impl(env, &generics[0])?),
                0,
            )),
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
        Item::Converge(converge) => converge.span,
        Item::World(world) => world.span,
        Item::Orchestrate(orchestrate) => orchestrate.span,
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
        .unwrap_or(ResolvedType::Unit);
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

fn check_block_semantics(
    env: &mut TypeEnv,
    block: &Block,
    ctx: &SemanticContext,
) -> KainResult<()> {
    for stmt in &block.stmts {
        check_stmt_semantics(env, stmt, ctx)?;
    }
    Ok(())
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
            ensure_type_compatible(
                env,
                &ResolvedType::Bool,
                &cond_ty,
                condition.span(),
                "while condition",
            )?;
            env.push_scope();
            check_block_semantics(env, body, ctx)?;
            env.pop_scope();
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
                ResolvedType::String => ResolvedType::String,
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
            env.push_scope();
            bind_pattern_types(env, binding, &item_ty)?;
            check_block_semantics(env, body, ctx)?;
            env.pop_scope();
        }
        Stmt::Loop { body, .. } => {
            env.push_scope();
            check_block_semantics(env, body, ctx)?;
            env.pop_scope();
        }
        Stmt::Item(item) => {
            let _ = check_item(env, item.as_ref())?;
        }
        Stmt::Break(_, _) | Stmt::Continue(_) => {}
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
        Expr::Ident(name, span) => env
            .lookup(name)
            .cloned()
            .ok_or_else(|| env.type_error(format!("Unknown identifier '{}'", name), *span)),
        Expr::Paren(inner, _) => infer_expr_type(env, inner, ctx),
        Expr::Block(block, _) => {
            env.push_scope();
            let block_ctx = ctx.cloned().unwrap_or(SemanticContext {
                function_name: "<block>".to_string(),
                return_type: ResolvedType::Unit,
                effects: EffectSet::new(),
            });
            check_block_semantics(env, block, &block_ctx)?;
            env.pop_scope();
            Ok(ResolvedType::Unit)
        }
        Expr::Unary { op, operand, span } => {
            let operand_ty = infer_expr_type(env, operand, ctx)?;
            match op {
                UnaryOp::Neg => {
                    if is_numeric_like(&operand_ty) || matches!(operand_ty, ResolvedType::Unknown) {
                        Ok(operand_ty)
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
                    if matches!(operand_ty, ResolvedType::Bool | ResolvedType::Unknown) {
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
                    if is_integer_like(&operand_ty) {
                        Ok(operand_ty)
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
            let arg_types = args
                .iter()
                .map(|arg| infer_expr_type(env, &arg.value, ctx))
                .collect::<Result<Vec<_>, _>>()?;
            match callee_ty {
                ResolvedType::Function { params, ret, .. } => {
                    if params.len() != arg_types.len() {
                        return Err(env.type_error(
                            format!(
                                "Expected {} argument(s), found {}",
                                params.len(),
                                arg_types.len()
                            ),
                            *span,
                        ));
                    }
                    for (param_ty, arg_ty) in params.iter().zip(arg_types.iter()) {
                        ensure_type_compatible(env, param_ty, arg_ty, *span, "stage argument")?;
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
            let callee_ty = infer_expr_type(env, callee, ctx)?;
            let arg_types = args
                .iter()
                .map(|arg| infer_expr_type(env, &arg.value, ctx))
                .collect::<Result<Vec<_>, _>>()?;

            match callee_ty {
                ResolvedType::Function {
                    params,
                    ret,
                    effects,
                } => {
                    if params.len() != arg_types.len() {
                        return Err(env.type_error(
                            format!(
                                "Expected {} argument(s), found {}",
                                params.len(),
                                arg_types.len()
                            ),
                            *span,
                        ));
                    }
                    for (param_ty, arg_ty) in params.iter().zip(arg_types.iter()) {
                        ensure_type_compatible(env, param_ty, arg_ty, *span, "function argument")?;
                    }
                    if let (Some(ctx), Expr::Ident(callee_name, _)) = (ctx, callee.as_ref()) {
                        check_effect_call(
                            &ctx.effects,
                            &effects,
                            &ctx.function_name,
                            callee_name,
                            *span,
                        )?;
                    }
                    Ok(*ret)
                }
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
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
            let arg_types = args
                .iter()
                .map(|arg| infer_expr_type(env, &arg.value, ctx))
                .collect::<Result<Vec<_>, _>>()?;
            infer_method_call_type(env, ctx, &receiver_ty, method, &arg_types, *span)
        }
        Expr::Field {
            object,
            field,
            span,
        } => {
            let object_ty = infer_expr_type(env, object, ctx)?;
            match object_ty {
                ResolvedType::Struct(_, fields) => fields
                    .get(field)
                    .cloned()
                    .ok_or_else(|| env.type_error(format!("Unknown field '{}'", field), *span)),
                ResolvedType::Tuple(items) => tuple_field_type(env, &items, field, *span),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "Field access requires a struct value, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::Index {
            object,
            index,
            span,
        } => {
            let object_ty = infer_expr_type(env, object, ctx)?;
            let index_ty = infer_expr_type(env, index, ctx)?;
            ensure_type_compatible(
                env,
                &ResolvedType::Int(IntSize::I64),
                &index_ty,
                index.span(),
                "index expression",
            )?;
            match object_ty {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => Ok(*inner),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "Indexing requires an array or slice, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
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
                        for ((_, value), expected_ty) in values.iter().zip(expected.iter()) {
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
                    _ => {}
                }
            }
            Ok(enum_ty)
        }
        Expr::Array(values, _) => infer_array_type(env, values, ctx),
        Expr::Tuple(values, _) => Ok(ResolvedType::Tuple(
            values
                .iter()
                .map(|value| infer_expr_type(env, value, ctx))
                .collect::<Result<_, _>>()?,
        )),
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
            span,
        } => {
            let cond_ty = infer_expr_type(env, condition, ctx)?;
            ensure_type_compatible(
                env,
                &ResolvedType::Bool,
                &cond_ty,
                condition.span(),
                "if condition",
            )?;

            let branch_ctx = ctx.cloned().unwrap_or(SemanticContext {
                function_name: "<if>".to_string(),
                return_type: ResolvedType::Unit,
                effects: EffectSet::new(),
            });

            env.push_scope();
            check_block_semantics(env, then_branch, &branch_ctx)?;
            env.pop_scope();
            let then_ty = ResolvedType::Unit;

            let else_ty = if let Some(else_branch) = else_branch {
                infer_else_branch_type(env, else_branch.as_ref(), &branch_ctx)?
            } else {
                ResolvedType::Unit
            };

            unify_types(&then_ty, &else_ty).ok_or_else(|| {
                env.type_error(
                    format!(
                        "if branches do not agree on a type: {} vs {}",
                        describe_type(&then_ty),
                        describe_type(&else_ty)
                    ),
                    *span,
                )
            })
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
            env.push_scope();
            let mut param_types = Vec::new();
            for param in params {
                let ty = resolve_param_type(env, param, None)?;
                env.define(param.name.clone(), ty.clone());
                param_types.push(ty);
            }
            let body_ty = infer_expr_type(env, body, ctx)?;
            env.pop_scope();
            if !matches!(ret, ResolvedType::Unknown) {
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
        Expr::Cast { target, .. } => resolve_type_in_env(env, target),
        Expr::Try(value, span) => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            match value_ty {
                ResolvedType::Result(ok, _) => Ok(*ok),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "'?' expects a Result value, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::Await(value, span) => {
            let value_ty = infer_expr_type(env, value, ctx)?;
            match value_ty {
                ResolvedType::Future(inner) => Ok(*inner),
                ResolvedType::Unknown => Ok(ResolvedType::Unknown),
                other => Err(env.type_error(
                    format!(
                        "await expects a Future value, found {}",
                        describe_type(&other)
                    ),
                    *span,
                )),
            }
        }
        Expr::AsyncBlock(value, _) => Ok(ResolvedType::Future(Box::new(infer_expr_type(
            env, value, ctx,
        )?))),
        Expr::Spawn { actor, .. } => Ok(ResolvedType::Struct(actor.clone(), HashMap::new())),
        Expr::SendMsg { .. } => Ok(ResolvedType::Unit),
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

fn infer_else_branch_type(
    env: &mut TypeEnv,
    else_branch: &ElseBranch,
    ctx: &SemanticContext,
) -> KainResult<ResolvedType> {
    match else_branch {
        ElseBranch::Else(block) => {
            env.push_scope();
            check_block_semantics(env, block, ctx)?;
            env.pop_scope();
            Ok(ResolvedType::Unit)
        }
        ElseBranch::ElseIf(cond, block, next) => {
            let cond_ty = infer_expr_type(env, cond, Some(ctx))?;
            ensure_type_compatible(
                env,
                &ResolvedType::Bool,
                &cond_ty,
                cond.span(),
                "else-if condition",
            )?;
            env.push_scope();
            check_block_semantics(env, block, ctx)?;
            env.pop_scope();
            let current_ty = ResolvedType::Unit;
            let next_ty = if let Some(next) = next {
                infer_else_branch_type(env, next.as_ref(), ctx)?
            } else {
                ResolvedType::Unit
            };
            unify_types(&current_ty, &next_ty).ok_or_else(|| {
                env.type_error(
                    format!(
                        "else-if branches do not agree on a type: {} vs {}",
                        describe_type(&current_ty),
                        describe_type(&next_ty)
                    ),
                    cond.span(),
                )
            })
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

        env.push_scope();
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
        let arm_ty = infer_expr_type(env, &arm.body, ctx)?;
        env.pop_scope();

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
            unify_types(&element_ty, &value_ty).ok_or_else(|| {
                env.type_error(
                    format!(
                        "Array elements do not agree on a type: {} vs {}",
                        describe_type(&element_ty),
                        describe_type(&value_ty)
                    ),
                    value.span(),
                )
            })?
        };
    }
    Ok(ResolvedType::Array(Box::new(element_ty), values.len()))
}

fn infer_method_call_type(
    env: &mut TypeEnv,
    ctx: Option<&SemanticContext>,
    receiver_ty: &ResolvedType,
    method: &str,
    arg_types: &[ResolvedType],
    span: Span,
) -> KainResult<ResolvedType> {
    match receiver_ty {
        ResolvedType::Struct(name, _) | ResolvedType::Enum(name, _) => {
            if let Some(method_ty) = env.lookup_method(name, method).cloned() {
                if let ResolvedType::Function {
                    params,
                    ret,
                    effects,
                } = method_ty
                {
                    let start_index = usize::from(
                        params
                            .first()
                            .map(|ty| types_compatible(ty, receiver_ty))
                            .unwrap_or(false),
                    );
                    let params = &params[start_index..];
                    if params.len() != arg_types.len() {
                        return Err(env.type_error(
                            format!(
                                "Method '{}' expects {} argument(s), found {}",
                                method,
                                params.len(),
                                arg_types.len()
                            ),
                            span,
                        ));
                    }
                    for (param_ty, arg_ty) in params.iter().zip(arg_types.iter()) {
                        ensure_type_compatible(env, param_ty, arg_ty, span, "method argument")?;
                    }
                    if let Some(ctx) = ctx {
                        check_effect_call(
                            &ctx.effects,
                            &effects,
                            &ctx.function_name,
                            method,
                            span,
                        )?;
                    }
                    Ok(*ret)
                } else {
                    Ok(ResolvedType::Unknown)
                }
            } else {
                Err(env.type_error(format!("Unknown method '{}' on {}", method, name), span))
            }
        }
        ResolvedType::Array(inner, _) => match method {
            "len" => Ok(ResolvedType::Int(IntSize::I64)),
            "push" => {
                if arg_types.len() == 1 {
                    ensure_type_compatible(env, inner.as_ref(), &arg_types[0], span, "array push")?;
                    Ok(ResolvedType::Unit)
                } else {
                    Err(env.type_error("Array.push expects exactly one argument", span))
                }
            }
            _ => Err(env.type_error(format!("Unknown method '{}' on Array", method), span)),
        },
        ResolvedType::String => match method {
            "len" => Ok(ResolvedType::Int(IntSize::I64)),
            _ => Err(env.type_error(format!("Unknown method '{}' on String", method), span)),
        },
        ResolvedType::Unknown => Ok(ResolvedType::Unknown),
        other => Err(env.type_error(
            format!(
                "Method call requires a receiver with known methods, found {}",
                describe_type(other)
            ),
            span,
        )),
    }
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

fn check_pattern_compatibility(
    env: &mut TypeEnv,
    pattern: &Pattern,
    scrutinee_ty: &ResolvedType,
) -> KainResult<()> {
    match pattern {
        Pattern::Wildcard(_) | Pattern::Binding { .. } => Ok(()),
        Pattern::Literal(expr) => {
            let literal_ty = infer_expr_type(env, expr, None)?;
            ensure_type_compatible(env, scrutinee_ty, &literal_ty, expr.span(), "match pattern")
        }
        Pattern::Tuple(patterns, span) => match scrutinee_ty {
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
        } => match scrutinee_ty {
            ResolvedType::Struct(struct_name, known_fields)
                if struct_name == name || matches!(scrutinee_ty, ResolvedType::Unknown) =>
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
            let expected_enum = enum_name
                .as_deref()
                .or_else(|| match scrutinee_ty {
                    ResolvedType::Enum(name, _) => Some(name.as_str()),
                    _ => None,
                })
                .unwrap_or_default();
            if !expected_enum.is_empty() {
                if let Some(field_types) = env
                    .lookup_enum_variant_fields(expected_enum, variant)
                    .cloned()
                {
                    match (fields, field_types.as_slice()) {
                        (VariantPatternFields::Unit, []) => {}
                        (VariantPatternFields::Tuple(patterns), types)
                            if patterns.len() == types.len() =>
                        {
                            for (pattern, field_ty) in patterns.iter().zip(types.iter()) {
                                check_pattern_compatibility(env, pattern, field_ty)?;
                            }
                        }
                        (VariantPatternFields::Struct(patterns), types)
                            if patterns.len() == types.len() =>
                        {
                            for ((_, pattern), field_ty) in patterns.iter().zip(types.iter()) {
                                check_pattern_compatibility(env, pattern, field_ty)?;
                            }
                        }
                        _ => {}
                    }
                }
            }
            match scrutinee_ty {
                ResolvedType::Enum(_, _) | ResolvedType::Unknown => Ok(()),
                other => Err(env.type_error(
                    format!("Variant pattern does not match {}", describe_type(other)),
                    *span,
                )),
            }
        }
        Pattern::Slice { patterns, span, .. } => match scrutinee_ty {
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
            if let ResolvedType::Tuple(items) = ty {
                for (pattern, item_ty) in patterns.iter().zip(items.iter()) {
                    bind_pattern_types(env, pattern, item_ty)?;
                }
            }
            Ok(())
        }
        Pattern::Struct { fields, .. } => {
            if let ResolvedType::Struct(_, known_fields) = ty {
                for (field_name, pattern) in fields {
                    if let Some(field_ty) = known_fields.get(field_name) {
                        bind_pattern_types(env, pattern, field_ty)?;
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
            let enum_name = enum_name.as_deref().or_else(|| match ty {
                ResolvedType::Enum(name, _) => Some(name.as_str()),
                _ => None,
            });
            if let Some(enum_name) = enum_name {
                if let Some(field_types) =
                    env.lookup_enum_variant_fields(enum_name, variant).cloned()
                {
                    match fields {
                        VariantPatternFields::Tuple(patterns) => {
                            for (pattern, field_ty) in patterns.iter().zip(field_types.iter()) {
                                bind_pattern_types(env, pattern, field_ty)?;
                            }
                        }
                        VariantPatternFields::Struct(patterns) => {
                            for ((_, pattern), field_ty) in patterns.iter().zip(field_types.iter())
                            {
                                bind_pattern_types(env, pattern, field_ty)?;
                            }
                        }
                        VariantPatternFields::Unit => {}
                    }
                }
            }
            Ok(())
        }
        Pattern::Slice { patterns, rest, .. } => {
            let item_ty = match ty {
                ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => {
                    inner.as_ref().clone()
                }
                _ => ResolvedType::Unknown,
            };
            for pattern in patterns {
                bind_pattern_types(env, pattern, &item_ty)?;
            }
            if let Some(rest_name) = rest {
                env.define(rest_name.clone(), ResolvedType::Slice(Box::new(item_ty)));
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
                if let JSXAttrValue::Expr(expr) = &attribute.value {
                    let _ = infer_expr_type(env, expr, ctx)?;
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
            env.push_scope();
            env.define(binding.clone(), item_ty);
            check_jsx_semantics(env, body, ctx)?;
            env.pop_scope();
        }
        JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let cond_ty = infer_expr_type(env, condition, ctx)?;
            ensure_type_compatible(
                env,
                &ResolvedType::Bool,
                &cond_ty,
                condition.span(),
                "jsx if condition",
            )?;
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
            if matches!((left, right), (ResolvedType::String, ResolvedType::String))
                && matches!(op, Add)
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
            if matches!(left, ResolvedType::Unknown) || matches!(right, ResolvedType::Unknown) {
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
        Eq | Ne | Lt | Gt | Le | Ge => {
            if types_compatible(left, right)
                || matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
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
            if (matches!(left, ResolvedType::Bool) || matches!(left, ResolvedType::Unknown))
                && (matches!(right, ResolvedType::Bool) || matches!(right, ResolvedType::Unknown))
            {
                Ok(ResolvedType::Bool)
            } else {
                Err(env.type_error(
                    format!(
                        "Logical operator expects Bool operands, found {} and {}",
                        describe_type(left),
                        describe_type(right)
                    ),
                    span,
                ))
            }
        }
        BitAnd | BitOr | BitXor | Shl | Shr => {
            if is_integer_like(left) && is_integer_like(right) {
                Ok(promote_numeric_type(left, right))
            } else if matches!(left, ResolvedType::Unknown)
                || matches!(right, ResolvedType::Unknown)
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

fn types_compatible(expected: &ResolvedType, actual: &ResolvedType) -> bool {
    match (expected, actual) {
        (ResolvedType::Unknown, _) | (_, ResolvedType::Unknown) => true,
        (ResolvedType::Never, _) | (_, ResolvedType::Never) => true,
        (ResolvedType::Generic(_), _) | (_, ResolvedType::Generic(_)) => true,
        (ResolvedType::Unit, ResolvedType::Unit)
        | (ResolvedType::Bool, ResolvedType::Bool)
        | (ResolvedType::String, ResolvedType::String)
        | (ResolvedType::Char, ResolvedType::Char) => true,
        (ResolvedType::Int(_), ResolvedType::Int(_)) => true,
        (ResolvedType::Float(_), ResolvedType::Float(_)) => true,
        (ResolvedType::Int(_), ResolvedType::Float(_))
        | (ResolvedType::Float(_), ResolvedType::Int(_)) => true,
        (ResolvedType::Array(left, left_len), ResolvedType::Array(right, right_len)) => {
            (left_len == right_len || *left_len == 0 || *right_len == 0)
                && types_compatible(left, right)
        }
        (ResolvedType::Slice(left), ResolvedType::Slice(right)) => types_compatible(left, right),
        (ResolvedType::Slice(left), ResolvedType::Array(right, _))
        | (ResolvedType::Array(left, _), ResolvedType::Slice(right)) => {
            types_compatible(left, right)
        }
        (ResolvedType::Tuple(left), ResolvedType::Tuple(right)) => {
            left.len() == right.len()
                && left
                    .iter()
                    .zip(right.iter())
                    .all(|(left, right)| types_compatible(left, right))
        }
        (ResolvedType::Option(left), ResolvedType::Option(right)) => types_compatible(left, right),
        (ResolvedType::Result(left_ok, left_err), ResolvedType::Result(right_ok, right_err)) => {
            types_compatible(left_ok, right_ok) && types_compatible(left_err, right_err)
        }
        (ResolvedType::Future(left), ResolvedType::Future(right)) => types_compatible(left, right),
        (
            ResolvedType::Ref {
                mutable: left_mut,
                inner: left,
            },
            ResolvedType::Ref {
                mutable: right_mut,
                inner: right,
            },
        ) => left_mut == right_mut && types_compatible(left, right),
        (
            ResolvedType::Ptr {
                mutable: left_mut,
                inner: left,
            },
            ResolvedType::Ptr {
                mutable: right_mut,
                inner: right,
            },
        ) => left_mut == right_mut && types_compatible(left, right),
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
        (ResolvedType::Struct(left, _), ResolvedType::Struct(right, _))
        | (ResolvedType::Enum(left, _), ResolvedType::Enum(right, _)) => left == right,
        _ => false,
    }
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
        ResolvedType::Ref { inner, .. } => format!("&{}", describe_type(inner)),
        ResolvedType::Ptr { inner, .. } => format!("ptr<{}>", describe_type(inner)),
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
        _ => return Err(env.type_error(format!("Unknown tuple/vector field '{}'", field), span)),
    };
    items.get(index).cloned().ok_or_else(|| {
        env.type_error(
            format!("Field '{}' is out of bounds for this tuple", field),
            span,
        )
    })
}
