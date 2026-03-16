use std::collections::HashMap;
use std::sync::{Arc, Once, RwLock};

use kain_core::error::{KainError, KainResult};
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};
use pyo3::prelude::*;
use pyo3::types::{PyByteArray, PyBytes, PyDict, PyList, PyTuple};

const PYTHON_EXTENSION_KEY: &str = "kain.python.scope";

static REGISTER: Once = Once::new();

struct PythonScopeState {
    scope: RwLock<PyObject>,
}

#[derive(Clone)]
struct PythonObjectRef {
    object: PyObject,
}

pub fn register() {
    REGISTER.call_once(|| {
        register_stdlib_extension("python", register_python_stdlib);
        register_env_extension("python", register_python_env);
    });
}

fn register_python_stdlib(stdlib: &mut StdLib) {
    for builtin in [
        BuiltinFn {
            name: "py_eval",
            params: vec![("code", "String")],
            return_type: "Any",
            doc: "Evaluate Python expression",
        },
        BuiltinFn {
            name: "py_eval_raw",
            params: vec![("code", "String")],
            return_type: "Any",
            doc: "Evaluate Python expression and keep the raw Python object",
        },
        BuiltinFn {
            name: "py_exec",
            params: vec![("code", "String")],
            return_type: "Unit",
            doc: "Execute Python code",
        },
        BuiltinFn {
            name: "py_import",
            params: vec![("module", "String")],
            return_type: "Any",
            doc: "Import Python module",
        },
        BuiltinFn {
            name: "py_call",
            params: vec![("target", "Any"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a Python callable or method with optional kwargs",
        },
        BuiltinFn {
            name: "py_call_raw",
            params: vec![("target", "Any"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a Python callable or method and keep the raw Python object",
        },
        BuiltinFn {
            name: "py_getattr",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Any",
            doc: "Read a Python attribute",
        },
        BuiltinFn {
            name: "py_getattr_raw",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Any",
            doc: "Read a Python attribute and keep the raw Python object",
        },
        BuiltinFn {
            name: "py_setattr",
            params: vec![("target", "Any"), ("name", "String"), ("value", "Any")],
            return_type: "Unit",
            doc: "Set a Python attribute",
        },
        BuiltinFn {
            name: "py_hasattr",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Bool",
            doc: "Check whether a Python attribute exists",
        },
        BuiltinFn {
            name: "py_buffer",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Create a raw Python memoryview for a buffer-capable object",
        },
        BuiltinFn {
            name: "py_buffer_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect shape, dtype, strides, contiguity, and byte size for a Python buffer",
        },
        BuiltinFn {
            name: "py_buffer_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot a Python buffer as a flat byte array",
        },
    ] {
        stdlib.functions.insert(builtin.name.to_string(), builtin);
    }
}

fn register_python_env(env: &mut Env) {
    if env
        .get_extension_state::<PythonScopeState>(PYTHON_EXTENSION_KEY)
        .is_none()
    {
        Python::with_gil(|py| {
            let scope = PyDict::new(py);
            let builtins = py
                .import("builtins")
                .expect("failed to import Python builtins");
            scope
                .set_item("__builtins__", builtins)
                .expect("failed to install Python builtins");
            env.set_extension_state(
                PYTHON_EXTENSION_KEY,
                Arc::new(PythonScopeState {
                    scope: RwLock::new(scope.into()),
                }),
            );
        });
    }

    env.register_native_fn("py_eval", py_eval_native);
    env.register_native_fn("py_eval_raw", py_eval_raw_native);
    env.register_native_fn("py_exec", py_exec_native);
    env.register_native_fn("py_import", py_import_native);
    env.register_native_fn("py_call", py_call_native);
    env.register_native_fn("py_call_raw", py_call_raw_native);
    env.register_native_fn("py_getattr", py_getattr_native);
    env.register_native_fn("py_getattr_raw", py_getattr_raw_native);
    env.register_native_fn("py_setattr", py_setattr_native);
    env.register_native_fn("py_hasattr", py_hasattr_native);
    env.register_native_fn("py_buffer", py_buffer_native);
    env.register_native_fn("py_buffer_info", py_buffer_info_native);
    env.register_native_fn("py_buffer_bytes", py_buffer_bytes_native);
}

fn py_eval_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime("py_eval: expected 1 argument (code)"));
    }
    let code = match &args[0] {
        Value::String(s) => s,
        _ => return Err(KainError::runtime("py_eval: expected string")),
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let result = py
            .eval(code, Some(scope_dict), Some(scope_dict))
            .map_err(|err| KainError::runtime(format!("Python Error: {err}")))?;
        py_to_value(result)
    })
}

