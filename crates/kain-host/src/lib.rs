//! Native Rust host runtime for KAIN.
//!
//! `kain-host` lets Rust applications:
//! - compile and register KAIN code for interpretation
//! - expose Rust native functions to KAIN
//! - call KAIN functions directly from Rust
//! - move common primitive values across the boundary without codegen

extern crate self as kain_host;

use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::path::PathBuf;
use std::sync::{Arc, RwLock};

pub use kain_core::ast::Program;
pub use kain_core::error::KainError;
pub use kain_core::runtime::{Env, NativeFn, Value};
pub use kain_core::{CompileTarget, TypedProgram};
pub use kain_reflect as reflect;
pub use kain_reflect::{KainReflect, StaticTypeRef, TypeRegistry, TypeSchema};

#[cfg(feature = "derive")]
pub use kain_host_derive::{FromKainValue, KainReflect, ToKainValue};

pub type HostResult<T> = Result<T, KainError>;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum HostType {
    Unit,
    Bool,
    Int,
    Float,
    String,
    Option(Box<HostType>),
    Array(Box<HostType>),
}

impl HostType {
    fn render(&self) -> String {
        match self {
            Self::Unit => "Unit".to_string(),
            Self::Bool => "Bool".to_string(),
            Self::Int => "Int".to_string(),
            Self::Float => "Float".to_string(),
            Self::String => "String".to_string(),
            Self::Option(inner) => format!("Option<{}>", inner.render()),
            Self::Array(inner) => format!("Array<{}>", inner.render()),
        }
    }

