//! KAIN Runtime - Interpreter and actor system

use crate::ast::*;
use crate::error::{KainError, KainResult};
use crate::language_features::runtime_supports_binary_op;
use crate::lexer::Lexer;
use crate::module_resolution::{
    filesystem_module_candidates, resolve_filesystem_module_file, resolve_stdlib_module_file,
};
use crate::parser::Parser;
use crate::span::Span;
use crate::types::{TypedItem, TypedProgram};
use crate::ui::{eval_jsx, VNode};
use flume::Sender;
use kain_actor::{ActorId, ActorIdAllocator, MessageEnvelope, DEFAULT_ASK_TIMEOUT_MS};
use kain_entangle::{EntangleBindingDescriptor, EntangleEndpointId, EntangleGraph};
use kain_fs::{
    DirectoryEntry, FsCapability, FsChunk, FsError, FsJournalEntry, FsMetadata, FsSandbox,
    FsWatchEvent, FsWatcher,
};
use kain_input::{InputEvent, InputSession, InputSource};
use kain_ownership::{
    OwnershipRegionDescriptor, OwnershipRegionKind, OwnershipTransition, OWNERSHIP_CAPABILITY,
};
use once_cell::sync::Lazy;
use std::any::Any;
use std::collections::{BTreeMap, HashMap};
use std::fmt;
use std::io::{self, BufRead, BufReader, Read};
use std::process::Command;
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Mutex, RwLock};

pub type EnvExtensionRegistrar = fn(&mut Env);

static ENV_EXTENSION_REGISTRARS: Lazy<RwLock<BTreeMap<String, EnvExtensionRegistrar>>> =
    Lazy::new(|| RwLock::new(BTreeMap::new()));
static RUNTIME_FS_SANDBOX: Lazy<RwLock<FsSandbox>> =
    Lazy::new(|| RwLock::new(FsSandbox::unrestricted_project()));
static RUNTIME_FS_WATCHERS: Lazy<RwLock<HashMap<i64, FsWatcher>>> =
    Lazy::new(|| RwLock::new(HashMap::new()));
static RUNTIME_FS_TRANSACTIONS: Lazy<RwLock<HashMap<i64, kain_fs::FsTransaction>>> =
    Lazy::new(|| RwLock::new(HashMap::new()));
static RUNTIME_INPUT_SESSIONS: Lazy<RwLock<HashMap<i64, InputSession>>> =
    Lazy::new(|| RwLock::new(HashMap::new()));
static RUNTIME_STDIN_READER: Lazy<Mutex<BufReader<io::Stdin>>> =
    Lazy::new(|| Mutex::new(BufReader::new(io::stdin())));
static RUNTIME_FS_NEXT_WATCHER_ID: AtomicI64 = AtomicI64::new(1);
static RUNTIME_FS_NEXT_TRANSACTION_ID: AtomicI64 = AtomicI64::new(1);
static RUNTIME_INPUT_NEXT_SESSION_ID: AtomicI64 = AtomicI64::new(1);

fn runtime_read_stdin_line() -> String {
    let mut line = String::new();
    if let Ok(mut reader) = RUNTIME_STDIN_READER.lock() {
        let _ = reader.read_line(&mut line);
    }
    line.trim_end().to_string()
}

fn runtime_read_stdin_exact(length: usize) -> KainResult<String> {
    let mut reader = RUNTIME_STDIN_READER
        .lock()
        .map_err(|_| KainError::runtime("stdin lock poisoned"))?;
    let mut buffer = vec![0u8; length];
    reader
        .read_exact(&mut buffer)
        .map_err(|err| KainError::runtime(format!("stdin_read_exact failed: {}", err)))?;
    String::from_utf8(buffer)
        .map_err(|err| KainError::runtime(format!("stdin_read_exact utf8 failed: {}", err)))
}

fn lowered_impl_function_names(type_name: &str, method_name: &str) -> [String; 2] {
    [
        format!("{type_name}_{method_name}"),
        format!("{type_name}__{method_name}"),
    ]
}

fn is_extern_runtime_declaration(function: &Function) -> bool {
    function.attributes.iter().any(|attr| attr.name == "extern") && function.body.stmts.is_empty()
}

fn module_scoped_name(module_path: &[String], item_name: &str) -> String {
    let mut parts = module_path.to_vec();
    parts.push(item_name.to_string());
    parts.join("__")
}

fn selfhost_enum_variant_alias_name(enum_name: &str, variant_name: &str) -> String {
    format!("{enum_name}__{variant_name}")
}

pub fn register_env_extension(name: impl Into<String>, registrar: EnvExtensionRegistrar) {
    ENV_EXTENSION_REGISTRARS
        .write()
        .unwrap()
        .insert(name.into(), registrar);
}

/// Runtime value
#[derive(Clone)]
pub enum Value {
    Unit,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    Array(Arc<RwLock<Vec<Value>>>),
    Tuple(Vec<Value>),
    Struct(String, Arc<RwLock<HashMap<String, Value>>>),
    HostObject(String, Arc<dyn Any + Send + Sync>),
    Function(String),
    Patch(String),
    Law(String),
    Converge(String),
    Orchestrate(String),
    NativeFn(String, fn(&mut Env, Vec<Value>) -> KainResult<Value>),
    ActorRef(ActorRef),
    None,
    /// Special value for return flow control
    Return(Box<Value>),
    /// Break from loop with optional value
    Break(Option<Box<Value>>),
    /// Continue to next loop iteration
    Continue,
    /// Result: Ok(true, val) or Err(false, val)
    Result(bool, Box<Value>),
    /// Closure: params, body, captured_scopes
    Closure(Vec<String>, Box<Expr>, Vec<HashMap<String, Value>>),
    /// Struct Constructor: name, field_names
    StructConstructor(String, Vec<String>),
    /// JSX Element
    JSX(VNode),
    /// Enum variant: (enum_name, variant_name, fields)
    EnumVariant(String, String, Vec<Value>),
    /// Poll result for async: Ready(value) or Pending
    Poll(bool, Option<Box<Value>>),
    /// Future state machine: (struct_name, state_struct, poll_fn_name)
    Future(String, Arc<RwLock<HashMap<String, Value>>>),
}

pub type NativeFn = fn(&mut Env, Vec<Value>) -> KainResult<Value>;

impl fmt::Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::Unit => write!(f, "Unit"),
            Value::Bool(b) => write!(f, "Bool({})", b),
            Value::Int(i) => write!(f, "Int({})", i),
            Value::Float(fl) => write!(f, "Float({})", fl),
            Value::String(s) => write!(f, "String({:?})", s),
            Value::Array(arr) => write!(f, "Array({:?})", arr),
            Value::Tuple(t) => write!(f, "Tuple({:?})", t),
            Value::Struct(name, fields) => write!(f, "Struct({}, {:?})", name, fields),
            Value::HostObject(name, _) => write!(f, "HostObject({})", name),
            Value::Function(name) => write!(f, "Function({})", name),
            Value::Patch(name) => write!(f, "Patch({})", name),
            Value::Law(name) => write!(f, "Law({})", name),
            Value::Converge(name) => write!(f, "Converge({})", name),
            Value::Orchestrate(name) => write!(f, "Orchestrate({})", name),
            Value::NativeFn(name, _) => write!(f, "NativeFn({})", name),
            Value::StructConstructor(name, _) => write!(f, "StructConstructor({})", name),
            Value::ActorRef(r) => write!(f, "ActorRef({:?})", r),
            Value::None => write!(f, "None"),
            Value::Return(v) => write!(f, "Return({:?})", v),
            Value::Result(ok, v) => {
                if *ok {
                    write!(f, "Ok({:?})", v)
                } else {
                    write!(f, "Err({:?})", v)
                }
            }
            Value::Closure(params, _, _) => write!(f, "Closure({:?})", params),
            Value::JSX(node) => write!(f, "JSX({:?})", node),
            Value::EnumVariant(e, v, _) => write!(f, "{}::{}", e, v),
            Value::Poll(ready, val) => {
                if *ready {
                    write!(f, "Poll::Ready({:?})", val)
                } else {
                    write!(f, "Poll::Pending")
                }
            }
            Value::Future(name, _) => write!(f, "Future<{}>", name),
            Value::Break(v) => write!(f, "Break({:?})", v),
            Value::Continue => write!(f, "Continue"),
        }
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Value::Unit => write!(f, "()"),
            Value::Bool(b) => write!(f, "{}", b),
            Value::Int(i) => write!(f, "{}", i),
            Value::Float(fl) => write!(f, "{}", fl),
            Value::String(s) => write!(f, "{}", s),
            Value::Array(arr) => {
                write!(f, "[")?;
                let arr = arr.read().unwrap();
                for (i, v) in arr.iter().enumerate() {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", v)?;
                }
                write!(f, "]")
            }
            Value::Tuple(t) => {
                write!(f, "(")?;
                for (i, v) in t.iter().enumerate() {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", v)?;
                }
                write!(f, ")")
            }
            Value::Struct(name, fields) => {
                write!(f, "{} {{", name)?;
                let fields = fields.read().unwrap();
                for (i, (k, v)) in fields.iter().enumerate() {
                    if i > 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}: {}", k, v)?;
                }
                write!(f, "}}")
            }
            Value::HostObject(name, _) => write!(f, "<host {}>", name),
            Value::Function(name) => write!(f, "<fn {}>", name),
            Value::Patch(name) => write!(f, "<patch {}>", name),
            Value::Law(name) => write!(f, "<law {}>", name),
            Value::Converge(name) => write!(f, "<converge {}>", name),
            Value::Orchestrate(name) => write!(f, "<orchestrate {}>", name),
            Value::NativeFn(name, _) => write!(f, "<native fn {}>", name),
            Value::StructConstructor(name, _) => write!(f, "<constructor {}>", name),
            Value::ActorRef(r) => write!(f, "<actor {}>", r.id),
            Value::None => write!(f, "none"),
            Value::Return(v) => write!(f, "{}", v),
            Value::Result(ok, v) => {
                if *ok {
                    write!(f, "Ok({})", v)
                } else {
                    write!(f, "Err({})", v)
                }
            }
            Value::Closure(_, _, _) => write!(f, "<closure>"),
            Value::JSX(node) => write!(f, "{}", node),
            Value::EnumVariant(enum_name, variant, fields) => {
                if fields.is_empty() {
                    write!(f, "{}::{}", enum_name, variant)
                } else {
                    write!(f, "{}::{}(", enum_name, variant)?;
                    for (i, v) in fields.iter().enumerate() {
                        if i > 0 {
                            write!(f, ", ")?;
                        }
                        write!(f, "{}", v)?;
                    }
                    write!(f, ")")
                }
            }
            Value::Poll(ready, val) => {
                if *ready {
                    if let Some(v) = val {
                        write!(f, "Poll::Ready({})", v)
                    } else {
                        write!(f, "Poll::Ready(())")
                    }
                } else {
                    write!(f, "Poll::Pending")
                }
            }
            Value::Future(name, _) => write!(f, "<future {}>", name),
            Value::Break(v) => {
                if let Some(val) = v {
                    write!(f, "<break {}>", val)
                } else {
                    write!(f, "<break>")
                }
            }
            Value::Continue => write!(f, "<continue>"),
        }
    }
}

impl Value {
    pub fn host_object(label: impl Into<String>, object: Arc<dyn Any + Send + Sync>) -> Self {
        Self::HostObject(label.into(), object)
    }

    pub fn downcast_host_object<T: Any + Send + Sync>(&self) -> Option<Arc<T>> {
        match self {
            Value::HostObject(_, object) => object.clone().downcast::<T>().ok(),
            _ => None,
        }
    }

    pub fn host_object_label(&self) -> Option<&str> {
        match self {
            Value::HostObject(label, _) => Some(label.as_str()),
            _ => None,
        }
    }
}

/// Reference to an actor
#[derive(Debug, Clone)]
pub struct ActorRef {
    pub id: ActorId,
    pub sender: Sender<Message>,
}

/// Message for actor communication
pub type Message = MessageEnvelope<Value>;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ExecutionLane {
    Interpret,
    Test,
}

#[derive(Debug, Clone)]
struct ActivePatchFrame {
    name: String,
    mutation_paths: Vec<String>,
    undo_mode: String,
    changes: Vec<ActivePatchChange>,
}

#[derive(Debug, Clone)]
enum PatchMutationTarget {
    StructField {
        fields: Arc<RwLock<HashMap<String, Value>>>,
        field: String,
    },
    ArrayIndex {
        values: Arc<RwLock<Vec<Value>>>,
        index: usize,
    },
}

#[derive(Debug, Clone)]
struct ActivePatchChange {
    path: String,
    target: PatchMutationTarget,
    old_value: Value,
    new_value: Value,
}

#[derive(Debug, Clone)]
pub struct PatchMutationRecord {
    pub path: String,
    pub old_value: Value,
    pub new_value: Value,
}

#[derive(Debug, Clone)]
pub struct PatchCollaborationEvent {
    pub event_id: String,
    pub patch_name: String,
    pub mutation_paths: Vec<String>,
    pub undo_mode: String,
}

#[derive(Debug, Clone)]
struct ReplayablePatchRecord {
    name: String,
    undo_mode: String,
    changes: Vec<ActivePatchChange>,
}

#[derive(Debug, Clone)]
pub struct PatchRuntimeRecord {
    pub name: String,
    pub mutation_paths: Vec<String>,
    pub undo_mode: String,
    pub changes: Vec<PatchMutationRecord>,
    pub collaboration_event: String,
}

/// Interpreter environment
#[derive(Clone)]
pub struct Env {
    scopes: Vec<HashMap<String, Value>>,
    functions: HashMap<String, Function>,
    function_inline_scopes: HashMap<String, HashMap<String, Value>>,
    patches: HashMap<String, PatchDef>,
    laws: HashMap<String, LawDef>,
    patch_undo_modes: HashMap<String, String>,
    converges: HashMap<String, ConvergeDef>,
    orchestrates: HashMap<String, OrchestrateDef>,
    worlds: HashMap<String, Arc<RwLock<HashMap<String, Value>>>>,
    entanglements: EntangleGraph,
    components: HashMap<String, Component>,
    inline_modules: HashMap<String, Vec<Item>>,
    /// Methods: type_name -> method_name -> function
    methods: HashMap<String, HashMap<String, Function>>,
    #[allow(dead_code)]
    actors: HashMap<ActorId, Sender<Message>>,
    #[allow(dead_code)]
    actor_ids: ActorIdAllocator,
    actor_defs: HashMap<String, Actor>,
    /// ID of the current actor if running inside one
    self_actor_id: Option<ActorId>,
    execution_lane: ExecutionLane,
    active_capabilities: Vec<String>,
    ownership_regions: HashMap<String, OwnershipRegionDescriptor>,
    active_patch_frames: Vec<ActivePatchFrame>,
    patch_records: Vec<PatchRuntimeRecord>,
    patch_replay_catalog: Vec<ReplayablePatchRecord>,
    replayable_patch_history: Vec<ReplayablePatchRecord>,
    undone_patch_records: Vec<ReplayablePatchRecord>,
    patch_collaboration_events: Vec<PatchCollaborationEvent>,
    extension_state: HashMap<String, Arc<dyn Any + Send + Sync>>,
}

impl Env {
    pub fn new() -> Self {
        let mut env = Self {
            scopes: vec![HashMap::new()],
            functions: HashMap::new(),
            function_inline_scopes: HashMap::new(),
            patches: HashMap::new(),
            laws: HashMap::new(),
            patch_undo_modes: HashMap::new(),
            converges: HashMap::new(),
            orchestrates: HashMap::new(),
            worlds: HashMap::new(),
            entanglements: EntangleGraph::default(),
            components: HashMap::new(),
            inline_modules: HashMap::new(),
            methods: HashMap::new(),
            actors: HashMap::new(),
            actor_ids: ActorIdAllocator::default(),
            actor_defs: HashMap::new(),
            self_actor_id: None,
            execution_lane: ExecutionLane::Interpret,
            active_capabilities: vec![
                "patch.transactions".to_string(),
                "converge.dispatch".to_string(),
                "orchestrate.pipeline".to_string(),
                OWNERSHIP_CAPABILITY.to_string(),
                "host.runtime.interpret".to_string(),
            ],
            ownership_regions: HashMap::new(),
            active_patch_frames: Vec::new(),
            patch_records: Vec::new(),
            patch_replay_catalog: Vec::new(),
            replayable_patch_history: Vec::new(),
            undone_patch_records: Vec::new(),
            patch_collaboration_events: Vec::new(),
            extension_state: HashMap::new(),
        };

        env.register_stdlib();
        env.register_net_stdlib();
        env.register_json_stdlib();
        env.register_kos_bridge();
        env.apply_registered_extensions();
        env
    }

    fn apply_registered_extensions(&mut self) {
        let registrars = ENV_EXTENSION_REGISTRARS
            .read()
            .unwrap()
            .values()
            .copied()
            .collect::<Vec<_>>();
        for registrar in registrars {
            registrar(self);
        }
    }

    pub fn set_extension_state(
        &mut self,
        key: impl Into<String>,
        state: Arc<dyn Any + Send + Sync>,
    ) {
        self.extension_state.insert(key.into(), state);
    }

    pub fn get_extension_state<T: Any + Send + Sync>(&self, key: &str) -> Option<Arc<T>> {
        self.extension_state
            .get(key)
            .cloned()
            .and_then(|state| state.downcast::<T>().ok())
    }