fn py_eval_raw_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_eval_raw: expected 1 argument (code)",
        ));
    }
    let code = match &args[0] {
        Value::String(s) => s,
        _ => return Err(KainError::runtime("py_eval_raw: expected string")),
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let result = py
            .eval(code, Some(scope_dict), Some(scope_dict))
            .map_err(|err| KainError::runtime(format!("Python Error: {err}")))?;
        wrap_python_object(result)
    })
}

fn py_exec_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime("py_exec: expected 1 argument"));
    }
    let code = match &args[0] {
        Value::String(s) => s,
        _ => return Err(KainError::runtime("py_exec: expected string")),
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        py.run(code, Some(scope_dict), Some(scope_dict))
            .map_err(|err| KainError::runtime(format!("Python Error: {err}")))?;
        Ok(Value::Unit)
    })
}

fn py_import_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime("py_import: expected 1 argument"));
    }
    let module_name = match &args[0] {
        Value::String(s) => s,
        _ => return Err(KainError::runtime("py_import: argument must be string")),
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let module = py
            .import(module_name.as_str())
            .map_err(|err| KainError::runtime(format!("Python error: {err}")))?;
        scope_dict
            .set_item(module_name, module)
            .map_err(|err| KainError::runtime(format!("Failed to set module: {err}")))?;
        py_to_value(module)
    })
}

fn py_call_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    py_call_with_mode(env, args, false)
}

fn py_call_raw_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    py_call_with_mode(env, args, true)
}

fn py_call_with_mode(env: &mut Env, args: Vec<Value>, raw_result: bool) -> KainResult<Value> {
    let state = python_scope_state(env)?;
    let call_spec = parse_python_call(&args)?;

    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let callable = if let Some(attr_name) = call_spec.attr_name.as_deref() {
            let target = resolve_python_target(py, scope_dict, call_spec.target)?;
            target
                .as_ref(py)
                .getattr(attr_name)
                .map_err(|err| KainError::runtime(format!("Python getattr error: {err}")))?
                .into_py(py)
        } else {
            resolve_python_target(py, scope_dict, call_spec.target)?
        };

        let py_args = positional_args_to_tuple(py, call_spec.positional_args)?;
        let py_kwargs = keyword_args_to_dict(py, call_spec.keyword_args)?;

        let result = callable
            .as_ref(py)
            .call(py_args, py_kwargs)
            .map_err(|err| KainError::runtime(format!("Python call error: {err}")))?;
        if raw_result {
            wrap_python_object(result)
        } else {
            py_to_value(result)
        }
    })
}

fn py_getattr_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    py_getattr_with_mode(env, args, false)
}

fn py_getattr_raw_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    py_getattr_with_mode(env, args, true)
}

fn py_getattr_with_mode(env: &mut Env, args: Vec<Value>, raw_result: bool) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "py_getattr: expected 2 arguments (target, name)",
        ));
    }
    let attr_name = match &args[1] {
        Value::String(name) => name.clone(),
        _ => {
            return Err(KainError::runtime(
                "py_getattr: attribute name must be string",
            ))
        }
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let attr = target
            .as_ref(py)
            .getattr(attr_name.as_str())
            .map_err(|err| KainError::runtime(format!("Python getattr error: {err}")))?;
        if raw_result {
            wrap_python_object(attr)
        } else {
            py_to_value(attr)
        }
    })
}

fn py_setattr_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 3 {
        return Err(KainError::runtime(
            "py_setattr: expected 3 arguments (target, name, value)",
        ));
    }
    let attr_name = match &args[1] {
        Value::String(name) => name.clone(),
        _ => {
            return Err(KainError::runtime(
                "py_setattr: attribute name must be string",
            ))
        }
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let value = value_to_pyobject(py, &args[2])?;
        target
            .as_ref(py)
            .setattr(attr_name.as_str(), value)
            .map_err(|err| KainError::runtime(format!("Python setattr error: {err}")))?;
        Ok(Value::Unit)
    })
}