    fn default_literal(&self) -> &'static str {
        match self {
            Self::Unit => "()",
            Self::Bool => "false",
            Self::Int => "0",
            Self::Float => "0.0",
            Self::String => "\"\"",
            Self::Option(_) => "none",
            Self::Array(_) => "[]",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct NativeParam {
    pub name: String,
    pub ty: HostType,
}

impl NativeParam {
    pub fn new(name: impl Into<String>, ty: HostType) -> Self {
        Self {
            name: name.into(),
            ty,
        }
    }
}

#[derive(Clone)]
pub struct NativeFunction {
    pub name: String,
    pub params: Vec<NativeParam>,
    pub return_type: HostType,
    pub func: NativeFn,
}

impl NativeFunction {
    pub fn new(
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
        func: NativeFn,
    ) -> Self {
        Self {
            name: name.into(),
            params,
            return_type,
            func,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HostPreludeConfig {
    pub module_name: String,
    pub auto_use_glob: bool,
}

impl Default for HostPreludeConfig {
    fn default() -> Self {
        Self {
            module_name: "engine".to_string(),
            auto_use_glob: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EngineModuleExportConfig {
    pub output_dir: PathBuf,
    pub module_name: String,
    pub module_file_name: String,
    pub import_shim_file_name: Option<String>,
    pub include_banner: bool,
}

impl EngineModuleExportConfig {
    pub fn for_module(module_name: impl Into<String>) -> Self {
        let module_name = module_name.into();
        Self {
            output_dir: PathBuf::from("."),
            module_file_name: format!("{module_name}.kn"),
            import_shim_file_name: Some(format!("{module_name}_prelude.kn")),
            module_name,
            include_banner: true,
        }
    }

    pub fn with_output_dir(mut self, output_dir: impl Into<PathBuf>) -> Self {
        self.output_dir = output_dir.into();
        self
    }

    pub fn with_module_file_name(mut self, file_name: impl Into<String>) -> Self {
        self.module_file_name = file_name.into();
        self
    }

    pub fn with_import_shim_file_name(mut self, file_name: impl Into<String>) -> Self {
        self.import_shim_file_name = Some(file_name.into());
        self
    }

    pub fn without_import_shim(mut self) -> Self {
        self.import_shim_file_name = None;
        self
    }

    pub fn with_banner(mut self, include_banner: bool) -> Self {
        self.include_banner = include_banner;
        self
    }

    pub fn module_path(&self) -> PathBuf {
        self.output_dir.join(&self.module_file_name)
    }

    pub fn import_shim_path(&self) -> Option<PathBuf> {
        self.import_shim_file_name
            .as_ref()
            .map(|file_name| self.output_dir.join(file_name))
    }
}

impl Default for EngineModuleExportConfig {
    fn default() -> Self {
        Self::for_module("engine")
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EngineModuleExport {
    pub module_name: String,
    pub module_path: PathBuf,
    pub module_source: String,
    pub import_shim_path: Option<PathBuf>,
    pub import_shim_source: Option<String>,
}

pub mod fabric;

pub mod bridge {
    use super::*;

    pub fn struct_value<K, I>(name: impl Into<String>, fields: I) -> Value
    where
        K: Into<String>,
        I: IntoIterator<Item = (K, Value)>,
    {
        let fields = fields
            .into_iter()
            .map(|(name, value)| (name.into(), value))
            .collect::<HashMap<_, _>>();
        Value::Struct(name.into(), Arc::new(RwLock::new(fields)))
    }

    pub fn expect_struct(value: Value, expected_name: &str) -> HostResult<HashMap<String, Value>> {
        match value {
            Value::Struct(name, fields) => {
                if name != expected_name {
                    return Err(KainError::runtime(format!(
                        "Expected struct {expected_name}, got struct {name}"
                    )));
                }
                let fields = fields
                    .read()
                    .map_err(|_| KainError::runtime("Failed to read struct fields"))?
                    .clone();
                Ok(fields)
            }
            other => Err(super::type_mismatch(expected_name, &other)),
        }
    }

    pub fn take_struct_field<T>(
        fields: &mut HashMap<String, Value>,
        field_name: &str,
    ) -> HostResult<T>
    where
        T: FromKainValue,
    {
        let value = fields
            .remove(field_name)
            .ok_or_else(|| KainError::runtime(format!("Missing struct field '{field_name}'")))?;
        T::from_kain_value(value)
    }

    pub fn enum_variant_value(
        enum_name: impl Into<String>,
        variant_name: impl Into<String>,
        fields: Vec<Value>,
    ) -> Value {
        Value::EnumVariant(enum_name.into(), variant_name.into(), fields)
    }

    pub fn expect_enum(value: Value, expected_name: &str) -> HostResult<(String, Vec<Value>)> {
        match value {
            Value::EnumVariant(enum_name, variant, fields) => {
                if enum_name != expected_name {
                    return Err(KainError::runtime(format!(
                        "Expected enum {expected_name}, got enum {enum_name}"
                    )));
                }
                Ok((variant, fields))
            }
            other => Err(super::type_mismatch(expected_name, &other)),
        }
    }

    pub fn expect_variant_len(
        fields: Vec<Value>,
        expected_len: usize,
        enum_name: &str,
        variant_name: &str,
    ) -> HostResult<Vec<Value>> {
        if fields.len() == expected_len {
            Ok(fields)
        } else {
            Err(KainError::runtime(format!(
                "Enum variant {enum_name}::{variant_name} expected {expected_len} field(s), got {}",
                fields.len()
            )))
        }
    }
}

pub trait ToKainValue {
    fn to_kain_value(self) -> Value;
}

pub trait FromKainValue: Sized {
    fn from_kain_value(value: Value) -> HostResult<Self>;
}

impl ToKainValue for Value {
    fn to_kain_value(self) -> Value {
        self
    }
}

impl ToKainValue for () {
    fn to_kain_value(self) -> Value {
        Value::Unit
    }
}

impl ToKainValue for bool {
    fn to_kain_value(self) -> Value {
        Value::Bool(self)
    }
}

impl ToKainValue for i64 {
    fn to_kain_value(self) -> Value {
        Value::Int(self)
    }
}

impl ToKainValue for i32 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for i16 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for i8 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for u32 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for u64 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for u16 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for u8 {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for usize {
    fn to_kain_value(self) -> Value {
        Value::Int(self as i64)
    }
}

impl ToKainValue for f64 {
    fn to_kain_value(self) -> Value {
        Value::Float(self)
    }
}

impl ToKainValue for f32 {
    fn to_kain_value(self) -> Value {
        Value::Float(self as f64)
    }
}

impl ToKainValue for String {
    fn to_kain_value(self) -> Value {
        Value::String(self)
    }
}

impl ToKainValue for &str {
    fn to_kain_value(self) -> Value {
        Value::String(self.to_string())
    }
}

impl<T> ToKainValue for Option<T>
where
    T: ToKainValue,
{
    fn to_kain_value(self) -> Value {
        match self {
            Some(value) => Value::Result(true, Box::new(value.to_kain_value())),
            None => Value::None,
        }
    }
}

impl<T> ToKainValue for Vec<T>
where
    T: ToKainValue,
{
    fn to_kain_value(self) -> Value {
        let values = self.into_iter().map(ToKainValue::to_kain_value).collect();
        Value::Array(std::sync::Arc::new(std::sync::RwLock::new(values)))
    }
}

impl FromKainValue for Value {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        Ok(value)
    }
}

impl FromKainValue for () {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::Unit => Ok(()),
            other => Err(type_mismatch("Unit", &other)),
        }
    }
}

impl FromKainValue for bool {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::Bool(value) => Ok(value),
            other => Err(type_mismatch("Bool", &other)),
        }
    }
}

impl FromKainValue for i64 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::Int(value) => Ok(value),
            other => Err(type_mismatch("Int", &other)),
        }
    }
}

impl FromKainValue for i32 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        i32::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into i32"))
    }
}

impl FromKainValue for i16 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        i16::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into i16"))
    }
}