    pub fn register_kos_bridge(&mut self) {
        self.define_native("spawn_cube", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "spawn_cube: expected 2 arguments (x, y)",
                ));
            }
            let x = match args[0] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => return Err(KainError::runtime("spawn_cube: x must be number")),
            };
            let y = match args[1] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => return Err(KainError::runtime("spawn_cube: y must be number")),
            };

            println!(
                " [KOS Bridge] Spawning Cube at {{ x: {:.2}, y: {:.2} }}",
                x, y
            );
            Ok(Value::Unit)
        });
        self.define_native("spawn_native_viewport", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "spawn_native_viewport: expected 2 arguments (x, y)",
                ));
            }
            let x = match args[0] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => {
                    return Err(KainError::runtime(
                        "spawn_native_viewport: x must be number",
                    ))
                }
            };
            let y = match args[1] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => {
                    return Err(KainError::runtime(
                        "spawn_native_viewport: y must be number",
                    ))
                }
            };

            println!(
                " [KOS Bridge] Spawning Native Viewport at {{ x: {:.2}, y: {:.2} }}",
                x, y
            );
            Ok(Value::Unit)
        });
        self.define_native("spawn_native_sculpt_lab", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "spawn_native_sculpt_lab: expected 2 arguments (x, y)",
                ));
            }
            let x = match args[0] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => {
                    return Err(KainError::runtime(
                        "spawn_native_sculpt_lab: x must be number",
                    ))
                }
            };
            let y = match args[1] {
                Value::Int(n) => n as f64,
                Value::Float(n) => n,
                _ => {
                    return Err(KainError::runtime(
                        "spawn_native_sculpt_lab: y must be number",
                    ))
                }
            };

            println!(
                " [KOS Bridge] Spawning Native Sculpt Lab at {{ x: {:.2}, y: {:.2} }}",
                x, y
            );
            Ok(Value::Unit)
        });
        self.define_native("native_config_string", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "native_config_string: expected 2 arguments (key, value)",
                ));
            }
            let key = match &args[0] {
                Value::String(value) => value.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "native_config_string: key must be string",
                    ))
                }
            };
            let value = match &args[1] {
                Value::String(value) => value.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "native_config_string: value must be string",
                    ))
                }
            };
            std::env::set_var(key, value);
            Ok(Value::Unit)
        });
        self.define_native("native_config_int", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "native_config_int: expected 2 arguments (key, value)",
                ));
            }
            let key = match &args[0] {
                Value::String(value) => value.clone(),
                _ => return Err(KainError::runtime("native_config_int: key must be string")),
            };
            let value = match args[1] {
                Value::Int(value) => value,
                _ => return Err(KainError::runtime("native_config_int: value must be int")),
            };
            std::env::set_var(key, value.to_string());
            Ok(Value::Unit)
        });
        self.define_native("native_config_float", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "native_config_float: expected 2 arguments (key, value)",
                ));
            }
            let key = match &args[0] {
                Value::String(value) => value.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "native_config_float: key must be string",
                    ))
                }
            };
            let value = match args[1] {
                Value::Int(value) => value as f64,
                Value::Float(value) => value,
                _ => {
                    return Err(KainError::runtime(
                        "native_config_float: value must be number",
                    ))
                }
            };
            std::env::set_var(key, format!("{value:.6}"));
            Ok(Value::Unit)
        });
        self.define_native("native_config_flag", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "native_config_flag: expected 2 arguments (key, enabled)",
                ));
            }
            let key = match &args[0] {
                Value::String(value) => value.clone(),
                _ => return Err(KainError::runtime("native_config_flag: key must be string")),
            };
            let enabled = match args[1] {
                Value::Bool(value) => value,
                Value::Int(value) => value != 0,
                _ => {
                    return Err(KainError::runtime(
                        "native_config_flag: enabled must be 0 or 1",
                    ))
                }
            };
            std::env::set_var(key, if enabled { "1" } else { "0" });
            Ok(Value::Unit)
        });
    }

    pub fn register_net_stdlib(&mut self) {
        // === HTTP Operations ===
        self.define_native("http_get", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("http_get: expected 1 argument (url)"));
            }
            let url = match &args[0] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("http_get: argument must be string url")),
            };

            let res = reqwest::blocking::get(&url);

            match res {
                Ok(resp) => match resp.text() {
                    Ok(text) => Ok(Value::String(text)),
                    Err(e) => Err(KainError::runtime(format!(
                        "http_get: failed to read body: {}",
                        e
                    ))),
                },
                Err(e) => Err(KainError::runtime(format!(
                    "http_get: request failed: {}",
                    e
                ))),
            }
        });

        self.define_native("http_post_json", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "http_post: expected 2 arguments (url, json_string)",
                ));
            }
            let url = match &args[0] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("http_post: url must be string")),
            };
            let body = match &args[1] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("http_post: body must be string")),
            };

            let client = reqwest::blocking::Client::new();

            let res = client
                .post(&url)
                .header("Content-Type", "application/json")
                .body(body)
                .send();

            match res {
                Ok(resp) => match resp.text() {
                    Ok(text) => Ok(Value::String(text)),
                    Err(e) => Err(KainError::runtime(format!(
                        "http_post: failed to read response: {}",
                        e
                    ))),
                },
                Err(e) => Err(KainError::runtime(format!(
                    "http_post: request failed: {}",
                    e
                ))),
            }
        });
    }

    pub fn register_json_stdlib(&mut self) {
        self.define_native("json_parse", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "json_parse: expected 1 argument (string)",
                ));
            }
            let s = match &args[0] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("json_parse: argument must be string")),
            };

            fn from_json(v: &serde_json::Value) -> Value {
                match v {
                    serde_json::Value::Null => Value::None,
                    serde_json::Value::Bool(b) => Value::Bool(*b),
                    serde_json::Value::Number(n) => {
                        if let Some(i) = n.as_i64() {
                            Value::Int(i)
                        } else if let Some(f) = n.as_f64() {
                            Value::Float(f)
                        } else {
                            Value::Int(0) // Should match
                        }
                    }
                    serde_json::Value::String(s) => Value::String(s.clone()),
                    serde_json::Value::Array(arr) => {
                        let k_arr = arr.iter().map(from_json).collect();
                        Value::Array(Arc::new(RwLock::new(k_arr)))
                    }
                    serde_json::Value::Object(obj) => {
                        let mut map = HashMap::new();
                        for (k, v) in obj {
                            map.insert(k.clone(), from_json(v));
                        }
                        Value::Struct("Json".to_string(), Arc::new(RwLock::new(map)))
                    }
                }
            }

            match serde_json::from_str::<serde_json::Value>(s) {
                Ok(v) => Ok(from_json(&v)),
                Err(e) => Err(KainError::runtime(format!(
                    "json_parse: invalid json: {}",
                    e
                ))),
            }
        });

        self.define_native("json_string", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("json_string: expected 1 argument"));
            }

            fn to_json(v: &Value) -> serde_json::Value {
                match v {
                    Value::Unit => serde_json::Value::Null,
                    Value::None => serde_json::Value::Null,
                    Value::Bool(b) => serde_json::Value::Bool(*b),
                    Value::Int(i) => serde_json::json!(i),
                    Value::Float(f) => serde_json::json!(f),
                    Value::String(s) => serde_json::Value::String(s.clone()),
                    Value::Array(arr) => {
                        let arr = arr.read().unwrap();
                        serde_json::Value::Array(arr.iter().map(to_json).collect())
                    }
                    Value::Struct(_, fields) => {
                        let fields = fields.read().unwrap();
                        let mut map = serde_json::Map::new();
                        for (k, v) in fields.iter() {
                            map.insert(k.clone(), to_json(v));
                        }
                        serde_json::Value::Object(map)
                    }
                    Value::Tuple(items) => {
                        serde_json::Value::Array(items.iter().map(to_json).collect())
                    }
                    _ => serde_json::Value::String(format!("{}", v)), // Fallback
                }
            }

            Ok(Value::String(to_json(&args[0]).to_string()))
        });
        self.define_native("json_get", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_get: expected 2 arguments (object, key)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields,
                _ => {
                    return Err(KainError::runtime(
                        "json_get: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key,
                _ => return Err(KainError::runtime("json_get: key must be a string")),
            };

            Ok(fields
                .read()
                .unwrap()
                .get(key)
                .cloned()
                .unwrap_or(Value::None))
        });
        self.define_native("json_get_string", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_get_string: expected 2 arguments (object, key)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields,
                _ => {
                    return Err(KainError::runtime(
                        "json_get_string: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key,
                _ => return Err(KainError::runtime("json_get_string: key must be a string")),
            };

            match fields.read().unwrap().get(key) {
                Some(Value::String(value)) => Ok(Value::String(value.clone())),
                Some(other) => Err(KainError::runtime(format!(
                    "json_get_string: key '{}' did not contain a string (found {:?})",
                    key, other
                ))),
                None => Err(KainError::runtime(format!(
                    "json_get_string: missing key '{}'",
                    key
                ))),
            }
        });
        self.define_native("json_get_int", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_get_int: expected 2 arguments (object, key)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields,
                _ => {
                    return Err(KainError::runtime(
                        "json_get_int: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key,
                _ => return Err(KainError::runtime("json_get_int: key must be a string")),
            };

            match fields.read().unwrap().get(key) {
                Some(Value::Int(value)) => Ok(Value::Int(*value)),
                Some(Value::Float(value)) => Ok(Value::Int(*value as i64)),
                Some(other) => Err(KainError::runtime(format!(
                    "json_get_int: key '{}' did not contain an int-like value (found {:?})",
                    key, other
                ))),
                None => Err(KainError::runtime(format!(
                    "json_get_int: missing key '{}'",
                    key
                ))),
            }
        });
        self.define_native("json_get_bool", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_get_bool: expected 2 arguments (object, key)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields,
                _ => {
                    return Err(KainError::runtime(
                        "json_get_bool: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key,
                _ => return Err(KainError::runtime("json_get_bool: key must be a string")),
            };

            match fields.read().unwrap().get(key) {
                Some(Value::Bool(value)) => Ok(Value::Bool(*value)),
                Some(other) => Err(KainError::runtime(format!(
                    "json_get_bool: key '{}' did not contain a bool (found {:?})",
                    key, other
                ))),
                None => Err(KainError::runtime(format!(
                    "json_get_bool: missing key '{}'",
                    key
                ))),
            }
        });
        self.define_native("json_has", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_has: expected 2 arguments (object, key)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields,
                _ => {
                    return Err(KainError::runtime(
                        "json_has: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key,
                _ => return Err(KainError::runtime("json_has: key must be a string")),
            };

            Ok(Value::Bool(fields.read().unwrap().contains_key(key)))
        });
        self.define_native("json_object_new", |_env, args| {
            if !args.is_empty() {
                return Err(KainError::runtime("json_object_new: expected 0 arguments"));
            }

            Ok(Value::Struct(
                "Json".to_string(),
                Arc::new(RwLock::new(HashMap::new())),
            ))
        });
        self.define_native("json_object_set", |_env, args| {
            if args.len() != 3 {
                return Err(KainError::runtime(
                    "json_object_set: expected 3 arguments (object, key, value)",
                ));
            }

            let fields = match &args[0] {
                Value::Struct(_, fields) => fields.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "json_object_set: first argument must be a struct-backed json object",
                    ))
                }
            };
            let key = match &args[1] {
                Value::String(key) => key.clone(),
                _ => return Err(KainError::runtime("json_object_set: key must be a string")),
            };

            fields.write().unwrap().insert(key, args[2].clone());
            Ok(Value::Unit)
        });
        self.define_native("json_array_new", |_env, args| {
            if !args.is_empty() {
                return Err(KainError::runtime("json_array_new: expected 0 arguments"));
            }

            Ok(Value::Array(Arc::new(RwLock::new(Vec::new()))))
        });
        self.define_native("json_array_push", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_array_push: expected 2 arguments (array, value)",
                ));
            }

            let array = match &args[0] {
                Value::Array(array) => array.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "json_array_push: first argument must be an array",
                    ))
                }
            };

            array.write().unwrap().push(args[1].clone());
            Ok(Value::Unit)
        });
        self.define_native("json_array_len", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "json_array_len: expected 1 argument (array)",
                ));
            }

            let array = match &args[0] {
                Value::Array(array) => array,
                _ => {
                    return Err(KainError::runtime(
                        "json_array_len: argument must be an array",
                    ))
                }
            };

            Ok(Value::Int(array.read().unwrap().len() as i64))
        });
        self.define_native("json_array_get", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "json_array_get: expected 2 arguments (array, index)",
                ));
            }

            let array = match &args[0] {
                Value::Array(array) => array,
                _ => {
                    return Err(KainError::runtime(
                        "json_array_get: first argument must be an array",
                    ))
                }
            };
            let index = match &args[1] {
                Value::Int(index) if *index >= 0 => *index as usize,
                Value::Int(_) => {
                    return Err(KainError::runtime(
                        "json_array_get: index must be non-negative",
                    ))
                }
                _ => return Err(KainError::runtime("json_array_get: index must be an int")),
            };

            Ok(array
                .read()
                .unwrap()
                .get(index)
                .cloned()
                .unwrap_or(Value::None))
        });
    }

    pub fn register_stdlib(&mut self) {
        // Register built-in constants
        self.define("None".to_string(), Value::None);
        self.define("none".to_string(), Value::None); // Also lowercase for convenience

        // Some is just an identity function - returns its argument
        // This lets code use Some(value) pattern even though we don't have proper Option types
        self.define_native("Some", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("Some: expected 1 argument"));
            }
            Ok(args[0].clone())
        });

        // Register built-in functions
        self.define_native("print", |_env, args| {
            for arg in args {
                print!("{} ", arg);
            }
            Ok(Value::Unit)
        });

        self.define_native("println", |_env, args| {
            for arg in args {
                print!("{} ", arg);
            }
            println!("");
            Ok(Value::Unit)
        });

        self.define_native("eprint", |_env, args| {
            for arg in args {
                eprint!("{} ", arg);
            }
            Ok(Value::Unit)
        });

        self.define_native("eprintln", |_env, args| {
            for arg in args {
                eprint!("{} ", arg);
            }
            eprintln!("");
            Ok(Value::Unit)
        });

        // Math functions
        self.define_native("min", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("min: expected 2 arguments"));
            }
            match (&args[0], &args[1]) {
                (Value::Int(a), Value::Int(b)) => Ok(Value::Int(*a.min(b))),
                (Value::Float(a), Value::Float(b)) => Ok(Value::Float(a.min(*b))),
                _ => Err(KainError::runtime("min: arguments must be numbers")),
            }
        });

        self.define_native("max", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("max: expected 2 arguments"));
            }
            match (&args[0], &args[1]) {
                (Value::Int(a), Value::Int(b)) => Ok(Value::Int(*a.max(b))),
                (Value::Float(a), Value::Float(b)) => Ok(Value::Float(a.max(*b))),
                _ => Err(KainError::runtime("max: arguments must be numbers")),
            }
        });

        self.define_native("abs", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("abs: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Int(n.abs())),
                Value::Float(n) => Ok(Value::Float(n.abs())),
                _ => Err(KainError::runtime("abs: argument must be number")),
            }
        });

        self.define_native("sqrt", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sqrt: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Float((*n as f64).sqrt())),
                Value::Float(n) => Ok(Value::Float(n.sqrt())),
                _ => Err(KainError::runtime("sqrt: argument must be number")),
            }
        });

        // Random
        self.define_native("random", |_env, _args| {
            // Simple LCG for deterministic behavior in prototype
            // In real impl use rand crate
            use std::time::SystemTime;
            let seed = SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos() as u64;
            let x = (seed % 1000) as f64 / 1000.0;
            Ok(Value::Float(x))
        });

        self.define_native("sleep", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sleep: expected 1 argument (ms)"));
            }
            match args[0] {
                Value::Int(ms) => {
                    std::thread::sleep(std::time::Duration::from_millis(ms as u64));
                    Ok(Value::Unit)
                }
                _ => Err(KainError::runtime("sleep: argument must be int")),
            }
        });

        // Collections
        self.define_native("len", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("len: expected 1 argument"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::Int(s.len() as i64)),
                Value::Array(arr) => Ok(Value::Int(arr.read().unwrap().len() as i64)),
                _ => Err(KainError::runtime("len: argument must be string or array")),
            }
        });

        // ord: get ASCII/Unicode code of first character
        self.define_native("ord", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("ord: expected 1 argument"));
            }
            match &args[0] {
                Value::String(s) => {
                    if let Some(c) = s.chars().next() {
                        Ok(Value::Int(c as i64))
                    } else {
                        Err(KainError::runtime("ord: empty string"))
                    }
                }
                _ => Err(KainError::runtime("ord: argument must be string")),
            }
        });

        // chr: convert code to character
        self.define_native("chr", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("chr: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => {
                    if let Some(c) = char::from_u32(*n as u32) {
                        Ok(Value::String(c.to_string()))
                    } else {
                        Err(KainError::runtime("chr: invalid code point"))
                    }
                }
                _ => Err(KainError::runtime("chr: argument must be int")),
            }
        });

        self.define_native("first", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("first: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => {
                    let arr = arr.read().unwrap();
                    if arr.is_empty() {
                        return Err(KainError::runtime("first: empty array"));
                    }
                    Ok(arr[0].clone())
                }
                _ => Err(KainError::runtime("first: argument must be array")),
            }
        });

        self.define_native("last", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("last: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => {
                    let arr = arr.read().unwrap();
                    if arr.is_empty() {
                        return Err(KainError::runtime("last: empty array"));
                    }
                    Ok(arr[arr.len() - 1].clone())
                }
                _ => Err(KainError::runtime("last: argument must be array")),
            }
        });

        self.define_native("push", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("push: expected 2 arguments"));
            }
            match &args[0] {
                Value::Array(arr) => {
                    arr.write().unwrap().push(args[1].clone());
                    Ok(Value::Unit)
                }
                _ => Err(KainError::runtime("push: first argument must be array")),
            }
        });

        // Range
        self.define_native("range", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("range: expected 2 arguments"));
            }
            let start = match args[0] {
                Value::Int(n) => n,
                _ => return Err(KainError::runtime("range: expected int")),
            };
            let end = match args[1] {
                Value::Int(n) => n,
                _ => return Err(KainError::runtime("range: expected int")),
            };

            let arr = (start..end).map(Value::Int).collect();
            Ok(Value::Array(Arc::new(RwLock::new(arr))))
        });

        // Array Utils
        self.define_native("first", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("first: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => arr
                    .read()
                    .unwrap()
                    .first()
                    .cloned()
                    .ok_or_else(|| KainError::runtime("Array is empty")),
                Value::String(s) => s
                    .chars()
                    .next()
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| KainError::runtime("String is empty")),
                _ => Err(KainError::runtime("first: expected array or string")),
            }
        });

        self.define_native("last", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("last: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => arr
                    .read()
                    .unwrap()
                    .last()
                    .cloned()
                    .ok_or_else(|| KainError::runtime("Array is empty")),
                Value::String(s) => s
                    .chars()
                    .last()
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| KainError::runtime("String is empty")),
                _ => Err(KainError::runtime("last: expected array or string")),
            }
        });

        self.define_native("reverse", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("reverse: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => {
                    let mut reversed = arr.read().unwrap().clone();
                    reversed.reverse();
                    Ok(Value::Array(Arc::new(RwLock::new(reversed))))
                }
                Value::String(s) => Ok(Value::String(s.chars().rev().collect())),
                _ => Err(KainError::runtime("reverse: expected array or string")),
            }
        });

        self.define_native("sum", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sum: expected 1 argument"));
            }
            match &args[0] {
                Value::Array(arr) => {
                    let mut total = 0i64;
                    for v in arr.read().unwrap().iter() {
                        match v {
                            Value::Int(n) => total += n,
                            _ => {
                                return Err(KainError::runtime("sum: array must contain integers"))
                            }
                        }
                    }
                    Ok(Value::Int(total))
                }
                _ => Err(KainError::runtime("sum: expected array")),
            }
        });

        // === Type checks ===
        self.define_native("type_of", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("type_of: expected 1 argument"));
            }
            let type_name = match &args[0] {
                Value::Unit => "unit",
                Value::Bool(_) => "bool",
                Value::Int(_) => "int",
                Value::Float(_) => "float",
                Value::String(_) => "string",
                Value::Array(_) => "array",
                Value::Tuple(_) => "tuple",
                Value::Struct(name, _) => name.as_str(),
                Value::HostObject(name, _) => return Ok(Value::String(name.clone())),
                Value::Function(_) => "function",
                Value::Patch(_) => "patch",
                Value::Law(_) => "law",
                Value::Converge(_) => "converge",
                Value::Orchestrate(_) => "orchestrate",
                Value::NativeFn(_, _) => "native_function",
                Value::ActorRef(_) => "actor",
                Value::None => "none",
                Value::Return(_) => "return_value",
                Value::Closure(_, _, _) => "function",
                Value::Result(_, _) => "result",
                Value::StructConstructor(_, _) => "struct_constructor",
                Value::JSX(_) => "jsx",
                Value::EnumVariant(enum_name, _, _) => return Ok(Value::String(enum_name.clone())),
                Value::Poll(_, _) => "poll",
                Value::Future(name, _) => return Ok(Value::String(format!("Future<{}>", name))),
                Value::Break(_) => "break",
                Value::Continue => "continue",
            };
            Ok(Value::String(type_name.to_string()))
        });

        // Get the variant name of an enum (e.g., "Int" from Expr::Int(42))
        self.define_native("variant_of", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("variant_of: expected 1 argument"));
            }
            match &args[0] {
                Value::EnumVariant(_, variant, _) => Ok(Value::String(variant.clone())),
                _ => Ok(Value::String("".to_string())), // Not an enum variant
            }
        });

        // Get a field from an enum variant by index (0-based)
        // Example: variant_field(Expr::Binary(left, op, right), 0) returns left
        self.define_native("variant_field", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "variant_field: expected 2 arguments (enum, index)",
                ));
            }
            let idx = match &args[1] {
                Value::Int(n) => *n as usize,
                _ => return Err(KainError::runtime("variant_field: index must be int")),
            };
            match &args[0] {
                Value::EnumVariant(_, _, fields) => {
                    if idx < fields.len() {
                        let field = fields[idx].clone();
                        // Auto-unwrap Box values (Struct "Box" with field "0")
                        if let Value::Struct(name, inner) = &field {
                            if name == "Box" {
                                let inner = inner.read().unwrap();
                                if let Some(boxed) = inner.get("0") {
                                    return Ok(boxed.clone());
                                }
                            }
                        }
                        // Auto-unwrap Box::new(...) pattern (EnumVariant "Box" / "new")
                        if let Value::EnumVariant(enum_name, variant_name, inner_fields) = &field {
                            if enum_name == "Box"
                                && variant_name == "new"
                                && inner_fields.len() == 1
                            {
                                return Ok(inner_fields[0].clone());
                            }
                        }
                        Ok(field)
                    } else {
                        Err(KainError::runtime(format!(
                            "variant_field: index {} out of bounds (has {} fields)",
                            idx,
                            fields.len()
                        )))
                    }
                }
                _ => Err(KainError::runtime(
                    "variant_field: first argument must be enum variant",
                )),
            }
        });

        self.define_native("str", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("str: expected 1 argument"));
            }
            Ok(Value::String(format!("{}", args[0])))
        });

        self.define_native("int", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("int: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Int(*n)),
                Value::Float(n) => Ok(Value::Int(*n as i64)),
                Value::String(s) => s
                    .parse::<i64>()
                    .map(Value::Int)
                    .map_err(|_| KainError::runtime("Invalid int string")),
                _ => Err(KainError::runtime("int: argument must be number or string")),
            }
        });

        self.define_native("float", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("float: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Float(*n as f64)),
                Value::Float(n) => Ok(Value::Float(*n)),
                Value::String(s) => s
                    .parse::<f64>()
                    .map(Value::Float)
                    .map_err(|_| KainError::runtime("Invalid float string")),
                _ => Err(KainError::runtime(
                    "float: argument must be number or string",
                )),
            }
        });

        // === Result / Error Handling ===
        self.define_native("ok", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("ok: expected 1 argument"));
            }
            Ok(Value::Result(true, Box::new(args[0].clone())))
        });

        self.define_native("err", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("err: expected 1 argument"));
            }
            Ok(Value::Result(false, Box::new(args[0].clone())))
        });

        self.define_native("sleep", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sleep: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => {
                    std::thread::sleep(std::time::Duration::from_secs(*n as u64));
                    Ok(Value::Unit)
                }
                Value::Float(n) => {
                    std::thread::sleep(std::time::Duration::from_secs_f64(*n));
                    Ok(Value::Unit)
                }
                _ => Err(KainError::runtime("sleep: expected number")),
            }
        });

        self.define_native("now", |_env, _args| {
            let start = std::time::SystemTime::now();
            let since_the_epoch = start
                .duration_since(std::time::UNIX_EPOCH)
                .map_err(|e| KainError::runtime(&format!("Time error: {}", e)))?;
            Ok(Value::Float(since_the_epoch.as_secs_f64()))
        });

        // === Higher-Order Functions ===
        // Note: These need special handling since they take closures
        // We'll register them but they need to be called via call_function
        self.define_native("map", |env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "map: expected 2 arguments (array, function)",
                ));
            }
            let arr = match &args[0] {
                Value::Array(a) => a.read().unwrap().clone(),
                _ => return Err(KainError::runtime("map: first argument must be an array")),
            };
            let func = args[1].clone();
            let mut results = Vec::new();
            for item in arr {
                let result = call_function(env, func.clone(), vec![item])?;
                results.push(result);
            }
            Ok(Value::Array(Arc::new(RwLock::new(results))))
        });

        self.define_native("filter", |env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "filter: expected 2 arguments (array, function)",
                ));
            }
            let arr = match &args[0] {
                Value::Array(a) => a.read().unwrap().clone(),
                _ => {
                    return Err(KainError::runtime(
                        "filter: first argument must be an array",
                    ))
                }
            };
            let func = args[1].clone();
            let mut results = Vec::new();
            for item in arr {
                let result = call_function(env, func.clone(), vec![item.clone()])?;
                match result {
                    Value::Bool(true) => results.push(item),
                    Value::Bool(false) => {}
                    _ => return Err(KainError::runtime("filter: function must return bool")),
                }
            }
            Ok(Value::Array(Arc::new(RwLock::new(results))))
        });

        self.define_native("reduce", |env, args| {
            if args.len() != 3 {
                return Err(KainError::runtime(
                    "reduce: expected 3 arguments (array, initial, function)",
                ));
            }
            let arr = match &args[0] {
                Value::Array(a) => a.read().unwrap().clone(),
                _ => {
                    return Err(KainError::runtime(
                        "reduce: first argument must be an array",
                    ))
                }
            };
            let mut acc = args[1].clone();
            let func = args[2].clone();
            for item in arr {
                acc = call_function(env, func.clone(), vec![acc, item])?;
            }
            Ok(acc)
        });

        self.define_native("foreach", |env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "foreach: expected 2 arguments (array, function)",
                ));
            }
            let arr = match &args[0] {
                Value::Array(a) => a.read().unwrap().clone(),
                _ => {
                    return Err(KainError::runtime(
                        "foreach: first argument must be an array",
                    ))
                }
            };
            let func = args[1].clone();
            for item in arr {
                call_function(env, func.clone(), vec![item])?;
            }
            Ok(Value::Unit)
        });

        // === File I/O ===
        self.define_native("read_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "read_file", "path")?;
            runtime_fs_strict("read_file", kain_fs::read_text(path).map(Value::String))
        });

        self.define_native("write_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "write_file", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "write_file", "content")?;
            runtime_fs_strict_unit("write_file", kain_fs::write_text(path, content))
        });

        self.define_native("fs_read_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_read_text", "path")?;
            runtime_fs_strict("fs_read_text", kain_fs::read_text(path).map(Value::String))
        });

        self.define_native("fs_try_read_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_read_text", "path")?;
            runtime_fs_result(kain_fs::read_text(path).map(Value::String))
        });

        self.define_native("fs_write_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_write_text", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "fs_write_text", "content")?;
            runtime_fs_strict_unit("fs_write_text", kain_fs::write_text(path, content))
        });

        self.define_native("fs_try_write_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_write_text", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "fs_try_write_text", "content")?;
            runtime_fs_result(kain_fs::write_text(path, content).map(|_| Value::Unit))
        });

        self.define_native("fs_append_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_append_text", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "fs_append_text", "content")?;
            runtime_fs_strict_unit("fs_append_text", kain_fs::append_text(path, content))
        });

        self.define_native("fs_try_append_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_append_text", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "fs_try_append_text", "content")?;
            runtime_fs_result(kain_fs::append_text(path, content).map(|_| Value::Unit))
        });

        self.define_native("fs_read_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_read_bytes", "path")?;
            runtime_fs_strict(
                "fs_read_bytes",
                kain_fs::read_bytes(path).map(|bytes| runtime_byte_array_value(&bytes)),
            )
        });

        self.define_native("fs_try_read_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_read_bytes", "path")?;
            runtime_fs_result(
                kain_fs::read_bytes(path).map(|bytes| runtime_byte_array_value(&bytes)),
            )
        });

        self.define_native("fs_write_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_write_bytes", "path")?;
            let bytes = runtime_expect_byte_array_arg(&args, 1, "fs_write_bytes", "bytes")?;
            runtime_fs_strict_unit("fs_write_bytes", kain_fs::write_bytes(path, &bytes))
        });

        self.define_native("fs_try_write_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_write_bytes", "path")?;
            let bytes = runtime_expect_byte_array_arg(&args, 1, "fs_try_write_bytes", "bytes")?;
            runtime_fs_result(kain_fs::write_bytes(path, &bytes).map(|_| Value::Unit))
        });

        self.define_native("fs_append_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_append_bytes", "path")?;
            let bytes = runtime_expect_byte_array_arg(&args, 1, "fs_append_bytes", "bytes")?;
            runtime_fs_strict_unit("fs_append_bytes", kain_fs::append_bytes(path, &bytes))
        });

        self.define_native("fs_try_append_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_append_bytes", "path")?;
            let bytes = runtime_expect_byte_array_arg(&args, 1, "fs_try_append_bytes", "bytes")?;
            runtime_fs_result(kain_fs::append_bytes(path, &bytes).map(|_| Value::Unit))
        });

        // === String Functions ===
        self.define_native("split", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "split: expected 2 arguments (string, delimiter)",
                ));
            }
            let s = match &args[0] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("split: first argument must be a string")),
            };
            let delim = match &args[1] {
                Value::String(s) => s.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "split: second argument must be a string",
                    ))
                }
            };
            // Handle empty delimiter specially - split into individual characters
            let parts: Vec<Value> = if delim.is_empty() {
                s.chars().map(|c| Value::String(c.to_string())).collect()
            } else {
                s.split(&delim)
                    .map(|p| Value::String(p.to_string()))
                    .collect()
            };
            Ok(Value::Array(Arc::new(RwLock::new(parts))))
        });

        self.define_native("join", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "join: expected 2 arguments (array, delimiter)",
                ));
            }
            let arr = match &args[0] {
                Value::Array(a) => a.read().unwrap().clone(),
                _ => return Err(KainError::runtime("join: first argument must be an array")),
            };
            let delim = match &args[1] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("join: second argument must be a string")),
            };
            let parts: Vec<String> = arr.iter().map(|v| format!("{}", v)).collect();
            Ok(Value::String(parts.join(&delim)))
        });

        self.define_native("trim", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("trim: expected 1 argument (string)"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::String(s.trim().to_string())),
                _ => Err(KainError::runtime("trim: argument must be a string")),
            }
        });

        self.define_native("to_upper", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("to_upper: expected 1 argument (string)"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::String(s.to_uppercase())),
                _ => Err(KainError::runtime("to_upper: argument must be a string")),
            }
        });

        self.define_native("upper", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("upper: expected 1 argument (string)"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::String(s.to_uppercase())),
                _ => Err(KainError::runtime("upper: argument must be a string")),
            }
        });

        self.define_native("to_lower", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("to_lower: expected 1 argument (string)"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::String(s.to_lowercase())),
                _ => Err(KainError::runtime("to_lower: argument must be a string")),
            }
        });

        self.define_native("lower", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("lower: expected 1 argument (string)"));
            }
            match &args[0] {
                Value::String(s) => Ok(Value::String(s.to_lowercase())),
                _ => Err(KainError::runtime("lower: argument must be a string")),
            }
        });

        self.define_native("contains", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime(
                    "contains: expected 2 arguments (string, pattern)",
                ));
            }
            let s = match &args[0] {
                Value::String(s) => s.clone(),
                Value::Array(arr) => {
                    // Support array.contains(element) for various types
                    let needle = &args[1];
                    return Ok(Value::Bool(arr.read().unwrap().iter().any(|v| {
                        match (v, needle) {
                            (Value::Int(n1), Value::Int(n2)) => n1 == n2,
                            (Value::String(s1), Value::String(s2)) => s1 == s2,
                            (Value::Bool(b1), Value::Bool(b2)) => b1 == b2,
                            _ => false,
                        }
                    })));
                }
                _ => {
                    return Err(KainError::runtime(
                        "contains: first argument must be a string or array",
                    ))
                }
            };
            let sub = match &args[1] {
                Value::String(s) => s.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "contains: second argument must be a string",
                    ))
                }
            };
            Ok(Value::Bool(s.contains(&sub)))
        });

        self.define_native("starts_with", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("starts_with: expected 2 arguments"));
            }
            let s = match &args[0] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            let sub = match &args[1] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            Ok(Value::Bool(s.starts_with(sub)))
        });

        self.define_native("ends_with", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("ends_with: expected 2 arguments"));
            }
            let s = match &args[0] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            let sub = match &args[1] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            Ok(Value::Bool(s.ends_with(sub)))
        });

        self.define_native("replace", |_env, args| {
            if args.len() != 3 {
                return Err(KainError::runtime(
                    "replace: expected 3 arguments (string, from, to)",
                ));
            }
            let s = match &args[0] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            let from = match &args[1] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            let to = match &args[2] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            Ok(Value::String(s.replace(from, to)))
        });

        self.define_native("char_at", |_env, args| {
            if args.len() != 2 {
                return Err(KainError::runtime("char_at: expected 2 arguments"));
            }
            let s = match &args[0] {
                Value::String(s) => s,
                _ => return Err(KainError::runtime("expected string")),
            };
            let idx = match &args[1] {
                Value::Int(n) => *n,
                _ => return Err(KainError::runtime("expected int")),
            };
            if idx < 0 {
                return Ok(Value::String(String::new()));
            }
            match s.chars().nth(idx as usize) {
                Some(c) => Ok(Value::String(c.to_string())),
                None => Ok(Value::String(String::new())),
            }
        });

        self.define_native("substring", |_env, args| {
            if args.len() < 2 || args.len() > 3 {
                return Err(KainError::runtime(
                    "substring: expected 2-3 arguments (string, start, [end])",
                ));
            }
            let s = match &args[0] {
                Value::String(s) => s.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "substring: first argument must be a string",
                    ))
                }
            };
            let start = match &args[1] {
                Value::Int(n) => *n as usize,
                _ => {
                    return Err(KainError::runtime(
                        "substring: second argument must be an integer",
                    ))
                }
            };
            let end = if args.len() == 3 {
                match &args[2] {
                    Value::Int(n) => *n as usize,
                    _ => {
                        return Err(KainError::runtime(
                            "substring: third argument must be an integer",
                        ))
                    }
                }
            } else {
                s.len()
            };
            let chars: String = s.chars().skip(start).take(end - start).collect();
            Ok(Value::String(chars))
        });

        // === Actor System ===

        self.define_native("send", |_env, args| {
            if args.len() < 2 {
                return Err(KainError::runtime(
                    "send: expected at least 2 arguments (actor, msg_name)",
                ));
            }
            let actor_ref = match &args[0] {
                Value::ActorRef(r) => r,
                _ => return Err(KainError::runtime("send: first argument must be actor ref")),
            };
            let msg_name = match &args[1] {
                Value::String(s) => s.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "send: second argument must be message name",
                    ))
                }
            };

            let msg_args = args[2..].to_vec();

            let _ = actor_ref.sender.send(Message {
                name: msg_name,
                args: msg_args,
            });

            Ok(Value::Unit)
        });

        self.define_native("ask", |env, args| {
            if args.len() != 3 {
                return Err(KainError::runtime(
                    "ask: expected 3 arguments (actor, msg_name, request)",
                ));
            }

            let actor_ref = match &args[0] {
                Value::ActorRef(r) => r.clone(),
                _ => return Err(KainError::runtime("ask: first argument must be actor ref")),
            };
            let msg_name = match &args[1] {
                Value::String(s) => s.clone(),
                _ => return Err(KainError::runtime("ask: second argument must be string")),
            };

            let reply_id = env.actor_ids.allocate();

            let (reply_tx, reply_rx) = flume::unbounded();
            let reply_actor_ref = ActorRef {
                id: reply_id,
                sender: reply_tx,
            };

            let mut msg_args = Vec::with_capacity(2);
            msg_args.push(Value::ActorRef(reply_actor_ref));
            msg_args.push(args[2].clone());

            if actor_ref
                .sender
                .send(Message {
                    name: msg_name,
                    args: msg_args,
                })
                .is_err()
            {
                return Err(KainError::runtime(
                    "ask: failed to send request to target actor",
                ));
            }

            match reply_rx.recv_timeout(std::time::Duration::from_millis(DEFAULT_ASK_TIMEOUT_MS)) {
                Ok(message) => match message.args.len() {
                    0 => Ok(Value::Unit),
                    1 => Ok(message.args[0].clone()),
                    _ => Ok(Value::Tuple(message.args)),
                },
                Err(_) => Err(KainError::runtime("ask: timed out waiting for actor reply")),
            }
        });
        self.define_native("ask_timeout", |env, args| {
            if args.len() != 4 {
                return Err(KainError::runtime(
                    "ask_timeout: expected 4 arguments (actor, msg_name, request, timeout_ms)",
                ));
            }

            let actor_ref = match &args[0] {
                Value::ActorRef(r) => r.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "ask_timeout: first argument must be actor ref",
                    ))
                }
            };
            let msg_name = match &args[1] {
                Value::String(s) => s.clone(),
                _ => {
                    return Err(KainError::runtime(
                        "ask_timeout: second argument must be string",
                    ))
                }
            };
            let timeout_ms = match &args[3] {
                Value::Int(value) if *value >= 0 => *value as u64,
                Value::Int(_) => {
                    return Err(KainError::runtime(
                        "ask_timeout: timeout must be non-negative",
                    ))
                }
                _ => return Err(KainError::runtime("ask_timeout: timeout must be an int")),
            };

            let reply_id = env.actor_ids.allocate();

            let (reply_tx, reply_rx) = flume::unbounded();
            let reply_actor_ref = ActorRef {
                id: reply_id,
                sender: reply_tx,
            };

            let mut msg_args = Vec::with_capacity(2);
            msg_args.push(Value::ActorRef(reply_actor_ref));
            msg_args.push(args[2].clone());

            if actor_ref
                .sender
                .send(Message {
                    name: msg_name,
                    args: msg_args,
                })
                .is_err()
            {
                return Err(KainError::runtime(
                    "ask_timeout: failed to send request to target actor",
                ));
            }

            match reply_rx.recv_timeout(std::time::Duration::from_millis(timeout_ms)) {
                Ok(message) => match message.args.len() {
                    0 => Ok(Value::Unit),
                    1 => Ok(message.args[0].clone()),
                    _ => Ok(Value::Tuple(message.args)),
                },
                Err(_) => Err(KainError::runtime(format!(
                    "ask_timeout: timed out waiting for actor reply after {} ms",
                    timeout_ms
                ))),
            }
        });

        self.define_native("sleep", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sleep: expected 1 argument (ms)"));
            }
            let ms = match args[0] {
                Value::Int(i) => i as u64,
                _ => return Err(KainError::runtime("sleep: expected int")),
            };
            std::thread::sleep(std::time::Duration::from_millis(ms));
            Ok(Value::Unit)
        });

        // === Utility Functions ===
        self.define_native("time", |_env, _args| {
            use std::time::{SystemTime, UNIX_EPOCH};
            let now = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default();
            Ok(Value::Float(now.as_secs_f64()))
        });

        self.define_native("exit", |_env, args| {
            let code = if args.len() > 0 {
                match args[0] {
                    Value::Int(n) => n as i32,
                    _ => 0,
                }
            } else {
                0
            };
            std::process::exit(code);
        });

        self.define_native("env", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("env: expected 1 argument"));
            }
            match &args[0] {
                Value::String(key) => match std::env::var(key) {
                    Ok(v) => Ok(Value::String(v)),
                    Err(_) => Ok(Value::String(String::new())),
                },
                _ => Err(KainError::runtime("env: expected string key")),
            }
        });
        self.define_native("cwd", |_env, args| {
            if !args.is_empty() {
                return Err(KainError::runtime("cwd: expected 0 arguments"));
            }

            let cwd = std::env::current_dir()
                .map_err(|err| KainError::runtime(format!("cwd failed: {}", err)))?;
            Ok(Value::String(cwd.to_string_lossy().into_owned()))
        });

        // args() -> Array<String>: returns the command-line arguments as an array of strings.
        // The first element is the program name; subsequent elements are user-supplied args.
        // Used by kainc.kn and other selfhost CLI scripts.
        self.define_native("args", |_env, _args| {
            let argv: Vec<Value> = std::env::args().map(|a| Value::String(a)).collect();
            Ok(Value::Array(Arc::new(RwLock::new(argv))))
        });
        self.define_native("command_run", |_env, args| {
            if args.len() != 3 {
                return Err(KainError::runtime(
                    "command_run: expected 3 arguments (program, args, workdir)",
                ));
            }

            let program = match &args[0] {
                Value::String(program) => program.clone(),
                _ => return Err(KainError::runtime("command_run: program must be a string")),
            };
            let argument_values = match &args[1] {
                Value::Array(values) => values.read().unwrap().clone(),
                _ => {
                    return Err(KainError::runtime(
                        "command_run: args must be an array of strings",
                    ))
                }
            };
            let mut command_args = Vec::with_capacity(argument_values.len());
            for value in argument_values {
                match value {
                    Value::String(text) => command_args.push(text),
                    other => {
                        return Err(KainError::runtime(format!(
                            "command_run: args must contain only strings, found {:?}",
                            other
                        )))
                    }
                }
            }
            let workdir = match &args[2] {
                Value::String(workdir) => workdir.clone(),
                _ => return Err(KainError::runtime("command_run: workdir must be a string")),
            };

            let mut fields = HashMap::new();
            fields.insert("program".to_string(), Value::String(program.clone()));
            fields.insert("workdir".to_string(), Value::String(workdir.clone()));
            fields.insert(
                "args".to_string(),
                Value::Array(Arc::new(RwLock::new(
                    command_args
                        .iter()
                        .cloned()
                        .map(Value::String)
                        .collect::<Vec<_>>(),
                ))),
            );

            let mut command = Command::new(&program);
            command.args(&command_args);
            if !workdir.is_empty() {
                command.current_dir(&workdir);
            }

            match command.output() {
                Ok(output) => {
                    fields.insert(
                        "stdout".to_string(),
                        Value::String(String::from_utf8_lossy(&output.stdout).into_owned()),
                    );
                    fields.insert(
                        "stderr".to_string(),
                        Value::String(String::from_utf8_lossy(&output.stderr).into_owned()),
                    );
                    fields.insert(
                        "status".to_string(),
                        Value::Int(output.status.code().unwrap_or(-1) as i64),
                    );
                    fields.insert("success".to_string(), Value::Bool(output.status.success()));
                }
                Err(err) => {
                    fields.insert("stdout".to_string(), Value::String(String::new()));
                    fields.insert("stderr".to_string(), Value::String(err.to_string()));
                    fields.insert("status".to_string(), Value::Int(-1));
                    fields.insert("success".to_string(), Value::Bool(false));
                }
            }

            Ok(Value::Struct(
                "CommandRunResult".to_string(),
                Arc::new(RwLock::new(fields)),
            ))
        });

        self.define_native("assert", |_env, args| {
            if args.len() < 1 {
                return Err(KainError::runtime("assert: expected condition"));
            }
            match &args[0] {
                Value::Bool(true) => Ok(Value::Unit),
                _ => {
                    let msg = if args.len() > 1 {
                        format!("{}", args[1])
                    } else {
                        "Assertion failed".to_string()
                    };
                    Err(KainError::runtime(msg))
                }
            }
        });

        self.define_native("panic", |_env, args| {
            let msg = if args.len() > 0 {
                format!("{}", args[0])
            } else {
                "Panic".to_string()
            };
            Err(KainError::runtime(msg))
        });

        // Debug
        self.define_native("dbg", |_env, args| {
            for arg in args {
                println!("[DEBUG] {:?}", arg);
            }
            Ok(Value::Unit)
        });

        // Conversion
        self.define_native("int", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("int: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Int(*n)),
                Value::Float(n) => Ok(Value::Int(*n as i64)),
                Value::String(s) => s
                    .parse::<i64>()
                    .map(Value::Int)
                    .map_err(|_| KainError::runtime(format!("Cannot parse '{}' as int", s))),
                Value::Bool(b) => Ok(Value::Int(if *b { 1 } else { 0 })),
                _ => Err(KainError::runtime("int: cannot convert this type")),
            }
        });

        self.define_native("float", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("float: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Float(*n as f64)),
                Value::Float(n) => Ok(Value::Float(*n)),
                Value::String(s) => s
                    .parse::<f64>()
                    .map(Value::Float)
                    .map_err(|_| KainError::runtime(format!("Cannot parse '{}' as float", s))),
                _ => Err(KainError::runtime("float: cannot convert this type")),
            }
        });

        self.define_native("str", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("str: expected 1 argument"));
            }
            Ok(Value::String(format!("{}", &args[0])))
        });

        // Alias for str
        self.define_native("to_string", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("to_string: expected 1 argument"));
            }
            Ok(Value::String(format!("{}", &args[0])))
        });

        self.define_native("bool", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("bool: expected 1 argument"));
            }
            let result = match &args[0] {
                Value::Bool(b) => *b,
                Value::Int(n) => *n != 0,
                Value::Float(n) => *n != 0.0,
                Value::String(s) => !s.is_empty(),
                Value::None => false,
                Value::Unit => false,
                _ => true,
            };
            Ok(Value::Bool(result))
        });

        // Legacy helpers
        self.define_native("to_int", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("to_int: expected 1 argument"));
            }
            match &args[0] {
                Value::Int(n) => Ok(Value::Int(*n)),
                Value::Float(n) => Ok(Value::Int(*n as i64)),
                Value::String(s) => s
                    .parse::<i64>()
                    .map(Value::Int)
                    .map_err(|_| KainError::runtime(format!("Cannot parse '{}' as int", s))),
                Value::Bool(b) => Ok(Value::Int(if *b { 1 } else { 0 })),
                _ => Err(KainError::runtime("to_int: cannot convert this type")),
            }
        });

        // === Math ===
        self.define_native("sqrt", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sqrt: expected 1 argument"));
            }
            match args[0] {
                Value::Int(n) => Ok(Value::Float((n as f64).sqrt())),
                Value::Float(n) => Ok(Value::Float(n.sqrt())),
                _ => Err(KainError::runtime("sqrt: expected number")),
            }
        });

        self.define_native("sin", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("sin: expected 1 argument"));
            }
            match args[0] {
                Value::Int(n) => Ok(Value::Float((n as f64).sin())),
                Value::Float(n) => Ok(Value::Float(n.sin())),
                _ => Err(KainError::runtime("sin: expected number")),
            }
        });

        self.define_native("cos", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("cos: expected 1 argument"));
            }
            match args[0] {
                Value::Int(n) => Ok(Value::Float((n as f64).cos())),
                Value::Float(n) => Ok(Value::Float(n.cos())),
                _ => Err(KainError::runtime("cos: expected number")),
            }
        });

        self.define_native("tan", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("tan: expected 1 argument"));
            }
            match args[0] {
                Value::Int(n) => Ok(Value::Float((n as f64).tan())),
                Value::Float(n) => Ok(Value::Float(n.tan())),
                _ => Err(KainError::runtime("tan: expected number")),
            }
        });

        // === I/O ===
        self.define_native("read_line", |_env, _args| {
            Ok(Value::String(runtime_read_stdin_line()))
        });

        self.define_native("stdout_write", |_env, args| {
            use std::io::{self, Write};
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "stdout_write: expected 1 argument (string)",
                ));
            }
            let text = match &args[0] {
                Value::String(text) => text,
                _ => return Err(KainError::runtime("stdout_write: argument must be string")),
            };
            let mut stdout = io::stdout().lock();
            stdout
                .write_all(text.as_bytes())
                .map_err(|err| KainError::runtime(format!("stdout_write failed: {}", err)))?;
            stdout
                .flush()
                .map_err(|err| KainError::runtime(format!("stdout_write flush failed: {}", err)))?;
            Ok(Value::Unit)
        });

        self.define_native("stdin_read_exact", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "stdin_read_exact: expected 1 argument (length)",
                ));
            }
            let length = match &args[0] {
                Value::Int(length) if *length >= 0 => *length as usize,
                Value::Int(_) => {
                    return Err(KainError::runtime(
                        "stdin_read_exact: length must be non-negative",
                    ))
                }
                _ => {
                    return Err(KainError::runtime(
                        "stdin_read_exact: argument must be an integer",
                    ))
                }
            };
            Ok(Value::String(runtime_read_stdin_exact(length)?))
        });

        self.define_native("kain_input_reset", |_env, _args| {
            RUNTIME_INPUT_SESSIONS.write().unwrap().clear();
            RUNTIME_INPUT_NEXT_SESSION_ID.store(1, Ordering::SeqCst);
            Ok(Value::Int(0))
        });

        self.define_native("kain_input_session_create", |_env, args| {
            let name = runtime_expect_string_arg(&args, 0, "kain_input_session_create", "name")?;
            let id = RUNTIME_INPUT_NEXT_SESSION_ID.fetch_add(1, Ordering::SeqCst);
            RUNTIME_INPUT_SESSIONS
                .write()
                .unwrap()
                .insert(id, InputSession::new(id, name));
            Ok(Value::Int(id))
        });

        self.define_native("kain_input_session_destroy", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_session_destroy", "session_id")?;
            let removed = RUNTIME_INPUT_SESSIONS
                .write()
                .unwrap()
                .remove(&session_id)
                .is_some();
            Ok(Value::Int(if removed { 0 } else { -1 }))
        });

        self.define_native("kain_input_bind_action", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_bind_action", "session_id")?;
            let source_kind =
                runtime_expect_string_arg(&args, 1, "kain_input_bind_action", "source_kind")?;
            let event_kind =
                runtime_expect_string_arg(&args, 2, "kain_input_bind_action", "event_kind")?;
            let code = runtime_expect_string_arg(&args, 3, "kain_input_bind_action", "code")?;
            let action = runtime_expect_string_arg(&args, 4, "kain_input_bind_action", "action")?;
            runtime_input_with_session_mut(session_id, "kain_input_bind_action", |session| {
                session.bind_action(source_kind, event_kind, code, action);
                Ok(Value::Int(0))
            })
        });

        self.define_native("kain_input_bind_axis", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_bind_axis", "session_id")?;
            let source_kind =
                runtime_expect_string_arg(&args, 1, "kain_input_bind_axis", "source_kind")?;
            let event_kind =
                runtime_expect_string_arg(&args, 2, "kain_input_bind_axis", "event_kind")?;
            let code = runtime_expect_string_arg(&args, 3, "kain_input_bind_axis", "code")?;
            let axis = runtime_expect_string_arg(&args, 4, "kain_input_bind_axis", "axis")?;
            let scale = runtime_expect_number_arg(&args, 5, "kain_input_bind_axis", "scale")?;
            runtime_input_with_session_mut(session_id, "kain_input_bind_axis", |session| {
                session.bind_axis(source_kind, event_kind, code, axis, scale);
                Ok(Value::Int(0))
            })
        });

        self.define_native("kain_input_push_event", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_push_event", "session_id")?;
            let source_kind =
                runtime_expect_string_arg(&args, 1, "kain_input_push_event", "source_kind")?;
            let source_id =
                runtime_expect_string_arg(&args, 2, "kain_input_push_event", "source_id")?;
            let event_kind =
                runtime_expect_string_arg(&args, 3, "kain_input_push_event", "event_kind")?;
            let code = runtime_expect_string_arg(&args, 4, "kain_input_push_event", "code")?;
            let value = runtime_expect_number_arg(&args, 5, "kain_input_push_event", "value")?;
            let text = runtime_expect_string_arg(&args, 6, "kain_input_push_event", "text")?;
            let confidence =
                runtime_expect_number_arg(&args, 7, "kain_input_push_event", "confidence")?;
            let mut event = InputEvent::new(
                InputSource::custom(source_kind, source_id),
                runtime_input_kind(event_kind),
            );
            event.kind = event_kind.to_string();
            event.code = code.to_string();
            event.value = value;
            event.text = text.to_string();
            event.confidence = confidence;
            runtime_input_with_session_mut(session_id, "kain_input_push_event", |session| {
                Ok(Value::Int(session.push_event(event) as i64))
            })
        });

        self.define_native("kain_input_push_agent_intent", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_push_agent_intent", "session_id")?;
            let source_id =
                runtime_expect_string_arg(&args, 1, "kain_input_push_agent_intent", "source_id")?;
            let action =
                runtime_expect_string_arg(&args, 2, "kain_input_push_agent_intent", "action")?;
            let command_text = runtime_expect_string_arg(
                &args,
                3,
                "kain_input_push_agent_intent",
                "command_text",
            )?;
            let confidence =
                runtime_expect_number_arg(&args, 4, "kain_input_push_agent_intent", "confidence")?;
            let event = InputEvent::agent_intent(source_id, action, command_text, confidence);
            runtime_input_with_session_mut(session_id, "kain_input_push_agent_intent", |session| {
                Ok(Value::Int(session.push_event(event) as i64))
            })
        });

        self.define_native("kain_input_begin_frame", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_begin_frame", "session_id")?;
            let delta_ms =
                runtime_expect_number_arg(&args, 1, "kain_input_begin_frame", "delta_ms")?;
            runtime_input_with_session_mut(session_id, "kain_input_begin_frame", |session| {
                Ok(Value::Int(session.begin_frame(delta_ms).frame_index as i64))
            })
        });

        self.define_native("kain_input_frame_index", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_frame_index", "session_id")?;
            runtime_input_with_session(session_id, "kain_input_frame_index", |session| {
                Ok(Value::Int(session.current_frame().frame_index as i64))
            })
        });

        self.define_native("kain_input_event_count", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_event_count", "session_id")?;
            runtime_input_with_session(session_id, "kain_input_event_count", |session| {
                Ok(Value::Int(session.current_frame().events.len() as i64))
            })
        });

        self.define_native("kain_input_event_kind", |_env, args| {
            runtime_input_frame_event_string(&args, "kain_input_event_kind", |event| {
                event.kind.as_str()
            })
        });

        self.define_native("kain_input_event_source_kind", |_env, args| {
            runtime_input_frame_event_string(&args, "kain_input_event_source_kind", |event| {
                event.source.kind.as_str()
            })
        });

        self.define_native("kain_input_event_code", |_env, args| {
            runtime_input_frame_event_string(&args, "kain_input_event_code", |event| {
                event.code.as_str()
            })
        });

        self.define_native("kain_input_event_action", |_env, args| {
            runtime_input_frame_event_string(&args, "kain_input_event_action", |event| {
                event.action.as_str()
            })
        });

        self.define_native("kain_input_event_text", |_env, args| {
            runtime_input_frame_event_string(&args, "kain_input_event_text", |event| {
                event.text.as_str()
            })
        });

        self.define_native("kain_input_action_pressed", |_env, args| {
            runtime_input_action_query(&args, "kain_input_action_pressed", |frame, action| {
                frame.action_pressed(action)
            })
        });

        self.define_native("kain_input_action_down", |_env, args| {
            runtime_input_action_query(&args, "kain_input_action_down", |frame, action| {
                frame.action_down(action)
            })
        });

        self.define_native("kain_input_action_released", |_env, args| {
            runtime_input_action_query(&args, "kain_input_action_released", |frame, action| {
                frame.action_released(action)
            })
        });

        self.define_native("kain_input_axis_value", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_axis_value", "session_id")?;
            let axis = runtime_expect_string_arg(&args, 1, "kain_input_axis_value", "axis")?;
            runtime_input_with_session(session_id, "kain_input_axis_value", |session| {
                Ok(Value::Float(session.current_frame().axis_value(axis)))
            })
        });

        self.define_native("kain_input_text_commit_count", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_text_commit_count", "session_id")?;
            runtime_input_with_session(session_id, "kain_input_text_commit_count", |session| {
                Ok(Value::Int(session.current_frame().text_commits.len() as i64))
            })
        });

        self.define_native("kain_input_text_commit", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_text_commit", "session_id")?;
            let index =
                runtime_expect_non_negative_int_arg(&args, 1, "kain_input_text_commit", "index")?
                    as usize;
            runtime_input_with_session(session_id, "kain_input_text_commit", |session| {
                Ok(Value::String(
                    session
                        .current_frame()
                        .text_commits
                        .get(index)
                        .cloned()
                        .unwrap_or_default(),
                ))
            })
        });

        self.define_native("kain_input_trace_json", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_trace_json", "session_id")?;
            runtime_input_with_session(session_id, "kain_input_trace_json", |session| {
                session
                    .trace()
                    .to_json()
                    .map(Value::String)
                    .map_err(|err| KainError::runtime(format!("kain_input_trace_json: {err}")))
            })
        });

        self.define_native("kain_input_replay_trace_json", |_env, args| {
            let session_id =
                runtime_expect_int_arg(&args, 0, "kain_input_replay_trace_json", "session_id")?;
            let trace_json =
                runtime_expect_string_arg(&args, 1, "kain_input_replay_trace_json", "trace_json")?;
            let trace = kain_input::InputTrace::from_json(trace_json).map_err(|err| {
                KainError::runtime(format!("kain_input_replay_trace_json: {err}"))
            })?;
            runtime_input_with_session_mut(session_id, "kain_input_replay_trace_json", |session| {
                session.replay_trace(&trace);
                Ok(Value::Int(0))
            })
        });

        self.define_native("file_exists", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "file_exists", "path")?;
            Ok(Value::Bool(kain_fs::exists(path)))
        });

        self.define_native("fs_exists", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_exists", "path")?;
            Ok(Value::Bool(kain_fs::exists(path)))
        });

        self.define_native("fs_is_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_is_file", "path")?;
            Ok(Value::Bool(kain_fs::is_file(path)))
        });

        self.define_native("fs_is_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_is_dir", "path")?;
            Ok(Value::Bool(kain_fs::is_dir(path)))
        });

        self.define_native("fs_is_symlink", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_is_symlink", "path")?;
            Ok(Value::Bool(kain_fs::is_symlink(path)))
        });

        self.define_native("fs_metadata", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_metadata", "path")?;
            runtime_fs_strict(
                "fs_metadata",
                kain_fs::metadata(path).map(runtime_fs_metadata_value),
            )
        });

        self.define_native("fs_try_metadata", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_metadata", "path")?;
            runtime_fs_result(kain_fs::metadata(path).map(runtime_fs_metadata_value))
        });

        self.define_native("fs_symlink_metadata", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_symlink_metadata", "path")?;
            runtime_fs_strict(
                "fs_symlink_metadata",
                kain_fs::symlink_metadata(path).map(runtime_fs_metadata_value),
            )
        });

        self.define_native("fs_try_symlink_metadata", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_symlink_metadata", "path")?;
            runtime_fs_result(kain_fs::symlink_metadata(path).map(runtime_fs_metadata_value))
        });

        self.define_native("create_dir_all", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "create_dir_all", "path")?;
            runtime_fs_strict_unit("create_dir_all", kain_fs::create_dir_all(path))
        });

        self.define_native("fs_create_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_create_dir", "path")?;
            runtime_fs_strict_unit("fs_create_dir", kain_fs::create_dir(path))
        });

        self.define_native("fs_try_create_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_create_dir", "path")?;
            runtime_fs_result(kain_fs::create_dir(path).map(|_| Value::Unit))
        });

        self.define_native("fs_create_dir_all", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_create_dir_all", "path")?;
            runtime_fs_strict_unit("fs_create_dir_all", kain_fs::create_dir_all(path))
        });

        self.define_native("fs_try_create_dir_all", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_create_dir_all", "path")?;
            runtime_fs_result(kain_fs::create_dir_all(path).map(|_| Value::Unit))
        });

        self.define_native("copy_file", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "copy_file", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "copy_file", "dest")?;
            runtime_fs_strict_unit("copy_file", kain_fs::copy_file(src, dest).map(|_| ()))
        });

        self.define_native("fs_copy_file", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_copy_file", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_copy_file", "dest")?;
            runtime_fs_strict_unit("fs_copy_file", kain_fs::copy_file(src, dest).map(|_| ()))
        });

        self.define_native("fs_try_copy_file", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_try_copy_file", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_try_copy_file", "dest")?;
            runtime_fs_result(kain_fs::copy_file(src, dest).map(|bytes| Value::Int(bytes as i64)))
        });

        self.define_native("fs_copy_path", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_copy_path", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_copy_path", "dest")?;
            runtime_fs_strict_unit("fs_copy_path", kain_fs::copy_path(src, dest))
        });

        self.define_native("fs_try_copy_path", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_try_copy_path", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_try_copy_path", "dest")?;
            runtime_fs_result(kain_fs::copy_path(src, dest).map(|_| Value::Unit))
        });

        self.define_native("fs_move_path", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_move_path", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_move_path", "dest")?;
            runtime_fs_strict_unit("fs_move_path", kain_fs::move_path(src, dest))
        });

        self.define_native("fs_try_move_path", |_env, args| {
            let src = runtime_expect_string_arg(&args, 0, "fs_try_move_path", "src")?;
            let dest = runtime_expect_string_arg(&args, 1, "fs_try_move_path", "dest")?;
            runtime_fs_result(kain_fs::move_path(src, dest).map(|_| Value::Unit))
        });

        self.define_native("remove_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "remove_file", "path")?;
            runtime_fs_strict_unit("remove_file", kain_fs::remove_file(path))
        });

        self.define_native("fs_remove_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_remove_file", "path")?;
            runtime_fs_strict_unit("fs_remove_file", kain_fs::remove_file(path))
        });

        self.define_native("fs_try_remove_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_remove_file", "path")?;
            runtime_fs_result(kain_fs::remove_file(path).map(|_| Value::Unit))
        });

        self.define_native("fs_remove_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_remove_dir", "path")?;
            runtime_fs_strict_unit("fs_remove_dir", kain_fs::remove_dir(path))
        });

        self.define_native("fs_try_remove_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_remove_dir", "path")?;
            runtime_fs_result(kain_fs::remove_dir(path).map(|_| Value::Unit))
        });

        self.define_native("fs_remove_dir_all", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_remove_dir_all", "path")?;
            runtime_fs_strict_unit("fs_remove_dir_all", kain_fs::remove_dir_all(path))
        });

        self.define_native("fs_try_remove_dir_all", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_remove_dir_all", "path")?;
            runtime_fs_result(kain_fs::remove_dir_all(path).map(|_| Value::Unit))
        });

        self.define_native("fs_remove_path", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_remove_path", "path")?;
            runtime_fs_strict_unit("fs_remove_path", kain_fs::remove_path(path))
        });

        self.define_native("fs_try_remove_path", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_remove_path", "path")?;
            runtime_fs_result(kain_fs::remove_path(path).map(|_| Value::Unit))
        });

        self.define_native("read_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "read_dir", "path")?;
            runtime_fs_strict(
                "read_dir",
                kain_fs::read_dir_paths(path).map(|entries| runtime_string_array_value(&entries)),
            )
        });

        self.define_native("fs_read_dir_paths", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_read_dir_paths", "path")?;
            runtime_fs_strict(
                "fs_read_dir_paths",
                kain_fs::read_dir_paths(path).map(|entries| runtime_string_array_value(&entries)),
            )
        });

        self.define_native("fs_try_read_dir_paths", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_read_dir_paths", "path")?;
            runtime_fs_result(
                kain_fs::read_dir_paths(path).map(|entries| runtime_string_array_value(&entries)),
            )
        });

        self.define_native("fs_read_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_read_dir", "path")?;
            runtime_fs_strict(
                "fs_read_dir",
                kain_fs::read_dir_entries(path).map(runtime_fs_dir_entry_array_value),
            )
        });

        self.define_native("fs_try_read_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_read_dir", "path")?;
            runtime_fs_result(kain_fs::read_dir_entries(path).map(runtime_fs_dir_entry_array_value))
        });

        self.define_native("fs_walk", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_walk", "path")?;
            runtime_fs_strict(
                "fs_walk",
                kain_fs::walk_dir_entries(path, kain_fs::WalkOptions::default())
                    .map(runtime_fs_dir_entry_array_value),
            )
        });

        self.define_native("fs_try_walk", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_walk", "path")?;
            runtime_fs_result(
                kain_fs::walk_dir_entries(path, kain_fs::WalkOptions::default())
                    .map(runtime_fs_dir_entry_array_value),
            )
        });

        self.define_native("fs_glob", |_env, args| {
            let pattern = runtime_expect_string_arg(&args, 0, "fs_glob", "pattern")?;
            runtime_fs_strict(
                "fs_glob",
                kain_fs::glob_paths(pattern).map(|entries| runtime_string_array_value(&entries)),
            )
        });

        self.define_native("fs_try_glob", |_env, args| {
            let pattern = runtime_expect_string_arg(&args, 0, "fs_try_glob", "pattern")?;
            runtime_fs_result(
                kain_fs::glob_paths(pattern).map(|entries| runtime_string_array_value(&entries)),
            )
        });

        self.define_native("fs_atomic_write_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_atomic_write_text", "path")?;
            let content = runtime_expect_string_arg(&args, 1, "fs_atomic_write_text", "content")?;
            runtime_fs_strict_unit(
                "fs_atomic_write_text",
                kain_fs::atomic_write_text(path, content),
            )
        });

        self.define_native("fs_try_atomic_write_text", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_atomic_write_text", "path")?;
            let content =
                runtime_expect_string_arg(&args, 1, "fs_try_atomic_write_text", "content")?;
            runtime_fs_result(kain_fs::atomic_write_text(path, content).map(|_| Value::Unit))
        });

        self.define_native("fs_atomic_write_bytes", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_atomic_write_bytes", "path")?;
            let bytes = runtime_expect_byte_array_arg(&args, 1, "fs_atomic_write_bytes", "bytes")?;
            runtime_fs_strict_unit(
                "fs_atomic_write_bytes",
                kain_fs::atomic_write_bytes(path, &bytes),
            )
        });

        self.define_native("fs_temp_file", |_env, args| {
            let prefix = runtime_expect_string_arg(&args, 0, "fs_temp_file", "prefix")?;
            runtime_fs_strict(
                "fs_temp_file",
                kain_fs::create_temp_file(prefix).map(Value::String),
            )
        });

        self.define_native("fs_temp_dir", |_env, args| {
            let prefix = runtime_expect_string_arg(&args, 0, "fs_temp_dir", "prefix")?;
            runtime_fs_strict(
                "fs_temp_dir",
                kain_fs::create_temp_dir(prefix).map(Value::String),
            )
        });

        self.define_native("fs_hash_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_hash_file", "path")?;
            runtime_fs_strict("fs_hash_file", kain_fs::hash_file(path).map(Value::String))
        });

        self.define_native("fs_try_hash_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_try_hash_file", "path")?;
            runtime_fs_result(kain_fs::hash_file(path).map(Value::String))
        });

        self.define_native("path_join", |_env, args| {
            let base = runtime_expect_string_arg(&args, 0, "path_join", "base")?;
            let child = runtime_expect_string_arg(&args, 1, "path_join", "child")?;
            Ok(Value::String(kain_fs::path_join(base, child)))
        });

        self.define_native("fs_path_join", |_env, args| {
            let base = runtime_expect_string_arg(&args, 0, "fs_path_join", "base")?;
            let child = runtime_expect_string_arg(&args, 1, "fs_path_join", "child")?;
            Ok(Value::String(kain_fs::path_join(base, child)))
        });

        self.define_native("path_parent", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_parent", "path")?;
            Ok(Value::String(kain_fs::path_parent(path)))
        });

        self.define_native("fs_path_parent", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_parent", "path")?;
            Ok(Value::String(kain_fs::path_parent(path)))
        });

        self.define_native("path_file_name", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_file_name", "path")?;
            Ok(Value::String(kain_fs::path_file_name(path)))
        });

        self.define_native("fs_path_file_name", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_file_name", "path")?;
            Ok(Value::String(kain_fs::path_file_name(path)))
        });

        self.define_native("path_extension", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_extension", "path")?;
            Ok(Value::String(kain_fs::path_extension(path)))
        });

        self.define_native("fs_path_extension", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_extension", "path")?;
            Ok(Value::String(kain_fs::path_extension(path)))
        });

        self.define_native("path_stem", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_stem", "path")?;
            Ok(Value::String(kain_fs::path_stem(path)))
        });

        self.define_native("fs_path_stem", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_stem", "path")?;
            Ok(Value::String(kain_fs::path_stem(path)))
        });

        self.define_native("path_is_file", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_is_file", "path")?;
            Ok(Value::Bool(kain_fs::is_file(path)))
        });

        self.define_native("path_is_dir", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "path_is_dir", "path")?;
            Ok(Value::Bool(kain_fs::is_dir(path)))
        });

        self.define_native("fs_path_normalize", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_normalize", "path")?;
            Ok(Value::String(kain_fs::normalize_path(path)))
        });

        self.define_native("fs_path_absolute", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_absolute", "path")?;
            runtime_fs_strict(
                "fs_path_absolute",
                kain_fs::absolute_path(path).map(Value::String),
            )
        });

        self.define_native("fs_path_canonicalize", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_path_canonicalize", "path")?;
            runtime_fs_strict(
                "fs_path_canonicalize",
                kain_fs::canonicalize_path(path).map(Value::String),
            )
        });

        self.define_native("fs_capability_describe", |_env, _args| {
            Ok(Value::String(RUNTIME_FS_SANDBOX.read().unwrap().describe()))
        });

        self.define_native("fs_capability_has", |_env, args| {
            let capability = runtime_expect_capability_arg(&args, 0, "fs_capability_has")?;
            Ok(Value::Bool(
                RUNTIME_FS_SANDBOX
                    .read()
                    .unwrap()
                    .has_capability(capability),
            ))
        });

        self.define_native("fs_capability_grant", |_env, args| {
            let capability = runtime_expect_capability_arg(&args, 0, "fs_capability_grant")?;
            RUNTIME_FS_SANDBOX.write().unwrap().grant(capability);
            Ok(Value::Unit)
        });

        self.define_native("fs_capability_revoke", |_env, args| {
            let capability = runtime_expect_capability_arg(&args, 0, "fs_capability_revoke")?;
            RUNTIME_FS_SANDBOX.write().unwrap().revoke(capability);
            Ok(Value::Unit)
        });

        self.define_native("fs_sandbox_allow_host_paths", |_env, args| {
            let allow = runtime_expect_bool_arg(&args, 0, "fs_sandbox_allow_host_paths", "allow")?;
            RUNTIME_FS_SANDBOX.write().unwrap().allow_host_paths(allow);
            Ok(Value::Unit)
        });

        self.define_native("fs_mount", |_env, args| {
            let key = runtime_expect_string_arg(&args, 0, "fs_mount", "key")?;
            let root = runtime_expect_string_arg(&args, 1, "fs_mount", "root")?;
            let mode = runtime_expect_string_arg(&args, 2, "fs_mount", "mode")?;
            let read_only = matches!(mode, "read_only" | "readonly" | "ro");
            RUNTIME_FS_SANDBOX
                .write()
                .unwrap()
                .mount(key, root, read_only);
            Ok(Value::Unit)
        });

        self.define_native("fs_unmount", |_env, args| {
            let key = runtime_expect_string_arg(&args, 0, "fs_unmount", "key")?;
            Ok(Value::Bool(
                RUNTIME_FS_SANDBOX.write().unwrap().unmount(key),
            ))
        });

        self.define_native("fs_resolve", |_env, args| {
            let path = runtime_expect_string_arg(&args, 0, "fs_resolve", "path")?;
            runtime_fs_strict(
                "fs_resolve",
                RUNTIME_FS_SANDBOX
                    .read()
                    .unwrap()
                    .resolve(path)
                    .map(|path| Value::String(path.to_string_lossy().into_owned())),
            )
        });

        self.define_native("fs_read_text_range", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_read_text_range", FsCapability::Read)?;
            let offset =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_read_text_range", "offset")?;
            let length =
                runtime_expect_non_negative_int_arg(&args, 2, "fs_read_text_range", "length")?;
            runtime_fs_strict(
                "fs_read_text_range",
                kain_fs::read_text_range(path, offset as u64, length as usize).map(Value::String),
            )
        });

        self.define_native("fs_read_bytes_range", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_read_bytes_range", FsCapability::Read)?;
            let offset =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_read_bytes_range", "offset")?;
            let length =
                runtime_expect_non_negative_int_arg(&args, 2, "fs_read_bytes_range", "length")?;
            runtime_fs_strict(
                "fs_read_bytes_range",
                kain_fs::read_byte_range(path, offset as u64, length as usize)
                    .map(|bytes| runtime_byte_array_value(&bytes)),
            )
        });

        self.define_native("fs_write_text_at", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_write_text_at", FsCapability::Write)?;
            let offset =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_write_text_at", "offset")?;
            let content = runtime_expect_string_arg(&args, 2, "fs_write_text_at", "content")?;
            runtime_fs_strict_unit(
                "fs_write_text_at",
                kain_fs::write_text_at(path, offset as u64, content),
            )
        });

        self.define_native("fs_write_bytes_at", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_write_bytes_at", FsCapability::Write)?;
            let offset =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_write_bytes_at", "offset")?;
            let bytes = runtime_expect_byte_array_arg(&args, 2, "fs_write_bytes_at", "bytes")?;
            runtime_fs_strict_unit(
                "fs_write_bytes_at",
                kain_fs::write_bytes_at(path, offset as u64, &bytes),
            )
        });

        self.define_native("fs_stream_chunks", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_stream_chunks", FsCapability::Read)?;
            let chunk_size =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_stream_chunks", "chunk_size")?;
            runtime_fs_strict(
                "fs_stream_chunks",
                kain_fs::stream_file_chunks(path, chunk_size.max(1) as usize)
                    .map(runtime_fs_chunk_array_value),
            )
        });

        self.define_native("fs_copy_file_streaming", |_env, args| {
            let src =
                runtime_expect_scoped_path(&args, 0, "fs_copy_file_streaming", FsCapability::Read)?;
            let dest = runtime_expect_scoped_path(
                &args,
                1,
                "fs_copy_file_streaming",
                FsCapability::Write,
            )?;
            let chunk_size = runtime_expect_non_negative_int_arg(
                &args,
                2,
                "fs_copy_file_streaming",
                "chunk_size",
            )?;
            runtime_fs_strict(
                "fs_copy_file_streaming",
                kain_fs::copy_file_streaming(src, dest, chunk_size.max(1) as usize)
                    .map(|bytes| Value::Int(bytes.min(i64::MAX as u64) as i64)),
            )
        });

        self.define_native("fs_read_bytes_hex", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_read_bytes_hex", FsCapability::Read)?;
            runtime_fs_strict(
                "fs_read_bytes_hex",
                kain_fs::read_bytes(path).map(|bytes| Value::String(runtime_hex_encode(&bytes))),
            )
        });

        self.define_native("fs_read_byte_range_hex", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_read_byte_range_hex", FsCapability::Read)?;
            let offset =
                runtime_expect_non_negative_int_arg(&args, 1, "fs_read_byte_range_hex", "offset")?;
            let length =
                runtime_expect_non_negative_int_arg(&args, 2, "fs_read_byte_range_hex", "length")?;
            runtime_fs_strict(
                "fs_read_byte_range_hex",
                kain_fs::read_byte_range(path, offset as u64, length as usize)
                    .map(|bytes| Value::String(runtime_hex_encode(&bytes))),
            )
        });

        self.define_native("fs_write_bytes_hex", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_write_bytes_hex", FsCapability::Write)?;
            let hex = runtime_expect_string_arg(&args, 1, "fs_write_bytes_hex", "hex")?;
            let bytes = runtime_hex_decode(hex)?;
            runtime_fs_strict_unit("fs_write_bytes_hex", kain_fs::write_bytes(path, &bytes))
        });

        self.define_native("fs_metadata_text", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_metadata_text", FsCapability::Metadata)?;
            runtime_fs_strict(
                "fs_metadata_text",
                kain_fs::metadata(path)
                    .map(|metadata| Value::String(runtime_fs_metadata_text(metadata))),
            )
        });

        self.define_native("fs_read_dir_paths_text", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_read_dir_paths_text", FsCapability::List)?;
            runtime_fs_strict(
                "fs_read_dir_paths_text",
                kain_fs::read_dir_paths(path)
                    .map(|paths| Value::String(runtime_join_lines(&paths))),
            )
        });

        self.define_native("fs_walk_paths_text", |_env, args| {
            let path =
                runtime_expect_scoped_path(&args, 0, "fs_walk_paths_text", FsCapability::List)?;
            runtime_fs_strict(
                "fs_walk_paths_text",
                kain_fs::walk_dir_entries(path, kain_fs::WalkOptions::default()).map(|entries| {
                    let paths = entries
                        .into_iter()
                        .map(|entry| entry.path.to_string_lossy().into_owned())
                        .collect::<Vec<_>>();
                    Value::String(runtime_join_lines(&paths))
                }),
            )
        });

        self.define_native("fs_watch", |_env, args| {
            let path = runtime_expect_scoped_path(&args, 0, "fs_watch", FsCapability::Watch)?;
            let recursive = runtime_expect_bool_arg(&args, 1, "fs_watch", "recursive")?;
            let watcher = kain_fs::FsWatcher::new(path, recursive)
                .map_err(|error| KainError::runtime(format!("fs_watch: {error}")))?;
            let id = RUNTIME_FS_NEXT_WATCHER_ID.fetch_add(1, Ordering::Relaxed);
            RUNTIME_FS_WATCHERS.write().unwrap().insert(id, watcher);
            Ok(Value::Int(id))
        });

        self.define_native("fs_watch_poll", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_watch_poll", "watcher_id")?;
            let mut watchers = RUNTIME_FS_WATCHERS.write().unwrap();
            let Some(watcher) = watchers.get_mut(&id) else {
                return Err(KainError::runtime(format!(
                    "fs_watch_poll: watcher {id} does not exist"
                )));
            };
            runtime_fs_strict(
                "fs_watch_poll",
                watcher.poll().map(runtime_fs_watch_event_array_value),
            )
        });

        self.define_native("fs_watch_close", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_watch_close", "watcher_id")?;
            Ok(Value::Bool(
                RUNTIME_FS_WATCHERS.write().unwrap().remove(&id).is_some(),
            ))
        });

        self.define_native("fs_tx_begin", |_env, _args| {
            let id = RUNTIME_FS_NEXT_TRANSACTION_ID.fetch_add(1, Ordering::Relaxed);
            RUNTIME_FS_TRANSACTIONS
                .write()
                .unwrap()
                .insert(id, kain_fs::FsTransaction::new());
            Ok(Value::Int(id))
        });

        self.define_native("fs_tx_write_text", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_write_text", "transaction_id")?;
            let path = runtime_expect_scoped_path(
                &args,
                1,
                "fs_tx_write_text",
                FsCapability::Transaction,
            )?;
            let content = runtime_expect_string_arg(&args, 2, "fs_tx_write_text", "content")?;
            runtime_with_transaction(id, "fs_tx_write_text", |transaction| {
                transaction.write_text(path, content);
                Ok(Value::Unit)
            })
        });

        self.define_native("fs_tx_append_text", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_append_text", "transaction_id")?;
            let path = runtime_expect_scoped_path(
                &args,
                1,
                "fs_tx_append_text",
                FsCapability::Transaction,
            )?;
            let content = runtime_expect_string_arg(&args, 2, "fs_tx_append_text", "content")?;
            runtime_with_transaction(id, "fs_tx_append_text", |transaction| {
                transaction.append_text(path, content);
                Ok(Value::Unit)
            })
        });

        self.define_native("fs_tx_remove_path", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_remove_path", "transaction_id")?;
            let path = runtime_expect_scoped_path(
                &args,
                1,
                "fs_tx_remove_path",
                FsCapability::Transaction,
            )?;
            runtime_with_transaction(id, "fs_tx_remove_path", |transaction| {
                transaction.remove_path(path);
                Ok(Value::Unit)
            })
        });

        self.define_native("fs_tx_copy_path", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_copy_path", "transaction_id")?;
            let src = runtime_expect_scoped_path(&args, 1, "fs_tx_copy_path", FsCapability::Read)?;
            let dest =
                runtime_expect_scoped_path(&args, 2, "fs_tx_copy_path", FsCapability::Transaction)?;
            runtime_with_transaction(id, "fs_tx_copy_path", |transaction| {
                transaction.copy_path(src, dest);
                Ok(Value::Unit)
            })
        });

        self.define_native("fs_tx_move_path", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_move_path", "transaction_id")?;
            let src =
                runtime_expect_scoped_path(&args, 1, "fs_tx_move_path", FsCapability::Transaction)?;
            let dest =
                runtime_expect_scoped_path(&args, 2, "fs_tx_move_path", FsCapability::Transaction)?;
            runtime_with_transaction(id, "fs_tx_move_path", |transaction| {
                transaction.move_path(src, dest);
                Ok(Value::Unit)
            })
        });

        self.define_native("fs_tx_commit", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_commit", "transaction_id")?;
            let Some(mut transaction) = RUNTIME_FS_TRANSACTIONS.write().unwrap().remove(&id) else {
                return Err(KainError::runtime(format!(
                    "fs_tx_commit: transaction {id} does not exist"
                )));
            };
            runtime_fs_strict(
                "fs_tx_commit",
                transaction.commit().map(runtime_fs_journal_array_value),
            )
        });

        self.define_native("fs_tx_rollback", |_env, args| {
            let id = runtime_expect_int_arg(&args, 0, "fs_tx_rollback", "transaction_id")?;
            let Some(mut transaction) = RUNTIME_FS_TRANSACTIONS.write().unwrap().remove(&id) else {
                return Err(KainError::runtime(format!(
                    "fs_tx_rollback: transaction {id} does not exist"
                )));
            };
            Ok(runtime_fs_journal_array_value(transaction.rollback_only()))
        });

        self.define_native("patch_history", |env, _args| {
            Ok(runtime_array_value(
                env.patch_records()
                    .iter()
                    .map(runtime_patch_record_value)
                    .collect(),
            ))
        });

        self.define_native("patch_collaboration_events", |env, _args| {
            Ok(runtime_array_value(
                env.patch_collaboration_events()
                    .iter()
                    .map(runtime_patch_collaboration_event_value)
                    .collect(),
            ))
        });

        self.define_native("patch_undo_last", |env, _args| {
            Ok(Value::Bool(env.undo_last_patch()?))
        });

        self.define_native("patch_replay_last", |env, _args| {
            Ok(Value::Bool(env.replay_last_undone_patch()?))
        });

        self.define_native("patch_replay", |env, args| {
            let Some(Value::Int(index)) = args.first() else {
                return Err(KainError::runtime(
                    "patch_replay: expected 1 integer argument (history_index)",
                ));
            };
            if *index < 0 {
                return Err(KainError::runtime(
                    "patch_replay: history_index must be non-negative",
                ));
            }
            Ok(Value::Bool(env.replay_patch_record(*index as usize)?))
        });

        // === ASYNC RUNTIME ===

        // block_on: Run a future to completion, blocking the current thread
        self.define_native("block_on", |env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("block_on: expected 1 argument (future)"));
            }

            let future_val = args[0].clone();
            poll_future_to_completion(env, future_val)
        });

        // spawn_task: Spawn an async task (runs it immediately in this simple executor)
        self.define_native("spawn_task", |env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "spawn_task: expected 1 argument (future)",
                ));
            }

            // For this simple executor, spawn is just block_on
            // A real executor would add to a task queue
            let future_val = args[0].clone();
            poll_future_to_completion(env, future_val)
        });

        // poll_once: Poll a future once and return the Poll result
        self.define_native("poll_once", |env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime(
                    "poll_once: expected 1 argument (future)",
                ));
            }

            poll_future_once(env, args[0].clone())
        });

        // is_ready: Check if a Poll value is Ready
        self.define_native("is_ready", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("is_ready: expected 1 argument"));
            }

            match &args[0] {
                Value::Poll(ready, _) => Ok(Value::Bool(*ready)),
                Value::EnumVariant(_, variant, _) => Ok(Value::Bool(variant == "Ready")),
                _ => Ok(Value::Bool(false)),
            }
        });

        // is_pending: Check if a Poll value is Pending
        self.define_native("is_pending", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("is_pending: expected 1 argument"));
            }

            match &args[0] {
                Value::Poll(ready, _) => Ok(Value::Bool(!*ready)),
                Value::EnumVariant(_, variant, _) => Ok(Value::Bool(variant == "Pending")),
                _ => Ok(Value::Bool(false)),
            }
        });

        // unwrap_ready: Extract the value from Poll::Ready, panic if Pending
        self.define_native("unwrap_ready", |_env, args| {
            if args.len() != 1 {
                return Err(KainError::runtime("unwrap_ready: expected 1 argument"));
            }

            match &args[0] {
                Value::Poll(true, Some(val)) => Ok(*val.clone()),
                Value::Poll(true, None) => Ok(Value::Unit),
                Value::Poll(false, _) => {
                    Err(KainError::runtime("unwrap_ready: called on Poll::Pending"))
                }
                Value::EnumVariant(_, variant, fields) if variant == "Ready" => {
                    if fields.is_empty() {
                        Ok(Value::Unit)
                    } else {
                        Ok(fields[0].clone())
                    }
                }
                Value::EnumVariant(_, variant, _) if variant == "Pending" => {
                    Err(KainError::runtime("unwrap_ready: called on Poll::Pending"))
                }
                _ => Err(KainError::runtime("unwrap_ready: expected Poll value")),
            }
        });
    }

    fn define_native(&mut self, name: &str, func: NativeFn) {
        self.register_native_fn(name, func);
    }

    pub fn register_native_fn(&mut self, name: impl Into<String>, func: NativeFn) {
        let name = name.into();
        self.scopes[0].insert(name.clone(), Value::NativeFn(name, func));
    }

    pub fn register_component(&mut self, component: Component) {
        self.components.insert(component.name.clone(), component);
    }

    fn ensure_active_capability(&mut self, capability: String) {
        if !self
            .active_capabilities
            .iter()
            .any(|entry| entry == &capability)
        {
            self.active_capabilities.push(capability);
        }
    }

    fn set_execution_lane(&mut self, lane: ExecutionLane) {
        self.execution_lane = lane;
        self.active_capabilities.retain(|capability| {
            capability != "host.runtime.interpret" && capability != "host.runtime.test"
        });
        self.active_capabilities.push(match lane {
            ExecutionLane::Interpret => "host.runtime.interpret".to_string(),
            ExecutionLane::Test => "host.runtime.test".to_string(),
        });
    }

    fn register_patch_value(&mut self, patch: &PatchDef, undo_mode: String) {
        self.patches.insert(patch.name.clone(), patch.clone());
        self.patch_undo_modes.insert(patch.name.clone(), undo_mode);
        self.define(patch.name.clone(), Value::Patch(patch.name.clone()));
    }

    fn register_law_value(&mut self, law: &LawDef) {
        self.laws.insert(law.name.clone(), law.clone());
        self.define(law.name.clone(), Value::Law(law.name.clone()));
    }

    fn register_converge_value(&mut self, converge: &ConvergeDef) {
        self.converges
            .insert(converge.name.clone(), converge.clone());
        self.define(
            converge.name.clone(),
            Value::Converge(converge.name.clone()),
        );
    }

    fn register_orchestrate_value(&mut self, orchestrate: &OrchestrateDef) {
        self.orchestrates
            .insert(orchestrate.name.clone(), orchestrate.clone());
        self.define(
            orchestrate.name.clone(),
            Value::Orchestrate(orchestrate.name.clone()),
        );
    }

    fn register_world_value(&mut self, world: &WorldDef) -> KainResult<()> {
        let mut state_values = HashMap::new();
        for state in &world.states {
            let value = eval_expr(self, &state.initial)?;
            state_values.insert(state.name.clone(), value);
        }
        let world_value = Arc::new(RwLock::new(state_values));
        self.worlds.insert(world.name.clone(), world_value.clone());
        self.define(
            world.name.clone(),
            Value::Struct(world.name.clone(), world_value),
        );
        Ok(())
    }

    fn register_entangle_value(&mut self, entangle: &EntangleDef) -> KainResult<()> {
        let binding = match entangle.policy {
            EntanglePolicy::SingleWriter => EntangleBindingDescriptor::single_writer(
                EntangleEndpointId::new(entangle.left.authored_path()),
                EntangleEndpointId::new(entangle.right.authored_path()),
            ),
        };
        self.entanglements
            .register(binding)
            .map_err(|err| KainError::runtime(err.to_string()))?;
        self.ensure_active_capability(kain_entangle::STATE_ENTANGLE_CAPABILITY.to_string());
        Ok(())
    }

    pub fn patch_records(&self) -> &[PatchRuntimeRecord] {
        &self.patch_records
    }

    pub fn patch_collaboration_events(&self) -> &[PatchCollaborationEvent] {
        &self.patch_collaboration_events
    }

    fn begin_active_patch(&mut self, name: &str) {
        let undo_mode = self
            .patch_undo_modes
            .get(name)
            .cloned()
            .unwrap_or_else(|| "best_effort".to_string());
        self.active_patch_frames.push(ActivePatchFrame {
            name: name.to_string(),
            mutation_paths: Vec::new(),
            undo_mode,
            changes: Vec::new(),
        });
    }

    fn record_patch_change(&mut self, change: ActivePatchChange) {
        if let Some(frame) = self.active_patch_frames.last_mut() {
            frame.mutation_paths.push(change.path.clone());
            frame.changes.push(change);
        }
    }

    fn finish_active_patch(&mut self) {
        if let Some(mut frame) = self.active_patch_frames.pop() {
            frame.mutation_paths.sort();
            frame.mutation_paths.dedup();
            let collaboration_event = format!("patch.{}.applied", frame.name);
            let changes = frame
                .changes
                .iter()
                .map(|change| PatchMutationRecord {
                    path: change.path.clone(),
                    old_value: change.old_value.clone(),
                    new_value: change.new_value.clone(),
                })
                .collect::<Vec<_>>();
            let replayable_record = ReplayablePatchRecord {
                name: frame.name.clone(),
                undo_mode: frame.undo_mode.clone(),
                changes: frame.changes,
            };
            self.patch_records.push(PatchRuntimeRecord {
                name: frame.name,
                mutation_paths: frame.mutation_paths,
                undo_mode: frame.undo_mode,
                changes,
                collaboration_event: collaboration_event.clone(),
            });
            self.patch_replay_catalog.push(replayable_record.clone());
            self.replayable_patch_history
                .push(replayable_record.clone());
            self.undone_patch_records.clear();
            self.patch_collaboration_events
                .push(PatchCollaborationEvent {
                    event_id: collaboration_event,
                    patch_name: replayable_record.name,
                    mutation_paths: self
                        .patch_records
                        .last()
                        .map(|record| record.mutation_paths.clone())
                        .unwrap_or_default(),
                    undo_mode: replayable_record.undo_mode,
                });
        }
    }

    fn cancel_active_patch(&mut self) -> KainResult<()> {
        if let Some(frame) = self.active_patch_frames.pop() {
            if frame.undo_mode == "reversible" {
                for change in frame.changes.iter().rev() {
                    apply_patch_change_value(change, false)?;
                }
            }
        }
        Ok(())
    }

    fn undo_last_patch(&mut self) -> KainResult<bool> {
        let Some(record) = self
            .replayable_patch_history
            .iter()
            .rposition(|record| record.undo_mode == "reversible")
            .map(|index| self.replayable_patch_history.remove(index))
        else {
            return Ok(false);
        };

        for change in record.changes.iter().rev() {
            apply_patch_change_value(change, false)?;
        }
        self.undone_patch_records.push(record);
        Ok(true)
    }

    fn replay_last_undone_patch(&mut self) -> KainResult<bool> {
        let Some(record) = self.undone_patch_records.pop() else {
            return Ok(false);
        };
        for change in &record.changes {
            apply_patch_change_value(change, true)?;
        }
        self.replayable_patch_history.push(record);
        Ok(true)
    }

    fn replay_patch_record(&mut self, index: usize) -> KainResult<bool> {
        let Some(record) = self.patch_replay_catalog.get(index).cloned() else {
            return Ok(false);
        };
        for change in &record.changes {
            apply_patch_change_value(change, true)?;
        }
        self.replayable_patch_history.push(record);
        Ok(true)
    }

    pub fn register_program_items(&mut self, program: &Program) -> KainResult<()> {
        for item in &program.items {
            self.register_item(item)?;
        }
        Ok(())
    }

    pub fn register_typed_program(&mut self, program: &TypedProgram) -> KainResult<()> {
        for item in &program.items {
            if let TypedItem::Mod(module) = item {
                self.register_inline_module(&module.ast, &[])?;
            }
        }
        for item in &program.items {
            self.register_typed_item(item)?;
        }
        Ok(())
    }

    fn register_item(&mut self, item: &Item) -> KainResult<()> {
        match item {
            Item::Use(u) => load_module(self, u)?,
            Item::Mod(module) => self.register_inline_module(module, &[])?,
            Item::Function(f) => {
                if !is_extern_runtime_declaration(f) {
                    self.functions.insert(f.name.clone(), f.clone());
                    self.define(f.name.clone(), Value::Function(f.name.clone()));
                }
            }
            Item::Patch(patch) => {
                self.register_patch_value(patch, infer_patch_undo_mode(patch));
            }
            Item::Law(law) => {
                self.register_law_value(law);
            }
            Item::Converge(converge) => {
                self.register_converge_value(converge);
            }
            Item::World(world) => {
                self.register_world_value(world)?;
            }
            Item::Entangle(entangle) => {
                self.register_entangle_value(entangle)?;
            }
            Item::Orchestrate(orchestrate) => {
                self.register_orchestrate_value(orchestrate);
            }
            Item::Component(c) => self.register_component(c.clone()),
            Item::Struct(s) => {
                let field_names = s.fields.iter().map(|field| field.name.clone()).collect();
                self.define(
                    s.name.clone(),
                    Value::StructConstructor(s.name.clone(), field_names),
                );
            }
            Item::Enum(e) => {
                for variant in &e.variants {
                    let variant_name = format!("{}::{}", e.name, variant.name);
                    self.define(variant_name.clone(), Value::Function(variant_name.clone()));
                    let alias = selfhost_enum_variant_alias_name(&e.name, &variant.name);
                    let alias_value = match &variant.fields {
                        VariantFields::Unit => {
                            Value::EnumVariant(e.name.clone(), variant.name.clone(), Vec::new())
                        }
                        VariantFields::Tuple(_) | VariantFields::Struct(_) => {
                            Value::Function(variant_name)
                        }
                    };
                    self.define(alias, alias_value);
                }
            }
            Item::Actor(a) => {
                self.actor_defs.insert(a.name.clone(), a.clone());
            }
            Item::Const(c) => {
                let value = eval_expr(self, &c.value)?;
                self.define(c.name.clone(), value);
            }
            Item::Impl(i) => {
                if let Type::Named { name, .. } = &i.target_type {
                    let lowered_fns: Vec<(String, Function)> = i
                        .methods
                        .iter()
                        .flat_map(|method| {
                            lowered_impl_function_names(name, &method.name)
                                .into_iter()
                                .map(|lowered_name| (lowered_name, method.clone()))
                        })
                        .collect();

                    for (lowered_name, method) in lowered_fns {
                        self.functions.insert(lowered_name.clone(), method);
                        self.define(lowered_name.clone(), Value::Function(lowered_name));
                    }

                    let type_methods = self.methods.entry(name.clone()).or_default();
                    for method in &i.methods {
                        type_methods.insert(method.name.clone(), method.clone());
                    }
                }
            }
            Item::Comptime(_)
            | Item::Macro(_)
            | Item::Test(_)
            | Item::Trait(_)
            | Item::TypeAlias(_)
            | Item::Shader(_)
            | Item::MaterialGraph(_)
            | Item::MaterialFunction(_)
            | Item::GraphEditor(_)
            | Item::GraphRuntime(_)
            | Item::StateMachine(_)
            | Item::AsyncTask(_)
            | Item::EditorModule(_)
            | Item::GameplayTags(_)
            | Item::GameplayAbility(_)
            | Item::GameplayEffect(_)
            | Item::GameplayCue(_)
            | Item::AbilityTask(_)
            | Item::TargetActor(_) => {}
        }

        Ok(())
    }

    fn register_typed_item(&mut self, item: &TypedItem) -> KainResult<()> {
        match item {
            TypedItem::Use(u) => load_module(self, &u.ast)?,
            TypedItem::Mod(module) => {
                for child in &module.items {
                    self.register_typed_item(child)?;
                }
            }
            TypedItem::Function(f) => {
                if !is_extern_runtime_declaration(&f.ast) {
                    self.functions.insert(f.ast.name.clone(), f.ast.clone());
                    self.define(f.ast.name.clone(), Value::Function(f.ast.name.clone()));
                }
            }
            TypedItem::Patch(patch) => {
                self.register_patch_value(
                    &patch.ast,
                    match patch.undo_mode {
                        crate::types::PatchUndoMode::Reversible => "reversible".to_string(),
                        crate::types::PatchUndoMode::BestEffort => "best_effort".to_string(),
                    },
                );
            }
            TypedItem::Law(law) => {
                self.register_law_value(&law.ast);
            }
            TypedItem::Converge(converge) => {
                self.register_converge_value(&converge.ast);
            }
            TypedItem::World(world) => {
                self.register_world_value(&world.ast)?;
            }
            TypedItem::Entangle(entangle) => {
                self.register_entangle_value(&entangle.ast)?;
            }
            TypedItem::Orchestrate(orchestrate) => {
                self.register_orchestrate_value(&orchestrate.ast);
            }
            TypedItem::Component(c) => self.register_component(c.ast.clone()),
            TypedItem::Struct(s) => {
                let field_names = s
                    .ast
                    .fields
                    .iter()
                    .map(|field| field.name.clone())
                    .collect();
                self.define(
                    s.ast.name.clone(),
                    Value::StructConstructor(s.ast.name.clone(), field_names),
                );
            }
            TypedItem::Enum(e) => {
                for variant in &e.ast.variants {
                    let variant_name = format!("{}::{}", e.ast.name, variant.name);
                    self.define(variant_name.clone(), Value::Function(variant_name.clone()));
                    let alias = selfhost_enum_variant_alias_name(&e.ast.name, &variant.name);
                    let alias_value = match &variant.fields {
                        VariantFields::Unit => {
                            Value::EnumVariant(e.ast.name.clone(), variant.name.clone(), Vec::new())
                        }
                        VariantFields::Tuple(_) | VariantFields::Struct(_) => {
                            Value::Function(variant_name)
                        }
                    };
                    self.define(alias, alias_value);
                }
            }
            TypedItem::Actor(a) => {
                self.actor_defs.insert(a.ast.name.clone(), a.ast.clone());
            }
            TypedItem::Const(c) => {
                let value = eval_expr(self, &c.ast.value)?;
                self.define(c.ast.name.clone(), value);
            }
            TypedItem::Impl(i) => {
                if let Type::Named { name, .. } = &i.ast.target_type {
                    let lowered_fns: Vec<(String, Function)> = i
                        .ast
                        .methods
                        .iter()
                        .flat_map(|method| {
                            lowered_impl_function_names(name, &method.name)
                                .into_iter()
                                .map(|lowered_name| (lowered_name, method.clone()))
                        })
                        .collect();

                    for (lowered_name, method) in lowered_fns {
                        self.functions.insert(lowered_name.clone(), method);
                        self.define(lowered_name.clone(), Value::Function(lowered_name));
                    }

                    let type_methods = self.methods.entry(name.clone()).or_default();
                    for method in &i.ast.methods {
                        type_methods.insert(method.name.clone(), method.clone());
                    }
                }
            }
            TypedItem::Comptime(_)
            | TypedItem::Shader(_)
            | TypedItem::Macro(_)
            | TypedItem::Trait(_)
            | TypedItem::Test(_)
            | TypedItem::TypeAlias(_)
            | TypedItem::MaterialGraph(_)
            | TypedItem::MaterialFunction(_)
            | TypedItem::GraphEditor(_)
            | TypedItem::GraphRuntime(_)
            | TypedItem::StateMachine(_)
            | TypedItem::AsyncTask(_)
            | TypedItem::EditorModule(_)
            | TypedItem::GameplayTags(_)
            | TypedItem::GameplayAbility(_)
            | TypedItem::GameplayEffect(_)
            | TypedItem::GameplayCue(_) => {}
        }

        Ok(())
    }

    pub(crate) fn define(&mut self, name: String, value: Value) {
        self.scopes.last_mut().unwrap().insert(name, value);
    }

    fn register_inline_module(&mut self, module: &Mod, parent_path: &[String]) -> KainResult<()> {
        let mut full_path = parent_path.to_vec();
        full_path.push(module.name.clone());
        let module_key = full_path.join("/");

        if let Some(children) = &module.inline {
            self.inline_modules.insert(module_key, children.clone());
            self.register_inline_module_aliases(children, &full_path)?;

            for child in children {
                if let Item::Mod(nested) = child {
                    self.register_inline_module(nested, &full_path)?;
                }
            }
        }

        Ok(())
    }

    fn register_inline_module_aliases(
        &mut self,
        items: &[Item],
        module_path: &[String],
    ) -> KainResult<()> {
        for item in items {
            match item {
                Item::Function(f) => {
                    let alias = module_scoped_name(module_path, &f.name);
                    self.functions.insert(alias.clone(), f.clone());
                    self.define(alias.clone(), Value::Function(alias));
                }
                Item::Const(c) => {
                    let alias = module_scoped_name(module_path, &c.name);
                    let value = eval_expr(self, &c.value)?;
                    self.define(alias, value);
                }
                Item::Mod(module) => {
                    if let Some(children) = &module.inline {
                        let mut nested_path = module_path.to_vec();
                        nested_path.push(module.name.clone());
                        self.register_inline_module_aliases(children, &nested_path)?;
                    }
                }
                _ => {}
            }
        }

        let scope_bindings = self.inline_module_scope_bindings(items, module_path);
        for item in items {
            if let Item::Function(f) = item {
                let alias = module_scoped_name(module_path, &f.name);
                self.function_inline_scopes
                    .insert(alias, scope_bindings.clone());
            }
        }
        Ok(())
    }

    fn inline_module_scope_bindings(
        &self,
        items: &[Item],
        module_path: &[String],
    ) -> HashMap<String, Value> {
        let mut bindings = HashMap::new();
        for item in items {
            match item {
                Item::Function(f) => {
                    let alias = module_scoped_name(module_path, &f.name);
                    bindings.insert(f.name.clone(), Value::Function(alias));
                }
                Item::Const(c) => {
                    let alias = module_scoped_name(module_path, &c.name);
                    if let Some(value) = self.lookup_value(&alias) {
                        bindings.insert(c.name.clone(), value);
                    }
                }
                _ => {}
            }
        }
        bindings
    }

    pub fn define_global(&mut self, name: impl Into<String>, value: Value) {
        self.scopes[0].insert(name.into(), value);
    }

    pub fn lookup_value(&self, name: &str) -> Option<Value> {
        self.lookup(name).cloned()
    }

    pub fn call_named_function(&mut self, name: &str, args: Vec<Value>) -> KainResult<Value> {
        let func = self
            .lookup(name)
            .cloned()
            .ok_or_else(|| KainError::runtime(format!("Function not found: {}", name)))?;
        call_function(self, func, args)
    }

    fn assign(&mut self, name: &str, value: Value) -> KainResult<()> {
        for scope in self.scopes.iter_mut().rev() {
            if scope.contains_key(name) {
                scope.insert(name.to_string(), value);
                return Ok(());
            }
        }
        Err(KainError::runtime(format!("Undefined variable '{}'", name)))
    }

    fn ensure_entangle_write_allowed(&self, endpoint: &str) -> KainResult<()> {
        self.entanglements
            .ensure_write_allowed(endpoint)
            .map_err(|err| KainError::runtime(err.to_string()))
    }

    fn propagate_entangled_write(&mut self, endpoint: &str, value: Value) -> KainResult<()> {
        for mirror in self.entanglements.mirrors_for_authority(endpoint) {
            self.assign_entangled_endpoint(&mirror, value.clone())?;
        }
        Ok(())
    }

    fn assign_entangled_endpoint(&mut self, endpoint: &str, value: Value) -> KainResult<()> {
        let segments = endpoint.split('.').collect::<Vec<_>>();
        let Some((root, fields)) = segments.split_first() else {
            return Err(KainError::runtime("Empty entangle endpoint"));
        };
        if fields.is_empty() {
            return self.assign(root, value);
        }
        let root_value = self
            .lookup(root)
            .cloned()
            .ok_or_else(|| KainError::runtime(format!("Undefined entangle root '{}'", root)))?;
        assign_entangled_field_path(root_value, fields, value)
    }

    fn lookup(&self, name: &str) -> Option<&Value> {
        for scope in self.scopes.iter().rev() {
            if let Some(v) = scope.get(name) {
                return Some(v);
            }
        }
        None
    }

    pub(crate) fn lookup_component(&self, name: &str) -> Option<&Component> {
        self.components.get(name)
    }

    pub(crate) fn push_scope(&mut self) {
        self.scopes.push(HashMap::new());
    }

    pub(crate) fn pop_scope(&mut self) {
        self.scopes.pop();
    }
}