fn py_hasattr_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "py_hasattr: expected 2 arguments (target, name)",
        ));
    }
    let attr_name = match &args[1] {
        Value::String(name) => name.clone(),
        _ => {
            return Err(KainError::runtime(
                "py_hasattr: attribute name must be string",
            ))
        }
    };

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let has_attr = target
            .as_ref(py)
            .hasattr(attr_name.as_str())
            .map_err(|err| KainError::runtime(format!("Python hasattr error: {err}")))?;
        Ok(Value::Bool(has_attr))
    })
}

fn py_buffer_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_buffer: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let view = create_memoryview(py, target.as_ref(py))?;
        wrap_python_object(view.as_ref(py))
    })
}

fn py_buffer_info_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_buffer_info: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let view = create_memoryview(py, target.as_ref(py))?;
        build_buffer_info(target.as_ref(py), view.as_ref(py))
    })
}

fn py_buffer_bytes_native(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "py_buffer_bytes: expected 1 argument (target)",
        ));
    }

    let state = python_scope_state(env)?;
    Python::with_gil(|py| {
        let scope = state.scope.read().unwrap();
        let scope_dict = scope_dict_from_guard(py, &scope)?;
        let target = resolve_python_target(py, scope_dict, &args[0])?;
        let view = create_memoryview(py, target.as_ref(py))?;
        let bytes = view
            .as_ref(py)
            .call_method0("tobytes")
            .map_err(|err| KainError::runtime(format!("Python buffer export error: {err}")))?;
        py_to_value(bytes)
    })
}

struct PythonCallSpec<'a> {
    target: &'a Value,
    attr_name: Option<String>,
    positional_args: &'a Value,
    keyword_args: Option<&'a Value>,
}

fn parse_python_call(args: &[Value]) -> KainResult<PythonCallSpec<'_>> {
    match args {
        [target, positional_args] => Ok(PythonCallSpec {
            target,
            attr_name: None,
            positional_args,
            keyword_args: None,
        }),
        [target, second, third] => {
            if let Value::String(attr_name) = second {
                Ok(PythonCallSpec {
                    target,
                    attr_name: Some(attr_name.clone()),
                    positional_args: third,
                    keyword_args: None,
                })
            } else {
                Ok(PythonCallSpec {
                    target,
                    attr_name: None,
                    positional_args: second,
                    keyword_args: Some(third),
                })
            }
        }
        [target, Value::String(attr_name), positional_args, keyword_args] => Ok(PythonCallSpec {
            target,
            attr_name: Some(attr_name.clone()),
            positional_args,
            keyword_args: Some(keyword_args),
        }),
        _ => Err(KainError::runtime(
            "py_call: expected (target, args), (target, args, kwargs), (target, attr, args), or (target, attr, args, kwargs)",
        )),
    }
}

fn positional_args_to_tuple<'py>(py: Python<'py>, value: &Value) -> KainResult<&'py PyTuple> {
    let items = match value {
        Value::Array(values) => values.read().unwrap().clone(),
        Value::Tuple(values) => values.clone(),
        Value::None | Value::Unit => Vec::new(),
        _ => {
            return Err(KainError::runtime(
                "py_call: positional args must be an array, tuple, none, or unit",
            ))
        }
    };

    let mut py_values = Vec::with_capacity(items.len());
    for item in &items {
        py_values.push(value_to_pyobject(py, item)?);
    }
    Ok(PyTuple::new(py, py_values))
}

fn keyword_args_to_dict<'py>(
    py: Python<'py>,
    value: Option<&Value>,
) -> KainResult<Option<&'py PyDict>> {
    let Some(value) = value else {
        return Ok(None);
    };

    match value {
        Value::None | Value::Unit => Ok(None),
        Value::Struct(_, fields) => {
            let dict = PyDict::new(py);
            let guard = fields.read().unwrap();
            for (key, value) in guard.iter() {
                dict.set_item(key, value_to_pyobject(py, value)?)
                    .map_err(|err| KainError::runtime(format!("Python kwargs error: {err}")))?;
            }
            Ok(Some(dict))
        }
        _ => Err(KainError::runtime(
            "py_call: keyword args must be a struct/dict-like value, none, or unit",
        )),
    }
}