impl FromKainValue for i8 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        i8::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into i8"))
    }
}

impl FromKainValue for u32 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        u32::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into u32"))
    }
}

impl FromKainValue for u64 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        u64::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into u64"))
    }
}

impl FromKainValue for u16 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        u16::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into u16"))
    }
}

impl FromKainValue for u8 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        u8::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into u8"))
    }
}

impl FromKainValue for usize {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        let value = i64::from_kain_value(value)?;
        usize::try_from(value).map_err(|_| KainError::runtime("Int value did not fit into usize"))
    }
}

impl FromKainValue for f64 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::Float(value) => Ok(value),
            Value::Int(value) => Ok(value as f64),
            other => Err(type_mismatch("Float", &other)),
        }
    }
}

impl FromKainValue for f32 {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        Ok(f64::from_kain_value(value)? as f32)
    }
}

impl FromKainValue for String {
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::String(value) => Ok(value),
            other => Err(type_mismatch("String", &other)),
        }
    }
}

impl<T> FromKainValue for Option<T>
where
    T: FromKainValue,
{
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::None => Ok(None),
            Value::Result(true, inner) => Ok(Some(T::from_kain_value(*inner)?)),
            other => Err(type_mismatch("Option", &other)),
        }
    }
}

impl<T> FromKainValue for Vec<T>
where
    T: FromKainValue,
{
    fn from_kain_value(value: Value) -> HostResult<Self> {
        match value {
            Value::Array(values) => {
                let values = values
                    .read()
                    .map_err(|_| KainError::runtime("Failed to read array value"))?
                    .clone();
                values.into_iter().map(T::from_kain_value).collect()
            }
            other => Err(type_mismatch("Array", &other)),
        }
    }
}

pub struct HostSession {
    env: Env,
    native_functions: BTreeMap<String, NativeFunction>,
    type_registry: TypeRegistry,
    prelude_config: HostPreludeConfig,
}

impl HostSession {
    pub fn new() -> Self {
        Self {
            env: Env::new(),
            native_functions: BTreeMap::new(),
            type_registry: TypeRegistry::new(),
            prelude_config: HostPreludeConfig::default(),
        }
    }

    pub fn env(&self) -> &Env {
        &self.env
    }

    pub fn env_mut(&mut self) -> &mut Env {
        &mut self.env
    }

    pub fn register_native_function(&mut self, native: NativeFunction) -> &mut Self {
        self.env
            .register_native_fn(native.name.clone(), native.func);
        self.native_functions.insert(native.name.clone(), native);
        self
    }

    pub fn register_native_fn(
        &mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
        func: NativeFn,
    ) -> &mut Self {
        self.register_native_function(NativeFunction::new(name, params, return_type, func))
    }

    pub fn declare_native_fn(
        &mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
    ) -> &mut Self {
        let native = NativeFunction::new(name, params, return_type, unresolved_native_stub);
        self.native_functions.insert(native.name.clone(), native);
        self
    }

    pub fn prelude_config(&self) -> &HostPreludeConfig {
        &self.prelude_config
    }

    pub fn prelude_config_mut(&mut self) -> &mut HostPreludeConfig {
        &mut self.prelude_config
    }

    pub fn set_prelude_module_name(&mut self, module_name: impl Into<String>) -> &mut Self {
        self.prelude_config.module_name = module_name.into();
        self
    }

    pub fn set_auto_use_prelude(&mut self, enabled: bool) -> &mut Self {
        self.prelude_config.auto_use_glob = enabled;
        self
    }