// === Evaluator ===

/// Interpret the program
pub fn interpret(program: &TypedProgram) -> KainResult<Value> {
    let mut env = Env::new();
    interpret_with_env(&mut env, program)
}

pub fn interpret_with_env(env: &mut Env, program: &TypedProgram) -> KainResult<Value> {
    env.set_execution_lane(ExecutionLane::Interpret);
    env.register_typed_program(program)?;
    env.apply_registered_extensions();
    if env.functions.contains_key("main") {
        env.call_named_function("main", Vec::new())
    } else {
        Ok(Value::Unit)
    }
}

fn load_module(env: &mut Env, u: &Use) -> KainResult<()> {
    let path = u.path.join("/");

    if let Some(items) = load_inline_module(env, u)? {
        for item in items {
            env.register_item(&item)?;
        }
        return Ok(());
    }

    // Check if it's core stdlib (already loaded)
    if path == "stdlib" {
        return Ok(());
    }

    // Check for stdlib submodules: std/option, std/hashmap, std/result
    let module_resolution = if path.starts_with("std/") || path.starts_with("stdlib/") {
        let module_name = path
            .trim_start_matches("std/")
            .trim_start_matches("stdlib/");

        let file_path = resolve_stdlib_module_file(module_name).ok_or_else(|| {
            KainError::runtime(format!("Stdlib module not found: {}", module_name))
        })?;

        (file_path, None, Vec::new())
    } else {
        let resolution = resolve_filesystem_module_file(&u.path).ok_or_else(|| {
            let tried_paths = filesystem_module_candidates(&u.path)
                .iter()
                .map(|p| p.display().to_string())
                .collect::<Vec<_>>();
            KainError::runtime(format!(
                "Module not found: {} (tried: {:?})",
                path, tried_paths
            ))
        })?;

        (
            resolution.file_path,
            resolution.selected_item,
            resolution.tried_paths,
        )
    };

    let (file_path, selected_item, _) = module_resolution;
    let source = kain_fs::read_text(&file_path)
        .map_err(|e| KainError::runtime(format!("Failed to read module {}: {}", path, e)))?;

    let lexer = Lexer::new(&source);
    let tokens = lexer.tokenize()?;
    let span_mapper = crate::diagnostics::SpanMapper::new(&source);
    let filename = file_path.to_string_lossy().to_string();
    let mut parser = Parser::new(&tokens, &span_mapper, &filename);
    let program = parser.parse()?;

    let items = select_filesystem_import_items(program.items, u, selected_item.as_deref())?;
    for item in items {
        env.register_item(&item)?;
    }

    Ok(())
}