fn resolve_python_target<'py>(
    py: Python<'py>,
    scope: &'py PyDict,
    value: &Value,
) -> KainResult<PyObject> {
    match value {
        Value::HostObject(_, _) => extract_python_object(value, py),
        Value::String(expr) => py
            .eval(expr, Some(scope), Some(scope))
            .map(|result| result.into_py(py))
            .map_err(|err| KainError::runtime(format!("Python resolution error: {err}"))),
        _ => value_to_pyobject(py, value),
    }
}

fn create_memoryview(py: Python<'_>, target: &PyAny) -> KainResult<PyObject> {
    let builtins = py
        .import("builtins")
        .map_err(|err| KainError::runtime(format!("Python import error: {err}")))?;
    builtins
        .getattr("memoryview")
        .and_then(|callable| callable.call1((target,)))
        .map(|view| view.into_py(py))
        .map_err(|err| KainError::runtime(format!("Python buffer error: {err}")))
}

fn build_buffer_info(target: &PyAny, view: &PyAny) -> KainResult<Value> {
    let mut fields = HashMap::new();
    fields.insert("kind".to_string(), Value::String(python_type_path(target)));
    fields.insert(
        "label".to_string(),
        Value::String(python_object_label(target)),
    );
    fields.insert(
        "format".to_string(),
        py_optional_string_attr(view, "format")?,
    );
    fields.insert("dtype".to_string(), numpy_dtype_name(target, view)?);
    fields.insert("item_size".to_string(), py_int_attr(view, "itemsize")?);
    fields.insert("ndim".to_string(), py_int_attr(view, "ndim")?);
    fields.insert("nbytes".to_string(), py_int_attr(view, "nbytes")?);
    fields.insert("readonly".to_string(), py_bool_attr(view, "readonly")?);
    fields.insert(
        "c_contiguous".to_string(),
        py_bool_attr(view, "c_contiguous")?,
    );
    fields.insert(
        "f_contiguous".to_string(),
        py_bool_attr(view, "f_contiguous")?,
    );
    fields.insert("contiguous".to_string(), py_bool_attr(view, "contiguous")?);
    fields.insert("shape".to_string(), py_index_sequence_attr(view, "shape")?);
    fields.insert(
        "strides".to_string(),
        py_index_sequence_attr(view, "strides")?,
    );

    Ok(Value::Struct(
        "PyBufferInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    ))
}

fn py_optional_string_attr(target: &PyAny, name: &str) -> KainResult<Value> {
    match target.getattr(name) {
        Ok(value) if value.is_none() => Ok(Value::None),
        Ok(value) => value
            .extract::<String>()
            .map(Value::String)
            .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}"))),
        Err(err) => Err(KainError::runtime(format!(
            "Python attribute error ({name}): {err}"
        ))),
    }
}

fn py_int_attr(target: &PyAny, name: &str) -> KainResult<Value> {
    target
        .getattr(name)
        .and_then(|value| value.extract::<i64>())
        .map(Value::Int)
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))
}

fn py_bool_attr(target: &PyAny, name: &str) -> KainResult<Value> {
    target
        .getattr(name)
        .and_then(|value| value.extract::<bool>())
        .map(Value::Bool)
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))
}

fn py_index_sequence_attr(target: &PyAny, name: &str) -> KainResult<Value> {
    let value = target
        .getattr(name)
        .map_err(|err| KainError::runtime(format!("Python attribute error ({name}): {err}")))?;
    if value.is_none() {
        return Ok(Value::None);
    }

    if let Ok(tuple) = value.downcast::<PyTuple>() {
        let values = tuple
            .iter()
            .map(|item| {
                item.extract::<i64>().map(Value::Int).map_err(|err| {
                    KainError::runtime(format!("Python sequence conversion error ({name}): {err}"))
                })
            })
            .collect::<KainResult<Vec<_>>>()?;
        return Ok(Value::Array(Arc::new(RwLock::new(values))));
    }

    if let Ok(list) = value.downcast::<PyList>() {
        let values = list
            .iter()
            .map(|item| {
                item.extract::<i64>().map(Value::Int).map_err(|err| {
                    KainError::runtime(format!("Python sequence conversion error ({name}): {err}"))
                })
            })
            .collect::<KainResult<Vec<_>>>()?;
        return Ok(Value::Array(Arc::new(RwLock::new(values))));
    }

    py_to_value(value)
}