    pub fn type_registry(&self) -> &TypeRegistry {
        &self.type_registry
    }

    pub fn type_registry_mut(&mut self) -> &mut TypeRegistry {
        &mut self.type_registry
    }

    pub fn register_type<T>(&mut self) -> &mut Self
    where
        T: KainReflect,
    {
        self.type_registry.register::<T>();
        self
    }

    pub fn register_schema(&mut self, schema: TypeSchema) -> &mut Self {
        self.type_registry.register_schema(schema);
        self
    }

    pub fn emit_type_prelude(&self) -> String {
        self.type_registry.render_kain_prelude()
    }

    pub fn emit_engine_module_source(&self) -> String {
        let mut body_sections = Vec::new();
        let type_prelude = self.emit_type_prelude();
        if !type_prelude.trim().is_empty() {
            body_sections.push(type_prelude.trim().to_string());
        }
        let native_prelude = self.emit_native_prelude();
        if !native_prelude.trim().is_empty() {
            body_sections.push(native_prelude.trim().to_string());
        }
        if body_sections.is_empty() {
            return String::new();
        }

        let mut output = body_sections.join("\n\n");
        if !output.ends_with('\n') {
            output.push('\n');
        }
        output
    }

    pub fn emit_engine_import_source(&self) -> String {
        format!("use {}::*\n", self.prelude_config.module_name)
    }

    pub fn emit_engine_prelude(&self) -> String {
        let module_source = self.emit_engine_module_source();
        if module_source.trim().is_empty() {
            return String::new();
        }

        let body = indent_block(module_source.trim_end(), 4);
        let mut output = format!("mod {}:\n{}\n", self.prelude_config.module_name, body);
        if self.prelude_config.auto_use_glob {
            output.push_str(&self.emit_engine_import_source());
        }
        output
    }

    pub fn export_engine_module(
        &self,
        config: &EngineModuleExportConfig,
    ) -> HostResult<EngineModuleExport> {
        fs::create_dir_all(&config.output_dir).map_err(io_to_host_error)?;

        let mut module_source = self.emit_engine_module_source();
        if config.include_banner {
            module_source = format!(
                "# Generated by kain-host for module {}\n# Regenerate from Rust reflection/native registrations.\n\n{}",
                config.module_name, module_source
            );
        }

        let module_path = config.module_path();
        fs::write(&module_path, &module_source).map_err(io_to_host_error)?;

        let import_shim = config.import_shim_path().map(|path| {
            let source = format!("use {}::*\n", config.module_name);
            (path, source)
        });

        if let Some((path, source)) = &import_shim {
            fs::write(path, source).map_err(io_to_host_error)?;
        }

        Ok(EngineModuleExport {
            module_name: config.module_name.clone(),
            module_path,
            module_source,
            import_shim_path: import_shim.as_ref().map(|(path, _)| path.clone()),
            import_shim_source: import_shim.map(|(_, source)| source),
        })
    }

    pub fn set_global<V>(&mut self, name: impl Into<String>, value: V) -> &mut Self
    where
        V: ToKainValue,
    {
        self.env.define_global(name, value.to_kain_value());
        self
    }

    pub fn compile_source(&self, source: &str) -> HostResult<TypedProgram> {
        Ok(self.compile_checked_source(source)?.typed)
    }

    pub fn compile_checked_source(&self, source: &str) -> HostResult<kain_driver::CheckedFrontend> {
        let mut sections = Vec::new();
        let engine_prelude = self.emit_engine_prelude();
        if !engine_prelude.trim().is_empty() {
            sections.push(engine_prelude);
        }
        sections.push(source.to_string());
        let augmented = sections.join("\n");
        kain_driver::frontend_to_checked_program(&augmented, CompileTarget::Interpret)
    }

    pub fn load_program(&mut self, program: &Program) -> HostResult<&mut Self> {
        self.env.register_program_items(program)?;
        self.rebind_natives();
        Ok(self)
    }

    pub fn load_typed_program(&mut self, program: &TypedProgram) -> HostResult<&mut Self> {
        self.env.register_typed_program(program)?;
        self.rebind_natives();
        Ok(self)
    }

    pub fn load_source(&mut self, source: &str) -> HostResult<&mut Self> {
        let checked = self.compile_checked_source(source)?;
        self.load_program(&checked.ast)
    }