fn select_filesystem_import_items(
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
        .find(|item| inline_item_name(item).is_some_and(|name| name == selected_item))
    else {
        return Err(KainError::runtime(format!(
            "Module item not found: {}",
            direct_path
        )));
    };

    Ok(vec![apply_use_alias(item, u.alias.as_deref())?])
}

fn load_inline_module(env: &mut Env, u: &Use) -> KainResult<Option<Vec<Item>>> {
    let direct_path = u.path.join("/");

    if u.glob {
        return Ok(env.inline_modules.get(&direct_path).cloned());
    }

    if let Some(items) = env.inline_modules.get(&direct_path).cloned() {
        return Ok(Some(items));
    }

    if u.path.len() < 2 {
        return Ok(None);
    }

    let module_path = u.path[..u.path.len() - 1].join("/");
    let item_name = u.path.last().unwrap();
    let Some(items) = env.inline_modules.get(&module_path) else {
        return Ok(None);
    };

    let selected = items
        .iter()
        .find(|item| inline_item_name(item).is_some_and(|name| name == item_name))
        .cloned();

    match selected {
        Some(item) => Ok(Some(vec![apply_use_alias(item, u.alias.as_deref())?])),
        None => Err(KainError::runtime(format!(
            "Inline module item not found: {}",
            direct_path
        ))),
    }
}

