//! Native Rust host runtime for KAIN.
//!
//! `kain-host` lets Rust applications:
//! - compile and register KAIN code for interpretation
//! - expose Rust native functions to KAIN
//! - call KAIN functions directly from Rust
//! - move common primitive values across the boundary without codegen

extern crate self as kain_host;

use std::collections::{BTreeMap, HashMap};
use std::sync::{Arc, RwLock};

pub use kain_core::error::KainError;
pub use kain_core::runtime::{Env, NativeFn, Value};
pub use kain_core::{CompileTarget, TypedProgram};

#[cfg(feature = "derive")]
pub use kain_host_derive::{FromKainValue, ToKainValue};

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
}

impl HostSession {
    pub fn new() -> Self {
        Self {
            env: Env::new(),
            native_functions: BTreeMap::new(),
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

    pub fn set_global<V>(&mut self, name: impl Into<String>, value: V) -> &mut Self
    where
        V: ToKainValue,
    {
        self.env.define_global(name, value.to_kain_value());
        self
    }

    pub fn compile_source(&self, source: &str) -> HostResult<TypedProgram> {
        let augmented = if self.native_functions.is_empty() {
            source.to_string()
        } else {
            format!("{}\n{}", self.emit_native_prelude(), source)
        };
        kain_driver::frontend_to_typed_program(&augmented, CompileTarget::Interpret)
    }

    pub fn load_typed_program(&mut self, program: &TypedProgram) -> HostResult<&mut Self> {
        self.env.register_typed_program(program)?;
        self.rebind_natives();
        Ok(self)
    }

    pub fn load_source(&mut self, source: &str) -> HostResult<&mut Self> {
        let program = self.compile_source(source)?;
        self.load_typed_program(&program)
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
        Value::Function(_) => "Function",
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
}