    pub fn run_main_value(&mut self) -> HostResult<Value> {
        self.call_value("main", Vec::new())
    }

    pub fn run_main<R>(&mut self) -> HostResult<R>
    where
        R: FromKainValue,
    {
        R::from_kain_value(self.run_main_value()?)
    }

    pub fn call_value(&mut self, function_name: &str, args: Vec<Value>) -> HostResult<Value> {
        self.env.call_named_function(function_name, args)
    }

    pub fn call<R>(&mut self, function_name: &str, args: Vec<Value>) -> HostResult<R>
    where
        R: FromKainValue,
    {
        R::from_kain_value(self.call_value(function_name, args)?)
    }

    fn rebind_natives(&mut self) {
        for native in self.native_functions.values() {
            self.env
                .register_native_fn(native.name.clone(), native.func);
        }
    }

    fn emit_native_prelude(&self) -> String {
        let mut output = String::new();
        for native in self.native_functions.values() {
            let params = native
                .params
                .iter()
                .map(|param| format!("{}: {}", param.name, param.ty.render()))
                .collect::<Vec<_>>()
                .join(", ");
            output.push_str(&format!(
                "fn {}({}) -> {}:\n    return {}\n\n",
                native.name,
                params,
                native.return_type.render(),
                native.return_type.default_literal(),
            ));
        }
        output
    }
}

impl Default for HostSession {
    fn default() -> Self {
        Self::new()
    }
}

fn type_mismatch(expected: &str, value: &Value) -> KainError {
    KainError::runtime(format!("Expected {expected}, got {}", value_kind(value)))
}

fn indent_block(input: &str, spaces: usize) -> String {
    let prefix = " ".repeat(spaces);
    input
        .lines()
        .map(|line| {
            if line.trim().is_empty() {
                String::new()
            } else {
                format!("{prefix}{line}")
            }
        })
        .collect::<Vec<_>>()
        .join("\n")
}

fn io_to_host_error(error: std::io::Error) -> KainError {
    KainError::runtime(format!("I/O error: {error}"))
}

fn unresolved_native_stub(_env: &mut Env, _args: Vec<Value>) -> HostResult<Value> {
    Err(KainError::runtime(
        "Native function was declared for Kain export but not bound to a Rust implementation",
    ))
}