fn inline_item_name(item: &Item) -> Option<&str> {
    match item {
        Item::Function(f) => Some(&f.name),
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

fn apply_use_alias(mut item: Item, alias: Option<&str>) -> KainResult<Item> {
    let Some(alias) = alias else {
        return Ok(item);
    };

    match &mut item {
        Item::Function(f) => f.name = alias.to_string(),
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
                "Inline alias is not supported for item: {:?}",
                other
            )))
        }
    }

    Ok(item)
}

pub fn eval_block(env: &mut Env, block: &Block) -> KainResult<Value> {
    for stmt in &block.stmts {
        let result = eval_stmt(env, stmt)?;
        // Propagate control flow up
        match &result {
            Value::Return(_) | Value::Break(_) | Value::Continue => return Ok(result),
            _ => {}
        }
    }
    Ok(Value::Unit)
}

fn eval_stmt(env: &mut Env, stmt: &Stmt) -> KainResult<Value> {
    match stmt {
        Stmt::Expr(expr) => {
            let val = eval_expr(env, expr)?;
            // Propagate control flow
            match &val {
                Value::Return(_) | Value::Break(_) | Value::Continue => return Ok(val),
                _ => {}
            }
            Ok(Value::Unit)
        }
        Stmt::Let { pattern, value, .. } => {
            let val = if let Some(expr) = value {
                eval_expr(env, expr)?
            } else {
                Value::None
            };
            if let Value::Return(_) = val {
                return Ok(val);
            }

            // Simple binding
            if let Pattern::Binding { name, .. } = pattern {
                env.define(name.clone(), val);
            }
            Ok(Value::Unit)
        }
        Stmt::Return(expr, _) => {
            let val = if let Some(e) = expr {
                eval_expr(env, e)?
            } else {
                Value::Unit
            };
            if let Value::Return(_) = val {
                return Ok(val);
            }
            Ok(Value::Return(Box::new(val)))
        }
        Stmt::For {
            binding,
            iter,
            body,
            ..
        } => {
            let iter_val = eval_expr(env, iter)?;
            if let Value::Return(_) = iter_val {
                return Ok(iter_val);
            }

            if let Value::Array(arr) = iter_val {
                let arr = arr.read().unwrap().clone();
                for val in arr.iter() {
                    env.push_scope();
                    if let Pattern::Binding { name, .. } = binding {
                        env.define(name.clone(), val.clone());
                    }
                    let res = eval_block(env, body)?;
                    env.pop_scope();

                    match res {
                        Value::Return(_) => return Ok(res),
                        Value::Break(_) => break,
                        Value::Continue => continue,
                        _ => {}
                    }
                }
            } else if let Value::String(s) = iter_val {
                for c in s.chars() {
                    env.push_scope();
                    if let Pattern::Binding { name, .. } = binding {
                        env.define(name.clone(), Value::String(c.to_string()));
                    }
                    let res = eval_block(env, body)?;
                    env.pop_scope();

                    match res {
                        Value::Return(_) => return Ok(res),
                        Value::Break(_) => break,
                        Value::Continue => continue,
                        _ => {}
                    }
                }
            }
            Ok(Value::Unit)
        }
        Stmt::While {
            condition, body, ..
        } => {
            loop {
                let cond = eval_expr(env, condition)?;
                if let Value::Return(_) = cond {
                    return Ok(cond);
                }
                if let Value::Bool(false) = cond {
                    break;
                }

                let res = eval_block(env, body)?;
                match res {
                    Value::Return(_) => return Ok(res),
                    Value::Break(_) => break,
                    Value::Continue => continue,
                    _ => {}
                }
            }
            Ok(Value::Unit)
        }
        Stmt::Loop { body, .. } => loop {
            let res = eval_block(env, body)?;
            match res {
                Value::Return(_) => return Ok(res),
                Value::Break(val) => {
                    return Ok(val.map(|v| *v).unwrap_or(Value::Unit));
                }
                Value::Continue => continue,
                _ => {}
            }
        },
        Stmt::Break(expr, _) => {
            let val = if let Some(e) = expr {
                Some(Box::new(eval_expr(env, e)?))
            } else {
                None
            };
            Ok(Value::Break(val))
        }
        Stmt::Continue(_) => Ok(Value::Continue),
        _ => Ok(Value::Unit),
    }
}

fn eval_assignment(env: &mut Env, target: &Expr, value: Value) -> KainResult<()> {
    let target_path = runtime_patch_target_path(target);
    if let Some(path) = &target_path {
        env.ensure_entangle_write_allowed(path)?;
    }

    let assigned_value = value.clone();
    let result = match target {
        Expr::Ident(name, _) => {
            env.assign(name, value)?;
            Ok(())
        }
        Expr::Field { object, field, .. } => {
            let obj_val = eval_expr(env, object)?;
            if let Value::Struct(_, fields) = obj_val {
                let old_value = fields
                    .read()
                    .unwrap()
                    .get(field)
                    .cloned()
                    .unwrap_or(Value::None);
                fields.write().unwrap().insert(field.clone(), value.clone());
                if let Some(path) = runtime_patch_target_path(target) {
                    env.record_patch_change(ActivePatchChange {
                        path,
                        target: PatchMutationTarget::StructField {
                            fields: fields.clone(),
                            field: field.clone(),
                        },
                        old_value,
                        new_value: value,
                    });
                }
            } else if let Value::ActorRef(r) = obj_val {
                if let Some(self_id) = env.self_actor_id {
                    if self_id == r.id {
                        return env.assign(field, value);
                    }
                }
                return Err(KainError::runtime("Cannot assign to remote actor fields"));
            } else {
                return Err(KainError::runtime(
                    "Field assignment only supported on structs",
                ));
            }
            Ok(())
        }
        Expr::Index { object, index, .. } => {
            let obj_val = eval_expr(env, object)?;
            let idx_val = eval_expr(env, index)?;
            match (obj_val, idx_val) {
                (Value::Array(values), Value::Int(i)) => {
                    let i = i as usize;
                    let mut array_values = values.write().unwrap();
                    if i < array_values.len() {
                        let old_value = array_values[i].clone();
                        array_values[i] = value.clone();
                        if let Some(path) = runtime_patch_target_path(target) {
                            env.record_patch_change(ActivePatchChange {
                                path,
                                target: PatchMutationTarget::ArrayIndex {
                                    values: values.clone(),
                                    index: i,
                                },
                                old_value,
                                new_value: value,
                            });
                        }
                    } else {
                        return Err(KainError::runtime("Index out of bounds"));
                    }
                }
                _ => {
                    return Err(KainError::runtime(
                        "Index assignment only supported on arrays with int index",
                    ))
                }
            }
            Ok(())
        }
        _ => Err(KainError::runtime("Invalid assignment target")),
    };

    result?;
    if let Some(path) = target_path {
        env.propagate_entangled_write(&path, assigned_value)?;
    }
    Ok(())
}

fn assign_entangled_field_path(current: Value, fields: &[&str], value: Value) -> KainResult<()> {
    let Some((field, rest)) = fields.split_first() else {
        return Ok(());
    };

    let Value::Struct(_, slots) = current else {
        return Err(KainError::runtime(
            "Entangle mirror propagation only supports struct field paths",
        ));
    };

    if rest.is_empty() {
        slots.write().unwrap().insert((*field).to_string(), value);
        return Ok(());
    }

    let next = slots
        .read()
        .unwrap()
        .get(*field)
        .cloned()
        .ok_or_else(|| KainError::runtime(format!("Entangle field '{}' not found", field)))?;
    assign_entangled_field_path(next, rest, value)
}

fn eval_else_branch(env: &mut Env, else_branch: &ElseBranch) -> KainResult<Value> {
    match else_branch {
        ElseBranch::Else(block) => eval_block(env, block),
        ElseBranch::ElseIf(condition, then_branch, nested_else_branch) => {
            let condition_value = eval_expr(env, condition)?;
            if let Value::Return(_) = condition_value {
                return Ok(condition_value);
            }
            match condition_value {
                Value::Bool(true) => eval_block(env, then_branch),
                Value::Bool(false) => match nested_else_branch {
                    Some(next_branch) => eval_else_branch(env, next_branch),
                    None => Ok(Value::Unit),
                },
                _ => Err(KainError::runtime(
                    "Type error: if condition must evaluate to Bool",
                )),
            }
        }
    }
}

fn runtime_ownership_region_id(target: &Expr) -> String {
    match target {
        Expr::Ident(name, _) => name.clone(),
        Expr::AddrOf { value, .. } | Expr::Ref { value, .. } => runtime_ownership_region_id(value),
        Expr::Paren(value, _) => runtime_ownership_region_id(value),
        other => {
            let span = other.span();
            format!("ownership-region:{}..{}", span.start, span.end)
        }
    }
}

fn apply_runtime_ownership_transition(
    env: &mut Env,
    target: &Expr,
    transition: OwnershipTransition,
    operation: &str,
    span: Span,
) -> KainResult<()> {
    let region_id = runtime_ownership_region_id(target);
    let region = env
        .ownership_regions
        .entry(region_id.clone())
        .or_insert_with(|| {
            OwnershipRegionDescriptor::new(region_id.clone(), OwnershipRegionKind::HeapAllocation)
        });
    region.apply(transition).map(|_| ()).map_err(|err| {
        KainError::runtime(format!(
            "{operation} ownership transition failed for '{region_id}' at {}..{}: {err}",
            span.start, span.end
        ))
    })
}

fn eval_scoped_ownership_expr(
    env: &mut Env,
    target: &Expr,
    body: &Expr,
    begin: OwnershipTransition,
    end: OwnershipTransition,
    operation: &str,
    span: Span,
) -> KainResult<Value> {
    let _ = eval_expr(env, target)?;
    apply_runtime_ownership_transition(env, target, begin, operation, span)?;
    let body_result = eval_expr(env, body);
    let end_result = apply_runtime_ownership_transition(env, target, end, operation, span);
    match (body_result, end_result) {
        (Ok(value), Ok(())) => Ok(value),
        (Err(err), Ok(())) => Err(err),
        (Ok(_), Err(err)) | (Err(_), Err(err)) => Err(err),
    }
}