fn numpy_dtype_name(target: &PyAny, view: &PyAny) -> KainResult<Value> {
    for candidate in [
        Some(target),
        target.getattr("obj").ok(),
        view.getattr("obj").ok(),
    ] {
        let Some(candidate) = candidate else {
            continue;
        };
        if let Ok(dtype) = candidate.getattr("dtype") {
            if let Ok(name) = dtype
                .getattr("name")
                .and_then(|value| value.extract::<String>())
            {
                return Ok(Value::String(name));
            }
            if let Ok(name) = dtype.str().and_then(|value| value.extract::<String>()) {
                return Ok(Value::String(name));
            }
        }
    }

    py_optional_string_attr(view, "format")
}

fn python_type_path(obj: &PyAny) -> String {
    let type_name = obj
        .get_type()
        .name()
        .map(|name| name.to_string())
        .unwrap_or_else(|_| "object".to_string());
    let module_name = obj
        .get_type()
        .getattr("__module__")
        .and_then(|value| value.extract::<String>())
        .unwrap_or_else(|_| "builtins".to_string());
    format!("{module_name}.{type_name}")
}

fn extract_python_object(value: &Value, py: Python<'_>) -> KainResult<PyObject> {
    value
        .downcast_host_object::<PythonObjectRef>()
        .map(|object| object.object.clone_ref(py))
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Expected Python object handle, got {}",
                value.host_object_label().unwrap_or("host object")
            ))
        })
}

fn value_to_pyobject(py: Python<'_>, value: &Value) -> KainResult<PyObject> {
    match value {
        Value::Unit | Value::None => Ok(py.None()),
        Value::Bool(value) => Ok(value.into_py(py)),
        Value::Int(value) => Ok(value.into_py(py)),
        Value::Float(value) => Ok(value.into_py(py)),
        Value::String(value) => Ok(value.into_py(py)),
        Value::Array(values) => {
            let items = values.read().unwrap();
            let mut py_values = Vec::with_capacity(items.len());
            for item in items.iter() {
                py_values.push(value_to_pyobject(py, item)?);
            }
            Ok(PyList::new(py, py_values).into())
        }
        Value::Tuple(values) => {
            let mut py_values = Vec::with_capacity(values.len());
            for value in values {
                py_values.push(value_to_pyobject(py, value)?);
            }
            Ok(PyTuple::new(py, py_values).into())
        }
        Value::Struct(_, fields) => {
            let dict = PyDict::new(py);
            let guard = fields.read().unwrap();
            for (key, value) in guard.iter() {
                dict.set_item(key, value_to_pyobject(py, value)?)
                    .map_err(|err| {
                        KainError::runtime(format!("Python dict conversion error: {err}"))
                    })?;
            }
            Ok(dict.into())
        }
        Value::HostObject(_, _) => extract_python_object(value, py),
        other => Err(KainError::runtime(format!(
            "Cannot convert Kain value to Python object: {other:?}"
        ))),
    }
}

fn py_to_value(obj: &PyAny) -> KainResult<Value> {
    if obj.is_none() {
        return Ok(Value::None);
    }
    if let Ok(value) = obj.extract::<bool>() {
        return Ok(Value::Bool(value));
    }
    if let Ok(value) = obj.extract::<i64>() {
        return Ok(Value::Int(value));
    }
    if let Ok(value) = obj.extract::<f64>() {
        return Ok(Value::Float(value));
    }
    if let Ok(value) = obj.extract::<String>() {
        return Ok(Value::String(value));
    }
    if let Ok(bytes) = obj.downcast::<PyBytes>() {
        let items = bytes
            .as_bytes()
            .iter()
            .map(|value| Value::Int(i64::from(*value)))
            .collect::<Vec<_>>();
        return Ok(Value::Array(Arc::new(RwLock::new(items))));
    }
    if let Ok(bytes) = obj.downcast::<PyByteArray>() {
        let items = bytes
            .to_vec()
            .iter()
            .map(|value| Value::Int(i64::from(*value)))
            .collect::<Vec<_>>();
        return Ok(Value::Array(Arc::new(RwLock::new(items))));
    }
    if let Ok(list) = obj.downcast::<PyList>() {
        let mut values = Vec::with_capacity(list.len());
        for item in list.iter() {
            values.push(py_to_value(item)?);
        }
        return Ok(Value::Array(Arc::new(RwLock::new(values))));
    }
    if let Ok(tuple) = obj.downcast::<PyTuple>() {
        let mut values = Vec::with_capacity(tuple.len());
        for item in tuple.iter() {
            values.push(py_to_value(item)?);
        }
        return Ok(Value::Tuple(values));
    }
    if let Ok(dict) = obj.downcast::<PyDict>() {
        let mut values = HashMap::with_capacity(dict.len());
        for (key, value) in dict.iter() {
            let Ok(key) = key.extract::<String>() else {
                return wrap_python_object(obj);
            };
            values.insert(key, py_to_value(value)?);
        }
        return Ok(Value::Struct(
            "PyDict".to_string(),
            Arc::new(RwLock::new(values)),
        ));
    }
    if let Some(value) = try_numpy_array_to_value(obj)? {
        return Ok(value);
    }

    wrap_python_object(obj)
}