fn value_kind(value: &Value) -> &'static str {
    match value {
        Value::Unit => "Unit",
        Value::Bool(_) => "Bool",
        Value::Int(_) => "Int",
        Value::Float(_) => "Float",
        Value::String(_) => "String",
        Value::Array(_) => "Array",
        Value::Tuple(_) => "Tuple",
        Value::Struct(_, _) => "Struct",
        Value::HostObject(_, _) => "HostObject",
        Value::Function(_) => "Function",
        Value::Patch(_) => "Patch",
        Value::Converge(_) => "Converge",
        Value::Orchestrate(_) => "Orchestrate",
        Value::NativeFn(_, _) => "NativeFn",
        Value::ActorRef(_) => "ActorRef",
        Value::None => "None",
        Value::Return(_) => "Return",
        Value::Break(_) => "Break",
        Value::Continue => "Continue",
        Value::Result(_, _) => "Result",
        Value::Closure(_, _, _) => "Closure",
        Value::StructConstructor(_, _) => "StructConstructor",
        Value::JSX(_) => "JSX",
        Value::EnumVariant(_, _, _) => "EnumVariant",
        Value::Poll(_, _) => "Poll",
        Value::Future(_, _) => "Future",
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::reflect::{TypeKind, TypeRef};
    use std::time::{SystemTime, UNIX_EPOCH};

    fn host_double(_env: &mut Env, args: Vec<Value>) -> HostResult<Value> {
        let value = match args.as_slice() {
            [Value::Int(value)] => *value,
            _ => {
                return Err(KainError::runtime(
                    "host_double expected a single Int argument",
                ))
            }
        };
        Ok(Value::Int(value * 2))
    }

    #[test]
    fn rust_can_call_kain_function_directly() {
        let mut host = HostSession::new();
        host.load_source(
            r#"
fn add(a: Int, b: Int) -> Int:
    return a + b
"#,
        )
        .expect("load source");

        let result = host
            .call::<i64>("add", vec![2_i64.to_kain_value(), 5_i64.to_kain_value()])
            .expect("call add");

        assert_eq!(result, 7);
    }

    #[test]
    fn kain_can_call_registered_rust_native() {
        let mut host = HostSession::new();
        host.register_native_fn(
            "host_double",
            vec![NativeParam::new("value", HostType::Int)],
            HostType::Int,
            host_double,
        );

        host.load_source(
            r#"
fn run(value: Int) -> Int:
    return host_double(value)
"#,
        )
        .expect("load source");

        let result = host
            .call::<i64>("run", vec![21_i64.to_kain_value()])
            .expect("call run");

        assert_eq!(result, 42);
    }

    #[test]
    fn value_kind_reports_host_object() {
        let value = Value::host_object("test", std::sync::Arc::new(7_i64));
        assert_eq!(value_kind(&value), "HostObject");
    }

    #[test]
    fn reflected_type_prelude_is_generated() {
        #[derive(Clone)]
        struct LocalVec3;

        impl KainReflect for LocalVec3 {
            fn schema() -> TypeSchema {
                TypeSchema::new(
                    "Vec3",
                    "LocalVec3",
                    TypeKind::Struct {
                        fields: vec![
                            reflect::FieldSchema::new(
                                "x",
                                TypeRef::Primitive(reflect::PrimitiveType::Float),
                            ),
                            reflect::FieldSchema::new(
                                "y",
                                TypeRef::Primitive(reflect::PrimitiveType::Float),
                            ),
                        ],
                    },
                )
            }
        }

        let mut host = HostSession::new();
        host.register_type::<LocalVec3>();
        let prelude = host.emit_type_prelude();

        assert!(prelude.contains("struct Vec3:"));
        assert!(prelude.contains("x: Float"));
    }

    #[test]
    fn engine_prelude_wraps_declarations_in_module() {
        #[derive(Clone)]
        struct LocalId;

        impl KainReflect for LocalId {
            fn schema() -> TypeSchema {
                TypeSchema::new(
                    "EntityId",
                    "LocalId",
                    TypeKind::Transparent {
                        inner: TypeRef::Primitive(reflect::PrimitiveType::Int),
                    },
                )
            }
        }

        let mut host = HostSession::new();
        host.register_type::<LocalId>();
        host.register_native_fn(
            "host_double",
            vec![NativeParam::new("value", HostType::Int)],
            HostType::Int,
            host_double,
        );

        let prelude = host.emit_engine_prelude();
        assert!(prelude.contains("mod engine:"));
        assert!(prelude.contains("use engine::*"));
        assert!(prelude.contains("type EntityId = Int"));
        assert!(prelude.contains("fn host_double(value: Int) -> Int:"));
    }

    #[test]
    fn engine_module_export_writes_physical_module_and_import_shim() {
        #[derive(Clone)]
        struct LocalVec3;

        impl KainReflect for LocalVec3 {
            fn schema() -> TypeSchema {
                TypeSchema::new(
                    "Vec3",
                    "LocalVec3",
                    TypeKind::Struct {
                        fields: vec![
                            reflect::FieldSchema::new(
                                "x",
                                TypeRef::Primitive(reflect::PrimitiveType::Float),
                            ),
                            reflect::FieldSchema::new(
                                "y",
                                TypeRef::Primitive(reflect::PrimitiveType::Float),
                            ),
                        ],
                    },
                )
            }
        }

        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time")
            .as_nanos();
        let export_dir = std::env::temp_dir().join(format!("kain_host_export_{unique}"));

        let mut host = HostSession::new();
        host.register_type::<LocalVec3>();
        host.register_native_fn(
            "host_double",
            vec![NativeParam::new("value", HostType::Int)],
            HostType::Int,
            host_double,
        );

        let config = EngineModuleExportConfig::for_module("engine").with_output_dir(&export_dir);
        let export = host
            .export_engine_module(&config)
            .expect("export engine module");

        let module_text = std::fs::read_to_string(&export.module_path).expect("read module");
        let shim_path = export.import_shim_path.clone().expect("import shim path");
        let shim_text = std::fs::read_to_string(&shim_path).expect("read import shim");

        assert!(module_text.contains("struct Vec3:"));
        assert!(module_text.contains("fn host_double(value: Int) -> Int:"));
        assert!(!module_text.contains("mod engine:"));
        assert_eq!(shim_text, "use engine::*\n");

        std::fs::remove_dir_all(export_dir).expect("cleanup export dir");
    }
}