pub fn eval_expr(env: &mut Env, expr: &Expr) -> KainResult<Value> {
    match expr {
        Expr::MethodCall {
            receiver,
            method,
            args,
            span: _,
        } => {
            // Handle method call: obj.method(args)
            let obj_val = eval_expr(env, receiver)?;
            if let Value::Return(_) = obj_val {
                return Ok(obj_val);
            }

            // Evaluate arguments
            let mut arg_vals = Vec::new();
            for arg in args {
                let v = eval_expr(env, &arg.value)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                arg_vals.push(v);
            }

            match obj_val {
                // Struct methods: StructName_method(obj, args)
                Value::Struct(ref name, _) | Value::Future(ref name, _) => {
                    if name == "Map" {
                        match method.as_str() {
                            "iter" => {
                                if !arg_vals.is_empty() {
                                    return Err(KainError::runtime(
                                        "Map.iter expects no arguments",
                                    ));
                                }
                                if let Value::Struct(_, fields) = &obj_val {
                                    let items = fields
                                        .read()
                                        .unwrap()
                                        .iter()
                                        .map(|(key, value)| {
                                            Value::Tuple(vec![
                                                Value::String(key.clone()),
                                                value.clone(),
                                            ])
                                        })
                                        .collect();
                                    return Ok(runtime_array_value(items));
                                }
                            }
                            _ => {}
                        }
                    }
                    let func_name = format!("{}_{}", name, method);

                    if let Some(func) = env.functions.get(&func_name).cloned() {
                        // Call function with self as first argument
                        env.push_scope();
                        env.define("self".to_string(), obj_val);

                        // Bind other params
                        let param_iter = if func
                            .params
                            .first()
                            .map(|p| p.name == "self")
                            .unwrap_or(false)
                        {
                            func.params.iter().skip(1)
                        } else {
                            func.params.iter().skip(0)
                        };

                        if param_iter.len() != arg_vals.len() {
                            return Err(KainError::runtime(format!(
                                "Method {} arg mismatch",
                                func_name
                            )));
                        }

                        for (param, arg) in param_iter.zip(arg_vals.into_iter()) {
                            env.define(param.name.clone(), arg);
                        }

                        let result = eval_block(env, &func.body)?;
                        env.pop_scope();

                        match result {
                            Value::Return(v) => Ok(*v),
                            v => Ok(v),
                        }
                    } else if method == "to_string" {
                        if !arg_vals.is_empty() {
                            Err(KainError::runtime(format!(
                                "{}.to_string expects no arguments",
                                name
                            )))
                        } else {
                            Ok(Value::String(obj_val.to_string()))
                        }
                    } else {
                        Err(KainError::runtime(format!(
                            "Method {} not found for type {}",
                            method, name
                        )))
                    }
                }

                // Native Type Methods (e.g. Array.push, String.len)
                Value::Int(value) => match method.as_str() {
                    "to_string" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime("Int.to_string expects no arguments"));
                        }
                        Ok(Value::String(obj_val.to_string()))
                    }
                    "min" => eval_i64_binary_method(value, &arg_vals, method, i64::min),
                    "max" => eval_i64_binary_method(value, &arg_vals, method, i64::max),
                    "div_ceil" => eval_i64_div_ceil_method(value, &arg_vals, method),
                    "saturating_add" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::saturating_add)
                    }
                    "saturating_sub" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::saturating_sub)
                    }
                    "saturating_mul" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::saturating_mul)
                    }
                    "wrapping_add" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::wrapping_add)
                    }
                    "wrapping_sub" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::wrapping_sub)
                    }
                    "wrapping_mul" => {
                        eval_i64_binary_method(value, &arg_vals, method, i64::wrapping_mul)
                    }
                    "wrapping_shl" => {
                        eval_i64_shift_method(value, &arg_vals, method, i64::wrapping_shl)
                    }
                    "wrapping_shr" => {
                        eval_i64_shift_method(value, &arg_vals, method, i64::wrapping_shr)
                    }
                    _ => Err(KainError::runtime(format!(
                        "Method {} not found on Int",
                        method
                    ))),
                },
                Value::Float(value) => match method.as_str() {
                    "to_string" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime("Float.to_string expects no arguments"));
                        }
                        Ok(Value::String(obj_val.to_string()))
                    }
                    "min" => eval_f64_binary_method(value, &arg_vals, method, f64::min),
                    "max" => eval_f64_binary_method(value, &arg_vals, method, f64::max),
                    _ => Err(KainError::runtime(format!(
                        "Method {} not found on Float",
                        method
                    ))),
                },
                Value::None => match method.as_str() {
                    "cloned" | "copied" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime(format!(
                                "Option.{method} expects no arguments"
                            )));
                        }
                        Ok(Value::None)
                    }
                    "take" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime("Option.take expects no arguments"));
                        }
                        Ok(Value::None)
                    }
                    "filter" => {
                        if arg_vals.len() != 1 {
                            return Err(KainError::runtime("Option.filter expects 1 argument"));
                        }
                        Ok(Value::None)
                    }
                    "or" | "or_" => {
                        if arg_vals.len() != 1 {
                            return Err(KainError::runtime("Option.or expects 1 argument"));
                        }
                        Ok(arg_vals[0].clone())
                    }
                    _ => Err(KainError::runtime(format!(
                        "Method {} not found on Option",
                        method
                    ))),
                },
                Value::EnumVariant(ref enum_name, ref variant, ref fields)
                    if enum_name == "Option" =>
                {
                    match method.as_str() {
                        "cloned" | "copied" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime(format!(
                                    "Option.{method} expects no arguments"
                                )));
                            }
                            Ok(obj_val)
                        }
                        "take" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime("Option.take expects no arguments"));
                            }
                            if variant == "Some" && fields.len() == 1 {
                                eval_assignment(env, receiver, Value::None)?;
                                Ok(obj_val)
                            } else {
                                Ok(Value::None)
                            }
                        }
                        "filter" => {
                            if arg_vals.len() != 1 {
                                return Err(KainError::runtime("Option.filter expects 1 argument"));
                            }
                            if variant == "Some" && fields.len() == 1 {
                                match call_function(
                                    env,
                                    arg_vals[0].clone(),
                                    vec![fields[0].clone()],
                                )? {
                                    Value::Bool(true) => Ok(obj_val),
                                    Value::Bool(false) => Ok(Value::None),
                                    other => Err(KainError::runtime(format!(
                                        "Option.filter predicate must return Bool, found {}",
                                        runtime_value_kind(&other)
                                    ))),
                                }
                            } else {
                                Ok(Value::None)
                            }
                        }
                        "or" | "or_" => {
                            if arg_vals.len() != 1 {
                                return Err(KainError::runtime("Option.or expects 1 argument"));
                            }
                            if variant == "Some" && fields.len() == 1 {
                                Ok(obj_val)
                            } else {
                                Ok(arg_vals[0].clone())
                            }
                        }
                        _ => Err(KainError::runtime(format!(
                            "Method {} not found on Option",
                            method
                        ))),
                    }
                }
                Value::Array(_) => {
                    // Map common array methods to native functions
                    match method.as_str() {
                        "to_string" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime(
                                    "Array.to_string expects no arguments",
                                ));
                            }
                            Ok(Value::String(obj_val.to_string()))
                        }
                        "is_empty" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime(
                                    "Array.is_empty expects no arguments",
                                ));
                            }
                            if let Value::Array(arr) = obj_val {
                                Ok(Value::Bool(arr.read().unwrap().is_empty()))
                            } else {
                                unreachable!()
                            }
                        }
                        "push" => {
                            if arg_vals.len() != 1 {
                                return Err(KainError::runtime("push expects 1 argument"));
                            }
                            if let Value::Array(arr) = obj_val {
                                arr.write().unwrap().push(arg_vals[0].clone());
                                Ok(Value::Unit)
                            } else {
                                unreachable!()
                            }
                        }
                        "first" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime("first expects no arguments"));
                            }
                            if let Value::Array(arr) = obj_val {
                                Ok(arr.read().unwrap().first().cloned().unwrap_or(Value::None))
                            } else {
                                unreachable!()
                            }
                        }
                        "last" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime("last expects no arguments"));
                            }
                            if let Value::Array(arr) = obj_val {
                                Ok(arr.read().unwrap().last().cloned().unwrap_or(Value::None))
                            } else {
                                unreachable!()
                            }
                        }
                        "pop" => {
                            if !arg_vals.is_empty() {
                                return Err(KainError::runtime("pop expects no arguments"));
                            }
                            if let Value::Array(arr) = obj_val {
                                Ok(arr.write().unwrap().pop().unwrap_or(Value::None))
                            } else {
                                unreachable!()
                            }
                        }
                        "len" => {
                            if let Value::Array(arr) = obj_val {
                                Ok(Value::Int(arr.read().unwrap().len() as i64))
                            } else {
                                unreachable!()
                            }
                        }
                        "binary_search" => {
                            if arg_vals.len() != 1 {
                                return Err(KainError::runtime("binary_search expects 1 argument"));
                            }
                            if let Value::Array(arr) = obj_val {
                                eval_array_binary_search(&arr.read().unwrap(), &arg_vals[0])
                            } else {
                                unreachable!()
                            }
                        }
                        _ => Err(KainError::runtime(format!(
                            "Method {} not found on Array",
                            method
                        ))),
                    }
                }
                Value::String(text) => match method.as_str() {
                    "to_string" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime(
                                "String.to_string expects no arguments",
                            ));
                        }
                        Ok(Value::String(text.clone()))
                    }
                    "is_empty" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime("String.is_empty expects no arguments"));
                        }
                        Ok(Value::Bool(text.is_empty()))
                    }
                    "len" => Ok(Value::Int(text.len() as i64)),
                    "trim" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime("String.trim expects no arguments"));
                        }
                        Ok(Value::String(text.trim().to_string()))
                    }
                    "to_ascii_lowercase" => {
                        if !arg_vals.is_empty() {
                            return Err(KainError::runtime(
                                "String.to_ascii_lowercase expects no arguments",
                            ));
                        }
                        Ok(Value::String(text.to_ascii_lowercase()))
                    }
                    "starts_with" => {
                        let prefix = expect_single_string_arg(&arg_vals, method)?;
                        Ok(Value::Bool(text.starts_with(prefix)))
                    }
                    "eq_ignore_ascii_case" => {
                        let other = expect_single_string_arg(&arg_vals, method)?;
                        Ok(Value::Bool(text.eq_ignore_ascii_case(other)))
                    }
                    "push_str" => {
                        let suffix = expect_single_string_arg(&arg_vals, method)?;
                        let mut updated = text.clone();
                        updated.push_str(suffix);
                        eval_assignment(env, receiver, Value::String(updated))?;
                        Ok(Value::Unit)
                    }
                    "repeat" => {
                        let count = *expect_single_int_arg(&arg_vals, method)?;
                        if count < 0 {
                            return Err(KainError::runtime(
                                "String.repeat expects a non-negative count",
                            ));
                        }
                        Ok(Value::String(text.repeat(count as usize)))
                    }
                    _ => Err(KainError::runtime(format!(
                        "Method {} not found on String",
                        method
                    ))),
                },

                _ if method == "to_string" => {
                    if !arg_vals.is_empty() {
                        Err(KainError::runtime("to_string expects no arguments"))
                    } else {
                        Ok(Value::String(obj_val.to_string()))
                    }
                }

                _ => Err(KainError::runtime(format!(
                    "Method calls not supported on this type: {:?}",
                    obj_val
                ))),
            }
        }

        Expr::Call { callee, args, .. } => {
            // Special case: Handle Type.method() or obj.method() calls
            if let Expr::Field { object, field, .. } = callee.as_ref() {
                // Check if this is a type-level static method call like RNG.new()
                if let Expr::Ident(type_name, _) = object.as_ref() {
                    // Check if it's a type with methods - clone to avoid borrow issues
                    let method = env
                        .methods
                        .get(type_name)
                        .and_then(|m| m.get(field))
                        .cloned();

                    if let Some(method) = method {
                        // Evaluate arguments
                        let mut arg_vals = Vec::new();
                        for arg in args {
                            let v = eval_expr(env, &arg.value)?;
                            if let Value::Return(_) = v {
                                return Ok(v);
                            }
                            arg_vals.push(v);
                        }

                        // Call the static method
                        env.push_scope();
                        for (param, arg) in method.params.iter().zip(arg_vals.into_iter()) {
                            env.define(param.name.clone(), arg);
                        }
                        let result = eval_block(env, &method.body);
                        env.pop_scope();

                        return match result? {
                            Value::Return(v) => Ok(*v),
                            v => Ok(v),
                        };
                    }
                }

                // Check if this is an instance method call like obj.method()
                let obj_val = eval_expr(env, object)?;
                if let Value::Return(_) = obj_val {
                    return Ok(obj_val);
                }

                // Get the type name from the value
                let type_name = match &obj_val {
                    Value::Struct(name, _) => Some(name.clone()),
                    _ => None,
                };

                if let Some(type_name) = type_name {
                    // Clone method to avoid borrow issues
                    let method = env
                        .methods
                        .get(&type_name)
                        .and_then(|m| m.get(field))
                        .cloned();

                    if let Some(method) = method {
                        // Evaluate arguments
                        let mut arg_vals = Vec::new();
                        for arg in args {
                            let v = eval_expr(env, &arg.value)?;
                            if let Value::Return(_) = v {
                                return Ok(v);
                            }
                            arg_vals.push(v);
                        }

                        // Call the instance method with `self` bound
                        env.push_scope();
                        env.define("self".to_string(), obj_val);

                        // Skip 'self' parameter if present in method definition
                        let params_iter = if let Some(first) = method.params.first() {
                            if first.name == "self" {
                                method.params.iter().skip(1)
                            } else {
                                method.params.iter().skip(0)
                            }
                        } else {
                            method.params.iter().skip(0)
                        };

                        for (param, arg) in params_iter.zip(arg_vals.into_iter()) {
                            env.define(param.name.clone(), arg);
                        }
                        let result = eval_block(env, &method.body);
                        env.pop_scope();

                        return match result? {
                            Value::Return(v) => Ok(*v),
                            v => Ok(v),
                        };
                    }
                }
            }

            // Normal function call
            let func_val = {
                let v = eval_expr(env, callee)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                v
            };

            // Evaluate arguments
            let mut arg_vals = Vec::new();
            for arg in args {
                let v = eval_expr(env, &arg.value)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                arg_vals.push(v);
            }

            call_function(env, func_val, arg_vals)
        }

        Expr::StageCall {
            runtime,
            function,
            args,
            ..
        } => {
            let mut arg_vals = Vec::new();
            for arg in args {
                let value = eval_expr(env, &arg.value)?;
                if let Value::Return(_) = value {
                    return Ok(value);
                }
                arg_vals.push(value);
            }
            execute_stage_call(env, *runtime, function, arg_vals)
        }

        Expr::Try(inner, _) => {
            let val = eval_expr(env, inner)?;
            if let Value::Return(_) = val {
                return Ok(val);
            }
            match val {
                Value::Result(true, v) => Ok(*v),
                Value::Result(false, e) => Ok(Value::Return(Box::new(Value::Result(false, e)))),
                Value::None => Ok(Value::Return(Box::new(Value::None))),
                Value::EnumVariant(enum_name, variant, fields)
                    if enum_name == "Option" && variant == "Some" && fields.len() == 1 =>
                {
                    Ok(fields[0].clone())
                }
                Value::EnumVariant(enum_name, variant, _)
                    if enum_name == "Option" && variant == "None" =>
                {
                    Ok(Value::Return(Box::new(Value::None)))
                }
                _ => Err(KainError::runtime(
                    "Type error: expected Result or Option for ? operator",
                )),
            }
        }

        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            let cond = eval_expr(env, condition)?;
            if let Value::Return(_) = cond {
                return Ok(cond);
            }
            if let Value::Bool(true) = cond {
                eval_block(env, then_branch)
            } else if let Some(eb) = else_branch {
                eval_else_branch(env, eb)
            } else {
                Ok(Value::Unit)
            }
        }

        Expr::Match {
            scrutinee, arms, ..
        } => {
            let val = eval_expr(env, scrutinee)?;
            if let Value::Return(_) = val {
                return Ok(val);
            }

            for arm in arms {
                if pattern_matches(&arm.pattern, &val) {
                    env.push_scope();
                    bind_pattern(env, &arm.pattern, &val);
                    let res = eval_expr(env, &arm.body)?;
                    env.pop_scope();
                    return Ok(res);
                }
            }
            // If no match, check if it's exhaustive or return unit?
            Ok(Value::Unit)
        }

        Expr::MacroCall { name, args, .. } => {
            // Built-in macros
            match name.as_str() {
                "vec" => {
                    let mut vals = Vec::new();
                    for arg in args {
                        let v = eval_expr(env, arg)?;
                        if let Value::Return(_) = v {
                            return Ok(v);
                        }
                        vals.push(v);
                    }
                    Ok(Value::Array(Arc::new(RwLock::new(vals))))
                }
                "format" => {
                    // TODO: proper format
                    let mut res = String::new();
                    for arg in args {
                        let v = eval_expr(env, arg)?;
                        res.push_str(&format!("{}", v));
                    }
                    Ok(Value::String(res))
                }
                "__kain_write_fmt" | "__kain_writeln_fmt" => {
                    if args.len() != 2 {
                        return Err(KainError::runtime(format!(
                            "{name}: expected destination and message"
                        )));
                    }
                    let message = eval_expr(env, &args[1])?;
                    let suffix = if name == "__kain_writeln_fmt" {
                        "\n"
                    } else {
                        ""
                    };
                    Ok(Value::String(format!("{}{}", message, suffix)))
                }
                "type_name" => {
                    if let Some(arg) = args.first() {
                        let v = eval_expr(env, arg)?;
                        let type_name = match v {
                            Value::Unit => "unit",
                            Value::Bool(_) => "bool",
                            Value::Int(_) => "int",
                            Value::Float(_) => "float",
                            Value::String(_) => "string",
                            Value::Array(_) => "array",
                            Value::Tuple(_) => "tuple",
                            Value::Struct(name, _) => return Ok(Value::String(name.clone())),
                            Value::HostObject(name, _) => return Ok(Value::String(name.clone())),
                            Value::Function(_) => "function",
                            Value::Patch(_) => "patch",
                            Value::Law(_) => "law",
                            Value::Converge(_) => "converge",
                            Value::Orchestrate(_) => "orchestrate",
                            Value::NativeFn(_, _) => "native_fn",
                            Value::StructConstructor(_, _) => "struct_constructor",
                            Value::ActorRef(_) => "actor",
                            Value::None => "none",
                            Value::Return(_) => "return",
                            Value::Result(_, _) => "result",
                            Value::Closure(_, _, _) => "closure",
                            Value::JSX(_) => "jsx",
                            Value::EnumVariant(enum_name, _, _) => {
                                return Ok(Value::String(enum_name.clone()))
                            }
                            Value::Poll(_, _) => "poll",
                            Value::Future(name, _) => {
                                return Ok(Value::String(format!("Future<{}>", name)))
                            }
                            Value::Break(_) => "break",
                            Value::Continue => "continue",
                        };
                        Ok(Value::String(type_name.to_string()))
                    } else {
                        Err(KainError::runtime("type_name! requires an argument"))
                    }
                }
                _ => Err(KainError::runtime(format!("Unknown macro: {}!", name))),
            }
        }
        Expr::Assign { target, value, .. } => {
            let v = eval_expr(env, value)?;
            if let Value::Return(_) = v {
                return Ok(v);
            }
            eval_assignment(env, target, v)?;
            Ok(Value::Unit)
        }
        Expr::Int(n, _) => Ok(Value::Int(*n)),
        Expr::Float(n, _) => Ok(Value::Float(*n)),
        Expr::String(s, _) => Ok(Value::String(s.clone())),
        Expr::FString(parts, _) => {
            let mut result = String::new();
            for part in parts {
                let val = eval_expr(env, part)?;
                if let Value::Return(_) = val {
                    return Ok(val);
                }
                result.push_str(&format!("{}", val));
            }
            Ok(Value::String(result))
        }
        Expr::Bool(b, _) => Ok(Value::Bool(*b)),
        Expr::None(_) => Ok(Value::None),
        Expr::Lambda { params, body, .. } => {
            let param_names = params.iter().map(|p| p.name.clone()).collect();
            Ok(Value::Closure(
                param_names,
                body.clone(),
                env.scopes.clone(),
            ))
        }
        Expr::Ident(name, _span) => env
            .lookup(name)
            .cloned()
            .ok_or_else(|| KainError::runtime(format!("Undefined: {}", name))),

        Expr::Binary {
            left, op, right, ..
        } => {
            let l = eval_expr(env, left)?;
            if let Value::Return(_) = l {
                return Ok(l);
            }
            let r = eval_expr(env, right)?;
            if let Value::Return(_) = r {
                return Ok(r);
            }
            eval_binop(*op, l, r)
        }

        Expr::Unary { op, operand, .. } => {
            let v = eval_expr(env, operand)?;
            if let Value::Return(_) = v {
                return Ok(v);
            }
            match (op, v) {
                (UnaryOp::Neg, Value::Int(n)) => Ok(Value::Int(-n)),
                (UnaryOp::Neg, Value::Float(n)) => Ok(Value::Float(-n)),
                (UnaryOp::Not, Value::Bool(b)) => Ok(Value::Bool(!b)),
                (UnaryOp::BitNot, Value::Int(n)) => Ok(Value::Int(!n)),
                _ => Err(KainError::runtime("Invalid unary operation")),
            }
        }

        Expr::Array(elements, _) => {
            let mut vals = Vec::new();
            for elem in elements {
                let v = eval_expr(env, elem)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                vals.push(v);
            }
            Ok(Value::Array(Arc::new(RwLock::new(vals))))
        }

        Expr::Index { object, index, .. } => {
            let obj = eval_expr(env, object)?;
            if let Value::Return(_) = obj {
                return Ok(obj);
            }

            if let Expr::Range { start, end, .. } = index.as_ref() {
                let start_idx = match start {
                    Some(expr) => match eval_expr(env, expr)? {
                        Value::Int(i) => i.max(0) as usize,
                        value @ Value::Return(_) => return Ok(value),
                        _ => {
                            return Err(KainError::runtime(
                                "Range index start must evaluate to an Int",
                            ))
                        }
                    },
                    None => 0,
                };
                let end_idx = match end {
                    Some(expr) => match eval_expr(env, expr)? {
                        Value::Int(i) => Some(i.max(0) as usize),
                        value @ Value::Return(_) => return Ok(value),
                        _ => {
                            return Err(KainError::runtime(
                                "Range index end must evaluate to an Int",
                            ))
                        }
                    },
                    None => None,
                };

                return match obj {
                    Value::Array(arr) => {
                        let arr = arr.read().unwrap();
                        let end_idx = end_idx.unwrap_or(arr.len()).min(arr.len());
                        if start_idx > end_idx {
                            return Err(KainError::runtime("Invalid array range slice"));
                        }
                        Ok(Value::Array(Arc::new(RwLock::new(
                            arr[start_idx..end_idx].to_vec(),
                        ))))
                    }
                    Value::String(text) => {
                        let end_idx = end_idx.unwrap_or(text.len()).min(text.len());
                        if start_idx > end_idx {
                            return Err(KainError::runtime("Invalid string range slice"));
                        }
                        match text.get(start_idx..end_idx) {
                            Some(slice) => Ok(Value::String(slice.to_string())),
                            None => Err(KainError::runtime(
                                "String range slice must align with UTF-8 boundaries",
                            )),
                        }
                    }
                    _ => Err(KainError::runtime(
                        "Range indexing requires an array or string receiver",
                    )),
                };
            }

            let idx = eval_expr(env, index)?;
            if let Value::Return(_) = idx {
                return Ok(idx);
            }

            match (obj, idx) {
                (Value::Array(arr), Value::Int(i)) => {
                    let i = i as usize;
                    let arr = arr.read().unwrap();
                    if i < arr.len() {
                        Ok(arr[i].clone())
                    } else {
                        Err(KainError::runtime(format!("Index out of bounds: {}", i)))
                    }
                }
                (Value::String(s), Value::Int(i)) => {
                    let i = i as usize;
                    if i < s.len() {
                        Ok(Value::String(s.chars().nth(i).unwrap().to_string()))
                    } else {
                        Err(KainError::runtime(format!("Index out of bounds: {}", i)))
                    }
                }
                _ => Err(KainError::runtime(
                    "Index operator requires array/string and int",
                )),
            }
        }

        // Structure creation
        Expr::Struct {
            name, fields, rest, ..
        } => {
            let mut field_vals = if let Some(rest) = rest {
                match eval_expr(env, rest)? {
                    Value::Return(value) => return Ok(Value::Return(value)),
                    Value::Struct(_, fields) => fields.read().unwrap().clone(),
                    Value::None => HashMap::new(),
                    other => {
                        return Err(KainError::runtime(format!(
                            "Struct update syntax requires a struct value, found {:?}",
                            other
                        )));
                    }
                }
            } else {
                HashMap::new()
            };
            for (k, expr) in fields {
                let v = eval_expr(env, expr)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                field_vals.insert(k.clone(), v);
            }
            Ok(Value::Struct(
                name.clone(),
                Arc::new(RwLock::new(field_vals)),
            ))
        }
        Expr::AggregateInit { ty, fields, .. } => {
            let mut field_vals = HashMap::new();
            for (k, expr) in fields {
                let v = eval_expr(env, expr)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                field_vals.insert(k.clone(), v);
            }
            let name = match ty {
                Type::Named { name, .. } => name.clone(),
                _ => "<aggregate>".to_string(),
            };
            Ok(Value::Struct(name, Arc::new(RwLock::new(field_vals))))
        }

        // JSX
        Expr::JSX(node, _) => eval_jsx(env, node),

        Expr::Field { object, field, .. } => {
            let obj_val = eval_expr(env, object)?;
            if let Value::Return(_) = obj_val {
                return Ok(obj_val);
            }

            match obj_val {
                Value::Struct(_, fields) => {
                    let fields = fields.read().unwrap();
                    fields
                        .get(field)
                        .cloned()
                        .ok_or_else(|| KainError::runtime(format!("Field not found: {}", field)))
                }
                Value::Tuple(items) => {
                    let index = match field.as_str() {
                        "x" | "r" => 0,
                        "y" | "g" => 1,
                        "z" | "b" => 2,
                        "w" | "a" => 3,
                        _ => match field.strip_prefix("__kain_tuple_") {
                            Some(index) => index.parse::<usize>().map_err(|_| {
                                KainError::runtime(format!(
                                    "Field access on tuple uses unknown field {field}"
                                ))
                            })?,
                            None => {
                                return Err(KainError::runtime(format!(
                                    "Field access on tuple uses unknown field {field}"
                                )))
                            }
                        },
                    };
                    items.get(index).cloned().ok_or_else(|| {
                        KainError::runtime(format!("Tuple field not found: {}", field))
                    })
                }
                Value::ActorRef(r) => {
                    // Check if it's the current actor (self)
                    if let Some(self_id) = env.self_actor_id {
                        if self_id == r.id {
                            return env.lookup(field).cloned().ok_or_else(|| {
                                KainError::runtime(format!("Actor field not found: {}", field))
                            });
                        }
                    }

                    // Allow accessing actor fields? For now maybe just id
                    if field == "id" {
                        return Ok(Value::Int(r.id.as_u64() as i64));
                    }
                    Err(KainError::runtime("Actor fields not accessible"))
                }
                _ => Err(KainError::runtime(format!(
                    "Field access on non-struct value: {:?}",
                    obj_val
                ))),
            }
        }

        Expr::Tuple(elements, _) => {
            let mut vals = Vec::new();
            for elem in elements {
                let v = eval_expr(env, elem)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                vals.push(v);
            }
            Ok(Value::Tuple(vals))
        }

        Expr::Spawn { actor, init, .. } => {
            // Find actor definition
            let actor_def = env
                .actor_defs
                .get(actor)
                .cloned()
                .ok_or_else(|| KainError::runtime(format!("Unknown actor: {}", actor)))?;

            // Evaluate init expressions
            let mut init_vals = HashMap::new();
            for (field, expr) in init {
                let v = eval_expr(env, expr)?;
                if let Value::Return(_) = v {
                    return Ok(v);
                }
                init_vals.insert(field.clone(), v);
            }

            // Create channel
            let (tx, rx) = flume::unbounded();
            let id = env.actor_ids.allocate();
            let sender = tx.clone();
            env.actors.insert(id, sender.clone());

            // Spawn thread
            let functions = env.functions.clone();
            let function_inline_scopes = env.function_inline_scopes.clone();
            let components = env.components.clone();
            let actor_defs = env.actor_defs.clone();
            let inline_modules = env.inline_modules.clone();
            let methods = env.methods.clone();
            let patches = env.patches.clone();
            let patch_undo_modes = env.patch_undo_modes.clone();
            let laws = env.laws.clone();
            let converges = env.converges.clone();
            let orchestrates = env.orchestrates.clone();
            let worlds = env.worlds.clone();
            let entanglements = env.entanglements.clone();
            let global_scope = env.scopes.first().cloned().unwrap_or_default();
            let actor_name = actor.clone();
            let self_sender = tx.clone();
            let execution_lane = env.execution_lane;
            let active_capabilities = env.active_capabilities.clone();

            std::thread::spawn(move || {
                let mut actor_env = Env {
                    scopes: vec![global_scope],
                    functions,
                    function_inline_scopes,
                    patches,
                    patch_undo_modes,
                    laws,
                    converges,
                    orchestrates,
                    worlds,
                    entanglements,
                    components,
                    inline_modules,
                    methods,
                    actors: HashMap::new(),
                    actor_ids: ActorIdAllocator::starting_after(
                        id.as_u64().saturating_mul(1_000_000),
                    ),
                    actor_defs,
                    self_actor_id: Some(id),
                    execution_lane,
                    active_capabilities,
                    ownership_regions: HashMap::new(),
                    active_patch_frames: Vec::new(),
                    patch_records: Vec::new(),
                    patch_replay_catalog: Vec::new(),
                    replayable_patch_history: Vec::new(),
                    undone_patch_records: Vec::new(),
                    patch_collaboration_events: Vec::new(),
                    extension_state: HashMap::new(),
                };

                actor_env.register_stdlib();
                actor_env.apply_registered_extensions();

                actor_env.push_scope(); // Actor scope

                // Define self
                let actor_val = Value::ActorRef(ActorRef {
                    id,
                    sender: self_sender,
                });
                actor_env.define("self".to_string(), actor_val);

                // Initialize state
                for state_decl in &actor_def.state {
                    if let Some(val) = init_vals.get(&state_decl.name) {
                        actor_env.define(state_decl.name.clone(), val.clone());
                    } else {
                        // Evaluate default value
                        match eval_expr(&mut actor_env, &state_decl.initial) {
                            Ok(val) => actor_env.define(state_decl.name.clone(), val),
                            Err(e) => {
                                eprintln!("Actor initialization error: {}", e);
                                return;
                            }
                        }
                    }
                }

                // Event loop
                while let Ok(msg) = rx.recv() {
                    // Find handler
                    let mut handled = false;
                    for handler in &actor_def.handlers {
                        if handler.message_type == msg.name {
                            // Run handler
                            actor_env.push_scope();
                            // Bind params by position
                            for (i, param) in handler.params.iter().enumerate() {
                                if let Some(val) = msg.args.get(i) {
                                    actor_env.define(param.name.clone(), val.clone());
                                }
                            }

                            if let Err(e) = eval_block(&mut actor_env, &handler.body) {
                                println!("Error in actor handler {}: {}", handler.message_type, e);
                            }
                            actor_env.pop_scope();
                            handled = true;
                            break;
                        }
                    }
                    if !handled {
                        println!(
                            "Actor {} received unknown message: {}",
                            actor_name, msg.name
                        );
                    }
                }
            });

            Ok(Value::ActorRef(ActorRef { id, sender }))
        }

        Expr::SendMsg {
            target,
            message,
            data,
            ..
        } => {
            let actor_val = eval_expr(env, target)?;
            if let Value::Return(_) = actor_val {
                return Ok(actor_val);
            }

            if let Value::ActorRef(r) = actor_val {
                let mut msg_args = Vec::new();
                for (_name, expr) in data {
                    let v = eval_expr(env, expr)?;
                    msg_args.push(v);
                }

                let msg = Message {
                    name: message.clone(),
                    args: msg_args,
                };

                let _ = r.sender.send(msg);
                Ok(Value::Unit)
            } else {
                Err(KainError::runtime("send target must be an actor"))
            }
        }

        // Block expression: { stmts }
        Expr::Block(block, _) => eval_block(env, block),

        // Return expression in expression context
        Expr::Return(expr, _) => {
            let val = if let Some(e) = expr {
                eval_expr(env, e)?
            } else {
                Value::Unit
            };
            Ok(Value::Return(Box::new(val)))
        }

        // Pointer/reference model is currently represented as direct value pass-through.
        Expr::Ref { value, .. } => eval_expr(env, value),
        Expr::AddrOf { value, .. } => eval_expr(env, value),

        // Pointer-style dereference is currently modeled as identity at runtime.
        // This keeps imported C-like code parseable/executable in non-native backends.
        Expr::Deref(inner, _) => eval_expr(env, inner),

        // Pointer offset is currently modeled as transparent pointer propagation.
        // Backend memory validation should stop unsupported targets before codegen.
        Expr::PtrOffset { pointer, .. } => eval_expr(env, pointer),

        // Raw memory load currently falls back to pointer propagation in the interpreter.
        Expr::MemLoad { pointer, .. } => eval_expr(env, pointer),

        // Raw memory store currently evaluates the value and returns unit in the interpreter.
        Expr::MemStore { value, .. } => {
            let evaluated = eval_expr(env, value)?;
            if let Value::Return(_) = evaluated {
                return Ok(evaluated);
            }
            Ok(Value::Unit)
        }

        // Layout-backed size query currently uses the same coarse scalar sizing as lowering fallback.
        Expr::SizeOfType { target, .. } => Ok(Value::Int(match target {
            Type::Named { name, .. } if name == "Int" || name == "UInt" || name == "Float" => 8,
            Type::Named { name, .. } if name == "Bool" || name == "Byte" || name == "Char" => 1,
            Type::Ptr { .. } | Type::Ref { .. } => 8,
            Type::Array(inner, size, _) => {
                let inner_size = match inner.as_ref() {
                    Type::Named { name, .. }
                        if name == "Int" || name == "UInt" || name == "Float" =>
                    {
                        8
                    }
                    Type::Named { name, .. }
                        if name == "Bool" || name == "Byte" || name == "Char" =>
                    {
                        1
                    }
                    Type::Ptr { .. } | Type::Ref { .. } => 8,
                    _ => 8,
                };
                inner_size * *size as i64
            }
            _ => 8,
        })),
        Expr::AlignOfType { target, .. } => Ok(Value::Int(match target {
            Type::Named { name, .. } if name == "Bool" || name == "Byte" || name == "Char" => 1,
            Type::Unit(_) | Type::Never(_) => 1,
            _ => 8,
        })),
        Expr::Alloca { ty, .. } => Ok(match ty {
            Type::Array(_, count, _) => Value::Array(Arc::new(RwLock::new(
                (0..*count).map(|_| Value::None).collect(),
            ))),
            _ => Value::None,
        }),
        Expr::Uninit { .. } => Ok(Value::None),
        Expr::Alloc { zeroed, .. } => Ok(if *zeroed { Value::Int(0) } else { Value::None }),
        Expr::Realloc { .. } => Ok(Value::Int(0)),
        Expr::Observe { target, body, span } => eval_scoped_ownership_expr(
            env,
            target,
            body,
            OwnershipTransition::BeginObserve,
            OwnershipTransition::EndObserve,
            "observe",
            *span,
        ),
        Expr::Collapse { target, body, span } => eval_scoped_ownership_expr(
            env,
            target,
            body,
            OwnershipTransition::BeginCollapse,
            OwnershipTransition::EndCollapse,
            "collapse",
            *span,
        ),
        Expr::Decay { target, span } => {
            let _ = eval_expr(env, target)?;
            apply_runtime_ownership_transition(
                env,
                target,
                OwnershipTransition::Decay,
                "decay",
                *span,
            )?;
            Ok(Value::Unit)
        }

        Expr::Paren(inner, _) => eval_expr(env, inner),

        // Await expression: await future_expr
        // Uses the async runtime to poll the future to completion
        Expr::Await(future_expr, _span) => {
            let future_val = eval_expr(env, future_expr)?;
            if let Value::Return(_) = future_val {
                return Ok(future_val);
            }

            // Use the async runtime to poll to completion
            poll_future_to_completion(env, future_val)
        }
        Expr::AsyncBlock(body, _) => eval_expr(env, body),

        // OR static method call: TypeName::method(args)
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            // First, check if this is a static method call
            // Check if enum_name is a type with methods and variant is a method name
            if let Some(type_methods) = env.methods.get(enum_name).cloned() {
                if let Some(method) = type_methods.get(variant).cloned() {
                    // This is a static method call like Lexer::new(source)
                    let arg_vals: Vec<Value> = match fields {
                        EnumVariantFields::Unit => Vec::new(),
                        EnumVariantFields::Tuple(exprs) => {
                            let mut vals = Vec::new();
                            for e in exprs {
                                let v = eval_expr(env, e)?;
                                if let Value::Return(_) = v {
                                    return Ok(v);
                                }
                                vals.push(v);
                            }
                            vals
                        }
                        EnumVariantFields::Struct(named_fields) => {
                            let mut vals = Vec::new();
                            for (_, e) in named_fields {
                                let v = eval_expr(env, e)?;
                                if let Value::Return(_) = v {
                                    return Ok(v);
                                }
                                vals.push(v);
                            }
                            vals
                        }
                    };

                    // Call the static method
                    env.push_scope();
                    for (param, arg) in method.params.iter().zip(arg_vals.into_iter()) {
                        env.define(param.name.clone(), arg);
                    }
                    let result = eval_block(env, &method.body)?;
                    env.pop_scope();

                    return match result {
                        Value::Return(v) => Ok(*v),
                        v => Ok(v),
                    };
                }
            }

            // Check for lowered function name: Type_method (from monomorphization)
            let lowered_name = format!("{}_{}", enum_name, variant);
            if let Some(func) = env.functions.get(&lowered_name).cloned() {
                // This is a lowered method call (Type_method from monomorphization)
                let arg_vals: Vec<Value> = match fields {
                    EnumVariantFields::Unit => Vec::new(),
                    EnumVariantFields::Tuple(exprs) => {
                        let mut vals = Vec::new();
                        for e in exprs {
                            let v = eval_expr(env, e)?;
                            if let Value::Return(_) = v {
                                return Ok(v);
                            }
                            vals.push(v);
                        }
                        vals
                    }
                    EnumVariantFields::Struct(named_fields) => {
                        let mut vals = Vec::new();
                        for (_, e) in named_fields {
                            let v = eval_expr(env, e)?;
                            if let Value::Return(_) = v {
                                return Ok(v);
                            }
                            vals.push(v);
                        }
                        vals
                    }
                };

                // Call the lowered function
                env.push_scope();
                for (param, arg) in func.params.iter().zip(arg_vals.into_iter()) {
                    env.define(param.name.clone(), arg);
                }
                let result = eval_block(env, &func.body)?;
                env.pop_scope();

                return match result {
                    Value::Return(v) => Ok(*v),
                    v => Ok(v),
                };
            }

            // Not a static method call - proceed with enum variant construction
            let field_vals = match fields {
                EnumVariantFields::Unit => Vec::new(),
                EnumVariantFields::Tuple(exprs) => {
                    let mut vals = Vec::new();
                    for e in exprs {
                        let v = eval_expr(env, e)?;
                        if let Value::Return(_) = v {
                            return Ok(v);
                        }
                        vals.push(v);
                    }
                    vals
                }
                EnumVariantFields::Struct(named_fields) => {
                    let mut vals = Vec::new();
                    for (_, e) in named_fields {
                        let v = eval_expr(env, e)?;
                        if let Value::Return(_) = v {
                            return Ok(v);
                        }
                        vals.push(v);
                    }
                    vals
                }
            };

            // Special case: Poll enum gets native representation
            if enum_name == "Poll" {
                match variant.as_str() {
                    "Ready" => {
                        let inner = if field_vals.is_empty() {
                            None
                        } else {
                            Some(Box::new(
                                field_vals.into_iter().next().unwrap_or(Value::Unit),
                            ))
                        };
                        Ok(Value::Poll(true, inner))
                    }
                    "Pending" => Ok(Value::Poll(false, None)),
                    _ => Ok(Value::EnumVariant(
                        enum_name.clone(),
                        variant.clone(),
                        field_vals,
                    )),
                }
            } else {
                Ok(Value::EnumVariant(
                    enum_name.clone(),
                    variant.clone(),
                    field_vals,
                ))
            }
        }

        Expr::Break(expr, _) => {
            let val = if let Some(e) = expr {
                Some(Box::new(eval_expr(env, e)?))
            } else {
                None
            };
            Ok(Value::Break(val))
        }

        Expr::Continue(_) => Ok(Value::Continue),

        _ => Err(KainError::runtime(format!(
            "Expression not supported in runtime: {:?}",
            expr
        ))),
    }
}

fn call_function(env: &mut Env, func: Value, args: Vec<Value>) -> KainResult<Value> {
    match func {
        Value::Function(name) => {
            let Some(f) = env.functions.get(&name).cloned() else {
                if let Some((enum_name, variant)) = name.rsplit_once("::") {
                    return Ok(Value::EnumVariant(
                        enum_name.to_string(),
                        variant.to_string(),
                        args,
                    ));
                }
                return Err(KainError::runtime(format!("Function not found: {}", name)));
            };
            if f.params.len() != args.len() {
                return Err(KainError::runtime(format!(
                    "Argument mismatch: expected {}, got {}",
                    f.params.len(),
                    args.len()
                )));
            }

            let inline_scope = env.function_inline_scopes.get(&name).cloned();
            if let Some(bindings) = &inline_scope {
                env.push_scope();
                for (binding_name, binding_value) in bindings {
                    env.define(binding_name.clone(), binding_value.clone());
                }
            }
            env.push_scope();
            for (param, arg) in f.params.iter().zip(args.into_iter()) {
                env.define(param.name.clone(), arg);
            }

            let result = eval_block(env, &f.body)?;
            env.pop_scope();
            if inline_scope.is_some() {
                env.pop_scope();
            }

            match result {
                Value::Return(v) => Ok(*v),
                v => Ok(v),
            }
        }
        Value::Patch(name) => execute_patch_call(env, &name, args),
        Value::Law(name) => execute_law_call(env, &name, args),
        Value::Converge(name) => execute_converge_call(env, &name, args),
        Value::Orchestrate(name) => execute_orchestrate_call(env, &name, args),
        Value::NativeFn(_, f) => f(env, args),
        Value::Closure(params, body, captured) => {
            if params.len() != args.len() {
                return Err(KainError::runtime(format!("Closure arg mismatch")));
            }

            // Restore captured scope + new scope
            let old_scopes = env.scopes.clone();
            env.scopes = captured;
            env.push_scope();

            for (name, arg) in params.iter().zip(args.into_iter()) {
                env.define(name.clone(), arg);
            }

            let result = eval_expr(env, &body)?;

            env.pop_scope();
            env.scopes = old_scopes;

            match result {
                Value::Return(v) => Ok(*v),
                v => Ok(v),
            }
        }
        Value::StructConstructor(name, fields) => {
            if fields.len() != args.len() {
                return Err(KainError::runtime(format!(
                    "Struct constructor for {} expected {} arguments, got {}",
                    name,
                    fields.len(),
                    args.len()
                )));
            }

            let mut field_vals = HashMap::new();
            for (i, val) in args.into_iter().enumerate() {
                field_vals.insert(fields[i].clone(), val);
            }

            Ok(Value::Struct(name, Arc::new(RwLock::new(field_vals))))
        }
        _ => Err(KainError::runtime("Not a function")),
    }
}

fn execute_patch_call(env: &mut Env, name: &str, args: Vec<Value>) -> KainResult<Value> {
    let patch = env
        .patches
        .get(name)
        .cloned()
        .ok_or_else(|| KainError::runtime(format!("Patch not found: {}", name)))?;
    if patch.params.len() != args.len() {
        return Err(KainError::runtime(format!(
            "Patch {} expected {} arguments, got {}",
            name,
            patch.params.len(),
            args.len()
        )));
    }

    env.begin_active_patch(name);
    env.push_scope();
    for (param, arg) in patch.params.iter().zip(args.into_iter()) {
        env.define(param.name.clone(), arg);
    }
    let result = eval_block(env, &patch.body);
    env.pop_scope();
    let result = match result {
        Ok(result) => result,
        Err(error) => {
            env.cancel_active_patch()?;
            return Err(error);
        }
    };
    env.finish_active_patch();

    match result {
        Value::Return(v) => Ok(*v),
        value => Ok(value),
    }
}