fn try_numpy_array_to_value(obj: &PyAny) -> KainResult<Option<Value>> {
    let module_name = obj
        .get_type()
        .getattr("__module__")
        .and_then(|value| value.extract::<String>())
        .unwrap_or_default();
    let type_name = obj
        .get_type()
        .name()
        .map(|name| name.to_string())
        .unwrap_or_default();

    if !module_name.starts_with("numpy") || type_name != "ndarray" {
        return Ok(None);
    }

    let list_value = obj
        .call_method0("tolist")
        .map_err(|err| KainError::runtime(format!("NumPy array conversion error: {err}")))?;
    Ok(Some(py_to_value(list_value)?))
}

fn wrap_python_object(obj: &PyAny) -> KainResult<Value> {
    let label = python_object_label(obj);
    Ok(Value::host_object(
        label,
        Arc::new(PythonObjectRef {
            object: obj.into_py(obj.py()),
        }),
    ))
}

fn python_object_label(obj: &PyAny) -> String {
    let type_name = obj
        .get_type()
        .name()
        .map(|name| name.to_string())
        .unwrap_or_else(|_| "object".to_string());
    if let Ok(module_name) = obj
        .getattr("__module__")
        .and_then(|value| value.extract::<String>())
    {
        if let Ok(qual_name) = obj
            .getattr("__qualname__")
            .and_then(|value| value.extract::<String>())
        {
            return format!("python:{module_name}.{qual_name}");
        }
        if let Ok(name) = obj
            .getattr("__name__")
            .and_then(|value| value.extract::<String>())
        {
            return format!("python:{module_name}.{name}");
        }
    }
    if let Ok(repr) = obj.repr().and_then(|value| value.extract::<String>()) {
        let trimmed = repr.chars().take(48).collect::<String>();
        return format!("python:{type_name}:{trimmed}");
    }
    format!("python:{}", type_name)
}

fn python_scope_state(env: &Env) -> KainResult<Arc<PythonScopeState>> {
    env.get_extension_state::<PythonScopeState>(PYTHON_EXTENSION_KEY)
        .ok_or_else(|| KainError::runtime("Python runtime is not registered for this environment"))
}

fn scope_dict_from_guard<'py>(py: Python<'py>, scope: &'py PyObject) -> KainResult<&'py PyDict> {
    scope
        .as_ref(py)
        .downcast::<PyDict>()
        .map_err(|err| KainError::runtime(format!("Python scope error: {err}")))
}

#[cfg(test)]
mod tests {
    use super::register;
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::parser::Parser;
    use kain_core::runtime::{interpret, Value};
    use kain_core::stdlib::StdLib;
    use kain_core::types;
    use pyo3::Python;

    #[test]
    fn python_builtins_extend_stdlib_metadata() {
        register();
        let stdlib = StdLib::new();
        assert!(stdlib.functions.contains_key("py_eval"));
        assert!(stdlib.functions.contains_key("py_eval_raw"));
        assert!(stdlib.functions.contains_key("py_exec"));
        assert!(stdlib.functions.contains_key("py_import"));
        assert!(stdlib.functions.contains_key("py_call"));
        assert!(stdlib.functions.contains_key("py_call_raw"));
        assert!(stdlib.functions.contains_key("py_getattr"));
        assert!(stdlib.functions.contains_key("py_getattr_raw"));
        assert!(stdlib.functions.contains_key("py_setattr"));
        assert!(stdlib.functions.contains_key("py_hasattr"));
        assert!(stdlib.functions.contains_key("py_buffer"));
        assert!(stdlib.functions.contains_key("py_buffer_info"));
        assert!(stdlib.functions.contains_key("py_buffer_bytes"));
    }

