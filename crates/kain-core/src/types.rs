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

/// Type environment for checking
pub struct TypeEnv<'a> {
    scopes: Vec<HashMap<String, ResolvedType>>,
    types: HashMap<String, ResolvedType>,
    globals: HashMap<String, ResolvedType>,
    methods: HashMap<String, HashMap<String, ResolvedType>>,
    enum_variants: HashMap<String, HashMap<String, EnumVariantTypeInfo>>,
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
        env.types.insert("Map".into(), selfhost_map_type());
        env.types.insert("Set".into(), selfhost_set_type());
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
        register_selfhost_constructor_globals(&mut env);
        register_selfhost_collection_methods(&mut env);
        register_selfhost_host_bridge(&mut env);
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
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted_message =
            format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, message.into());
        KainError::type_error(formatted_message, span)
    }
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
        "read_file".into(),
        builtin_function_type(vec![ResolvedType::String], ResolvedType::String),
    );
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
    for item in &program.items {
        register_item_types(&mut env, item)?;
    }

    // Third pass: Refresh registrations now that every type shape is present.
    // This resolves recursive payloads like enums that reference structs
    // declared later in the same program.
    for item in &program.items {
        register_item_types(&mut env, item)?;
    }

    // Fourth pass: Type check all items.
    let mut typed_items = Vec::new();
    for item in &program.items {
        check_item_into(&mut env, item, &mut typed_items)?;
    }

    Ok(TypedProgram { items: typed_items })
}