fn execute_law_call(env: &mut Env, name: &str, args: Vec<Value>) -> KainResult<Value> {
    let law = env
        .laws
        .get(name)
        .cloned()
        .ok_or_else(|| KainError::runtime(format!("Law not found: {}", name)))?;
    if law.params.len() != args.len() {
        return Err(KainError::runtime(format!(
            "Law {} expected {} arguments, got {}",
            name,
            law.params.len(),
            args.len()
        )));
    }
    let result = execute_function_body(env, &law.params, &law.body, args)?;
    match result {
        Value::Bool(_) => Ok(result),
        other => Err(KainError::runtime(format!(
            "Law {} must return Bool at runtime, found {:?}",
            name, other
        ))),
    }
}

fn execute_converge_call(env: &mut Env, name: &str, args: Vec<Value>) -> KainResult<Value> {
    let converge =
        env.converges.get(name).cloned().ok_or_else(|| {
            KainError::runtime(format!("Converge definition not found: {}", name))
        })?;
    if converge.params.len() != args.len() {
        return Err(KainError::runtime(format!(
            "Converge {} expected {} arguments, got {}",
            name,
            converge.params.len(),
            args.len()
        )));
    }

    let selected_lane = select_converge_lane(env, &converge);
    let selected_result =
        execute_function_body(env, &converge.params, &selected_lane.body, args.clone())?;
    if let Some(sample_count) = converge.verify_random_count {
        verify_converge_selected_against_spec(
            env,
            &converge,
            selected_lane,
            &args,
            &selected_result,
            "call",
        )?;
        for sample_index in 0..sample_count {
            let sample_args = synthesize_converge_sample_args(&converge, sample_index)?;
            let sample_result = execute_function_body(
                env,
                &converge.params,
                &selected_lane.body,
                sample_args.clone(),
            )?;
            verify_converge_selected_against_spec(
                env,
                &converge,
                selected_lane,
                &sample_args,
                &sample_result,
                &format!("sample {}", sample_index + 1),
            )?;
        }
    }

    Ok(selected_result)
}

fn execute_orchestrate_call(env: &mut Env, name: &str, args: Vec<Value>) -> KainResult<Value> {
    let orchestrate =
        env.orchestrates.get(name).cloned().ok_or_else(|| {
            KainError::runtime(format!("Orchestrate definition not found: {}", name))
        })?;
    if orchestrate.params.len() != args.len() {
        return Err(KainError::runtime(format!(
            "Orchestrate {} expected {} arguments, got {}",
            name,
            orchestrate.params.len(),
            args.len()
        )));
    }
    execute_function_body(env, &orchestrate.params, &orchestrate.body, args)
}

fn execute_stage_call(
    env: &mut Env,
    runtime: OrchestrateStageRuntime,
    function: &str,
    args: Vec<Value>,
) -> KainResult<Value> {
    match runtime {
        OrchestrateStageRuntime::Kain => env.call_named_function(function, args),
        OrchestrateStageRuntime::Rust => execute_rust_stage_call(env, function, args),
        OrchestrateStageRuntime::Python => execute_python_stage_call(env, function, args),
        OrchestrateStageRuntime::Node => execute_node_stage_call(env, function, args),
    }
}

fn execute_rust_stage_call(env: &mut Env, function: &str, args: Vec<Value>) -> KainResult<Value> {
    match env.lookup_value(function) {
        Some(Value::NativeFn(_, native)) => native(env, args),
        Some(other) => Err(KainError::runtime(format!(
            "Rust orchestrate stage '{}' must resolve to a native function, found {}",
            function,
            runtime_value_kind(&other)
        ))),
        None => Err(KainError::runtime(format!(
            "Rust orchestrate stage '{}' was not registered as a native function",
            function
        ))),
    }
}

fn execute_python_stage_call(env: &mut Env, function: &str, args: Vec<Value>) -> KainResult<Value> {
    if env.lookup_value("py_call").is_none() {
        return Err(KainError::runtime(
            "Python orchestrate stage requested but python bridge is not registered",
        ));
    }

    let args_value = runtime_array_value(args);
    if let Some((module_name, attr_name)) = function.rsplit_once("::") {
        let module = env.call_named_function(
            "py_import",
            vec![Value::String(module_name.replace("::", "."))],
        )?;
        env.call_named_function(
            "py_call",
            vec![module, Value::String(attr_name.to_string()), args_value],
        )
    } else {
        env.call_named_function(
            "py_call",
            vec![Value::String(function.to_string()), args_value],
        )
    }
}

fn execute_node_stage_call(env: &mut Env, function: &str, args: Vec<Value>) -> KainResult<Value> {
    if env.lookup_value("js_call").is_none() {
        return Err(KainError::runtime(
            "Node orchestrate stage requested but node bridge is not registered",
        ));
    }

    let args_value = runtime_array_value(args);
    if let Some((module_name, attr_name)) = function.rsplit_once("::") {
        let module_import_builtin = if env.lookup_value("js_import_raw").is_some() {
            "js_import_raw"
        } else {
            "js_import"
        };
        let module = env.call_named_function(
            module_import_builtin,
            vec![Value::String(module_name.to_string())],
        )?;
        env.call_named_function(
            "js_call_method",
            vec![module, Value::String(attr_name.to_string()), args_value],
        )
    } else {
        env.call_named_function(
            "js_call",
            vec![Value::String(function.to_string()), args_value],
        )
    }
}

fn execute_function_body(
    env: &mut Env,
    params: &[Param],
    body: &Block,
    args: Vec<Value>,
) -> KainResult<Value> {
    env.push_scope();
    for (param, arg) in params.iter().zip(args.into_iter()) {
        env.define(param.name.clone(), arg);
    }
    let result = eval_block(env, body)?;
    env.pop_scope();
    match result {
        Value::Return(v) => Ok(*v),
        value => Ok(value),
    }
}

fn select_converge_lane<'a>(env: &Env, converge: &'a ConvergeDef) -> &'a ConvergeLane {
    converge
        .fast_lanes
        .iter()
        .find(|lane| match lane.selector.as_ref() {
            Some(selector) => converge_selector_matches(env, selector),
            None => true,
        })
        .unwrap_or(&converge.spec_lane)
}

fn verify_converge_selected_against_spec(
    env: &mut Env,
    converge: &ConvergeDef,
    selected_lane: &ConvergeLane,
    args: &[Value],
    selected_result: &Value,
    verification_label: &str,
) -> KainResult<()> {
    let spec_result = execute_function_body(
        env,
        &converge.params,
        &converge.spec_lane.body,
        args.to_vec(),
    )?;
    if runtime_values_semantically_equal(selected_result, &spec_result) {
        return Ok(());
    }
    Err(KainError::runtime(format!(
        "Converge verification failed for {} during {}: selected lane '{}' diverged from spec '{}'",
        converge.name, verification_label, selected_lane.lane_name, converge.spec_lane.lane_name
    )))
}

fn synthesize_converge_sample_args(
    converge: &ConvergeDef,
    sample_index: u32,
) -> KainResult<Vec<Value>> {
    let mut synthesizer = DeterministicValueSynthesizer::new(stable_converge_sample_seed(
        &converge.name,
        sample_index,
    ));
    converge
        .params
        .iter()
        .map(|param| synthesize_value_for_type(&param.ty, &mut synthesizer))
        .collect()
}

fn stable_converge_sample_seed(name: &str, sample_index: u32) -> u64 {
    let mut hash = 0xcbf29ce484222325u64;
    for byte in name.as_bytes() {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash ^ u64::from(sample_index).wrapping_mul(0x9e3779b97f4a7c15)
}

struct DeterministicValueSynthesizer {
    state: u64,
}

impl DeterministicValueSynthesizer {
    fn new(seed: u64) -> Self {
        Self { state: seed.max(1) }
    }

    fn next_u64(&mut self) -> u64 {
        self.state ^= self.state << 13;
        self.state ^= self.state >> 7;
        self.state ^= self.state << 17;
        self.state
    }

    fn next_bool(&mut self) -> bool {
        self.next_u64() & 1 == 0
    }
}

fn synthesize_value_for_type(
    ty: &Type,
    synthesizer: &mut DeterministicValueSynthesizer,
) -> KainResult<Value> {
    match ty {
        Type::Named { name, generics, .. } => match name.as_str() {
            "Bool" => Ok(Value::Bool(synthesizer.next_bool())),
            "Int" | "i8" | "i16" | "i32" | "i64" | "i128" | "isize" => {
                Ok(Value::Int(((synthesizer.next_u64() % 2001) as i64) - 1000))
            }
            "UInt" | "u8" | "u16" | "u32" | "u64" | "u128" | "usize" => {
                Ok(Value::Int((synthesizer.next_u64() % 2001) as i64))
            }
            "Float" | "f32" | "f64" => Ok(Value::Float(
                ((synthesizer.next_u64() % 20001) as f64 / 100.0) - 100.0,
            )),
            "Char" => {
                let offset = (synthesizer.next_u64() % 95) as u32;
                Ok(Value::String(
                    char::from_u32(32 + offset).unwrap().to_string(),
                ))
            }
            "Array" if generics.len() == 1 => {
                let len = ((synthesizer.next_u64() % 3) + 1) as usize;
                let mut values = Vec::with_capacity(len);
                for _ in 0..len {
                    values.push(synthesize_value_for_type(&generics[0], synthesizer)?);
                }
                Ok(runtime_array_value(values))
            }
            "Option" if generics.len() == 1 => {
                if synthesizer.next_bool() {
                    Ok(Value::None)
                } else {
                    synthesize_value_for_type(&generics[0], synthesizer)
                }
            }
            other => Err(KainError::runtime(format!(
                "verify random(n) does not support synthesized values for type {}",
                other
            ))),
        },
        Type::Tuple(items, _) => {
            let mut values = Vec::with_capacity(items.len());
            for item in items {
                values.push(synthesize_value_for_type(item, synthesizer)?);
            }
            Ok(Value::Tuple(values))
        }
        Type::Array(inner, len, _) => {
            let mut values = Vec::with_capacity(*len);
            for _ in 0..*len {
                values.push(synthesize_value_for_type(inner, synthesizer)?);
            }
            Ok(runtime_array_value(values))
        }
        Type::Option(inner, _) => {
            if synthesizer.next_bool() {
                Ok(Value::None)
            } else {
                synthesize_value_for_type(inner, synthesizer)
            }
        }
        other => Err(KainError::runtime(format!(
            "verify random(n) does not support synthesized values for type {:?}",
            other
        ))),
    }
}

fn runtime_value_kind(value: &Value) -> &'static str {
    match value {
        Value::Unit => "unit",
        Value::Bool(_) => "bool",
        Value::Int(_) => "int",
        Value::Float(_) => "float",
        Value::String(_) => "string",
        Value::Array(_) => "array",
        Value::Tuple(_) => "tuple",
        Value::Struct(_, _) => "struct",
        Value::HostObject(_, _) => "host_object",
        Value::Function(_) => "function",
        Value::Patch(_) => "patch",
        Value::Law(_) => "law",
        Value::Converge(_) => "converge",
        Value::Orchestrate(_) => "orchestrate",
        Value::NativeFn(_, _) => "native_function",
        Value::ActorRef(_) => "actor",
        Value::None => "none",
        Value::Return(_) => "return",
        Value::Break(_) => "break",
        Value::Continue => "continue",
        Value::Result(_, _) => "result",
        Value::Closure(_, _, _) => "closure",
        Value::StructConstructor(_, _) => "struct_constructor",
        Value::JSX(_) => "jsx",
        Value::EnumVariant(_, _, _) => "enum_variant",
        Value::Poll(_, _) => "poll",
        Value::Future(_, _) => "future",
    }
}

fn expect_single_int_arg<'a>(args: &'a [Value], method: &str) -> KainResult<&'a i64> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!(
            "{} expects exactly one argument",
            method
        )));
    }
    match &args[0] {
        Value::Int(value) => Ok(value),
        other => Err(KainError::runtime(format!(
            "{} expects an Int argument, found {}",
            method,
            runtime_value_kind(other)
        ))),
    }
}

fn expect_single_float_arg<'a>(args: &'a [Value], method: &str) -> KainResult<&'a f64> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!(
            "{} expects exactly one argument",
            method
        )));
    }
    match &args[0] {
        Value::Float(value) => Ok(value),
        other => Err(KainError::runtime(format!(
            "{} expects a Float argument, found {}",
            method,
            runtime_value_kind(other)
        ))),
    }
}

fn expect_single_string_arg<'a>(args: &'a [Value], method: &str) -> KainResult<&'a str> {
    if args.len() != 1 {
        return Err(KainError::runtime(format!(
            "{} expects exactly one argument",
            method
        )));
    }
    match &args[0] {
        Value::String(value) => Ok(value),
        other => Err(KainError::runtime(format!(
            "{} expects a String argument, found {}",
            method,
            runtime_value_kind(other)
        ))),
    }
}

fn eval_i64_binary_method(
    receiver: i64,
    args: &[Value],
    method: &str,
    op: fn(i64, i64) -> i64,
) -> KainResult<Value> {
    let rhs = *expect_single_int_arg(args, method)?;
    Ok(Value::Int(op(receiver, rhs)))
}

fn eval_i64_div_ceil_method(receiver: i64, args: &[Value], method: &str) -> KainResult<Value> {
    let rhs = *expect_single_int_arg(args, method)?;
    if rhs == 0 {
        return Err(KainError::runtime(format!(
            "{} does not allow division by zero",
            method
        )));
    }
    let quotient = receiver / rhs;
    let remainder = receiver % rhs;
    let needs_round_up = remainder != 0 && ((remainder > 0) == (rhs > 0));
    Ok(Value::Int(if needs_round_up {
        quotient + 1
    } else {
        quotient
    }))
}

fn eval_i64_shift_method(
    receiver: i64,
    args: &[Value],
    method: &str,
    op: fn(i64, u32) -> i64,
) -> KainResult<Value> {
    let rhs = *expect_single_int_arg(args, method)?;
    let shift = u32::try_from(rhs).map_err(|_| {
        KainError::runtime(format!(
            "{} expects a non-negative Int shift count, found {}",
            method, rhs
        ))
    })?;
    Ok(Value::Int(op(receiver, shift)))
}

fn eval_f64_binary_method(
    receiver: f64,
    args: &[Value],
    method: &str,
    op: fn(f64, f64) -> f64,
) -> KainResult<Value> {
    let rhs = *expect_single_float_arg(args, method)?;
    Ok(Value::Float(op(receiver, rhs)))
}

fn eval_array_binary_search(values: &[Value], needle: &Value) -> KainResult<Value> {
    let result = match needle {
        Value::Int(target) => values.binary_search_by(|value| match value {
            Value::Int(candidate) => candidate.cmp(target),
            _ => std::cmp::Ordering::Less,
        }),
        Value::String(target) => values.binary_search_by(|value| match value {
            Value::String(candidate) => candidate.cmp(target),
            _ => std::cmp::Ordering::Less,
        }),
        Value::Bool(target) => values.binary_search_by(|value| match value {
            Value::Bool(candidate) => candidate.cmp(target),
            _ => std::cmp::Ordering::Less,
        }),
        other => {
            return Err(KainError::runtime(format!(
                "binary_search does not support {} needles",
                runtime_value_kind(other)
            )))
        }
    };
    match result {
        Ok(index) => Ok(Value::Result(true, Box::new(Value::Int(index as i64)))),
        Err(index) => Ok(Value::Result(false, Box::new(Value::Int(index as i64)))),
    }
}

fn converge_selector_matches(env: &Env, selector: &ConvergeSelector) -> bool {
    match selector {
        ConvergeSelector::Target(target) => match env.execution_lane {
            ExecutionLane::Interpret => target == "interpret",
            ExecutionLane::Test => target == "test",
        },
        ConvergeSelector::Capability(capability) => env
            .active_capabilities
            .iter()
            .any(|entry| entry == capability),
    }
}

fn runtime_values_semantically_equal(left: &Value, right: &Value) -> bool {
    match (left, right) {
        (Value::Unit, Value::Unit)
        | (Value::None, Value::None)
        | (Value::Continue, Value::Continue) => true,
        (Value::Bool(left), Value::Bool(right)) => left == right,
        (Value::Int(left), Value::Int(right)) => left == right,
        (Value::Float(left), Value::Float(right)) => (left - right).abs() <= 1e-6,
        (Value::String(left), Value::String(right)) => left == right,
        (Value::Tuple(left), Value::Tuple(right)) => {
            left.len() == right.len()
                && left
                    .iter()
                    .zip(right.iter())
                    .all(|(left, right)| runtime_values_semantically_equal(left, right))
        }
        (Value::Array(left), Value::Array(right)) => {
            let left = left.read().unwrap();
            let right = right.read().unwrap();
            left.len() == right.len()
                && left
                    .iter()
                    .zip(right.iter())
                    .all(|(left, right)| runtime_values_semantically_equal(left, right))
        }
        (Value::Struct(left_name, left_fields), Value::Struct(right_name, right_fields)) => {
            if left_name != right_name {
                return false;
            }
            let left_fields = left_fields.read().unwrap();
            let right_fields = right_fields.read().unwrap();
            left_fields.len() == right_fields.len()
                && left_fields.iter().all(|(key, left_value)| {
                    right_fields.get(key).is_some_and(|right_value| {
                        runtime_values_semantically_equal(left_value, right_value)
                    })
                })
        }
        (
            Value::EnumVariant(left_enum, left_variant, left_fields),
            Value::EnumVariant(right_enum, right_variant, right_fields),
        ) => {
            left_enum == right_enum
                && left_variant == right_variant
                && left_fields.len() == right_fields.len()
                && left_fields
                    .iter()
                    .zip(right_fields.iter())
                    .all(|(left, right)| runtime_values_semantically_equal(left, right))
        }
        (Value::Return(left), Value::Return(right))
        | (Value::Break(Some(left)), Value::Break(Some(right))) => {
            runtime_values_semantically_equal(left, right)
        }
        (Value::Break(None), Value::Break(None)) => true,
        (Value::Result(left_ok, left_value), Value::Result(right_ok, right_value)) => {
            left_ok == right_ok && runtime_values_semantically_equal(left_value, right_value)
        }
        (Value::Poll(left_ready, left_value), Value::Poll(right_ready, right_value)) => {
            left_ready == right_ready
                && match (left_value, right_value) {
                    (Some(left), Some(right)) => runtime_values_semantically_equal(left, right),
                    (None, None) => true,
                    _ => false,
                }
        }
        _ => false,
    }
}

fn infer_patch_undo_mode(patch: &PatchDef) -> String {
    if block_contains_runtime_best_effort_effects(&patch.body) {
        "best_effort".to_string()
    } else {
        "reversible".to_string()
    }
}

fn block_contains_runtime_best_effort_effects(block: &Block) -> bool {
    block
        .stmts
        .iter()
        .any(stmt_contains_runtime_best_effort_effects)
}

fn stmt_contains_runtime_best_effort_effects(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Expr(expr) => expr_contains_runtime_best_effort_effects(expr),
        Stmt::Let { value, .. } => value
            .as_ref()
            .is_some_and(|value| expr_contains_runtime_best_effort_effects(value)),
        Stmt::Return(value, _) | Stmt::Break(value, _) => value
            .as_ref()
            .is_some_and(|value| expr_contains_runtime_best_effort_effects(value)),
        Stmt::For { iter, body, .. } => {
            expr_contains_runtime_best_effort_effects(iter)
                || block_contains_runtime_best_effort_effects(body)
        }
        Stmt::While {
            condition, body, ..
        } => {
            expr_contains_runtime_best_effort_effects(condition)
                || block_contains_runtime_best_effort_effects(body)
        }
        Stmt::Loop { body, .. } => block_contains_runtime_best_effort_effects(body),
        Stmt::Item(_) | Stmt::Continue(_) => false,
    }
}

fn expr_contains_runtime_best_effort_effects(expr: &Expr) -> bool {
    match expr {
        Expr::Call { .. }
        | Expr::MethodCall { .. }
        | Expr::StageCall { .. }
        | Expr::Spawn { .. }
        | Expr::SendMsg { .. }
        | Expr::Await(_, _)
        | Expr::AsyncBlock(_, _) => true,
        Expr::Assign { target, value, .. } => {
            expr_contains_runtime_best_effort_effects(target)
                || expr_contains_runtime_best_effort_effects(value)
        }
        Expr::Binary { left, right, .. } => {
            expr_contains_runtime_best_effort_effects(left)
                || expr_contains_runtime_best_effort_effects(right)
        }
        Expr::Unary { operand, .. }
        | Expr::Try(operand, _)
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Paren(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Cast { value: operand, .. } => expr_contains_runtime_best_effort_effects(operand),
        Expr::Field { object, .. } => expr_contains_runtime_best_effort_effects(object),
        Expr::Index { object, index, .. } => {
            expr_contains_runtime_best_effort_effects(object)
                || expr_contains_runtime_best_effort_effects(index)
        }
        Expr::Struct { fields, rest, .. } => {
            fields
                .iter()
                .any(|(_, value)| expr_contains_runtime_best_effort_effects(value))
                || rest
                    .as_ref()
                    .is_some_and(|rest| expr_contains_runtime_best_effort_effects(rest))
        }
        Expr::AggregateInit { fields, .. } => fields
            .iter()
            .any(|(_, value)| expr_contains_runtime_best_effort_effects(value)),
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Tuple(values) => {
                values.iter().any(expr_contains_runtime_best_effort_effects)
            }
            EnumVariantFields::Struct(values) => values
                .iter()
                .any(|(_, value)| expr_contains_runtime_best_effort_effects(value)),
            EnumVariantFields::Unit => false,
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
            values.iter().any(expr_contains_runtime_best_effort_effects)
        }
        Expr::Range { start, end, .. } => {
            start
                .as_ref()
                .is_some_and(|value| expr_contains_runtime_best_effort_effects(value))
                || end
                    .as_ref()
                    .is_some_and(|value| expr_contains_runtime_best_effort_effects(value))
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            expr_contains_runtime_best_effort_effects(condition)
                || block_contains_runtime_best_effort_effects(then_branch)
                || else_branch
                    .as_ref()
                    .is_some_and(|branch| match branch.as_ref() {
                        ElseBranch::Else(block) => {
                            block_contains_runtime_best_effort_effects(block)
                        }
                        ElseBranch::ElseIf(condition, block, next) => {
                            expr_contains_runtime_best_effort_effects(condition)
                                || block_contains_runtime_best_effort_effects(block)
                                || next.as_ref().is_some_and(|next| match next.as_ref() {
                                    ElseBranch::Else(block) => {
                                        block_contains_runtime_best_effort_effects(block)
                                    }
                                    ElseBranch::ElseIf(..) => true,
                                })
                        }
                    })
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            expr_contains_runtime_best_effort_effects(scrutinee)
                || arms.iter().any(|arm| {
                    arm.guard
                        .as_ref()
                        .is_some_and(|guard| expr_contains_runtime_best_effort_effects(guard))
                        || expr_contains_runtime_best_effort_effects(&arm.body)
                })
        }
        Expr::Lambda { body, .. } => expr_contains_runtime_best_effort_effects(body),
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            expr_contains_runtime_best_effort_effects(pointer)
                || expr_contains_runtime_best_effort_effects(offset)
        }
        Expr::MemLoad { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        } => expr_contains_runtime_best_effort_effects(pointer),
        Expr::MemStore { pointer, value, .. } => {
            expr_contains_runtime_best_effort_effects(pointer)
                || expr_contains_runtime_best_effort_effects(value)
        }
        Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
            expr_contains_runtime_best_effort_effects(target)
                || expr_contains_runtime_best_effort_effects(body)
        }
        Expr::Return(value, _) | Expr::Break(value, _) => value
            .as_ref()
            .is_some_and(|value| expr_contains_runtime_best_effort_effects(value)),
        Expr::Block(block, _) => block_contains_runtime_best_effort_effects(block),
        Expr::JSX(_, _)
        | Expr::MacroCall { .. }
        | Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
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

fn runtime_patch_target_path(target: &Expr) -> Option<String> {
    match target {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, field, .. } => {
            runtime_patch_target_path(object).map(|base| format!("{base}.{field}"))
        }
        Expr::Index { object, index, .. } => runtime_patch_target_path(object).map(|base| {
            let suffix = match index.as_ref() {
                Expr::Int(value, _) => format!("[{value}]"),
                _ => "[]".to_string(),
            };
            format!("{base}{suffix}")
        }),
        Expr::Deref(inner, _) => runtime_patch_target_path(inner),
        _ => None,
    }
}

fn apply_patch_change_value(change: &ActivePatchChange, use_new_value: bool) -> KainResult<()> {
    let value = if use_new_value {
        change.new_value.clone()
    } else {
        change.old_value.clone()
    };
    match &change.target {
        PatchMutationTarget::StructField { fields, field } => {
            fields.write().unwrap().insert(field.clone(), value);
            Ok(())
        }
        PatchMutationTarget::ArrayIndex { values, index } => {
            let mut items = values.write().unwrap();
            if *index >= items.len() {
                return Err(KainError::runtime(format!(
                    "Patch replay target '{}' no longer exists at index {}",
                    change.path, index
                )));
            }
            items[*index] = value;
            Ok(())
        }
    }
}

fn runtime_array_value(values: Vec<Value>) -> Value {
    Value::Array(Arc::new(RwLock::new(values)))
}

fn runtime_string_array_value(values: &[String]) -> Value {
    runtime_array_value(values.iter().cloned().map(Value::String).collect())
}

fn runtime_struct_value(name: &str, fields: Vec<(String, Value)>) -> Value {
    let mut values = HashMap::new();
    for (field, value) in fields {
        values.insert(field, value);
    }
    Value::Struct(name.to_string(), Arc::new(RwLock::new(values)))
}

fn runtime_expect_string_arg<'a>(
    args: &'a [Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<&'a str> {
    match args.get(index) {
        Some(Value::String(value)) => Ok(value),
        Some(_) => Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be string"
        ))),
        None => Err(KainError::runtime(format!(
            "{function_name}: missing argument {argument_name}"
        ))),
    }
}

fn runtime_expect_int_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<i64> {
    match args.get(index) {
        Some(Value::Int(value)) => Ok(*value),
        Some(_) => Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be int"
        ))),
        None => Err(KainError::runtime(format!(
            "{function_name}: missing argument {argument_name}"
        ))),
    }
}

fn runtime_expect_number_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<f64> {
    match args.get(index) {
        Some(Value::Int(value)) => Ok(*value as f64),
        Some(Value::Float(value)) => Ok(*value),
        Some(_) => Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be number"
        ))),
        None => Err(KainError::runtime(format!(
            "{function_name}: missing argument {argument_name}"
        ))),
    }
}

fn runtime_expect_non_negative_int_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<i64> {
    let value = runtime_expect_int_arg(args, index, function_name, argument_name)?;
    if value < 0 {
        return Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be non-negative"
        )));
    }
    Ok(value)
}

fn runtime_input_kind(kind: &str) -> kain_input::InputEventKind {
    match kind {
        "key_down" => kain_input::InputEventKind::KeyDown,
        "key_up" => kain_input::InputEventKind::KeyUp,
        "text" => kain_input::InputEventKind::Text,
        "pointer_down" => kain_input::InputEventKind::PointerDown,
        "pointer_up" => kain_input::InputEventKind::PointerUp,
        "pointer_move" => kain_input::InputEventKind::PointerMove,
        "axis" => kain_input::InputEventKind::Axis,
        "action" => kain_input::InputEventKind::Action,
        "action_down" => kain_input::InputEventKind::ActionDown,
        "action_up" => kain_input::InputEventKind::ActionUp,
        _ => kain_input::InputEventKind::Lifecycle,
    }
}

fn runtime_input_with_session<F>(session_id: i64, function_name: &str, op: F) -> KainResult<Value>
where
    F: FnOnce(&InputSession) -> KainResult<Value>,
{
    let sessions = RUNTIME_INPUT_SESSIONS.read().unwrap();
    let Some(session) = sessions.get(&session_id) else {
        return Err(KainError::runtime(format!(
            "{function_name}: invalid input session {session_id}"
        )));
    };
    op(session)
}

fn runtime_input_with_session_mut<F>(
    session_id: i64,
    function_name: &str,
    op: F,
) -> KainResult<Value>
where
    F: FnOnce(&mut InputSession) -> KainResult<Value>,
{
    let mut sessions = RUNTIME_INPUT_SESSIONS.write().unwrap();
    let Some(session) = sessions.get_mut(&session_id) else {
        return Err(KainError::runtime(format!(
            "{function_name}: invalid input session {session_id}"
        )));
    };
    op(session)
}

fn runtime_input_frame_event_string<F>(
    args: &[Value],
    function_name: &str,
    select: F,
) -> KainResult<Value>
where
    F: for<'a> Fn(&'a InputEvent) -> &'a str,
{
    let session_id = runtime_expect_int_arg(args, 0, function_name, "session_id")?;
    let index = runtime_expect_non_negative_int_arg(args, 1, function_name, "index")? as usize;
    runtime_input_with_session(session_id, function_name, |session| {
        Ok(Value::String(
            session
                .current_frame()
                .events
                .get(index)
                .map(&select)
                .unwrap_or_default()
                .to_string(),
        ))
    })
}

fn runtime_input_action_query<F>(args: &[Value], function_name: &str, query: F) -> KainResult<Value>
where
    F: Fn(&kain_input::InputFrame, &str) -> bool,
{
    let session_id = runtime_expect_int_arg(args, 0, function_name, "session_id")?;
    let action = runtime_expect_string_arg(args, 1, function_name, "action")?;
    runtime_input_with_session(session_id, function_name, |session| {
        Ok(Value::Int(if query(session.current_frame(), action) {
            1
        } else {
            0
        }))
    })
}

fn runtime_expect_bool_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<bool> {
    match args.get(index) {
        Some(Value::Bool(value)) => Ok(*value),
        Some(_) => Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be bool"
        ))),
        None => Err(KainError::runtime(format!(
            "{function_name}: missing argument {argument_name}"
        ))),
    }
}

fn runtime_expect_capability_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
) -> KainResult<FsCapability> {
    let capability = runtime_expect_string_arg(args, index, function_name, "capability")?;
    FsCapability::from_str(capability).ok_or_else(|| {
        KainError::runtime(format!(
            "{function_name}: unknown filesystem capability '{capability}'"
        ))
    })
}