    #[test]
    fn python_bridge_exec_scope_persists_between_calls() {
        let result = interpret_source(
            r#"
fn main() -> Int:
    py_exec("x = 3")
    return py_eval("x + 1")
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 4),
            other => panic!("expected Python bridge to return Int(4), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_calls_module_methods_and_returns_scalars() {
        let result = interpret_source(
            r#"
fn main() -> Float:
    let math = py_import("math")
    return py_call(math, "sqrt", [9.0])
"#,
        );

        match result {
            Value::Float(value) => assert_eq!(value, 3.0),
            other => panic!("expected Python bridge to return Float(3.0), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_supports_keyword_args_and_dict_conversion() {
        let result = interpret_source(
            r#"
fn main() -> Int:
    py_exec("def take_kw(*, value): return value")
    let kwargs = py_eval("{'value': 7}")
    return py_call("take_kw", [], kwargs)
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 7),
            other => panic!("expected Python bridge to return Int(7), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_returns_host_objects_for_modules_and_functions() {
        let result = interpret_source(
            r#"
fn main():
    let math = py_import("math")
    return py_getattr(math, "sqrt")
"#,
        );

        match result {
            Value::HostObject(label, _) => assert!(label.starts_with("python:")),
            other => panic!("expected Python bridge to return HostObject, got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_converts_numpy_arrays_when_available() {
        if !numpy_available() {
            eprintln!("skipping NumPy bridge test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    let np = py_import("numpy")
    return py_call(np, "linspace", [-1.0, 1.0, 5])
"#,
        );

        let Value::Array(values) = result else {
            panic!("expected NumPy bridge to return Array, got {result:?}");
        };
        let values = values.read().unwrap();
        assert_eq!(values.len(), 5);

        match &values[0] {
            Value::Float(value) => assert_eq!(*value, -1.0),
            other => panic!("expected first NumPy sample to be Float(-1.0), got {other:?}"),
        }
        match &values[2] {
            Value::Float(value) => assert_eq!(*value, 0.0),
            other => panic!("expected midpoint NumPy sample to be Float(0.0), got {other:?}"),
        }
        match &values[4] {
            Value::Float(value) => assert_eq!(*value, 1.0),
            other => panic!("expected last NumPy sample to be Float(1.0), got {other:?}"),
        }
    }

    #[test]
    fn python_bridge_exposes_numpy_buffer_metadata_when_available() {
        if !numpy_available() {
            eprintln!("skipping NumPy buffer test because numpy is not installed");
            return;
        }

        let result = interpret_source(
            r#"
fn main():
    py_exec("import numpy as np\ndef make_plane():\n    return np.arange(24, dtype=np.uint8).reshape(2, 3, 4)")
    let plane = py_call_raw("make_plane", [])
    let view = py_buffer(plane)
    let info = py_buffer_info(view)
    assert(info.dtype == "uint8", "expected uint8 dtype")
    assert(info.ndim == 3, "expected 3 dimensions")
    assert(info.shape[0] == 2, "expected height 2")
    assert(info.shape[1] == 3, "expected width 3")
    assert(info.shape[2] == 4, "expected 4 channels")
    assert(info.nbytes == 24, "expected 24 bytes")
    assert(info.c_contiguous == true, "expected contiguous buffer")
    let bytes = py_buffer_bytes(view)
    return bytes[5]
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 5),
            other => panic!("expected buffer bridge to return Int(5), got {other:?}"),
        }
    }

    fn interpret_source(source: &str) -> Value {
        register();

        let tokens = Lexer::new(source).tokenize().unwrap();
        let span_mapper = SpanMapper::new(source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .unwrap();
        kain_core::comptime::eval_program(&mut ast).unwrap();
        let typed = types::check(&ast, &span_mapper, "<test>").unwrap();
        interpret(&typed).unwrap()
    }

    fn numpy_available() -> bool {
        Python::with_gil(|py| py.import("numpy").is_ok())
    }
}