fn predeclare_item_types(env: &mut TypeEnv, item: &Item) {
    match item {
        Item::Struct(s) => {
            env.types
                .entry(s.name.clone())
                .or_insert_with(|| ResolvedType::Struct(s.name.clone(), HashMap::new()));
        }
        Item::Enum(e) => {
            env.types
                .entry(e.name.clone())
                .or_insert_with(|| ResolvedType::Enum(e.name.clone(), Vec::new()));
            env.enum_variants.entry(e.name.clone()).or_default();
        }
        Item::World(world) => {
            env.types
                .entry(world.name.clone())
                .or_insert_with(|| ResolvedType::Struct(world.name.clone(), HashMap::new()));
        }
        Item::Component(component) => {
            env.types
                .entry(component.name.clone())
                .or_insert_with(|| ResolvedType::Struct(component.name.clone(), HashMap::new()));
        }
        Item::Actor(actor) => {
            env.types
                .entry(actor.name.clone())
                .or_insert_with(|| ResolvedType::Struct(actor.name.clone(), HashMap::new()));
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
            env.types.insert(e.name.clone(), enum_ty.clone());
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
                let module_path = vec![module.name.clone()];
                register_inline_module_type_aliases(env, children, &module_path);
                register_inline_module_global_aliases(env, children, &module_path)?;
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
        env.define_method(
            type_name.to_string(),
            method.name.clone(),
            signature.clone(),
        );
        if !method_has_receiver_param(method) {
            env.define_global(
                lowered_impl_function_name(type_name, &method.name),
                signature.clone(),
            );
            env.define_global(
                selfhost_static_impl_function_name(type_name, &method.name),
                signature,
            );
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
                env.define_global(
                    module_scoped_name(module_path, &f.name),
                    function_signature(env, f, None)?,
                );
            }
            Item::Const(c) => {
                env.define_global(
                    module_scoped_name(module_path, &c.name),
                    resolve_type_in_env(env, &c.ty)?,
                );
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
        let bindings = inline_module_scope_bindings(env, children)?;
        for child in children {
            match child {
                Item::Mod(_) => check_item_into(env, child, &mut items)?,
                _ => {
                    env.push_scope();
                    define_scope_bindings(env, &bindings);
                    check_item_into(env, child, &mut items)?;
                    env.pop_scope();
                }
            }
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
    env.push_scope();
    for (name, ty) in &props {
        env.define(name.clone(), ty.clone());
    }
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
        env.define(state.name.clone(), state_ty);
    }

    for method in &c.methods {
        check_function_with_self(env, method, &self_ty)?;
    }

    check_jsx_semantics(env, &c.body, None)?;
    env.pop_scope();

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
        env.push_scope();
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
        env.pop_scope();

        if !matches!(annotated_ret, ResolvedType::Unknown) {
            ensure_type_compatible(env, &annotated_ret, &body_ty, body.span(), "lambda body")?;
        } else {
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
    for stmt in &block.stmts {
        check_stmt_semantics(env, stmt, ctx)?;
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
            let block_ctx = ctx.cloned().unwrap_or(SemanticContext {
                function_name: "<block>".to_string(),
                return_type: ResolvedType::Unit,
                effects: EffectSet::new(),
            });
            block_value_type(env, block, &block_ctx)
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
                        ensure_type_compatible(env, param_ty, &arg_ty, *span, "function argument")?;
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
                ensure_type_compatible(
                    env,
                    &ResolvedType::Int(IntSize::I64),
                    &index_ty,
                    index.span(),
                    "index expression",
                )?;
                match object_ty {
                    ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => Ok(*inner),
                    ResolvedType::String => Ok(ResolvedType::String),
                    ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
                        match *inner {
                            ResolvedType::Array(item, _) | ResolvedType::Slice(item) => Ok(*item),
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

            let then_ty = block_value_type(env, then_branch, &branch_ctx)?;

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
        ElseBranch::Else(block) => block_value_type(env, block, ctx),
        ElseBranch::ElseIf(cond, block, next) => {
            let cond_ty = infer_expr_type(env, cond, Some(ctx))?;
            ensure_type_compatible(
                env,
                &ResolvedType::Bool,
                &cond_ty,
                cond.span(),
                "else-if condition",
            )?;
            let current_ty = block_value_type(env, block, ctx)?;
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
            _ => Err(env.type_error(format!("Unknown method '{}' on Array", method), span)),
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
            _ => Err(env.type_error(format!("Unknown method '{}' on Slice", method), span)),
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
            _ => Err(env.type_error(format!("Unknown method '{}' on Option", method), span)),
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
            _ => Err(env.type_error(
                format!(
                    "Unknown method '{}' on {}",
                    method,
                    describe_type(receiver_ty)
                ),
                span,
            )),
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
            if params.len() != args.len() {
                return Err(env.type_error(
                    format!(
                        "Method '{}' expects {} argument(s), found {}",
                        method,
                        params.len(),
                        args.len()
                    ),
                    span,
                ));
            }
            for (param_ty, arg) in params.iter().zip(args.iter()) {
                let arg_ty = infer_expr_type_with_expected(env, &arg.value, ctx, Some(param_ty))?;
                ensure_type_compatible(env, param_ty, &arg_ty, span, "method argument")?;
            }
            if let Some(ctx) = ctx {
                check_effect_call(&ctx.effects, &effects, &ctx.function_name, method, span)?;
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
        Err(env.type_error(
            format!("Unknown method '{}' on {}", method, type_name),
            span,
        ))
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
    let string_item = ResolvedType::String;
    let borrowed_string_item = shared_ref_type(ResolvedType::String);
    if !types_compatible(&string_item, item_ty) && !types_compatible(&borrowed_string_item, item_ty)
    {
        return Err(env.type_error(
            format!(
                "{method_name} requires a String sequence, found {}",
                describe_type(item_ty)
            ),
            span,
        ));
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
    if args.len() != 1 {
        return Err(env.type_error(format!("{method_name} expects exactly one argument"), span));
    }
    let expected_borrowed = shared_ref_type(ResolvedType::String);
    let arg_ty = infer_expr_type_with_expected(env, &args[0].value, ctx, Some(&expected_borrowed))?;
    if !types_compatible(&ResolvedType::String, &arg_ty)
        && !types_compatible(&expected_borrowed, &arg_ty)
        && !types_compatible(&ResolvedType::String, peel_shared_refs(&arg_ty))
    {
        return Err(env.type_error(
            format!(
                "{method_name} expected String or &String, found {}",
                describe_type(&arg_ty)
            ),
            args[0].span,
        ));
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
            if (is_numeric_like(left) && is_numeric_like(right))
                || types_compatible(left, right)
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
            Err(env.type_error(format!("Unknown field '{}'", field), span))
        }
        ResolvedType::Tuple(items) => tuple_field_type(env, items, field, span),
        ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
            field_access_type(env, inner, field, span)
        }
        ResolvedType::Unknown => Ok(ResolvedType::Unknown),
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
    use crate::diagnostics::SpanMapper;
    use crate::lexer::Lexer;
    use crate::parser::Parser;

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
                    fields: vec![],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
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
                    fields: vec![],
                    methods: vec![],
                    attributes: vec![],
                    visibility: Visibility::Private,
                    span,
                }),
                Item::Function(Function {
                    name: "inspect".to_string(),
                    generics: vec![],
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
                    fields: vec![],
                    methods: vec![],
                    visibility: Visibility::Public,
                    attributes: vec![],
                    span,
                }),
                Item::Impl(Impl {
                    generics: vec![],
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
}