fn runtime_expect_scoped_path(
    args: &[Value],
    index: usize,
    function_name: &str,
    capability: FsCapability,
) -> KainResult<String> {
    let path = runtime_expect_string_arg(args, index, function_name, "path")?;
    RUNTIME_FS_SANDBOX
        .read()
        .unwrap()
        .authorize(capability, path)
        .map(|path| path.to_string_lossy().into_owned())
        .map_err(|error| KainError::runtime(format!("{function_name}: {error}")))
}

fn runtime_expect_byte_array_arg(
    args: &[Value],
    index: usize,
    function_name: &str,
    argument_name: &str,
) -> KainResult<Vec<u8>> {
    let Some(Value::Array(values)) = args.get(index) else {
        return Err(KainError::runtime(format!(
            "{function_name}: {argument_name} must be Array<Int>"
        )));
    };
    values
        .read()
        .unwrap()
        .iter()
        .enumerate()
        .map(|(byte_index, value)| match value {
            Value::Int(value) if (0..=255).contains(value) => Ok(*value as u8),
            Value::Int(_) => Err(KainError::runtime(format!(
                "{function_name}: byte at index {byte_index} is outside 0..255"
            ))),
            _ => Err(KainError::runtime(format!(
                "{function_name}: byte at index {byte_index} must be Int"
            ))),
        })
        .collect()
}

fn runtime_byte_array_value(bytes: &[u8]) -> Value {
    runtime_array_value(
        bytes
            .iter()
            .map(|byte| Value::Int(i64::from(*byte)))
            .collect(),
    )
}

fn runtime_hex_encode(bytes: &[u8]) -> String {
    const DIGITS: &[u8; 16] = b"0123456789abcdef";
    let mut output = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        output.push(DIGITS[(byte >> 4) as usize] as char);
        output.push(DIGITS[(byte & 0x0f) as usize] as char);
    }
    output
}

fn runtime_hex_decode(hex: &str) -> KainResult<Vec<u8>> {
    if hex.len() % 2 != 0 {
        return Err(KainError::runtime(
            "fs_write_bytes_hex: hex string must have an even length",
        ));
    }
    let mut bytes = Vec::with_capacity(hex.len() / 2);
    let chars = hex.as_bytes();
    let mut index = 0;
    while index < chars.len() {
        let hi = runtime_hex_digit(chars[index])?;
        let lo = runtime_hex_digit(chars[index + 1])?;
        bytes.push((hi << 4) | lo);
        index += 2;
    }
    Ok(bytes)
}

fn runtime_hex_digit(byte: u8) -> KainResult<u8> {
    match byte {
        b'0'..=b'9' => Ok(byte - b'0'),
        b'a'..=b'f' => Ok(byte - b'a' + 10),
        b'A'..=b'F' => Ok(byte - b'A' + 10),
        _ => Err(KainError::runtime(
            "fs_write_bytes_hex: hex string contains a non-hex character",
        )),
    }
}

fn runtime_join_lines(lines: &[String]) -> String {
    if lines.is_empty() {
        String::new()
    } else {
        format!("{}\n", lines.join("\n"))
    }
}

fn runtime_fs_strict(function_name: &str, result: Result<Value, FsError>) -> KainResult<Value> {
    result.map_err(|error| KainError::runtime(format!("{function_name}: {error}")))
}

fn runtime_fs_strict_unit(function_name: &str, result: Result<(), FsError>) -> KainResult<Value> {
    runtime_fs_strict(function_name, result.map(|_| Value::Unit))
}

fn runtime_fs_result(result: Result<Value, FsError>) -> KainResult<Value> {
    Ok(match result {
        Ok(value) => Value::Result(true, Box::new(value)),
        Err(error) => Value::Result(false, Box::new(runtime_fs_error_value(&error))),
    })
}

fn runtime_fs_error_value(error: &FsError) -> Value {
    runtime_struct_value(
        "FsError",
        vec![
            (
                "kind".to_string(),
                Value::String(error.kind.as_str().to_string()),
            ),
            (
                "operation".to_string(),
                Value::String(error.operation.clone()),
            ),
            (
                "path".to_string(),
                Value::String(error.path.to_string_lossy().into_owned()),
            ),
            (
                "other_path".to_string(),
                Value::String(
                    error
                        .other_path
                        .as_ref()
                        .map(|path| path.to_string_lossy().into_owned())
                        .unwrap_or_default(),
                ),
            ),
            ("message".to_string(), Value::String(error.message.clone())),
            (
                "raw_code".to_string(),
                Value::Int(error.raw_code.map(i64::from).unwrap_or(-1)),
            ),
        ],
    )
}

fn runtime_fs_metadata_value(metadata: FsMetadata) -> Value {
    runtime_struct_value(
        "FsMetadata",
        vec![
            (
                "file_type".to_string(),
                Value::String(metadata.file_type.as_str().to_string()),
            ),
            (
                "len".to_string(),
                Value::Int(metadata.len.min(i64::MAX as u64) as i64),
            ),
            ("readonly".to_string(), Value::Bool(metadata.readonly)),
            (
                "created_millis".to_string(),
                runtime_optional_millis_value(metadata.created_millis),
            ),
            (
                "modified_millis".to_string(),
                runtime_optional_millis_value(metadata.modified_millis),
            ),
            (
                "accessed_millis".to_string(),
                runtime_optional_millis_value(metadata.accessed_millis),
            ),
        ],
    )
}

fn runtime_fs_metadata_text(metadata: FsMetadata) -> String {
    format!(
        "file_type={}\nlen={}\nreadonly={}\ncreated_millis={}\nmodified_millis={}\naccessed_millis={}\n",
        metadata.file_type.as_str(),
        metadata.len,
        if metadata.readonly { 1 } else { 0 },
        metadata.created_millis.unwrap_or(0),
        metadata.modified_millis.unwrap_or(0),
        metadata.accessed_millis.unwrap_or(0)
    )
}

fn runtime_fs_dir_entry_value(entry: DirectoryEntry) -> Value {
    runtime_struct_value(
        "FsDirEntry",
        vec![
            (
                "path".to_string(),
                Value::String(entry.path.to_string_lossy().into_owned()),
            ),
            ("file_name".to_string(), Value::String(entry.file_name)),
            (
                "file_type".to_string(),
                Value::String(entry.file_type.as_str().to_string()),
            ),
            (
                "metadata".to_string(),
                runtime_fs_metadata_value(entry.metadata),
            ),
        ],
    )
}

fn runtime_fs_dir_entry_array_value(entries: Vec<DirectoryEntry>) -> Value {
    runtime_array_value(
        entries
            .into_iter()
            .map(runtime_fs_dir_entry_value)
            .collect(),
    )
}

fn runtime_fs_chunk_value(chunk: FsChunk) -> Value {
    runtime_struct_value(
        "FsChunk",
        vec![
            (
                "index".to_string(),
                Value::Int(chunk.index.min(i64::MAX as u64) as i64),
            ),
            (
                "offset".to_string(),
                Value::Int(chunk.offset.min(i64::MAX as u64) as i64),
            ),
            (
                "len".to_string(),
                Value::Int(chunk.len().min(i64::MAX as u64) as i64),
            ),
            ("bytes".to_string(), runtime_byte_array_value(&chunk.bytes)),
        ],
    )
}

fn runtime_fs_chunk_array_value(chunks: Vec<FsChunk>) -> Value {
    runtime_array_value(chunks.into_iter().map(runtime_fs_chunk_value).collect())
}

fn runtime_fs_watch_event_value(event: FsWatchEvent) -> Value {
    runtime_struct_value(
        "FsWatchEvent",
        vec![
            (
                "kind".to_string(),
                Value::String(event.kind.as_str().to_string()),
            ),
            (
                "path".to_string(),
                Value::String(event.path.to_string_lossy().into_owned()),
            ),
            (
                "before_len".to_string(),
                Value::Int(
                    event
                        .before
                        .as_ref()
                        .map(|snapshot| snapshot.len.min(i64::MAX as u64) as i64)
                        .unwrap_or(-1),
                ),
            ),
            (
                "after_len".to_string(),
                Value::Int(
                    event
                        .after
                        .as_ref()
                        .map(|snapshot| snapshot.len.min(i64::MAX as u64) as i64)
                        .unwrap_or(-1),
                ),
            ),
        ],
    )
}

fn runtime_fs_watch_event_array_value(events: Vec<FsWatchEvent>) -> Value {
    runtime_array_value(
        events
            .into_iter()
            .map(runtime_fs_watch_event_value)
            .collect(),
    )
}

fn runtime_fs_journal_entry_value(entry: FsJournalEntry) -> Value {
    runtime_struct_value(
        "FsJournalEntry",
        vec![
            ("operation".to_string(), Value::String(entry.operation)),
            (
                "path".to_string(),
                Value::String(entry.path.to_string_lossy().into_owned()),
            ),
            (
                "other_path".to_string(),
                Value::String(
                    entry
                        .other_path
                        .as_ref()
                        .map(|path| path.to_string_lossy().into_owned())
                        .unwrap_or_default(),
                ),
            ),
            ("status".to_string(), Value::String(entry.status)),
            ("message".to_string(), Value::String(entry.message)),
        ],
    )
}

fn runtime_fs_journal_array_value(entries: Vec<FsJournalEntry>) -> Value {
    runtime_array_value(
        entries
            .into_iter()
            .map(runtime_fs_journal_entry_value)
            .collect(),
    )
}

fn runtime_with_transaction(
    id: i64,
    function_name: &str,
    operation: impl FnOnce(&mut kain_fs::FsTransaction) -> KainResult<Value>,
) -> KainResult<Value> {
    let mut transactions = RUNTIME_FS_TRANSACTIONS.write().unwrap();
    let Some(transaction) = transactions.get_mut(&id) else {
        return Err(KainError::runtime(format!(
            "{function_name}: transaction {id} does not exist"
        )));
    };
    operation(transaction)
}

fn runtime_optional_millis_value(value: Option<u128>) -> Value {
    Value::Int(
        value
            .and_then(|millis| i64::try_from(millis).ok())
            .unwrap_or(-1),
    )
}

fn runtime_patch_mutation_record_value(change: &PatchMutationRecord) -> Value {
    runtime_struct_value(
        "PatchMutationRecord",
        vec![
            ("path".to_string(), Value::String(change.path.clone())),
            ("old_value".to_string(), change.old_value.clone()),
            ("new_value".to_string(), change.new_value.clone()),
        ],
    )
}

fn runtime_patch_record_value(record: &PatchRuntimeRecord) -> Value {
    runtime_struct_value(
        "PatchRuntimeRecord",
        vec![
            ("name".to_string(), Value::String(record.name.clone())),
            (
                "mutation_paths".to_string(),
                runtime_string_array_value(&record.mutation_paths),
            ),
            (
                "undo_mode".to_string(),
                Value::String(record.undo_mode.clone()),
            ),
            (
                "collaboration_event".to_string(),
                Value::String(record.collaboration_event.clone()),
            ),
            (
                "changes".to_string(),
                runtime_array_value(
                    record
                        .changes
                        .iter()
                        .map(runtime_patch_mutation_record_value)
                        .collect(),
                ),
            ),
        ],
    )
}

fn runtime_patch_collaboration_event_value(event: &PatchCollaborationEvent) -> Value {
    runtime_struct_value(
        "PatchCollaborationEvent",
        vec![
            (
                "event_id".to_string(),
                Value::String(event.event_id.clone()),
            ),
            (
                "patch_name".to_string(),
                Value::String(event.patch_name.clone()),
            ),
            (
                "mutation_paths".to_string(),
                runtime_string_array_value(&event.mutation_paths),
            ),
            (
                "undo_mode".to_string(),
                Value::String(event.undo_mode.clone()),
            ),
        ],
    )
}

fn eval_binop(op: BinaryOp, left: Value, right: Value) -> KainResult<Value> {
    match (op, &left, &right) {
        (BinaryOp::Add, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a + b)),
        (BinaryOp::Sub, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a - b)),
        (BinaryOp::Mul, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a * b)),
        (BinaryOp::Div, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a / b)),
        (BinaryOp::Mod, Value::Int(a), Value::Int(b)) => Ok(Value::Int(a % b)),
        (BinaryOp::Add, Value::Float(a), Value::Float(b)) => Ok(Value::Float(a + b)),
        (BinaryOp::Sub, Value::Float(a), Value::Float(b)) => Ok(Value::Float(a - b)),
        (BinaryOp::Mul, Value::Float(a), Value::Float(b)) => Ok(Value::Float(a * b)),
        (BinaryOp::Div, Value::Float(a), Value::Float(b)) => Ok(Value::Float(a / b)),
        (BinaryOp::Add, Value::String(a), Value::String(b)) => Ok(Value::String(a.to_owned() + b)),
        (BinaryOp::Eq, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a == b)),
        (BinaryOp::Ne, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a != b)),
        (BinaryOp::Eq, Value::String(a), Value::String(b)) => Ok(Value::Bool(a == b)),
        (BinaryOp::Ne, Value::String(a), Value::String(b)) => Ok(Value::Bool(a != b)),
        (BinaryOp::Eq, Value::Bool(a), Value::Bool(b)) => Ok(Value::Bool(a == b)),
        (BinaryOp::Ne, Value::Bool(a), Value::Bool(b)) => Ok(Value::Bool(a != b)),

        // Float comparisons
        (BinaryOp::Lt, Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a < b)),
        (BinaryOp::Gt, Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a > b)),
        (BinaryOp::Le, Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a <= b)),
        (BinaryOp::Ge, Value::Float(a), Value::Float(b)) => Ok(Value::Bool(a >= b)),
        (BinaryOp::Eq, Value::Float(a), Value::Float(b)) => {
            Ok(Value::Bool((a - b).abs() < f64::EPSILON))
        }
        (BinaryOp::Ne, Value::Float(a), Value::Float(b)) => {
            Ok(Value::Bool((a - b).abs() >= f64::EPSILON))
        }

        (BinaryOp::Lt, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a < b)),
        (BinaryOp::Gt, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a > b)),
        (BinaryOp::Le, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a <= b)),
        (BinaryOp::Ge, Value::Int(a), Value::Int(b)) => Ok(Value::Bool(a >= b)),
        (BinaryOp::And, Value::Bool(a), Value::Bool(b)) => Ok(Value::Bool(*a && *b)),
        (BinaryOp::Or, Value::Bool(a), Value::Bool(b)) => Ok(Value::Bool(*a || *b)),
        (BinaryOp::BitAnd, Value::Int(a), Value::Int(b))
            if runtime_supports_binary_op(BinaryOp::BitAnd) =>
        {
            Ok(Value::Int(a & b))
        }
        (BinaryOp::BitOr, Value::Int(a), Value::Int(b))
            if runtime_supports_binary_op(BinaryOp::BitOr) =>
        {
            Ok(Value::Int(a | b))
        }
        (BinaryOp::BitXor, Value::Int(a), Value::Int(b))
            if runtime_supports_binary_op(BinaryOp::BitXor) =>
        {
            Ok(Value::Int(a ^ b))
        }
        (BinaryOp::Shl, Value::Int(a), Value::Int(b))
            if runtime_supports_binary_op(BinaryOp::Shl) =>
        {
            let shift = (*b as u32) & 63;
            Ok(Value::Int(a.wrapping_shl(shift)))
        }
        (BinaryOp::Shr, Value::Int(a), Value::Int(b))
            if runtime_supports_binary_op(BinaryOp::Shr) =>
        {
            let shift = (*b as u32) & 63;
            Ok(Value::Int(a.wrapping_shr(shift)))
        }
        (BinaryOp::Eq, Value::None, Value::None) => Ok(Value::Bool(true)),
        (BinaryOp::Ne, Value::None, Value::None) => Ok(Value::Bool(false)),
        (BinaryOp::Eq, Value::Unit, Value::Unit) => Ok(Value::Bool(true)),
        (BinaryOp::Ne, Value::Unit, Value::Unit) => Ok(Value::Bool(false)),
        (BinaryOp::Eq, _, _) => Ok(Value::Bool(false)),
        (BinaryOp::Ne, _, _) => Ok(Value::Bool(true)),

        // Error on mismatch unless one is Any?
        _ => Err(KainError::runtime(format!(
            "Type mismatch in binary operation: {:?} {:?} {:?}",
            left, op, right
        ))),
    }
}

fn pattern_matches(pattern: &Pattern, value: &Value) -> bool {
    match pattern {
        Pattern::Wildcard(_) => true,
        Pattern::Binding { .. } => true,
        Pattern::Literal(Expr::Int(n, _)) => matches!(value, Value::Int(v) if *v == *n),
        Pattern::Literal(Expr::String(s, _)) => matches!(value, Value::String(v) if v == s),
        Pattern::Literal(Expr::Bool(b, _)) => matches!(value, Value::Bool(v) if *v == *b),
        Pattern::Literal(Expr::None(_)) => matches!(value, Value::None),
        Pattern::Variant {
            variant, fields, ..
        } => {
            if let Value::Poll(ready, val) = value {
                if *variant == "Ready" {
                    if !ready {
                        return false;
                    }
                    if let VariantPatternFields::Tuple(pats) = fields {
                        if pats.len() == 1 {
                            return if let Some(v) = val {
                                pattern_matches(&pats[0], v)
                            } else {
                                false
                            };
                        }
                    }
                    return false;
                } else if *variant == "Pending" {
                    return !ready;
                }
                return false;
            }
            if let Value::EnumVariant(_, v_name, v_fields) = value {
                if variant != v_name {
                    return false;
                }
                match fields {
                    VariantPatternFields::Unit => v_fields.is_empty(),
                    VariantPatternFields::Tuple(pats) => {
                        if pats.len() != v_fields.len() {
                            return false;
                        }
                        pats.iter()
                            .zip(v_fields.iter())
                            .all(|(p, v)| pattern_matches(p, v))
                    }
                    _ => false,
                }
            } else {
                false
            }
        }
        _ => false,
    }
}

fn bind_pattern(env: &mut Env, pattern: &Pattern, value: &Value) {
    match pattern {
        Pattern::Binding { name, .. } => {
            env.define(name.clone(), value.clone());
        }
        Pattern::Variant {
            variant, fields, ..
        } => {
            if let Value::Poll(ready, val) = value {
                if *variant == "Ready" && *ready {
                    if let VariantPatternFields::Tuple(pats) = fields {
                        if pats.len() == 1 {
                            if let Some(v) = val {
                                bind_pattern(env, &pats[0], v);
                            }
                        }
                    }
                }
            } else if let Value::EnumVariant(_, _, v_fields) = value {
                match fields {
                    VariantPatternFields::Tuple(pats) => {
                        for (p, v) in pats.iter().zip(v_fields.iter()) {
                            bind_pattern(env, p, v);
                        }
                    }
                    _ => {}
                }
            }
        }
        _ => {}
    }
}

/// Run all tests in the program
pub fn run_tests(program: &TypedProgram) -> KainResult<()> {
    println!("\n Running Tests...\n");
    let mut passed = 0;
    let mut failed = 0;
    let mut failure_messages = Vec::new();
    let mut tests = Vec::new();

    // Initialize env
    let mut env = Env::new();
    env.set_execution_lane(ExecutionLane::Test);

    env.register_typed_program(program)?;
    env.apply_registered_extensions();
    collect_runtime_tests(&program.items, &mut Vec::new(), &mut tests);

    // Run tests
    for (test_name, test) in tests {
        print!("test {} ... ", test_name);

        // Isolate test scope
        env.push_scope();

        match eval_block(&mut env, &test.ast.body) {
            Ok(_) => {
                println!("ok");
                passed += 1;
            }
            Err(e) => {
                println!("FAILED");
                println!("  Error: {}", e);
                failed += 1;
                failure_messages.push(format!("{}: {}", test_name, e));
            }
        }

        env.pop_scope();
    }

    println!(
        "\nTest result: {}. {} passed; {} failed",
        if failed == 0 { "ok" } else { "FAILED" },
        passed,
        failed
    );

    if failed > 0 {
        Err(KainError::runtime(format!(
            "Some tests failed: {}",
            failure_messages.join("; ")
        )))
    } else {
        Ok(())
    }
}

fn collect_runtime_tests<'a>(
    items: &'a [crate::types::TypedItem],
    module_path: &mut Vec<String>,
    output: &mut Vec<(String, &'a crate::types::TypedTest)>,
) {
    for item in items {
        match item {
            crate::types::TypedItem::Test(test) => {
                let mut name = module_path.join("::");
                if !name.is_empty() {
                    name.push_str("::");
                }
                name.push_str(&test.ast.name);
                output.push((name, test));
            }
            crate::types::TypedItem::Mod(module) => {
                module_path.push(module.ast.name.clone());
                collect_runtime_tests(&module.items, module_path, output);
                module_path.pop();
            }
            _ => {}
        }
    }
}

// === ASYNC RUNTIME HELPERS ===

/// Poll a future repeatedly until it returns Ready
fn poll_future_to_completion(env: &mut Env, future_val: Value) -> KainResult<Value> {
    let max_iterations = 100000; // Prevent infinite loops
    let mut iterations = 0;
    let current_future = future_val;

    loop {
        iterations += 1;
        if iterations > max_iterations {
            return Err(KainError::runtime("Async timeout: future did not complete"));
        }

        let poll_result = poll_future_once(env, current_future.clone())?;

        match extract_poll_result(&poll_result) {
            PollState::Ready(val) => return Ok(val),
            PollState::Pending => {
                // In a real async runtime, we'd yield to other tasks here
                // For now, just continue polling (cooperative busy-wait)
                std::thread::sleep(std::time::Duration::from_micros(10));
                continue;
            }
            PollState::NotAPoll => {
                // Not a recognizable poll result - return as-is (might be already resolved)
                return Ok(poll_result);
            }
        }
    }
}

/// Poll a future exactly once and return the Poll result
fn poll_future_once(env: &mut Env, future_val: Value) -> KainResult<Value> {
    match &future_val {
        // Handle Future struct (from async fn transformation)
        Value::Future(struct_name, state) => {
            let poll_fn_name = format!("{}_poll", struct_name);

            if let Some(poll_fn) = env.functions.get(&poll_fn_name).cloned() {
                // Create a temporary struct value from the state
                let struct_val = Value::Struct(struct_name.clone(), state.clone());

                // Call poll function with self parameter
                env.push_scope();
                env.define("self".to_string(), struct_val.clone());

                if let Some(first_param) = poll_fn.params.first() {
                    env.define(first_param.name.clone(), struct_val);
                }

                let result = eval_block(env, &poll_fn.body)?;
                env.pop_scope();

                // Unwrap Value::Return if present
                let actual_result = match result {
                    Value::Return(v) => *v,
                    v => v,
                };

                // Normalize the result to our Poll representation
                Ok(normalize_poll_result(actual_result))
            } else {
                // No poll function - treat as immediately ready with unit
                Ok(Value::Poll(true, Some(Box::new(Value::Unit))))
            }
        }

        // Handle plain struct that might be a future
        Value::Struct(struct_name, _) => {
            let poll_fn_name = format!("{}_poll", struct_name);

            if let Some(poll_fn) = env.functions.get(&poll_fn_name).cloned() {
                // Call poll with the future as self
                env.push_scope();
                env.define("self".to_string(), future_val.clone());

                if let Some(first_param) = poll_fn.params.first() {
                    env.define(first_param.name.clone(), future_val.clone());
                }

                let result = eval_block(env, &poll_fn.body)?;
                env.pop_scope();

                // Unwrap Value::Return if present
                let actual_result = match result {
                    Value::Return(v) => *v,
                    v => v,
                };

                Ok(normalize_poll_result(actual_result))
            } else {
                // No poll function - might be an already-resolved value
                Ok(Value::Poll(true, Some(Box::new(future_val))))
            }
        }

        // Already a Poll value - return as-is
        Value::Poll(_, _) => Ok(future_val),

        // EnumVariant that might be Poll::Ready or Poll::Pending
        Value::EnumVariant(enum_name, _, _) if enum_name == "Poll" => {
            Ok(normalize_poll_result(future_val))
        }

        // Non-future value - treat as immediately ready
        _ => Ok(Value::Poll(true, Some(Box::new(future_val)))),
    }
}

/// Internal enum for poll state extraction
enum PollState {
    Ready(Value),
    Pending,
    NotAPoll,
}

/// Extract the poll state from a value
fn extract_poll_result(val: &Value) -> PollState {
    match val {
        // Native Poll value
        Value::Poll(true, Some(inner)) => PollState::Ready(*inner.clone()),
        Value::Poll(true, None) => PollState::Ready(Value::Unit),
        Value::Poll(false, _) => PollState::Pending,

        // EnumVariant style Poll::Ready/Poll::Pending
        Value::EnumVariant(enum_name, variant, fields) if enum_name == "Poll" => {
            match variant.as_str() {
                "Ready" => {
                    if fields.is_empty() {
                        PollState::Ready(Value::Unit)
                    } else {
                        PollState::Ready(fields[0].clone())
                    }
                }
                "Pending" => PollState::Pending,
                _ => PollState::NotAPoll,
            }
        }

        // Struct-based Poll (struct Poll_Ready { value: T } or struct Poll_Pending {})
        Value::Struct(name, fields) => {
            if name.contains("Ready") {
                let fields_guard = fields.read().unwrap();
                if let Some(val) = fields_guard
                    .get("0")
                    .or(fields_guard.get("value"))
                    .or(fields_guard.values().next())
                {
                    PollState::Ready(val.clone())
                } else {
                    PollState::Ready(Value::Unit)
                }
            } else if name.contains("Pending") {
                PollState::Pending
            } else {
                PollState::NotAPoll
            }
        }

        // Tuple style: ("Ready", value) or ("Pending",)
        Value::Tuple(elems) if elems.len() >= 1 => {
            if let Value::String(tag) = &elems[0] {
                match tag.as_str() {
                    "Ready" if elems.len() >= 2 => PollState::Ready(elems[1].clone()),
                    "Ready" => PollState::Ready(Value::Unit),
                    "Pending" => PollState::Pending,
                    _ => PollState::NotAPoll,
                }
            } else {
                PollState::NotAPoll
            }
        }

        _ => PollState::NotAPoll,
    }
}

/// Normalize various poll representations to our standard Value::Poll
fn normalize_poll_result(val: Value) -> Value {
    match extract_poll_result(&val) {
        PollState::Ready(inner) => Value::Poll(true, Some(Box::new(inner))),
        PollState::Pending => Value::Poll(false, None),
        PollState::NotAPoll => val, // Keep as-is
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ast::{CallArg, EnumVariantFields};
    use crate::span::Span;

    #[test]
    fn eval_method_call_supports_named_enum_to_string() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::MethodCall {
            receiver: Box::new(Expr::EnumVariant {
                enum_name: "KainError".to_string(),
                variant: "Runtime".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::String("boom".to_string(), span)]),
                span,
            }),
            method: "to_string".to_string(),
            args: Vec::<CallArg>::new(),
            span,
        };

        let value = eval_expr(&mut env, &expr).expect("named enums should stringify at runtime");
        match value {
            Value::String(rendered) => assert_eq!(rendered, "KainError::Runtime(boom)"),
            other => panic!("expected Value::String, found {other:?}"),
        }
    }

    #[test]
    fn eval_option_copied_keeps_some_payload() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::MethodCall {
            receiver: Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Bool(true, span)]),
                span,
            }),
            method: "copied".to_string(),
            args: Vec::<CallArg>::new(),
            span,
        };

        let value = eval_expr(&mut env, &expr).expect("Option.copied should evaluate");
        match value {
            Value::EnumVariant(enum_name, variant, fields)
                if enum_name == "Option" && variant == "Some" =>
            {
                assert!(matches!(fields.as_slice(), [Value::Bool(true)]));
            }
            other => panic!("expected Some(true), found {other:?}"),
        }
    }

    #[test]
    fn eval_option_filter_drops_some_when_predicate_is_false() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::MethodCall {
            receiver: Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Bool(true, span)]),
                span,
            }),
            method: "filter".to_string(),
            args: vec![CallArg {
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
                    body: Box::new(Expr::Bool(false, span)),
                    span,
                },
                span,
            }],
            span,
        };

        let value = eval_expr(&mut env, &expr).expect("Option.filter should evaluate");
        assert!(matches!(value, Value::None));
    }

    #[test]
    fn eval_option_take_clears_some_value_and_returns_previous_option() {
        let span = Span::default();
        let mut env = Env::new();
        env.define(
            "bit_pack".to_string(),
            Value::EnumVariant(
                "Option".to_string(),
                "Some".to_string(),
                vec![Value::String("pack".to_string())],
            ),
        );
        let expr = Expr::MethodCall {
            receiver: Box::new(Expr::Ident("bit_pack".to_string(), span)),
            method: "take".to_string(),
            args: vec![],
            span,
        };

        let value = eval_expr(&mut env, &expr).expect("Option.take should evaluate");
        assert!(matches!(
            value,
            Value::EnumVariant(enum_name, variant, fields)
                if enum_name == "Option" && variant == "Some" && fields.len() == 1
        ));
        assert!(matches!(env.lookup_value("bit_pack"), Some(Value::None)));
    }

    #[test]
    fn eval_imported_synthetic_tuple_field_access() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::Field {
            object: Box::new(Expr::Tuple(
                vec![
                    Expr::String("left".to_string(), span),
                    Expr::Bool(true, span),
                ],
                span,
            )),
            field: "__kain_tuple_1".to_string(),
            span,
        };

        let value = eval_expr(&mut env, &expr).expect("synthetic tuple field should evaluate");
        assert!(matches!(value, Value::Bool(true)));
    }

    #[test]
    fn eval_try_unwraps_option_some_payload() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::Try(
            Box::new(Expr::EnumVariant {
                enum_name: "Option".to_string(),
                variant: "Some".to_string(),
                fields: EnumVariantFields::Tuple(vec![Expr::Bool(true, span)]),
                span,
            }),
            span,
        );

        let value = eval_expr(&mut env, &expr).expect("Option-based '?' should unwrap Some");
        assert!(matches!(value, Value::Bool(true)));
    }

    #[test]
    fn eval_try_propagates_option_none_as_early_return() {
        let span = Span::default();
        let mut env = Env::new();
        let expr = Expr::Try(Box::new(Expr::None(span)), span);

        let value = eval_expr(&mut env, &expr).expect("Option-based '?' should propagate None");
        assert!(matches!(value, Value::Return(inner) if matches!(inner.as_ref(), Value::None)));
    }
}
